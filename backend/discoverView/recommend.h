#pragma once

#include "common/library/library.h"

#include <set>
#include <string>
#include <vector>

// "For you": a plain guess from tags, not a learning recommender. It takes the genre and theme
// tags that come up most often in the library's MangaDex titles; Discover then lists popular
// titles with all of them that are not in the library yet.
namespace discoverview {

// The tags worth remembering for a library entry: genres and themes (not formats or content warnings).
std::vector<std::string> tagsToKeep(const std::vector<sources::Tag>& tags);

// The `count` most common tags across the entries; ties go to the tag of the most recently added
// entry. Empty when no entry has tags.
std::vector<std::string> topTags(const std::vector<library::Entry>& entries, std::size_t count = 3);

// The manga ids of one source's entries.
std::set<std::string> ownedIds(const std::vector<library::Entry>& entries, const std::string& sourceId);

} // namespace discoverview
