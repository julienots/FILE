// 2D rendering: lit sprites (Light2D), tilemaps, sprite sheet frames, particles,
// parallax and camera scale modes, on every graphics backend.
#include <filesystem>

#include "GpuFixture.hpp"
#include "agr/assets/AssetManager.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/Renderer.hpp"
#include "agr/render/Skin2D.hpp"
#include "agr/render/ParticleCurves.hpp"
#include "agr/twod/Particles2D.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/twod/TilemapEditing.hpp"

using namespace agr;
using namespace agr::test;
using math::Vec3;

namespace {

struct Render2DFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("agr_gpu2d_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    JobSystem jobs{1};
    AssetManager assets{vfs, jobs, gpu().device.get()};
    Renderer renderer{*gpu().device, assets};
    SpriteSheetLibrary sheets{vfs};
    Scene scene;

    Render2DFixture() {
        std::filesystem::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        registerRenderComponents();
        REQUIRE(renderer.initialize());
        GraphicsProfile p = graphicsProfile(GraphicsTier::Low);
        p.renderScale = 1.0f;
        renderer.setProfile(p);
        renderer.setSpriteSheets(&sheets);
        // 2x1 texture (yellow | magenta) cut into two 1x1 frames; frame 1 has its pivot bottom-left.
        Image img;
        img.width = 2;
        img.height = 1;
        img.pixels = {255, 255, 0, 255, 255, 0, 255, 255};
        auto png = encodePng(2, 1, img.pixels);
        REQUIRE(png);
        REQUIRE(vfs.writeBytes("assets://two.png", png.value().data(), png.value().size()));
        SpriteSheet sheet = SpriteSheet::grid("assets://two.png", 2, 1, 1, 1, 0, 0, "f");
        sheet.pixelArt = true;
        sheet.frames[1].pivot = {0.0f, 0.0f};
        REQUIRE(vfs.writeText("assets://two.agrsheet", sheet.toJson()));
    }
    ~Render2DFixture() { std::filesystem::remove_all(root); }

    /// Orthographic camera at (x, y, 10) showing [-5, 5] vertically (6.4 px per unit), black background.
    CameraComponent& camera(float x = 0.0f, float y = 0.0f) {
        const auto cam = scene.createEntity("Camera");
        scene.registry().get<TransformComponent>(cam).position = {x, y, 10};
        auto& c = scene.registry().emplace<CameraComponent>(cam);
        c.projection = Projection::Orthographic;
        c.orthographicSize = 5.0f;
        c.clearColor = {0, 0, 0, 1};
        return c;
    }
    ecs::Entity sprite(Vec3 pos, float size = 4.0f) {
        const auto e = scene.createEntity("Sprite");
        scene.registry().get<TransformComponent>(e).position = pos;
        scene.registry().emplace<SpriteRendererComponent>(e).size = {size, size};
        return e;
    }
    void frame() {
        scene.updateTransforms();
        assets.update();
        renderer.render(scene, kWidth, kHeight);
    }
    /// Renders until textures are uploaded.
    void settle() {
        frame();
        assets.waitAll();
        frame();
    }
};

}  // namespace

TEST_SUITE("gpu.renderer2d") {

TEST_CASE("lit sprites: ambient only, then a Light2D with distance falloff") {
    Render2DFixture f;
    f.camera();
    const auto env = f.scene.createEntity("Env");
    auto& en = f.scene.registry().emplace<EnvironmentComponent>(env);
    en.ambientColor = {1, 1, 1, 1};
    en.ambientIntensity = 0.2f;
    const auto s = f.sprite({0, 0, 0}, 8.0f);
    f.scene.registry().get<SpriteRendererComponent>(s).lit = true;
    f.frame();
    // white * 0.2 (linear) -> 124 after gamma encoding.
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32), 124, 124, 124, 4), pixel(*gpu().device, 32, 32));
    CHECK(f.renderer.stats().lights2D == 0);

    const auto light = f.scene.createEntity("Light");
    auto& l = f.scene.registry().emplace<Light2DComponent>(light);
    l.color = {1, 1, 1, 1};
    l.intensity = 0.6f;
    l.radius = 4.0f;
    l.falloff = 1.0f;
    f.frame();
    CHECK(f.renderer.stats().lights2D == 1);
    // Center: 0.2 + 0.6 = 0.8 -> 230. At 2 units: 0.2 + 0.6 * 0.5 = 0.5 -> 186. Beyond the radius: ambient.
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32), 230, 230, 230, 5), pixel(*gpu().device, 32, 32));
    CHECK_MESSAGE(near(pixel(*gpu().device, 32 + 13, 32), 186, 186, 186, 8), pixel(*gpu().device, 45, 32));
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32 + 25), 124, 124, 124, 4), pixel(*gpu().device, 32, 57));
    // An unlit sprite ignores lights and ambient.
    f.scene.registry().get<SpriteRendererComponent>(s).lit = false;
    f.frame();
    CHECK(near(pixel(*gpu().device, 32 + 13, 32), 255, 255, 255));
}

