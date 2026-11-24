#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <thread>

#include "agr/runtime/Engine.hpp"
#include "agr/runtime/Events.hpp"
#include "agr/scene/SceneSerializer.hpp"

using namespace agr;

namespace {

std::string tempDir(const char* tag) {
    auto p = std::filesystem::temp_directory_path() / (std::string("agr_") + tag + "_" + Uuid::generate().toString());
    std::filesystem::create_directories(p);
    return p.string();
}

EngineConfig headlessConfig(u64 frames) {
    EngineConfig c;
    c.headless = true;
    c.maxFrames = frames;
    c.workerThreads = 2;
    c.userDirectory = tempDir("user");
    c.assetsDirectory = c.userDirectory;
    return c;
}

struct CountingApp : Application {
    int starts = 0, updates = 0, fixed = 0, shutdowns = 0;
    Status onStart(Engine&) override { ++starts; return {}; }
    void onUpdate(Engine&, const FrameTime&) override { ++updates; }
    void onFixedUpdate(Engine&, const FrameTime&) override { ++fixed; }
    void onShutdown(Engine&) override { ++shutdowns; }
};

class RecordingPlugin : public Plugin {
public:
    RecordingPlugin(std::string name, std::vector<std::string> deps, std::vector<std::string>& log, bool fail = false)
        : m_name(std::move(name)), m_deps(std::move(deps)), m_log(log), m_fail(fail) {}
    PluginInfo info() const override { return {m_name, "1.0", "", "", m_deps}; }
    Status onLoad(Engine&) override {
        if (m_fail) return makeError(Error::Code::Unknown, "boom");
        m_log.push_back("load " + m_name);
        return {};
    }
    void onUnload(Engine&) override { m_log.push_back("unload " + m_name); }

private:
    std::string m_name;
    std::vector<std::string> m_deps;
    std::vector<std::string>& m_log;
    bool m_fail;
};

}  // namespace

