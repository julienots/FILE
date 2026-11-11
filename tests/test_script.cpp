#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>

#include "agr/audio/Wav.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/runtime/PlatformLayer.hpp"
#include "agr/runtime/SaveStore.hpp"
#include "agr/physics2d/Physics2DWorld.hpp"
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/runtime/InputActions.hpp"
#include "agr/runtime/Localization.hpp"
#include "agr/scene/Prefab.hpp"
#include "agr/script/ScriptEngine.hpp"

using namespace agr;
namespace fs = std::filesystem;

namespace {

struct ScriptFixture {
    fs::path root = fs::temp_directory_path() / ("agr_script_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    Scene scene;
    Input input;
    ScriptEngine scripts{vfs};
    FrameTime time;

    ScriptFixture() {
        fs::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        registerRenderComponents();
        registerPhysics2DComponents();
        registerScriptComponent();
        scripts.setInput(&input);
        time.deltaSeconds = 0.1;
        time.fixedDeltaSeconds = 1.0 / 60.0;
    }
    ~ScriptFixture() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    void write(const std::string& uri, const std::string& code) { REQUIRE(vfs.writeText(uri, code)); }
    /// Rewrites a file and moves its date forward (hot reload detection).
    void rewrite(const std::string& relative, const std::string& code) {
        write("assets://" + relative, code);
        const fs::path p = root / relative;
        fs::last_write_time(p, fs::last_write_time(p) + std::chrono::seconds(5));
    }
    ecs::Entity scripted(const char* name, const std::string& uri, const std::string& properties = "") {
        const auto e = scene.createEntity(name);
        auto& s = scene.registry().emplace<ScriptComponent>(e);
        s.script = uri;
        s.properties = properties;
        return e;
    }
    void update(int frames = 1) {
        for (int i = 0; i < frames; ++i) {
            scripts.update(scene, time);
            scene.flushDestroyed();
            scene.updateTransforms();
        }
    }
};

}  // namespace

TEST_SUITE("script") {

TEST_CASE("lifecycle, properties overridden from the component, entity transform") {
    ScriptFixture f;
    f.write("assets://s/mover.lua", R"lua(
local Mover = { properties = { speed = 2, label = "a", active = true } }
function Mover:onStart() self.starts = (self.starts or 0) + 1 end
function Mover:onUpdate(dt) if self.active then self.entity:translate(self.speed * dt, 0) end end
function Mover:onDestroy() agr.find("Witness"):setName("destroyed " .. self.label) end
return Mover
)lua");
    f.scene.createEntity("Witness");
    const auto e = f.scripted("Mover", "assets://s/mover.lua", R"({"speed": 4, "label": "b"})");
    f.update(10);
    CHECK(f.scene.registry().get<TransformComponent>(e).position.x == doctest::Approx(4.0f));
    CHECK(f.scripts.field(e, "starts").value() == "1");
    CHECK(f.scripts.field(e, "label").value() == "b");
    CHECK(f.scripts.stats().instances == 1);
    f.scene.registry().remove<ScriptComponent>(e);
    f.update();
    CHECK_FALSE(f.scene.findByName("destroyed b").isNull());
    CHECK(f.scripts.stats().instances == 0);
}

TEST_CASE("describe lists properties and callbacks; compile errors carry the line") {
    ScriptFixture f;
    f.write("assets://s/p.lua", "local P = { properties = { speed = 3.5, jump = true, tag = 'hero', ignored = {} } }\n"
                                "function P:onUpdate(dt) end\nfunction P:onStart() end\nreturn P\n");
    const ScriptInfo info = f.scripts.describe("assets://s/p.lua");
    REQUIRE(info.valid);
    REQUIRE(info.properties.size() == 3);
    CHECK(info.properties[0].name == "jump");
    CHECK(info.properties[1].name == "speed");
    CHECK(info.properties[1].number == doctest::Approx(3.5));
    CHECK(info.properties[2].string == "hero");
    CHECK(info.callbacks == std::vector<std::string>{"onStart", "onUpdate"});

    f.write("assets://s/bad.lua", "local B = {}\nfunction B:onUpdate(dt)\n  local x = = 1\nend\nreturn B\n");
    const ScriptInfo bad = f.scripts.describe("assets://s/bad.lua");
    CHECK_FALSE(bad.valid);
    CHECK(bad.error.find("assets://s/bad.lua:3:") != std::string::npos);
    f.write("assets://s/notable.lua", "return 42\n");
    CHECK_FALSE(f.scripts.describe("assets://s/notable.lua").valid);
    CHECK_FALSE(f.scripts.describe("assets://s/missing.lua").valid);
}

TEST_CASE("runtime errors pause the instance, report once, and do not stop others") {
    ScriptFixture f;
    f.write("assets://s/boom.lua", "local B = {}\nfunction B:onUpdate(dt)\n  local t = nil\n  t.x = 1\nend\nreturn B\n");
    f.write("assets://s/ok.lua", "local O = {}\nfunction O:onUpdate(dt) self.entity:translate(1, 0) end\nreturn O\n");
    f.scripted("Boom", "assets://s/boom.lua");
    const auto ok = f.scripted("Ok", "assets://s/ok.lua");
    f.update(5);
    CHECK(f.scripts.stats().errors == 1);
    CHECK(f.scripts.stats().faulted == 1);
    CHECK(f.scene.registry().get<TransformComponent>(ok).position.x == doctest::Approx(5.0f));
    // Fixing the file resumes it (hot reload).
    f.rewrite("s/boom.lua", "local B = {}\nfunction B:onUpdate(dt) self.fixed = true end\nreturn B\n");
    CHECK(f.scripts.reloadChanged() == 1);
    f.update();
    CHECK(f.scripts.stats().faulted == 0);
    CHECK(f.scripts.field(f.scene.findByName("Boom"), "fixed").value() == "true");
}

TEST_CASE("sandbox: no file, OS, debug or bytecode access; require through the VFS") {
    ScriptFixture f;
    CHECK(f.scripts.run("assert(io == nil and os == nil and debug == nil and package == nil)"));
    CHECK(f.scripts.run("assert(load == nil and loadfile == nil and dofile == nil and string.dump == nil)"));
    f.write("assets://s/util.lua", "local U = {}\nfunction U.twice(x) return x * 2 end\nU.loads = (U.loads or 0) + 1\nreturn U\n");
    CHECK(f.scripts.run("local u = require('assets://s/util.lua'); local v = require('assets://s/util.lua')\n"
                        "assert(u.twice(21) == 42 and u == v)"));
    const Status missing = f.scripts.run("require('assets://s/none.lua')");
    CHECK_FALSE(missing);
    CHECK(missing.error().message.find("none.lua") != std::string::npos);
}

TEST_CASE("the watchdog stops infinite loops") {
    ScriptFixture f;
    f.scripts.setCallbackBudget(0.05);
    f.write("assets://s/loop.lua", "local L = {}\nfunction L:onStart() while true do end end\nreturn L\n");
    f.scripted("Loop", "assets://s/loop.lua");
    f.update();
    CHECK(f.scripts.stats().faulted == 1);
    const Status s = f.scripts.run("while true do end");
    CHECK_FALSE(s);
    CHECK(s.error().message.find("infinite loop") != std::string::npos);
}

TEST_CASE("components through reflection: get, set, add, has, remove") {
    ScriptFixture f;
    const auto e = f.scene.createEntity("Hero");
    CHECK(f.scripts.run(R"lua(
local e = agr.find("Hero")
e:add("SpriteRenderer")
assert(e:has("SpriteRenderer") and not e:has("Camera"))
e:set("SpriteRenderer", "color", {r = 1, g = 0.5, b = 0, a = 1})
e:set("SpriteRenderer", "size", {2, 3})
e:set("Transform", "rotation", {x = 0, y = 0, z = 90})
local c = e:get("SpriteRenderer", "color")
assert(math.abs(c.y - 0.5) < 1e-6)
assert(e:get("SpriteRenderer", "size").y == 3)
e:add("Camera")
assert(e:get("Camera", "projection") == "Perspective")
e:set("Camera", "projection", "Orthographic")
e:remove("Camera")
local child = agr.spawn("Child", e)
assert(e:find("Child") == child and child:parent() == e and #e:children() == 1)
assert(tostring(e) == "Entity(Hero)")
)lua", &f.scene));
    CHECK(f.scene.registry().get<SpriteRendererComponent>(e).size.y == 3.0f);
    const math::Vec3 euler = f.scene.registry().get<TransformComponent>(e).rotation.toEuler();
    CHECK(math::degrees(euler.z) == doctest::Approx(90.0f).epsilon(0.001));
    CHECK_FALSE(f.scene.registry().has<CameraComponent>(e));
    CHECK_FALSE(f.scripts.run("agr.find('Hero'):get('Nope', 'x')", &f.scene));
    CHECK_FALSE(f.scripts.run("agr.find('Hero'):set('Camera', 'projection', 'Fisheye')", &f.scene));
    // Destroyed entities raise clean errors instead of crashing.
    CHECK(f.scripts.run("local c = agr.find('Child'); c:destroy()", &f.scene));
    f.scene.flushDestroyed();
    CHECK_FALSE(f.scripts.run("agr.find('Hero'):children()[1]:name()", &f.scene));
}

TEST_CASE("hot reload keeps instance fields and swaps functions") {
    ScriptFixture f;
    f.write("assets://s/counter.lua", "local C = {}\nfunction C:onUpdate(dt) self.n = (self.n or 0) + 1 end\nreturn C\n");
    const auto e = f.scripted("Counter", "assets://s/counter.lua");
    f.update(3);
    CHECK(f.scripts.field(e, "n").value() == "3");
    f.rewrite("s/counter.lua", "local C = {}\nfunction C:onUpdate(dt) self.n = (self.n or 0) + 100 end\nreturn C\n");
    f.scripts.reloadChanged();
    f.update();
    CHECK(f.scripts.field(e, "n").value() == "103");
    // A broken edit keeps the previous code running.
    f.rewrite("s/counter.lua", "local C = {\n");
    CHECK(f.scripts.reloadChanged() == 0);
    f.update();
    CHECK(f.scripts.field(e, "n").value() == "203");
}

TEST_CASE("input, time and physics bindings; collision callbacks") {
    ScriptFixture f;
    Physics2DWorld physics;
    f.scripts.setPhysics2D(&physics);
    f.write("assets://s/player.lua", R"lua(
local P = { properties = { speed = 3 } }
function P:onUpdate(dt)
  self.axis = agr.input.axis("horizontal")
  self.jump = agr.input.keyPressed("Space")
  local vx, vy = self.entity:velocity()
  self.entity:setVelocity(self.axis * self.speed, vy)
  local hit = agr.physics2d.raycast(0, 10, 0, -1, 50)
  self.groundY = hit and hit.y or -1
end
function P:onCollision(other, began) if began then self.touched = other:name() end end
return P
)lua");
    const auto ground = f.scene.createEntity("Ground");
    f.scene.registry().emplace<Collider2DComponent>(ground).size = {20, 1};
    const auto player = f.scripted("Player", "assets://s/player.lua");
    f.scene.registry().get<TransformComponent>(player).position = {0, 3, 0};
    f.scene.registry().emplace<RigidBody2DComponent>(player).fixedRotation = true;
    f.scene.registry().emplace<Collider2DComponent>(player);
    f.input.beginFrame();
    f.input.setKey(Key::Right, true);
    f.input.setKey(Key::Space, true);
    physics.step(f.scene, 1.0f / 60.0f);
    f.update();
    CHECK(f.scripts.field(player, "axis").value() == "1.0");
    CHECK(f.scripts.field(player, "jump").value() == "true");
    CHECK(std::stod(f.scripts.field(player, "groundY").value()) == doctest::Approx(3.5).epsilon(0.01));  // top of the player's own box
    for (int i = 0; i < 120; ++i) {
        physics.step(f.scene, 1.0f / 60.0f);
        for (const auto& ev : physics.events()) f.scripts.dispatchCollision(f.scene, ev);
        f.update();
    }
    CHECK(f.scripts.field(player, "touched").value() == "Ground");
    CHECK(f.scene.registry().get<TransformComponent>(player).position.x > 2.0f);  // moved right at 3 m/s
    CHECK(f.scripts.run("assert(agr.time.delta() > 0.09)", &f.scene));
}

TEST_CASE("scripts drive 3D bodies through the same entity methods") {
    ScriptFixture f;
    registerPhysics3DComponents();
    Physics3DWorld physics;
    f.scripts.setPhysics3D(&physics);
    f.write("assets://s/ball.lua", R"lua(
local B = {}
function B:onStart() self.entity:applyImpulse(0, 0, -5) end
function B:onUpdate(dt)
  local vx, vy, vz = self.entity:velocity()
  self.vz = vz
  local hit = agr.physics3d.raycast(0, 10, -100, 0, -1, 0, 50)
  self.floor = hit and hit.y or -99
end
function B:onCollision(other, began) if began then self.hit = other:name() end end
return B
)lua");
    const auto floor = f.scene.createEntity("Floor");
    f.scene.registry().emplace<ColliderComponent>(floor).size = {400, 1, 400};
    const auto ball = f.scripted("Ball", "assets://s/ball.lua");
    f.scene.registry().get<TransformComponent>(ball).position = {0, 2, 0};
    f.scene.registry().emplace<RigidBodyComponent>(ball).linearDamping = 0.0f;
    auto& col = f.scene.registry().emplace<ColliderComponent>(ball);
    col.shape = ColliderShape3D::Sphere;
    col.density = 1.0f / (4.0f / 3.0f * math::kPi * 0.125f);  // 1 kg sphere of radius 0.5: the impulse gives 5 m/s
    physics.step(f.scene, 1.0f / 60.0f);
    f.update();
    for (int i = 0; i < 60; ++i) {
        physics.step(f.scene, 1.0f / 60.0f);
        for (const auto& ev : physics.events()) f.scripts.dispatchCollision(f.scene, ev);
        f.update();
    }
    CHECK(std::stod(f.scripts.field(ball, "vz").value()) < -1.0);
    CHECK(f.scripts.field(ball, "hit").value() == "Floor");
    CHECK(std::stod(f.scripts.field(ball, "floor").value()) == doctest::Approx(0.5).epsilon(0.01));
}

TEST_CASE("reloading the scene in place drops its script instances") {
    ScriptFixture f;
    f.write("assets://s/a.lua", "local A = {}\nfunction A:onUpdate() end\nreturn A\n");
    f.scripted("A", "assets://s/a.lua");
    f.update();
    CHECK(f.scripts.stats().instances == 1);
    f.scene.clear();
    f.update();
    CHECK(f.scripts.stats().instances == 0);
}

TEST_CASE("audio API: one-shots, buses, entity sources and errors") {
    ScriptFixture f;
    registerAudioComponents();
    AudioSystem audio(f.vfs);
    REQUIRE(audio.initialize({.output = AudioOutput::Manual}));
    f.scripts.setAudio(&audio);
    std::vector<float> samples(4800, 0.25f);
    const ByteBuffer wav = encodeWav16(samples.data(), 4800, 1, 48000);
    REQUIRE(f.vfs.writeBytes("assets://a.wav", wav.data(), wav.size()));
    f.write("assets://s/sound.lua", R"lua(
local T = {}
function T:onStart()
    self.voice = agr.audio.play("assets://a.wav", { bus = "Music", volume = 0.5, loop = true })
    self.missing, self.err = agr.audio.play("assets://none.wav")
    agr.audio.setBusVolume("music", 0.25)
    self.volume = agr.audio.busVolume("Music")
    agr.audio.setBusMuted("Effects", true)
    self.muted = agr.audio.busMuted("Effects")
    self.played = self.entity:playSound()
    self.playing = self.entity:isSoundPlaying() and agr.audio.isPlaying(self.voice)
    self.badBus = not pcall(agr.audio.setBusVolume, "Nope", 1)
    self.noSource = not pcall(function() agr.find("Other"):playSound() end)
end
return T
)lua");
    const auto e = f.scripted("Speaker", "assets://s/sound.lua");
    auto& src = f.scene.registry().emplace<AudioSourceComponent>(e);
    src.clip = "assets://a.wav";
    src.playOnStart = false;
    f.scene.createEntity("Other");
    f.update();
    CHECK(f.scripts.field(e, "voice").value() == "1");
    CHECK(f.scripts.field(e, "missing").value() == "nil");
    CHECK(f.scripts.field(e, "err").value().find("none.wav") != std::string::npos);
    CHECK(f.scripts.field(e, "volume").value() == "0.25");
    CHECK(f.scripts.field(e, "muted").value() == "true");
    CHECK(f.scripts.field(e, "played").value() == "true");
    CHECK(f.scripts.field(e, "playing").value() == "true");
    CHECK(f.scripts.field(e, "badBus").value() == "true");
    CHECK(f.scripts.field(e, "noSource").value() == "true");
    CHECK(audio.stats().voices == 2);
    CHECK(audio.busVolume(AudioBus::Music) == doctest::Approx(0.25f));
    CHECK(f.scripts.run("agr.audio.stop(1)", &f.scene));
    audio.update(0.0);
    CHECK(audio.stats().voices == 1);
}

TEST_CASE("UI API: clicks, slider changes, target messages, texts and visibility") {
    ScriptFixture f;
    registerUIComponents();
    UISystem ui(f.vfs);
    f.scripts.setUI(&ui);
    f.write("assets://s/button.lua", R"lua(
local B = {}
function B:onClick() self.clicks = (self.clicks or 0) + 1 end
return B
)lua");
    f.write("assets://s/hud.lua", R"lua(
local H = {}
function H:onStart() self.label = agr.find("Label") end
function H:play(button) self.played = button:name(); self.label:setText(42) end
function H:volume(slider, v) self.level = v end
function H:onUpdate() self.over = agr.ui.pointerOverUI(); self.held = agr.ui.isPressed(agr.find("Play")) end
function H:hide() self.label:setActive(false); self.visible = self.label:isActive() end
return H
)lua");
    const auto canvas = f.scene.createEntity("Canvas");
    f.scene.registry().emplace<UICanvasComponent>(canvas).scaleMode = UIScaleMode::ConstantPixelSize;
    const auto hud = f.scripted("HUD", "assets://s/hud.lua");
    auto rect = [&](const char* name, math::Vec2 pos, math::Vec2 size) {
        const auto e = f.scene.createEntity(name, canvas);
        auto& r = f.scene.registry().emplace<UIRectComponent>(e);
        r.anchorMin = r.anchorMax = r.pivot = {0, 0};
        r.position = pos;
        r.size = size;
        return e;
    };
    const auto play = rect("Play", {0, 0}, {100, 50});
    f.scene.registry().emplace<UIImageComponent>(play);
    auto& b = f.scene.registry().emplace<UIButtonComponent>(play);
    b.target = hud;
    b.message = "play";
    f.scene.registry().emplace<ScriptComponent>(play).script = "assets://s/button.lua";
    const auto slider = rect("Volume", {0, 100}, {220, 20});
    auto& sl = f.scene.registry().emplace<UISliderComponent>(slider);
    sl.target = hud;
    sl.message = "volume";
    const auto label = rect("Label", {0, 200}, {100, 40});
    f.scene.registry().emplace<UITextComponent>(label).text = "0";
    f.update();
    ui.layout(f.scene, 800, 600);

    auto pump = [&](std::initializer_list<UIPointer> p) {
        ui.processInput(f.scene, std::span<const UIPointer>(p.begin(), p.size()));
        for (const UIEvent& e : ui.takeEvents()) {
            if (e.type == UIEventType::Click) f.scripts.dispatchUI(f.scene, UIClickEvent{e.entity});
            else f.scripts.dispatchUI(f.scene, UIValueChangedEvent{e.entity, e.value});
        }
        f.update();
    };
    pump({{0, {50, 25}, true}});
    CHECK(f.scripts.field(hud, "over").value() == "true");
    CHECK(f.scripts.field(hud, "held").value() == "true");
    pump({{0, {50, 25}, false}});
    CHECK(f.scripts.field(play, "clicks").value() == "1");
    CHECK(f.scripts.field(hud, "played").value() == "Play");
    CHECK(f.scene.registry().get<UITextComponent>(label).text == "42");
    pump({{0, {60, 110}, true}});  // usable range 10 .. 210
    CHECK(f.scripts.field(hud, "level").value() == "0.25");
    CHECK(f.scripts.run("local h = agr.find('HUD'):script(); h:hide(); assert(h.visible == false)", &f.scene));
    CHECK_FALSE(f.scene.registry().get<StateComponent>(label).active);
    CHECK_FALSE(f.scripts.run("agr.find('Play'):setText('x')", &f.scene));  // no UIText: clear error
}

TEST_CASE("animation API: parameters, triggers, states and animation events") {
    ScriptFixture f;
    registerAnimationComponents();
    AnimationSystem anim(f.vfs);
    f.scripts.setAnimation(&anim);
    AnimationClip run;
    run.track("", "Transform", "position", 1).curve.setKey(0.0f, 3.0f);
    run.events = {{0.05f, "step"}};
    REQUIRE(f.vfs.writeText("assets://run.agranim", run.toJson()));
    AnimatorController ctrl;
    ctrl.parameters = {{"speed", ParameterType::Float, 0.0f}, {"hit", ParameterType::Trigger, 0.0f}};
    ctrl.states = {{"Idle", "", "", 1.0f, {}}, {"Run", "assets://run.agranim", "", 1.0f, {}}};
    ctrl.transitions = {{"Idle", "Run", {{"speed", ConditionOp::Greater, 0.5f}}, -1.0f, 0.0f}};
    REQUIRE(f.vfs.writeText("assets://hero.agrctrl", ctrl.toJson()));
    f.write("assets://s/anim.lua", R"lua(
local A = { }
function A:onStart()
    self.entity:setParameter("speed", 1)
    self.speed = self.entity:getParameter("speed")
    self.bad = not pcall(function() self.entity:setParameter("nope", 1) end)
    self.events = ""
end
function A:onStateEnter(name) self.events = self.events .. "enter:" .. name .. ";" end
function A:onAnimationEvent(name) self.events = self.events .. name .. ";"; self.state = self.entity:currentState() end
return A
)lua");
    const auto e = f.scripted("Hero", "assets://s/anim.lua");
    f.scene.registry().emplace<AnimatorComponent>(e).controller = "assets://hero.agrctrl";
    auto frame = [&] {
        f.update();
        anim.update(f.scene, 0.1f);
        for (const AnimationEvent& ev : anim.takeEvents()) f.scripts.dispatchAnimation(f.scene, ev);
    };
    frame();
    frame();
    CHECK(f.scripts.field(e, "speed").value() == "1.0");
    CHECK(f.scripts.field(e, "bad").value() == "true");
    CHECK(f.scripts.field(e, "events").value() == "enter:Idle;enter:Run;step;");
    CHECK(f.scripts.field(e, "state").value() == "Run");
    CHECK(f.scene.registry().get<TransformComponent>(e).position.y == doctest::Approx(3.0f));
    CHECK(f.scripts.run("agr.find('Hero'):playState('Idle')", &f.scene));
    CHECK(anim.currentState(e) == "Idle");
    CHECK_FALSE(f.scripts.run("agr.find('Hero'):playState('Fly')", &f.scene));
}

TEST_CASE("mobile API: local save, device services, motion sensors") {
    ScriptFixture f;
    f.vfs.mount("user", std::make_unique<NativeFileProvider>((f.root / "user").string(), false));
    SaveStore save(f.vfs, "user://save.json");
    f.scripts.setSaveStore(&save);
    struct FakePhone final : PlatformLayer {
        Status initialize(const EngineConfig&) override { return {}; }
        void shutdown() override {}
        bool pollEvents(EventBus&, Input&) override { return true; }
        void present() override {}
        bool canRender() const override { return false; }
        math::Vec2 framebufferSize() const override { return {1080, 2400}; }
        const char* name() const override { return "Fake"; }
        math::Vec4 safeAreaInsets() const override { return {0, 90, 0, 40}; }
        void vibrate(float seconds, float strength) override { vibrated += seconds * strength; }
        bool canVibrate() const override { return true; }
        float vibrated = 0.0f;
    } phone;
    f.scripts.setPlatform(&phone);

    CHECK(f.scripts.run(R"lua(
        agr.save.set("level", 3)
        agr.save.set("name", "Héros")
        agr.save.set("best", 12.5)
        agr.save.set("unlocked", { "forest", "cave" })
        agr.save.set("options", { music = 0.5, haptics = true, keys = { jump = "Space" } })
        assert(agr.save.get("level") == 3 and math.type(agr.save.get("level")) == "integer")
        assert(agr.save.get("missing") == nil and agr.save.get("missing", 7) == 7)
        assert(agr.save.has("name") and not agr.save.has("missing"))
        assert(agr.save.get("unlocked")[2] == "cave")
        assert(agr.save.get("options").keys.jump == "Space")
        agr.save.set("temp", 1); agr.save.remove("temp"); assert(not agr.save.has("temp"))
        assert(not pcall(agr.save.set, "bad", function() end))
        assert(agr.save.flush())
    )lua"));
    CHECK(f.vfs.exists("user://save.json"));
    save.reload();  // as after restarting the game
    CHECK(save.get("best").get<double>() == doctest::Approx(12.5));
    CHECK(save.get("name").get<std::string>() == "Héros");
    CHECK(save.get("options")["haptics"].get<bool>());
    REQUIRE(f.vfs.writeText("user://save.json", "{ damaged"));
    save.reload();
    CHECK_FALSE(save.has("level"));  // a damaged file starts an empty save instead of failing

    CHECK(f.scripts.run(R"lua(
        local l, t, r, b = agr.device.safeArea()
        assert(t == 90 and b == 40 and l == 0)
        assert(agr.device.canVibrate())
        agr.device.vibrate(0.2, 0.5)
        assert(type(agr.device.platform()) == "string")
        assert(agr.input.accelerometer() == nil)
    )lua"));
    CHECK(phone.vibrated == doctest::Approx(0.1f));
    f.input.setAccelerometer({0.0f, -9.81f, 0.5f});
    CHECK(f.scripts.run("local x, y, z = agr.input.accelerometer(); assert(math.abs(y + 9.81) < 1e-4 and z == 0.5)"));
}

}  // TEST_SUITE

TEST_CASE("agr.instantiate spawns prefab instances whose scripts run") {
    ScriptFixture f;
    PrefabLibrary prefabs(f.vfs);
    prefabs.install();
    f.write("assets://coin.lua", R"lua(
local Coin = {}
function Coin:onStart() self.entity:set("Transform", "scale", {2, 2, 2}) end
return Coin
)lua");
    {
        Scene author;
        const auto coin = author.createEntity("Coin");
        author.registry().emplace<ScriptComponent>(coin).script = "assets://coin.lua";
        author.createEntity("Sparkle", coin);
        REQUIRE(prefabs.save("assets://coin.agrprefab", prefab::fromEntity(author, coin)));
    }
    f.write("assets://spawner.lua", R"lua(
local Spawner = {}
function Spawner:onStart()
    for i = 1, 3 do
        local c = agr.instantiate("assets://coin.agrprefab", self.entity)
        c:set("Transform", "position", {i, 0, 0})
    end
    local none, err = agr.instantiate("assets://missing.agrprefab")
    assert(none == nil and err:find("missing"))
end
return Spawner
)lua");
    const auto spawner = f.scripted("Spawner", "assets://spawner.lua");
    f.update(3);
    const auto kids = f.scene.children(spawner);
    REQUIRE(kids.size() == 3);
    for (auto c : kids) {
        CHECK(prefab::isInstanceRoot(f.scene, c));
        CHECK(f.scene.children(c).size() == 1);
        CHECK(f.scene.registry().get<TransformComponent>(c).scale.x == doctest::Approx(2.0f));  // its script started
    }
    CHECK(f.scripts.stats().errors == 0);
    setPrefabResolver(nullptr);
}

TEST_CASE("input actions and gamepads from Lua") {
    ScriptFixture f;
    InputActions actions;
    actions.setMap(InputMap::defaults());
    f.scripts.setActions(&actions);
    f.input.setKey(Key::Space, true);
    f.input.gamepadConnect(5, "Pad");
    f.input.setGamepadAxis(5, GamepadAxis::LeftX, -1.0f);
    f.input.setGamepadButton(5, GamepadButton::North, true);
    actions.update(f.input, {800, 600});
    CHECK(f.scripts.run(R"lua(
assert(agr.input.down("jump") and agr.input.pressed("jump") and not agr.input.released("jump"))
assert(agr.input.value("move_x") == -1)
assert(agr.input.gamepadCount() == 1)
assert(agr.input.gamepadDown("North") and agr.input.gamepadDown("North", 1) and not agr.input.gamepadDown("South"))
assert(agr.input.gamepadAxis("LeftX") == -1)
)lua", &f.scene));
    CHECK_FALSE(f.scripts.run("agr.input.down('teleport')", &f.scene));
    CHECK_FALSE(f.scripts.run("agr.input.gamepadDown('Z')", &f.scene));
    CHECK_FALSE(f.scripts.run("agr.input.gamepadAxis('LeftX', 9)", &f.scene));
}

TEST_CASE("agr.tr and agr.locale") {
    ScriptFixture f;
    Localization l;
    REQUIRE(l.parse("key,fr,en\nhud.score,Score : {1} / {2},Score: {1} / {2}\nhello,Bonjour,Hello\n"));
    std::string remembered;
    f.scripts.setLocalization(&l, [&](std::string_view c) {
        if (!l.setLanguage(c)) return false;
        remembered = l.language();
        return true;
    });
    CHECK(f.scripts.run(R"lua(
assert(agr.tr("hello") == "Bonjour")
assert(agr.tr("hud.score", 3, 10) == "Score : 3 / 10")
assert(agr.locale.get() == "fr")
local langs = agr.locale.list()
assert(#langs == 2 and langs[2] == "en")
assert(agr.locale.set("en-GB") and agr.locale.get() == "en")
assert(agr.tr("hello") == "Hello")
assert(not agr.locale.set("xx"))
assert(agr.tr("not.a.key") == "not.a.key")
)lua", &f.scene));
    CHECK(remembered == "en");
}
