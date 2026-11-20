#include <doctest/doctest.h>

#include <algorithm>
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

TEST_CASE("collision layers: masks decide who collides; raycasts filter by layer") {
    World2D w;
    const auto ground = w.box("Ground", {0, 0}, {20, 1}, BodyType2D::Static, false);
    auto& gc = w.scene.registry().get<Collider2DComponent>(ground);
    gc.layer = 2;
    gc.collidesWith = ~(1u << 1);  // not layer 1 (ghosts)
    const auto solid = w.box("Solid", {-2, 3}, {1, 1}, BodyType2D::Dynamic);
    const auto ghost = w.box("Ghost", {2, 3}, {1, 1}, BodyType2D::Dynamic);
    w.scene.registry().get<Collider2DComponent>(ghost).layer = 1;
    w.run(2.0f);
    CHECK(w.position(solid).y == doctest::Approx(1.0f).epsilon(0.05));
    CHECK(w.position(ghost).y < -5.0f);  // fell through
    // Both sides must agree: a body whose own mask excludes the ground's layer falls too.
    const auto picky = w.box("Picky", {0, 3}, {1, 1}, BodyType2D::Dynamic);
    w.scene.registry().get<Collider2DComponent>(picky).collidesWith = ~(1u << 2);
    w.run(1.5f);
    CHECK(w.position(picky).y < -3.0f);
    // Raycasts.
    CHECK(w.physics.raycast({6, 5}, {0, -1}, 10.0f).has_value());
    CHECK(w.physics.raycast({6, 5}, {0, -1}, 10.0f, 1u << 2)->entity == ground);
    CHECK_FALSE(w.physics.raycast({6, 5}, {0, -1}, 10.0f, ~(1u << 2)).has_value());
}

