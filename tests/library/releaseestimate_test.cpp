#include "testkit.h"

#include "common/library/releaseestimate.h"

namespace {
constexpr std::int64_t day = 86'400'000;
constexpr std::int64_t start = 1'700'000'000'000;

std::vector<std::int64_t> every(std::int64_t gap, int count)
{
    std::vector<std::int64_t> out;
    for (int i = 0; i < count; ++i) out.push_back(start + i * gap);
    return out;
}
} // namespace

TEST(releaseestimate, weekly_series)
{
    const auto times = every(7 * day, 6);
    CHECK_EQ(library::estimateNext(times, times.back() + day), times.back() + 7 * day);
}

TEST(releaseestimate, small_wobble_is_still_steady)
{
    std::vector<std::int64_t> times = {start, start + 7 * day, start + 13 * day, start + 21 * day, start + 28 * day + day / 2};
    const std::int64_t next = library::estimateNext(times, start + 29 * day);
    CHECK(next > start + 34 * day && next < start + 37 * day);
}

TEST(releaseestimate, too_few_releases)
{
    CHECK_EQ(library::estimateNext(every(7 * day, 3), start), std::int64_t(0));
    CHECK_EQ(library::estimateNext({}, start), std::int64_t(0));
}

TEST(releaseestimate, irregular_or_bulk_uploads)
{
    std::vector<std::int64_t> bulk = every(60'000, 10);            // ten chapters in ten minutes
    CHECK_EQ(library::estimateNext(bulk, start), std::int64_t(0));
    std::vector<std::int64_t> wild = {start, start + 2 * day, start + 30 * day, start + 31 * day, start + 70 * day};
    CHECK_EQ(library::estimateNext(wild, start + 71 * day), std::int64_t(0));
}

TEST(releaseestimate, late_but_not_paused)
{
    const auto times = every(7 * day, 6);
    CHECK_EQ(library::estimateNext(times, times.back() + 10 * day), times.back() + 7 * day);   // 3 days late
    CHECK_EQ(library::estimateNext(times, times.back() + 15 * day), std::int64_t(0));          // on a break
}

TEST(releaseestimate, uses_only_the_latest_gaps)
{
    std::vector<std::int64_t> times = every(30 * day, 10);          // monthly for a long time ...
    const std::int64_t last = times.back();
    for (int i = 1; i <= 9; ++i) times.push_back(last + i * 7 * day);   // ... then weekly
    CHECK_EQ(library::estimateNext(times, last + 63 * day + day), last + 70 * day);
}
