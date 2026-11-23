#pragma once

#include "common/library/library.h"
#include "common/platform/httpclient.h"
#include "common/platform/lannetwork.h"
#include "common/platform/scheduler.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>

// Keeps the library the same on the user's devices that share a local network (home Wi-Fi),
// without the server. Each device announces itself every few seconds; devices whose
// announcements are signed with the same key (made from the secret key built into the app)
// exchange their whole libraries over an encrypted connection and merge them
// (library::mergeInto). A change here goes out with the next announcement heard, since what was
// last sent no longer matches. Anyone else's announcements are ignored.
namespace devicesync {

class LanSync {
public:
    LanSync(library::Library& library, platform::LanNetwork& network, platform::Scheduler& scheduler);

    // secret: the app's secret key (empty: Wi-Fi sync stays off).
    void start(const std::string& secret, const std::string& deviceId);
    void stop();
    bool running() const { return m_running; }

    // Own devices heard from in the last nearbyMs.
    int nearbyDevices() const;
    // Why Wi-Fi sync can't run; empty when it can.
    const std::string& error() const { return m_error; }

    std::function<void()> onStatusChanged;

    static constexpr std::int64_t announceMs = 5'000;
    static constexpr std::int64_t nearbyMs = 20'000;
    static constexpr std::int64_t exchangeEveryMs = 30'000;
    // How far apart two devices' clocks may be before announcements are refused as old.
    static constexpr std::int64_t clockSlackMs = 120'000;
    // The 32-byte key both the announcements and the encryption use.
    static std::string keyFor(const std::string& secret);

private:
    struct Peer {
        std::string address;
        int port = 0;
        std::int64_t lastSeenMs = 0;
        std::int64_t lastAnnouncedMs = 0;
        std::int64_t lastExchangeMs = 0;
        std::string lastSent;           // hash of the library last sent to this device
        bool exchanging = false;
    };

    void announceLoop(int generation);
    void heard(const std::string& message, const std::string& from);
    void exchangeWith(const std::string& device);
    bool merge(const std::string& text);
    std::string ownText() const;
    std::string tagFor(const std::string& device, int port, std::int64_t timeMs) const;
    void statusChanged();

    library::Library& m_library;
    platform::LanNetwork& m_network;
    platform::Scheduler& m_scheduler;
    std::string m_key;
    std::string m_deviceId;
    bool m_running = false;
    int m_generation = 0;
    int m_lastNearby = 0;
    std::map<std::string, Peer> m_peers;
    std::string m_error;
    platform::Lifetime m_lifetime;
};

} // namespace devicesync