TEST_SUITE("runtime") {

TEST_CASE("EngineConfig parses and validates JSON") {
    auto c = EngineConfig::fromJson(R"({"appName":"Game","targetFps":30,"window":{"width":800,"height":480,"graphics":"none"}})");
    REQUIRE(c);
    CHECK(c.value().appName == "Game");
    CHECK(c.value().targetFps == 30);
    CHECK(c.value().window.width == 800);
    CHECK(c.value().window.graphics == GraphicsApi::None);
    CHECK_FALSE(EngineConfig::fromJson("[]"));
    CHECK_FALSE(EngineConfig::fromJson(R"({"fixedTimestep":0})"));
    CHECK_FALSE(EngineConfig::fromJson(R"({"window":{"graphics":"directx"}})"));
    CHECK_FALSE(EngineConfig::fromJson(R"({"targetFps":"fast"})"));
    auto again = EngineConfig::fromJson(c.value().toJson());
    REQUIRE(again);
    CHECK(again.value().window.height == 480);
}

TEST_CASE("Input frame semantics") {
    Input in;
    in.setKey(Key::Space, true);
    CHECK(in.keyPressed(Key::Space));
    in.beginFrame();
    CHECK(in.keyDown(Key::Space));
    CHECK_FALSE(in.keyPressed(Key::Space));
    in.setKey(Key::Space, false);
    CHECK(in.keyReleased(Key::Space));

    in.setMousePosition({10, 10});
    in.setMousePosition({15, 12});
    CHECK(in.mouseDelta() == math::Vec2{5, 2});
    in.beginFrame();
    CHECK(in.mouseDelta() == math::Vec2{0, 0});

    in.touchDown(1, {100, 100}, 1.0f);
    in.touchDown(2, {200, 200}, 1.0f);
    CHECK(in.touches().size() == 2);
    in.beginFrame();
    in.touchMove(1, {110, 100}, 0.5f);
    CHECK(in.findTouch(1)->phase == TouchPhase::Moved);
    CHECK(in.findTouch(1)->delta == math::Vec2{10, 0});
    CHECK(in.findTouch(2)->phase == TouchPhase::Stationary);
    in.touchUp(2, {200, 200});
    CHECK(in.findTouch(2)->phase == TouchPhase::Ended);
    in.beginFrame();
    CHECK(in.findTouch(2) == nullptr);
    CHECK(in.touches().size() == 1);
    in.reset();
    CHECK(in.touches().empty());
}

TEST_CASE("headless engine runs the expected number of frames") {
    Engine engine(headlessConfig(10));
    REQUIRE(engine.initialize(nullptr));
    CountingApp app;
    CHECK(engine.run(app) == 0);
    CHECK(app.starts == 1);
    CHECK(app.updates == 10);
    CHECK(app.shutdowns == 1);
    CHECK(engine.time().frameIndex == 10);
}

TEST_CASE("fixed timestep is bounded per frame") {
    EngineConfig cfg = headlessConfig(3);
    cfg.fixedTimestep = 1.0 / 1000.0;  // tiny step: frames will need many catch-up steps
    cfg.maxFixedStepsPerFrame = 2;
    Engine engine(cfg);
    REQUIRE(engine.initialize(nullptr));
    CountingApp app;
    engine.run(app);
    CHECK(app.fixed <= 3 * 2);
}

TEST_CASE("engine saves and loads scenes through the VFS") {
    Engine engine(headlessConfig(1));
    REQUIRE(engine.initialize(nullptr));
    const auto e = engine.scene().createEntity("Saved");
    engine.scene().registry().get<TransformComponent>(e).position = {1, 2, 3};
    REQUIRE(engine.saveScene("user://level.agrscene"));
    engine.scene().clear();
    REQUIRE(engine.loadScene("user://level.agrscene"));
    const auto loaded = engine.scene().findByName("Saved");
    REQUIRE(engine.scene().valid(loaded));
    CHECK(engine.scene().registry().get<TransformComponent>(loaded).position == math::Vec3{1, 2, 3});
    CHECK_FALSE(engine.loadScene("user://missing.agrscene"));
}

TEST_CASE("quit request from an event handler") {
    Engine engine(headlessConfig(0));  // would run forever
    REQUIRE(engine.initialize(nullptr));
    struct QuitApp : Application {
        void onUpdate(Engine& e, const FrameTime& t) override {
            if (t.frameIndex == 4) e.events().publish(WindowCloseRequestedEvent{});
        }
    } app;
    engine.run(app);
    CHECK(engine.time().frameIndex == 5);
}

TEST_CASE("static plugins load in dependency order and unload in reverse") {
    std::vector<std::string> log;
    Engine engine(headlessConfig(1));
    engine.plugins().addStatic(std::make_unique<RecordingPlugin>("Physics", std::vector<std::string>{"Core"}, log));
    engine.plugins().addStatic(std::make_unique<RecordingPlugin>("Core", std::vector<std::string>{}, log));
    engine.plugins().addStatic(std::make_unique<RecordingPlugin>("Orphan", std::vector<std::string>{"Missing"}, log));
    engine.plugins().addStatic(std::make_unique<RecordingPlugin>("Broken", std::vector<std::string>{}, log, true));
    engine.plugins().addStatic(std::make_unique<RecordingPlugin>("NeedsBroken", std::vector<std::string>{"Broken"}, log));
    REQUIRE(engine.initialize(nullptr));
    CHECK(log == std::vector<std::string>{"load Core", "load Physics"});
    CHECK(engine.plugins().isLoaded("Physics"));
    CHECK_FALSE(engine.plugins().isLoaded("Orphan"));
    CHECK_FALSE(engine.plugins().isLoaded("NeedsBroken"));
    CountingApp app;
    engine.run(app);
    CHECK(log == std::vector<std::string>{"load Core", "load Physics", "unload Physics", "unload Core"});
}

TEST_CASE("dependency cycles are reported") {
    std::vector<std::string> log;
    PluginManager pm;
    pm.addStatic(std::make_unique<RecordingPlugin>("A", std::vector<std::string>{"B"}, log));
    pm.addStatic(std::make_unique<RecordingPlugin>("B", std::vector<std::string>{"A"}, log));
    Engine engine(headlessConfig(1));
    CHECK_FALSE(pm.loadAll(engine));
    CHECK(log.empty());
}

#if defined(AGR_TEST_PLUGIN_PATH)
TEST_CASE("dynamic plugin: unload and load again (editor hot reload); static plugins stay") {
    Engine engine(headlessConfig(1));
    REQUIRE(engine.plugins().addDynamic(AGR_TEST_PLUGIN_PATH));
    REQUIRE(engine.initialize(nullptr));
    REQUIRE(engine.plugins().isLoaded("Spin"));
    REQUIRE(ComponentRegistry::instance().find("Spin"));
    REQUIRE(engine.plugins().unloadDynamic(engine, "Spin"));
    CHECK_FALSE(engine.plugins().isLoaded("Spin"));
    CHECK(ComponentRegistry::instance().find("Spin") == nullptr);  // its onUnload cleaned up
    CHECK_FALSE(engine.plugins().unloadDynamic(engine, "Spin").ok());  // already gone
    REQUIRE(engine.plugins().addDynamic(AGR_TEST_PLUGIN_PATH));       // a new build
    REQUIRE(engine.plugins().loadAll(engine));
    CHECK(engine.plugins().isLoaded("Spin"));
    CHECK(ComponentRegistry::instance().find("Spin"));
}

TEST_CASE("dynamic plugin registers a component and drives the scene") {
    Engine engine(headlessConfig(30));
    REQUIRE(engine.plugins().addDynamic(AGR_TEST_PLUGIN_PATH));
    CHECK_FALSE(engine.plugins().addDynamic(AGR_TEST_PLUGIN_PATH));  // duplicate name
    REQUIRE(engine.initialize(nullptr));
    REQUIRE(engine.plugins().isLoaded("Spin"));

    const ComponentInfo* spin = ComponentRegistry::instance().find("Spin");
    REQUIRE(spin);
    CHECK((spin->field("speed")->flags & FieldFlag_Angle) != 0);

    Scene& scene = engine.scene();
    const auto e = scene.createEntity("Spinner");
    auto& pool = scene.registry().ensurePool(spin->id, spin->makePool);
    REQUIRE(pool.emplaceDefault(scene.registry(), e));

    // Serialization sees the plugin component through reflection.
    CHECK(SceneSerializer::toString(scene).find("\"Spin\"") != std::string::npos);

    struct CheckApp : Application {
        math::Quat finalRotation;
        // Make frames last long enough for a measurable rotation.
        void onUpdate(Engine&, const FrameTime&) override { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        void onShutdown(Engine& eng) override {
            finalRotation = eng.scene().registry().get<TransformComponent>(eng.scene().findByName("Spinner")).rotation;
        }
    } app;
    engine.run(app);
    // The plugin rotated at its default speed (90 deg/s) for the whole run.
    const math::Quat q = app.finalRotation;
    const float angle = 2.0f * std::atan2(std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z), q.w);
    const float expected = math::radians(90.0f) * static_cast<float>(engine.time().totalSeconds);
    CHECK(expected > 0.05f);
    CHECK(angle == doctest::Approx(expected).epsilon(0.01));
    // After unload the component type is gone from the catalogue.
    CHECK(ComponentRegistry::instance().find("Spin") == nullptr);
}

TEST_CASE("non-plugin libraries are rejected") {
    PluginManager pm;
    CHECK_FALSE(pm.addDynamic("/definitely/not/here/libagr_plugin_x.so"));
}
#endif

}  // TEST_SUITE

