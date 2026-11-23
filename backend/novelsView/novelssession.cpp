#include "novelsView/novelssession.h"

#include <algorithm>

namespace novelsview {

namespace {
std::string trimmed(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
} // namespace

NovelsSession::NovelsSession(const sources::SourceRegistry& sources, std::vector<std::string> sourceIds)
    : m_sources(sources), m_sourceIds(std::move(sourceIds))
{
    if (!m_sourceIds.empty()) m_sourceId = m_sourceIds.front();
}

void NovelsSession::start()
{
    if (m_started) return;
    m_started = true;
    restart();
}

void NovelsSession::setSource(const std::string& sourceId)
{
    if (sourceId == m_sourceId || std::find(m_sourceIds.begin(), m_sourceIds.end(), sourceId) == m_sourceIds.end()) return;
    m_sourceId = sourceId;
    m_started = true;
    restart();
}

void NovelsSession::search(const std::string& query)
{
    m_query = trimmed(query);
    m_started = true;
    restart();
}

void NovelsSession::loadMore()
{
    if (canLoadMore()) fetch();
}

void NovelsSession::restart()
{
    ++m_generation;
    m_results.clear();
    m_error.clear();
    m_total = 0;
    m_nextOffset = 0;
    m_loading = false;
    m_listName.clear();
    fetch();
}

void NovelsSession::fetch()
{
    sources::Source* source = m_sources.find(m_sourceId);
    if (!source) {
        m_error = "No novel source is available";
        changed();
        return;
    }
    m_loading = true;
    changed();
    const int generation = m_generation;
    auto alive = m_lifetime.watch();
    auto take = [this, alive, generation](sources::Result<sources::SearchPage> page) {
        if (alive.expired() || generation != m_generation) return;
        m_loading = false;
        if (!page.ok()) {
            m_error = page.error;
        } else {
            for (auto& item : page.value.items) {
                const bool seen = std::any_of(m_results.begin(), m_results.end(),
                                              [&](const auto& r) { return r.mangaId == item.mangaId; });
                if (!seen) m_results.push_back(std::move(item));
            }
            m_total = page.value.total;
            m_nextOffset = std::max(page.value.nextOffset, m_nextOffset + 1);
            m_listName = page.value.listName;
        }
        changed();
    };
    if (m_query.empty()) source->browse({}, m_nextOffset, std::move(take));
    else source->search(m_query, m_nextOffset, {}, std::move(take));
}

void NovelsSession::changed()
{
    if (onChanged) onChanged();
}

} // namespace novelsview
