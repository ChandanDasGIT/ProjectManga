#include "common/sources/sourceregistry.h"

#include <algorithm>

namespace sources {

void SourceRegistry::add(std::unique_ptr<Source> source)
{
    if (!source) return;
    const std::string id = source->id();
    std::erase_if(m_sources, [&](const auto& s) { return s->id() == id; });
    m_sources.push_back(std::move(source));
}

Source* SourceRegistry::find(std::string_view id) const
{
    for (const auto& s : m_sources)
        if (s->id() == id) return s.get();
    return nullptr;
}

std::string SourceRegistry::nameOf(std::string_view id) const
{
    const Source* s = find(id);
    return s ? s->name() : std::string(id);
}

} // namespace sources
