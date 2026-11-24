#include <doctest/doctest.h>
#include "agr/physics3d/Physics3DComponents.hpp"
#include "agr/editor/ApiReference.hpp"
#include "agr/core/Etc2.hpp"
#include "agr/editor/CurveView.hpp"
#include "agr/runtime/GameManifest.hpp"
#include "agr/editor/AssetRefactor.hpp"
#include "agr/editor/ScriptDocs.hpp"
#include "agr/editor/EditHistory.hpp"
#include "agr/editor/PackageRepository.hpp"
#include "agr/editor/UiGuides.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "agr/core/Uuid.hpp"
#include "agr/assets/Environment.hpp"
#include "agr/assets/Image.hpp"
#include "agr/editor/AndroidExport.hpp"
#include "agr/editor/EditorContext.hpp"
#include "agr/editor/ParticlePresets.hpp"
#include "agr/editor/Picking.hpp"
#include "agr/editor/SceneTemplates.hpp"
#include "agr/scene/Prefab.hpp"
#include <nlohmann/json.hpp>
#include "agr/physics2d/Physics2DComponents.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/physics2d/Physics2DWorld.hpp"
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/script/ScriptEngine.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/audio/AudioSystem.hpp"
#include "agr/ui/UISystem.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/scene/SceneSerializer.hpp"
#include "agr/twod/SpriteAnimator.hpp"
#include "agr/twod/TilemapEditing.hpp"

using namespace agr;
using namespace agr::editor;
using math::Vec3;
namespace fs = std::filesystem;

