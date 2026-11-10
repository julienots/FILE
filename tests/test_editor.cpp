#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>

#include "agr/core/Uuid.hpp"
#include "agr/editor/EditorContext.hpp"
#include "agr/editor/Picking.hpp"
#include "agr/editor/SceneTemplates.hpp"
#include "agr/physics2d/Physics2DComponents.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/physics2d/Physics2DWorld.hpp"
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/script/ScriptEngine.hpp"
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
    scripts.setInput(&input);
    scripts.setPhysics2D(&physics);
    FrameTime time;
    time.deltaSeconds = 1.0 / 60.0;
    time.fixedDeltaSeconds = 1.0 / 60.0;
    auto frame = [&] {
        physics.step(scene, 1.0f / 60.0f);
        for (const auto& ev : physics.events()) scripts.dispatchCollision(scene, ev);
        scripts.update(scene, time);
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
    input.setKey(Key::Space, true);
    frame();
    input.setKey(Key::Space, false);
    float top = walked.y;
    for (int i = 0; i < 30; ++i) {
        frame();
        top = std::max(top, scene.registry().get<TransformComponent>(hero).position.y);
    }
    CHECK(top > walked.y + 3.0f);  // jumps more than 3 m
    CHECK(scripts.stats().errors == 0);
    CHECK(scripts.stats().faulted == 0);

    // 3D: Space throws a ball that the physics moves.
    f.ctx->newScene(SceneTemplate::Showcase3D);
    Scene& scene3 = f.scene();
    Physics3DWorld physics3;
    scripts.setPhysics3D(&physics3);
    const usize before = scene3.entityCount();
    scripts.update(scene3, time);
    input.setKey(Key::Space, true);
    scripts.update(scene3, time);
    input.beginFrame();
    input.setKey(Key::Space, false);
    scene3.updateTransforms();
    CHECK(scene3.entityCount() == before + 1);
    const ecs::Entity ball = scene3.findByName("Ball 1");
    REQUIRE_FALSE(ball.isNull());
    const Vec3 b0 = scene3.registry().get<TransformComponent>(ball).position;
    for (int i = 0; i < 30; ++i) {
        physics3.step(scene3, 1.0f / 60.0f);
        scripts.update(scene3, time);
        scene3.updateTransforms();
    }
    const Vec3 b1 = scene3.registry().get<TransformComponent>(ball).position;
    CHECK(b1.z < b0.z - 3.0f);  // flew toward the scene (camera looks down -Z)
    CHECK(physics3.stats().bodies > 15);
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
    REQUIRE(f.ctx->saveScene());
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
