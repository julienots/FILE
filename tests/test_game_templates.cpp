// Game templates are played headless through the real engine (every plugin, the project's
// copied assets, the startup scene saved and reloaded) with simulated keys, clicks and taps.
#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

#include "agr/animation/AnimationPlugin.hpp"
#include "agr/audio/AudioPlugin.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/editor/Project.hpp"
#include "agr/editor/SceneTemplates.hpp"
#include "agr/scene/SceneSerializer.hpp"
#include "agr/navigation/NavigationPlugin.hpp"
#include "agr/net/NetPlugin.hpp"
#include "agr/net/NetSession.hpp"
#include <nlohmann/json.hpp>
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
    /// Screen point of a world position through the game's camera (asked to Lua: agr.camera).
    math::Vec2 worldToScreen(float x, float y, float z = 0.0f) {
        const ecs::Entity carrier = scene().createEntity("__carrier");
        lua("local sx, sy = agr.camera.worldToScreen(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) +
            "); agr.find('__carrier'):setName(string.format('%.3f %.3f', sx, sy))");
        float sx = 0.0f, sy = 0.0f;
        REQUIRE(std::sscanf(scene().nameOf(carrier).c_str(), "%f %f", &sx, &sy) == 2);
        scene().destroyEntity(carrier);
        return {sx, sy};
    }
    void clickWorld(float x, float y, float z = 0.0f) { click(worldToScreen(x, y, z)); }
    /// Click on a UI element (center of its laid-out rectangle).
    void clickUI(std::string_view name) {
        const ecs::Entity e = find(name);
        frames(1);  // laid out if it was just shown
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

TEST_CASE("platformer: menus, story, run and variable jump, signs, stomp, spiky, spring, crystals, checkpoint, results, movers, bats, crumbling blocks, fish, cannons, victory, game over") {
    GameHarness h(SceneTemplate::GamePlatformer);
    h.frames(5);
    auto ok = [&](const std::string& lua) { h.lua("assert(" + lua + ")"); };
    auto menuClick = [&](const char* button) {
        h.seconds(0.6);
        h.clickUI(button);
        h.frames(2);
    };
    auto put = [&](const std::string& level, const std::string& group, const std::string& name, float dx, float dy) {
        h.lua("local e = agr.find('" + level + "'):find('" + group + "'):find('" + name + "'); local x, y = e:position();"
              " agr.find('Hero'):setPosition(x + " + std::to_string(dx) + ", y + " + std::to_string(dy) + "); agr.find('Hero'):setVelocity(0, 0)");
    };
    auto number = [&](const std::string& expr) {
        h.lua("agr.find('Game'):script().probe = " + expr);
        return h.number("Game", "probe");
    };
    CHECK(h.field("Game", "page") == "title");
    menuClick("LevelsButton");
    ok("agr.find('LevelButton 2'):get('UIButton', 'interactable') == false");
    menuClick("BackButton");
    menuClick("PlayButton");
    CHECK(h.field("Game", "page") == "story");
    menuClick("NextButton");
    CHECK(h.field("Game", "state") == "playing");
    CHECK(h.text("LevelText") == "La Prairie");
    h.seconds(1.0);
    const math::Vec3 start = h.position("Hero");
    CHECK(start.y == doctest::Approx(-1.0f).epsilon(0.05));
    // Run, collect the first coins; a tap jump is lower than a held jump.
    h.holdKey(Key::Right, 1.2);
    CHECK(h.position("Hero").x > start.x + 4.0f);
    CHECK(h.number("Game", "coins") >= 1);
    auto peak = [&](int frames) {
        h.seconds(0.6);
        const float y0 = h.position("Hero").y;
        h.key(Key::Space, true);
        float top = y0;
        for (int i = 0; i < 50; ++i) {
            if (i == frames) h.key(Key::Space, false);
            h.frames(1);
            top = std::max(top, h.position("Hero").y);
        }
        h.key(Key::Space, false);
        h.seconds(0.8);
        return top - y0;
    };
    const float tap = peak(3), full = peak(50);
    CHECK(full > 3.5f);
    CHECK(tap < full - 1.0f);
    // A sign shows a hint while the hero stands in front of it.
    put("Level 1", "Props", "Sign 1", 0.0f, 0.1f);
    h.frames(5);
    CHECK(h.active("HintPanel"));
    CHECK(h.text("Hint").find("Saut") != std::string::npos);
    // Stomp a slime (+1 coin), walk into another (-1 heart).
    const double bank = number("agr.find('Game'):script().data.bank");
    put("Level 1", "Enemies", "Slime 1", 0.0f, 2.5f);
    h.seconds(1.0);
    ok("not agr.find('Level 1'):find('Enemies'):find('Slime 1'):isActive()");
    CHECK(number("agr.find('Game'):script().data.bank") >= bank + 1);
    CHECK(h.number("Game", "hearts") == 3);
    put("Level 1", "Enemies", "Slime 2", -2.0f, 0.1f);
    h.holdKey(Key::Right, 0.8);
    CHECK(h.number("Game", "hearts") == 2);
    // Landing on the hedgehog hurts too.
    h.lua("agr.find('Hero'):script().invulnerable = 0");
    put("Level 1", "Enemies", "Spiky 1", 0.0f, 2.5f);
    h.seconds(1.0);
    CHECK(h.number("Game", "hearts") == 1);
    ok("agr.find('Level 1'):find('Enemies'):find('Spiky 1'):isActive()");
    h.lua("agr.find('Game'):script().hearts = 3; agr.find('Hero'):script().invulnerable = 0");
    // The spring launches the hero high; crystals light their slot.
    put("Level 1", "Props", "Spring 1", 0.0f, 0.6f);
    float high = -10.0f;
    for (int i = 0; i < 40; ++i) {
        h.frames(1);
        high = std::max(high, h.position("Hero").y);
    }
    CHECK(high > start.y + 7.0f);
    put("Level 1", "Items", "Crystal 2", 0.0f, -0.3f);
    h.frames(3);
    CHECK(h.number("Game", "found") == 2);
    ok("agr.find('CrystalIcon 2'):get('UIImage', 'color').w > 0.9");
    // Checkpoint, then a fall into the lake: back at the checkpoint with one heart less.
    put("Level 1", "Props", "Checkpoint", 0.0f, 0.1f);
    h.frames(5);
    const double cx = h.number("Game", "spawnX");
    h.lua("agr.find('Hero'):setPosition(-10 + 53.5, 0)");  // the second lake (tiles 52..55)
    h.seconds(2.0);
    CHECK(h.number("Game", "hearts") == 2);
    CHECK(h.position("Hero").x == doctest::Approx(cx).epsilon(0.05));
    // The goal flag: results, progress saved, next level.
    put("Level 1", "Props", "Goal", 0.0f, 0.2f);
    h.frames(5);
    CHECK(h.field("Game", "state") == "results");
    CHECK(h.text("MenuText").find("★") != std::string::npos);
    ok("agr.save.get('plat').unlocked == 2");
    ok("agr.save.get('plat').crystals['1'] == 2");
    menuClick("NextButton");
    CHECK(h.number("Game", "level") == 2);
    CHECK(h.position("Hero").y < -40.0f);
    ok("select(2, agr.find('Main Camera'):position()) < -30");
    // Level 2: the moving platform and the bat move.
    h.lua("local m = agr.find('Level 2'):find('Props'):find('Mover 1'); _G.mx = m:position();"
          " local b = agr.find('Level 2'):find('Enemies'):find('Bat 1'); _G.bx, _G.by = b:position()");
    h.seconds(1.5);
    ok("math.abs(agr.find('Level 2'):find('Props'):find('Mover 1'):position() - _G.mx) > 1");
    ok("math.abs(agr.find('Level 2'):find('Enemies'):find('Bat 1'):position() - _G.bx) > 0.5");
    // Level 3: a crumbling block falls when stood on, and comes back.
    h.lua("agr.find('Game'):script():startLevel(3)");
    h.frames(3);
    put("Level 3", "Props", "Crumble 1", 0.0f, 0.6f);
    h.seconds(1.2);
    ok("not agr.find('Level 3'):find('Props'):find('Crumble 1'):isActive()");
    h.lua("agr.find('Hero'):setPosition(-10 + 5, -5 - 80 + 4.1)");
    h.seconds(3.5);
    ok("agr.find('Level 3'):find('Props'):find('Crumble 1'):isActive()");
    // Level 4: the fish leaps out of the water.
    h.lua("agr.find('Game'):script():startLevel(4); _G.leap = false; _G.fy = select(2, agr.find('Level 4'):find('Enemies'):find('Fish 1'):position())");
    for (int i = 0; i < 25; ++i) {
        h.seconds(0.2);
        h.lua("if select(2, agr.find('Level 4'):find('Enemies'):find('Fish 1'):position()) > _G.fy + 3 then _G.leap = true end");
    }
    ok("_G.leap");
    // Level 5: the cannon fires cannonballs that hurt.
    h.lua("agr.find('Game'):script():startLevel(5)");
    h.frames(3);
    put("Level 5", "Enemies", "Cannon 1", -8.0f, 0.1f);
    const double hearts5 = h.number("Game", "hearts");
    for (int i = 0; i < 30 && h.number("Game", "hearts") >= hearts5; ++i) h.seconds(0.2);
    CHECK(h.number("Game", "hearts") < hearts5);
    put("Level 5", "Props", "Goal", 0.0f, 0.2f);
    h.frames(5);
    CHECK(h.field("Game", "state") == "victory");
    ok("agr.save.get('plat').finished == true");
    // Game over and retry.
    h.seconds(1.5);
    menuClick("LevelsButton");
    menuClick("LevelButton 1");
    CHECK(h.number("Game", "level") == 1);
    h.lua("local g = agr.find('Game'):script(); g.hearts = 1; g:playerHurt(false)");
    CHECK(h.field("Game", "state") == "dying");
    h.seconds(1.5);
    CHECK(h.field("Game", "state") == "gameover");
    menuClick("RetryButton");
    CHECK(h.field("Game", "state") == "playing");
    CHECK(h.number("Game", "hearts") == 3);
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
    const math::Vec3 bat0{10.0f, -5.0f, 0.0f};
    h.lua(R"(local b = agr.find("Bat 1"); b:stopNavigation(); b:setPosition(10, -5); agr.find("Hero"):setPosition(6, -5.5))");
    const double hp = h.number("Hero", "hp");
    for (int k = 0; k < 16 && h.number("Hero", "hp") >= hp; ++k) h.seconds(0.5);  // it may detour around obstacles
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
    CHECK(h.engine->save().flush());
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


TEST_CASE("tower defense: build and upgrade towers, waves, kills pay gold, leaks, defeat and victory") {
    GameHarness h(SceneTemplate::GameTowerDefense);
    h.frames(10);
    CHECK(h.number("Game", "goldNow") == 100);
    const math::Vec3 slot = h.position("Slot 2");
    h.clickWorld(slot.x, slot.y);
    h.frames(2);
    CHECK(h.number("Game", "goldNow") == 50);
    CHECK_FALSE(h.scene().findByName("Tower Slot 2").isNull());
    const math::Vec3 slot4 = h.position("Slot 4");
    h.clickWorld(slot4.x, slot4.y);
    CHECK(h.number("Game", "goldNow") == 0);
    h.clickUI("NextWave");
    CHECK(h.number("Game", "wave") == 1);
    h.seconds(25.0);
    CHECK(h.number("Game", "kills") >= 3);  // the tower shot creeps
    CHECK(h.number("Game", "goldNow") >= 40);  // spent 100, then 5 + 1 gold per kill
    CHECK(h.text("GoldText").find("Or") != std::string::npos);
    // Upgrade: tap the tower.
    const double gold = h.number("Game", "goldNow");
    h.clickWorld(slot.x, slot.y);
    h.frames(2);
    CHECK(h.number("Game", "goldNow") == gold - 40);
    CHECK(h.number("Tower Slot 2", "level") == 2);
    // Defeat: no defense and one life left.
    h.lua(R"(local g = agr.find("Game"):script(); g.livesNow = 1; for _, t in pairs(g.towers) do t:destroy() end; g.towers = {})");
    h.frames(2);
    h.pressKey(Key::N);
    h.seconds(45.0);  // the road is about 60 m long
    CHECK(h.field("Game", "state") == "lost");
    CHECK(h.active("Restart"));
    h.clickUI("Restart");
    CHECK(h.field("Game", "state") == "playing");
    CHECK(h.number("Game", "wave") == 0);
    CHECK(h.number("Game", "goldNow") == 100);
    h.frames(2);
    CHECK(h.scene().findByName("Creep").isNull());
    // Victory: last wave against strong towers everywhere.
    h.lua(R"(local g = agr.find("Game"):script(); g.goldNow = 100000; for _, s in ipairs(agr.find("Slots"):children()) do g:build(s) end)");
    h.frames(2);
    h.lua(R"(local g = agr.find("Game"):script(); for _, t in pairs(g.towers) do t:script().damage = 50; t:script().range = 30 end; g.wave = g.waves - 1; g:nextWave())");
    h.seconds(40.0);
    CHECK(h.field("Game", "state") == "won");
    CHECK(h.text("Message").find("vagues") != std::string::npos);
    h.noScriptErrors();
}


TEST_CASE("cards: play cards with energy, armor, enemy intents and turns, victory saved, defeat") {
    GameHarness h(SceneTemplate::GameCards);
    h.frames(5);
    CHECK(h.number("Game", "energy") == 3);
    CHECK(h.active("Card 5"));
    CHECK(h.text("EnergyText") == "Énergie 3 / 3");
    // Known hand for the test: strike, shield, heavy, potion, bolt.
    h.lua(R"(local g = agr.find("Game"):script(); g.hand = {"strike", "shield", "heavy", "potion", "bolt"}; g.intent = {kind = "attack", value = 8}; g:refresh())");
    h.frames(2);
    CHECK(h.text("Name").size() > 0);
    h.clickUI("Card 1");  // strike: 5 damage
    CHECK(h.number("Game", "energy") == 2);
    h.lua(R"(_G.foeHp = agr.find("Game"):script().foe.hp)");
    CHECK(h.text("Log").find("5") != std::string::npos);
    h.clickUI("Card 1");  // the shield moved to slot 1: +6 armor
    CHECK(h.text("PlayerStats").find("Armure 6") != std::string::npos);
    h.clickUI("Card 1");  // heavy costs 2: refused with 1 energy left
    CHECK(h.text("Log") == "Pas assez d'énergie");
    CHECK(h.number("Game", "energy") == 1);
    const double hp = 30;
    h.clickUI("EndTurn");  // the golem attacks for 8: 6 absorbed by armor
    CHECK(h.text("PlayerStats").find("PV 28 / 30") != std::string::npos);
    CHECK(h.number("Game", "energy") == 3);
    CHECK(h.active("Card 5"));  // drew back to five
    (void)hp;
    // Victory.
    h.lua(R"(local g = agr.find("Game"):script(); g.foe.hp = 3; g.foe.armor = 0; g.hand = {"strike"}; g:refresh())");
    h.frames(2);
    h.clickUI("Card 1");
    CHECK(h.field("Game", "state") == "won");
    CHECK(h.engine->save().get("cards.wins") == 1);
    CHECK_FALSE(h.active("Card 1"));
    h.clickUI("Restart");
    CHECK(h.field("Game", "state") == "playing");
    // Defeat.
    h.lua(R"(local g = agr.find("Game"):script(); g.player.hp = 2; g.intent = {kind = "heavy", value = 14})");
    h.clickUI("EndTurn");
    CHECK(h.field("Game", "state") == "lost");
    CHECK(h.text("Message").find("golem") != std::string::npos);
    h.noScriptErrors();
}


TEST_CASE("farm simulation: sow, water, growth while moist, withering, rain, harvest at market price, goal") {
    GameHarness h(SceneTemplate::GameFarm);
    h.frames(5);
    CHECK(h.number("Game", "coinsNow") == 20);
    const math::Vec3 plot = h.position("Plot 1-1");
    h.clickWorld(plot.x, plot.y);  // the seed tool is selected at start
    CHECK(h.number("Game", "coinsNow") == 18);
    CHECK(h.active("Crop"));
    h.lua(R"(local g = agr.find("Game"):script(); g.rainy = false)");
    h.seconds(9.0);  // dry soil: no growth
    h.lua(R"(_G.stage = agr.find("Game"):script().plots[1].stage)");
    CHECK(h.scripts().run("assert(_G.stage == 0)", &h.scene()));
    h.clickUI("ToolWater");
    h.clickWorld(plot.x, plot.y);
    CHECK(h.scene().registry().get<SpriteRendererComponent>(h.find("Plot 1-1")).frame == 10);  // wet soil
    h.seconds(17.0);  // two growth steps of 8 s
    CHECK(h.scripts().run("assert(agr.find('Game'):script().plots[1].stage == 2)", &h.scene()));
    h.pressKey(Key::Num3);  // harvest tool: unripe crops stay
    h.clickWorld(plot.x, plot.y);
    CHECK(h.scripts().run("assert(agr.find('Game'):script().plots[1].stage == 2)", &h.scene()));
    // Ripe: sold at the day's price.
    h.lua(R"(local g = agr.find("Game"):script(); g:setStage(g.plots[1], 3); g.price = 7)");
    const double coins = h.number("Game", "coinsNow");
    h.clickWorld(plot.x, plot.y);
    CHECK(h.number("Game", "coinsNow") == coins + 7);
    CHECK_FALSE(h.active("Crop"));
    // Forgotten crops wither; rain waters every plot.
    h.lua(R"(local g = agr.find("Game"):script(); g.rainy = false; local p = g.plots[2]; g:setStage(p, 1); p.water = 0; p.dry = 39.5)");
    h.seconds(1.0);
    CHECK(h.scripts().run("assert(agr.find('Game'):script().plots[2].stage == 'withered')", &h.scene()));
    h.lua(R"(local g = agr.find("Game"):script(); g.rainy = true)");
    h.frames(2);
    CHECK(h.scripts().run("for _, p in ipairs(agr.find('Game'):script().plots) do assert(p.water > 0) end", &h.scene()));
    // Days pass.
    h.lua(R"(local g = agr.find("Game"):script(); g.clock = g.dayLength - 0.05)");
    h.frames(5);
    CHECK(h.number("Game", "day") == 2);
    CHECK(h.text("DayText").find("Jour 2") != std::string::npos);
    // Goal.
    h.lua(R"(local g = agr.find("Game"):script(); g.coinsNow = 198; g:setStage(g.plots[5], 3); g.price = 4)");
    const math::Vec3 p5 = h.position("Plot 1-5");
    h.clickWorld(p5.x, p5.y);
    CHECK(h.field("Game", "state") == "won");
    CHECK(h.active("Restart"));
    h.clickUI("Restart");
    CHECK(h.number("Game", "coinsNow") == 20);
    h.noScriptErrors();
}


TEST_CASE("idle: tap the crystal, buy generators with growing prices, production, pickaxe, offline progress, prestige") {
    GameHarness h(SceneTemplate::GameIdle);
    h.frames(5);
    const math::Vec3 crystal = h.position("Crystal");
    for (int i = 0; i < 3; ++i) h.clickWorld(crystal.x, crystal.y);
    CHECK(h.number("Game", "crystals") == doctest::Approx(3.0));
    h.clickUI("BuyMiner");  // 15 needed
    CHECK(h.text("Message") == "Pas assez de cristaux");
    h.lua(R"(agr.find("Game"):script().crystals = 100)");
    h.clickUI("BuyMiner");
    CHECK(h.number("Game", "crystals") == doctest::Approx(85.0).epsilon(0.01));
    h.clickUI("BuyMiner");  // 15 * 1.15 = 17
    CHECK(h.scripts().run("assert(agr.find('Game'):script().owned.miner == 2)", &h.scene()));
    const double before = h.number("Game", "crystals");
    h.seconds(10.0);  // 2 miners x 0.5 / s
    CHECK(h.number("Game", "crystals") == doctest::Approx(before + 10.0).epsilon(0.02));
    CHECK(h.text("RateText").find("1 / s") != std::string::npos);
    // Pickaxe: taps worth 2.
    h.lua(R"(agr.find("Game"):script().crystals = 60)");
    h.clickUI("BuyPickaxe");
    const double c0 = h.number("Game", "crystals");
    h.clickWorld(crystal.x, crystal.y);
    CHECK(h.number("Game", "crystals") == doctest::Approx(c0 + 2.0).epsilon(0.01));
    // Offline progress: the save says the player left one hour ago.
    h.lua(R"(local g = agr.find("Game"):script(); g:save(); local s = agr.save.get("idle"); s.time = agr.time.now() - 3600; s.crystals = 0; agr.save.set("idle", s); g:load())");
    CHECK(h.number("Game", "offline") == doctest::Approx(3600.0).epsilon(0.01));  // 1 crystal / s
    // Prestige: +25 % production.
    h.lua(R"(agr.find("Game"):script().crystals = 100000)");
    h.clickUI("Prestige");
    CHECK(h.number("Game", "prestige") == 1);
    CHECK(h.number("Game", "crystals") == 0);
    CHECK(h.scripts().run("assert(agr.find('Game'):script():tapValue() == 1.25)", &h.scene()));
    CHECK(h.engine->save().get("idle")["prestige"] == 1);
    h.noScriptErrors();
}


TEST_CASE("tycoon: build shops, customers queue and pay, impatience hurts reputation, upgrades, goal") {
    GameHarness h(SceneTemplate::GameTycoon);
    h.frames(5);
    CHECK(h.number("Game", "moneyNow") == 250);
    const math::Vec3 plot = h.position("Plot 2");
    h.clickWorld(plot.x, plot.y);
    CHECK(h.number("Game", "moneyNow") == 150);
    CHECK(h.scripts().run("assert(agr.find('Game'):script().shops[2].level == 1)", &h.scene()));
    // Customers come, queue and pay 8 each.
    h.seconds(40.0);
    CHECK(h.number("Game", "served") >= 2);
    CHECK(h.number("Game", "moneyNow") >= 150 + 16);
    CHECK(h.text("DayText").find("servis") != std::string::npos);
    // Upgrade: faster and pricier.
    h.lua(R"(agr.find("Game"):script().moneyNow = 1000)");
    h.clickWorld(plot.x, plot.y);
    CHECK(h.number("Game", "moneyNow") == 1000 - 120);
    CHECK(h.scripts().run("local g = agr.find('Game'):script(); assert(g.shops[2].level == 2 and g:price(g.shops[2]) == 12)", &h.scene()));
    // Endless service: customers give up and reputation drops.
    h.lua(R"(local g = agr.find("Game"):script(); g.serviceTime = function() return 1000 end; g.patience = 2)");
    const double rep = h.number("Game", "reputation");
    h.seconds(30.0);
    CHECK(h.number("Game", "unhappy") >= 1);
    CHECK(h.number("Game", "reputation") < rep);
    // Goal.
    h.lua(R"(agr.find("Game"):script().moneyNow = 2000)");
    h.frames(2);
    CHECK(h.field("Game", "state") == "won");
    h.clickUI("Restart");
    CHECK(h.number("Game", "moneyNow") == 250);
    h.frames(2);
    CHECK(h.scene().findByName("Customer").isNull());
    h.noScriptErrors();
}


TEST_CASE("survival: hunger, berries, wood, campfire, zombies at night chase and burn, dawn, three nights") {
    GameHarness h(SceneTemplate::GameSurvival);
    h.frames(5);
    CHECK(h.number("Survivor", "health") == 10);
    h.seconds(5.0);
    CHECK(h.number("Survivor", "hunger") < 97);  // 0.8 / s
    // Berries.
    h.lua(R"(local b = agr.find("Bush 1"); local x, y = b:position(); agr.find("Survivor"):setPosition(x + 0.8, y))");
    h.frames(3);
    const double hunger = h.number("Survivor", "hunger");
    h.pressKey(Key::E);
    CHECK(h.number("Survivor", "hunger") == doctest::Approx(std::min(100.0, hunger + 25)).epsilon(0.02));
    // Wood: three hits fell a tree.
    h.lua(R"(local t = agr.find("Tree 1"); local x, y = t:position(); agr.find("Survivor"):setPosition(x + 1.0, y))");
    h.seconds(0.4);  // action cooldown
    for (int i = 0; i < 3; ++i) {
        h.pressKey(Key::Space);
        h.seconds(0.4);
    }
    CHECK_FALSE(h.active("Tree 1"));
    CHECK(h.number("Survivor", "wood") == 4);
    h.pressKey(Key::C);  // not enough wood
    CHECK(h.text("Message").find("bois") != std::string::npos);
    h.lua(R"(agr.find("Survivor"):script().wood = 5; agr.find("Survivor"):setPosition(0, -0.5))");
    h.pressKey(Key::C);
    CHECK_FALSE(h.scene().findByName("Campfire").isNull());
    CHECK(h.number("Survivor", "wood") == 0);
    // Night: zombies spawn and chase.
    h.lua(R"(local s = agr.find("Survivor"):script(); s.clock = s.dayLength - 0.05; s.hunger = 100)");
    h.seconds(2.0);
    CHECK(h.field("Survivor", "night") == "true");
    CHECK_FALSE(h.scene().findByName("Zombie").isNull());
    // Away from the fire, a zombie reaches the survivor.
    h.lua(R"(agr.find("Survivor"):setPosition(-12, 0); local z = agr.find("Zombie"); z:setPosition(-9, 0))");
    const double health = h.number("Survivor", "health");
    for (int k = 0; k < 16 && h.number("Survivor", "health") >= health; ++k) h.seconds(0.5);
    CHECK(h.number("Survivor", "health") < health);
    // A zombie next to the fire burns.
    h.lua(R"(local f = agr.find("Campfire"); local x, y = f:position(); local z = agr.find("Zombie"); z:setPosition(x + 1, y); z:script().life = 1)");
    h.seconds(1.5);
    h.lua(R"(_G.zombies = 0; for _, z in ipairs(agr.find("Survivor"):script().zombies) do if z:isValid() then _G.zombies = _G.zombies + 1 end end)");
    // Dawn after the night: zombies gone, one night survived.
    h.lua(R"(local s = agr.find("Survivor"):script(); s.clock = s.nightLength - 0.05; s.health = 10)");
    h.frames(3);
    CHECK(h.number("Survivor", "survived") == 1);
    h.frames(2);
    CHECK(h.scene().findByName("Zombie").isNull());
    // Three nights: victory.
    h.lua(R"(local s = agr.find("Survivor"):script(); s.survived = 2; s.night = true; s.clock = s.nightLength - 0.05)");
    h.frames(3);
    CHECK(h.field("Survivor", "state") == "won");
    h.clickUI("Restart");
    CHECK(h.field("Survivor", "state") == "playing");
    CHECK(h.active("Tree 1"));
    // Starvation kills.
    h.lua(R"(local s = agr.find("Survivor"):script(); s.hunger = 0; s.health = 0.2)");
    h.seconds(1.0);
    CHECK(h.field("Survivor", "state") == "dead");
    h.noScriptErrors();
}


TEST_CASE("RPG 3D: walk on the terrain relative to the camera, quest orbs, golems chase on the navmesh, sword, levels") {
    GameHarness h(SceneTemplate::GameRpg3D);
    h.seconds(1.0);  // the hero settles on the terrain
    const math::Vec3 start = h.position("Hero");
    h.holdKey(Key::Up, 1.0);  // forward = away from the camera (-Z)
    CHECK(h.position("Hero").z < start.z - 2.5f);
    CHECK(std::abs(h.position("Hero").y - start.y) < 1.0f);  // still on the ground
    // Quest from the elder.
    h.lua(R"(local x, y, z = agr.find("Sage"):position(); agr.find("Hero"):setPosition(x + 1.5, y + 0.2, z))");
    h.frames(5);
    h.pressKey(Key::E);
    CHECK(h.field("Hero", "quest") == "active");
    // Orbs: 3D triggers.
    for (const char* o : {"Orb 1", "Orb 2", "Orb 3"}) {
        h.lua(std::string("local x, y, z = agr.find('") + o + "'):position(); agr.find('Hero'):setPosition(x, y - 1.1, z); agr.find('Hero'):setVelocity(0, 0, 0)");
        h.frames(10);
        CHECK_MESSAGE(!h.active(o), o);
    }
    CHECK(h.number("Hero", "gems") == 3);
    h.lua(R"(local x, y, z = agr.find("Sage"):position(); agr.find("Hero"):setPosition(x + 1.5, y + 0.2, z))");
    h.frames(10);
    h.pressKey(Key::E);
    CHECK(h.field("Hero", "quest") == "done");
    CHECK(h.number("Hero", "level") == 2);
    // A golem walks to the hero along the navigation mesh and strikes.
    h.lua(R"(local x, y, z = agr.find("Golem 1"):position(); agr.find("Hero"):setPosition(x - 7, y + 0.3, z))");
    const double hp = h.number("Hero", "hp");
    const math::Vec3 g0 = h.position("Golem 1");
    for (int k = 0; k < 16 && h.number("Hero", "hp") >= hp; ++k) h.seconds(0.5);
    CHECK(math::length(h.position("Golem 1") - g0) > 2.0f);
    CHECK(h.number("Hero", "hp") < hp);
    // Sword until it falls.
    const double xp = h.number("Hero", "xp");
    for (int i = 0; i < 8 && h.active("Golem 1"); ++i) {
        h.lua(R"(local x, y, z = agr.find("Hero"):position(); agr.find("Golem 1"):setPosition(x + 1, y, z))");
        h.pressKey(Key::Space);
        h.seconds(0.45);
    }
    CHECK_FALSE(h.active("Golem 1"));
    CHECK(h.number("Hero", "xp") == xp + 3);
    CHECK(h.engine->save().get("rpg3d.quest") == "done");
    h.noScriptErrors();
}


TEST_CASE("strategy: workers mine, training, tap to select and send, combat, red AI attacks, base destroyed") {
    GameHarness h(SceneTemplate::GameStrategy);
    h.frames(10);
    auto blue = [&]() -> double {
        const ecs::Entity c = h.scene().createEntity("__c");
        h.lua("agr.find('__c'):setName(tostring(math.floor(agr.find('Game'):script().bank.blue)))");
        const double v = std::stod(h.scene().nameOf(c));
        h.scene().destroyEntity(c);
        return v;
    };
    CHECK(blue() == 100);
    h.seconds(25.0);  // workers walk to a mine, dig, come back
    CHECK(blue() > 100);
    h.clickUI("TrainSoldier");
    h.frames(3);
    CHECK(h.text("ArmyText").find("Soldats : 3") != std::string::npos);
    // Select one soldier by tapping it, send it to the middle.
    h.lua(R"(_G.sol = nil; for _, u in ipairs(agr.find("Game"):script().units.blue) do if u:script().kind == "soldier" then _G.sol = u end end; _G.sol:setName("Picked"))");
    h.frames(2);
    const math::Vec3 p0 = h.position("Picked");
    h.clickWorld(p0.x, 0.0f, p0.z);
    CHECK(h.scripts().run("assert(agr.find('Picked'):script().selected == true and agr.find('Picked'):find('Ring'):isActive())", &h.scene()));
    h.clickWorld(-8.0f, 0.0f, 3.0f);
    h.seconds(5.0);
    const math::Vec3 p1 = h.position("Picked");
    CHECK(std::abs(p1.x - -8.0f) < 1.5f);
    CHECK(std::abs(p1.z - 3.0f) < 1.5f);
    // A red soldier next to it: they fight to the death.
    h.lua(R"(local g = agr.find("Game"):script(); local r = g:spawn("red", "soldier"); r:setName("Foe"); local x, y, z = agr.find("Picked"):position(); r:setPosition(x + 1.2, 0, z))");
    h.seconds(12.0);
    const bool foeDead = h.scene().findByName("Foe").isNull(), pickedDead = h.scene().findByName("Picked").isNull();
    CHECK((foeDead || pickedDead));
    // The red AI attacks the blue base once it has an army.
    h.lua(R"(local g = agr.find("Game"):script(); for i = 1, 4 do local r = g:spawn("red", "soldier"); r:setPosition(-12 + i * 0.5, 0, -2) end; for _, u in ipairs(g.units.blue) do if u:script() and u:script().kind == "soldier" then u:destroy() end end)");
    h.seconds(14.0);
    CHECK(h.scripts().run("assert(agr.find('Game'):script().hpOf.blue < 60)", &h.scene()));
    // Victory: the red base falls.
    h.lua(R"(local g = agr.find("Game"):script(); g.hpOf.red = 1; local s = g:spawn("blue", "soldier"); s:setPosition(14.6, 0, 0))");
    for (int k = 0; k < 20 && h.field("Game", "state") == "playing"; ++k) h.seconds(0.5);
    CHECK(h.field("Game", "state") != "playing");
    CHECK(h.active("Restart"));
    h.clickUI("Restart");
    CHECK(h.field("Game", "state") == "playing");
    h.noScriptErrors();
}


TEST_CASE("racing: countdown, throttle and steering, off-road slowdown, AI follows the circuit, laps, finish, best lap saved") {
    GameHarness h(SceneTemplate::GameRacing);
    h.frames(5);
    CHECK(h.field("Race", "state") == "countdown");
    h.key(Key::Up, true);
    h.seconds(1.0);
    CHECK(std::abs(h.position("Player Car").x - -17.0f) < 0.2f);  // held at the line
    h.key(Key::Up, false);
    h.seconds(2.2);
    CHECK(h.field("Race", "state") == "racing");
    const math::Vec3 ai0 = h.position("Rival Car");
    h.holdKey(Key::Up, 2.0);
    CHECK(h.number("Player Car", "speed") > 15);
    CHECK(h.position("Player Car").x > -10.0f);  // heading +X along the straight
    CHECK(std::abs(h.position("Player Car").z - 14.5f) < 1.0f);
    CHECK(h.position("Rival Car").x > ai0.x + 20.0f);
    CHECK(h.text("SpeedText").find("km/h") != std::string::npos);
    // Steering turns the car.
    h.lua(R"(_G.y0 = agr.find("Player Car"):script().yaw)");
    h.key(Key::Up, true);
    h.holdKey(Key::Right, 0.5);
    h.key(Key::Up, false);
    CHECK(h.scripts().run("assert(math.abs(agr.find('Player Car'):script().yaw - _G.y0) > 10)", &h.scene()));
    // Off the road: speed capped.
    h.lua(R"(local c = agr.find("Player Car"); c:setPosition(0, 0.3, 0); c:script().speed = 26)");
    h.holdKey(Key::Up, 2.0);
    CHECK(h.field("Player Car", "offRoad") == "true");
    CHECK(h.number("Player Car", "speed") <= 26 * 0.45 + 0.5);
    // Laps: drive the waypoints in order (teleporting along the circuit), three times.
    for (int lap = 0; lap < 3; ++lap)
        for (int w = 2; w <= 25; ++w) {
            const int wp = (w - 1) % 24 + 1;
            h.lua("local w = agr.find('WP " + std::to_string(wp) + "'); local x, y, z = w:position(); local c = agr.find('Player Car'); c:setPosition(x, 0.3, z)");
            h.frames(2);
        }
    CHECK(h.field("Race", "state") == "finished");
    CHECK(h.active("Restart"));
    CHECK(h.engine->save().get("racing.bestLap").is_number());
    CHECK(h.text("Message").find("s") != std::string::npos);
    h.clickUI("Restart");
    CHECK(h.field("Race", "state") == "countdown");
    CHECK(std::abs(h.position("Player Car").x - -17.0f) < 0.5f);
    h.noScriptErrors();
}


TEST_CASE("arena: two local players race for coins; network host simulates a remote player and broadcasts the state") {
    GameHarness h(SceneTemplate::GameArena);
    h.frames(5);
    CHECK(h.active("Player 1"));
    CHECK(h.active("Player 2"));
    CHECK_FALSE(h.active("Player 3"));
    const math::Vec3 a0 = h.position("Player 1"), b0 = h.position("Player 2");
    h.key(Key::D, true);
    h.key(Key::Left, true);
    h.seconds(0.5);
    h.key(Key::D, false);
    h.key(Key::Left, false);
    h.frames(1);
    CHECK(h.position("Player 1").x > a0.x + 2.0f);
    CHECK(h.position("Player 2").x < b0.x - 2.0f);
    // Coin: put it on player 1.
    // (the random coin may already have been picked up on the way: start the count from zero)
    h.lua(R"(local g = agr.find("Game"):script(); for i = 1, 4 do g.players[i].score = 0 end; local p = g.players[1]; g.coinX, g.coinY = p.x, p.y)");
    h.frames(2);
    CHECK(h.text("Score1") == "Joueur 1 : 1");
    h.lua(R"(local g = agr.find("Game"):script(); g.players[2].score = 9; local p = g.players[2]; g.coinX, g.coinY = p.x, p.y)");
    h.frames(2);
    CHECK(h.text("Message") == "Le joueur 2 gagne !");
    h.seconds(3.5);
    CHECK(h.text("Score2") == "Joueur 2 : 0");  // next round

    // Network: host on a free port (the game's port may be in use on this machine).
    h.lua(R"(agr.find("Game"):script().port = 0)");
    h.clickUI("Host");
    CHECK(h.field("Game", "mode") == "host");
    CHECK_FALSE(h.active("Player 2"));
    h.lua(R"(_G.__port = agr.net.port())");
    const ecs::Entity carrier = h.scene().createEntity("__c");
    h.lua("agr.find('__c'):setName(tostring(_G.__port))");
    const u16 port = static_cast<u16>(std::stoi(h.scene().nameOf(carrier)));
    h.scene().destroyEntity(carrier);
    REQUIRE(port != 0);
    net::NetSession client;
    REQUIRE(client.join("127.0.0.1", port));
    f64 now = 0.0;
    std::vector<net::NetEvent> received;
    for (int i = 0; i < 120 && !client.connected(); ++i) {
        h.frames(1);
        client.update(now += 1.0 / 60.0);
    }
    REQUIRE(client.connected());
    CHECK(client.localId() == 2);
    // The remote player holds "right": the host moves player 2 and broadcasts positions.
    h.frames(2);
    const float x0 = h.position("Player 2").x;
    for (int i = 0; i < 40; ++i) {
        client.send(net::NetSession::kHostId, R"({"t":"in","x":1,"y":0})", false);
        h.frames(1);
        client.update(now += 1.0 / 60.0);
        for (auto& e : client.takeEvents()) received.push_back(std::move(e));
    }
    CHECK(h.active("Player 2"));
    CHECK(h.position("Player 2").x > x0 + 1.0f);
    REQUIRE(received.size() >= 5);  // about 20 states per second
    const auto st = nlohmann::json::parse(received.back().data);
    CHECK(st["t"] == "st");
    CHECK(st["p"][1][3] == 1);                                 // player 2 active
    CHECK(st["p"][1][0].get<double>() > x0 + 1.0);             // its position
    CHECK(h.text("Status").find("2 joueur") != std::string::npos);
    client.close();
    for (int i = 0; i < 10; ++i) h.frames(1);
    CHECK_FALSE(h.active("Player 2"));  // left
    h.noScriptErrors();
}

TEST_CASE("2.5D adventure: menus, options, story, signs, pots, chest, gems and gate, enemies, shop, spikes, chasm, archers, puzzle, brute, boss, victory, game over") {
    GameHarness h(SceneTemplate::GameAdventure25D);
    h.frames(5);
    auto ok = [&](const std::string& lua) { h.lua("assert(" + lua + ")"); };
    auto menuClick = [&](const char* button) {
        h.seconds(0.6);  // the menu slides in
        h.clickUI(button);
        h.frames(2);
    };
    // ---- title menu, options (volumes, language), level select.
    CHECK(h.field("Game", "state") == "menu");
    CHECK(h.field("Game", "page") == "title");
    CHECK(h.active("PlayButton"));
    CHECK(h.active("LevelsButton"));
    CHECK_FALSE(h.active("Attack"));
    menuClick("OptionsButton");
    CHECK(h.field("Game", "page") == "options");
    menuClick("MusicButton");
    ok("math.abs(agr.audio.busVolume('Music') - 0.75) < 0.01");
    ok("agr.find('MusicButton'):find('Label'):text():find('75')");
    menuClick("LanguageButton");
    ok("agr.locale.get() == 'en'");
    ok("agr.find('LanguageButton'):find('Label'):text():find('English')");
    menuClick("LanguageButton");
    menuClick("LanguageButton");
    ok("agr.locale.get() == 'fr'");
    menuClick("BackButton");
    CHECK(h.field("Game", "page") == "title");
    menuClick("LevelsButton");
    ok("agr.find('LevelButton 1'):get('UIButton', 'interactable') == true");
    ok("agr.find('LevelButton 2'):get('UIButton', 'interactable') == false");
    menuClick("BackButton");
    // ---- first game: the story, then hall 1.
    menuClick("PlayButton");
    CHECK(h.field("Game", "page") == "story");
    CHECK(h.text("MenuText").find("Ambre") != std::string::npos);
    menuClick("NextButton");
    CHECK(h.field("Game", "state") == "playing");
    CHECK_FALSE(h.active("Menu"));
    CHECK(h.active("Attack"));
    CHECK(h.text("LevelText").find("Hall des Statues") != std::string::npos);
    // Controls (enemies set aside: they chase and knock the hero back).
    h.lua("for _, e in ipairs(agr.find('Area 1'):find('Enemies'):children()) do e:setActive(false) end");
    h.seconds(1.0);
    const math::Vec3 start = h.position("Hero");
    CHECK(std::abs(start.y) < 0.2f);
    CHECK(h.field("Hero", "grounded") == "true");
    h.holdKey(Key::Right, 1.0);
    CHECK(h.position("Hero").x > start.x + 3.0f);
    h.holdKey(Key::Up, 0.5);
    CHECK(h.position("Hero").z < start.z - 1.0f);
    h.key(Key::Space, true);
    h.frames(8);
    h.key(Key::Space, false);
    CHECK(h.position("Hero").y > 0.5f);
    h.seconds(1.5);
    // Signs show a hint nearby.
    auto teleport = [&](const std::string& area, const std::string& group, const std::string& name, float dx = 0.0f, float dz = 0.0f) {
        const std::string path = "agr.find('" + area + "')" + (group.empty() ? "" : ":find('" + group + "')") + ":find('" + name + "')";
        h.lua("local e = " + path + "; local x, y, z = e:worldPosition();"
              " agr.find('Hero'):setPosition(x + " + std::to_string(dx) + ", 0.05, z + " + std::to_string(dz) + "); agr.find('Hero'):setVelocity(0, 0, 0)");
        h.frames(3);
    };
    teleport("Area 1", "Props", "Sign 1", 1.0f);
    CHECK(h.active("HintPanel"));
    CHECK(h.text("Hint").find("Déplacez") != std::string::npos);
    teleport("Area 1", "Props", "Sign 1", 6.0f);
    CHECK_FALSE(h.active("HintPanel"));
    // Pots break under the sword; the chest opens on contact (+5 coins).
    teleport("Area 1", "Props", "Pot 3", -1.2f);
    h.pressKey(Key::Right);  // face it
    h.lua("agr.find('Hero'):setVelocity(0, 0, 0)");
    teleport("Area 1", "Props", "Pot 3", -1.2f);
    h.pressKey(Key::J);
    ok("not agr.find('Area 1'):find('Props'):find('Pot 3'):isActive()");
    h.lua("_G.bank0 = agr.find('Game'):script().data.bank");
    teleport("Area 1", "Props", "Chest 1");
    ok("agr.find('Game'):script().data.bank == _G.bank0 + 5");
    ok("agr.find('Area 1'):find('Props'):find('Chest 1'):find('Sprite'):get('SpriteRenderer', 'texture'):find('chest_open')");
    // Gems and coins; the third gem opens the gate (it sinks, then disappears).
    teleport("Area 1", "Items", "Coin 1");
    CHECK(h.number("Game", "coinItems") >= 1);  // more if the walk crossed a coin
    teleport("Area 1", "Items", "Gem 1");
    teleport("Area 1", "Items", "Gem 2");
    CHECK(h.number("Game", "gems") == 2);
    teleport("Area 1", "Items", "Gem 3");
    ok("agr.find('Area 1'):find('Gate'):isActive()");
    h.seconds(1.5);
    ok("not agr.find('Area 1'):find('Gate'):isActive()");
    CHECK(h.text("GemsText").find("3 / 3") != std::string::npos);
    // A slime chases the hero on the navigation mesh, hurts him, and dies to the sword (2 hits).
    h.lua("for _, e in ipairs(agr.find('Area 1'):find('Enemies'):children()) do e:setActive(true) end;"
          " agr.find('Area 1'):find('Enemies'):find('Slime 1'):setPosition(-1, 0, -3)");
    h.frames(2);
    teleport("Area 1", "Enemies", "Slime 1", -5.0f);
    const double health = h.number("Hero", "health");
    for (int k = 0; k < 16 && h.number("Hero", "health") >= health; ++k) h.seconds(0.5);
    CHECK(h.number("Hero", "health") < health);
    for (int i = 0; i < 8; ++i) {
        h.lua("local s = agr.find('Area 1'):find('Enemies'):find('Slime 1'); if s:isActive() then local x, y, z = agr.find('Hero'):position();"
              " s:setPosition(x + 1, 0, z); agr.find('Hero'):script().facing = 1 end");
        h.pressKey(Key::J);
        h.seconds(0.4);
    }
    ok("not agr.find('Area 1'):find('Enemies'):find('Slime 1'):isActive()");
    // Pause and resume.
    h.pressKey(Key::P);
    CHECK(h.field("Game", "state") == "paused");
    CHECK(h.active("ResumeButton"));
    CHECK(h.active("RestartButton"));
    menuClick("ResumeButton");
    CHECK(h.field("Game", "state") == "playing");
    // ---- the portal: results, stars, the shop.
    h.lua("agr.find('Game'):script().coinItems = agr.find('Game'):script().coinItemsTotal");  // pretend every coin was picked
    teleport("Area 1", "", "Portal", -0.5f);
    CHECK(h.field("Game", "state") == "levelcomplete");
    h.seconds(2.0);
    CHECK(h.field("Game", "state") == "shop");
    CHECK(h.active("Shop"));
    CHECK(h.text("ShopResults").find("★★") != std::string::npos);
    ok("agr.find('Game'):script():starsOf(1) >= 3");
    h.lua("local g = agr.find('Game'):script(); g.data.bank = 100; g:refreshShop()");
    menuClick("BuyHeart");
    menuClick("BuySword");
    menuClick("BuyPotion");
    ok("agr.find('Game'):script().data.upgrades.heart == 1");
    ok("agr.find('Game'):script().data.upgrades.sword == 1");
    ok("agr.find('Game'):script().data.bank == 100 - 10 - 15 - 8");
    ok("agr.find('BuyPotion'):get('UIButton', 'interactable') == false");
    menuClick("ShopContinue");
    CHECK(h.number("Game", "level") == 2);
    CHECK(h.number("Hero", "maxHealth") == 6);
    CHECK(h.number("Hero", "power") == 2);
    CHECK(h.field("Hero", "guard") == "true");
    CHECK(h.position("Hero").x > 50.0f);
    ok("agr.save.get('ambre').unlocked == 2");
    // ---- hall 2: spike traps (the guard potion absorbs the first hit).
    h.lua("for _, e in ipairs(agr.find('Area 2'):find('Enemies'):children()) do e:setActive(false) end");
    h.seconds(1.0);
    const double h2 = h.number("Hero", "health");
    for (int i = 0; i < 2; ++i) {
        teleport("Area 2", "Props", "Spikes 1");
        h.lua("local s = agr.find('Area 2'):find('Props'):find('Spikes 1'):script(); s.t = 1.6 + 0.5 + 0.05; agr.find('Hero'):script().invulnerable = 0");
        h.frames(3);
    }
    CHECK(h.field("Hero", "guard") == "false");
    CHECK(h.number("Hero", "health") == h2 - 1);
    // ---- hall 3: a fall into the chasm brings the hero back to safe ground (one heart).
    h.lua("agr.find('Game'):script():startLevel(3)");
    h.lua("for _, e in ipairs(agr.find('Area 3'):find('Enemies'):children()) do if not e:name():find('^Archer') then e:setActive(false) end end");
    h.seconds(1.0);
    const double h3 = h.number("Hero", "health");
    h.lua("agr.find('Hero'):setPosition(140 - 6, 0.05, 4); agr.find('Hero'):setVelocity(0, 0, 0)");
    h.seconds(0.8);  // safe ground recorded
    h.lua("agr.find('Hero'):setPosition(140 + 0, 0.5, 4)");  // over the chasm
    h.seconds(2.5);
    CHECK(h.position("Hero").y > -1.0f);
    CHECK(h.position("Hero").x < 140.0f - 4.0f);
    CHECK(h.number("Hero", "health") <= h3 - 1);
    // Archers shoot arrows that hurt.
    h.lua("local a = agr.find('Area 3'):find('Enemies'):find('Archer 1'); local x, y, z = a:worldPosition(); agr.find('Hero'):setPosition(x - 7, 0.05, z);"
          " agr.find('Hero'):script().health = 8; agr.find('Hero'):script().invulnerable = 0; _G.sawArrow = false");
    const double h3b = h.number("Hero", "health");
    for (int k = 0; k < 20 && h.number("Hero", "health") >= h3b; ++k) {
        h.seconds(0.3);
        h.lua("if next(agr.find('Game'):script().arrows) then _G.sawArrow = true end");
    }
    ok("_G.sawArrow");
    CHECK(h.number("Hero", "health") < h3b);
    // ---- hall 4: the plates puzzle reveals the third gem; the brute charges.
    h.lua("agr.find('Game'):script():startLevel(4)");
    h.lua("for _, e in ipairs(agr.find('Area 4'):find('Enemies'):children()) do if not e:name():find('^Brute') then e:setActive(false) end end");
    h.frames(3);
    ok("not agr.find('Area 4'):find('Items'):find('Gem 3'):isActive()");
    teleport("Area 4", "Props", "Plate 1");
    teleport("Area 4", "Props", "Plate 1", -4.0f);  // step off
    teleport("Area 4", "Props", "Plate 4");
    ok("agr.find('Area 4'):find('Items'):find('Gem 3'):isActive()");
    h.lua("local b = agr.find('Area 4'):find('Enemies'):find('Brute 1'); local x, y, z = b:worldPosition(); agr.find('Hero'):setPosition(x - 5, 0.05, z);"
          " _G.charged = false");
    for (int k = 0; k < 30; ++k) {
        h.seconds(0.2);
        h.lua("local s = agr.find('Area 4'):find('Enemies'):find('Brute 1'):script(); if s.phase == 'charge' then _G.charged = true end");
    }
    ok("_G.charged");
    // ---- hall 6: the boss (minions, fury and falling boulders), then victory.
    h.lua("agr.find('Game'):script():startLevel(6)");
    h.frames(3);
    CHECK(h.active("BossBar"));
    h.lua("local b = agr.find('Area 6'):find('Enemies'):find('Boss'):script(); b:summonMinions()");
    h.frames(3);
    ok("#(function() local t = {} for _, e in ipairs(agr.find('Area 6'):find('Enemies'):children()) do if e:name() == 'Minion' then t[#t + 1] = e end end return t end)() == 2");
    h.lua("local b = agr.find('Area 6'):find('Enemies'):find('Boss'); b:script().life = 16; b:script():hit(1, agr.find('Hero'))");
    ok("agr.find('Area 6'):find('Enemies'):find('Boss'):script().furious");
    h.lua("local b = agr.find('Area 6'):find('Enemies'):find('Boss'):script(); local x, _, z = agr.find('Hero'):position(); b:rockRain(x + 20, z)");
    ok("agr.find('Area 6'):find('Enemies'):find('Boulder 1'):isActive()");
    h.seconds(1.8);
    ok("not agr.find('Area 6'):find('Enemies'):find('Boulder 1'):isActive()");
    h.lua("local b = agr.find('Area 6'):find('Enemies'):find('Boss'); b:script().life = 1; b:script():hit(1, agr.find('Hero'))");
    h.seconds(2.5);
    ok("not agr.find('Area 6'):find('Enemies'):find('Boss'):isActive()");
    ok("not agr.find('Area 6'):find('Gate'):isActive()");
    teleport("Area 6", "", "Portal", -0.5f);
    CHECK(h.field("Game", "state") == "victory");
    CHECK(h.active("Menu"));
    CHECK(h.text("MenuTitle").find("Victoire") != std::string::npos);
    ok("agr.save.get('ambre').finished == true");
    // ---- game over and retry; the continue button now starts at the furthest hall.
    h.seconds(1.5);  // the victory flash fades
    menuClick("MenuButton");
    ok("agr.find('PlayButton'):find('Label'):text():find('2')");
    menuClick("LevelsButton");
    menuClick("LevelButton 1");
    CHECK(h.number("Game", "level") == 1);
    h.lua("local hs = agr.find('Hero'):script(); hs.invulnerable = 0; hs.guard = false; hs.health = 1; hs:damage(1)");
    CHECK(h.field("Game", "state") == "dying");
    h.seconds(2.0);
    CHECK(h.field("Game", "state") == "gameover");
    menuClick("RetryButton");
    CHECK(h.field("Game", "state") == "playing");
    CHECK(h.number("Hero", "health") == 6);
    h.noScriptErrors();
}

TEST_CASE("launcher menu: a button opens its game scene (agr.loadScene)") {
    GameHarness h(SceneTemplate::GamePlatformer);
    Scene game;
    buildSceneTemplate(game, SceneTemplate::GameCards);
    std::ofstream(h.root / "game" / "scenes" / "cards.agrscene") << SceneSerializer::toString(game);
    Scene menu;
    buildLauncherScene(menu, "AGR ENGINE", {{"Cartes", "game://scenes/cards.agrscene"}});
    std::ofstream(h.root / "game" / "scenes" / "menu.agrscene") << SceneSerializer::toString(menu);
    REQUIRE(h.engine->loadScene("game://scenes/menu.agrscene"));
    h.frames(3);
    h.clickUI("Game 1");
    h.frames(2);
    CHECK(h.engine->sceneUri() == "game://scenes/cards.agrscene");
    CHECK_FALSE(h.scene().findByName("Card 1").isNull());
    h.noScriptErrors();
}

TEST_CASE("arena battle: menu, drag and tap to deploy, elixir, hand cycling, units cross the bridges and fight, towers, AI, fireball, crowns, trophies") {
    GameHarness h(SceneTemplate::GameArenaBattle);
    h.frames(5);
    h.noScriptErrors();
    auto ok = [&](const std::string& lua) { h.lua("assert(" + lua + ")"); };
    CHECK(h.field("Game", "state") == "menu");
    CHECK(h.active("Menu"));
    CHECK_FALSE(h.active("Card Tray"));
    CHECK(h.text("TrophiesText").find("100") != std::string::npos);
    h.clickUI("BattleButton");
    h.frames(2);
    CHECK(h.field("Game", "state") == "battle");
    CHECK_FALSE(h.active("Menu"));
    CHECK(h.active("Card Tray"));
    CHECK(h.text("TimerText") == "3:00");
    ok("math.floor(agr.find('Game'):script().elixir.blue) == 5");
    // Tests drive the opponent themselves.
    h.lua("local g = agr.find('Game'):script(); g.thinkAI = function() end");
    // A known hand: knight, archer, giant, goblin; next fireball, then tank.
    h.lua("local g = agr.find('Game'):script(); g.decks.blue = { hand = { 'knight', 'archer', 'giant', 'goblin' }, queue = { 'fireball', 'tank' } };"
          " g.elixir.blue = 10; g:refreshCards()");
    h.frames(2);
    ok("agr.find('Card 1'):find('Label'):text():find('Chevalier')");
    ok("agr.find('NextCard'):text():find('Boule de feu')");

    // ---- drag card 1 onto the blue half: a knight appears where the finger is released.
    const math::Vec4 r = h.scene().registry().get<UIRectComponent>(h.find("Card 1")).screenRect;
    const math::Vec2 from{r.x + r.z * 0.5f, r.y + r.w * 0.5f};
    const math::Vec2 to = h.worldToScreen(-6.0f, 0.0f, -3.0f);
    h.input().setMousePosition(from);
    h.frames(1);
    h.input().setMouseButton(MouseButton::Left, true);
    h.frames(2);
    for (int i = 1; i <= 12; ++i) {
        h.input().setMousePosition(from + (to - from) * (static_cast<float>(i) / 12.0f));
        h.frames(1);
        if (i == 6) {
            CHECK(h.active("Drop Marker"));
            CHECK(h.active("Deploy Zone"));
        }
    }
    h.input().setMouseButton(MouseButton::Left, false);
    h.frames(3);
    CHECK_FALSE(h.active("Drop Marker"));
    ok("#agr.find('Game'):script().units.blue == 1");
    ok("math.abs(agr.find('Game'):script().elixir.blue - 7) < 0.2");
    h.lua("local u = agr.find('Game'):script().units.blue[1]; local x, _, z = u:position();"
          " assert(u:name():find('^Knight blue'), u:name()); assert(math.abs(x + 6) < 0.8 and math.abs(z + 3) < 0.8, x .. ' ' .. z)");
    // The played card goes to the back of the queue; the next one takes its slot.
    ok("agr.find('Game'):script().decks.blue.hand[1] == 'fireball'");
    ok("agr.find('NextCard'):text():find('Mini')");

    // ---- tap a card, then tap the arena: archers (2). The enemy half is refused.
    h.lua("agr.find('Game'):script().elixir.blue = 10; agr.find('Game'):script():refreshCards()");
    h.clickUI("Card 2");
    ok("agr.find('Game'):script().selected == 2");
    h.clickWorld(5.0f, 0.0f, 0.0f);
    ok("agr.find('Game'):script().selected == 2");  // refused: still selected
    ok("#agr.find('Game'):script().units.blue == 1");
    h.clickWorld(-7.0f, 0.0f, 3.0f);
    h.frames(2);
    ok("agr.find('Game'):script().selected == nil");
    ok("#agr.find('Game'):script().units.blue == 3");
    // A card costing more than the elixir is disabled.
    h.lua("local g = agr.find('Game'):script(); g.elixir.blue = 1; g:refreshCards()");
    ok("agr.find('Card 3'):get('UIButton', 'interactable') == false");
    // Elixir refills: about 1 every 2.8 s.
    h.seconds(2.9);
    ok("agr.find('Game'):script().elixir.blue >= 2");

    // ---- the units walk to the enemy side across a bridge and attack a princess tower (which shoots back).
    h.lua("FAR = -99; STATES = {}");
    for (int i = 0; i < 40; ++i) {
        h.seconds(0.5);
        h.lua("for _, u in ipairs(agr.find('Game'):script().units.blue) do local x = u:position(); FAR = math.max(FAR, x)"
              " if u:has('Animator') then STATES[u:currentState()] = true end end");
    }
    ok("FAR > 4");
    // The cartoon characters (generated .glb with Idle / Walk / Attack) are animated by the game.
    ok("STATES.Locomotion == true");  // Idle / Walk blended by speed
    ok("STATES.Attack == true");
    ok("agr.find('Red Princess N'):script().life < 1400 or agr.find('Red Princess S'):script().life < 1400");

    // ---- the enemy towers shoot: an AI knight dropped in front of a blue tower dies to it.
    h.lua("local g = agr.find('Game'):script(); g:clearUnits(); g.elixir.red = 10;"
          " g.decks.red = { hand = { 'knight', 'goblin', 'giant', 'fireball' }, queue = { 'archer', 'tank' } }; assert(g:play('red', 2, 1.6, -4.5))");
    h.frames(3);
    ok("#agr.find('Game'):script().units.red == 3");
    ok("agr.find('Game'):script():play('red', 1, -3, 0) == false");  // red cannot deploy on the blue side
    h.seconds(12.0);
    ok("#agr.find('Game'):script().units.red < 3");

    // ---- the AI plays on its own.
    h.lua("local g = agr.find('Game'):script(); g:clearUnits(); g.thinkAI = nil; g.elixir.red = 10; g.aiThreshold = 6");
    h.seconds(1.5);
    ok("#agr.find('Game'):script().units.red >= 1 or agr.find('Game'):script().elixir.red < 6");
    h.lua("local g = agr.find('Game'):script(); g.thinkAI = function() end; g:clearUnits()");

    // ---- fireball: area damage on enemy units and towers.
    h.lua("local g = agr.find('Game'):script(); g.decks.blue.hand[1] = 'fireball'; g.elixir.blue = 10;"
          " assert(g:play('blue', 1, 9, -4.5))");
    h.seconds(1.0);
    ok("agr.find('Red Princess N'):script().life <= 1400 - 130");

    // ---- a princess tower falls: one crown, the king wakes up, the lane opens for deploying.
    h.lua("agr.find('Red Princess N'):script():damage(5000)");
    h.frames(3);
    CHECK_FALSE(h.active("Red Princess N"));
    ok("agr.find('Game'):script().crowns.blue == 1");
    ok("agr.find('Red King'):script().awake == true");
    CHECK(h.text("CrownsText").find("1 - 0") != std::string::npos);
    ok("agr.find('Game'):script():canDeploy('blue', { cost = 3 }, 4, -4) == true");
    ok("agr.find('Game'):script():canDeploy('blue', { cost = 3 }, 4, 4) == false");
    // ---- the king tower falls: three crowns, victory, +30 trophies, saved.
    h.lua("agr.find('Red King'):script():damage(99999)");
    h.frames(3);
    CHECK(h.field("Game", "state") == "over");
    CHECK(h.active("Menu"));
    CHECK(h.text("MenuTitle").find("Victoire") != std::string::npos);
    ok("agr.save.get('duel.trophies') == 130");
    CHECK(h.text("TrophiesText").find("130") != std::string::npos);

    // ---- a new battle resets the towers; time out with a crown down = defeat (-20).
    h.clickUI("BattleButton");
    h.frames(3);
    CHECK(h.field("Game", "state") == "battle");
    CHECK(h.active("Red Princess N"));
    CHECK(h.active("Red King"));
    ok("agr.find('Red King'):script().awake == false");
    ok("#agr.find('Units'):children() == 0");
    h.lua("local g = agr.find('Game'):script(); g.thinkAI = function() end; agr.find('Blue Princess S'):script():damage(99999)");
    h.frames(2);
    ok("agr.find('Game'):script().crowns.red == 1");
    h.lua("agr.find('Game'):script().time = 61");
    h.seconds(1.5);
    ok("agr.find('Game'):script().double == true");
    h.lua("agr.find('Game'):script().time = 0.1");
    h.frames(10);
    CHECK(h.field("Game", "state") == "over");
    CHECK(h.text("MenuTitle").find("Défaite") != std::string::npos);
    ok("agr.save.get('duel.trophies') == 110");
    // ---- tie at the end of time: sudden-death overtime, then a draw.
    h.clickUI("BattleButton");
    h.frames(3);
    h.lua("local g = agr.find('Game'):script(); g.thinkAI = function() end; g.time = 0.05");
    h.frames(5);
    ok("agr.find('Game'):script().overtime == true");
    CHECK(h.field("Game", "state") == "battle");
    h.lua("agr.find('Game'):script().time = 0.05");
    h.frames(5);
    CHECK(h.text("MenuTitle").find("galit") != std::string::npos);
    h.noScriptErrors();
}

TEST_CASE("strategy camera script: drag keeps the ground under the finger, inertia, pinch zoom, bounds, UI ignored") {
    GameHarness h(SceneTemplate::GameArenaBattle);
    h.frames(3);
    h.lua("agr.find('Menu'):setActive(false); agr.find('Card Tray'):setActive(false)");
    auto& sc = h.scene().registry().emplace<ScriptComponent>(h.find("Main Camera"));
    sc.script = "assets://scripts/strategy_camera.lua";
    sc.properties = R"({"minX": -6, "maxX": 6, "minY": -10, "maxY": 6, "maxZoom": 2})";  // room for the flick below
    h.frames(3);
    auto ok = [&](const std::string& lua) { h.lua("assert(" + lua + ")"); };
    ok("agr.find('Main Camera'):script() ~= nil");
    const math::Vec2 size = h.screen();
    const math::Vec2 c = size * 0.5f, to = c + math::Vec2{-150.0f, 60.0f};
    const math::Vec3 p0 = h.position("Main Camera");
    h.lua("W0 = { agr.camera.screenToWorld(" + std::to_string(c.x) + ", " + std::to_string(c.y) + ", 0) }");
    // ---- slow drag: the ground point under the finger follows it; no drift once held still.
    h.input().setMousePosition(c);
    h.frames(1);
    h.input().setMouseButton(MouseButton::Left, true);
    h.frames(1);
    for (int i = 1; i <= 15; ++i) {
        h.input().setMousePosition(c + (to - c) * (static_cast<float>(i) / 15.0f));
        h.frames(1);
    }
    h.frames(6);
    h.input().setMouseButton(MouseButton::Left, false);
    h.frames(2);
    const math::Vec3 p1 = h.position("Main Camera");
    CHECK(p1.x > p0.x + 1.0f);  // the finger went left: the view moves right
    CHECK(p1.y == doctest::Approx(p0.y));
    h.lua("local x, _, z = agr.camera.screenToWorld(" + std::to_string(to.x) + ", " + std::to_string(to.y) + ", 0);"
          " assert(math.abs(x - W0[1]) < 0.2 and math.abs(z - W0[3]) < 0.2, string.format('%.2f %.2f vs %.2f %.2f', x, z, W0[1], W0[3]))");
    h.frames(30);
    CHECK(h.position("Main Camera").x == doctest::Approx(p1.x).epsilon(0.001));  // held still before release: no momentum
    // ---- flick: the view keeps going after the release, then stops.
    h.input().setMousePosition(c);
    h.frames(1);
    h.input().setMouseButton(MouseButton::Left, true);
    h.frames(1);
    for (int i = 1; i <= 4; ++i) {
        h.input().setMousePosition(c + math::Vec2{0.0f, 25.0f * static_cast<float>(i)});
        h.frames(1);
    }
    h.input().setMouseButton(MouseButton::Left, false);
    h.frames(1);
    const math::Vec3 released = h.position("Main Camera");
    h.frames(20);
    const math::Vec3 coasted = h.position("Main Camera");
    CHECK(coasted.z < released.z - 0.2f);  // finger down = the view goes up the screen (-z)
    h.frames(120);
    const math::Vec3 rest = h.position("Main Camera");
    h.frames(10);
    CHECK(h.position("Main Camera").z == doctest::Approx(rest.z).epsilon(0.001));
    // ---- pinch with two fingers: zoom in (closer to the ground), clamped to maxZoom.
    h.input().touchDown(1, c - math::Vec2{60.0f, 0.0f}, 1.0f);
    h.input().touchDown(2, c + math::Vec2{60.0f, 0.0f}, 1.0f);
    h.frames(2);
    for (int i = 1; i <= 10; ++i) {
        h.input().touchMove(1, c - math::Vec2{60.0f + 6.0f * static_cast<float>(i), 0.0f}, 1.0f);
        h.input().touchMove(2, c + math::Vec2{60.0f + 6.0f * static_cast<float>(i), 0.0f}, 1.0f);
        h.frames(1);
    }
    h.input().touchUp(1, c - math::Vec2{120.0f, 0.0f});
    h.input().touchUp(2, c + math::Vec2{120.0f, 0.0f});
    h.frames(2);
    const double zoom = std::stod(h.field("Main Camera", "zoom"));
    CHECK(zoom == doctest::Approx(2.0).epsilon(0.05));
    CHECK(h.position("Main Camera").y < p0.y * 0.6f);
    // ---- bounds: a long drag stops at maxX.
    h.input().setMousePosition({size.x - 20.0f, c.y});
    h.frames(1);
    h.input().setMouseButton(MouseButton::Left, true);
    h.frames(1);
    for (int i = 1; i <= 20; ++i) {
        h.input().setMousePosition({size.x - 20.0f - (size.x - 40.0f) * static_cast<float>(i) / 20.0f, c.y});
        h.frames(1);
    }
    h.frames(3);
    h.input().setMouseButton(MouseButton::Left, false);
    h.frames(2);
    CHECK(std::stod(h.field("Main Camera", "fx")) == doctest::Approx(6.0));
    // ---- a drag that starts on the interface (a menu) does not move the view.
    h.lua("agr.find('Menu'):setActive(true)");
    h.frames(2);
    const math::Vec3 before = h.position("Main Camera");
    h.input().setMousePosition(c);
    h.frames(1);
    h.input().setMouseButton(MouseButton::Left, true);
    h.frames(1);
    for (int i = 1; i <= 8; ++i) {
        h.input().setMousePosition(c + math::Vec2{20.0f * static_cast<float>(i), 0.0f});
        h.frames(1);
    }
    h.input().setMouseButton(MouseButton::Left, false);
    h.frames(10);
    CHECK(h.position("Main Camera").x == doctest::Approx(before.x));
    CHECK(h.position("Main Camera").z == doctest::Approx(before.z));
    h.noScriptErrors();
}

TEST_CASE("scripts drive physics: character controller, layer names, raycast masks, joints and onJointBreak") {
    GameHarness h(SceneTemplate::Empty3D);
    const fs::path assets = h.root / "game" / "assets";
    {
        std::ofstream(assets / "physics_layers.json") << R"({"layers": ["Default", "Player", "Enemies", "Ghosts"]})";
        std::ofstream(assets / "hero_test.lua") << R"(
local Hero = {}
function Hero:onStart() self.jumps = 0 end
function Hero:onUpdate(dt)
    local _, vy, _ = self.entity:velocity()
    if self.entity:isGrounded() and self.wantJump then
        vy = 5
        self.wantJump = false
        self.jumps = self.jumps + 1
    end
    self.entity:setVelocity(2, vy, 0)
end
return Hero
)";
        std::ofstream(assets / "rope_test.lua") << R"(
local Rope = {}
function Rope:onJointBreak(force) self.brokeWith = force end
return Rope
)";
    }
    Scene& sc = h.scene();
    auto& reg = sc.registry();
    const auto ground = sc.createEntity("Ground");
    reg.get<TransformComponent>(ground).position = {0, -0.5f, 0};
    reg.emplace<ColliderComponent>(ground).size = {100, 1, 100};
    const auto hero = sc.createEntity("Hero");
    reg.get<TransformComponent>(hero).position = {0, 0.5f, 0};
    reg.emplace<CharacterController3DComponent>(hero).layer = 1;
    reg.emplace<ScriptComponent>(hero).script = "assets://hero_test.lua";
    const auto enemy = sc.createEntity("Enemy");
    reg.get<TransformComponent>(enemy).position = {0, 0.5f, 10};
    reg.emplace<ColliderComponent>(enemy).layer = 2;
    const auto weight = sc.createEntity("Weight");
    reg.get<TransformComponent>(weight).position = {-20, 7, 0};
    reg.emplace<RigidBodyComponent>(weight).mass = 1.0f;
    reg.emplace<ColliderComponent>(weight).size = {0.5f, 0.5f, 0.5f};
    auto& rope = reg.emplace<Joint3DComponent>(weight);
    rope.type = JointType3D::Distance;
    rope.autoConnectedAnchor = false;
    rope.connectedAnchor = {-20, 10, 0};
    rope.breakForce = 50.0f;
    reg.emplace<ScriptComponent>(weight).script = "assets://rope_test.lua";
    h.seconds(1.0);
    auto ok = [&](const std::string& lua) { h.lua("assert(" + lua + ")"); };
    // Layer names and masks.
    ok("agr.physics3d.layer('Enemies') == 2");
    ok("agr.physics3d.mask('Player', 'Ghosts') == (1 << 1) | (1 << 3)");
    ok("agr.physics2d.layer('Default') == 0");
    h.lua("local okCall = pcall(agr.physics3d.layer, 'Nope'); assert(not okCall)");
    // Raycast masks by name / table / number.
    ok("agr.physics3d.raycast(0, 0.5, 15, 0, 0, -1, 20, 'Enemies').entity:name() == 'Enemy'");
    ok("agr.physics3d.raycast(0, 0.5, 15, 0, 0, -1, 20, {'Default'}) == nil");
    // The character walks (velocity from the script) on the ground and jumps once.
    ok("agr.find('Hero'):isGrounded()");
    CHECK(h.position("Hero").x > 1.0f);
    CHECK(std::abs(h.position("Hero").y) < 0.05f);
    h.lua("agr.find('Hero'):script().wantJump = true");
    h.seconds(0.3);
    CHECK(h.position("Hero").y > 0.5f);
    h.lua("assert(not agr.find('Hero'):isGrounded())");
    h.seconds(1.0);
    ok("agr.find('Hero'):isGrounded() and agr.find('Hero'):script().jumps == 1");
    ok("select(2, agr.find('Hero'):groundNormal()) > 0.99");
    // Joints from scripts: value, then breaking calls onJointBreak.
    ok("math.abs(agr.find('Weight'):jointValue() - 3) < 0.05");
    h.lua("agr.find('Weight'):set('RigidBody', 'mass', 100)");
    h.seconds(0.5);
    ok("agr.find('Weight'):script().brokeWith ~= nil and agr.find('Weight'):script().brokeWith > 50");
    ok("agr.find('Weight'):get('Joint3D', 'broken') == true");
    ok("agr.find('Weight'):jointValue() == nil");
    h.noScriptErrors();
}

}