namespace {

fs::path tempPath(const char* tag) { return fs::temp_directory_path() / (std::string("agr_ed_") + tag + "_" + Uuid::generate().toString()); }

std::string readTextFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if (!in) return {};
    std::string text(static_cast<usize>(in.tellg()), '\0');
    in.seekg(0);
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    return text;
}
void writeTextFile(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

struct EditorFixture {
    fs::path root = tempPath("proj");
    EngineConfig config;
    std::unique_ptr<Engine> engine;
    std::unique_ptr<EditorContext> ctx;

    explicit EditorFixture(ProjectMode mode = ProjectMode::Mode3D) {
        config.headless = true;
        config.workerThreads = 1;
        config.userDirectory = (root / "user").string();
        config.assetsDirectory = root.string();
        engine = std::make_unique<Engine>(config);
        REQUIRE(engine->initialize(nullptr));
        ctx = std::make_unique<EditorContext>(*engine);
        ProjectSettings settings;
        settings.name = "Test Game";
        settings.mode = mode;
        auto project = Project::create(root / "game", settings, defaultTemplateFor(mode), AGR_TEST_TEMPLATE_ASSETS);
        REQUIRE(project);
        REQUIRE(ctx->openProject(std::move(project.value())));
    }
    ~EditorFixture() {
        ctx.reset();
        engine.reset();
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    Scene& scene() { return ctx->scene(); }
};

}  // namespace

TEST_SUITE("editor") {

TEST_CASE("projects are created with folders, assets and a playable startup scene") {
    EditorFixture f;
    const fs::path game = f.root / "game";
    CHECK(fs::exists(game / Project::kFileName));
    CHECK(fs::exists(game / "assets/textures/agr_mark.png"));
    CHECK(fs::exists(game / "assets/2d/hero.agrsheet"));
    CHECK(fs::exists(game / "assets/3d/crate.glb"));
    CHECK(fs::exists(game / "assets/2d/forest_tiles.png"));
    CHECK(fs::exists(game / "scenes/main.agrscene"));
    CHECK(f.ctx->scenePath() == "scenes/main.agrscene");
    CHECK_FALSE(f.scene().findByName("Main Camera").isNull());
    CHECK_FALSE(f.scene().findByName("Ground").isNull());
    CHECK_FALSE(f.ctx->dirty());

    auto reopened = Project::open(game);
    REQUIRE(reopened);
    CHECK(reopened.value().settings().name == "Test Game");
    CHECK(reopened.value().settings().mode == ProjectMode::Mode3D);
    CHECK(reopened.value().listScenes() == std::vector<std::string>{"scenes/main.agrscene"});
    // Creating into a non-empty folder is refused instead of overwriting anything.
    CHECK_FALSE(Project::create(game, ProjectSettings{}, SceneTemplate::Empty3D, AGR_TEST_TEMPLATE_ASSETS));
    CHECK_FALSE(Project::open(f.root / "missing"));
}

TEST_CASE("every template contains a primary camera") {
    for (SceneTemplate t : allTemplates()) {
        Scene s;
        buildSceneTemplate(s, t);
        bool primary = false;
        s.registry().view<CameraComponent>().each([&](ecs::Entity, CameraComponent& c) { primary |= c.primary; });
        CHECK(primary);
    }
    Scene s2d;
    buildSceneTemplate(s2d, SceneTemplate::Empty2D);
    CHECK(s2d.registry().get<CameraComponent>(s2d.findByName("Main Camera")).projection == Projection::Orthographic);
}

TEST_CASE("the 2D showcase template only references shipped, valid 2D assets") {
    EditorFixture f(ProjectMode::Mode2D);
    f.ctx->newScene(SceneTemplate::Showcase2D);
    Scene& scene = f.scene();
    auto& reg = scene.registry();
    SpriteSheetLibrary sheets(f.engine->vfs());

    const ecs::Entity ground = scene.findByName("Ground");
    REQUIRE_FALSE(ground.isNull());
    const auto& tm = reg.get<TilemapComponent>(ground);
    const SpriteSheet* tiles = sheets.get(tm.tileset);
    REQUIRE(tiles != nullptr);
    u32 painted = 0;
    for (u32 y = 0; y < tm.height; ++y)
        for (u32 x = 0; x < tm.width; ++x)
            if (const i32 t = tm.tileAt(x, y); t >= 0) {
                ++painted;
                CHECK(tiles->validFrame(t));
            }
    CHECK(painted > 100);

    const ecs::Entity hero = scene.findByName("Hero");
    const auto& sr = reg.get<SpriteRendererComponent>(hero);
    const SpriteSheet* heroSheet = sheets.get(sr.sheet);
    REQUIRE(heroSheet != nullptr);
    CHECK(heroSheet->findClip(reg.get<SpriteAnimatorComponent>(hero).clip) != nullptr);
    CHECK(f.engine->vfs().exists(heroSheet->texture));
    reg.view<SpriteRendererComponent>().each([&](ecs::Entity, SpriteRendererComponent& s) {
        if (!s.texture.empty()) CHECK(f.engine->vfs().exists(s.texture));
    });

    // Survives a save / load round trip (RLE tiles, 2D components).
    const std::string text = SceneSerializer::toString(scene);
    Scene copy;
    REQUIRE(SceneSerializer::fromString(copy, text));
    const auto& tm2 = copy.registry().get<TilemapComponent>(copy.findByName("Ground"));
    CHECK(tm2.tiles == tm.tiles);
    CHECK(copy.registry().get<SpriteAnimatorComponent>(copy.findByName("Hero")).clip == "idle");
    CHECK(copy.registry().get<ScriptComponent>(copy.findByName("Hero")).script == "assets://scripts/platformer_hero.lua");
    CHECK(copy.registry().has<TilemapCollider2DComponent>(copy.findByName("Ground")));

    // Phase 7 assets: audio files, animation files, font, HUD.
    reg.view<AudioSourceComponent>().each([&](ecs::Entity, AudioSourceComponent& a) { CHECK(f.engine->vfs().exists(a.clip)); });
    VirtualFileSystem& vfs = f.engine->vfs();
    for (const char* uri : {"assets://audio/jump.wav", "assets://audio/coin.wav", "assets://fonts/Inter-Regular.ttf"}) CHECK_MESSAGE(vfs.exists(uri), uri);
    AnimationLibrary anims(vfs);
    auto ctrl = anims.controller(reg.get<AnimatorComponent>(hero).controller);
    REQUIRE(ctrl);
    CHECK(ctrl.value()->validate().empty());
    for (const AnimatorState& st : ctrl.value()->states) {
        CHECK(anims.clip(st.clip));
        CHECK(heroSheet->findClip(st.spriteClip) != nullptr);
    }
    CHECK(anims.clip(reg.get<AnimatorComponent>(scene.findByName("Mushroom 1")).clip));
    CHECK(reg.get<UITextComponent>(scene.findByName("Score")).text == "Champignons : 0 / 3");
    for (const char* b : {"Left", "Right", "Jump"}) CHECK(reg.has<UIButtonComponent>(scene.findByName(b)));
    CHECK(copy.registry().get<UIRectComponent>(copy.findByName("Jump")).anchorMin.x == doctest::Approx(1.0f));
}

TEST_CASE("picking: tilemaps on painted cells, sprites with sheet frame pivots") {
    EditorFixture f(ProjectMode::Mode2D);
    f.ctx->newScene(SceneTemplate::Empty2D);
    Scene& scene = f.scene();
    auto& reg = scene.registry();
    const ecs::Entity map = scene.createEntity("Map");
    reg.get<TransformComponent>(map).position = {-10.0f, -10.0f, -1.0f};
    auto& tm = reg.emplace<TilemapComponent>(map);
    tm.tileset = "assets://2d/forest_tiles.agrsheet";
    tilemapEnsureLayout(tm);
    tilemapSet(tm, 2, 3, 1);
    scene.updateTransforms();
    auto rayAt = [](float x, float y) { return math::Ray{{x, y, 50.0f}, {0.0f, 0.0f, -1.0f}}; };
    auto hit = pick(scene, rayAt(-7.5f, -6.5f));  // cell (2, 3)
    REQUIRE(hit);
    CHECK(hit->entity == map);
    CHECK(hit->point.z == doctest::Approx(-1.0f));
    CHECK_FALSE(pick(scene, rayAt(-6.5f, -6.5f)));  // empty cell: nothing behind

    // Hero frames have their pivot at the feet: the quad is above the entity.
    const ecs::Entity hero = scene.createEntity("Hero");
    reg.get<TransformComponent>(hero).position = {20.0f, 0.0f, 0.0f};
    auto& sr = reg.emplace<SpriteRendererComponent>(hero);
    sr.sheet = "assets://2d/hero.agrsheet";
    sr.frame = 0;
    scene.updateTransforms();
    SpriteSheetLibrary sheets(f.engine->vfs());
    CHECK(pick(scene, rayAt(20.0f, 0.75f), {&sheets}));
    CHECK_FALSE(pick(scene, rayAt(20.0f, -0.25f), {&sheets}));
    CHECK(pick(scene, rayAt(20.0f, -0.25f)));  // without sheets the component pivot (center) is used
    CHECK(worldBounds(scene, map).max.x == doctest::Approx(6.0f));
}

TEST_CASE("dropping a sprite sheet creates an animated sprite at native size") {
    EditorFixture f(ProjectMode::Mode2D);
    SpriteSheetLibrary sheets(f.engine->vfs());
    const SpriteSheet* sheet = sheets.get("assets://2d/hero.agrsheet");
    REQUIRE(sheet != nullptr);
    const auto e = f.ctx->createSprite("assets://2d/hero.agrsheet", {1.0f, 2.0f, 0.0f}, {}, sheet);
    auto& reg = f.scene().registry();
    const auto& sr = reg.get<SpriteRendererComponent>(e);
    CHECK(sr.sheet == "assets://2d/hero.agrsheet");
    CHECK(sr.texture.empty());
    CHECK(sr.frame == 0);
    CHECK(sr.pixelArt);
    CHECK(sr.size.x == doctest::Approx(1.0f));  // 32 px at 32 px per unit
    REQUIRE(reg.has<SpriteAnimatorComponent>(e));
    CHECK(reg.get<SpriteAnimatorComponent>(e).clip == "idle");
    CHECK(f.ctx->undoStack().undoLabel() == "Create Sprite");
    for (CreateKind k : {CreateKind::Tilemap, CreateKind::Particles2D, CreateKind::Light2D, CreateKind::ParallaxLayer}) {
        const auto c = f.ctx->create(k);
        CHECK(f.scene().nameOf(c) == createKindName(k));
    }
    const auto tmE = f.scene().findByName("Tilemap");
    const auto& tm = reg.get<TilemapComponent>(tmE);
    CHECK(tm.tiles.size() == static_cast<usize>(tm.width) * tm.height);
    CHECK(reg.get<TransformComponent>(tmE).position.x == std::floor(reg.get<TransformComponent>(tmE).position.x));
    // 2D skeleton: root, a chain of 3 bones with their rest pose, a skin over them; one undo step.
    const auto skel = f.ctx->create(CreateKind::Skeleton2D);
    CHECK(reg.has<Skeleton2DComponent>(skel));
    usize bones = 0;
    reg.view<Bone2DComponent>().each([&](ecs::Entity, Bone2DComponent& b) {
        ++bones;
        CHECK(b.restSet);
    });
    CHECK(bones == 3);
    const auto skin = f.scene().findByName("Skin");
    REQUIRE_FALSE(skin.isNull());
    CHECK(f.scene().parent(skin) == skel);
    CHECK(reg.has<Skin2DComponent>(skin));
    f.ctx->undo();
    CHECK(f.scene().findByName("Bone 1").isNull());
}

TEST_CASE("the 3D showcase references shipped models and materials; models instantiate as one undo step") {
    EditorFixture f(ProjectMode::Mode3D);
    f.ctx->newScene(SceneTemplate::Showcase3D);
    auto& reg = f.scene().registry();
    ModelLibrary models(f.engine->vfs(), f.engine->jobs(), nullptr);
    MaterialLibrary materials(f.engine->vfs());
    usize meshRenderers = 0;
    reg.view<MeshRendererComponent>().each([&](ecs::Entity, MeshRendererComponent& m) {
        ++meshRenderers;
        if (!m.model.empty()) {
            const ModelAsset* a = models.load(m.model);
            REQUIRE(a);
            CHECK_MESSAGE(a->state == AssetState::Ready, m.model << ": " << a->error);
            CHECK(static_cast<usize>(m.meshIndex) < a->meshes.size());
        }
        if (!m.material.empty()) CHECK_MESSAGE(materials.get(m.material) != nullptr, m.material);
        if (!m.texture.empty()) CHECK(f.engine->vfs().exists(m.texture));
    });
    CHECK(meshRenderers > 15);
    usize postEffects = 0;
    reg.view<CameraComponent, PostProcessComponent>().each([&](ecs::Entity, CameraComponent&, PostProcessComponent& pp) {
        ++postEffects;
        CHECK(pp.bloom);
    });
    CHECK(postEffects == 1);
    usize imageSkies = 0;
    reg.view<EnvironmentComponent>().each([&](ecs::Entity, EnvironmentComponent& env) {
        if (env.sky != SkyMode::Image) return;
        ++imageSkies;
        auto bytes = f.engine->vfs().readBytes(env.skyTexture);
        REQUIRE(bytes);
        auto img = decodeImageLinear(bytes.value(), env.skyTexture);
        REQUIRE(img);
        CHECK(img.value().width == 2 * img.value().height);
        auto map = buildEnvironmentMap(img.value(), 1024);
        REQUIRE(map);
        // The sun (above 1: an HDR image) and a brighter sky above than ground below.
        CHECK(*std::max_element(img.value().rgb.begin(), img.value().rgb.end()) > 10.0f);
        CHECK(evaluateIrradiance(map.value().sh, {0, 1, 0}).z > evaluateIrradiance(map.value().sh, {0, -1, 0}).z);
    });
    CHECK(imageSkies == 1);
    usize pointLights = 0;
    reg.view<PointLightComponent>().each([&](ecs::Entity, PointLightComponent&) { ++pointLights; });
    CHECK(pointLights >= 1);
    const ModelAsset* crate = models.load("assets://3d/crate.glb");
    REQUIRE(crate);
    REQUIRE(crate->state == AssetState::Ready);
    CHECK(crate->materials.size() == 1);
    CHECK(crate->materials[0].normalTexture == "assets://3d/crate.glb#image1");
    const usize before = f.scene().entityCount();
    const ecs::Entity root = f.ctx->instantiateModel(*crate, {1.0f, 0.0f, 2.0f});
    REQUIRE_FALSE(root.isNull());
    CHECK(f.scene().entityCount() == before + 2);  // root + the crate node
    CHECK(f.ctx->selection().primary() == f.scene().uuidOf(root));
    CHECK(reg.get<TransformComponent>(root).position.z == doctest::Approx(2.0f));
    CHECK(f.ctx->undoStack().undoLabel() == "Add Model");
    REQUIRE(f.ctx->undo());
    CHECK(f.scene().entityCount() == before);
    // Model bounds drive picking once loaded (unit box before).
    REQUIRE(f.ctx->redo());
    const ecs::Entity node = f.scene().children(f.scene().findByUuid(f.ctx->selection().primary())).front();
    const math::Aabb b = worldBounds(f.scene(), node, {nullptr, &models});
    CHECK(b.min.y == doctest::Approx(0.0f).epsilon(0.01));  // the crate sits on its origin
    CHECK(b.max.y == doctest::Approx(1.0f).epsilon(0.01));
}

TEST_CASE("template gameplay runs headless: platformer hero, collectibles, 3D ball launcher") {
    EditorFixture f(ProjectMode::Mode2D);
    for (const char* uri : {"assets://scripts/platformer_hero.lua", "assets://scripts/camera_follow.lua", "assets://scripts/collectible.lua",
                            "assets://scripts/ball_launcher.lua"}) {
        ScriptEngine probe(f.engine->vfs());
        const ScriptInfo info = probe.describe(uri);
        CHECK_MESSAGE(info.valid, uri << ": " << info.error);
    }
    f.ctx->newScene(SceneTemplate::Showcase2D);
    Scene& scene = f.scene();
    Input input;
    ScriptEngine scripts(f.engine->vfs());
    Physics2DWorld physics;
    AudioSystem audio(f.engine->vfs());
    REQUIRE(audio.initialize({.output = AudioOutput::Manual}));
    AnimationSystem anim(f.engine->vfs());
    UISystem ui(f.engine->vfs());
    InputActions actions;  // as the engine: the project's actions, evaluated every frame
    REQUIRE(actions.load(f.engine->vfs()));
    scripts.setInput(&input);
    scripts.setActions(&actions);
    scripts.setLocalization(&f.engine->localization());
    ui.setLocalization(&f.engine->localization());
    scripts.setPhysics2D(&physics);
    scripts.setAudio(&audio);
    scripts.setAnimation(&anim);
    scripts.setUI(&ui);
    FrameTime time;
    time.deltaSeconds = 1.0 / 60.0;
    time.fixedDeltaSeconds = 1.0 / 60.0;
    std::vector<UIPointer> pointers;
    std::vector<float> mixBuffer(800 * 2);
    auto frame = [&] {
        actions.update(input, {1920.0f, 1080.0f});
        physics.step(scene, 1.0f / 60.0f);
        for (const auto& ev : physics.events()) scripts.dispatchCollision(scene, ev);
        ui.layout(scene, 1920, 1080);
        ui.processInput(scene, pointers);
        scripts.update(scene, time);
        anim.update(scene, 1.0f / 60.0f);
        for (const AnimationEvent& ev : anim.takeEvents()) scripts.dispatchAnimation(scene, ev);
        audio.updateScene(scene, true);
        audio.mix(mixBuffer.data(), 800);
        audio.update(0.0);
        scene.flushDestroyed();
        scene.updateTransforms();
        input.beginFrame();
    };
    const ecs::Entity hero = scene.findByName("Hero");
    for (int i = 0; i < 60; ++i) frame();  // lands on the ground
    const Vec3 start = scene.registry().get<TransformComponent>(hero).position;
    CHECK(start.y == doctest::Approx(-1.0f).epsilon(0.02));
    CHECK(scripts.field(hero, "grounded").value() == "true");
    input.setKey(Key::Left, true);  // walk to the first mushroom (x = -14)
    for (int i = 0; i < 150; ++i) frame();
    input.setKey(Key::Left, false);
    const Vec3 walked = scene.registry().get<TransformComponent>(hero).position;
    CHECK(walked.x < start.x - 8.0f);
    CHECK(scene.registry().get<SpriteRendererComponent>(hero).flipX);
    CHECK(scene.findByName("Mushroom 1").isNull());  // collected (trigger + script call)
    CHECK(scripts.field(hero, "collected").value() == "1");
    CHECK(scene.registry().get<UITextComponent>(scene.findByName("Score")).text == "Champignons : 1 / 3");  // HUD
    CHECK(audio.sourcePlaying(scene.findByName("Music")));
    CHECK(audio.stats().clips >= 2);  // music + coin (and jump later)
    frame();
    frame();
    CHECK(anim.currentState(hero) == "Idle");  // stopped walking: back to Idle via the state machine
    CHECK(scene.registry().get<SpriteAnimatorComponent>(hero).clip == "idle");
    input.setKey(Key::Space, true);
    frame();
    input.setKey(Key::Space, false);
    float top = walked.y;
    for (int i = 0; i < 30; ++i) {
        frame();
        top = std::max(top, scene.registry().get<TransformComponent>(hero).position.y);
    }
    CHECK(top > walked.y + 3.0f);  // jumps more than 3 m
    // Touch controls: holding the on-screen "Right" button walks right.
    const math::Vec4 right = scene.registry().get<UIRectComponent>(scene.findByName("Right")).screenRect;
    REQUIRE(right.z > 0.0f);
    for (int i = 0; i < 40; ++i) frame();  // lands
    const float beforeX = scene.registry().get<TransformComponent>(hero).position.x;
    pointers = {{7, {right.x + right.z * 0.5f, right.y + right.w * 0.5f}, true}};
    for (int i = 0; i < 20; ++i) frame();
    CHECK(anim.currentState(hero) == "Run");
    CHECK(scene.registry().get<SpriteAnimatorComponent>(hero).clip == "run");
    pointers = {{7, {right.x + right.z * 0.5f, right.y + right.w * 0.5f}, false}};
    frame();
    pointers.clear();
    CHECK(scene.registry().get<TransformComponent>(hero).position.x > beforeX + 1.0f);
    CHECK_FALSE(scene.registry().get<SpriteRendererComponent>(hero).flipX);
    CHECK(scripts.stats().errors == 0);
    CHECK(scripts.stats().faulted == 0);

    // 3D: Space throws a ball that the physics moves.
    f.ctx->newScene(SceneTemplate::Showcase3D);
    Scene& scene3 = f.scene();
    Physics3DWorld physics3;
    scripts.setPhysics3D(&physics3);
    ui.layout(scene3, 1920, 1080);
    ui.processInput(scene3, {});  // no pointer over the UI: Space fires
    const usize before = scene3.entityCount();
    actions.update(input, {1920.0f, 1080.0f});
    scripts.update(scene3, time);
    input.setKey(Key::Space, true);
    actions.update(input, {1920.0f, 1080.0f});  // "jump" pressed
    scripts.update(scene3, time);
    input.beginFrame();
    input.setKey(Key::Space, false);
    scene3.updateTransforms();
    CHECK(scene3.entityCount() == before + 1);
    const ecs::Entity ball = scene3.findByName("Ball 1");
    REQUIRE_FALSE(ball.isNull());
    const Vec3 b0 = scene3.registry().get<TransformComponent>(ball).position;
    for (int i = 0; i < 30; ++i) {
        actions.update(input, {1920.0f, 1080.0f});  // every frame, as the engine
        physics3.step(scene3, 1.0f / 60.0f);
        scripts.update(scene3, time);
        scene3.updateTransforms();
    }
    const Vec3 b1 = scene3.registry().get<TransformComponent>(ball).position;
    CHECK(b1.z < b0.z - 3.0f);  // flew toward the scene (camera looks down -Z)
    CHECK(physics3.stats().bodies > 15);
    CHECK_MESSAGE(scene3.registry().get<UITextComponent>(scene3.findByName("Counter")).text.find("Balles : 1 / 25") == 0, scene3.registry().get<UITextComponent>(scene3.findByName("Counter")).text);
    // The speed slider calls the launcher script (target message).
    scripts.dispatchUI(scene3, UIValueChangedEvent{scene3.findByName("Power"), 20.0f});
    CHECK(scripts.field(scene3.findByName("Main Camera"), "speed").value() == "20.0");

    // The skinned sprout: its state machine plays the model's Idle clip; a ball hitting it makes it dance.
    const ecs::Entity sprout = scene3.findByName("Sprout");
    REQUIRE_FALSE(sprout.isNull());
    for (int i = 0; i < 5; ++i) {
        scripts.update(scene3, time);
        anim.update(scene3, 1.0f / 60.0f);
        for (const AnimationEvent& ev : anim.takeEvents()) scripts.dispatchAnimation(scene3, ev);
    }
    CHECK(anim.currentState(sprout) == "Idle");
    const ecs::Entity stem = scene3.findByName("Stem1");
    REQUIRE_FALSE(stem.isNull());
    CHECK_FALSE(math::nearlyEqual(scene3.registry().get<TransformComponent>(stem).rotation, math::Quat::identity()));
    scripts.dispatchCollision(scene3, Collision3DEvent{sprout, ball, true, false});
    scripts.update(scene3, time);
    anim.update(scene3, 1.0f / 60.0f);
    CHECK(anim.currentState(sprout) == "Dance");
    CHECK(scripts.stats().errors == 0);
}

TEST_CASE("create point and spot lights") {
    EditorFixture f;
    const auto p = f.ctx->create(CreateKind::PointLight);
    const auto sp = f.ctx->create(CreateKind::SpotLight);
    CHECK(f.scene().registry().has<PointLightComponent>(p));
    REQUIRE(f.scene().registry().has<SpotLightComponent>(sp));
    // The spot points down by default.
    const Vec3 dir = f.scene().worldMatrix(sp).transformVector({0, 0, -1});
    CHECK(dir.y == doctest::Approx(-1.0f).epsilon(0.001));
}

TEST_CASE("create, rename, undo and redo") {
    EditorFixture f;
    const usize base = f.scene().entityCount();
    const auto cube = f.ctx->create(CreateKind::Cube);
    const Uuid id = f.scene().uuidOf(cube);
    CHECK(f.scene().entityCount() == base + 1);
    CHECK(f.ctx->selection().primary() == id);
    CHECK(f.ctx->dirty());
    f.ctx->rename(cube, "Crate");
    CHECK(f.ctx->undoStack().undoLabel() == "Rename");

    REQUIRE(f.ctx->undo());  // rename undone
    CHECK(f.scene().nameOf(f.scene().findByUuid(id)) == "Cube");
    REQUIRE(f.ctx->undo());  // creation undone
    CHECK(f.scene().findByUuid(id).isNull());
    CHECK(f.scene().entityCount() == base);
    CHECK_FALSE(f.ctx->dirty());  // back to the saved state
    f.ctx->update();
    CHECK(f.ctx->selection().empty());

    REQUIRE(f.ctx->redo());
    REQUIRE(f.ctx->redo());
    CHECK(f.scene().nameOf(f.scene().findByUuid(id)) == "Crate");
    CHECK(f.ctx->selection().primary() == id);  // selection restored with the state
    CHECK_FALSE(f.ctx->redo());
}

TEST_CASE("delete and duplicate keep hierarchies consistent") {
    EditorFixture f;
    const auto parent = f.ctx->create(CreateKind::Empty);
    const auto child = f.ctx->create(CreateKind::Sphere, parent);
    const Uuid parentId = f.scene().uuidOf(parent), childId = f.scene().uuidOf(child);
    f.ctx->selection().assign({parentId, childId});  // both selected: duplicate only once
    const usize before = f.scene().entityCount();
    f.ctx->duplicateSelection();
    CHECK(f.scene().entityCount() == before + 2);
    REQUIRE(f.ctx->selection().size() == 1);
    const auto copy = f.scene().findByUuid(f.ctx->selection().primary());
    CHECK(f.scene().children(copy).size() == 1);

    f.ctx->selection().assign({parentId, childId});
    f.ctx->deleteSelection();
    CHECK(f.scene().findByUuid(parentId).isNull());
    CHECK(f.scene().findByUuid(childId).isNull());
    CHECK(f.scene().entityCount() == before);
    REQUIRE(f.ctx->undo());
    CHECK_FALSE(f.scene().findByUuid(childId).isNull());
    CHECK(f.scene().parent(f.scene().findByUuid(childId)) == f.scene().findByUuid(parentId));
}

TEST_CASE("grouping keeps world positions and creates a folder entity") {
    EditorFixture f;
    Scene& s = f.scene();
    const auto a = f.ctx->create(CreateKind::Cube);
    const auto b = f.ctx->create(CreateKind::Sphere);
    s.registry().get<TransformComponent>(a).position = {2, 0, 0};
    s.registry().get<TransformComponent>(b).position = {4, 2, 0};
    s.updateTransforms();
    const Uuid idA = s.uuidOf(a);
    f.ctx->selection().assign({idA, s.uuidOf(b)});
    const auto group = f.ctx->groupSelection();
    REQUIRE_FALSE(group.isNull());
    CHECK(s.nameOf(group) == "Group");
    CHECK(s.parent(a) == group);
    CHECK(s.parent(b) == group);
    CHECK(math::nearlyEqual(s.worldMatrix(group).translation(), Vec3{3, 1, 0}));
    CHECK(math::nearlyEqual(s.worldMatrix(a).translation(), Vec3{2, 0, 0}));  // unchanged in the world
    CHECK(math::nearlyEqual(s.worldMatrix(b).translation(), Vec3{4, 2, 0}));
    CHECK(f.ctx->selection().primary() == s.uuidOf(group));
    REQUIRE(f.ctx->undo());  // entities are recreated: look them up by id
    const auto restored = s.findByUuid(idA);
    REQUIRE_FALSE(restored.isNull());
    CHECK(s.parent(restored).isNull());
    CHECK(s.findByName("Group").isNull());
}

TEST_CASE("components are added and removed through reflection") {
    EditorFixture f;
    const auto e = f.ctx->create(CreateKind::Empty);
    CHECK(f.ctx->addComponent(e, "SpriteRenderer"));
    CHECK_FALSE(f.ctx->addComponent(e, "SpriteRenderer"));  // already present
    CHECK_FALSE(f.ctx->addComponent(e, "NoSuchComponent"));
    CHECK(f.scene().registry().has<SpriteRendererComponent>(e));
    CHECK(f.ctx->removeComponent(e, "SpriteRenderer"));
    CHECK_FALSE(f.scene().registry().has<SpriteRendererComponent>(e));
}

TEST_CASE("play mode runs the simulation and stop restores the edited scene") {
    EditorFixture f;
    const auto cube = f.scene().findByName("Cube");
    REQUIRE_FALSE(cube.isNull());
    const Uuid id = f.scene().uuidOf(cube);
    CHECK(f.engine->time().timeScale == 0.0);  // frozen while editing
    f.ctx->play();
    CHECK(f.ctx->playState() == PlayState::Playing);
    CHECK(f.engine->time().timeScale == 1.0);
    // Gameplay modifies the scene during play...
    f.scene().registry().get<TransformComponent>(cube).position = {5, 5, 5};
    f.scene().destroyEntity(f.scene().findByName("Ground"));
    f.ctx->commit("ignored during play");
    f.ctx->pause();
    CHECK(f.ctx->playState() == PlayState::Paused);
    CHECK(f.engine->time().timeScale == 0.0);
    f.ctx->stop();
    // ...and everything is back after stop.
    CHECK(f.ctx->playState() == PlayState::Edit);
    CHECK(f.scene().registry().get<TransformComponent>(f.scene().findByUuid(id)).position == Vec3{0.0f, 0.5f, 0.0f});
    CHECK_FALSE(f.scene().findByName("Ground").isNull());
    CHECK_FALSE(f.ctx->dirty());
}

TEST_CASE("scenes save, load and save-as") {
    EditorFixture f;
    f.ctx->create(CreateKind::Sphere);
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    CHECK_FALSE(f.ctx->dirty());
    REQUIRE(f.ctx->saveSceneAs("scenes/level2"));
    CHECK(f.ctx->scenePath() == "scenes/level2.agrscene");
    f.ctx->newScene(SceneTemplate::Empty2D);
    CHECK(f.ctx->scenePath().empty());
    CHECK_FALSE(f.ctx->saveScene());  // needs Save As
    REQUIRE(f.ctx->loadScene("scenes/level2.agrscene"));
    CHECK_FALSE(f.scene().findByName("Sphere").isNull());
    CHECK(f.ctx->project().listScenes().size() == 2);
}

TEST_CASE("picking hits the nearest visible renderable") {
    EditorFixture f;
    Scene& s = f.scene();
    s.destroyEntity(s.findByName("Cube"));  // the 3D template's cube sits on the test ray
    const auto front = f.ctx->create(CreateKind::Cube);
    const auto back = f.ctx->create(CreateKind::Cube);
    s.registry().get<TransformComponent>(front).position = {0, 0, 2};
    s.registry().get<TransformComponent>(back).position = {0, 0, -2};
    const auto sprite = f.ctx->create(CreateKind::Sprite);
    s.registry().get<TransformComponent>(sprite).position = {10, 0, 0};
    s.updateTransforms();
    const math::Ray ray{{0, 0.3f, 10}, {0, 0, -1}};  // above the ground plane (y = 0)
    auto hit = pick(s, ray);
    REQUIRE(hit);
    CHECK(hit->entity == front);
    CHECK(hit->distance == doctest::Approx(7.5f));
    s.registry().get<StateComponent>(front).lockedInEditor = true;  // locked: not pickable
    hit = pick(s, ray);
    REQUIRE(hit);
    CHECK(hit->entity == back);
    auto spriteHit = pick(s, math::Ray{{10.2f, 0.3f, 5}, {0, 0, -1}});
    REQUIRE(spriteHit);
    CHECK(spriteHit->entity == sprite);
    CHECK_FALSE(pick(s, math::Ray{{10.6f, 0, 5}, {0, 0, -1}}));  // just outside the 1x1 sprite
    CHECK(intersectPlane(math::Ray{{0, 5, 0}, {0, -1, 0}}, {0, 1, 0}, 0.0f) == doctest::Approx(5.0f));
}

TEST_CASE("editor camera rays match its view") {
    EditorCamera cam;
    cam.target = {1, 2, 3};
    cam.distance = 10;
    const math::Ray center = cam.ray({0.5f, 0.5f}, 16.0f / 9.0f);
    CHECK(math::nearlyEqual(center.direction, cam.forward()));
    CHECK(math::nearlyEqual(center.at(10.0f), cam.target, 1e-3f));
    const auto view = cam.renderView();
    CHECK(math::nearlyEqual(view.view.transformPoint(cam.target), Vec3{0, 0, -10}, 1e-3f));
    cam.orbit(0, math::radians(-200.0f));
    CHECK(cam.pitch >= math::radians(-89.5f));  // clamped
    const Vec3 before = cam.position();
    cam.pan({100, 0}, 500);
    CHECK(math::dot(cam.position() - before, cam.right()) < 0.0f);  // dragging right moves the view left
    cam.mode2D = true;
    cam.position2D = {3, 4};
    const math::Ray r2 = cam.ray({0.5f, 0.5f}, 1.0f);
    CHECK(math::nearlyEqual(r2.origin, Vec3{3, 4, 50}));
    cam.focus(math::Aabb::fromCenterExtents({7, 8, 0}, Vec3{2}));
    CHECK(cam.position2D == math::Vec2{7, 8});
}

TEST_CASE("recent projects list") {
    RecentProjects r;
    r.touch("/a");
    r.touch("/b");
    r.touch("/a");
    CHECK(r.paths == std::vector<std::string>{"/a", "/b"});
    CHECK(RecentProjects::fromJson(r.toJson()).paths == r.paths);
    CHECK(RecentProjects::fromJson("garbage").paths.empty());
}

}  // TEST_SUITE

TEST_CASE("create UI elements: a canvas is created or reused, buttons get a label") {
    EditorFixture f;
    auto& reg = f.scene().registry();
    const auto text = f.ctx->create(CreateKind::UIText);
    const ecs::Entity canvas = f.scene().parent(text);
    REQUIRE_FALSE(canvas.isNull());
    CHECK(reg.has<UICanvasComponent>(canvas));
    CHECK(reg.has<UIRectComponent>(text));
    const auto button = f.ctx->create(CreateKind::UIButton);
    CHECK(f.scene().parent(button) == canvas);  // reused
    CHECK(reg.has<UIButtonComponent>(button));
    REQUIRE(f.scene().children(button).size() == 1);
    CHECK(reg.get<UITextComponent>(f.scene().children(button)[0]).text == "Button");
    const auto inner = f.ctx->create(CreateKind::UISlider, button);  // under a UI parent: kept
    CHECK(f.scene().parent(inner) == button);
    const auto outside = f.ctx->create(CreateKind::UIPanel, f.ctx->create(CreateKind::Cube));
    CHECK(f.scene().parent(outside) == canvas);  // a 3D object is not a UI parent
    CHECK(reg.has<UILayoutComponent>(f.ctx->create(CreateKind::UIColumn)));
    CHECK(reg.has<AudioSourceComponent>(f.ctx->create(CreateKind::AudioSource)));
    CHECK(f.ctx->undo());  // every creation is undoable
}

TEST_CASE("game export: data package, desktop folder safety, Android Gradle project") {
    EditorFixture f(ProjectMode::Mode2D);
    f.ctx->newScene(SceneTemplate::Showcase2D);
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    Project& project = f.ctx->project();
    project.settings().exportSettings.organization = "Studio Ünïcode";
    CHECK(defaultPackageId(project.settings()) == "com.studioncode.testgame");
    CHECK(GameManifest::validPackageId("com.studio.game"));
    CHECK_FALSE(GameManifest::validPackageId("game"));
    CHECK_FALSE(GameManifest::validPackageId("com.1studio.game"));

    // Game data.
    const fs::path out = f.root / "export";
    auto data = packageGameData(project, out / "game");
    REQUIRE(data);
    CHECK(data.value().files > 20);
    CHECK(fs::exists(out / "game" / "assets" / "audio" / "music.wav"));
    auto manifest = GameManifest::fromJson(readTextFile(out / "game" / "game.json"));
    REQUIRE(manifest);
    CHECK(manifest.value().startupScene == project.settings().startupScene);
    CHECK(fs::exists(out / "game" / manifest.value().startupScene));
    // Scenes ship in the binary format (verified lossless) and load to the same scene.
    CHECK(data.value().binaryScenes >= 1);
    {
        const std::string exported = readTextFile(out / "game" / manifest.value().startupScene);
        const std::string original = readTextFile(project.root() / project.settings().startupScene);
        CHECK(SceneSerializer::isBinary(exported));
        Scene fromBinary, fromText;
        REQUIRE(SceneSerializer::fromBytes(fromBinary, exported));
        REQUIRE(SceneSerializer::fromBytes(fromText, original));
        CHECK(SceneSerializer::toString(fromBinary) == SceneSerializer::toString(fromText));
        CHECK(exported.size() < original.size());
    }
    // A folder that is not an export output is never emptied.
    fs::create_directories(out / "mine");
    writeTextFile(out / "mine" / "notes.txt", "keep me");
    CHECK_FALSE(prepareExportFolder(out / "mine"));
    CHECK(fs::exists(out / "mine" / "notes.txt"));
    CHECK(prepareExportFolder(out / "game"));  // a previous export: replaced
    CHECK_FALSE(fs::exists(out / "game" / "game.json"));

    // Android project with stand-in templates (no SDK needed to generate it).
    const fs::path templates = f.root / "templates";
    for (const char* lib : {"libmain.so", "libSDL3.so"}) writeTextFile(templates / "android" / "arm64-v8a" / lib, "elf");
    writeTextFile(templates / "android" / "java" / "org" / "libsdl" / "app" / "SDLActivity.java", "package org.libsdl.app;");
    AndroidToolchain tools;
    tools.templates = templates;
    tools.gradle = "/usr/bin/true";
    tools.sdk = f.root / "sdk";
    CHECK_FALSE(checkAndroidExport(project, tools).empty());  // no SDK platform
    writeTextFile(tools.sdk / "platforms" / "android-35" / "android.jar", "jar");
    CHECK(checkAndroidExport(project, tools).empty());
    project.settings().exportSettings.orientation = ScreenOrientation::Portrait;
    project.settings().exportSettings.versionCode = 7;
    auto gradle = generateAndroidProject(project, tools, out / "android");
    REQUIRE(gradle);
    const fs::path main = out / "android" / "app" / "src" / "main";
    const std::string manifestXml = readTextFile(main / "AndroidManifest.xml");
    CHECK(manifestXml.find("sensorPortrait") != std::string::npos);
    CHECK(manifestXml.find("android.permission.VIBRATE") != std::string::npos);
    CHECK(manifestXml.find("android.permission.INTERNET") == std::string::npos);  // only for network games
    project.settings().exportSettings.network = true;
    REQUIRE(generateAndroidProject(project, tools, out / "android_net"));
    CHECK(readTextFile(out / "android_net" / "app" / "src" / "main" / "AndroidManifest.xml").find("android.permission.INTERNET") != std::string::npos);
    CHECK(manifestXml.find("RECORD_AUDIO") == std::string::npos);
    project.settings().exportSettings.microphone = true;
    project.settings().exportSettings.iconBackground = {1.0f, 0.5f, 0.0f, 1.0f};
    REQUIRE(generateAndroidProject(project, tools, out / "android_mic"));
    const fs::path micMain = out / "android_mic" / "app" / "src" / "main";
    CHECK(readTextFile(micMain / "AndroidManifest.xml").find("android.permission.RECORD_AUDIO") != std::string::npos);
    CHECK(readTextFile(micMain / "res" / "mipmap-anydpi-v26" / "ic_launcher.xml").find("ic_launcher_foreground") != std::string::npos);
    CHECK(readTextFile(micMain / "res" / "values" / "colors.xml").find("#FF8000") != std::string::npos);
    CHECK(fs::exists(micMain / "res" / "mipmap-xxxhdpi" / "ic_launcher_foreground.png"));
    const std::string appGradle = readTextFile(out / "android" / "app" / "build.gradle");
    CHECK(appGradle.find("applicationId 'com.studioncode.testgame'") != std::string::npos);
    CHECK(appGradle.find("versionCode 7") != std::string::npos);
    CHECK(appGradle.find("signingConfigs") == std::string::npos);  // no keystore: debug signing only
    CHECK(fs::exists(main / "assets" / "game" / "game.json"));
    CHECK(fs::exists(main / "jniLibs" / "arm64-v8a" / "libmain.so"));
    CHECK(fs::exists(main / "java" / "com" / "studioncode" / "testgame" / "AgrActivity.java"));
    CHECK(fs::exists(main / "res" / "mipmap-xxxhdpi" / "ic_launcher.png"));
    project.settings().exportSettings.abis = {"arm64-v8a", "x86_64"};
    const auto problems = checkAndroidExport(project, tools);
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].find("x86_64") != std::string::npos);

    // Icons: square, transparent margins.
    const std::string mark = readTextFile(fs::path(AGR_TEST_TEMPLATE_ASSETS) / "textures" / "agr_mark.png");
    auto icon = makeLauncherIcon(std::span(reinterpret_cast<const u8*>(mark.data()), mark.size()), 96);
    REQUIRE(icon);
    auto decoded = decodeImage(icon.value());
    REQUIRE(decoded);
    CHECK(decoded.value().width == 96);
    CHECK(decoded.value().pixels[3] == 0);  // corner pixel transparent
}

