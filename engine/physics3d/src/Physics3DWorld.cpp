#include "agr/physics3d/Physics3DWorld.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <unordered_map>

// Jolt must be included first.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"

namespace agr {

using math::Mat4;
using math::Quat;
using math::Vec3;

namespace {

// ------------------------------------------------------------ Jolt process setup
std::mutex g_joltMutex;
int g_joltUsers = 0;

void joltTrace(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    AGR_LOG_INFO("Physics3D", "{}", buf);
}

void acquireJolt() {
    std::lock_guard lock(g_joltMutex);
    if (g_joltUsers++ == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = joltTrace;
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
}

void releaseJolt() {
    std::lock_guard lock(g_joltMutex);
    if (--g_joltUsers == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

// ------------------------------------------------------------ layers
namespace Layers {
constexpr JPH::ObjectLayer kStatic = 0;
constexpr JPH::ObjectLayer kMoving = 1;
constexpr JPH::uint kCount = 2;
} // namespace Layers

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override { return JPH::BroadPhaseLayer(static_cast<JPH::uint8>(layer)); }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return static_cast<JPH::BroadPhaseLayer::Type>(layer) == 0 ? "static" : "moving";
    }
#endif
};

class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        return layer == Layers::kMoving || static_cast<JPH::BroadPhaseLayer::Type>(bp) == Layers::kMoving;
    }
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override { return a == Layers::kMoving || b == Layers::kMoving; }
};

JPH::Vec3 toJ(Vec3 v) { return {v.x, v.y, v.z}; }
Vec3 fromJ(JPH::Vec3 v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
JPH::Quat toJ(Quat q) { return {q.x, q.y, q.z, q.w}; }
Quat fromJ(JPH::Quat q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }

u64 mix(u64 h, u64 v) { return h ^ (v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2)); }
u64 mixf(u64 h, float f) { return mix(h, std::bit_cast<u32>(f)); }

struct Pose {
    Vec3 position;
    Quat rotation;
    Vec3 scale{1.0f};
};

Pose poseOf(const Mat4& m) {
    Pose p;
    math::decompose(m, p.position, p.rotation, p.scale);
    return p;
}

/// Records contacts from Jolt's callbacks (collision steps may run on job threads).
class Listener final : public JPH::ContactListener {
public:
    struct Pair {
        JPH::BodyID a, b;
        bool begin;
        bool sensor;
    };
    void OnContactAdded(const JPH::Body& b1, const JPH::Body& b2, const JPH::ContactManifold&, JPH::ContactSettings&) override {
        std::lock_guard lock(mutex);
        pairs.push_back({b1.GetID(), b2.GetID(), true, b1.IsSensor() || b2.IsSensor()});
    }
    void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
        std::lock_guard lock(mutex);
        pairs.push_back({pair.GetBody1ID(), pair.GetBody2ID(), false, false});
    }
    std::mutex mutex;
    std::vector<Pair> pairs;
};

} // namespace

struct Physics3DWorld::Impl {
    struct Body {
        JPH::BodyID id;
        ecs::Entity entity;
        u64 hash = 0;
        BodyType3D type = BodyType3D::Static;
        bool sensor = false;
        Vec3 lastPosition;
        Quat lastRotation;
    };

    BroadPhaseLayers bpLayers;
    ObjectVsBroadPhase objectVsBp;
    ObjectPairs objectPairs;
    std::unique_ptr<JPH::TempAllocatorImpl> temp;
    std::unique_ptr<JPH::JobSystemSingleThreaded> jobs;
    std::unique_ptr<JPH::PhysicsSystem> system;
    Listener listener;
    u64 sceneGeneration = 0;
    std::unordered_map<u64, Body> bodies;           // entity -> body
    std::unordered_map<u32, ecs::Entity> byBodyId;  // Jolt body index -> entity
    std::unordered_map<u64, u32> activePairs;       // body pair -> sub-shape contact count
    std::unordered_map<u64, bool> pairIsSensor;
    std::vector<Collision3DEvent> events;
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    Physics3DStats stats;

    Impl() {
        acquireJolt();
        temp = std::make_unique<JPH::TempAllocatorImpl>(8 * 1024 * 1024);
        jobs = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
        createSystem();
    }
    ~Impl() {
        destroyAll();
        system.reset();
        jobs.reset();
        temp.reset();
        releaseJolt();
    }

