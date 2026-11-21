#include <doctest/doctest.h>

#include <filesystem>

#include "agr/core/Uuid.hpp"
#include "agr/scene/SceneSerializer.hpp"
#include "agr/twod/Atlas.hpp"
#include "agr/twod/Particles2D.hpp"
#include "agr/twod/SpriteAnimator.hpp"
#include "agr/twod/TilemapEditing.hpp"

using namespace agr;
using math::Vec2;

namespace {
Image solid(u32 w, u32 h, u8 r, u8 g, u8 b) {
    Image img;
    img.width = w;
    img.height = h;
    img.pixels.resize(static_cast<usize>(w) * h * 4);
    for (usize i = 0; i < img.pixels.size(); i += 4) img.pixels[i] = r, img.pixels[i + 1] = g, img.pixels[i + 2] = b, img.pixels[i + 3] = 255;
    return img;
}
const char* kSheet = R"({"format":"agr.spritesheet","version":1,"texture":"assets://hero.png","textureWidth":64,"textureHeight":32,
  "pixelsPerUnit":32,"pixelArt":true,
  "frames":[{"name":"a","x":0,"y":0,"w":32,"h":32},{"name":"b","x":32,"y":0,"w":32,"h":32}],
  "clips":[{"name":"run","frames":["a","b"],"fps":10,"loop":true},{"name":"once","frames":[0,1],"fps":10,"loop":false}]})";
}  // namespace

