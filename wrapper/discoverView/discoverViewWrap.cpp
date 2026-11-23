#include "discoverViewWrap.h"

#include "appCoreWrap.h"
#include "discoverView/recommend.h"

#include <QVariantMap>

#include <algorithm>

namespace {

const QString sourceId = QStringLiteral("mangadex");

QVariantList summaries(const std::vector<sources::MangaSummary>& items)
{
    QVariantList list;
    for (const auto& m : items) {
        list.append(QVariantMap{
            {"sourceId", sourceId},
            {"mangaId", QString::fromStdString(m.mangaId)},
            {"title", QString::fromStdString(m.title)},
            {"coverUrl", QString::fromStdString(m.coverUrl)},
            {"subtitle", QString::fromStdString(m.subtitle)},
        });
    }
    return list;
}

} // namespace

int ShelfListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ShelfListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const Row& r = m_rows[static_cast<std::size_t>(index.row())];
    switch (role) {
    case TitleRole: return r.title;
    case ItemsRole: return r.items;
    case LoadingRole: return r.loading;
    case ErrorRole: return r.error;
    default: return {};
    }
}

QHash<int, QByteArray> ShelfListModel::roleNames() const
{
    return {{TitleRole, "title"}, {ItemsRole, "items"}, {LoadingRole, "loading"}, {ErrorRole, "error"}};
}

void ShelfListModel::update(const std::vector<discoverview::Shelf>& shelves)
{
    std::vector<Row> rows;
    rows.reserve(shelves.size());
    for (const auto& s : shelves)
        rows.push_back({QString::fromStdString(s.key), QString::fromStdString(s.title), summaries(s.items), s.loading,
                        QString::fromStdString(s.error)});

    const bool sameShelves = std::equal(rows.begin(), rows.end(), m_rows.begin(), m_rows.end(),
                                        [](const Row& a, const Row& b) { return a.key == b.key; });
    if (!sameShelves) {
        beginResetModel();
        m_rows = std::move(rows);
        endResetModel();
        return;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        Row& old = m_rows[i];
        Row& now = rows[i];
        QList<int> roles;
        if (old.title != now.title) roles << TitleRole;
        if (old.items != now.items) roles << ItemsRole;
        if (old.loading != now.loading) roles << LoadingRole;
        if (old.error != now.error) roles << ErrorRole;
        if (roles.isEmpty()) continue;
        old = std::move(now);
        const QModelIndex at = index(static_cast<int>(i));
        emit dataChanged(at, at, roles);
    }
}

DiscoverViewWrap::DiscoverViewWrap(QObject* parent) : QObject(parent)
{
    AppCoreWrap* core = AppCoreWrap::instance();
    sources::Source* source = core ? core->sources().find(sourceId.toStdString()) : nullptr;
    if (!source) return;
    m_session = std::make_unique<discoverview::DiscoverSession>(*source);
    connect(this, &DiscoverViewWrap::changed, this, [this] {
        m_shelfModel.update(m_session->shelves());
        m_resultModel.update(sourceId, m_session->results());
    });
    m_session->onChanged = [this] { emit changed(); };
    m_session->setContentRatings(core->settings().contentRatings());
    updateForYou();
    m_shelfModel.update(m_session->shelves());
    connect(core, &AppCoreWrap::settingsChanged, this, [this, core] {
        m_session->setContentRatings(core->settings().contentRatings());
        emit changed();
    });
    connect(core, &AppCoreWrap::libraryChanged, this, &DiscoverViewWrap::updateForYou);
}

void DiscoverViewWrap::start()
{
    if (m_started || !m_session) return;
    m_started = true;
    m_session->load();
}

DiscoverViewWrap::~DiscoverViewWrap() = default;

void DiscoverViewWrap::updateForYou()
{
    const auto& entries = AppCoreWrap::instance()->library().entries();
    m_session->setForYou(discoverview::topTags(entries, 2), discoverview::ownedIds(entries, sourceId.toStdString()));
}

QVariantList DiscoverViewWrap::shelves() const
{
    QVariantList list;
    if (!m_session) return list;
    for (const auto& s : m_session->shelves()) {
        list.append(QVariantMap{
            {"key", QString::fromStdString(s.key)},
            {"title", QString::fromStdString(s.title)},
            {"loading", s.loading},
            {"error", QString::fromStdString(s.error)},
            {"items", summaries(s.items)},
        });
    }
    return list;
}

QVariantList DiscoverViewWrap::tagsOf(const char* group) const
{
    QVariantList list;
    if (!m_session) return list;
    for (const auto& t : m_session->tags()) {
        if (t.group != group) continue;
        list.append(QVariantMap{
            {"id", QString::fromStdString(t.id)},
            {"name", QString::fromStdString(t.name)},
            {"selected", m_session->tagSelected(t.id)},
        });
    }
    return list;
}

QVariantList DiscoverViewWrap::demographics() const
{
    QVariantList list;
    for (const auto& d : discoverview::demographics()) {
        list.append(QVariantMap{
            {"key", QString::fromLatin1(d.key)},
            {"label", QString::fromLatin1(d.label)},
            {"selected", m_session && m_session->demographicSelected(d.key)},
        });
    }
    return list;
}

bool DiscoverViewWrap::tagsLoading() const
{
    return m_session && m_session->tagsLoading();
}

QString DiscoverViewWrap::tagsError() const
{
    return m_session ? QString::fromStdString(m_session->tagsError()) : QString("Discover is not available");
}

int DiscoverViewWrap::selectedCount() const
{
    if (!m_session) return 0;
    int count = 0;
    for (const auto& t : m_session->tags()) count += m_session->tagSelected(t.id) ? 1 : 0;
    for (const auto& d : discoverview::demographics()) count += m_session->demographicSelected(d.key) ? 1 : 0;
    return count;
}

bool DiscoverViewWrap::browsing() const
{
    return m_session && m_session->browsing();
}

QVariantList DiscoverViewWrap::results() const
{
    return m_session ? summaries(m_session->results()) : QVariantList();
}

bool DiscoverViewWrap::resultsLoading() const
{
    return m_session && m_session->resultsLoading();
}

QString DiscoverViewWrap::resultsError() const
{
    return m_session ? QString::fromStdString(m_session->resultsError()) : QString();
}

bool DiscoverViewWrap::canLoadMore() const
{
    return m_session && m_session->canLoadMore();
}

QVariantList DiscoverViewWrap::ratings() const
{
    AppCoreWrap* core = AppCoreWrap::instance();
    QVariantList list;
    if (!core) return list;
    const auto& s = core->settings();
    for (const auto& [key, label] : {std::pair{"safe", "Safe"}, {"suggestive", "Suggestive"}, {"erotica", "Erotica"}})
        list.append(QVariantMap{{"key", key}, {"label", label}, {"on", s.rating(key)}});
    return list;
}

void DiscoverViewWrap::toggleTag(const QString& tagId)
{
    if (m_session) m_session->toggleTag(tagId.toStdString());
}

void DiscoverViewWrap::toggleDemographic(const QString& key)
{
    if (m_session) m_session->toggleDemographic(key.toStdString());
}

void DiscoverViewWrap::clearBrowse()
{
    if (m_session) m_session->clearBrowse();
}

void DiscoverViewWrap::loadMore()
{
    if (m_session) m_session->loadMore();
}

void DiscoverViewWrap::reload()
{
    if (m_session) m_session->load();
}

void DiscoverViewWrap::setRating(const QString& key, bool on)
{
    if (AppCoreWrap* core = AppCoreWrap::instance()) core->setRating(key, on);
}