    void createSystem() {
        system = std::make_unique<JPH::PhysicsSystem>();
        system->Init(65536, 0, 65536, 16384, bpLayers, objectVsBp, objectPairs);
        system->SetGravity(toJ(gravity));
        system->SetContactListener(&listener);
    }

    void destroyAll() {
        if (!system) return;
        JPH::BodyInterface& bi = system->GetBodyInterface();
        for (auto& [key, body] : bodies) {
            bi.RemoveBody(body.id);
            bi.DestroyBody(body.id);
        }
        bodies.clear();
        byBodyId.clear();
        activePairs.clear();
        pairIsSensor.clear();
        listener.pairs.clear();
    }

    void reset() {
        destroyAll();
        events.clear();
        sceneGeneration = 0;
    }

    ecs::Entity owner(Scene& s, ecs::Entity e) const {
        for (ecs::Entity cur = e; !cur.isNull(); cur = s.parent(cur))
            if (s.registry().has<RigidBodyComponent>(cur)) return cur;
        return {};
    }

    const Body* find(ecs::Entity e) const {
        auto it = bodies.find(e.packed());
        return it == bodies.end() ? nullptr : &it->second;
    }

    void destroyBody(Body& b) {
        JPH::BodyInterface& bi = system->GetBodyInterface();
        byBodyId.erase(b.id.GetIndex());
        bi.RemoveBody(b.id);
        bi.DestroyBody(b.id);
    }

