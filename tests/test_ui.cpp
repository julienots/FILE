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

TEST_CASE("an icon or a label inside a button forwards the click to the button") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto card = f.button("Card", {200, 200}, {200, 300});
    const auto icon = f.element("Icon", card, {0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {120, 120});
    f.scene.registry().emplace<UIImageComponent>(icon);  // an image drawn over the button: hit first
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {200, 200}, false}});
    CHECK(f.ui.isHovered(card));
    f.pointers({{0, {200, 200}, true}});
    CHECK(f.ui.isPressed(card));
    f.pointers({{0, {205, 205}, false}});
    const auto events = f.ui.takeEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].entity == card);
    // A plain image outside any button still blocks the game behind without clicking anything.
    const auto panel = f.element("Panel", f.canvas, {0, 0}, {0, 0}, {0.5f, 0.5f}, {600, 400}, {100, 100});
    f.scene.registry().emplace<UIImageComponent>(panel);
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {600, 400}, true}});
    CHECK(f.ui.pointerOverUI());
    f.pointers({{0, {600, 400}, false}});
    CHECK(f.ui.takeEvents().empty());
    // ... unless it lets the pointer through (overlays): a covered button is clicked.
    const auto veil = f.element("Veil", f.canvas, {0, 0}, {1, 1}, {0.5f, 0.5f}, {0, 0}, {0, 0});
    f.scene.registry().emplace<UIImageComponent>(veil).blocksPointer = false;
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {200, 200}, true}});
    f.pointers({{0, {200, 200}, false}});
    const auto through = f.ui.takeEvents();
    REQUIRE(through.size() == 1);
    CHECK(through[0].entity == card);
    f.pointers({{0, {700, 50}, true}});
    CHECK_FALSE(f.ui.pointerOverUI());
    f.pointers({{0, {700, 50}, false}});
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
    REQUIRE(accents.value().glyphs.size() == 10);  // spaces are kept, with no size (underlines span them)
    usize empty = 0;
    for (const TextGlyph& g : accents.value().glyphs) empty += g.max.x == g.min.x ? 1 : 0;
    CHECK(empty == 2);
    CHECK(accents.value().glyphs[3].source == 3);
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

TEST_CASE("safe area: canvases keep their children away from notches") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto top = f.element("Top bar", f.canvas, {0, 0}, {1, 0}, {0.5f, 0}, {0, 0}, {0, 100});
    const auto corner = f.element("Pause", f.canvas, {1, 1}, {1, 1}, {1, 1}, {0, 0}, {80, 80});
    f.ui.setSafeArea({0, 120, 30, 60});  // notch at the top, rounded corner, gesture bar
    f.ui.layout(f.scene, 1080, 2400);
    checkRect(f.rect(top), {0, 120, 1050, 100});
    checkRect(f.rect(corner), {1050 - 80, 2400 - 60 - 80, 80, 80});
    f.scene.registry().get<UICanvasComponent>(f.canvas).respectSafeArea = false;  // full-screen backgrounds
    f.ui.layout(f.scene, 1080, 2400);
    checkRect(f.rect(top), {0, 0, 1080, 100});
}


TEST_CASE("toggles: check boxes flip, radio groups keep one on, disabled ones ignore clicks") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    auto toggle = [&](const char* name, math::Vec2 at, const char* group) {
        const auto e = f.element(name, f.canvas, {0, 0}, {0, 0}, {0.5f, 0.5f}, at, {200, 40});
        f.scene.registry().emplace<UIToggleComponent>(e).group = group;
        return e;
    };
    const auto music = toggle("Music", {150, 50}, "");
    const auto easy = toggle("Easy", {150, 150}, "difficulty");
    const auto hard = toggle("Hard", {150, 250}, "difficulty");
    f.scene.registry().get<UIToggleComponent>(easy).isOn = true;
    f.ui.layout(f.scene, 800, 600);
    auto click = [&](math::Vec2 p) {
        f.pointers({{0, p, true}});
        f.pointers({{0, p, false}});
    };
    click({150, 50});
    CHECK(f.scene.registry().get<UIToggleComponent>(music).isOn);
    auto ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].type == UIEventType::ValueChanged);
    CHECK(ev[0].value == 1.0f);
    click({150, 50});
    CHECK_FALSE(f.scene.registry().get<UIToggleComponent>(music).isOn);
    f.ui.takeEvents();
    click({150, 250});  // radio: hard on, easy off
    CHECK(f.scene.registry().get<UIToggleComponent>(hard).isOn);
    CHECK_FALSE(f.scene.registry().get<UIToggleComponent>(easy).isOn);
    CHECK(f.ui.takeEvents().size() == 2);
    click({150, 250});  // clicking the chosen radio button keeps it on
    CHECK(f.scene.registry().get<UIToggleComponent>(hard).isOn);
    f.scene.registry().get<UIToggleComponent>(music).interactable = false;
    click({150, 50});
    CHECK_FALSE(f.scene.registry().get<UIToggleComponent>(music).isOn);
}