TEST_CASE("2D light shadows: occluder boxes and tilemaps, soft edges, penetration, off by default") {
    Render2DFixture f;
    f.camera();  // 6.4 pixels per unit, world (0, 0) at pixel (32, 32)
    auto px = [&](float x, float y) { return pixel(*gpu().device, static_cast<u32>(32.0f + x * 6.4f), static_cast<u32>(32.0f - y * 6.4f)); };
    auto lum = [](Rgba p) { return int(p.r) + int(p.g) + int(p.b); };
    auto& env = f.scene.registry().emplace<EnvironmentComponent>(f.scene.createEntity("Env"));
    env.ambientIntensity = 0.0f;  // only the light
    const auto ground = f.sprite({0, 0, 0}, 10.0f);
    f.scene.registry().get<SpriteRendererComponent>(ground).lit = true;
    const auto light = f.scene.createEntity("Light");
    f.scene.registry().get<TransformComponent>(light).position = {-2.5f, 0, 0};
    auto& l = f.scene.registry().emplace<Light2DComponent>(light);
    l.color = {1, 1, 1, 1};
    l.intensity = 1.0f;
    l.radius = 6.0f;
    l.falloff = 1.0f;
    const auto wall = f.scene.createEntity("Wall");
    f.scene.registry().get<TransformComponent>(wall).position = {-1.0f, 0, 0};
    f.scene.registry().emplace<LightOccluder2DComponent>(wall).size = {0.5f, 1.0f};
    f.frame();
    const int behindOff = lum(px(1.0f, 0.0f));
    CHECK_MESSAGE(behindOff > 150, behindOff);  // castShadows off: no shadow
    CHECK(f.renderer.stats().shadowedLights2D == 0);
    l.castShadows = true;
    l.shadowSoftness = 0.0f;
    l.shadowPenetration = 0.0f;
    f.frame();
    CHECK(f.renderer.stats().shadowedLights2D == 1);
    CHECK_MESSAGE(lum(px(1.0f, 0.0f)) < 20, lum(px(1.0f, 0.0f)));                // behind the wall
    CHECK_MESSAGE(lum(px(1.0f, 2.5f)) > 100, lum(px(1.0f, 2.5f)));               // beside the shadow
    CHECK_MESSAGE(lum(px(-1.15f, 0.0f)) < 20, lum(px(-1.15f, 0.0f)));            // inside the wall, past its lit face
    l.shadowPenetration = 0.5f;  // the light enters the wall: its face stays lit
    f.frame();
    CHECK_MESSAGE(lum(px(-1.15f, 0.0f)) > 150, lum(px(-1.15f, 0.0f)));
    CHECK(lum(px(1.0f, 0.0f)) < 20);  // still dark further behind
    l.shadowStrength = 0.5f;  // half-dark shadows
    f.frame();
    const int half = lum(px(1.0f, 0.0f));
    // Half the light (linear) shows at 0.5^(1/2.2) = 73 % of the value after gamma encoding.
    CHECK_MESSAGE((half > behindOff * 65 / 100 && half < behindOff * 82 / 100), half, " vs ", behindOff);
    l.shadowStrength = 1.0f;

    // A tilemap occludes with its filled cells: one cell at x in [-0.5, 0.5], on the light's other side.
    f.scene.destroyEntity(wall);
    f.scene.registry().get<TransformComponent>(light).position = {-3.0f, 0, 0};
    const auto map = f.scene.createEntity("Map");
    f.scene.registry().get<TransformComponent>(map).position = {-1.5f, -0.5f, 0};
    auto& tm = f.scene.registry().emplace<TilemapComponent>(map);
    tm.width = 3;
    tm.height = 1;
    tm.dataWidth = 3;
    tm.tiles = {-1, 0, -1};
    f.scene.registry().emplace<LightOccluder2DComponent>(map);
    f.frame();
    CHECK_MESSAGE(lum(px(2.0f, 0.0f)) < 20, lum(px(2.0f, 0.0f)));
    CHECK_MESSAGE(lum(px(2.0f, 2.2f)) > 60, lum(px(2.0f, 2.2f)));
    CHECK(f.renderer.stats().occluderEdges2D >= 4);
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("2D normal maps: relief lit from the light's side, follows flips and rotation") {
    Render2DFixture f;
    f.camera();  // 6.4 pixels per unit, world (0, 0) at pixel (32, 32)
    auto lum = [](Rgba p) { return int(p.r) + int(p.g) + int(p.b); };
    auto center = [&] { return lum(pixel(*gpu().device, 32, 32)); };
    f.scene.registry().emplace<EnvironmentComponent>(f.scene.createEntity("Env")).ambientIntensity = 0.0f;
    // Normal map whose surface faces +x (right), OpenGL convention: (0.8, 0, 0.6).
    Image nm;
    nm.width = nm.height = 4;
    for (int i = 0; i < 16; ++i) nm.pixels.insert(nm.pixels.end(), {230, 128, 204, 255});
    auto png = encodePng(4, 4, nm.pixels);
    REQUIRE(png);
    REQUIRE(f.vfs.writeBytes("assets://bump_n.png", png.value().data(), png.value().size()));
    const auto sprite = f.sprite({0, 0, 0}, 6.0f);
    auto& sr = f.scene.registry().get<SpriteRendererComponent>(sprite);
    sr.lit = true;
    const auto light = f.scene.createEntity("Light");
    auto& lt = f.scene.registry().get<TransformComponent>(light);
    auto& l = f.scene.registry().emplace<Light2DComponent>(light);
    l.color = {1, 1, 1, 1};
    l.intensity = 1.0f;
    l.radius = 8.0f;
    l.falloff = 1.0f;
    l.height = 0.5f;
    lt.position = {2.5f, 0, 0};
    f.settle();
    const int plainRight = center();
    lt.position = {-2.5f, 0, 0};
    f.frame();
    const int plainLeft = center();
    CHECK(std::abs(plainRight - plainLeft) <= 10);  // without a normal map: same from both sides
    CHECK(f.renderer.stats().normalMappedSprites == 0);

    sr.normalMap = "assets://bump_n.png";
    lt.position = {2.5f, 0, 0};
    f.settle();
    CHECK(f.renderer.stats().normalMappedSprites == 1);
    const int facing = center();
    lt.position = {-2.5f, 0, 0};
    f.frame();
    const int away = center();
    CHECK_MESSAGE(facing > 120, facing);
    CHECK_MESSAGE(away < 15, away);  // the relief faces away from this light
    sr.flipX = true;  // mirrored: the surface now faces left
    f.frame();
    CHECK_MESSAGE(center() > 120, center());
    sr.flipX = false;
    f.scene.registry().get<TransformComponent>(sprite).rotation = math::Quat::fromEuler({0, 0, math::radians(180.0f)});
    f.frame();
    CHECK_MESSAGE(center() > 120, center());  // turned around: faces left too
    // A higher light shades the relief less.
    f.scene.registry().get<TransformComponent>(sprite).rotation = {};
    l.height = 20.0f;
    f.frame();
    CHECK_MESSAGE(center() > away + 40, center());
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("2D skins: a picture bent by its bones") {
    Render2DFixture f;
    f.camera();  // 6.4 pixels per unit, world (0, 0) at pixel (32, 32)
    auto lum = [&](float x, float y) {
        const Rgba p = pixel(*gpu().device, static_cast<u32>(32.0f + x * 6.4f), static_cast<u32>(32.0f - y * 6.4f));
        return int(p.r) + int(p.g) + int(p.b);
    };
    const auto root = f.scene.createEntity("Creature");
    f.scene.registry().emplace<Skeleton2DComponent>(root);
    const auto b1 = f.scene.createEntity("Bone1", root);
    f.scene.registry().emplace<Bone2DComponent>(b1).length = 2.0f;
    const auto b2 = f.scene.createEntity("Bone2", b1);
    f.scene.registry().get<TransformComponent>(b2).position = {2, 0, 0};
    f.scene.registry().emplace<Bone2DComponent>(b2).length = 2.0f;
    const auto body = f.scene.createEntity("Body", root);
    auto& sk = f.scene.registry().emplace<Skin2DComponent>(body);
    sk.size = {4.0f, 0.8f};
    sk.pivot = {0.0f, 0.5f};
    sk.columns = 12;
    sk.rows = 2;
    captureBone2DRest(f.scene);
    f.settle();
    CHECK(f.renderer.stats().skins2D == 1);
    CHECK(lum(3.5f, 0.0f) > 600);  // straight: the far end is on the right
    CHECK(lum(2.0f, 1.5f) < 30);
    f.scene.registry().get<TransformComponent>(b2).rotation = math::Quat::fromAxisAngle({0, 0, 1}, math::radians(90.0f));
    f.frame();
    CHECK(lum(3.5f, 0.0f) < 30);   // bent up: the far end left this place...
    CHECK(lum(2.0f, 1.5f) > 600);  // ...and points up from the joint
    CHECK(lum(0.5f, 0.0f) > 600);  // the near end did not move
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("sprite sheet frames select UVs and pivots") {
    Render2DFixture f;
    f.camera();
    const auto e = f.sprite({0, 0, 0}, 2.0f);
    auto* sr = &f.scene.registry().get<SpriteRendererComponent>(e);
    sr->sheet = "assets://two.agrsheet";
    sr->frame = 0;
    f.settle();
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32), 255, 255, 0), pixel(*gpu().device, 32, 32));
    // Frame 1: magenta, pivot bottom-left -> covers [0, 2] x [0, 2] only.
    sr = &f.scene.registry().get<SpriteRendererComponent>(e);
    sr->frame = 1;
    f.frame();
    CHECK_MESSAGE(near(pixel(*gpu().device, 36, 28), 255, 0, 255), pixel(*gpu().device, 36, 28));
    CHECK(near(pixel(*gpu().device, 28, 36), 0, 0, 0));
    // Invalid frame: whole texture (both colors), no crash.
    sr->frame = 7;
    f.frame();
    CHECK(near(pixel(*gpu().device, 28, 32), 255, 255, 0));
    CHECK(near(pixel(*gpu().device, 36, 32), 255, 0, 255));
}