// Real Android builds: needs the Android SDK (ANDROID_HOME), Gradle, a JDK, the export templates
// (AGR_EXPORT_TEMPLATES) and network access for Gradle. Enabled with AGR_ANDROID_EXPORT_TESTS=1.
TEST_CASE("Android export: signed release APK and App Bundle" * doctest::skip(std::getenv("AGR_ANDROID_EXPORT_TESTS") == nullptr)) {
    EditorFixture f(ProjectMode::Mode2D);
    f.ctx->newScene(SceneTemplate::Showcase2D);
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    Project& project = f.ctx->project();
    const AndroidToolchain tools = findAndroidToolchain(".");
    const auto problems = checkAndroidExport(project, tools);
    REQUIRE_MESSAGE(problems.empty(), problems.front());
    REQUIRE(createKeystore(tools, f.root / "keys" / "release.p12", "game", "s3cret-pass", "AGR Test"));
    CHECK_FALSE(createKeystore(tools, f.root / "keys" / "release.p12", "game", "s3cret-pass", "AGR Test"));  // never overwritten
    project.settings().exportSettings.keystorePath = (f.root / "keys" / "release.p12").string();
    project.settings().exportSettings.keyAlias = "game";
    AndroidBuildOptions o;
    o.kind = AndroidBuildKind::ReleaseBundle;
    CHECK_FALSE(buildAndroid(project, tools, f.root / "android", o));  // password required
    o.storePassword = "s3cret-pass";
    auto aab = buildAndroid(project, tools, f.root / "android", o);
    REQUIRE_MESSAGE(aab, (aab ? "" : aab.error().message));
    CHECK(aab.value().output.extension() == ".aab");
    CHECK(fs::file_size(aab.value().output) > 5u * 1024 * 1024);
    o.kind = AndroidBuildKind::ReleaseApk;
    auto apk = buildAndroid(project, tools, f.root / "android", o);
    REQUIRE(apk);
    const fs::path apksigner = tools.sdk / "build-tools" / "35.0.1" / "apksigner";
    if (fs::exists(apksigner)) {
        const std::string cmd = apksigner.string() + " verify --print-certs '" + apk.value().output.string() + "' 2>&1";
        FILE* p = popen(cmd.c_str(), "r");
        std::string outText;
        char buf[512];
        while (p && std::fgets(buf, sizeof buf, p)) outText += buf;
        const int code = p ? pclose(p) : -1;
        CHECK(code == 0);
        CHECK(outText.find("CN=AGR Test") != std::string::npos);  // our key, not the debug key
    }
}

