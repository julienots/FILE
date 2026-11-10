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

}  // TEST_SUITE
