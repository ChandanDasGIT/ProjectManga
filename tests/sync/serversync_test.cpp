#include "testkit.h"

#include "common/json.h"
#include "common/sync/serversync.h"
#include "fakes.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

const std::string base = "https://sync.test";

// The sync server's API, kept in memory. Requests wait until answerAll(), like fakes::Http.
class FakeServer : public platform::HttpClient {
public:
    explicit FakeServer(fakes::Scheduler& clock) : m_clock(clock) {}

    std::string key = "secret";
    bool down = false;
    std::int64_t wipedAt = 0;              // 0: never
    std::size_t fileLimit = 20;
    bool hasChanges = true;                // false: an older server without /changes
    std::int64_t version = 0;
    std::map<std::string, std::pair<std::string, std::int64_t>> files;      // name -> body, mtime
    std::vector<platform::HttpRequest> requests;

    void get(const platform::HttpRequest& request, std::function<void(platform::HttpResponse)> done) override
    {
        requests.push_back(request);
        m_waiting.push_back({request, std::move(done)});
    }

    void answerAll()
    {
        while (!m_waiting.empty()) {
            auto waiting = std::move(m_waiting);
            m_waiting.clear();
            for (auto& [request, done] : waiting) {
                if (holds(request)) {
                    m_held.push_back({request, std::move(done)});
                    continue;
                }
                done(answer(request));
            }
        }
    }
    // The server's 25-second wait runs out: every held /changes request gets an answer.
    void timeOutWaiting()
    {
        auto held = std::move(m_held);
        m_held.clear();
        for (auto& [request, done] : held) done(answer(request));
        answerAll();
    }
    std::size_t heldCount() const { return m_held.size(); }

    int count(const std::string& method, const std::string& path) const
    {
        int n = 0;
        for (const auto& r : requests)
            if (r.method == method && r.url == base + path) ++n;
        return n;
    }

private:
    static std::int64_t afterOf(const std::string& url)
    {
        const auto at = url.find("?after=");
        return at == std::string::npos ? -1 : std::stoll(url.substr(at + 7));
    }
    // A /changes request with nothing newer than it knows waits, as on the real server.
    bool holds(const platform::HttpRequest& request) const
    {
        return hasChanges && !down && request.url.starts_with(base + "/changes") &&
               afterOf(request.url) >= version;
    }
    void changed()
    {
        version = std::max(version + 1, m_clock.now);
        for (auto& held : m_held) m_waiting.push_back(std::move(held));
        m_held.clear();
    }

    platform::HttpResponse answer(const platform::HttpRequest& request)
    {
        platform::HttpResponse r;
        if (down) {
            r.error = "Connection refused";
            return r;
        }
        std::string sent;
        for (const auto& [k, v] : request.headers)
            if (k == "X-App-Key") sent = v;
        if (sent != key) {
            r.status = 403;
            r.body = R"({"error":"app_key"})";
            return r;
        }
        const std::string path = request.url.substr(base.size());
        if (request.method == "GET" && path.starts_with("/changes") && hasChanges) {
            r.status = 200;
            r.body = R"({"version":)" + std::to_string(version) + "}";
            return r;
        }
        if (request.method == "GET" && path == "/info") {
            r.status = 200;
            r.body = R"({"syncKey":"abc=","wipedAt":)" + std::to_string(wipedAt) + "}";
            return r;
        }
        if (request.method == "GET" && path == "/files") {
            json::Value list = json::Value::array();
            for (const auto& [name, file] : files) {
                json::Value item;
                item.set("name", name);
                item.set("size", static_cast<std::int64_t>(file.first.size()));
                item.set("mtime", file.second);
                list.push(std::move(item));
            }
            r.status = 200;
            r.body = json::write(list);
            return r;
        }
        if (request.method == "POST" && path == "/delete-all") {
            if (request.body.find("DELETE") == std::string::npos) {
                r.status = 400;
                return r;
            }
            files.clear();
            wipedAt = m_clock.now;
            changed();
            r.status = 204;
            return r;
        }
        if (path.starts_with("/files/")) {
            const std::string name = path.substr(7);
            const auto it = files.find(name);
            if (request.method == "PUT") {
                for (const auto& [k, v] : request.headers)
                    if (k == "X-Wiped-At" && v != std::to_string(wipedAt)) {
                        r.status = 409;
                        r.body = R"({"error":"wiped","wipedAt":)" + std::to_string(wipedAt) + "}";
                        return r;
                    }
                if (it == files.end() && files.size() >= fileLimit) {
                    r.status = 507;
                    r.body = R"({"error":"file_limit"})";
                    return r;
                }
                files[name] = {request.body, m_clock.now};
                changed();
                r.status = 200;
                r.body = "{}";
            } else if (it == files.end()) {
                r.status = 404;
            } else if (request.method == "GET") {
                r.status = 200;
                r.body = it->second.first;
            } else if (request.method == "DELETE") {
                files.erase(it);
                changed();
                r.status = 204;
            }
            return r;
        }
        r.status = 404;
        return r;
    }