TEST_CASE("editor prefab workflow: create, place, apply to every instance, undo, save and reopen") {
    EditorFixture f(ProjectMode::Mode3D);
    f.ctx->newScene(SceneTemplate::Empty3D);
    Scene& s = f.scene();
    const auto tower = s.createEntity("Tower");
    s.registry().emplace<MeshRendererComponent>(tower).mesh = PrimitiveMesh::Cube;
    const auto flag = s.createEntity("Flag", tower);
    s.registry().get<TransformComponent>(flag).position = {0, 2, 0};
    f.ctx->commit("Setup");

    auto uri = f.ctx->createPrefab(tower);
    REQUIRE(uri);
    CHECK(uri.value() == "assets://prefabs/Tower.agrprefab");
    CHECK(fs::exists(f.root / "game/assets/prefabs/Tower.agrprefab"));
    CHECK(f.ctx->undoStack().undoLabel() == "Create Prefab");
    auto second = f.ctx->instantiatePrefab(uri.value(), {5, 0, 0});
    REQUIRE(second);
    CHECK(s.registry().get<TransformComponent>(second.value()).position.x == doctest::Approx(5.0f));
    const Uuid secondId = s.uuidOf(second.value());

    // Change the first tower's flag height and apply: the second tower follows.
    const auto firstFlag = s.findByUuid(s.uuidOf(s.children(s.findByName("Tower"))[0]));
    s.registry().get<TransformComponent>(firstFlag).position.y = 4.0f;
    f.ctx->commit("Move");  // as the inspector / gizmo do
    REQUIRE(f.ctx->applyPrefab(s.findByName("Tower")));
    const auto secondNow = s.findByUuid(secondId);
    REQUIRE(s.valid(secondNow));
    CHECK(s.registry().get<TransformComponent>(s.children(secondNow)[0]).position.y == doctest::Approx(4.0f));
    CHECK(s.registry().get<TransformComponent>(secondNow).position.x == doctest::Approx(5.0f));  // its own place

    // Undo restores the scene (the prefab file keeps its new content, like materials).
    REQUIRE(f.ctx->undo());
    CHECK(f.ctx->undoStack().redoLabel() == "Apply Prefab");
    REQUIRE(f.ctx->redo());

    // Save, reopen: two instances, children expanded from the prefab file.
    REQUIRE(f.ctx->saveSceneAs("scenes/towers.agrscene"));
    REQUIRE(f.ctx->loadScene("scenes/towers.agrscene"));
    usize instances = 0;
    f.scene().registry().view<PrefabInstanceComponent>().each([&](ecs::Entity e, PrefabInstanceComponent& p) {
        ++instances;
        CHECK(p.prefab == uri.value());
        CHECK(f.scene().children(e).size() == 1);
    });
    CHECK(instances == 2);

    // A prefab edited outside the editor is reloaded and the instances refreshed.
    nlohmann::ordered_json content = *f.engine->prefabs().get(uri.value());
    content["entities"][1]["components"]["Transform"]["position"] = {0.0f, 7.0f, 0.0f};
    std::ofstream(f.root / "game/assets/prefabs/Tower.agrprefab") << content.dump(2);
    fs::last_write_time(f.root / "game/assets/prefabs/Tower.agrprefab",
                        fs::last_write_time(f.root / "game/assets/prefabs/Tower.agrprefab") + std::chrono::seconds(5));
    CHECK(f.ctx->reloadChangedPrefabs() == 2);
    f.scene().registry().view<PrefabInstanceComponent>().each([&](ecs::Entity e, PrefabInstanceComponent&) {
        CHECK(f.scene().registry().get<TransformComponent>(f.scene().children(e)[0]).position.y == doctest::Approx(7.0f));
    });

    // Unpack: ordinary entities.
    const auto any = f.scene().findByName("Tower");
    f.ctx->unpackPrefab(any);
    CHECK_FALSE(prefab::isInstanceRoot(f.scene(), any));
}

// Builds the mobile demo (the platformer template) as a signed release APK at $AGR_DEMO_APK.
TEST_CASE("demo game APK" * doctest::skip(std::getenv("AGR_DEMO_APK") == nullptr)) {
    EditorFixture f(ProjectMode::Mode2D);
    f.ctx->newScene(SceneTemplate::GamePlatformer);
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    Project& project = f.ctx->project();
    project.settings().name = "Robo et les Cristaux";
    project.settings().exportSettings.packageId = "com.superessence.robocristaux";
    const AndroidToolchain tools = findAndroidToolchain(".");
    const auto problems = checkAndroidExport(project, tools);
    REQUIRE_MESSAGE(problems.empty(), problems.front());
    REQUIRE(createKeystore(tools, f.root / "keys" / "demo.p12", "demo", "agr-demo-pass", "AGR Demo"));
    project.settings().exportSettings.keystorePath = (f.root / "keys" / "demo.p12").string();
    project.settings().exportSettings.keyAlias = "demo";
    AndroidBuildOptions o;
    o.kind = AndroidBuildKind::ReleaseApk;
    o.storePassword = "agr-demo-pass";
    auto apk = buildAndroid(project, tools, f.root / "android", o);
    REQUIRE_MESSAGE(apk, (apk ? "" : apk.error().message));
    fs::copy_file(apk.value().output, std::getenv("AGR_DEMO_APK"), fs::copy_options::overwrite_existing);
    CHECK(fs::file_size(std::getenv("AGR_DEMO_APK")) > 1024u * 1024u);
}

// "Les Ruines d'Ambre", the complete 2.5D game, as a signed release APK at $AGR_AMBRE_APK.
TEST_CASE("2.5D game APK (Ruines d'Ambre)" * doctest::skip(std::getenv("AGR_AMBRE_APK") == nullptr)) {
    EditorFixture f(ProjectMode::Mode25D);
    f.ctx->newScene(SceneTemplate::GameAdventure25D);
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    Project& project = f.ctx->project();
    project.settings().name = "Les Ruines d'Ambre";
    project.settings().exportSettings.packageId = "com.superessence.ruinesdambre";
    const AndroidToolchain tools = findAndroidToolchain(".");
    const auto problems = checkAndroidExport(project, tools);
    REQUIRE_MESSAGE(problems.empty(), problems.front());
    REQUIRE(createKeystore(tools, f.root / "keys" / "ambre.p12", "ambre", "agr-ambre-pass", "Superessence Studio"));
    project.settings().exportSettings.keystorePath = (f.root / "keys" / "ambre.p12").string();
    project.settings().exportSettings.keyAlias = "ambre";
    AndroidBuildOptions o;
    o.kind = AndroidBuildKind::ReleaseApk;
    o.storePassword = "agr-ambre-pass";
    auto apk = buildAndroid(project, tools, f.root / "android", o);
    REQUIRE_MESSAGE(apk, (apk ? "" : apk.error().message));
    fs::copy_file(apk.value().output, std::getenv("AGR_AMBRE_APK"), fs::copy_options::overwrite_existing);
    CHECK(fs::file_size(std::getenv("AGR_AMBRE_APK")) > 1024u * 1024u);
}

// "Duel des Tours", the real-time card battle, as a signed release APK at $AGR_DUEL_APK.
TEST_CASE("card battle game APK (Duel des Tours)" * doctest::skip(std::getenv("AGR_DUEL_APK") == nullptr)) {
    EditorFixture f(ProjectMode::Mode3D);
    f.ctx->newScene(SceneTemplate::GameArenaBattle);
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    Project& project = f.ctx->project();
    project.settings().name = "Duel des Tours";
    project.settings().exportSettings.packageId = "com.superessence.dueldestours";
    const AndroidToolchain tools = findAndroidToolchain(".");
    const auto problems = checkAndroidExport(project, tools);
    REQUIRE_MESSAGE(problems.empty(), problems.front());
    REQUIRE(createKeystore(tools, f.root / "keys" / "duel.p12", "duel", "agr-duel-pass", "Superessence Studio"));
    project.settings().exportSettings.keystorePath = (f.root / "keys" / "duel.p12").string();
    project.settings().exportSettings.keyAlias = "duel";
    AndroidBuildOptions o;
    o.kind = AndroidBuildKind::ReleaseApk;
    o.storePassword = "agr-duel-pass";
    auto apk = buildAndroid(project, tools, f.root / "android", o);
    REQUIRE_MESSAGE(apk, (apk ? "" : apk.error().message));
    fs::copy_file(apk.value().output, std::getenv("AGR_DUEL_APK"), fs::copy_options::overwrite_existing);
    CHECK(fs::file_size(std::getenv("AGR_DUEL_APK")) > 1024u * 1024u);
}

// The engine showcase for Android: a menu launching every game template, signed release APK at $AGR_ENGINE_APK.
TEST_CASE("engine showcase APK (all game templates)" * doctest::skip(std::getenv("AGR_ENGINE_APK") == nullptr)) {
    EditorFixture f(ProjectMode::Mode2D);
    Project& project = f.ctx->project();
    std::vector<std::pair<std::string, std::string>> entries;
    int n = 0;
    for (SceneTemplate t : allTemplates()) {
        if (!isGameTemplate(t)) continue;
        Scene game;
        buildSceneTemplate(game, t);
        const std::string rel = "scenes/game_" + std::to_string(++n) + ".agrscene";
        writeTextFile(project.root() / rel, SceneSerializer::toString(game));
        std::string title = templateName(t);
        if (title.rfind("Jeu : ", 0) == 0) title = title.substr(6);
        entries.emplace_back(title, "game://" + rel);
    }
    Scene menu;
    buildLauncherScene(menu, "AGR ENGINE — Build Beyond Limits.", entries);
    writeTextFile(project.root() / project.settings().startupScene, SceneSerializer::toString(menu));
    project.settings().name = "AGR ENGINE";
    project.settings().exportSettings.network = true;  // the multiplayer arena
    const AndroidToolchain tools = findAndroidToolchain(".");
    const auto problems = checkAndroidExport(project, tools);
    REQUIRE_MESSAGE(problems.empty(), problems.front());
    REQUIRE(createKeystore(tools, f.root / "keys" / "engine.p12", "agr", "agr-engine-pass", "AGR ENGINE"));
    project.settings().exportSettings.keystorePath = (f.root / "keys" / "engine.p12").string();
    project.settings().exportSettings.keyAlias = "agr";
    AndroidBuildOptions o;
    o.kind = AndroidBuildKind::ReleaseApk;
    o.storePassword = "agr-engine-pass";
    auto apk = buildAndroid(project, tools, f.root / "android", o);
    REQUIRE_MESSAGE(apk, (apk ? "" : apk.error().message));
    fs::copy_file(apk.value().output, std::getenv("AGR_ENGINE_APK"), fs::copy_options::overwrite_existing);
}

// The editor itself as an Android app (tools/build_android_editor.sh): resources packed under
// assets/editor_resources with a files.txt index, signed release APK at $AGR_EDITOR_APK.
TEST_CASE("Android editor APK" * doctest::skip(std::getenv("AGR_EDITOR_APK") == nullptr)) {
    EditorFixture f(ProjectMode::Mode2D);
    Project& project = f.ctx->project();
    // This temporary packaging project only carries the editor's resources.
    const fs::path assets = project.assetsDirectory();
    fs::remove_all(assets);
    const fs::path resources = fs::path(AGR_TEST_TEMPLATE_ASSETS).parent_path().parent_path();
    const fs::path packed = assets / "editor_resources";
    std::string index;
    u64 hash = 1469598103934665603ull;
    for (const auto& entry : fs::recursive_directory_iterator(resources)) {
        if (!entry.is_regular_file()) continue;
        const std::string rel = fs::relative(entry.path(), resources).generic_string();
        fs::create_directories((packed / rel).parent_path());
        fs::copy_file(entry.path(), packed / rel);
        index += rel + "\n";
        for (char c : rel + std::to_string(entry.file_size())) hash = (hash ^ static_cast<u8>(c)) * 1099511628211ull;
    }
    // The integrated documentation: the engine's guides (docs/*.md).
    for (const auto& entry : fs::directory_iterator(resources.parent_path().parent_path() / "docs")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".md") continue;
        const std::string rel = "docs/" + entry.path().filename().string();
        fs::create_directories((packed / rel).parent_path());
        fs::copy_file(entry.path(), packed / rel);
        index += rel + "\n";
        for (char c : rel + std::to_string(entry.file_size())) hash = (hash ^ static_cast<u8>(c)) * 1099511628211ull;
    }
    writeTextFile(packed / "files.txt", std::to_string(hash) + "\n" + index);
    fs::copy_file(resources / "templates" / "assets" / "textures" / "agr_mark.png", assets / "icon.png");  // launcher icon
    project.settings().name = "AGR ENGINE Editor";
    project.settings().exportSettings.packageId = "com.superessence.agrengine.editor";
    project.settings().exportSettings.network = true;
    project.settings().exportSettings.backQuits = false;
    const AndroidToolchain tools = findAndroidToolchain(".");
    const auto problems = checkAndroidExport(project, tools);
    REQUIRE_MESSAGE(problems.empty(), problems.front());
    REQUIRE(createKeystore(tools, f.root / "keys" / "editor.p12", "agr", "agr-editor-pass", "AGR ENGINE"));
    project.settings().exportSettings.keystorePath = (f.root / "keys" / "editor.p12").string();
    project.settings().exportSettings.keyAlias = "agr";
    AndroidBuildOptions o;
    o.kind = AndroidBuildKind::ReleaseApk;
    o.storePassword = "agr-editor-pass";
    auto apk = buildAndroid(project, tools, f.root / "android", o, [](const std::string& line) { std::printf("%s\n", line.c_str()); });
    REQUIRE_MESSAGE(apk, (apk ? "" : apk.error().message));
    fs::copy_file(apk.value().output, std::getenv("AGR_EDITOR_APK"), fs::copy_options::overwrite_existing);
}

#include "agr/editor/Package.hpp"

TEST_SUITE("packages") {

TEST_CASE("packages: create, preview, install, conflicts, integrity, dependencies, uninstall keeps modified files") {
    const fs::path root = fs::temp_directory_path() / ("agr_pkg_" + Uuid::generate().toString());
    const fs::path src = root / "src" / "assets", dst = root / "dst" / "assets", repo = root / "repo";
    fs::create_directories(src / "trees" / "pine");
    fs::create_directories(dst);
    auto write = [](const fs::path& p, const std::string& text) {
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << text;
    };
    auto read = [](const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };
    write(src / "trees" / "pine" / "pine.agrmat", "{\"color\": [0, 1, 0, 1]}");
    write(src / "trees" / "oak.png", std::string("\x89PNG\r\n\x1a\nfake", 12));
    write(src / "scripts" / "wind.lua", "return {}");
    // Name and path rules.
    CHECK_FALSE(validatePackageName("Mon Paquet").ok());
    CHECK(validatePackageName("forest-pack_2.0").ok());
    CHECK_FALSE(validatePackagePath("../evil.lua").ok());
    CHECK_FALSE(validatePackagePath("/etc/passwd").ok());
    CHECK_FALSE(validatePackagePath("C:/x").ok());
    // Create from a folder and a file.
    PackageManifest m;
    m.name = "forest";
    m.version = "1.2.0";
    m.author = "Superessence Studio";
    m.description = "Arbres";
    auto written = writePackage(repo / "forest.agrpkg", m, src, {"trees", "scripts/wind.lua"});
    REQUIRE_MESSAGE(written, (written ? "" : written.error().message));
    CHECK(written.value().files.size() == 3);
    CHECK_FALSE(writePackage(repo / "x.agrpkg", m, src, {"nothing/here"}).ok());
    auto back = readPackageManifest(repo / "forest.agrpkg");
    REQUIRE(back);
    CHECK(back.value().name == "forest");
    CHECK(back.value().version == "1.2.0");
    CHECK(back.value().files.size() == 3);
    // A dependent package.
    PackageManifest d;
    d.name = "forest-extras";
    d.dependencies = {"forest"};
    write(src / "extras" / "leaf.txt", "leaf");
    REQUIRE(writePackage(repo / "extras.agrpkg", d, src, {"extras"}));
    auto listing = listRepository(repo);
    REQUIRE(listing.size() == 2);
    CHECK(listing[0].manifest.name == "forest");
    // Missing dependency: refused.
    auto planD = planInstall(repo / "extras.agrpkg", dst);
    REQUIRE(planD);
    CHECK(planD.value().missingDependencies == std::vector<std::string>{"forest"});
    CHECK_FALSE(installPackage(repo / "extras.agrpkg", dst, false).ok());
    // Conflict preview: one file already there with other content, one identical.
    write(dst / "scripts" / "wind.lua", "-- mine");
    write(dst / "trees" / "oak.png", std::string("\x89PNG\r\n\x1a\nfake", 12));
    auto plan = planInstall(repo / "forest.agrpkg", dst);
    REQUIRE(plan);
    CHECK(plan.value().newFiles == std::vector<std::string>{"trees/pine/pine.agrmat"});
    CHECK(plan.value().changedFiles == std::vector<std::string>{"scripts/wind.lua"});
    CHECK(plan.value().identicalFiles == std::vector<std::string>{"trees/oak.png"});
    CHECK_FALSE(installPackage(repo / "forest.agrpkg", dst, false).ok());  // would overwrite "mine"
    CHECK(read(dst / "scripts" / "wind.lua") == "-- mine");
    REQUIRE(installPackage(repo / "forest.agrpkg", dst, true));
    CHECK(read(dst / "trees" / "pine" / "pine.agrmat") == "{\"color\": [0, 1, 0, 1]}");
    CHECK(read(dst / "scripts" / "wind.lua") == "return {}");
    REQUIRE(installedPackages(dst).size() == 1);
    CHECK(installedPackages(dst)[0].version == "1.2.0");
    REQUIRE(installPackage(repo / "extras.agrpkg", dst, false));
    // Uninstall: refused while needed; then modified files are kept.
    CHECK_FALSE(uninstallPackage("forest", dst, false).ok());
    REQUIRE(uninstallPackage("forest-extras", dst, false));
    CHECK_FALSE(fs::exists(dst / "extras"));  // emptied folder removed
    write(dst / "trees" / "pine" / "pine.agrmat", "{\"color\": [1, 0, 0, 1]}");  // the user edited it
    auto un = planUninstall("forest", dst);
    REQUIRE(un);
    CHECK(un.value().modified == std::vector<std::string>{"trees/pine/pine.agrmat"});
    CHECK(un.value().remove.size() == 2);
    REQUIRE(uninstallPackage("forest", dst, false));
    CHECK(fs::exists(dst / "trees" / "pine" / "pine.agrmat"));
    CHECK_FALSE(fs::exists(dst / "trees" / "oak.png"));
    CHECK_FALSE(fs::exists(dst / "scripts" / "wind.lua"));
    CHECK(installedPackages(dst).empty());
    // Corruption is detected before anything is written.
    {
        std::string bytes = read(repo / "forest.agrpkg");
        bytes.back() ^= 0x55;
        std::ofstream(repo / "broken.agrpkg", std::ios::binary) << bytes;
    }
    const fs::path clean = root / "clean" / "assets";
    fs::create_directories(clean);
    const Status broken = installPackage(repo / "broken.agrpkg", clean, false);
    CHECK_FALSE(broken.ok());
    CHECK(fs::is_empty(clean));
    CHECK_FALSE(readPackageManifest(src / "trees" / "oak.png").ok());  // not a package
    std::error_code ec;
    fs::remove_all(root, ec);
}

}

