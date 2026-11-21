// Game UI overlay: drawn over the scene in pixels (top-left origin), directly or after
// an off-screen pass (render scale / MSAA), with images and text, on every backend.
#include <filesystem>

#include "GpuFixture.hpp"
#include "agr/assets/AssetManager.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/Renderer.hpp"
#include "agr/assets/Image.hpp"
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
    // One line (wrapping would put each letter of this 64 px box on its own line, below the box).
    t.text = "AGR";
    t.wrap = false;
    f.frame();
    CHECK_MESSAGE(f.ui.stats().quads == 3, f.ui.stats().quads);
    CHECK(f.ui.fonts().stats().glyphs >= 4);
}

TEST_CASE("overlay: style boxes with rounded corners, outline and soft shadow") {
    OverlayFixture f;
    const auto b = f.box({4, 4}, {56, 56}, {1, 0, 0, 1});
    auto& img = f.scene.registry().get<UIImageComponent>(b);
    img.cornerRadius = 16.0f;
    img.outlineWidth = 4.0f;
    img.outlineColor = {0, 1, 0, 1};
    f.frame();
    auto& d = *gpu().device;
    CHECK_MESSAGE(near(pixel(d, 5, 5), 0, 0, 255), pixel(d, 5, 5));      // cut corner: background
    CHECK_MESSAGE(near(pixel(d, 32, 32), 255, 0, 0), pixel(d, 32, 32));  // fill
    CHECK_MESSAGE(near(pixel(d, 6, 32), 0, 255, 0), pixel(d, 6, 32));    // outline along the left edge
    CHECK_MESSAGE(near(pixel(d, 32, 57), 0, 255, 0), pixel(d, 32, 57));  // and the bottom edge
    CHECK_MESSAGE(near(pixel(d, 10, 10), 0, 255, 0, 40), pixel(d, 10, 10));  // the rounded outline at 45 degrees
    CHECK_MESSAGE(near(pixel(d, 13, 13), 255, 0, 0, 40), pixel(d, 13, 13));  // fill inside the inner radius
    // Soft shadow: darkens the background around the box, fading outward.
    auto& i2 = f.scene.registry().get<UIImageComponent>(b);
    i2.outlineWidth = 0.0f;
    i2.cornerRadius = 0.0f;
    i2.shadowSize = 8.0f;
    i2.shadowOffset = {0, 0};
    i2.shadowColor = {0, 0, 0, 1};
    auto& r = f.scene.registry().get<UIRectComponent>(b);
    r.position = {16, 16};
    r.size = {32, 32};
    f.frame();
    const Rgba near1 = pixel(d, 14, 32), far1 = pixel(d, 10, 32), out1 = pixel(d, 2, 32);
    CHECK_MESSAGE(near1.b < far1.b, near1 << " vs " << far1);
    CHECK_MESSAGE(far1.b < 250, far1);
    CHECK_MESSAGE(out1.b > 250, out1);  // beyond the shadow
    CHECK(near(pixel(d, 32, 32), 255, 0, 0));
}

TEST_CASE("overlay: inline images in rich text") {
    OverlayFixture f;
    std::vector<u8> red(4 * 4 * 4);
    for (usize i = 0; i < red.size(); i += 4) red[i] = 255, red[i + 1] = 0, red[i + 2] = 0, red[i + 3] = 255;
    auto png = encodePng(4, 4, red);
    REQUIRE(png);
    REQUIRE(f.vfs.writeBytes("assets://red.png", png.value().data(), png.value().size()));
    const auto e = f.box({2, 2}, {60, 30}, {0, 0, 0, 0});
    auto& t = f.scene.registry().emplace<UITextComponent>(e);
    t.text = "[img=assets://red.png]";
    t.richText = true;
    t.fontSize = 24.0f;
    t.hAlign = UIHAlign::Left;
    t.wrap = false;
    f.frame();
    f.assets.waitAll();
    f.frame();
    f.frame();
    auto& d = *gpu().device;
    int reds = 0;
    for (u32 y = 2; y < 32; ++y)
        for (u32 x = 2; x < 30; ++x) {
            const Rgba p = pixel(d, x, y);
            reds += p.r > 200 && p.g < 60 && p.b < 60;
        }
    CHECK_MESSAGE(reds > 150, reds);  // ~21 x 21 pixels of image
}
