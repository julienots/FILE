#include "agr/editor/SceneTemplates.hpp"

#include "agr/render/RenderComponents.hpp"

namespace agr::editor {

using math::Quat;
using math::Vec3;

namespace {

void lookAt(Scene& scene, ecs::Entity e, Vec3 eye, Vec3 target) {
    auto& t = scene.registry().get<TransformComponent>(e);
    Vec3 s;
    math::decompose(math::inverse(math::Mat4::lookAt(eye, target, Vec3::up())), t.position, t.rotation, s);
}

ecs::Entity sun(Scene& scene) {
    const auto light = scene.createEntity("Sun Light");
    scene.registry().emplace<DirectionalLightComponent>(light).intensity = 1.1f;
    scene.registry().emplace<EnvironmentComponent>(light);
    scene.registry().get<TransformComponent>(light).rotation =
        Quat::fromEuler({math::radians(-50.0f), math::radians(35.0f), 0.0f});
    return light;
}

} // namespace

const char* templateName(SceneTemplate t) {
    switch (t) {
        case SceneTemplate::Empty2D: return "Scène 2D vide";
        case SceneTemplate::Empty25D: return "Scène 2.5D vide";
        case SceneTemplate::Empty3D: return "Scène 3D vide";
    }
    return "?";
}

const char* templateDescription(SceneTemplate t) {
    switch (t) {
        case SceneTemplate::Empty2D: return "Caméra orthographique et un sprite : base d'un jeu 2D (plateforme, puzzle, cartes...).";
        case SceneTemplate::Empty25D: return "Caméra perspective inclinée, sol 3D et sprites : base d'un jeu 2.5D (stratégie, tower defense...).";
        case SceneTemplate::Empty3D: return "Caméra, soleil, ambiance et sol texturé : base d'un jeu 3D.";
    }
    return "";
}

SceneTemplate defaultTemplateFor(ProjectMode mode) {
    switch (mode) {
        case ProjectMode::Mode2D: return SceneTemplate::Empty2D;
        case ProjectMode::Mode25D: return SceneTemplate::Empty25D;
        case ProjectMode::Mode3D: return SceneTemplate::Empty3D;
    }
    return SceneTemplate::Empty3D;
}

void buildSceneTemplate(Scene& scene, SceneTemplate tmpl) {
    registerRenderComponents();
    auto& reg = scene.registry();
    const char* mark = "assets://textures/agr_mark.png";
    const char* grid = "assets://textures/grid.png";
    switch (tmpl) {
        case SceneTemplate::Empty2D: {
            const auto cam = scene.createEntity("Main Camera");
            auto& c = reg.emplace<CameraComponent>(cam);
            c.projection = Projection::Orthographic;
            c.orthographicSize = 5.0f;
            reg.get<TransformComponent>(cam).position = {0.0f, 0.0f, 10.0f};
            const auto logo = scene.createEntity("AGR Mark");
            auto& s = reg.emplace<SpriteRendererComponent>(logo);
            s.texture = mark;
            s.size = {2.0f, 2.0f};
            break;
        }
        case SceneTemplate::Empty25D: {
            const auto cam = scene.createEntity("Main Camera");
            reg.emplace<CameraComponent>(cam).fieldOfView = math::radians(45.0f);
            lookAt(scene, cam, {0.0f, 9.0f, 9.0f}, {0.0f, 0.0f, 0.0f});
            sun(scene);
            const auto ground = scene.createEntity("Ground");
            reg.get<TransformComponent>(ground).scale = {20.0f, 1.0f, 20.0f};
            auto& gm = reg.emplace<MeshRendererComponent>(ground);
            gm.mesh = PrimitiveMesh::Plane;
            gm.texture = grid;
            gm.uvScale = {10.0f, 10.0f};
            const auto unit = scene.createEntity("Unit");
            reg.get<TransformComponent>(unit).position = {0.0f, 1.0f, 0.0f};
            auto& s = reg.emplace<SpriteRendererComponent>(unit);
            s.texture = mark;
            s.size = {2.0f, 2.0f};
            break;
        }
        case SceneTemplate::Empty3D: {
            const auto cam = scene.createEntity("Main Camera");
            reg.emplace<CameraComponent>(cam);
            lookAt(scene, cam, {0.0f, 3.0f, 7.0f}, {0.0f, 0.5f, 0.0f});
            sun(scene);
            const auto ground = scene.createEntity("Ground");
            reg.get<TransformComponent>(ground).scale = {20.0f, 1.0f, 20.0f};
            auto& gm = reg.emplace<MeshRendererComponent>(ground);
            gm.mesh = PrimitiveMesh::Plane;
            gm.texture = grid;
            gm.uvScale = {10.0f, 10.0f};
            const auto cube = scene.createEntity("Cube");
            reg.get<TransformComponent>(cube).position = {0.0f, 0.5f, 0.0f};
            reg.emplace<MeshRendererComponent>(cube).color = {0.96f, 0.65f, 0.14f, 1.0f};
            break;
        }
    }
    scene.updateTransforms();
}

} // namespace agr::editor