TEST_SUITE("2d") {

TEST_CASE("sprite sheets parse, validate and round trip") {
    auto s = SpriteSheet::parse(kSheet);
    REQUIRE(s);
    const SpriteSheet& sheet = s.value();
    CHECK(sheet.frames.size() == 2);
    CHECK(sheet.findFrame("b") == 1);
    CHECK(sheet.uv(1) == math::Vec4{0.5f, 0.0f, 1.0f, 1.0f});
    CHECK(sheet.frameWorldSize(0) == Vec2{1.0f, 1.0f});
    REQUIRE(sheet.findClip("run"));
    CHECK(sheet.findClip("run")->frames == std::vector<u32>{0, 1});
    auto again = SpriteSheet::parse(sheet.toJson());
    REQUIRE(again);
    CHECK(again.value().frames.size() == 2);
    CHECK(again.value().pixelArt);
    // Invalid content is rejected with a message instead of crashing later.
    CHECK_FALSE(SpriteSheet::parse(R"({"format":"agr.spritesheet","texture":"t","textureWidth":8,"textureHeight":8,"frames":[{"name":"x","x":4,"y":0,"w":8,"h":8}]})"));
    CHECK_FALSE(SpriteSheet::parse(R"({"format":"agr.spritesheet","texture":"t","textureWidth":8,"textureHeight":8,"clips":[{"name":"c","frames":["nope"]}]})"));
    const SpriteSheet g = SpriteSheet::grid("t.png", 100, 64, 32, 32);
    CHECK(g.frames.size() == 6);  // 3 columns (100 px) x 2 rows
    CHECK(g.frames[4].x == 32);
    CHECK(g.frames[4].y == 32);
}

TEST_CASE("clip playback loops or holds") {
    SpriteClip loop{"l", {4, 5, 6}, 10.0f, true};
    CHECK(clipFrameAt(loop, 0.0f) == 4);
    CHECK(clipFrameAt(loop, 0.15f) == 5);
    CHECK(clipFrameAt(loop, 0.35f) == 4);  // wrapped
    SpriteClip once{"o", {1, 2}, 10.0f, false};
    CHECK(clipFrameAt(once, 5.0f) == 2);
}

TEST_CASE("atlas packing places every image without overlap and extrudes borders") {
    std::vector<AtlasInput> in;
    for (int i = 0; i < 20; ++i) in.push_back({"img" + std::to_string(i), solid(10 + static_cast<u32>(i) * 3, 12 + static_cast<u32>(i % 5) * 7, static_cast<u8>(i * 10), 50, 200)});
    auto r = packAtlas(in, 1024, 2);
    REQUIRE(r);
    const auto& frames = r.value().frames;
    REQUIRE(frames.size() == in.size());
    CHECK(std::has_single_bit(r.value().atlas.width));
    for (usize i = 0; i < frames.size(); ++i) {
        CHECK(frames[i].w == in[i].image.width);
        CHECK(frames[i].x + frames[i].w + 2 <= r.value().atlas.width);
        for (usize j = i + 1; j < frames.size(); ++j) {  // padded rectangles never overlap
            const bool apart = frames[i].x + frames[i].w + 2 <= frames[j].x - 2 || frames[j].x + frames[j].w + 2 <= frames[i].x - 2 ||
                               frames[i].y + frames[i].h + 2 <= frames[j].y - 2 || frames[j].y + frames[j].h + 2 <= frames[i].y - 2;
            CHECK(apart);
        }
    }
    // The padding pixel left of frame 3 repeats its first column.
    const Image& a = r.value().atlas;
    const auto px = [&](u32 x, u32 y) { return a.pixels[(static_cast<usize>(y) * a.width + x) * 4]; };
    CHECK(px(frames[3].x - 1, frames[3].y) == 30);
    CHECK(px(frames[3].x, frames[3].y) == 30);
    CHECK_FALSE(packAtlas({{"big", solid(300, 300, 1, 1, 1)}}, 256, 2));
    CHECK_FALSE(packAtlas({}, 256, 2));
}

TEST_CASE("atlas and grid sheets are written through the VFS") {
    const auto root = std::filesystem::temp_directory_path() / ("agr_atlas_" + Uuid::generate().toString());
    std::filesystem::create_directories(root);
    VirtualFileSystem vfs;
    vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
    for (const char* name : {"red", "green"}) {
        const Image img = solid(16, 8, name[0] == 'r' ? 255 : 0, name[0] == 'g' ? 255 : 0, 0);
        auto png = encodePng(img.width, img.height, img.pixels);
        REQUIRE(vfs.writeBytes(std::string("assets://") + name + ".png", png.value().data(), png.value().size()));
    }
    auto sheet = buildAtlasFiles(vfs, {"assets://red.png", "assets://green.png"}, "assets://atlas.png", "assets://atlas.agrsheet", 16.0f, true);
    REQUIRE(sheet);
    CHECK(sheet.value().findFrame("green") == 1);
    SpriteSheetLibrary lib(vfs);
    const SpriteSheet* loaded = lib.get("assets://atlas.agrsheet");
    REQUIRE(loaded);
    CHECK(loaded->frames.size() == 2);
    CHECK(loaded->pixelArt);
    auto grid = buildGridSheetFile(vfs, "assets://red.png", 8, 8, "assets://red.agrsheet");
    REQUIRE(grid);
    CHECK(grid.value().frames.size() == 2);
    CHECK(lib.get("assets://missing.agrsheet") == nullptr);
    std::filesystem::remove_all(root);
}

TEST_CASE("tilemap editing: layout, set, rectangle, flood fill, picking") {
    TilemapComponent tm;
    tm.width = 4;
    tm.height = 3;
    tm.dataWidth = 4;
    tilemapEnsureLayout(tm);
    CHECK(tm.tiles.size() == 12);
    CHECK(tilemapSet(tm, 1, 2, 7));
    CHECK_FALSE(tilemapSet(tm, 1, 2, 7));  // unchanged
    CHECK_FALSE(tilemapSet(tm, 9, 0, 1));  // outside
    CHECK(tm.tileAt(1, 2) == 7);
    CHECK(tilemapFillRect(tm, 0, 0, 1, 1, 3) == 4);
    CHECK(tilemapFloodFill(tm, 3, 0, 5) == 6);  // every empty cell connected to (3,0)
    CHECK(tm.tileAt(2, 2) == 5);
    CHECK(tm.tileAt(0, 0) == 3);
    // Widening keeps tiles at their (x, y).
    tm.width = 6;
    tilemapEnsureLayout(tm);
    CHECK(tm.tileAt(1, 2) == 7);
    CHECK(tm.tileAt(5, 0) == -1);
    const auto cell = tilemapCellAt(math::Mat4::translate({10, 0, 0}), tm, math::Ray{{11.5f, 2.2f, 5}, {0, 0, -1}});
    REQUIRE(cell);
    CHECK(cell->first == 1);
    CHECK(cell->second == 2);
}

TEST_CASE("tile data serializes compactly (run-length encoded)") {
    registerRenderComponents();
    Scene s;
    const auto e = s.createEntity("Map");
    auto& tm = s.registry().emplace<TilemapComponent>(e);
    tm.width = 100;
    tm.height = 100;
    tm.dataWidth = 100;
    tilemapEnsureLayout(tm);
    tilemapFillRect(tm, 0, 0, 99, 9, 2);
    const std::string text = SceneSerializer::toString(s, -1);
    CHECK(text.size() < 2000);  // 10 000 cells -> two runs
    Scene loaded;
    REQUIRE(SceneSerializer::fromString(loaded, text));
    const auto& lt = loaded.registry().get<TilemapComponent>(loaded.findByName("Map"));
    CHECK(lt.tiles.size() == 10000);
    CHECK(lt.tileAt(50, 5) == 2);
    CHECK(lt.tileAt(50, 50) == -1);
}

TEST_CASE("particles: emission, lifetime, limits and determinism") {
    registerRenderComponents();
    auto run = [](u32 seed) {
        Scene s;
        const auto e = s.createEntity("Fx");
        auto& em = s.registry().emplace<ParticleEmitter2DComponent>(e);
        em.rate = 100.0f;
        em.burst = 10;
        em.maxParticles = 50;
        em.lifetimeMin = em.lifetimeMax = 0.5f;
        em.seed = seed;
        s.updateTransforms();
        std::vector<u32> counts;
        for (int i = 0; i < 60; ++i) counts.push_back(simulateParticles2D(s, 1.0f / 60.0f).particles);
        const auto& st = s.registry().get<ParticleState2DComponent>(e);
        return std::make_pair(counts, st.particles.empty() ? Vec2{} : st.particles[0].position);
    };
    const auto [counts, first] = run(42);
    CHECK(counts.front() >= 10);  // burst
    for (u32 c : counts) CHECK(c <= 50);  // maxParticles respected
    CHECK(counts.back() == 50);  // saturated: 100/s * 0.5 s lifetime + burst
    const auto [counts2, first2] = run(42);
    CHECK(counts2 == counts);
    CHECK(first2 == first);  // same seed, same simulation

    Scene s;
    const auto e = s.createEntity("OneShot");
    auto& em = s.registry().emplace<ParticleEmitter2DComponent>(e);
    em.loop = false;
    em.duration = 0.1f;
    em.rate = 0.0f;
    em.burst = 5;
    em.lifetimeMin = em.lifetimeMax = 0.2f;
    s.updateTransforms();
    CHECK(simulateParticles2D(s, 0.01f).particles == 5);
    for (int i = 0; i < 30; ++i) simulateParticles2D(s, 0.02f);
    CHECK(s.registry().get<ParticleState2DComponent>(e).particles.empty());  // all expired, no new cycle
}

TEST_CASE("animators drive the sprite frame") {
    registerRenderComponents();
    registerSpriteAnimatorComponent();
    const auto root = std::filesystem::temp_directory_path() / ("agr_anim_" + Uuid::generate().toString());
    std::filesystem::create_directories(root);
    VirtualFileSystem vfs;
    vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
    REQUIRE(vfs.writeText("assets://hero.agrsheet", kSheet));
    SpriteSheetLibrary lib(vfs);
    Scene s;
    const auto e = s.createEntity("Hero");
    s.registry().emplace<SpriteRendererComponent>(e).sheet = "assets://hero.agrsheet";
    s.registry().emplace<SpriteAnimatorComponent>(e).clip = "run";
    CHECK(updateSpriteAnimators(s, lib, 0.0f) == 1);
    CHECK(s.registry().get<SpriteRendererComponent>(e).frame == 0);
    updateSpriteAnimators(s, lib, 0.12f);
    CHECK(s.registry().get<SpriteRendererComponent>(e).frame == 1);
    s.registry().get<SpriteAnimatorComponent>(e).clip = "once";  // switching clip restarts it
    updateSpriteAnimators(s, lib, 0.0f);
    CHECK(s.registry().get<SpriteAnimatorComponent>(e).time == 0.0f);
    std::filesystem::remove_all(root);
}

TEST_CASE("orthographic scale modes adapt to screen shapes") {
    const float size = 5.0f, ref = 16.0f / 9.0f;
    // Same shape as the design: every mode shows the design area exactly.
    for (ScaleMode m : {ScaleMode::FixedHeight, ScaleMode::FixedWidth, ScaleMode::FitInside, ScaleMode::Crop}) {
        const Vec2 he = orthographicHalfExtents(m, size, ref, ref);
        CHECK(he.y == doctest::Approx(5.0f));
        CHECK(he.x == doctest::Approx(5.0f * ref));
    }
    const float tall = 9.0f / 19.5f;  // phone in portrait
    CHECK(orthographicHalfExtents(ScaleMode::FixedHeight, size, ref, tall).y == doctest::Approx(5.0f));
    CHECK(orthographicHalfExtents(ScaleMode::FixedWidth, size, ref, tall).x == doctest::Approx(5.0f * ref));
    const Vec2 fit = orthographicHalfExtents(ScaleMode::FitInside, size, ref, tall);
    CHECK(fit.x >= 5.0f * ref - 1e-3f);  // whole design width visible
    CHECK(fit.y >= 5.0f);
    const Vec2 crop = orthographicHalfExtents(ScaleMode::Crop, size, ref, tall);
    CHECK(crop.y <= 5.0f + 1e-3f);        // fills the screen, cuts the sides
}


TEST_CASE("particles: ground and collider bounces, attractors, sub-emitters, bursts") {
    registerRenderComponents();
    using math::Vec3;
    Scene scene;
    auto settle = [&](float seconds, float step = 1.0f / 60.0f) {
        for (float t = 0.0f; t < seconds; t += step) {
            scene.updateTransforms();
            simulateParticles2D(scene, step);
            simulateParticles3D(scene, step);
        }
    };
    // 2D: falling particles bounce on the ground plane and never go through it.
    const auto rain = scene.createEntity("Rain");
    scene.registry().get<TransformComponent>(rain).position = {0, 2, 0};
    auto& em = scene.registry().emplace<ParticleEmitter2DComponent>(rain);
    em.rate = 0.0f;
    em.burst = 20;
    em.loop = false;
    em.speedMin = em.speedMax = 0.0f;
    em.lifetimeMin = em.lifetimeMax = 3.0f;
    em.gravity = {0.0f, -10.0f};
    em.collisions = true;
    em.bounce = 0.5f;
    em.seed = 3;
    u32 hits = 0;
    for (int i = 0; i < 90; ++i) {
        scene.updateTransforms();
        hits += simulateParticles2D(scene, 1.0f / 60.0f).collisions;
        for (const Particle2D& p : scene.registry().get<ParticleState2DComponent>(rain).particles) CHECK(p.position.y >= -1e-4f);
    }
    CHECK(hits > 0);
    // dieOnCollision: they disappear on impact.
    restartParticles2D(scene, rain);
    em.dieOnCollision = true;
    settle(1.5f);
    CHECK(scene.registry().get<ParticleState2DComponent>(rain).particles.empty());

    // 3D: a particle shot at a sphere collider is pushed out and comes back.
    const auto gun = scene.createEntity("Gun");
    auto& e3 = scene.registry().emplace<ParticleEmitter3DComponent>(gun);
    e3.shape = EmitterShape3D::Point;
    e3.coneAngle = 0.0f;
    e3.shape = EmitterShape3D::Cone;
    e3.shapeRadius = 0.0f;
    e3.rate = 0.0f;
    e3.burst = 1;
    e3.loop = false;
    e3.speedMin = e3.speedMax = 4.0f;  // straight up (+Y)
    e3.lifetimeMin = e3.lifetimeMax = 5.0f;
    e3.collisions = true;
    e3.groundPlane = false;
    e3.bounce = 1.0f;
    e3.collisionFriction = 0.0f;
    const auto ball = scene.createEntity("Ball");
    scene.registry().get<TransformComponent>(ball).position = {0, 3, 0};
    scene.registry().emplace<ParticleColliderComponent>(ball).size = {2, 2, 2};  // radius 1: surface at y = 2
    settle(1.0f);
    auto& p3 = scene.registry().get<ParticleState3DComponent>(gun).particles;
    REQUIRE(p3.size() == 1);
    CHECK(p3[0].velocity.y < -3.5f);  // bounced back down
    CHECK(p3[0].position.y < 2.0f);
    // A box collider stops 2D particles too (its XY section).
    const auto wall = scene.createEntity("Wall");
    scene.registry().get<TransformComponent>(wall).position = {3, 0, 0};
    auto& box = scene.registry().emplace<ParticleColliderComponent>(wall);
    box.shape = ParticleColliderComponent::Shape::Box;
    box.size = {1, 10, 1};
    const auto jet = scene.createEntity("Jet");
    auto& ej = scene.registry().emplace<ParticleEmitter2DComponent>(jet);
    ej.rate = 0.0f, ej.burst = 5, ej.loop = false, ej.direction = 0.0f, ej.spread = 0.0f, ej.speedMin = ej.speedMax = 6.0f;
    ej.gravity = {0, 0}, ej.lifetimeMin = ej.lifetimeMax = 4.0f, ej.collisions = true, ej.groundPlane = false, ej.seed = 9;
    settle(1.0f);
    for (const Particle2D& p : scene.registry().get<ParticleState2DComponent>(jet).particles) CHECK(p.position.x <= 2.5f + 1e-3f);

    // Attractor: pulls a resting particle toward its center.
    const auto hole = scene.createEntity("Hole");
    scene.registry().get<TransformComponent>(hole).position = {-10, 0, 0};
    scene.registry().emplace<ParticleAttractorComponent>(hole).radius = 5.0f;
    const auto dust = scene.createEntity("Dust");
    scene.registry().get<TransformComponent>(dust).position = {-8, 0, 0};
    auto& ed = scene.registry().emplace<ParticleEmitter2DComponent>(dust);
    ed.rate = 0.0f, ed.burst = 1, ed.loop = false, ed.speedMin = ed.speedMax = 0.0f, ed.gravity = {0, 0}, ed.lifetimeMin = ed.lifetimeMax = 5.0f;
    settle(0.5f);
    CHECK(scene.registry().get<ParticleState2DComponent>(dust).particles[0].position.x < -8.2f);

    // Sub-emitter: each dying particle bursts 4 sparks where it was.
    const auto sparks = scene.createEntity("Sparks");
    auto& es = scene.registry().emplace<ParticleEmitter2DComponent>(sparks);
    es.emitting = false;
    es.lifetimeMin = es.lifetimeMax = 5.0f;
    es.speedMin = es.speedMax = 0.0f;
    es.gravity = {0, 0};
    const auto rocket = scene.createEntity("Rocket");
    scene.registry().get<TransformComponent>(rocket).position = {20, 0, 0};
    auto& er = scene.registry().emplace<ParticleEmitter2DComponent>(rocket);
    er.rate = 0.0f, er.burst = 3, er.loop = false, er.lifetimeMin = er.lifetimeMax = 0.1f, er.speedMin = er.speedMax = 0.0f, er.gravity = {0, 0};
    er.subEmitter = sparks;
    er.subEmitCount = 4;
    settle(0.3f);
    const auto& sp = scene.registry().get<ParticleState2DComponent>(sparks).particles;
    REQUIRE(sp.size() == 12);
    CHECK(sp[0].position.x == doctest::Approx(20.0f));
    // Bursts on demand (impacts, scripts).
    CHECK(burstParticles2D(scene, sparks, {1, 2, 0}, 5) == 5);
    CHECK(burstParticles3D(scene, gun, {0, 0, 0}, 2) == 2);
    CHECK(burstParticles2D(scene, hole, {0, 0, 0}, 2) == 0);  // not an emitter
}

TEST_CASE("trails record points as the entity moves, then fade") {
    registerRenderComponents();
    Scene scene;
    const auto e = scene.createEntity("Comet");
    auto& t = scene.registry().emplace<TrailComponent>(e);
    t.lifetime = 0.5f;
    t.minDistance = 0.25f;
    t.maxPoints = 6;
    auto& tr = scene.registry().get<TransformComponent>(e);
    for (int i = 0; i <= 10; ++i) {  // 1 unit in 10 steps of 0.1 s
        tr.position = {0.1f * static_cast<float>(i), 0, 0};
        scene.updateTransforms();
        updateTrails(scene, 0.01f);
    }
    auto& st = scene.registry().get<TrailStateComponent>(e);
    REQUIRE(st.points.size() >= 4);
    CHECK(st.points.back().position.x == doctest::Approx(1.0f));  // the head follows the entity
    for (usize i = 1; i + 1 < st.points.size(); ++i)
        CHECK(st.points[i].position.x - st.points[i - 1].position.x >= 0.25f - 1e-4f);
    // Capped.
    for (int i = 11; i <= 40; ++i) {
        tr.position = {0.1f * static_cast<float>(i), 0, 0};
        scene.updateTransforms();
        updateTrails(scene, 0.01f);
    }
    CHECK(st.points.size() <= 6);
    // Not emitting: the points expire after the lifetime.
    t.emitting = false;
    updateTrails(scene, 0.3f);
    CHECK_FALSE(st.points.empty());
    updateTrails(scene, 0.3f);
    CHECK(st.points.empty());
}
}  // TEST_SUITE

