#include "agr/physics2d/Physics2DWorld.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cfloat>
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

float cross2(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }

float signedArea(const std::vector<Vec2>& p) {
    float a = 0.0f;
    for (usize i = 0; i < p.size(); ++i) a += cross2(p[i], p[(i + 1) % p.size()]);
    return a * 0.5f;
}

bool isConvex(const std::vector<Vec2>& p) {  // counter-clockwise input
    for (usize i = 0; i < p.size(); ++i)
        if (cross2(p[(i + 1) % p.size()] - p[i], p[(i + 2) % p.size()] - p[(i + 1) % p.size()]) < -1e-7f) return false;
    return true;
}

/// Ear clipping of a simple counter-clockwise polygon into triangles.
std::vector<std::array<Vec2, 3>> triangulate(std::vector<Vec2> p) {
    std::vector<std::array<Vec2, 3>> out;
    auto inside = [](Vec2 q, Vec2 a, Vec2 b, Vec2 c) {
        return cross2(b - a, q - a) >= 0.0f && cross2(c - b, q - b) >= 0.0f && cross2(a - c, q - c) >= 0.0f;
    };
    usize guard = 0;
    while (p.size() > 3 && guard++ < 10000) {
        bool clipped = false;
        for (usize i = 0; i < p.size(); ++i) {
            const Vec2 a = p[(i + p.size() - 1) % p.size()], b = p[i], c = p[(i + 1) % p.size()];
            if (cross2(b - a, c - b) <= 1e-9f) continue;  // reflex or flat
            bool ear = true;
            for (usize k = 0; k < p.size() && ear; ++k) {
                const Vec2 q = p[k];
                if ((q.x == a.x && q.y == a.y) || (q.x == b.x && q.y == b.y) || (q.x == c.x && q.y == c.y)) continue;
                if (inside(q, a, b, c)) ear = false;
            }
            if (!ear) continue;
            out.push_back({a, b, c});
            p.erase(p.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) break;  // self-intersecting: keep what was found
    }
    if (p.size() == 3) out.push_back({p[0], p[1], p[2]});
    return out;
}

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
    // One-way platforms: shape -> local up of its body (read by the pre-solve callback).
    std::unordered_map<u64, Vec2> oneWayShapes;
    struct Character {
        b2BodyId id = b2_nullBodyId;
        b2ShapeId shape = b2_nullShapeId;
        ecs::Entity entity;
        u64 hash = 0;
        Vec2 position;        // capsule origin (entity position)
        b2BodyId platform = b2_nullBodyId;  // floor body of the last step (moving platforms)
        ecs::Entity storage;  // shape user data
    };
    std::unordered_map<u64, Character> characters;

    static u64 shapeKey(b2ShapeId id) { return b2StoreShapeId(id); }

    static bool preSolve(b2ShapeId a, b2ShapeId b, b2Manifold* manifold, void* context) {
        auto* self = static_cast<Impl*>(context);
        if (self->oneWayShapes.empty()) return true;
        auto ia = self->oneWayShapes.find(shapeKey(a));
        auto ib = ia == self->oneWayShapes.end() ? self->oneWayShapes.find(shapeKey(b)) : self->oneWayShapes.end();
        if (ia == self->oneWayShapes.end() && ib == self->oneWayShapes.end()) return true;
        // Manifold normal points from A to B: we want it from the platform toward the other body.
        const bool platformIsA = ia != self->oneWayShapes.end();
        const b2ShapeId platform = platformIsA ? a : b;
        const Vec2 localUp = platformIsA ? ia->second : ib->second;
        const b2Rot rot = b2Body_GetRotation(b2Shape_GetBody(platform));
        const Vec2 up = fromB2(b2RotateVector(rot, toB2(localUp)));
        const Vec2 n = fromB2(manifold->normal) * (platformIsA ? 1.0f : -1.0f);
        return math::dot(n, up) > 0.7f;
    }

    Impl() { createWorld(); }
    ~Impl() { b2DestroyWorld(world); }

    void createWorld() {
        b2WorldDef def = b2DefaultWorldDef();
        def.gravity = toB2(gravity);
        world = b2CreateWorld(&def);
        b2World_SetPreSolveCallback(world, &Impl::preSolve, this);
        b2BodyDef gd = b2DefaultBodyDef();
        ground = b2CreateBody(world, &gd);
    }

    void reset() {
        bodies.clear();
        characters.clear();
        oneWayShapes.clear();
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
                forgetOneWay(it->second.id);
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
                    h = mix(h, static_cast<u64>(col->loop) | (static_cast<u64>(col->oneWay) << 1) | (col->points.size() << 2));
                    for (const Vec2& p : col->points) h = mixf(mixf(h, p.x), p.y);
                }
                if (const auto* tc = reg.tryGet<TilemapCollider2DComponent>(c)) {
                    h = mixf(mixf(h, tc->friction), tc->restitution);
                    h = mix(mix(h, std::hash<std::string>{}(tc->oneWayTiles)), std::hash<std::string>{}(tc->slopeTiles));
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
                    forgetOneWay(it->second.id);
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
        syncCharacters(s);
    }

    void forgetOneWay(b2BodyId body) {
        if (oneWayShapes.empty() || !b2Body_IsValid(body)) return;
        const int n = b2Body_GetShapeCount(body);
        std::vector<b2ShapeId> ids(static_cast<usize>(n));
        b2Body_GetShapes(body, ids.data(), n);
        for (b2ShapeId id : ids) oneWayShapes.erase(shapeKey(id));
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

    /// One-way platform: up = the collider's local +Y in its body's frame.
    void rememberOneWay(const Collider2DComponent* col, b2ShapeId shape, float relAngle) {
        if (col->oneWay && !col->isTrigger) oneWayShapes[shapeKey(shape)] = rotate({0.0f, 1.0f}, relAngle);
    }

    void addChain(Body& body, const b2ShapeDef& sd, const std::vector<Vec2>& pts, bool loop, ecs::Entity& entity) {
        if (pts.size() < 2) return;
        // Open chains: Box2D uses the first and last points as ghost vertices only (no segment);
        // extrapolated ghosts make every user segment solid.
        std::vector<Vec2> chain = pts;
        if (!loop) {
            chain.insert(chain.begin(), pts[0] + (pts[0] - pts[1]));
            chain.push_back(pts.back() + (pts.back() - pts[pts.size() - 2]));
        }
        if (chain.size() >= 4) {
            // Box2D chains are one-sided (solid to the right of their direction): one chain per
            // direction makes the outline solid on both sides whatever order the points are in.
            for (int pass = 0; pass < 2; ++pass) {
                std::vector<b2Vec2> bp;
                for (const Vec2& p : chain) bp.push_back(toB2(p));
                if (pass == 1) std::reverse(bp.begin(), bp.end());
                b2ChainDef cd = b2DefaultChainDef();
                cd.userData = &entity;
                cd.points = bp.data();
                cd.count = static_cast<int>(bp.size());
                cd.materials = &sd.material;
                cd.materialCount = 1;
                cd.filter = sd.filter;
                cd.isLoop = loop;
                cd.enableSensorEvents = true;
                b2CreateChain(body.id, &cd);
            }
            return;
        }
        // A loop of 3 points: plain two-sided segments.
        const usize edges = loop && pts.size() > 2 ? pts.size() : pts.size() - 1;
        for (usize i = 0; i < edges; ++i) {
            const b2Segment seg{toB2(pts[i]), toB2(pts[(i + 1) % pts.size()])};
            b2CreateSegmentShape(body.id, &sd, &seg);
        }
    }

    void addShapes(Scene& s, Body& body, ecs::Entity& entity, const Pose2D& rel) {
        auto& reg = s.registry();
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.userData = &entity;
        sd.enableSensorEvents = true;
        sd.enableContactEvents = true;
        const Vec2 scale{std::fabs(rel.scale.x), std::fabs(rel.scale.y)};
        sd.enablePreSolveEvents = body.type == BodyType2D::Dynamic;  // one-way platforms (pre-solve runs for dynamic bodies)
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
                    rememberOneWay(col, b2CreatePolygonShape(body.id, &sd, &box), rel.angle);
                    break;
                }
                case ColliderShape2D::Circle: {
                    b2Circle circle{toB2(center), std::max(col->radius * std::max(scale.x, scale.y), 0.005f)};
                    rememberOneWay(col, b2CreateCircleShape(body.id, &sd, &circle), rel.angle);
                    break;
                }
                case ColliderShape2D::Capsule: {
                    const float r = std::max(col->size.x * scale.x * 0.5f, 0.005f);
                    const float half = std::max(col->size.y * scale.y * 0.5f - r, 0.001f);
                    const Vec2 axis = rotate({0.0f, half}, rel.angle);
                    b2Capsule capsule{toB2(center - axis), toB2(center + axis), r};
                    rememberOneWay(col, b2CreateCapsuleShape(body.id, &sd, &capsule), rel.angle);
                    break;
                }
                case ColliderShape2D::Polygon:
                case ColliderShape2D::Chain: {
                    // Local points -> body frame (offset, scale, rotation of the collider entity).
                    std::vector<Vec2> pts;
                    pts.reserve(col->points.size());
                    for (const Vec2& p : col->points) pts.push_back(center + rotate({p.x * scale.x, p.y * scale.y}, rel.angle));
                    if (col->shape == ColliderShape2D::Chain) {
                        addChain(body, sd, pts, col->loop, entity);
                        if (col->oneWay) AGR_LOG_WARN("Physics2D", "chains cannot be one-way platforms (use a Polygon)");
                        break;
                    }
                    if (pts.size() < 3) break;
                    if (signedArea(pts) < 0.0f) std::reverse(pts.begin(), pts.end());
                    if (pts.size() <= B2_MAX_POLYGON_VERTICES && isConvex(pts)) {
                        std::vector<b2Vec2> bp;
                        for (const Vec2& p : pts) bp.push_back(toB2(p));
                        const b2Hull hull = b2ComputeHull(bp.data(), static_cast<int>(bp.size()));
                        if (hull.count >= 3) {
                            const b2Polygon poly = b2MakePolygon(&hull, 0.0f);
                            rememberOneWay(col, b2CreatePolygonShape(body.id, &sd, &poly), rel.angle);
                        }
                    } else {
                        for (const auto& t : triangulate(pts)) {  // concave: convex pieces
                            const b2Vec2 tri[3] = {toB2(t[0]), toB2(t[1]), toB2(t[2])};
                            const b2Hull hull = b2ComputeHull(tri, 3);
                            if (hull.count < 3) continue;
                            const b2Polygon poly = b2MakePolygon(&hull, 0.0f);
                            rememberOneWay(col, b2CreatePolygonShape(body.id, &sd, &poly), rel.angle);
                        }
                    }
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
            // Tile kinds: 0 solid, 1 one-way, 2 slope rising right, 3 slope rising left.
            std::unordered_map<i32, int> kinds;
            {
                std::string cur;
                for (char c : tc->oneWayTiles + ",") {
                    if (c == ',' || c == ';') {
                        if (!cur.empty()) kinds[std::atoi(cur.c_str())] = 1;
                        cur.clear();
                    } else if (c != ' ') {
                        cur += c;
                    }
                }
                for (char c : tc->slopeTiles + ",") {
                    if (c == ',' || c == ';') {
                        const usize colon = cur.find(':');
                        if (colon != std::string::npos && colon + 1 < cur.size()) {
                            // "/" "\" full slopes; "/1" "/2" ("\1" "\2"): lower / upper half of a gentle slope over two cells.
                            const bool right = cur[colon + 1] == '/';
                            const char half = colon + 2 < cur.size() ? cur[colon + 2] : '0';
                            kinds[std::atoi(cur.substr(0, colon).c_str())] = (right ? 2 : 3) + (half == '1' ? 2 : half == '2' ? 4 : 0);
                        }
                        cur.clear();
                    } else if (c != ' ') {
                        cur += c;
                    }
                }
            }
            auto kindAt = [&](u32 x, u32 y) {
                const i32 id = tm->tileAt(x, y);
                if (id < 0) return -1;
                auto it = kinds.find(id);
                return it == kinds.end() ? 0 : it->second;
            };
            const Vec2 up = rotate({0.0f, 1.0f}, rel.angle);
            for (u32 y = 0; y < tm->height; ++y) {
                u32 x = 0;
                while (x < tm->width) {
                    const int kind = kindAt(x, y);
                    if (kind < 0) {
                        ++x;
                        continue;
                    }
                    if (kind >= 2) {  // slope: under the line from (x0, hLeft) to (x1, hRight) in the cell
                        const float x0 = static_cast<float>(x) * cw, x1 = x0 + cw, y0 = static_cast<float>(y) * ch;
                        const bool right = kind % 2 == 0;  // 2, 4, 6 rise to the right
                        float lo = 0.0f, hi = 1.0f;          // heights (fraction of the cell) at the low / high ends
                        if (kind >= 6) lo = 0.5f, hi = 1.0f; // upper half of a gentle slope
                        else if (kind >= 4) lo = 0.0f, hi = 0.5f;
                        const float hl = (right ? lo : hi) * ch, hr = (right ? hi : lo) * ch;
                        std::vector<Vec2> poly = {{x0, y0}, {x1, y0}};
                        if (hr > 1e-6f) poly.push_back({x1, y0 + hr});
                        if (hl > 1e-6f) poly.push_back({x0, y0 + hl});
                        b2Vec2 pts[4];
                        for (usize k = 0; k < poly.size(); ++k) pts[k] = toB2(rel.position + rotate(poly[k], rel.angle));
                        const b2Hull hull = b2ComputeHull(pts, static_cast<int>(poly.size()));
                        if (hull.count >= 3) {
                            const b2Polygon poly = b2MakePolygon(&hull, 0.0f);
                            b2CreatePolygonShape(body.id, &sd, &poly);
                            ++body.shapes;
                        }
                        ++x;
                        continue;
                    }
                    const u32 start = x;
                    while (x < tm->width && kindAt(x, y) == kind) ++x;  // merge the run of the same kind
                    const Vec2 local{(static_cast<float>(start + x) * 0.5f) * cw, (static_cast<float>(y) + 0.5f) * ch};
                    const Vec2 center = rel.position + rotate(local, rel.angle);
                    const b2Polygon box = b2MakeOffsetBox(static_cast<float>(x - start) * cw * 0.5f, ch * 0.5f, toB2(center), b2MakeRot(rel.angle));
                    const b2ShapeId shape = b2CreatePolygonShape(body.id, &sd, &box);
                    if (kind == 1) oneWayShapes[shapeKey(shape)] = up;
                    ++body.shapes;
                }
            }
        }
    }

    // ------------------------------------------------------------ characters
    struct MoverQuery {
        const Impl* self;
        b2BodyId ownBody;
        Vec2 velocity;
        std::vector<b2CollisionPlane> planes;
        bool floor = false, wall = false, ceiling = false;
        Vec2 floorNormal{0.0f, 1.0f};
        b2BodyId floorBody = b2_nullBodyId;
        float floorCos = 0.7f;
        Vec2 up{0.0f, 1.0f};
        bool pushDynamic = true;  // dynamic bodies are soft planes, pushed by the kinematic body
    };

    /// Shapes a character ignores: its own body, triggers, and one-way platforms it is not
    /// standing on (`normal`: from the shape toward the character; zero = unknown).
    bool ignoredByMover(b2ShapeId id, const MoverQuery& q, Vec2 normal) const {
        if (B2_ID_EQUALS(b2Shape_GetBody(id), q.ownBody) || b2Shape_IsSensor(id)) return true;
        auto ow = oneWayShapes.find(shapeKey(id));
        if (ow == oneWayShapes.end()) return false;
        const Vec2 up = fromB2(b2RotateVector(b2Body_GetRotation(b2Shape_GetBody(id)), toB2(ow->second)));
        if (math::dot(q.velocity, up) > 0.01f) return true;  // going up through it
        return math::dot(normal, normal) > 0.0f && math::dot(normal, up) < 0.7f;
    }

    static b2ShapeProxy proxyOf(b2ShapeId id) {
        switch (b2Shape_GetType(id)) {
            case b2_circleShape: {
                const b2Circle c = b2Shape_GetCircle(id);
                return b2MakeProxy(&c.center, 1, c.radius);
            }
            case b2_capsuleShape: {
                const b2Capsule c = b2Shape_GetCapsule(id);
                const b2Vec2 p[2] = {c.center1, c.center2};
                return b2MakeProxy(p, 2, c.radius);
            }
            case b2_segmentShape: {
                const b2Segment sg = b2Shape_GetSegment(id);
                const b2Vec2 p[2] = {sg.point1, sg.point2};
                return b2MakeProxy(p, 2, 0.0f);
            }
            case b2_chainSegmentShape: {
                const b2ChainSegment cs = b2Shape_GetChainSegment(id);
                const b2Vec2 p[2] = {cs.segment.point1, cs.segment.point2};
                return b2MakeProxy(p, 2, 0.0f);
            }
            case b2_polygonShape: {
                const b2Polygon poly = b2Shape_GetPolygon(id);
                return b2MakeProxy(poly.vertices, poly.count, poly.radius);
            }
            default: return b2MakeProxy(nullptr, 0, 0.0f);
        }
    }

    /// Fraction of `translation` the capsule can travel (shapes it already touches do not stop it).
    float castMover(const b2Capsule& mover, Vec2 translation, b2QueryFilter filter, MoverQuery& q) const {
        b2AABB box;
        box.lowerBound = {std::min(mover.center1.x, mover.center2.x) - mover.radius, std::min(mover.center1.y, mover.center2.y) - mover.radius};
        box.upperBound = {std::max(mover.center1.x, mover.center2.x) + mover.radius, std::max(mover.center1.y, mover.center2.y) + mover.radius};
        box.lowerBound = {box.lowerBound.x + std::min(translation.x, 0.0f), box.lowerBound.y + std::min(translation.y, 0.0f)};
        box.upperBound = {box.upperBound.x + std::max(translation.x, 0.0f), box.upperBound.y + std::max(translation.y, 0.0f)};
        std::vector<b2ShapeId> found;
        b2World_OverlapAABB(world, box, filter, [](b2ShapeId id, void* ctx) {
            static_cast<std::vector<b2ShapeId>*>(ctx)->push_back(id);
            return true;
        }, &found);
        const b2Vec2 pts[2] = {mover.center1, mover.center2};
        float fraction = 1.0f;
        for (b2ShapeId id : found) {
            if (ignoredByMover(id, q, {})) continue;
            b2ShapeCastPairInput in{};
            in.proxyA = proxyOf(id);
            if (in.proxyA.count == 0) continue;
            in.transformA = b2Body_GetTransform(b2Shape_GetBody(id));
            in.proxyB = b2MakeProxy(pts, 2, mover.radius);
            in.transformB = b2Transform_identity;
            in.translationB = toB2(translation);
            in.maxFraction = fraction;
            in.canEncroach = true;
            const b2CastOutput out = b2ShapeCast(&in);
            if (!out.hit || out.fraction <= 0.0f) continue;  // touching already: the planes handle it
            Vec2 n = fromB2(out.normal);
            if (math::dot(n, translation) > 0.0f) n = -n;  // toward the mover
            if (oneWayShapes.contains(shapeKey(id)) && ignoredByMover(id, q, n)) continue;
            fraction = std::min(fraction, out.fraction);
        }
        return fraction;
    }

    void collectPlanes(const b2Capsule& mover, b2QueryFilter filter, MoverQuery& q) const {
        q.planes.clear();
        b2World_CollideMover(world, &mover, filter, [](b2ShapeId id, const b2PlaneResult* r, void* ctx) {
            auto* query = static_cast<MoverQuery*>(ctx);
            const Vec2 n = fromB2(r->plane.normal);
            if (query->self->ignoredByMover(id, *query, n)) return true;
            const bool soft = query->pushDynamic && b2Body_GetType(b2Shape_GetBody(id)) == b2_dynamicBody;
            query->planes.push_back({r->plane, soft ? 0.0f : FLT_MAX, 0.0f, !soft});
            const float d = math::dot(n, query->up);
            if (d >= query->floorCos) {
                if (!query->floor || d > math::dot(query->floorNormal, query->up)) {
                    query->floorNormal = n;
                    query->floorBody = b2Shape_GetBody(id);
                }
                query->floor = true;
            } else if (d <= -0.7f) {
                query->ceiling = true;
            } else {
                query->wall = true;
            }
            return query->planes.size() < 16;
        }, &q);
    }

    void syncCharacters(Scene& s) {
        auto& reg = s.registry();
        std::unordered_set<u64> seen;
        reg.view<CharacterController2DComponent>().each([&](ecs::Entity e, CharacterController2DComponent& c) {
            if (!s.isActiveInHierarchy(e)) return;
            seen.insert(e.packed());
            const Pose2D pose = poseOf(s.worldMatrix(e));
            u64 h = mix(mix(static_cast<u64>(c.layer & 31), c.collidesWith), e.packed());
            for (float f : {c.size.x, c.size.y, c.offset.x, c.offset.y, pose.scale.x, pose.scale.y}) h = mixf(h, f);
            Character& ch = characters[e.packed()];
            if (ch.hash == h && b2Body_IsValid(ch.id)) return;
            if (b2Body_IsValid(ch.id)) b2DestroyBody(ch.id);
            ch.entity = ch.storage = e;
            ch.hash = h;
            ch.position = pose.position;
            b2BodyDef def = b2DefaultBodyDef();
            def.type = b2_kinematicBody;
            def.position = toB2(pose.position);
            def.fixedRotation = true;
            ch.id = b2CreateBody(world, &def);
            b2ShapeDef sd = b2DefaultShapeDef();
            sd.userData = &ch.storage;
            sd.enableSensorEvents = true;
            sd.enableContactEvents = true;
            sd.filter = filterOf(c.layer, c.collidesWith);
            const b2Capsule cap = capsuleOf(c, pose, {0.0f, 0.0f});
            ch.shape = b2CreateCapsuleShape(ch.id, &sd, &cap);
        });
        for (auto it = characters.begin(); it != characters.end();) {
            if (!seen.contains(it->first) || !s.valid(it->second.entity)) {
                if (b2Body_IsValid(it->second.id)) b2DestroyBody(it->second.id);
                it = characters.erase(it);
            } else {
                ++it;
            }
        }
    }

    /// Capsule of a character, relative to `origin` (body frame when origin = 0).
    static b2Capsule capsuleOf(const CharacterController2DComponent& c, const Pose2D& pose, Vec2 origin) {
        const Vec2 scale{std::fabs(pose.scale.x), std::fabs(pose.scale.y)};
        const float r = std::max(c.size.x * scale.x * 0.5f, 0.01f);
        const float half = std::max(c.size.y * scale.y * 0.5f - r, 0.0f);
        const Vec2 center = origin + Vec2{c.offset.x * scale.x, c.offset.y * scale.y};
        return {toB2(center - Vec2{0.0f, half}), toB2(center + Vec2{0.0f, half}), r};
    }

    /// move_and_slide: collide, solve the planes, sweep, a few iterations; then clip the velocity.
    void moveCharacters(Scene& s, float dt) {
        if (dt <= 0.0f) return;
        auto& reg = s.registry();
        for (auto& [key, ch] : characters) {
            auto* c = reg.tryGet<CharacterController2DComponent>(ch.entity);
            if (!c || !b2Body_IsValid(ch.id)) continue;
            const Pose2D pose = poseOf(s.worldMatrix(ch.entity));
            if (math::length(pose.position - ch.position) > 1e-5f) ch.position = pose.position;  // moved by gameplay: teleport
            MoverQuery q{this, ch.id, c->velocity, {}, false, false, false, {0.0f, 1.0f}, b2_nullBodyId, std::cos(std::clamp(c->maxSlope, 0.0f, 1.55f)), {0.0f, 1.0f},
                         c->pushForce > 0.0f};
            b2QueryFilter filter{1ull << static_cast<u32>(std::clamp(c->layer, 0, 31)), c->collidesWith};
            Vec2 velocity = c->velocity;
            const bool wasOnFloor = c->onFloor;
            if (!(wasOnFloor && math::dot(velocity, q.up) <= 0.0f)) velocity += gravity * c->gravityScale * dt;
            q.velocity = velocity;
            // Moving platform under the character: carried along.
            Vec2 carry{0.0f, 0.0f};
            if (wasOnFloor && b2Body_IsValid(ch.platform) && b2Body_GetType(ch.platform) != b2_staticBody)
                carry = fromB2(b2Body_GetWorldPointVelocity(ch.platform, toB2(ch.position))) * dt;
            Vec2 pos = ch.position;
            const Vec2 target = pos + velocity * dt + carry;
            bool wallSeen = false, ceilingSeen = false;
            for (int iteration = 0; iteration < 5; ++iteration) {
                const b2Capsule mover = capsuleOf(*c, pose, pos);
                collectPlanes(mover, filter, q);
                wallSeen |= q.wall;
                ceilingSeen |= q.ceiling;
                const b2PlaneSolverResult result = b2SolvePlanes(toB2(target - pos), q.planes.data(), static_cast<int>(q.planes.size()));
                const Vec2 translation = fromB2(result.translation);
                const float fraction = castMover(mover, translation, filter, q);
                pos += translation * fraction;
                if (math::dot(translation * fraction, translation * fraction) < 1e-8f) break;
            }
            velocity = fromB2(b2ClipVector(toB2(velocity), q.planes.data(), static_cast<int>(q.planes.size())));
            // Snap down slopes and steps while walking (not when jumping).
            if (wasOnFloor && c->snapDistance > 0.0f && math::dot(velocity, q.up) <= 0.0f) {
                const b2Capsule mover = capsuleOf(*c, pose, pos);
                const Vec2 down = q.up * -c->snapDistance;
                const float f = castMover(mover, down, filter, q);
                if (f < 1.0f) pos += down * std::max(f - 0.01f, 0.0f);
            }
            // State at the final position: planes slightly below (floor contact without overlap).
            q.floor = q.wall = q.ceiling = false;
            q.floorBody = b2_nullBodyId;
            q.velocity = velocity;
            collectPlanes(capsuleOf(*c, pose, pos - q.up * 0.02f), filter, q);
            const bool floor = q.floor;
            const Vec2 floorNormal = q.floorNormal;
            const b2BodyId floorBody = q.floorBody;
            wallSeen |= q.wall;
            q.floor = q.wall = q.ceiling = false;
            collectPlanes(capsuleOf(*c, pose, pos + q.up * 0.02f), filter, q);  // ceiling probe
            ceilingSeen |= q.ceiling;
            q.floor = floor;
            q.floorNormal = floorNormal;
            q.floorBody = floorBody;
            c->onFloor = q.floor;
            c->onWall = wallSeen;
            c->onCeiling = ceilingSeen;
            c->floorNormal = q.floor ? q.floorNormal : Vec2{0.0f, 1.0f};
            ch.platform = q.floor ? q.floorBody : b2_nullBodyId;
            if (q.floor && math::dot(velocity, q.up) < 0.0f) velocity -= q.up * math::dot(velocity, q.up);
            c->velocity = velocity;
            // The kinematic body reaches the new position during the step (pushes dynamic bodies).
            const Vec2 current = fromB2(b2Body_GetPosition(ch.id));
            b2Body_SetLinearVelocity(ch.id, toB2((pos - current) / dt));
            ch.position = pos;
        }
    }

    void writeCharacters(Scene& s) {
        for (auto& [key, ch] : characters) {
            if (!b2Body_IsValid(ch.id) || !s.valid(ch.entity)) continue;
            b2Body_SetTransform(ch.id, toB2(ch.position), b2Rot_identity);  // exact, whatever pushed it
            b2Body_SetLinearVelocity(ch.id, {0.0f, 0.0f});
            const Pose2D pose = poseOf(s.worldMatrix(ch.entity));
            s.setWorldMatrix(ch.entity, Mat4::trs({ch.position.x, ch.position.y, pose.z}, math::Quat::fromAxisAngle({0, 0, 1}, pose.angle),
                                                   {pose.scale.x, pose.scale.y, pose.scaleZ}));
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
        moveCharacters(s, dt);
        if (dt > 0.0f) b2World_Step(world, dt, subSteps);
        writeCharacters(s);
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

namespace {
b2ShapeProxy queryProxy(const QueryShape2D& q, Vec2 position, float angle) {
    std::vector<b2Vec2> pts;
    float radius = 0.0f;
    switch (q.shape) {
        case ColliderShape2D::Circle: pts = {{0.0f, 0.0f}}; radius = std::max(q.radius, 0.0f); break;
        case ColliderShape2D::Capsule: {
            radius = std::max(q.size.x * 0.5f, 0.0f);
            const float half = std::max(q.size.y * 0.5f - radius, 0.0f);
            pts = {{0.0f, -half}, {0.0f, half}};
            break;
        }
        case ColliderShape2D::Polygon:
        case ColliderShape2D::Chain:
            for (usize i = 0; i < q.points.size() && i < B2_MAX_POLYGON_VERTICES; ++i) pts.push_back(toB2(q.points[i]));
            break;
        case ColliderShape2D::Box:
        default: {
            const Vec2 h = q.size * 0.5f;
            pts = {{-h.x, -h.y}, {h.x, -h.y}, {h.x, h.y}, {-h.x, h.y}};
            break;
        }
    }
    if (pts.empty()) pts = {{0.0f, 0.0f}};
    return b2MakeOffsetProxy(pts.data(), static_cast<int>(pts.size()), radius, toB2(position), b2MakeRot(angle));
}

b2QueryFilter queryFilter(u32 layerMask) {
    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = ~0ull;  // seen by every shape, whatever its own mask
    filter.maskBits = layerMask;
    return filter;
}
} // namespace

std::vector<ecs::Entity> Physics2DWorld::overlap(const QueryShape2D& shape, Vec2 position, float angle, u32 layerMask, bool includeTriggers) const {
    struct Ctx {
        const Impl* self;
        bool triggers;
        std::vector<ecs::Entity> out;
    } ctx{m.get(), includeTriggers, {}};
    const b2ShapeProxy proxy = queryProxy(shape, position, angle);
    b2World_OverlapShape(m->world, &proxy, queryFilter(layerMask), [](b2ShapeId id, void* c) {
        auto* ctx = static_cast<Ctx*>(c);
        if (!ctx->triggers && b2Shape_IsSensor(id)) return true;
        const ecs::Entity e = ctx->self->entityOf(id);
        if (!e.isNull()) ctx->out.push_back(e);
        return true;
    }, &ctx);
    std::sort(ctx.out.begin(), ctx.out.end(), [](ecs::Entity a, ecs::Entity b) { return a.packed() < b.packed(); });
    ctx.out.erase(std::unique(ctx.out.begin(), ctx.out.end()), ctx.out.end());
    return ctx.out;
}

std::optional<RaycastHit2D> Physics2DWorld::castShape(const QueryShape2D& shape, Vec2 position, float angle, Vec2 direction, float maxDistance,
                                                      u32 layerMask) const {
    const float len = math::length(direction);
    if (len < 1e-8f || maxDistance <= 0.0f) return std::nullopt;
    struct Ctx {
        const Impl* self;
        float fraction = 2.0f;
        b2ShapeId shape = b2_nullShapeId;
        b2Vec2 point{}, normal{};
    } ctx{m.get()};
    const b2ShapeProxy proxy = queryProxy(shape, position, angle);
    b2World_CastShape(m->world, &proxy, toB2(direction / len * maxDistance), queryFilter(layerMask),
                      [](b2ShapeId id, b2Vec2 point, b2Vec2 normal, float fraction, void* c) -> float {
                          auto* ctx = static_cast<Ctx*>(c);
                          if (b2Shape_IsSensor(id) || fraction <= 0.0f) return -1.0f;  // triggers, initial overlaps
                          if (fraction < ctx->fraction) {
                              ctx->fraction = fraction;
                              ctx->shape = id;
                              ctx->point = point;
                              ctx->normal = normal;
                          }
                          return fraction;
                      },
                      &ctx);
    if (ctx.fraction > 1.0f) return std::nullopt;
    RaycastHit2D hit;
    hit.entity = m->entityOf(ctx.shape);
    hit.point = fromB2(ctx.point);
    hit.normal = fromB2(ctx.normal);
    hit.distance = ctx.fraction * maxDistance;
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