TEST_CASE("text fields: focus, typing, UTF-8 backspace, max length, numeric, password, submit, blur") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto field = f.element("Name", f.canvas, {0, 0}, {0, 0}, {0.5f, 0.5f}, {200, 50}, {300, 50});
    auto& fc = f.scene.registry().emplace<UIInputFieldComponent>(field);
    fc.maxLength = 6;
    fc.placeholder = "Votre nom";
    f.scene.registry().emplace<UITextComponent>(field);
    int keyboard = 0;
    f.ui.setTextInputHandler([&](bool on) { keyboard += on ? 1 : -1; });
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.scene.registry().get<UITextComponent>(field).text == "Votre nom");  // placeholder shown
    f.ui.processText(f.scene, "ignored", 0, false);  // not focused yet
    CHECK(f.scene.registry().get<UIInputFieldComponent>(field).text.empty());
    f.pointers({{0, {200, 50}, true}});
    f.pointers({{0, {200, 50}, false}});
    CHECK(f.ui.focusedField() == field);
    CHECK(keyboard == 1);  // on-screen keyboard requested
    f.ui.processText(f.scene, "Zoé", 0, false);
    CHECK(f.scene.registry().get<UIInputFieldComponent>(field).text == "Zoé");
    f.ui.processText(f.scene, "", 1, false);  // removes the whole "é" (2 bytes)
    CHECK(f.scene.registry().get<UIInputFieldComponent>(field).text == "Zo");
    f.ui.processText(f.scene, "abcdefgh\x01", 0, false);  // capped at 6 characters, control char dropped
    CHECK(f.scene.registry().get<UIInputFieldComponent>(field).text == "Zoabcd");
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.scene.registry().get<UITextComponent>(field).text == "Zoabcd");
    auto ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 3);
    CHECK(ev.back().type == UIEventType::TextChanged);
    CHECK(ev.back().text == "Zoabcd");
    f.ui.processText(f.scene, "", 0, true);  // Enter: submit and blur
    ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].type == UIEventType::Submit);
    CHECK(ev[0].text == "Zoabcd");
    CHECK(f.ui.focusedField().isNull());
    CHECK(keyboard == 0);  // keyboard hidden again
    // Numeric and password fields.
    auto& fc2 = f.scene.registry().get<UIInputFieldComponent>(field);
    fc2.text.clear();
    fc2.numeric = true;
    fc2.password = true;
    f.ui.focusField(field);
    f.ui.processText(f.scene, "4a2-.x", 0, false);
    CHECK(fc2.text == "42-.");
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.scene.registry().get<UITextComponent>(field).text == "****");
    // A press elsewhere removes the focus.
    f.pointers({{0, {700, 500}, true}});
    f.pointers({{0, {700, 500}, false}});
    CHECK(f.ui.focusedField().isNull());
}

TEST_CASE("scroll views: content is moved and clipped, wheel and drag scroll, a drag cancels the click, limits") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto view = f.element("List", f.canvas, {0, 0}, {0, 0}, {0, 0}, {100, 100}, {300, 200});
    f.scene.registry().emplace<UIScrollViewComponent>(view).wheelSpeed = 50.0f;
    auto& lay = f.scene.registry().emplace<UILayoutComponent>(view);
    lay.spacing = 0.0f;
    lay.padding = {0, 0, 0, 0};
    std::vector<ecs::Entity> items;
    for (int i = 0; i < 10; ++i) {  // 10 x 60 = 600 px of content in 200 px
        const auto b = f.element(("Item " + std::to_string(i)).c_str(), view, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {300, 60});
        f.scene.registry().emplace<UIImageComponent>(b);
        f.scene.registry().emplace<UIButtonComponent>(b);
        items.push_back(b);
    }
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.rect(items[3]).y == doctest::Approx(280.0f));
    // Item 5 (y 400..460) is laid out below the view: clipped, not clickable.
    CHECK(f.ui.hitTest({250, 430}).isNull());
    CHECK(f.ui.hitTest({250, 130}) == items[0]);
    // Wheel down 2 notches: 100 px.
    CHECK(f.ui.scrollAt(f.scene, {250, 150}, {0, -2}));
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.scene.registry().get<UIScrollViewComponent>(view).scroll.y == doctest::Approx(100.0f));
    CHECK(f.rect(items[0]).y == doctest::Approx(0.0f));
    CHECK(f.ui.hitTest({250, 130}) == items[2]);  // items move up by 100 px: item 2 spans 120..180
    CHECK(f.ui.hitTest({250, 110}) == items[1]);  // item 1 partly visible (100..120)
    CHECK_FALSE(f.ui.scrollAt(f.scene, {700, 500}, {0, -1}));  // outside: nothing
    // Drag up by 150 px with a finger that started on a button: scrolls, no click.
    f.pointers({{7, {250, 250}, true}});
    f.pointers({{7, {250, 200}, true}});
    f.pointers({{7, {250, 100}, true}});
    f.pointers({{7, {250, 100}, false}});
    CHECK(f.ui.takeEvents().empty());
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.scene.registry().get<UIScrollViewComponent>(view).scroll.y == doctest::Approx(250.0f));
    // Limits: content 600 - view 200 = 400 at most, 0 at least.
    f.ui.scrollAt(f.scene, {250, 150}, {0, -100});
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.scene.registry().get<UIScrollViewComponent>(view).scroll.y == doctest::Approx(400.0f));
    f.ui.scrollAt(f.scene, {250, 150}, {0, 100});
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.scene.registry().get<UIScrollViewComponent>(view).scroll.y == doctest::Approx(0.0f));
    // A short tap still clicks.
    f.pointers({{8, {250, 130}, true}});
    f.pointers({{8, {253, 132}, false}});
    const auto ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].entity == items[0]);
    // Drawing is clipped to the view: no quad outside its rectangle.
    OverlayDrawList out;
    f.ui.buildOverlay(f.scene, 800, 600, out);
    REQUIRE(!out.vertices.empty());
    for (const auto& v : out.vertices) {
        CHECK(v.position.y >= 100.0f - 0.01f);
        CHECK(v.position.y <= 300.0f + 0.01f);
    }
}

