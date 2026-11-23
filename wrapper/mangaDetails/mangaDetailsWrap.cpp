#include "mangaDetailsWrap.h"

#include "appCoreWrap.h"
#include "qmlNetworkWrap.h"
#include "discoverView/recommend.h"

#include <QDateTime>
#include <QVariantMap>

#include <limits>

MangaDetailsWrap::MangaDetailsWrap(QObject* parent) : QObject(parent)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core) return;
    m_list = std::make_unique<mangadetails::ChapterList>(core->sources());
    m_list->onChanged = [this] {
        fillMissingCover();
        keepTags();
        emit changed();
    };
    m_list->onChaptersLoaded = [this] { chaptersArrived(); };
    connect(core, &AppCoreWrap::libraryChanged, this, [this] {
        rebuildRows();
        emit changed();
    });
    connect(core, &AppCoreWrap::settingsChanged, this, [this] {
        rebuildRows();
        emit changed();
    });
}

const library::Entry* MangaDetailsWrap::entry() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core || !m_list) return nullptr;
    return core->library().find(m_list->sourceId(), m_list->mangaId());
}

void MangaDetailsWrap::open(const QString& sourceId, const QString& mangaId)
{
    if (!m_list) return;
    const library::Entry* e = AppCoreWrap::instance()->library().find(sourceId.toStdString(), mangaId.toStdString());
    m_seenBefore = e ? e->seenLatest : std::numeric_limits<double>::max();
    m_rows.clear();
    m_list->open(sourceId.toStdString(), mangaId.toStdString());
}

void MangaDetailsWrap::reload()
{
    if (m_list) open(QString::fromStdString(m_list->sourceId()), QString::fromStdString(m_list->mangaId()));
}

void MangaDetailsWrap::chaptersArrived()
{
    AppCoreWrap* core = AppCoreWrap::instance();
    core->rememberChapters(m_list->sourceId(), m_list->mangaId(), m_list->chapters());
    if (entry()) {
        auto& lib = core->library();
        lib.applyChapters(m_list->sourceId(), m_list->mangaId(), m_list->chapters(), core->scheduler().nowMs());
        if (const library::Entry* e = entry(); e && (m_seenBefore < 0 || m_seenBefore > e->seenLatest))
            m_seenBefore = e->seenLatest;
        lib.markSeen(m_list->sourceId(), m_list->mangaId());
    }
    rebuildRows();
}

void MangaDetailsWrap::rebuildRows()
{
    m_rows.clear();
    if (!m_list || !m_list->chaptersLoaded()) return;
    library::Entry shown;
    const library::Entry* e = entry();
    if (e) shown = *e;
    shown.seenLatest = m_seenBefore;
    AppCoreWrap* core = AppCoreWrap::instance();
    const sources::Source* source = core->sources().find(m_list->sourceId());
    const bool newestFirst = mangadetails::newestFirst(core->settings().chapterOrder, !source || source->updates());
    for (const auto& row : mangadetails::rows(m_list->chapters(), e ? &shown : nullptr, newestFirst)) {
        const auto& c = row.chapter;
        const QString label = QString::fromStdString(row.label);
        m_rows.append(QVariantMap{
            {"id", QString::fromStdString(c.id)},
            {"label", label},
            {"group", QString::fromStdString(c.group)},
            {"date", c.publishedMs > 0 ? QDateTime::fromMSecsSinceEpoch(c.publishedMs).toString("d MMM yyyy") : QString()},
            {"read", row.read},
            {"isNew", row.isNew},
            {"external", !c.externalUrl.empty()},
            {"externalUrl", QString::fromStdString(c.externalUrl)},
        });
    }
}

bool MangaDetailsWrap::newestFirst() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core || !m_list) return true;
    const sources::Source* source = core->sources().find(m_list->sourceId());
    return mangadetails::newestFirst(core->settings().chapterOrder, !source || source->updates());
}