TEST_CASE("joints: hinge pendulum and limits, motor, rope, spring, slider, weld between two bodies") {
    World2D w;
    // Hinge pendulum: pivot at (0, 5), bob 2 m to the right; it swings down and keeps its radius.
    const auto bob = w.box("Bob", {2, 5}, {0.4f, 0.4f}, BodyType2D::Dynamic);
    auto& hinge = w.scene.registry().emplace<Joint2DComponent>(bob);
    hinge.type = JointType2D::Hinge;
    hinge.anchor = {-2, 0};
    float lowest = 99.0f;
    for (int i = 0; i < 90; ++i) {
        w.physics.step(w.scene, 1.0f / 60.0f);
        const Vec3 p = w.position(bob);
        CHECK(std::hypot(p.x, p.y - 5.0f) == doctest::Approx(2.0f).epsilon(0.03));
        lowest = std::min(lowest, p.y);
    }
    CHECK(lowest < 3.3f);
    CHECK(w.physics.jointCount() == 1);
    CHECK(w.physics.jointValue(bob).has_value());
    // Limits: a second pendulum may only swing 0.3 rad.
    const auto limited = w.box("Limited", {12, 5}, {0.4f, 0.4f}, BodyType2D::Dynamic);
    auto& lim = w.scene.registry().emplace<Joint2DComponent>(limited);
    lim.anchor = {-2, 0};
    lim.enableLimits = true;
    lim.lowerAngle = -0.3f;
    lim.upperAngle = 0.3f;
    for (int i = 0; i < 120; ++i) {
        w.physics.step(w.scene, 1.0f / 60.0f);
        const float a = w.physics.jointValue(limited).value();
        CHECK(a > -0.42f);  // Box2D limits are slightly soft on impact
        CHECK(a < 0.42f);
    }
    // Motor: a wheel pinned to the world turns at the motor speed.
    const auto wheel = w.scene.createEntity("Wheel");
    w.scene.registry().get<TransformComponent>(wheel).position = {-10, 5, 0};
    w.scene.registry().emplace<RigidBody2DComponent>(wheel).gravityScale = 0.0f;
    auto& wc = w.scene.registry().emplace<Collider2DComponent>(wheel);
    wc.shape = ColliderShape2D::Circle;
    auto& motor = w.scene.registry().emplace<Joint2DComponent>(wheel);
    motor.enableMotor = true;
    motor.motorSpeed = 3.0f;
    motor.maxMotorForce = 500.0f;
    // Angle travelled in `seconds` (the joint angle wraps at +-pi).
    auto turned = [&](float seconds) {
        float total = 0.0f, last = w.physics.jointValue(wheel).value();
        for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) {
            w.physics.step(w.scene, 1.0f / 60.0f);
            const float now = w.physics.jointValue(wheel).value();
            float d = now - last;
            if (d > math::kPi) d -= 2.0f * math::kPi;
            if (d < -math::kPi) d += 2.0f * math::kPi;
            total += d;
            last = now;
        }
        return total;
    };
    w.physics.step(w.scene, 1.0f / 60.0f);  // creates the joint
    turned(0.5f);
    CHECK(turned(1.0f) == doctest::Approx(3.0f).epsilon(0.05));
    // The motor speed changes live (scripts drive it), without rebuilding the joint.
    motor.motorSpeed = -2.0f;
    turned(0.5f);
    CHECK(turned(1.0f) == doctest::Approx(-2.0f).epsilon(0.05));

    // Rope (distance limits 0..3): the ball hangs at most 3 m below the hook.
    const auto ball = w.box("Ball", {20, 9}, {0.3f, 0.3f}, BodyType2D::Dynamic);
    auto& rope = w.scene.registry().emplace<Joint2DComponent>(ball);
    rope.type = JointType2D::Distance;
    rope.autoConnectedAnchor = false;
    rope.connectedAnchor = {20, 10};  // world point (no connected body)
    rope.enableLimits = true;
    rope.lowerLimit = 0.0f;
    rope.upperLimit = 3.0f;
    w.run(2.0f);
    CHECK(w.position(ball).y == doctest::Approx(7.0f).epsilon(0.02));
    CHECK(w.physics.jointValue(ball).value() == doctest::Approx(3.0f).epsilon(0.02));

    // Slider on a vertical axis: falls straight down until the lower limit.
    const auto lift = w.box("Lift", {30, 5}, {1, 0.2f}, BodyType2D::Dynamic);
    w.scene.registry().get<TransformComponent>(lift).rotation = math::Quat::fromAxisAngle({0, 0, 1}, 0.0f);
    auto& slider = w.scene.registry().emplace<Joint2DComponent>(lift);
    slider.type = JointType2D::Slider;
    slider.axis = {0, 1};
    slider.enableLimits = true;
    slider.lowerLimit = -1.5f;
    slider.upperLimit = 0.0f;
    w.physics.setVelocity(lift, {5, 0});  // sideways push: the slider forbids it (body exists after a first step)
    w.run(0.05f);
    w.physics.setVelocity(lift, {5, 0});
    w.run(2.0f);
    CHECK(w.position(lift).x == doctest::Approx(30.0f).epsilon(0.01));
    CHECK(w.position(lift).y == doctest::Approx(3.5f).epsilon(0.02));
    // Slider motor lifts it back up at 1 m/s.
    slider.enableMotor = true;
    slider.motorSpeed = 1.0f;
    slider.maxMotorForce = 10000.0f;
    w.run(1.0f);
    CHECK(w.position(lift).y == doctest::Approx(4.5f).epsilon(0.03));

    // Weld between two dynamic bodies: they fall together, keeping their offset.
    const auto a = w.box("A", {40, 5}, {1, 1}, BodyType2D::Dynamic);
    const auto b = w.box("B", {41.5f, 5}, {1, 1}, BodyType2D::Dynamic);
    auto& weld = w.scene.registry().emplace<Joint2DComponent>(b);
    weld.type = JointType2D::Fixed;
    weld.connectedBody = a;
    w.run(0.5f);
    CHECK(w.position(a).y < 4.0f);
    CHECK(w.position(b).x - w.position(a).x == doctest::Approx(1.5f).epsilon(0.02));
    CHECK(w.position(b).y == doctest::Approx(w.position(a).y).epsilon(0.02));
    // Spring: a mass on a 2 Hz spring of rest length 2 oscillates, then settles below the rest point.
    const auto mass = w.box("Mass", {50, 8}, {0.5f, 0.5f}, BodyType2D::Dynamic);
    auto& spring = w.scene.registry().emplace<Joint2DComponent>(mass);
    spring.type = JointType2D::Distance;
    spring.autoConnectedAnchor = false;
    spring.connectedAnchor = {50, 10};
    spring.springFrequency = 2.0f;
    spring.springDamping = 0.5f;
    w.run(4.0f);
    const float len = w.physics.jointValue(mass).value();
    CHECK(len > 2.0f);   // stretched by gravity
    CHECK(len < 2.2f);   // 2 Hz: g / (2 pi f)^2 ~ 6 cm
}

