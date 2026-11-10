#include <doctest/doctest.h>

#include <filesystem>

#include "agr/animation/AnimationSystem.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/twod/SpriteAnimator.hpp"

using namespace agr;
namespace fs = std::filesystem;

namespace {

struct AnimFixture {
    fs::path root = fs::temp_directory_path() / ("agr_anim_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    Scene scene;
    AnimationSystem anim{vfs};

    AnimFixture() {
        fs::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        registerRenderComponents();
        registerSpriteAnimatorComponent();
        registerAnimationComponents();
    }
    ~AnimFixture() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    void save(const std::string& uri, const AnimationClip& c) { REQUIRE(vfs.writeText(uri, c.toJson())); }
    void save(const std::string& uri, const AnimatorController& c) { REQUIRE(vfs.writeText(uri, c.toJson())); }
    float x(ecs::Entity e) { return scene.registry().get<TransformComponent>(e).position.x; }
    std::vector<AnimationEvent> step(float dt) {
        anim.update(scene, dt);
        return anim.takeEvents();
    }
};

AnimationClip constantX(float value, float duration = 1.0f) {
    AnimationClip c;
    c.duration = duration;
    c.track("", "Transform", "position", 0).curve.setKey(0.0f, value);
    return c;
}

} // namespace

TEST_SUITE("animation") {

TEST_CASE("curves: constant, linear, smooth and custom keys") {
    Curve c;
    c.setKey(1.0f, 10.0f, Interpolation::Linear);
    c.setKey(0.0f, 0.0f, Interpolation::Linear);  // inserted before: keys stay sorted
    c.setKey(2.0f, 10.0f, Interpolation::Linear);
    REQUIRE(c.keys.size() == 3);
    CHECK(c.keys[0].time == 0.0f);
    CHECK(c.evaluate(-1.0f) == 0.0f);
    CHECK(c.evaluate(0.25f) == doctest::Approx(2.5f));
    CHECK(c.evaluate(5.0f) == 10.0f);
    c.setKey(1.0f, 20.0f);  // same time: replaced
    CHECK(c.keys.size() == 3);
    CHECK(c.evaluate(1.0f) == doctest::Approx(20.0f));

    c.keys[0].interpolation = Interpolation::Constant;
    CHECK(c.evaluate(0.99f) == 0.0f);

    // Smooth: eased, never overshoots a peak (flat tangent at the extreme).
    Curve s;
    s.setKey(0.0f, 0.0f);
    s.setKey(1.0f, 1.0f);
    s.setKey(2.0f, 0.0f);
    CHECK(s.evaluate(0.5f) == doctest::Approx(0.5f));
    for (float t = 0.0f; t <= 2.0f; t += 0.05f) CHECK(s.evaluate(t) <= 1.0f + 1e-5f);
    CHECK(s.evaluate(0.1f) < 0.1f);  // ease in
    // Monotone data stays monotone.
    Curve m;
    m.setKey(0.0f, 0.0f);
    m.setKey(0.1f, 5.0f);
    m.setKey(1.0f, 6.0f);
    float prev = -1.0f;
    for (float t = 0.0f; t <= 1.0f; t += 0.01f) {
        const float v = m.evaluate(t);
        CHECK(v >= prev - 1e-5f);
        CHECK(v <= 6.0f + 1e-5f);
        prev = v;
    }
    // Custom tangents: slope 0 out of key 0 and in key 1 = smoothstep.
    Curve h;
    h.setKey(0.0f, 0.0f, Interpolation::Custom);
    h.setKey(1.0f, 1.0f, Interpolation::Custom);
    CHECK(h.evaluate(0.25f) == doctest::Approx(0.15625f));
    h.keys[0].outTangent = 1.0f;
    h.keys[1].inTangent = 1.0f;
    CHECK(h.evaluate(0.25f) == doctest::Approx(0.25f));  // straight line
}

TEST_CASE("clips and state machines round-trip through JSON and are validated") {
    AnimationClip c;
    c.loop = false;
    auto& t = c.track("Arm", "Transform", "rotation", 2);
    t.curve.setKey(0.0f, 0.0f, Interpolation::Custom).outTangent = 3.0f;
    t.curve.setKey(0.5f, 90.0f, Interpolation::Constant);
    c.track("", "SpriteAnimator", "clip", -1).curve.setKey(0.25f, 0.0f).text = "run";
    c.events.push_back({0.4f, "step"});
    c.fitDuration();
    CHECK(c.duration == doctest::Approx(0.5f));
    auto back = AnimationClip::fromJson(c.toJson());
    REQUIRE(back);
    CHECK_FALSE(back.value().loop);
    REQUIRE(back.value().tracks.size() == 2);
    CHECK(back.value().tracks[0].label() == "Arm: Transform.rotation.z");
    CHECK(back.value().tracks[0].curve.keys[0].outTangent == doctest::Approx(3.0f));
    CHECK(back.value().tracks[0].curve.keys[1].interpolation == Interpolation::Constant);
    CHECK(back.value().tracks[1].curve.evaluateText(1.0f) == "run");
    CHECK(back.value().events[0].name == "step");
    CHECK_FALSE(AnimationClip::fromJson("{ broken"));
    CHECK_FALSE(AnimationClip::fromJson(R"({"tracks":[{"keys":[]}]})"));

    AnimatorController ctrl;
    ctrl.parameters = {{"speed", ParameterType::Float, 0.0f}, {"jump", ParameterType::Trigger, 0.0f}};
    ctrl.states = {{"Idle", "", "idle", 1.0f, {10, 20}}, {"Run", "", "run", 1.5f, {}}};
    ctrl.transitions = {{"Idle", "Run", {{"speed", ConditionOp::Greater, 0.1f}}, -1.0f, 0.2f}};
    ctrl.entry = "Idle";
    CHECK(ctrl.validate().empty());
    auto cb = AnimatorController::fromJson(ctrl.toJson());
    REQUIRE(cb);
    CHECK(cb.value().states[1].speed == doctest::Approx(1.5f));
    CHECK(cb.value().states[0].editorPosition.y == doctest::Approx(20.0f));
    CHECK(cb.value().transitions[0].conditions[0].op == ConditionOp::Greater);
    CHECK(cb.value().parameters[1].type == ParameterType::Trigger);
    ctrl.transitions.push_back({"Run", "Fly", {{"nope", ConditionOp::IsTrue, 0}}, -1.0f, 0.0f});
    ctrl.transitions.push_back({"Run", "Idle", {}, -1.0f, 0.0f});
    CHECK(ctrl.validate().size() == 3);
}

TEST_CASE("a clip animates fields of the entity and its children") {
    AnimFixture f;
    AnimationClip c;
    c.duration = 1.0f;
    auto& px = c.track("", "Transform", "position", 0);
    px.curve.setKey(0.0f, 0.0f, Interpolation::Linear);
    px.curve.setKey(1.0f, 10.0f, Interpolation::Linear);
    c.track("Arm", "Transform", "rotation", 2).curve.setKey(0.0f, 90.0f);
    auto& alpha = c.track("Arm", "SpriteRenderer", "color", 3);
    alpha.curve.setKey(0.0f, 1.0f, Interpolation::Linear);
    alpha.curve.setKey(1.0f, 0.0f, Interpolation::Linear);
    auto& flip = c.track("Arm", "SpriteRenderer", "flipX", -1).curve;
    flip.setKey(0.0f, 0.0f, Interpolation::Constant);
    flip.setKey(0.5f, 1.0f, Interpolation::Constant);
    c.track("", "Transform", "missing", 0).curve.setKey(0.0f, 1.0f);  // ignored
    c.track("Nobody", "Transform", "position", 1).curve.setKey(0.0f, 1.0f);  // ignored
    f.save("assets://wave.agranim", c);

    const auto e = f.scene.createEntity("Robot");
    f.scene.registry().emplace<AnimatorComponent>(e).clip = "assets://wave.agranim";
    const auto arm = f.scene.createEntity("Arm", e);
    f.scene.registry().emplace<SpriteRendererComponent>(arm);
    f.step(0.25f);
    CHECK(f.x(e) == doctest::Approx(2.5f));
    const math::Vec3 euler = f.scene.registry().get<TransformComponent>(arm).rotation.toEuler();
    CHECK(math::degrees(euler.z) == doctest::Approx(90.0f).epsilon(0.001));
    CHECK(f.scene.registry().get<SpriteRendererComponent>(arm).color.w == doctest::Approx(0.75f));
    CHECK_FALSE(f.scene.registry().get<SpriteRendererComponent>(arm).flipX);
    f.step(0.5f);
    CHECK(f.x(e) == doctest::Approx(7.5f));
    CHECK(f.scene.registry().get<SpriteRendererComponent>(arm).flipX);
    f.step(0.5f);  // loops: 1.25 s
    CHECK(f.x(e) == doctest::Approx(2.5f));

    // Speed, pause, non-looping clip holding its last value.
    f.scene.registry().get<AnimatorComponent>(e).speed = 2.0f;
    f.step(0.25f);
    CHECK(f.x(e) == doctest::Approx(7.5f));
    f.scene.registry().get<AnimatorComponent>(e).playing = false;
    f.step(1.0f);
    CHECK(f.x(e) == doctest::Approx(7.5f));
    c.loop = false;
    f.save("assets://once.agranim", c);
    f.scene.registry().get<AnimatorComponent>(e) = {"", "assets://once.agranim", 1.0f, true};
    f.step(3.0f);
    CHECK(f.x(e) == doctest::Approx(10.0f));
    CHECK(f.anim.stats().animators == 1);

    // Editor helpers.
    CHECK(AnimationSystem::resolvePath(f.scene, e, "Arm") == arm);
    CHECK(AnimationSystem::resolvePath(f.scene, e, "Arm/Nope").isNull());
    CHECK(AnimationSystem::pathOf(f.scene, e, arm).value() == "Arm");
    CHECK(AnimationSystem::pathOf(f.scene, e, e).value().empty());
    CHECK_FALSE(AnimationSystem::pathOf(f.scene, arm, e));
    f.anim.preview(f.scene, e, c, 0.5f);
    CHECK(f.x(e) == doctest::Approx(5.0f));
    CHECK(f.anim.readTrack(f.scene, e, c.tracks[0]).value() == doctest::Approx(5.0f));
    CHECK(f.anim.readTrack(f.scene, e, c.tracks[1]).value() == doctest::Approx(90.0f).epsilon(0.001));
}

TEST_CASE("clip events fire once per pass, across loops") {
    AnimFixture f;
    AnimationClip c = constantX(0.0f);
    c.events = {{0.0f, "start"}, {0.25f, "a"}, {0.75f, "b"}};
    f.save("assets://ev.agranim", c);
    const auto e = f.scene.createEntity("E");
    f.scene.registry().emplace<AnimatorComponent>(e).clip = "assets://ev.agranim";
    auto names = [](const std::vector<AnimationEvent>& v) {
        std::string s;
        for (const auto& ev : v) s += ev.name + ",";
        return s;
    };
    CHECK(names(f.step(0.5f)) == "start,a,");
    CHECK(names(f.step(0.1f)).empty());
    CHECK(names(f.step(1.0f)) == "b,start,a,");  // 0.6 -> 1.6
    CHECK(names(f.step(0.3f)) == "b,");
}

TEST_CASE("state machine: conditions, triggers, exit time, crossfade, sprite clips") {
    AnimFixture f;
    f.save("assets://idle.agranim", constantX(0.0f));
    f.save("assets://run.agranim", constantX(10.0f));
    f.save("assets://jump.agranim", constantX(-5.0f, 0.5f));
    AnimatorController ctrl;
    ctrl.parameters = {{"speed", ParameterType::Float, 0.0f}, {"grounded", ParameterType::Bool, 1.0f}, {"jump", ParameterType::Trigger, 0.0f}};
    ctrl.states = {{"Idle", "assets://idle.agranim", "idle", 1.0f, {}},
                   {"Run", "assets://run.agranim", "run", 1.0f, {}},
                   {"Jump", "assets://jump.agranim", "jump", 1.0f, {}}};
    ctrl.transitions = {{"Idle", "Run", {{"speed", ConditionOp::Greater, 0.1f}}, -1.0f, 0.0f},
                        {"Run", "Idle", {{"speed", ConditionOp::Less, 0.1f}}, -1.0f, 0.5f},
                        {"*", "Jump", {{"jump", ConditionOp::IsTrue, 0.0f}, {"grounded", ConditionOp::IsTrue, 0.0f}}, -1.0f, 0.0f},
                        {"Jump", "Idle", {}, 1.0f, 0.0f}};
    f.save("assets://hero.agrctrl", ctrl);
    const auto e = f.scene.createEntity("Hero");
    f.scene.registry().emplace<AnimatorComponent>(e).controller = "assets://hero.agrctrl";
    f.scene.registry().emplace<SpriteAnimatorComponent>(e);

    auto events = f.step(0.1f);
    CHECK(f.anim.currentState(e) == "Idle");
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == AnimationEvent::Kind::StateEnter);
    CHECK(f.scene.registry().get<SpriteAnimatorComponent>(e).clip == "idle");

    CHECK(f.anim.setFloat(f.scene, e, "speed", 2.0f));
    CHECK_FALSE(f.anim.setFloat(f.scene, e, "unknown", 1.0f));
    f.step(0.1f);
    CHECK(f.anim.currentState(e) == "Run");
    CHECK(f.x(e) == doctest::Approx(10.0f));
    CHECK(f.scene.registry().get<SpriteAnimatorComponent>(e).clip == "run");

    // Back to Idle with a 0.5 s crossfade.
    f.anim.setFloat(f.scene, e, "speed", 0.0f);
    f.step(0.25f);
    CHECK(f.anim.currentState(e) == "Idle");
    CHECK(f.x(e) == doctest::Approx(10.0f));  // weight 0 on the transition frame
    f.step(0.25f);
    CHECK(f.x(e) == doctest::Approx(5.0f));
    f.step(0.25f);
    CHECK(f.x(e) == doctest::Approx(0.0f));

    // Trigger, consumed by the transition; blocked when a condition fails.
    f.anim.setBool(f.scene, e, "grounded", false);
    f.anim.setTrigger(f.scene, e, "jump");
    f.step(0.1f);
    CHECK(f.anim.currentState(e) == "Idle");
    CHECK(f.anim.parameter(f.scene, e, "jump").value() == 1.0f);  // still pending
    f.anim.setBool(f.scene, e, "grounded", true);
    f.step(0.1f);
    CHECK(f.anim.currentState(e) == "Jump");
    CHECK(f.anim.parameter(f.scene, e, "jump").value() == 0.0f);
    CHECK(f.x(e) == doctest::Approx(-5.0f));
    CHECK(f.scene.registry().get<SpriteAnimatorComponent>(e).clip == "jump");
    // Exit time 1 = end of the 0.5 s clip.
    f.step(0.3f);
    CHECK(f.anim.currentState(e) == "Jump");
    f.step(0.3f);
    CHECK(f.anim.currentState(e) == "Idle");
    CHECK(f.anim.stateTime(e) == doctest::Approx(0.0f));

    // Forced state, and a reloaded scene restarts from the entry state.
    f.anim.setFloat(f.scene, e, "speed", 1.0f);  // else Run -> Idle fires at once
    CHECK(f.anim.play(f.scene, e, "Run"));
    CHECK_FALSE(f.anim.play(f.scene, e, "Swim"));
    f.step(0.1f);
    CHECK(f.anim.currentState(e) == "Run");
    CHECK(f.anim.stats().transitions >= 4);
    f.scene.clear();
    const auto again = f.scene.createEntity("Hero");
    f.scene.registry().emplace<AnimatorComponent>(again).controller = "assets://hero.agrctrl";
    f.step(0.1f);
    CHECK(f.anim.currentState(again) == "Idle");
}

TEST_CASE("missing or broken animation files are reported without crashing") {
    AnimFixture f;
    REQUIRE(f.vfs.writeText("assets://bad.agrctrl", "{ nope"));
    const auto a = f.scene.createEntity("A");
    f.scene.registry().emplace<AnimatorComponent>(a).controller = "assets://bad.agrctrl";
    const auto b = f.scene.createEntity("B");
    f.scene.registry().emplace<AnimatorComponent>(b).clip = "assets://none.agranim";
    AnimatorController ctrl;
    ctrl.states = {{"S", "assets://missing.agranim", "", 1.0f, {}}};
    f.save("assets://ok.agrctrl", ctrl);
    const auto c = f.scene.createEntity("C");
    f.scene.registry().emplace<AnimatorComponent>(c).controller = "assets://ok.agrctrl";
    f.step(0.1f);
    f.step(0.1f);
    CHECK(f.anim.currentState(a).empty());
    CHECK(f.anim.currentState(c) == "S");  // runs, without a clip
    CHECK(f.anim.stats().animators == 1);
}

}  // TEST_SUITE
