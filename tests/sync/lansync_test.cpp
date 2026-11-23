#include "testkit.h"

#include "common/json.h"
#include "common/sync/lansync.h"
#include "fakes.h"

#include <memory>
#include <string>
#include <vector>

namespace {

class FakeLan;

// One device's network card on the fake LAN.
class FakeCard : public platform::LanNetwork {
public:
    FakeCard(FakeLan& lan, std::string address) : m_lan(lan), m_address(std::move(address)) {}

    bool start(const std::string& presharedKey, std::string&) override
    {
        key = presharedKey;
        started = true;
        return true;
    }
    void stop() override { started = false; }
    int exchangePort() const override { return port; }
    void announce(const std::string& message) override;
    void exchange(const std::string& address, int port, const std::string& mine,
                  std::function<void(const std::string&, const std::string&)> done) override;

    std::string key;
    bool started = false;
    int port = 4000;
    int exchanges = 0;
    const std::string& address() const { return m_address; }

private:
    FakeLan& m_lan;
    std::string m_address;
};

// Delivers announcements to every card, and exchanges between cards with the same key (like
// TLS with a pre-shared key: a different key fails the handshake). Everything arrives on the
// next scheduler turn, never inside the call.
class FakeLan {
public:
    explicit FakeLan(fakes::Scheduler& clock) : clock(clock) {}
    fakes::Scheduler& clock;
    std::vector<FakeCard*> cards;
    std::vector<std::string> sent;          // every announcement, for inspection

    void announce(FakeCard& from, const std::string& message)
    {
        sent.push_back(message);
        for (FakeCard* card : cards) {
            if (!card->started) continue;
            const std::string address = from.address();
            clock.after(0, [card, message, address] {
                if (card->started && card->onAnnouncement) card->onAnnouncement(message, address);
            });
        }
    }

    void exchange(FakeCard& from, const std::string& address, int port, const std::string& mine,
                  std::function<void(const std::string&, const std::string&)> done)
    {
        ++from.exchanges;
        FakeCard* to = nullptr;
        for (FakeCard* card : cards)
            if (card->address() == address && card->port == port && card->started) to = card;
        const std::string key = from.key;
        clock.after(0, [to, key, mine, done] {
            if (!to || !to->onExchange) {
                done({}, "Connection refused");
                return;
            }
            if (to->key != key) {
                done({}, "handshake failed");
                return;
            }
            done(to->onExchange(mine), {});
        });
    }

private:
};

void FakeCard::announce(const std::string& message) { m_lan.announce(*this, message); }
void FakeCard::exchange(const std::string& address, int port, const std::string& mine,
                        std::function<void(const std::string&, const std::string&)> done)
{
    m_lan.exchange(*this, address, port, mine, std::move(done));
}

library::Entry manga(const std::string& id, std::int64_t addedMs)
{
    library::Entry e;
    e.sourceId = "mangadex";
    e.mangaId = id;
    e.title = "Title " + id;
    e.addedMs = addedMs;
    return e;
}

struct Device {
    FakeCard card;
    library::Library library;
    devicesync::LanSync sync;
    std::string id;
    Device(FakeLan& lan, fakes::Scheduler& clock, std::string address, std::string deviceId)
        : card(lan, std::move(address)), sync(library, card, clock), id(std::move(deviceId))
    {
        lan.cards.push_back(&card);
    }
    void start(const std::string& secret = "app secret") { sync.start(secret, id); }
};

} // namespace

TEST(lansync, two_own_devices_find_each_other_and_sync)
{
    fakes::Scheduler clock;
    FakeLan lan(clock);
    Device pc(lan, clock, "192.168.1.10", "aaaa");
    Device phone(lan, clock, "192.168.1.20", "bbbb");
    pc.library.add(manga("from-pc", 10));
    phone.library.add(manga("from-phone", 20));
    pc.start();
    phone.start();
    clock.advance(devicesync::LanSync::announceMs * 2);

    CHECK(pc.library.contains("mangadex", "from-phone"));
    CHECK(phone.library.contains("mangadex", "from-pc"));
    CHECK_EQ(pc.sync.nearbyDevices(), 1);
    CHECK_EQ(phone.sync.nearbyDevices(), 1);
    CHECK_EQ(pc.card.key, devicesync::LanSync::keyFor("app secret"));
    CHECK_EQ(pc.card.key.size(), std::size_t(32));
}