TEST_CASE("a joint pulled harder than breakForce breaks once; clearing `broken` rebuilds it; removing the component frees it") {
    World2D w;
    const auto weight = w.box("Weight", {0, 7}, {1, 1}, BodyType2D::Dynamic);  // 1 kg/m^2 x 1 m^2
    auto& rope = w.scene.registry().emplace<Joint2DComponent>(weight);
    rope.type = JointType2D::Distance;
    rope.autoConnectedAnchor = false;
    rope.connectedAnchor = {0, 10};
    rope.breakForce = 50.0f;  // holds the weight (~9.8 N)
    w.run(1.0f);
    CHECK(w.position(weight).y == doctest::Approx(7.0f).epsilon(0.01));
    CHECK_FALSE(rope.broken);
    // Heavier: 100 kg pulls ~980 N -> breaks.
    w.scene.registry().get<Collider2DComponent>(weight).density = 100.0f;
    int breaks = 0;
    for (int i = 0; i < 60; ++i) {
        w.physics.step(w.scene, 1.0f / 60.0f);
        for (const auto& e : w.physics.jointBreaks()) {
            ++breaks;
            CHECK(e.entity == weight);
            CHECK(e.force > 50.0f);
        }
    }
    CHECK(breaks == 1);
    CHECK(rope.broken);
    CHECK(w.physics.jointCount() == 0);
    CHECK(w.position(weight).y < 5.0f);  // falling
    rope.broken = false;
    rope.breakForce = 0.0f;
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.jointCount() == 1);
    w.scene.registry().remove<Joint2DComponent>(weight);
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.jointCount() == 0);
    // A joint on an entity without a body is reported, not created.
    const auto lonely = w.scene.createEntity("Lonely");
    w.scene.registry().emplace<Joint2DComponent>(lonely);
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.jointCount() == 0);
}


TEST_CASE("polygon colliders (convex and concave) and smooth chains") {
    World2D w;
    // An L-shaped (concave) static floor: a box falling into the inner corner rests on the low step.
    const auto floor = w.scene.createEntity("Floor");
    auto& fc = w.scene.registry().emplace<Collider2DComponent>(floor);
    fc.shape = ColliderShape2D::Polygon;
    fc.points = {{-4, -1}, {4, -1}, {4, 2}, {1, 2}, {1, 0}, {-4, 0}};  // high block on the right
    const auto crateL = w.box("Left", {-2, 4}, {1, 1}, BodyType2D::Dynamic);
    const auto crateR = w.box("Right", {2.5f, 5}, {1, 1}, BodyType2D::Dynamic);
    // A dynamic triangle (convex polygon) has the area of a triangle.
    const auto tri = w.scene.createEntity("Tri");
    w.scene.registry().get<TransformComponent>(tri).position = {-3.0f, 6.0f, 0.0f};
    w.scene.registry().emplace<RigidBody2DComponent>(tri);
    auto& tc = w.scene.registry().emplace<Collider2DComponent>(tri);
    tc.shape = ColliderShape2D::Polygon;
    tc.points = {{0, 0}, {2, 0}, {0, 1}};
    w.run(3.0f);
    CHECK(w.position(crateL).y == doctest::Approx(0.5f).epsilon(0.03));
    CHECK(w.position(crateR).y == doctest::Approx(2.5f).epsilon(0.03));
    CHECK(w.physics.mass(tri).value() == doctest::Approx(1.0f).epsilon(0.01));
    // A chain (hilly ground): a ball rolls down into the valley and stays above the outline.
    World2D c;
    const auto ground = c.scene.createEntity("Ground");
    auto& gc = c.scene.registry().emplace<Collider2DComponent>(ground);
    gc.shape = ColliderShape2D::Chain;
    gc.points = {{-10, 4}, {-5, 1}, {-2, 0}, {0, -0.2f}, {2, 0}, {5, 1}, {10, 4}};
    const auto ball = c.scene.createEntity("Ball");
    c.scene.registry().get<TransformComponent>(ball).position = {-7.0f, 5.0f, 0.0f};
    c.scene.registry().emplace<RigidBody2DComponent>(ball).angularDamping = 1.0f;
    auto& bc = c.scene.registry().emplace<Collider2DComponent>(ball);
    bc.shape = ColliderShape2D::Circle;
    bc.radius = 0.4f;
    for (int i = 0; i < 20 * 60; ++i) {
        c.physics.step(c.scene, 1.0f / 60.0f);
        CHECK(c.position(ball).y > -0.25f);  // never falls through
    }
    CHECK(std::fabs(c.position(ball).x) < 4.0f);  // ended in the valley
    // Edited points rebuild the body; the chain also works with only 3 points (segments).
    c.scene.registry().get<Collider2DComponent>(ground).points = {{-10, 0}, {0, 0}, {10, 0}};
    const auto rebuilt = c.physics.stats().rebuilt;
    c.physics.step(c.scene, 1.0f / 60.0f);
    CHECK(c.physics.stats().rebuilt > 0);
    (void)rebuilt;
    c.run(2.0f);
    CHECK(c.position(ball).y == doctest::Approx(0.4f).epsilon(0.05));
}

