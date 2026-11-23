#pragma once

#include "discoverView/searchView/searchsession.h"
#include "resultListModel.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

// The search view's model: MangaDex search results as a list of
// { sourceId, mangaId, title, coverUrl, subtitle }, and the same as rows in resultModel.
class SearchViewWrap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* resultModel READ resultModel CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString searchedFor READ searchedFor NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY changed)

public:
    explicit SearchViewWrap(QObject* parent = nullptr);
    ~SearchViewWrap() override;

    QVariantList results() const;
    QAbstractItemModel* resultModel() { return &m_resultModel; }
    bool loading() const;
    QString searchedFor() const;
    QString errorText() const;
    bool canLoadMore() const;

    Q_INVOKABLE void search(const QString& query);
    Q_INVOKABLE void loadMore();

signals:
    void changed();

private:
    std::unique_ptr<searchview::SearchSession> m_session;
    QString m_sourceId;
    ResultListModel m_resultModel;
};
