#include "agr/physics2d/Physics2DWorld.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include <box2d/box2d.h>

#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr {

using math::Mat4;
using math::Vec2;
using math::Vec3;

namespace {

/// 2D pose of an entity: position, rotation about Z, scale of its X / Y axes.
struct Pose2D {
    Vec2 position;
    float angle = 0.0f;
    Vec2 scale{1.0f, 1.0f};
    float z = 0.0f;
    float scaleZ = 1.0f;
};

Pose2D poseOf(const Mat4& m) {
    Pose2D p;
    p.position = {m.c[3].x, m.c[3].y};
    p.z = m.c[3].z;
    p.angle = std::atan2(m.c[0].y, m.c[0].x);
    p.scale = {math::length(Vec3{m.c[0].x, m.c[0].y, m.c[0].z}), math::length(Vec3{m.c[1].x, m.c[1].y, m.c[1].z})};
    p.scaleZ = math::length(Vec3{m.c[2].x, m.c[2].y, m.c[2].z});
    return p;
}

Vec2 rotate(Vec2 v, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

b2Vec2 toB2(Vec2 v) { return {v.x, v.y}; }
Vec2 fromB2(b2Vec2 v) { return {v.x, v.y}; }

u64 mix(u64 h, u64 v) { return h ^ (v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2)); }
u64 mixf(u64 h, float f) { return mix(h, std::bit_cast<u32>(f)); }

float angleDelta(float a, float b) {
    float d = a - b;
    while (d > math::kPi) d -= 2.0f * math::kPi;
    while (d < -math::kPi) d += 2.0f * math::kPi;
    return d;
}

} // namespace

struct Physics2DWorld::Impl {
    struct Body {
        b2BodyId id = b2_nullBodyId;
        ecs::Entity entity;
        u64 hash = 0;
        BodyType2D type = BodyType2D::Static;
        Vec2 lastPosition;
        float lastAngle = 0.0f;
        bool driven = false;  // kinematic velocity set from a transform change this step
        std::vector<ecs::Entity> colliders;  // shape user data points into this vector
        u32 shapes = 0;
    };

    struct Joint {
        b2JointId id = b2_nullJointId;
        ecs::Entity entity;
        JointType2D type = JointType2D::Hinge;
        u64 hash = 0;       // parameters + bodies: rebuilt when it changes
        u64 paramHash = 0;  // parameters only: keeps the resolved anchors across body rebuilds
        bool resolved = false;
        Vec2 worldB;        // connected anchor (world point) resolved at the first creation
        float length = 0.0f;
        float referenceAngle = 0.0f;
        bool warned = false;
    };

    b2WorldId world = b2_nullWorldId;
    b2BodyId ground = b2_nullBodyId;  // static body at the origin: joints to "the world"
    std::unordered_map<u64, Joint> joints;
    std::vector<JointBreak2DEvent> breaks;
    u64 sceneGeneration = 0;
    std::unordered_map<u64, Body> bodies;  // node based: Body addresses are stable
    std::vector<Collision2DEvent> events;
    Vec2 gravity{0.0f, -9.81f};
    int subSteps = 4;
    Physics2DStats stats;

    Impl() { createWorld(); }
    ~Impl() { b2DestroyWorld(world); }

    void createWorld() {
        b2WorldDef def = b2DefaultWorldDef();
        def.gravity = toB2(gravity);
        world = b2CreateWorld(&def);
        b2BodyDef gd = b2DefaultBodyDef();
        ground = b2CreateBody(world, &gd);
    }

    void reset() {
        bodies.clear();
        joints.clear();
        breaks.clear();
        b2DestroyWorld(world);
        createWorld();
        events.clear();
        sceneGeneration = 0;
    }

    const Body* find(ecs::Entity e) const {
        auto it = bodies.find(e.packed());
        return it == bodies.end() || !b2Body_IsValid(it->second.id) ? nullptr : &it->second;
    }

    ecs::Entity entityOf(b2ShapeId shape) const {
        if (!b2Shape_IsValid(shape)) return {};
        const auto* e = static_cast<const ecs::Entity*>(b2Shape_GetUserData(shape));
        return e ? *e : ecs::Entity{};
    }

