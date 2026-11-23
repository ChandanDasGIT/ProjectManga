#include "resultListModel.h"

#include <algorithm>

int ResultListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant ResultListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const auto& m = m_items[static_cast<std::size_t>(index.row())];
    switch (role) {
    case SourceIdRole: return m_sourceId;
    case MangaIdRole: return QString::fromStdString(m.mangaId);
    case TitleRole: return QString::fromStdString(m.title);
    case CoverUrlRole: return QString::fromStdString(m.coverUrl);
    case SubtitleRole: return QString::fromStdString(m.subtitle);
    default: return {};
    }
}

QHash<int, QByteArray> ResultListModel::roleNames() const
{
    return {{SourceIdRole, "sourceId"}, {MangaIdRole, "mangaId"}, {TitleRole, "title"},
            {CoverUrlRole, "coverUrl"}, {SubtitleRole, "subtitle"}};
}

void ResultListModel::update(const QString& sourceId, const std::vector<sources::MangaSummary>& items)
{
    const auto same = [](const sources::MangaSummary& a, const sources::MangaSummary& b) {
        return a.mangaId == b.mangaId && a.title == b.title && a.coverUrl == b.coverUrl && a.subtitle == b.subtitle;
    };
    const bool continues = sourceId == m_sourceId && items.size() >= m_items.size() &&
                           std::equal(m_items.begin(), m_items.end(), items.begin(), same);
    if (!continues) {
        beginResetModel();
        m_sourceId = sourceId;
        m_items = items;
        endResetModel();
        return;
    }
    if (items.size() == m_items.size()) return;
    beginInsertRows({}, static_cast<int>(m_items.size()), static_cast<int>(items.size()) - 1);
    m_items.insert(m_items.end(), items.begin() + static_cast<std::ptrdiff_t>(m_items.size()), items.end());
    endInsertRows();
}