TEST_CASE("3D particles: shapes, gravity, lifetime, local / world space, seed, presets") {
    registerRenderComponents();
    Scene s;
    const auto e = s.createEntity("Fx");
    s.registry().get<TransformComponent>(e).position = {10, 0, 0};
    auto& em = s.registry().emplace<ParticleEmitter3DComponent>(e);
    em.rate = 0.0f;
    em.burst = 200;
    em.shape = EmitterShape3D::Cone;
    em.coneAngle = math::radians(20.0f);
    em.shapeRadius = 0.0f;
    em.speedMin = em.speedMax = 2.0f;
    em.lifetimeMin = em.lifetimeMax = 1.0f;
    em.seed = 7;
    s.updateTransforms();
    simulateParticles3D(s, 0.0f);
    auto& st = s.registry().get<ParticleState3DComponent>(e);
    REQUIRE(st.particles.size() == 200);
    for (const Particle3D& p : st.particles) {
        const math::Vec3 d = math::normalize(p.velocity);
        CHECK(d.y >= std::cos(math::radians(20.0f)) - 1e-4f);  // inside the cone around +Y
        CHECK(p.position.x == doctest::Approx(10.0f));         // world space: at the emitter
    }
    // Gravity bends, lifetime removes.
    em.gravity = {0, -10, 0};
    simulateParticles3D(s, 0.5f);
    CHECK(st.particles[0].velocity.y < 2.0f);
    simulateParticles3D(s, 0.6f);
    CHECK(st.particles.empty());

    // Same seed, same particles.
    restartParticles3D(s, e);
    simulateParticles3D(s, 0.0f);
    const auto first = st.particles[3].velocity;
    restartParticles3D(s, e);
    simulateParticles3D(s, 0.0f);
    CHECK(st.particles[3].velocity.x == doctest::Approx(first.x));

    // Box shape and local space: positions in the emitter's frame, inside the box.
    em.shape = EmitterShape3D::Box;
    em.boxSize = {2, 0, 4};
    em.worldSpace = false;
    restartParticles3D(s, e);
    simulateParticles3D(s, 0.0f);
    for (const Particle3D& p : st.particles) {
        CHECK(std::abs(p.position.x) <= 1.0f);
        CHECK(std::abs(p.position.z) <= 2.0f);
    }
    // Sphere: within the radius.
    em.shape = EmitterShape3D::Sphere;
    em.shapeRadius = 0.5f;
    restartParticles3D(s, e);
    simulateParticles3D(s, 0.0f);
    for (const Particle3D& p : st.particles) CHECK(math::length(p.position) <= 0.5f + 1e-4f);

    // Every preset is a valid looking emitter that produces particles.
    for (ParticlePreset preset : kParticlePresets) {
        applyParticlePreset(em, preset);
        em.seed = 3;
        restartParticles3D(s, e);
        usize most = 0;
        for (int i = 0; i < 30; ++i) {
            simulateParticles3D(s, 1.0f / 30.0f);
            most = std::max(most, st.particles.size());
        }
        CHECK_MESSAGE(most > 0, particlePresetName(preset));
        CHECK(std::string(particlePresetName(preset)).size() > 2);
    }
}