TEST_CASE("tilemaps draw only visible cells from the tileset") {
    Render2DFixture f;
    f.camera();
    const auto map = f.scene.createEntity("Map");
    f.scene.registry().get<TransformComponent>(map).position = {-2, -2, 0};
    auto& tm = f.scene.registry().emplace<TilemapComponent>(map);
    tm.tileset = "assets://two.agrsheet";
    tm.width = 4;
    tm.height = 4;
    tilemapFillRect(tm, 0, 0, 3, 3, 0);
    tilemapSet(tm, 3, 3, 1);
    tilemapSet(tm, 0, 0, -1);
    f.settle();
    CHECK(f.renderer.stats().tilesVisible == 15);
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32), 255, 255, 0), pixel(*gpu().device, 32, 32));
    CHECK_MESSAGE(near(pixel(*gpu().device, 32 + 9, 32 - 9), 255, 0, 255), pixel(*gpu().device, 41, 23));  // cell (3, 3)
    CHECK(near(pixel(*gpu().device, 32 - 9, 32 + 9), 0, 0, 0));                                           // empty cell (0, 0)

    // A large map is clipped to the camera view: cost follows the screen, not the map.
    auto& big = f.scene.registry().get<TilemapComponent>(map);
    big.width = 1000;
    big.height = 1000;
    tilemapFillRect(big, 0, 0, 999, 999, 0);
    f.frame();
    CHECK(f.renderer.stats().tilesVisible <= 12u * 13u);
    CHECK(f.renderer.stats().tilesVisible >= 7u * 7u);
}

