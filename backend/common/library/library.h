#pragma once

#include "common/sources/mangatypes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The user's saved manga and sites, what was read, and what is new.
//
// "New" means chapters that appeared after the manga was last opened (or added): adding a manga
// with 263 chapters shows no badge; chapter 264 arriving shows "1 new" until it is opened.
namespace library {

struct Entry {
    std::string sourceId;               // "mangadex", "web"
    std::string mangaId;                // the source's id (a web link: the chapter list's address)
    std::string title;
    std::string coverUrl;
    std::int64_t addedMs = 0;

    std::vector<double> chapterNumbers; // every numbered chapter known, ascending
    double seenLatest = -1.0;           // the highest chapter number when last opened
    double lastReadNumber = -1.0;       // the last chapter finished
    std::vector<double> readNumbers;    // numbered chapters finished
    std::vector<std::string> readIds;   // chapters without a number, finished

    std::vector<std::int64_t> releaseTimes; // first release of the latest numbered chapters, ascending
    std::int64_t lastCheckedMs = 0;
    std::int64_t nextEstimateMs = 0;    // 0: cannot be estimated
    std::int64_t lastNewMs = 0;         // when a check last found a new chapter
    std::int64_t lastReadMs = 0;        // when the manga was last read (a chapter finished, or a place kept)
    std::vector<std::string> tags;      // the source's genre and theme ids (MangaDex), for "For you"
    std::string positionChapterId;      // the chapter being read ...
    double positionFraction = 0.0;      // ... how far into it (0 start, 1 end) ...
    std::string positionLabel;          // ... its name ("Chapter 12") ...
    double positionNumber = -1.0;       // ... and its number (-1: none), for the progress bar
    std::int64_t positionMs = 0;        // ... and when it was kept (sync keeps the newest)

    bool isRead(const sources::Chapter& chapter) const;
    int newCount() const;
    double latest() const { return chapterNumbers.empty() ? -1.0 : chapterNumbers.back(); }
};

// A manga taken out of the library, remembered so another device's copy does not bring it back.
struct Removal {
    std::string sourceId;
    std::string mangaId;
    std::int64_t removedMs = 0;
};

class Library {
public:
    const std::vector<Entry>& entries() const { return m_entries; }
    const Entry* find(const std::string& sourceId, const std::string& mangaId) const;
    bool contains(const std::string& sourceId, const std::string& mangaId) const
    {
        return find(sourceId, mangaId) != nullptr;
    }

    // Adds an entry, or updates the title and cover of the one already there. Newest first.
    void add(Entry entry);
    // `nowMs` is kept as the time of the removal (0: not kept).
    bool remove(const std::string& sourceId, const std::string& mangaId, std::int64_t nowMs = 0);
    void replaceAll(std::vector<Entry> entries, std::vector<Removal> removals = {});
    const std::vector<Removal>& removals() const { return m_removals; }

    // A fresh chapter list for an entry: remembers the chapter numbers and release times and
    // re-estimates the next release. The first list an entry gets sets what counts as "seen".
    // Returns how many chapter numbers were not known before.
    int applyChapters(const std::string& sourceId, const std::string& mangaId,
                      const std::vector<sources::Chapter>& chapters, std::int64_t nowMs);
    // A check that failed: try again in an hour instead of hammering the site.
    void checkFailed(const std::string& sourceId, const std::string& mangaId, std::int64_t nowMs);
    // The manga was opened: nothing is new any more.
    void markSeen(const std::string& sourceId, const std::string& mangaId);
    void markRead(const std::string& sourceId, const std::string& mangaId, const sources::Chapter& chapter,
                  std::int64_t nowMs);
    void setTags(const std::string& sourceId, const std::string& mangaId, std::vector<std::string> tags);
    // Where reading stopped inside a chapter; saved without counting as "read".
    void setPosition(const std::string& sourceId, const std::string& mangaId, const std::string& chapterId,
                     const std::string& chapterLabel, double chapterNumber, double fraction, std::int64_t nowMs);

    int totalNew() const;

    // Called after every change (to save and to refresh the screens).
    std::function<void()> onChanged;

    static constexpr int releaseTimesKept = 12;

private:
    Entry* findMutable(const std::string& sourceId, const std::string& mangaId);
    void changed();

    std::vector<Entry> m_entries;
    std::vector<Removal> m_removals;
};

} // namespace library
