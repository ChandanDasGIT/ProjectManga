#pragma once

#include "common/library/library.h"
#include "common/platform/httpclient.h"
#include "common/sources/sourceregistry.h"

#include <functional>
#include <string>
#include <vector>

// The details view: a manga's details and its chapter list, one row per chapter, with what the
// library knows about it (read, new since last opened).
namespace mangadetails {

// One chapter per chapter number (several groups may translate the same one): a readable upload
// beats one hosted elsewhere, then the earliest. Chapters without a number are all kept. Oldest first.
std::vector<sources::Chapter> merged(std::vector<sources::Chapter> chapters);

struct Row {
    sources::Chapter chapter;
    std::string label;
    bool read = false;
    bool isNew = false;                 // newer than the last time the manga was opened
};

// For the list on screen: newest first, or in reading order (finished books). `entry` may be null
// (not in the library).
// Whether the list shows newest first: `order` is the saved choice ("auto", "newest", "oldest");
// "auto" follows the source (series newest first, finished books in reading order).
bool newestFirst(const std::string& order, bool sourceUpdates);

std::vector<Row> rows(const std::vector<sources::Chapter>& oldestFirst, const library::Entry* entry, bool newestFirst = true);

// Where "Read" should start: a novel chapter left half read, else the first chapter after the last
// one read, else the first one; external chapters are skipped. Empty when there is nothing to read.
std::string continueChapterId(const std::vector<sources::Chapter>& oldestFirst, const library::Entry* entry);

class ChapterList {
public:
    explicit ChapterList(const sources::SourceRegistry& sources);

    // Loads the details and the chapters; earlier answers still on their way are dropped.
    void open(const std::string& sourceId, const std::string& mangaId);
    void reload() { open(m_sourceId, m_mangaId); }

    const std::string& sourceId() const { return m_sourceId; }
    const std::string& mangaId() const { return m_mangaId; }
    const sources::MangaDetails& details() const { return m_details; }
    const std::vector<sources::Chapter>& chapters() const { return m_chapters; }   // merged, oldest first
    bool loading() const { return m_pending > 0; }
    bool chaptersLoaded() const { return m_chaptersLoaded; }
    const std::string& error() const { return m_error; }

    std::function<void()> onChanged;
    // Called once the chapter list has arrived (to update the library).
    std::function<void()> onChaptersLoaded;

private:
    void changed();

    const sources::SourceRegistry& m_sources;
    std::string m_sourceId;
    std::string m_mangaId;
    sources::MangaDetails m_details;
    std::vector<sources::Chapter> m_chapters;
    bool m_chaptersLoaded = false;
    int m_pending = 0;
    std::string m_error;
    int m_generation = 0;
    platform::Lifetime m_lifetime;
};

} // namespace mangadetails
