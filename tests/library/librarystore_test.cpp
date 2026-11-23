#include "testkit.h"

#include "common/library/librarystore.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

// A fresh folder under the temporary folder ($TMPDIR), removed at the end of the test.
struct TempFolder {
    std::filesystem::path path;
    TempFolder()
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "projectmanga-test-XXXXXX").string();
        path = mkdtemp(pattern.data());
    }
    ~TempFolder() { std::filesystem::remove_all(path); }
};

library::Entry sample()
{
    library::Entry e;
    e.sourceId = "web";
    e.mangaId = "https://moon.example/manga/?a=1&b=\"2\"";
    e.title = "Moon Knights — ムーン";
    e.coverUrl = "https://moon.example/c.jpg";
    e.addedMs = 1'700'000'000'123;
    e.chapterNumbers = {1, 1.5, 2};
    e.seenLatest = 1.5;
    e.lastReadNumber = 1;
    e.readNumbers = {1};
    e.readIds = {"special"};
    e.releaseTimes = {1'700'000'000'000, 1'700'600'000'000};
    e.lastCheckedMs = 1'700'700'000'000;
    e.nextEstimateMs = 1'701'200'000'000;
    e.lastNewMs = 1'700'650'000'000;
    e.lastReadMs = 1'700'660'000'000;
    e.tags = {"tag-1", "tag-2"};
    e.positionChapterId = "c-12";
    e.positionFraction = 0.4;
    e.positionLabel = "Chapter 12";
    e.positionNumber = 12;
    return e;
}

} // namespace

TEST(librarystore, saves_and_loads_everything)
{
    TempFolder dir;
    const std::string path = (dir.path / "deeper" / "library.json").string();
    CHECK_EQ(library::save({sample()}, path), std::string());
    CHECK(!std::filesystem::exists(path + ".tmp"));

    const auto loaded = library::load(path);
    CHECK(loaded.ok());
    CHECK_EQ(loaded.entries.size(), std::size_t(1));
    const auto& e = loaded.entries[0];
    const auto s = sample();
    CHECK_EQ(e.mangaId, s.mangaId);
    CHECK_EQ(e.title, s.title);
    CHECK_EQ(e.addedMs, s.addedMs);
    CHECK(e.chapterNumbers == s.chapterNumbers);
    CHECK_EQ(e.seenLatest, s.seenLatest);
    CHECK(e.readIds == s.readIds);
    CHECK(e.releaseTimes == s.releaseTimes);
    CHECK_EQ(e.nextEstimateMs, s.nextEstimateMs);
    CHECK_EQ(e.lastNewMs, s.lastNewMs);
    CHECK_EQ(e.lastReadMs, s.lastReadMs);
    CHECK(e.tags == s.tags);
    CHECK_EQ(e.positionChapterId, s.positionChapterId);
    CHECK_EQ(e.positionFraction, s.positionFraction);
    CHECK_EQ(e.positionLabel, s.positionLabel);
    CHECK_EQ(e.positionNumber, s.positionNumber);
}

TEST(librarystore, files_from_before_tags_still_load)
{
    const auto doc = json::parse(R"({"version":1,"entries":[{"sourceId":"web","mangaId":"x","title":"Old"}]})");
    const auto entries = library::fromJson(doc.value);
    CHECK_EQ(entries.size(), std::size_t(1));
    CHECK(entries[0].tags.empty());
    CHECK_EQ(entries[0].lastReadMs, std::int64_t(0));
}

TEST(librarystore, missing_file_is_an_empty_library)
{
    TempFolder dir;
    const auto loaded = library::load((dir.path / "none.json").string());
    CHECK(loaded.ok());
    CHECK(loaded.entries.empty());
}

TEST(librarystore, damaged_file_is_reported)
{
    TempFolder dir;
    const auto path = dir.path / "library.json";
    std::ofstream(path) << "{\"entries\": [";
    const auto loaded = library::load(path.string());
    CHECK(!loaded.ok());
    CHECK(loaded.error.find("damaged") != std::string::npos);
    std::ofstream(path) << "[1,2]";
    CHECK(!library::load(path.string()).ok());
}

TEST(librarystore, skips_entries_without_ids)
{
    const auto doc = json::parse(R"({"version":1,"entries":[{"sourceId":"web"},{"sourceId":"web","mangaId":"x"}]})");
    CHECK_EQ(library::fromJson(doc.value).size(), std::size_t(1));
}

TEST(librarystore, keeps_removals_and_when_the_place_was_kept)
{
    TempFolder dir;
    const std::string path = (dir.path / "library.json").string();
    library::Entry e = sample();
    e.positionMs = 1'700'665'000'000;
    const std::vector<library::Removal> removed = {{"mangadex", "gone-1", 1'700'000'500'000}};
    CHECK_EQ(library::save({e}, removed, path), std::string());
    const auto loaded = library::load(path);
    CHECK(loaded.ok());
    CHECK_EQ(loaded.entries.at(0).positionMs, std::int64_t(1'700'665'000'000));
    CHECK_EQ(loaded.removals.size(), std::size_t(1));
    CHECK_EQ(loaded.removals.at(0).mangaId, std::string("gone-1"));
    CHECK_EQ(loaded.removals.at(0).removedMs, std::int64_t(1'700'000'500'000));

    // Older files: no removals, and the place counts as kept when the manga was last read.
    std::ofstream(path) << R"({"entries":[{"sourceId":"web","mangaId":"m","lastReadMs":5,"positionChapterId":"c"}]})";
    const auto old = library::load(path);
    CHECK(old.removals.empty());
    CHECK_EQ(old.entries.at(0).positionMs, std::int64_t(5));
}
