#include <doctest/doctest.h>

#include <filesystem>

#include "agr/core/Uuid.hpp"
#include "agr/ui/UISystem.hpp"

using namespace agr;
namespace fs = std::filesystem;

namespace {

struct UIFixture {
    fs::path root = fs::temp_directory_path() / ("agr_ui_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    Scene scene;
    UISystem ui{vfs};
    ecs::Entity canvas;

    UIFixture() {
        fs::create_directories(root / "fonts");
        fs::copy_file(AGR_TEST_FONT, root / "fonts" / "Inter-Regular.ttf");
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        registerUIComponents();
        canvas = scene.createEntity("Canvas");
        scene.registry().emplace<UICanvasComponent>(canvas);  // reference 1920 x 1080
    }
    ~UIFixture() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    ecs::Entity element(const char* name, ecs::Entity parent, math::Vec2 anchorMin, math::Vec2 anchorMax, math::Vec2 pivot, math::Vec2 position,
                        math::Vec2 size) {
        const auto e = scene.createEntity(name, parent);
        auto& r = scene.registry().emplace<UIRectComponent>(e);
        r.anchorMin = anchorMin;
        r.anchorMax = anchorMax;
        r.pivot = pivot;
        r.position = position;
        r.size = size;
        return e;
    }
    ecs::Entity button(const char* name, math::Vec2 center, math::Vec2 size) {
        const auto e = element(name, canvas, {0, 0}, {0, 0}, {0.5f, 0.5f}, center, size);
        scene.registry().emplace<UIImageComponent>(e);
        scene.registry().emplace<UIButtonComponent>(e);
        return e;
    }
    math::Vec4 rect(ecs::Entity e) { return scene.registry().get<UIRectComponent>(e).screenRect; }
    void pointers(std::initializer_list<UIPointer> list) { ui.processInput(scene, std::span<const UIPointer>(list.begin(), list.size())); }
};

void checkRect(math::Vec4 a, math::Vec4 b) {
    CHECK(a.x == doctest::Approx(b.x));
    CHECK(a.y == doctest::Approx(b.y));
    CHECK(a.z == doctest::Approx(b.z));
    CHECK(a.w == doctest::Approx(b.w));
}

} // namespace

TEST_SUITE("ui") {

TEST_CASE("anchors, pivot and canvas scaling") {
    UIFixture f;
    const auto corner = f.element("Corner", f.canvas, {1, 0}, {1, 0}, {1, 0}, {-20, 20}, {200, 60});
    const auto fill = f.element("Fill", f.canvas, {0, 0}, {1, 1}, {0.5f, 0.5f}, {0, 0}, {-40, -40});
    const auto child = f.element("Child", fill, {0.5f, 1}, {0.5f, 1}, {0.5f, 1}, {0, 0}, {100, 50});

    f.ui.layout(f.scene, 960, 540);  // half the reference resolution: scale 0.5
    CHECK(f.ui.canvasScale(f.canvas) == doctest::Approx(0.5f));
    checkRect(f.rect(corner), {960 - 10 - 100, 10, 100, 30});
    checkRect(f.rect(fill), {10, 10, 940, 520});
    checkRect(f.rect(child), {10 + 470 - 25, 10 + 520 - 25, 50, 25});  // bottom-center of its parent

    // Portrait phone: match 0 follows the width, 1 the height.
    f.ui.layout(f.scene, 1080, 1920);
    CHECK(f.ui.canvasScale(f.canvas) == doctest::Approx(std::exp2(0.5f * std::log2(1080.0f / 1920) + 0.5f * std::log2(1920.0f / 1080))));
    f.scene.registry().get<UICanvasComponent>(f.canvas).match = 0.0f;
    f.ui.layout(f.scene, 1080, 1920);
    CHECK(f.ui.canvasScale(f.canvas) == doctest::Approx(1080.0f / 1920.0f));
    f.scene.registry().get<UICanvasComponent>(f.canvas).match = 1.0f;
    f.ui.layout(f.scene, 1080, 1920);
    CHECK(f.ui.canvasScale(f.canvas) == doctest::Approx(1920.0f / 1080.0f));
    auto& c = f.scene.registry().get<UICanvasComponent>(f.canvas);
    c.scaleMode = UIScaleMode::ConstantPixelSize;
    c.pixelScale = 2.0f;
    f.ui.layout(f.scene, 1080, 1920);
    checkRect(f.rect(corner), {1080 - 40 - 400, 40, 400, 120});

    // Inactive elements and their children are not laid out.
    f.scene.registry().get<StateComponent>(fill).active = false;
    f.ui.layout(f.scene, 1080, 1920);
    CHECK(f.ui.stats().elements == 2);  // canvas + corner
}

TEST_CASE("layouts: column, row and grid") {
    UIFixture f;
    auto& canvas = f.scene.registry().get<UICanvasComponent>(f.canvas);
    canvas.scaleMode = UIScaleMode::ConstantPixelSize;
    const auto panel = f.element("Panel", f.canvas, {0, 0}, {0, 0}, {0, 0}, {100, 100}, {300, 400});
    auto& lay = f.scene.registry().emplace<UILayoutComponent>(panel);
    lay.spacing = 5;
    lay.padding = {10, 20, 10, 20};
    std::vector<ecs::Entity> items;
    for (int i = 0; i < 3; ++i) items.push_back(f.element("Item", panel, {0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {999, 999}, {100, 40}));

    f.ui.layout(f.scene, 1000, 1000);
    checkRect(f.rect(items[0]), {110, 120, 100, 40});
    checkRect(f.rect(items[1]), {110, 165, 100, 40});
    checkRect(f.rect(items[2]), {110, 210, 100, 40});
    lay.childAlignment = UIAlign::Center;
    f.ui.layout(f.scene, 1000, 1000);
    CHECK(f.rect(items[0]).x == doctest::Approx(100 + 10 + (280 - 100) / 2.0f));
    lay.expandChildren = true;
    f.ui.layout(f.scene, 1000, 1000);
    checkRect(f.rect(items[1]), {110, 165, 280, 40});

    lay.type = UILayoutType::Horizontal;
    lay.expandChildren = false;
    lay.childAlignment = UIAlign::End;
    f.ui.layout(f.scene, 1000, 1000);
    checkRect(f.rect(items[0]), {110, 100 + 400 - 20 - 40, 100, 40});
    CHECK(f.rect(items[2]).x == doctest::Approx(110 + 2 * 105));

    lay.type = UILayoutType::Grid;
    lay.cellSize = {80, 80};
    lay.childAlignment = UIAlign::Start;
    f.ui.layout(f.scene, 1000, 1000);  // 280 wide: 3 columns of 80 + 2 * 5
    checkRect(f.rect(items[2]), {110 + 2 * 85, 120, 80, 80});
    lay.cellSize = {100, 60};  // 2 columns
    f.ui.layout(f.scene, 1000, 1000);
    checkRect(f.rect(items[2]), {110, 120 + 65, 100, 60});
}

TEST_CASE("buttons: click, cancel, hover, disabled and multi-touch") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto a = f.button("A", {100, 100}, {100, 50});
    const auto b = f.button("B", {400, 100}, {100, 50});
    f.ui.layout(f.scene, 800, 600);

    f.pointers({{0, {100, 100}, false}});
    CHECK(f.ui.isHovered(a));
    CHECK(f.ui.pointerOverUI());
    f.pointers({{0, {100, 100}, true}});
    CHECK(f.ui.isPressed(a));
    CHECK(f.ui.takeEvents().empty());
    f.pointers({{0, {110, 105}, false}});
    auto events = f.ui.takeEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == UIEventType::Click);
    CHECK(events[0].entity == a);
    CHECK_FALSE(f.ui.isPressed(a));

    // Released elsewhere: cancelled.
    f.pointers({{0, {100, 100}, true}});
    f.pointers({{0, {600, 400}, false}});
    CHECK(f.ui.takeEvents().empty());
    CHECK_FALSE(f.ui.pointerOverUI());

    // Two fingers hold both buttons (on-screen controls), then lift one.
    f.pointers({{1, {100, 100}, true}, {2, {400, 100}, true}});
    CHECK(f.ui.isPressed(a));
    CHECK(f.ui.isPressed(b));
    f.pointers({{1, {100, 100}, true}, {2, {400, 100}, false}});
    CHECK(f.ui.isPressed(a));
    CHECK_FALSE(f.ui.isPressed(b));
    events = f.ui.takeEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].entity == b);
    f.pointers({});  // finger 1 vanished: released without a click
    CHECK_FALSE(f.ui.isPressed(a));
    CHECK(f.ui.takeEvents().empty());

    // Disabled buttons ignore clicks; a transparent image does not block.
    f.scene.registry().get<UIButtonComponent>(a).interactable = false;
    f.pointers({{0, {100, 100}, true}});
    f.pointers({{0, {100, 100}, false}});
    CHECK(f.ui.takeEvents().empty());
    const auto glass = f.element("Glass", f.canvas, {0, 0}, {1, 1}, {0.5f, 0.5f}, {0, 0}, {0, 0});
    f.scene.registry().emplace<UIImageComponent>(glass).color.w = 0.0f;
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.ui.hitTest({400, 100}) == b);
    CHECK(f.ui.hitTest({600, 400}).isNull());
    // An opaque overlay on top blocks the buttons below.
    f.scene.registry().get<UIImageComponent>(glass).color.w = 1.0f;
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.ui.hitTest({400, 100}) == glass);
    f.pointers({{0, {400, 100}, true}});
    CHECK_FALSE(f.ui.isPressed(b));
}

