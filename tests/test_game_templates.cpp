// Game templates are played headless through the real engine (every plugin, the project's
// copied assets, the startup scene saved and reloaded) with simulated keys, clicks and taps.
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

#include "agr/animation/AnimationPlugin.hpp"
#include "agr/audio/AudioPlugin.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/editor/Project.hpp"
#include "agr/editor/SceneTemplates.hpp"
#include "agr/navigation/NavigationPlugin.hpp"
#include "agr/net/NetPlugin.hpp"
#include "agr/physics2d/Physics2DPlugin.hpp"
#include "agr/physics3d/Physics3DPlugin.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/RenderPlugin.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/script/ScriptEngine.hpp"
#include "agr/script/ScriptPlugin.hpp"
#include "agr/twod/TwoDPlugin.hpp"
#include "agr/ui/UIComponents.hpp"
#include "agr/ui/UIPlugin.hpp"

using namespace agr;
using namespace agr::editor;
namespace fs = std::filesystem;

namespace {

struct NullApp final : Application {
    Status onStart(Engine&) override { return {}; }
};

/// A game project created from a template, running in a headless engine.
struct GameHarness {
    fs::path root = fs::temp_directory_path() / ("agr_game_" + Uuid::generate().toString());
    std::unique_ptr<Engine> engine;
    NullApp app;
    f64 dt = 1.0 / 60.0;