TEST(lansync, a_change_goes_over_with_the_next_announcement)
{
    fakes::Scheduler clock;
    FakeLan lan(clock);
    Device pc(lan, clock, "192.168.1.10", "aaaa");
    Device phone(lan, clock, "192.168.1.20", "bbbb");
    pc.start();
    phone.start();
    clock.advance(devicesync::LanSync::announceMs * 2);

    // Settled: no more exchanges until the next periodic one.
    const int before = pc.card.exchanges + phone.card.exchanges;
    clock.advance(devicesync::LanSync::announceMs * 2);
    CHECK_EQ(pc.card.exchanges + phone.card.exchanges, before);

    phone.library.add(manga("new", 30));
    clock.advance(devicesync::LanSync::announceMs + 1);
    CHECK(pc.library.contains("mangadex", "new"));
}

TEST(lansync, another_users_devices_are_ignored)
{
    fakes::Scheduler clock;
    FakeLan lan(clock);
    Device mine(lan, clock, "192.168.1.10", "aaaa");
    Device stranger(lan, clock, "192.168.1.30", "cccc");
    mine.library.add(manga("private", 10));
    stranger.library.add(manga("theirs", 20));
    mine.start("app secret");
    stranger.start("someone else's secret");
    clock.advance(devicesync::LanSync::announceMs * 4);

    CHECK_EQ(mine.sync.nearbyDevices(), 0);
    CHECK_EQ(stranger.sync.nearbyDevices(), 0);
    CHECK(!stranger.library.contains("mangadex", "private"));
    CHECK(!mine.library.contains("mangadex", "theirs"));
    CHECK_EQ(mine.card.exchanges + stranger.card.exchanges, 0);
}

TEST(lansync, forged_and_old_announcements_are_ignored)
{
    fakes::Scheduler clock;
    FakeLan lan(clock);
    Device pc(lan, clock, "192.168.1.10", "aaaa");
    pc.start();
    clock.advance(1);

    // A real announcement of pc's, replayed later from elsewhere under another name.
    const std::string real = lan.sent.at(0);
    auto parsed = json::parse(real).value;
    parsed.set("device", "zzzz");
    pc.card.onAnnouncement(json::write(parsed), "192.168.1.99");
    CHECK_EQ(pc.sync.nearbyDevices(), 0);

    // Correctly signed, but older than the clock slack allows (made on a separate network, so
    // only the replay reaches pc).
    FakeLan elsewhere(clock);
    Device other(elsewhere, clock, "192.168.1.20", "bbbb");
    other.start();
    const std::string old = elsewhere.sent.back();
    other.sync.stop();
    clock.advance(devicesync::LanSync::clockSlackMs + 1);
    pc.card.onAnnouncement(old, "192.168.1.20");
    CHECK_EQ(pc.sync.nearbyDevices(), 0);
}

TEST(lansync, stop_goes_quiet)
{
    fakes::Scheduler clock;
    FakeLan lan(clock);
    Device pc(lan, clock, "192.168.1.10", "aaaa");
    pc.start();
    clock.advance(devicesync::LanSync::announceMs);
    pc.sync.stop();
    CHECK(!pc.card.started);
    const std::size_t sent = lan.sent.size();
    clock.advance(devicesync::LanSync::announceMs * 3);
    CHECK_EQ(lan.sent.size(), sent);
}

TEST(lansync, no_secret_means_off)
{
    fakes::Scheduler clock;
    FakeLan lan(clock);
    Device pc(lan, clock, "192.168.1.10", "aaaa");
    pc.start("");
    CHECK(!pc.sync.running());
    CHECK(!pc.card.started);
}