TEST_CASE("tile rules: autotile bitmask from same-group neighbours, animated tiles") {
    TilemapComponent tm;
    tm.width = 4;
    tm.height = 3;
    tilemapFillRect(tm, 0, 0, 3, 2, -1);
    // Group at 16 (frames 16..31); an L of three cells: (1,1) (2,1) (1,2). Cell (2,2) is another frame.
    tilemapSet(tm, 1, 1, 16);
    tilemapSet(tm, 2, 1, 16);
    tilemapSet(tm, 1, 2, 20);  // any frame of the group joins it
    tilemapSet(tm, 2, 2, 3);
    tm.autotile = "16";
    TileRules r = TileRules::parse(tm, 64);
    CHECK(r.resolve(tm, 1, 1, 0.0) == 16 + 1 + 2);  // +y and +x neighbours
    CHECK(r.resolve(tm, 2, 1, 0.0) == 16 + 8);      // -x only ((2,2) is not in the group)
    CHECK(r.resolve(tm, 1, 2, 0.0) == 16 + 4);      // -y only
    CHECK(r.resolve(tm, 2, 2, 0.0) == 3);
    CHECK(r.resolve(tm, 0, 0, 0.0) == -1);
    // Animation: frame 3 cycles 3, 4, 5 at 2 fps; resolved autotile frames can animate too.
    tm.animatedTiles = "3,4,5@2; 24,40@1";
    r = TileRules::parse(tm, 64);
    CHECK(r.resolve(tm, 2, 2, 0.0) == 3);
    CHECK(r.resolve(tm, 2, 2, 0.6) == 4);
    CHECK(r.resolve(tm, 2, 2, 1.1) == 5);
    CHECK(r.resolve(tm, 2, 2, 1.6) == 3);
    CHECK(r.resolve(tm, 2, 1, 1.5) == 40);  // 16 + 8 = 24 -> second frame
    // Junk is ignored.
    tm.animatedTiles = "x;7@;99,1@5";
    tm.autotile = "abc;-4";
    r = TileRules::parse(tm, 64);
    CHECK(r.empty());
}