TEST_CASE("keyboard / gamepad navigation: opt-in per canvas, moves by direction, activates, adjusts sliders") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto play = f.button("Play", {400, 100}, {200, 60});
    const auto options = f.button("Options", {400, 200}, {200, 60});
    const auto quit = f.button("Quit", {400, 300}, {200, 60});
    const auto volume = f.element("Volume", f.canvas, {0, 0}, {0, 0}, {0.5f, 0.5f}, {700, 200}, {200, 40});
    f.scene.registry().emplace<UISliderComponent>(volume).value = 0.5f;
    f.ui.layout(f.scene, 800, 600);
    CHECK_FALSE(f.ui.navigationEnabled());
    f.scene.registry().get<UICanvasComponent>(f.canvas).keyboardNavigation = true;
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.ui.navigationEnabled());
    f.ui.navigate(f.scene, {0, 1});  // first press: the top-left control
    CHECK(f.ui.navFocus() == play);
    f.ui.navigate(f.scene, {0, 1});
    CHECK(f.ui.navFocus() == options);
    f.ui.navigate(f.scene, {0, 1});
    CHECK(f.ui.navFocus() == quit);
    f.ui.navigate(f.scene, {0, 1});  // nothing below: stays
    CHECK(f.ui.navFocus() == quit);
    f.ui.navigate(f.scene, {0, -1});
    f.ui.navigate(f.scene, {1, 0});  // to the right of Options: the slider
    CHECK(f.ui.navFocus() == volume);
    f.ui.navAdjust(f.scene, 1.0f);
    CHECK(f.scene.registry().get<UISliderComponent>(volume).value == doctest::Approx(0.6f));
    f.ui.takeEvents();
    f.ui.navigate(f.scene, {-1, 0});
    f.ui.navActivate(f.scene);
    const auto ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].entity == options);
    // The highlight is drawn (an extra outline) and a pointer press hides it.
    f.pointers({{0, {50, 550}, true}});
    f.pointers({{0, {50, 550}, false}});
    CHECK(f.ui.navFocus().isNull());
}
TEST_CASE("progress bars, vertical sliders, rich text") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto bar = f.element("Bar", f.canvas, {0, 0}, {0, 0}, {0, 0}, {100, 100}, {200, 20});
    auto& pb = f.scene.registry().emplace<UIProgressBarComponent>(bar);
    pb.value = 0.25f;
    pb.inset = 0.0f;
    OverlayDrawList list;
    f.ui.buildOverlay(f.scene, 800, 600, list);
    REQUIRE(f.ui.stats().quads == 2);  // background + fill
    CHECK(list.vertices[4].position.x == doctest::Approx(100.0f));
    CHECK(list.vertices[6].position.x == doctest::Approx(150.0f));  // a quarter of 200
    pb.direction = UIFillDirection::BottomToTop;
    list.clear();
    f.ui.buildOverlay(f.scene, 800, 600, list);
    CHECK(list.vertices[4].position.y == doctest::Approx(115.0f));  // the bottom quarter of 100..120
    pb.value = 0.0f;
    list.clear();
    f.ui.buildOverlay(f.scene, 800, 600, list);
    CHECK(f.ui.stats().quads == 1);

    // Vertical slider: bottom = min, top = max (half a handle at each end: usable 110..290 for 100..300).
    const auto vs = f.element("V", f.canvas, {0, 0}, {0, 0}, {0, 0}, {400, 100}, {20, 200});
    auto& slider = f.scene.registry().emplace<UISliderComponent>(vs);
    slider.vertical = true;
    slider.minValue = 0;
    slider.maxValue = 10;
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {410, 290}, true}});
    CHECK(slider.value == doctest::Approx(0.0f));
    f.pointers({{0, {410, 200}, true}});
    CHECK(slider.value == doctest::Approx(5.0f));
    f.pointers({{0, {410, 0}, true}});
    CHECK(slider.value == doctest::Approx(10.0f));
    f.pointers({{0, {410, 0}, false}});

    // Rich text: tags removed, colored, bold doubles the glyph, underline adds a bar.
    f.scene.destroyEntity(bar);
    f.scene.destroyEntity(vs);
    f.scene.flushDestroyed();
    const auto label = f.element("Label", f.canvas, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {600, 60});
    auto& t = f.scene.registry().emplace<UITextComponent>(label);
    t.text = "ab[color=#ff0000]cd[/color]";
    t.richText = true;
    t.hAlign = UIHAlign::Left;
    list.clear();
    f.ui.buildOverlay(f.scene, 800, 600, list);
    REQUIRE(f.ui.stats().quads == 4);
    CHECK(list.vertices[0].color == list.vertices[4].color);  // a, b: the text color
    CHECK(list.vertices[8].color == packColor({1, 0, 0, 1}));  // c, d: red
    CHECK(list.vertices[12].color == packColor({1, 0, 0, 1}));
    t.text = "[b]ab[/b] [u]c[/u] [[x] [nope]";
    list.clear();
    f.ui.buildOverlay(f.scene, 800, 600, list);
    // a, b twice (bold); c + its underline; "[x]" and "[nope]" stay as text (unknown tag / escaped bracket).
    CHECK(f.ui.stats().quads == 4 + 2 + 3 + 6);
}

