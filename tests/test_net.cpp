// Networking: real UDP sessions on the loopback interface, reliable ordered delivery over a
// lossy, reordering transport, capacity, disconnects and timeouts; agr.net from Lua.
#include <set>
#include <cctype>
#include <doctest/doctest.h>
#include <functional>

#include <algorithm>
#include <deque>
#include <map>
#include <random>

#include "agr/net/NetSession.hpp"
#include "agr/net/Replication.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Components.hpp"
#include "agr/scene/Prefab.hpp"
#include "agr/core/Uuid.hpp"
#include <filesystem>

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
    CHECK_FALSE(a.send(NetSession::kHostId, std::string(NetSession::kMaxPayload + 1, 'x'), false));     // unreliable: one datagram
    CHECK_FALSE(a.send(NetSession::kHostId, std::string(NetSession::kMaxReliable + 1, 'x'), true));     // reliable: limit
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

TEST_CASE("large reliable messages are fragmented and rebuilt in order despite loss; system channel; LAN discovery") {
    LossyNetwork netw;
    netw.loss = 0.3f;
    NetSession host(std::make_unique<LossyTransport>(netw));
    NetSession client(std::make_unique<LossyTransport>(netw));
    REQUIRE(host.host(7000, 4));
    REQUIRE(client.join("127.0.0.1", 7000));
    f64 now = 0.0;
    for (int i = 0; i < 400 && !client.connected(); ++i) {
        now += 0.02;
        host.update(now);
        client.update(now);
    }
    REQUIRE(client.connected());
    host.takeEvents();
    client.takeEvents();
    host.takeSystemEvents();
    // 10 KB message (9 fragments) + a small one after it: both arrive whole and in order.
    std::string big(10000, ' ');
    for (usize i = 0; i < big.size(); ++i) big[i] = static_cast<char>('a' + i % 26);
    REQUIRE(client.send(NetSession::kHostId, big, true));
    REQUIRE(client.send(NetSession::kHostId, "after", true));
    const std::string sys = std::string(1, NetSession::kSystemTag) + "replication";
    REQUIRE(client.send(NetSession::kHostId, sys, true));
    std::vector<NetEvent> got, gotSys;
    for (int i = 0; i < 2000 && (got.size() < 2 || gotSys.empty()); ++i) {
        now += 0.02;
        client.update(now);
        host.update(now);
        for (auto& e : host.takeEvents()) got.push_back(std::move(e));
        for (auto& e : host.takeSystemEvents()) gotSys.push_back(std::move(e));
    }
    REQUIRE(got.size() == 2);
    CHECK(got[0].data == big);
    CHECK(got[1].data == "after");
    REQUIRE(gotSys.size() == 1);  // system messages do not reach the game's queue
    CHECK(gotSys[0].data == sys);

    // LAN discovery: a third session finds the host and its name.
    netw.loss = 0.0f;
    host.advertise("Partie de Julien");
    NetSession seeker(std::make_unique<LossyTransport>(netw));
    REQUIRE(seeker.discover(7000));
    CHECK(seeker.discovering());
    for (int i = 0; i < 50 && seeker.lanGames().empty(); ++i) {
        now += 0.05;
        seeker.update(now);
        host.update(now);
        client.update(now);
    }
    REQUIRE(seeker.lanGames().size() == 1);
    CHECK(seeker.lanGames()[0].name == "Partie de Julien");
    CHECK(seeker.lanGames()[0].players == 2);
    CHECK(seeker.lanGames()[0].maxPlayers == 5);
    CHECK_FALSE(host.discover(7000));  // a hosting session does not look for games
    seeker.stopDiscovery();
    CHECK(seeker.lanGames().empty());
}

