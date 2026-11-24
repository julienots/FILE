#include <doctest/doctest.h>

#include <filesystem>

#include "agr/editor/OnionSkin.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/animation/IK2D.hpp"
#include "agr/animation/IK3D.hpp"
#include "agr/render/Skin2D.hpp"
#include "agr/assets/Model.hpp"
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

#ifdef AGR_TEST_TEMPLATE_ASSETS
TEST_CASE("skeletal clips: imported from a glTF model, played and crossfaded by a state machine") {
    AnimFixture f;
    fs::create_directories(f.root / "3d");
    fs::copy_file(fs::path(AGR_TEST_TEMPLATE_ASSETS) / "3d" / "sprout.glb", f.root / "3d" / "sprout.glb");

    // Import: skin, weights, clips.
    auto model = importModel(f.vfs, "assets://3d/sprout.glb");
    REQUIRE(model);
    const ModelData& m = model.value();
    REQUIRE(m.skins.size() == 1);
    CHECK(m.skins[0].joints.size() == 4);
    CHECK(m.skins[0].inverseBind[1].c[3].y == doctest::Approx(-0.35f));
    REQUIRE(m.meshes.size() == 1);
    REQUIRE(m.meshes[0].skinWeights.size() == m.meshes[0].vertices.size());
    for (const ModelSkinWeights& w : m.meshes[0].skinWeights) CHECK(w.weights.x + w.weights.y + w.weights.z + w.weights.w == doctest::Approx(1.0f));
    REQUIRE(m.animations.size() == 2);
    CHECK(m.animations[0].name == "Idle");
    CHECK(m.animations[0].duration == doctest::Approx(3.0f));
    CHECK(m.animations[1].duration == doctest::Approx(1.2f));
    CHECK(modelNodePath(m.nodes, {"Sprout"}, static_cast<usize>(m.skins[0].joints[2])) == "Sprout/Root/Stem1/Stem2");
    CHECK(f.anim.library().modelClipNames("assets://3d/sprout.glb") == std::vector<std::string>{"Idle", "Dance"});

    // The entity hierarchy instantiateModel would build: root > Sprout > Root > Stem1 > Stem2 > Top.
    const auto root = f.scene.createEntity("sprout");
    const auto sprout = f.scene.createEntity("Sprout", root);
    ecs::Entity parent = sprout;
    std::vector<ecs::Entity> joints;
    for (const char* name : {"Root", "Stem1", "Stem2", "Top"}) {
        parent = f.scene.createEntity(name, parent);
        joints.push_back(parent);
    }
    auto clip = f.anim.library().clip("assets://3d/sprout.glb#Dance");
    REQUIRE(clip);
    CHECK(clip.value()->transformTracks.size() == 4);
    CHECK(clip.value()->duration == doctest::Approx(1.2f));
    CHECK_FALSE(f.anim.library().clip("assets://3d/sprout.glb#Fly"));

    // A single clip: the joints follow the samples (shortest-arc slerp between keys).
    f.scene.registry().emplace<AnimatorComponent>(root).clip = "assets://3d/sprout.glb#Dance";
    f.step(0.3f);
    const TransformTrack* stem1 = nullptr;
    for (const TransformTrack& t : clip.value()->transformTracks)
        if (t.target == "Sprout/Root/Stem1" && t.path == TransformTrack::Path::Rotation) stem1 = &t;
    REQUIRE(stem1 != nullptr);
    const math::Vec4 expected = stem1->sample(0.3f);
    const math::Quat q = f.scene.registry().get<TransformComponent>(joints[1]).rotation;
    CHECK(q.x == doctest::Approx(expected.x).epsilon(1e-4));
    CHECK(q.z == doctest::Approx(expected.z).epsilon(1e-4));
    CHECK(q.w == doctest::Approx(expected.w).epsilon(1e-4));
    CHECK(std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w) == doctest::Approx(1.0f));
    CHECK(f.scene.registry().get<TransformComponent>(joints[0]).position.y > 0.0f);  // the bounce moves the root

    // State machine Idle -> Dance with a crossfade: halfway, the pose lies between both clips.
    AnimatorController ctrl;
    ctrl.parameters = {{"party", ParameterType::Bool, 0.0f}};
    ctrl.states = {{"Idle", "assets://3d/sprout.glb#Idle", "", 1.0f, {}}, {"Dance", "assets://3d/sprout.glb#Dance", "", 1.0f, {}}};
    ctrl.transitions = {{"Idle", "Dance", {{"party", ConditionOp::IsTrue, 0.0f}}, -1.0f, 0.4f}};
    f.save("assets://sprout.agrctrl", ctrl);
    f.scene.registry().get<AnimatorComponent>(root) = {"assets://sprout.agrctrl", "", 1.0f, true};
    f.step(0.0f);
    CHECK(f.anim.currentState(root) == "Idle");
    f.anim.setBool(f.scene, root, "party", true);
    f.step(0.0f);
    CHECK(f.anim.currentState(root) == "Dance");
    f.step(0.2f);
    const float rootY = f.scene.registry().get<TransformComponent>(joints[0]).position.y;
    CHECK(rootY > 0.0f);  // Idle does not bounce, Dance does: half of Dance's bounce at 0.2 s
    const auto danceAt = [&](float t) {
        for (const TransformTrack& tr : clip.value()->transformTracks)
            if (tr.target == "Sprout/Root" && tr.path == TransformTrack::Path::Translation) return tr.sample(t).y;
        return -1.0f;
    };
    CHECK(rootY == doctest::Approx(danceAt(0.2f) * 0.5f).epsilon(0.02));

    // ---- Blend state (1D blend tree): Idle at 0, Dance at 1, mixed by "energy" with phases in step.
    AnimatorController blend;
    blend.parameters = {{"energy", ParameterType::Float, 0.0f}};
    AnimatorState move{"Move", "", "", 1.0f, {}};
    move.blendParameter = "energy";
    move.motions = {{"assets://3d/sprout.glb#Dance", 1.0f}, {"assets://3d/sprout.glb#Idle", 0.0f}};  // sorted on load
    blend.states = {move};
    f.save("assets://blend.agrctrl", blend);
    {
        auto loaded = AnimatorController::fromJson(blend.toJson());
        REQUIRE(loaded);
        REQUIRE(loaded.value().states[0].motions.size() == 2);
        CHECK(loaded.value().states[0].motions[0].threshold == 0.0f);  // sorted
        CHECK(loaded.value().states[0].blendParameter == "energy");
        CHECK(loaded.value().validate().empty());
        AnimatorController bad = loaded.value();
        bad.states[0].blendParameter = "nope";
        CHECK_FALSE(bad.validate().empty());
    }
    auto rootYAfter = [&](float energy, float seconds) {
        f.scene.registry().get<AnimatorComponent>(root) = {"assets://blend.agrctrl", "", 1.0f, true};
        f.anim.reset();
        f.step(0.0f);
        f.anim.setFloat(f.scene, root, "energy", energy);
        f.step(seconds);
        return f.scene.registry().get<TransformComponent>(joints[0]).position.y;
    };
    // Weight 0: Idle only (no bounce); weight 1: Dance at its own pace.
    CHECK(rootYAfter(0.0f, 0.2f) == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(rootYAfter(1.0f, 0.2f) == doctest::Approx(danceAt(0.2f)).epsilon(0.02));
    CHECK(rootYAfter(3.0f, 0.2f) == doctest::Approx(danceAt(0.2f)).epsilon(0.02));  // clamped above the last motion
    // Half way: half the bounce, Dance sampled at the shared phase (cycle length between 3 s and 1.2 s).
    const float phase = 0.2f / (3.0f + (1.2f - 3.0f) * 0.5f);
    CHECK(rootYAfter(0.5f, 0.2f) == doctest::Approx(danceAt(phase * 1.2f) * 0.5f).epsilon(0.02));
    CHECK(f.anim.currentState(root) == "Move");
}
#endif

