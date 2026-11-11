// Networking: real UDP sessions on the loopback interface, reliable ordered delivery over a
// lossy, reordering transport, capacity, disconnects and timeouts; agr.net from Lua.
#include <doctest/doctest.h>

#include <algorithm>
#include <deque>
#include <map>
#include <random>

#include "agr/net/NetSession.hpp"

using namespace agr;
using namespace agr::net;

namespace {

/// Steps sessions together until `done` (simulated time, 10 ms per step).
template <typename Done>
bool pump(std::initializer_list<NetSession*> sessions, f64& now, Done done, int maxSteps = 600) {
    for (int i = 0; i < maxSteps; ++i) {
        now += 0.01;
        for (NetSession* s : sessions) s->update(now);
        if (done()) return true;
    }
    return false;
}

std::vector<NetEvent> collect(std::vector<NetEvent>& into, NetSession& s) {
    for (auto& e : s.takeEvents()) into.push_back(std::move(e));
    return into;
}

/// In-memory network that loses and reorders datagrams.
struct LossyNetwork {
    struct Datagram {
        Address from, to;
        std::vector<u8> bytes;
    };
    std::deque<Datagram> inFlight;
    std::mt19937 rng{1234};
    float loss = 0.3f;
    u16 nextPort = 40000;
};

class LossyTransport final : public NetTransport {
public:
    explicit LossyTransport(LossyNetwork& n) : m_net(n) {}
    Status open(u16 port) override {
        m_port = port ? port : m_net.nextPort++;
        return {};
    }
    void close() override { m_port = 0; }
    u16 localPort() const override { return m_port; }
    Status send(const Address& to, const void* data, usize size) override {
        if (std::uniform_real_distribution<float>(0, 1)(m_net.rng) < m_net.loss) return {};  // lost
        LossyNetwork::Datagram d{{0x7F000001, m_port}, to, std::vector<u8>(static_cast<const u8*>(data), static_cast<const u8*>(data) + size)};
        // Reordering: insert at a random place among the datagrams in flight.
        const usize at = m_net.inFlight.empty() ? 0 : std::uniform_int_distribution<usize>(0, m_net.inFlight.size())(m_net.rng);
        m_net.inFlight.insert(m_net.inFlight.begin() + static_cast<std::ptrdiff_t>(at), std::move(d));
        return {};
    }
    bool receive(Address& from, std::vector<u8>& data) override {
        for (auto it = m_net.inFlight.begin(); it != m_net.inFlight.end(); ++it)
            if (it->to.port == m_port && m_port != 0) {
                from = it->from;
                data = std::move(it->bytes);
                m_net.inFlight.erase(it);
                return true;
            }
        return false;
    }

private:
    LossyNetwork& m_net;
    u16 m_port = 0;
};

}  // namespace

