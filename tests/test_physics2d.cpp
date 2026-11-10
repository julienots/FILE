#include <doctest/doctest.h>

#include <cmath>

#include "agr/physics2d/Physics2DWorld.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/twod/TilemapEditing.hpp"

using namespace agr;
using math::Vec2;
using math::Vec3;

namespace {

struct World2D {
    Scene scene;
    Physics2DWorld physics;
    World2D() {
        registerRenderComponents();
        registerPhysics2DComponents();
    }
    ecs::Entity box(const char* name, Vec2 pos, Vec2 size, BodyType2D type, bool body = true) {
        const auto e = scene.createEntity(name);
        scene.registry().get<TransformComponent>(e).position = {pos.x, pos.y, 0.0f};
        if (body) scene.registry().emplace<RigidBody2DComponent>(e).type = type;
        scene.registry().emplace<Collider2DComponent>(e).size = size;
        return e;
    }
    Vec3 position(ecs::Entity e) { return scene.registry().get<TransformComponent>(e).position; }
    /// Runs `seconds` at 60 Hz, collecting the events.
    std::vector<Collision2DEvent> run(float seconds) {
        std::vector<Collision2DEvent> all;
        for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) {
            physics.step(scene, 1.0f / 60.0f);
            all.insert(all.end(), physics.events().begin(), physics.events().end());
        }
        return all;
    }
};

}  // namespace

TEST_SUITE("physics2d") {

TEST_CASE("a dynamic box falls and rests on a static collider") {
    World2D w;
    const auto ground = w.box("Ground", {0, 0}, {10, 1}, BodyType2D::Static, false);  // implicit static body
    const auto crate = w.box("Crate", {0.3f, 5}, {1, 1}, BodyType2D::Dynamic);
    const auto events = w.run(3.0f);
    CHECK(w.position(crate).y == doctest::Approx(1.0f).epsilon(0.02));  // ground top 0.5 + half size 0.5
    CHECK(w.position(crate).x == doctest::Approx(0.3f).epsilon(0.01));
    CHECK(std::fabs(w.physics.velocity(crate).value().y) < 0.05f);
    CHECK(w.position(ground).y == 0.0f);
    bool touched = false;
    for (const auto& e : events) touched |= e.begin && !e.trigger && ((e.a == crate && e.b == ground) || (e.a == ground && e.b == crate));
    CHECK(touched);
    CHECK(w.physics.stats().bodies == 2);
    CHECK(w.physics.stats().dynamicBodies == 1);
    CHECK(w.physics.mass(crate).value() == doctest::Approx(1.0f));  // 1 m^2 at density 1
}

TEST_CASE("triggers report enter and exit without blocking") {
    World2D w;
    const auto zone = w.box("Zone", {0, 0}, {4, 1}, BodyType2D::Static, false);
    w.scene.registry().get<Collider2DComponent>(zone).isTrigger = true;
    const auto ball = w.scene.createEntity("Ball");
    w.scene.registry().get<TransformComponent>(ball).position = {0, 3, 0};
    w.scene.registry().emplace<RigidBody2DComponent>(ball);
    auto& c = w.scene.registry().emplace<Collider2DComponent>(ball);
    c.shape = ColliderShape2D::Circle;
    c.radius = 0.25f;
    const auto events = w.run(2.0f);
    int enters = 0, exits = 0;
    for (const auto& e : events) {
        if (!e.trigger) continue;
        CHECK(e.a == zone);
        CHECK(e.b == ball);
        (e.begin ? enters : exits)++;
    }
    CHECK(enters == 1);
    CHECK(exits == 1);
    CHECK(w.position(ball).y < -3.0f);  // fell through
}

TEST_CASE("children colliders form one compound body; edits rebuild it") {
    World2D w;
    const auto body = w.scene.createEntity("Body");
    w.scene.registry().emplace<RigidBody2DComponent>(body).gravityScale = 0.0f;
    for (float x : {-1.0f, 1.0f}) {
        const auto part = w.scene.createEntity("Part", body);
        w.scene.registry().get<TransformComponent>(part).position = {x, 0, 0};
        w.scene.registry().get<TransformComponent>(part).scale = {2.0f, 1.0f, 1.0f};  // scaled collider: 2 x 1
        w.scene.registry().emplace<Collider2DComponent>(part);
    }
    w.scene.updateTransforms();
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().bodies == 1);
    CHECK(w.physics.stats().shapes == 2);
    CHECK(w.physics.mass(body).value() == doctest::Approx(4.0f));
    CHECK(w.physics.stats().rebuilt == 1);
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().rebuilt == 0);  // unchanged: kept
    w.scene.registry().view<Collider2DComponent>().each([](ecs::Entity, Collider2DComponent& c) { c.density = 2.0f; });
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().rebuilt == 1);
    CHECK(w.physics.mass(body).value() == doctest::Approx(8.0f));
}

