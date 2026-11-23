#pragma once

#include "common/sources/source.h"

#include <memory>
#include <string_view>
#include <vector>

// The sources the app knows, found by their id.
namespace sources {

class SourceRegistry {
public:
    // Replaces a source with the same id.
    void add(std::unique_ptr<Source> source);
    Source* find(std::string_view id) const;
    // The user-facing name; the id itself for an unknown source.
    std::string nameOf(std::string_view id) const;

private:
    std::vector<std::unique_ptr<Source>> m_sources;
};

} // namespace sources
