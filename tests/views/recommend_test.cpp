#include "testkit.h"

#include "discoverView/recommend.h"

namespace {
library::Entry entry(const std::string& source, const std::string& id, std::int64_t added, std::vector<std::string> tags)
{
    library::Entry e;
    e.sourceId = source;
    e.mangaId = id;
    e.addedMs = added;
    e.tags = std::move(tags);
    return e;
}
} // namespace

TEST(recommend, keeps_genres_and_themes_only)
{
    const std::vector<sources::Tag> tags = {
        {"g", "Action", "genre"}, {"t", "Isekai", "theme"}, {"f", "Long Strip", "format"}, {"c", "Gore", "content"}};
    CHECK(discoverview::tagsToKeep(tags) == std::vector<std::string>({"g", "t"}));
}

TEST(recommend, most_common_tags_first)
{
    const std::vector<library::Entry> entries = {
        entry("mangadex", "1", 100, {"action", "isekai"}),
        entry("mangadex", "2", 300, {"romance", "isekai"}),
        entry("mangadex", "3", 200, {"action", "isekai", "comedy"}),
        entry("web", "site", 400, {}),
    };
    CHECK(discoverview::topTags(entries, 2) == std::vector<std::string>({"isekai", "action"}));
    // romance and comedy appear once each; romance belongs to the most recently added entry
    CHECK(discoverview::topTags(entries, 3) == std::vector<std::string>({"isekai", "action", "romance"}));
    CHECK(discoverview::topTags({entry("web", "s", 1, {})}).empty());
}

TEST(recommend, owned_ids_per_source)
{
    const std::vector<library::Entry> entries = {entry("mangadex", "1", 0, {}), entry("web", "2", 0, {})};
    CHECK(discoverview::ownedIds(entries, "mangadex") == std::set<std::string>({"1"}));
}
