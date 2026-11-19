#include "agr/physics3d/Physics3DWorld.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

// Jolt must be included first.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "agr/core/JobSystem.hpp"
#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/scene/Terrain.hpp"

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
/// Hash of a derived value (relative pose): rounded so float noise from a moving body does not
/// look like a shape change (which would rebuild the body every frame).
u64 mixq(u64 h, float f) { return mix(h, static_cast<u64>(static_cast<i64>(std::llround(static_cast<double>(f) * 1e4)))); }

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

u64 pairKey(u32 a, u32 b) { return (static_cast<u64>(std::min(a, b)) << 32) | std::max(a, b); }

/// Collision layer and mask of each body (by Jolt body index); bodies not listed: layer 0, all.
struct LayerTable {
    struct Entry {
        u32 bit = 1u;
        u32 mask = 0xFFFFFFFFu;
    };
    std::unordered_map<u32, Entry> byIndex;
    std::unordered_set<u64> ignoredPairs;  // bodies joined without collideConnected
    Entry of(u32 index) const {
        auto it = byIndex.find(index);
        return it == byIndex.end() ? Entry{} : it->second;
    }
    bool collide(u32 a, u32 b) const {
        const Entry ea = of(a), eb = of(b);
        return (ea.mask & eb.bit) && (eb.mask & ea.bit) && !ignoredPairs.contains(pairKey(a, b));
    }
};

/// Body filter of queries and characters: layer mask, optionally one ignored body.
class MaskBodyFilter final : public JPH::BodyFilter {
public:
    MaskBodyFilter(const LayerTable& t, u32 mask, JPH::BodyID ignore = {}) : m_table(t), m_mask(mask), m_ignore(ignore) {}
    bool ShouldCollide(const JPH::BodyID& id) const override { return id != m_ignore && (m_mask & m_table.of(id.GetIndex()).bit) != 0; }
    bool ShouldCollideLocked(const JPH::Body& body) const override { return ShouldCollide(body.GetID()); }

private:
    const LayerTable& m_table;
    u32 m_mask;
    JPH::BodyID m_ignore;
};

/// Jolt job system running on AGR's JobSystem: physics jobs share the engine's
/// worker threads instead of spawning a second pool (no oversubscription, and
/// the calling thread helps through Jolt's barrier while it waits).
class AgrJoltJobSystem final : public JPH::JobSystemWithBarrier {
public:
    explicit AgrJoltJobSystem(agr::JobSystem& jobs) : JobSystemWithBarrier(JPH::cMaxPhysicsBarriers), m_jobs(jobs) {
        m_pool.Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsJobs);
    }
    int GetMaxConcurrency() const override { return static_cast<int>(m_jobs.workerCount()) + 1; }
    JobHandle CreateJob(const char* name, JPH::ColorArg color, const JobFunction& fn, JPH::uint32 numDependencies) override {
        JPH::uint32 index;
        for (;;) {
            index = m_pool.ConstructObject(name, color, this, fn, numDependencies);
            if (index != Pool::cInvalidObjectIndex) break;
            std::this_thread::yield();  // every job slot in flight: let them finish
        }
        Job* job = &m_pool.Get(index);
        JobHandle handle(job);  // keeps the job alive: it may complete right after QueueJob
        if (numDependencies == 0) QueueJob(job);
        return handle;
    }

protected:
    void QueueJob(Job* job) override {
        job->AddRef();
        m_jobs.submit([job] {
            job->Execute();  // no-op when a barrier wait already ran it
            job->Release();
        });
    }
    void QueueJobs(Job** jobs, JPH::uint count) override {
        for (JPH::uint i = 0; i < count; ++i) QueueJob(jobs[i]);
    }
    void FreeJob(Job* job) override { m_pool.DestructObject(job); }

private:
    using Pool = JPH::FixedSizeFreeList<Job>;
    agr::JobSystem& m_jobs;
    Pool m_pool;
};

/// Records contacts from Jolt's callbacks (collision steps may run on job threads).
class Listener final : public JPH::ContactListener {
public:
    explicit Listener(const LayerTable& t) : table(t) {}
    JPH::ValidateResult OnContactValidate(const JPH::Body& b1, const JPH::Body& b2, JPH::RVec3Arg, const JPH::CollideShapeResult&) override {
        return table.collide(b1.GetID().GetIndex(), b2.GetID().GetIndex()) ? JPH::ValidateResult::AcceptAllContactsForThisBodyPair
                                                                         : JPH::ValidateResult::RejectAllContactsForThisBodyPair;
    }
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
    const LayerTable& table;
};

} // namespace

