#include <doctest/doctest.h>

#include <cmath>

#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/render/RenderComponents.hpp"

using namespace agr;
using math::Vec3;

namespace {

struct World3D {
    Scene scene;
    Physics3DWorld physics;
    World3D() {
        registerRenderComponents();
        registerPhysics3DComponents();
    }
    ecs::Entity box(const char* name, Vec3 pos, Vec3 size, BodyType3D type, bool body = true) {
        const auto e = scene.createEntity(name);
        scene.registry().get<TransformComponent>(e).position = pos;
        if (body) scene.registry().emplace<RigidBodyComponent>(e).type = type;
        scene.registry().emplace<ColliderComponent>(e).size = size;
        return e;
    }
    Vec3 position(ecs::Entity e) { return scene.registry().get<TransformComponent>(e).position; }
    std::vector<Collision3DEvent> run(float seconds) {
        std::vector<Collision3DEvent> all;
        for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) {
            physics.step(scene, 1.0f / 60.0f);
            all.insert(all.end(), physics.events().begin(), physics.events().end());
        }
        return all;
    }
};

}  // namespace

TEST_SUITE("physics3d") {

TEST_CASE("a dynamic box falls and rests on static ground; mass from density or override") {
    World3D w;
    const auto ground = w.box("Ground", {0, 0, 0}, {20, 1, 20}, BodyType3D::Static, false);
    const auto crate = w.box("Crate", {0.2f, 5, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    const auto events = w.run(3.0f);
    CHECK(w.position(crate).y == doctest::Approx(1.0f).epsilon(0.02));
    CHECK(w.position(crate).x == doctest::Approx(0.2f).epsilon(0.02));
    CHECK(w.physics.mass(crate).value() == doctest::Approx(1000.0f).epsilon(0.01));  // 1 m^3 of water density
    bool touched = false;
    for (const auto& e : events) touched |= e.begin && !e.trigger && ((e.a == crate && e.b == ground) || (e.a == ground && e.b == crate));
    CHECK(touched);
    CHECK(w.physics.stats().bodies == 2);
    w.scene.registry().get<RigidBodyComponent>(crate).mass = 5.0f;
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().rebuilt == 1);
    CHECK(w.physics.mass(crate).value() == doctest::Approx(5.0f));
}

TEST_CASE("triggers report enter / exit; compound bodies from children") {
    World3D w;
    const auto zone = w.box("Zone", {0, 0, 0}, {4, 1, 4}, BodyType3D::Static, false);
    w.scene.registry().get<ColliderComponent>(zone).isTrigger = true;
    const auto ball = w.scene.createEntity("Ball");
    w.scene.registry().get<TransformComponent>(ball).position = {0, 3, 0};
    w.scene.registry().emplace<RigidBodyComponent>(ball);
    auto& c = w.scene.registry().emplace<ColliderComponent>(ball);
    c.shape = ColliderShape3D::Sphere;
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
    CHECK(w.position(ball).y < -3.0f);

    World3D w2;
    const auto body = w2.scene.createEntity("Body");
    w2.scene.registry().emplace<RigidBodyComponent>(body).gravityScale = 0.0f;
    for (float x : {-1.0f, 1.0f}) {
        const auto part = w2.scene.createEntity("Part", body);
        w2.scene.registry().get<TransformComponent>(part).position = {x, 0, 0};
        w2.scene.registry().emplace<ColliderComponent>(part).density = 10.0f;
    }
    w2.scene.updateTransforms();
    w2.physics.step(w2.scene, 1.0f / 60.0f);
    CHECK(w2.physics.stats().bodies == 1);
    CHECK(w2.physics.mass(body).value() == doctest::Approx(20.0f).epsilon(0.01));
}

TEST_CASE("kinematic platforms, teleports, impulses, frozen rotation, raycasts") {
    World3D w;
    const auto platform = w.box("Platform", {0, 0, 0}, {4, 0.5f, 4}, BodyType3D::Kinematic);
    const auto crate = w.box("Crate", {0, 0.75f, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    w.run(0.5f);
    for (int i = 0; i < 60; ++i) {
        w.scene.registry().get<TransformComponent>(platform).position.y += 2.0f / 60.0f;
        w.physics.step(w.scene, 1.0f / 60.0f);
    }
    CHECK(w.position(platform).y == doctest::Approx(2.0f).epsilon(0.01));
    CHECK(w.position(crate).y == doctest::Approx(2.75f).epsilon(0.05));

    auto hit = w.physics.raycast({0, 10, 0}, {0, -1, 0}, 50.0f);
    REQUIRE(hit);
    CHECK(hit->entity == crate);
    CHECK(hit->normal.y == doctest::Approx(1.0f).epsilon(0.01));
    CHECK(hit->distance == doctest::Approx(10.0f - 3.25f).epsilon(0.02));
    CHECK_FALSE(w.physics.raycast({0, 10, 0}, {0, 1, 0}, 50.0f));

    World3D f;
    const auto b = f.box("B", {0, 0, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    auto& rb = f.scene.registry().get<RigidBodyComponent>(b);
    rb.gravityScale = 0.0f;
    rb.freezeRotation = true;
    rb.linearDamping = 0.0f;
    f.physics.sync(f.scene);
    CHECK(f.physics.applyImpulse(b, {2000, 0, 0}));  // 1000 kg -> 2 m/s
    CHECK(f.physics.setAngularVelocity(b, {0, 5, 0}));
    f.run(1.0f);
    CHECK(f.position(b).x == doctest::Approx(2.0f).epsilon(0.03));
    CHECK(std::fabs(f.scene.registry().get<TransformComponent>(b).rotation.y) < 1e-3f);  // frozen
    f.scene.registry().get<TransformComponent>(b).position = {-5, 1, 2};
    f.run(0.1f);
    CHECK(f.position(b).z == doctest::Approx(2.0f).epsilon(0.01));
    CHECK(f.position(b).x < -4.5f);
}

TEST_CASE("destroyed entities and reloaded scenes drop their bodies; deterministic") {
    World3D w;
    const auto a = w.box("A", {0, 0, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    w.box("B", {3, 0, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().bodies == 2);
    w.scene.destroyEntity(a);
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().bodies == 1);
    w.scene.clear();
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().bodies == 0);

    auto simulate = [] {
        World3D s;
        s.box("Ground", {0, 0, 0}, {20, 1, 20}, BodyType3D::Static, false);
        std::vector<ecs::Entity> boxes;
        for (int i = 0; i < 30; ++i)
            boxes.push_back(s.box("Box", {static_cast<float>(i % 3) * 0.6f, 1.0f + static_cast<float>(i) * 1.1f, static_cast<float>(i % 2) * 0.3f},
                                  {0.8f, 0.8f, 0.8f}, BodyType3D::Dynamic));
        s.run(2.0f);
        std::vector<float> out;
        for (auto e : boxes) out.push_back(s.position(e).x + s.position(e).y * 3.0f + s.position(e).z * 7.0f);
        return out;
    };
    CHECK(simulate() == simulate());
}

}  // TEST_SUITE
