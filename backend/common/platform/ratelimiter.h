#pragma once

#include "common/platform/scheduler.h"

#include <cstdint>
#include <deque>
#include <functional>

// Starts at most `maxStarts` jobs in any `windowMs`; the rest wait in order. Used to stay inside a
// site's request limits (MangaDex: 5 requests a second, 40 a minute for its image servers).
namespace platform {

class RateLimiter {
public:
    RateLimiter(Scheduler& scheduler, int maxStarts, std::int64_t windowMs);

    // Runs `job` now when the limit allows it, else later (jobs keep their order).
    void run(std::function<void()> job);
    std::size_t waiting() const { return m_pending.size(); }

private:
    void pump();

    Scheduler& m_scheduler;
    int m_maxStarts;
    std::int64_t m_windowMs;
    std::deque<std::int64_t> m_starts;
    std::deque<std::function<void()>> m_pending;
    bool m_timerSet = false;
};

} // namespace platform
