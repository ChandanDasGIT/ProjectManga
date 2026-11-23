#pragma once

#include "common/library/library.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

// What the library view shows: the entries of the chosen kind whose title contains the typed text,
// in the chosen order.
namespace libraryview {

enum class Sort { Added, Title, MostNew, Updated, Read };

struct SortOption {
    Sort sort;
    const char* key;            // stored in the settings: "added", "title", ...
    const char* label;          // shown to the user
};
const std::vector<SortOption>& sortOptions();
// Unknown keys give Added.
Sort sortFromKey(std::string_view key);

// Which entries show: all, or only manga or only novels (sources whose chapters are text).
enum class Kind { All, Manga, Novel };
// "all", "manga", "novel"; unknown keys give All.
Kind kindFromKey(std::string_view key);
const char* keyOf(Kind kind);

// Title matching ignores case (for ASCII letters) and the spaces around the text. isNovel tells a
// novel source from a manga one (needed unless kind is All).
std::vector<const library::Entry*> arrange(const std::vector<library::Entry>& entries, std::string_view filter, Sort sort,
                                           Kind kind = Kind::All,
                                           const std::function<bool(const std::string& sourceId)>& isNovel = {});

} // namespace libraryview
