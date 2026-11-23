#include "libraryView/resume.h"

#include <algorithm>

namespace libraryview {

bool started(const library::Entry& e)
{
    return !e.positionChapterId.empty() || e.lastReadNumber >= 0 || !e.readNumbers.empty() || !e.readIds.empty();
}

double bookProgress(const library::Entry& e)
{
    if (!started(e) || e.chapterNumbers.empty()) return -1.0;
    const auto& numbers = e.chapterNumbers;
    const double total = static_cast<double>(numbers.size());
    if (!e.positionChapterId.empty() && e.positionNumber >= 0 && e.positionFraction < 0.98) {
        const auto before = std::lower_bound(numbers.begin(), numbers.end(), e.positionNumber) - numbers.begin();
        return std::clamp((static_cast<double>(before) + e.positionFraction) / total, 0.0, 1.0);
    }
    const double last = std::max(e.lastReadNumber, e.positionNumber);
    if (last < 0) return 0.0;
    const auto done = std::upper_bound(numbers.begin(), numbers.end(), last) - numbers.begin();
    return std::clamp(static_cast<double>(done) / total, 0.0, 1.0);
}

} // namespace libraryview