    /// Nearest entity at or above `e` carrying a RigidBody2D, or null.
    ecs::Entity bodyOwner(Scene& s, ecs::Entity e) const {
        auto& reg = s.registry();
        for (ecs::Entity cur = e; !cur.isNull(); cur = s.parent(cur))
            if (reg.has<RigidBody2DComponent>(cur)) return cur;
        return {};
    }

    u64 tilesHash(const TilemapComponent& tm) const {
        u64 h = mix(mix(tm.width, tm.height), tm.dataWidth);
        h = mixf(mixf(h, tm.cellSize.x), tm.cellSize.y);
        for (i32 t : tm.tiles) h = mix(h, static_cast<u32>(t >= 0));
        return h;
    }

    void sync(Scene& s) {
        if (s.generation() != sceneGeneration) {  // another scene, or the scene was reloaded (editor Stop)
            if (sceneGeneration) reset();
            sceneGeneration = s.generation();
        }
        auto& reg = s.registry();
        stats.rebuilt = 0;
        // 1. Owners and their colliders.
        std::unordered_map<u64, std::vector<ecs::Entity>> owned;
        std::unordered_map<u64, ecs::Entity> owners;
        auto addCollider = [&](ecs::Entity e) {
            if (!s.isActiveInHierarchy(e)) return;
            ecs::Entity owner = bodyOwner(s, e);
            if (owner.isNull()) owner = e;  // implicit static body
            owners[owner.packed()] = owner;
            owned[owner.packed()].push_back(e);
        };
        reg.view<Collider2DComponent>().each([&](ecs::Entity e, Collider2DComponent&) { addCollider(e); });
        reg.view<TilemapCollider2DComponent>().each([&](ecs::Entity e, TilemapCollider2DComponent&) {
            if (reg.has<TilemapComponent>(e)) addCollider(e);
        });
        reg.view<RigidBody2DComponent>().each([&](ecs::Entity e, RigidBody2DComponent&) {
            if (s.isActiveInHierarchy(e)) owners[e.packed()] = e;  // a body without collider still moves
        });
        // 2. Remove bodies whose owner disappeared.
        for (auto it = bodies.begin(); it != bodies.end();) {
            if (!owners.contains(it->first) || !s.valid(it->second.entity)) {
                if (b2Body_IsValid(it->second.id)) b2DestroyBody(it->second.id);
                it = bodies.erase(it);
            } else {
                ++it;
            }
        }
        // 3. Create or rebuild bodies whose description changed.
        for (auto& [key, owner] : owners) {
            std::vector<ecs::Entity>& cols = owned[key];
            std::sort(cols.begin(), cols.end(), [](ecs::Entity a, ecs::Entity b) { return a.index < b.index; });
            const Mat4 ownerWorld = s.worldMatrix(owner);
            const Pose2D ownerPose = poseOf(ownerWorld);
            const RigidBody2DComponent* rb = reg.tryGet<RigidBody2DComponent>(owner);
            u64 h = rb ? mix(static_cast<u64>(rb->type) + 1, rb->fixedRotation | (rb->bullet << 1)) : 0;
            if (rb) h = mixf(mixf(mixf(h, rb->gravityScale), rb->linearDamping), rb->angularDamping);
            const Mat4 bodyFrameInv = math::inverse(Mat4::trs({ownerPose.position.x, ownerPose.position.y, ownerPose.z},
                                                              math::Quat::fromAxisAngle({0, 0, 1}, ownerPose.angle), Vec3{1.0f}));
            struct Placed {
                ecs::Entity e;
                Pose2D rel;
            };
            std::vector<Placed> placed;
            for (ecs::Entity c : cols) {
                const Pose2D rel = poseOf(bodyFrameInv * s.worldMatrix(c));
                placed.push_back({c, rel});
                h = mix(h, c.packed());
                h = mixf(mixf(mixf(mixf(mixf(h, rel.position.x), rel.position.y), rel.angle), rel.scale.x), rel.scale.y);
                if (const auto* col = reg.tryGet<Collider2DComponent>(c)) {
                    h = mix(h, static_cast<u64>(col->shape) | (static_cast<u64>(col->isTrigger) << 8) | (static_cast<u64>(col->layer & 31) << 16));
                    h = mix(h, col->collidesWith);
                    for (float f : {col->size.x, col->size.y, col->radius, col->offset.x, col->offset.y, col->density, col->friction, col->restitution})
                        h = mixf(h, f);
                }
                if (const auto* tc = reg.tryGet<TilemapCollider2DComponent>(c)) {
                    h = mixf(mixf(h, tc->friction), tc->restitution);
                    h = mix(mix(h, static_cast<u64>(tc->layer & 31)), tc->collidesWith);
                    if (const auto* tm = reg.tryGet<TilemapComponent>(c)) h = mix(h, tilesHash(*tm));
                }
            }
            auto it = bodies.find(key);
            if (it != bodies.end() && it->second.hash == h && b2Body_IsValid(it->second.id)) continue;
            Vec2 keepVelocity{};
            float keepAngular = 0.0f;
            bool keep = false;
            if (it != bodies.end()) {
                if (b2Body_IsValid(it->second.id)) {
                    keepVelocity = fromB2(b2Body_GetLinearVelocity(it->second.id));
                    keepAngular = b2Body_GetAngularVelocity(it->second.id);
                    keep = true;
                    b2DestroyBody(it->second.id);
                }
                bodies.erase(it);
            }
            Body& body = bodies[key];
            body.entity = owner;
            body.hash = h;
            body.type = rb ? rb->type : BodyType2D::Static;
            b2BodyDef def = b2DefaultBodyDef();
            def.type = body.type == BodyType2D::Dynamic ? b2_dynamicBody : body.type == BodyType2D::Kinematic ? b2_kinematicBody : b2_staticBody;
            def.position = toB2(ownerPose.position);
            def.rotation = b2MakeRot(ownerPose.angle);
            if (rb) {
                def.gravityScale = rb->gravityScale;
                def.linearDamping = rb->linearDamping;
                def.angularDamping = rb->angularDamping;
                def.fixedRotation = rb->fixedRotation;
                def.isBullet = rb->bullet;
                def.linearVelocity = toB2(keep ? keepVelocity : rb->initialVelocity);
                def.angularVelocity = keep ? keepAngular : 0.0f;
            }
            body.id = b2CreateBody(world, &def);
            body.lastPosition = ownerPose.position;
            body.lastAngle = ownerPose.angle;
            body.colliders.reserve(placed.size());
            for (const Placed& p : placed) body.colliders.push_back(p.e);  // stable addresses from here on
            for (usize i = 0; i < placed.size(); ++i) addShapes(s, body, body.colliders[i], placed[i].rel);
            ++stats.rebuilt;
        }
        syncJoints(s);
    }