TEST_CASE("drop-down lists, tabs and tooltips") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto dd = f.element("Quality", f.canvas, {0, 0}, {0, 0}, {0, 0}, {100, 100}, {200, 40});
    auto& d = f.scene.registry().emplace<UIDropdownComponent>(dd);
    d.options = "Low;Medium;High";
    d.selected = 1;
    const auto behind = f.button("Behind", {200, 170}, {200, 40});  // under the list
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {150, 120}, true}});
    f.pointers({{0, {150, 120}, false}});
    CHECK(f.ui.openDropdown() == dd);
    OverlayDrawList list;
    f.ui.buildOverlay(f.scene, 800, 600, list);
    // The list (3 x 40 under the box) is drawn last, on top of the button behind it.
    CHECK(list.vertices[list.vertices.size() - 4].position.y >= 140.0f);
    f.pointers({{0, {150, 230}, false}});  // hover "High"
    f.pointers({{0, {150, 230}, true}});
    f.pointers({{0, {150, 230}, false}});
    CHECK(d.selected == 2);
    CHECK(f.ui.openDropdown().isNull());
    auto events = f.ui.takeEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == UIEventType::ValueChanged);
    CHECK(events[0].value == doctest::Approx(2.0f));
    // Open, then click elsewhere: closes without clicking the button behind.
    f.pointers({{0, {150, 120}, true}});
    f.pointers({{0, {150, 120}, false}});
    REQUIRE(f.ui.openDropdown() == dd);
    f.pointers({{0, {600, 500}, true}});
    f.pointers({{0, {600, 500}, false}});
    CHECK(f.ui.openDropdown().isNull());
    CHECK(f.ui.takeEvents().empty());
    (void)behind;

    // Tabs: only the current page is laid out, under the header; a click on a tab switches.
    const auto tabs = f.element("Tabs", f.canvas, {0, 0}, {0, 0}, {0, 0}, {400, 0}, {300, 300});
    auto& tb = f.scene.registry().emplace<UITabsComponent>(tabs);
    tb.tabs = "A;B;C";
    tb.headerHeight = 50.0f;
    std::vector<ecs::Entity> pages;
    for (int i = 0; i < 3; ++i) pages.push_back(f.element("Page", tabs, {0, 0}, {1, 1}, {0.5f, 0.5f}, {0, 0}, {0, 0}));
    f.ui.layout(f.scene, 800, 600);
    checkRect(f.rect(pages[0]), {400, 50, 300, 250});
    CHECK(f.ui.elementAt({550, 200}) == pages[0]);
    f.pointers({{0, {650, 20}, true}});  // third tab
    f.pointers({{0, {650, 20}, false}});
    CHECK(tb.current == 2);
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.ui.elementAt({550, 200}) == pages[2]);
    events = f.ui.takeEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].value == doctest::Approx(2.0f));
    f.pointers({{0, {650, 200}, true}});  // a click in the page does not switch tabs
    f.pointers({{0, {650, 200}, false}});
    CHECK(tb.current == 2);

    // Tooltip: shown after the delay while the pointer rests on the element.
    const auto help = f.button("Help", {100, 400}, {80, 40});
    auto& tip = f.scene.registry().emplace<UITooltipComponent>(help);
    tip.delay = 0.0f;
    tip.text = "Aide";
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {100, 400}, false}});
    f.pointers({{0, {100, 400}, false}});
    CHECK(f.ui.tooltipShown() == help);
    list.clear();
    f.ui.buildOverlay(f.scene, 800, 600, list);
    CHECK(f.ui.stats().quads > 0);
    f.pointers({{0, {700, 550}, false}});
    CHECK(f.ui.tooltipShown().isNull());
}

