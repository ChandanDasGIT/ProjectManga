#include "common/library/sync.h"

#include <algorithm>

namespace library {

namespace {

template <typename T>
bool addMissing(std::vector<T>& mine, const std::vector<T>& theirs)
{
    bool changed = false;
    for (const auto& v : theirs)
        if (std::find(mine.begin(), mine.end(), v) == mine.end()) {
            mine.push_back(v);
            changed = true;
        }
    return changed;
}

template <typename T>
bool takeIfBigger(T& mine, const T& theirs)
{
    if (theirs <= mine) return false;
    mine = theirs;
    return true;
}

bool takeIfEmpty(std::string& mine, const std::string& theirs)
{
    if (!mine.empty() || theirs.empty()) return false;
    mine = theirs;
    return true;
}

bool mergeEntry(Entry& m, const Entry& t)
{
    bool changed = false;
    if (t.addedMs > 0 && (m.addedMs == 0 || t.addedMs < m.addedMs)) {
        m.addedMs = t.addedMs;
        changed = true;
    }
    changed |= takeIfEmpty(m.title, t.title);
    changed |= takeIfEmpty(m.coverUrl, t.coverUrl);
    if (m.tags.empty() && !t.tags.empty()) {
        m.tags = t.tags;
        changed = true;
    }

    if (addMissing(m.readNumbers, t.readNumbers)) {
        std::sort(m.readNumbers.begin(), m.readNumbers.end());
        changed = true;
    }
    changed |= addMissing(m.readIds, t.readIds);
    changed |= takeIfBigger(m.lastReadNumber, t.lastReadNumber);
    changed |= takeIfBigger(m.seenLatest, t.seenLatest);
    changed |= takeIfBigger(m.lastReadMs, t.lastReadMs);

    if (addMissing(m.chapterNumbers, t.chapterNumbers)) {
        std::sort(m.chapterNumbers.begin(), m.chapterNumbers.end());
        changed = true;
    }
    if (t.lastCheckedMs > m.lastCheckedMs) {
        m.lastCheckedMs = t.lastCheckedMs;
        m.releaseTimes = t.releaseTimes;
        m.nextEstimateMs = t.nextEstimateMs;
        changed = true;
    }
    changed |= takeIfBigger(m.lastNewMs, t.lastNewMs);

    if (t.positionMs > m.positionMs) {
        m.positionChapterId = t.positionChapterId;
        m.positionFraction = t.positionFraction;
        m.positionLabel = t.positionLabel;
        m.positionNumber = t.positionNumber;
        m.positionMs = t.positionMs;
        changed = true;
    }
    return changed;
}

bool sameManga(const std::string& sourceA, const std::string& mangaA, const std::string& sourceB,
               const std::string& mangaB)
{
    return sourceA == sourceB && mangaA == mangaB;
}

} // namespace

bool mergeInto(Snapshot& mine, const Snapshot& theirs)
{
    bool changed = false;

    for (const auto& r : theirs.removals) {
        auto it = std::find_if(mine.removals.begin(), mine.removals.end(), [&](const Removal& m) {
            return sameManga(m.sourceId, m.mangaId, r.sourceId, r.mangaId);
        });
        if (it == mine.removals.end()) {
            mine.removals.push_back(r);
            changed = true;
        } else {
            changed |= takeIfBigger(it->removedMs, r.removedMs);
        }
    }
    auto removedAt = [&](const std::string& sourceId, const std::string& mangaId) -> std::int64_t {
        for (const auto& r : mine.removals)
            if (sameManga(r.sourceId, r.mangaId, sourceId, mangaId)) return r.removedMs;
        return -1;
    };

    for (const auto& t : theirs.entries) {
        auto it = std::find_if(mine.entries.begin(), mine.entries.end(), [&](const Entry& m) {
            return sameManga(m.sourceId, m.mangaId, t.sourceId, t.mangaId);
        });
        if (it != mine.entries.end()) {
            changed |= mergeEntry(*it, t);
        } else if (t.addedMs > removedAt(t.sourceId, t.mangaId)) {
            mine.entries.push_back(t);
            changed = true;
        }
    }

    // Removed somewhere after it was added: gone here too.
    const auto before = mine.entries.size();
    std::erase_if(mine.entries, [&](const Entry& e) { return removedAt(e.sourceId, e.mangaId) >= e.addedMs; });
    changed |= mine.entries.size() != before;

    // Newest first, as the library keeps them.
    std::stable_sort(mine.entries.begin(), mine.entries.end(),
                     [](const Entry& a, const Entry& b) { return a.addedMs > b.addedMs; });
    return changed;
}

} // namespace library
