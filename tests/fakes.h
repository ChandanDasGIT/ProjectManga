#pragma once

#include "common/platform/httpclient.h"
#include "common/platform/scheduler.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Stand-ins for the host: a clock the test moves by hand, and a web whose answers the test sets.
namespace fakes {

class Scheduler : public platform::Scheduler {
public:
    std::int64_t now = 1'700'000'000'000;            // 2023-11-14

    std::int64_t nowMs() const override { return now; }
    void after(std::int64_t delayMs, std::function<void()> fn) override
    {
        m_timers.push_back({now + std::max<std::int64_t>(0, delayMs), m_nextOrder++, std::move(fn)});
    }

    // Moves the clock forward, running every timer that falls due on the way, in order.
    void advance(std::int64_t ms)
    {
        const std::int64_t end = now + ms;
        while (true) {
            auto due = std::min_element(m_timers.begin(), m_timers.end(), [](const auto& a, const auto& b) {
                return a.at != b.at ? a.at < b.at : a.order < b.order;
            });
            if (due == m_timers.end() || due->at > end) break;
            now = std::max(now, due->at);
            auto fn = std::move(due->fn);
            m_timers.erase(due);
            fn();
        }
        now = end;
    }
    void post(std::function<void()> fn) override
    {
        std::lock_guard<std::mutex> lock(m_postMutex);
        m_posted.push_back(std::move(fn));
    }
    // Runs what other threads posted (the tests' stand-in for the event loop).
    std::size_t runPosted()
    {
        std::vector<std::function<void()>> posted;
        {
            std::lock_guard<std::mutex> lock(m_postMutex);
            posted.swap(m_posted);
        }
        for (auto& fn : posted) fn();
        return posted.size();
    }
    std::size_t pendingTimers() const { return m_timers.size(); }
    // The time the earliest timer fires; -1 when there is none.
    std::int64_t nextTimerAt() const
    {
        std::int64_t best = -1;
        for (const auto& t : m_timers)
            if (best < 0 || t.at < best) best = t.at;
        return best;
    }

private:
    struct Timer {
        std::int64_t at;
        int order;
        std::function<void()> fn;
    };
    std::vector<Timer> m_timers;
    int m_nextOrder = 0;
    std::mutex m_postMutex;
    std::vector<std::function<void()>> m_posted;
};

// Requests wait until the test calls answerAll(); unknown URLs answer 404.
class Http : public platform::HttpClient {
public:
    std::map<std::string, platform::HttpResponse> pages;
    std::vector<platform::HttpRequest> requests;

    void set(const std::string& url, std::string body, int status = 200)
    {
        platform::HttpResponse r;
        r.status = status;
        r.body = std::move(body);
        pages[url] = std::move(r);
    }

    void get(const platform::HttpRequest& request, std::function<void(platform::HttpResponse)> done) override
    {
        requests.push_back(request);
        m_waiting.push_back({request.url, std::move(done)});
    }

    // Answers everything asked so far, and what those answers ask in turn, until nothing waits.
    void answerAll()
    {
        while (!m_waiting.empty()) {
            auto waiting = std::move(m_waiting);
            m_waiting.clear();
            for (auto& [url, done] : waiting) {
                const auto it = pages.find(url);
                if (it != pages.end()) {
                    done(it->second);
                } else {
                    platform::HttpResponse notFound;
                    notFound.status = 404;
                    done(notFound);
                }
            }
        }
    }
    std::size_t waitingCount() const { return m_waiting.size(); }
    bool asked(const std::string& url) const
    {
        return std::any_of(requests.begin(), requests.end(), [&](const auto& r) { return r.url == url; });
    }
    std::string header(std::size_t request, const std::string& name) const
    {
        for (const auto& [k, v] : requests.at(request).headers)
            if (k == name) return v;
        return {};
    }

private:
    std::vector<std::pair<std::string, std::function<void(platform::HttpResponse)>>> m_waiting;
};

} // namespace fakes
