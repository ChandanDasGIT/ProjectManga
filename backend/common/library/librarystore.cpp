#include "common/library/librarystore.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace library {

namespace {

constexpr int formatVersion = 1;

json::Value numbers(const std::vector<double>& values)
{
    json::Value a = json::Value::array();
    for (double v : values) a.push(v);
    return a;
}

json::Value times(const std::vector<std::int64_t>& values)
{
    json::Value a = json::Value::array();
    for (std::int64_t v : values) a.push(v);
    return a;
}

json::Value strings(const std::vector<std::string>& values)
{
    json::Value a = json::Value::array();
    for (const auto& v : values) a.push(v);
    return a;
}

std::vector<double> readNumbers(const json::Value& a)
{
    std::vector<double> out;
    for (const auto& v : a.asArray())
        if (v.isNumber()) out.push_back(v.asNumber());
    return out;
}

std::vector<std::int64_t> readTimes(const json::Value& a)
{
    std::vector<std::int64_t> out;
    for (const auto& v : a.asArray())
        if (v.isNumber()) out.push_back(v.asInt());
    return out;
}

std::vector<std::string> readStrings(const json::Value& a)
{
    std::vector<std::string> out;
    for (const auto& v : a.asArray())
        if (v.isString()) out.push_back(v.asString());
    return out;
}

} // namespace

json::Value toJson(const std::vector<Entry>& entries, const std::vector<Removal>& removals)
{
    json::Value list = json::Value::array();
    for (const auto& e : entries) {
        json::Value o;
        o.set("sourceId", e.sourceId);
        o.set("mangaId", e.mangaId);
        o.set("title", e.title);
        o.set("coverUrl", e.coverUrl);
        o.set("addedMs", e.addedMs);
        o.set("chapterNumbers", numbers(e.chapterNumbers));
        o.set("seenLatest", e.seenLatest);
        o.set("lastReadNumber", e.lastReadNumber);
        o.set("readNumbers", numbers(e.readNumbers));
        o.set("readIds", strings(e.readIds));
        o.set("releaseTimes", times(e.releaseTimes));
        o.set("lastCheckedMs", e.lastCheckedMs);
        o.set("nextEstimateMs", e.nextEstimateMs);
        o.set("lastNewMs", e.lastNewMs);
        o.set("lastReadMs", e.lastReadMs);
        o.set("tags", strings(e.tags));
        o.set("positionChapterId", e.positionChapterId);
        o.set("positionFraction", e.positionFraction);
        o.set("positionLabel", e.positionLabel);
        o.set("positionNumber", e.positionNumber);
        o.set("positionMs", e.positionMs);
        list.push(std::move(o));
    }
    json::Value removed = json::Value::array();
    for (const auto& r : removals) {
        json::Value o;
        o.set("sourceId", r.sourceId);
        o.set("mangaId", r.mangaId);
        o.set("removedMs", r.removedMs);
        removed.push(std::move(o));
    }
    json::Value doc;
    doc.set("version", formatVersion);
    doc.set("entries", std::move(list));
    doc.set("removed", std::move(removed));
    return doc;
}

std::vector<Removal> removalsFromJson(const json::Value& document)
{
    std::vector<Removal> out;
    for (const auto& o : document["removed"].asArray()) {
        Removal r{o["sourceId"].asString(), o["mangaId"].asString(), o["removedMs"].asInt()};
        if (!r.sourceId.empty() && !r.mangaId.empty()) out.push_back(std::move(r));
    }
    return out;
}

std::vector<Entry> fromJson(const json::Value& document)
{
    std::vector<Entry> out;
    for (const auto& o : document["entries"].asArray()) {
        Entry e;
        e.sourceId = o["sourceId"].asString();
        e.mangaId = o["mangaId"].asString();
        if (e.sourceId.empty() || e.mangaId.empty()) continue;
        e.title = o["title"].asString();
        e.coverUrl = o["coverUrl"].asString();
        e.addedMs = o["addedMs"].asInt();
        e.chapterNumbers = readNumbers(o["chapterNumbers"]);
        std::sort(e.chapterNumbers.begin(), e.chapterNumbers.end());
        e.seenLatest = o["seenLatest"].asNumber(-1.0);
        e.lastReadNumber = o["lastReadNumber"].asNumber(-1.0);
        e.readNumbers = readNumbers(o["readNumbers"]);
        e.readIds = readStrings(o["readIds"]);
        e.releaseTimes = readTimes(o["releaseTimes"]);
        e.lastCheckedMs = o["lastCheckedMs"].asInt();
        e.nextEstimateMs = o["nextEstimateMs"].asInt();
        e.lastNewMs = o["lastNewMs"].asInt();
        e.lastReadMs = o["lastReadMs"].asInt();
        e.tags = readStrings(o["tags"]);
        e.positionChapterId = o["positionChapterId"].asString();
        e.positionFraction = o["positionFraction"].asNumber(0.0);
        e.positionLabel = o["positionLabel"].asString();
        e.positionNumber = o["positionNumber"].asNumber(-1.0);
        // Files from before positionMs: the last read time is the best guess.
        e.positionMs = o["positionMs"].asInt(e.positionChapterId.empty() ? 0 : e.lastReadMs);
        out.push_back(std::move(e));
    }
    return out;
}

LoadResult load(const std::string& path)
{
    LoadResult r;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return r;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        r.error = "cannot open " + path;
        return r;
    }
    std::stringstream text;
    text << in.rdbuf();
    const auto parsed = json::parse(text.str());
    if (!parsed.ok()) {
        r.error = path + " is damaged (" + parsed.error + ")";
        return r;
    }
    if (!parsed.value["entries"].isArray()) {
        r.error = path + " is not a library file";
        return r;
    }
    r.entries = fromJson(parsed.value);
    r.removals = removalsFromJson(parsed.value);
    return r;
}

std::string writeWhole(const std::string& text, const std::string& path, const std::string& temporaryName)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path target(path);
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    if (ec) return "cannot create " + target.parent_path().string() + ": " + ec.message();

    const fs::path temporary = target.parent_path() / temporaryName;
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return "cannot write " + temporary.string();
        out << text;
        out.flush();
        if (!out) return "cannot write " + temporary.string();
    }
    fs::rename(temporary, target, ec);
    if (ec) return "cannot replace " + target.string() + ": " + ec.message();
    return {};
}

std::string save(const std::vector<Entry>& entries, const std::vector<Removal>& removals, const std::string& path)
{
    const std::filesystem::path target(path);
    return writeWhole(json::write(toJson(entries, removals), true) + "\n", path, target.filename().string() + ".tmp");
}

std::string save(const std::vector<Entry>& entries, const std::string& path)
{
    return save(entries, {}, path);
}

} // namespace library
