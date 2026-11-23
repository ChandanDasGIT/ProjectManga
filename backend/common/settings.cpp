#include "common/settings.h"

#include "common/json.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

namespace settings {

std::vector<std::string> Settings::contentRatings() const
{
    std::vector<std::string> out;
    if (safe) out.push_back("safe");
    if (suggestive) out.push_back("suggestive");
    if (erotica) out.push_back("erotica");
    if (out.empty()) out.push_back("safe");
    return out;
}

bool Settings::rating(const std::string& name) const
{
    if (name == "safe") return safe;
    if (name == "suggestive") return suggestive;
    if (name == "erotica") return erotica;
    return false;
}

bool Settings::setRating(const std::string& name, bool on)
{
    bool* flag = name == "safe" ? &safe : name == "suggestive" ? &suggestive : name == "erotica" ? &erotica : nullptr;
    if (!flag) return false;
    const int onCount = int(safe) + int(suggestive) + int(erotica);
    if (!on && *flag && onCount == 1) return true;           // keep at least one
    *flag = on;
    return true;
}

void Settings::setReaderFontSize(int size)
{
    readerFontSize = std::clamp(size, 12, 40);
}

void Settings::setReaderLineSpacing(double spacing)
{
    readerLineSpacing = std::clamp(spacing, 1.0, 2.5);
}

void Settings::setReaderWidth(int width)
{
    readerWidth = std::clamp(width, 480, 1400);
}

void Settings::setReaderTheme(const std::string& theme)
{
    readerTheme = (theme == "sepia" || theme == "light") ? theme : "dark";
}

void Settings::setMangaReaderMode(const std::string& mode)
{
    mangaReaderMode = (mode == "page" || mode == "flip") ? mode : "scroll";
}

void Settings::setChapterOrder(const std::string& order)
{
    chapterOrder = (order == "newest" || order == "oldest") ? order : "auto";
}

void Settings::setFlipSpeed(const std::string& speed)
{
    flipSpeed = (speed == "slow" || speed == "fast" || speed == "veryFast") ? speed : "normal";
}

std::string newDeviceId()
{
    std::random_device device;
    std::mt19937_64 generator((std::uint64_t(device()) << 32) ^ device());
    const char* digits = "0123456789abcdef";
    std::string id;
    for (std::uint64_t bits = generator(); id.size() < 16; bits >>= 4) id += digits[bits & 15];
    return id;
}

Settings load(const std::string& path)
{
    Settings s;
    std::ifstream in(path, std::ios::binary);
    if (!in) return s;
    std::stringstream text;
    text << in.rdbuf();
    const auto parsed = json::parse(text.str());
    if (!parsed.ok()) return s;
    const auto& ratings = parsed.value["contentRatings"];
    s.safe = ratings["safe"].asBool(s.safe);
    s.suggestive = ratings["suggestive"].asBool(s.suggestive);
    s.erotica = ratings["erotica"].asBool(s.erotica);
    if (!s.safe && !s.suggestive && !s.erotica) s.safe = true;
    s.librarySort = parsed.value["librarySort"].asString(s.librarySort);
    s.libraryKind = parsed.value["libraryKind"].asString(s.libraryKind);
    const auto& reader = parsed.value["reader"];
    s.setReaderFontSize(static_cast<int>(reader["fontSize"].asInt(s.readerFontSize)));
    s.setReaderLineSpacing(reader["lineSpacing"].asNumber(s.readerLineSpacing));
    s.setReaderWidth(static_cast<int>(reader["width"].asInt(s.readerWidth)));
    s.setReaderTheme(reader["theme"].asString(s.readerTheme));
    s.setMangaReaderMode(parsed.value["mangaReader"]["mode"].asString(s.mangaReaderMode));
    s.setFlipSpeed(parsed.value["mangaReader"]["flipSpeed"].asString(s.flipSpeed));
    s.setChapterOrder(parsed.value["chapterOrder"].asString(s.chapterOrder));
    s.deviceId = parsed.value["sync"]["deviceId"].asString();
    s.syncEnabled = parsed.value["sync"]["enabled"].asBool(true);
    s.serverWipedAt = parsed.value["sync"]["serverWipedAt"].asInt(-1);
    return s;
}

std::string save(const Settings& s, const std::string& path)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path target(path);
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    if (ec) return "cannot create " + target.parent_path().string() + ": " + ec.message();

    json::Value ratings;
    ratings.set("safe", s.safe);
    ratings.set("suggestive", s.suggestive);
    ratings.set("erotica", s.erotica);
    json::Value doc;
    doc.set("version", 1);
    doc.set("contentRatings", std::move(ratings));
    doc.set("librarySort", s.librarySort);
    doc.set("libraryKind", s.libraryKind);
    json::Value reader;
    reader.set("fontSize", s.readerFontSize);
    reader.set("lineSpacing", s.readerLineSpacing);
    reader.set("width", s.readerWidth);
    reader.set("theme", s.readerTheme);
    doc.set("reader", std::move(reader));
    json::Value mangaReader;
    mangaReader.set("mode", s.mangaReaderMode);
    mangaReader.set("flipSpeed", s.flipSpeed);
    doc.set("mangaReader", std::move(mangaReader));
    doc.set("chapterOrder", s.chapterOrder);
    json::Value sync;
    sync.set("deviceId", s.deviceId);
    sync.set("enabled", s.syncEnabled);
    sync.set("serverWipedAt", s.serverWipedAt);
    doc.set("sync", std::move(sync));

    const fs::path temporary = target.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return "cannot write " + temporary.string();
        out << json::write(doc, true) << '\n';
        if (!out) return "cannot write " + temporary.string();
    }
    fs::rename(temporary, target, ec);
    if (ec) return "cannot replace " + target.string() + ": " + ec.message();
    return {};
}

} // namespace settings
