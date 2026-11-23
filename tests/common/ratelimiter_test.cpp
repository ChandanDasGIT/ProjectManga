#include "testkit.h"
#include "fakes.h"

#include "common/platform/ratelimiter.h"

TEST(ratelimiter, runs_up_to_the_limit_at_once)
{
    fakes::Scheduler clock;
    platform::RateLimiter limiter(clock, 5, 1000);
    int ran = 0;
    for (int i = 0; i < 5; ++i) limiter.run([&] { ++ran; });
    CHECK_EQ(ran, 5);
    CHECK_EQ(limiter.waiting(), std::size_t(0));
}

TEST(ratelimiter, holds_the_rest_until_the_window_moves_on)
{
    fakes::Scheduler clock;
    platform::RateLimiter limiter(clock, 5, 1000);
    std::vector<std::int64_t> startedAt;
    for (int i = 0; i < 12; ++i) limiter.run([&] { startedAt.push_back(clock.now); });
    CHECK_EQ(startedAt.size(), std::size_t(5));
    CHECK_EQ(limiter.waiting(), std::size_t(7));

    clock.advance(999);
    CHECK_EQ(startedAt.size(), std::size_t(5));
    clock.advance(1);
    CHECK_EQ(startedAt.size(), std::size_t(10));
    clock.advance(1000);
    CHECK_EQ(startedAt.size(), std::size_t(12));

    for (std::size_t i = 5; i < startedAt.size(); ++i)          // never more than 5 in any second
        CHECK(startedAt[i] - startedAt[i - 5] >= 1000);
}

TEST(ratelimiter, keeps_the_order)
{
    fakes::Scheduler clock;
    platform::RateLimiter limiter(clock, 1, 100);
    std::vector<int> order;
    for (int i = 0; i < 4; ++i) limiter.run([&order, i] { order.push_back(i); });
    clock.advance(1000);
    CHECK(order == std::vector<int>({0, 1, 2, 3}));
}

TEST(ratelimiter, sets_only_one_timer_while_waiting)
{
    fakes::Scheduler clock;
    platform::RateLimiter limiter(clock, 1, 1000);
    for (int i = 0; i < 10; ++i) limiter.run([] {});
    CHECK_EQ(clock.pendingTimers(), std::size_t(1));
}