TEST_CASE("particles: ring shape, turbulence, inherited emitter velocity (2D and 3D)") {
    registerRenderComponents();
    Scene s;
    const auto e = s.createEntity("Fx");
    auto& em = s.registry().emplace<ParticleEmitter3DComponent>(e);
    em.rate = 0.0f;
    em.burst = 100;
    em.shape = EmitterShape3D::Ring;
    em.shapeRadius = 2.0f;
    em.coneAngle = 0.0f;
    em.speedMin = em.speedMax = 1.0f;
    em.lifetimeMin = em.lifetimeMax = 10.0f;
    em.seed = 3;
    s.updateTransforms();
    simulateParticles3D(s, 0.0f);
    {
        auto& st = s.registry().get<ParticleState3DComponent>(e);
        REQUIRE(st.particles.size() == 100);
        for (const Particle3D& p : st.particles) {
            CHECK(std::sqrt(p.position.x * p.position.x + p.position.z * p.position.z) == doctest::Approx(2.0f).epsilon(1e-3));
            CHECK(p.position.y == doctest::Approx(0.0f));
            CHECK(p.velocity.y == doctest::Approx(1.0f));
        }
    }
    // Turbulence bends straight paths; without it they stay straight.
    em.turbulence = 20.0f;
    for (int i = 0; i < 10; ++i) simulateParticles3D(s, 0.05f);
    {
        auto& st = s.registry().get<ParticleState3DComponent>(e);
        float sideways = 0.0f;
        for (const Particle3D& p : st.particles) sideways += std::fabs(p.velocity.x) + std::fabs(p.velocity.z);
        CHECK(sideways / 100.0f > 0.5f);
    }
    // Inherited velocity: an emitter moving at +5 x / s gives 0.5 of it to its new particles.
    auto& em2 = s.registry().get<ParticleEmitter3DComponent>(e);
    em2.turbulence = 0.0f;
    em2.shape = EmitterShape3D::Point;
    em2.rate = 100.0f;
    em2.burst = 0;
    em2.inheritVelocity = 0.5f;
    restartParticles3D(s, e);
    simulateParticles3D(s, 0.0f);
    s.registry().get<TransformComponent>(e).position = {0.5f, 0, 0};
    s.updateTransforms();
    simulateParticles3D(s, 0.1f);
    {
        auto& st = s.registry().get<ParticleState3DComponent>(e);
        REQUIRE(!st.particles.empty());
        CHECK(st.particles.back().velocity.x == doctest::Approx(2.5f).epsilon(1e-3));
    }

    // 2D: the same inheritance and turbulence.
    const auto f = s.createEntity("Fx2");
    auto& e2 = s.registry().emplace<ParticleEmitter2DComponent>(f);
    e2.rate = 100.0f;
    e2.speedMin = e2.speedMax = 0.0f;
    e2.gravity = {0, 0};
    e2.lifetimeMin = e2.lifetimeMax = 10.0f;
    e2.inheritVelocity = 1.0f;
    e2.seed = 5;
    s.updateTransforms();
    simulateParticles2D(s, 0.0f);
    s.registry().get<TransformComponent>(f).position = {0, 1, 0};
    s.updateTransforms();
    simulateParticles2D(s, 0.1f);
    {
        auto& st = s.registry().get<ParticleState2DComponent>(f);
        REQUIRE(!st.particles.empty());
        CHECK(st.particles.back().velocity.y == doctest::Approx(10.0f).epsilon(1e-3));
    }
    s.registry().get<ParticleEmitter2DComponent>(f).turbulence = 10.0f;
    s.registry().get<ParticleEmitter2DComponent>(f).inheritVelocity = 0.0f;
    restartParticles2D(s, f);
    simulateParticles2D(s, 0.0f);
    for (int i = 0; i < 10; ++i) simulateParticles2D(s, 0.05f);
    {
        auto& st = s.registry().get<ParticleState2DComponent>(f);
        REQUIRE(!st.particles.empty());
        CHECK(math::length(st.particles.front().velocity) > 0.5f);
    }
}