TEST_CASE("tilemaps: animated tiles follow the renderer time, autotiles pick frames from neighbours") {
    Render2DFixture f;
    f.camera();
    const auto map = f.scene.createEntity("Map");
    f.scene.registry().get<TransformComponent>(map).position = {-2, -2, 0};
    auto& tm = f.scene.registry().emplace<TilemapComponent>(map);
    tm.tileset = "assets://two.agrsheet";
    tm.width = 4;
    tm.height = 4;
    tilemapFillRect(tm, 0, 0, 3, 3, 0);
    tm.animatedTiles = "0,1@1";
    f.renderer.setTime(0.2);
    f.settle();
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32), 255, 255, 0), pixel(*gpu().device, 32, 32));
    f.renderer.setTime(1.2);
    f.frame();
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32), 255, 0, 255), pixel(*gpu().device, 32, 32));
    // Autotile group at 0 on a 2-frame sheet: a lone cell (mask 0) is frame 0, one with a +y
    // neighbour is frame 1 (yellow vs magenta).
    auto& t2 = f.scene.registry().get<TilemapComponent>(map);
    t2.animatedTiles.clear();
    t2.autotile = "0";
    tilemapFillRect(t2, 0, 0, 3, 3, -1);
    tilemapSet(t2, 0, 0, 0);  // alone
    tilemapSet(t2, 3, 2, 0);  // has (3,3) above
    tilemapSet(t2, 3, 3, 0);  // mask 4 -> frame 4: out of the sheet, not drawn
    f.frame();
    CHECK_MESSAGE(near(pixel(*gpu().device, 32 - 9, 32 + 9), 255, 255, 0), pixel(*gpu().device, 23, 41));
    CHECK_MESSAGE(near(pixel(*gpu().device, 32 + 9, 32 - 3), 255, 0, 255), pixel(*gpu().device, 41, 29));
    CHECK(near(pixel(*gpu().device, 32 + 9, 32 - 9), 0, 0, 0));
}