TEST_CASE("one-way platforms: bodies jump through from below and land on top") {
    World2D w;
    const auto plat = w.box("Platform", {0, 2}, {4, 0.3f}, BodyType2D::Static, false);
    w.scene.registry().get<Collider2DComponent>(plat).oneWay = true;
    w.box("Ground", {0, -0.5f}, {20, 1}, BodyType2D::Static, false);
    const auto ball = w.box("Ball", {0, 0.5f}, {0.5f, 0.5f}, BodyType2D::Dynamic);
    w.physics.step(w.scene, 1.0f / 60.0f);
    w.physics.setVelocity(ball, {0.0f, 10.0f});  // jumps from below through the platform
    float highest = 0.0f;
    for (int i = 0; i < 120; ++i) {
        w.physics.step(w.scene, 1.0f / 60.0f);
        highest = std::max(highest, w.position(ball).y);
    }
    CHECK(highest > 3.0f);                                                    // went through
    CHECK(w.position(ball).y == doctest::Approx(2.15f + 0.25f).epsilon(0.03));  // landed on top
    // A plain platform blocks from below.
    World2D b;
    b.box("Platform", {0, 2}, {4, 0.3f}, BodyType2D::Static, false);
    b.box("Ground", {0, -0.5f}, {20, 1}, BodyType2D::Static, false);
    const auto ball2 = b.box("Ball", {0, 0.5f}, {0.5f, 0.5f}, BodyType2D::Dynamic);
    b.physics.step(b.scene, 1.0f / 60.0f);
    b.physics.setVelocity(ball2, {0.0f, 10.0f});
    float top = 0.0f;
    for (int i = 0; i < 120; ++i) {
        b.physics.step(b.scene, 1.0f / 60.0f);
        top = std::max(top, b.position(ball2).y);
    }
    CHECK(top < 1.9f);
}

