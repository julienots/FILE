#include <filesystem>

#include "GpuFixture.hpp"
#include "agr/assets/AssetManager.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/Renderer.hpp"

using namespace agr;
using namespace agr::test;
using math::Vec3;

namespace {

struct RenderFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("agr_gpu_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    JobSystem jobs{1};
    AssetManager assets{vfs, jobs, gpu().device.get()};
    Renderer renderer{*gpu().device, assets};
    Scene scene;

    RenderFixture() {
        std::filesystem::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        registerRenderComponents();
        REQUIRE(renderer.initialize());
        GraphicsProfile p = graphicsProfile(GraphicsTier::Low);
        p.renderScale = 1.0f;  // pixel-exact checks
        renderer.setProfile(p);
    }
    ~RenderFixture() { std::filesystem::remove_all(root); }

    /// Orthographic camera at z = 10 showing [-5, 5] vertically, black background.
    ecs::Entity orthoCamera() {
        const auto cam = scene.createEntity("Camera");
        auto& c = scene.registry().emplace<CameraComponent>(cam);
        c.projection = Projection::Orthographic;
        c.orthographicSize = 5.0f;
        c.clearColor = {0, 0, 0, 1};
        scene.registry().get<TransformComponent>(cam).position = {0, 0, 10};
        return cam;
    }
    ecs::Entity sprite(const char* name, Vec3 pos, math::Color color, std::string texture = "", i32 order = 0) {
        const auto e = scene.createEntity(name);
        scene.registry().get<TransformComponent>(e).position = pos;
        auto& s = scene.registry().emplace<SpriteRendererComponent>(e);
        s.color = color;
        s.size = {4.0f, 4.0f};
        s.texture = std::move(texture);
        s.orderInLayer = order;
        return e;
    }
    void frame() {
        scene.updateTransforms();
        assets.update();
        renderer.render(scene, kWidth, kHeight);
    }
};

}  // namespace

TEST_SUITE("gpu.renderer") {

TEST_CASE("no camera clears to the AGR background") {
    RenderFixture f;
    f.frame();
    CHECK_FALSE(f.renderer.stats().hasCamera);
    CHECK(near(pixel(*gpu().device, 32, 32), 14, 16, 19, 2));
}

TEST_CASE("sprites: color, sorting, culling and batching") {
    RenderFixture f;
    f.orthoCamera();
    f.sprite("Back", {0, 0, 0}, {1, 0, 0, 1}, "", 0);
    f.sprite("Front", {0, 0, 0}, {0, 1, 0, 1}, "", 1);  // higher order wins
    f.sprite("Away", {100, 0, 0}, {0, 0, 1, 1});        // outside the view
    f.frame();
    const auto& s = f.renderer.stats();
    CHECK(s.hasCamera);
    CHECK(s.spritesVisible == 2);
    CHECK(s.spritesCulled == 1);
    CHECK(s.spriteBatches == 1);  // same (white) texture -> one draw
    CHECK(s.drawCalls == 1);
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32), 0, 255, 0), pixel(*gpu().device, 32, 32));
    CHECK(near(pixel(*gpu().device, 2, 2), 0, 0, 0));  // outside the 4x4 sprite
}

TEST_CASE("sprite alpha blending is done in gamma space") {
    RenderFixture f;
    f.orthoCamera();
    f.sprite("Half", {0, 0, 0}, {1, 1, 1, 0.5f});
    f.frame();
    const Rgba p = pixel(*gpu().device, 32, 32);
    CHECK_MESSAGE(near(p, 128, 128, 128, 3), p);
}

TEST_CASE("textured sprites load through the asset manager") {
    RenderFixture f;
    // 2x1 texture: left yellow, right magenta.
    Image img;
    img.width = 2;
    img.height = 1;
    img.pixels = {255, 255, 0, 255, 255, 0, 255, 255};
    auto png = encodePng(2, 1, img.pixels);
    REQUIRE(png);
    REQUIRE(f.vfs.writeBytes("assets://two.png", png.value().data(), png.value().size()));
    f.orthoCamera();
    const auto e = f.sprite("Tex", {0, 0, 0}, {1, 1, 1, 1}, "assets://two.png");
    f.scene.registry().get<SpriteRendererComponent>(e).pixelArt = true;
    f.frame();                 // first frame: texture still loading -> white fallback
    f.assets.waitAll();
    f.frame();
    CHECK_MESSAGE(near(pixel(*gpu().device, 26, 32), 255, 255, 0), pixel(*gpu().device, 26, 32));
    CHECK_MESSAGE(near(pixel(*gpu().device, 38, 32), 255, 0, 255), pixel(*gpu().device, 38, 32));
    // flipX swaps the halves.
    f.scene.registry().get<SpriteRendererComponent>(e).flipX = true;
    f.frame();
    CHECK(near(pixel(*gpu().device, 26, 32), 255, 0, 255));
    // A missing file shows the magenta / black error checkerboard, not a crash.
    f.scene.registry().get<SpriteRendererComponent>(e).texture = "assets://missing.png";
    f.frame();
    f.assets.waitAll();
    f.frame();
    CHECK(f.assets.stats().failed == 1);
    // Two different textures -> two batches.
    f.sprite("Other", {0, 0, -1}, {1, 1, 1, 1}, "assets://two.png");
    f.frame();
    CHECK(f.renderer.stats().spriteBatches == 2);
}