TEST_CASE("2D skeleton: skins follow their bones with smooth weights, rest pose, no skeleton = flat") {
    Scene scene;
    registerRenderComponents();
    using math::Vec3;
    const auto root = scene.createEntity("Creature");
    scene.registry().emplace<Skeleton2DComponent>(root);
    const auto b1 = scene.createEntity("Bone1", root);
    scene.registry().emplace<Bone2DComponent>(b1).length = 1.0f;
    const auto b2 = scene.createEntity("Bone2", b1);
    scene.registry().get<TransformComponent>(b2).position = {1, 0, 0};
    scene.registry().emplace<Bone2DComponent>(b2).length = 1.0f;
    const auto body = scene.createEntity("Body", root);
    auto& sk = scene.registry().emplace<Skin2DComponent>(body);
    sk.size = {2.0f, 0.4f};
    sk.pivot = {0.0f, 0.5f};
    sk.columns = 8;
    sk.rows = 2;
    captureBone2DRest(scene);
    CHECK(scene.registry().get<Bone2DComponent>(b2).restSet);
    scene.updateTransforms();
    std::vector<Vec3> pts;
    REQUIRE(skin2DPoints(scene, body, pts));
    REQUIRE(pts.size() == 9 * 3);
    auto at = [&](int i, int j) { return pts[static_cast<usize>(j * 9 + i)]; };
    // At rest: the flat grid.
    CHECK(at(0, 0).x == doctest::Approx(0.0f));
    CHECK(at(0, 0).y == doctest::Approx(-0.2f));
    CHECK(at(8, 2).x == doctest::Approx(2.0f).epsilon(0.001));
    CHECK(at(8, 2).y == doctest::Approx(0.2f).epsilon(0.001));
    // Bend the second bone up by 90 degrees: the far end follows it, the near end does not move.
    scene.registry().get<TransformComponent>(b2).rotation = math::Quat::fromAxisAngle({0, 0, 1}, math::radians(90.0f));
    scene.updateTransforms();
    REQUIRE(skin2DPoints(scene, body, pts));
    CHECK(at(8, 1).x == doctest::Approx(1.0f).epsilon(0.02));
    CHECK(at(8, 1).y == doctest::Approx(1.0f).epsilon(0.02));
    CHECK(at(0, 1).x == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(std::abs(at(0, 1).y) < 1e-3f);
    // Around the joint the points blend between both bones (no tearing): neighbours stay close.
    for (int i = 0; i < 8; ++i) CHECK(math::length(at(i + 1, 1) - at(i, 1)) < 0.6f);
    // The rest pose is kept: moving the root moves everything rigidly.
    scene.registry().get<TransformComponent>(root).position = {5, 0, 0};
    scene.updateTransforms();
    REQUIRE(skin2DPoints(scene, body, pts));
    CHECK(at(0, 1).x == doctest::Approx(5.0f).epsilon(0.001));
    // Without a skeleton: drawn flat like a sprite.
    const auto loose = scene.createEntity("Loose");
    scene.registry().emplace<Skin2DComponent>(loose).size = {1, 1};
    scene.updateTransforms();
    REQUIRE(skin2DPoints(scene, loose, pts));
    CHECK(pts.front().x == doctest::Approx(-0.5f));
    CHECK_FALSE(skin2DPoints(scene, root, pts));
}

TEST_CASE("2D two-bone IK reaches the target, bends either way, clamps, blends") {
    Scene scene;
    registerRenderComponents();
    registerAnimationComponents();
    using math::Vec3;
    const auto shoulder = scene.createEntity("Shoulder");
    scene.registry().get<TransformComponent>(shoulder).position = {2, 3, 0};
    const auto upper = scene.createEntity("Upper", shoulder);
    scene.registry().emplace<Bone2DComponent>(upper).length = 1.0f;
    const auto lower = scene.createEntity("Lower", upper);
    scene.registry().get<TransformComponent>(lower).position = {1, 0, 0};
    scene.registry().emplace<Bone2DComponent>(lower).length = 1.0f;
    const auto target = scene.createEntity("Target");
    scene.registry().get<TransformComponent>(target).position = {3, 4, 0};  // (1, 1) from the shoulder
    auto& ik = scene.registry().emplace<TwoBoneIK2DComponent>(upper);
    ik.target = target;
    auto tip = [&] { return scene.worldMatrix(lower).transformPoint(Vec3(1, 0, 0)); };
    auto elbow = [&] { return scene.worldMatrix(lower).transformPoint(Vec3(0.0f)); };
    CHECK(solveIK2D(scene) == 1);
    CHECK(math::length(tip() - Vec3(3, 4, 0)) < 1e-3f);
    const Vec3 elbowA = elbow();
    ik.flipBend = true;
    solveIK2D(scene);
    CHECK(math::length(tip() - Vec3(3, 4, 0)) < 1e-3f);
    CHECK(math::length(elbow() - elbowA) > 0.5f);  // the other elbow position
    // Out of reach: the arm points straight at the target.
    scene.registry().get<TransformComponent>(target).position = {12, 3, 0};
    solveIK2D(scene);
    CHECK(tip().x == doctest::Approx(4.0f).epsilon(0.001));
    CHECK(tip().y == doctest::Approx(3.0f).epsilon(0.01));
    // Weight 0: untouched; disabled: untouched.
    ik.weight = 0.0f;
    const auto before = scene.registry().get<TransformComponent>(upper).rotation;
    scene.registry().get<TransformComponent>(target).position = {2, 5, 0};
    CHECK(solveIK2D(scene) == 0);
    CHECK(scene.registry().get<TransformComponent>(upper).rotation.z == doctest::Approx(before.z));
    // Through the animation system, every frame.
    ik.weight = 1.0f;
    VirtualFileSystem vfs;
    AnimationSystem anim{vfs};
    anim.update(scene, 0.016f);
    CHECK(anim.stats().ik2DChains == 1);
    CHECK(math::length(tip() - Vec3(2, 5, 0)) < 1e-3f);
}

TEST_CASE("layers: masks, weights, their own states; blend states mix property tracks; 2D blend spaces") {
    AnimFixture f;
    auto clipXY = [](const char* target, float x, float y) {
        AnimationClip c;
        c.duration = 1.0f;
        c.loop = true;
        c.track(target, "Transform", "position", 0).curve.setKey(0.0f, x);
        c.track(target, "Transform", "position", 1).curve.setKey(0.0f, y);
        return c;
    };
    // Base: legs and arms at x = 1. Layer "Upper" (mask: Arms): arms (and legs, masked out) at x = 5.
    AnimationClip base;
    base.duration = 1.0f;
    base.loop = true;
    base.track("Legs", "Transform", "position", 0).curve.setKey(0.0f, 1.0f);
    base.track("Arms", "Transform", "position", 0).curve.setKey(0.0f, 1.0f);
    AnimationClip shoot;
    shoot.duration = 1.0f;
    shoot.loop = true;
    shoot.track("Legs", "Transform", "position", 0).curve.setKey(0.0f, 5.0f);
    shoot.track("Arms", "Transform", "position", 0).curve.setKey(0.0f, 5.0f);
    AnimationClip wave = constantX(9.0f);
    f.save("assets://base.agranim", base);
    f.save("assets://shoot.agranim", shoot);
    AnimationClip waveArms;
    waveArms.duration = 1.0f;
    waveArms.track("Arms", "Transform", "position", 0).curve.setKey(0.0f, 9.0f);
    f.save("assets://wave.agranim", waveArms);
    AnimatorController ctrl;
    ctrl.parameters = {{"aim", ParameterType::Bool, 0.0f}};
    ctrl.states = {{"Run", "assets://base.agranim"}};
    AnimatorLayer upper;
    upper.name = "Upper";
    upper.mask = {"Arms"};
    upper.states = {{"Shoot", "assets://shoot.agranim"}, {"Wave", "assets://wave.agranim"}};
    upper.transitions = {{"Shoot", "Wave", {{"aim", ConditionOp::IsTrue, 0.0f}}, -1.0f, 0.0f}};
    ctrl.layers = {upper};
    CHECK(ctrl.validate().empty());
    // JSON round trip keeps the layers.
    auto parsed = AnimatorController::fromJson(ctrl.toJson());
    REQUIRE(parsed);
    REQUIRE(parsed.value().layers.size() == 1);
    CHECK(parsed.value().layers[0].mask == std::vector<std::string>{"Arms"});
    CHECK(parsed.value().layers[0].states.size() == 2);
    CHECK(upper.masks("Arms"));
    CHECK(upper.masks("Arms/Hand"));
    CHECK_FALSE(upper.masks("Armsy"));
    CHECK_FALSE(upper.masks("Legs"));
    AnimatorController bad = ctrl;
    bad.layers[0].transitions[0].to = "Nope";
    CHECK_FALSE(bad.validate().empty());
    f.save("assets://hero.agrctrl", ctrl);

    const auto hero = f.scene.createEntity("Hero");
    const auto legs = f.scene.createEntity("Legs", hero);
    const auto arms = f.scene.createEntity("Arms", hero);
    f.scene.registry().emplace<AnimatorComponent>(hero).controller = "assets://hero.agrctrl";
    f.step(0.1f);
    CHECK(f.x(legs) == doctest::Approx(1.0f));  // outside the mask: the base layer
    CHECK(f.x(arms) == doctest::Approx(5.0f));  // the layer wins inside its mask
    CHECK(f.anim.currentState(hero, "Upper") == "Shoot");
    CHECK(f.anim.setLayerWeight(f.scene, hero, "Upper", 0.5f));
    CHECK(f.anim.layerWeight(f.scene, hero, "Upper").value() == doctest::Approx(0.5f));
    f.step(0.1f);
    CHECK(f.x(arms) == doctest::Approx(3.0f));  // half way
    f.anim.setLayerWeight(f.scene, hero, "Upper", 0.0f);
    f.step(0.1f);
    CHECK(f.x(arms) == doctest::Approx(1.0f));
    f.anim.setLayerWeight(f.scene, hero, "Upper", 1.0f);
    CHECK(f.anim.setBool(f.scene, hero, "aim", true));  // the layer's transitions read the shared parameters
    f.step(0.1f);
    CHECK(f.anim.currentState(hero, "Upper") == "Wave");
    CHECK(f.x(arms) == doctest::Approx(9.0f));
    f.anim.setBool(f.scene, hero, "aim", false);
    CHECK(f.anim.play(f.scene, hero, "Shoot", 0.0f, "Upper"));
    CHECK_FALSE(f.anim.play(f.scene, hero, "Run", 0.0f, "Upper"));  // not a state of that layer
    CHECK_FALSE(f.anim.play(f.scene, hero, "Shoot", 0.0f, "Nope"));
    CHECK_FALSE(f.anim.setLayerWeight(f.scene, hero, "Nope", 1.0f));
    f.step(0.1f);
    CHECK(f.x(arms) == doctest::Approx(5.0f));
    CHECK(f.anim.currentState(hero) == "Run");

    // 1D blend of property tracks (not only imported transform tracks).
    f.save("assets://left.agranim", clipXY("", -10.0f, 0.0f));
    f.save("assets://right.agranim", clipXY("", 10.0f, 0.0f));
    f.save("assets://up.agranim", clipXY("", 0.0f, 10.0f));
    f.save("assets://down.agranim", clipXY("", 0.0f, -10.0f));
    f.save("assets://center.agranim", clipXY("", 0.0f, 0.0f));
    AnimatorController oneD;
    oneD.parameters = {{"speed", ParameterType::Float, 0.0f}};
    AnimatorState move;
    move.name = "Move";
    move.blendParameter = "speed";
    move.motions = {{"assets://left.agranim", 0.0f}, {"assets://right.agranim", 1.0f}};
    oneD.states = {move};
    f.save("assets://oned.agrctrl", oneD);
    const auto walker = f.scene.createEntity("Walker");
    f.scene.registry().emplace<AnimatorComponent>(walker).controller = "assets://oned.agrctrl";
    f.anim.setFloat(f.scene, walker, "speed", 0.75f);
    f.step(0.1f);
    CHECK(f.x(walker) == doctest::Approx(5.0f));

    // 2D blend space: center and four directions.
    AnimatorController twoD;
    twoD.parameters = {{"mx", ParameterType::Float, 0.0f}, {"my", ParameterType::Float, 0.0f}};
    AnimatorState strafe;
    strafe.name = "Strafe";
    strafe.blendParameter = "mx";
    strafe.blendParameterY = "my";
    strafe.motions = {{"assets://center.agranim", 0.0f, {0, 0}}, {"assets://right.agranim", 0.0f, {1, 0}}, {"assets://left.agranim", 0.0f, {-1, 0}},
                      {"assets://up.agranim", 0.0f, {0, 1}},     {"assets://down.agranim", 0.0f, {0, -1}}};
    twoD.states = {strafe};
    CHECK(twoD.validate().empty());
    auto p2 = AnimatorController::fromJson(twoD.toJson());
    REQUIRE(p2);
    REQUIRE(p2.value().states[0].isBlend2D());
    CHECK(p2.value().states[0].motions[3].position.y == doctest::Approx(1.0f));
    CHECK(p2.value().states[0].motions[1].clip == "assets://right.agranim");  // 2D motions keep their order
    f.save("assets://twod.agrctrl", twoD);
    const auto strafer = f.scene.createEntity("Strafer");
    f.scene.registry().emplace<AnimatorComponent>(strafer).controller = "assets://twod.agrctrl";
    auto pos = [&] { return f.scene.registry().get<TransformComponent>(strafer).position; };
    f.anim.setFloat(f.scene, strafer, "mx", 1.0f);
    f.step(0.1f);
    CHECK(pos().x == doctest::Approx(10.0f));  // exactly on a motion: that motion alone
    CHECK(pos().y == doctest::Approx(0.0f));
    f.anim.setFloat(f.scene, strafer, "mx", 0.5f);
    f.step(0.1f);
    CHECK(pos().x == doctest::Approx(5.0f));  // half way between the center and "right"
    f.anim.setFloat(f.scene, strafer, "mx", 0.0f);
    f.anim.setFloat(f.scene, strafer, "my", -1.0f);
    f.step(0.1f);
    CHECK(pos().y == doctest::Approx(-10.0f));
    f.anim.setFloat(f.scene, strafer, "mx", 0.5f);
    f.anim.setFloat(f.scene, strafer, "my", 0.5f);
    f.step(0.1f);
    CHECK(pos().x > 1.0f);  // diagonal: some of "right" and of "up"
    CHECK(pos().y > 1.0f);
    CHECK(pos().x == doctest::Approx(pos().y));
    f.anim.setFloat(f.scene, strafer, "mx", 3.0f);  // far outside: clamped to the nearest band
    f.anim.setFloat(f.scene, strafer, "my", 0.0f);
    f.step(0.1f);
    CHECK(pos().x == doctest::Approx(10.0f));
    (void)wave;
}

TEST_CASE("additive layers add their change from the first frame, scaled by the weight") {
    AnimFixture f;
    AnimationClip base;
    base.duration = 1.0f;
    base.track("Arms", "Transform", "position", 0).curve.setKey(0.0f, 1.0f);
    base.track("Arms", "Transform", "rotation", 2).curve.setKey(0.0f, 30.0f);
    f.save("assets://base.agranim", base);
    AnimationClip breath;  // +2 on x and +20 degrees over one second, from its own first frame
    breath.duration = 1.0f;
    breath.loop = false;
    auto& bx = breath.track("Arms", "Transform", "position", 0).curve;
    bx.setKey(0.0f, 10.0f, Interpolation::Linear);
    bx.setKey(1.0f, 12.0f, Interpolation::Linear);
    auto& bz = breath.track("Arms", "Transform", "rotation", 2).curve;
    bz.setKey(0.0f, 0.0f, Interpolation::Linear);
    bz.setKey(1.0f, 20.0f, Interpolation::Linear);
    f.save("assets://breath.agranim", breath);
    AnimatorController ctrl;
    ctrl.states = {{"Idle", "assets://base.agranim"}};
    AnimatorLayer add;
    add.name = "Breath";
    add.additive = true;
    add.states = {{"Breathe", "assets://breath.agranim"}};
    ctrl.layers = {add};
    auto parsed = AnimatorController::fromJson(ctrl.toJson());
    REQUIRE(parsed);
    CHECK(parsed.value().layers[0].additive);
    f.save("assets://c.agrctrl", ctrl);
    const auto hero = f.scene.createEntity("Hero");
    const auto arms = f.scene.createEntity("Arms", hero);
    f.scene.registry().emplace<AnimatorComponent>(hero).controller = "assets://c.agrctrl";
    f.step(0.5f);
    CHECK(f.x(arms) == doctest::Approx(2.0f));  // 1 + (11 - 10)
    auto zDeg = [&] { return math::degrees(f.scene.registry().get<TransformComponent>(arms).rotation.toEuler().z); };
    CHECK(zDeg() == doctest::Approx(40.0f).epsilon(0.01));  // 30 + 10
    f.anim.setLayerWeight(f.scene, hero, "Breath", 0.5f);
    f.step(0.0f);
    CHECK(f.x(arms) == doctest::Approx(1.5f));
    CHECK(zDeg() == doctest::Approx(35.0f).epsilon(0.01));
    f.anim.setLayerWeight(f.scene, hero, "Breath", 0.0f);
    f.step(0.0f);
    CHECK(f.x(arms) == doctest::Approx(1.0f));
}

TEST_CASE("3D IK: two bones reach the target and bend toward the pole; look-at within a limit") {
    Scene scene;
    registerAnimationComponents();
    using math::Vec3;
    const auto body = scene.createEntity("Body");
    scene.registry().get<TransformComponent>(body).position = {3, 1, -2};
    scene.registry().get<TransformComponent>(body).rotation = math::Quat::fromAxisAngle({0, 1, 0}, 0.7f);  // any orientation
    const auto upper = scene.createEntity("Upper", body);
    const auto mid = scene.createEntity("Mid", upper);
    scene.registry().get<TransformComponent>(mid).position = {0, -1, 0};
    const auto tip = scene.createEntity("Tip", mid);
    scene.registry().get<TransformComponent>(tip).position = {0, -1, 0};
    const auto target = scene.createEntity("Target");
    scene.registry().get<TransformComponent>(target).position = {3.8f, 0.2f, -1.5f};
    const auto pole = scene.createEntity("Pole");
    scene.registry().get<TransformComponent>(pole).position = {3, 1, 5};
    auto& ik = scene.registry().emplace<TwoBoneIK3DComponent>(upper);
    ik.target = target;
    ik.pole = pole;
    auto wpos = [&](ecs::Entity e) { return scene.worldMatrix(e).transformPoint(Vec3(0.0f)); };
    for (int i = 0; i < 2; ++i) solveIK3D(scene);  // stable: a second solve changes nothing
    CHECK(math::length(wpos(tip) - wpos(target)) < 1e-3f);
    CHECK(math::length(wpos(mid) - wpos(upper)) == doctest::Approx(1.0f).epsilon(0.001));  // bone lengths kept
    CHECK(math::length(wpos(tip) - wpos(mid)) == doctest::Approx(1.0f).epsilon(0.001));
    // The elbow is on the pole's side of the line shoulder -> target.
    const Vec3 a = wpos(upper), t = wpos(target), dir = math::normalize(t - a);
    auto side = [&](Vec3 p) { const Vec3 v = p - a; return v - dir * math::dot(v, dir); };
    CHECK(math::dot(side(wpos(mid)), side(wpos(pole))) > 0.0f);
    // Move the pole to the other side: the elbow follows.
    scene.registry().get<TransformComponent>(pole).position = {3, 1, -9};
    solveIK3D(scene);
    CHECK(math::length(wpos(tip) - wpos(target)) < 1e-3f);
    CHECK(math::dot(side(wpos(mid)), side(wpos(pole))) > 0.0f);
    // Out of reach: the arm points straight at it.
    scene.registry().get<TransformComponent>(target).position = {3, 1, 10};
    solveIK3D(scene);
    CHECK(wpos(tip).z == doctest::Approx(0.0f).epsilon(0.01));
    CHECK(math::length(wpos(tip) - wpos(upper)) == doctest::Approx(2.0f).epsilon(0.001));
    // Weight 0: nothing; invalid chain: not solved.
    ik.weight = 0.0f;
    CHECK_FALSE(solveTwoBoneIK3D(scene, upper, ik));
    TwoBoneIK3DComponent lonely;
    lonely.target = target;
    CHECK_FALSE(solveTwoBoneIK3D(scene, tip, lonely));

    // Look-at, limited to 45 degrees.
    const auto head = scene.createEntity("Head");
    const auto seen = scene.createEntity("Seen");
    scene.registry().get<TransformComponent>(seen).position = {5, 0, 0};
    auto& la = scene.registry().emplace<LookAtIK3DComponent>(head);
    la.target = seen;
    la.maxAngle = 3.1f;  // the default limit (80 degrees) is below the 90 degrees needed here
    solveIK3D(scene);
    const Vec3 fwd = scene.worldMatrix(head).transformVector({0, 0, 1});
    CHECK(fwd.x == doctest::Approx(1.0f).epsilon(0.001));
    scene.registry().get<TransformComponent>(head).rotation = {};
    la.maxAngle = math::radians(45.0f);
    solveIK3D(scene);
    const Vec3 fwd2 = scene.worldMatrix(head).transformVector({0, 0, 1});
    CHECK(std::acos(std::clamp(fwd2.z, -1.0f, 1.0f)) == doctest::Approx(math::radians(45.0f)).epsilon(0.001));
    // Run by the animation system after the clips.
    VirtualFileSystem vfs;
    AnimationSystem anim{vfs};
    ik.weight = 1.0f;
    anim.update(scene, 0.016f);
    CHECK(anim.stats().ik3DChains == 2);
}


TEST_CASE("chain IK (FABRIK): long 3D chains reach, keep their lengths, bend limits, poles; 2D chains") {
    Scene scene;
    registerAnimationComponents();
    registerRenderComponents();
    using math::Vec3;
    // A 5-bone tentacle going up from the origin, bones of 1.
    std::vector<ecs::Entity> bones{scene.createEntity("B0")};
    for (int i = 1; i <= 5; ++i) {
        bones.push_back(scene.createEntity(("B" + std::to_string(i)).c_str(), bones.back()));
        scene.registry().get<TransformComponent>(bones.back()).position = {0, 1, 0};
    }
    const auto target = scene.createEntity("Target");
    scene.registry().get<TransformComponent>(target).position = {2.5f, 2.0f, 1.0f};
    auto& ik = scene.registry().emplace<ChainIK3DComponent>(bones[0]);
    ik.target = target;
    ik.boneCount = 5;
    auto wpos = [&](ecs::Entity e) { return scene.worldMatrix(e).transformPoint(Vec3(0.0f)); };
    CHECK(solveChainIK3D(scene, bones[0], ik));
    CHECK(math::length(wpos(bones[5]) - wpos(target)) < 0.01f);
    for (int i = 0; i < 5; ++i) CHECK(math::length(wpos(bones[i + 1]) - wpos(bones[i])) == doctest::Approx(1.0f).epsilon(0.001));
    CHECK(math::length(wpos(bones[0])) < 1e-5f);  // the root stays
    // Out of reach: stretched toward the target.
    scene.registry().get<TransformComponent>(target).position = {20, 0, 0};
    solveChainIK3D(scene, bones[0], ik);
    CHECK(wpos(bones[5]).x == doctest::Approx(5.0f).epsilon(0.01));
    // Bend limit: a target folded back below the root cannot be reached with joints of 20 degrees.
    scene.registry().get<TransformComponent>(target).position = {0.5f, -1.0f, 0};
    ik.maxAngle = math::radians(20.0f);
    solveChainIK3D(scene, bones[0], ik);
    for (int i = 1; i < 5; ++i) {
        const Vec3 a = math::normalize(wpos(bones[i]) - wpos(bones[i - 1])), b = math::normalize(wpos(bones[i + 1]) - wpos(bones[i]));
        CHECK(std::acos(std::clamp(math::dot(a, b), -1.0f, 1.0f)) <= math::radians(20.0f) + 0.02f);
    }
    // Pole: the middle joints bend toward it.
    ik.maxAngle = 3.1416f;
    scene.registry().get<TransformComponent>(target).position = {0, 3, 0};
    const auto pole = scene.createEntity("Pole");
    scene.registry().get<TransformComponent>(pole).position = {0, 1.5f, 5};
    ik.pole = pole;
    solveChainIK3D(scene, bones[0], ik);
    CHECK(wpos(bones[2]).z > 0.3f);
    CHECK(math::length(wpos(bones[5]) - wpos(target)) < 0.05f);

    // 2D chain of 4 bones of length 1 along +X.
    std::vector<ecs::Entity> b2{scene.createEntity("T0")};
    scene.registry().emplace<Bone2DComponent>(b2[0]).length = 1.0f;
    for (int i = 1; i < 4; ++i) {
        b2.push_back(scene.createEntity("T", b2.back()));
        scene.registry().get<TransformComponent>(b2.back()).position = {1, 0, 0};
        scene.registry().emplace<Bone2DComponent>(b2.back()).length = 1.0f;
    }
    const auto t2 = scene.createEntity("T2");
    scene.registry().get<TransformComponent>(t2).position = {1.5f, 2.0f, 0};
    auto& c2 = scene.registry().emplace<ChainIK2DComponent>(b2[0]);
    c2.target = t2;
    c2.boneCount = 4;
    CHECK(solveIK2D(scene) == 1);
    const Vec3 tip = scene.worldMatrix(b2[3]).transformPoint(Vec3(1.0f, 0.0f, 0.0f));
    CHECK(math::length(tip - Vec3{1.5f, 2.0f, 0}) < 0.02f);
    CHECK(tip.z == doctest::Approx(0.0f));
}

TEST_CASE("interruptible transitions and weighted masks") {
    AnimFixture f;
    f.save("assets://a.agranim", constantX(0.0f));
    f.save("assets://b.agranim", constantX(10.0f));
    f.save("assets://c.agranim", constantX(-10.0f));
    AnimatorController ctrl;
    ctrl.parameters = {{"go", ParameterType::Trigger, 0.0f}, {"hit", ParameterType::Trigger, 0.0f}};
    ctrl.states = {{"A", "assets://a.agranim"}, {"B", "assets://b.agranim"}, {"C", "assets://c.agranim"}};
    AnimatorTransition ab{"A", "B", {{"go", ConditionOp::IsTrue, 0.0f}}, -1.0f, 1.0f};
    ab.interruptible = true;
    ctrl.transitions = {ab, {"B", "C", {{"hit", ConditionOp::IsTrue, 0.0f}}, -1.0f, 0.5f}};
    auto parsed = AnimatorController::fromJson(ctrl.toJson());
    REQUIRE(parsed);
    CHECK(parsed.value().transitions[0].interruptible);
    CHECK_FALSE(parsed.value().transitions[1].interruptible);
    f.save("assets://ctrl.agrctrl", ctrl);
    const auto e = f.scene.createEntity("E");
    f.scene.registry().emplace<AnimatorComponent>(e).controller = "assets://ctrl.agrctrl";
    f.step(0.1f);
    f.anim.setTrigger(f.scene, e, "go");
    f.step(0.1f);
    CHECK(f.anim.currentState(e) == "B");
    f.step(0.2f);  // mid-fade toward B (1 s)
    const float before = f.x(e);
    CHECK(before > 1.0f);
    CHECK(before < 6.0f);
    f.anim.setTrigger(f.scene, e, "hit");
    f.step(0.05f);
    CHECK(f.anim.currentState(e) == "C");  // interrupted the A -> B fade
    // No jump: the B -> C fade (0.5 s) starts from the pose on screen, not from B's 10.
    const float after = f.x(e);
    CHECK(after < before + 0.5f);
    CHECK(after > before - 2.5f);
    CHECK(f.anim.stats().interruptions == 1);
    for (int i = 0; i < 20; ++i) f.step(0.05f);
    CHECK(f.x(e) == doctest::Approx(-10.0f).epsilon(0.01));

    // Weighted mask entries: the most specific one wins.
    AnimatorLayer l;
    l.mask = {"Body:0.5", "Body/Arm", "Tail:0"};
    CHECK(l.maskWeight("Body") == doctest::Approx(0.5f));
    CHECK(l.maskWeight("Body/Leg") == doctest::Approx(0.5f));
    CHECK(l.maskWeight("Body/Arm/Hand") == doctest::Approx(1.0f));
    CHECK(l.maskWeight("Tail") == doctest::Approx(0.0f));
    CHECK(l.maskWeight("Head") == doctest::Approx(0.0f));
    // In a layer: a child at half weight lands half way between the base and the layer.
    AnimationClip baseClip;
    baseClip.duration = 1.0f;
    baseClip.loop = true;
    baseClip.track("Arm", "Transform", "position", 0).curve.setKey(0.0f, 0.0f);
    baseClip.track("Leg", "Transform", "position", 0).curve.setKey(0.0f, 0.0f);
    AnimationClip layerClip = baseClip;
    layerClip.tracks[0].curve.setKey(0.0f, 8.0f);
    layerClip.tracks[1].curve.setKey(0.0f, 8.0f);
    f.save("assets://base2.agranim", baseClip);
    f.save("assets://layer2.agranim", layerClip);
    AnimatorController c2;
    c2.states = {{"Base", "assets://base2.agranim"}};
    AnimatorLayer up;
    up.name = "Up";
    up.mask = {"Arm", "Leg:0.25"};
    up.states = {{"L", "assets://layer2.agranim"}};
    c2.layers = {up};
    f.save("assets://c2.agrctrl", c2);
    const auto h = f.scene.createEntity("H");
    const auto arm = f.scene.createEntity("Arm", h);
    const auto leg = f.scene.createEntity("Leg", h);
    f.scene.registry().emplace<AnimatorComponent>(h).controller = "assets://c2.agrctrl";
    f.step(0.1f);
    f.step(0.1f);
    CHECK(f.x(arm) == doctest::Approx(8.0f));
    CHECK(f.x(leg) == doctest::Approx(2.0f));
}

TEST_CASE("clip key operations: copy / paste at a time, move several keys, remove") {
    AnimationClip c;
    AnimationTrack& x = c.track("", "Transform", "position", 0);
    x.curve.setKey(0.0f, 0.0f);
    x.curve.setKey(0.5f, 1.0f, Interpolation::Linear);
    x.curve.setKey(1.0f, 2.0f);
    c.track("Arm", "Transform", "position", 1).curve.setKey(0.5f, 5.0f);
    const std::vector<KeyRef> sel = {{0, 0.5f}, {0, 1.0f}, {1, 0.5f}};
    const auto copied = c.copyKeys(sel);
    REQUIRE(copied.size() == 3);
    CHECK(copied[0].offset == doctest::Approx(0.0f));
    CHECK(copied[1].offset == doctest::Approx(0.5f));
    // Pasted at 2 s: same tracks, same spacing, interpolation kept.
    const auto pasted = c.pasteKeys(copied, 2.0f);
    REQUIRE(pasted.size() == 3);
    CHECK(c.tracks[0].curve.keys.size() == 5);
    CHECK(c.tracks[0].curve.evaluate(2.5f) == doctest::Approx(2.0f));
    CHECK(c.tracks[0].curve.keys[3].interpolation == Interpolation::Linear);
    CHECK(c.tracks[1].curve.evaluate(2.0f) == doctest::Approx(5.0f));
    // Into a clip without the track: created.
    AnimationClip other;
    other.pasteKeys(copied, 0.0f);
    CHECK(other.tracks.size() == 2);
    // Moving the pasted keys back by 0.25 s.
    const auto moved = c.moveKeys(pasted, -0.25f);
    REQUIRE(moved.size() == 3);
    CHECK(moved[0].time == doctest::Approx(1.75f));
    CHECK(c.tracks[0].curve.keys[3].time == doctest::Approx(1.75f));
    CHECK(c.moveKeys({{0, 0.0f}}, -1.0f)[0].time == doctest::Approx(0.0f));  // clamped at 0
    c.removeKeys(moved);
    CHECK(c.tracks[0].curve.keys.size() == 3);
    CHECK(c.tracks[1].curve.keys.size() == 1);
}

TEST_CASE("chain IK: per joint bend limits") {
    Scene scene;
    registerAnimationComponents();
    registerRenderComponents();
    using math::Vec3;
    std::vector<ecs::Entity> bones{scene.createEntity("B0")};
    for (int i = 1; i <= 4; ++i) {
        bones.push_back(scene.createEntity(("B" + std::to_string(i)).c_str(), bones.back()));
        scene.registry().get<TransformComponent>(bones.back()).position = {0, 1, 0};
    }
    const auto target = scene.createEntity("Target");
    scene.registry().get<TransformComponent>(target).position = {2.0f, 1.0f, 0};
    auto& ik = scene.registry().emplace<ChainIK3DComponent>(bones[0]);
    ik.target = target;
    ik.boneCount = 4;
    ik.jointLimits = "0, , 10";  // joint 1 straight, joint 2 free (maxAngle), joint 3 at most 10 degrees
    auto wpos = [&](ecs::Entity e) { return scene.worldMatrix(e).transformPoint(Vec3(0.0f)); };
    auto bend = [&](int j) {
        const Vec3 a = math::normalize(wpos(bones[j]) - wpos(bones[j - 1])), b = math::normalize(wpos(bones[j + 1]) - wpos(bones[j]));
        return std::acos(std::clamp(math::dot(a, b), -1.0f, 1.0f));
    };
    solveChainIK3D(scene, bones[0], ik);
    CHECK(bend(1) < math::radians(1.0f));
    CHECK(bend(3) <= math::radians(10.0f) + 0.02f);
    CHECK(bend(2) > math::radians(20.0f));  // the free joint takes the bend
}
TEST_CASE("chain IK: elliptical joint limits (front x side) bend freely one way and barely the other") {
    Scene scene;
    registerAnimationComponents();
    registerRenderComponents();
    using math::Vec3;
    // A chain along +X; "front" = up (no pole), "side" = Z.
    std::vector<ecs::Entity> bones{scene.createEntity("B0")};
    for (int i = 1; i <= 3; ++i) {
        bones.push_back(scene.createEntity(("B" + std::to_string(i)).c_str(), bones.back()));
        scene.registry().get<TransformComponent>(bones.back()).position = {1, 0, 0};
    }
    const auto target = scene.createEntity("Target");
    auto& ik = scene.registry().emplace<ChainIK3DComponent>(bones[0]);
    ik.target = target;
    ik.boneCount = 3;
    ik.iterations = 32;
    ik.jointLimits = "10x80, 10x80";  // 10 degrees up / down, 80 sideways
    auto wpos = [&](ecs::Entity e) { return scene.worldMatrix(e).transformPoint(Vec3(0.0f)); };
    auto reset = [&] {
        for (int i = 1; i <= 3; ++i) scene.registry().get<TransformComponent>(bones[i]).rotation = math::Quat::identity();
        scene.registry().get<TransformComponent>(bones[0]).rotation = math::Quat::identity();
        scene.updateTransforms();
    };
    auto reach = [&](Vec3 goal) {
        reset();
        scene.registry().get<TransformComponent>(target).position = goal;
        scene.updateTransforms();
        solveChainIK3D(scene, bones[0], ik);
        scene.updateTransforms();
        return math::length(wpos(bones[3]) - goal);
    };
    auto joint = [&](int j) {  // bend of joint j split into its up (y) and sideways (z) parts
        const Vec3 a = math::normalize(wpos(bones[j]) - wpos(bones[j - 1])), b = math::normalize(wpos(bones[j + 1]) - wpos(bones[j]));
        return std::acos(std::clamp(math::dot(a, b), -1.0f, 1.0f));
    };
    // Curling sideways (in the XZ plane) is allowed: reached.
    CHECK(reach({1.6f, 0, 1.6f}) < 0.05f);
    // Curling up as much is not: the inner joints stay within ~10 degrees, the target is missed.
    const float upMiss = reach({1.6f, 1.6f, 0});
    CHECK(upMiss > 0.3f);
    CHECK(joint(1) <= math::radians(10.0f) + 0.03f);
    CHECK(joint(2) <= math::radians(10.0f) + 0.03f);
    // The same chain with round 80 degree limits reaches both.
    ik.jointLimits = "80, 80";
    CHECK(reach({1.6f, 1.6f, 0}) < 0.05f);
}

TEST_CASE("imported clips become editable curves: faithful, unwrapped rotations, redundant keys removed") {
    AnimationClip imported;
    imported.duration = 2.0f;
    imported.events = {{1.0f, "step"}};
    TransformTrack move{"Body", TransformTrack::Path::Translation};
    TransformTrack spin{"Body/Head", TransformTrack::Path::Rotation};
    for (int i = 0; i <= 120; ++i) {
        const float t = i / 60.0f;
        move.times.push_back(t);
        move.values.push_back({3.0f * t, 1.0f, 0.0f, 0.0f});  // straight line: 2 keys are enough
        spin.times.push_back(t);
        const math::Quat q = math::Quat::fromEuler({0.0f, t * 2.0f * 3.14159265f, 0.0f});  // two full turns
        spin.values.push_back({q.x, q.y, q.z, q.w});
    }
    imported.transformTracks = {move, spin};
    const AnimationClip e = makeEditableClip(imported);
    CHECK(e.transformTracks.empty());
    CHECK(e.events.size() == 1);
    CHECK(e.duration == doctest::Approx(2.0f));
    const AnimationTrack* x = e.findTrack("Body", "Transform", "position", 0);
    const AnimationTrack* y = e.findTrack("Body", "Transform", "position", 1);
    const AnimationTrack* yaw = e.findTrack("Body/Head", "Transform", "rotation", 1);
    REQUIRE(x);
    REQUIRE(y);
    REQUIRE(yaw);
    CHECK(x->curve.keys.size() == 2);  // the straight line
    CHECK(y->curve.keys.size() == 2);  // constant
    CHECK(yaw->curve.keys.size() < 121);
    for (float t : {0.0f, 0.37f, 1.0f, 1.61f, 2.0f}) {
        CHECK(x->curve.evaluate(t) == doctest::Approx(3.0f * t).epsilon(0.002));
        CHECK(yaw->curve.evaluate(t) == doctest::Approx(t * 360.0f).epsilon(0.002));  // unwrapped: no jump back at 180
    }
    // Round trip through the .agranim format.
    auto back = AnimationClip::fromJson(e.toJson());
    REQUIRE(back);
    CHECK(back.value().tracks.size() == e.tracks.size());
}
}  // TEST_SUITE