    static b2Filter filterOf(i32 layer, u32 mask) {
        b2Filter f = b2DefaultFilter();
        f.categoryBits = 1ull << static_cast<u32>(std::clamp(layer, 0, 31));
        f.maskBits = mask;
        return f;
    }

    static Vec2 worldPoint2(const Mat4& m, Vec2 local) {
        const math::Vec4 w = m * math::Vec4{local.x, local.y, 0.0f, 1.0f};
        return {w.x, w.y};
    }

    void syncJoints(Scene& s) {
        auto& reg = s.registry();
        std::unordered_set<u64> seen;
        reg.view<Joint2DComponent>().each([&](ecs::Entity e, Joint2DComponent& j) {
            if (!s.isActiveInHierarchy(e) || j.broken) return;
            Joint& rec = joints[e.packed()];
            rec.entity = e;
            const ecs::Entity ownerA = bodyOwner(s, e);
            const Body* a = ownerA.isNull() ? nullptr : find(ownerA);
            const Body* b = nullptr;
            if (!j.connectedBody.isNull()) {
                if (!s.valid(j.connectedBody) || !s.isActiveInHierarchy(j.connectedBody)) return;
                const ecs::Entity ownerB = bodyOwner(s, j.connectedBody);
                b = ownerB.isNull() ? find(j.connectedBody) : find(ownerB);
            }
            if (!a || (!j.connectedBody.isNull() && !b) || (b && b == a)) {
                if (!rec.warned)
                    AGR_LOG_WARN("Physics2D", "'{}': Joint2D needs a RigidBody2D on the entity and on its connected body (distinct)", s.nameOf(e));
                rec.warned = true;
                return;
            }
            seen.insert(e.packed());
            u64 p = mix(static_cast<u64>(j.type) + 1, j.connectedBody.packed());
            for (float f : {j.anchor.x, j.anchor.y, j.connectedAnchor.x, j.connectedAnchor.y, j.axis.x, j.axis.y, j.length, j.springFrequency,
                            j.springDamping, j.lowerAngle, j.upperAngle, j.lowerLimit, j.upperLimit})
                p = mixf(p, f);
            p = mix(p, static_cast<u64>(j.autoConnectedAnchor) | (static_cast<u64>(j.enableLimits) << 1) | (static_cast<u64>(j.collideConnected) << 2));
            const b2BodyId bodyB = a->id;                    // this entity
            const b2BodyId bodyA = b ? b->id : ground;      // connected body or the world
            const u64 h = mix(mix(p, b2StoreBodyId(bodyA)), b2StoreBodyId(bodyB));
            if (rec.hash == h && b2Joint_IsValid(rec.id)) {
                applyMotor(rec, j);
                return;
            }
            if (b2Joint_IsValid(rec.id)) b2DestroyJoint(rec.id);
            rec.id = b2_nullJointId;
            if (rec.paramHash != p) {
                rec.paramHash = p;
                rec.resolved = false;
            }
            const Vec2 worldA = worldPoint2(s.worldMatrix(e), j.anchor);
            if (!rec.resolved) {
                if (j.autoConnectedAnchor) rec.worldB = worldA;
                else rec.worldB = j.connectedBody.isNull() ? j.connectedAnchor : worldPoint2(s.worldMatrix(j.connectedBody), j.connectedAnchor);
                rec.length = j.length > 0.0f ? j.length : math::length(rec.worldB - worldA);
                rec.referenceAngle = b2Rot_GetAngle(b2Body_GetRotation(bodyB)) - b2Rot_GetAngle(b2Body_GetRotation(bodyA));
                rec.resolved = true;
            }
            const b2Vec2 localA = b2Body_GetLocalPoint(bodyA, toB2(rec.worldB));
            const b2Vec2 localB = b2Body_GetLocalPoint(bodyB, toB2(worldA));
            rec.type = j.type;
            switch (j.type) {
                case JointType2D::Fixed: {
                    b2WeldJointDef d = b2DefaultWeldJointDef();
                    d.bodyIdA = bodyA, d.bodyIdB = bodyB, d.localAnchorA = localA, d.localAnchorB = localB;
                    d.referenceAngle = rec.referenceAngle;
                    d.linearHertz = d.angularHertz = std::max(j.springFrequency, 0.0f);
                    d.linearDampingRatio = d.angularDampingRatio = std::max(j.springDamping, 0.0f);
                    d.collideConnected = j.collideConnected;
                    rec.id = b2CreateWeldJoint(world, &d);
                    break;
                }
                case JointType2D::Hinge: {
                    b2RevoluteJointDef d = b2DefaultRevoluteJointDef();
                    d.bodyIdA = bodyA, d.bodyIdB = bodyB, d.localAnchorA = localA, d.localAnchorB = localB;
                    d.referenceAngle = rec.referenceAngle;
                    d.enableSpring = j.springFrequency > 0.0f;
                    d.hertz = j.springFrequency;
                    d.dampingRatio = j.springDamping;
                    d.enableLimit = j.enableLimits;
                    d.lowerAngle = std::min(j.lowerAngle, j.upperAngle);
                    d.upperAngle = std::max(j.lowerAngle, j.upperAngle);
                    d.collideConnected = j.collideConnected;
                    rec.id = b2CreateRevoluteJoint(world, &d);
                    break;
                }
                case JointType2D::Distance: {
                    b2DistanceJointDef d = b2DefaultDistanceJointDef();
                    d.bodyIdA = bodyA, d.bodyIdB = bodyB, d.localAnchorA = localA, d.localAnchorB = localB;
                    d.length = std::max(rec.length, 0.01f);
                    // A spring of zero stiffness inside limits is a rope.
                    d.enableSpring = j.springFrequency > 0.0f || j.enableLimits;
                    d.hertz = j.springFrequency;
                    d.dampingRatio = j.springDamping;
                    d.enableLimit = j.enableLimits;
                    d.minLength = std::max(0.0f, std::min(j.lowerLimit, j.upperLimit));
                    d.maxLength = std::max({0.01f, j.lowerLimit, j.upperLimit});
                    d.collideConnected = j.collideConnected;
                    rec.id = b2CreateDistanceJoint(world, &d);
                    break;
                }
                case JointType2D::Slider: {
                    b2PrismaticJointDef d = b2DefaultPrismaticJointDef();
                    d.bodyIdA = bodyA, d.bodyIdB = bodyB, d.localAnchorA = localA, d.localAnchorB = localB;
                    const Pose2D pe = poseOf(s.worldMatrix(e));
                    Vec2 axis = rotate(j.axis, pe.angle);
                    const float len = math::length(axis);
                    axis = len > 1e-6f ? axis / len : Vec2{1.0f, 0.0f};
                    d.localAxisA = b2Body_GetLocalVector(bodyA, toB2(axis));
                    d.referenceAngle = rec.referenceAngle;
                    d.enableSpring = j.springFrequency > 0.0f;
                    d.hertz = j.springFrequency;
                    d.dampingRatio = j.springDamping;
                    d.enableLimit = j.enableLimits;
                    d.lowerTranslation = std::min(j.lowerLimit, j.upperLimit);
                    d.upperTranslation = std::max(j.lowerLimit, j.upperLimit);
                    d.collideConnected = j.collideConnected;
                    rec.id = b2CreatePrismaticJoint(world, &d);
                    break;
                }
            }
            rec.hash = h;
            applyMotor(rec, j);
            b2Joint_WakeBodies(rec.id);
        });
        for (auto it = joints.begin(); it != joints.end();) {
            if (seen.contains(it->first)) {
                ++it;
                continue;
            }
            if (b2Joint_IsValid(it->second.id)) b2DestroyJoint(it->second.id);
            // Keep the record of a joint waiting for its bodies (warned once); drop the others.
            if (s.valid(it->second.entity) && s.registry().has<Joint2DComponent>(it->second.entity) && it->second.warned) {
                it->second.id = b2_nullJointId;
                it->second.hash = 0;
                ++it;
            } else {
                it = joints.erase(it);
            }
        }
    }

