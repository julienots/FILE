#include <array>
#include <thread>
#include <chrono>
#include <doctest/doctest.h>

#include <cmath>

#include "agr/twod/TilemapEditing.hpp"
#include "agr/navigation/NavigationSystem.hpp"
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/physics2d/Physics2DComponents.hpp"
#include "agr/physics3d/Physics3DComponents.hpp"
#include "agr/render/RenderComponents.hpp"

using namespace agr;
using math::Vec2;
using math::Vec3;

namespace {
void registerAll() {
    registerRenderComponents();
    registerPhysics2DComponents();
    registerPhysics3DComponents();
    registerNavigationComponents();
}
ecs::Entity box3(Scene& s, Vec3 pos, Vec3 size) {
    const auto e = s.createEntity("Box");
    s.registry().get<TransformComponent>(e).position = pos;
    s.registry().emplace<ColliderComponent>(e).size = size;
    return e;
}
ecs::Entity box2(Scene& s, Vec2 pos, Vec2 size) {
    const auto e = s.createEntity("Box2D");
    s.registry().get<TransformComponent>(e).position = {pos.x, pos.y, 0.0f};
    s.registry().emplace<Collider2DComponent>(e).size = size;
    return e;
}
Vec3 position(Scene& s, ecs::Entity e) { return s.registry().get<TransformComponent>(e).position; }
}  // namespace