TEST_SUITE("animation") {
TEST_CASE("root motion: the root bone's walk moves the character (loops, rotation, vertical kept, delta only)") {
    AnimFixture f;
    AnimationClip walk;
    walk.duration = 1.0f;
    walk.loop = true;
    auto& x = walk.track("Hips", "Transform", "position", 0).curve;
    x.setKey(0.0f, 0.0f);
    x.setKey(1.0f, 2.0f);  // 2 m per cycle
    auto& y = walk.track("Hips", "Transform", "position", 1).curve;
    y.setKey(0.0f, 1.0f);
    y.setKey(0.5f, 1.2f);  // bobbing
    y.setKey(1.0f, 1.0f);
    f.save("assets://walk.agranim", walk);
    const auto hero = f.scene.createEntity("Hero");
    const auto hips = f.scene.createEntity("Hips", hero);
    auto& a = f.scene.registry().emplace<AnimatorComponent>(hero);
    a.clip = "assets://walk.agranim";
    a.rootMotion = "Hips";
    f.scene.updateTransforms();
    for (int i = 0; i < 120; ++i) {  // 2 s in 1/60 steps: two cycles, across the loop
        f.step(1.0f / 60.0f);
        f.scene.updateTransforms();
    }
    CHECK(f.x(hero) == doctest::Approx(4.0f).epsilon(0.02));
    CHECK(f.x(hips) == doctest::Approx(0.0f));  // the bone stays in place horizontally
    const float hipsY = f.scene.registry().get<TransformComponent>(hips).position.y;
    CHECK(hipsY >= 0.99f);
    CHECK(f.scene.registry().get<TransformComponent>(hero).position.y == doctest::Approx(0.0f));  // vertical stays on the bone
    // Turned 90 degrees: the walk goes along the character's own forward (+x local = -z world).
    auto& t = f.scene.registry().get<TransformComponent>(hero);
    t.position = {0, 0, 0};
    t.rotation = math::Quat::fromAxisAngle({0, 1, 0}, math::radians(90.0f));
    f.scene.updateTransforms();
    for (int i = 0; i < 30; ++i) {
        f.step(1.0f / 60.0f);
        f.scene.updateTransforms();
    }
    CHECK(f.scene.registry().get<TransformComponent>(hero).position.z == doctest::Approx(-1.0f).epsilon(0.05));
    CHECK(std::abs(f.x(hero)) < 0.01f);
    // Delta only: the entity stays, scripts read rootMotionDelta.
    f.scene.registry().get<AnimatorComponent>(hero).applyRootMotion = false;
    const math::Vec3 before = f.scene.registry().get<TransformComponent>(hero).position;
    f.step(0.1f);
    CHECK(f.scene.registry().get<TransformComponent>(hero).position.z == doctest::Approx(before.z));
    const math::Vec3 d = f.scene.registry().get<AnimatorComponent>(hero).rootMotionDelta;
    CHECK(d.z < -0.1f);  // forward (smooth curve: faster mid-cycle than the 2 m/s average)
    CHECK(d.z > -0.4f);
    CHECK(std::abs(d.x) < 0.01f);
}
}

