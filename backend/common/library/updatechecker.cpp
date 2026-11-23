#include "common/library/updatechecker.h"

#include <algorithm>

namespace library {

UpdateChecker::UpdateChecker(Library& library, const sources::SourceRegistry& sources, platform::Scheduler& scheduler)
    : m_library(library), m_sources(sources), m_scheduler(scheduler)
{
}

std::int64_t UpdateChecker::dueAt(const Entry& entry)
{
    if (entry.lastCheckedMs <= 0) return 0;
    std::int64_t due = entry.lastCheckedMs + dayMs;
    if (entry.nextEstimateMs > entry.lastCheckedMs) due = std::min(due, entry.nextEstimateMs);
    return due;
}

void UpdateChecker::start()
{
    m_started = true;
    runDue();
}

void UpdateChecker::reschedule()
{
    if (m_started) runDue();
}

void UpdateChecker::runDue()
{
    const std::int64_t now = m_scheduler.nowMs();
    std::int64_t earliest = -1;
    for (const auto& e : m_library.entries()) {
        const auto key = std::make_pair(e.sourceId, e.mangaId);
        if (std::find(m_queue.begin(), m_queue.end(), key) != m_queue.end()) continue;
        if (const sources::Source* s = m_sources.find(e.sourceId); s && !s->updates()) continue;
        const std::int64_t due = dueAt(e);
        if (due <= now) m_queue.push_back(key);
        else if (earliest < 0 || due < earliest) earliest = due;
    }

    ++m_timerGeneration;
    if (earliest >= 0) {
        const int generation = m_timerGeneration;
        auto alive = m_lifetime.watch();
        m_scheduler.after(earliest - now, [this, generation, alive] {
            if (!alive.expired() && generation == m_timerGeneration) runDue();
        });
    }
    checkNext();
}

void UpdateChecker::checkNext()
{
    if (m_busy) return;
    while (!m_queue.empty()) {
        const auto [sourceId, mangaId] = m_queue.front();
        m_queue.pop_front();
        sources::Source* source = m_sources.find(sourceId);
        if (!source || !m_library.contains(sourceId, mangaId)) continue;

        m_busy = true;
        auto alive = m_lifetime.watch();
        source->chapters(mangaId, [this, alive, sourceId, mangaId](sources::Result<std::vector<sources::Chapter>> r) {
            if (alive.expired()) return;
            if (r.ok()) m_library.applyChapters(sourceId, mangaId, r.value, m_scheduler.nowMs());
            else m_library.checkFailed(sourceId, mangaId, m_scheduler.nowMs());
            m_busy = false;
            if (m_queue.empty()) runDue();     // sets the next timer from the fresh dates
            else checkNext();
        });
        return;
    }
}

} // namespace library
