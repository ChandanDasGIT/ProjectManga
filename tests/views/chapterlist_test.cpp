#include "testkit.h"
#include "fakes.h"

#include "common/sources/webLink/weblinksource.h"
#include "mangaDetails/chapterlist.h"

namespace {

sources::Chapter chapter(const std::string& id, double number, std::int64_t publishedMs = 0, std::string external = "")
{
    sources::Chapter c;
    c.id = id;
    c.numberValue = number;
    if (number >= 0) {
        c.number = std::to_string(number);
        c.number.erase(c.number.find_last_not_of('0') + 1);
        if (c.number.back() == '.') c.number.pop_back();
    }
    c.publishedMs = publishedMs;
    c.externalUrl = std::move(external);
    return c;
}

} // namespace

TEST(chapterlist, merges_uploads_of_the_same_chapter)
{
    const auto merged = mangadetails::merged({
        chapter("2-groupA", 2, 200),
        chapter("1-official", 1, 50, "https://official.example/1"),
        chapter("1-groupA", 1, 100),
        chapter("2-groupB", 2, 150),
        chapter("special", -1),
        chapter("1.5", 1.5, 120),
    });
    CHECK_EQ(merged.size(), std::size_t(4));
    CHECK_EQ(merged[0].id, std::string("special"));
    CHECK_EQ(merged[1].id, std::string("1-groupA"));      // readable beats official-only
    CHECK_EQ(merged[2].id, std::string("1.5"));
    CHECK_EQ(merged[3].id, std::string("2-groupB"));      // the earlier upload
}

TEST(chapterlist, labels)
{
    auto c = chapter("x", 12);
    CHECK_EQ(sources::chapterLabel(c), std::string("Chapter 12"));
    c.title = "The Return";
    CHECK_EQ(sources::chapterLabel(c), std::string("Chapter 12 · The Return"));
    CHECK_EQ(sources::chapterLabel(chapter("y", -1)), std::string("Oneshot"));
}

TEST(chapterlist, rows_show_read_and_new)
{
    library::Entry e;
    e.seenLatest = 2;
    e.readNumbers = {1};
    const std::vector<sources::Chapter> list = {chapter("a", 1), chapter("b", 2), chapter("c", 3)};
    const auto rows = mangadetails::rows(list, &e);
    CHECK_EQ(rows.size(), std::size_t(3));
    CHECK_EQ(rows[0].chapter.id, std::string("c"));       // newest first
    CHECK(rows[0].isNew);
    CHECK(!rows[1].isNew);
    CHECK(rows[2].read);
    CHECK(!mangadetails::rows(list, nullptr)[0].isNew);
    CHECK_EQ(mangadetails::rows(list, nullptr, false)[0].chapter.id, std::string("a"));   // reading order
}

TEST(chapterlist, chosen_order_beats_the_sources_own)
{
    CHECK(mangadetails::newestFirst("auto", true));
    CHECK(!mangadetails::newestFirst("auto", false));
    CHECK(mangadetails::newestFirst("newest", false));
    CHECK(!mangadetails::newestFirst("oldest", true));
}

TEST(chapterlist, where_reading_continues)
{
    const std::vector<sources::Chapter> list = {chapter("a", 1), chapter("b", 2, 0, "https://official.example/2"),
                                                chapter("c", 3)};
    CHECK_EQ(mangadetails::continueChapterId(list, nullptr), std::string("a"));
    library::Entry e;
    e.lastReadNumber = 1;
    CHECK_EQ(mangadetails::continueChapterId(list, &e), std::string("c"));   // 2 is on an official site
    e.lastReadNumber = 3;
    CHECK_EQ(mangadetails::continueChapterId(list, &e), std::string("a"));   // all read: from the start
    CHECK_EQ(mangadetails::continueChapterId({}, &e), std::string());
    e.positionChapterId = "a";
    e.positionFraction = 0.4;                                   // a novel left half read
    CHECK_EQ(mangadetails::continueChapterId(list, &e), std::string("a"));
    e.positionFraction = 0.99;
    CHECK_EQ(mangadetails::continueChapterId(list, &e), std::string("a"));   // finished: the next after the last read
}

TEST(chapterlist, loads_details_and_chapters)
{
    fakes::Http http;
    fakes::Scheduler clock;
    sources::SourceRegistry registry;
    registry.add(std::make_unique<weblink::WebLinkSource>(http, clock));
    http.set("https://a.example/s/", "<title>Sun</title><a href='/s/chapter-2/'>2</a><a href='/s/chapter-1/'>1</a>");

    mangadetails::ChapterList list(registry);
    int loadedCalls = 0;
    list.onChaptersLoaded = [&] { ++loadedCalls; };
    list.open("web", "https://a.example/s/");
    CHECK(list.loading());
    http.answerAll();
    clock.advance(1000);              // the page has no cover: the first chapter page is asked for next
    http.answerAll();
    CHECK(!list.loading());
    CHECK(list.error().empty());
    CHECK_EQ(list.details().title, std::string("Sun"));
    CHECK_EQ(list.chapters().size(), std::size_t(2));
    CHECK_EQ(list.chapters()[0].number, std::string("1"));
    CHECK_EQ(loadedCalls, 1);
}

TEST(chapterlist, reports_unknown_sources_and_failures)
{
    fakes::Http http;
    fakes::Scheduler clock;
    sources::SourceRegistry registry;
    registry.add(std::make_unique<weblink::WebLinkSource>(http, clock));
    mangadetails::ChapterList list(registry);
    list.open("nowhere", "x");
    CHECK_EQ(list.error(), std::string("Unknown source \"nowhere\""));
    list.open("web", "https://gone.example/");
    http.answerAll();
    CHECK(!list.loading());
    CHECK(list.error().find("404") != std::string::npos);
    CHECK(!list.chaptersLoaded());
}