namespace {
/// The ```lua blocks of a tutorial whose first line is "-- assets/<path>": path -> code.
std::vector<std::pair<std::string, std::string>> tutorialScripts(const std::string& doc) {
    std::ifstream in(fs::path(AGR_SOURCE_DIR) / "docs" / doc);
    const std::string text((std::istreambuf_iterator<char>(in)), {});
    std::vector<std::pair<std::string, std::string>> out;
    for (usize at = text.find("```lua\n"); at != std::string::npos; at = text.find("```lua\n", at + 1)) {
        const usize start = at + 7, end = text.find("```", start);
        const std::string code = text.substr(start, end - start);
        if (code.rfind("-- assets/", 0) == 0) out.emplace_back(code.substr(10, code.find('\n') - 10), code);
    }
    return out;
}
} // namespace

TEST_CASE("tutorial 2D (docs/TUTORIAL_2D.md): its scripts lint clean, the player lands, runs and collects the coin") {
    GameHarness h(SceneTemplate::Empty2D);
    const fs::path assets = h.root / "game" / "assets";
    const auto scripts = tutorialScripts("TUTORIAL_2D.md");
    REQUIRE(scripts.size() == 2);
    for (const auto& [path, code] : scripts) {
        INFO(path);
        CHECK_FALSE(h.scripts().checkSyntax(code, path));
        for (const auto& w : h.scripts().lint(code, path)) CHECK_MESSAGE(false, "line ", w.line, ": ", w.message);
        fs::create_directories((assets / path).parent_path());
        std::ofstream(assets / path) << code;
    }
    // The scene of the tutorial, step by step.
    Scene& sc = h.scene();
    auto& reg = sc.registry();
    const auto ground = sc.createEntity("Sol");
    reg.get<TransformComponent>(ground).position = {0, -1, 0};
    reg.get<TransformComponent>(ground).scale = {12, 1, 1};
    reg.emplace<SpriteRendererComponent>(ground);
    reg.emplace<Collider2DComponent>(ground);
    const auto player = sc.createEntity("Joueur");
    reg.get<TransformComponent>(player).position = {0, 2, 0};
    reg.emplace<SpriteRendererComponent>(player);
    reg.emplace<CharacterController2DComponent>(player).size = {0.8f, 1.0f};
    reg.emplace<ScriptComponent>(player).script = "assets://scripts/joueur.lua";
    const auto canvas = sc.createEntity("Canvas");
    reg.emplace<UICanvasComponent>(canvas);
    const auto score = sc.createEntity("Score", canvas);
    reg.emplace<UIRectComponent>(score);
    reg.emplace<UITextComponent>(score).text = "Pièces : 0";
    const auto coin = sc.createEntity("Pièce");
    reg.get<TransformComponent>(coin).position = {3, -0.2f, 0};
    reg.get<TransformComponent>(coin).scale = {0.4f, 0.4f, 1};
    reg.emplace<SpriteRendererComponent>(coin);
    reg.emplace<Collider2DComponent>(coin).isTrigger = true;
    reg.emplace<ScriptComponent>(coin).script = "assets://scripts/piece.lua";

    h.seconds(1.5);  // falls and lands on the ground
    const float landedY = reg.get<TransformComponent>(player).position.y;
    CHECK_MESSAGE(landedY > -0.2f, landedY);
    CHECK_MESSAGE(landedY < 0.2f, landedY);
    CHECK(h.text("Score") == "Pièces : 0");
    h.holdKey(Key::Right, 0.75);  // runs right through the coin (the ground ends at x = 6)
    h.seconds(0.2);
    CHECK(reg.get<TransformComponent>(h.find("Joueur")).position.x > 3.5f);
    CHECK(h.scene().findByName("Pièce").isNull());
    CHECK(h.text("Score") == "Pièces : 1");
    // Jump: the controller leaves the floor.
    h.key(Key::Space, true);
    h.frames(1);
    h.key(Key::Space, false);
    h.seconds(0.25);
    CHECK(h.scene().registry().get<TransformComponent>(h.find("Joueur")).position.y > landedY + 1.0f);
    CHECK(h.scripts().stats().errors == 0);
}

