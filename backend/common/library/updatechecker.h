#pragma once

#include "common/library/library.h"
#include "common/platform/httpclient.h"
#include "common/platform/scheduler.h"
#include "common/sources/sourceregistry.h"

#include <deque>
#include <functional>
#include <string>
#include <utility>

// Looks for new chapters of everything in the library: each entry once a day, and once more at
// its estimated release time when it has one. Entries are checked one after another.
namespace library {

class UpdateChecker {
public:
    UpdateChecker(Library& library, const sources::SourceRegistry& sources, platform::Scheduler& scheduler);

    // Checks what is due now and keeps itself scheduled from then on.
    void start();
    // The library changed (an entry added): look again at what is due when.
    void reschedule();

    bool checking() const { return m_busy || !m_queue.empty(); }

    // When an entry should be checked next (ms since 1970); 0: now.
    static std::int64_t dueAt(const Entry& entry);

    static constexpr std::int64_t dayMs = 86'400'000;

private:
    void runDue();
    void checkNext();

    Library& m_library;
    const sources::SourceRegistry& m_sources;
    platform::Scheduler& m_scheduler;
    std::deque<std::pair<std::string, std::string>> m_queue;   // (sourceId, mangaId)
    bool m_busy = false;
    bool m_started = false;
    int m_timerGeneration = 0;
    platform::Lifetime m_lifetime;
};

} // namespace library
