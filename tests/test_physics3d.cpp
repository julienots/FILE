#include <doctest/doctest.h>

#include <cmath>
#include <cstring>

#include "agr/core/JobSystem.hpp"

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

TEST_CASE("simulation on the JobSystem matches the single-threaded one exactly (positions and events)") {
    auto build = [](World3D& w) {
        w.box("Ground", {0, -0.5f, 0}, {40, 1, 40}, BodyType3D::Static, false);
        // A sensor the pile falls through and 300 boxes tumbling onto each other.
        auto& trig = w.scene.registry().emplace<ColliderComponent>(w.scene.createEntity("Zone"));
        trig.size = {6, 1, 6};
        trig.isTrigger = true;
        for (int i = 0; i < 300; ++i)
            w.box("Box", {static_cast<float>(i % 6) * 1.05f - 3.0f + static_cast<float>(i / 36) * 0.13f, 2.0f + static_cast<float>(i / 36) * 1.1f,
                          static_cast<float>((i / 6) % 6) * 1.05f - 3.0f},
                  {1, 1, 1}, BodyType3D::Dynamic);
    };
    World3D single, multi;
    JobSystem jobs(3);
    multi.physics.setJobSystem(&jobs);
    build(single);
    build(multi);
    for (int step = 0; step < 120; ++step) {
        single.physics.step(single.scene, 1.0f / 60.0f);
        multi.physics.step(multi.scene, 1.0f / 60.0f);
        const auto& ea = single.physics.events();
        const auto& eb = multi.physics.events();
        REQUIRE(ea.size() == eb.size());
        for (usize i = 0; i < ea.size(); ++i) {
            CHECK(ea[i].a == eb[i].a);
            CHECK(ea[i].b == eb[i].b);
            CHECK(ea[i].begin == eb[i].begin);
            CHECK(ea[i].trigger == eb[i].trigger);
        }
    }
    usize differing = 0;
    single.scene.registry().view<RigidBodyComponent>().each([&](ecs::Entity e, RigidBodyComponent&) {
        const Vec3 a = single.position(e), b = multi.position(e);
        if (std::memcmp(&a, &b, sizeof a) != 0) ++differing;
    });
    CHECK(differing == 0);
    multi.physics.setJobSystem(nullptr);  // back to single-threaded before `jobs` goes away
    multi.physics.step(multi.scene, 1.0f / 60.0f);
}