TEST_SUITE("animation") {
TEST_CASE("root motion in a blend state: walk / run mixed by speed, the distance follows the weights") {
    AnimFixture f;
    auto makeClip = [&](const char* uri, float meters) {
        AnimationClip c;
        c.duration = 1.0f;
        c.loop = true;
        auto& x = c.track("Hips", "Transform", "position", 0).curve;
        x.setKey(0.0f, 0.0f);
        x.setKey(1.0f, meters);
        f.save(uri, c);
    };
    makeClip("assets://walk.agranim", 2.0f);
    makeClip("assets://run.agranim", 6.0f);
    AnimatorController ctrl;
    ctrl.parameters = {{"speed", ParameterType::Float, 0.0f}};
    AnimatorState move;
    move.name = "Move";
    move.blendParameter = "speed";
    move.motions = {{"assets://walk.agranim", 0.0f}, {"assets://run.agranim", 1.0f}};
    ctrl.states = {move};
    f.save("assets://move.agrctrl", ctrl);
    const auto hero = f.scene.createEntity("Hero");
    const auto hips = f.scene.createEntity("Hips", hero);
    auto& a = f.scene.registry().emplace<AnimatorComponent>(hero);
    a.controller = "assets://move.agrctrl";
    a.rootMotion = "Hips";
    f.scene.updateTransforms();
    auto run = [&](float seconds) {
        for (int i = 0; i < static_cast<int>(seconds * 60.0f + 0.5f); ++i) {
            f.step(1.0f / 60.0f);
            f.scene.updateTransforms();
        }
    };
    f.anim.setFloat(f.scene, hero, "speed", 0.0f);
    run(1.0f);
    CHECK(f.x(hero) == doctest::Approx(2.0f).epsilon(0.03));
    CHECK(std::abs(f.x(hips)) < 0.01f);  // pinned in place
    f.scene.registry().get<TransformComponent>(hero).position = {0, 0, 0};
    f.anim.setFloat(f.scene, hero, "speed", 0.5f);
    run(1.0f);
    CHECK(f.x(hero) == doctest::Approx(4.0f).epsilon(0.03));
    f.scene.registry().get<TransformComponent>(hero).position = {0, 0, 0};
    f.anim.setFloat(f.scene, hero, "speed", 1.0f);
    run(0.5f);
    CHECK(f.x(hero) == doctest::Approx(3.0f).epsilon(0.03));
}
}