TEST_CASE("sprite sheets: automatic slicing by transparency, merge distance, frame removal and moves") {
    Image img;
    img.width = 40;
    img.height = 30;
    img.pixels.assign(static_cast<usize>(img.width) * img.height * 4, 0);
    auto fill = [&](u32 x0, u32 y0, u32 x1, u32 y1) {
        for (u32 y = y0; y <= y1; ++y)
            for (u32 x = x0; x <= x1; ++x) img.pixels[(static_cast<usize>(y) * img.width + x) * 4 + 3] = 255;
    };
    fill(20, 2, 27, 9);    // row 1, right
    fill(2, 3, 9, 10);     // row 1, left (starts lower, same row)
    fill(11, 4, 11, 4);    // 1-pixel speck 1 px right of the left box: dropped (minSize), merged when mergeDistance >= 1
    fill(5, 18, 14, 25);   // row 2
    fill(15, 26, 16, 27);  // touches row 2 diagonally: same island
    auto frames = autoSliceFrames(img, 8, 2, 0, "hero");
    REQUIRE(frames.size() == 3);
    CHECK(frames[0].x == 2);
    CHECK(frames[0].w == 8);
    CHECK(frames[1].x == 20);
    CHECK(frames[2].x == 5);
    CHECK(frames[2].w == 12);  // diagonal neighbour joined (8-connected)
    CHECK(frames[2].h == 10);
    CHECK(frames[0].name == "hero_0");
    frames = autoSliceFrames(img, 8, 2, 1);
    REQUIRE(frames.size() == 3);
    CHECK(frames[0].w == 10);  // the speck merged in
    frames = autoSliceFrames(img, 8, 1, 0);
    CHECK(frames.size() == 4);  // speck kept as its own frame

    SpriteSheet sheet;
    for (int i = 0; i < 4; ++i) sheet.frames.push_back({"f" + std::to_string(i), 0, 0, 1, 1, {0.5f, 0.5f}});
    sheet.clips.push_back({"run", {0, 1, 2, 3}, 10.0f, true});
    sheet.removeFrame(1);
    CHECK(sheet.frames.size() == 3);
    CHECK(sheet.clips[0].frames == std::vector<u32>{0, 1, 2});
    CHECK(sheet.frames[1].name == "f2");
    sheet.moveFrame(0, 2);  // f0 to the end: f2, f3, f0
    CHECK(sheet.frames[2].name == "f0");
    CHECK(sheet.clips[0].frames == std::vector<u32>{2, 0, 1});  // still f0, f2, f3
}

