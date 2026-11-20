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
#include <functional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "agr/net/NetSession.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::net {

/// Keeps an entity in step over the network: its Transform (interpolated on the machines that
/// do not own it) and reflected fields ("Health.value;SpriteRenderer.color").
struct NetSyncComponent {
    i32 owner = 1;              ///< Peer that simulates the entity (1 = host).
    bool syncTransform = true;
    bool interpolate = true;    ///< Smooth between received states (else snap).
    float rate = 20.0f;         ///< States sent per second by the owner.
    std::string fields;         ///< "Component.field" separated by ';' (numbers, booleans, vectors, colors, texts).
};

void registerNetComponents();

struct ReplicationStats {
    u32 synced = 0;          ///< NetSync entities this frame.
    u32 statesSent = 0;      ///< Entity states sent this frame.
    u32 statesReceived = 0;  ///< Entity states applied this frame.
    u32 spawned = 0;         ///< Network-spawned entities alive.
    u32 rpcsSent = 0, rpcsReceived = 0;  ///< Since start.
};

class Replicator {
public:
    /// Called for a remote procedure call to run here: entity, function, arguments, sender.
    using RpcHandler = std::function<void(Scene&, ecs::Entity, const std::string& function, const nlohmann::json& args, u32 sender)>;
    void setRpcHandler(RpcHandler handler) { m_rpc = std::move(handler); }

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
    bool isOwner(Scene& scene, NetSession& session, ecs::Entity e) const;
    /// Calls `function` of the entity's script on `target` (0 = every other machine, 1 = host,
    /// n = that peer), through the host. Reliable and in order.
    bool rpc(Scene& scene, NetSession& session, ecs::Entity e, const std::string& function, const nlohmann::json& args, u32 target = 0);
    /// Peer whose RPC is running (0 outside an RPC).
    u32 rpcSender() const { return m_sender; }
    /// Forgets the session's state (game stopped, scene changed).
    void reset();
    const ReplicationStats& stats() const { return m_stats; }

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

    RpcHandler m_rpc;
    std::unordered_map<u64, Remote> m_remote;   // uuid -> interpolation
    std::unordered_map<u64, f64> m_lastSent;    // uuid -> time of its last state
    std::unordered_map<u64, Spawned> m_spawned; // uuid -> prefab (host: for late joiners)
    u32 m_sender = 0;
    u64 m_sceneGeneration = 0;
    Scene* m_scene = nullptr;
    ReplicationStats m_stats;
};

} // namespace agr::net
