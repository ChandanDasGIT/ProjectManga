#include "lanNetworkWrap.h"

#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSslCipher>
#include <QSslPreSharedKeyAuthenticator>
#include <QSslSocket>
#include <QTimer>
#include <QtEndian>

#include <memory>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

namespace {

QByteArray framed(const QByteArray& message)
{
    QByteArray out(4, '\0');
    qToBigEndian<quint32>(static_cast<quint32>(message.size()), out.data());
    return out + message;
}

// One length-prefixed message from `buffer`: 1 when complete (in `message`), 0 when more is
// needed, -1 when it is too long.
int takeFramed(QByteArray& buffer, QByteArray& message)
{
    if (buffer.size() < 4) return 0;
    const qint64 length = qFromBigEndian<quint32>(buffer.constData());
    if (length > LanNetworkWrap::maxMessageBytes) return -1;
    if (buffer.size() < 4 + length) return 0;
    message = buffer.mid(4, length);
    buffer.remove(0, 4 + length);
    return 1;
}

QSslConfiguration pskConfiguration()
{
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::TlsV1_2);
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    QList<QSslCipher> ciphers;
    for (const char* name : {"ECDHE-PSK-CHACHA20-POLY1305", "PSK-AES256-GCM-SHA384", "PSK-AES128-GCM-SHA256"}) {
        const QSslCipher cipher(QString::fromLatin1(name));
        if (!cipher.isNull()) ciphers.append(cipher);
    }
    config.setCiphers(ciphers);
    return config;
}

} // namespace

LanNetworkWrap::LanNetworkWrap(quint16 announcePort, QObject* parent) : QObject(parent), m_announcePort(announcePort)
{
    connect(&m_udp, &QUdpSocket::readyRead, this, [this] {
        while (m_udp.hasPendingDatagrams()) {
            const QNetworkDatagram datagram = m_udp.receiveDatagram(4096);
            if (!m_running || !onAnnouncement) continue;
            onAnnouncement(datagram.data().toStdString(), datagram.senderAddress().toString().toStdString());
        }
    });
    connect(&m_server, &QSslServer::preSharedKeyAuthenticationRequired, this,
            [this](QSslSocket*, QSslPreSharedKeyAuthenticator* authenticator) { authenticator->setPreSharedKey(m_key); });
    connect(&m_server, &QTcpServer::pendingConnectionAvailable, this, [this] {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            if (auto* ssl = qobject_cast<QSslSocket*>(socket))
                incoming(ssl);
            else
                socket->deleteLater();
        }
    });
}

LanNetworkWrap::~LanNetworkWrap()
{
    stop();
}

bool LanNetworkWrap::start(const std::string& presharedKey, std::string& problem)
{
    stop();
    m_config = pskConfiguration();
    if (!QSslSocket::supportsSsl() || m_config.ciphers().isEmpty()) {
        problem = "Wi-Fi sync needs encryption this device doesn't offer";
        return false;
    }
    m_key = QByteArray::fromStdString(presharedKey);
    m_server.setSslConfiguration(m_config);
    if (!m_server.listen(QHostAddress::AnyIPv4)) {
        problem = "Wi-Fi sync could not open a port: " + m_server.errorString().toStdString();
        return false;
    }
    if (!m_udp.bind(QHostAddress::AnyIPv4, m_announcePort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        problem = "Wi-Fi sync could not listen on the network: " + m_udp.errorString().toStdString();
        m_server.close();
        return false;
    }
    m_running = true;
    holdMulticastLock(true);
    return true;
}

void LanNetworkWrap::stop()
{
    if (!m_running) return;
    m_running = false;
    m_udp.close();
    m_server.close();
    holdMulticastLock(false);
}

int LanNetworkWrap::exchangePort() const
{
    return m_server.serverPort();
}

void LanNetworkWrap::announce(const std::string& message)
{
    if (!m_running) return;
    const QByteArray data = QByteArray::fromStdString(message);
    // Each network's own broadcast address: 255.255.255.255 alone leaves by one interface only.
    QList<QHostAddress> targets{QHostAddress::Broadcast};
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) ||
            !(flags & QNetworkInterface::CanBroadcast) || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry& entry : interface.addressEntries())
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.broadcast().isNull() &&
                !targets.contains(entry.broadcast()))
                targets.append(entry.broadcast());
    }
    for (const QHostAddress& target : targets) m_udp.writeDatagram(data, target, m_announcePort);
}

