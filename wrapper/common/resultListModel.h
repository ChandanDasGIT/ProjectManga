#pragma once

#include "common/sources/mangatypes.h"

#include <QAbstractListModel>
#include <QString>

#include <vector>

// Titles as rows (roles: sourceId, mangaId, title, coverUrl, subtitle). When the new list continues
// the old one (more titles loaded), the new rows are added and the shown ones stay where they are;
// any other change starts the rows over.
class ResultListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { SourceIdRole = Qt::UserRole + 1, MangaIdRole, TitleRole, CoverUrlRole, SubtitleRole };
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void update(const QString& sourceId, const std::vector<sources::MangaSummary>& items);

private:
    QString m_sourceId;
    std::vector<sources::MangaSummary> m_items;
};
