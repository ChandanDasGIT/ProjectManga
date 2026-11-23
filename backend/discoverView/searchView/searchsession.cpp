#include "discoverView/searchView/searchsession.h"

#include <algorithm>
#include <cctype>

namespace searchview {

namespace {

std::string trimmed(const std::string& s)
{
    auto first = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    auto last = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return first < last ? std::string(first, last) : std::string();
}

} // namespace

SearchSession::SearchSession(sources::Source& source) : m_source(source)
{
}

void SearchSession::search(const std::string& query)
{
    ++m_generation;
    m_query = trimmed(query);
    m_results.clear();
    m_error.clear();
    m_total = 0;
    m_nextOffset = 0;
    m_loading = false;
    if (m_query.empty()) {
        changed();
        return;
    }
    fetch();
}

void SearchSession::setContentRatings(std::vector<std::string> ratings)
{
    if (ratings == m_ratings) return;
    m_ratings = std::move(ratings);
    if (!m_query.empty()) search(m_query);
}

void SearchSession::loadMore()
{
    if (!canLoadMore()) return;
    fetch();
}

void SearchSession::fetch()
{
    m_loading = true;
    changed();
    const int generation = m_generation;
    auto alive = m_lifetime.watch();
    m_source.search(m_query, m_nextOffset, m_ratings, [this, generation, alive](sources::Result<sources::SearchPage> page) {
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
        }
        changed();
    });
}

void SearchSession::changed()
{
    if (onChanged) onChanged();
}

} // namespace searchview