#include "agr/runtime/Gestures.hpp"

TEST_CASE("gestures: tap, double tap, long press, drag and swipe, pinch, mouse and wheel") {
    Input in;
    GestureRecognizer g;
    auto frame = [&](f64 dt = 1.0 / 60.0) {
        g.update(in, dt);
        std::vector<GestureType> types;
        for (const auto& e : g.events()) types.push_back(e.type);
        in.beginFrame();
        return types;
    };
    auto has = [](const std::vector<GestureType>& v, GestureType t) { return std::find(v.begin(), v.end(), t) != v.end(); };
    // Tap, then a second tap nearby soon after: double tap.
    in.touchDown(1, {100, 100}, 1.0f);
    frame();
    in.touchUp(1, {102, 101});
    auto t = frame();
    CHECK(has(t, GestureType::Tap));
    in.touchDown(2, {104, 103}, 1.0f);
    frame();
    in.touchUp(2, {104, 103});
    CHECK(has(frame(), GestureType::DoubleTap));
    // Long press: held still, no tap afterwards.
    in.touchDown(3, {300, 300}, 1.0f);
    bool longPress = false;
    for (int i = 0; i < 40; ++i) longPress |= has(frame(), GestureType::LongPress);
    CHECK(longPress);
    in.touchUp(3, {300, 300});
    CHECK_FALSE(has(frame(), GestureType::Tap));
    // Slow drag: begin, moves, end; no swipe, no tap.
    in.touchDown(4, {100, 400}, 1.0f);
    frame();
    in.touchMove(4, {130, 400}, 1.0f);
    t = frame();
    CHECK(has(t, GestureType::DragBegin));
    CHECK(g.dragDelta().x == doctest::Approx(30.0f));
    for (int i = 1; i <= 5; ++i) {
        in.touchMove(4, {130.0f + i * 2.0f, 400}, 1.0f);
        t = frame();
        CHECK(has(t, GestureType::Drag));
    }
    in.touchUp(4, {140, 400});
    t = frame();
    CHECK(has(t, GestureType::DragEnd));
    CHECK_FALSE(has(t, GestureType::Swipe));
    CHECK_FALSE(has(t, GestureType::Tap));
    // Fast flick: swipe.
    in.touchDown(5, {100, 500}, 1.0f);
    frame();
    for (int i = 1; i <= 4; ++i) {
        in.touchMove(5, {100.0f + i * 40.0f, 500}, 1.0f);
        frame();
    }
    in.touchUp(5, {260, 500});
    t = frame();
    CHECK(has(t, GestureType::Swipe));
    CHECK(g.events().back().velocity.x > 900.0f);
    // Two fingers apart: pinch > 1; together: < 1; lifting ends it without a tap.
    in.touchDown(6, {400, 300}, 1.0f);
    in.touchDown(7, {500, 300}, 1.0f);
    frame();
    in.touchMove(6, {350, 300}, 1.0f);
    in.touchMove(7, {550, 300}, 1.0f);
    t = frame();
    CHECK(has(t, GestureType::Pinch));
    CHECK(g.pinchScale() == doctest::Approx(2.0f));
    CHECK(g.pinching());
    in.touchMove(6, {400, 320}, 1.0f);
    in.touchMove(7, {500, 320}, 1.0f);
    frame();
    CHECK(g.pinchScale() == doctest::Approx(0.5f));
    CHECK(g.pinchPan().y == doctest::Approx(20.0f));
    in.touchUp(6, {400, 320});
    in.touchUp(7, {500, 320});
    t = frame();
    CHECK_FALSE(has(t, GestureType::Tap));
    CHECK_FALSE(g.pinching());
    // Mouse: the left button is a finger; the wheel zooms.
    in.setMousePosition({50, 50});
    in.setMouseButton(MouseButton::Left, true);
    frame();
    in.setMousePosition({90, 50});
    CHECK(has(frame(), GestureType::DragBegin));
    in.setMouseButton(MouseButton::Left, false);
    CHECK(has(frame(), GestureType::DragEnd));
    in.addWheel({0, 2});
    frame();
    CHECK(g.pinchScale() == doctest::Approx(1.21f));
}

