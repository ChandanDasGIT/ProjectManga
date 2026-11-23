#include "common/sync/serversync.h"

#include "common/json.h"
#include "common/library/librarystore.h"
#include "common/library/sync.h"

#include <algorithm>

namespace devicesync {

namespace {

const std::string filePrefix = "ProjectManga-";
const std::string fileSuffix = ".json";

// The names the server accepts: ProjectManga-<letters, digits, '_' or '-'>.json.
bool isDeviceFile(const std::string& name)
{
    if (name.size() <= filePrefix.size() + fileSuffix.size()) return false;
    if (!name.starts_with(filePrefix) || !name.ends_with(fileSuffix)) return false;
    const std::string id = name.substr(filePrefix.size(), name.size() - filePrefix.size() - fileSuffix.size());
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

} // namespace

ServerSync::ServerSync(library::Library& library, platform::HttpClient& http, platform::Scheduler& scheduler)
    : m_library(library), m_http(http), m_scheduler(scheduler)
{
}

std::string ServerSync::fileNameFor(const std::string& deviceId)
{
    return filePrefix + deviceId + fileSuffix;
}

void ServerSync::start(const std::string& address, const std::string& key, const std::string& deviceId,
                       std::int64_t knownWipedAt)
{
    stop();
    m_knownWipedAt = knownWipedAt;
    m_address = address;
    while (!m_address.empty() && m_address.back() == '/') m_address.pop_back();
    m_key = key;
    m_deviceId = deviceId;
    m_merged.clear();
    m_lastUploaded.clear();
    m_ownListed = false;
    if (!running()) {
        statusChanged();
        return;
    }
    m_version = -1;
    m_watchRetryMs = watchRetryMs;
    runCycle(true);
    scheduleNextPoll();
    watch(m_watchGeneration);
}

void ServerSync::stop()
{
    m_address.clear();
    ++m_generation;
    ++m_pollGeneration;
    ++m_watchGeneration;
    m_busy = false;
    m_deleting = false;
    m_wipeQuestion = false;
    m_pullAgain = false;
    m_pushAgain = false;
    m_writePending = false;
    m_unreachable = false;
    m_toDownload.clear();
    m_lastSyncMs = 0;
    m_otherDevices = 0;
    m_error.clear();
    statusChanged();
}

void ServerSync::answerWipe(bool putBack)
{
    if (!m_wipeQuestion) return;
    m_wipeQuestion = false;
    setKnownWipedAt(m_newWipedAt);
    if (!putBack) {
        stop();
        return;
    }
    m_merged.clear();
    m_lastUploaded.clear();
    m_ownListed = false;
    statusChanged();
    runCycle(true);
}

void ServerSync::setKnownWipedAt(std::int64_t wipedAt)
{
    if (wipedAt == m_knownWipedAt) return;
    m_knownWipedAt = wipedAt;
    if (onKnownWipedAtChanged) onKnownWipedAtChanged(wipedAt);
}

void ServerSync::wiped(std::int64_t wipedAt)
{
    m_newWipedAt = wipedAt;
    m_wipeQuestion = true;
    m_busy = false;
    m_pullAgain = false;
    m_pushAgain = false;
    m_error.clear();
    m_unreachable = false;
    statusChanged();
}

void ServerSync::syncNow()
{
    runCycle(true);
}

void ServerSync::libraryChanged()
{
    if (!running() || m_merging || m_writePending) return;
    m_writePending = true;
    const int generation = m_generation;
    auto alive = m_lifetime.watch();
    m_scheduler.after(writeDelayMs, [this, generation, alive] {
        if (alive.expired() || generation != m_generation) return;
        m_writePending = false;
        runCycle(false);
    });
}

void ServerSync::deleteAll(std::function<void(const std::string& problem)> done)
{
    if (!running()) {
        done("Sync isn't set up in this build");
        return;
    }
    // Nothing goes up while the server is being emptied.
    ++m_generation;
    ++m_pollGeneration;
    m_busy = false;
    m_deleting = true;
    m_pullAgain = false;
    m_pushAgain = false;
    m_writePending = false;
    const int generation = m_generation;
    auto alive = m_lifetime.watch();
    m_http.get(request("POST", "/delete-all", R"({"confirm":"DELETE"})"),
               [this, generation, alive, done = std::move(done)](platform::HttpResponse response) {
                   if (alive.expired() || generation != m_generation) return;
                   m_deleting = false;
                   if (!response.ok()) {
                       failed(response);
                       scheduleNextPoll();
                       done(m_error);
                       return;
                   }
                   // This device made the wipe: turning sync on again here is the user's answer.
                   setKnownWipedAt(-1);
                   stop();
                   done({});
               });
}

platform::HttpRequest ServerSync::request(const std::string& method, const std::string& path, std::string body) const
{
    platform::HttpRequest r;
    r.method = method;
    r.url = m_address + path;
    r.headers.push_back({"X-App-Key", m_key});
    if (!body.empty()) r.headers.push_back({"Content-Type", "application/json"});
    r.body = std::move(body);
    return r;
}

void ServerSync::runCycle(bool pull)
{
    if (!running() || m_deleting || m_wipeQuestion) return;
    if (m_busy) {
        (pull ? m_pullAgain : m_pushAgain) = true;
        return;
    }
    m_busy = true;
    statusChanged();
    const int generation = m_generation;
    if (!pull) {
        upload(generation);
        return;
    }
    auto alive = m_lifetime.watch();
    m_http.get(request("GET", "/info"), [this, generation, alive](platform::HttpResponse response) {
        if (alive.expired() || generation != m_generation) return;
        checkedInfo(generation, response);
    });
}

void ServerSync::checkedInfo(int generation, const platform::HttpResponse& response)
{
    if (!response.ok()) {
        failed(response);
        return;
    }
    const auto parsed = json::parse(response.body);
    if (!parsed.ok() || !parsed.value["wipedAt"].isNumber()) {
        finish("The sync server sent an answer the app can't read");
        return;
    }
    const std::int64_t wipedAt = parsed.value["wipedAt"].asInt();
    if (m_knownWipedAt < 0) {
        setKnownWipedAt(wipedAt);
    } else if (wipedAt != m_knownWipedAt) {
        wiped(wipedAt);
        return;
    }
    auto alive = m_lifetime.watch();
    m_http.get(request("GET", "/files"), [this, generation, alive](platform::HttpResponse response) {
        if (alive.expired() || generation != m_generation) return;
        listed(generation, response);
    });
}

void ServerSync::listed(int generation, const platform::HttpResponse& response)
{
    if (!response.ok()) {
        failed(response);
        return;
    }
    const auto parsed = json::parse(response.body);
    if (!parsed.ok() || !parsed.value.isArray()) {
        finish("The sync server sent an answer the app can't read");
        return;
    }
    const std::string own = fileNameFor(m_deviceId);
    std::map<std::string, Listed> seen;
    m_toDownload.clear();
    m_ownListed = false;
    for (const auto& item : parsed.value.asArray()) {
        const std::string name = item["name"].asString();
        if (name == own) {
            m_ownListed = true;
            continue;
        }
        if (!isDeviceFile(name)) continue;
        const Listed now{item["mtime"].asInt(), item["size"].asInt()};
        seen[name] = now;
        const auto it = m_merged.find(name);
        if (it == m_merged.end() || !(it->second == now)) m_toDownload.push_back({name, now});
    }
    // A file deleted on the server is downloaded again if it ever comes back.
    std::erase_if(m_merged, [&](const auto& kept) { return !seen.contains(kept.first); });
    m_otherDevices = static_cast<int>(seen.size());
    downloadNext(generation);
}

void ServerSync::downloadNext(int generation)
{
    if (m_toDownload.empty()) {
        upload(generation);
        return;
    }
    const auto [name, listed] = m_toDownload.front();
    m_toDownload.erase(m_toDownload.begin());
    auto alive = m_lifetime.watch();
    m_http.get(request("GET", "/files/" + name),
               [this, generation, alive, name, listed](platform::HttpResponse response) {
                   if (alive.expired() || generation != m_generation) return;
                   if (response.status == 404) {            // deleted since the list was made
                       downloadNext(generation);
                       return;
                   }
                   if (!response.ok()) {
                       failed(response);
                       return;
                   }
                   const auto parsed = json::parse(response.body);
                   if (parsed.ok() && parsed.value["entries"].isArray()) {
                       library::Snapshot mine{m_library.entries(), m_library.removals()};
                       const library::Snapshot theirs{library::fromJson(parsed.value),
                                                      library::removalsFromJson(parsed.value)};
                       if (library::mergeInto(mine, theirs)) {
                           m_merging = true;
                           m_library.replaceAll(std::move(mine.entries), std::move(mine.removals));
                           m_merging = false;
                       }
                   }
                   // A file the app can't read is passed over until it changes.
                   m_merged[name] = listed;
                   downloadNext(generation);
               });
}

void ServerSync::upload(int generation)
{
    std::string text = ownText();
    if (text == m_lastUploaded && m_ownListed) {
        finish({});
        return;
    }
    auto put = request("PUT", "/files/" + fileNameFor(m_deviceId), text);
    if (m_knownWipedAt >= 0) put.headers.push_back({"X-Wiped-At", std::to_string(m_knownWipedAt)});
    auto alive = m_lifetime.watch();
    m_http.get(put, [this, generation, alive, text](platform::HttpResponse response) {
                   if (alive.expired() || generation != m_generation) return;
                   if (response.status == 409) {        // wiped since this device last looked
                       const auto parsed = json::parse(response.body);
                       wiped(parsed.ok() ? parsed.value["wipedAt"].asInt() : 0);
                       return;
                   }
                   if (!response.ok()) {
                       failed(response);
                       return;
                   }
                   m_lastUploaded = text;
                   m_ownListed = true;
                   finish({});
               });
}

void ServerSync::failed(const platform::HttpResponse& response)
{
    const int s = response.status;
    if (!response.error.empty() || s == 0 || s == 502 || s == 503 || s == 504)
        finish("Can't reach the sync server right now; it tries again by itself", true);
    else if (s == 403)
        finish("The sync server refused this app's key");
    else if (s == 507)
        finish("There are too many device files on the sync server (the limit is 20); remove old ones on the server");
    else
        finish("The sync server answered " + std::to_string(s));
}

void ServerSync::finish(const std::string& problem, bool unreachable)
{
    m_busy = false;
    m_error = problem;
    m_unreachable = unreachable;
    if (problem.empty()) m_lastSyncMs = m_scheduler.nowMs();
    statusChanged();
    if (m_pullAgain) {
        m_pullAgain = false;
        m_pushAgain = false;
        runCycle(true);
    } else if (m_pushAgain) {
        m_pushAgain = false;
        runCycle(false);
    }
}

void ServerSync::watch(int watchGeneration)
{
    if (watchGeneration != m_watchGeneration || !running()) return;
    auto r = request("GET", m_version < 0 ? "/changes" : "/changes?after=" + std::to_string(m_version));
    r.timeoutMs = watchTimeoutMs;
    auto alive = m_lifetime.watch();
    m_http.get(r, [this, watchGeneration, alive](platform::HttpResponse response) {
        if (alive.expired() || watchGeneration != m_watchGeneration) return;
        watchAnswered(watchGeneration, response);
    });
}

void ServerSync::watchAnswered(int watchGeneration, const platform::HttpResponse& response)
{
    const auto parsed = json::parse(response.body);
    if (!response.ok() || !parsed.ok() || !parsed.value["version"].isNumber()) {
        // A server without /changes, or one out of reach: the regular check covers it meanwhile.
        if (response.status == 403) return;
        watchLater(watchGeneration, response.status == 404 ? pollMs : m_watchRetryMs);
        m_watchRetryMs = std::min(m_watchRetryMs * 2, pollMs);
        return;
    }
    m_watchRetryMs = watchRetryMs;
    const std::int64_t version = parsed.value["version"].asInt();
    const bool changed = m_version >= 0 && version > m_version;
    m_version = std::max(m_version, version);
    if (changed) runCycle(true);
    watch(watchGeneration);
}

void ServerSync::watchLater(int watchGeneration, std::int64_t delayMs)
{
    auto alive = m_lifetime.watch();
    m_scheduler.after(delayMs, [this, watchGeneration, alive] {
        if (!alive.expired()) watch(watchGeneration);
    });
}

void ServerSync::scheduleNextPoll()
{
    const int poll = ++m_pollGeneration;
    auto alive = m_lifetime.watch();
    m_scheduler.after(pollMs, [this, poll, alive] {
        if (alive.expired() || poll != m_pollGeneration || !running()) return;
        runCycle(true);
        scheduleNextPoll();
    });
}

std::string ServerSync::ownText() const
{
    json::Value doc = library::toJson(m_library.entries(), m_library.removals());
    doc.set("deviceId", m_deviceId);
    return json::write(doc, true) + "\n";
}

void ServerSync::statusChanged()
{
    if (onStatusChanged) onStatusChanged();
}

} // namespace devicesync
