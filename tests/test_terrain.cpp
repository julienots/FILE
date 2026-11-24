#include <doctest/doctest.h>

#include <cmath>

#include <nlohmann/json.hpp>

#include "agr/assets/Image.hpp"
#include "agr/navigation/NavigationSystem.hpp"
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/scene/SceneSerializer.hpp"
#include "agr/scene/Terrain.hpp"

using namespace agr;
using math::Vec2;
using math::Vec3;

TEST_SUITE("terrain") {

TEST_CASE("data: flat by default, bilinear heights, resampling keeps the shape") {
    TerrainComponent t;
    t.size = 10.0f;
    t.resolution = 11;  // 1 m spacing
    CHECK(terrain::heightAt(t, 0, 0) == 0.0f);  // no data: flat
    terrain::ensureData(t);
    REQUIRE(t.heights.size() == 121);
    CHECK(t.splat[0] == 0xFF);
    t.heights[5 * 11 + 5] = 2000;  // 2 m at the center vertex (0, 0)
    CHECK(terrain::heightAt(t, 0, 0) == doctest::Approx(2.0f));
    CHECK(terrain::heightAt(t, 0.5f, 0) == doctest::Approx(1.0f));
    CHECK(terrain::heightAt(t, 100, 100) == 0.0f);  // clamped at the edge
    const Vec3 n = terrain::normalAt(t, -0.9f, 0);
    CHECK(n.x < 0.0f);  // the slope faces away from the peak
    t.resolution = 21;
    terrain::ensureData(t);
    CHECK(t.heights.size() == 441);
    CHECK(terrain::heightAt(t, 0, 0) == doctest::Approx(2.0f));
    std::vector<Vec3> pos;
    std::vector<u32> idx;
    terrain::buildMesh(t, pos, idx);
    CHECK(pos.size() == 441);
    CHECK(idx.size() == 20u * 20u * 6u);
    // Triangles face up (counter-clockwise seen from above).
    const Vec3 a = pos[idx[0]], b = pos[idx[1]], c = pos[idx[2]];
    CHECK(math::cross(b - a, c - a).y > 0.0f);
}

TEST_CASE("brushes: raise / lower with falloff, flatten, smooth, paint keeps weights normalized") {
    TerrainComponent t;
    t.size = 20.0f;
    t.resolution = 21;
    terrain::BrushSettings b;
    b.radius = 4.0f;
    b.strength = 2.0f;
    REQUIRE(terrain::applyBrush(t, {0, 0}, b));
    CHECK(terrain::heightAt(t, 0, 0) == doctest::Approx(2.0f));
    CHECK(terrain::heightAt(t, 2, 0) > 0.5f);
    CHECK(terrain::heightAt(t, 2, 0) < 2.0f);
    CHECK(terrain::heightAt(t, 6, 0) == 0.0f);  // outside the radius
    b.brush = terrain::Brush::Smooth;
    b.strength = 1.0f;
    const float peak = terrain::heightAt(t, 0, 0);
    REQUIRE(terrain::applyBrush(t, {0, 0}, b));
    CHECK(terrain::heightAt(t, 0, 0) < peak);
    b.brush = terrain::Brush::Flatten;
    b.targetHeight = 1.0f;
    b.radius = 20.0f;
    for (int i = 0; i < 40; ++i) terrain::applyBrush(t, {0, 0}, b);
    CHECK(terrain::heightAt(t, 0, 0) == doctest::Approx(1.0f).epsilon(0.02));
    b.brush = terrain::Brush::Paint;
    b.layer = 2;
    b.radius = 3.0f;
    REQUIRE(terrain::applyBrush(t, {0, 0}, b));
    const u32 w = static_cast<u32>(t.splat[10 * 21 + 10]);
    CHECK(((w >> 16) & 0xFF) == 255);  // layer 2 at the center
    u32 sum = 0;
    for (u32 c = 0; c < 4; ++c) sum += (static_cast<u32>(t.splat[10 * 21 + 12]) >> (c * 8)) & 0xFF;
    CHECK(sum >= 253);
    CHECK(sum <= 257);
    CHECK_FALSE(terrain::applyBrush(t, {500, 500}, b));  // outside the terrain
}

TEST_CASE("raycast hits the surface; serialization keeps heights compactly") {
    TerrainComponent t;
    t.size = 20.0f;
    t.resolution = 21;
    terrain::BrushSettings b;
    b.radius = 5.0f;
    b.strength = 3.0f;
    terrain::applyBrush(t, {0, 0}, b);
    float d = 0.0f;
    REQUIRE(terrain::raycast(t, {{0, 10, 0}, {0, -1, 0}}, 100.0f, d));
    CHECK(d == doctest::Approx(7.0f).epsilon(0.01));
    REQUIRE(terrain::raycast(t, {{-9, 1, -9}, math::normalize(Vec3{1, -0.2f, 1})}, 100.0f, d));
    CHECK_FALSE(terrain::raycast(t, {{0, 10, 0}, {0, 1, 0}}, 100.0f, d));

    Scene s;
    const auto e = s.createEntity("Terrain");
    s.registry().emplace<TerrainComponent>(e) = t;
    const std::string text = SceneSerializer::toString(s);
    CHECK(text.size() < 20000);  // RLE: flat areas are a few numbers
    Scene back;
    REQUIRE(SceneSerializer::fromString(back, text));
    CHECK(back.registry().get<TerrainComponent>(back.findByName("Terrain")).heights == t.heights);
}

TEST_CASE("collision: a ball rolls down the hill; navigation goes around a ridge") {
    registerPhysics3DComponents();
    registerNavigationComponents();
    Scene s;
    const auto e = s.createEntity("Terrain");
    auto& t = s.registry().emplace<TerrainComponent>(e);
    t.size = 40.0f;
    t.resolution = 41;
    terrain::BrushSettings hill;
    hill.radius = 8.0f;
    hill.strength = 4.0f;
    terrain::applyBrush(t, {0, 0}, hill);
    const auto ball = s.createEntity("Ball");
    s.registry().get<TransformComponent>(ball).position = {1.5f, 8.0f, 0.0f};
    s.registry().emplace<RigidBodyComponent>(ball);
    auto& c = s.registry().emplace<ColliderComponent>(ball);
    c.shape = ColliderShape3D::Sphere;
    c.radius = 0.5f;
    s.updateTransforms();
    Physics3DWorld physics;
    for (int i = 0; i < 240; ++i) physics.step(s, 1.0f / 60.0f);
    const Vec3 p = s.registry().get<TransformComponent>(ball).position;
    CHECK(p.y > terrain::heightAt(t, p.x, p.z) - 0.05f);  // never through the ground
    CHECK(p.x > 4.0f);                                   // rolled away from the top (+X side)

    // A long ridge across Z = 0 with a gap at x > 12: the agent walks through the gap.
    TerrainComponent& t2 = s.registry().get<TerrainComponent>(e);
    t2.heights.assign(41 * 41, 0);
    for (u32 x = 0; x < 30; ++x) t2.heights[20 * 41 + x] = 6000;  // 6 m wall at z = 0, x < 9
    s.destroyEntity(ball);
    const auto agent = s.createEntity("Agent");
    s.registry().get<TransformComponent>(agent).position = {-10, 0, -8};
    s.registry().emplace<NavAgentComponent>(agent);
    s.updateTransforms();
    NavigationSystem nav;
    REQUIRE(nav.navigateTo(s, agent, {-10, 0, 8}));
    float maxX = -1e9f;
    for (const Vec3& q : nav.path(agent)) maxX = std::max(maxX, q.x);
    CHECK(maxX > 9.0f);  // around the end of the ridge
}

}