    void sync(Scene& s) {
        if (s.generation() != sceneGeneration) {
            if (sceneGeneration) reset();
            sceneGeneration = s.generation();
        }
        auto& reg = s.registry();
        JPH::BodyInterface& bi = system->GetBodyInterface();
        stats.rebuilt = 0;
        std::unordered_map<u64, ecs::Entity> owners;
        std::unordered_map<u64, std::vector<ecs::Entity>> owned;
        reg.view<ColliderComponent>().each([&](ecs::Entity e, ColliderComponent&) {
            if (!s.isActiveInHierarchy(e)) return;
            ecs::Entity o = owner(s, e);
            if (o.isNull()) o = e;
            owners[o.packed()] = o;
            owned[o.packed()].push_back(e);
        });
        reg.view<RigidBodyComponent>().each([&](ecs::Entity e, RigidBodyComponent&) {
            if (s.isActiveInHierarchy(e)) owners[e.packed()] = e;
        });
        for (auto it = bodies.begin(); it != bodies.end();) {
            if (!owners.contains(it->first) || !s.valid(it->second.entity)) {
                destroyBody(it->second);
                it = bodies.erase(it);
            } else {
                ++it;
            }
        }
        for (auto& [key, o] : owners) {
            std::vector<ecs::Entity>& cols = owned[key];
            std::sort(cols.begin(), cols.end(), [](ecs::Entity a, ecs::Entity b) { return a.index < b.index; });
            const Pose op = poseOf(s.worldMatrix(o));
            const RigidBodyComponent* rb = reg.tryGet<RigidBodyComponent>(o);
            u64 h = rb ? mix(static_cast<u64>(rb->type) + 1, rb->freezeRotation | (rb->bullet << 1)) : 0;
            if (rb) for (float f : {rb->mass, rb->linearDamping, rb->angularDamping, rb->gravityScale}) h = mixf(h, f);
            const Mat4 frameInv = math::inverse(Mat4::trs(op.position, op.rotation, Vec3{1.0f}));
            struct Placed {
                const ColliderComponent* col;
                Pose rel;
            };
            std::vector<Placed> placed;
            for (ecs::Entity c : cols) {
                const Pose rel = poseOf(frameInv * s.worldMatrix(c));
                const ColliderComponent& col = reg.get<ColliderComponent>(c);
                placed.push_back({&col, rel});
                h = mix(h, c.packed());
                for (float f : {rel.position.x, rel.position.y, rel.position.z, rel.rotation.x, rel.rotation.y, rel.rotation.z, rel.rotation.w,
                                rel.scale.x, rel.scale.y, rel.scale.z, col.size.x, col.size.y, col.size.z, col.radius, col.height, col.offset.x,
                                col.offset.y, col.offset.z, col.density, col.friction, col.restitution})
                    h = mixf(h, f);
                h = mix(h, static_cast<u64>(col.shape) | (static_cast<u64>(col.isTrigger) << 8));
            }
            auto it = bodies.find(key);
            if (it != bodies.end() && it->second.hash == h) continue;
            Vec3 keepVel{}, keepAng{};
            bool keep = false;
            if (it != bodies.end()) {
                keepVel = fromJ(bi.GetLinearVelocity(it->second.id));
                keepAng = fromJ(bi.GetAngularVelocity(it->second.id));
                keep = true;
                destroyBody(it->second);
                bodies.erase(it);
            }
            // Shape: single collider at the origin, or a compound of the placed colliders.
            JPH::Ref<JPH::Shape> shape;
            JPH::StaticCompoundShapeSettings compound;
            bool sensor = false;
            for (const Placed& p : placed) {
                const ColliderComponent& c = *p.col;
                const Vec3 sc{std::fabs(p.rel.scale.x), std::fabs(p.rel.scale.y), std::fabs(p.rel.scale.z)};
                JPH::Ref<JPH::ShapeSettings> settings;
                switch (c.shape) {
                    case ColliderShape3D::Box: {
                        const JPH::Vec3 half(std::max(c.size.x * sc.x * 0.5f, 0.01f), std::max(c.size.y * sc.y * 0.5f, 0.01f),
                                             std::max(c.size.z * sc.z * 0.5f, 0.01f));
                        auto* box = new JPH::BoxShapeSettings(half, std::min(0.05f, half.ReduceMin() * 0.5f));
                        box->SetDensity(std::max(c.density, 0.001f));
                        settings = box;
                        break;
                    }
                    case ColliderShape3D::Sphere: {
                        auto* sphere = new JPH::SphereShapeSettings(std::max(c.radius * std::max({sc.x, sc.y, sc.z}), 0.005f));
                        sphere->SetDensity(std::max(c.density, 0.001f));
                        settings = sphere;
                        break;
                    }
                    case ColliderShape3D::Capsule: {
                        const float r = std::max(c.radius * std::max(sc.x, sc.z), 0.005f);
                        const float halfCyl = std::max(c.height * sc.y * 0.5f - r, 0.001f);
                        auto* capsule = new JPH::CapsuleShapeSettings(halfCyl, r);
                        capsule->SetDensity(std::max(c.density, 0.001f));
                        settings = capsule;
                        break;
                    }
                }
                sensor |= c.isTrigger;
                const Vec3 center = p.rel.position + (p.rel.rotation * Vec3{c.offset.x * sc.x, c.offset.y * sc.y, c.offset.z * sc.z});
                compound.AddShape(toJ(center), toJ(p.rel.rotation), settings);
            }
            if (placed.empty()) {  // body without collider: a tiny sphere so it can still move
                shape = new JPH::SphereShape(0.05f);
            } else {
                JPH::ShapeSettings::ShapeResult r = compound.Create();
                if (r.HasError()) {
                    AGR_LOG_ERROR("Physics3D", "'{}': invalid collision shape: {}", s.nameOf(o), r.GetError().c_str());
                    continue;
                }
                shape = r.Get();
            }
            Body body;
            body.entity = o;
            body.hash = h;
            body.type = rb ? rb->type : BodyType3D::Static;
            body.sensor = sensor;
            const JPH::EMotionType motion = body.type == BodyType3D::Dynamic   ? JPH::EMotionType::Dynamic
                                            : body.type == BodyType3D::Kinematic ? JPH::EMotionType::Kinematic
                                                                                 : JPH::EMotionType::Static;
            JPH::BodyCreationSettings bs(shape, JPH::RVec3(op.position.x, op.position.y, op.position.z), toJ(math::normalize(op.rotation)), motion,
                                         motion == JPH::EMotionType::Static ? Layers::kStatic : Layers::kMoving);
            bs.mUserData = o.packed();
            bs.mIsSensor = sensor;
            if (!placed.empty()) {
                bs.mFriction = placed.front().col->friction;
                bs.mRestitution = placed.front().col->restitution;
            }
            if (rb) {
                bs.mLinearDamping = rb->linearDamping;
                bs.mAngularDamping = rb->angularDamping;
                bs.mGravityFactor = rb->gravityScale;
                bs.mMotionQuality = rb->bullet ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
                if (rb->freezeRotation) bs.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
                if (rb->mass > 0.0f) {
                    bs.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                    bs.mMassPropertiesOverride.mMass = rb->mass;
                }
                bs.mLinearVelocity = toJ(keep ? keepVel : rb->initialVelocity);
                bs.mAngularVelocity = toJ(keepAng);
            }
            body.id = bi.CreateAndAddBody(bs, JPH::EActivation::Activate);
            if (body.id.IsInvalid()) {
                AGR_LOG_ERROR("Physics3D", "too many bodies: '{}' has no physics", s.nameOf(o));
                continue;
            }
            body.lastPosition = op.position;
            body.lastRotation = op.rotation;
            byBodyId[body.id.GetIndex()] = o;
            bodies.emplace(key, body);
            ++stats.rebuilt;
        }
    }