TEST_CASE("character controller 2D: floors, walls, slopes, ceilings, moving and one-way platforms") {
    World2D w;
    w.box("Ground", {0, -0.5f}, {40, 1}, BodyType2D::Static, false);
    w.box("Wall", {6, 2}, {1, 6}, BodyType2D::Static, false);
    const auto hero = w.scene.createEntity("Hero");
    w.scene.registry().get<TransformComponent>(hero).position = {0, 3, 0};
    w.scene.registry().emplace<CharacterController2DComponent>(hero);
    auto ctrl = [&]() -> CharacterController2DComponent& { return w.scene.registry().get<CharacterController2DComponent>(hero); };
    w.run(1.5f);
    CHECK(ctrl().onFloor);
    CHECK(w.position(hero).y == doctest::Approx(0.8f).epsilon(0.03));  // capsule height 1.6 on the ground
    CHECK(std::fabs(ctrl().velocity.y) < 0.01f);                         // the floor removes the fall
    // Walk right into the wall: stops against it, onWall, keeps standing.
    for (int i = 0; i < 120; ++i) {
        ctrl().velocity.x = 5.0f;
        w.physics.step(w.scene, 1.0f / 60.0f);
    }
    CHECK(w.position(hero).x == doctest::Approx(5.5f - 0.4f).epsilon(0.02));
    CHECK(ctrl().onWall);
    CHECK(ctrl().onFloor);
    CHECK(ctrl().velocity.x == doctest::Approx(0.0f).epsilon(0.01));
    // Jump: leaves the floor, comes back.
    ctrl().velocity = {-2.0f, 6.0f};
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK_FALSE(ctrl().onFloor);
    w.run(2.0f);
    CHECK(ctrl().onFloor);
    // Triggers see the character (pick-ups).
    const auto coin = w.box("Coin", {w.position(hero).x, 0.8f}, {0.5f, 0.5f}, BodyType2D::Static, false);
    w.scene.registry().get<Collider2DComponent>(coin).isTrigger = true;
    bool picked = false;
    for (const auto& e : w.run(0.2f)) picked |= e.trigger && e.begin && e.a == coin && e.b == hero;
    CHECK(picked);
    // Queries see it too.
    const auto found = w.physics.overlap(QueryShape2D::circle(0.3f), {w.position(hero).x, w.position(hero).y});
    CHECK(std::find(found.begin(), found.end(), hero) != found.end());
}

TEST_CASE("character controller 2D: slopes, ceilings, moving platform, one-way platform, pushing crates") {
    // Slope of 30 degrees: walks up it; on 60 degrees: a wall.
    World2D w;
    const auto ramp = w.scene.createEntity("Ramp");
    auto& rc = w.scene.registry().emplace<Collider2DComponent>(ramp);
    rc.shape = ColliderShape2D::Polygon;
    rc.points = {{0, 0}, {10, 0}, {10, 10 * std::tan(math::radians(30.0f))}};
    w.box("Ground", {0, -0.5f}, {60, 1}, BodyType2D::Static, false);
    const auto hero = w.scene.createEntity("Hero");
    w.scene.registry().get<TransformComponent>(hero).position = {-2, 1, 0};
    w.scene.registry().emplace<CharacterController2DComponent>(hero);
    auto ctrl = [&]() -> CharacterController2DComponent& { return w.scene.registry().get<CharacterController2DComponent>(hero); };
    for (int i = 0; i < 180; ++i) {
        ctrl().velocity.x = 3.0f;
        w.physics.step(w.scene, 1.0f / 60.0f);
    }
    CHECK(w.position(hero).x > 4.0f);
    CHECK(w.position(hero).y > 2.0f);  // climbed
    CHECK(ctrl().onFloor);
    CHECK(ctrl().floorNormal.y == doctest::Approx(std::cos(math::radians(30.0f))).epsilon(0.05));
    // Standing still on the slope: does not slide down.
    const float x0 = w.position(hero).x;
    ctrl().velocity = {0, 0};
    w.run(1.0f);
    CHECK(w.position(hero).x == doctest::Approx(x0).epsilon(0.02));
    // Steep slope (maxSlope 20 degrees): the 30 degree ramp is a wall now.
    ctrl().maxSlope = math::radians(20.0f);
    w.run(1.0f);
    CHECK_FALSE(ctrl().onFloor);

    // Ceiling, moving platform, one-way platform.
    World2D p;
    p.box("Ground", {0, -0.5f}, {60, 1}, BodyType2D::Static, false);
    p.box("Ceiling", {-6, 3}, {4, 0.5f}, BodyType2D::Static, false);
    const auto lift = p.box("Lift", {6, 1}, {3, 0.4f}, BodyType2D::Kinematic);
    const auto oneWay = p.box("OneWay", {0, 2.5f}, {3, 0.3f}, BodyType2D::Static, false);
    p.scene.registry().get<Collider2DComponent>(oneWay).oneWay = true;
    auto spawn = [&](Vec2 at) {
        const auto e = p.scene.createEntity("C");
        p.scene.registry().get<TransformComponent>(e).position = {at.x, at.y, 0};
        p.scene.registry().emplace<CharacterController2DComponent>(e);
        return e;
    };
    const auto under = spawn({-6, 1});
    const auto rider = spawn({6, 3});
    const auto jumper = spawn({0, 1});
    p.run(1.0f);
    auto cc = [&](ecs::Entity e) -> CharacterController2DComponent& { return p.scene.registry().get<CharacterController2DComponent>(e); };
    CHECK(cc(rider).onFloor);
    const float riderY = p.position(rider).y;
    cc(under).velocity.y = 8.0f;   // bumps the ceiling
    cc(jumper).velocity.y = 9.0f;  // jumps through the one-way platform
    bool hitCeiling = false;
    float jumperTop = 0.0f;
    for (int i = 0; i < 150; ++i) {
        p.physics.step(p.scene, 1.0f / 60.0f);
        hitCeiling |= cc(under).onCeiling;
        jumperTop = std::max(jumperTop, p.position(jumper).y);
        // The lift goes up 1 m/s.
        auto& t = p.scene.registry().get<TransformComponent>(lift);
        t.position.y += 1.0f / 60.0f;
    }
    CHECK(hitCeiling);
    CHECK(p.position(under).y < 3.0f);
    CHECK(jumperTop > 3.0f);
    CHECK(p.position(jumper).y == doctest::Approx(2.65f + 0.8f).epsilon(0.03));  // standing on it
    CHECK(cc(jumper).onFloor);
    CHECK(p.position(rider).y == doctest::Approx(riderY + 2.5f).epsilon(0.05));  // carried up
    // Pushing a crate (pushForce > 0) vs blocked by it (pushForce = 0).
    World2D k;
    k.box("Ground", {0, -0.5f}, {60, 1}, BodyType2D::Static, false);
    const auto crate = k.box("Crate", {2, 0.5f}, {1, 1}, BodyType2D::Dynamic);
    const auto pusher = k.scene.createEntity("P");
    k.scene.registry().get<TransformComponent>(pusher).position = {0, 0.8f, 0};
    k.scene.registry().emplace<CharacterController2DComponent>(pusher);
    for (int i = 0; i < 120; ++i) {
        k.scene.registry().get<CharacterController2DComponent>(pusher).velocity.x = 2.0f;
        k.physics.step(k.scene, 1.0f / 60.0f);
    }
    CHECK(k.position(crate).x > 3.0f);
}

