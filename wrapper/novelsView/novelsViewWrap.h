#pragma once

#include "common/platform/httpclient.h"
#include "novelsView/novelssession.h"
#include "resultListModel.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

// The Novels page's model: the novel sources to choose from ([{ id, name, selected }]), the chosen
// source's books or search results ([{ sourceId, mangaId, title, coverUrl, subtitle }], and the same
// as rows in resultModel), and adding a novel site by its address.
class NovelsViewWrap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY changed)
    // "Newest releases", "Most downloaded" or "Results for “moon”".
    Q_PROPERTY(QString listTitle READ listTitle NOTIFY changed)
    // What the page says when there is nothing to show ("Loading…", the error, or what was searched).
    Q_PROPERTY(QString emptyText READ emptyText NOTIFY changed)
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* resultModel READ resultModel CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(QString searchedFor READ searchedFor NOTIFY changed)
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY changed)
    Q_PROPERTY(bool adding READ adding NOTIFY changed)
    Q_PROPERTY(QString addError READ addError NOTIFY changed)

public:
    explicit NovelsViewWrap(QObject* parent = nullptr);

    QVariantList sources() const;
    QString sourceName() const;
    QString listTitle() const;
    QString emptyText() const;
    QVariantList results() const;
    QAbstractItemModel* resultModel() { return &m_resultModel; }
    bool loading() const;
    QString errorText() const;
    QString searchedFor() const;
    bool canLoadMore() const;
    bool adding() const { return m_adding; }
    QString addError() const { return m_addError; }

    // Loads the chosen source's list the first time the page is shown.
    Q_INVOKABLE void start();
    Q_INVOKABLE void setSource(const QString& sourceId);
    // Empty: back to the source's own list.
    Q_INVOKABLE void search(const QString& query);
    Q_INVOKABLE void loadMore();
    // A novel site by its address; siteAdded follows on success, else addError says why.
    Q_INVOKABLE void addSite(const QString& name, const QString& address);
    Q_INVOKABLE void clearAddError();

signals:
    void changed();
    void siteAdded();

private:
    void addFailed(const QString& why);

    std::unique_ptr<novelsview::NovelsSession> m_session;
    ResultListModel m_resultModel;
    bool m_adding = false;
    QString m_addError;
    platform::Lifetime m_lifetime;
};