TEST_CASE("replication: synced transforms and fields, owners, spawns for late joiners, RPCs, disconnects") {
    registerRenderComponents();
    registerNetComponents();
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / ("agr_netrep_" + Uuid::generate().toString());
    fs::create_directories(root);
    VirtualFileSystem vfs;
    vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
    PrefabLibrary prefabs(vfs);
    prefabs.install();
    {  // a "Rocket" prefab
        Scene tmp;
        const auto r = tmp.createEntity("Rocket");
        tmp.registry().emplace<SpriteRendererComponent>(r).size = {0.5f, 0.5f};
        REQUIRE(prefabs.save("assets://rocket.agrprefab", prefab::fromEntity(tmp, r)));
    }
    LossyNetwork netw;
    netw.loss = 0.1f;
    struct Machine {
        NetSession session;
        Scene scene;
        Replicator rep;
        std::vector<std::string> calls;
        explicit Machine(LossyNetwork& n) : session(std::make_unique<LossyTransport>(n)) {
            rep.setRpcHandler([this](Scene& sc, ecs::Entity e, const std::string& fn, const nlohmann::json& args, u32 sender) {
                calls.push_back(sc.nameOf(e) + ":" + fn + ":" + args.dump() + ":from" + std::to_string(sender));
            });
        }
    };
    const Uuid shipId = Uuid::generate(), ballId = Uuid::generate();
    auto build = [&](Machine& m) {  // the same scene on every machine
        const auto ship = m.scene.createEntity("Ship", {}, shipId);
        auto& sync = m.scene.registry().emplace<NetSyncComponent>(ship);
        sync.fields = "SpriteRenderer.color;SpriteRenderer.size";
        sync.rate = 30.0f;
        m.scene.registry().emplace<SpriteRendererComponent>(ship);
        const auto ball = m.scene.createEntity("Ball", {}, ballId);
        m.scene.registry().emplace<NetSyncComponent>(ball);
    };
    Machine host(netw), a(netw), b(netw);
    build(host);
    build(a);
    build(b);
    REQUIRE(host.session.host(7100, 4));
    REQUIRE(a.session.join("127.0.0.1", 7100));
    f64 now = 0.0;
    std::vector<Machine*> all = {&host, &a};
    auto step = [&](int frames, const std::function<void()>& each = {}) {
        for (int i = 0; i < frames; ++i) {
            now += 1.0 / 60.0;
            if (each) each();
            for (Machine* m : all) {
                m->session.update(now);
                m->rep.update(m->scene, m->session, now);
                m->session.takeEvents();
            }
        }
    };
    step(60);
    REQUIRE(a.session.connected());
    auto pos = [](Machine& m, Uuid id) { return m.scene.registry().get<TransformComponent>(m.scene.findByUuid(id)).position; };
    // Host-owned ship: moves on the host, follows (interpolated) on the client; fields too.
    auto& hostShip = host.scene.registry().get<TransformComponent>(host.scene.findByUuid(shipId));
    host.scene.registry().get<SpriteRendererComponent>(host.scene.findByUuid(shipId)).color = {1, 0, 0, 1};
    step(90, [&] { hostShip.position.x += 0.05f; });
    step(30);
    CHECK(pos(a, shipId).x == doctest::Approx(hostShip.position.x).epsilon(0.01));
    CHECK(a.scene.registry().get<SpriteRendererComponent>(a.scene.findByUuid(shipId)).color.y == doctest::Approx(0.0f));
    CHECK(a.rep.stats().statesReceived >= 0);
    // A client-owned ball: the host gives it to A; A moves it; the host follows.
    REQUIRE(host.rep.setOwner(host.scene, host.session, host.scene.findByUuid(ballId), a.session.localId()));
    step(30);
    CHECK(a.rep.isOwner(a.scene, a.session, a.scene.findByUuid(ballId)));
    CHECK_FALSE(host.rep.isOwner(host.scene, host.session, host.scene.findByUuid(ballId)));
    auto& aBall = a.scene.registry().get<TransformComponent>(a.scene.findByUuid(ballId));
    step(60, [&] { aBall.position.y += 0.05f; });
    step(30);
    CHECK(pos(host, ballId).y == doctest::Approx(aBall.position.y).epsilon(0.02));
    // The host spawns a rocket owned by A; B joins later and receives it, the owners and the states.
    auto rocket = host.rep.spawn(host.scene, host.session, "assets://rocket.agrprefab", {3, 4, 0}, a.session.localId());
    REQUIRE(rocket);
    CHECK_FALSE(a.rep.spawn(a.scene, a.session, "assets://rocket.agrprefab", {0, 0, 0}));  // clients do not spawn
    step(40);
    const Uuid rocketId = host.scene.uuidOf(rocket.value());
    REQUIRE_FALSE(a.scene.findByUuid(rocketId).isNull());
    CHECK(pos(a, rocketId).x == doctest::Approx(3.0f));
    REQUIRE(b.session.join("127.0.0.1", 7100));
    all.push_back(&b);
    step(120);
    REQUIRE(b.session.connected());
    REQUIRE_FALSE(b.scene.findByUuid(rocketId).isNull());
    CHECK(b.rep.ownerOf(b.scene, b.scene.findByUuid(ballId)) == a.session.localId());
    CHECK(pos(b, ballId).y == doctest::Approx(pos(a, ballId).y).epsilon(0.02));  // A -> host -> B
    // RPCs: client -> everyone (relayed by the host with the real sender), host -> one peer, client -> host.
    REQUIRE(a.rep.rpc(a.scene, a.session, a.scene.findByUuid(shipId), "boom", nlohmann::json::array({1, "x"})));
    REQUIRE(host.rep.rpc(host.scene, host.session, host.scene.findByUuid(shipId), "hello", nlohmann::json::array(), b.session.localId()));
    REQUIRE(b.rep.rpc(b.scene, b.session, b.scene.findByUuid(ballId), "ping", nlohmann::json::array({true}), NetSession::kHostId));
    step(60);
    const std::string aId = std::to_string(a.session.localId()), bId = std::to_string(b.session.localId());
    CHECK(std::count(host.calls.begin(), host.calls.end(), "Ship:boom:[1,\"x\"]:from" + aId) == 1);
    CHECK(std::count(b.calls.begin(), b.calls.end(), "Ship:boom:[1,\"x\"]:from" + aId) == 1);
    CHECK(std::count(a.calls.begin(), a.calls.end(), "Ship:boom:[1,\"x\"]:from" + aId) == 0);  // not back to the sender
    CHECK(std::count(b.calls.begin(), b.calls.end(), "Ship:hello:[]:from1") == 1);
    CHECK(std::count(a.calls.begin(), a.calls.end(), "Ship:hello:[]:from1") == 0);
    CHECK(std::count(host.calls.begin(), host.calls.end(), "Ball:ping:[true]:from" + bId) == 1);
    // Despawn everywhere; A leaves: what it owned comes back to the host.
    REQUIRE(host.rep.despawn(host.scene, host.session, rocket.value()));
    step(40);
    CHECK(a.scene.findByUuid(rocketId).isNull());
    CHECK(b.scene.findByUuid(rocketId).isNull());
    a.session.close();
    all = {&host, &b};
    step(40);
    CHECK(host.rep.ownerOf(host.scene, host.scene.findByUuid(ballId)) == NetSession::kHostId);
    CHECK(b.rep.ownerOf(b.scene, b.scene.findByUuid(ballId)) == NetSession::kHostId);
    setPrefabResolver(nullptr);
    std::error_code ec;
    fs::remove_all(root, ec);

}
TEST_CASE("game password, round-trip time") {
    LossyNetwork network;
    network.loss = 0.0f;
    NetSession host(std::make_unique<LossyTransport>(network)), good(std::make_unique<LossyTransport>(network)),
        bad(std::make_unique<LossyTransport>(network));
    host.setPassword("tartiflette");
    REQUIRE(host.host(9100, 4));
    bad.setPassword("raclette");
    REQUIRE(bad.join("127.0.0.1", 9100));
    f64 now = 0.0;
    std::vector<NetEvent> badEvents;
    REQUIRE(pump({&host, &bad}, now, [&] {
        collect(badEvents, bad);
        return !badEvents.empty();
    }));
    CHECK(badEvents[0].type == NetEvent::Type::Disconnected);
    CHECK(bad.joinError() == "wrong password");
    CHECK(host.peers().empty());
    good.setPassword("tartiflette");
    REQUIRE(good.join("127.0.0.1", 9100));
    REQUIRE(pump({&host, &good}, now, [&] { return good.connected(); }));
    CHECK(good.joinError().empty());
    // Pings are answered: both sides measure the round trip (one update apart here: 10 ms).
    REQUIRE(pump({&host, &good}, now, [&] { return good.rtt(NetSession::kHostId) > 0.0 && host.rtt(good.localId()) > 0.0; }, 300));
    CHECK(good.rtt(NetSession::kHostId) < 0.1);
}

