#include "common/sources/mangaDex/mangadexparse.h"

#include "common/json.h"

#include <algorithm>
#include <charconv>
#include <cctype>

namespace mangadex {

namespace {

using sources::Result;

const std::string unreadable = "MangaDex sent an answer the app cannot read";

// A localized string map ({"en": "...", "ja-ro": "..."}): English first, then romanized
// Japanese, then whatever is there.
std::string pickLocalized(const json::Value& map)
{
    for (const char* lang : {"en", "ja-ro", "en-us"})
        if (const auto& v = map[lang]; v.isString() && !v.asString().empty()) return v.asString();
    for (const auto& [lang, v] : map.asObject())
        if (v.isString() && !v.asString().empty()) return v.asString();
    return {};
}

std::string title(const json::Value& attributes)
{
    const auto& titles = attributes["title"];
    if (const auto& en = titles["en"]; en.isString() && !en.asString().empty()) return en.asString();
    for (const auto& alt : attributes["altTitles"].asArray())
        if (const auto& en = alt["en"]; en.isString() && !en.asString().empty()) return en.asString();
    return pickLocalized(titles);
}

std::string capitalized(std::string s)
{
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

std::string joinDot(std::initializer_list<std::string> parts)
{
    std::string out;
    for (const auto& p : parts) {
        if (p.empty()) continue;
        if (!out.empty()) out += " · ";
        out += p;
    }
    return out;
}

std::string statusAndYear(const json::Value& attributes)
{
    const auto& year = attributes["year"];
    return joinDot({capitalized(attributes["status"].asString()),
                    year.isNumber() ? std::to_string(year.asInt()) : std::string()});
}

const json::Value* relationship(const json::Value& item, std::string_view type)
{
    for (const auto& rel : item["relationships"].asArray())
        if (rel["type"].asString() == type) return &rel;
    return nullptr;
}

std::string cover(const json::Value& item)
{
    const json::Value* art = relationship(item, "cover_art");
    if (!art) return {};
    const std::string file = (*art)["attributes"]["fileName"].asString();
    return file.empty() ? std::string() : coverUrl(item["id"].asString(), file);
}

// Parses the body and checks MangaDex's "result" field.
Result<json::Value> open(std::string_view body)
{
    auto parsed = json::parse(body);
    if (!parsed.ok()) return Result<json::Value>::failure(unreadable);
    if (parsed.value["result"].asString() == "error") {
        const std::string why = errorMessage(body);
        return Result<json::Value>::failure(why.empty() ? "MangaDex reported an error" : "MangaDex: " + why);
    }
    Result<json::Value> r;
    r.value = std::move(parsed.value);
    return r;
}

double numberValue(const std::string& number)
{
    double v = -1.0;
    return json::parseDouble(number, v) == std::errc() ? v : -1.0;
}

int digits(std::string_view s, std::size_t pos, std::size_t count, bool& ok)
{
    int v = 0;
    if (pos + count > s.size()) { ok = false; return 0; }
    for (std::size_t i = pos; i < pos + count; ++i) {
        if (s[i] < '0' || s[i] > '9') { ok = false; return 0; }
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

// Days from 1970-01-01 to a civil date (Howard Hinnant's algorithm).
std::int64_t daysFromCivil(int y, int m, int d)
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<std::int64_t>(era) * 146097 + doe - 719468;
}

sources::Tag tag(const json::Value& item)
{
    sources::Tag t;
    t.id = item["id"].asString();
    t.name = pickLocalized(item["attributes"]["name"]);
    t.group = item["attributes"]["group"].asString();
    return t;
}

} // namespace

std::string coverUrl(std::string_view mangaId, std::string_view fileName)
{
    return "https://uploads.mangadex.org/covers/" + std::string(mangaId) + "/" + std::string(fileName) + ".256.jpg";
}

std::string errorMessage(std::string_view body)
{
    const auto parsed = json::parse(body);
    if (!parsed.ok() || parsed.value["result"].asString() != "error") return {};
    const auto& first = parsed.value["errors"][0];
    std::string why = first["detail"].asString();
    if (why.empty()) why = first["title"].asString();
    return why;
}

std::int64_t parseIsoTime(std::string_view s)
{
    bool ok = true;
    const int year = digits(s, 0, 4, ok);
    const int month = digits(s, 5, 2, ok);
    const int day = digits(s, 8, 2, ok);
    const int hour = digits(s, 11, 2, ok);
    const int minute = digits(s, 14, 2, ok);
    const int second = digits(s, 17, 2, ok);
    if (!ok || s[4] != '-' || s[7] != '-' || (s[10] != 'T' && s[10] != ' ') || s[13] != ':' || s[16] != ':')
        return 0;
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) return 0;

    std::size_t pos = 19;
    if (pos < s.size() && s[pos] == '.')                       // fractions of a second are dropped
        for (++pos; pos < s.size() && s[pos] >= '0' && s[pos] <= '9'; ++pos) {}
    int offsetMinutes = 0;
    if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
        const int sign = s[pos] == '-' ? -1 : 1;
        const int oh = digits(s, pos + 1, 2, ok);
        const bool colon = pos + 3 < s.size() && s[pos + 3] == ':';
        const int om = digits(s, pos + (colon ? 4 : 3), 2, ok);
        if (!ok) return 0;
        offsetMinutes = sign * (oh * 60 + om);
    }
    const std::int64_t days = daysFromCivil(year, month, day);
    const std::int64_t seconds = days * 86400 + hour * 3600 + minute * 60 + second - offsetMinutes * 60;
    return seconds * 1000;
}

Result<sources::SearchPage> parseSearch(std::string_view body)
{
    auto doc = open(body);
    if (!doc.ok()) return Result<sources::SearchPage>::failure(doc.error);
    Result<sources::SearchPage> r;
    for (const auto& item : doc.value["data"].asArray()) {
        const auto& attributes = item["attributes"];
        sources::MangaSummary m;
        m.mangaId = item["id"].asString();
        m.title = title(attributes);
        m.coverUrl = cover(item);
        m.subtitle = statusAndYear(attributes);
        if (!m.mangaId.empty()) r.value.items.push_back(std::move(m));
    }
    r.value.total = static_cast<int>(doc.value["total"].asInt());
    r.value.nextOffset = static_cast<int>(doc.value["offset"].asInt() + doc.value["data"].size());
    return r;
}

Result<sources::MangaDetails> parseManga(std::string_view body)
{
    auto doc = open(body);
    if (!doc.ok()) return Result<sources::MangaDetails>::failure(doc.error);
    const auto& item = doc.value["data"];
    const auto& attributes = item["attributes"];
    if (item["id"].asString().empty()) return Result<sources::MangaDetails>::failure(unreadable);

    Result<sources::MangaDetails> r;
    r.value.mangaId = item["id"].asString();
    r.value.title = title(attributes);
    r.value.coverUrl = cover(item);
    std::string description = pickLocalized(attributes["description"]);
    if (const auto cut = description.find("\n---"); cut != std::string::npos) description.resize(cut);
    while (!description.empty() && std::isspace(static_cast<unsigned char>(description.back()))) description.pop_back();
    r.value.description = description;
    const json::Value* author = relationship(item, "author");
    r.value.subtitle = joinDot({author ? (*author)["attributes"]["name"].asString() : std::string(),
                                statusAndYear(attributes)});
    for (const auto& t : attributes["tags"].asArray())
        if (auto parsed = tag(t); !parsed.id.empty()) r.value.tags.push_back(std::move(parsed));
    return r;
}

Result<FeedPage> parseFeed(std::string_view body)
{
    auto doc = open(body);
    if (!doc.ok()) return Result<FeedPage>::failure(doc.error);
    Result<FeedPage> r;
    for (const auto& item : doc.value["data"].asArray()) {
        const auto& attributes = item["attributes"];
        sources::Chapter c;
        c.id = item["id"].asString();
        if (c.id.empty()) continue;
        c.number = attributes["chapter"].asString();
        c.numberValue = numberValue(c.number);
        c.title = attributes["title"].asString();
        c.publishedMs = parseIsoTime(attributes["publishAt"].asString());
        c.externalUrl = attributes["externalUrl"].asString();
        if (const json::Value* group = relationship(item, "scanlation_group"))
            c.group = (*group)["attributes"]["name"].asString();
        r.value.chapters.push_back(std::move(c));
    }
    r.value.total = static_cast<int>(doc.value["total"].asInt());
    r.value.offset = static_cast<int>(doc.value["offset"].asInt());
    r.value.limit = static_cast<int>(doc.value["limit"].asInt());
    return r;
}

Result<std::vector<sources::Tag>> parseTags(std::string_view body)
{
    auto doc = open(body);
    if (!doc.ok()) return Result<std::vector<sources::Tag>>::failure(doc.error);
    Result<std::vector<sources::Tag>> r;
    for (const auto& item : doc.value["data"].asArray())
        if (auto t = tag(item); !t.id.empty() && !t.name.empty()) r.value.push_back(std::move(t));
    std::sort(r.value.begin(), r.value.end(), [](const auto& a, const auto& b) {
        return a.group != b.group ? a.group < b.group : a.name < b.name;
    });
    return r;
}

Result<sources::PageList> parseAtHome(std::string_view body)
{
    auto doc = open(body);
    if (!doc.ok()) return Result<sources::PageList>::failure(doc.error);
    const std::string base = doc.value["baseUrl"].asString();
    const std::string hash = doc.value["chapter"]["hash"].asString();
    const auto& files = doc.value["chapter"]["data"].asArray();
    if (base.empty() || hash.empty()) return Result<sources::PageList>::failure(unreadable);
    if (files.empty()) return Result<sources::PageList>::failure("This chapter has no pages on MangaDex");

    Result<sources::PageList> r;
    for (const auto& file : files)
        if (file.isString()) r.value.urls.push_back(base + "/data/" + hash + "/" + file.asString());
    return r;
}

} // namespace mangadex
