#pragma once

#include <functional>
#include <string>

// What the backend needs from the host to talk to devices on the same local network: short
// announcements to everyone (UDP broadcast), and encrypted one-to-one exchanges of a file each
// way (TLS with a pre-shared key, so only devices holding the same key can connect).
// Everything happens on one thread, like HttpClient.
namespace platform {

class LanNetwork {
public:
    virtual ~LanNetwork() = default;

    // Starts listening for announcements and exchanges. `presharedKey`: 32 raw bytes.
    // Returns false when the network can't be used (the reason in `problem`).
    virtual bool start(const std::string& presharedKey, std::string& problem) = 0;
    virtual void stop() = 0;
    // The port exchanges are accepted on (valid after start()).
    virtual int exchangePort() const = 0;

    virtual void announce(const std::string& message) = 0;
    // An announcement from someone on the network (this device's own ones included).
    std::function<void(const std::string& message, const std::string& fromAddress)> onAnnouncement;

    // Sends `mine` to address:port and hands back what the other side sends in return, or why
    // it failed.
    virtual void exchange(const std::string& address, int port, const std::string& mine,
                          std::function<void(const std::string& theirs, const std::string& problem)> done) = 0;
    // Another device opened an exchange and sent `theirs`; the answer is what goes back.
    std::function<std::string(const std::string& theirs)> onExchange;
};

} // namespace platform
