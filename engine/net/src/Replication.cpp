#include "agr/net/Replication.hpp"

#include <algorithm>
#include <cstring>

#include "agr/core/Log.hpp"
#include "agr/scene/Components.hpp"
#include "agr/scene/Prefab.hpp"
#include "agr/scene/Reflection.hpp"

namespace agr::net {

using json = nlohmann::json;
using math::Quat;
using math::Vec3;

namespace {

constexpr char kState = 'S', kControl = 'J', kInput = 'I';
constexpr usize kInputRedundancy = 8;  // each input packet repeats the last unacknowledged ones (losses)
constexpr usize kPacketBudget = NetSession::kMaxPayload - 16;

void putF(std::string& out, float v) { out.append(reinterpret_cast<const char*>(&v), 4); }
void putU64(std::string& out, u64 v) { out.append(reinterpret_cast<const char*>(&v), 8); }
void putU16(std::string& out, u16 v) { out.append(reinterpret_cast<const char*>(&v), 2); }
struct Reader {
    std::string_view d;
    usize at = 0;
    bool ok = true;
    template <typename T>
    T get() {
        T v{};
        if (at + sizeof(T) > d.size()) {
            ok = false;
            return v;
        }
        std::memcpy(&v, d.data() + at, sizeof(T));
        at += sizeof(T);
        return v;
    }
};

// ---- reflected fields as JSON (numbers, booleans, vectors, quaternions, texts, enums)
struct FieldRef {
    const ComponentInfo* component;
    const FieldInfo* field;
};
std::vector<FieldRef> parseFields(const std::string& spec) {
    std::vector<FieldRef> out;
    usize start = 0;
    while (start <= spec.size()) {
        usize end = spec.find(';', start);
        if (end == std::string::npos) end = spec.size();
        const std::string item = spec.substr(start, end - start);
        if (const usize dot = item.find('.'); dot != std::string::npos) {
            const ComponentInfo* c = ComponentRegistry::instance().find(std::string_view(item).substr(0, dot));
            const FieldInfo* f = c ? c->field(std::string_view(item).substr(dot + 1)) : nullptr;
            if (f) out.push_back({c, f});
        }
        start = end + 1;
    }
    return out;
}
void* componentOf(Scene& scene, ecs::Entity e, const ComponentInfo& c) {
    ecs::PoolBase* pool = scene.registry().findPool(c.id);
    return pool ? pool->getRaw(e) : nullptr;
}
json readField(void* comp, const FieldInfo& f) {
    switch (f.type) {
        case FieldType::Float: return f.ref<float>(comp);
        case FieldType::Int32:
        case FieldType::Enum: return f.ref<i32>(comp);
        case FieldType::UInt32: return f.ref<u32>(comp);
        case FieldType::Bool: return f.ref<bool>(comp);
        case FieldType::String: return f.ref<std::string>(comp);
        case FieldType::Vec2: { auto& v = f.ref<math::Vec2>(comp); return json::array({v.x, v.y}); }
        case FieldType::Vec3: { auto& v = f.ref<math::Vec3>(comp); return json::array({v.x, v.y, v.z}); }
        case FieldType::Vec4: { auto& v = f.ref<math::Vec4>(comp); return json::array({v.x, v.y, v.z, v.w}); }
        case FieldType::Quat: { auto& v = f.ref<math::Quat>(comp); return json::array({v.x, v.y, v.z, v.w}); }
        default: return nullptr;
    }
}
void writeField(void* comp, const FieldInfo& f, const json& j) {
    try {
        auto arr = [&](int n, float* dst) {
            if (j.is_array() && static_cast<int>(j.size()) == n)
                for (int i = 0; i < n; ++i) dst[i] = j[static_cast<usize>(i)].get<float>();
        };
        switch (f.type) {
            case FieldType::Float: if (j.is_number()) f.ref<float>(comp) = j.get<float>(); break;
            case FieldType::Int32:
            case FieldType::Enum: if (j.is_number()) f.ref<i32>(comp) = j.get<i32>(); break;
            case FieldType::UInt32: if (j.is_number()) f.ref<u32>(comp) = j.get<u32>(); break;
            case FieldType::Bool: if (j.is_boolean()) f.ref<bool>(comp) = j.get<bool>(); break;
            case FieldType::String: if (j.is_string()) f.ref<std::string>(comp) = j.get<std::string>(); break;
            case FieldType::Vec2: arr(2, &f.ref<math::Vec2>(comp).x); break;
            case FieldType::Vec3: arr(3, &f.ref<math::Vec3>(comp).x); break;
            case FieldType::Vec4: arr(4, &f.ref<math::Vec4>(comp).x); break;
            case FieldType::Quat: arr(4, &f.ref<math::Quat>(comp).x); break;
            default: break;
        }
    } catch (const json::exception&) {
    }
}

} // namespace

void registerNetComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<NetSyncComponent>()) return;
    reg.add<NetSyncComponent>("NetSync")
        .category("Network")
        .description("Kept in step over the network: Transform (interpolated) and listed fields, sent by its owner.")
        .field("owner", &NetSyncComponent::owner, {.tooltip = "Peer that simulates it (1 = host); set by the host at run time"})
        .field("syncTransform", &NetSyncComponent::syncTransform)
        .field("interpolate", &NetSyncComponent::interpolate, {.tooltip = "Smooth between received states"})
        .field("controller", &NetSyncComponent::controller, {.tooltip = "Peer whose inputs drive it (host-authoritative, predicted on that peer); 0 = none"})
        .field("rate", &NetSyncComponent::rate, {.tooltip = "States per second", .rangeMin = 1.0f, .rangeMax = 120.0f, .speed = 0.5f})
        .field("fields", &NetSyncComponent::fields, {.tooltip = "Component.field separated by ';' (e.g. Health.value;SpriteRenderer.color)"})
        .field("visibility", &NetSyncComponent::visibility,
               {.tooltip = "Who receives it: everyone, its owner only, players owning an entity within visibleRange",
                .enumEntries = {{"All", 0}, {"OwnerOnly", 1}, {"Range", 2}}})
        .field("visibleRange", &NetSyncComponent::visibleRange, {.rangeMin = 0.0f, .rangeMax = 100000.0f, .speed = 0.5f})
        .field("onlyChanges", &NetSyncComponent::onlyChanges, {.tooltip = "Send only when the state changed (and once per second)"})
        .field("spawnWhenVisible", &NetSyncComponent::spawnWhenVisible, {.tooltip = "Network-spawned: exists only for the players that can see it"});
}