struct Physics3DWorld::Impl {
    struct TerrainBody {
        JPH::BodyID id;
        u64 hash = 0;
    };
    std::unordered_map<u64, TerrainBody> terrainBodies;
    struct Body {
        JPH::BodyID id;
        ecs::Entity entity;
        u64 hash = 0;
        BodyType3D type = BodyType3D::Static;
        bool sensor = false;
        Vec3 lastPosition;
        Quat lastRotation;
    };

    struct Joint {
        JPH::Ref<JPH::TwoBodyConstraint> c;
        ecs::Entity entity;
        JointType3D type = JointType3D::Hinge;
        u64 hash = 0, paramHash = 0;
        bool resolved = false, warned = false;
        Vec3 worldB;
        float length = 0.0f;
        JPH::BodyID a, b;  // this body, connected body (invalid = world)
        Vec3 localA, localB;
    };
    struct Character {
        JPH::Ref<JPH::CharacterVirtual> ch;
        ecs::Entity entity;
        u64 hash = 0;
        Vec3 velocity{0.0f};
        Vec3 lastPosition;
        float gravityScale = 1.0f, stepHeight = 0.3f;
        u32 mask = 0xFFFFFFFFu;
        std::unordered_set<u32> contacts;
    };

    LayerTable layers;
    BroadPhaseLayers bpLayers;
    ObjectVsBroadPhase objectVsBp;
    ObjectPairs objectPairs;
    std::unordered_map<u64, Joint> joints;
    std::unordered_map<u64, Character> characters;
    std::vector<JointBreak3DEvent> breaks;
    std::unique_ptr<JPH::TempAllocatorImpl> temp;
    std::unique_ptr<JPH::JobSystem> jobs;
    std::unique_ptr<JPH::PhysicsSystem> system;
    Listener listener{layers};
    u64 sceneGeneration = 0;
    std::unordered_map<u64, Body> bodies;           // entity -> body
    std::unordered_map<u32, ecs::Entity> byBodyId;  // Jolt body index -> entity
    std::unordered_map<u64, u32> activePairs;       // body pair -> sub-shape contact count
    std::unordered_map<u64, bool> pairIsSensor;
    std::vector<Collision3DEvent> events;
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    Physics3DStats stats;

    void setJobSystem(JobSystem* js) {
        if (js && js->workerCount() > 0) jobs = std::make_unique<AgrJoltJobSystem>(*js);
        else jobs = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
    }

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

    void removeJoint(Joint& j) {
        if (j.c) system->RemoveConstraint(j.c);
        if (j.c) layers.ignoredPairs.erase(pairKey(j.a.GetIndex(), j.b.IsInvalid() ? 0xFFFFFFFFu : j.b.GetIndex()));
        j.c = nullptr;
        j.hash = 0;
    }

    void destroyCharacter(Character& c) {
        if (!c.ch) return;
        const JPH::BodyID inner = c.ch->GetInnerBodyID();
        if (!inner.IsInvalid()) {
            byBodyId.erase(inner.GetIndex());
            layers.byIndex.erase(inner.GetIndex());
        }
        c.ch = nullptr;  // removes the inner body
    }

