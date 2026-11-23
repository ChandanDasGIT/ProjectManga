#include "testkit.h"
#include "fakes.h"

#include "common/library/updatechecker.h"
#include "common/sources/webLink/weblinksource.h"

namespace {

constexpr std::int64_t day = 86'400'000;
constexpr std::int64_t hour = 3'600'000;

std::string seriesWith(int chapters)
{
    std::string html;
    for (int i = 1; i <= chapters; ++i)
        html += "<a href=\"/s/chapter-" + std::to_string(i) + "/\">Chapter " + std::to_string(i) + "</a>";
    return html;
}

struct Setup {
    fakes::Http http;
    fakes::Scheduler clock;
    sources::SourceRegistry sources;
    library::Library lib;
    library::UpdateChecker checker{lib, sources, clock};

    Setup() { sources.add(std::make_unique<weblink::WebLinkSource>(http, clock)); }

    void add(const std::string& address)
    {
        library::Entry e;
        e.sourceId = "web";
        e.mangaId = address;
        lib.add(e);
    }
    // Runs timers and answers requests until a moment `ms` from now.
    void run(std::int64_t ms)
    {
        const std::int64_t end = clock.now + ms;
        while (true) {
            http.answerAll();
            const std::int64_t next = clock.nextTimerAt();
            if (next < 0 || next > end) break;
            clock.advance(std::max<std::int64_t>(0, next - clock.now));
        }
        clock.advance(end - clock.now);
        http.answerAll();
    }
};

} // namespace

TEST(updatechecker, checks_new_entries_at_start)
{
    Setup s;
    s.http.set("https://a.example/s/", seriesWith(3));
    s.add("https://a.example/s/");
    s.checker.start();
    s.run(0);
    CHECK_EQ(s.lib.find("web", "https://a.example/s/")->chapterNumbers.size(), std::size_t(3));
    CHECK_EQ(s.lib.totalNew(), 0);
}

TEST(updatechecker, finds_a_new_chapter_the_next_day)
{
    Setup s;
    s.http.set("https://a.example/s/", seriesWith(3));
    s.add("https://a.example/s/");
    s.checker.start();
    s.run(0);

    s.http.set("https://a.example/s/", seriesWith(4));
    s.run(day - hour);
    CHECK_EQ(s.lib.totalNew(), 0);                    // not checked again yet
    s.run(2 * hour);
    CHECK_EQ(s.lib.totalNew(), 1);
}

TEST(updatechecker, retries_a_failed_check_after_an_hour)
{
    Setup s;
    s.add("https://down.example/s/");
    s.checker.start();
    s.run(0);
    CHECK_EQ(s.http.requests.size(), std::size_t(1));
    s.run(hour - 1000);
    CHECK_EQ(s.http.requests.size(), std::size_t(1));
    s.run(2000);
    CHECK_EQ(s.http.requests.size(), std::size_t(2));
}

TEST(updatechecker, checks_at_the_estimated_release_time)
{
    Setup s;
    library::Entry e;
    e.sourceId = "web";
    e.mangaId = "https://a.example/s/";
    e.chapterNumbers = {1, 2, 3};
    e.seenLatest = 3;
    e.lastCheckedMs = s.clock.now;
    e.nextEstimateMs = s.clock.now + 5 * hour;
    s.lib.add(e);
    s.http.set("https://a.example/s/", seriesWith(4));

    s.checker.start();
    s.run(5 * hour - 1000);
    CHECK(s.http.requests.empty());
    s.run(2000);
    CHECK_EQ(s.http.requests.size(), std::size_t(1));
    CHECK_EQ(s.lib.totalNew(), 1);
}

TEST(updatechecker, one_entry_after_another)
{
    Setup s;
    for (const char* site : {"https://a.example/s/", "https://b.example/s/", "https://c.example/s/"}) {
        s.http.set(site, seriesWith(2));
        s.add(site);
    }
    s.checker.start();
    CHECK_EQ(s.http.waitingCount(), std::size_t(1));
    s.run(0);
    s.run(1000);                                      // the web source allows 2 requests a second
    CHECK_EQ(s.http.requests.size(), std::size_t(3));
    CHECK(!s.checker.checking());
}

TEST(updatechecker, due_time)
{
    library::Entry e;
    CHECK_EQ(library::UpdateChecker::dueAt(e), std::int64_t(0));
    e.lastCheckedMs = 10 * day;
    CHECK_EQ(library::UpdateChecker::dueAt(e), 11 * day);
    e.nextEstimateMs = 10 * day + hour;
    CHECK_EQ(library::UpdateChecker::dueAt(e), 10 * day + hour);
    e.nextEstimateMs = 9 * day;                       // a past estimate does not count again
    CHECK_EQ(library::UpdateChecker::dueAt(e), 11 * day);
}

TEST(updatechecker, leaves_finished_books_alone)
{
    struct Book : weblink::WebLinkSource {
        using WebLinkSource::WebLinkSource;
        std::string id() const override { return "book"; }
        bool updates() const override { return false; }
    };
    Setup s;
    s.sources.add(std::make_unique<Book>(s.http, s.clock));
    library::Entry e;
    e.sourceId = "book";
    e.mangaId = "https://a.example/b/";
    s.lib.add(e);
    s.checker.start();
    s.run(2 * day);
    CHECK(s.http.requests.empty());
}
