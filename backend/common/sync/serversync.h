#pragma once

#include "common/library/library.h"
#include "common/platform/httpclient.h"
#include "common/platform/scheduler.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Keeps the library the same on all of the user's devices through their own sync server. Every
// device uploads only its own file, ProjectManga-<device id>.json (its whole library), and
// downloads the other devices' files, merging them in (library::mergeInto). The server only
// stores the files; every request carries the secret key built into the app.
//
// The server remembers when its data was last deleted ("wipedAt", from GET /info). A device
// that sees a newer wipe than the one it knows stops before uploading and waits for the user to
// decide whether its data goes back (wipeQuestion(), answerWipe()), so a deletion made on one
// device isn't undone by another quietly uploading again. Uploads carry the wipe they know of,
// and the server refuses one that is out of date (409).
//
// While running, one request to GET /changes stays open: the server answers it as soon as any
// device uploads or deletes, and a sync follows at once. The regular check every pollMs remains
// as a fallback (for a server without /changes, or when the waiting request keeps failing).
namespace devicesync {

class ServerSync {
public:
    ServerSync(library::Library& library, platform::HttpClient& http, platform::Scheduler& scheduler);

    // address: "https://host" (empty: no server, sync stays off). knownWipedAt: the wipe this
    // device last saw (-1: none yet; the first answer is then taken as known). Syncs at once,
    // then every pollMs.
    void start(const std::string& address, const std::string& key, const std::string& deviceId,
               std::int64_t knownWipedAt);
    void stop();
    bool running() const { return !m_address.empty(); }

    // Downloads what changed on the other devices, merges it, and uploads this device's file.
    void syncNow();
    // The library changed here: this device's file goes up a moment later.
    void libraryChanged();
    // Deletes every device's file on the server and stops syncing. `done` gets an empty string
    // or why it failed.
    void deleteAll(std::function<void(const std::string& problem)> done);

    // The server's data was deleted since this device last synced: nothing goes up until
    // answerWipe(). putBack: upload this device's data again; otherwise sync stays stopped.
    bool wipeQuestion() const { return m_wipeQuestion; }
    void answerWipe(bool putBack);

    bool busy() const { return m_busy; }
    std::int64_t lastSyncMs() const { return m_lastSyncMs; }
    int otherDevices() const { return m_otherDevices; }
    // Why the last sync failed; empty when it worked.
    const std::string& error() const { return m_error; }
    // The last sync only failed because the server couldn't be reached (laptop off or asleep).
    bool unreachable() const { return m_unreachable; }

    std::function<void()> onStatusChanged;
    // The wipe this device knows of changed: keep it for the next start().
    std::function<void(std::int64_t wipedAt)> onKnownWipedAtChanged;

    static constexpr std::int64_t pollMs = 60'000;
    // How long the server may hold a /changes request (it answers within 25 s).
    static constexpr int watchTimeoutMs = 40'000;
    // After a failed /changes request: wait this long, doubling each time up to pollMs.
    static constexpr std::int64_t watchRetryMs = 10'000;
    static constexpr std::int64_t writeDelayMs = 1'500;
    static std::string fileNameFor(const std::string& deviceId);

private:
    struct Listed {
        std::int64_t mtime = 0;
        std::int64_t size = 0;
        bool operator==(const Listed&) const = default;
    };

    platform::HttpRequest request(const std::string& method, const std::string& path,
                                  std::string body = {}) const;
    void runCycle(bool pull);
    void checkedInfo(int generation, const platform::HttpResponse& response);
    void listed(int generation, const platform::HttpResponse& response);
    void downloadNext(int generation);
    void upload(int generation);
    void wiped(std::int64_t wipedAt);
    void setKnownWipedAt(std::int64_t wipedAt);
    void finish(const std::string& problem, bool unreachable = false);
    void failed(const platform::HttpResponse& response);
    void scheduleNextPoll();
    void watch(int watchGeneration);
    void watchAnswered(int watchGeneration, const platform::HttpResponse& response);
    void watchLater(int watchGeneration, std::int64_t delayMs);
    std::string ownText() const;
    void statusChanged();

    library::Library& m_library;
    platform::HttpClient& m_http;
    platform::Scheduler& m_scheduler;
    std::string m_address;
    std::string m_key;
    std::string m_deviceId;
    std::int64_t m_knownWipedAt = -1;
    std::int64_t m_newWipedAt = 0;
    int m_generation = 0;
    int m_pollGeneration = 0;
    int m_watchGeneration = 0;
    // The server's change counter as last seen (-1: not yet); /changes waits for anything newer.
    std::int64_t m_version = -1;
    std::int64_t m_watchRetryMs = watchRetryMs;

    bool m_busy = false;
    bool m_deleting = false;
    bool m_wipeQuestion = false;
    bool m_pullAgain = false;
    bool m_pushAgain = false;
    bool m_writePending = false;
    bool m_merging = false;
    bool m_ownListed = false;
    bool m_unreachable = false;

    // What was downloaded and merged of each other device's file, by name.
    std::map<std::string, Listed> m_merged;
    std::vector<std::pair<std::string, Listed>> m_toDownload;
    std::string m_lastUploaded;

    std::int64_t m_lastSyncMs = 0;
    int m_otherDevices = 0;
    std::string m_error;
    platform::Lifetime m_lifetime;
};

} // namespace devicesync