    void destroyAll() {
        if (!system) return;
        for (auto& [key, j] : joints) removeJoint(j);
        joints.clear();
        for (auto& [key, c] : characters) destroyCharacter(c);
        characters.clear();
        layers.byIndex.clear();
        layers.ignoredPairs.clear();
        JPH::BodyInterface& bi = system->GetBodyInterface();
        for (auto& [key, body] : bodies) {
            bi.RemoveBody(body.id);
            bi.DestroyBody(body.id);
        }
        bodies.clear();
        for (auto& [key, t] : terrainBodies) {
            bi.RemoveBody(t.id);
            bi.DestroyBody(t.id);
        }
        terrainBodies.clear();
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
        // Constraints must go before their bodies.
        for (auto& [key, j] : joints)
            if (j.c && (j.a == b.id || j.b == b.id)) removeJoint(j);
        layers.byIndex.erase(b.id.GetIndex());
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
                // The body's own collider sits exactly at its frame (only the world scale applies).
                const Pose rel = c == o ? Pose{Vec3{0.0f}, Quat::identity(), op.scale} : poseOf(frameInv * s.worldMatrix(c));
                const ColliderComponent& col = reg.get<ColliderComponent>(c);
                placed.push_back({&col, rel});
                h = mix(h, c.packed());
                for (float f : {rel.position.x, rel.position.y, rel.position.z, rel.rotation.x, rel.rotation.y, rel.rotation.z, rel.rotation.w,
                                rel.scale.x, rel.scale.y, rel.scale.z})
                    h = mixq(h, f);
                for (float f : {col.size.x, col.size.y, col.size.z, col.radius, col.height, col.offset.x, col.offset.y, col.offset.z, col.density,
                                col.friction, col.restitution})
                    h = mixf(h, f);
                h = mix(h, static_cast<u64>(col.shape) | (static_cast<u64>(col.isTrigger) << 8) | (static_cast<u64>(col.layer & 31) << 16));
                h = mix(h, col.collidesWith);
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
                // Carried over from a rebuilt body: Jolt clamps velocities to exactly its limits, so
                // the length recomputed here can exceed them by rounding; keep a margin.
                auto clampLength = [](Vec3 v, float maxLength) {
                    const float len = math::length(v);
                    const float limit = maxLength * 0.999f;
                    return len > limit ? v * (limit / len) : v;
                };
                bs.mLinearVelocity = toJ(clampLength(keep ? keepVel : rb->initialVelocity, bs.mMaxLinearVelocity));
                bs.mAngularVelocity = toJ(clampLength(keepAng, bs.mMaxAngularVelocity));
            }
            body.id = bi.CreateAndAddBody(bs, JPH::EActivation::Activate);
            if (body.id.IsInvalid()) {
                AGR_LOG_ERROR("Physics3D", "too many bodies: '{}' has no physics", s.nameOf(o));
                continue;
            }
            body.lastPosition = op.position;
            body.lastRotation = op.rotation;
            byBodyId[body.id.GetIndex()] = o;
            if (!placed.empty())
                layers.byIndex[body.id.GetIndex()] = {1u << static_cast<u32>(std::clamp(placed.front().col->layer, 0, 31)), placed.front().col->collidesWith};
            bodies.emplace(key, body);
            ++stats.rebuilt;
        }
        syncTerrains(s);
        syncCharacters(s);
        syncJoints(s);
    }

    // ------------------------------------------------------------ characters
    void syncCharacters(Scene& s) {
        auto& reg = s.registry();
        std::unordered_set<u64> seen;
        reg.view<CharacterController3DComponent>().each([&](ecs::Entity e, CharacterController3DComponent& cc) {
            if (!s.isActiveInHierarchy(e)) return;
            seen.insert(e.packed());
            u64 h = mix(static_cast<u64>(cc.layer & 31) + 1, cc.collidesWith);
            for (float f : {cc.radius, cc.height, cc.maxSlope, cc.stepHeight, cc.mass, cc.pushForce}) h = mixf(h, f);
            Character& c = characters[e.packed()];
            c.gravityScale = cc.gravityScale;
            c.stepHeight = std::max(cc.stepHeight, 0.0f);
            c.mask = cc.collidesWith;
            if (c.ch && c.hash == h) return;
            const Pose p = poseOf(s.worldMatrix(e));
            destroyCharacter(c);
            const float r = std::max(cc.radius, 0.02f);
            const float halfCyl = std::max(cc.height * 0.5f - r, 0.01f);
            JPH::RotatedTranslatedShapeSettings shapeSettings(JPH::Vec3(0.0f, halfCyl + r, 0.0f), JPH::Quat::sIdentity(), new JPH::CapsuleShape(halfCyl, r));
            JPH::ShapeSettings::ShapeResult shape = shapeSettings.Create();
            if (shape.HasError()) {
                AGR_LOG_ERROR("Physics3D", "'{}': invalid character shape: {}", s.nameOf(e), shape.GetError().c_str());
                return;
            }
            JPH::CharacterVirtualSettings cs;
            cs.mShape = shape.Get();
            cs.mMaxSlopeAngle = std::clamp(cc.maxSlope, 0.0f, 1.55f);
            cs.mMass = std::max(cc.mass, 0.1f);
            cs.mMaxStrength = std::max(cc.pushForce, 0.0f);
            cs.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -r);  // contacts below the bottom sphere's center support
            cs.mInnerBodyShape = cs.mShape;
            cs.mInnerBodyLayer = Layers::kMoving;
            c.ch = new JPH::CharacterVirtual(&cs, JPH::RVec3(p.position.x, p.position.y, p.position.z), JPH::Quat::sIdentity(), e.packed(), system.get());
            c.entity = e;
            c.hash = h;
            c.lastPosition = p.position;
            const JPH::BodyID inner = c.ch->GetInnerBodyID();
            if (!inner.IsInvalid()) {
                byBodyId[inner.GetIndex()] = e;
                layers.byIndex[inner.GetIndex()] = {1u << static_cast<u32>(std::clamp(cc.layer, 0, 31)), cc.collidesWith};
            }
            ++stats.rebuilt;
        });
        for (auto it = characters.begin(); it != characters.end();) {
            if (seen.contains(it->first) && s.valid(it->second.entity)) {
                ++it;
                continue;
            }
            destroyCharacter(it->second);
            it = characters.erase(it);
        }
    }

    void stepCharacters(Scene& s, float dt) {
        if (dt <= 0.0f) return;
        for (auto& [key, c] : characters) {
            if (!c.ch || !s.valid(c.entity)) continue;
            const Pose pose = poseOf(s.worldMatrix(c.entity));
            const Vec3 moved = pose.position - c.lastPosition;
            if (math::dot(moved, moved) > 1e-10f) c.ch->SetPosition(JPH::RVec3(pose.position.x, pose.position.y, pose.position.z));  // teleported
            const Vec3 g = gravity * c.gravityScale;
            const bool grounded = c.ch->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
            Vec3 v = c.velocity;
            if (grounded && v.y <= 0.0f) v.y = 0.0f;
            else v += g * dt;
            const Vec3 groundVel = grounded ? fromJ(c.ch->GetGroundVelocity()) : Vec3{0.0f};
            c.ch->SetLinearVelocity(toJ(v + groundVel));
            JPH::CharacterVirtual::ExtendedUpdateSettings us;
            us.mWalkStairsStepUp = JPH::Vec3(0.0f, c.stepHeight, 0.0f);
            us.mStickToFloorStepDown = JPH::Vec3(0.0f, -std::max(c.stepHeight, 0.1f), 0.0f);
            const JPH::DefaultBroadPhaseLayerFilter bpFilter = system->GetDefaultBroadPhaseLayerFilter(Layers::kMoving);
            const JPH::DefaultObjectLayerFilter olFilter = system->GetDefaultLayerFilter(Layers::kMoving);
            const MaskBodyFilter bodyFilter(layers, c.mask, c.ch->GetInnerBodyID());
            const JPH::ShapeFilter shapeFilter;
            c.ch->ExtendedUpdate(dt, toJ(g), us, bpFilter, olFilter, bodyFilter, shapeFilter, *temp);
            const bool nowGrounded = c.ch->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
            Vec3 after = fromJ(c.ch->GetLinearVelocity()) - (nowGrounded ? fromJ(c.ch->GetGroundVelocity()) : Vec3{0.0f});
            if (nowGrounded && after.y < 0.0f) after.y = 0.0f;
            c.velocity = {c.velocity.x, after.y, c.velocity.z};  // horizontal motion stays what the script asked
            const JPH::RVec3 jp = c.ch->GetPosition();
            const Vec3 np{static_cast<float>(jp.GetX()), static_cast<float>(jp.GetY()), static_cast<float>(jp.GetZ())};
            s.setWorldMatrix(c.entity, Mat4::trs(np, pose.rotation, pose.scale));
            c.lastPosition = np;
            // Contacts -> begin / end collision events with the bodies touched.
            std::unordered_set<u32> now;
            for (const auto& ct : c.ch->GetActiveContacts())
                if (ct.mHadCollision && !ct.mIsSensorB && !ct.mBodyB.IsInvalid()) now.insert(ct.mBodyB.GetIndex());
            for (u32 idx : now)
                if (!c.contacts.contains(idx))
                    if (auto it = byBodyId.find(idx); it != byBodyId.end()) events.push_back({c.entity, it->second, true, false});
            for (u32 idx : c.contacts)
                if (!now.contains(idx))
                    if (auto it = byBodyId.find(idx); it != byBodyId.end()) events.push_back({c.entity, it->second, false, false});
            c.contacts = std::move(now);
        }
    }

    const Character* character(ecs::Entity e) const {
        auto it = characters.find(e.packed());
        return it == characters.end() || !it->second.ch ? nullptr : &it->second;
    }

    // ------------------------------------------------------------ joints
    static Vec3 worldPoint(const Mat4& m, Vec3 local) {
        const math::Vec4 w = m * math::Vec4{local.x, local.y, local.z, 1.0f};
        return {w.x, w.y, w.z};
    }

    static JPH::Vec3 perpendicular(JPH::Vec3 axis) {
        const JPH::Vec3 other = std::fabs(axis.GetY()) < 0.9f ? JPH::Vec3::sAxisY() : JPH::Vec3::sAxisX();
        return axis.Cross(other).Normalized();
    }

    void syncJoints(Scene& s) {
        auto& reg = s.registry();
        JPH::BodyInterface& bi = system->GetBodyInterface();
        std::unordered_set<u64> seen;
        reg.view<Joint3DComponent>().each([&](ecs::Entity e, Joint3DComponent& j) {
            if (!s.isActiveInHierarchy(e) || j.broken) return;
            Joint& rec = joints[e.packed()];
            rec.entity = e;
            const ecs::Entity ownerA = owner(s, e);
            const Body* a = ownerA.isNull() ? nullptr : find(ownerA);
            const Body* b = nullptr;
            if (!j.connectedBody.isNull()) {
                if (!s.valid(j.connectedBody) || !s.isActiveInHierarchy(j.connectedBody)) return;
                const ecs::Entity ownerB = owner(s, j.connectedBody);
                b = find(ownerB.isNull() ? j.connectedBody : ownerB);
            }
            if (!a || a->type == BodyType3D::Static || (!j.connectedBody.isNull() && !b) || (b && b == a)) {
                if (!rec.warned)
                    AGR_LOG_WARN("Physics3D", "'{}': Joint3D needs a non-static RigidBody on the entity and a body on its connected entity", s.nameOf(e));
                rec.warned = true;
                return;
            }
            seen.insert(e.packed());
            u64 p = mix(static_cast<u64>(j.type) + 1, j.connectedBody.packed());
            for (float f : {j.anchor.x, j.anchor.y, j.anchor.z, j.connectedAnchor.x, j.connectedAnchor.y, j.connectedAnchor.z, j.axis.x, j.axis.y, j.axis.z,
                            j.length, j.springFrequency, j.springDamping, j.lowerAngle, j.upperAngle, j.lowerLimit, j.upperLimit})
                p = mixf(p, f);
            p = mix(p, static_cast<u64>(j.autoConnectedAnchor) | (static_cast<u64>(j.enableLimits) << 1) | (static_cast<u64>(j.collideConnected) << 2));
            const JPH::BodyID idA = a->id, idB = b ? b->id : JPH::BodyID();
            const u64 h = mix(mix(p, idA.GetIndexAndSequenceNumber()), idB.GetIndexAndSequenceNumber());
            if (rec.c && rec.hash == h) {
                applyMotor(rec, j);
                return;
            }
            removeJoint(rec);
            if (rec.paramHash != p) {
                rec.paramHash = p;
                rec.resolved = false;
            }
            const Mat4 me = s.worldMatrix(e);
            const Vec3 worldA = worldPoint(me, j.anchor);
            if (!rec.resolved) {
                if (j.autoConnectedAnchor) rec.worldB = worldA;
                else rec.worldB = j.connectedBody.isNull() ? j.connectedAnchor : worldPoint(s.worldMatrix(j.connectedBody), j.connectedAnchor);
                rec.length = j.length > 0.0f ? j.length : math::length(rec.worldB - worldA);
                rec.resolved = true;
            }
            JPH::Vec3 axis = toJ(poseOf(me).rotation * j.axis);
            axis = axis.LengthSq() > 1e-10f ? axis.Normalized() : JPH::Vec3::sAxisY();
            const JPH::RVec3 pA(worldA.x, worldA.y, worldA.z), pB(rec.worldB.x, rec.worldB.y, rec.worldB.z);
            JPH::Ref<JPH::TwoBodyConstraintSettings> settings;
            const JPH::SpringSettings spring(JPH::ESpringMode::FrequencyAndDamping, std::max(j.springFrequency, 0.0f), std::max(j.springDamping, 0.0f));
            switch (j.type) {
                case JointType3D::Fixed: {
                    auto* f = new JPH::FixedConstraintSettings();
                    f->mPoint1 = pB;
                    f->mPoint2 = pA;
                    settings = f;
                    break;
                }
                case JointType3D::Hinge: {
                    auto* hs = new JPH::HingeConstraintSettings();
                    hs->mPoint1 = pB;
                    hs->mPoint2 = pA;
                    hs->mHingeAxis1 = hs->mHingeAxis2 = axis;
                    hs->mNormalAxis1 = hs->mNormalAxis2 = perpendicular(axis);
                    if (j.enableLimits) {
                        hs->mLimitsMin = std::clamp(std::min(j.lowerAngle, j.upperAngle), -JPH::JPH_PI, 0.0f);
                        hs->mLimitsMax = std::clamp(std::max(j.lowerAngle, j.upperAngle), 0.0f, JPH::JPH_PI);
                    }
                    if (j.springFrequency > 0.0f) hs->mLimitsSpringSettings = spring;
                    hs->mMotorSettings = JPH::MotorSettings(2.0f, 1.0f, 0.0f, std::max(j.maxMotorForce, 0.0f));
                    settings = hs;
                    break;
                }
                case JointType3D::Distance: {
                    auto* d = new JPH::DistanceConstraintSettings();
                    d->mPoint1 = pB;
                    d->mPoint2 = pA;
                    if (j.enableLimits) {
                        d->mMinDistance = std::max(0.0f, std::min(j.lowerLimit, j.upperLimit));
                        d->mMaxDistance = std::max({0.0f, j.lowerLimit, j.upperLimit});
                    } else {
                        d->mMinDistance = d->mMaxDistance = rec.length;
                    }
                    if (j.springFrequency > 0.0f) d->mLimitsSpringSettings = spring;
                    settings = d;
                    break;
                }
                case JointType3D::Slider: {
                    auto* sl = new JPH::SliderConstraintSettings();
                    sl->mPoint1 = pB;
                    sl->mPoint2 = pA;
                    sl->SetSliderAxis(axis);
                    if (j.enableLimits) {
                        sl->mLimitsMin = std::min({j.lowerLimit, j.upperLimit, 0.0f});
                        sl->mLimitsMax = std::max({j.lowerLimit, j.upperLimit, 0.0f});
                    }
                    if (j.springFrequency > 0.0f) sl->mLimitsSpringSettings = spring;
                    sl->mMotorSettings = JPH::MotorSettings(2.0f, 1.0f, std::max(j.maxMotorForce, 0.0f), 0.0f);
                    settings = sl;
                    break;
                }
                case JointType3D::Ball: {
                    auto* pt = new JPH::PointConstraintSettings();
                    pt->mPoint1 = pB;
                    pt->mPoint2 = pA;
                    settings = pt;
                    break;
                }
            }
            rec.c = bi.CreateConstraint(settings, idB, idA);
            if (!rec.c) return;
            system->AddConstraint(rec.c);
            bi.ActivateConstraint(rec.c);
            rec.type = j.type;
            rec.a = idA;
            rec.b = idB;
            rec.hash = h;
            // Anchors in each body's frame (for jointValue of a distance joint).
            const auto toLocal = [&](JPH::BodyID id, Vec3 w) {
                if (id.IsInvalid()) return w;
                const JPH::RMat44 t = bi.GetWorldTransform(id);
                const JPH::Vec3 l = t.InversedRotationTranslation() * JPH::RVec3(w.x, w.y, w.z);
                return fromJ(l);
            };
            rec.localA = toLocal(idA, worldA);
            rec.localB = toLocal(idB, rec.worldB);
            if (!j.collideConnected) layers.ignoredPairs.insert(pairKey(idA.GetIndex(), idB.IsInvalid() ? 0xFFFFFFFFu : idB.GetIndex()));
            applyMotor(rec, j);
        });
        for (auto it = joints.begin(); it != joints.end();) {
            if (seen.contains(it->first)) {
                ++it;
                continue;
            }
            removeJoint(it->second);
            if (s.valid(it->second.entity) && s.registry().has<Joint3DComponent>(it->second.entity) && it->second.warned) ++it;
            else it = joints.erase(it);
        }
    }

    void applyMotor(Joint& rec, const Joint3DComponent& j) {
        if (!rec.c) return;
        const JPH::EMotorState state = j.enableMotor ? JPH::EMotorState::Velocity : JPH::EMotorState::Off;
        if (rec.type == JointType3D::Hinge) {
            auto* h = static_cast<JPH::HingeConstraint*>(rec.c.GetPtr());
            h->GetMotorSettings().SetTorqueLimit(std::max(j.maxMotorForce, 0.0f));
            h->SetMotorState(state);
            h->SetTargetAngularVelocity(j.motorSpeed);
        } else if (rec.type == JointType3D::Slider) {
            auto* sl = static_cast<JPH::SliderConstraint*>(rec.c.GetPtr());
            sl->GetMotorSettings().SetForceLimit(std::max(j.maxMotorForce, 0.0f));
            sl->SetMotorState(state);
            sl->SetTargetVelocity(j.motorSpeed);
        } else {
            return;
        }
        if (j.enableMotor) system->GetBodyInterface().ActivateConstraint(rec.c);
    }

    float impulseOf(const Joint& rec) const {
        switch (rec.type) {
            case JointType3D::Fixed: return static_cast<JPH::FixedConstraint*>(rec.c.GetPtr())->GetTotalLambdaPosition().Length();
            case JointType3D::Hinge: return static_cast<JPH::HingeConstraint*>(rec.c.GetPtr())->GetTotalLambdaPosition().Length();
            case JointType3D::Distance: return std::fabs(static_cast<JPH::DistanceConstraint*>(rec.c.GetPtr())->GetTotalLambdaPosition());
            case JointType3D::Slider: {
                const auto* sl = static_cast<JPH::SliderConstraint*>(rec.c.GetPtr());
                const JPH::Vector<2> l = sl->GetTotalLambdaPosition();
                return std::sqrt(l[0] * l[0] + l[1] * l[1]) + std::fabs(sl->GetTotalLambdaPositionLimits());
            }
            case JointType3D::Ball: return static_cast<JPH::PointConstraint*>(rec.c.GetPtr())->GetTotalLambdaPosition().Length();
        }
        return 0.0f;
    }

    void checkBreaks(Scene& s, float dt) {
        breaks.clear();
        if (dt <= 0.0f) return;
        for (auto& [key, rec] : joints) {
            if (!rec.c || !s.valid(rec.entity)) continue;
            auto* j = s.registry().tryGet<Joint3DComponent>(rec.entity);
            if (!j || j->breakForce <= 0.0f) continue;
            const float force = impulseOf(rec) / dt;
            if (force <= j->breakForce) continue;
            system->GetBodyInterface().ActivateConstraint(rec.c);
            removeJoint(rec);
            j->broken = true;
            breaks.push_back({rec.entity, force});
        }
    }

    /// Terrains with collision: static triangle meshes, rebuilt when their shape or place changes.
    void syncTerrains(Scene& s) {
        JPH::BodyInterface& bi = system->GetBodyInterface();
        auto& reg = s.registry();
        std::unordered_map<u64, bool> seen;
        reg.view<TerrainComponent>().each([&](ecs::Entity e, TerrainComponent& t) {
            if (!t.collision || !s.isActiveInHierarchy(e)) return;
            seen[e.packed()] = true;
            const Pose p = poseOf(s.worldMatrix(e));
            u64 h = terrain::heightsHash(t);
            for (float f : {p.position.x, p.position.y, p.position.z, p.rotation.x, p.rotation.y, p.rotation.z, p.rotation.w, p.scale.x, p.scale.y,
                            p.scale.z})
                h = mixq(h, f);
            auto it = terrainBodies.find(e.packed());
            if (it != terrainBodies.end() && it->second.hash == h) return;
            if (it != terrainBodies.end()) {
                byBodyId.erase(it->second.id.GetIndex());
                bi.RemoveBody(it->second.id);
                bi.DestroyBody(it->second.id);
                terrainBodies.erase(it);
            }
            terrain::ensureData(t);
            std::vector<Vec3> positions;
            std::vector<u32> indices;
            terrain::buildMesh(t, positions, indices);
            JPH::TriangleList triangles;
            triangles.reserve(indices.size() / 3);
            auto v = [&](u32 i) {
                const Vec3 q{positions[i].x * p.scale.x, positions[i].y * p.scale.y, positions[i].z * p.scale.z};
                return JPH::Float3(q.x, q.y, q.z);
            };
            for (usize i = 0; i + 2 < indices.size(); i += 3) triangles.push_back(JPH::Triangle(v(indices[i]), v(indices[i + 1]), v(indices[i + 2])));
            JPH::MeshShapeSettings settings(triangles);
            JPH::ShapeSettings::ShapeResult r = settings.Create();
            if (r.HasError()) {
                AGR_LOG_ERROR("Physics3D", "'{}': terrain collision failed: {}", s.nameOf(e), r.GetError().c_str());
                return;
            }
            JPH::BodyCreationSettings bs(r.Get(), JPH::RVec3(p.position.x, p.position.y, p.position.z), toJ(math::normalize(p.rotation)),
                                         JPH::EMotionType::Static, Layers::kStatic);
            bs.mUserData = e.packed();
            bs.mFriction = 0.8f;
            const JPH::BodyID id = bi.CreateAndAddBody(bs, JPH::EActivation::DontActivate);
            if (id.IsInvalid()) {
                AGR_LOG_ERROR("Physics3D", "too many bodies: terrain '{}' has no collision", s.nameOf(e));
                return;
            }
            byBodyId[id.GetIndex()] = e;
            terrainBodies[e.packed()] = {id, h};
            ++stats.rebuilt;
        });
        for (auto it = terrainBodies.begin(); it != terrainBodies.end();) {
            if (seen.contains(it->first)) {
                ++it;
                continue;
            }
            byBodyId.erase(it->second.id.GetIndex());
            bi.RemoveBody(it->second.id);
            bi.DestroyBody(it->second.id);
            it = terrainBodies.erase(it);
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
            if (!moved) {
                // A kinematic body keeps the velocity MoveKinematic gave it: without a new target
                // (several fixed steps in one frame) it must stop where it is, not drift on.
                if (body.type == BodyType3D::Kinematic) bi.SetLinearAndAngularVelocity(body.id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
                continue;
            }
            if (body.type == BodyType3D::Kinematic && dt > 0.0f) {
                bi.MoveKinematic(body.id, JPH::RVec3(p.position.x, p.position.y, p.position.z), toJ(math::normalize(p.rotation)), dt);
            } else {
                bi.SetPositionAndRotation(body.id, JPH::RVec3(p.position.x, p.position.y, p.position.z), toJ(math::normalize(p.rotation)),
                                          JPH::EActivation::Activate);
            }
        }
        stepCharacters(s, dt);
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
        // Callbacks arrive from several job threads in any order: sort them so that
        // events are identical to a single-threaded step (all begins, then all ends,
        // each by body pair). Jolt reports a sub-shape pair at most once per step.
        std::stable_sort(pairs.begin(), pairs.end(), [](const Listener::Pair& x, const Listener::Pair& y) {
            if (x.begin != y.begin) return x.begin;
            const u32 xa = std::min(x.a.GetIndex(), x.b.GetIndex()), xb = std::max(x.a.GetIndex(), x.b.GetIndex());
            const u32 ya = std::min(y.a.GetIndex(), y.b.GetIndex()), yb = std::max(y.a.GetIndex(), y.b.GetIndex());
            if (xa != ya) return xa < ya;
            if (xb != yb) return xb < yb;
            return x.a.GetIndex() < y.a.GetIndex();
        });
        for (const Listener::Pair& p : pairs) {
            const u32 ia = p.a.GetIndex(), ib = p.b.GetIndex();
            const u64 key = (static_cast<u64>(std::min(ia, ib)) << 32) | std::max(ia, ib);
            auto ea = byBodyId.find(ia), eb = byBodyId.find(ib);
            if (p.begin) {
                if (activePairs[key]++ == 0) {
                    pairIsSensor[key] = p.sensor;
                    if (ea != byBodyId.end() && eb != byBodyId.end()) {
                        Collision3DEvent e{ea->second, eb->second, true, p.sensor};
                        const Body* fa = find(e.a);
                        if (p.sensor && (!fa || !fa->sensor)) std::swap(e.a, e.b);  // a = the sensor
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
        checkBreaks(s, dt);
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
void Physics3DWorld::setJobSystem(JobSystem* jobs) { m->setJobSystem(jobs); }
void Physics3DWorld::setGravity(Vec3 g) {
    m->gravity = g;
    m->system->SetGravity(toJ(g));
}
Vec3 Physics3DWorld::gravity() const { return m->gravity; }
const std::vector<Collision3DEvent>& Physics3DWorld::events() const { return m->events; }
Physics3DStats Physics3DWorld::stats() const { return m->stats; }
bool Physics3DWorld::hasBody(ecs::Entity e) const { return m->find(e) != nullptr || m->character(e) != nullptr; }

const std::vector<JointBreak3DEvent>& Physics3DWorld::jointBreaks() const { return m->breaks; }

u32 Physics3DWorld::jointCount() const {
    u32 n = 0;
    for (const auto& [key, j] : m->joints) n += j.c ? 1u : 0u;
    return n;
}

std::optional<float> Physics3DWorld::jointValue(ecs::Entity e) const {
    auto it = m->joints.find(e.packed());
    if (it == m->joints.end() || !it->second.c) return std::nullopt;
    const auto& rec = it->second;
    switch (rec.type) {
        case JointType3D::Hinge: return static_cast<JPH::HingeConstraint*>(rec.c.GetPtr())->GetCurrentAngle();
        case JointType3D::Slider: return static_cast<JPH::SliderConstraint*>(rec.c.GetPtr())->GetCurrentPosition();
        case JointType3D::Distance: {
            const JPH::BodyInterface& bi = m->system->GetBodyInterface();
            auto world = [&](JPH::BodyID id, Vec3 local) {
                if (id.IsInvalid()) return local;
                const JPH::RVec3 w = bi.GetWorldTransform(id) * JPH::RVec3(local.x, local.y, local.z);
                return Vec3{static_cast<float>(w.GetX()), static_cast<float>(w.GetY()), static_cast<float>(w.GetZ())};
            };
            return math::length(world(rec.a, rec.localA) - world(rec.b, rec.localB));
        }
        case JointType3D::Fixed:
        case JointType3D::Ball: return 0.0f;
    }
    return std::nullopt;
}

bool Physics3DWorld::hasCharacter(ecs::Entity e) const { return m->character(e) != nullptr; }
bool Physics3DWorld::isGrounded(ecs::Entity e) const {
    const auto* c = m->character(e);
    return c && c->ch->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
}
std::optional<Vec3> Physics3DWorld::groundNormal(ecs::Entity e) const {
    const auto* c = m->character(e);
    if (!c || !c->ch->IsSupported()) return std::nullopt;
    return fromJ(c->ch->GetGroundNormal());
}

std::optional<RaycastHit3D> Physics3DWorld::raycast(Vec3 origin, Vec3 direction, float maxDistance, u32 layerMask) const {
    const float len = math::length(direction);
    if (len < 1e-8f || maxDistance <= 0.0f) return std::nullopt;
    const Vec3 dir = direction / len * maxDistance;
    JPH::RRayCast ray{JPH::RVec3(origin.x, origin.y, origin.z), toJ(dir)};
    JPH::RayCastResult result;
    const MaskBodyFilter filter(m->layers, layerMask);
    if (!m->system->GetNarrowPhaseQuery().CastRay(ray, result, {}, {}, filter)) return std::nullopt;
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
    if (const auto* c = m->character(e)) return c->velocity;
    const auto* b = m->find(e);
    if (!b) return std::nullopt;
    return fromJ(m->system->GetBodyInterface().GetLinearVelocity(b->id));
}
bool Physics3DWorld::setVelocity(ecs::Entity e, Vec3 v) {
    if (auto it = m->characters.find(e.packed()); it != m->characters.end() && it->second.ch) {
        it->second.velocity = v;
        return true;
    }
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