    fakes::Scheduler& m_clock;
    std::vector<std::pair<platform::HttpRequest, std::function<void(platform::HttpResponse)>>> m_waiting;
    std::vector<std::pair<platform::HttpRequest, std::function<void(platform::HttpResponse)>>> m_held;
};

library::Entry manga(const std::string& id, std::int64_t addedMs)
{
    library::Entry e;
    e.sourceId = "mangadex";
    e.mangaId = id;
    e.title = "Title " + id;
    e.addedMs = addedMs;
    return e;
}

// One device: its library, synced through the server.
struct Device {
    library::Library library;
    devicesync::ServerSync sync;
    std::string id;
    Device(FakeServer& server, fakes::Scheduler& clock, std::string deviceId)
        : sync(library, server, clock), id(std::move(deviceId))
    {
        library.onChanged = [this] { sync.libraryChanged(); };
    }
    std::int64_t knownWipedAt = -1;
    void start(const std::string& key = "secret")
    {
        sync.onKnownWipedAtChanged = [this](std::int64_t w) { knownWipedAt = w; };
        sync.start(base, key, id, knownWipedAt);
    }
};

} // namespace

TEST(serversync, two_devices_share_their_libraries)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    Device phone(server, clock, "phone");
    pc.library.add(manga("from-pc", 10));
    phone.library.add(manga("from-phone", 20));

    pc.start();
    server.answerAll();
    phone.start();
    server.answerAll();
    CHECK(phone.library.contains("mangadex", "from-pc"));
    CHECK(phone.library.contains("mangadex", "from-phone"));
    CHECK_EQ(phone.sync.otherDevices(), 1);

    pc.sync.syncNow();
    server.answerAll();
    CHECK(pc.library.contains("mangadex", "from-phone"));
    CHECK_EQ(server.files.size(), std::size_t(2));
    CHECK(server.files.contains("ProjectManga-pc.json"));
    CHECK(server.files.contains("ProjectManga-phone.json"));
    CHECK(pc.sync.error().empty());
    CHECK(pc.sync.lastSyncMs() > 0);

    // Settled: another round changes nothing and sends nothing up.
    const int putsBefore = server.count("PUT", "/files/ProjectManga-pc.json");
    pc.sync.syncNow();
    server.answerAll();
    CHECK_EQ(server.count("PUT", "/files/ProjectManga-pc.json"), putsBefore);
}

TEST(serversync, only_changed_files_are_downloaded)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    Device phone(server, clock, "phone");
    phone.start();
    server.answerAll();
    pc.start();
    server.answerAll();
    CHECK_EQ(server.count("GET", "/files/ProjectManga-phone.json"), 1);

    pc.sync.syncNow();
    server.answerAll();
    CHECK_EQ(server.count("GET", "/files/ProjectManga-phone.json"), 1);

    clock.now += 5000;
    phone.library.add(manga("new", 30));
    clock.advance(devicesync::ServerSync::writeDelayMs);
    server.answerAll();
    pc.sync.syncNow();
    server.answerAll();
    CHECK_EQ(server.count("GET", "/files/ProjectManga-phone.json"), 2);
    CHECK(pc.library.contains("mangadex", "new"));
}

TEST(serversync, a_change_goes_up_a_moment_later_and_the_list_is_polled)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    pc.start();
    server.answerAll();
    const int puts = server.count("PUT", "/files/ProjectManga-pc.json");

    pc.library.add(manga("a", 10));
    clock.advance(devicesync::ServerSync::writeDelayMs - 1);
    server.answerAll();
    CHECK_EQ(server.count("PUT", "/files/ProjectManga-pc.json"), puts);
    clock.advance(1);
    server.answerAll();
    CHECK_EQ(server.count("PUT", "/files/ProjectManga-pc.json"), puts + 1);
    CHECK(server.files.at("ProjectManga-pc.json").first.find("\"a\"") != std::string::npos);

    const int lists = server.count("GET", "/files");
    clock.advance(devicesync::ServerSync::pollMs);
    server.answerAll();
    CHECK_EQ(server.count("GET", "/files"), lists + 1);
}