    /// Motor settings change without rebuilding the joint (scripts drive them every frame).
    static void applyMotor(const Joint& rec, const Joint2DComponent& j) {
        if (!b2Joint_IsValid(rec.id)) return;
        switch (rec.type) {
            case JointType2D::Hinge:
                b2RevoluteJoint_EnableMotor(rec.id, j.enableMotor);
                b2RevoluteJoint_SetMotorSpeed(rec.id, j.motorSpeed);
                b2RevoluteJoint_SetMaxMotorTorque(rec.id, std::max(j.maxMotorForce, 0.0f));
                break;
            case JointType2D::Slider:
                b2PrismaticJoint_EnableMotor(rec.id, j.enableMotor);
                b2PrismaticJoint_SetMotorSpeed(rec.id, j.motorSpeed);
                b2PrismaticJoint_SetMaxMotorForce(rec.id, std::max(j.maxMotorForce, 0.0f));
                break;
            case JointType2D::Distance:
                b2DistanceJoint_EnableMotor(rec.id, j.enableMotor);
                b2DistanceJoint_SetMotorSpeed(rec.id, j.motorSpeed);
                b2DistanceJoint_SetMaxMotorForce(rec.id, std::max(j.maxMotorForce, 0.0f));
                break;
            case JointType2D::Fixed: break;
        }
        if (j.enableMotor) b2Joint_WakeBodies(rec.id);
    }

