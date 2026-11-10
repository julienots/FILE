#include <doctest/doctest.h>

#include <filesystem>

#include "agr/core/Uuid.hpp"
#include "agr/editor/EditorContext.hpp"
#include "agr/editor/Picking.hpp"
#include "agr/editor/SceneTemplates.hpp"
#include "agr/scene/SceneSerializer.hpp"

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
    for (SceneTemplate t : {SceneTemplate::Empty2D, SceneTemplate::Empty25D, SceneTemplate::Empty3D}) {
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