TEST_CASE("tutorial 3D (docs/TUTORIAL_3D.md): scripts lint clean, the hero runs and collects an orb, the enemy goes around the wall and catches him") {
    GameHarness h(SceneTemplate::Empty3D);
    const fs::path assets = h.root / "game" / "assets";
    const auto scripts = tutorialScripts("TUTORIAL_3D.md");
    REQUIRE(scripts.size() == 3);
    for (const auto& [path, code] : scripts) {
        INFO(path);
        CHECK_FALSE(h.scripts().checkSyntax(code, path));
        for (const auto& w : h.scripts().lint(code, path)) CHECK_MESSAGE(false, "line ", w.line, ": ", w.message);
        fs::create_directories((assets / path).parent_path());
        std::ofstream(assets / path) << code;
    }
    Scene& sc = h.scene();
    auto& reg = sc.registry();
    sc.destroyEntity(h.find("Cube"));
    const auto ground = h.find("Ground");
    reg.get<NameComponent>(ground).name = "Sol";
    reg.get<TransformComponent>(ground).scale = {30, 1, 30};
    reg.emplace<ColliderComponent>(ground).size = {1, 0.1f, 1};
    const auto hero = sc.createEntity("Héros");
    reg.get<TransformComponent>(hero).position = {0, 1, 0};
    reg.emplace<MeshRendererComponent>(hero);
    reg.emplace<CharacterController3DComponent>(hero);
    reg.emplace<ScriptComponent>(hero).script = "assets://scripts/heros.lua";
    const auto canvas = sc.createEntity("Canvas");
    reg.emplace<UICanvasComponent>(canvas);
    const auto score = sc.createEntity("Score", canvas);
    reg.emplace<UIRectComponent>(score);
    reg.emplace<UITextComponent>(score).text = "Orbes : 0";
    const auto orb = sc.createEntity("Orbe");
    reg.get<TransformComponent>(orb).position = {4, 0.8f, 0};
    reg.get<TransformComponent>(orb).scale = {0.6f, 0.6f, 0.6f};
    reg.emplace<MeshRendererComponent>(orb).mesh = PrimitiveMesh::Sphere;
    auto& orbCollider = reg.emplace<ColliderComponent>(orb);
    orbCollider.shape = ColliderShape3D::Sphere;
    orbCollider.isTrigger = true;
    reg.emplace<ScriptComponent>(orb).script = "assets://scripts/orbe.lua";

    h.seconds(1.0);  // lands
    const float y0 = reg.get<TransformComponent>(h.find("Héros")).position.y;
    CHECK_MESSAGE(std::abs(y0) < 0.3f, y0);
    CHECK(reg.get<TransformComponent>(h.find("Orbe")).rotation.y != doctest::Approx(0.0f));  // spins
    h.holdKey(Key::Right, 1.0);  // 5 m/s to the right, through the orb
    h.seconds(0.2);
    CHECK(reg.get<TransformComponent>(h.find("Héros")).position.x > 4.0f);
    CHECK(h.scene().findByName("Orbe").isNull());
    CHECK(h.text("Score") == "Orbes : 1");

    // The wall and the chasing enemy: it goes around the wall (|z| > 4 at some point) and catches the hero.
    const auto wall = sc.createEntity("Mur");
    reg.get<TransformComponent>(wall).position = {-3, 1, 0};
    reg.get<TransformComponent>(wall).scale = {1, 2, 8};
    reg.emplace<MeshRendererComponent>(wall);
    reg.emplace<ColliderComponent>(wall);
    reg.get<TransformComponent>(h.find("Héros")).position = {2, 0, 0};
    const auto enemy = sc.createEntity("Ennemi");
    reg.get<TransformComponent>(enemy).position = {-8, 0.5f, 0};
    reg.emplace<MeshRendererComponent>(enemy);
    reg.emplace<NavAgentComponent>(enemy).speed = 3.0f;
    reg.emplace<ScriptComponent>(enemy).script = "assets://scripts/ennemi.lua";
    float widest = 0.0f;
    for (int i = 0; i < 12 * 60 && h.text("Score") != "Attrapé !"; ++i) {
        h.frames(1);
        const ecs::Entity e = h.find("Ennemi");
        widest = std::max(widest, std::abs(h.scene().registry().get<TransformComponent>(e).position.z));
    }
    CHECK(h.text("Score") == "Attrapé !");
    CHECK_MESSAGE(widest > 4.0f, widest);  // went around the 8 m wall
    h.lua("agr.time.setScale(1)");
    CHECK(h.scripts().stats().errors == 0);
}