TEST_CASE("encryption: messages arrive, the wire shows no plaintext, tampered and replayed packets are dropped") {
    LossyNetwork network;
    network.loss = 0.0f;
    // A transport that records what goes on the wire.
    struct Tap final : NetTransport {
        LossyTransport inner;
        std::vector<std::vector<u8>>* wire;
        Tap(LossyNetwork& n, std::vector<std::vector<u8>>* w) : inner(n), wire(w) {}
        Status open(u16 port) override { return inner.open(port); }
        void close() override { inner.close(); }
        u16 localPort() const override { return inner.localPort(); }
        Status send(const Address& to, const void* data, usize size) override {
            wire->emplace_back(static_cast<const u8*>(data), static_cast<const u8*>(data) + size);
            return inner.send(to, data, size);
        }
        bool receive(Address& from, std::vector<u8>& data) override { return inner.receive(from, data); }
    };
    std::vector<std::vector<u8>> wire;
    NetSession host(std::make_unique<Tap>(network, &wire)), client(std::make_unique<Tap>(network, &wire));
    host.setPassword("fondue savoyarde");
    host.setEncryption(true);
    client.setPassword("fondue savoyarde");
    REQUIRE(host.host(9200, 4));
    REQUIRE(client.join("127.0.0.1", 9200));
    f64 now = 0.0;
    REQUIRE(pump({&host, &client}, now, [&] { return client.connected(); }));
    CHECK(client.encrypted(NetSession::kHostId));
    CHECK(host.encrypted(client.localId()));
    const std::string secret = "position du tresor: 42,17";
    const std::string big(5000, 'Z');  // fragmented reliable message
    REQUIRE(client.send(NetSession::kHostId, secret, true));
    REQUIRE(client.send(NetSession::kHostId, big, true));
    REQUIRE(host.send(client.localId(), "unreliable hello", false));
    std::vector<NetEvent> hostEvents, clientEvents;
    REQUIRE(pump({&host, &client}, now, [&] {
        collect(hostEvents, host);
        collect(clientEvents, client);
        int got = 0;
        for (const auto& e : hostEvents) got += e.type == NetEvent::Type::Message && (e.data == secret || e.data == big);
        bool unreliable = false;
        for (const auto& e : clientEvents) unreliable |= e.type == NetEvent::Type::Message && e.data == "unreliable hello";
        return got == 2 && unreliable;
    }));
    // Nothing readable on the wire.
    for (const auto& d : wire) {
        const std::string bytes(d.begin(), d.end());
        CHECK(bytes.find("tresor") == std::string::npos);
        CHECK(bytes.find("ZZZZZZZZ") == std::string::npos);
        CHECK(bytes.find("unreliable") == std::string::npos);
    }
    // Replay an encrypted packet, then a tampered copy: both dropped, nothing delivered.
    std::vector<u8> captured;
    for (const auto& d : wire)
        if (d.size() > 40 && (d[4] & 0x80)) captured = d;
    REQUIRE_FALSE(captured.empty());
    const u64 before = host.stats().rejected + client.stats().rejected;
    std::vector<u8> tampered = captured;
    tampered[tampered.size() - 20] ^= 1;
    // Deliver straight into both sessions' inboxes (whoever it was meant for will check it).
    for (const u16 port : {u16(9200), client.port()}) {
        network.inFlight.push_back({{0x7F000001, port == 9200 ? client.port() : u16(9200)}, {0x7F000001, port}, captured});
        network.inFlight.push_back({{0x7F000001, port == 9200 ? client.port() : u16(9200)}, {0x7F000001, port}, tampered});
    }
    hostEvents.clear();
    clientEvents.clear();
    pump({&host, &client}, now, [] { return false; }, 20);
    collect(hostEvents, host);
    collect(clientEvents, client);
    CHECK(host.stats().rejected + client.stats().rejected >= before + 2);
    for (const auto& e : hostEvents) CHECK(e.type != NetEvent::Type::Message);
    // Wrong password: refused as before; without encryption on the host, links stay clear.
    NetSession plainHost(std::make_unique<LossyTransport>(network)), plainClient(std::make_unique<LossyTransport>(network));
    plainHost.setPassword("x");
    plainClient.setPassword("x");
    REQUIRE(plainHost.host(9201, 2));
    REQUIRE(plainClient.join("127.0.0.1", 9201));
    REQUIRE(pump({&plainHost, &plainClient}, now, [&] { return plainClient.connected(); }));
    CHECK_FALSE(plainClient.encrypted(NetSession::kHostId));
}