    void checkBreaks(Scene& s) {
        breaks.clear();
        for (auto& [key, rec] : joints) {
            if (!b2Joint_IsValid(rec.id) || !s.valid(rec.entity)) continue;
            auto* j = s.registry().tryGet<Joint2DComponent>(rec.entity);
            if (!j || j->breakForce <= 0.0f) continue;
            const float force = math::length(fromB2(b2Joint_GetConstraintForce(rec.id)));
            if (force <= j->breakForce) continue;
            b2Joint_WakeBodies(rec.id);
            b2DestroyJoint(rec.id);
            rec.id = b2_nullJointId;
            rec.hash = 0;
            j->broken = true;
            breaks.push_back({rec.entity, force});
        }
    }

    void addShapes(Scene& s, Body& body, ecs::Entity& entity, const Pose2D& rel) {
        auto& reg = s.registry();
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.userData = &entity;
        sd.enableSensorEvents = true;
        sd.enableContactEvents = true;
        const Vec2 scale{std::fabs(rel.scale.x), std::fabs(rel.scale.y)};
        if (const auto* col = reg.tryGet<Collider2DComponent>(entity)) {
            sd.density = std::max(col->density, 0.0f);
            sd.material.friction = std::max(col->friction, 0.0f);
            sd.material.restitution = std::clamp(col->restitution, 0.0f, 1.0f);
            sd.isSensor = col->isTrigger;
            sd.filter = filterOf(col->layer, col->collidesWith);
            const Vec2 center = rel.position + rotate({col->offset.x * scale.x, col->offset.y * scale.y}, rel.angle);
            switch (col->shape) {
                case ColliderShape2D::Box: {
                    const b2Polygon box = b2MakeOffsetBox(std::max(col->size.x * scale.x * 0.5f, 0.005f),
                                                          std::max(col->size.y * scale.y * 0.5f, 0.005f), toB2(center), b2MakeRot(rel.angle));
                    b2CreatePolygonShape(body.id, &sd, &box);
                    break;
                }
                case ColliderShape2D::Circle: {
                    b2Circle circle{toB2(center), std::max(col->radius * std::max(scale.x, scale.y), 0.005f)};
                    b2CreateCircleShape(body.id, &sd, &circle);
                    break;
                }
                case ColliderShape2D::Capsule: {
                    const float r = std::max(col->size.x * scale.x * 0.5f, 0.005f);
                    const float half = std::max(col->size.y * scale.y * 0.5f - r, 0.001f);
                    const Vec2 axis = rotate({0.0f, half}, rel.angle);
                    b2Capsule capsule{toB2(center - axis), toB2(center + axis), r};
                    b2CreateCapsuleShape(body.id, &sd, &capsule);
                    break;
                }
            }
            ++body.shapes;
        }
        const auto* tc = reg.tryGet<TilemapCollider2DComponent>(entity);
        const auto* tm = reg.tryGet<TilemapComponent>(entity);
        if (tc && tm) {
            sd.density = 0.0f;
            sd.material.friction = std::max(tc->friction, 0.0f);
            sd.material.restitution = std::clamp(tc->restitution, 0.0f, 1.0f);
            sd.isSensor = false;
            sd.filter = filterOf(tc->layer, tc->collidesWith);
            const float cw = tm->cellSize.x * scale.x, ch = tm->cellSize.y * scale.y;
            for (u32 y = 0; y < tm->height; ++y) {
                u32 x = 0;
                while (x < tm->width) {
                    if (tm->tileAt(x, y) < 0) {
                        ++x;
                        continue;
                    }
                    const u32 start = x;
                    while (x < tm->width && tm->tileAt(x, y) >= 0) ++x;  // merge the run
                    const Vec2 local{(static_cast<float>(start + x) * 0.5f) * cw, (static_cast<float>(y) + 0.5f) * ch};
                    const Vec2 center = rel.position + rotate(local, rel.angle);
                    const b2Polygon box = b2MakeOffsetBox(static_cast<float>(x - start) * cw * 0.5f, ch * 0.5f, toB2(center), b2MakeRot(rel.angle));
                    b2CreatePolygonShape(body.id, &sd, &box);
                    ++body.shapes;
                }
            }
        }
    }

