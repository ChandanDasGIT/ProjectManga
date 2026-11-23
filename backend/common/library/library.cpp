#include "common/library/library.h"

#include "common/library/releaseestimate.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace library {

namespace {
constexpr std::int64_t hourMs = 3'600'000;
constexpr std::int64_t dayMs = 24 * hourMs;
}

bool Entry::isRead(const sources::Chapter& chapter) const
{
    if (chapter.numberValue >= 0)
        return std::find(readNumbers.begin(), readNumbers.end(), chapter.numberValue) != readNumbers.end();
    return std::find(readIds.begin(), readIds.end(), chapter.id) != readIds.end();
}

int Entry::newCount() const
{
    return static_cast<int>(std::count_if(chapterNumbers.begin(), chapterNumbers.end(),
                                          [&](double n) { return n > seenLatest; }));
}

const Entry* Library::find(const std::string& sourceId, const std::string& mangaId) const
{
    for (const auto& e : m_entries)
        if (e.sourceId == sourceId && e.mangaId == mangaId) return &e;
    return nullptr;
}

Entry* Library::findMutable(const std::string& sourceId, const std::string& mangaId)
{
    return const_cast<Entry*>(find(sourceId, mangaId));
}

void Library::add(Entry entry)
{
    if (Entry* existing = findMutable(entry.sourceId, entry.mangaId)) {
        if (!entry.title.empty()) existing->title = entry.title;
        if (!entry.coverUrl.empty()) existing->coverUrl = entry.coverUrl;
    } else {
        std::erase_if(m_removals, [&](const Removal& r) {
            return r.sourceId == entry.sourceId && r.mangaId == entry.mangaId;
        });
        m_entries.insert(m_entries.begin(), std::move(entry));
    }
    changed();
}

bool Library::remove(const std::string& sourceId, const std::string& mangaId, std::int64_t nowMs)
{
    const auto removed = std::erase_if(m_entries, [&](const Entry& e) {
        return e.sourceId == sourceId && e.mangaId == mangaId;
    });
    if (removed == 0) return false;
    if (nowMs > 0) {
        std::erase_if(m_removals, [&](const Removal& r) { return r.sourceId == sourceId && r.mangaId == mangaId; });
        m_removals.push_back({sourceId, mangaId, nowMs});
    }
    changed();
    return true;
}

void Library::replaceAll(std::vector<Entry> entries, std::vector<Removal> removals)
{
    m_entries = std::move(entries);
    m_removals = std::move(removals);
    changed();
}

int Library::applyChapters(const std::string& sourceId, const std::string& mangaId,
                           const std::vector<sources::Chapter>& chapters, std::int64_t nowMs)
{
    Entry* e = findMutable(sourceId, mangaId);
    if (!e) return 0;

    std::map<double, std::int64_t> firstRelease;            // number -> earliest known release (0: unknown)
    for (const auto& c : chapters) {
        if (c.numberValue < 0) continue;
        auto [it, inserted] = firstRelease.try_emplace(c.numberValue, c.publishedMs);
        if (!inserted && c.publishedMs > 0 && (it->second == 0 || c.publishedMs < it->second))
            it->second = c.publishedMs;
    }

    std::vector<double> numbers;
    numbers.reserve(firstRelease.size());
    for (const auto& [n, t] : firstRelease) numbers.push_back(n);

    const bool firstList = e->chapterNumbers.empty() && e->seenLatest < 0;
    int added = 0;
    for (double n : numbers)
        if (!std::binary_search(e->chapterNumbers.begin(), e->chapterNumbers.end(), n)) ++added;
    // A site that briefly lists fewer chapters must not lose the ones already known.
    std::vector<double> all;
    std::set_union(e->chapterNumbers.begin(), e->chapterNumbers.end(), numbers.begin(), numbers.end(),
                   std::back_inserter(all));
    e->chapterNumbers = std::move(all);
    if (firstList) e->seenLatest = e->latest();

    std::vector<std::int64_t> times;
    for (auto it = firstRelease.rbegin(); it != firstRelease.rend() && times.size() < releaseTimesKept; ++it)
        if (it->second > 0) times.push_back(it->second);
    std::sort(times.begin(), times.end());
    e->releaseTimes = std::move(times);
    e->nextEstimateMs = estimateNext(e->releaseTimes, nowMs);
    e->lastCheckedMs = nowMs;
    if (added > 0 && !firstList) e->lastNewMs = nowMs;
    changed();
    return added;
}

void Library::checkFailed(const std::string& sourceId, const std::string& mangaId, std::int64_t nowMs)
{
    Entry* e = findMutable(sourceId, mangaId);
    if (!e) return;
    e->lastCheckedMs = nowMs - dayMs + hourMs;
    changed();
}

void Library::markSeen(const std::string& sourceId, const std::string& mangaId)
{
    Entry* e = findMutable(sourceId, mangaId);
    if (!e || e->seenLatest >= e->latest()) return;
    e->seenLatest = e->latest();
    changed();
}

void Library::markRead(const std::string& sourceId, const std::string& mangaId, const sources::Chapter& chapter,
                       std::int64_t nowMs)
{
    Entry* e = findMutable(sourceId, mangaId);
    if (!e) return;
    e->lastReadMs = nowMs;
    if (chapter.numberValue >= 0) {
        if (!e->isRead(chapter)) e->readNumbers.push_back(chapter.numberValue);
        e->lastReadNumber = chapter.numberValue;
    } else if (!e->isRead(chapter)) {
        e->readIds.push_back(chapter.id);
    }
    changed();
}

void Library::setTags(const std::string& sourceId, const std::string& mangaId, std::vector<std::string> tags)
{
    Entry* e = findMutable(sourceId, mangaId);
    if (!e || e->tags == tags) return;
    e->tags = std::move(tags);
    changed();
}

void Library::setPosition(const std::string& sourceId, const std::string& mangaId, const std::string& chapterId,
                          const std::string& chapterLabel, double chapterNumber, double fraction, std::int64_t nowMs)
{
    Entry* e = findMutable(sourceId, mangaId);
    if (!e) return;
    fraction = std::clamp(fraction, 0.0, 1.0);
    if (e->positionChapterId == chapterId && std::abs(e->positionFraction - fraction) < 0.005) return;
    e->positionChapterId = chapterId;
    e->positionFraction = fraction;
    e->positionLabel = chapterLabel;
    e->positionNumber = chapterNumber;
    e->positionMs = nowMs;
    e->lastReadMs = nowMs;
    changed();
}

int Library::totalNew() const
{
    int total = 0;
    for (const auto& e : m_entries) total += e.newCount();
    return total;
}

void Library::changed()
{
    if (onChanged) onChanged();
}

} // namespace library