TEST_CASE("game files mounted after initialize (exported players): translations and input actions are read from them") {
    // Same order as the player: the engine starts with an empty assets folder, the game comes after.
    Engine engine(headlessConfig(2));
    REQUIRE(engine.initialize(nullptr));
    CHECK(engine.localization().tr("menu.play") == "menu.play");  // nothing yet
    const std::string game = tempDir("game");
    {
        std::ofstream(std::filesystem::path(game) / "localization.csv") << "key,fr,en\nmenu.play,Jouer,Play\n";
    }
    engine.vfs().mount("assets", std::make_unique<NativeFileProvider>(game, false));
    struct ReadAtStart : Application {
        std::string atStart;
        Status onStart(Engine& e) override {
            atStart = e.localization().tr("menu.play");  // the first scene's scripts read it here
            return {};
        }
        void onUpdate(Engine&, const FrameTime&) override {}
    } app;
    CHECK(engine.run(app) == 0);
    CHECK((app.atStart == "Jouer" || app.atStart == "Play"));
    CHECK(engine.localization().languages().size() == 2);
}

TEST_CASE("gestures: two-finger twist gives the rotation (clockwise on screen > 0), scale stays 1") {
    Input in;
    GestureRecognizer g;
    auto at = [](float angle, float sign) { return math::Vec2{150.0f + sign * 50.0f * std::cos(angle), 100.0f + sign * 50.0f * std::sin(angle)}; };
    in.touchDown(1, at(0.0f, -1.0f), 1.0f);
    in.touchDown(2, at(0.0f, 1.0f), 1.0f);
    g.update(in, 1.0 / 60.0);
    in.beginFrame();
    float total = 0.0f, scale = 1.0f;
    for (int i = 1; i <= 10; ++i) {
        const float a = 0.05f * static_cast<float>(i);
        in.touchMove(1, at(a, -1.0f), 1.0f);
        in.touchMove(2, at(a, 1.0f), 1.0f);
        g.update(in, 1.0 / 60.0);
        total += g.pinchRotation();
        scale *= g.pinchScale();
        in.beginFrame();
    }
    CHECK(total == doctest::Approx(0.5f).epsilon(0.01));
    CHECK(scale == doctest::Approx(1.0f).epsilon(0.01));
}
