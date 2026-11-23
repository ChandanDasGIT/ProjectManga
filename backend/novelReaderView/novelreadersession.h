#pragma once

#include "common/platform/httpclient.h"
#include "common/sources/sourceregistry.h"

#include <functional>
#include <string>
#include <vector>

// The novel reader: one chapter's text, and moving to the chapter before or after it.
namespace novelreaderview {

class NovelReaderSession {
public:
    explicit NovelReaderSession(const sources::SourceRegistry& sources);

    // `oldestFirst`: the book's chapter list, as the details view has it.
    void open(const std::string& sourceId, std::vector<sources::Chapter> oldestFirst, const std::string& chapterId);
    void next();
    void previous();
    void reload();

    bool hasChapter() const { return m_index < m_chapters.size(); }
    const sources::Chapter& chapter() const;
    bool hasNext() const { return hasChapter() && m_index + 1 < m_chapters.size(); }
    bool hasPrevious() const { return hasChapter() && m_index > 0; }
    const sources::ChapterText& text() const { return m_text; }
    bool loading() const { return m_loading; }
    const std::string& error() const { return m_error; }

    std::function<void()> onChanged;

private:
    void load();
    void changed();

    const sources::SourceRegistry& m_sources;
    std::string m_sourceId;
    std::vector<sources::Chapter> m_chapters;
    std::size_t m_index = 0;
    sources::ChapterText m_text;
    bool m_loading = false;
    std::string m_error;
    int m_generation = 0;
    platform::Lifetime m_lifetime;
};

} // namespace novelreaderview
