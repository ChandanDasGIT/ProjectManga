#include "mangaDetails/chapterlist.h"

#include <algorithm>
#include <map>

namespace mangadetails {

namespace {

bool better(const sources::Chapter& a, const sources::Chapter& b)
{
    const bool aReadable = a.externalUrl.empty();
    const bool bReadable = b.externalUrl.empty();
    if (aReadable != bReadable) return aReadable;
    if (a.publishedMs != b.publishedMs) {
        if (a.publishedMs == 0) return false;
        if (b.publishedMs == 0) return true;
        return a.publishedMs < b.publishedMs;
    }
    return false;
}

} // namespace

std::vector<sources::Chapter> merged(std::vector<sources::Chapter> chapters)
{
    std::vector<sources::Chapter> out;
    std::map<double, std::size_t> byNumber;
    for (auto& c : chapters) {
        if (c.numberValue < 0) {
            out.push_back(std::move(c));
            continue;
        }
        const auto it = byNumber.find(c.numberValue);
        if (it == byNumber.end()) {
            byNumber[c.numberValue] = out.size();
            out.push_back(std::move(c));
        } else if (better(c, out[it->second])) {
            out[it->second] = std::move(c);
        }
    }
    // Numbered chapters by number; the others keep their place relative to the start.
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if (a.numberValue < 0 || b.numberValue < 0) return a.numberValue < 0 && b.numberValue >= 0;
        return a.numberValue < b.numberValue;
    });
    return out;
}

bool newestFirst(const std::string& order, bool sourceUpdates)
{
    if (order == "newest") return true;
    if (order == "oldest") return false;
    return sourceUpdates;
}

std::vector<Row> rows(const std::vector<sources::Chapter>& oldestFirst, const library::Entry* entry, bool newestFirst)
{
    std::vector<Row> out;
    out.reserve(oldestFirst.size());
    for (const auto& c : oldestFirst) {
        Row row;
        row.chapter = c;
        row.label = sources::chapterLabel(c);
        if (entry) {
            row.read = entry->isRead(c);
            row.isNew = c.numberValue >= 0 && c.numberValue > entry->seenLatest;
        }
        out.push_back(std::move(row));
    }
    if (newestFirst) std::reverse(out.begin(), out.end());
    return out;
}

std::string continueChapterId(const std::vector<sources::Chapter>& oldestFirst, const library::Entry* entry)
{
    // A novel left in the middle of a chapter goes on there.
    if (entry && !entry->positionChapterId.empty() && entry->positionFraction < 0.98)
        for (const auto& c : oldestFirst)
            if (c.id == entry->positionChapterId) return c.id;
    std::size_t start = 0;
    if (entry && entry->lastReadNumber >= 0) {
        start = oldestFirst.size();
        for (std::size_t i = 0; i < oldestFirst.size(); ++i)
            if (oldestFirst[i].numberValue > entry->lastReadNumber) { start = i; break; }
        if (start == oldestFirst.size()) start = 0;                // all read: start again
    }
    for (std::size_t i = start; i < oldestFirst.size(); ++i)
        if (oldestFirst[i].externalUrl.empty()) return oldestFirst[i].id;
    return {};
}

ChapterList::ChapterList(const sources::SourceRegistry& sources) : m_sources(sources)
{
}

void ChapterList::open(const std::string& sourceId, const std::string& mangaId)
{
    ++m_generation;
    m_sourceId = sourceId;
    m_mangaId = mangaId;
    m_details = {};
    m_chapters.clear();
    m_chaptersLoaded = false;
    m_error.clear();
    m_pending = 0;

    sources::Source* source = m_sources.find(sourceId);
    if (!source) {
        m_error = "Unknown source \"" + sourceId + "\"";
        changed();
        return;
    }

    m_pending = 2;
    changed();
    const int generation = m_generation;
    auto alive = m_lifetime.watch();
    auto current = [this, generation, alive] { return !alive.expired() && generation == m_generation; };

    source->details(mangaId, [this, current](sources::Result<sources::MangaDetails> r) {
        if (!current()) return;
        --m_pending;
        if (r.ok()) m_details = std::move(r.value);
        else if (m_error.empty()) m_error = r.error;
        changed();
    });
    source->chapters(mangaId, [this, current](sources::Result<std::vector<sources::Chapter>> r) {
        if (!current()) return;
        --m_pending;
        if (r.ok()) {
            m_chapters = merged(std::move(r.value));
            m_chaptersLoaded = true;
            if (onChaptersLoaded) onChaptersLoaded();
        } else {
            m_error = r.error;
        }
        changed();
    });
}

void ChapterList::changed()
{
    if (onChanged) onChanged();
}

} // namespace mangadetails
