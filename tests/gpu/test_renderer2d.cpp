// 2D rendering: lit sprites (Light2D), tilemaps, sprite sheet frames, particles,
// parallax and camera scale modes, on every graphics backend.
#include <filesystem>

#include "GpuFixture.hpp"
#include "agr/assets/AssetManager.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/Renderer.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/twod/Particles2D.hpp"
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