void LanNetworkWrap::exchange(const std::string& address, int port, const std::string& mine,
                              std::function<void(const std::string&, const std::string&)> done)
{
    auto* socket = new QSslSocket(this);
    socket->setSslConfiguration(m_config);
    auto buffer = std::make_shared<QByteArray>();
    auto finished = std::make_shared<bool>(false);
    auto finish = [socket, finished, done = std::move(done)](const std::string& theirs, const std::string& problem) {
        if (*finished) return;
        *finished = true;
        socket->abort();
        socket->deleteLater();
        done(theirs, problem);
    };
    connect(socket, &QSslSocket::preSharedKeyAuthenticationRequired, socket,
            [this](QSslPreSharedKeyAuthenticator* authenticator) {
                authenticator->setIdentity("ProjectManga");
                authenticator->setPreSharedKey(m_key);
            });
    connect(socket, &QSslSocket::encrypted, socket,
            [socket, message = framed(QByteArray::fromStdString(mine))] { socket->write(message); });
    connect(socket, &QSslSocket::readyRead, socket, [socket, buffer, finish] {
        *buffer += socket->readAll();
        QByteArray theirs;
        const int state = takeFramed(*buffer, theirs);
        if (state < 0) finish({}, "the other device sent too much");
        if (state > 0) finish(theirs.toStdString(), {});
    });
    connect(socket, &QSslSocket::errorOccurred, socket,
            [socket, finish](QAbstractSocket::SocketError) { finish({}, socket->errorString().toStdString()); });
    QTimer::singleShot(exchangeTimeoutMs, socket, [finish] { finish({}, "the other device did not answer"); });
    socket->connectToHostEncrypted(QString::fromStdString(address), static_cast<quint16>(port));
}

void LanNetworkWrap::incoming(QSslSocket* socket)
{
    auto buffer = std::make_shared<QByteArray>();
    connect(socket, &QSslSocket::readyRead, socket, [this, socket, buffer] {
        *buffer += socket->readAll();
        QByteArray theirs;
        const int state = takeFramed(*buffer, theirs);
        if (state < 0) {
            socket->abort();
            return;
        }
        if (state == 0) return;
        const std::string answer = onExchange ? onExchange(theirs.toStdString()) : std::string();
        socket->write(framed(QByteArray::fromStdString(answer)));
        socket->disconnectFromHost();
    });
    connect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);
    QTimer::singleShot(exchangeTimeoutMs, socket, [socket] { socket->abort(); });
}

void LanNetworkWrap::holdMulticastLock(bool hold)
{
#ifdef Q_OS_ANDROID
    static QJniObject lock;
    if (hold && !lock.isValid()) {
        const QJniObject context = QNativeInterface::QAndroidApplication::context();
        const QJniObject wifi = context.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
                                                         QJniObject::fromString("wifi").object<jstring>());
        if (!wifi.isValid()) return;
        lock = wifi.callObjectMethod("createMulticastLock",
                                     "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
                                     QJniObject::fromString("ProjectManga").object<jstring>());
        if (!lock.isValid()) return;
        lock.callMethod<void>("setReferenceCounted", "(Z)V", jboolean(false));
    }
    if (!lock.isValid()) return;
    if (hold)
        lock.callMethod<void>("acquire", "()V");
    else
        lock.callMethod<void>("release", "()V");
#else
    Q_UNUSED(hold);
#endif
}