TEST_CASE("tile rules: 47-tile blob autotiling with the 8 neighbours (inner corners)") {
    const auto& masks = blobTileMasks();
    CHECK(masks.size() == 47);
    CHECK(masks.front() == 0);
    CHECK(masks.back() == 255);
    for (usize i = 1; i < masks.size(); ++i) CHECK(masks[i - 1] < masks[i]);
    auto index = [&](u8 m) { return static_cast<i32>(std::find(masks.begin(), masks.end(), m) - masks.begin()); };
    TilemapComponent tm;
    tm.width = 5;
    tm.height = 5;
    tilemapFillRect(tm, 0, 0, 4, 4, -1);
    tilemapFillRect(tm, 1, 1, 3, 3, 100);
    tm.autotile = "100:47";
    TileRules r = TileRules::parse(tm, 200);
    CHECK(r.resolve(tm, 2, 2, 0.0) == 100 + 46);                // surrounded
    CHECK(r.resolve(tm, 1, 1, 0.0) == 100 + index(1 | 2 | 4));  // bottom-left corner: N, NE, E
    CHECK(r.resolve(tm, 2, 1, 0.0) == 100 + index(1 | 2 | 4 | 64 | 128));
    tilemapSet(tm, 3, 3, -1);  // top-right gone: the center gets an inner corner
    r = TileRules::parse(tm, 200);
    CHECK(r.resolve(tm, 2, 2, 0.0) == 100 + index(255 - 2));
    CHECK(r.resolve(tm, 3, 2, 0.0) == 100 + index(16 | 32 | 64));  // S, SW, W (N is empty now)
    // 16-frame groups still work alongside.
    tm.autotile = "0;100:47";
    r = TileRules::parse(tm, 200);
    CHECK(r.resolve(tm, 2, 2, 0.0) == 100 + index(255 - 2));
}
