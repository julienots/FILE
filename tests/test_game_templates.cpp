// Game templates are played headless through the real engine (every plugin, the project's
// copied assets, the startup scene saved and reloaded) with simulated keys, clicks and taps.
#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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
    h.lua(R"(local g = agr.find("Game"):script(); local p = g.players[1]; g.coinX, g.coinY = p.x, p.y)");
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

}