void Replicator::setVisibleTo(Scene& scene, ecs::Entity e, u32 peer, bool visible) {
    if (scene.valid(e)) m_visibility[scene.uuidOf(e).value()][peer] = visible;
}

void Replicator::clearVisibility(Scene& scene, ecs::Entity e) {
    if (scene.valid(e)) m_visibility.erase(scene.uuidOf(e).value());
}

bool Replicator::visibleTo(Scene& scene, ecs::Entity e, u32 peer) const {
    if (!scene.valid(e)) return false;
    const auto* s = scene.registry().tryGet<NetSyncComponent>(e);
    if (!s) return false;
    const u32 owner = s->owner > 0 ? static_cast<u32>(s->owner) : NetSession::kHostId;
    if (owner == peer) return true;
    if (auto it = m_visibility.find(scene.uuidOf(e).value()); it != m_visibility.end())
        if (auto f = it->second.find(peer); f != it->second.end()) return f->second;
    switch (s->visibility) {
        case NetVisibility::All: return true;
        case NetVisibility::OwnerOnly: return false;
        case NetVisibility::Range: {
            // Near any entity the player owns (no entity: everything is visible).
            const math::Vec3 p = scene.worldMatrix(e).translation();
            bool owns = false, near = false;
            scene.registry().view<NetSyncComponent>().each([&](ecs::Entity o, NetSyncComponent& os) {
                if (near || os.owner != static_cast<i32>(peer)) return;
                owns = true;
                near = math::length(scene.worldMatrix(o).translation() - p) <= s->visibleRange;
            });
            return !owns || near;
        }
    }
    return true;
}

void Replicator::reset() {
    m_remote.clear();
    m_lastSent.clear();
    m_lastBytes.clear();
    m_lastFull.clear();
    m_visibility.clear();
    m_wasVisible.clear();
    m_spawnedFor.clear();
    m_spawned.clear();
    m_prediction.clear();
    m_history.clear();
    m_sender = 0;
}