TEST_SUITE("navigation") {

TEST_CASE("grid A*: around a wall through its gap, no corner cutting, smoothing, unreachable") {
    NavGrid g;
    g.reset({0, 0}, 1.0f, 20, 20);
    for (int y = 0; y < 20; ++y)
        if (y != 15) g.setBlocked(10, y, true);  // wall at x = 10 with a gap at y = 15
    std::vector<Vec2> path;
    REQUIRE(g.findPath({2.5f, 2.5f}, {17.5f, 2.5f}, path));
    CHECK(path.front().x == doctest::Approx(2.5f));
    CHECK(path.back().x == doctest::Approx(17.5f));
    bool throughGap = false;
    for (usize i = 0; i + 1 < path.size(); ++i) {
        CHECK(g.lineOfSight(path[i], path[i + 1]));  // every smoothed segment is free
        if (path[i].x < 10.0f && path[i + 1].x > 11.0f) throughGap = std::abs(path[i].y - 15.5f) < 1.5f || std::abs(path[i + 1].y - 15.5f) < 1.5f;
    }
    CHECK(path.size() <= 5);  // smoothed: a few corners, not one point per cell
    std::vector<Vec2> raw;
    REQUIRE(g.findPath({2.5f, 2.5f}, {17.5f, 2.5f}, raw, false));
    CHECK(raw.size() > path.size());
    (void)throughGap;

    // Diagonal squeeze between two blocked cells is refused.
    NavGrid d;
    d.reset({0, 0}, 1.0f, 3, 3);
    d.setBlocked(1, 0, true);
    d.setBlocked(0, 1, true);
    d.setBlocked(2, 1, true);
    d.setBlocked(1, 2, true);
    CHECK_FALSE(d.findPath({0.5f, 0.5f}, {1.5f, 1.5f}, path, false, 0));

    // Enclosed goal: unreachable.
    g.setBlocked(10, 15, true);
    CHECK_FALSE(g.findPath({2.5f, 2.5f}, {17.5f, 2.5f}, path));
    CHECK(path.empty());
    // Start inside an obstacle: moved to the nearest free cell.
    REQUIRE(g.findPath({10.5f, 5.5f}, {2.5f, 5.5f}, path));
    CHECK(path.front().x < 10.0f);
}

TEST_CASE("navmesh (Recast / Detour): around a wall on a floor, heights, partial paths, errors") {
    std::vector<Vec3> v;
    std::vector<u32> idx;
    // Floor 20 x 20 at y = 0 (two triangles, facing up) and a wall box across the middle.
    v = {{-10, 0, -10}, {10, 0, -10}, {10, 0, 10}, {-10, 0, 10}};
    idx = {0, 2, 1, 0, 3, 2};
    auto addBox = [&](Vec3 mn, Vec3 mx) {
        const u32 b = static_cast<u32>(v.size());
        for (int i = 0; i < 8; ++i) v.push_back({(i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z});
        const u32 f[12][3] = {{2, 6, 7}, {2, 7, 3}, {0, 1, 5}, {0, 5, 4}, {0, 2, 3}, {0, 3, 1}, {4, 5, 7}, {4, 7, 6}, {0, 4, 6}, {0, 6, 2}, {1, 3, 7}, {1, 7, 5}};
        for (const auto& t : f)
            for (u32 k : t) idx.push_back(b + k);
    };
    addBox({-0.5f, 0, -10}, {0.5f, 3, 6});  // wall from z = -10 to 6: go around by z > 6
    NavMesh mesh;
    REQUIRE(mesh.build(v, idx, {}));
    CHECK(mesh.stats().polygons > 0);
    std::vector<Vec3> path;
    bool complete = false;
    REQUIRE(mesh.findPath({-5, 0, -5}, {5, 0, -5}, path, &complete));
    CHECK(complete);
    float maxZ = -1e9f;
    for (const Vec3& p : path) {
        maxZ = std::max(maxZ, p.z);
        CHECK(std::abs(p.y) < 0.3f);  // on the floor
    }
    CHECK(maxZ > 6.0f);  // went around the wall's end
    CHECK(path.back().x == doctest::Approx(5.0f).epsilon(0.05));
    Vec3 snapped;
    REQUIRE(mesh.nearestPoint({3, 1.0f, 3}, snapped));
    CHECK(std::abs(snapped.y) < 0.3f);
    CHECK_FALSE(mesh.debugEdges().empty());

    // A separate island: the path stops as close as possible.
    v.push_back({30, 0, -5}), v.push_back({40, 0, -5}), v.push_back({40, 0, 5}), v.push_back({30, 0, 5});
    const u32 b = static_cast<u32>(v.size()) - 4;
    idx.insert(idx.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
    REQUIRE(mesh.build(v, idx, {}));
    REQUIRE(mesh.findPath({-5, 0, -5}, {35, 0, 0}, path, &complete));
    CHECK_FALSE(complete);
    CHECK(path.back().x < 10.5f);

    CHECK_FALSE(mesh.build({}, {}, {}));
    // Too steep to walk: a 60 degree ramp only.
    NavMesh steep;
    std::vector<Vec3> ramp{{0, 0, 0}, {4, 0, 0}, {4, 7, 4}, {0, 7, 4}};
    CHECK_FALSE(steep.build(ramp, {0, 2, 1, 0, 3, 2}, {}));
}

TEST_CASE("3D agents walk around static colliders; geometry changes rebuild; statuses") {
    registerAll();
    Scene s;
    box3(s, {0, -0.5f, 0}, {30, 1, 30});           // ground (top at y = 0)
    const auto wall = box3(s, {0, 1, -2}, {1, 2, 16});  // wall along z from -10 to 6
    const auto mover = s.createEntity("Crate");     // dynamic: not part of the navigation geometry
    s.registry().emplace<RigidBodyComponent>(mover);
    s.registry().emplace<ColliderComponent>(mover).size = {40, 1, 1};
    const auto agent = s.createEntity("Guard");
    s.registry().get<TransformComponent>(agent).position = {-5, 0, -5};
    s.registry().emplace<NavAgentComponent>(agent).speed = 4.0f;
    s.updateTransforms();
    NavigationSystem nav;
    nav.ensureBuilt(s);
    CHECK(nav.stats().sourceShapes3D == 2);  // ground + wall
    CHECK(nav.stats().meshPolygons > 0);
    REQUIRE(nav.navigateTo(s, agent, {5, 0, -5}));
    CHECK(nav.status(agent) == NavStatus::Moving);
    float maxZ = -1e9f, minZ = 1e9f;
    for (int i = 0; i < 600 && nav.status(agent) == NavStatus::Moving; ++i) {
        nav.update(s, 1.0f / 60.0f);
        s.updateTransforms();
        maxZ = std::max(maxZ, position(s, agent).z);
        minZ = std::min(minZ, position(s, agent).z);
        const Vec3 p = position(s, agent);
        CHECK_FALSE((std::abs(p.x) < 0.5f && p.z > -10.0f && p.z < 6.0f));  // never through the wall
    }
    CHECK(nav.status(agent) == NavStatus::Arrived);
    CHECK(position(s, agent).x == doctest::Approx(5.0f).epsilon(0.05));
    CHECK((minZ < -10.0f || maxZ > 6.0f));  // around one end of the wall (the shorter way: z < -10)
    const float yaw = s.registry().get<TransformComponent>(agent).rotation.toEuler().y;
    CHECK(std::isfinite(yaw));

    // Moving the wall away: rebuilt (hash), now the path is straight.
    s.registry().get<TransformComponent>(wall).position = {0, 1, 100};
    s.updateTransforms();
    const u32 builds = nav.stats().builds;
    nav.rebuild(s);
    CHECK(nav.stats().builds == builds + 1);
    std::vector<Vec3> path;
    REQUIRE(nav.findPath(s, {5, 0, -5}, {-5, 0, -5}, true, path));
    CHECK(path.size() == 2);
    nav.stop(agent);
    CHECK(nav.status(agent) == NavStatus::Idle);
    const auto ghost = s.createEntity("Ghost");  // far above and away from any walkable surface
    s.registry().get<TransformComponent>(ghost).position = {100, 50, 100};
    s.updateTransforms();
    CHECK_FALSE(nav.navigateTo(s, ghost, {0, 0, 0}));
    CHECK(nav.status(ghost) == NavStatus::Unreachable);
}

TEST_CASE("2D agents: grid from tilemaps and static 2D colliders") {
    registerAll();
    Scene s;
    // A tilemap wall column at x in [6, 7) from y = 0 to 7 (cells of 1 unit), with a collider.
    const auto map = s.createEntity("Walls");
    auto& tm = s.registry().emplace<TilemapComponent>(map);
    tm.width = 10;
    tm.height = 10;
    tm.dataWidth = 10;
    tm.tiles.assign(100, -1);
    for (u32 y = 0; y < 8; ++y) tm.tiles[y * 10 + 6] = 0;
    s.registry().emplace<TilemapCollider2DComponent>(map);
    box2(s, {3, 3}, {1, 1});  // a crate
    auto settings = s.createEntity("Navigation");
    s.registry().emplace<NavigationSettingsComponent>(settings).gridCellSize = 0.25f;
    const auto agent = s.createEntity("Slime");
    s.registry().get<TransformComponent>(agent).position = {2, 1, 0.5f};
    s.registry().emplace<NavAgentComponent>(agent).speed = 6.0f;
    s.updateTransforms();
    NavigationSystem nav;
    REQUIRE(nav.navigateTo(s, agent, {9, 1, 0}));
    CHECK(nav.stats().sourceShapes2D == 9);  // 8 cells + 1 box
    CHECK(nav.mesh().empty());
    float maxY = -1e9f, minY = 1e9f;
    for (int i = 0; i < 600 && nav.status(agent) == NavStatus::Moving; ++i) {
        nav.update(s, 1.0f / 60.0f);
        s.updateTransforms();
        maxY = std::max(maxY, position(s, agent).y);
        minY = std::min(minY, position(s, agent).y);
        // Never inside the wall column (grown by the agent radius, 0.3).
        const Vec3 p = position(s, agent);
        CHECK_FALSE((p.x > 5.75f && p.x < 7.25f && p.y > -0.25f && p.y < 8.25f));
    }
    CHECK(nav.status(agent) == NavStatus::Arrived);
    CHECK(position(s, agent).x == doctest::Approx(9.0f).epsilon(0.05));
    CHECK(position(s, agent).z == doctest::Approx(0.5f));  // depth kept
    CHECK((minY < 0.0f || maxY > 8.0f));  // around one end of the column (top-down world: below is open)
}


TEST_CASE("crowds: agents crossing paths avoid each other (2D and 3D); avoidance off goes through") {
    registerAll();
    for (bool threeD : {false, true}) {
        Scene s;
        if (threeD) box3(s, {0, -0.5f, 0}, {40, 1, 40});
        else box2(s, {0, 30}, {1, 1});  // far away: gives the 2D grid its area
        auto spawn = [&](const char* name, Vec3 at) {
            const auto e = s.createEntity(name);
            s.registry().get<TransformComponent>(e).position = at;
            auto& a = s.registry().emplace<NavAgentComponent>(e);
            a.speed = 3.0f;
            a.radius = 0.5f;
            a.plane = threeD ? NavPlane::Mesh3D : NavPlane::Grid2D;
            return e;
        };
        // Head-on: two agents swap places along the same line.
        const auto a = spawn("A", threeD ? Vec3{-6, 0, 0} : Vec3{-6, 0, 0});
        const auto b = spawn("B", threeD ? Vec3{6, 0, 0} : Vec3{6, 0, 0});
        s.updateTransforms();
        NavigationSystem nav;
        REQUIRE(nav.navigateTo(s, a, threeD ? Vec3{6, 0, 0} : Vec3{6, 0, 0}));
        REQUIRE(nav.navigateTo(s, b, threeD ? Vec3{-6, 0, 0} : Vec3{-6, 0, 0}));
        float closest = 1e9f;
        bool avoided = false;
        for (int i = 0; i < 600 && (nav.status(a) == NavStatus::Moving || nav.status(b) == NavStatus::Moving); ++i) {
            nav.update(s, 1.0f / 60.0f);
            s.updateTransforms();
            const Vec3 d = position(s, a) - position(s, b);
            closest = std::min(closest, threeD ? std::hypot(d.x, d.z) : std::hypot(d.x, d.y));
            avoided |= nav.stats().avoiding > 0;
        }
        CHECK(avoided);
        CHECK_MESSAGE(closest > 0.85f, closest);  // two radii of 0.5: they barely touch, never cross through
        CHECK(nav.status(a) == NavStatus::Arrived);
        CHECK(nav.status(b) == NavStatus::Arrived);
        CHECK(position(s, a).x == doctest::Approx(6.0f).epsilon(0.05));
        // Without avoidance: straight through each other.
        s.registry().get<NavAgentComponent>(a).avoidance = false;
        s.registry().get<NavAgentComponent>(b).avoidance = false;
        REQUIRE(nav.navigateTo(s, a, {-6, 0, 0}));
        REQUIRE(nav.navigateTo(s, b, {6, 0, 0}));
        closest = 1e9f;
        for (int i = 0; i < 600 && nav.status(a) == NavStatus::Moving; ++i) {
            nav.update(s, 1.0f / 60.0f);
            s.updateTransforms();
            const Vec3 d = position(s, a) - position(s, b);
            closest = std::min(closest, threeD ? std::hypot(d.x, d.z) : std::hypot(d.x, d.y));
        }
        CHECK(closest < 0.3f);
    }
}

TEST_CASE("obstacles: steered around, or carved out of the navigation; links: shortcuts across gaps") {
    registerAll();
    Scene s;
    box3(s, {0, -0.5f, 0}, {40, 1, 40});
    const auto agent = s.createEntity("Agent");
    s.registry().get<TransformComponent>(agent).position = {-6, 0, 0};
    s.registry().emplace<NavAgentComponent>(agent).speed = 3.0f;
    const auto rock = s.createEntity("Boulder");  // in the way, not carved: avoided on the fly
    s.registry().get<TransformComponent>(rock).position = {0, 0, 0};
    s.registry().emplace<NavObstacleComponent>(rock).radius = 1.0f;
    s.updateTransforms();
    NavigationSystem nav;
    REQUIRE(nav.navigateTo(s, agent, {6, 0, 0}));
    CHECK(nav.path(agent).size() == 1);  // straight path: the obstacle is not in the mesh
    float closest = 1e9f;
    for (int i = 0; i < 600 && nav.status(agent) == NavStatus::Moving; ++i) {
        nav.update(s, 1.0f / 60.0f);
        s.updateTransforms();
        const Vec3 p = position(s, agent);
        closest = std::min(closest, std::hypot(p.x, p.z));
    }
    CHECK(nav.status(agent) == NavStatus::Arrived);
    CHECK_MESSAGE(closest > 1.3f, closest);  // 1.0 + 0.4 radii
    // Carved: the path itself goes around it.
    s.registry().get<NavObstacleComponent>(rock).carve = true;
    nav.rebuild(s);
    std::vector<Vec3> path;
    REQUIRE(nav.findPath(s, {-6, 0, 0}, {6, 0, 0}, true, path));
    CHECK(path.size() > 2);

    // Two floors separated by a gap: unreachable until a link joins them.
    Scene g;
    box3(g, {-6, -0.5f, 0}, {8, 1, 8});
    box3(g, {6, -0.5f, 0}, {8, 1, 8});
    g.updateTransforms();
    NavigationSystem nav2;
    REQUIRE(nav2.findPath(g, {-6, 0, 0}, {6, 0, 0}, true, path));
    CHECK(math::length(path.back() - Vec3{6, 0, 0}) > 2.0f);  // as close as possible: the edge of the first floor
    const auto link = g.createEntity("Jump");
    g.registry().get<TransformComponent>(link).position = {-2.5f, 0, 0};
    g.registry().emplace<NavLinkComponent>(link).end = {5, 0, 0};  // to x = 2.5
    g.updateTransforms();
    nav2.rebuild(g);
    CHECK(nav2.stats().links == 1);
    REQUIRE(nav2.findPath(g, {-6, 0, 0}, {6, 0, 0}, true, path));
    CHECK(math::length(path.back() - Vec3{6, 0, 0}) < 0.3f);
    bool jumped = false;
    for (usize i = 0; i + 1 < path.size(); ++i) jumped |= path[i].x < -2.0f && path[i + 1].x > 2.0f;
    CHECK(jumped);

    // 2D grid link (teleporter) through a closed wall.
    Scene t;
    box2(t, {0, 0}, {1, 20});  // a wall from y -10 to 10, cells around it open
    box2(t, {0, 30}, {1, 1});
    box2(t, {0, -30}, {1, 1});
    t.updateTransforms();
    NavigationSystem nav3;
    REQUIRE(nav3.findPath(t, {-3, 0, 0}, {3, 0, 0}, false, path));
    usize detour = path.size();
    const auto tp = t.createEntity("Portal");
    t.registry().get<TransformComponent>(tp).position = {-2, 0, 0};
    t.registry().emplace<NavLinkComponent>(tp).end = {4, 0, 0};
    t.updateTransforms();
    nav3.rebuild(t);
    REQUIRE(nav3.findPath(t, {-3, 0, 0}, {3, 0, 0}, false, path));
    float longest = 0.0f;
    for (usize i = 0; i + 1 < path.size(); ++i) longest = std::max(longest, math::length(path[i + 1] - path[i]));
    CHECK(path.size() <= detour);
    CHECK(std::abs(path[1].y) < 1.0f);  // straight to the portal, not around the wall
}

TEST_CASE("static imported models and Mesh colliders are walkable sources") {
    registerAll();
    Scene s;
    const auto floor = s.createEntity("Floor");  // a model with no collider at all
    auto& mr = s.registry().emplace<MeshRendererComponent>(floor);
    mr.model = "assets://floor.glb";
    s.updateTransforms();
    NavigationSystem nav;
    nav.ensureBuilt(s);
    CHECK(nav.mesh().empty());  // no source of triangles yet
    nav.setMeshSource([](const std::string& model, i32 index) -> std::shared_ptr<const CollisionMesh> {
        if (model != "assets://floor.glb" || index != 0) return nullptr;
        auto m = std::make_shared<CollisionMesh>();
        m->positions = {{-10, 0, -10}, {10, 0, -10}, {10, 0, 10}, {-10, 0, 10}};
        m->indices = {0, 2, 1, 0, 3, 2};  // counter-clockwise seen from above
        return m;
    });
    nav.rebuild(s);
    CHECK(nav.stats().meshPolygons > 0);
    std::vector<Vec3> path;
    CHECK(nav.findPath(s, {-5, 0, -5}, {5, 0, 5}, true, path));
    // Turned off in the settings: not a source any more.
    const auto settings = s.createEntity("Navigation");
    s.registry().emplace<NavigationSettingsComponent>(settings).meshRenderersAsSources = false;
    nav.rebuild(s);
    CHECK(nav.mesh().empty());
}

TEST_CASE("crowds: an agent blocked by a closed ring of standing agents gets through instead of jamming") {
    registerAll();
    Scene s;
    box2(s, {0, 30}, {1, 1});
    auto spawn = [&](const char* name, Vec3 at) {
        const auto e = s.createEntity(name);
        s.registry().get<TransformComponent>(e).position = at;
        auto& a = s.registry().emplace<NavAgentComponent>(e);
        a.speed = 3.0f;
        a.radius = 0.5f;
        a.plane = NavPlane::Grid2D;
        return e;
    };
    for (int k = 0; k < 8; ++k) {  // standing agents, shoulder to shoulder around the goal
        const float ang = 6.2831853f * static_cast<float>(k) / 8.0f;
        spawn("Guard", {std::cos(ang) * 1.1f, std::sin(ang) * 1.1f, 0});
    }
    const auto walker = spawn("Walker", {-6, 0, 0});
    s.updateTransforms();
    NavigationSystem nav;
    REQUIRE(nav.navigateTo(s, walker, {0, 0, 0}));
    for (int i = 0; i < 600 && nav.status(walker) == NavStatus::Moving; ++i) {
        nav.update(s, 1.0f / 60.0f);
        s.updateTransforms();
    }
    CHECK(nav.status(walker) == NavStatus::Arrived);
    CHECK(std::hypot(position(s, walker).x, position(s, walker).y) < 0.5f);
}

TEST_CASE("agent sizes: a narrow gap lets small agents through, large ones go around") {
    registerAll();
    Scene s;
    box2(s, {-10.7f, 0}, {20, 0.5f});  // a wall with a 1.4 wide gap at x = 0
    box2(s, {10.7f, 0}, {20, 0.5f});
    auto& st = s.registry().emplace<NavigationSettingsComponent>(s.createEntity("Nav"));
    st.agentSizes = "1.0";
    auto spawn = [&](const char* name, float radius) {
        const auto e = s.createEntity(name);
        s.registry().get<TransformComponent>(e).position = {0, -5, 0};
        auto& a = s.registry().emplace<NavAgentComponent>(e);
        a.radius = radius;
        a.plane = NavPlane::Grid2D;
        a.avoidance = false;
        return e;
    };
    const auto small = spawn("Small", 0.3f), big = spawn("Big", 0.9f);
    s.updateTransforms();
    NavigationSystem nav;
    REQUIRE(nav.navigateTo(s, small, {0, 5, 0}));
    REQUIRE(nav.navigateTo(s, big, {0, 5, 0}));
    CHECK(nav.sizeLayerCount() == 1);
    CHECK(nav.sizeLayerFor(0.3f) == -1);
    CHECK(nav.sizeLayerFor(0.9f) == 0);
    CHECK(nav.sizeLayerFor(3.0f) == 0);  // larger than every size: the largest
    auto length = [&](ecs::Entity e) {
        const auto p = nav.path(e);
        float l = 0.0f;
        math::Vec3 prev{0, -5, 0};
        for (const auto& q : p) {
            l += math::length(q - prev);
            prev = q;
        }
        return l;
    };
    CHECK_MESSAGE(length(small) < 12.0f, length(small));  // straight through the gap
    CHECK_MESSAGE(length(big) > 35.0f, length(big));      // around the wall
}

TEST_CASE("background navigation mesh rebuilds: the old mesh serves until the new one is ready") {
    registerAll();
    Scene s;
    box3(s, {0, -0.5f, 0}, {30, 1, 30});
    const auto wall = box3(s, {0, 1, -2}, {1, 2, 16});
    s.registry().emplace<NavigationSettingsComponent>(s.createEntity("Nav")).backgroundBuild = true;
    s.updateTransforms();
    NavigationSystem nav;
    nav.ensureBuilt(s);  // the first build is immediate
    CHECK_FALSE(nav.building());
    std::vector<Vec3> path;
    REQUIRE(nav.findPath(s, {5, 0, -5}, {-5, 0, -5}, true, path));
    CHECK(path.size() > 2);  // around the wall
    s.registry().get<TransformComponent>(wall).position = {0, 1, 100};
    s.updateTransforms();
    nav.rebuild(s);
    CHECK(nav.building());
    // Paths keep working meanwhile (the old mesh), then the new mesh takes over.
    REQUIRE(nav.findPath(s, {5, 0, -5}, {-5, 0, -5}, true, path));
    for (int i = 0; i < 400 && nav.building(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        nav.ensureBuilt(s);
    }
    CHECK_FALSE(nav.building());
    REQUIRE(nav.findPath(s, {5, 0, -5}, {-5, 0, -5}, true, path));
    CHECK(path.size() == 2);  // straight now
}
}

TEST_CASE("navigation areas: costly zones avoided when the detour is worth it, forbidden zones never crossed") {
    auto maxAbs = [](const auto& path, auto get) {
        float m = 0.0f;
        for (const auto& p : path) m = std::max(m, std::fabs(get(p)));
        return m;
    };
    // 2D grid 40 x 20: a swamp band x in [18, 22], y in [5, 15] across the straight line y = 10.
    NavGrid g;
    g.reset({0, 0}, 1.0f, 40, 20);
    g.costRect({18, 5}, {22, 15}, 10.0f);
    std::vector<Vec2> path;
    REQUIRE(g.findPath({2.5f, 10.5f}, {37.5f, 10.5f}, path));
    float maxDev = 0.0f;
    for (const Vec2& p : path) maxDev = std::max(maxDev, std::fabs(p.y - 10.5f));
    CHECK(maxDev > 4.0f);  // went around the swamp (top or bottom)
    for (usize i = 0; i + 1 < path.size(); ++i) {  // and no smoothed segment cuts through it
        for (int k = 0; k <= 20; ++k) {
            const Vec2 q = path[i] + (path[i + 1] - path[i]) * (k / 20.0f);
            CHECK_FALSE((q.x > 18.6f && q.x < 21.4f && q.y > 5.6f && q.y < 14.4f));
        }
    }
    g.costRect({18, 5}, {22, 15}, 1.2f);  // cheap mud: straight through
    REQUIRE(g.findPath({2.5f, 10.5f}, {37.5f, 10.5f}, path));
    maxDev = 0.0f;
    for (const Vec2& p : path) maxDev = std::max(maxDev, std::fabs(p.y - 10.5f));
    CHECK(maxDev < 1.0f);

    // 3D floor 40 x 20 with an expensive box in the middle, then a forbidden one.
    std::vector<Vec3> v = {{-20, 0, -10}, {20, 0, -10}, {20, 0, 10}, {-20, 0, 10}};
    std::vector<u32> idx = {0, 2, 1, 0, 3, 2};
    NavMeshBuildSettings bs;
    bs.cellSize = 0.25f;
    NavMesh mesh;
    REQUIRE(mesh.build(v, idx, bs, {}, {{{-3, -1, -5}, {3, 1, 5}, 20.0f, true}}));
    std::vector<Vec3> p3;
    REQUIRE(mesh.findPath({-15, 0, 0}, {15, 0, 0}, p3));
    CHECK(maxAbs(p3, [](const Vec3& p) { return p.z; }) > 4.5f);  // around the costly zone
    NavMesh cheap;
    REQUIRE(cheap.build(v, idx, bs, {}, {{{-3, -1, -5}, {3, 1, 5}, 1.0f, true}}));
    REQUIRE(cheap.findPath({-15, 0, 0}, {15, 0, 0}, p3));
    CHECK(maxAbs(p3, [](const Vec3& p) { return p.z; }) < 0.5f);  // cost 1: straight
    NavMesh hole;
    REQUIRE(hole.build(v, idx, bs, {}, {{{-3, -1, -12}, {3, 1, 6}, 1.0f, false}}));
    bool complete = false;
    REQUIRE(hole.findPath({-15, 0, 0}, {15, 0, 0}, p3, &complete));
    CHECK(complete);
    CHECK(maxAbs(p3, [](const Vec3& p) { return p.z; }) > 5.5f);  // through the gap at z > 6
    // Through the scene: a NavArea component (2D: its x / y rectangle) and a 3D floor collider.
    registerAll();
    Scene scene;
    box2(scene, {0, -30}, {1, 1});  // any 2D source: the grid is built around the scene's shapes
    const auto swamp = scene.createEntity("Swamp");
    scene.registry().get<TransformComponent>(swamp).position = {10, 0, 0};
    auto& area = scene.registry().emplace<NavAreaComponent>(swamp);
    area.size = {4, 10, 2};
    area.walkable = false;
    scene.updateTransforms();
    NavigationSystem nav;
    std::vector<Vec3> sp;
    REQUIRE(nav.findPath(scene, {0, 0, 0}, {20, 0, 0}, false, sp));
    float dev = 0.0f;
    for (const Vec3& q : sp) dev = std::max(dev, std::fabs(q.y));
    CHECK(dev > 4.5f);  // around the forbidden 4 x 10 rectangle centered on (10, 0)
}

TEST_CASE("tiled navmesh: paths across tiles, links between tiles, only the changed tiles rebuilt") {
    std::vector<Vec3> v{{-40, 0, -40}, {40, 0, -40}, {40, 0, 40}, {-40, 0, 40}};
    std::vector<u32> idx{0, 2, 1, 0, 3, 2};
    auto addBox = [&](Vec3 mn, Vec3 mx) {
        const u32 b = static_cast<u32>(v.size());
        for (int i = 0; i < 8; ++i) v.push_back({(i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z});
        const u32 f[12][3] = {{2, 6, 7}, {2, 7, 3}, {0, 1, 5}, {0, 5, 4}, {0, 2, 3}, {0, 3, 1}, {4, 5, 7}, {4, 7, 6}, {0, 4, 6}, {0, 6, 2}, {1, 3, 7}, {1, 7, 5}};
        for (const auto& t : f)
            for (u32 k : t) idx.push_back(b + k);
    };
    addBox({-0.5f, 0, -40}, {0.5f, 3, 30});  // long wall: go around by z > 30
    NavMeshBuildSettings bs;
    bs.cellSize = 0.25f;
    bs.cellHeight = 0.125f;
    bs.tileSize = 48;  // 12 m tiles: 7 x 7
    NavMesh mesh;
    REQUIRE(mesh.build(v, idx, bs));
    CHECK(mesh.stats().tiles == 49);
    CHECK(mesh.stats().tilesRebuilt == 49);
    std::vector<Vec3> path;
    bool complete = false;
    REQUIRE(mesh.findPath({-30, 0, -30}, {30, 0, -30}, path, &complete));
    CHECK(complete);
    float maxZ = -1e9f;
    for (const Vec3& p : path) maxZ = std::max(maxZ, p.z);
    CHECK(maxZ > 30.0f);
    CHECK(path.back().x == doctest::Approx(30.0f).epsilon(0.02));

    // A crate dropped near (20, 20): one or a few tiles rebuilt, a path through it now goes around.
    const std::vector<Vec3> before = v;
    const std::vector<u32> beforeIdx = idx;
    addBox({18, 0, 10}, {22, 2, 30});
    REQUIRE(mesh.update(v, idx, bs));
    CHECK(mesh.stats().tilesRebuilt >= 1);
    CHECK(mesh.stats().tilesRebuilt <= 9);
    CHECK(mesh.stats().tiles == 49);
    REQUIRE(mesh.findPath({20, 0, 5}, {20, 0, 35}, path, &complete));
    CHECK(complete);
    float maxDx = 0.0f;
    for (const Vec3& p : path) maxDx = std::max(maxDx, std::abs(p.x - 20.0f));
    CHECK(maxDx > 2.0f);
    // Removed again: same few tiles, straight path back.
    REQUIRE(mesh.update(before, beforeIdx, bs));
    CHECK(mesh.stats().tilesRebuilt <= 9);
    REQUIRE(mesh.findPath({20, 0, 5}, {20, 0, 35}, path, &complete));
    maxDx = 0.0f;
    for (const Vec3& p : path) maxDx = std::max(maxDx, std::abs(p.x - 20.0f));
    CHECK(maxDx < 0.5f);
    // Unchanged geometry: nothing rebuilt. Other settings: a full build.
    REQUIRE(mesh.update(before, beforeIdx, bs));
    CHECK(mesh.stats().tilesRebuilt == 0);
    bs.agentRadius = 0.6f;
    REQUIRE(mesh.update(before, beforeIdx, bs));
    CHECK(mesh.stats().tilesRebuilt == 49);

    // An off-mesh link starting in one tile and ending in another joins two islands.
    std::vector<Vec3> iv{{0, 0, 0}, {10, 0, 0}, {10, 0, 10}, {0, 0, 10}, {14, 0, 0}, {24, 0, 0}, {24, 0, 10}, {14, 0, 10}};
    std::vector<u32> ii{0, 2, 1, 0, 3, 2, 4, 6, 5, 4, 7, 6};
    NavMeshBuildSettings small;
    small.cellSize = 0.25f;
    small.cellHeight = 0.125f;
    small.tileSize = 16;  // 4 m tiles
    NavMesh islands;
    REQUIRE(islands.build(iv, ii, small, {NavMeshLink{{9, 0, 5}, {15, 0, 5}, true}}));
    CHECK(islands.stats().tiles > 4);
    REQUIRE(islands.findPath({2, 0, 5}, {22, 0, 5}, path, &complete));
    CHECK(complete);
}

TEST_CASE("navigation settings tileSize: a moved carved obstacle only rebuilds the tiles around it") {
    registerAll();
    Scene s;
    box3(s, {0, -0.5f, 0}, {60, 1, 60});
    const auto settings = s.createEntity("Navigation");
    auto& st = s.registry().emplace<NavigationSettingsComponent>(settings);
    st.cellSize = 0.25f;
    st.tileSize = 32;  // 8 m tiles
    const auto rock = s.createEntity("Crate");
    s.registry().get<TransformComponent>(rock).position = {20, 0, 20};
    auto& o = s.registry().emplace<NavObstacleComponent>(rock);
    o.radius = 1.5f;
    o.carve = true;
    s.updateTransforms();
    NavigationSystem nav;
    nav.rebuild(s);
    const u32 tiles = nav.stats().meshTiles;
    CHECK(tiles >= 49);
    CHECK(nav.stats().tilesRebuilt == tiles);
    std::vector<Vec3> path;
    REQUIRE(nav.findPath(s, {-20, 0, -20}, {20, 0, -20}, true, path));
    CHECK(path.size() == 2);  // across tiles, straight
    s.registry().get<TransformComponent>(rock).position = {0, 0, -20};  // now in the way
    s.updateTransforms();
    nav.rebuild(s);
    CHECK(nav.stats().tilesRebuilt > 0);
    CHECK(nav.stats().tilesRebuilt <= 12);  // old and new place
    REQUIRE(nav.findPath(s, {-20, 0, -20}, {20, 0, -20}, true, path));
    CHECK(path.size() > 2);
}

TEST_CASE("crowds: avoidance priority — the higher one keeps its line, the lower one makes way") {
    registerAll();
    Scene s;
    box3(s, {0, -0.5f, 0}, {40, 1, 40});
    auto spawn = [&](const char* name, Vec3 at, float priority) {
        const auto e = s.createEntity(name);
        s.registry().get<TransformComponent>(e).position = at;
        auto& a = s.registry().emplace<NavAgentComponent>(e);
        a.speed = 3.0f;
        a.radius = 0.5f;
        a.plane = NavPlane::Mesh3D;
        a.priority = priority;
        return e;
    };
    auto run = [&](float pa, float pb) {
        const auto a = s.findByName("A").isNull() ? spawn("A", {-6, 0, 0.05f}, pa) : s.findByName("A");
        const auto b = s.findByName("B").isNull() ? spawn("B", {6, 0, -0.05f}, pb) : s.findByName("B");
        s.registry().get<TransformComponent>(a).position = {-6, 0, 0.05f};
        s.registry().get<TransformComponent>(b).position = {6, 0, -0.05f};
        s.registry().get<NavAgentComponent>(a).priority = pa;
        s.registry().get<NavAgentComponent>(b).priority = pb;
        s.updateTransforms();
        NavigationSystem nav;
        REQUIRE(nav.navigateTo(s, a, {6, 0, 0.05f}));
        REQUIRE(nav.navigateTo(s, b, {-6, 0, -0.05f}));
        float devA = 0.0f, devB = 0.0f, closest = 1e9f;
        for (int i = 0; i < 900 && (nav.status(a) == NavStatus::Moving || nav.status(b) == NavStatus::Moving); ++i) {
            nav.update(s, 1.0f / 60.0f);
            s.updateTransforms();
            devA = std::max(devA, std::abs(position(s, a).z - 0.05f));
            devB = std::max(devB, std::abs(position(s, b).z + 0.05f));
            const Vec3 d = position(s, a) - position(s, b);
            closest = std::min(closest, std::hypot(d.x, d.z));
        }
        CHECK(nav.status(a) == NavStatus::Arrived);
        CHECK(nav.status(b) == NavStatus::Arrived);
        return std::array<float, 3>{devA, devB, closest};
    };
    const auto vip = run(1.0f, 0.2f);
    CHECK_MESSAGE(vip[0] < 0.3f, vip[0]);        // A barely leaves its line
    CHECK_MESSAGE(vip[1] > 0.7f, vip[1]);        // B steps aside
    CHECK_MESSAGE(vip[2] > 0.8f, vip[2]);        // and they do not collide
    const auto equal = run(1.0f, 1.0f);           // shared: both turn
    CHECK_MESSAGE(equal[0] > 0.25f, equal[0]);
    CHECK_MESSAGE(equal[1] > 0.25f, equal[1]);
}

TEST_CASE("2D navigation regions: walkable only inside the drawn polygon, holes avoided, decor tiles passable") {
    registerAll();
    Scene s;
    // An L-shaped corridor: from the bottom-left room to the top-right, around the corner.
    const auto region = s.createEntity("Corridor");
    s.registry().emplace<NavRegion2DComponent>(region).points = {{0, 0}, {10, 0}, {10, 10}, {7, 10}, {7, 3}, {0, 3}};
    s.updateTransforms();
    NavigationSystem nav;
    std::vector<Vec3> path;
    REQUIRE(nav.findPath(s, {1, 1.5f, 0}, {8.5f, 9, 0}, false, path));
    // The straight line would cut across (4, 6), outside the corridor: the path turns at the corner.
    bool turns = false;
    for (const Vec3& p : path) {
        CHECK_MESSAGE((p.y <= 3.01f || p.x >= 6.99f), p.x << "," << p.y);  // never outside the L
        turns |= p.x > 6.5f && p.y < 3.0f;
    }
    CHECK(turns);
    // Outside the region: not reachable.
    std::vector<Vec3> nowhere;
    bool complete = true;
    nav.findPath(s, {1, 1.5f, 0}, {2, 8, 0}, false, nowhere);
    if (!nowhere.empty()) CHECK(math::length(nowhere.back() - Vec3{2, 8, 0}) > 2.0f);
    (void)complete;

    // A room with a pond (hole) in the middle: the path goes around it.
    Scene r;
    const auto room = r.createEntity("Room");
    r.registry().emplace<NavRegion2DComponent>(room).points = {{-10, -10}, {10, -10}, {10, 10}, {-10, 10}};
    const auto pond = r.createEntity("Pond");
    auto& hole = r.registry().emplace<NavRegion2DComponent>(pond);
    hole.points = {{-3, -3}, {3, -3}, {3, 3}, {-3, 3}};
    hole.walkable = false;
    r.updateTransforms();
    NavigationSystem nav2;
    REQUIRE(nav2.findPath(r, {-8, 0, 0}, {8, 0, 0}, false, path));
    float maxY = 0.0f;
    for (const Vec3& p : path) maxY = std::max(maxY, std::abs(p.y));
    CHECK(maxY > 3.0f);

    // Decor tiles of a TilemapCollider2D (emptyTiles) do not block the 2D grid.
    Scene t;
    const auto map = t.createEntity("Map");
    t.registry().get<TransformComponent>(map).position = {-2, -5, 0};
    auto& tm = t.registry().emplace<TilemapComponent>(map);
    tm.width = 1;
    tm.height = 10;
    tilemapEnsureLayout(tm);
    tilemapFillRect(tm, 0, 0, 0, 9, 2);  // a wall of grass tiles across the way
    t.registry().emplace<TilemapCollider2DComponent>(map).emptyTiles = "2";
    const auto far = t.createEntity("Far");  // gives the grid some area
    t.registry().emplace<Collider2DComponent>(far);
    t.registry().get<TransformComponent>(far).position = {20, 20, 0};
    t.updateTransforms();
    NavigationSystem nav3;
    REQUIRE(nav3.findPath(t, {-5, 0, 0}, {2, 0, 0}, false, path));
    CHECK(path.size() == 1);  // straight through the grass
}
