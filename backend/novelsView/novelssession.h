#pragma once

#include "common/platform/httpclient.h"
#include "common/sources/sourceregistry.h"

#include <functional>
#include <string>
#include <vector>

// The Novels page: the books of the chosen source, either its own list (newest or most read) or
// what a search found, a page at a time.
namespace novelsview {

class NovelsSession {
public:
    NovelsSession(const sources::SourceRegistry& sources, std::vector<std::string> sourceIds);

    const std::vector<std::string>& sourceIds() const { return m_sourceIds; }
    const std::string& sourceId() const { return m_sourceId; }
    // Shows that source's list (or the same search there). Unknown ids are ignored.
    void setSource(const std::string& sourceId);
    // Empty (or spaces): back to the source's own list.
    void search(const std::string& query);
    void loadMore();
    // Loads the first page when nothing is shown yet.
    void start();

    const std::string& query() const { return m_query; }
    // The list the source chose itself, when it did ("Recently added …"); empty otherwise.
    const std::string& listName() const { return m_listName; }
    const std::vector<sources::MangaSummary>& results() const { return m_results; }
    bool loading() const { return m_loading; }
    const std::string& error() const { return m_error; }
    bool canLoadMore() const { return !m_loading && m_error.empty() && m_nextOffset < m_total; }

    std::function<void()> onChanged;

private:
    void restart();
    void fetch();
    void changed();

    const sources::SourceRegistry& m_sources;
    std::vector<std::string> m_sourceIds;
    std::string m_sourceId;
    std::string m_query;
    std::string m_listName;
    std::vector<sources::MangaSummary> m_results;
    bool m_loading = false;
    bool m_started = false;
    std::string m_error;
    int m_total = 0;
    int m_nextOffset = 0;
    int m_generation = 0;
    platform::Lifetime m_lifetime;
};

} // namespace novelsview