TEST_CASE("replication: only changes are sent, visibility per player (owner only, range, overrides)") {
    registerRenderComponents();
    registerNetComponents();
    LossyNetwork netw;
    netw.loss = 0.0f;
    struct Machine {
        NetSession session;
        Scene scene;
        Replicator rep;
        explicit Machine(LossyNetwork& n) : session(std::make_unique<LossyTransport>(n)) {}
    };
    const Uuid treasureId = Uuid::generate(), secretId = Uuid::generate(), heroId = Uuid::generate();
    auto build = [&](Machine& m) {
        m.scene.createEntity("Treasure", {}, treasureId);
        m.scene.registry().emplace<NetSyncComponent>(m.scene.findByUuid(treasureId)).visibility = NetVisibility::Range;
        m.scene.registry().get<NetSyncComponent>(m.scene.findByUuid(treasureId)).visibleRange = 10.0f;
        m.scene.createEntity("Secret", {}, secretId);
        m.scene.registry().emplace<NetSyncComponent>(m.scene.findByUuid(secretId)).visibility = NetVisibility::OwnerOnly;
        m.scene.createEntity("Hero", {}, heroId);
        m.scene.registry().emplace<NetSyncComponent>(m.scene.findByUuid(heroId));
    };
    Machine host(netw), a(netw);
    build(host);
    build(a);
    REQUIRE(host.session.host(7200, 4));
    REQUIRE(a.session.join("127.0.0.1", 7200));
    f64 now = 0.0;
    auto step = [&](int frames, const std::function<void()>& each = {}) {
        for (int i = 0; i < frames; ++i) {
            now += 1.0 / 60.0;
            if (each) each();
            for (Machine* m : {&host, &a}) {
                m->session.update(now);
                m->rep.update(m->scene, m->session, now);
                m->session.takeEvents();
            }
        }
    };
    step(60);
    REQUIRE(a.session.connected());
    auto tr = [](Machine& m, Uuid id) -> TransformComponent& { return m.scene.registry().get<TransformComponent>(m.scene.findByUuid(id)); };
    // A's hero far from the treasure (A owns it).
    REQUIRE(host.rep.setOwner(host.scene, host.session, host.scene.findByUuid(heroId), a.session.localId()));
    tr(a, heroId).position = {100, 0, 0};
    step(30);
    // Idle entities: nothing sent but the once-per-second refresh.
    u32 skipped = 0, sent = 0;
    step(30, [&] { skipped += host.rep.stats().statesSkipped, sent += host.rep.stats().statesSent; });
    CHECK(skipped > sent * 3);
    // Moving the treasure and the secret on the host: A does not see them (out of range / not its own).
    step(60, [&] {
        tr(host, treasureId).position.y += 0.1f;
        tr(host, secretId).position.y += 0.1f;
    });
    CHECK(tr(a, treasureId).position.y == doctest::Approx(0.0f));
    CHECK(tr(a, secretId).position.y == doctest::Approx(0.0f));
    CHECK_FALSE(host.rep.visibleTo(host.scene, host.scene.findByUuid(treasureId), a.session.localId()));
    // A's hero comes near the treasure: it shows up.
    tr(a, heroId).position = {3, 0, 0};
    step(60);
    step(30);
    CHECK(host.rep.visibleTo(host.scene, host.scene.findByUuid(treasureId), a.session.localId()));
    CHECK(tr(a, treasureId).position.y == doctest::Approx(tr(host, treasureId).position.y).epsilon(0.01));
    // Override: the secret is shown to A, the treasure hidden again.
    host.rep.setVisibleTo(host.scene, host.scene.findByUuid(secretId), a.session.localId(), true);
    host.rep.setVisibleTo(host.scene, host.scene.findByUuid(treasureId), a.session.localId(), false);
    u32 hidden = 0;
    step(60, [&] {
        tr(host, treasureId).position.y += 0.1f;
        tr(host, secretId).position.y += 0.1f;
        hidden += host.rep.stats().statesHidden;
    });
    step(30);
    CHECK(tr(a, secretId).position.y == doctest::Approx(tr(host, secretId).position.y).epsilon(0.01));
    CHECK(tr(a, treasureId).position.y < tr(host, treasureId).position.y - 1.0f);
    CHECK(hidden > 0);
}

