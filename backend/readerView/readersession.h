#pragma once

#include "common/platform/httpclient.h"
#include "common/sources/sourceregistry.h"

#include <functional>
#include <string>
#include <vector>

// The reader: one chapter's pages, and moving to the chapter before or after it.
namespace readerview {

class ReaderSession {
public:
    explicit ReaderSession(const sources::SourceRegistry& sources);

    // `oldestFirst`: the manga's chapter list, as the details view has it.
    void open(const std::string& sourceId, std::vector<sources::Chapter> oldestFirst, const std::string& chapterId);
    void next();
    void previous();
    void reload();

    bool hasChapter() const { return m_index < m_chapters.size(); }
    const sources::Chapter& chapter() const;
    bool hasNext() const { return hasChapter() && m_index + 1 < m_chapters.size(); }
    bool hasPrevious() const { return hasChapter() && m_index > 0; }
    const sources::PageList& pages() const { return m_pages; }
    bool loading() const { return m_loading; }
    const std::string& error() const { return m_error; }
    // The chapter's pages are on another site; the reader offers to open it instead.
    bool external() const { return hasChapter() && !chapter().externalUrl.empty(); }

    std::function<void()> onChanged;

private:
    void load();
    void changed();

    const sources::SourceRegistry& m_sources;
    std::string m_sourceId;
    std::vector<sources::Chapter> m_chapters;
    std::size_t m_index = 0;
    sources::PageList m_pages;
    bool m_loading = false;
    std::string m_error;
    int m_generation = 0;
    platform::Lifetime m_lifetime;
};

} // namespace readerview