TEST_CASE("directional Light2D: uniform light, no radius, shades normal maps from its direction") {
    Render2DFixture f;
    f.camera();
    f.scene.registry().emplace<EnvironmentComponent>(f.scene.createEntity("Env")).ambientIntensity = 0.0f;
    const auto s = f.sprite({0, 0, 0}, 10.0f);
    f.scene.registry().get<SpriteRendererComponent>(s).lit = true;
    const auto light = f.scene.createEntity("Sun");
    f.scene.registry().get<TransformComponent>(light).position = {100, 100, 0};  // position is ignored
    auto& l = f.scene.registry().emplace<Light2DComponent>(light);
    l.color = {1, 1, 1, 1};
    l.intensity = 0.5f;
    l.radius = 0.1f;
    l.directional = true;
    f.frame();
    CHECK(f.renderer.stats().lights2D == 1);
    // 0.5 linear everywhere -> 186.
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32), 186, 186, 186, 5), pixel(*gpu().device, 32, 32));
    CHECK_MESSAGE(near(pixel(*gpu().device, 5, 5), 186, 186, 186, 5), pixel(*gpu().device, 5, 5));
    CHECK_MESSAGE(near(pixel(*gpu().device, 58, 58), 186, 186, 186, 5), pixel(*gpu().device, 58, 58));
}