TEST_SUITE("animation") {
TEST_CASE("onion skinning: ghosts before / after the current time, clamped to the clip, pose restored") {
    AnimFixture f;
    AnimationClip c;
    c.duration = 1.0f;
    c.loop = false;
    auto& x = c.track("Arm", "Transform", "position", 0).curve;
    x.setKey(0.0f, 0.0f);
    x.setKey(1.0f, 10.0f);
    for (auto& k : x.keys) k.interpolation = Interpolation::Linear;
    const auto hero = f.scene.createEntity("Hero");
    const auto arm = f.scene.createEntity("Arm", hero);
    f.scene.registry().emplace<SpriteRendererComponent>(arm);
    f.scene.createEntity("Empty", hero);  // animated nothing, draws nothing: no ghost
    f.scene.updateTransforms();
    f.anim.preview(f.scene, hero, c, 0.5f);
    const auto ghosts = editor::onionSkin(f.scene, f.anim, hero, c, 0.5f, 0.1f, 2, 2);
    REQUIRE(ghosts.size() == 4);
    CHECK(ghosts[0].offset == -2);
    CHECK(ghosts[0].entity == arm);
    CHECK(ghosts[0].position.x == doctest::Approx(3.0f));
    CHECK(ghosts[3].position.x == doctest::Approx(7.0f));
    CHECK(ghosts[3].bounds.valid());
    CHECK(f.x(arm) == doctest::Approx(5.0f));  // back at the current time
    // Near the start of a clip that does not loop: no ghost before 0.
    c.loop = false;
    CHECK(editor::onionSkin(f.scene, f.anim, hero, c, 0.05f, 0.1f, 2, 1).size() == 1);
}
}