#include "agr/editor/Scatter.hpp"

TEST_CASE("procedural scatter: deterministic, spacing, exclusion zones, weights, variations") {
    using namespace agr::editor::templates;
    registerRenderComponents();
    auto build = [](Scene& s, u32 seed) {
        ScatterSettings st;
        st.seed = seed;
        st.min = {-20.0f, -20.0f};
        st.max = {20.0f, 20.0f};
        st.height = -1.0f;
        st.exclude = {{{-5.0f, -5.0f}, {5.0f, 5.0f}}};
        st.spacing = 0.5f;
        st.maxCount = 80;
        const std::vector<ScatterItem> items = {{"assets://a.glb", "Tree", 3.0f, 0.8f, 1.2f, 1.0f, 1.5f},
                                                {"assets://b.glb", "Rock", 1.0f, 0.5f, 1.0f, 0.6f}};
        return scatter(s, s.createEntity("Land"), st, items);
    };
    Scene a, b, c;
    const auto pa = build(a, 42), pb = build(b, 42), pc = build(c, 43);
    REQUIRE(pa.size() > 40);
    REQUIRE(pa.size() == pb.size());
    int trees = 0, rocks = 0;
    for (usize i = 0; i < pa.size(); ++i) {
        const auto& ta = a.registry().get<TransformComponent>(pa[i]);
        const auto& tb = b.registry().get<TransformComponent>(pb[i]);
        CHECK(ta.position == tb.position);  // same seed, same layout
        CHECK(ta.position.y == doctest::Approx(-1.0f));
        CHECK_FALSE((std::abs(ta.position.x) < 5.0f && std::abs(ta.position.z) < 5.0f));  // exclusion zone
        (a.nameOf(pa[i]).rfind("Tree", 0) == 0 ? trees : rocks)++;
        // Model > Skin with the cartoon look and a darker-only tint.
        const auto skin = a.children(a.children(pa[i])[0])[0];
        const auto& mr = a.registry().get<MeshRendererComponent>(skin);
        CHECK(mr.toon);
        CHECK(mr.color.x <= 1.0f);
        for (usize j = 0; j < i; ++j) {  // footprints never overlap
            const auto& tj = a.registry().get<TransformComponent>(pa[j]);
            const float dx = ta.position.x - tj.position.x, dz = ta.position.z - tj.position.z;
            CHECK(std::sqrt(dx * dx + dz * dz) > 0.5f * 0.5f);
        }
    }
    CHECK(trees > rocks);  // weights 3 : 1
    bool different = pc.size() != pa.size();
    for (usize i = 0; !different && i < pa.size(); ++i)
        different = !(c.registry().get<TransformComponent>(pc[i]).position == a.registry().get<TransformComponent>(pa[i]).position);
    CHECK(different);  // another seed, another layout
}

TEST_CASE("particle presets fill 2D and 3D emitters with shipped textures and curves") {
    namespace fs = std::filesystem;
    for (ParticlePreset p : kParticlePresets) {
        CAPTURE(particlePresetName(p));
        CHECK(std::string(particlePresetName(p)).size() > 0);
        ParticleEmitter2DComponent e2;
        ParticleEmitter3DComponent e3;
        applyParticlePreset(p, e2);
        applyParticlePreset(p, e3);
        CHECK(e2.texture == e3.texture);
        CHECK(e2.curves == e3.curves);
        CHECK((e2.rate > 0.0f || e2.burst > 0));
        CHECK(e2.lifetimeMin <= e2.lifetimeMax);
        for (const std::string& uri : {e2.texture, e2.curves})
            if (!uri.empty()) CHECK_MESSAGE(fs::exists(fs::path(AGR_TEST_TEMPLATE_ASSETS) / uri.substr(9)), uri);
        CHECK(e3.gravity.y == doctest::Approx(e2.gravity.y));
    }
    ParticleEmitter2DComponent ex;
    applyParticlePreset(ParticlePreset::Explosion, ex);
    CHECK(ex.flipbookColumns == 4);
    CHECK(ex.flipbookRows == 4);
    ParticleEmitter3DComponent snow;
    applyParticlePreset(ParticlePreset::Snow, snow);
    CHECK(snow.shape == EmitterShape3D::Box);
    CHECK(snow.gravity.y < 0.0f);

}
TEST_CASE("multi-selection component edits are one undo step; history jumps back and forth") {
    EditorFixture f;
    f.ctx->newScene(SceneTemplate::Empty3D);
    const auto a = f.ctx->create(CreateKind::Cube);
    const auto b = f.ctx->create(CreateKind::Sphere);
    const auto c = f.ctx->create(CreateKind::Empty);
    auto& reg = f.ctx->scene().registry();
    const usize before = f.ctx->undoStack().size();
    CHECK(f.ctx->addComponent(std::vector<ecs::Entity>{a, b, c}, "BlobShadow") == 3);
    CHECK(f.ctx->undoStack().size() == before + 1);
    CHECK(f.ctx->undoStack().undoLabel() == "Add BlobShadow (3)");
    CHECK(f.ctx->addComponent(std::vector<ecs::Entity>{a, b}, "BlobShadow") == 0);  // already there: no entry
    CHECK(f.ctx->undoStack().size() == before + 1);
    CHECK(f.ctx->removeComponent(std::vector<ecs::Entity>{a, c}, "BlobShadow") == 2);
    CHECK_FALSE(reg.has<BlobShadowComponent>(f.ctx->scene().findByName("Cube")));
    CHECK(reg.has<BlobShadowComponent>(f.ctx->scene().findByName("Sphere")));
    // History: back two states at once (before both), then forward to the latest.
    const usize latest = f.ctx->undoStack().position();
    CHECK(f.ctx->jumpToHistory(latest - 2));
    CHECK_FALSE(reg.has<BlobShadowComponent>(f.ctx->scene().findByName("Sphere")));
    CHECK(f.ctx->undoStack().canRedo());
    CHECK(f.ctx->jumpToHistory(latest));
    CHECK(f.ctx->scene().registry().has<BlobShadowComponent>(f.ctx->scene().findByName("Sphere")));
    CHECK_FALSE(f.ctx->jumpToHistory(latest));  // already there
    CHECK_FALSE(f.ctx->jumpToHistory(9999));
}

TEST_CASE("Windows export: the cross-compiled player is found in the export templates, the game is packaged") {
    EditorFixture f(ProjectMode::Mode2D);
    f.ctx->newScene(SceneTemplate::Showcase2D);
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    const fs::path templates = f.root / "templates";
    const fs::path editorDir = f.root / "editor" / "bin";
    fs::create_directories(editorDir);
    CHECK(findWindowsPlayer(editorDir).empty());
    fs::create_directories(templates / "windows-x64");
    { std::ofstream(templates / "windows-x64" / "agr_player.exe") << "MZ fake player"; }
#if defined(_WIN32)
    _putenv_s("AGR_EXPORT_TEMPLATES", templates.string().c_str());
#else
    setenv("AGR_EXPORT_TEMPLATES", templates.string().c_str(), 1);
#endif
    const fs::path player = findWindowsPlayer(editorDir);
#if defined(_WIN32)
    _putenv_s("AGR_EXPORT_TEMPLATES", "");
#else
    unsetenv("AGR_EXPORT_TEMPLATES");
#endif
    REQUIRE_FALSE(player.empty());
    CHECK(player.filename() == "agr_player.exe");
    auto report = exportDesktop(f.ctx->project(), f.root / "out", player);
    REQUIRE_MESSAGE(report, (report ? "" : report.error().message));
    const bool exe = fs::exists(report.value().output / (report.value().output.filename().string() + ".exe"));
    CHECK(exe);  // <Name>/<Name>.exe
    CHECK(fs::exists(report.value().output / "icon.png"));
    CHECK_FALSE(fs::exists(report.value().output / (report.value().output.filename().string() + ".desktop")));  // Windows: no desktop entry
    { std::ofstream(templates / "agr_player") << "ELF fake player"; }
    auto linux = exportDesktop(f.ctx->project(), f.root / "out_linux", templates / "agr_player");
    REQUIRE(linux);
    const fs::path desktop = linux.value().output / (linux.value().output.filename().string() + ".desktop");
    REQUIRE(fs::exists(desktop));
    CHECK(readTextFile(desktop).find("Categories=Game;") != std::string::npos);
    CHECK(fs::exists(report.value().output / "game" / "game.json"));
    // Next to the editor (export_templates/) also works.
    fs::create_directories(editorDir / "export_templates" / "windows-x64");
    fs::copy_file(templates / "windows-x64" / "agr_player.exe", editorDir / "export_templates" / "windows-x64" / "agr_player.exe");
    CHECK_FALSE(findWindowsPlayer(editorDir).empty());
}

TEST_CASE("splash screen: project export settings reach game.json and read back") {
    GameManifest m;
    CHECK(m.splashImage.empty());
    m.splashImage = "assets://splash.png";
    m.splashColor = {1.0f, 0.5f, 0.25f, 1.0f};
    m.splashSeconds = 2.5f;
    auto back = GameManifest::fromJson(m.toJson());
    REQUIRE(back);
    CHECK(back.value().splashImage == "assets://splash.png");
    CHECK(back.value().splashColor[1] == doctest::Approx(0.5f));
    CHECK(back.value().splashSeconds == doctest::Approx(2.5f));

    EditorFixture f(ProjectMode::Mode2D);
    ExportSettings& x = f.ctx->project().settings().exportSettings;
    x.splashImage = "assets://logo.png";
    x.splashColor = {0.1f, 0.2f, 0.3f, 1.0f};
    x.splashSeconds = 0.75f;
    REQUIRE(f.ctx->project().save());
    auto reopened = Project::open(f.ctx->project().root());
    REQUIRE(reopened);
    CHECK(reopened.value().settings().exportSettings.splashImage == "assets://logo.png");
    CHECK(reopened.value().settings().exportSettings.splashColor.z == doctest::Approx(0.3f));
    const GameManifest gm = gameManifest(reopened.value());
    CHECK(gm.splashImage == "assets://logo.png");
    CHECK(gm.splashSeconds == doctest::Approx(0.75f));
}

TEST_CASE("UI Builder smart guides: edges and centers snap within the threshold") {
    const std::vector<math::Vec4> targets = {{0, 0, 800, 600}, {100, 100, 200, 50}};
    // Left edge 3 px right of the other element's left edge: snaps back by 3.
    GuideSnap g = snapToGuides({103, 310, 120, 40}, targets, 6.0f);
    CHECK(g.offset.x == doctest::Approx(-3.0f));
    CHECK(g.offset.y == doctest::Approx(0.0f));  // nothing within 6 px vertically
    REQUIRE_FALSE(g.xs.empty());
    CHECK(g.xs[0] == doctest::Approx(100.0f));
    CHECK(g.ys.empty());
    // Center on the parent's center (400, 300).
    g = snapToGuides({338, 276, 120, 40}, targets, 6.0f);
    CHECK(g.offset.x == doctest::Approx(2.0f));
    CHECK(g.offset.y == doctest::Approx(4.0f));
    // Too far: no snap, no guide.
    g = snapToGuides({500, 400, 50, 50}, targets, 6.0f);
    CHECK(g.offset.x == 0.0f);
    CHECK(g.offset.y == 0.0f);
    CHECK(g.xs.empty());
    CHECK(g.ys.empty());
}

TEST_CASE("UI Builder align / distribute: offsets for edges, centers and even gaps") {
    const std::vector<math::Vec4> r = {{10, 10, 50, 20}, {100, 40, 30, 30}, {200, 5, 20, 40}};
    auto left = alignRects(r, AlignMode::Left);
    CHECK(left[1].x == doctest::Approx(-90.0f));
    CHECK(left[2].x == doctest::Approx(-190.0f));
    CHECK(left[0].y == 0.0f);
    auto right = alignRects(r, AlignMode::Right);
    CHECK(right[0].x == doctest::Approx(160.0f));  // right edge 60 -> 220
    auto middle = alignRects(r, AlignMode::Middle);  // extremes 5..70, center 37.5
    CHECK(middle[0].y == doctest::Approx(17.5f));
    auto dist = alignRects(r, AlignMode::DistributeX);  // span 10..220, widths 100: gaps 55
    CHECK(dist[0].x == doctest::Approx(0.0f));
    CHECK(dist[1].x == doctest::Approx(15.0f));  // 10 + 50 + 55 = 115
    CHECK(dist[2].x == doctest::Approx(0.0f));
    CHECK(alignRects({r[0], r[1]}, AlignMode::DistributeX)[1].x == 0.0f);  // needs 3
}

TEST_CASE("Create > Vehicle: a dynamic body with Vehicle3D and the four named wheels") {
    EditorFixture f;
    const auto car = f.ctx->create(CreateKind::Vehicle);
    REQUIRE_FALSE(car.isNull());
    Scene& s = f.ctx->scene();
    CHECK(s.registry().has<Vehicle3DComponent>(car));
    CHECK(s.registry().get<RigidBodyComponent>(car).type == BodyType3D::Dynamic);
    int wheels = 0;
    for (ecs::Entity c : s.children(car))
        if (s.nameOf(c).rfind("Wheel", 0) == 0) ++wheels;
    CHECK(wheels == 4);
    // Simulated on a ground: it rests on its wheels.
    const auto ground = s.createEntity("Ground");
    s.registry().get<TransformComponent>(ground).position = {0, -0.5f, 0};
    s.registry().emplace<RigidBodyComponent>(ground).type = BodyType3D::Static;
    s.registry().emplace<ColliderComponent>(ground).size = {100, 1, 100};
    s.registry().get<TransformComponent>(car).position = {0, 1.0f, 0};
    s.updateTransforms();
    Physics3DWorld physics;
    for (int i = 0; i < 120; ++i) physics.step(s, 1.0f / 60.0f);
    CHECK(s.registry().get<TransformComponent>(car).position.y > 0.4f);
}

