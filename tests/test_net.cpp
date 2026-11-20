// Networking: real UDP sessions on the loopback interface, reliable ordered delivery over a
// lossy, reordering transport, capacity, disconnects and timeouts; agr.net from Lua.
#include <doctest/doctest.h>

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