TEST_CASE("themes recolor the controls of a canvas") {
    UIFixture f;
    UITheme theme;
    theme.accent = {0, 1, 0, 1};
    theme.track = {0, 0, 1, 1};
    auto parsed = UITheme::fromJson(theme.toJson());
    REQUIRE(parsed);
    CHECK(parsed.value().accent.y == doctest::Approx(1.0f));
    CHECK_FALSE(UITheme::fromJson("{\"accent\": [1]}"));
    REQUIRE(f.vfs.writeText("assets://t.agrtheme", theme.toJson()));
    auto& c = f.scene.registry().get<UICanvasComponent>(f.canvas);
    c.scaleMode = UIScaleMode::ConstantPixelSize;
    const auto bar = f.element("Bar", f.canvas, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {100, 20});
    f.scene.registry().emplace<UIProgressBarComponent>(bar).value = 1.0f;
    OverlayDrawList list;
    f.ui.buildOverlay(f.scene, 800, 600, list);
    CHECK(list.vertices[4].color == packColor(UIProgressBarComponent{}.fillColor));
    c.theme = "assets://t.agrtheme";
    list.clear();
    f.ui.buildOverlay(f.scene, 800, 600, list);
    CHECK(list.vertices[0].color == packColor({0, 0, 1, 1}));  // track
    CHECK(list.vertices[4].color == packColor({0, 1, 0, 1}));  // accent
    CHECK(f.ui.theme("assets://none.agrtheme") == nullptr);
}


TEST_CASE("item lists and trees: rows, selection, double click, folding, scrolling") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto e = f.element("List", f.canvas, {0, 0}, {0, 0}, {0, 0}, {100, 100}, {300, 200});
    auto& l = f.scene.registry().emplace<UIItemListComponent>(e);
    l.items = "Fruits;>Apple;>Pear;>>Williams;Vegetables;>Leek;Cereals;Nuts;Rice";
    l.tree = true;
    l.itemHeight = 40;
    f.ui.layout(f.scene, 800, 600);
    // Click on "Apple" (row 1): selects item 1.
    f.pointers({{0, {250, 160}, true}});
    f.pointers({{0, {250, 160}, false}});
    CHECK(l.selected == 1);
    auto ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].type == UIEventType::ValueChanged);
    CHECK(ev[0].value == doctest::Approx(1.0f));
    // Double click: activates it.
    f.pointers({{0, {250, 160}, true}});
    f.pointers({{0, {250, 160}, false}});
    ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].type == UIEventType::Submit);
    CHECK(ev[0].text == "Apple");
    // The arrow of "Fruits" folds it: Vegetables moves up to row 1.
    f.pointers({{0, {110, 120}, true}});
    f.pointers({{0, {110, 120}, false}});
    CHECK(l.collapsed == "0");
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {250, 160}, true}});
    f.pointers({{0, {250, 160}, false}});
    CHECK(l.selected == 4);  // Vegetables
    // Unfold; 9 rows of 40 in 200 px: the wheel scrolls, clamped at the end.
    f.pointers({{0, {110, 120}, true}});
    f.pointers({{0, {110, 120}, false}});
    CHECK(l.collapsed.empty());
    f.ui.layout(f.scene, 800, 600);
    CHECK(f.ui.scrollAt(f.scene, {250, 200}, {0, -10}));
    f.ui.layout(f.scene, 800, 600);
    CHECK(l.scroll == doctest::Approx(9 * 40 - 200));
    OverlayDrawList list;
    f.ui.buildOverlay(f.scene, 800, 600, list);
    CHECK(f.ui.stats().texts >= 5);  // the rows in view, clipped
}

TEST_CASE("modal dialogs block the UI behind and report the button") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto behind = f.button("Behind", {400, 300}, {800, 600});
    const auto dlg = f.scene.createEntity("Quit?", f.canvas);
    auto& d = f.scene.registry().emplace<UIDialogComponent>(dlg);
    d.title = "Quitter ?";
    d.buttons = "Oui;Non";
    d.size = {400, 200};
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {400, 300}, true}});
    f.pointers({{0, {400, 300}, false}});
    auto ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].entity == behind);  // closed: the button works
    d.open = true;
    f.ui.layout(f.scene, 800, 600);
    OverlayDrawList list;
    f.ui.buildOverlay(f.scene, 800, 600, list);
    f.pointers({{0, {400, 300}, true}});  // the panel's middle: blocked
    f.pointers({{0, {400, 300}, false}});
    CHECK(f.ui.takeEvents().empty());
    // Buttons at the bottom right of the 400 x 200 panel centered in 800 x 600 (x 200..600, y 200..400).
    f.pointers({{0, {570, 360}, true}});  // "Non"
    f.pointers({{0, {570, 360}, false}});
    ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].entity == dlg);
    CHECK(ev[0].value == doctest::Approx(1.0f));
    CHECK_FALSE(d.open);
    CHECK(d.result == 1);
}

