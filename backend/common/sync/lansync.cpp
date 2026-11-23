#include "common/sync/lansync.h"

#include "common/json.h"
#include "common/library/librarystore.h"
#include "common/library/sync.h"
#include "common/sha256.h"

#include <cstdlib>

namespace devicesync {

LanSync::LanSync(library::Library& library, platform::LanNetwork& network, platform::Scheduler& scheduler)
    : m_library(library), m_network(network), m_scheduler(scheduler)
{
}

std::string LanSync::keyFor(const std::string& secret)
{
    return sha256::hmac(secret, "ProjectManga Wi-Fi sync");
}

void LanSync::start(const std::string& secret, const std::string& deviceId)
{
    stop();
    if (secret.empty()) return;
    m_key = keyFor(secret);
    m_deviceId = deviceId;
    std::string problem;
    if (!m_network.start(m_key, problem)) {
        m_error = problem.empty() ? "Wi-Fi sync can't use the network" : problem;
        statusChanged();
        return;
    }
    m_running = true;
    m_network.onAnnouncement = [this](const std::string& message, const std::string& from) { heard(message, from); };
    m_network.onExchange = [this](const std::string& theirs) {
        merge(theirs);
        return ownText();
    };
    announceLoop(m_generation);
}

void LanSync::stop()
{
    ++m_generation;
    if (m_running) m_network.stop();
    m_network.onAnnouncement = nullptr;
    m_network.onExchange = nullptr;
    m_running = false;
    m_peers.clear();
    m_error.clear();
    m_lastNearby = 0;
    statusChanged();
}

int LanSync::nearbyDevices() const
{
    int n = 0;
    const std::int64_t now = m_scheduler.nowMs();
    for (const auto& [device, peer] : m_peers)
        if (now - peer.lastSeenMs <= nearbyMs) ++n;
    return n;
}

void LanSync::announceLoop(int generation)
{
    if (generation != m_generation || !m_running) return;
    const std::int64_t now = m_scheduler.nowMs();
    const int port = m_network.exchangePort();
    json::Value message;
    message.set("app", "ProjectManga");
    message.set("v", 1);
    message.set("device", m_deviceId);
    message.set("port", port);
    message.set("time", now);
    message.set("tag", sha256::hex(tagFor(m_deviceId, port, now)));
    m_network.announce(json::write(message));

    if (nearbyDevices() != m_lastNearby) {
        m_lastNearby = nearbyDevices();
        statusChanged();
    }
    auto alive = m_lifetime.watch();
    m_scheduler.after(announceMs, [this, generation, alive] {
        if (!alive.expired()) announceLoop(generation);
    });
}

std::string LanSync::tagFor(const std::string& device, int port, std::int64_t timeMs) const
{
    return sha256::hmac(m_key, device + "|" + std::to_string(port) + "|" + std::to_string(timeMs));
}

void LanSync::heard(const std::string& message, const std::string& from)
{
    const auto parsed = json::parse(message);
    if (!parsed.ok()) return;
    const json::Value& m = parsed.value;
    if (m["app"].asString() != "ProjectManga" || m["v"].asInt() != 1) return;
    const std::string device = m["device"].asString();
    const int port = static_cast<int>(m["port"].asInt());
    const std::int64_t time = m["time"].asInt();
    if (device.empty() || device == m_deviceId || port <= 0 || port > 65535) return;
    // Signed with our key: one of this user's devices. Anything else is someone else's.
    if (!sha256::sameBytes(sha256::hex(tagFor(device, port, time)), m["tag"].asString())) return;
    const std::int64_t now = m_scheduler.nowMs();
    if (std::llabs(now - time) > clockSlackMs) return;

    Peer& peer = m_peers[device];
    if (time <= peer.lastAnnouncedMs) return;           // the same announcement again
    const bool wasNearby = now - peer.lastSeenMs <= nearbyMs;
    peer.lastAnnouncedMs = time;
    peer.lastSeenMs = now;
    peer.address = from;
    peer.port = port;
    if (!wasNearby) {
        m_lastNearby = nearbyDevices();
        statusChanged();
    }

    // One side keeps the pair in step; either side sends its own changes at once.
    const bool due = device > m_deviceId && now - peer.lastExchangeMs >= exchangeEveryMs;
    const bool changed = sha256::digest(ownText()) != peer.lastSent;
    if (!peer.exchanging && (due || changed)) exchangeWith(device);
}

void LanSync::exchangeWith(const std::string& device)
{
    Peer& peer = m_peers[device];
    peer.exchanging = true;
    const std::string mine = ownText();
    const int generation = m_generation;
    auto alive = m_lifetime.watch();
    m_network.exchange(peer.address, peer.port, mine,
                       [this, generation, alive, device, mine](const std::string& theirs, const std::string& problem) {
                           if (alive.expired() || generation != m_generation) return;
                           Peer& p = m_peers[device];
                           p.exchanging = false;
                           p.lastExchangeMs = m_scheduler.nowMs();
                           if (!problem.empty()) return;            // tried again on a later announcement
                           p.lastSent = sha256::digest(mine);
                           merge(theirs);
                       });
}

bool LanSync::merge(const std::string& text)
{
    const auto parsed = json::parse(text);
    if (!parsed.ok() || !parsed.value["entries"].isArray()) return false;
    library::Snapshot mine{m_library.entries(), m_library.removals()};
    const library::Snapshot theirs{library::fromJson(parsed.value), library::removalsFromJson(parsed.value)};
    if (!library::mergeInto(mine, theirs)) return false;
    m_library.replaceAll(std::move(mine.entries), std::move(mine.removals));
    return true;
}

std::string LanSync::ownText() const
{
    json::Value doc = library::toJson(m_library.entries(), m_library.removals());
    doc.set("deviceId", m_deviceId);
    return json::write(doc);
}

void LanSync::statusChanged()
{
    if (onStatusChanged) onStatusChanged();
}

} // namespace devicesync
