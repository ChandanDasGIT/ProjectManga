#include "readerViewWrap.h"

#include "appCoreWrap.h"
#include "qmlNetworkWrap.h"

ReaderViewWrap::ReaderViewWrap(QObject* parent) : QObject(parent)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core) return;
    m_session = std::make_unique<readerview::ReaderSession>(core->sources());
    m_session->onChanged = [this] { sessionChanged(); };
}

ReaderViewWrap::~ReaderViewWrap() = default;

void ReaderViewWrap::sessionChanged()
{
    m_pages.clear();
    const auto& list = m_session->pages();
    const QString referer = QString::fromStdString(list.referer);
    for (const auto& url : list.urls)
        m_pages.append(QmlNetworkWrap::withReferer(QString::fromStdString(url), referer).toString(QUrl::FullyEncoded));

    const std::string id = m_session->chapter().id;
    const bool otherChapter = id != m_shownChapterId;
    m_shownChapterId = id;
    if (otherChapter) {
        m_startFraction = 0.0;
        AppCoreWrap* core = AppCoreWrap::instance();
        if (m_openAtEnd)
            m_startFraction = 1.0;
        else if (const library::Entry* e = core->library().find(m_sourceId, m_mangaId);
                 e && e->positionChapterId == id && e->positionFraction < 0.98)
            m_startFraction = e->positionFraction;
        m_openAtEnd = false;
    }
    emit changed();
    if (otherChapter) emit chapterChanged();
}

void ReaderViewWrap::open(const QString& sourceId, const QString& mangaId, const QString& chapterId)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!m_session || !core) return;
    m_sourceId = sourceId.toStdString();
    m_mangaId = mangaId.toStdString();
    const auto* chapters = core->chaptersOf(m_sourceId, m_mangaId);
    m_session->open(m_sourceId, chapters ? *chapters : std::vector<sources::Chapter>{}, chapterId.toStdString());
}

void ReaderViewWrap::next()
{
    if (m_session) m_session->next();
}

void ReaderViewWrap::previous()
{
    if (m_session) m_session->previous();
}

void ReaderViewWrap::previousAtEnd()
{
    if (!m_session || !m_session->hasPrevious()) return;
    m_openAtEnd = true;
    m_session->previous();
}

void ReaderViewWrap::reload()
{
    if (m_session) m_session->reload();
}

void ReaderViewWrap::savePosition(double fraction)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core || !m_session || !m_session->hasChapter() || m_session->pages().urls.empty()) return;
    core->library().setPosition(m_sourceId, m_mangaId, m_session->chapter().id, sources::chapterLabel(m_session->chapter()),
                                m_session->chapter().numberValue, fraction, core->scheduler().nowMs());
}

void ReaderViewWrap::finished()
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core || !m_session || !m_session->hasChapter() || m_session->pages().urls.empty()) return;
    core->library().markRead(m_sourceId, m_mangaId, m_session->chapter(), core->scheduler().nowMs());
}

QString ReaderViewWrap::chapterLabel() const
{
    if (!m_session || !m_session->hasChapter()) return {};
    return QString::fromStdString(sources::chapterLabel(m_session->chapter()));
}

bool ReaderViewWrap::loading() const
{
    return m_session && m_session->loading();
}

QString ReaderViewWrap::errorText() const
{
    return m_session ? QString::fromStdString(m_session->error()) : QString("Not available");
}

bool ReaderViewWrap::external() const
{
    return m_session && m_session->external();
}

QString ReaderViewWrap::externalUrl() const
{
    return m_session ? QString::fromStdString(m_session->chapter().externalUrl) : QString();
}

bool ReaderViewWrap::hasNext() const
{
    return m_session && m_session->hasNext();
}

bool ReaderViewWrap::hasPrevious() const
{
    return m_session && m_session->hasPrevious();
}