bool Replicator::sendInput(Scene& scene, NetSession& session, ecs::Entity e, const json& input, float dt) {
    if (!scene.valid(e)) return false;
    const auto* s = scene.registry().tryGet<NetSyncComponent>(e);
    if (!session.connected()) {  // single player: the same script code, applied directly
        if (m_input) m_input(scene, e, input, dt);
        return true;
    }
    const u32 local = session.localId();
    if (!s || (s->controller != static_cast<i32>(local) && !(local == NetSession::kHostId && s->controller <= 1))) return false;
    if (m_input) m_input(scene, e, input, dt);  // applied here at once
    if (session.role() == NetRole::Host) return true;  // the host is the authority itself
    Prediction& pr = m_prediction[scene.uuidOf(e).value()];
    pr.pending.push_back({pr.nextSeq++, dt, input.dump()});
    if (pr.pending.size() > 256) pr.pending.pop_front();  // the host stopped answering: forget the oldest
    // Packet: tag, kind, uuid, count, then (seq, dt, length, json) newest last.
    std::string packet(1, NetSession::kSystemTag);
    packet += kInput;
    putU64(packet, scene.uuidOf(e).value());
    const usize first = pr.pending.size() > kInputRedundancy ? pr.pending.size() - kInputRedundancy : 0;
    std::string body;
    u16 n = 0;
    for (usize i = first; i < pr.pending.size(); ++i) {
        const PendingInput& p = pr.pending[i];
        if (body.size() + p.input.size() + 10 > kPacketBudget - 16) continue;
        body.append(reinterpret_cast<const char*>(&p.seq), 4);
        putF(body, p.dt);
        putU16(body, static_cast<u16>(p.input.size()));
        body += p.input;
        ++n;
    }
    putU16(packet, n);
    packet += body;
    session.send(NetSession::kHostId, packet, false);
    ++m_stats.inputsSent;
    return true;
}

void Replicator::handleInput(Scene& scene, u32 from, std::string_view data) {
    Reader r{data, 0};
    const u64 uuid = r.get<u64>();
    const u16 count = r.get<u16>();
    const ecs::Entity e = scene.findByUuid(Uuid(uuid));
    if (!r.ok || e.isNull()) return;
    const auto* s = scene.registry().tryGet<NetSyncComponent>(e);
    if (!s || s->controller != static_cast<i32>(from)) return;  // only its controller drives it
    Prediction& pr = m_prediction[uuid];
    for (u16 i = 0; i < count && r.ok; ++i) {
        const u32 seq = r.get<u32>();
        const float dt = r.get<float>();
        const u16 len = r.get<u16>();
        if (!r.ok || r.at + len > data.size()) return;
        const std::string_view text = data.substr(r.at, len);
        r.at += len;
        if (seq <= pr.processed) continue;  // already applied (redundant copy)
        const json input = json::parse(text, nullptr, false);
        if (input.is_discarded()) continue;
        if (m_input) m_input(scene, e, input, std::clamp(dt, 0.0f, 0.25f));  // a client cannot claim more time
        pr.processed = seq;
        ++m_stats.inputsApplied;
    }
}

Vec3 Replicator::positionAt(Scene& scene, ecs::Entity e, f64 time) const {
    if (!scene.valid(e)) return Vec3{0.0f};
    const Vec3 now = scene.registry().get<TransformComponent>(e).position;
    auto it = m_history.find(scene.uuidOf(e).value());
    if (it == m_history.end() || it->second.empty()) return now;
    const auto& h = it->second;
    if (time >= h.back().first) return now;
    if (time <= h.front().first) return h.front().second;
    for (usize i = h.size() - 1; i > 0; --i)
        if (h[i - 1].first <= time) {
            const auto& [t0, p0] = h[i - 1];
            const auto& [t1, p1] = h[i];
            const float k = t1 > t0 ? static_cast<float>((time - t0) / (t1 - t0)) : 1.0f;
            return p0 + (p1 - p0) * k;
        }
    return now;
}

u32 Replicator::ownerOf(Scene& scene, ecs::Entity e) const {
    const auto* s = scene.valid(e) ? scene.registry().tryGet<NetSyncComponent>(e) : nullptr;
    return s && s->owner > 0 ? static_cast<u32>(s->owner) : NetSession::kHostId;
}

