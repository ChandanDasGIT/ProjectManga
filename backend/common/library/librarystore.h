#pragma once

#include "common/json.h"
#include "common/library/library.h"

#include <string>
#include <vector>

// The library on disk: one JSON file, written whole to a temporary file and then renamed, so a
// crash while saving never leaves half a file.
namespace library {

json::Value toJson(const std::vector<Entry>& entries, const std::vector<Removal>& removals = {});
// Entries it cannot read are skipped.
std::vector<Entry> fromJson(const json::Value& document);
std::vector<Removal> removalsFromJson(const json::Value& document);

struct LoadResult {
    std::vector<Entry> entries;
    std::vector<Removal> removals;
    std::string error;              // empty: fine (a missing file is fine: an empty library)
    bool ok() const { return error.empty(); }
};
LoadResult load(const std::string& path);

// Creates the folder when needed. Returns an empty string or why it failed.
std::string save(const std::vector<Entry>& entries, const std::string& path);
std::string save(const std::vector<Entry>& entries, const std::vector<Removal>& removals, const std::string& path);
// Writes `text` to `path` through a temporary file next to it (`temporaryName`), then renames.
std::string writeWhole(const std::string& text, const std::string& path, const std::string& temporaryName);

} // namespace library
