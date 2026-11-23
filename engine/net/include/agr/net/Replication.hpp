#pragma once
// High-level multiplayer on top of NetSession (like Godot's MultiplayerSynchronizer,
// MultiplayerSpawner and RPCs): entities with a NetSync component are kept in step on every
// machine, prefabs are spawned on all of them, and scripts call functions remotely.
//
//  * Identity: an entity is the same on every machine through its scene id (Uuid): scenes are
//    loaded identically, and network-spawned prefabs get the same id everywhere.
//  * Authority: each NetSync entity has an owner (peer id; 1 = the host). The owner simulates
//    it and sends its state; the others interpolate it. Only the host spawns, despawns and
//    changes owners; it relays what clients send to the other clients.
//  * Late joiners receive the spawned entities and the owners when they connect.
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "agr/net/NetSession.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::net {

/// Keeps an entity in step over the network: its Transform (interpolated on the machines that
/// do not own it) and reflected fields ("Health.value;SpriteRenderer.color").
enum class NetVisibility : i32 { All = 0, OwnerOnly = 1, Range = 2 };

struct NetSyncComponent {
    i32 owner = 1;              ///< Peer that simulates the entity (1 = host).
    bool syncTransform = true;
    bool interpolate = true;    ///< Smooth between received states (else snap).
    float rate = 20.0f;         ///< States sent per second by the owner.
    std::string fields;         ///< "Component.field" separated by ';' (numbers, booleans, vectors, colors, texts).
    /// Who receives its state (host side): everyone, only its owner, or the players that own an
    /// entity within `visibleRange` of it (fog of war, big worlds). Replicator::setVisibleTo
    /// overrides it per player.
    NetVisibility visibility = NetVisibility::All;
    float visibleRange = 50.0f;
    /// Sends a state only when it changed (plus one per second, against losses): idle entities
    /// cost nothing.
    bool onlyChanges = true;
    /// Network-spawned entities: exist only on the machines of the players that can see them
    /// (visibility above): spawned for a player when it starts seeing them, removed when it stops
    /// (hidden units, loot, big worlds). Off: they exist everywhere and only states are filtered.
    bool spawnWhenVisible = false;
    /// Server-authoritative movement with client prediction: the peer whose inputs drive the
    /// entity (0 = none). The host owns and simulates it from those inputs; the controller applies
    /// each input at once (prediction), then takes the host's state and replays the inputs the
    /// host has not processed yet (reconciliation): no delay for the player, no cheating on position.
    i32 controller = 0;
};

void registerNetComponents();

struct ReplicationStats {
    u32 synced = 0;          ///< NetSync entities this frame.
    u32 statesSent = 0;      ///< Entity states sent this frame.
    u32 statesReceived = 0;  ///< Entity states applied this frame.
    u32 spawned = 0;         ///< Network-spawned entities alive.
    u32 rpcsSent = 0, rpcsReceived = 0;  ///< Since start.
    u32 statesSkipped = 0;   ///< Unchanged states not sent this frame (onlyChanges).
    u32 statesHidden = 0;    ///< States not sent to a player that cannot see the entity, this frame.
    u32 spawnsHidden = 0;    ///< (Entity, player) pairs not spawned there because hidden (spawnWhenVisible).
    u32 inputsSent = 0, inputsApplied = 0;  ///< Prediction: inputs sent (controller) / applied (host), since start.
    u32 replayed = 0;        ///< Inputs replayed after a correction, since start.
    float lastCorrection = 0.0f;  ///< Distance between the predicted and the host's position at the last state.
};

class Replicator {
public:
    /// Called for a remote procedure call to run here: entity, function, arguments, sender.
    using RpcHandler = std::function<void(Scene&, ecs::Entity, const std::string& function, const nlohmann::json& args, u32 sender)>;
    void setRpcHandler(RpcHandler handler) { m_rpc = std::move(handler); }
    /// Applies one input to an entity (the script's onNetInput(input, dt)); used for prediction,
    /// on the host, and for the replay after a correction.
    using InputHandler = std::function<void(Scene&, ecs::Entity, const nlohmann::json& input, float dt)>;
    void setInputHandler(InputHandler handler) { m_input = std::move(handler); }
    /// Controller: applies `input` here at once and sends it to the host (redundantly, unreliable).
    /// The host itself (or a machine without a session) just applies it. False if this machine
    /// does not control the entity.
    bool sendInput(Scene& scene, NetSession& session, ecs::Entity e, const nlohmann::json& input, float dt);
    /// Host: `peer` drives the entity with its inputs (prediction on its machine); the host keeps
    /// owning and simulating it. 0 = nobody.
    bool setController(Scene& scene, NetSession& session, ecs::Entity e, u32 peer);
    /// Host (lag compensation): where the entity was at `time` (host clock, see
    /// NetSession::networkTime) during the last second; its current position otherwise.
    math::Vec3 positionAt(Scene& scene, ecs::Entity e, f64 time) const;
    /// Interpolation delay of the others' entities shown here (average state interval). What a
    /// client sees is about networkTime() - rtt / 2 - viewDelay() on the host's clock: it stamps
    /// its shots with that (agr.net.viewTime()) for positionAt.
    f64 viewDelay() const { return m_viewDelay; }

