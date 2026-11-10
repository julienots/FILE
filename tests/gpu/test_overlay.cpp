// Game UI overlay: drawn over the scene in pixels (top-left origin), directly or after
// an off-screen pass (render scale / MSAA), with images and text, on every backend.
#include <filesystem>

#include "GpuFixture.hpp"
#include "agr/assets/AssetManager.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/Renderer.hpp"
#include "agr/ui/UISystem.hpp"

using namespace agr;
using namespace agr::test;

namespace {

struct OverlayFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("agr_gpuui_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    JobSystem jobs{1};
    AssetManager assets{vfs, jobs, gpu().device.get()};
    Renderer renderer{*gpu().device, assets};
    UISystem ui{vfs};
    Scene scene;
    ecs::Entity canvas;

    OverlayFixture() {
        std::filesystem::create_directories(root / "fonts");
        std::filesystem::copy_file(AGR_TEST_FONT, root / "fonts" / "Inter-Regular.ttf");
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        registerRenderComponents();
        registerUIComponents();
        REQUIRE(renderer.initialize());
        GraphicsProfile p = graphicsProfile(GraphicsTier::Low);
        p.renderScale = 1.0f;
        p.msaaSamples = 1;
        renderer.setProfile(p);
        ui.setRenderer(&renderer);
        renderer.setOverlaySource(&ui);
        const auto cam = scene.createEntity("Camera");
        scene.registry().get<TransformComponent>(cam).position = {0, 0, 10};
        auto& c = scene.registry().emplace<CameraComponent>(cam);
        c.projection = Projection::Orthographic;
        c.clearColor = {0, 0, 1, 1};  // blue background
        canvas = scene.createEntity("Canvas");
        auto& cv = scene.registry().emplace<UICanvasComponent>(canvas);
        cv.scaleMode = UIScaleMode::ConstantPixelSize;
    }
    ~OverlayFixture() { std::filesystem::remove_all(root); }

    ecs::Entity box(math::Vec2 pos, math::Vec2 size, math::Vec4 color) {
        const auto e = scene.createEntity("Box", canvas);
        auto& r = scene.registry().emplace<UIRectComponent>(e);
        r.anchorMin = r.anchorMax = r.pivot = {0, 0};
        r.position = pos;
        r.size = size;
        scene.registry().emplace<UIImageComponent>(e).color = color;
        return e;
    }
    void frame() {
        scene.updateTransforms();
        assets.update();
        renderer.render(scene, kWidth, kHeight);
    }
};

} // namespace

TEST_CASE("overlay: UI rectangles in pixels over the scene, top-left origin") {
    OverlayFixture f;
    f.box({0, 0}, {16, 16}, {1, 0, 0, 1});          // red, top-left corner
    f.box({48, 40}, {16, 24}, {0, 1, 0, 0.5f});     // half-transparent green, bottom-right
    // A sprite in the scene, under the UI.
    const auto s = f.scene.createEntity("Sprite");
    f.scene.registry().emplace<SpriteRendererComponent>(s).size = {100, 100};
    f.frame();
    auto& d = *gpu().device;
    CHECK_MESSAGE(near(pixel(d, 4, 4), 255, 0, 0), pixel(d, 4, 4));
    CHECK_MESSAGE(near(pixel(d, 32, 32), 255, 255, 255), pixel(d, 32, 32));  // white sprite, no UI
    // 50 % green over white (blended in gamma space).
    CHECK_MESSAGE(near(pixel(d, 56, 60), 128, 255, 128, 3), pixel(d, 56, 60));
    CHECK(f.renderer.stats().overlayQuads == 2);

    // Same picture through the off-screen path (render scale): the UI is at full resolution.
    GraphicsProfile p = f.renderer.profile();
    p.renderScale = 0.5f;
    f.renderer.setProfile(p);
    f.frame();
    CHECK_MESSAGE(near(pixel(d, 1, 1), 255, 0, 0), pixel(d, 1, 1));
    CHECK_MESSAGE(near(pixel(d, 15, 15), 255, 0, 0), pixel(d, 15, 15));
    CHECK_MESSAGE(near(pixel(d, 17, 17), 255, 255, 255), pixel(d, 17, 17));
    CHECK_MESSAGE(near(pixel(d, 56, 60), 128, 255, 128, 3), pixel(d, 56, 60));

    // A view without overlay (editor scene view) shows no UI.
    d.beginFrame(kWidth, kHeight);
    RenderView view = *RenderView::fromPrimaryCamera(f.scene);
    view.overlay = false;
    f.renderer.renderView(f.scene, view, {}, kWidth, kHeight);
    d.endFrame();
    CHECK_MESSAGE(near(pixel(d, 4, 4), 255, 255, 255), pixel(d, 4, 4));
}

TEST_CASE("overlay: text from the glyph atlas") {
    OverlayFixture f;
    const auto label = f.box({0, 0}, {64, 64}, {0, 0, 0, 0});
    auto& t = f.scene.registry().emplace<UITextComponent>(label);
    t.text = "W";
    t.fontSize = 60;
    t.color = {1, 1, 0, 1};
    f.frame();
    auto& d = *gpu().device;
    CHECK(f.ui.fonts().texture());
    CHECK(f.ui.stats().texts == 1);
    // Solid yellow strokes inside the centered glyph box, blue background around.
    std::vector<u8> px;
    REQUIRE(d.readBackbuffer(0, 0, kWidth, kHeight, px));
    int yellow = 0, other = 0;
    for (usize i = 0; i < px.size(); i += 4) {
        if (px[i] > 230 && px[i + 1] > 230 && px[i + 2] < 30) ++yellow;
        else if (!(px[i] < 30 && px[i + 1] < 30 && px[i + 2] > 225)) ++other;  // anti-aliased edges
    }
    CHECK(yellow > 60);
    CHECK(other > 0);
    CHECK(other < yellow * 2);
    CHECK_MESSAGE(near(pixel(d, 1, 1), 0, 0, 255), pixel(d, 1, 1));
    CHECK_MESSAGE(near(pixel(d, 62, 62), 0, 0, 255), pixel(d, 62, 62));
    // Changing the text rasterizes new glyphs (atlas upload while rendering).
    t.text = "AGR";
    f.frame();
    CHECK(f.ui.stats().quads == 3);
    CHECK(f.ui.fonts().stats().glyphs >= 4);
}