TEST_CASE("replication: spawnWhenVisible entities exist only for the players that see them") {
    registerRenderComponents();
    registerNetComponents();
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / ("agr_netvis_" + Uuid::generate().toString());
    fs::create_directories(root);
    VirtualFileSystem vfs;
    vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
    PrefabLibrary prefabs(vfs);
    prefabs.install();
    {  // a hidden "Loot" prefab: range visibility, spawned only when visible
        Scene tmp;
        const auto r = tmp.createEntity("Loot");
        auto& sync = tmp.registry().emplace<NetSyncComponent>(r);
        sync.visibility = NetVisibility::Range;
        sync.visibleRange = 10.0f;
        sync.spawnWhenVisible = true;
        REQUIRE(prefabs.save("assets://loot.agrprefab", prefab::fromEntity(tmp, r)));
    }
    LossyNetwork netw;
    netw.loss = 0.0f;
    struct Machine {
        NetSession session;
        Scene scene;
        Replicator rep;
        explicit Machine(LossyNetwork& n) : session(std::make_unique<LossyTransport>(n)) {}
    };
    const Uuid heroId = Uuid::generate();
    Machine host(netw), a(netw);
    for (Machine* m : {&host, &a}) m->scene.registry().emplace<NetSyncComponent>(m->scene.createEntity("Hero", {}, heroId));
    REQUIRE(host.session.host(7300, 4));
    REQUIRE(a.session.join("127.0.0.1", 7300));
    f64 now = 0.0;
    auto step = [&](int frames) {
        for (int i = 0; i < frames; ++i) {
            now += 1.0 / 60.0;
            for (Machine* m : {&host, &a}) {
                m->session.update(now);
                m->rep.update(m->scene, m->session, now);
                m->session.takeEvents();
            }
        }
    };
    step(60);
    REQUIRE(a.session.connected());
    REQUIRE(host.rep.setOwner(host.scene, host.session, host.scene.findByUuid(heroId), a.session.localId()));
    auto hero = [&]() -> TransformComponent& { return a.scene.registry().get<TransformComponent>(a.scene.findByUuid(heroId)); };
    hero().position = {100, 0, 0};
    step(30);
    auto loot = host.rep.spawn(host.scene, host.session, "assets://loot.agrprefab", {0, 0, 0});
    REQUIRE(loot);
    const Uuid lootId = host.scene.uuidOf(loot.value());
    step(30);
    CHECK(a.scene.findByUuid(lootId).isNull());  // far away: not on A's machine at all
    CHECK(host.rep.stats().spawnsHidden == 1);
    hero().position = {3, 0, 0};  // walks near it
    step(40);
    CHECK_FALSE(a.scene.findByUuid(lootId).isNull());
    hero().position = {100, 0, 0};  // walks away
    step(40);
    CHECK(a.scene.findByUuid(lootId).isNull());
    hero().position = {2, 0, 0};
    step(40);
    REQUIRE_FALSE(a.scene.findByUuid(lootId).isNull());
    REQUIRE(host.rep.despawn(host.scene, host.session, loot.value()));
    step(20);
    CHECK(a.scene.findByUuid(lootId).isNull());
    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST_CASE("network clock: clients read the host's time through the pings") {
    LossyNetwork netw;
    netw.loss = 0.0f;
    NetSession host(std::make_unique<LossyTransport>(netw)), client(std::make_unique<LossyTransport>(netw));
    REQUIRE(host.host(7400, 4));
    REQUIRE(client.join("127.0.0.1", 7400));
    // The client's clock starts 1000 s apart from the host's.
    f64 t = 0.0;
    for (int i = 0; i < 180; ++i) {
        t += 1.0 / 60.0;
        host.update(t);
        client.update(t + 1000.0);
        host.takeEvents();
        client.takeEvents();
    }
    REQUIRE(client.connected());
    CHECK(client.clockSynced());
    CHECK(std::fabs(client.networkTime() - host.networkTime()) < 0.05);
    CHECK(host.networkTime() == doctest::Approx(t));
}

TEST_CASE("IPv6: address parsing and a session over ::1") {
    auto a = Address::resolve("::1", 5000);
    REQUIRE(a);
    CHECK(a.value().v6);
    CHECK(a.value().toString() == "[::1]:5000");
    auto b = Address::resolve("[2001:db8::7]", 80);
    REQUIRE(b);
    CHECK(b.value().toString() == "[2001:db8::7]:80");
    CHECK(Address::resolve("127.0.0.1", 1).value().toString() == "127.0.0.1:1");
    CHECK_FALSE(a.value() == Address::resolve("127.0.0.1", 5000).value());

    NetSession host, client;
    REQUIRE(host.host(0, 2));
    REQUIRE(client.join("::1", host.port()));
    f64 now = 0.0;
    if (!pump({&host, &client}, now, [&] { return client.connected(); })) {
        WARN_MESSAGE(false, "no IPv6 loopback on this machine");  // IPv4-only sandbox
        return;
    }
    CHECK(host.peers().size() == 1);
    CHECK(client.send(NetSession::kHostId, "hello v6", true));
    std::vector<NetEvent> got;
    pump({&host, &client}, now, [&] {
        collect(got, host);
        return std::any_of(got.begin(), got.end(), [](const NetEvent& e) { return e.data == "hello v6"; });
    });
    CHECK(std::any_of(got.begin(), got.end(), [](const NetEvent& e) { return e.data == "hello v6"; }));
}

TEST_CASE("prediction: the controller moves at once, the host is the authority, corrections replay pending inputs; lag compensation") {
    registerRenderComponents();
    registerNetComponents();
    LossyNetwork netw;
    netw.loss = 0.2f;  // and reordering
    struct Machine {
        NetSession session;
        Scene scene;
        Replicator rep;
        explicit Machine(LossyNetwork& n) : session(std::make_unique<LossyTransport>(n)) {
            // The game's movement code (a script's onNetInput): 5 m/s along x, jump = +1 y at once.
            rep.setInputHandler([](Scene& sc, ecs::Entity e, const nlohmann::json& in, float dt) {
                auto& t = sc.registry().get<TransformComponent>(e);
                t.position.x += in.value("x", 0.0f) * 5.0f * dt;
                if (in.value("jump", false)) t.position.y += 1.0f;
            });
        }
    };
    const Uuid heroId = Uuid::generate(), targetId = Uuid::generate();
    auto build = [&](Machine& m) {
        const auto hero = m.scene.createEntity("Hero", {}, heroId);
        m.scene.registry().emplace<NetSyncComponent>(hero).rate = 30.0f;
        const auto target = m.scene.createEntity("Target", {}, targetId);
        m.scene.registry().emplace<NetSyncComponent>(target).rate = 30.0f;
    };
    Machine host(netw), client(netw), other(netw);
    build(host);
    build(client);
    build(other);
    REQUIRE(host.session.host(7300, 4));
    REQUIRE(client.session.join("127.0.0.1", 7300));
    REQUIRE(other.session.join("127.0.0.1", 7300));
    f64 now = 0.0;
    std::vector<Machine*> all = {&host, &client, &other};
    auto step = [&](int frames, const std::function<void()>& each = {}) {
        for (int i = 0; i < frames; ++i) {
            now += 1.0 / 60.0;
            if (each) each();
            for (Machine* m : all) {
                m->session.update(now);
                m->rep.update(m->scene, m->session, now);
                m->session.takeEvents();
            }
        }
    };
    step(90);
    REQUIRE(client.session.connected());
    REQUIRE(other.session.connected());
    const auto hostHero = host.scene.findByUuid(heroId), clientHero = client.scene.findByUuid(heroId);
    REQUIRE(host.rep.setController(host.scene, host.session, hostHero, client.session.localId()));
    step(60);
    REQUIRE(client.scene.registry().get<NetSyncComponent>(clientHero).controller == static_cast<i32>(client.session.localId()));
    auto x = [&](Machine& m) { return m.scene.registry().get<TransformComponent>(m.scene.findByUuid(heroId)).position.x; };

    // Prediction: the first input moves the hero here in the same frame, before any round trip.
    const float before = x(client);
    REQUIRE(client.rep.sendInput(client.scene, client.session, clientHero, {{"x", 1.0}}, 1.0f / 60.0f));
    CHECK(x(client) == doctest::Approx(before + 5.0f / 60.0f));
    // 59 more frames of "right", then idle: host and client agree on 5 m despite losses.
    step(59, [&] { client.rep.sendInput(client.scene, client.session, clientHero, {{"x", 1.0}}, 1.0f / 60.0f); });
    step(90, [&] { client.rep.sendInput(client.scene, client.session, clientHero, {{"x", 0.0}}, 1.0f / 60.0f); });
    CHECK(x(host) == doctest::Approx(before + 5.0f).epsilon(0.01));
    CHECK(x(client) == doctest::Approx(x(host)).epsilon(0.001));
    CHECK(x(other) == doctest::Approx(x(host)).epsilon(0.02));  // the third player sees it interpolated
    CHECK(host.rep.stats().inputsApplied >= 140);
    CHECK(client.rep.stats().replayed > 0);
    CHECK(client.rep.stats().lastCorrection < 0.01f);  // the prediction matched the host
    // Only the controller drives it; a client cannot claim more time than 0.25 s per input.
    const float hostX = x(host);
    CHECK_FALSE(other.rep.sendInput(other.scene, other.session, other.scene.findByUuid(heroId), {{"x", 1.0}}, 1.0f / 60.0f));
    client.rep.sendInput(client.scene, client.session, clientHero, {{"x", 1.0}}, 10.0f);
    step(30, [&] { client.rep.sendInput(client.scene, client.session, clientHero, {{"x", 0.0}}, 1.0f / 60.0f); });
    CHECK(x(host) <= hostX + 5.0f * 0.25f + 0.01f);
    CHECK(x(client) == doctest::Approx(x(host)).epsilon(0.001));  // the client is corrected to the host's

    // Network profiler: per-entity traffic and one bandwidth sample per second.
    CHECK(host.rep.profile().at(heroId.value()).statesOut > 0);
    CHECK(client.rep.profile().at(heroId.value()).statesIn > 0);
    CHECK(client.rep.profile().at(heroId.value()).stateBytesIn > 0);
    CHECK(host.session.bandwidthHistory().size() >= 4);
    CHECK(host.session.bandwidthHistory().back().sent > 0);
    // Lag compensation: the host remembers where the target was.
    const auto target = host.scene.findByUuid(targetId);
    const f64 t0 = host.session.networkTime();
    step(30, [&] { host.scene.registry().get<TransformComponent>(target).position.x += 0.1f; });  // 3 m in 0.5 s
    const math::Vec3 past = host.rep.positionAt(host.scene, target, t0 + 0.25);
    CHECK(past.x == doctest::Approx(1.5f).epsilon(0.08));
    CHECK(host.rep.positionAt(host.scene, target, t0 - 5.0).x < 0.11f);  // oldest kept
    CHECK(host.rep.positionAt(host.scene, target, host.session.networkTime() + 1.0).x == doctest::Approx(3.0f));
}

TEST_CASE("unreliable ordered channels: late packets are dropped, channels are independent") {
    LossyNetwork network;
    network.loss = 0.0f;  // the network still reorders datagrams
    NetSession host(std::make_unique<LossyTransport>(network)), client(std::make_unique<LossyTransport>(network));
    REQUIRE(host.host(9300, 2));
    REQUIRE(client.join("127.0.0.1", 9300));
    f64 now = 0.0;
    REQUIRE(pump({&host, &client}, now, [&] { return client.connected(); }));
    host.takeEvents();
    client.takeEvents();
    for (int i = 1; i <= 40; ++i) REQUIRE(client.sendOrdered(NetSession::kHostId, std::to_string(i), 0));
    for (int i = 1; i <= 10; ++i) REQUIRE(client.sendOrdered(NetSession::kHostId, "c" + std::to_string(i), 3));
    std::vector<NetEvent> got;
    pump({&host, &client}, now, [] { return false; }, 5);
    collect(got, host);
    std::vector<int> ch0, ch3;
    for (const auto& e : got)
        if (e.type == NetEvent::Type::Message) {
            if (e.data[0] == 'c') ch3.push_back(std::stoi(e.data.substr(1)));
            else ch0.push_back(std::stoi(e.data));
        }
    // Whatever the arrival order: never an older message after a newer one, per channel.
    CHECK(std::is_sorted(ch0.begin(), ch0.end()));
    CHECK(std::adjacent_find(ch0.begin(), ch0.end()) == ch0.end());
    CHECK(std::is_sorted(ch3.begin(), ch3.end()));
    CHECK_FALSE(ch0.empty());
    CHECK_FALSE(ch3.empty());  // channel 3 is not blocked by channel 0
    CHECK(ch0.size() + ch3.size() + host.stats().staleDropped == 50);  // each one delivered or dropped as stale
    CHECK(host.stats().staleDropped > 0);  // the shuffling did make some late
    // One at a time (nothing to overtake): all delivered.
    got.clear();
    for (int i = 41; i <= 45; ++i) {
        REQUIRE(client.sendOrdered(NetSession::kHostId, std::to_string(i), 0));
        pump({&host, &client}, now, [] { return false; }, 2);
    }
    collect(got, host);
    int delivered = 0;
    for (const auto& e : got) delivered += e.type == NetEvent::Type::Message;
    CHECK(delivered == 5);
}

TEST_CASE("compression: smaller packets, same messages, also with encryption and losses") {
    for (bool encrypt : {false, true}) {
        LossyNetwork network;
        network.loss = 0.15f;
        NetSession host(std::make_unique<LossyTransport>(network)), client(std::make_unique<LossyTransport>(network));
        host.setPassword("raclette");
        client.setPassword("raclette");
        host.setEncryption(encrypt);
        client.setCompression(true);  // only the client compresses: the host reads it anyway
        REQUIRE(host.host(9400, 2));
        REQUIRE(client.join("127.0.0.1", 9400));
        f64 now = 0.0;
        REQUIRE(pump({&host, &client}, now, [&] { return client.connected(); }));
        std::string state;
        for (int i = 0; i < 12; ++i) state += R"({"kind":"state","id":)" + std::to_string(i) + R"(,"x":12.5,"y":3.25,"hp":100,"team":"red"},)";
        const std::string big(6000, 'Q');
        std::vector<NetEvent> got;
        REQUIRE(client.send(NetSession::kHostId, state, true));
        REQUIRE(client.send(NetSession::kHostId, big, true));
        REQUIRE(pump({&host, &client}, now, [&] {
            collect(got, host);
            int n = 0;
            for (const auto& e : got) n += e.type == NetEvent::Type::Message && (e.data == state || e.data == big);
            return n == 2;
        }));
        const auto& st = client.stats();
        CHECK(st.bytesAfterCompression * 3 < st.bytesBeforeCompression);
        CHECK(client.encrypted(NetSession::kHostId) == encrypt);
    }
}

#include "agr/net/Http.hpp"
#include "agr/net/Sockets.hpp"
#include <thread>

TEST_CASE("sockets: TCP server and connection, UDP datagrams, on loopback") {
    auto server = TcpServer::listen(0, true);
    REQUIRE(server);
    auto client = TcpConnection::connect("127.0.0.1", server.value()->port());
    REQUIRE(client);
    std::unique_ptr<TcpConnection> peer;
    std::string atServer, atClient;
    const std::string big(200000, 'B');  // more than a socket buffer: sent over several polls
    client.value()->send("hello server");
    for (int i = 0; i < 400 && (atServer.size() < 12 + big.size() || atClient != "hello client"); ++i) {
        if (!peer) {
            peer = server.value()->accept();
            if (peer) peer->send("hello client");
        }
        client.value()->poll();
        if (peer) {
            peer->poll();
            atServer += peer->receive();
        }
        atClient += client.value()->receive();
        if (i == 5) client.value()->send(big);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(client.value()->connected());
    CHECK(atClient == "hello client");
    CHECK(atServer == "hello server" + big);
    // The other side closes: seen as closed.
    peer.reset();
    for (int i = 0; i < 100 && !client.value()->closed(); ++i) {
        client.value()->poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(client.value()->closed());
    CHECK_FALSE(TcpConnection::connect("no-such-host.invalid", 80));

    auto a = UdpSocket::open(0), b = UdpSocket::open(0);
    REQUIRE(a);
    REQUIRE(b);
    REQUIRE(a.value()->sendTo("127.0.0.1", b.value()->port(), "ping"));
    std::vector<UdpSocket::Datagram> got;
    for (int i = 0; i < 100 && got.empty(); ++i) {
        got = b.value()->receive();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(got.size() == 1);
    CHECK(got[0].data == "ping");
    CHECK(got[0].port == a.value()->port());
    CHECK(got[0].host == "127.0.0.1");
}

TEST_CASE("WebSocket server: the engine's own WebSocket client connects, text and binary both ways, large and fragmented-size messages, ping, close") {
    auto server = WebSocketServer::listen(0, true);
    REQUIRE(server);
    WebSocketServer& s = *server.value();
    WebSocket ws;
    ws.connect("ws://127.0.0.1:" + std::to_string(s.port()) + "/game");
    std::vector<WebSocketServer::Event> events;
    auto pumpUntil = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 1500 && !done(); ++i) {
            s.poll();
            for (auto& e : s.takeEvents()) events.push_back(std::move(e));
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return done();
    };
    REQUIRE(pumpUntil([&] { return ws.state() == WebSocket::State::Open && !events.empty(); }));
    REQUIRE(events[0].type == WebSocketServer::Event::Type::Connected);
    const u32 id = events[0].client;
    CHECK(s.clients() == std::vector<u32>{id});
    // Client -> server: text, binary, 100 KB (64-bit length).
    const std::string big(100000, 'x');
    ws.send("bonjour");
    ws.sendBinary(std::string("\x00\x01\x02", 3));
    ws.send(big);
    REQUIRE(pumpUntil([&] { return events.size() >= 4; }));
    CHECK(events[1].data == "bonjour");
    CHECK_FALSE(events[1].binary);
    CHECK(events[2].binary);
    CHECK(events[2].data == std::string("\x00\x01\x02", 3));
    CHECK(events[3].data == big);
    // Server -> client: text, a 70 KB message, broadcast.
    REQUIRE(s.send(id, "salut"));
    REQUIRE(s.send(id, std::string(70000, 'y')));
    s.broadcast("everyone");
    std::vector<WebSocket::Message> received;
    REQUIRE(pumpUntil([&] {
        for (auto& m : ws.receive()) received.push_back(std::move(m));
        return received.size() >= 3;
    }));
    CHECK(received[0].data == "salut");
    CHECK(received[1].data.size() == 70000);
    CHECK(received[2].data == "everyone");
    // The client closes: the server reports it.
    ws.close();
    REQUIRE(pumpUntil([&] { return events.back().type == WebSocketServer::Event::Type::Disconnected; }));
    CHECK(s.clients().empty());
    // Not a WebSocket request: refused.
    auto raw = TcpConnection::connect("127.0.0.1", s.port());
    REQUIRE(raw);
    raw.value()->send("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
    std::string answer;
    pumpUntil([&] {
        raw.value()->poll();
        answer += raw.value()->receive();
        return answer.find("400") != std::string::npos;
    });
    CHECK(answer.find("400 Bad Request") != std::string::npos);
}

#include "agr/net/Upnp.hpp"
#include <atomic>
#include <mutex>

TEST_CASE("UPnP: router found by SSDP, description read, public address, port mapping added and removed (fake router on loopback)") {
    // A fake Internet gateway: SSDP answers on UDP, description and SOAP control over HTTP.
    auto ssdp = UdpSocket::open(0);
    auto http = TcpServer::listen(0, true);
    REQUIRE(ssdp);
    REQUIRE(http);
    const u16 httpPort = http.value()->port();
    std::atomic<bool> stop{false};
    std::mutex mutex;
    std::vector<std::string> soapCalls;
    std::set<u16> mapped;
    std::thread router([&] {
        std::vector<std::unique_ptr<TcpConnection>> conns;
        std::vector<std::string> buffers;
        while (!stop) {
            for (const auto& d : ssdp.value()->receive())
                if (d.data.rfind("M-SEARCH", 0) == 0)
                    ssdp.value()->sendTo(d.host, d.port, "HTTP/1.1 200 OK\r\nST: upnp:rootdevice\r\nLocation: http://127.0.0.1:" + std::to_string(httpPort) + "/rootDesc.xml\r\n\r\n");
            while (auto c = http.value()->accept()) {
                conns.push_back(std::move(c));
                buffers.emplace_back();
            }
            for (usize i = 0; i < conns.size(); ++i) {
                conns[i]->poll();
                buffers[i] += conns[i]->receive();
                const usize headEnd = buffers[i].find("\r\n\r\n");
                if (headEnd == std::string::npos) continue;
                const std::string head = buffers[i].substr(0, headEnd);
                usize length = 0;
                if (const usize cl = head.find("Content-Length: "); cl != std::string::npos) length = std::stoul(head.substr(cl + 16));
                if (buffers[i].size() < headEnd + 4 + length) continue;
                const std::string body = buffers[i].substr(headEnd + 4, length);
                buffers[i].clear();
                std::string reply, status = "200 OK";
                if (head.rfind("GET /rootDesc.xml", 0) == 0) {
                    reply = "<?xml version=\"1.0\"?><root><device><deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType><deviceList><device>"
                            "<serviceList><service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
                            "<controlURL>/ctl/IPConn</controlURL></service></serviceList></device></deviceList></device></root>";
                } else if (head.rfind("POST /ctl/IPConn", 0) == 0) {
                    std::lock_guard lock(mutex);
                    soapCalls.push_back(head + "\n" + body);
                    auto value = [&](const char* tag) {
                        const std::string open = std::string("<") + tag + ">", close = std::string("</") + tag + ">";
                        const usize a = body.find(open), b = body.find(close);
                        return a == std::string::npos ? std::string() : body.substr(a + open.size(), b - a - open.size());
                    };
                    if (head.find("#GetExternalIPAddress") != std::string::npos) {
                        reply = "<s:Envelope><s:Body><u:GetExternalIPAddressResponse><NewExternalIPAddress>203.0.113.7</NewExternalIPAddress></u:GetExternalIPAddressResponse></s:Body></s:Envelope>";
                    } else if (head.find("#AddPortMapping") != std::string::npos) {
                        mapped.insert(static_cast<u16>(std::stoi(value("NewExternalPort"))));
                    } else if (head.find("#DeletePortMapping") != std::string::npos) {
                        if (!mapped.erase(static_cast<u16>(std::stoi(value("NewExternalPort"))))) {
                            status = "500 Internal Server Error";
                            reply = "<s:Envelope><s:Body><s:Fault><detail><UPnPError><errorCode>714</errorCode><errorDescription>NoSuchEntryInArray</errorDescription></UPnPError></detail></s:Fault></s:Body></s:Envelope>";
                        }
                    }
                }
                conns[i]->send("HTTP/1.1 " + status + "\r\nContent-Type: text/xml\r\nContent-Length: " + std::to_string(reply.size()) + "\r\nConnection: close\r\n\r\n" + reply);
                conns[i]->poll();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    auto gateway = UpnpGateway::discover(3.0, "127.0.0.1", ssdp.value()->port());
    REQUIRE_MESSAGE(gateway, (gateway ? "" : gateway.error().message));
    CHECK(gateway.value().controlUrl() == "http://127.0.0.1:" + std::to_string(httpPort) + "/ctl/IPConn");
    CHECK(gateway.value().serviceType() == "urn:schemas-upnp-org:service:WANIPConnection:1");
    CHECK(gateway.value().localAddress() == "127.0.0.1");
    auto ip = gateway.value().externalAddress();
    REQUIRE(ip);
    CHECK(ip.value() == "203.0.113.7");
    REQUIRE(gateway.value().addPortMapping(7777, 7777, "UDP", "Mon jeu <AGR>"));
    {
        std::lock_guard lock(mutex);
        CHECK(mapped.count(7777) == 1);
        const std::string& add = soapCalls.back();
        CHECK(add.find("SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#AddPortMapping\"") != std::string::npos);
        CHECK(add.find("<NewInternalClient>127.0.0.1</NewInternalClient>") != std::string::npos);
        CHECK(add.find("<NewProtocol>UDP</NewProtocol>") != std::string::npos);
        CHECK(add.find("Mon jeu &lt;AGR&gt;") != std::string::npos);  // escaped
    }
    REQUIRE(gateway.value().deletePortMapping(7777, "UDP"));
    auto again = gateway.value().deletePortMapping(7777, "UDP");
    REQUIRE_FALSE(again);
    CHECK(again.error().message.find("NoSuchEntryInArray") != std::string::npos);
    stop = true;
    router.join();
    // Nobody answers: a clear failure after the timeout.
    auto none = UdpSocket::open(0);
    REQUIRE(none);
    auto missing = UpnpGateway::discover(0.3, "127.0.0.1", none.value()->port());
    REQUIRE_FALSE(missing);
    CHECK(missing.error().message.find("no UPnP router") != std::string::npos);
}

TEST_CASE("RPC delivery modes: unreliable and ordered RPCs arrive and are relayed by the host") {
    registerRenderComponents();
    registerNetComponents();
    LossyNetwork netw;
    netw.loss = 0.0f;
    struct Machine {
        NetSession session;
        Scene scene;
        Replicator rep;
        std::vector<std::string> calls;
        explicit Machine(LossyNetwork& n) : session(std::make_unique<LossyTransport>(n)) {
            rep.setRpcHandler([this](Scene&, ecs::Entity, const std::string& fn, const nlohmann::json& args, u32) { calls.push_back(fn + args.dump()); });
        }
    };
    const Uuid id = Uuid::generate();
    Machine host(netw), a(netw), b(netw);
    for (Machine* m : {&host, &a, &b}) m->scene.registry().emplace<NetSyncComponent>(m->scene.createEntity("Ship", {}, id));
    REQUIRE(host.session.host(7400, 4));
    REQUIRE(a.session.join("127.0.0.1", 7400));
    REQUIRE(b.session.join("127.0.0.1", 7400));
    f64 now = 0.0;
    auto step = [&](int frames) {
        for (int i = 0; i < frames; ++i) {
            now += 1.0 / 60.0;
            for (Machine* m : {&host, &a, &b}) {
                m->session.update(now);
                m->rep.update(m->scene, m->session, now);
                m->session.takeEvents();
            }
        }
    };
    step(60);
    REQUIRE(b.session.connected());
    const auto ship = a.scene.findByUuid(id);
    REQUIRE(a.rep.rpc(a.scene, a.session, ship, "aim", {1}, 0, Replicator::Delivery::Unreliable));
    REQUIRE(a.rep.rpc(a.scene, a.session, ship, "move", {2}, 0, Replicator::Delivery::Ordered, 3));
    step(10);
    CHECK(std::find(host.calls.begin(), host.calls.end(), "aim[1]") != host.calls.end());
    CHECK(std::find(b.calls.begin(), b.calls.end(), "move[2]") != b.calls.end());  // relayed by the host, same delivery
    CHECK(std::find(b.calls.begin(), b.calls.end(), "aim[1]") != b.calls.end());
}