QString MangaDetailsWrap::title() const
{
    if (!m_list) return {};
    if (const library::Entry* e = entry(); e && !e->title.empty()) return QString::fromStdString(e->title);
    return QString::fromStdString(m_list->details().title);
}

QString MangaDetailsWrap::description() const
{
    if (!m_list) return {};
    return QString::fromStdString(m_list->details().description);
}

QString MangaDetailsWrap::coverUrl() const
{
    if (!m_list) return {};
    std::string cover = m_list->details().coverUrl;
    if (cover.empty())
        if (const library::Entry* e = entry()) cover = e->coverUrl;
    if (cover.empty()) return {};
    // A site's images often load only when they seem to come from the site's own page.
    const QString referer = m_list->sourceId() == "web" ? QString::fromStdString(m_list->mangaId()) : QString();
    return QmlNetworkWrap::withReferer(QString::fromStdString(cover), referer).toString(QUrl::FullyEncoded);
}

void MangaDetailsWrap::keepTags()
{
    if (!entry() || m_list->details().tags.empty()) return;
    AppCoreWrap::instance()->library().setTags(m_list->sourceId(), m_list->mangaId(),
                                               discoverview::tagsToKeep(m_list->details().tags));
}

void MangaDetailsWrap::fillMissingCover()
{
    const library::Entry* e = entry();
    if (!e || !e->coverUrl.empty() || m_list->details().coverUrl.empty()) return;
    library::Entry update;
    update.sourceId = e->sourceId;
    update.mangaId = e->mangaId;
    update.coverUrl = m_list->details().coverUrl;
    AppCoreWrap::instance()->library().add(std::move(update));
}

QString MangaDetailsWrap::subtitle() const
{
    return m_list ? QString::fromStdString(m_list->details().subtitle) : QString();
}

QString MangaDetailsWrap::sourceName() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    return core && m_list ? QString::fromStdString(core->sources().nameOf(m_list->sourceId())) : QString();
}

bool MangaDetailsWrap::loading() const
{
    return m_list && m_list->loading();
}

QString MangaDetailsWrap::errorText() const
{
    return m_list ? QString::fromStdString(m_list->error()) : QString("Not available");
}

bool MangaDetailsWrap::inLibrary() const
{
    return entry() != nullptr;
}

QString MangaDetailsWrap::continueChapterId() const
{
    if (!m_list) return {};
    return QString::fromStdString(mangadetails::continueChapterId(m_list->chapters(), entry()));
}

QString MangaDetailsWrap::continueLabel() const
{
    if (!m_list) return {};
    const std::string id = mangadetails::continueChapterId(m_list->chapters(), entry());
    if (id.empty()) return "Read";
    const library::Entry* e = entry();
    const bool halfRead = e && !e->positionChapterId.empty() && e->positionFraction > 0.0;
    if (!e || (e->lastReadNumber < 0 && !halfRead)) return "Start reading";
    for (const auto& c : m_list->chapters())
        if (c.id == id) return "Continue · " + QString::fromStdString(sources::chapterLabel(c));
    return "Continue";
}

void MangaDetailsWrap::toggleLibrary()
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core || !m_list || m_list->mangaId().empty()) return;
    auto& lib = core->library();
    if (entry()) {
        lib.remove(m_list->sourceId(), m_list->mangaId(), core->scheduler().nowMs());
        return;
    }
    library::Entry e;
    e.sourceId = m_list->sourceId();
    e.mangaId = m_list->mangaId();
    e.title = m_list->details().title;
    e.coverUrl = m_list->details().coverUrl;
    e.addedMs = core->scheduler().nowMs();
    e.tags = discoverview::tagsToKeep(m_list->details().tags);
    lib.add(std::move(e));
    if (m_list->chaptersLoaded()) {
        lib.applyChapters(m_list->sourceId(), m_list->mangaId(), m_list->chapters(), core->scheduler().nowMs());
        m_seenBefore = entry()->seenLatest;
        rebuildRows();
        emit changed();
    }
}
