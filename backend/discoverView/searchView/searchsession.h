#pragma once

#include "common/platform/httpclient.h"
#include "common/sources/source.h"

#include <functional>
#include <string>
#include <vector>

// The search view's state: what was searched, the results so far, loading and errors. A new search
// drops the answers still on their way for the old one.
namespace searchview {

class SearchSession {
public:
    explicit SearchSession(sources::Source& source);

    // Starts a new search; an empty (or all-space) query clears the results.
    void search(const std::string& query);
    // Which content ratings to include; a search on screen is run again with them.
    void setContentRatings(std::vector<std::string> ratings);
    // The next page of the same search, added to the end.
    void loadMore();

    const std::string& query() const { return m_query; }
    const std::vector<sources::MangaSummary>& results() const { return m_results; }
    bool loading() const { return m_loading; }
    const std::string& error() const { return m_error; }
    bool canLoadMore() const { return !m_loading && m_error.empty() && m_nextOffset < m_total; }

    // Called after every change.
    std::function<void()> onChanged;

private:
    void fetch();
    void changed();

    sources::Source& m_source;
    std::vector<std::string> m_ratings{"safe"};
    std::string m_query;
    std::vector<sources::MangaSummary> m_results;
    bool m_loading = false;
    std::string m_error;
    int m_total = 0;
    int m_nextOffset = 0;
    int m_generation = 0;
    platform::Lifetime m_lifetime;
};

} // namespace searchview
