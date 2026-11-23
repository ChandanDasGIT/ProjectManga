#include "novelsViewWrap.h"

#include "appCoreWrap.h"

#include <QVariantMap>

NovelsViewWrap::NovelsViewWrap(QObject* parent) : QObject(parent)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core) return;
    m_session = std::make_unique<novelsview::NovelsSession>(core->sources(),
                                                             std::vector<std::string>{"standardebooks", "gutenberg"});
    m_session->onChanged = [this] {
        m_resultModel.update(QString::fromStdString(m_session->sourceId()), m_session->results());
        emit changed();
    };
}

QVariantList NovelsViewWrap::sources() const
{
    QVariantList list;
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!m_session || !core) return list;
    for (const auto& id : m_session->sourceIds())
        list.append(QVariantMap{{"id", QString::fromStdString(id)},
                                {"name", QString::fromStdString(core->sources().nameOf(id))},
                                {"selected", id == m_session->sourceId()}});
    return list;
}

QString NovelsViewWrap::sourceName() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    return core && m_session ? QString::fromStdString(core->sources().nameOf(m_session->sourceId())) : QString();
}

QString NovelsViewWrap::listTitle() const
{
    if (!m_session) return {};
    if (!m_session->query().empty()) return "Results for “" + QString::fromStdString(m_session->query()) + "”";
    if (!m_session->listName().empty()) return QString::fromStdString(m_session->listName());
    return m_session->sourceId() == "gutenberg" ? QStringLiteral("Most downloaded") : QStringLiteral("Newest releases");
}

QString NovelsViewWrap::emptyText() const
{
    if (!m_session) return {};
    if (m_session->loading()) return QStringLiteral("Loading…");
    if (!m_session->error().empty()) return QString::fromStdString(m_session->error());
    return QStringLiteral("Nothing found.");
}

QVariantList NovelsViewWrap::results() const
{
    QVariantList list;
    if (!m_session) return list;
    const QString sourceId = QString::fromStdString(m_session->sourceId());
    for (const auto& m : m_session->results()) {
        list.append(QVariantMap{{"sourceId", sourceId},
                                {"mangaId", QString::fromStdString(m.mangaId)},
                                {"title", QString::fromStdString(m.title)},
                                {"coverUrl", QString::fromStdString(m.coverUrl)},
                                {"subtitle", QString::fromStdString(m.subtitle)}});
    }
    return list;
}

bool NovelsViewWrap::loading() const
{
    return m_session && m_session->loading();
}

QString NovelsViewWrap::errorText() const
{
    return m_session ? QString::fromStdString(m_session->error()) : QString("Novels are not available");
}

QString NovelsViewWrap::searchedFor() const
{
    return m_session ? QString::fromStdString(m_session->query()) : QString();
}

bool NovelsViewWrap::canLoadMore() const
{
    return m_session && m_session->canLoadMore();
}

void NovelsViewWrap::start()
{
    if (m_session) m_session->start();
}

void NovelsViewWrap::setSource(const QString& sourceId)
{
    if (m_session) m_session->setSource(sourceId.toStdString());
}

void NovelsViewWrap::search(const QString& query)
{
    if (m_session) m_session->search(query.trimmed().toStdString());
}

void NovelsViewWrap::loadMore()
{
    if (m_session) m_session->loadMore();
}

void NovelsViewWrap::clearAddError()
{
    if (m_addError.isEmpty()) return;
    m_addError.clear();
    emit changed();
}

void NovelsViewWrap::addFailed(const QString& why)
{
    m_adding = false;
    m_addError = why;
    emit changed();
}

void NovelsViewWrap::addSite(const QString& name, const QString& address)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    if (!core || m_adding) return;
    const std::string url = address.trimmed().toStdString();
    if (const std::string why = core->siteAdder().check("webnovel", name.toStdString(), url); !why.empty())
        return addFailed(QString::fromStdString(why));
    m_adding = true;
    m_addError.clear();
    emit changed();
    auto alive = m_lifetime.watch();
    core->siteAdder().add("webnovel", name.toStdString(), url, [this, alive, core, url](std::string error, std::vector<sources::Chapter> chapters) {
        if (alive.expired()) return;
        if (!error.empty()) return addFailed(QString::fromStdString(error));
        core->rememberChapters("webnovel", url, std::move(chapters));
        m_adding = false;
        emit changed();
        emit siteAdded();
    });
}