TEST_CASE("sliders follow the pointer and report changes") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto s = f.element("Volume", f.canvas, {0, 0}, {0, 0}, {0, 0}, {100, 100}, {220, 20});
    auto& slider = f.scene.registry().emplace<UISliderComponent>(s);
    slider.minValue = 0;
    slider.maxValue = 10;
    slider.value = 0;
    f.ui.layout(f.scene, 800, 600);
    // Usable range: 110 .. 310 (half a handle at each end).
    f.pointers({{0, {210, 110}, true}});
    CHECK(slider.value == doctest::Approx(5.0f));
    f.pointers({{0, {1000, 110}, true}});  // dragged past the end, outside the slider
    CHECK(slider.value == doctest::Approx(10.0f));
    f.pointers({{0, {1000, 110}, false}});
    auto events = f.ui.takeEvents();
    REQUIRE(events.size() == 2);
    CHECK(events[1].type == UIEventType::ValueChanged);
    CHECK(events[1].value == doctest::Approx(10.0f));
    slider.wholeNumbers = true;
    f.pointers({{0, {153, 110}, true}});
    CHECK(slider.value == doctest::Approx(2.0f));
}

TEST_CASE("text layout: lines, wrapping, alignment, UTF-8 and overlay geometry") {
    UIFixture f;
    FontLibrary& fonts = f.ui.fonts();
    auto one = fonts.layout("assets://fonts/Inter-Regular.ttf", "Hello", 32, 500, true, 1.0f, UIHAlign::Left);
    REQUIRE(one);
    CHECK(one.value().lines == 1);
    CHECK(one.value().glyphs.size() == 5);
    CHECK(one.value().size.y == doctest::Approx(32));
    CHECK(one.value().size.x > 50);
    CHECK(one.value().size.x < 120);

    auto two = fonts.layout("assets://fonts/Inter-Regular.ttf", "Hello\nWorld", 32, 500, true, 1.5f, UIHAlign::Left);
    REQUIRE(two);
    CHECK(two.value().lines == 2);
    CHECK(two.value().size.y == doctest::Approx(96));

    const std::string para = "The quick brown fox jumps over the lazy dog again and again";
    auto wrapped = fonts.layout("assets://fonts/Inter-Regular.ttf", para, 24, 200, true, 1.0f, UIHAlign::Left);
    REQUIRE(wrapped);
    CHECK(wrapped.value().lines >= 3);
    for (const TextGlyph& g : wrapped.value().glyphs) CHECK(g.max.x <= 201.0f);
    auto unwrapped = fonts.layout("assets://fonts/Inter-Regular.ttf", para, 24, 200, false, 1.0f, UIHAlign::Left);
    CHECK(unwrapped.value().lines == 1);
    auto longWord = fonts.layout("assets://fonts/Inter-Regular.ttf", "Supercalifragilisticexpialidocious", 24, 100, true, 1.0f, UIHAlign::Left);
    CHECK(longWord.value().lines >= 3);

    auto centered = fonts.layout("assets://fonts/Inter-Regular.ttf", "Hi", 32, 400, true, 1.0f, UIHAlign::Center);
    auto left = fonts.layout("assets://fonts/Inter-Regular.ttf", "Hi", 32, 400, true, 1.0f, UIHAlign::Left);
    const float shift = centered.value().glyphs[0].min.x - left.value().glyphs[0].min.x;
    CHECK(shift == doctest::Approx((400 - left.value().size.x) / 2).epsilon(0.02));

    auto accents = fonts.layout("assets://fonts/Inter-Regular.ttf", "été à Noël", 32, 0, false, 1.0f, UIHAlign::Left);
    CHECK(accents.value().glyphs.size() == 8);  // spaces have no bitmap
    CHECK(fonts.stats().glyphs > 10);

    auto missing = fonts.layout("assets://fonts/none.ttf", "x", 32, 0, false, 1.0f, UIHAlign::Left);
    CHECK_FALSE(missing);
    REQUIRE(f.vfs.writeText("assets://fonts/bad.ttf", "not a font"));
    CHECK_FALSE(fonts.layout("assets://fonts/bad.ttf", "x", 32, 0, false, 1.0f, UIHAlign::Left));

    // Overlay: a panel (1 quad), a text (5 glyphs), a slider (3 quads); a 9-slice needs the texture size.
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto panel = f.element("Panel", f.canvas, {0, 0}, {1, 1}, {0.5f, 0.5f}, {0, 0}, {0, 0});
    f.scene.registry().emplace<UIImageComponent>(panel).color = {0.1f, 0.1f, 0.1f, 0.8f};
    const auto label = f.element("Label", panel, {0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {300, 60});
    auto& text = f.scene.registry().emplace<UITextComponent>(label);
    text.text = "Score";
    f.scene.registry().emplace<UISliderComponent>(f.element("S", panel, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {100, 20}));
    OverlayDrawList list;
    f.ui.buildOverlay(f.scene, 800, 600, list);
    CHECK(f.ui.stats().quads == 1 + 5 + 3);
    CHECK(f.ui.stats().texts == 1);
    // Glyphs are centered in the label: inside 250..550 x 270..330.
    for (usize q = 1; q < 6; ++q) {
        const OverlayVertex& v = list.vertices[q * 4];
        CHECK(v.position.x >= 250.0f);
        CHECK(v.position.x <= 550.0f);
        CHECK(v.position.y >= 270.0f);
        CHECK(v.position.y <= 330.0f);
    }
    // A broken font is reported but does not stop the rest of the UI.
    text.font = "assets://fonts/bad.ttf";
    list.clear();
    f.ui.buildOverlay(f.scene, 800, 600, list);
    CHECK(f.ui.stats().quads == 1 + 3);
}

TEST_CASE("canvas sort order decides what is on top") {
    UIFixture f;
    auto& c1 = f.scene.registry().get<UICanvasComponent>(f.canvas);
    c1.scaleMode = UIScaleMode::ConstantPixelSize;
    const auto under = f.button("Under", {100, 100}, {100, 100});
    const auto top = f.scene.createEntity("Top canvas");
    auto& c2 = f.scene.registry().emplace<UICanvasComponent>(top);
    c2.scaleMode = UIScaleMode::ConstantPixelSize;
    c2.sortOrder = -1;
    const auto over = f.element("Over", top, {0, 0}, {0, 0}, {0.5f, 0.5f}, {100, 100}, {100, 100});
    f.scene.registry().emplace<UIImageComponent>(over);
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.ui.hitTest({100, 100}) == under);  // the second canvas is drawn first
    c2.sortOrder = 5;
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.ui.hitTest({100, 100}) == over);
}

}  // TEST_SUITE
