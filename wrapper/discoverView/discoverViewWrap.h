#pragma once

#include "discoverView/discoversession.h"
#include "resultListModel.h"

#include <QAbstractListModel>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

// The shelves as rows (roles: title, items, loading, error). A view's shelves stay alive across
// updates: only a shelf whose data changed is told so, and the rows are rebuilt only when the
// set of shelves changes.
class ShelfListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { TitleRole = Qt::UserRole + 1, ItemsRole, LoadingRole, ErrorRole };
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void update(const std::vector<discoverview::Shelf>& shelves);

private:
    struct Row {
        QString key;
        QString title;
        QVariantList items;
        bool loading = false;
        QString error;
    };
    std::vector<Row> m_rows;
};

// The Discover page's model, from MangaDex:
//   shelves      [{ key, title, loading, error, items: [{ sourceId, mangaId, title, coverUrl, subtitle }] }]
//   shelfModel   the same shelves as a ShelfListModel
//   genres, themes [{ id, name, selected }], demographics [{ key, label, selected }]
//   results      the chosen categories' titles (same shape as shelf items); resultModel: the same as a ResultListModel
//   ratings      [{ key, label, on }] - the content filter, kept in the settings
class DiscoverViewWrap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList shelves READ shelves NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* shelfModel READ shelfModel CONSTANT)
    Q_PROPERTY(QVariantList genres READ genres NOTIFY changed)
    Q_PROPERTY(QVariantList themes READ themes NOTIFY changed)
    Q_PROPERTY(QVariantList demographics READ demographics NOTIFY changed)
    Q_PROPERTY(bool tagsLoading READ tagsLoading NOTIFY changed)
    Q_PROPERTY(QString tagsError READ tagsError NOTIFY changed)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY changed)
    Q_PROPERTY(bool browsing READ browsing NOTIFY changed)
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* resultModel READ resultModel CONSTANT)
    Q_PROPERTY(bool resultsLoading READ resultsLoading NOTIFY changed)
    Q_PROPERTY(QString resultsError READ resultsError NOTIFY changed)
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY changed)
    Q_PROPERTY(QVariantList ratings READ ratings NOTIFY changed)

public:
    explicit DiscoverViewWrap(QObject* parent = nullptr);
    ~DiscoverViewWrap() override;

    QVariantList shelves() const;
    QAbstractItemModel* shelfModel() { return &m_shelfModel; }
    QVariantList genres() const { return tagsOf("genre"); }
    QVariantList themes() const { return tagsOf("theme"); }
    QVariantList demographics() const;
    bool tagsLoading() const;
    QString tagsError() const;
    int selectedCount() const;
    bool browsing() const;
    QVariantList results() const;
    QAbstractItemModel* resultModel() { return &m_resultModel; }
    bool resultsLoading() const;
    QString resultsError() const;
    bool canLoadMore() const;
    QVariantList ratings() const;

    // Loads the page the first time it is shown; later calls do nothing.
    Q_INVOKABLE void start();
    Q_INVOKABLE void toggleTag(const QString& tagId);
    Q_INVOKABLE void toggleDemographic(const QString& key);
    Q_INVOKABLE void clearBrowse();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void setRating(const QString& key, bool on);

signals:
    void changed();

private:
    QVariantList tagsOf(const char* group) const;
    void updateForYou();

    std::unique_ptr<discoverview::DiscoverSession> m_session;
    ShelfListModel m_shelfModel;
    ResultListModel m_resultModel;
    bool m_started = false;
};