TEST_SUITE("net") {

TEST_CASE("UDP on loopback: join, unreliable and reliable messages, broadcast, leave") {
    NetSession host, a, b;
    REQUIRE(host.host(0, 4));
    const u16 port = host.port();
    REQUIRE(port != 0);
    CHECK(host.role() == NetRole::Host);
    CHECK(host.localId() == NetSession::kHostId);
    REQUIRE(a.join("127.0.0.1", port));
    REQUIRE(b.join("localhost", port));
    f64 now = 0.0;
    std::vector<NetEvent> hostEvents, aEvents, bEvents;
    REQUIRE(pump({&host, &a, &b}, now, [&] { return a.connected() && b.connected() && host.peers().size() == 2; }));
    collect(hostEvents, host);
    collect(aEvents, a);
    collect(bEvents, b);
    CHECK(a.localId() >= 2);
    CHECK(b.localId() >= 2);
    CHECK(a.localId() != b.localId());
    REQUIRE(hostEvents.size() == 2);
    CHECK(hostEvents[0].type == NetEvent::Type::Connected);
    REQUIRE(aEvents.size() == 1);
    CHECK(aEvents[0].type == NetEvent::Type::Connected);
    CHECK(aEvents[0].peer == NetSession::kHostId);

    // Client -> host (unreliable and reliable), host -> everyone.
    CHECK(a.send(NetSession::kHostId, "pos 1 2", false));
    CHECK(a.send(NetSession::kBroadcast, "score 3", true));
    CHECK(host.send(NetSession::kBroadcast, "round 2", true));
    CHECK_FALSE(a.send(NetSession::kHostId, std::string(NetSession::kMaxPayload + 1, 'x'), true));  // too large
    hostEvents.clear();
    aEvents.clear();
    bEvents.clear();
    pump({&host, &a, &b}, now, [&] {
        collect(hostEvents, host);
        collect(aEvents, a);
        collect(bEvents, b);
        return hostEvents.size() >= 2 && !aEvents.empty() && !bEvents.empty();
    });
    REQUIRE(hostEvents.size() == 2);
    CHECK(hostEvents[0].peer == a.localId());
    CHECK(hostEvents[0].data == "pos 1 2");
    CHECK(hostEvents[1].data == "score 3");
    REQUIRE(aEvents.size() == 1);
    CHECK(aEvents[0].data == "round 2");
    REQUIRE(bEvents.size() == 1);
    CHECK(bEvents[0].data == "round 2");
    CHECK(host.stats().packetsReceived > 0);

    // Leaving: the host is told at once; the client's session is closed.
    const u32 bId = b.localId();
    b.close();
    CHECK(b.role() == NetRole::None);
    hostEvents.clear();
    pump({&host, &a}, now, [&] {
        collect(hostEvents, host);
        return !hostEvents.empty();
    });
    REQUIRE(hostEvents.size() == 1);
    CHECK(hostEvents[0].type == NetEvent::Type::Disconnected);
    CHECK(hostEvents[0].peer == bId);
    CHECK(host.peers() == std::vector<u32>{a.localId()});
}

TEST_CASE("reliable messages arrive once and in order despite 30 % loss and reordering") {
    LossyNetwork network;
    NetSession host(std::make_unique<LossyTransport>(network)), client(std::make_unique<LossyTransport>(network));
    REQUIRE(host.host(7777, 2));
    REQUIRE(client.join("127.0.0.1", 7777));
    f64 now = 0.0;
    REQUIRE(pump({&host, &client}, now, [&] { return client.connected() && host.peers().size() == 1; }, 2000));
    host.takeEvents();
    client.takeEvents();
    for (int i = 0; i < 100; ++i) REQUIRE(client.send(NetSession::kHostId, std::to_string(i), true));
    std::vector<NetEvent> got;
    REQUIRE(pump({&host, &client}, now, [&] {
        collect(got, host);
        return got.size() >= 100;
    }, 5000));
    for (int i = 0; i < 100; ++i) CHECK(got[static_cast<usize>(i)].data == std::to_string(i));
    pump({&host, &client}, now, [] { return false; }, 300);  // late duplicates are not delivered again
    collect(got, host);
    CHECK(got.size() == 100);
    CHECK(client.stats().resent > 0);
}

TEST_CASE("a full host refuses players; silent players time out") {
    LossyNetwork network;
    network.loss = 0.0f;
    NetSession host(std::make_unique<LossyTransport>(network)), a(std::make_unique<LossyTransport>(network)),
        b(std::make_unique<LossyTransport>(network));
    REQUIRE(host.host(9000, 1));
    REQUIRE(a.join("127.0.0.1", 9000));
    f64 now = 0.0;
    REQUIRE(pump({&host, &a}, now, [&] { return a.connected(); }));
    REQUIRE(b.join("127.0.0.1", 9000));
    std::vector<NetEvent> bEvents;
    REQUIRE(pump({&host, &a, &b}, now, [&] {
        collect(bEvents, b);
        return !bEvents.empty();
    }));
    CHECK(bEvents[0].type == NetEvent::Type::Disconnected);  // refused
    CHECK_FALSE(b.connected());
    CHECK(host.peers().size() == 1);

    // `a` stops answering (crashed): dropped after the timeout.
    host.setTimeout(1.0);
    host.takeEvents();
    std::vector<NetEvent> hostEvents;
    REQUIRE(pump({&host}, now, [&] {
        collect(hostEvents, host);
        return !hostEvents.empty();
    }, 200));
    CHECK(hostEvents[0].type == NetEvent::Type::Disconnected);
    CHECK(host.peers().empty());
}

TEST_CASE("joining an unknown host fails cleanly") {
    NetSession s;
    CHECK_FALSE(s.join("no-such-host.invalid", 1234));
    CHECK(s.role() == NetRole::None);
}

}