TEST_SUITE("terrain") {
TEST_CASE("holes: brush cuts cells out of the mesh, raycast and physics; fill restores; serialized") {
    registerPhysics3DComponents();
    Scene s;
    const auto e = s.createEntity("Terrain");
    auto& t = s.registry().emplace<TerrainComponent>(e);
    t.size = 20.0f;
    t.resolution = 21;
    std::vector<Vec3> pos;
    std::vector<u32> idx;
    terrain::buildMesh(t, pos, idx);
    const usize full = idx.size();
    const u64 h0 = terrain::heightsHash(t);
    terrain::BrushSettings hole;
    hole.brush = terrain::Brush::Hole;
    hole.radius = 2.0f;
    REQUIRE(terrain::applyBrush(t, {0, 0}, hole));
    CHECK_FALSE(terrain::applyBrush(t, {0, 0}, hole));  // already cut
    CHECK(std::is_sorted(t.holes.begin(), t.holes.end()));
    CHECK(terrain::isHole(t, 10, 10));
    CHECK_FALSE(terrain::isHole(t, 0, 0));
    CHECK(terrain::heightsHash(t) != h0);  // meshes are rebuilt
    terrain::buildMesh(t, pos, idx);
    CHECK(idx.size() == full - t.holes.size() * 6);
    float d = 0.0f;
    CHECK_FALSE(terrain::raycast(t, {{0.3f, 5, 0.3f}, {0, -1, 0}}, 100.0f, d));  // falls through
    CHECK(terrain::raycast(t, {{6, 5, 6}, {0, -1, 0}}, 100.0f, d));

    const auto ball = s.createEntity("Ball");
    s.registry().get<TransformComponent>(ball).position = {0.3f, 2.0f, 0.3f};
    s.registry().emplace<RigidBodyComponent>(ball);
    auto& c = s.registry().emplace<ColliderComponent>(ball);
    c.shape = ColliderShape3D::Sphere;
    c.radius = 0.3f;
    const auto ball2 = s.createEntity("Ball2");
    s.registry().get<TransformComponent>(ball2).position = {6.0f, 2.0f, 6.0f};
    s.registry().emplace<RigidBodyComponent>(ball2);
    s.registry().emplace<ColliderComponent>(ball2).shape = ColliderShape3D::Sphere;
    s.updateTransforms();
    Physics3DWorld physics;
    for (int i = 0; i < 90; ++i) physics.step(s, 1.0f / 60.0f);
    CHECK(s.registry().get<TransformComponent>(ball).position.y < -2.0f);   // through the hole
    CHECK(s.registry().get<TransformComponent>(ball2).position.y > 0.0f);   // stays on the ground

    auto json = SceneSerializer::toJson(s);
    CHECK(json.dump().find("\"holes\"") != std::string::npos);
    hole.brush = terrain::Brush::Fill;
    hole.radius = 50.0f;
    REQUIRE(terrain::applyBrush(t, {0, 0}, hole));
    CHECK(t.holes.empty());
}

TEST_CASE("heightmap: import resamples 16-bit values into the range, RAW export round trips") {
    TerrainComponent t;
    t.size = 10.0f;
    std::vector<u16> v(3 * 3, 0);
    v[4] = 65535;  // white center
    REQUIRE(terrain::importHeightmap(t, v, 3, 3, -1.0f, 4.0f));
    CHECK(t.resolution == 3);
    CHECK(terrain::sample(t, 1, 1) == doctest::Approx(4.0f));
    CHECK(terrain::sample(t, 0, 0) == doctest::Approx(-1.0f));
    CHECK_FALSE(terrain::importHeightmap(t, v, 4, 4, 0, 1));
    float lo = 0, hi = 0;
    const auto raw = terrain::exportHeightmapRaw(t, lo, hi);
    CHECK(raw.size() == 3 * 3 * 2);
    CHECK(lo == doctest::Approx(-1.0f));
    CHECK(hi == doctest::Approx(4.0f));
    std::vector<u16> back(9);
    for (usize i = 0; i < 9; ++i) back[i] = static_cast<u16>(raw[2 * i] | (raw[2 * i + 1] << 8));
    TerrainComponent t2;
    REQUIRE(terrain::importHeightmap(t2, back, 3, 3, lo, hi));
    CHECK(t2.heights == t.heights);
}
}

TEST_SUITE("terrain") {
TEST_CASE("heightmap files: RAW .r16 and an 8-bit PNG (luminance)") {
    std::vector<u8> raw = {0, 0, 0xff, 0xff, 0x00, 0x80, 0x34, 0x12};
    auto r = decodeHeightmap(raw, "map.R16");
    REQUIRE(r);
    CHECK(r.value().width == 2);
    CHECK(r.value().values[1] == 65535);
    CHECK(r.value().values[3] == 0x1234);
    raw.pop_back();
    CHECK_FALSE(decodeHeightmap(raw, "map.raw"));
    const std::vector<u8> rgba = {0, 0, 0, 255, 255, 255, 255, 255};
    auto png = encodePng(2, 1, rgba);
    REQUIRE(png);
    auto g = decodeHeightmap(png.value(), "map.png");
    REQUIRE(g);
    CHECK(g.value().values[0] == 0);
    CHECK(g.value().values[1] == 65535);
}
}
