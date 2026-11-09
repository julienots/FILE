#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
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
