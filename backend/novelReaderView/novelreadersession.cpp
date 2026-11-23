#include "novelReaderView/novelreadersession.h"

namespace novelreaderview {

NovelReaderSession::NovelReaderSession(const sources::SourceRegistry& sources) : m_sources(sources)
{
}

const sources::Chapter& NovelReaderSession::chapter() const
{
    static const sources::Chapter none;
    return hasChapter() ? m_chapters[m_index] : none;
}

void NovelReaderSession::open(const std::string& sourceId, std::vector<sources::Chapter> oldestFirst,
                              const std::string& chapterId)
{
    m_sourceId = sourceId;
    m_chapters = std::move(oldestFirst);
    m_index = m_chapters.size();
    for (std::size_t i = 0; i < m_chapters.size(); ++i)
        if (m_chapters[i].id == chapterId) { m_index = i; break; }
    load();
}

void NovelReaderSession::next()
{
    if (!hasNext()) return;
    ++m_index;
    load();
}

void NovelReaderSession::previous()
{
    if (!hasPrevious()) return;
    --m_index;
    load();
}

void NovelReaderSession::reload()
{
    load();
}

void NovelReaderSession::load()
{
    ++m_generation;
    m_text = {};
    m_error.clear();
    m_loading = false;
    sources::Source* source = m_sources.find(m_sourceId);
    if (!hasChapter()) m_error = "Chapter not found";
    else if (!source) m_error = "Unknown source \"" + m_sourceId + "\"";
    if (!m_error.empty()) {
        changed();
        return;
    }
    m_loading = true;
    changed();
    const int generation = m_generation;
    auto alive = m_lifetime.watch();
    source->chapterText(chapter(), [this, generation, alive](sources::Result<sources::ChapterText> r) {
        if (alive.expired() || generation != m_generation) return;
        m_loading = false;
        if (r.ok()) m_text = std::move(r.value);
        else m_error = r.error;
        changed();
    });
}

void NovelReaderSession::changed()
{
    if (onChanged) onChanged();
}

} // namespace novelreaderview
