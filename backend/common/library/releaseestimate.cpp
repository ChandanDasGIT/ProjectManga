#include "common/library/releaseestimate.h"

#include <algorithm>

namespace library {

std::int64_t estimateNext(std::vector<std::int64_t> releaseTimes, std::int64_t nowMs)
{
    constexpr std::int64_t dayMs = 86'400'000;
    constexpr std::size_t maxGaps = 8;

    std::erase_if(releaseTimes, [](std::int64_t t) { return t <= 0; });
    std::sort(releaseTimes.begin(), releaseTimes.end());
    releaseTimes.erase(std::unique(releaseTimes.begin(), releaseTimes.end()), releaseTimes.end());
    if (releaseTimes.size() < 4) return 0;
    if (releaseTimes.size() > maxGaps + 1) releaseTimes.erase(releaseTimes.begin(), releaseTimes.end() - (maxGaps + 1));

    std::vector<std::int64_t> gaps;
    for (std::size_t i = 1; i < releaseTimes.size(); ++i) gaps.push_back(releaseTimes[i] - releaseTimes[i - 1]);
    std::vector<std::int64_t> sorted = gaps;
    std::sort(sorted.begin(), sorted.end());
    const std::int64_t median = sorted[sorted.size() / 2];
    if (median < dayMs || median > 60 * dayMs) return 0;

    const auto steady = std::count_if(gaps.begin(), gaps.end(), [&](std::int64_t g) {
        return g * 10 >= median * 6 && g * 10 <= median * 16;     // within 0.6x .. 1.6x
    });
    if (steady * 4 < static_cast<std::int64_t>(gaps.size()) * 3) return 0;   // fewer than 3 in 4

    const std::int64_t next = releaseTimes.back() + median;
    if (nowMs > next + median) return 0;
    return next;
}

} // namespace library
