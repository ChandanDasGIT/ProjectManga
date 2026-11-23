#pragma once

#include "common/library/library.h"

#include <vector>

// Each device keeps its own copy of the library; merging another device's copy into it keeps
// the library the same on all of them.
namespace library {

struct Snapshot {
    std::vector<Entry> entries;
    std::vector<Removal> removals;
};

// Merges another device's copy into `mine`. Returns whether `mine` changed.
//  - read chapters add up; the last chapter read and what counts as seen take the furthest;
//  - the place inside a chapter is the one kept most recently;
//  - chapter lists add up, release estimates come from the latest check;
//  - a manga is in the library unless it was removed after it was added (on any device).
// Merging the same copy twice changes nothing the second time.
bool mergeInto(Snapshot& mine, const Snapshot& theirs);

} // namespace library