TEST_CASE("overlap and shape cast queries 2D") {
    World2D w;
    const auto a = w.box("A", {0, 0}, {1, 1}, BodyType2D::Static, false);
    const auto b = w.box("B", {3, 0}, {1, 1}, BodyType2D::Static, false);
    const auto t = w.box("T", {-3, 0}, {1, 1}, BodyType2D::Static, false);
    w.scene.registry().get<Collider2DComponent>(t).isTrigger = true;
    w.scene.registry().get<Collider2DComponent>(b).layer = 2;
    w.physics.sync(w.scene);
    auto ov = w.physics.overlap(QueryShape2D::circle(0.6f), {0.9f, 0});
    REQUIRE(ov.size() == 1);
    CHECK(ov[0] == a);
    CHECK(w.physics.overlap(QueryShape2D::circle(0.3f), {1.5f, 0}).empty());
    CHECK(w.physics.overlap(QueryShape2D::box({8, 1}), {0, 0}).size() == 2);  // the trigger is left out
    CHECK(w.physics.overlap(QueryShape2D::box({8, 1}), {0, 0}, 0.0f, ~0u, true).size() == 3);
    CHECK(w.physics.overlap(QueryShape2D::box({8, 1}), {0, 0}, 0.0f, 1u << 2).size() == 1);  // layer filter
    // Cast a circle of radius 0.5 from x = -1.5 rightward... starts inside the trigger: ignored; hits A.
    const auto hit = w.physics.castShape(QueryShape2D::circle(0.5f), {0, 3}, 0.0f, {0, -1}, 10.0f);
    REQUIRE(hit);
    CHECK(hit->entity == a);
    CHECK(hit->distance == doctest::Approx(2.0f).epsilon(0.01));  // top of A at 0.5, circle bottom at 2.5
    CHECK(hit->normal.y == doctest::Approx(1.0f).epsilon(0.01));
    CHECK_FALSE(w.physics.castShape(QueryShape2D::circle(0.5f), {0, 3}, 0.0f, {0, 1}, 10.0f));
}
}  // TEST_SUITE