bool Replicator::isOwner(Scene& scene, NetSession& session, ecs::Entity e) const {
    const u32 local = session.connected() ? session.localId() : NetSession::kHostId;  // offline: everything is ours
    return ownerOf(scene, e) == local;
}

void Replicator::sendControl(NetSession& session, u32 peer, const json& msg) {
    std::string data(1, NetSession::kSystemTag);
    data += kControl;
    data += msg.dump();
    session.send(peer, data, true);
}

ecs::Entity Replicator::instantiate(Scene& scene, const std::string& prefab, Uuid id, Vec3 position, u32 owner) {
    if (ecs::Entity existing = scene.findByUuid(id); !existing.isNull()) return existing;
    auto e = prefab::instantiate(scene, prefab);
    if (!e) {
        AGR_LOG_ERROR("Net", "spawn '{}': {}", prefab, e.error().message);
        return {};
    }
    scene.setUuid(e.value(), id);
    scene.registry().get<TransformComponent>(e.value()).position = position;
    auto& sync = scene.registry().has<NetSyncComponent>(e.value()) ? scene.registry().get<NetSyncComponent>(e.value())
                                                                    : scene.registry().emplace<NetSyncComponent>(e.value());
    sync.owner = static_cast<i32>(owner);
    m_spawned[id.value()] = {prefab, position};
    return e.value();
}

Result<ecs::Entity> Replicator::spawn(Scene& scene, NetSession& session, const std::string& prefab, Vec3 position, u32 owner) {
    if (session.role() == NetRole::Client) return makeError(Error::Code::InvalidArgument, "only the host spawns network entities");
    const Uuid id = Uuid::generate();
    const ecs::Entity e = instantiate(scene, prefab, id, position, owner);
    if (e.isNull()) return makeError(Error::Code::NotFound, "cannot instantiate '" + prefab + "'");
    const auto* sync = scene.registry().tryGet<NetSyncComponent>(e);
    if (session.role() == NetRole::Host && !(sync && sync->spawnWhenVisible))  // else: per player, when visible (update)
        sendControl(session, NetSession::kBroadcast,
                    {{"k", "spawn"}, {"prefab", prefab}, {"id", id.toString()}, {"pos", {position.x, position.y, position.z}}, {"owner", owner}});
    return e;
}

bool Replicator::despawn(Scene& scene, NetSession& session, ecs::Entity e) {
    if (session.role() == NetRole::Client || !scene.valid(e)) return false;
    const Uuid id = scene.uuidOf(e);
    m_spawned.erase(id.value());
    m_spawnedFor.erase(id.value());
    m_remote.erase(id.value());
    scene.destroyEntity(e);
    if (session.role() == NetRole::Host) sendControl(session, NetSession::kBroadcast, {{"k", "despawn"}, {"id", id.toString()}});
    return true;
}

bool Replicator::setOwner(Scene& scene, NetSession& session, ecs::Entity e, u32 owner) {
    if (session.role() == NetRole::Client || !scene.valid(e)) return false;
    auto* s = scene.registry().tryGet<NetSyncComponent>(e);
    if (!s) s = &scene.registry().emplace<NetSyncComponent>(e);
    s->owner = static_cast<i32>(std::max<u32>(owner, 1));
    m_remote.erase(scene.uuidOf(e).value());
    if (session.role() == NetRole::Host) sendControl(session, NetSession::kBroadcast, {{"k", "owner"}, {"id", scene.uuidOf(e).toString()}, {"owner", s->owner}});
    return true;
}

bool Replicator::setController(Scene& scene, NetSession& session, ecs::Entity e, u32 peer) {
    if (session.role() == NetRole::Client || !scene.valid(e)) return false;
    auto* s = scene.registry().tryGet<NetSyncComponent>(e);
    if (!s) s = &scene.registry().emplace<NetSyncComponent>(e);
    s->controller = static_cast<i32>(peer);
    s->owner = static_cast<i32>(NetSession::kHostId);  // the host simulates it from the inputs
    m_prediction.erase(scene.uuidOf(e).value());
    if (session.role() == NetRole::Host) sendControl(session, NetSession::kBroadcast, {{"k", "controller"}, {"id", scene.uuidOf(e).toString()}, {"peer", peer}});
    return true;
}