TEST_CASE("zipFolder: a stored ZIP of the export folder") {
    const fs::path root = fs::temp_directory_path() / ("agr_zip_" + Uuid::generate().toString());
    fs::create_directories(root / "Game" / "game");
    { std::ofstream(root / "Game" / "Game") << "player"; }
    { std::ofstream(root / "Game" / "game" / "game.json") << "{}"; }
    { std::ofstream(root / "Game" / ".agr-export") << "marker"; }
    REQUIRE(zipFolder(root / "Game", root / "Game.zip"));
    const std::string zip = readTextFile(root / "Game.zip");
    REQUIRE(zip.size() > 22);
    CHECK(zip.compare(0, 4, "PK\x03\x04") == 0);
    const usize end = zip.size() - 22;  // end of central directory: entry count at +10
    CHECK(zip.compare(end, 4, "PK\x05\x06") == 0);
    CHECK((static_cast<unsigned char>(zip[end + 10]) | (static_cast<unsigned char>(zip[end + 11]) << 8)) == 2);  // the marker is left out
    CHECK(zip.find("Game/game/game.json") != std::string::npos);
    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST_CASE("curve editor: screen mapping, tangent handles and making a key Custom keeps the curve") {
    using agr::editor::CurveView;
    const CurveView v{{100, 50}, {400, 200}, 2.0f, -1.0f, 1.0f};
    const auto p = v.toScreen(1.0f, 0.0f);
    CHECK(p.x == doctest::Approx(300.0f));
    CHECK(p.y == doctest::Approx(150.0f));
    CHECK(v.timeAt(p.x) == doctest::Approx(1.0f));
    CHECK(v.valueAt(50.0f) == doctest::Approx(1.0f));
    Keyframe k;
    k.time = 1.0f;
    // A handle placed along a slope reads back as that slope, on both sides.
    for (float slope : {0.0f, 0.5f, -2.0f}) {
        const auto out = v.handle(k, slope, 1), in = v.handle(k, slope, -1);
        CHECK(std::hypot(out.x - p.x, out.y - p.y) == doctest::Approx(40.0f));
        CHECK(v.slopeFromHandle(k, out, 1) == doctest::Approx(slope).epsilon(0.01));
        CHECK(v.slopeFromHandle(k, in, -1) == doctest::Approx(slope).epsilon(0.01));
    }
    // An out handle dragged behind the key: a steep slope, not a flipped one.
    CHECK(v.slopeFromHandle(k, {p.x - 10, p.y - 10}, 1) >= 99.0f);

    Curve c;
    c.setKey(0.0f, 0.0f);
    c.setKey(1.0f, 1.0f);
    c.setKey(2.0f, 3.0f);
    const float before = c.evaluate(0.7f), before2 = c.evaluate(1.4f);
    CHECK(c.autoSlope(1) > 0.0f);
    c.makeCustom(1);
    CHECK(c.keys[1].interpolation == Interpolation::Custom);
    CHECK(c.keys[1].outTangent == doctest::Approx(c.autoSlope(1)));
    CHECK(c.evaluate(0.7f) == doctest::Approx(before));  // same shape until a handle moves
    CHECK(c.evaluate(1.4f) == doctest::Approx(before2));
    c.keys[1].outTangent = c.keys[1].inTangent = 0.0f;  // flattened handles
    CHECK(c.evaluate(1.1f) < before2);
}

TEST_CASE("data pack export: encrypted game.agrpak, key embedded in the player, patch of the changed files") {
    EditorFixture f(ProjectMode::Mode2D);
    f.ctx->newScene(SceneTemplate::Showcase2D);
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    auto& x = f.ctx->project().settings().exportSettings;
    x.dataPack = true;
    x.encryptData = true;
    // A fake player with an empty key slot (the marker, 32 key bytes, the "in use" byte).
    std::string marker = "AGRPACKKEYSLOT01";
    std::string fake = "ELF fake player..." + marker + std::string(48, '\0') + "...end";
    const fs::path player = f.root / "agr_player";
    { std::ofstream(player, std::ios::binary) << fake; }
    CHECK_FALSE(readEmbeddedPackKey(player));
    auto report = exportDesktop(f.ctx->project(), f.root / "out", player);
    REQUIRE_MESSAGE(report, (report ? "" : report.error().message));
    const fs::path game = report.value().output / "game";
    CHECK(fs::exists(game / "game.json"));
    CHECK(fs::exists(game / "game.agrpak"));
    CHECK_FALSE(fs::exists(game / "assets"));  // nothing left loose
    CHECK_FALSE(fs::exists(game / "scenes"));
    auto key = projectPackKey(f.ctx->project());
    REQUIRE(key);
    CHECK(fs::exists(f.ctx->project().root() / "keys" / "data.key"));
    const fs::path exe = report.value().output / report.value().output.filename();
    auto embedded = readEmbeddedPackKey(exe);
    REQUIRE(embedded);
    CHECK(*embedded == key.value());
    CHECK(projectPackKey(f.ctx->project()).value() == key.value());  // stable
    CHECK_FALSE(PackFileProvider::openFile(game / "game.agrpak"));   // useless without the key
    auto pack = PackFileProvider::openFile(game / "game.agrpak", &key.value());
    REQUIRE(pack);
    CHECK(pack.value()->exists(f.ctx->project().settings().startupScene));

    // Nothing changed: an empty patch. One asset changed: a one-file patch.
    auto empty = exportPatch(f.ctx->project(), game / "game.agrpak", f.root / "p0.agrpak");
    REQUIRE(empty);
    CHECK(empty.value().files == 0);
    const fs::path asset = f.ctx->project().assetsDirectory() / "notes.txt";
    { std::ofstream(asset) << "new in 1.1"; }
    auto patch = exportPatch(f.ctx->project(), game / "game.agrpak", f.root / "p1.agrpak");
    REQUIRE(patch);
    CHECK(patch.value().files == 1);
    auto p1 = PackFileProvider::openFile(f.root / "p1.agrpak", &key.value());
    REQUIRE(p1);
    REQUIRE(p1.value()->entries().size() == 1);
    CHECK(p1.value()->entries()[0].path == "assets/notes.txt");
    // A player without a slot is refused rather than shipping an unreadable game.
    { std::ofstream(f.root / "old_player", std::ios::binary) << "no slot here"; }
    CHECK_FALSE(embedPackKey(f.root / "old_player", key.value()));
}

TEST_CASE("package repository: index, dependency order, checked downloads, install from the repository") {
    const fs::path root = fs::temp_directory_path() / ("agr_repo_" + Uuid::generate().toString());
    const fs::path src = root / "src", repo = root / "site", dst = root / "game" / "assets";
    fs::create_directories(dst);
    auto write = [](const fs::path& p, const std::string& text) {
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << text;
    };
    write(src / "core" / "util.lua", "return {}");
    write(src / "enemies" / "slime.lua", "return {}");
    write(src / "levels" / "cave.agrscene", "{}");
    auto pkg = [&](const std::string& name, const std::string& version, std::vector<std::string> deps, const std::string& dir) {
        PackageManifest m;
        m.name = name, m.version = version, m.dependencies = std::move(deps);
        REQUIRE(writePackage(repo / (name + "-" + version + ".agrpkg"), m, src, {dir}));
    };
    pkg("core", "1.0.0", {}, "core");
    pkg("enemies", "1.0.0", {"core"}, "enemies");
    pkg("enemies", "1.10.0", {"core"}, "enemies");
    pkg("caves", "2.0.0", {"enemies"}, "levels");
    auto index = writeRepositoryIndex(repo, "Test store");
    REQUIRE(index);
    CHECK(fs::exists(repo / "index.json"));
    auto fetched = fetchRepositoryIndex("file://" + repo.generic_string() + "/");
    REQUIRE(fetched);
    const OnlineIndex& idx = fetched.value();
    CHECK(idx.title == "Test store");
    CHECK(idx.packages.size() == 4);
    REQUIRE(idx.find("enemies"));
    CHECK(idx.find("enemies")->version == "1.10.0");  // latest first (numeric order)

    auto order = resolveInstallOrder("caves", idx, {});
    REQUIRE(order);
    CHECK(order.value() == std::vector<std::string>{"core", "enemies", "caves"});
    CHECK(resolveInstallOrder("caves", idx, {"core"}).value() == std::vector<std::string>{"enemies", "caves"});
    CHECK_FALSE(resolveInstallOrder("dragons", idx, {}));

    // A cycle and a missing dependency are reported, not looped on.
    OnlineIndex bad = idx;
    bad.packages.push_back({"a", "1", "", "", {"b"}, "x", std::string(64, '0'), 0});
    bad.packages.push_back({"b", "1", "", "", {"a"}, "x", std::string(64, '0'), 0});
    bad.packages.push_back({"c", "1", "", "", {"zzz"}, "x", std::string(64, '0'), 0});
    CHECK(resolveInstallOrder("a", bad, {}).error().message.find("cycle") != std::string::npos);
    CHECK(resolveInstallOrder("c", bad, {}).error().message.find("zzz") != std::string::npos);

    // Install with dependencies, in order.
    auto done = installFromRepository("caves", idx, root / "downloads", dst, false);
    REQUIRE_MESSAGE(done, (done ? "" : done.error().message));
    CHECK(done.value().size() == 3);
    CHECK(fs::exists(dst / "core" / "util.lua"));
    CHECK(fs::exists(dst / "levels" / "cave.agrscene"));
    CHECK(installedPackages(dst).size() == 3);

    // A tampered file is refused and not kept.
    OnlinePackage tampered = *idx.find("core");
    tampered.sha256 = std::string(64, 'a');
    CHECK_FALSE(downloadPackage(tampered, root / "dl2"));
    CHECK_FALSE(fs::exists(root / "dl2" / "core-1.0.0.agrpkg"));
    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST_CASE("Windows export: the game's icon replaces the player's placeholder icon resource") {
    // Needs the cross-compiled player (preset windows-x64); skipped without it.
    fs::path player;
    for (const fs::path& p : {fs::path(AGR_SOURCE_DIR) / "build" / "windows-x64" / "bin" / "agr_player.exe"})
        if (fs::exists(p)) player = p;
    if (player.empty()) {
        WARN_MESSAGE(false, "no Windows player built: icon test skipped");
        return;
    }
    const std::vector<u8> placeholder = readWindowsIcon(player);
    REQUIRE(placeholder.size() > 1000);  // the placeholder's PNG and its room
    EditorFixture f(ProjectMode::Mode2D);
    f.ctx->newScene(SceneTemplate::Showcase2D);
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    // A 300 x 200 red icon: fitted into 256 x 256 with transparent margins.
    std::vector<u8> red(300 * 200 * 4);
    for (usize i = 0; i < red.size(); i += 4) red[i] = 220, red[i + 1] = 40, red[i + 2] = 40, red[i + 3] = 255;
    auto png = encodePng(300, 200, red);
    REQUIRE(png);
    { std::ofstream(f.ctx->project().assetsDirectory() / "icon.png", std::ios::binary).write(reinterpret_cast<const char*>(png.value().data()), static_cast<std::streamsize>(png.value().size())); }
    auto report = exportDesktop(f.ctx->project(), f.root / "out", player);
    REQUIRE(report);
    CHECK(report.value().warnings.empty());
    const fs::path exe = report.value().output / (report.value().output.filename().string() + ".exe");
    const std::vector<u8> icon = readWindowsIcon(exe);
    auto expected = squareIconPng(f.ctx->project(), 256);
    REQUIRE(expected);
    CHECK(icon == expected.value());
    auto decoded = decodeImage(icon);
    REQUIRE(decoded);
    CHECK(decoded.value().width == 256);
    CHECK(decoded.value().pixels[(128 * 256 + 128) * 4] == 220);    // red in the middle
    CHECK(decoded.value().pixels[(5 * 256 + 128) * 4 + 3] == 0);    // transparent margin above
    CHECK(readWindowsIcon(player) == placeholder);                  // the template is untouched
    // Too large an image is refused rather than corrupting the executable.
    CHECK_FALSE(embedWindowsIcon(exe, std::vector<u8>(512 * 1024, 7)));
}

TEST_CASE("undo history: asset file edits (materials) are undone and redone with the scene changes, in order") {
    EditorFixture f(ProjectMode::Mode3D);
    auto& vfs = f.engine->vfs();
    const std::string uri = "assets://red.agrmat";
    REQUIRE(vfs.writeText(uri, "v1"));
    REQUIRE(vfs.writeText(uri, "v2"));
    f.ctx->commitAssetFile("Material red", uri, "v1", "v2");
    const auto cube = f.ctx->create(CreateKind::Cube);  // a scene action after it
    f.ctx->commit("Create Cube");
    const Uuid cubeId = f.ctx->scene().uuidOf(cube);
    REQUIRE(vfs.writeText(uri, "v3"));
    f.ctx->commitAssetFile("Material red", uri, "v2", "v3");
    auto text = [&] { return vfs.readText(uri).value(); };
    REQUIRE(f.ctx->undo());
    CHECK(text() == "v2");
    CHECK_FALSE(f.ctx->scene().findByUuid(cubeId).isNull());  // the cube is still there
    REQUIRE(f.ctx->undo());
    CHECK(text() == "v2");
    CHECK(f.ctx->scene().findByUuid(cubeId).isNull());  // creation undone
    REQUIRE(f.ctx->undo());
    CHECK(text() == "v1");
    REQUIRE(f.ctx->redo());
    CHECK(text() == "v2");
    // Jumping in the history applies every file change on the way.
    REQUIRE(f.ctx->jumpToHistory(f.ctx->undoStack().size() - 1));
    CHECK(text() == "v3");
    REQUIRE(f.ctx->jumpToHistory(0));
    CHECK(text() == "v1");
    // An edit that changes nothing is not recorded.
    const usize n = f.ctx->undoStack().size();
    f.ctx->commitAssetFile("Material red", uri, "same", "same");
    CHECK(f.ctx->undoStack().size() == n);
}

TEST_CASE("export filters: glob patterns, assets reached from the scenes through materials and scripts") {
    CHECK(exportPatternMatch("*.psd", "art/hero.psd"));
    CHECK_FALSE(exportPatternMatch("*.psd", "art/hero.png"));
    CHECK(exportPatternMatch("sources/*", "sources/a.blend"));
    CHECK_FALSE(exportPatternMatch("sources/*", "sources/deep/a.blend"));
    CHECK(exportPatternMatch("sources/**", "sources/deep/a.blend"));
    CHECK(exportPatternMatch("levels/lvl?.agrscene", "levels/lvl3.agrscene"));
    CHECK_FALSE(exportPatternMatch("levels/lvl?.agrscene", "levels/lvl10.agrscene"));

    EditorFixture f(ProjectMode::Mode3D);
    const fs::path a = f.ctx->project().assetsDirectory();
    auto write = [&](const std::string& rel, const std::string& text) {
        fs::create_directories((a / rel).parent_path());
        std::ofstream(a / rel, std::ios::binary) << text;
    };
    write("mat/rock.agrmat", R"({"baseColorTexture": "assets://tex/rock.png", "shader": "assets://fx/wet.agrshader"})");
    write("tex/rock.png", "PNG");
    write("fx/wet.agrshader", "param texture n = \"assets://tex/noise.png\";\nvoid surface(inout Surface s) {}\n");
    write("tex/noise.png", "PNG");
    write("scripts/boss.lua", "local S = {}\nfunction S:onStart() agr.audio.play('assets://sfx/roar.ogg') end\nreturn S\n");
    write("sfx/roar.ogg", "OGG");
    write("models/ship.gltf", "{}");
    write("models/ship.bin", "BIN");
    write("models/ship_albedo.png", "PNG");
    write("unused/old.png", "PNG");
    write("unused/notes.txt", "x");
    // A scene that references the material, the script and the model.
    const auto e = f.ctx->create(CreateKind::Cube);
    f.ctx->scene().registry().get<MeshRendererComponent>(e).material = "assets://mat/rock.agrmat";
    f.ctx->scene().registry().emplace<ScriptComponent>(e).script = "assets://scripts/boss.lua";
    const auto ship = f.ctx->create(CreateKind::Cube);
    f.ctx->scene().registry().get<MeshRendererComponent>(ship).model = "assets://models/ship.gltf#0";
    REQUIRE(f.ctx->saveSceneAs(f.ctx->project().settings().startupScene));
    const auto used = usedAssets(f.ctx->project());
    for (const char* rel : {"mat/rock.agrmat", "tex/rock.png", "fx/wet.agrshader", "tex/noise.png", "scripts/boss.lua", "sfx/roar.ogg", "models/ship.gltf",
                            "models/ship.bin", "models/ship_albedo.png"})
        CHECK_MESSAGE(used.count(rel), rel);
    CHECK_FALSE(used.count("unused/old.png"));
    // Export: only the used ones, plus the include filter, minus the exclude filter.
    auto& x = f.ctx->project().settings().exportSettings;
    x.onlyUsedAssets = true;
    x.includeFilter = "unused/*.txt";
    x.excludeFilter = "*.bin";
    const fs::path out = fs::temp_directory_path() / ("agr_filter_" + Uuid::generate().toString());
    auto report = packageGameData(f.ctx->project(), out);
    REQUIRE_MESSAGE(report, (report ? "" : report.error().message));
    CHECK(fs::exists(out / "assets" / "sfx" / "roar.ogg"));
    CHECK(fs::exists(out / "assets" / "unused" / "notes.txt"));
    CHECK_FALSE(fs::exists(out / "assets" / "unused" / "old.png"));
    CHECK_FALSE(fs::exists(out / "assets" / "models" / "ship.bin"));
    CHECK(report.value().skippedAssets >= 2);
}

TEST_CASE("API reference: every component and field, the Lua modules; docs/API.md is up to date") {
    VirtualFileSystem vfs;
    ScriptEngine scripts(vfs);
    const std::string md = apiReferenceMarkdown(&scripts);
    CHECK(md.find("### NavArea") != std::string::npos);
    CHECK(md.find("`occlusionStrength`") != std::string::npos);
    CHECK(md.find("**`agr.http`**") != std::string::npos);
    CHECK(md.find("`agr.loadPack`") != std::string::npos);
    // The committed reference lists every registered component (regenerate it after adding one).
    std::ifstream in(fs::path(AGR_SOURCE_DIR) / "docs" / "API.md", std::ios::binary);
    const std::string committed((std::istreambuf_iterator<char>(in)), {});
    for (const ComponentInfo* c : ComponentRegistry::instance().all())
        if (c->serializable) CHECK_MESSAGE(committed.find("### " + c->name + "\n") != std::string::npos, "docs/API.md lacks ", c->name, ": run agr_editor --api-reference docs/API.md");
}

TEST_CASE("official packages: scripts pass the static checks, all install, the modules work") {
    const fs::path src = fs::path(AGR_SOURCE_DIR) / "packages";
    // Every script compiles and only uses the real API (lint: unknown agr.* paths, typos).
    VirtualFileSystem lintVfs;
    ScriptEngine linter(lintVfs);
    int scripts = 0;
    for (const auto& e : fs::recursive_directory_iterator(src)) {
        if (e.path().extension() != ".lua") continue;
        std::ifstream in(e.path(), std::ios::binary);
        const std::string code((std::istreambuf_iterator<char>(in)), {});
        INFO(e.path().string());
        CHECK_FALSE(linter.checkSyntax(code, e.path().filename().string()));
        for (const auto& w : linter.lint(code, e.path().filename().string())) CHECK_MESSAGE(false, "line ", w.line, ": ", w.message);
        ++scripts;
    }
    CHECK(scripts >= 18);
    // The shipped library installs whole into a project.
    auto index = fetchRepositoryIndex("file://" + (fs::path(AGR_SOURCE_DIR) / "editor" / "resources" / "packages").generic_string() + "/");
    REQUIRE(index);
    CHECK(index.value().packages.size() >= 16);
    EditorFixture f(ProjectMode::Mode3D);
    for (const OnlinePackage& p : index.value().packages) {
        auto done = installFromRepository(p.name, index.value(), f.root / "downloads", f.ctx->project().assetsDirectory(), false);
        CHECK_MESSAGE(done, p.name, ": ", (done ? "" : done.error().message));
    }
    CHECK(fs::exists(f.ctx->project().assetsDirectory() / "kits" / "inventory" / "inventory.lua"));
    CHECK(fs::exists(f.ctx->project().assetsDirectory() / "kits" / "shaders" / "water.agrshader"));
    // Modules run: inventory stacking / removal, dialogue branches with conditions, the state machine.
    VirtualFileSystem vfs;
    vfs.mount("assets", std::make_unique<NativeFileProvider>(f.ctx->project().assetsDirectory().string(), true));
    ScriptEngine lua(vfs);
    const Status run = lua.run(R"(
local Inventory = require("assets://kits/inventory/inventory.lua")
local inv = Inventory.new(3, "assets://kits/inventory/items.json")
assert(inv:add("potion", 25) == 0, "25 potions fit in 3 slots of 10")
assert(inv:count("potion") == 25)
assert(inv:add("sword", 1) == 1, "no slot left for the sword")
assert(inv:remove("potion", 6) and inv:count("potion") == 19)
assert(not inv:remove("potion", 100))
assert(inv:value() == 19 * 5)
local Dialogue = require("assets://kits/dialogue/dialogue.lua")
local d = assert(Dialogue.load("assets://kits/dialogue/example.json"))
local line = d:start("hello")
assert(line.speaker == "Marchande" and #line.choices == 2, "the paid choice is hidden without coins")
d.vars.coins = 12
line = d:start("hello")
assert(#line.choices == 3)
line = d:choose(3)
assert(line.text:find("épée") and d.vars.coins == 2)
assert(d:choose(nil) == nil and d:finished())
local FSM = require("assets://kits/fsm/fsm.lua")
local log = {}
local owner = {}
local m = FSM.new(owner, { a = { enter = function() log[#log+1] = "enterA" end, update = function(_, _, t) if t > 0.5 then return "b" end end,
                                exit = function() log[#log+1] = "exitA" end },
                           b = { enter = function() log[#log+1] = "enterB" end } }, "a")
m:update(0.3) ; assert(m.state == "a")
m:update(0.3) ; assert(m.state == "b" and m.time == 0)
m:back() ; assert(m.state == "a")
assert(table.concat(log, ",") == "enterA,exitA,enterB,enterA")
-- Saves: serialized with a version, older saves migrated, newer ones refused.
local Save = require("assets://kits/save/save.lua")
local v1 = Save.new({ version = 1 })
local text = v1:serialize({ coins = 7, level = 2 })
local v2 = Save.new({ version = 2, migrations = { [2] = function(d) d.gold = d.coins; d.coins = nil end } })
local data = assert(v2:deserialize(text))
assert(data.gold == 7 and data.coins == nil and data.level == 2, "migrated to version 2")
assert(v1:deserialize(v2:serialize(data)) == nil, "a newer save is refused")
assert(v2:deserialize("not json") == nil)
-- Loot: same seed, same drops; pity guarantees a rare item.
local Loot = require("assets://kits/loot/loot.lua")
local entries = { { item = "coin", weight = 95, min = 5, max = 20 }, { item = "gem", weight = 5, rare = true } }
local a, b = Loot.new(entries, { seed = 9 }), Loot.new(entries, { seed = 9 })
for _ = 1, 20 do
    local x, y = a:rollOne(), b:rollOne()
    assert(x.item == y.item and x.count == y.count, "deterministic")
    if x.item == "coin" then assert(x.count >= 5 and x.count <= 20) end
end
local pity = Loot.new({ { item = "coin", weight = 1000000 }, { item = "gem", weight = 1, rare = true } }, { seed = 3, pity = 10 })
local drops = pity:roll(10)
assert(drops[10].item == "gem" and drops[10].rare, "10th roll forced rare")
assert(math.abs(Loot.new(entries):chance("gem") - 0.05) < 1e-9)
-- Quests: requirements, counted objectives, completion callback, save / restore.
local Quests = require("assets://kits/quests/quests.lua")
local q = Quests.new({ wolves = { title = "Chasse", objectives = { { id = "kill", count = 3 } } },
                       den = { title = "Tanière", requires = { "wolves" }, objectives = { { id = "find" } } } })
local done = {}
q.onCompleted = function(id) done[#done + 1] = id end
assert(q:status("den") == "locked" and not q:start("den"))
assert(q:start("wolves") and q:status("wolves") == "active")
assert(not q:progress("wolves", "kill", 2) and q:count("wolves", "kill") == 2)
assert(q:progress("wolves", "kill", 5) and q:status("wolves") == "completed" and q:count("wolves", "kill") == 3)
assert(q:status("den") == "available" and q:start("den"))
local saved = agr.json.decode(agr.json.encode(q:save()))
local q2 = Quests.new(q.defs)
q2:restore(saved)
assert(q2:status("wolves") == "completed" and q2:active()[1] == "den" and done[1] == "wolves")
-- Waves: schedule with pauses, positions on a ring.
local Waves = require("assets://kits/waves/waves.lua")
local plan = Waves.schedule({ { prefab = "e", count = 3, interval = 1 }, { prefab = "f", count = 2, interval = 0.5 } }, 4)
assert(#plan == 5 and plan[3].time == 2 and plan[4].time == 6 and plan[4].wave == 2 and plan[5].time == 6.5)
local rx, rz = Waves.ring(1, 4, 2, 10, 0)
assert(math.abs(rx - 12) < 1e-6 and math.abs(rz) < 1e-6)
-- Joystick: dead zone, full tilt clamped, y up.
local Stick = require("assets://kits/joystick/stick.lua")
local sx, sy = Stick.compute(100, 100, 105, 100, 90, 0.15)
assert(sx == 0 and sy == 0, "inside the dead zone")
sx, sy = Stick.compute(100, 100, 400, 100, 90, 0.15)
assert(math.abs(sx - 1) < 1e-6 and math.abs(sy) < 1e-6, "clamped to 1")
sx, sy = Stick.compute(100, 100, 100, 55, 90, 0)
assert(math.abs(sy - 0.5) < 1e-6, "finger up = positive y")
)");
    CHECK_MESSAGE(run, (run ? "" : run.error().message));
}

TEST_CASE("texture compression for Android: images become ETC2 KTX under their names, excluded ones stay, loading still works") {
    const fs::path dir = fs::temp_directory_path() / ("agr_etc_" + Uuid::generate().toString());
    fs::create_directories(dir / "tex");
    fs::create_directories(dir / "terrain");
    std::vector<u8> px(32 * 32 * 4, 128);
    auto png = encodePng(32, 32, px);
    REQUIRE(png);
    for (const char* rel : {"tex/wall.png", "terrain/heightmap.png", "tex/tiny.png"}) {
        std::ofstream out(dir / rel, std::ios::binary);
        out.write(reinterpret_cast<const char*>(png.value().data()), static_cast<std::streamsize>(png.value().size()));
    }
    auto tiny = encodePng(4, 4, std::vector<u8>(64, 9));
    { std::ofstream(dir / "tex" / "tiny.png", std::ios::binary).write(reinterpret_cast<const char*>(tiny.value().data()), static_cast<std::streamsize>(tiny.value().size())); }
    u64 saved = 0;
    CHECK(compressTexturesInFolder(dir, "*height*", &saved) == 1);
    CHECK(saved > 0);
    auto read = [&](const char* rel) {
        std::ifstream in(dir / rel, std::ios::binary);
        return std::vector<u8>((std::istreambuf_iterator<char>(in)), {});
    };
    CHECK(etc2::isKtx(read("tex/wall.png")));
    CHECK_FALSE(etc2::isKtx(read("terrain/heightmap.png")));  // excluded: still PNG
    CHECK_FALSE(etc2::isKtx(read("tex/tiny.png")));           // too small to bother
    auto img = decodeImage(read("tex/wall.png"));             // CPU readers still get pixels
    REQUIRE(img);
    CHECK(img.value().width == 32);
    CHECK(std::abs(int(img.value().pixels[0]) - 128) <= 3);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

#if !defined(_WIN32)
TEST_CASE("Android one-click deploy: adb devices parsed, APK installed then started on the chosen device") {
    const auto devices = parseAdbDevices("* daemon started successfully\nList of devices attached\n"
                                         "R58M123ABC             device usb:1-1 product:beyond1 model:SM_G973F device:beyond1 transport_id:1\n"
                                         "emulator-5554          device product:sdk_gphone64 model:sdk_gphone64_x86_64 transport_id:2\n"
                                         "0123456789             unauthorized usb:1-2 transport_id:3\n\n");
    REQUIRE(devices.size() == 3);
    CHECK(devices[0].serial == "R58M123ABC");
    CHECK(devices[0].model == "SM_G973F");
    CHECK(devices[0].ready());
    CHECK(devices[2].state == "unauthorized");
    CHECK_FALSE(devices[2].ready());

    // A fake adb: records its arguments, answers like the real one.
    const fs::path dir = fs::temp_directory_path() / ("agr_adb_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    const fs::path calls = dir / "calls.txt", list = dir / "devices.txt";
    {
        std::ofstream sh(dir / "adb");
        sh << "#!/bin/sh\necho \"$@\" >> '" << calls.string() << "'\n"
           << "case \"$*\" in\n"
           << "  'devices -l') cat '" << list.string() << "' ;;\n"
           << "  *' install -r '*) echo 'Performing Streamed Install'; echo 'Success' ;;\n"
           << "  *' shell am start '*) echo 'Starting: Intent { cmp=x }' ;;\n"
           << "esac\n";
    }
    fs::permissions(dir / "adb", fs::perms::owner_all);
    const fs::path apk = dir / "Game-debug.apk";
    std::ofstream(apk) << "apk";
    AndroidToolchain tools;
    tools.adb = dir / "adb";
    auto setList = [&](const std::string& text) { std::ofstream(list) << "List of devices attached\n" << text; };
    auto readCalls = [&] {
        std::ifstream in(calls);
        return std::string((std::istreambuf_iterator<char>(in)), {});
    };

    setList("emulator-5554\tdevice product:x model:Pixel_7 transport_id:2\n");
    std::vector<std::string> log;
    REQUIRE(deployToAndroid(tools, apk, "com.studio.game", "", [&](const std::string& l) { log.push_back(l); }));
    const std::string c = readCalls();
    CHECK(c.find("-s emulator-5554 install -r " + apk.string()) != std::string::npos);
    CHECK(c.find("-s emulator-5554 shell am start -n com.studio.game/.AgrActivity") != std::string::npos);
    CHECK(c.find("install") < c.find("am start"));
    CHECK(std::find(log.begin(), log.end(), "Success") != log.end());
    CHECK(androidLogcatCommand(tools, "emulator-5554").find("-s 'emulator-5554' logcat -s AGR:V") != std::string::npos);

    // Several devices: a serial is needed, and must be ready.
    setList("A1\tdevice\nB2\tdevice\nC3\tunauthorized\n");
    CHECK_FALSE(deployToAndroid(tools, apk, "com.studio.game", ""));
    CHECK(deployToAndroid(tools, apk, "com.studio.game", "B2"));
    CHECK(readCalls().find("-s B2 install") != std::string::npos);
    CHECK_FALSE(deployToAndroid(tools, apk, "com.studio.game", "C3"));
    // Only an unauthorized phone: the error says what to do.
    setList("C3\tunauthorized\n");
    auto st = deployToAndroid(tools, apk, "com.studio.game", "");
    REQUIRE_FALSE(st);
    CHECK(st.error().message.find("USB debugging") != std::string::npos);
    setList("");
    CHECK_FALSE(deployToAndroid(tools, apk, "com.studio.game", ""));
    CHECK_FALSE(deployToAndroid(tools, dir / "missing.apk", "com.studio.game", ""));
    CHECK_FALSE(deployToAndroid(tools, apk, "not a package", ""));
    fs::remove_all(dir);
}
#endif

TEST_CASE("edit history: one step per change, drags merged, undo / redo, new branch clears redo") {
    EditHistory h(3);
    h.reset("a");
    CHECK_FALSE(h.canUndo());
    h.observe("a", false);  // unchanged
    CHECK_FALSE(h.canUndo());
    h.observe("b", false);
    h.observe("c", false);
    CHECK(h.undoCount() == 2);
    // A drag: many intermediate states while interacting, one step.
    h.observe("c1", true);
    h.observe("c2", true);
    h.observe("c3", true);
    h.observe("c3", false);
    CHECK(h.undoCount() == 3);
    CHECK(h.undo() == "c");
    CHECK(h.undo() == "b");
    CHECK(h.redo() == "c");
    CHECK(h.canRedo());
    h.observe("x", false);  // new edit: redo dropped
    CHECK_FALSE(h.canRedo());
    CHECK(h.undo() == "c");
    // Limit: the oldest steps are forgotten.
    h.reset("0");
    for (const char* s : {"1", "2", "3", "4", "5"}) h.observe(s, false);
    CHECK(h.undoCount() == 3);
    CHECK(h.undo() == "4");
    CHECK(h.undo() == "3");
    CHECK(h.undo() == "2");
    CHECK_FALSE(h.undo());
}

TEST_CASE("asset refactoring: renaming or moving an asset rewrites every reference in the project") {
    // Whole paths only; model sub-assets and folders.
    std::string t = R"({"a": "assets://tex/hero.png", "b": "assets://tex/hero.png2", "c": "assets://tex/hero.png#x", "d": "assets://tex/heroes.png"})";
    CHECK(rewriteAssetReferences(t, "assets://tex/hero.png", "assets://chars/hero.png", false) == 2);
    CHECK(t == R"({"a": "assets://chars/hero.png", "b": "assets://tex/hero.png2", "c": "assets://chars/hero.png#x", "d": "assets://tex/heroes.png"})");
    std::string f = "require(\"assets://kits/fsm/fsm.lua\") -- assets://kits/fsmx/a.lua assets://kits/fsm";
    CHECK(rewriteAssetReferences(f, "assets://kits/fsm", "assets://lib/fsm", true) == 1);
    CHECK(f == "require(\"assets://lib/fsm/fsm.lua\") -- assets://kits/fsmx/a.lua assets://kits/fsm");

    EditorFixture fx(ProjectMode::Mode2D);
    Project& project = fx.ctx->project();
    const fs::path assets = project.assetsDirectory();
    fs::create_directories(assets / "tex");
    fs::create_directories(assets / "scripts" / "lib");
    std::ofstream(assets / "tex" / "hero.png") << "png";
    std::ofstream(assets / "scripts" / "lib" / "util.lua") << "return {}";
    std::ofstream(assets / "scripts" / "player.lua") << "local U = require(\"assets://scripts/lib/util.lua\")\nreturn {}";
    std::ofstream(assets / "hero.agrmat") << R"({"albedoTexture": "assets://tex/hero.png"})";
    // A scene that uses the image, saved.
    Scene& scene = fx.ctx->scene();
    const auto e = scene.createEntity("Hero");
    scene.registry().emplace<SpriteRendererComponent>(e).texture = "assets://tex/hero.png";
    REQUIRE(fx.ctx->saveSceneAs("scenes/level.agrscene"));

    auto r = moveAsset(project.root(), assets, "tex/hero.png", "characters/hero_v2.png");
    REQUIRE(r);
    CHECK(fs::exists(assets / "characters" / "hero_v2.png"));
    CHECK_FALSE(fs::exists(assets / "tex" / "hero.png"));
    CHECK(r.value().references == 2);
    CHECK(r.value().updatedFiles.size() == 2);
    auto read = [](const fs::path& p) {
        std::ifstream in(p);
        return std::string((std::istreambuf_iterator<char>(in)), {});
    };
    CHECK(read(assets / "hero.agrmat").find("assets://characters/hero_v2.png") != std::string::npos);
    REQUIRE(fx.ctx->loadScene("scenes/level.agrscene"));
    const auto hero = fx.ctx->scene().findByName("Hero");
    REQUIRE_FALSE(hero.isNull());
    CHECK(fx.ctx->scene().registry().get<SpriteRendererComponent>(hero).texture == "assets://characters/hero_v2.png");

    // Find usages: the material (line 1) and the scene.
    const auto uses = findAssetUsages(project.root(), assets, "characters/hero_v2.png");
    REQUIRE(uses.size() == 2);
    bool material = false;
    for (const auto& u : uses) material |= u.file.filename() == "hero.agrmat" && u.lines == std::vector<int>{1};
    CHECK(material);
    CHECK(findAssetUsages(project.root(), assets, "taken_nowhere.png").empty());
    CHECK(findAssetUsages(project.root(), assets, "scripts/lib").size() == 1);  // a folder: player.lua requires a file in it
    // A folder: every path under it follows, scripts included.
    r = moveAsset(project.root(), assets, "scripts/lib", "shared/lib");
    REQUIRE(r);
    CHECK(read(assets / "scripts" / "player.lua").find("assets://shared/lib/util.lua") != std::string::npos);
    // Refusals: an existing destination, a folder into itself, outside the assets folder, a missing asset.
    std::ofstream(assets / "taken.png") << "x";
    CHECK_FALSE(moveAsset(project.root(), assets, "characters/hero_v2.png", "taken.png"));
    CHECK_FALSE(moveAsset(project.root(), assets, "shared", "shared/inner/shared"));
    CHECK_FALSE(moveAsset(project.root(), assets, "taken.png", "../outside.png"));
    CHECK_FALSE(moveAsset(project.root(), assets, "nope.png", "a.png"));
    CHECK(fs::exists(assets / "characters" / "hero_v2.png"));
}

TEST_CASE("UI builder group resize: the union box scaled from a corner keeps every element's relative place and size") {
    const std::vector<math::Vec4> rects{{10, 10, 20, 10}, {40, 30, 10, 20}};
    const math::Vec4 box = unionRect(rects);
    CHECK(box.x == doctest::Approx(10));
    CHECK(box.y == doctest::Approx(10));
    CHECK(box.z == doctest::Approx(40));
    CHECK(box.w == doctest::Approx(40));
    // Dragged the bottom-right corner: twice as wide, 1.5x as high, top-left fixed.
    const auto out = scaleRectsInBox(rects, box, {10, 10, 80, 60});
    CHECK(out[0].x == doctest::Approx(10));
    CHECK(out[0].z == doctest::Approx(40));
    CHECK(out[0].w == doctest::Approx(15));
    CHECK(out[1].x == doctest::Approx(70));
    CHECK(out[1].y == doctest::Approx(40));
    CHECK(out[1].z == doctest::Approx(20));
    CHECK(out[1].w == doctest::Approx(30));
    const math::Vec4 after = unionRect(out);
    CHECK(after.z == doctest::Approx(80));
    CHECK(after.w == doctest::Approx(60));
}

#if !defined(_WIN32)
TEST_CASE("Android device log: logcat lines parsed, each poll returns only the new lines") {
    double t = 0;
    std::string text;
    REQUIRE(AndroidLogReader::parseLine("  1700000000.250  4321  4340 I AGR     : [Script] hello", t, text));
    CHECK(t == doctest::Approx(1700000000.25));
    CHECK(text == "I AGR: [Script] hello");
    CHECK_FALSE(AndroidLogReader::parseLine("--------- beginning of main", t, text));

    const fs::path dir = fs::temp_directory_path() / ("agr_logcat_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    const fs::path log = dir / "log.txt", calls = dir / "calls.txt";
    {
        std::ofstream sh(dir / "adb");
        // A fake adb whose logcat prints the lines of log.txt at or after -T (all of them for -t 1: last one).
        sh << "#!/bin/sh\necho \"$@\" >> '" << calls.string() << "'\n"
           << "since=0; last=0\nwhile [ $# -gt 0 ]; do case \"$1\" in -T) since=$2; shift;; -t) last=1;; esac; shift; done\n"
           << "if [ $last = 1 ]; then tail -n 1 '" << log.string() << "'; exit 0; fi\n"
           << "awk -v s=\"$since\" '$1 >= s' '" << log.string() << "'\n";
    }
    fs::permissions(dir / "adb", fs::perms::owner_all);
    AndroidToolchain tools;
    tools.adb = dir / "adb";
    auto append = [&](const std::string& l) { std::ofstream(log, std::ios::app) << l << "\n"; };
    append("100.000 1 1 I AGR : old line from before");
    AndroidLogReader reader(tools, "emulator-5554");
    CHECK(reader.poll().empty());  // marks now
    append("101.000 1 1 I AGR : [Engine] started");
    append("101.000 1 1 W AGR : [Script] same time");
    auto lines = reader.poll();
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "I AGR: [Engine] started");
    CHECK(lines[1] == "W AGR: [Script] same time");
    CHECK(reader.poll().empty());  // nothing new (the -T bound repeats the last second's lines: dropped)
    append("102.500 1 1 E AGR : [Script] boom");
    lines = reader.poll();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "E AGR: [Script] boom");
    std::ifstream in(calls);
    const std::string c((std::istreambuf_iterator<char>(in)), {});
    CHECK(c.find("-s emulator-5554 logcat -d -v epoch -T 101.000") != std::string::npos);
    fs::remove_all(dir);
}
#endif

TEST_CASE("dedicated server export: no images nor sounds, flagged as a server; the real player runs it with no display and the script hosts") {
    EditorFixture f(ProjectMode::Mode2D);
    Project& project = f.ctx->project();
    const fs::path assets = project.assetsDirectory();
    fs::create_directories(assets / "scripts");
    std::ofstream(assets / "scripts" / "server.lua") << R"(
local S = {}
function S:onStart()
    if agr.isServer() then
        assert(agr.net.host(0, 8))
        agr.file.write("user://server.txt", "hosting " .. agr.net.port())
    else
        agr.file.write("user://server.txt", "client")
    end
end
return S
)";
    Scene& scene = f.ctx->scene();
    scene.registry().emplace<ScriptComponent>(scene.createEntity("Server")).script = "assets://scripts/server.lua";
    REQUIRE(f.ctx->saveSceneAs("scenes/main.agrscene"));
    project.settings().startupScene = "scenes/main.agrscene";
    project.settings().exportSettings.serverTickRate = 30;
#if defined(__linux__)
    const fs::path player = fs::read_symlink("/proc/self/exe").parent_path() / "agr_player";
#else
    const fs::path player;
#endif
    fs::path fake = f.root / "fake_player";
    { std::ofstream(fake) << "player"; }
    auto report = exportDedicatedServer(project, f.root / "out", fs::exists(player) ? player : fake);
    REQUIRE_MESSAGE(report, (report ? "" : report.error().message));
    const fs::path dir = report.value().output;
    CHECK(dir.filename().string().find("Server") != std::string::npos);
    int images = 0, sounds = 0, scripts = 0;
    for (const auto& e : fs::recursive_directory_iterator(dir)) {
        const std::string ext = e.path().extension().string();
        images += ext == ".png" && e.path().parent_path() != dir;  // the icon next to the executable is fine
        sounds += ext == ".wav" || ext == ".ogg";
        scripts += ext == ".lua";
    }
    CHECK(images == 0);
    CHECK(sounds == 0);
    CHECK(scripts >= 1);
    std::ifstream manifestIn(dir / "game" / "game.json");
    const std::string manifest((std::istreambuf_iterator<char>(manifestIn)), {});
    CHECK(manifest.find("\"dedicatedServer\": true") != std::string::npos);
    CHECK(manifest.find("\"serverTickRate\": 30") != std::string::npos);
#if defined(__linux__)
    if (fs::exists(player)) {
        // No DISPLAY: a server must not need one. 30 ticks per second, 60 ticks = about 2 s.
        const fs::path exe = dir / (GameManifest::safeName(project.settings().name + " Server"));
        const fs::path user = f.root / "server_user";
        const std::string cmd = "env -u DISPLAY -u WAYLAND_DISPLAY SDL_AUDIODRIVER=dummy '" + exe.string() + "' --frames 60 --user '" + user.string() + "' > '" +
                                (f.root / "server.log").string() + "' 2>&1";
        const auto start = std::chrono::steady_clock::now();
        CHECK(std::system(cmd.c_str()) == 0);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        CHECK_MESSAGE(seconds > 1.5, seconds);  // paced at the tick rate, not as fast as possible
        std::ifstream in(user / "server.txt");
        const std::string said((std::istreambuf_iterator<char>(in)), {});
        CHECK_MESSAGE(said.rfind("hosting ", 0) == 0, said);
    }
#endif
}

TEST_CASE("export presets: saved in the project with their own settings, reloaded") {
    EditorFixture f(ProjectMode::Mode2D);
    Project& project = f.ctx->project();
    ExportPreset store;
    store.name = "Play Store";
    store.target = "android-aab";
    store.settings = project.settings().exportSettings;
    store.settings.versionCode = 42;
    store.settings.abis = {"arm64-v8a", "armeabi-v7a"};
    ExportPreset demo;
    demo.name = "Démo PC";
    demo.target = "desktop";
    demo.output = "build/demo";
    demo.settings = project.settings().exportSettings;
    demo.settings.excludeFilter = "levels/secret/**";
    project.settings().exportPresets = {store, demo};
    REQUIRE(project.save());
    auto again = Project::open(project.root());
    REQUIRE(again);
    const auto& presets = again.value().settings().exportPresets;
    REQUIRE(presets.size() == 2);
    CHECK(presets[0].name == "Play Store");
    CHECK(presets[0].target == "android-aab");
    CHECK(presets[0].settings.versionCode == 42);
    CHECK(presets[0].settings.abis == std::vector<std::string>{"arm64-v8a", "armeabi-v7a"});
    CHECK(presets[1].name == "Démo PC");
    CHECK(presets[1].output == "build/demo");
    CHECK(presets[1].settings.excludeFilter == "levels/secret/**");
    CHECK(again.value().settings().exportSettings.versionCode != 42);  // the project's own settings are untouched
}

TEST_CASE("script docs: functions with signature and comment docs; references outside comments and strings") {
    const std::string player = R"(local Player = { properties = { speed = 4 } }

--- Makes the player jump.
-- height: in meters
function Player:jump(height, double)
    self.vy = height
end

local function helper(x) return x * 2 end  -- not documented

Player.spawn = function(where) end

function Player:onUpdate(dt)
    -- jump is also mentioned here, in a comment
    if agr.input.pressed("jump") then self:jump(helper(2)) end
    local text = [[ jump in a long string ]]
end
return Player
)";
    const auto symbols = scriptSymbols(player);
    const ScriptSymbol* jump = findSymbol(symbols, "jump");
    REQUIRE(jump);
    CHECK(jump->name == "Player:jump");
    CHECK(jump->signature == "Player:jump(height, double)");
    CHECK(jump->doc == "Makes the player jump.\nheight: in meters");
    CHECK(jump->line == 5);
    const ScriptSymbol* helper = findSymbol(symbols, "helper");
    REQUIRE(helper);
    CHECK(helper->doc.empty());
    const ScriptSymbol* spawn = findSymbol(symbols, "spawn");
    REQUIRE(spawn);
    CHECK(spawn->signature == "Player.spawn(where)");
    CHECK_FALSE(findSymbol(symbols, "nothing"));

    const std::string enemy = "local E = {}\nfunction E:onUpdate() agr.find('Player'):script():jump(3) end\nreturn E\n";
    const auto refs = findReferences({{"player.lua", player}, {"enemy.lua", enemy}}, "jump");
    // player.lua: the definition (line 5) and the call (line 15); not the comment, the "jump" action string, the long string.
    REQUIRE(refs.size() == 3);
    CHECK(refs[0].file == "player.lua");
    CHECK(refs[0].line == 5);
    CHECK(refs[0].definition);
    CHECK(refs[1].line == 15);
    CHECK_FALSE(refs[1].definition);
    CHECK(refs[2].file == "enemy.lua");
    CHECK(refs[2].line == 2);
    CHECK(findReferences({{"a.lua", "local jumper = 1 -- jump"}}, "jump").empty());  // whole words only
}