TEST_CASE("tutorial 2D (English, docs/TUTORIAL_2D_EN.md): its scripts lint clean, the player lands, runs and collects the coin") {
    GameHarness h(SceneTemplate::Empty2D);
    const fs::path assets = h.root / "game" / "assets";
    const auto scripts = tutorialScripts("TUTORIAL_2D_EN.md");
    REQUIRE(scripts.size() == 2);
    for (const auto& [path, code] : scripts) {
        INFO(path);
        CHECK_FALSE(h.scripts().checkSyntax(code, path));
        for (const auto& w : h.scripts().lint(code, path)) CHECK_MESSAGE(false, "line ", w.line, ": ", w.message);
        fs::create_directories((assets / path).parent_path());
        std::ofstream(assets / path) << code;
    }
    // The scene of the tutorial, step by step.
    Scene& sc = h.scene();
    auto& reg = sc.registry();
    const auto ground = sc.createEntity("Floor");
    reg.get<TransformComponent>(ground).position = {0, -1, 0};
    reg.get<TransformComponent>(ground).scale = {12, 1, 1};
    reg.emplace<SpriteRendererComponent>(ground);
    reg.emplace<Collider2DComponent>(ground);
    const auto player = sc.createEntity("Player");
    reg.get<TransformComponent>(player).position = {0, 2, 0};
    reg.emplace<SpriteRendererComponent>(player);
    reg.emplace<CharacterController2DComponent>(player).size = {0.8f, 1.0f};
    reg.emplace<ScriptComponent>(player).script = "assets://scripts/player.lua";
    const auto canvas = sc.createEntity("Canvas");
    reg.emplace<UICanvasComponent>(canvas);
    const auto score = sc.createEntity("Score", canvas);
    reg.emplace<UIRectComponent>(score);
    reg.emplace<UITextComponent>(score).text = "Coins: 0";
    const auto coin = sc.createEntity("Coin");
    reg.get<TransformComponent>(coin).position = {3, -0.2f, 0};
    reg.get<TransformComponent>(coin).scale = {0.4f, 0.4f, 1};
    reg.emplace<SpriteRendererComponent>(coin);
    reg.emplace<Collider2DComponent>(coin).isTrigger = true;
    reg.emplace<ScriptComponent>(coin).script = "assets://scripts/coin.lua";

    h.seconds(1.5);  // falls and lands on the ground
    const float landedY = reg.get<TransformComponent>(player).position.y;
    CHECK_MESSAGE(landedY > -0.2f, landedY);
    CHECK_MESSAGE(landedY < 0.2f, landedY);
    CHECK(h.text("Score") == "Coins: 0");
    h.holdKey(Key::Right, 0.75);  // runs right through the coin (the ground ends at x = 6)
    h.seconds(0.2);
    CHECK(reg.get<TransformComponent>(h.find("Player")).position.x > 3.5f);
    CHECK(h.scene().findByName("Coin").isNull());
    CHECK(h.text("Score") == "Coins: 1");
    // Jump: the controller leaves the floor.
    h.key(Key::Space, true);
    h.frames(1);
    h.key(Key::Space, false);
    h.seconds(0.25);
    CHECK(h.scene().registry().get<TransformComponent>(h.find("Player")).position.y > landedY + 1.0f);
    CHECK(h.scripts().stats().errors == 0);
}