TEST_CASE("color picker, multiline fields, scaled and rotated elements") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto e = f.element("Picker", f.canvas, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {300, 200});
    auto& c = f.scene.registry().emplace<UIColorPickerComponent>(e);
    c.color = {1, 0, 0, 1};
    f.ui.layout(f.scene, 800, 600);
    // Square 200 x 200: top-right corner = full saturation and value of the hue (red).
    f.pointers({{0, {199, 1}, true}});
    f.pointers({{0, {199, 1}, false}});
    CHECK(c.color.x == doctest::Approx(1.0f).epsilon(0.02));
    CHECK(c.color.y < 0.02f);
    // Bottom-left: black.
    f.pointers({{0, {1, 199}, true}});
    CHECK(c.color.x < 0.02f);
    // Drag to the middle-left: gray of value 0.5.
    f.pointers({{0, {0, 100}, true}});
    f.pointers({{0, {0, 100}, false}});
    CHECK(c.color.x == doctest::Approx(0.5f).epsilon(0.02));
    CHECK(c.color.y == doctest::Approx(0.5f).epsilon(0.02));
    // Hue bar (x 212..236): a third of the way down = green; the square keeps saturation / value.
    f.pointers({{0, {199, 1}, true}});
    f.pointers({{0, {199, 1}, false}});
    f.pointers({{0, {220, 200.0f / 3.0f}, true}});
    f.pointers({{0, {220, 200.0f / 3.0f}, false}});
    CHECK(c.color.y == doctest::Approx(1.0f).epsilon(0.03));
    CHECK(c.color.x < 0.05f);
    // Alpha bar (x 248..272): half way = alpha 0.5.
    f.pointers({{0, {260, 100}, true}});
    f.pointers({{0, {260, 100}, false}});
    CHECK(c.color.w == doctest::Approx(0.5f).epsilon(0.02));
    CHECK_FALSE(f.ui.takeEvents().empty());

    // Multiline field: Enter adds a line instead of submitting.
    const auto field = f.element("Notes", f.canvas, {0, 0}, {0, 0}, {0, 0}, {400, 0}, {300, 200});
    auto& in = f.scene.registry().emplace<UIInputFieldComponent>(field);
    in.multiline = true;
    f.scene.registry().emplace<UITextComponent>(field);
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {500, 100}, true}});
    f.pointers({{0, {500, 100}, false}});
    f.ui.processText(f.scene, "abc", 0, true);
    f.ui.processText(f.scene, "de", 0, false);
    CHECK(in.text == "abc\nde");
    for (const auto& ev : f.ui.takeEvents()) CHECK(ev.type != UIEventType::Submit);

    // Scale around the pivot (children too) and rotation (hits follow it).
    const auto box = f.button("Box", {400, 450}, {200, 100});
    auto& r = f.scene.registry().get<UIRectComponent>(box);
    r.scale = {0.5f, 0.5f};
    f.ui.layout(f.scene, 800, 600);
    checkRect(f.rect(box), {350, 425, 100, 50});
    f.pointers({{0, {460, 450}, true}});  // outside the scaled box
    f.pointers({{0, {460, 450}, false}});
    CHECK(f.ui.takeEvents().empty());
    r.scale = {1, 1};
    r.rotation = math::radians(90.0f);  // 200 x 100 turned: now 100 wide, 200 tall
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {400, 530}, true}});  // below the unrotated box, inside the rotated one
    f.pointers({{0, {400, 530}, false}});
    auto evs = f.ui.takeEvents();
    REQUIRE(evs.size() == 1);
    CHECK(evs[0].entity == box);
    f.pointers({{0, {490, 450}, true}});  // inside the unrotated box, outside the rotated one
    f.pointers({{0, {490, 450}, false}});
    CHECK(f.ui.takeEvents().empty());
    OverlayDrawList list;
    f.ui.buildOverlay(f.scene, 800, 600, list);
    CHECK_FALSE(list.vertices.empty());
}