TEST_CASE("forces, impulses, velocities, teleports and fixed rotation") {
    World2D w;
    const auto b = w.box("B", {0, 0}, {1, 1}, BodyType2D::Dynamic);
    w.scene.registry().get<RigidBody2DComponent>(b).gravityScale = 0.0f;
    w.physics.sync(w.scene);
    REQUIRE(w.physics.hasBody(b));
    CHECK(w.physics.applyImpulse(b, {2, 0}));  // mass 1 -> 2 m/s
    w.run(1.0f);
    CHECK(w.position(b).x == doctest::Approx(2.0f).epsilon(0.02));
    CHECK(w.physics.setVelocity(b, {0, 0}));
    w.scene.registry().get<TransformComponent>(b).position = {10, 5, 0};  // teleport by gameplay
    w.run(0.1f);
    CHECK(w.position(b).x == doctest::Approx(10.0f));
    CHECK(w.position(b).y == doctest::Approx(5.0f));
    CHECK(w.physics.setAngularVelocity(b, 1.0f));
    w.run(0.5f);
    const math::Quat q = w.scene.registry().get<TransformComponent>(b).rotation;
    CHECK(std::fabs(2.0f * std::atan2(q.z, q.w)) == doctest::Approx(0.5f).epsilon(0.05));
    CHECK_FALSE(w.physics.applyForce(ecs::Entity{}, {1, 0}));
    const auto wall = w.box("Wall", {0, -5}, {1, 1}, BodyType2D::Static);
    w.physics.sync(w.scene);
    CHECK_FALSE(w.physics.setVelocity(wall, {1, 0}));  // static bodies do not move
}

TEST_CASE("kinematic bodies follow their transform and push dynamic bodies") {
    World2D w;
    const auto platform = w.box("Platform", {0, 0}, {4, 0.5f}, BodyType2D::Kinematic);
    const auto crate = w.box("Crate", {0, 0.75f}, {1, 1}, BodyType2D::Dynamic);
    w.run(0.5f);
    for (int i = 0; i < 60; ++i) {  // lift the platform by 2 m in 1 s, from gameplay
        w.scene.registry().get<TransformComponent>(platform).position.y += 2.0f / 60.0f;
        w.physics.step(w.scene, 1.0f / 60.0f);
    }
    CHECK(w.position(platform).y == doctest::Approx(2.0f).epsilon(0.01));
    CHECK(w.position(crate).y == doctest::Approx(2.75f).epsilon(0.05));  // carried on top
}

TEST_CASE("tilemap colliders merge painted runs; raycasts hit them") {
    World2D w;
    const auto map = w.scene.createEntity("Map");
    w.scene.registry().get<TransformComponent>(map).position = {-5, -1, 0};
    auto& tm = w.scene.registry().emplace<TilemapComponent>(map);
    tm.width = 10;
    tm.height = 3;
    tilemapEnsureLayout(tm);
    tilemapFillRect(tm, 0, 0, 9, 0, 1);  // floor: one run
    tilemapSet(tm, 2, 2, 1);             // two floating cells: two runs
    tilemapSet(tm, 7, 2, 1);
    w.scene.registry().emplace<TilemapCollider2DComponent>(map);
    const auto crate = w.box("Crate", {0.5f, 3}, {0.5f, 0.5f}, BodyType2D::Dynamic);
    w.run(3.0f);
    CHECK(w.physics.stats().shapes == 4);  // 3 runs + the crate
    CHECK(w.position(crate).y == doctest::Approx(0.25f).epsilon(0.02));  // floor top at y = 0
    auto hit = w.physics.raycast({-2.5f, 5}, {0, -1}, 20.0f);
    REQUIRE(hit);
    CHECK(hit->entity == map);
    CHECK(hit->point.y == doctest::Approx(2.0f));  // top of the floating cell (2, 2)
    CHECK(hit->normal.y == doctest::Approx(1.0f));
    CHECK(hit->distance == doctest::Approx(3.0f));
    CHECK_FALSE(w.physics.raycast({-2.5f, 5}, {0, 1}, 20.0f));
    // Erasing tiles rebuilds the collision.
    tilemapSet(w.scene.registry().get<TilemapComponent>(map), 2, 2, -1);
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().shapes == 3);
}

TEST_CASE("destroyed entities and reloaded scenes drop their bodies; results are deterministic") {
    World2D w;
    const auto a = w.box("A", {0, 0}, {1, 1}, BodyType2D::Dynamic);
    w.box("B", {3, 0}, {1, 1}, BodyType2D::Dynamic);
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().bodies == 2);
    w.scene.destroyEntity(a);
    w.scene.flushDestroyed();
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().bodies == 1);
    w.scene.clear();  // editor Stop: the scene is reloaded in place
    w.box("C", {0, 0}, {1, 1}, BodyType2D::Dynamic);
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().bodies == 1);

    auto simulate = [] {
        World2D s;
        s.box("Ground", {0, 0}, {20, 1}, BodyType2D::Static, false);
        std::vector<ecs::Entity> boxes;
        for (int i = 0; i < 20; ++i) boxes.push_back(s.box("Box", {static_cast<float>(i % 5) * 0.9f - 2.0f, 2.0f + static_cast<float>(i)}, {0.8f, 0.8f}, BodyType2D::Dynamic));
        s.run(2.0f);
        std::vector<float> out;
        for (auto e : boxes) out.push_back(s.position(e).x + s.position(e).y);
        return out;
    };
    CHECK(simulate() == simulate());
}

}  // TEST_SUITE
