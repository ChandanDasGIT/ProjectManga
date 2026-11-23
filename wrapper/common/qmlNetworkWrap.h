#pragma once

#include <QMutex>
#include <QNetworkAccessManager>
#include <QQmlNetworkAccessManagerFactory>
#include <QString>
#include <QUrl>

// The network QML's Image uses for covers and pages: the app's User-Agent, a disk cache, and the
// Referer header some image hosts insist on. The reader asks for it by adding the referring page
// to the image link (withReferer); the part after '#' never reaches the server.
class QmlNetworkWrap : public QQmlNetworkAccessManagerFactory {
public:
    // cacheFolder: where images are kept between runs (each network instance has its own subfolder).
    explicit QmlNetworkWrap(QString cacheFolder);

    QNetworkAccessManager* create(QObject* parent) override;

    static QUrl withReferer(const QString& imageUrl, const QString& referer);

    // QML's Image logs a line for every cover or page that cannot be fetched (a site down, no
    // network). The screen already says so ("No cover", "Page 3 did not load"), so those lines are
    // left out of the log; every other message still goes through.
    static void quietImageLoadErrors();
    static bool isImageLoadError(const QString& message);

private:
    QString m_cacheFolder;
    QMutex m_mutex;
    int m_created = 0;
};

// Exposed for the tests: the request a QML image link turns into.
class ImageNetwork : public QNetworkAccessManager {
public:
    using QNetworkAccessManager::QNetworkAccessManager;
    static QNetworkRequest prepare(const QNetworkRequest& request);

protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request, QIODevice* data) override;
};
