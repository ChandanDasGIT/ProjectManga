#pragma once

#include "common/library/library.h"
#include "common/platform/httpclient.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// The library view's model: the entries as
// { sourceId, mangaId, title, coverUrl, sourceName, unreadCount, nextEstimate, started, progress }
// (progress: 0 .. 1 through the book, -1 none), of the chosen kind (all, manga or novels), filtered
// by the typed text and in the chosen order (kind and order remembered in the settings); adding a
// site and removing an entry.
class LibraryViewWrap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY changed)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY changed)
    Q_PROPERTY(QString sortKey READ sortKey WRITE setSortKey NOTIFY changed)
    Q_PROPERTY(QVariantList sortOptions READ sortOptions CONSTANT)
    // "all", "manga" or "novel"; kindCount: the entries of that kind, before the typed text.
    Q_PROPERTY(QString kindKey READ kindKey WRITE setKindKey NOTIFY changed)
    Q_PROPERTY(int kindCount READ kindCount NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(QString libraryProblem READ libraryProblem NOTIFY changed)

public:
    explicit LibraryViewWrap(QObject* parent = nullptr);

    QVariantList entries() const;
    int totalCount() const;
    QString filterText() const { return m_filter; }
    void setFilterText(const QString& text);
    QString sortKey() const;
    void setSortKey(const QString& key);
    // [{ key, label }]
    QVariantList sortOptions() const;
    QString kindKey() const;
    void setKindKey(const QString& key);
    int kindCount() const;
    bool busy() const { return m_busy; }
    QString errorText() const { return m_error; }
    QString libraryProblem() const;

    // Reads the page; when it lists chapters, saves it under `name`. siteAdded follows on success,
    // else errorText says why.
    Q_INVOKABLE void addSite(const QString& name, const QString& address);
    Q_INVOKABLE void clearError();
    // Takes the entry out of the library (its reading progress too); other devices follow on sync.
    Q_INVOKABLE void remove(const QString& sourceId, const QString& mangaId);

    // "≈ 28 Nov (estimated)", "≈ 20 Nov (estimated, late)", "unknown" (dated chapters but no steady
    // rhythm) or "" (the source gives no dates).
    static QString estimateText(qint64 estimateMs, bool hasDates, qint64 nowMs);
    // The cover link QML loads (web covers carry the site's page as Referer).
    static QString coverLink(const library::Entry& entry);

signals:
    void changed();
    void siteAdded();

private:
    void fail(const QString& why);

    QString m_filter;
    bool m_busy = false;
    QString m_error;
    platform::Lifetime m_lifetime;
};
