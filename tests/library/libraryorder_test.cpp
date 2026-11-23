#include "testkit.h"

#include "libraryView/libraryorder.h"

namespace {

library::Entry entry(const std::string& title, std::int64_t added, std::int64_t updated = 0, std::int64_t read = 0,
                     int newChapters = 0)
{
    library::Entry e;
    e.sourceId = "web";
    e.mangaId = title;
    e.title = title;
    e.addedMs = added;
    e.lastNewMs = updated;
    e.lastReadMs = read;
    e.seenLatest = 0;
    for (int i = 1; i <= newChapters; ++i) e.chapterNumbers.push_back(i);
    return e;
}

std::string titles(const std::vector<const library::Entry*>& list)
{
    std::string out;
    for (const auto* e : list) out += (out.empty() ? "" : ",") + e->title;
    return out;
}

const std::vector<library::Entry> sample = {
    entry("moon Knights", 300, 0, 50, 1),
    entry("Apple", 100, 900, 0, 0),
    entry("Zebra Moon", 200, 500, 70, 3),
};

} // namespace

TEST(libraryorder, sorts)
{
    using libraryview::Sort;
    CHECK_EQ(titles(libraryview::arrange(sample, "", Sort::Added)), std::string("moon Knights,Zebra Moon,Apple"));
    CHECK_EQ(titles(libraryview::arrange(sample, "", Sort::Title)), std::string("Apple,moon Knights,Zebra Moon"));
    CHECK_EQ(titles(libraryview::arrange(sample, "", Sort::MostNew)), std::string("Zebra Moon,moon Knights,Apple"));
    CHECK_EQ(titles(libraryview::arrange(sample, "", Sort::Updated)), std::string("Apple,Zebra Moon,moon Knights"));
    CHECK_EQ(titles(libraryview::arrange(sample, "", Sort::Read)), std::string("Zebra Moon,moon Knights,Apple"));
}

TEST(libraryorder, filters_by_title)
{
    using libraryview::Sort;
    CHECK_EQ(titles(libraryview::arrange(sample, "  MOON ", Sort::Title)), std::string("moon Knights,Zebra Moon"));
    CHECK_EQ(titles(libraryview::arrange(sample, "pear", Sort::Added)), std::string());
}

TEST(libraryorder, sort_keys)
{
    CHECK(libraryview::sortFromKey("title") == libraryview::Sort::Title);
    CHECK(libraryview::sortFromKey("read") == libraryview::Sort::Read);
    CHECK(libraryview::sortFromKey("nonsense") == libraryview::Sort::Added);
    CHECK_EQ(libraryview::sortOptions().size(), std::size_t(5));
}

TEST(libraryorder, filters_by_kind)
{
    using libraryview::Kind;
    using libraryview::Sort;
    std::vector<library::Entry> mixed = {entry("Moon manga", 300), entry("Holmes", 200), entry("Sun manga", 100)};
    mixed[1].sourceId = "standardebooks";
    const auto isNovel = [](const std::string& sourceId) { return sourceId == "standardebooks"; };
    CHECK_EQ(titles(libraryview::arrange(mixed, "", Sort::Added, Kind::All, isNovel)),
             std::string("Moon manga,Holmes,Sun manga"));
    CHECK_EQ(titles(libraryview::arrange(mixed, "", Sort::Added, Kind::Manga, isNovel)), std::string("Moon manga,Sun manga"));
    CHECK_EQ(titles(libraryview::arrange(mixed, "", Sort::Added, Kind::Novel, isNovel)), std::string("Holmes"));
    // With the title filter and another order.
    CHECK_EQ(titles(libraryview::arrange(mixed, "manga", Sort::Title, Kind::Manga, isNovel)), std::string("Moon manga,Sun manga"));
    CHECK_EQ(titles(libraryview::arrange(mixed, "moon", Sort::Added, Kind::Novel, isNovel)), std::string());

    CHECK(libraryview::kindFromKey("manga") == Kind::Manga);
    CHECK(libraryview::kindFromKey("novel") == Kind::Novel);
    CHECK(libraryview::kindFromKey("all") == Kind::All);
    CHECK(libraryview::kindFromKey("something") == Kind::All);
    CHECK_EQ(std::string(libraryview::keyOf(Kind::Novel)), std::string("novel"));
}