    explicit GameHarness(SceneTemplate tmpl) {
        ProjectSettings settings;
        settings.name = "Template Test";
        settings.mode = templateMode(tmpl);
        auto project = Project::create(root / "game", settings, tmpl, AGR_TEST_TEMPLATE_ASSETS);
        REQUIRE(project);
        EngineConfig config;
        config.headless = true;
        config.workerThreads = 1;
        config.assetsDirectory = (root / "game" / "assets").string();
        config.userDirectory = (root / "user").string();
        engine = std::make_unique<Engine>(config);
        engine->plugins().addStatic(std::make_unique<RenderPlugin>());
        engine->plugins().addStatic(std::make_unique<TwoDPlugin>());
        engine->plugins().addStatic(std::make_unique<UIPlugin>());
        engine->plugins().addStatic(std::make_unique<ScriptPlugin>());
        engine->plugins().addStatic(std::make_unique<AnimationPlugin>());
        engine->plugins().addStatic(std::make_unique<NetPlugin>());
        engine->plugins().addStatic(std::make_unique<NavigationPlugin>());
        engine->plugins().addStatic(std::make_unique<Physics2DPlugin>());
        engine->plugins().addStatic(std::make_unique<Physics3DPlugin>());
        engine->plugins().addStatic(std::make_unique<AudioPlugin>());
        REQUIRE(engine->initialize(nullptr));
        engine->vfs().mount("game", std::make_unique<NativeFileProvider>((root / "game").string(), true));
        // The startup scene written by the project, as the player loads it.
        REQUIRE(engine->loadScene("game://" + project.value().settings().startupScene));
        REQUIRE(engine->start(app));
    }
    ~GameHarness() {
        engine.reset();
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    Scene& scene() { return engine->scene(); }
    Input& input() { return engine->input(); }
    ScriptEngine& scripts() { return *engine->service<ScriptPlugin>()->scripts(); }
    ecs::Entity find(std::string_view name) {
        const ecs::Entity e = scene().findByName(name);
        REQUIRE_MESSAGE(!e.isNull(), "no entity named " << name);
        return e;
    }
    void frames(int n) {
        for (int i = 0; i < n; ++i) engine->advance(app, dt);
    }
    void seconds(f64 s) { frames(static_cast<int>(s / dt + 0.5)); }
    /// Script field as text ("" when missing).
    std::string field(std::string_view entity, const std::string& name) {
        auto r = scripts().field(find(entity), name);
        return r ? r.value() : std::string();
    }
    double number(std::string_view entity, const std::string& name) {
        const std::string v = field(entity, name);
        REQUIRE_MESSAGE(!v.empty(), entity << "." << name << " is not set");
        return std::stod(v);
    }
    /// Runs Lua in the game (moves, cheats for the test); fails the test on error.
    void lua(const std::string& code) { REQUIRE_MESSAGE(scripts().run(code, &scene()), code); }
    void key(Key k, bool down) { input().setKey(k, down); }
    void holdKey(Key k, f64 s) {
        key(k, true);
        seconds(s);
        key(k, false);
        frames(1);
    }
    void pressKey(Key k) {
        key(k, true);
        frames(1);
        key(k, false);
        frames(1);
    }
    math::Vec2 screen() { return engine->platform().windowSize(); }
    /// Left click at a screen point (pixels).
    void click(math::Vec2 p) {
        input().setMousePosition(p);
        frames(1);
        input().setMouseButton(MouseButton::Left, true);
        frames(1);
        input().setMouseButton(MouseButton::Left, false);
        frames(1);
    }
    /// Click on a UI element (center of its laid-out rectangle).
    void clickUI(std::string_view name) {
        const ecs::Entity e = find(name);
        const math::Vec4 r = scene().registry().get<UIRectComponent>(e).screenRect;
        REQUIRE_MESSAGE(r.z > 0.0f, name << " is not laid out (inactive?)");
        click({r.x + r.z * 0.5f, r.y + r.w * 0.5f});
    }
    std::string text(std::string_view name) { return scene().registry().get<UITextComponent>(find(name)).text; }
    bool active(std::string_view name) { return scene().isActiveInHierarchy(find(name)); }
    math::Vec3 position(std::string_view name) { return scene().registry().get<TransformComponent>(find(name)).position; }
    /// No script reported an error so far.
    void noScriptErrors() {
        for (const auto& line : engine->consoleLog()->snapshot())
            if (line.level >= LogLevel::Error && (line.channel == "Script" || line.channel == "Scripts")) FAIL_CHECK(line.text);
    }
};

}  // namespace

TEST_SUITE("game_templates") {

TEST_CASE("every game template builds, saves and has a camera, a HUD and scripts that compile") {
    for (SceneTemplate t : allTemplates()) {
        if (!isGameTemplate(t)) continue;
        CAPTURE(templateName(t));
        GameHarness h(t);
        h.frames(3);
        bool camera = false, canvas = false;
        h.scene().registry().view<CameraComponent>().each([&](ecs::Entity, CameraComponent&) { camera = true; });
        h.scene().registry().view<UICanvasComponent>().each([&](ecs::Entity, UICanvasComponent&) { canvas = true; });
        CHECK(camera);
        CHECK(canvas);
        CHECK(std::string(templateDescription(t)).size() > 40);
        h.noScriptErrors();
    }
}

TEST_CASE("platformer: run, collect coins, stomp a slime, get hurt, checkpoint, finish, restart") {
    GameHarness h(SceneTemplate::GamePlatformer);
    h.frames(60);
    CHECK(h.position("Hero").y == doctest::Approx(-1.0f).epsilon(0.03));
    CHECK(h.text("CoinsText") == "Pièces : 0 / 12");
    h.holdKey(Key::Right, 1.0);  // three coins on the path
    CHECK(h.position("Hero").x > -3.0f);
    CHECK(h.number("Hero", "coins") >= 2);
    CHECK_FALSE(h.active("Coin 1"));
    // Stomp: dropped onto the first slime.
    h.lua(R"(local s = agr.find("Slime 1"); local x = s:position(); agr.find("Hero"):setPosition(x, 1.5); agr.find("Hero"):setVelocity(0, -2))");
    h.seconds(1.0);
    CHECK_FALSE(h.active("Slime 1"));
    CHECK(h.number("Hero", "livesLeft") == 3);
    // Walking into the second one costs a life.
    h.lua(R"(local s = agr.find("Slime 2"); local x = s:position(); agr.find("Hero"):setPosition(x - 1.6, -0.9))");
    h.holdKey(Key::Right, 0.8);
    CHECK(h.number("Hero", "livesLeft") == 2);
    CHECK(h.text("LivesText") == "Vies : 2");
    // Checkpoint, then a fall into the next pit: back at the checkpoint.
    h.seconds(1.6);  // invulnerability ends
    h.lua(R"(agr.find("Hero"):setPosition(37.5, -0.9))");
    h.frames(10);
    CHECK(h.number("Hero", "spawnX") == doctest::Approx(37.5));
    h.lua(R"(agr.find("Hero"):setPosition(32.5, -0.9))");  // second pit (tiles 41..44 -> x 31..35)
    h.seconds(1.5);
    CHECK(h.number("Hero", "livesLeft") == 1);
    CHECK(h.position("Hero").x == doctest::Approx(37.5f).epsilon(0.05));
    // Finish line.
    h.lua(R"(agr.find("Hero"):setPosition(76, 4.1))");
    h.frames(10);
    CHECK(h.field("Hero", "state") == "won");
    CHECK(h.active("Restart"));
    CHECK(h.text("Message").find("Arrivée") != std::string::npos);
    h.clickUI("Restart");
    CHECK(h.field("Hero", "state") == "playing");
    CHECK(h.number("Hero", "coins") == 0);
    CHECK(h.number("Hero", "livesLeft") == 3);
    CHECK(h.active("Coin 1"));
    CHECK(h.active("Slime 1"));
    CHECK_FALSE(h.active("Restart"));
    h.noScriptErrors();
}


TEST_CASE("RPG 2D: quest, gems, bats chase along the grid, sword, experience, death and save") {
    GameHarness h(SceneTemplate::GameRpg2D);
    h.frames(30);
    CHECK(h.text("QuestText").find("sage") != std::string::npos);
    const math::Vec3 start = h.position("Hero");
    h.key(Key::Right, true);
    h.key(Key::Up, true);
    h.seconds(0.5);
    h.key(Key::Right, false);
    h.key(Key::Up, false);
    h.frames(5);
    CHECK(h.position("Hero").x > start.x + 1.0f);
    CHECK(h.position("Hero").y > start.y + 1.0f);
    // Talk to the sage (E nearby): quest given.
    h.lua(R"(agr.find("Hero"):setPosition(-3, 1.2))");
    h.frames(3);
    CHECK(h.text("Hint").find("E") != std::string::npos);
    h.pressKey(Key::E);
    CHECK(h.field("Hero", "quest") == "active");
    CHECK(h.active("Dialog"));
    // Gems: walk over them.
    for (const char* g : {"Gem 1", "Gem 2", "Gem 3"}) {
        h.lua(std::string("local g = agr.find('") + g + "'); local x, y = g:position(); agr.find('Hero'):setPosition(x, y - 0.3)");
        h.frames(8);
        CHECK_FALSE(h.active(g));
    }
    CHECK(h.number("Hero", "gems") == 3);
    h.lua(R"(agr.find("Hero"):setPosition(-3, 1.2))");
    h.frames(3);
    h.pressKey(Key::E);
    CHECK(h.field("Hero", "quest") == "done");
    CHECK(h.number("Hero", "level") == 2);  // 10 XP: level 1 needs 5
    CHECK(h.text("LevelText").find("Niveau 2") != std::string::npos);
    // A bat notices the hero and flies to him around obstacles, then hurts him.
    const math::Vec3 bat0 = h.position("Bat 1");
    h.lua(R"(local x, y = agr.find("Bat 1"):position(); agr.find("Hero"):setPosition(x - 4, y))");
    const double hp = h.number("Hero", "hp");
    h.seconds(3.0);
    CHECK(std::abs(h.position("Bat 1").x - bat0.x) > 1.0f);
    CHECK(h.number("Hero", "hp") < hp);
    // Sword: three hits (level 2 deals 1) defeat it; experience goes up.
    const double xp = h.number("Hero", "xp");
    for (int i = 0; i < 4 && h.active("Bat 1"); ++i) {
        h.lua(R"(local x, y = agr.find("Hero"):position(); agr.find("Bat 1"):setPosition(x + 0.5, y + 0.5))");
        h.pressKey(Key::Space);
        h.seconds(0.4);
    }
    CHECK_FALSE(h.active("Bat 1"));
    CHECK(h.number("Hero", "xp") == xp + 2);
    // Progress is saved.
    h.engine->save().flush();
    CHECK(h.engine->save().get("rpg2d.level") == 2);
    CHECK(h.engine->save().get("rpg2d.quest") == "done");
    // Death and restart.
    h.lua(R"(local s = agr.find("Hero"):script(); s.invulnerable = 0; s:damage(100))");
    h.frames(2);
    CHECK(h.field("Hero", "state") == "dead");
    h.clickUI("Restart");
    CHECK(h.field("Hero", "state") == "playing");
    CHECK(h.number("Hero", "hp") == 12);
    CHECK(h.active("Bat 1"));
    h.noScriptErrors();
}

TEST_CASE("the harness reports script errors") {
    GameHarness h(SceneTemplate::GamePlatformer);
    h.frames(2);
    std::ofstream(h.root / "game" / "assets" / "bad.lua") << "local B = {}\nfunction B:onUpdate() error('boom') end\nreturn B\n";
    const auto bad = h.scene().createEntity("Bad");
    h.scene().registry().emplace<ScriptComponent>(bad).script = "assets://bad.lua";
    h.frames(2);
    bool found = false;
    for (const auto& line : h.engine->consoleLog()->snapshot()) found |= line.level >= LogLevel::Error && line.text.find("boom") != std::string::npos;
    CHECK(found);
}

}