void Replicator::runRpc(Scene& scene, ecs::Entity e, const std::string& fn, const json& args, u32 sender) {
    if (!m_rpc || !scene.valid(e)) return;
    m_sender = sender;
    ++m_stats.rpcsReceived;
    m_rpc(scene, e, fn, args, sender);
    m_sender = 0;
}

bool Replicator::rpc(Scene& scene, NetSession& session, ecs::Entity e, const std::string& function, const json& args, u32 target) {
    if (!session.connected() || !scene.valid(e)) return false;
    const json msg = {{"k", "rpc"}, {"id", scene.uuidOf(e).toString()}, {"fn", function}, {"args", args}, {"to", target}, {"from", session.localId()}};
    ++m_stats.rpcsSent;
    if (session.role() == NetRole::Host) {
        if (target == NetSession::kHostId) {
            runRpc(scene, e, function, args, NetSession::kHostId);
            return true;
        }
        sendControl(session, target == 0 ? NetSession::kBroadcast : target, msg);
        return true;
    }
    if (target == session.localId()) {  // to ourselves
        runRpc(scene, e, function, args, session.localId());
        return true;
    }
    sendControl(session, NetSession::kHostId, msg);
    return true;
}

void Replicator::handleControl(Scene& scene, NetSession& session, u32 from, const json& msg) {
    const std::string kind = msg.value("k", "");
    const Uuid id = Uuid::fromString(msg.value("id", ""));
    const bool fromHost = from == NetSession::kHostId;
    if (kind == "spawn" && fromHost && session.role() == NetRole::Client) {
        Vec3 pos{0.0f};
        if (const auto p = msg.find("pos"); p != msg.end() && p->is_array() && p->size() == 3) pos = {(*p)[0].get<float>(), (*p)[1].get<float>(), (*p)[2].get<float>()};
        instantiate(scene, msg.value("prefab", ""), id, pos, msg.value("owner", 1u));
    } else if (kind == "despawn" && fromHost && session.role() == NetRole::Client) {
        if (ecs::Entity e = scene.findByUuid(id); !e.isNull()) scene.destroyEntity(e);
        m_spawned.erase(id.value());
        m_remote.erase(id.value());
    } else if (kind == "owner" && fromHost && session.role() == NetRole::Client) {
        if (ecs::Entity e = scene.findByUuid(id); !e.isNull()) {
            auto* s = scene.registry().tryGet<NetSyncComponent>(e);
            if (!s) s = &scene.registry().emplace<NetSyncComponent>(e);
            s->owner = msg.value("owner", 1);
            m_remote.erase(id.value());
        }
    } else if (kind == "controller" && fromHost && session.role() == NetRole::Client) {
        if (ecs::Entity e = scene.findByUuid(id); !e.isNull()) {
            auto* s = scene.registry().tryGet<NetSyncComponent>(e);
            if (!s) s = &scene.registry().emplace<NetSyncComponent>(e);
            s->controller = msg.value("peer", 0);
            s->owner = static_cast<i32>(NetSession::kHostId);
            m_prediction.erase(id.value());
            m_remote.erase(id.value());
        }
    } else if (kind == "rpc") {
        const ecs::Entity e = scene.findByUuid(id);
        const std::string fn = msg.value("fn", "");
        const json args = msg.value("args", json::array());
        const u32 to = msg.value("to", 0u);
        if (session.role() == NetRole::Host) {
            // From a client (its id is `from`, whatever it claims): run here and / or relay.
            json relayed = msg;
            relayed["from"] = from;
            if (to == 0) {
                runRpc(scene, e, fn, args, from);
                for (u32 peer : session.peers())
                    if (peer != from) sendControl(session, peer, relayed);
            } else if (to == NetSession::kHostId) {
                runRpc(scene, e, fn, args, from);
            } else {
                sendControl(session, to, relayed);
            }
        } else if (fromHost) {
            runRpc(scene, e, fn, args, msg.value("from", NetSession::kHostId));
        }
    }
}

