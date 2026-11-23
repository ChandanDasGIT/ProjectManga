#include "libraryViewWrap.h"

#include "appCoreWrap.h"
#include "qmlNetworkWrap.h"
#include "libraryView/libraryorder.h"
#include "libraryView/resume.h"

#include <QDateTime>
#include <QVariantMap>

#include <functional>

LibraryViewWrap::LibraryViewWrap(QObject* parent) : QObject(parent)
{
    if (AppCoreWrap* core = AppCoreWrap::instance()) {
        connect(core, &AppCoreWrap::libraryChanged, this, &LibraryViewWrap::changed);
        connect(core, &AppCoreWrap::settingsChanged, this, &LibraryViewWrap::changed);
    }
}

QString LibraryViewWrap::estimateText(qint64 estimateMs, bool hasDates, qint64 nowMs)
{
    if (estimateMs <= 0) return hasDates ? QStringLiteral("unknown") : QString();
    const QString day = QDateTime::fromMSecsSinceEpoch(estimateMs).toString("d MMM");
    return "≈ " + day + (estimateMs < nowMs ? " (estimated, late)" : " (estimated)");
}

QString LibraryViewWrap::coverLink(const library::Entry& e)
{
    if (e.coverUrl.empty()) return {};
    // A site's images often load only when they seem to come from the site's own page.
    const QString referer = e.sourceId == "web" ? QString::fromStdString(e.mangaId) : QString();
    return QmlNetworkWrap::withReferer(QString::fromStdString(e.coverUrl), referer).toString(QUrl::FullyEncoded);
}

namespace {

std::function<bool(const std::string&)> novelTest(AppCoreWrap* core)
{
    return [core](const std::string& sourceId) { return core->isNovelSource(QString::fromStdString(sourceId)); };
}

} // namespace

QVariantList LibraryViewWrap::entries() const
{
    QVariantList list;
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core) return list;
    const qint64 now = core->scheduler().nowMs();
    const auto sort = libraryview::sortFromKey(core->settings().librarySort);
    const auto kind = libraryview::kindFromKey(core->settings().libraryKind);
    for (const library::Entry* entry :
         libraryview::arrange(core->library().entries(), m_filter.toStdString(), sort, kind, novelTest(core))) {
        const library::Entry& e = *entry;
        list.append(QVariantMap{
            {"sourceId", QString::fromStdString(e.sourceId)},
            {"mangaId", QString::fromStdString(e.mangaId)},
            {"title", QString::fromStdString(e.title)},
            {"coverUrl", coverLink(e)},
            {"sourceName", QString::fromStdString(core->sources().nameOf(e.sourceId))},
            {"unreadCount", e.newCount()},
            {"nextEstimate", estimateText(e.nextEstimateMs, !e.releaseTimes.empty(), now)},
            {"started", libraryview::started(e)},
            {"progress", libraryview::bookProgress(e)},
        });
    }
    return list;
}

int LibraryViewWrap::totalCount() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    return core ? static_cast<int>(core->library().entries().size()) : 0;
}

void LibraryViewWrap::setFilterText(const QString& text)
{
    if (text == m_filter) return;
    m_filter = text;
    emit changed();
}

QString LibraryViewWrap::sortKey() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core) return QStringLiteral("added");
    const auto sort = libraryview::sortFromKey(core->settings().librarySort);
    for (const auto& o : libraryview::sortOptions())
        if (o.sort == sort) return QString::fromLatin1(o.key);
    return QStringLiteral("added");
}

void LibraryViewWrap::setSortKey(const QString& key)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core) return;
    const auto sort = libraryview::sortFromKey(key.toStdString());
    for (const auto& o : libraryview::sortOptions())
        if (o.sort == sort) core->setLibrarySort(o.key);
}

QString LibraryViewWrap::kindKey() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    return QString::fromLatin1(libraryview::keyOf(libraryview::kindFromKey(core ? core->settings().libraryKind : "")));
}

void LibraryViewWrap::setKindKey(const QString& key)
{
    if (AppCoreWrap* core = AppCoreWrap::instance())
        core->setLibraryKind(libraryview::keyOf(libraryview::kindFromKey(key.toStdString())));
}

int LibraryViewWrap::kindCount() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core) return 0;
    const auto kind = libraryview::kindFromKey(core->settings().libraryKind);
    return static_cast<int>(
        libraryview::arrange(core->library().entries(), "", libraryview::Sort::Added, kind, novelTest(core)).size());
}

void LibraryViewWrap::remove(const QString& sourceId, const QString& mangaId)
{
    if (AppCoreWrap* core = AppCoreWrap::instance())
        core->library().remove(sourceId.toStdString(), mangaId.toStdString(), core->scheduler().nowMs());
}

QVariantList LibraryViewWrap::sortOptions() const
{
    QVariantList list;
    for (const auto& o : libraryview::sortOptions())
        list.append(QVariantMap{{"key", QString::fromLatin1(o.key)}, {"label", QString::fromUtf8(o.label)}});
    return list;
}

QString LibraryViewWrap::libraryProblem() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    return core ? core->libraryProblem() : QString();
}

void LibraryViewWrap::clearError()
{
    if (m_error.isEmpty()) return;
    m_error.clear();
    emit changed();
}

void LibraryViewWrap::fail(const QString& why)
{
    m_busy = false;
    m_error = why;
    emit changed();
}

void LibraryViewWrap::addSite(const QString& name, const QString& address)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core || m_busy) return;
    const std::string url = address.trimmed().toStdString();
    if (const std::string why = core->siteAdder().check("web", name.toStdString(), url); !why.empty())
        return fail(QString::fromStdString(why));

    m_busy = true;
    m_error.clear();
    emit changed();
    auto alive = m_lifetime.watch();
    core->siteAdder().add("web", name.toStdString(), url, [this, alive, core, url](std::string error, std::vector<sources::Chapter> chapters) {
        if (alive.expired()) return;
        if (!error.empty()) return fail(QString::fromStdString(error));
        core->rememberChapters("web", url, std::move(chapters));
        m_busy = false;
        emit changed();
        emit siteAdded();
    });
}