TEST_CASE("text editing: caret keys, selection, word jumps, clipboard, click and drag to select") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto field = f.element("Name", f.canvas, {0, 0}, {0, 0}, {0, 0}, {100, 100}, {400, 50});
    auto& fc = f.scene.registry().emplace<UIInputFieldComponent>(field);
    fc.maxLength = 0;
    auto& tc = f.scene.registry().emplace<UITextComponent>(field);
    tc.hAlign = UIHAlign::Left;
    tc.wrap = false;
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {300, 125}, true}});
    f.pointers({{0, {300, 125}, false}});
    REQUIRE(f.ui.focusedField() == field);
    using TK = UISystem::TextKey;
    auto text = [&] { return f.scene.registry().get<UIInputFieldComponent>(field).text; };
    f.ui.processText(f.scene, "hello world", 0, false);
    CHECK(f.ui.caret() == 11);
    f.ui.processTextKey(f.scene, TK::Left, false, true);  // word left: before "world"
    CHECK(f.ui.caret() == 6);
    f.ui.processText(f.scene, "big ", 0, false);
    CHECK(text() == "hello big world");
    f.ui.processTextKey(f.scene, TK::Home);
    f.ui.processTextKey(f.scene, TK::Right, true);  // shift: select "he"
    f.ui.processTextKey(f.scene, TK::Right, true);
    CHECK(f.ui.selectionAnchor() == 0);
    CHECK(f.ui.caret() == 2);
    f.ui.processText(f.scene, "J", 0, false);  // typing replaces the selection
    CHECK(text() == "Jllo big world");
    f.ui.processTextKey(f.scene, TK::Delete);
    CHECK(text() == "Jlo big world");
    f.ui.processTextKey(f.scene, TK::End);
    f.ui.processText(f.scene, "", 2, false);  // backspaces at the caret
    CHECK(text() == "Jlo big wor");
    // Select all, copy, paste at the end (internal clipboard when there is no system one).
    f.ui.processTextKey(f.scene, TK::SelectAll);
    f.ui.processTextKey(f.scene, TK::Copy);
    f.ui.processTextKey(f.scene, TK::End);
    f.ui.processText(f.scene, " ", 0, false);
    f.ui.processTextKey(f.scene, TK::Paste);
    CHECK(text() == "Jlo big wor Jlo big wor");
    f.ui.processTextKey(f.scene, TK::SelectAll);
    f.ui.processTextKey(f.scene, TK::Cut);
    CHECK(text().empty());
    std::string sys;
    f.ui.setClipboard([&] { return std::string("from system"); }, [&](const std::string& s) { sys = s; });
    f.ui.processTextKey(f.scene, TK::Paste);
    CHECK(text() == "from system");
    f.ui.processTextKey(f.scene, TK::Left, true, true);
    f.ui.processTextKey(f.scene, TK::Copy);
    CHECK(sys == "system");

    // Clicks place the caret between the characters drawn; a drag selects.
    f.ui.processTextKey(f.scene, TK::End);
    f.ui.layout(f.scene, 800, 600);
    OverlayDrawList list;
    f.ui.buildOverlay(f.scene, 800, 600, list);
    f.pointers({{0, {101, 125}, true}});
    CHECK(f.ui.caret() == 0);
    f.pointers({{0, {499, 125}, true}});  // drag to the far right: everything selected
    f.pointers({{0, {499, 125}, false}});
    CHECK(f.ui.selectionAnchor() == 0);
    CHECK(f.ui.caret() == 11);
    f.ui.buildOverlay(f.scene, 800, 600, list);  // selection drawn under the text
    f.pointers({{0, {101, 125}, true}});
    f.pointers({{0, {101, 125}, false}});
    CHECK(f.ui.caret() == 0);
    CHECK(f.ui.selectionAnchor() == 0);

    // Multiline: Home / End per line, Up / Down by lines.
    auto& m = f.scene.registry().get<UIInputFieldComponent>(field);
    m.multiline = true;
    m.text.clear();
    f.ui.processText(f.scene, "abc", 0, true);
    f.ui.processText(f.scene, "defgh", 0, false);
    CHECK(text() == "abc\ndefgh");
    f.ui.processTextKey(f.scene, TK::Home);
    CHECK(f.ui.caret() == 4);
    f.ui.processTextKey(f.scene, TK::End);
    CHECK(f.ui.caret() == 9);
    f.ui.setCaret(6, 6);
    f.ui.processTextKey(f.scene, TK::Up);  // stops are from the single-line drawing: by text lines
    f.ui.processTextKey(f.scene, TK::Down);
    CHECK(f.ui.caret() >= 4);
}