TEST_CASE("particles: soft default texture, additive blending") {
    Render2DFixture f;
    f.camera();
    const auto e = f.scene.createEntity("Emitter");
    auto& em = f.scene.registry().emplace<ParticleEmitter2DComponent>(e);
    em.rate = 0.0f;
    em.burst = 2;
    em.speedMin = em.speedMax = 0.0f;
    em.gravity = {0, 0};
    em.lifetimeMin = em.lifetimeMax = 100.0f;
    em.startSize = em.endSize = 3.0f;
    em.startColor = em.endColor = {0.5f, 0.0f, 0.0f, 1.0f};
    em.additive = true;
    em.seed = 1;
    f.scene.updateTransforms();
    const ParticleStats2D st = simulateParticles2D(f.scene, 0.016f);
    CHECK(st.particles == 2);
    f.frame();
    CHECK(f.renderer.stats().particlesVisible == 2);
    // Two additive particles at the same place: 2 x 0.5 red at the center of the soft disc.
    const Rgba c = pixel(*gpu().device, 32, 32);
    CHECK_MESSAGE(c.r > 200, c);
    CHECK_MESSAGE(c.g < 10, c);
    CHECK(near(pixel(*gpu().device, 4, 4), 0, 0, 0));
}

TEST_CASE("particles: over-lifetime curves and flipbook frames") {
    Render2DFixture f;
    f.camera();  // 6.4 pixels per unit, world (0, 0) at pixel (32, 32)
    ParticleCurvesLibrary curves(f.vfs);
    f.renderer.setParticleCurves(&curves);
    ParticleCurves c;
    c.size = {{0.0f, 1.0f}, {1.0f, 0.5f}};
    c.gradient = {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
    REQUIRE(curves.save("assets://fx.agrparticle", c));
    const auto e = f.scene.createEntity("Emitter");
    auto& em = f.scene.registry().emplace<ParticleEmitter2DComponent>(e);
    em.emitting = false;
    em.startSize = 2.0f;
    em.texture = "assets://two.png";  // 2 x 1: yellow | magenta
    em.curves = "assets://fx.agrparticle";
    auto& st = f.scene.registry().emplace<ParticleState2DComponent>(e);
    st.started = true;
    auto particle = [](float x, float age) {
        Particle2D p;
        p.position = {x, 0.0f};
        p.age = age;
        p.lifetime = 1.0f;
        return p;
    };
    st.particles = {particle(-3.0f, 0.0f), particle(3.0f, 1.0f)};
    f.settle();
    CHECK(f.renderer.stats().particlesVisible == 2);
    auto at = [&](float x, float y) { return pixel(*gpu().device, static_cast<u32>(32.0f + x * 6.4f), static_cast<u32>(32.0f - y * 6.4f)); };
    // Birth: red, 2 units wide (texture sampled across both halves: yellow x red = red, magenta x red = red).
    CHECK(at(-3.0f, 0.0f).r > 150);
    CHECK(at(-3.0f, 0.0f).b < 40);
    CHECK(at(-3.7f, 0.0f).r > 150);  // still inside: half size 1.0
    // Death: blue, half the size (0.5 unit half width): 0.8 units from the center is outside.
    CHECK(at(3.0f, 0.0f).b > 150);
    CHECK(at(3.0f, 0.0f).r < 40);
    CHECK(int(at(3.8f, 0.0f).b) < 30);
    // Flipbook 2 x 1 over the life: frame 0 (yellow) young, frame 1 (magenta) old; white gradient.
    c.gradient = {{0.0f, {1, 1, 1, 1}}};
    c.size = {};
    curves.preview("assets://fx.agrparticle", c);
    em.startSize = em.endSize = 2.0f;
    em.flipbookColumns = 2;
    st.particles = {particle(-3.0f, 0.25f), particle(3.0f, 0.75f)};
    f.frame();
    const Rgba young = at(-3.0f, 0.0f), old = at(3.0f, 0.0f);
    CHECK_MESSAGE((young.r > 200 && young.g > 200 && young.b < 60), young);  // yellow
    CHECK_MESSAGE((old.r > 200 && old.g < 60 && old.b > 200), old);          // magenta
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("trails and velocity-stretched particles") {
    Render2DFixture f;
    f.camera();  // 6.4 pixels per unit, world (0, 0) at pixel (32, 32)
    auto lum = [&](float x, float y) {
        const Rgba p = pixel(*gpu().device, static_cast<u32>(32.0f + x * 6.4f), static_cast<u32>(32.0f - y * 6.4f));
        return int(p.r) + int(p.g) + int(p.b);
    };
    // A trail recorded along x from -4 to 4 (fresh points: full width).
    const auto comet = f.scene.createEntity("Comet");
    auto& t = f.scene.registry().emplace<TrailComponent>(comet);
    t.width = 1.0f;
    t.lifetime = 100.0f;
    t.color = {1, 0.2f, 0.1f, 1};
    t.minDistance = 0.5f;
    auto& tr = f.scene.registry().get<TransformComponent>(comet);
    for (int i = 0; i <= 16; ++i) {
        tr.position = {-4.0f + 0.5f * static_cast<float>(i), 3.0f, 0};
        f.scene.updateTransforms();
        updateTrails(f.scene, 0.001f);
    }
    f.settle();
    CHECK(f.renderer.stats().trails == 1);
    CHECK(lum(0.0f, 3.0f) > 150);  // on the ribbon
    CHECK(lum(0.0f, 4.2f) < 20);   // above it: half width 0.5
    // A particle moving at 10 units/s with a 0.2 s stretch: 0.5 wide, 0.5 + 2 long.
    const auto e = f.scene.createEntity("Spark");
    auto& em = f.scene.registry().emplace<ParticleEmitter2DComponent>(e);
    em.emitting = false;
    em.startSize = em.endSize = 0.5f;
    em.startColor = em.endColor = {1, 1, 1, 1};
    em.texture = "assets://two.png";
    em.velocityStretch = 0.2f;
    auto& st = f.scene.registry().emplace<ParticleState2DComponent>(e);
    st.started = true;
    Particle2D p;
    p.position = {0.0f, -2.0f};
    p.velocity = {10.0f, 0.0f};
    p.lifetime = 100.0f;
    st.particles = {p};
    f.frame();
    CHECK(lum(1.0f, -2.0f) > 150);  // stretched along x
    CHECK(lum(0.0f, -2.6f) < 20);   // still thin across
    em.velocityStretch = 0.0f;
    f.frame();
    CHECK(lum(1.0f, -2.0f) < 20);
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("parallax: factor 0 follows the camera, 1 stays in the world") {
    Render2DFixture f;
    f.camera(4.0f, 0.0f);
    const auto layer = f.scene.createEntity("Layer");
    f.scene.registry().emplace<ParallaxComponent>(layer).factor = {0.0f, 0.0f};
    const auto child = f.sprite({0, 0, 0}, 2.0f);
    f.scene.setParent(child, layer);
    f.frame();
    CHECK(near(pixel(*gpu().device, 32, 32), 255, 255, 255));  // drawn at the camera center
    f.scene.registry().get<ParallaxComponent>(layer).factor = {1.0f, 1.0f};
    f.frame();
    CHECK(near(pixel(*gpu().device, 32, 32), 0, 0, 0));                   // back at x = 0
    CHECK(near(pixel(*gpu().device, 32 - 26, 32), 255, 255, 255));        // 4 units left
}

TEST_CASE("camera scale modes for mobile aspect ratios") {
    Render2DFixture f;
    auto& cam = f.camera();
    cam.referenceAspect = 2.0f;
    f.sprite({0, 0, 0}, 4.0f);
    // Square viewport. FixedHeight: 10 units tall -> the 4-unit sprite spans 25.6 px.
    f.frame();
    CHECK(near(pixel(*gpu().device, 32 + 11, 32), 255, 255, 255));
    // FixedWidth: 10 * 2 = 20 units wide -> 12.8 px; the same pixel is now outside.
    f.scene.registry().get<CameraComponent>(f.scene.findByName("Camera")).scaleMode = ScaleMode::FixedWidth;
    f.frame();
    CHECK(near(pixel(*gpu().device, 32 + 11, 32), 0, 0, 0));
    CHECK(near(pixel(*gpu().device, 32 + 5, 32), 255, 255, 255));
}

}  // TEST_SUITE
