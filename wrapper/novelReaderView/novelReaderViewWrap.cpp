#include "novelReaderViewWrap.h"

#include "appCoreWrap.h"

NovelReaderViewWrap::NovelReaderViewWrap(QObject* parent) : QObject(parent)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core) return;
    m_session = std::make_unique<novelreaderview::NovelReaderSession>(core->sources());
    m_session->onChanged = [this] { sessionChanged(); };
}

void NovelReaderViewWrap::sessionChanged()
{
    const bool fresh = !m_session->loading() && !m_session->text().paragraphs.empty() && m_session->chapter().id != m_shownChapterId;
    if (fresh) {
        m_shownChapterId = m_session->chapter().id;
        m_startFraction = 0.0;
        AppCoreWrap* core = AppCoreWrap::instance();
        if (const library::Entry* e = core->library().find(m_sourceId, m_mangaId);
            e && e->positionChapterId == m_shownChapterId && e->positionFraction < 0.98)
            m_startFraction = e->positionFraction;
    }
    m_paragraphs.clear();
    for (const auto& p : m_session->text().paragraphs) m_paragraphs.append(QString::fromStdString(p));
    emit changed();
    if (fresh) emit textChanged();
}

void NovelReaderViewWrap::open(const QString& sourceId, const QString& mangaId, const QString& chapterId)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!m_session || !core) return;
    m_sourceId = sourceId.toStdString();
    m_mangaId = mangaId.toStdString();
    m_shownChapterId.clear();
    const auto* chapters = core->chaptersOf(m_sourceId, m_mangaId);
    m_session->open(m_sourceId, chapters ? *chapters : std::vector<sources::Chapter>{}, chapterId.toStdString());
}

void NovelReaderViewWrap::next()
{
    if (m_session) m_session->next();
}

void NovelReaderViewWrap::previous()
{
    if (m_session) m_session->previous();
}

void NovelReaderViewWrap::reload()
{
    if (!m_session) return;
    m_shownChapterId.clear();
    m_session->reload();
}

void NovelReaderViewWrap::savePosition(double fraction)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core || !m_session || !m_session->hasChapter() || m_paragraphs.isEmpty()) return;
    core->library().setPosition(m_sourceId, m_mangaId, m_session->chapter().id, sources::chapterLabel(m_session->chapter()),
                                m_session->chapter().numberValue, fraction, core->scheduler().nowMs());
}

void NovelReaderViewWrap::finished()
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core || !m_session || !m_session->hasChapter() || m_paragraphs.isEmpty()) return;
    core->library().markRead(m_sourceId, m_mangaId, m_session->chapter(), core->scheduler().nowMs());
}

QString NovelReaderViewWrap::chapterLabel() const
{
    if (!m_session || !m_session->hasChapter()) return {};
    return QString::fromStdString(sources::chapterLabel(m_session->chapter()));
}

QString NovelReaderViewWrap::heading() const
{
    if (!m_session) return {};
    return QString::fromStdString(m_session->text().title);
}

bool NovelReaderViewWrap::loading() const
{
    return m_session && m_session->loading();
}

QString NovelReaderViewWrap::errorText() const
{
    return m_session ? QString::fromStdString(m_session->error()) : QString("Not available");
}

bool NovelReaderViewWrap::hasNext() const
{
    return m_session && m_session->hasNext();
}

bool NovelReaderViewWrap::hasPrevious() const
{
    return m_session && m_session->hasPrevious();
}