    void step(Scene& s, float dt) {
        Stopwatch sw;
        sync(s);
        events.clear();
        // Gameplay moved some bodies: teleport them (static / dynamic) or drive them (kinematic).
        for (auto& [key, body] : bodies) {
            const Pose2D pose = poseOf(s.worldMatrix(body.entity));
            const bool moved = math::dot(pose.position - body.lastPosition, pose.position - body.lastPosition) > 1e-10f || std::fabs(angleDelta(pose.angle, body.lastAngle)) > 1e-6f;
            body.driven = false;
            if (body.type == BodyType2D::Kinematic) {
                const Vec2 current = fromB2(b2Body_GetPosition(body.id));
                const float currentAngle = b2Rot_GetAngle(b2Body_GetRotation(body.id));
                if (moved && dt > 0.0f) {  // reach the new transform during this step (pushes dynamic bodies)
                    b2Body_SetLinearVelocity(body.id, toB2((pose.position - current) / dt));
                    b2Body_SetAngularVelocity(body.id, angleDelta(pose.angle, currentAngle) / dt);
                    body.driven = true;
                }
            } else if (moved) {
                b2Body_SetTransform(body.id, toB2(pose.position), b2MakeRot(pose.angle));
                b2Body_SetAwake(body.id, true);
            }
        }
        if (dt > 0.0f) b2World_Step(world, dt, subSteps);
        checkBreaks(s);
        // Write back.
        u32 dynamicBodies = 0, shapes = 0;
        for (auto& [key, body] : bodies) {
            shapes += body.shapes;
            if (body.type == BodyType2D::Static) continue;
            if (body.type == BodyType2D::Dynamic) ++dynamicBodies;
            const Vec2 p = fromB2(b2Body_GetPosition(body.id));
            const float a = b2Rot_GetAngle(b2Body_GetRotation(body.id));
            if (body.driven) {  // reached its target: stop until moved again (setVelocity keeps moving it)
                b2Body_SetLinearVelocity(body.id, {0.0f, 0.0f});
                b2Body_SetAngularVelocity(body.id, 0.0f);
            }
            const Pose2D pose = poseOf(s.worldMatrix(body.entity));
            s.setWorldMatrix(body.entity, Mat4::trs({p.x, p.y, pose.z}, math::Quat::fromAxisAngle({0, 0, 1}, a), {pose.scale.x, pose.scale.y, pose.scaleZ}));
            body.lastPosition = p;
            body.lastAngle = a;
        }
        // Events.
        const b2ContactEvents contacts = b2World_GetContactEvents(world);
        for (int i = 0; i < contacts.beginCount; ++i)
            events.push_back({entityOf(contacts.beginEvents[i].shapeIdA), entityOf(contacts.beginEvents[i].shapeIdB), true, false});
        for (int i = 0; i < contacts.endCount; ++i)
            events.push_back({entityOf(contacts.endEvents[i].shapeIdA), entityOf(contacts.endEvents[i].shapeIdB), false, false});
        const b2SensorEvents sensors = b2World_GetSensorEvents(world);
        for (int i = 0; i < sensors.beginCount; ++i)
            events.push_back({entityOf(sensors.beginEvents[i].sensorShapeId), entityOf(sensors.beginEvents[i].visitorShapeId), true, true});
        for (int i = 0; i < sensors.endCount; ++i)
            events.push_back({entityOf(sensors.endEvents[i].sensorShapeId), entityOf(sensors.endEvents[i].visitorShapeId), false, true});
        std::erase_if(events, [](const Collision2DEvent& e) { return e.a.isNull() || e.b.isNull(); });
        stats.bodies = static_cast<u32>(bodies.size());
        stats.shapes = shapes;
        stats.dynamicBodies = dynamicBodies;
        stats.stepMs = sw.elapsedMilliseconds();
    }
};

