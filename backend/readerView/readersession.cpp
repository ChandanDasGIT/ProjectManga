#include "readerView/readersession.h"

namespace readerview {

ReaderSession::ReaderSession(const sources::SourceRegistry& sources) : m_sources(sources)
{
}

const sources::Chapter& ReaderSession::chapter() const
{
    static const sources::Chapter none;
    return hasChapter() ? m_chapters[m_index] : none;
}

void ReaderSession::open(const std::string& sourceId, std::vector<sources::Chapter> oldestFirst,
                         const std::string& chapterId)
{
    m_sourceId = sourceId;
    m_chapters = std::move(oldestFirst);
    m_index = m_chapters.size();
    for (std::size_t i = 0; i < m_chapters.size(); ++i)
        if (m_chapters[i].id == chapterId) { m_index = i; break; }
    load();
}

void ReaderSession::next()
{
    if (!hasNext()) return;
    ++m_index;
    load();
}

void ReaderSession::previous()
{
    if (!hasPrevious()) return;
    --m_index;
    load();
}

void ReaderSession::reload()
{
    load();
}

void ReaderSession::load()
{
    ++m_generation;
    m_pages = {};
    m_error.clear();
    m_loading = false;

    sources::Source* source = m_sources.find(m_sourceId);
    if (!hasChapter()) m_error = "Chapter not found";
    else if (!source) m_error = "Unknown source \"" + m_sourceId + "\"";
    if (!m_error.empty() || external()) {
        changed();
        return;
    }

    m_loading = true;
    changed();
    const int generation = m_generation;
    auto alive = m_lifetime.watch();
    source->pages(chapter(), [this, generation, alive](sources::Result<sources::PageList> r) {
        if (alive.expired() || generation != m_generation) return;
        m_loading = false;
        if (r.ok()) m_pages = std::move(r.value);
        else m_error = r.error;
        changed();
    });
}

void ReaderSession::changed()
{
    if (onChanged) onChanged();
}

} // namespace readerview
