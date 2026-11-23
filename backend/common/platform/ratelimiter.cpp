#include "common/platform/ratelimiter.h"

#include <algorithm>
#include <utility>

namespace platform {

RateLimiter::RateLimiter(Scheduler& scheduler, int maxStarts, std::int64_t windowMs)
    : m_scheduler(scheduler), m_maxStarts(std::max(1, maxStarts)), m_windowMs(std::max<std::int64_t>(1, windowMs))
{
}

void RateLimiter::run(std::function<void()> job)
{
    m_pending.push_back(std::move(job));
    pump();
}

void RateLimiter::pump()
{
    while (!m_pending.empty()) {
        const std::int64_t now = m_scheduler.nowMs();
        while (!m_starts.empty() && m_starts.front() + m_windowMs <= now) m_starts.pop_front();

        if (static_cast<int>(m_starts.size()) >= m_maxStarts) {
            if (!m_timerSet) {
                m_timerSet = true;
                m_scheduler.after(m_starts.front() + m_windowMs - now, [this] {
                    m_timerSet = false;
                    pump();
                });
            }
            return;
        }

        m_starts.push_back(now);
        auto job = std::move(m_pending.front());
        m_pending.pop_front();
        job();
    }
}

} // namespace platform