TEST_CASE("rich text: italic, strike-through and clickable links") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto label = f.element("Label", f.canvas, {0, 0}, {0, 0}, {0, 0}, {100, 100}, {600, 60});
    auto& t = f.scene.registry().emplace<UITextComponent>(label);
    t.richText = true;
    t.hAlign = UIHAlign::Left;
    t.wrap = false;
    t.text = "Plain";
    OverlayDrawList plain;
    f.ui.buildOverlay(f.scene, 800, 600, plain);
    const usize plainQuads = plain.indices.size() / 6;
    f.scene.registry().get<UITextComponent>(label).text = "[i]Plain[/i]";
    OverlayDrawList italic;
    f.ui.buildOverlay(f.scene, 800, 600, italic);
    CHECK(italic.indices.size() / 6 == plainQuads);  // same glyphs, sheared
    bool sheared = false;
    for (usize q = 0; q + 3 < italic.vertices.size(); q += 4)
        sheared |= std::fabs(italic.vertices[q].position.x - italic.vertices[q + 3].position.x) > 0.5f;  // top-left vs bottom-left
    CHECK(sheared);
    f.scene.registry().get<UITextComponent>(label).text = "[s]Plain[/s]";
    OverlayDrawList strike;
    f.ui.buildOverlay(f.scene, 800, 600, strike);
    CHECK(strike.indices.size() / 6 == plainQuads * 2);  // a line per glyph
    CHECK(f.ui.linkBoxes().empty());

    f.scene.registry().get<UITextComponent>(label).text = "Go to the [url=shop]shop[/url] or [url=map]map[/url]";
    OverlayDrawList links;
    f.ui.buildOverlay(f.scene, 800, 600, links);
    REQUIRE(f.ui.linkBoxes().size() == 2);
    CHECK(f.ui.linkBoxes()[0].id == "shop");
    const math::Vec4 r = f.ui.linkBoxes()[1].rect;
    CHECK(r.x > f.ui.linkBoxes()[0].rect.x);
    const math::Vec2 c{r.x + r.z * 0.5f, r.y + r.w * 0.5f};
    f.ui.takeEvents();
    f.pointers({{0, c, true}});
    f.pointers({{0, c, false}});
    const auto ev = f.ui.takeEvents();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].type == UIEventType::Link);
    CHECK(ev[0].text == "map");
    CHECK(ev[0].entity == label);
    f.pointers({{0, {110, 50}, true}});  // outside any link
    f.pointers({{0, {110, 50}, false}});
    CHECK(f.ui.takeEvents().empty());
}

TEST_CASE("item lists: multi-selection with Ctrl (toggle) and Shift (range)") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto e = f.element("List", f.canvas, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {300, 240});
    auto& list = f.scene.registry().emplace<UIItemListComponent>(e);
    list.items = "A;B;C;D;E";
    list.multiSelect = true;
    f.ui.layout(f.scene, 800, 600);
    auto click = [&](int row, bool ctrl, bool shift) {
        f.ui.setModifiers(ctrl, shift);
        const math::Vec2 p{100, 48.0f * static_cast<float>(row) + 24.0f};
        f.pointers({{0, p, true}});
        f.pointers({{0, p, false}});
        f.ui.setModifiers(false, false);
    };
    auto sel = [&] { return f.scene.registry().get<UIItemListComponent>(e).selection; };
    click(1, false, false);
    CHECK(sel() == "1");
    click(3, true, false);
    CHECK(sel() == "1,3");
    click(4, false, true);  // range from the last clicked (3)
    CHECK(sel() == "3,4");
    click(1, true, true);   // Ctrl+Shift: range added to the selection
    CHECK(sel() == "1,2,3,4");
    click(2, true, false);  // Ctrl: toggled off
    CHECK(sel() == "1,3,4");
    CHECK(UISystem::listSelection(f.scene.registry().get<UIItemListComponent>(e)) == std::vector<i32>{1, 3, 4});
    click(0, false, false);  // a plain click starts over
    CHECK(sel() == "0");
    bool changed = false;
    for (const auto& ev : f.ui.takeEvents()) changed |= ev.type == UIEventType::ValueChanged;
    CHECK(changed);
}

TEST_CASE("item lists: reorderable rows move by drag and drop") {
    UIFixture f;
    f.scene.registry().get<UICanvasComponent>(f.canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto e = f.element("List", f.canvas, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {300, 240});
    auto& list = f.scene.registry().emplace<UIItemListComponent>(e);
    list.items = "A;B;C;D";
    list.reorderable = true;
    f.ui.layout(f.scene, 800, 600);
    f.ui.takeEvents();
    f.pointers({{0, {100, 24}, true}});   // A
    f.pointers({{0, {100, 80}, true}});
    f.pointers({{0, {100, 130}, true}});
    f.pointers({{0, {100, 130}, false}});  // dropped on C
    auto& l = f.scene.registry().get<UIItemListComponent>(e);
    CHECK(l.items == "B;C;A;D");
    CHECK(l.selected == 2);
    bool changed = false;
    for (const auto& ev : f.ui.takeEvents()) changed |= ev.type == UIEventType::ValueChanged && ev.value == 2.0f;
    CHECK(changed);
    l.reorderable = false;  // off: a drag does not move anything
    f.ui.layout(f.scene, 800, 600);
    f.pointers({{0, {100, 24}, true}});
    f.pointers({{0, {100, 130}, true}});
    f.pointers({{0, {100, 130}, false}});
    CHECK(f.scene.registry().get<UIItemListComponent>(e).items == "B;C;A;D");
}
}  // TEST_SUITE