Physics2DWorld::Physics2DWorld() : m(std::make_unique<Impl>()) {}
Physics2DWorld::~Physics2DWorld() = default;

void Physics2DWorld::step(Scene& scene, float dt) { m->step(scene, dt); }
void Physics2DWorld::sync(Scene& scene) { m->sync(scene); }
void Physics2DWorld::reset() { m->reset(); }

void Physics2DWorld::setGravity(Vec2 g) {
    m->gravity = g;
    b2World_SetGravity(m->world, toB2(g));
}
Vec2 Physics2DWorld::gravity() const { return m->gravity; }
void Physics2DWorld::setSubSteps(int n) { m->subSteps = std::clamp(n, 1, 16); }
const std::vector<Collision2DEvent>& Physics2DWorld::events() const { return m->events; }
Physics2DStats Physics2DWorld::stats() const { return m->stats; }

const std::vector<JointBreak2DEvent>& Physics2DWorld::jointBreaks() const { return m->breaks; }

u32 Physics2DWorld::jointCount() const {
    u32 n = 0;
    for (const auto& [key, j] : m->joints) n += b2Joint_IsValid(j.id) ? 1u : 0u;
    return n;
}

std::optional<float> Physics2DWorld::jointValue(ecs::Entity e) const {
    auto it = m->joints.find(e.packed());
    if (it == m->joints.end() || !b2Joint_IsValid(it->second.id)) return std::nullopt;
    switch (it->second.type) {
        case JointType2D::Hinge: return b2RevoluteJoint_GetAngle(it->second.id);
        case JointType2D::Slider: return b2PrismaticJoint_GetTranslation(it->second.id);
        case JointType2D::Distance: return b2DistanceJoint_GetCurrentLength(it->second.id);
        case JointType2D::Fixed: return 0.0f;
    }
    return std::nullopt;
}