    void step(Scene& s, float dt) {
        Stopwatch sw;
        sync(s);
        events.clear();
        JPH::BodyInterface& bi = system->GetBodyInterface();
        for (auto& [key, body] : bodies) {
            const Pose p = poseOf(s.worldMatrix(body.entity));
            const Vec3 d = p.position - body.lastPosition;
            const bool moved = math::dot(d, d) > 1e-10f || std::fabs(math::dot(p.rotation, body.lastRotation)) < 0.999999f;
            if (!moved) continue;
            if (body.type == BodyType3D::Kinematic && dt > 0.0f) {
                bi.MoveKinematic(body.id, JPH::RVec3(p.position.x, p.position.y, p.position.z), toJ(math::normalize(p.rotation)), dt);
            } else {
                bi.SetPositionAndRotation(body.id, JPH::RVec3(p.position.x, p.position.y, p.position.z), toJ(math::normalize(p.rotation)),
                                          JPH::EActivation::Activate);
            }
        }
        if (dt > 0.0f) {
            const JPH::EPhysicsUpdateError err = system->Update(dt, 1, temp.get(), jobs.get());
            if (err != JPH::EPhysicsUpdateError::None) AGR_LOG_WARN("Physics3D", "simulation limits reached (error {})", static_cast<int>(err));
        }
        u32 active = 0;
        for (auto& [key, body] : bodies) {
            if (body.type == BodyType3D::Static) continue;
            if (bi.IsActive(body.id)) ++active;
            JPH::RVec3 jp;
            JPH::Quat jr;
            bi.GetPositionAndRotation(body.id, jp, jr);
            const Vec3 p{static_cast<float>(jp.GetX()), static_cast<float>(jp.GetY()), static_cast<float>(jp.GetZ())};
            const Quat r = fromJ(jr);
            const Pose current = poseOf(s.worldMatrix(body.entity));
            s.setWorldMatrix(body.entity, Mat4::trs(p, r, current.scale));
            body.lastPosition = p;
            body.lastRotation = r;
        }
        // Contacts: one begin / end per body pair (compound shapes report each sub-shape).
        std::vector<Listener::Pair> pairs;
        {
            std::lock_guard lock(listener.mutex);
            pairs.swap(listener.pairs);
        }
        for (const Listener::Pair& p : pairs) {
            const u32 ia = p.a.GetIndex(), ib = p.b.GetIndex();
            const u64 key = (static_cast<u64>(std::min(ia, ib)) << 32) | std::max(ia, ib);
            auto ea = byBodyId.find(ia), eb = byBodyId.find(ib);
            if (p.begin) {
                if (activePairs[key]++ == 0) {
                    pairIsSensor[key] = p.sensor;
                    if (ea != byBodyId.end() && eb != byBodyId.end()) {
                        Collision3DEvent e{ea->second, eb->second, true, p.sensor};
                        if (p.sensor && !find(e.a)->sensor) std::swap(e.a, e.b);  // a = the sensor
                        events.push_back(e);
                    }
                }
            } else if (auto it = activePairs.find(key); it != activePairs.end() && --it->second == 0) {
                const bool sensor = pairIsSensor[key];
                activePairs.erase(it);
                pairIsSensor.erase(key);
                if (ea != byBodyId.end() && eb != byBodyId.end()) {
                    Collision3DEvent e{ea->second, eb->second, false, sensor};
                    if (sensor && find(e.a) && !find(e.a)->sensor) std::swap(e.a, e.b);
                    events.push_back(e);
                }
            }
        }
        stats.bodies = static_cast<u32>(bodies.size());
        stats.activeBodies = active;
        stats.stepMs = sw.elapsedMilliseconds();
    }
};