TEST_CASE("tutorial 3D (English, docs/TUTORIAL_3D_EN.md): scripts lint clean, the hero runs and collects an orb, the enemy goes around the wall and catches him") {
    GameHarness h(SceneTemplate::Empty3D);
    const fs::path assets = h.root / "game" / "assets";
    const auto scripts = tutorialScripts("TUTORIAL_3D_EN.md");
    REQUIRE(scripts.size() == 3);
    for (const auto& [path, code] : scripts) {
        INFO(path);
        CHECK_FALSE(h.scripts().checkSyntax(code, path));
        for (const auto& w : h.scripts().lint(code, path)) CHECK_MESSAGE(false, "line ", w.line, ": ", w.message);
        fs::create_directories((assets / path).parent_path());
        std::ofstream(assets / path) << code;
    }
    Scene& sc = h.scene();
    auto& reg = sc.registry();
    sc.destroyEntity(h.find("Cube"));
    const auto ground = h.find("Ground");
    reg.get<NameComponent>(ground).name = "Floor";
    reg.get<TransformComponent>(ground).scale = {30, 1, 30};
    reg.emplace<ColliderComponent>(ground).size = {1, 0.1f, 1};
    const auto hero = sc.createEntity("Hero");
    reg.get<TransformComponent>(hero).position = {0, 1, 0};
    reg.emplace<MeshRendererComponent>(hero);
    reg.emplace<CharacterController3DComponent>(hero);
    reg.emplace<ScriptComponent>(hero).script = "assets://scripts/hero.lua";
    const auto canvas = sc.createEntity("Canvas");
    reg.emplace<UICanvasComponent>(canvas);
    const auto score = sc.createEntity("Score", canvas);
    reg.emplace<UIRectComponent>(score);
    reg.emplace<UITextComponent>(score).text = "Orbs: 0";
    const auto orb = sc.createEntity("Orb");
    reg.get<TransformComponent>(orb).position = {4, 0.8f, 0};
    reg.get<TransformComponent>(orb).scale = {0.6f, 0.6f, 0.6f};
    reg.emplace<MeshRendererComponent>(orb).mesh = PrimitiveMesh::Sphere;
    auto& orbCollider = reg.emplace<ColliderComponent>(orb);
    orbCollider.shape = ColliderShape3D::Sphere;
    orbCollider.isTrigger = true;
    reg.emplace<ScriptComponent>(orb).script = "assets://scripts/orb.lua";

    h.seconds(1.0);  // lands
    const float y0 = reg.get<TransformComponent>(h.find("Hero")).position.y;
    CHECK_MESSAGE(std::abs(y0) < 0.3f, y0);
    CHECK(reg.get<TransformComponent>(h.find("Orb")).rotation.y != doctest::Approx(0.0f));  // spins
    h.holdKey(Key::Right, 1.0);  // 5 m/s to the right, through the orb
    h.seconds(0.2);
    CHECK(reg.get<TransformComponent>(h.find("Hero")).position.x > 4.0f);
    CHECK(h.scene().findByName("Orb").isNull());
    CHECK(h.text("Score") == "Orbs: 1");

    // The wall and the chasing enemy: it goes around the wall (|z| > 4 at some point) and catches the hero.
    const auto wall = sc.createEntity("Wall");
    reg.get<TransformComponent>(wall).position = {-3, 1, 0};
    reg.get<TransformComponent>(wall).scale = {1, 2, 8};
    reg.emplace<MeshRendererComponent>(wall);
    reg.emplace<ColliderComponent>(wall);
    reg.get<TransformComponent>(h.find("Hero")).position = {2, 0, 0};
    const auto enemy = sc.createEntity("Enemy");
    reg.get<TransformComponent>(enemy).position = {-8, 0.5f, 0};
    reg.emplace<MeshRendererComponent>(enemy);
    reg.emplace<NavAgentComponent>(enemy).speed = 3.0f;
    reg.emplace<ScriptComponent>(enemy).script = "assets://scripts/enemy.lua";
    float widest = 0.0f;
    for (int i = 0; i < 12 * 60 && h.text("Score") != "Caught!"; ++i) {
        h.frames(1);
        const ecs::Entity e = h.find("Enemy");
        widest = std::max(widest, std::abs(h.scene().registry().get<TransformComponent>(e).position.z));
    }
    CHECK(h.text("Score") == "Caught!");
    CHECK_MESSAGE(widest > 4.0f, widest);  // went around the 8 m wall
    h.lua("agr.time.setScale(1)");
    CHECK(h.scripts().stats().errors == 0);
}

