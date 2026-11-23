#pragma once

#include "common/library/library.h"

// How far into a book or series the reader is, for the library's progress bars.
namespace libraryview {

// Something was read (a chapter finished, or a place kept inside one).
bool started(const library::Entry& entry);

// 0 .. 1 through the known chapters: the chapters before the one being read, plus how far into it;
// or, with no place kept, the chapters up to the last one finished. -1 when it cannot be told
// (not started, or no numbered chapters).
double bookProgress(const library::Entry& entry);

} // namespace libraryview
