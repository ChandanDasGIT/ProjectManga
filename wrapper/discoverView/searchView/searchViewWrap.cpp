#include "searchViewWrap.h"

#include "appCoreWrap.h"

#include <QVariantMap>

SearchViewWrap::SearchViewWrap(QObject* parent) : QObject(parent), m_sourceId("mangadex")
{
    AppCoreWrap* core = AppCoreWrap::instance();
    sources::Source* source = core ? core->sources().find(m_sourceId.toStdString()) : nullptr;
    if (!source) return;
    m_session = std::make_unique<searchview::SearchSession>(*source);
    m_session->onChanged = [this] {
        m_resultModel.update(m_sourceId, m_session->results());
        emit changed();
    };
    m_session->setContentRatings(core->settings().contentRatings());
    connect(core, &AppCoreWrap::settingsChanged, this, [this, core] {
        m_session->setContentRatings(core->settings().contentRatings());
    });
}

SearchViewWrap::~SearchViewWrap() = default;

QVariantList SearchViewWrap::results() const
{
    QVariantList list;
    if (!m_session) return list;
    for (const auto& r : m_session->results()) {
        list.append(QVariantMap{
            {"sourceId", m_sourceId},
            {"mangaId", QString::fromStdString(r.mangaId)},
            {"title", QString::fromStdString(r.title)},
            {"coverUrl", QString::fromStdString(r.coverUrl)},
            {"subtitle", QString::fromStdString(r.subtitle)},
        });
    }
    return list;
}

bool SearchViewWrap::loading() const
{
    return m_session && m_session->loading();
}

QString SearchViewWrap::searchedFor() const
{
    return m_session ? QString::fromStdString(m_session->query()) : QString();
}

QString SearchViewWrap::errorText() const
{
    if (!m_session) return "Search is not available";
    return QString::fromStdString(m_session->error());
}

bool SearchViewWrap::canLoadMore() const
{
    return m_session && m_session->canLoadMore();
}

void SearchViewWrap::search(const QString& query)
{
    if (m_session) m_session->search(query.toStdString());
}

void SearchViewWrap::loadMore()
{
    if (m_session) m_session->loadMore();
}