TEST(serversync, an_unreachable_server_is_reported_and_tried_again)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    server.down = true;
    Device pc(server, clock, "pc");
    pc.library.add(manga("a", 10));
    pc.start();
    server.answerAll();
    CHECK_EQ(pc.sync.error(), std::string("Can't reach the sync server right now; it tries again by itself"));
    CHECK(pc.sync.unreachable());
    CHECK(!pc.sync.busy());
    CHECK(server.files.empty());

    server.down = false;
    clock.advance(devicesync::ServerSync::pollMs);
    server.answerAll();
    CHECK(pc.sync.error().empty());
    CHECK(!pc.sync.unreachable());
    CHECK(server.files.contains("ProjectManga-pc.json"));
}

TEST(serversync, a_wrong_key_is_reported)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    pc.start("wrong");
    server.answerAll();
    CHECK_EQ(pc.sync.error(), std::string("The sync server refused this app's key"));
    CHECK(server.files.empty());
}

TEST(serversync, delete_all_empties_the_server_and_stops)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    Device phone(server, clock, "phone");
    pc.library.add(manga("a", 10));
    pc.start();
    phone.start();
    server.answerAll();
    CHECK_EQ(server.files.size(), std::size_t(2));

    std::string problem = "not called";
    pc.sync.deleteAll([&](const std::string& p) { problem = p; });
    server.answerAll();
    CHECK_EQ(problem, std::string());
    CHECK(server.files.empty());
    CHECK(!pc.sync.running());

    // Stopped: changes and polls send nothing.
    pc.library.add(manga("b", 20));
    clock.advance(devicesync::ServerSync::pollMs * 2);
    server.answerAll();
    CHECK(!server.files.contains("ProjectManga-pc.json"));
    CHECK(pc.library.contains("mangadex", "a"));
}

TEST(serversync, a_failed_delete_keeps_syncing)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    pc.start();
    server.answerAll();
    server.down = true;
    std::string problem;
    pc.sync.deleteAll([&](const std::string& p) { problem = p; });
    server.answerAll();
    CHECK_EQ(problem, std::string("Can't reach the sync server right now; it tries again by itself"));
    CHECK(pc.sync.running());
}

TEST(serversync, a_server_emptied_elsewhere_gets_the_file_again)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    pc.library.add(manga("a", 10));
    pc.start();
    server.answerAll();
    server.files.clear();
    pc.sync.syncNow();
    server.answerAll();
    CHECK(server.files.contains("ProjectManga-pc.json"));
}

TEST(serversync, files_it_does_not_know_are_passed_over)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    server.files["notes.txt"] = {"hello", 1};
    server.files["ProjectManga-broken.json"] = {"not json", 1};
    Device pc(server, clock, "pc");
    pc.start();
    server.answerAll();
    CHECK(pc.sync.error().empty());
    CHECK_EQ(server.count("GET", "/files/notes.txt"), 0);
    pc.sync.syncNow();
    server.answerAll();
    CHECK_EQ(server.count("GET", "/files/ProjectManga-broken.json"), 1);
}

TEST(serversync, the_first_sync_learns_the_last_wipe)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    server.wipedAt = 1234;
    Device pc(server, clock, "pc");
    pc.start();
    server.answerAll();
    CHECK_EQ(pc.knownWipedAt, std::int64_t(1234));
    CHECK(!pc.sync.wipeQuestion());
    CHECK(server.files.contains("ProjectManga-pc.json"));
}

TEST(serversync, a_wipe_from_another_device_asks_before_uploading)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    Device phone(server, clock, "phone");
    pc.library.add(manga("a", 10));
    pc.start();
    phone.start();
    server.answerAll();

    std::string problem = "not called";
    phone.sync.deleteAll([&](const std::string& p) { problem = p; });
    server.answerAll();
    CHECK(problem.empty());
    CHECK(server.files.empty());

    // The pc notices on its next sync and uploads nothing until the user answers.
    pc.sync.syncNow();
    server.answerAll();
    CHECK(pc.sync.wipeQuestion());
    CHECK(server.files.empty());
    pc.library.add(manga("b", 20));
    clock.advance(devicesync::ServerSync::pollMs * 2);
    server.answerAll();
    CHECK(server.files.empty());

    pc.sync.answerWipe(true);
    server.answerAll();
    CHECK(!pc.sync.wipeQuestion());
    CHECK_EQ(pc.knownWipedAt, server.wipedAt);
    CHECK(server.files.contains("ProjectManga-pc.json"));
}

TEST(serversync, keeping_sync_off_after_a_wipe_uploads_nothing)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    pc.library.add(manga("a", 10));
    pc.start();
    server.answerAll();
    server.files.clear();
    server.wipedAt = clock.now;

    pc.sync.syncNow();
    server.answerAll();
    CHECK(pc.sync.wipeQuestion());
    pc.sync.answerWipe(false);
    CHECK(!pc.sync.running());
    CHECK_EQ(pc.knownWipedAt, server.wipedAt);
    clock.advance(devicesync::ServerSync::pollMs * 2);
    server.answerAll();
    CHECK(server.files.empty());

    // Turned on again later: the wipe is known, so it simply uploads.
    pc.start();
    server.answerAll();
    CHECK(!pc.sync.wipeQuestion());
    CHECK(server.files.contains("ProjectManga-pc.json"));
}

