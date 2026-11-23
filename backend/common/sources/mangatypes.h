#pragma once

#include <cstdint>
#include <string>
#include <vector>

// What every source hands back, whatever site it reads.
namespace sources {

struct MangaSummary {
    std::string mangaId;
    std::string title;
    std::string coverUrl;
    std::string subtitle;           // "Ongoing · 2021"
};

struct SearchPage {
    std::vector<MangaSummary> items;
    int total = 0;                  // all matches, not only this page
    int nextOffset = 0;
    std::string listName;           // a list the source chose itself ("Recently added"); empty: the usual one
};

struct Tag {
    std::string id;
    std::string name;               // "Action", "Isekai"
    std::string group;              // "genre", "theme", "format", "content"
};

struct MangaDetails {
    std::string mangaId;
    std::string title;
    std::string description;
    std::string coverUrl;
    std::string subtitle;           // "Author · Ongoing · 2021"
    std::vector<Tag> tags;
};

// What to list when browsing instead of searching.
struct BrowseQuery {
    std::string order = "followedCount";        // also "latestUploadedChapter", "rating", "createdAt"
    std::vector<std::string> includedTags;      // tag ids
    bool anyTag = false;                        // false: all of them, true: at least one
    std::vector<std::string> demographics;      // "shounen", "shoujo", "seinen", "josei"
    std::vector<std::string> contentRatings;    // "safe", "suggestive", "erotica"
    int limit = 24;
};

struct Chapter {
    std::string id;                 // the source's chapter id (a web link: the chapter page's address)
    std::string number;             // as the site writes it: "12", "12.5"; empty for a oneshot
    double numberValue = -1.0;      // for sorting; -1 without a number
    std::string title;
    std::string group;              // who translated it, when known
    std::int64_t publishedMs = 0;   // 0 when unknown
    std::string externalUrl;        // set when the pages live on another site (an official one)
    bool titleOnly = false;         // the number only orders the chapters; show the title alone
};

// A chapter of a novel: its heading and its paragraphs, as plain text.
struct ChapterText {
    std::string title;
    std::vector<std::string> paragraphs;
};

struct PageList {
    std::vector<std::string> urls;
    std::string referer;            // the Referer the image host expects; empty when none
};

// "Chapter 12 · The Title", "Chapter 12", "Oneshot", or the title alone when there is no number.
inline std::string chapterLabel(const Chapter& chapter)
{
    if (chapter.titleOnly && !chapter.title.empty()) return chapter.title;
    if (chapter.number.empty()) return chapter.title.empty() ? "Oneshot" : chapter.title;
    std::string out = "Chapter " + chapter.number;
    if (!chapter.title.empty()) out += " · " + chapter.title;
    return out;
}

// The value, or why there is none.
template <typename T>
struct Result {
    T value{};
    std::string error;
    bool ok() const { return error.empty(); }

    static Result failure(std::string why)
    {
        Result r;
        r.error = std::move(why);
        return r;
    }
};

} // namespace sources