TEST_CASE("mesh and convex colliders: exact triangles for levels, hulls for moving bodies, MeshRenderer model by default") {
    World3D w;
    // Geometry source: a 20 x 20 ramp rising along +x (y = 0.5 x), and a unit cube's corners.
    std::vector<std::string> asked;
    w.physics.setCollisionMeshSource([&](const std::string& model, i32 index) -> std::shared_ptr<const CollisionMesh> {
        asked.push_back(model + "#" + std::to_string(index));
        auto m = std::make_shared<CollisionMesh>();
        if (model == "assets://ramp.glb") {
            m->positions = {{-10, -5, -10}, {10, 5, -10}, {10, 5, 10}, {-10, -5, 10}};
            m->indices = {0, 2, 1, 0, 3, 2};
        } else if (model == "assets://cube.glb") {
            for (int i = 0; i < 8; ++i) m->positions.push_back({(i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f});
            m->indices = {0, 1, 2};  // hulls only need the points
        } else {
            return nullptr;
        }
        return m;
    });
    // Static level: the ramp's exact triangles. A ray finds the slope (y = 0.5 x).
    const auto ramp = w.scene.createEntity("Ramp");
    auto& rc = w.scene.registry().emplace<ColliderComponent>(ramp);
    rc.shape = ColliderShape3D::Mesh;
    rc.model = "assets://ramp.glb";
    w.run(1.0f / 60.0f);
    auto hit = w.physics.raycast({4, 20, 0}, {0, -1, 0}, 50);
    REQUIRE(hit);
    CHECK(hit->point.y == doctest::Approx(2.0f).epsilon(0.01));
    CHECK(hit->entity == ramp);
    // A ball dropped on the slope rolls downhill (toward -x).
    const auto ball = w.box("Ball", {2, 6, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    auto& bc = w.scene.registry().get<ColliderComponent>(ball);
    bc.shape = ColliderShape3D::Sphere;
    bc.radius = 0.4f;
    w.run(2.0f);
    CHECK(w.position(ball).x < 1.0f);
    CHECK(w.position(ball).y > 0.5f * w.position(ball).x);  // above the slope, not through it

    // Moving body: the convex hull of the cube falls on flat static ground and rests on its face.
    World3D v;
    v.physics.setCollisionMeshSource([&](const std::string& model, i32) -> std::shared_ptr<const CollisionMesh> {
        if (model != "assets://cube.glb") return nullptr;
        auto m = std::make_shared<CollisionMesh>();
        for (int i = 0; i < 8; ++i) m->positions.push_back({(i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f});
        m->indices = {0, 1, 2};
        return m;
    });
    v.box("Ground", {0, -0.5f, 0}, {20, 1, 20}, BodyType3D::Static, false);
    const auto crate = v.box("Crate", {0, 3, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    auto& cc = v.scene.registry().get<ColliderComponent>(crate);
    cc.shape = ColliderShape3D::Convex;
    cc.model = "assets://cube.glb";
    // A dynamic body asking for an exact Mesh gets the hull too; its model comes from its MeshRenderer.
    const auto barrel = v.box("Barrel", {3, 3, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    v.scene.registry().get<ColliderComponent>(barrel).shape = ColliderShape3D::Mesh;
    auto& mr = v.scene.registry().emplace<MeshRendererComponent>(barrel);
    mr.model = "assets://cube.glb";
    mr.meshIndex = 0;
    v.run(3.0f);
    CHECK(v.position(crate).y == doctest::Approx(0.5f).epsilon(0.05));
    CHECK(v.position(barrel).y == doctest::Approx(0.5f).epsilon(0.05));
    CHECK(v.physics.mass(crate).value() == doctest::Approx(1000.0f).epsilon(0.05));  // 1 m^3 at the default density

    // A missing mesh is skipped (warning), the body still exists and falls.
    const auto ghost = v.box("Ghost", {-3, 3, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    auto& gc = v.scene.registry().get<ColliderComponent>(ghost);
    gc.shape = ColliderShape3D::Convex;
    gc.model = "assets://missing.glb";
    v.run(0.5f);
    CHECK(v.physics.hasBody(ghost));
    CHECK(v.position(ghost).y < 3.0f);
}

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

TEST_CASE("a kinematic body moved by a script stops at its target when a frame has several fixed steps") {
    World3D w;
    const auto k = w.box("Floating", {0, 1, 0}, {1, 1, 1}, BodyType3D::Kinematic);
    w.run(0.1f);
    // A slow frame: the script moves the body once, then physics steps three times.
    for (int frame = 0; frame < 60; ++frame) {
        w.scene.registry().get<TransformComponent>(k).position.y = 1.0f + std::sin(static_cast<float>(frame) * 0.2f) * 0.25f;
        for (int stepI = 0; stepI < 3; ++stepI) w.physics.step(w.scene, 1.0f / 60.0f);
        const float expected = 1.0f + std::sin(static_cast<float>(frame) * 0.2f) * 0.25f;
        REQUIRE(w.position(k).y == doctest::Approx(expected).epsilon(0.01));
    }
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

TEST_CASE("turning bodies are not rebuilt; a rebuilt fast body keeps a valid velocity") {
    World3D w;
    const auto spinner = w.box("Spinner", {0, 0, 0}, {2, 0.5f, 2}, BodyType3D::Kinematic);
    w.scene.registry().get<TransformComponent>(spinner).scale = {1.5f, 1.0f, 1.5f};
    const auto child = w.scene.createEntity("Arm", spinner);  // compound: a collider on a child
    w.scene.registry().get<TransformComponent>(child).position = {2, 0, 0};
    w.scene.registry().emplace<ColliderComponent>(child).shape = ColliderShape3D::Sphere;
    w.physics.step(w.scene, 1.0f / 60.0f);
    u32 rebuilt = 0;
    for (int i = 0; i < 120; ++i) {
        // Fast arbitrary turns (above the engine's maximum angular velocity for new bodies).
        auto& t = w.scene.registry().get<TransformComponent>(spinner);
        t.rotation = math::normalize(math::Quat::fromAxisAngle(math::normalize(Vec3{0.3f, 1.0f, 0.01f * static_cast<float>(i)}),
                                                               math::radians(70.0f + 0.7f * static_cast<float>(i))) * t.rotation);
        w.physics.step(w.scene, 1.0f / 60.0f);
        rebuilt += w.physics.stats().rebuilt;
    }
    CHECK(rebuilt == 0);
    // A real shape change while turning rebuilds it, carrying the (clamped) velocity over.
    auto& t = w.scene.registry().get<TransformComponent>(spinner);
    t.rotation = math::normalize(math::Quat::fromAxisAngle({0, 1, 0}, math::radians(90.0f)) * t.rotation);
    w.scene.registry().get<ColliderComponent>(spinner).size.x = 3.0f;
    w.physics.step(w.scene, 1.0f / 60.0f);
    CHECK(w.physics.stats().rebuilt == 1);
    w.run(0.1f);
    CHECK(w.physics.stats().bodies == 1);
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

TEST_CASE("collision layers: masks decide who collides; raycasts filter by layer") {
    World3D w;
    const auto ground = w.box("Ground", {0, 0, 0}, {30, 1, 30}, BodyType3D::Static, false);
    auto& gc = w.scene.registry().get<ColliderComponent>(ground);
    gc.layer = 2;
    gc.collidesWith = ~(1u << 1);
    const auto solid = w.box("Solid", {-3, 3, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    const auto ghost = w.box("Ghost", {3, 3, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    w.scene.registry().get<ColliderComponent>(ghost).layer = 1;
    w.run(2.0f);
    CHECK(w.position(solid).y == doctest::Approx(1.0f).epsilon(0.03));
    CHECK(w.position(ghost).y < -5.0f);
    CHECK(w.physics.raycast({8, 5, 0}, {0, -1, 0}, 10.0f, 1u << 2)->entity == ground);
    CHECK_FALSE(w.physics.raycast({8, 5, 0}, {0, -1, 0}, 10.0f, ~(1u << 2)).has_value());
}

TEST_CASE("joints 3D: hinge door with motor and limits, rope, slider, ball chain, fixed, connected bodies don't collide") {
    World3D w;
    w.box("Ground", {0, -0.5f, 0}, {80, 1, 80}, BodyType3D::Static, false);
    // Door: 2 x 2 panel hinged on its left edge about Y, motor 1 rad/s, limited to 90 degrees.
    const auto door = w.box("Door", {1, 1.2f, 0}, {2, 2, 0.1f}, BodyType3D::Dynamic);
    auto& h = w.scene.registry().emplace<Joint3DComponent>(door);
    h.type = JointType3D::Hinge;
    h.anchor = {-1, 0, 0};
    h.axis = {0, 1, 0};
    h.enableMotor = true;
    h.motorSpeed = 1.0f;
    h.maxMotorForce = 5000.0f;
    h.enableLimits = true;
    h.lowerAngle = -1.5707963f;
    h.upperAngle = 1.5707963f;
    w.run(0.5f);
    const float a0 = w.physics.jointValue(door).value();
    w.run(0.5f);
    CHECK(w.physics.jointValue(door).value() - a0 == doctest::Approx(0.5f).epsilon(0.08));
    w.run(2.0f);
    CHECK(w.physics.jointValue(door).value() == doctest::Approx(1.5707963f).epsilon(0.05));  // stopped by the limit
    const Vec3 dp = w.position(door);
    CHECK(std::hypot(dp.x - 0.0f, dp.z) == doctest::Approx(1.0f).epsilon(0.03));  // still 1 m from the hinge
    CHECK(dp.y == doctest::Approx(1.2f).epsilon(0.03));                            // held up by the hinge
    // Rope 3 m below a world hook.
    const auto ball = w.box("Ball", {10, 9, 0}, {0.3f, 0.3f, 0.3f}, BodyType3D::Dynamic);
    auto& rope = w.scene.registry().emplace<Joint3DComponent>(ball);
    rope.type = JointType3D::Distance;
    rope.autoConnectedAnchor = false;
    rope.connectedAnchor = {10, 10, 0};
    rope.enableLimits = true;
    rope.lowerLimit = 0.0f;
    rope.upperLimit = 3.0f;
    w.run(2.0f);
    CHECK(w.position(ball).y == doctest::Approx(7.0f).epsilon(0.02));
    CHECK(w.physics.jointValue(ball).value() == doctest::Approx(3.0f).epsilon(0.02));
    // Slider along Y with limits: drops 1.5 m and stays above x/z.
    const auto lift = w.box("Lift", {20, 5, 0}, {1, 0.2f, 1}, BodyType3D::Dynamic);
    auto& sl = w.scene.registry().emplace<Joint3DComponent>(lift);
    sl.type = JointType3D::Slider;
    sl.axis = {0, 1, 0};
    sl.enableLimits = true;
    sl.lowerLimit = -1.5f;
    sl.upperLimit = 0.0f;
    w.run(0.05f);
    w.physics.setVelocity(lift, {4, 0, 4});
    w.run(2.0f);
    CHECK(w.position(lift).x == doctest::Approx(20.0f).epsilon(0.01));
    CHECK(w.position(lift).z == doctest::Approx(0.0f));
    CHECK(w.position(lift).y == doctest::Approx(3.5f).epsilon(0.02));
    sl.enableMotor = true;
    sl.motorSpeed = 1.0f;
    sl.maxMotorForce = 100000.0f;
    w.run(1.0f);
    CHECK(w.position(lift).y == doctest::Approx(4.5f).epsilon(0.04));
    // Chain of two links on ball joints, hanging from a world point; overlapping links do not
    // push each other apart (connected bodies ignore each other).
    const auto l1 = w.box("Link1", {30, 8, 0}, {0.3f, 1.2f, 0.3f}, BodyType3D::Dynamic);
    const auto l2 = w.box("Link2", {30, 7, 0}, {0.3f, 1.2f, 0.3f}, BodyType3D::Dynamic);
    auto& j1 = w.scene.registry().emplace<Joint3DComponent>(l1);
    j1.type = JointType3D::Ball;
    j1.anchor = {0, 0.5f, 0};
    auto& j2 = w.scene.registry().emplace<Joint3DComponent>(l2);
    j2.type = JointType3D::Ball;
    j2.anchor = {0, 0.5f, 0};
    j2.connectedBody = l1;
    w.run(2.0f);
    CHECK(w.position(l1).y == doctest::Approx(8.0f).epsilon(0.02));
    CHECK(w.position(l2).y == doctest::Approx(7.0f).epsilon(0.02));
    CHECK(w.physics.jointCount() == 5);
    // Fixed: a crate welded to a falling base keeps its offset.
    const auto base = w.box("Base", {40, 4, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    const auto top = w.box("Top", {40, 5.5f, 0}, {1, 1, 1}, BodyType3D::Dynamic);
    auto& fx = w.scene.registry().emplace<Joint3DComponent>(top);
    fx.type = JointType3D::Fixed;
    fx.connectedBody = base;
    w.run(0.5f);
    CHECK(w.position(base).y < 3.5f);
    CHECK(w.position(top).y - w.position(base).y == doctest::Approx(1.5f).epsilon(0.02));
}

TEST_CASE("joints 3D break above breakForce and are removed with their bodies") {
    World3D w;
    const auto weight = w.box("Weight", {0, 7, 0}, {0.5f, 0.5f, 0.5f}, BodyType3D::Dynamic);
    w.scene.registry().get<RigidBodyComponent>(weight).mass = 1.0f;
    auto& rope = w.scene.registry().emplace<Joint3DComponent>(weight);
    rope.type = JointType3D::Distance;
    rope.autoConnectedAnchor = false;
    rope.connectedAnchor = {0, 10, 0};
    rope.breakForce = 50.0f;
    w.run(1.0f);
    CHECK_FALSE(rope.broken);
    CHECK(w.position(weight).y == doctest::Approx(7.0f).epsilon(0.01));
    w.scene.registry().get<RigidBodyComponent>(weight).mass = 100.0f;
    int breaks = 0;
    for (int i = 0; i < 60; ++i) {
        w.physics.step(w.scene, 1.0f / 60.0f);
        breaks += static_cast<int>(w.physics.jointBreaks().size());
    }
    CHECK(breaks == 1);
    CHECK(rope.broken);
    CHECK(w.physics.jointCount() == 0);
    CHECK(w.position(weight).y < 5.0f);
    rope.broken = false;
    rope.breakForce = 0.0f;
    w.run(0.1f);
    CHECK(w.physics.jointCount() == 1);
    w.scene.destroyEntity(weight);
    w.run(0.1f);
    CHECK(w.physics.jointCount() == 0);
}

TEST_CASE("character controller: walks, is grounded, blocked by walls, climbs steps not ledges, jumps, pushes boxes, seen by triggers and rays") {
    World3D w;
    const auto ground = w.box("Ground", {0, -0.5f, 0}, {60, 1, 60}, BodyType3D::Static, false);
    const auto wall = w.box("Wall", {0, 1, 6}, {10, 2, 0.5f}, BodyType3D::Static, false);
    w.box("Step", {9, 0.125f, 0}, {8, 0.25f, 4}, BodyType3D::Static, false);   // 25 cm: climbable
    w.box("Ledge", {-6, 0.4f, 0}, {2, 0.8f, 4}, BodyType3D::Static, false);   // 80 cm: not
    const auto hero = w.scene.createEntity("Hero");
    w.scene.registry().get<TransformComponent>(hero).position = {0, 1, 0};
    w.scene.registry().emplace<CharacterController3DComponent>(hero);
    w.run(1.0f);
    CHECK(w.physics.hasCharacter(hero));
    CHECK(w.physics.isGrounded(hero));
    CHECK(w.position(hero).y == doctest::Approx(0.0f).epsilon(0.05));
    CHECK(w.physics.groundNormal(hero)->y == doctest::Approx(1.0f).epsilon(0.01));
    // Walk toward +z into the wall: stops in front of it, collision event with the wall.
    REQUIRE(w.physics.setVelocity(hero, {0, 0, 4}));
    bool hitWall = false;
    for (const auto& e : w.run(2.0f)) hitWall |= e.begin && e.a == hero && e.b == wall;
    CHECK(hitWall);
    CHECK(w.position(hero).z == doctest::Approx(6.0f - 0.25f - 0.35f).epsilon(0.03));
    CHECK(w.position(hero).y == doctest::Approx(0.0f).epsilon(0.05));
    // Up the 25 cm step.
    w.scene.registry().get<TransformComponent>(hero).position = {3, 0, 0};
    w.physics.setVelocity(hero, {3, 0, 0});
    w.run(1.5f);
    CHECK(w.position(hero).x > 6.0f);
    CHECK(w.position(hero).y == doctest::Approx(0.25f).epsilon(0.05));
    // Not up the 80 cm ledge.
    w.scene.registry().get<TransformComponent>(hero).position = {-3, 0, 0};
    w.physics.setVelocity(hero, {-3, 0, 0});
    w.run(1.5f);
    CHECK(w.position(hero).x > -5.0f - 0.36f);
    CHECK(w.position(hero).y == doctest::Approx(0.0f).epsilon(0.05));
    // Jump: leaves the ground, comes back down.
    w.physics.setVelocity(hero, {0, 5, 0});
    float top = 0.0f;
    bool airborne = false;
    for (int i = 0; i < 90; ++i) {
        w.physics.step(w.scene, 1.0f / 60.0f);
        top = std::max(top, w.position(hero).y);
        airborne |= !w.physics.isGrounded(hero);
    }
    CHECK(airborne);
    CHECK(top == doctest::Approx(25.0f / (2.0f * 9.81f)).epsilon(0.1));  // v^2 / 2g
    CHECK(w.physics.isGrounded(hero));
    CHECK(w.physics.velocity(hero)->y == doctest::Approx(0.0f));
    // Pushes a light dynamic box.
    w.scene.registry().get<TransformComponent>(hero).position = {0, 0, -10};
    const auto crate = w.box("Crate", {2, 0.5f, -10}, {1, 1, 1}, BodyType3D::Dynamic);
    w.scene.registry().get<RigidBodyComponent>(crate).mass = 5.0f;
    w.physics.setVelocity(hero, {2, 0, 0});
    w.run(2.0f);
    CHECK(w.position(crate).x > 3.0f);
    // A trigger sees the character; rays hit it.
    const auto zone = w.box("Zone", {0, 1, -20}, {2, 2, 2}, BodyType3D::Static, false);
    w.scene.registry().get<ColliderComponent>(zone).isTrigger = true;
    w.physics.setVelocity(hero, {0, 0, 0});
    w.scene.registry().get<TransformComponent>(hero).position = {0, 0, -20};
    bool entered = false;
    for (const auto& e : w.run(0.5f)) entered |= e.begin && e.trigger && e.a == zone && e.b == hero;
    CHECK(entered);
    const auto hit = w.physics.raycast({-5, 1, -20}, {1, 0, 0}, 20.0f, ~(1u << 0) | 1u);
    REQUIRE(hit.has_value());
    CHECK((hit->entity == hero || hit->entity == zone));
    // Layers: a character whose mask excludes the ground falls through it.
    auto& cc = w.scene.registry().get<CharacterController3DComponent>(hero);
    cc.collidesWith = 0;
    w.scene.registry().get<TransformComponent>(hero).position = {20, 0, 20};
    w.run(1.0f);
    CHECK(w.position(hero).y < -1.0f);
    (void)ground;
}

}  // TEST_SUITE
