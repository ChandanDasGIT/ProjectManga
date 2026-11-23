#include "testkit.h"

#include "common/settings.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
struct TempFolder {
    std::filesystem::path path;
    TempFolder()
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "projectmanga-test-XXXXXX").string();
        path = mkdtemp(pattern.data());
    }
    ~TempFolder() { std::filesystem::remove_all(path); }
};
} // namespace

TEST(settings, defaults_show_safe_only)
{
    const settings::Settings s;
    CHECK(s.contentRatings() == std::vector<std::string>({"safe"}));
    CHECK_EQ(s.librarySort, std::string("added"));
}

TEST(settings, ratings_on_and_off)
{
    settings::Settings s;
    CHECK(s.setRating("erotica", true));
    CHECK(s.setRating("suggestive", true));
    CHECK(s.contentRatings() == std::vector<std::string>({"safe", "suggestive", "erotica"}));
    s.setRating("safe", false);
    s.setRating("suggestive", false);
    s.setRating("erotica", false);                           // the last one stays on
    CHECK(s.contentRatings() == std::vector<std::string>({"erotica"}));
    CHECK(!s.setRating("pornographic", true));
    CHECK(!s.rating("pornographic"));
}

TEST(settings, saves_and_loads)
{
    TempFolder dir;
    const std::string path = (dir.path / "sub" / "settings.json").string();
    settings::Settings s;
    s.setRating("erotica", true);
    s.librarySort = "title";
    CHECK_EQ(settings::load(path).libraryKind, std::string("all"));
    s.libraryKind = "novel";
    CHECK_EQ(settings::save(s, path), std::string());
    const auto loaded = settings::load(path);
    CHECK_EQ(loaded.libraryKind, std::string("novel"));
    CHECK(loaded.safe);
    CHECK(!loaded.suggestive);
    CHECK(loaded.erotica);
    CHECK_EQ(loaded.librarySort, std::string("title"));
}

TEST(settings, missing_or_damaged_file_gives_defaults)
{
    TempFolder dir;
    CHECK(settings::load((dir.path / "none.json").string()).contentRatings() == std::vector<std::string>({"safe"}));
    const auto path = dir.path / "settings.json";
    std::ofstream(path) << "{ broken";
    CHECK(settings::load(path.string()).safe);
    std::ofstream(path) << R"({"contentRatings":{"safe":false,"suggestive":false,"erotica":false}})";
    CHECK(settings::load(path.string()).safe);               // all off is not allowed
}

TEST(settings, reader_values_stay_in_range)
{
    settings::Settings s;
    s.setReaderFontSize(100);
    s.setReaderLineSpacing(0.2);
    s.setReaderWidth(10);
    s.setReaderTheme("neon");
    CHECK_EQ(s.readerFontSize, 40);
    CHECK_EQ(s.readerLineSpacing, 1.0);
    CHECK_EQ(s.readerWidth, 480);
    CHECK_EQ(s.readerTheme, std::string("dark"));

    TempFolder dir;
    const std::string path = (dir.path / "settings.json").string();
    s.setReaderTheme("sepia");
    s.setReaderFontSize(22);
    settings::save(s, path);
    const auto loaded = settings::load(path);
    CHECK_EQ(loaded.readerTheme, std::string("sepia"));
    CHECK_EQ(loaded.readerFontSize, 22);
}

TEST(settings, manga_reader_mode_and_chapter_order)
{
    settings::Settings s;
    CHECK_EQ(s.mangaReaderMode, std::string("scroll"));
    CHECK_EQ(s.chapterOrder, std::string("auto"));
    CHECK_EQ(s.flipSpeed, std::string("normal"));
    s.setFlipSpeed("slow");
    CHECK_EQ(s.flipSpeed, std::string("slow"));
    s.setFlipSpeed("ludicrous");
    CHECK_EQ(s.flipSpeed, std::string("normal"));
    s.setMangaReaderMode("sideways");
    s.setChapterOrder("random");
    CHECK_EQ(s.mangaReaderMode, std::string("scroll"));
    CHECK_EQ(s.chapterOrder, std::string("auto"));

    TempFolder dir;
    const std::string path = (dir.path / "settings.json").string();
    s.setMangaReaderMode("flip");
    s.setChapterOrder("oldest");
    s.setFlipSpeed("veryFast");
    CHECK_EQ(settings::save(s, path), std::string());
    const auto loaded = settings::load(path);
    CHECK_EQ(loaded.flipSpeed, std::string("veryFast"));
    CHECK_EQ(loaded.mangaReaderMode, std::string("flip"));
    CHECK_EQ(loaded.chapterOrder, std::string("oldest"));

    std::ofstream(path) << R"({"mangaReader":{"mode":"nope","flipSpeed":"warp"},"chapterOrder":7})";
    CHECK_EQ(settings::load(path).flipSpeed, std::string("normal"));
    CHECK_EQ(settings::load(path).mangaReaderMode, std::string("scroll"));
    CHECK_EQ(settings::load(path).chapterOrder, std::string("auto"));
}

TEST(settings, device_id)
{
    const std::string a = settings::newDeviceId();
    const std::string b = settings::newDeviceId();
    CHECK_EQ(a.size(), std::size_t(16));
    CHECK(a != b);
    CHECK(a.find_first_not_of("0123456789abcdef") == std::string::npos);

    TempFolder dir;
    const std::string path = (dir.path / "settings.json").string();
    settings::Settings s;
    s.deviceId = a;
    CHECK_EQ(settings::save(s, path), std::string());
    CHECK_EQ(settings::load(path).deviceId, a);
    CHECK(settings::load(path).syncEnabled);
    CHECK_EQ(settings::load(path).serverWipedAt, std::int64_t(-1));
    s.syncEnabled = false;
    s.serverWipedAt = 1795400000123;
    CHECK_EQ(settings::save(s, path), std::string());
    CHECK(!settings::load(path).syncEnabled);
    CHECK_EQ(settings::load(path).serverWipedAt, std::int64_t(1795400000123));

    // A file from before the sync folder was dropped still loads, and saving drops the folder.
    std::ofstream(path) << R"({"sync":{"folder":"/home/me/Sync/manga","deviceId":"0123456789abcdef"}})";
    const auto old = settings::load(path);
    CHECK_EQ(old.deviceId, std::string("0123456789abcdef"));
    CHECK(old.syncEnabled);
    CHECK_EQ(settings::save(old, path), std::string());
    std::ifstream in(path);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(text.find("folder") == std::string::npos);
}