void Replicator::handleState(Scene& scene, u32 from, std::string_view data, f64 now) {
    Reader r{data, 0};
    const u16 count = r.get<u16>();
    for (u16 i = 0; i < count && r.ok; ++i) {
        const u64 uuid = r.get<u64>();
        const u8 flags = r.get<u8>();
        Vec3 p, s;
        Quat q;
        if (flags & 1) {
            p = {r.get<float>(), r.get<float>(), r.get<float>()};
            q = {r.get<float>(), r.get<float>(), r.get<float>(), r.get<float>()};
            s = {r.get<float>(), r.get<float>(), r.get<float>()};
        }
        u32 ack = 0;
        if (flags & 4) ack = r.get<u32>();
        std::string fieldsJson;
        if (flags & 2) {
            const u16 n = r.get<u16>();
            if (r.at + n > data.size()) return;
            fieldsJson.assign(data.substr(r.at, n));
            r.at += n;
        }
        if (!r.ok) return;
        const ecs::Entity e = scene.findByUuid(Uuid(uuid));
        if (e.isNull()) continue;
        const auto* sync = scene.registry().tryGet<NetSyncComponent>(e);
        // Only the owner's state counts (the host relays the others' with its own).
        if (!sync || (from != static_cast<u32>(sync->owner) && from != NetSession::kHostId)) continue;
        ++m_stats.statesReceived;
        if ((flags & 4) && (flags & 1) && m_localId != 0 && sync->controller == static_cast<i32>(m_localId)) {
            // Reconciliation: the host's state, then the inputs it has not applied yet, again.
            auto& t = scene.registry().get<TransformComponent>(e);
            const Vec3 predicted = t.position;
            t.position = p, t.rotation = q, t.scale = s;
            Prediction& pr = m_prediction[uuid];
            while (!pr.pending.empty() && pr.pending.front().seq <= ack) pr.pending.pop_front();
            for (const PendingInput& in : pr.pending) {
                const json input = json::parse(in.input, nullptr, false);
                if (m_input && !input.is_discarded()) m_input(scene, e, input, in.dt);
                ++m_stats.replayed;
            }
            m_stats.lastCorrection = math::length(t.position - predicted);
            continue;  // not interpolated: this machine shows its own prediction
        }
        if (flags & 1) {
            Remote& rm = m_remote[uuid];
            if (rm.received < 0.0) {
                rm.p0 = p, rm.r0 = q, rm.s0 = s;
            } else {  // from where it is shown now to the new state
                const auto& t = scene.registry().get<TransformComponent>(e);
                rm.p0 = t.position, rm.r0 = t.rotation, rm.s0 = t.scale;
                // States only when it moves (onlyChanges): a gap is a rest, not a slow rate.
                rm.interval = std::clamp(std::min(now - rm.received, 1.5 / std::clamp(static_cast<f64>(sync->rate), 1.0, 120.0)), 0.01, 0.5);
                m_viewDelay = m_viewDelay * 0.9 + rm.interval * 0.1;
            }
            rm.p1 = p, rm.r1 = q, rm.s1 = s;
            rm.received = now;
        }
        if (!fieldsJson.empty() && sync) {
            const json values = json::parse(fieldsJson, nullptr, false);
            const auto refs = parseFields(sync->fields);
            if (values.is_array())
                for (usize k = 0; k < refs.size() && k < values.size(); ++k)
                    if (void* c = componentOf(scene, e, *refs[k].component)) writeField(c, *refs[k].field, values[k]);
        }
    }
}

