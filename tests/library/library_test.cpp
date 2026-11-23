#include "testkit.h"

#include "common/library/library.h"

namespace {

sources::Chapter chapter(double number, std::int64_t publishedMs = 0)
{
    sources::Chapter c;
    c.number = std::to_string(static_cast<int>(number));
    c.numberValue = number;
    c.id = "c" + std::to_string(number);
    c.publishedMs = publishedMs;
    return c;
}

std::vector<sources::Chapter> chapters(int count)
{
    std::vector<sources::Chapter> out;
    for (int i = 1; i <= count; ++i) out.push_back(chapter(i));
    return out;
}

library::Entry entry(const std::string& id)
{
    library::Entry e;
    e.sourceId = "web";
    e.mangaId = id;
    e.title = "Title " + id;
    return e;
}

} // namespace

TEST(library, add_find_remove)
{
    library::Library lib;
    int changes = 0;
    lib.onChanged = [&] { ++changes; };
    lib.add(entry("a"));
    lib.add(entry("b"));
    CHECK_EQ(lib.entries().size(), std::size_t(2));
    CHECK_EQ(lib.entries()[0].mangaId, std::string("b"));          // newest first
    CHECK(lib.contains("web", "a"));
    CHECK(!lib.contains("mangadex", "a"));

    auto renamed = entry("a");
    renamed.title = "New name";
    lib.add(renamed);
    CHECK_EQ(lib.entries().size(), std::size_t(2));
    CHECK_EQ(lib.find("web", "a")->title, std::string("New name"));

    CHECK(lib.remove("web", "a"));
    CHECK(!lib.remove("web", "a"));
    CHECK_EQ(lib.entries().size(), std::size_t(1));
    CHECK_EQ(changes, 4);
}

TEST(library, first_chapter_list_is_not_new)
{
    library::Library lib;
    lib.add(entry("a"));
    CHECK_EQ(lib.applyChapters("web", "a", chapters(263), 1000), 263);
    CHECK_EQ(lib.find("web", "a")->newCount(), 0);
    CHECK_EQ(lib.totalNew(), 0);
    CHECK_EQ(lib.find("web", "a")->lastCheckedMs, std::int64_t(1000));
}

TEST(library, new_chapters_count_until_opened)
{
    library::Library lib;
    lib.add(entry("a"));
    lib.applyChapters("web", "a", chapters(263), 1000);
    CHECK_EQ(lib.find("web", "a")->lastNewMs, std::int64_t(0));       // the first list is not "new"
    CHECK_EQ(lib.applyChapters("web", "a", chapters(265), 2000), 2);
    CHECK_EQ(lib.find("web", "a")->lastNewMs, std::int64_t(2000));
    CHECK_EQ(lib.find("web", "a")->newCount(), 2);
    CHECK_EQ(lib.totalNew(), 2);
    lib.markSeen("web", "a");
    CHECK_EQ(lib.totalNew(), 0);
}

TEST(library, a_shorter_list_keeps_known_chapters)
{
    library::Library lib;
    lib.add(entry("a"));
    lib.applyChapters("web", "a", chapters(10), 1000);
    CHECK_EQ(lib.applyChapters("web", "a", chapters(3), 2000), 0);
    CHECK_EQ(lib.find("web", "a")->chapterNumbers.size(), std::size_t(10));
}

TEST(library, reading_marks)
{
    library::Library lib;
    lib.add(entry("a"));
    const auto five = chapter(5);
    CHECK(!lib.find("web", "a")->isRead(five));
    lib.markRead("web", "a", five, 5000);
    lib.markRead("web", "a", five, 5000);
    CHECK(lib.find("web", "a")->isRead(five));
    CHECK_EQ(lib.find("web", "a")->readNumbers.size(), std::size_t(1));
    CHECK_EQ(lib.find("web", "a")->lastReadNumber, 5.0);
    CHECK_EQ(lib.find("web", "a")->lastReadMs, std::int64_t(5000));

    sources::Chapter oneshot;
    oneshot.id = "special";
    lib.markRead("web", "a", oneshot, 6000);
    CHECK(lib.find("web", "a")->isRead(oneshot));
    CHECK_EQ(lib.find("web", "a")->lastReadNumber, 5.0);
}

TEST(library, release_times_and_estimate)
{
    constexpr std::int64_t week = 7LL * 86'400'000;
    const std::int64_t start = 1'700'000'000'000;
    std::vector<sources::Chapter> list;
    for (int i = 1; i <= 20; ++i) list.push_back(chapter(i, start + (i - 1) * week));
    auto duplicate = chapter(20, start + 30 * week);          // a later upload of the same chapter
    list.push_back(duplicate);

    library::Library lib;
    lib.add(entry("a"));
    lib.applyChapters("web", "a", list, start + 19 * week + 1000);
    const auto* e = lib.find("web", "a");
    CHECK_EQ(e->releaseTimes.size(), std::size_t(library::Library::releaseTimesKept));
    CHECK_EQ(e->releaseTimes.back(), start + 19 * week);
    CHECK_EQ(e->nextEstimateMs, start + 20 * week);
}

TEST(library, failed_check_retries_in_an_hour)
{
    library::Library lib;
    lib.add(entry("a"));
    lib.checkFailed("web", "a", 100'000'000);
    CHECK_EQ(lib.find("web", "a")->lastCheckedMs, std::int64_t(100'000'000 - 86'400'000 + 3'600'000));
}

TEST(library, tags)
{
    library::Library lib;
    lib.add(entry("a"));
    int changes = 0;
    lib.onChanged = [&] { ++changes; };
    lib.setTags("web", "a", {"t1", "t2"});
    lib.setTags("web", "a", {"t1", "t2"});
    CHECK_EQ(changes, 1);
    CHECK_EQ(lib.find("web", "a")->tags.size(), std::size_t(2));
}

TEST(library, reading_position)
{
    library::Library lib;
    lib.add(entry("a"));
    lib.setPosition("web", "a", "c3", "Chapter 3", 3, 1.7, 900);
    CHECK_EQ(lib.find("web", "a")->positionChapterId, std::string("c3"));
    CHECK_EQ(lib.find("web", "a")->positionLabel, std::string("Chapter 3"));
    CHECK_EQ(lib.find("web", "a")->positionNumber, 3.0);
    CHECK_EQ(lib.find("web", "a")->positionFraction, 1.0);
    CHECK_EQ(lib.find("web", "a")->lastReadMs, std::int64_t(900));
    int changes = 0;
    lib.onChanged = [&] { ++changes; };
    lib.setPosition("web", "a", "c3", "Chapter 3", 3, 0.999, 1000);              // too small a move to save again
    CHECK_EQ(changes, 0);
}