TEST(serversync, a_wipe_between_the_check_and_the_upload_is_caught)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    pc.start();
    server.answerAll();

    // Wiped on another device; then a change here goes straight to the upload.
    server.files.clear();
    server.wipedAt = clock.now;
    pc.library.add(manga("a", 10));
    clock.advance(devicesync::ServerSync::writeDelayMs);
    server.answerAll();
    CHECK(pc.sync.wipeQuestion());
    CHECK(server.files.empty());
}

TEST(serversync, own_delete_does_not_ask_when_turned_on_again)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    pc.library.add(manga("a", 10));
    pc.start();
    server.answerAll();
    pc.sync.deleteAll([](const std::string&) {});
    server.answerAll();
    CHECK(server.files.empty());

    pc.start();
    server.answerAll();
    CHECK(!pc.sync.wipeQuestion());
    CHECK(server.files.contains("ProjectManga-pc.json"));
    CHECK_EQ(pc.knownWipedAt, server.wipedAt);
}

TEST(serversync, too_many_device_files_is_reported)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    server.fileLimit = 1;
    server.files["ProjectManga-old.json"] = {R"({"entries":[]})", 1};
    Device pc(server, clock, "pc");
    pc.start();
    server.answerAll();
    CHECK_EQ(pc.sync.error(),
             std::string("There are too many device files on the sync server (the limit is 20); remove old ones on the server"));
    CHECK(!pc.sync.unreachable());
}

TEST(serversync, a_change_arrives_at_once_without_waiting_for_the_poll)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    Device phone(server, clock, "phone");
    pc.start();
    phone.start();
    server.answerAll();
    CHECK_EQ(server.heldCount(), std::size_t(2));      // both wait for news

    clock.now += 1000;
    phone.library.add(manga("new", 30));
    clock.advance(devicesync::ServerSync::writeDelayMs);
    server.answerAll();
    // Well before the regular check: the pc already has it.
    CHECK(pc.library.contains("mangadex", "new"));
    CHECK_EQ(server.heldCount(), std::size_t(2));      // and both wait again
    // The /changes requests may stay open longer than ordinary ones.
    int longWaits = 0;
    for (const auto& r : server.requests)
        if (r.url.find("/changes") != std::string::npos) longWaits += r.timeoutMs == devicesync::ServerSync::watchTimeoutMs;
    CHECK(longWaits > 0);

    // A removal travels the same way.
    pc.library.remove("mangadex", "new", clock.now + 1);
    clock.advance(devicesync::ServerSync::writeDelayMs);
    server.answerAll();
    CHECK(!phone.library.contains("mangadex", "new"));
}

TEST(serversync, a_quiet_wait_just_asks_again)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    pc.start();
    server.answerAll();
    const int lists = server.count("GET", "/files");
    server.timeOutWaiting();
    CHECK_EQ(server.heldCount(), std::size_t(1));
    CHECK_EQ(server.count("GET", "/files"), lists);    // nothing changed: no sync
}

TEST(serversync, an_older_server_without_changes_still_syncs_by_polling)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    server.hasChanges = false;
    Device pc(server, clock, "pc");
    Device phone(server, clock, "phone");
    pc.start();
    phone.start();
    server.answerAll();
    CHECK(pc.sync.error().empty());
    phone.library.add(manga("new", 30));
    clock.advance(devicesync::ServerSync::writeDelayMs);
    server.answerAll();
    CHECK(!pc.library.contains("mangadex", "new"));
    clock.advance(devicesync::ServerSync::pollMs);
    server.answerAll();
    CHECK(pc.library.contains("mangadex", "new"));
}

TEST(serversync, waiting_resumes_after_the_server_comes_back)
{
    fakes::Scheduler clock;
    FakeServer server(clock);
    Device pc(server, clock, "pc");
    Device phone(server, clock, "phone");
    pc.start();
    phone.start();
    server.answerAll();
    // The laptop goes to sleep: the waiting requests fail.
    server.down = true;
    server.timeOutWaiting();
    CHECK_EQ(server.heldCount(), std::size_t(0));
    server.down = false;
    clock.advance(devicesync::ServerSync::watchRetryMs);
    server.answerAll();
    CHECK_EQ(server.heldCount(), std::size_t(2));

    phone.library.add(manga("back", 40));
    clock.advance(devicesync::ServerSync::writeDelayMs);
    server.answerAll();
    CHECK(pc.library.contains("mangadex", "back"));
}