Physics3DWorld::Physics3DWorld() : m(std::make_unique<Impl>()) {}
Physics3DWorld::~Physics3DWorld() = default;

void Physics3DWorld::step(Scene& scene, float dt) { m->step(scene, dt); }
void Physics3DWorld::sync(Scene& scene) { m->sync(scene); }
void Physics3DWorld::reset() { m->reset(); }
void Physics3DWorld::setGravity(Vec3 g) {
    m->gravity = g;
    m->system->SetGravity(toJ(g));
}
Vec3 Physics3DWorld::gravity() const { return m->gravity; }
const std::vector<Collision3DEvent>& Physics3DWorld::events() const { return m->events; }
Physics3DStats Physics3DWorld::stats() const { return m->stats; }
bool Physics3DWorld::hasBody(ecs::Entity e) const { return m->find(e) != nullptr; }

std::optional<RaycastHit3D> Physics3DWorld::raycast(Vec3 origin, Vec3 direction, float maxDistance) const {
    const float len = math::length(direction);
    if (len < 1e-8f || maxDistance <= 0.0f) return std::nullopt;
    const Vec3 dir = direction / len * maxDistance;
    JPH::RRayCast ray{JPH::RVec3(origin.x, origin.y, origin.z), toJ(dir)};
    JPH::RayCastResult result;
    if (!m->system->GetNarrowPhaseQuery().CastRay(ray, result)) return std::nullopt;
    RaycastHit3D hit;
    const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
    hit.point = {static_cast<float>(point.GetX()), static_cast<float>(point.GetY()), static_cast<float>(point.GetZ())};
    hit.distance = result.mFraction * maxDistance;
    {
        JPH::BodyLockRead lock(m->system->GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded()) {
            hit.normal = fromJ(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
            const u64 packed = lock.GetBody().GetUserData();
            hit.entity = ecs::Entity{static_cast<u32>(packed), static_cast<u32>(packed >> 32)};
        }
    }
    return hit;
}

std::optional<Vec3> Physics3DWorld::velocity(ecs::Entity e) const {
    const auto* b = m->find(e);
    if (!b) return std::nullopt;
    return fromJ(m->system->GetBodyInterface().GetLinearVelocity(b->id));
}
bool Physics3DWorld::setVelocity(ecs::Entity e, Vec3 v) {
    const auto* b = m->find(e);
    if (!b || b->type == BodyType3D::Static) return false;
    m->system->GetBodyInterface().SetLinearVelocity(b->id, toJ(v));
    m->system->GetBodyInterface().ActivateBody(b->id);
    return true;
}
bool Physics3DWorld::setAngularVelocity(ecs::Entity e, Vec3 w) {
    const auto* b = m->find(e);
    if (!b || b->type == BodyType3D::Static) return false;
    m->system->GetBodyInterface().SetAngularVelocity(b->id, toJ(w));
    m->system->GetBodyInterface().ActivateBody(b->id);
    return true;
}
bool Physics3DWorld::applyForce(ecs::Entity e, Vec3 f) {
    const auto* b = m->find(e);
    if (!b || b->type != BodyType3D::Dynamic) return false;
    m->system->GetBodyInterface().AddForce(b->id, toJ(f));
    return true;
}
bool Physics3DWorld::applyImpulse(ecs::Entity e, Vec3 i) {
    const auto* b = m->find(e);
    if (!b || b->type != BodyType3D::Dynamic) return false;
    m->system->GetBodyInterface().AddImpulse(b->id, toJ(i));
    return true;
}
std::optional<float> Physics3DWorld::mass(ecs::Entity e) const {
    const auto* b = m->find(e);
    if (!b) return std::nullopt;
    JPH::BodyLockRead lock(m->system->GetBodyLockInterface(), b->id);
    if (!lock.Succeeded()) return std::nullopt;
    const JPH::MotionProperties* mp = lock.GetBody().GetMotionPropertiesUnchecked();
    if (!mp || lock.GetBody().IsStatic()) return 0.0f;
    const float inv = mp->GetInverseMassUnchecked();
    return inv > 0.0f ? 1.0f / inv : 0.0f;
}

} // namespace agr