void Replicator::update(Scene& scene, NetSession& session, f64 now) {
    m_stats.synced = m_stats.statesSent = m_stats.statesReceived = m_stats.statesSkipped = m_stats.statesHidden = 0;
    if (&scene != m_scene || scene.generation() != m_sceneGeneration) {
        reset();
        m_scene = &scene;
        m_sceneGeneration = scene.generation();
    }
    auto& reg = scene.registry();
    // 1. What arrived.
    for (NetEvent& ev : session.takeSystemEvents()) {
        if (ev.type == NetEvent::Type::Connected && session.role() == NetRole::Host) {
            // A late joiner gets the spawned entities and the owners.
            for (const auto& [uuid, sp] : m_spawned) {
                const ecs::Entity e = scene.findByUuid(Uuid(uuid));
                if (const auto* s = e.isNull() ? nullptr : reg.tryGet<NetSyncComponent>(e); s && s->spawnWhenVisible) continue;  // when visible
                const Vec3 pos = e.isNull() ? sp.position : reg.get<TransformComponent>(e).position;
                sendControl(session, ev.peer,
                            {{"k", "spawn"}, {"prefab", sp.prefab}, {"id", Uuid(uuid).toString()}, {"pos", {pos.x, pos.y, pos.z}}, {"owner", ownerOf(scene, e)}});
            }
            reg.view<NetSyncComponent>().each([&](ecs::Entity e, NetSyncComponent& s) {
                if (s.owner != static_cast<i32>(NetSession::kHostId)) sendControl(session, ev.peer, {{"k", "owner"}, {"id", scene.uuidOf(e).toString()}, {"owner", s.owner}});
                if (s.controller > 1) sendControl(session, ev.peer, {{"k", "controller"}, {"id", scene.uuidOf(e).toString()}, {"peer", s.controller}});
            });
        } else if (ev.type == NetEvent::Type::Disconnected && session.role() == NetRole::Host) {
            // What the player owned comes back to the host.
            std::vector<ecs::Entity> orphans;
            reg.view<NetSyncComponent>().each([&](ecs::Entity e, NetSyncComponent& s) {
                if (s.owner == static_cast<i32>(ev.peer)) orphans.push_back(e);
            });
            for (ecs::Entity e : orphans) setOwner(scene, session, e, NetSession::kHostId);
        } else if (ev.type == NetEvent::Type::Message && ev.data.size() >= 2) {
            if (ev.data[1] == kInput && session.role() == NetRole::Host) {
                handleInput(scene, ev.peer, std::string_view(ev.data).substr(2));
            } else if (ev.data[1] == kState) {
                handleState(scene, ev.peer, std::string_view(ev.data).substr(2), now);
            } else if (ev.data[1] == kControl) {
                const json msg = json::parse(std::string_view(ev.data).substr(2), nullptr, false);
                if (msg.is_object()) handleControl(scene, session, ev.peer, msg);
            }
        }
    }
    if (!session.connected()) return;
    const u32 local = session.localId();
    m_localId = local;
    // Host: recent positions for lag compensation (positionAt).
    if (session.role() == NetRole::Host)
        reg.view<NetSyncComponent, TransformComponent>().each([&](ecs::Entity e, NetSyncComponent&, TransformComponent& t) {
            auto& h = m_history[scene.uuidOf(e).value()];
            h.emplace_back(session.networkTime(), t.position);
            while (!h.empty() && h.front().first < session.networkTime() - 1.0) h.pop_front();
        });
    // Spawns that follow visibility: created / removed on each player's machine as it sees them.
    m_stats.spawnsHidden = 0;
    if (session.role() == NetRole::Host)
        for (const auto& [uuid, sp] : m_spawned) {
            const ecs::Entity e = scene.findByUuid(Uuid(uuid));
            const auto* s = e.isNull() ? nullptr : reg.tryGet<NetSyncComponent>(e);
            if (!s || !s->spawnWhenVisible) continue;
            for (u32 peer : session.peers()) {
                const bool vis = visibleTo(scene, e, peer);
                bool& there = m_spawnedFor[uuid][peer];
                if (vis && !there) {
                    const Vec3 pos = reg.get<TransformComponent>(e).position;
                    sendControl(session, peer,
                                {{"k", "spawn"}, {"prefab", sp.prefab}, {"id", Uuid(uuid).toString()}, {"pos", {pos.x, pos.y, pos.z}}, {"owner", ownerOf(scene, e)}});
                    m_wasVisible[uuid][peer] = false;  // its state follows at once
                } else if (!vis && there) {
                    sendControl(session, peer, {{"k", "despawn"}, {"id", Uuid(uuid).toString()}});
                }
                there = vis;
                if (!vis) ++m_stats.spawnsHidden;
            }
        }
    // 2. States of what this machine is responsible for. Host: everything, to each client but the
    // owner; client: what it owns, to the host.
    struct Item {
        u64 uuid;
        u32 owner;
        std::string bytes;
        ecs::Entity entity;
    };
    std::vector<Item> items;
    reg.view<NetSyncComponent, TransformComponent>().each([&](ecs::Entity e, NetSyncComponent& s, TransformComponent& t) {
        ++m_stats.synced;
        const u32 owner = s.owner > 0 ? static_cast<u32>(s.owner) : NetSession::kHostId;
        if (session.role() == NetRole::Client && owner != local) return;
        const u64 uuid = scene.uuidOf(e).value();
        f64& last = m_lastSent[uuid];
        if (now - last < 1.0 / std::clamp(static_cast<f64>(s.rate), 1.0, 120.0)) return;
        last = now;
        std::string b;
        putU64(b, uuid);
        const auto refs = s.fields.empty() ? std::vector<FieldRef>{} : parseFields(s.fields);
        const bool predicted = s.controller > 1 && session.role() == NetRole::Host;
        b.push_back(static_cast<char>((s.syncTransform ? 1 : 0) | (refs.empty() ? 0 : 2) | (predicted ? 4 : 0)));
        if (s.syncTransform) {
            for (float v : {t.position.x, t.position.y, t.position.z, t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w, t.scale.x, t.scale.y, t.scale.z})
                putF(b, v);
        }
        if (predicted) {  // the last input of the controller applied in this state
            const u32 ack = m_prediction[uuid].processed;
            b.append(reinterpret_cast<const char*>(&ack), 4);
        }
        if (!refs.empty()) {
            json values = json::array();
            for (const FieldRef& f : refs) {
                void* c = componentOf(scene, e, *f.component);
                values.push_back(c ? readField(c, *f.field) : json(nullptr));
            }
            const std::string text = values.dump();
            if (text.size() > 900) return;  // too big for a datagram: use RPCs for such data
            putU16(b, static_cast<u16>(text.size()));
            b += text;
        }
        // A player that starts seeing the entity needs its state even if it did not change.
        bool newlyVisible = false;
        if (session.role() == NetRole::Host)
            for (u32 peer : session.peers()) {
                const bool vis = visibleTo(scene, e, peer);
                bool& was = m_wasVisible[uuid][peer];
                newlyVisible |= vis && !was;
                was = vis;
            }
        if (s.onlyChanges) {  // unchanged since the last state: skipped (one per second all the same)
            std::string& lastBytes = m_lastBytes[uuid];
            f64& full = m_lastFull[uuid];
            if (lastBytes == b && now - full < 1.0 && !newlyVisible) {
                ++m_stats.statesSkipped;
                return;
            }
            lastBytes = b;
            full = now;
        }
        items.push_back({uuid, owner, std::move(b), e});
    });
    auto flush = [&](u32 peer, const std::vector<const Item*>& list) {
        usize i = 0;
        while (i < list.size()) {
            std::string packet(1, NetSession::kSystemTag);
            packet += kState;
            putU16(packet, 0);
            u16 n = 0;
            while (i < list.size() && packet.size() + list[i]->bytes.size() <= kPacketBudget) {
                packet += list[i]->bytes;
                ++n, ++i;
            }
            if (n == 0) {  // one entity alone too large: skipped
                ++i;
                continue;
            }
            std::memcpy(packet.data() + 2, &n, 2);
            session.send(peer, packet, false);
            m_stats.statesSent += n;
        }
    };
    if (!items.empty()) {
        if (session.role() == NetRole::Host) {
            for (u32 peer : session.peers()) {
                std::vector<const Item*> list;
                for (const Item& it : items) {
                    if (it.owner == peer) continue;
                    if (!visibleTo(scene, it.entity, peer)) {
                        ++m_stats.statesHidden;
                        continue;
                    }
                    list.push_back(&it);
                }
                flush(peer, list);
            }
        } else {
            std::vector<const Item*> list;
            for (const Item& it : items) list.push_back(&it);
            flush(NetSession::kHostId, list);
        }
    }
    // 3. Show the others' entities between their last two states.
    reg.view<NetSyncComponent, TransformComponent>().each([&](ecs::Entity e, NetSyncComponent& s, TransformComponent& t) {
        if ((s.owner > 0 ? static_cast<u32>(s.owner) : NetSession::kHostId) == local || !s.syncTransform) return;
        if (s.controller == static_cast<i32>(local)) return;  // predicted here
        auto it = m_remote.find(scene.uuidOf(e).value());
        if (it == m_remote.end() || it->second.received < 0.0) return;
        const Remote& rm = it->second;
        const float k = s.interpolate ? static_cast<float>(std::clamp((now - rm.received) / rm.interval, 0.0, 1.0)) : 1.0f;
        t.position = rm.p0 + (rm.p1 - rm.p0) * k;
        t.scale = rm.s0 + (rm.s1 - rm.s0) * k;
        t.rotation = math::slerp(rm.r0, rm.r1, k);
    });
    m_stats.spawned = static_cast<u32>(m_spawned.size());
}

} // namespace agr::net