    /// Every frame after NetSession::update: applies what arrived (states, spawns, owners,
    /// RPCs), sends the states of the entities this machine owns, interpolates the others.
    void update(Scene& scene, NetSession& session, f64 now);
    /// Host: instantiates a prefab here and on every machine (late joiners included).
    Result<ecs::Entity> spawn(Scene& scene, NetSession& session, const std::string& prefab, math::Vec3 position, u32 owner = NetSession::kHostId);
    /// Host: destroys a network-spawned (or scene) entity everywhere.
    bool despawn(Scene& scene, NetSession& session, ecs::Entity e);
    /// Host: gives an entity to a peer (it then sends its state).
    bool setOwner(Scene& scene, NetSession& session, ecs::Entity e, u32 owner);
    u32 ownerOf(Scene& scene, ecs::Entity e) const;
    /// Host: shows / hides an entity to one player whatever its NetSync visibility (0 = clear).
    void setVisibleTo(Scene& scene, ecs::Entity e, u32 peer, bool visible);
    void clearVisibility(Scene& scene, ecs::Entity e);
    /// Host: whether `peer` receives the state of `e`.
    bool visibleTo(Scene& scene, ecs::Entity e, u32 peer) const;
    bool isOwner(Scene& scene, NetSession& session, ecs::Entity e) const;
    /// Calls `function` of the entity's script on `target` (0 = every other machine, 1 = host,
    /// n = that peer), through the host. Reliable and in order.
    bool rpc(Scene& scene, NetSession& session, ecs::Entity e, const std::string& function, const nlohmann::json& args, u32 target = 0);
    /// Peer whose RPC is running (0 outside an RPC).
    u32 rpcSender() const { return m_sender; }
    /// Forgets the session's state (game stopped, scene changed).
    void reset();
    const ReplicationStats& stats() const { return m_stats; }
    /// Network profiler: traffic per entity since start (or the last clearProfile()).
    struct EntityTraffic {
        u64 stateBytesOut = 0, stateBytesIn = 0;
        u32 statesOut = 0, statesIn = 0;
        u32 rpcsOut = 0, rpcsIn = 0;
        std::unordered_map<std::string, u32> rpcCalls;  ///< function -> calls (both ways)
    };
    const std::unordered_map<u64, EntityTraffic>& profile() const { return m_profile; }  ///< uuid -> traffic
    void clearProfile() { m_profile.clear(); }

private:
    struct Remote {
        math::Vec3 p0, p1, s0{1.0f}, s1{1.0f};
        math::Quat r0, r1;
        f64 received = -1.0, interval = 0.05;
    };
    struct Spawned {
        std::string prefab;
        math::Vec3 position;
    };
    void handleControl(Scene& scene, NetSession& session, u32 from, const nlohmann::json& msg);
    void handleState(Scene& scene, u32 from, std::string_view data, f64 now);
    void sendControl(NetSession& session, u32 peer, const nlohmann::json& msg);
    void runRpc(Scene& scene, ecs::Entity e, const std::string& fn, const nlohmann::json& args, u32 sender);
    ecs::Entity instantiate(Scene& scene, const std::string& prefab, Uuid id, math::Vec3 position, u32 owner);

    struct PendingInput {
        u32 seq;
        float dt;
        std::string input;  // JSON
    };
    struct Prediction {
        u32 nextSeq = 1;
        std::deque<PendingInput> pending;  // controller: not yet acknowledged
        u32 processed = 0;                 // host: last input applied
    };
    void handleInput(Scene& scene, u32 from, std::string_view data);
    RpcHandler m_rpc;
    InputHandler m_input;
    std::unordered_map<u64, EntityTraffic> m_profile;
    std::unordered_map<u64, Prediction> m_prediction;  // uuid -> inputs
    std::unordered_map<u64, std::deque<std::pair<f64, math::Vec3>>> m_history;  // host: uuid -> (time, position), 1 s
    f64 m_viewDelay = 0.05;
    u32 m_localId = 0;
    std::unordered_map<u64, Remote> m_remote;   // uuid -> interpolation
    std::unordered_map<u64, f64> m_lastSent;    // uuid -> time of its last state
    std::unordered_map<u64, std::string> m_lastBytes;  // uuid -> last state sent (onlyChanges)
    std::unordered_map<u64, f64> m_lastFull;    // uuid -> time of its last sent state, changed or not
    std::unordered_map<u64, std::unordered_map<u32, bool>> m_visibility;  // uuid -> peer -> forced
    std::unordered_map<u64, std::unordered_map<u32, bool>> m_wasVisible;
    std::unordered_map<u64, std::unordered_map<u32, bool>> m_spawnedFor;  // uuid -> peer -> exists there (spawnWhenVisible)  // uuid -> peer -> visible last frame
    std::unordered_map<u64, Spawned> m_spawned; // uuid -> prefab (host: for late joiners)
    u32 m_sender = 0;
    u64 m_sceneGeneration = 0;
    Scene* m_scene = nullptr;
    ReplicationStats m_stats;
};

} // namespace agr::net