TEST_CASE("background scene loading: agr.loadSceneAsync, progress, automatic or manual switch, failures") {
    GameHarness h(SceneTemplate::Empty2D);
    // A big level saved next to the game's scenes.
    {
        Scene big;
        for (int i = 0; i < 3000; ++i) {
            const auto e = big.createEntity(i == 2999 ? "LastOfTheBigLevel" : "Rock");
            big.registry().get<TransformComponent>(e).position = {static_cast<float>(i % 60), static_cast<float>(i / 60), 0.0f};
        }
        REQUIRE(h.engine->vfs().writeText("user://big.agrscene", SceneSerializer::toString(big)));
    }
    // Manual switch: ready, but the current scene stays until agr.switchScene().
    h.lua("agr.loadSceneAsync('user://big.agrscene', false)");
    std::string status;
    for (int i = 0; i < 1000 && status != "ready"; ++i) {  // up to 10 s of real time
        h.frames(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        status = h.engine->sceneLoadState().ready ? "ready" : "";
    }
    INFO("failed: ", h.engine->sceneLoadState().failed, " error: ", h.engine->sceneLoadState().error, " progress: ", h.engine->sceneLoadState().progress);
    REQUIRE(status == "ready");
    CHECK(h.scene().findByName("LastOfTheBigLevel").isNull());
    h.lua("local p, s = agr.sceneLoadProgress(); assert(p == 1 and s == 'ready', s)");
    h.lua("agr.switchScene()");
    h.frames(1);
    CHECK_FALSE(h.scene().findByName("LastOfTheBigLevel").isNull());
    CHECK(h.scene().entityCount() >= 3000);
    h.lua("local _, s = agr.sceneLoadProgress(); assert(s == 'none', s)");
    // Automatic switch (default).
    REQUIRE(h.engine->vfs().writeText("user://small.agrscene", SceneSerializer::toString(Scene{})));
    h.lua("agr.loadSceneAsync('user://small.agrscene')");
    for (int i = 0; i < 1000 && !h.scene().findByName("LastOfTheBigLevel").isNull(); ++i) {
        h.frames(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(h.scene().findByName("LastOfTheBigLevel").isNull());
    // A missing file: failed with the reason, the scene stays.
    h.lua("agr.loadSceneAsync('user://nope.agrscene')");
    for (int i = 0; i < 1000 && !h.engine->sceneLoadState().failed; ++i) {
        h.frames(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    h.lua("local p, s, err = agr.sceneLoadProgress(); assert(s == 'failed' and err ~= nil, s)");
}

TEST_CASE("Lua remapping: capture a key, rebind jump, save the override, reset; pad rumble without a pad is false") {
    GameHarness h(SceneTemplate::GamePlatformer);
    h.frames(2);
    h.lua("assert(agr.input.capture() == nil)");
    h.lua("local b = agr.input.bindings('jump'); assert(#b >= 1 and b[1].text ~= nil)");
    h.key(Key::W, true);
    h.lua("C = agr.input.capture(); assert(C and C.type == 'key' and C.input == 'W', C and C.input)");
    h.lua("agr.input.setBindings('jump', { C })");
    h.frames(1);
    h.lua("assert(agr.input.down('jump'))");
    h.key(Key::W, false);
    h.key(Key::Space, true);
    h.frames(1);
    h.lua("assert(not agr.input.down('jump'))");
    h.key(Key::Space, false);
    h.lua("agr.input.saveBindings(); local o = agr.save.get('agr.input'); assert(o.jump[1].input == 'W')");
    h.lua("agr.input.resetBindings(); agr.input.saveBindings(); assert(next(agr.save.get('agr.input')) == nil)");
    h.lua("assert(agr.input.vibrate(1, 0.5, 0.1) == false)");
    h.lua("local ok = pcall(agr.input.setBindings, 'jump', { {type = 'key', input = 'Nope'} }); assert(not ok)");
}
