#include "qmlNetworkWrap.h"

#include "httpClientWrap.h"

#include <QMutexLocker>
#include <QNetworkDiskCache>

namespace {
const QString refererPrefix = QStringLiteral("pm-referer=");
}

QmlNetworkWrap::QmlNetworkWrap(QString cacheFolder) : m_cacheFolder(std::move(cacheFolder))
{
}

QNetworkAccessManager* QmlNetworkWrap::create(QObject* parent)
{
    // QML calls this from its loader threads; each network gets a cache folder of its own.
    int index = 0;
    {
        QMutexLocker lock(&m_mutex);
        index = m_created++;
    }
    auto* network = new ImageNetwork(parent);
    auto* cache = new QNetworkDiskCache(network);
    cache->setCacheDirectory(m_cacheFolder + "/" + QString::number(index));
    cache->setMaximumCacheSize(qint64(512) * 1024 * 1024);
    network->setCache(cache);
    return network;
}

namespace {
QtMessageHandler s_previousHandler = nullptr;

void filterImageLoadErrors(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg && QmlNetworkWrap::isImageLoadError(message)) return;
    if (s_previousHandler) s_previousHandler(type, context, message);
}
} // namespace

bool QmlNetworkWrap::isImageLoadError(const QString& message)
{
    return message.contains(QLatin1String("QQuickImage: Error transferring"));
}

void QmlNetworkWrap::quietImageLoadErrors()
{
    if (!s_previousHandler) s_previousHandler = qInstallMessageHandler(filterImageLoadErrors);
}

QUrl QmlNetworkWrap::withReferer(const QString& imageUrl, const QString& referer)
{
    QUrl url(imageUrl);
    if (referer.isEmpty()) return url;
    url.setFragment(refererPrefix + QString::fromLatin1(QUrl::toPercentEncoding(referer)), QUrl::StrictMode);
    return url;
}

QNetworkRequest ImageNetwork::prepare(const QNetworkRequest& request)
{
    QNetworkRequest prepared(request);
    QUrl url = request.url();
    const QString fragment = url.fragment(QUrl::FullyEncoded);
    if (fragment.startsWith(refererPrefix)) {
        const QByteArray referer = QByteArray::fromPercentEncoding(fragment.mid(refererPrefix.size()).toLatin1());
        if (!referer.isEmpty()) prepared.setRawHeader("Referer", referer);
        url.setFragment(QString());
        prepared.setUrl(url);
    }
    prepared.setHeader(QNetworkRequest::UserAgentHeader, HttpClientWrap::userAgent());
    prepared.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    prepared.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    prepared.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);   // see HttpClientWrap::get
    return prepared;
}

QNetworkReply* ImageNetwork::createRequest(Operation op, const QNetworkRequest& request, QIODevice* data)
{
    return QNetworkAccessManager::createRequest(op, prepare(request), data);
}