std::optional<RaycastHit2D> Physics2DWorld::raycast(Vec2 origin, Vec2 direction, float maxDistance, u32 layerMask) const {
    const float len = math::length(direction);
    if (len < 1e-8f || maxDistance <= 0.0f) return std::nullopt;
    const Vec2 translation = direction / len * maxDistance;
    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = ~0ull;  // seen by every shape, whatever its own mask
    filter.maskBits = layerMask;
    const b2RayResult r = b2World_CastRayClosest(m->world, toB2(origin), toB2(translation), filter);
    if (!r.hit) return std::nullopt;
    RaycastHit2D hit;
    hit.entity = m->entityOf(r.shapeId);
    hit.point = fromB2(r.point);
    hit.normal = fromB2(r.normal);
    hit.distance = r.fraction * maxDistance;
    return hit;
}

bool Physics2DWorld::hasBody(ecs::Entity e) const { return m->find(e) != nullptr; }

std::optional<Vec2> Physics2DWorld::velocity(ecs::Entity e) const {
    const auto* b = m->find(e);
    if (!b) return std::nullopt;
    return fromB2(b2Body_GetLinearVelocity(b->id));
}
bool Physics2DWorld::setVelocity(ecs::Entity e, Vec2 v) {
    const auto* b = m->find(e);
    if (!b || b->type == BodyType2D::Static) return false;
    b2Body_SetLinearVelocity(b->id, toB2(v));
    return true;
}
bool Physics2DWorld::setAngularVelocity(ecs::Entity e, float w) {
    const auto* b = m->find(e);
    if (!b || b->type == BodyType2D::Static) return false;
    b2Body_SetAngularVelocity(b->id, w);
    return true;
}
bool Physics2DWorld::applyForce(ecs::Entity e, Vec2 f) {
    const auto* b = m->find(e);
    if (!b || b->type != BodyType2D::Dynamic) return false;
    b2Body_ApplyForceToCenter(b->id, toB2(f), true);
    return true;
}
bool Physics2DWorld::applyImpulse(ecs::Entity e, Vec2 i) {
    const auto* b = m->find(e);
    if (!b || b->type != BodyType2D::Dynamic) return false;
    b2Body_ApplyLinearImpulseToCenter(b->id, toB2(i), true);
    return true;
}
std::optional<float> Physics2DWorld::mass(ecs::Entity e) const {
    const auto* b = m->find(e);
    if (!b) return std::nullopt;
    return b2Body_GetMass(b->id);
}

} // namespace agr
