#pragma once

#include "common/platform/lannetwork.h"

#include <QObject>
#include <QSslConfiguration>
#include <QSslServer>
#include <QUdpSocket>

// The backend's LanNetwork on Qt: announcements as UDP broadcasts on announcePort, exchanges
// over TLS with the pre-shared key (no certificates), each side sending one length-prefixed
// message. On Android the Wi-Fi multicast lock is held while running, since without it many
// phones drop broadcasts.
class LanNetworkWrap : public QObject, public platform::LanNetwork {
    Q_OBJECT

public:
    explicit LanNetworkWrap(quint16 announcePort = defaultAnnouncePort, QObject* parent = nullptr);
    ~LanNetworkWrap() override;

    bool start(const std::string& presharedKey, std::string& problem) override;
    void stop() override;
    int exchangePort() const override;
    void announce(const std::string& message) override;
    void exchange(const std::string& address, int port, const std::string& mine,
                  std::function<void(const std::string& theirs, const std::string& problem)> done) override;

    static constexpr quint16 defaultAnnouncePort = 45454;
    static constexpr qint64 maxMessageBytes = 16 * 1024 * 1024;
    static constexpr int exchangeTimeoutMs = 15000;

private:
    void incoming(QSslSocket* socket);
    void holdMulticastLock(bool hold);

    quint16 m_announcePort;
    QUdpSocket m_udp;
    QSslServer m_server;
    QSslConfiguration m_config;
    QByteArray m_key;
    bool m_running = false;
};