TEST_CASE("lit meshes: back-face culling, lighting and frustum culling") {
    RenderFixture f;
    const auto cam = f.scene.createEntity("Camera");
    auto& c = f.scene.registry().emplace<CameraComponent>(cam);
    c.clearColor = {0, 0, 0, 1};
    f.scene.registry().get<TransformComponent>(cam).position = {0, 0, 3};
    const auto light = f.scene.createEntity("Light");
    f.scene.registry().emplace<DirectionalLightComponent>(light).color = {1, 1, 1, 1};  // shines along -Z: onto the quad
    auto& env = f.scene.registry().emplace<EnvironmentComponent>(light);
    env.ambientIntensity = 0.0f;
    const auto quad = f.scene.createEntity("Quad");
    auto& m = f.scene.registry().emplace<MeshRendererComponent>(quad);
    m.mesh = PrimitiveMesh::Quad;
    m.color = {1, 0, 0, 1};
    f.scene.registry().get<TransformComponent>(quad).scale = Vec3{2.0f};
    const auto far = f.scene.createEntity("Behind camera");
    f.scene.registry().emplace<MeshRendererComponent>(far).castShadows = false;  // would shadow the quad (tested separately)
    f.scene.registry().get<TransformComponent>(far).position = {0, 0, 50};

    f.frame();
    CHECK(f.renderer.stats().meshesVisible == 1);
    CHECK(f.renderer.stats().meshesCulled == 1);
    const Rgba lit = pixel(*gpu().device, 32, 32);
    CHECK_MESSAGE(lit.r > 225, lit);  // fully lit red (ACES maps 1.0 to ~0.8 before gamma)...
    // ...plus a white GGX highlight (camera faces the light): equal green and blue.
    CHECK_MESSAGE(std::abs(int(lit.g) - int(lit.b)) <= 2, lit);
    CHECK_MESSAGE(lit.g > 40, lit);
    // Away from the highlight centre the surface is mostly diffuse red.
    const Rgba edge = pixel(*gpu().device, 14, 32);
    CHECK_MESSAGE(edge.r > 215, edge);
    CHECK_MESSAGE(edge.g < lit.g, edge);

    // Rotated 180 degrees the quad shows its back face: culled, background visible.
    f.scene.registry().get<TransformComponent>(quad).rotation = math::Quat::fromAxisAngle({0, 1, 0}, math::kPi);
    f.frame();
    CHECK_MESSAGE(near(pixel(*gpu().device, 32, 32), 0, 0, 0), pixel(*gpu().device, 32, 32));

    // Light from behind the surface: only ambient (0) remains -> black.
    f.scene.registry().get<TransformComponent>(quad).rotation = math::Quat::identity();
    f.scene.registry().get<TransformComponent>(light).rotation = math::Quat::fromAxisAngle({0, 1, 0}, math::kPi);
    f.frame();
    CHECK(near(pixel(*gpu().device, 32, 32), 0, 0, 0, 2));
}

TEST_CASE("render scale and MSAA follow the profile") {
    RenderFixture f;
    f.orthoCamera();
    f.sprite("S", {0, 0, 0}, {0, 0, 1, 1});
    GraphicsProfile p = graphicsProfile(GraphicsTier::UltraMobile);
    f.renderer.setProfile(p);
    f.frame();
    CHECK(f.renderer.stats().renderWidth == kWidth / 2);
    CHECK(near(pixel(*gpu().device, 32, 32), 0, 0, 255));  // upscaled result still correct
    p = graphicsProfile(GraphicsTier::Ultra);
    f.renderer.setProfile(p);
    f.frame();
    CHECK(f.renderer.stats().renderWidth == kWidth);
    CHECK(f.renderer.stats().msaaSamples == std::min(8u, gpu().device->info().maxSamples));
    CHECK(near(pixel(*gpu().device, 32, 32), 0, 0, 255));
    p.renderScale = 0.5f;  // MSAA + downscale: resolve then upscale
    f.renderer.setProfile(p);
    f.frame();
    CHECK(near(pixel(*gpu().device, 32, 32), 0, 0, 255));
}

TEST_CASE("inactive entities are not drawn") {
    RenderFixture f;
    f.orthoCamera();
    const auto parent = f.scene.createEntity("Group");
    const auto s = f.sprite("Child", {0, 0, 0}, {1, 1, 1, 1});
    f.scene.setParent(s, parent);
    f.scene.registry().get<StateComponent>(parent).active = false;
    f.frame();
    CHECK(f.renderer.stats().spritesVisible == 0);
    CHECK(near(pixel(*gpu().device, 32, 32), 0, 0, 0));
}

}  // TEST_SUITE
