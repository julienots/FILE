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

    b2WorldId world = b2_nullWorldId;
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
    }

    void reset() {
        bodies.clear();
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
                    h = mix(h, static_cast<u64>(col->shape) | (static_cast<u64>(col->isTrigger) << 8));
                    for (float f : {col->size.x, col->size.y, col->radius, col->offset.x, col->offset.y, col->density, col->friction, col->restitution})
                        h = mixf(h, f);
                }
                if (const auto* tc = reg.tryGet<TilemapCollider2DComponent>(c)) {
                    h = mixf(mixf(h, tc->friction), tc->restitution);
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

std::optional<RaycastHit2D> Physics2DWorld::raycast(Vec2 origin, Vec2 direction, float maxDistance) const {
    const float len = math::length(direction);
    if (len < 1e-8f || maxDistance <= 0.0f) return std::nullopt;
    const Vec2 translation = direction / len * maxDistance;
    const b2RayResult r = b2World_CastRayClosest(m->world, toB2(origin), toB2(translation), b2DefaultQueryFilter());
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
