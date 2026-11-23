#pragma once

#include <cstdint>
#include <vector>

// When the next chapter is likely, from when the last ones came out. Only for a steady rhythm:
// at least 4 releases, a typical gap between 1 and 60 days, and most gaps close to it. It reads
// release dates as the source reports them (MangaDex: when the translation was uploaded).
namespace library {

// releaseTimes: ms since 1970, any order. Returns the likely time (it may already be past: late),
// or 0 when there is no steady rhythm or the series looks paused (more than a full gap overdue).
std::int64_t estimateNext(std::vector<std::int64_t> releaseTimes, std::int64_t nowMs);

} // namespace library
