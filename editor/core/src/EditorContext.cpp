#include "agr/editor/EditorContext.hpp"

#include <cmath>
#include <filesystem>

#include "agr/core/Log.hpp"
#include "agr/editor/Picking.hpp"
#include "agr/editor/SceneTemplates.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/scene/SceneSerializer.hpp"
#include "agr/twod/SpriteAnimator.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor {

using math::Vec3;

const char* createKindName(CreateKind kind) {
    switch (kind) {
        case CreateKind::Empty: return "Empty";
        case CreateKind::Cube: return "Cube";
        case CreateKind::Sphere: return "Sphere";
        case CreateKind::Plane: return "Plane";
        case CreateKind::Quad: return "Quad";
        case CreateKind::Sprite: return "Sprite";
        case CreateKind::Camera: return "Camera";
        case CreateKind::DirectionalLight: return "Directional Light";
        case CreateKind::Environment: return "Environment";
        case CreateKind::Tilemap: return "Tilemap";
        case CreateKind::Particles2D: return "Particles 2D";
        case CreateKind::Light2D: return "Light 2D";
        case CreateKind::ParallaxLayer: return "Parallax Layer";
        case CreateKind::PointLight: return "Point Light";
        case CreateKind::SpotLight: return "Spot Light";
    }
    return "Entity";
}

EditorContext::EditorContext(Engine& engine) : m_engine(engine) {
    registerRenderComponents();
    registerSpriteAnimatorComponent();
    m_engine.setTimeScale(0.0);  // edit mode: gameplay systems are frozen
    m_undo.reset(scene(), {});
}

// ------------------------------------------------------------------ project
Status EditorContext::openProject(Project project) {
    std::error_code ec;
    std::filesystem::create_directories(project.assetsDirectory(), ec);
    auto& vfs = m_engine.vfs();
    // In the editor, assets are writable (import, rename); games mount them read-only.
    vfs.mount("assets", std::make_unique<NativeFileProvider>(project.assetsDirectory().string(), false));
    vfs.mount("project", std::make_unique<NativeFileProvider>(project.root().string(), false));
    m_project = std::move(project);
    m_camera = EditorCamera{};
    m_camera.mode2D = m_project->settings().mode == ProjectMode::Mode2D;
    if (Status s = loadScene(m_project->settings().startupScene); !s) {
        AGR_LOG_WARN("Editor", "startup scene not loaded ({}); starting from a template", s.error().message);
        newScene(defaultTemplateFor(m_project->settings().mode));
    }
    AGR_LOG_INFO("Editor", "project '{}' opened ({})", m_project->settings().name, m_project->root().string());
    return {};
}

// ------------------------------------------------------------------- scenes
Status EditorContext::loadScene(const std::string& path) {
    if (m_playState != PlayState::Edit) stop();
    if (Status s = m_engine.loadScene("project://" + path); !s) return s;
    m_scenePath = path;
    m_selection.clear();
    scene().updateTransforms();
    m_undo.reset(scene(), {});
    return {};
}

Status EditorContext::saveScene() {
    if (m_scenePath.empty()) return makeError(Error::Code::InvalidArgument, "the scene has never been saved: use Save As");
    return saveSceneAs(m_scenePath);
}

Status EditorContext::saveSceneAs(const std::string& path) {
    if (m_playState != PlayState::Edit) return makeError(Error::Code::Unsupported, "stop the game before saving");
    std::string p = path;
    if (std::filesystem::path(p).extension() != ".agrscene") p += ".agrscene";
    if (Status s = m_engine.saveScene("project://" + p); !s) return s;
    m_scenePath = p;
    if (scene().name() == "Untitled") scene().setName(std::filesystem::path(p).stem().string());
    m_undo.markSaved();
    AGR_LOG_INFO("Editor", "scene saved: {}", p);
    return {};
}

void EditorContext::newScene(SceneTemplate tmpl) {
    if (m_playState != PlayState::Edit) stop();
    auto s = std::make_unique<Scene>("Untitled");
    buildSceneTemplate(*s, tmpl);
    m_engine.setScene(std::move(s));
    m_scenePath.clear();
    m_selection.clear();
    m_undo.reset(scene(), {});
}

// ------------------------------------------------------------------ editing
void EditorContext::commit(const std::string& label) {
    if (m_playState != PlayState::Edit) return;  // play-mode changes are discarded by stop()
    m_undo.commit(label, scene(), m_selection.ids());
}

bool EditorContext::undo() {
    if (m_playState != PlayState::Edit) return false;
    std::vector<Uuid> sel;
    if (!m_undo.undo(scene(), sel)) return false;
    m_selection.assign(std::move(sel));
    scene().updateTransforms();
    return true;
}

bool EditorContext::redo() {
    if (m_playState != PlayState::Edit) return false;
    std::vector<Uuid> sel;
    if (!m_undo.redo(scene(), sel)) return false;
    m_selection.assign(std::move(sel));
    scene().updateTransforms();
    return true;
}

ecs::Entity EditorContext::create(CreateKind kind, ecs::Entity parent) {
    Scene& s = scene();
    auto& reg = s.registry();
    const ecs::Entity e = s.createEntity(createKindName(kind), parent);
    auto& t = reg.get<TransformComponent>(e);
    if (parent.isNull()) {
        t.position = m_camera.mode2D ? Vec3{m_camera.position2D.x, m_camera.position2D.y, 0.0f} : m_camera.target;
    }
    switch (kind) {
        case CreateKind::Empty: break;
        case CreateKind::Cube: reg.emplace<MeshRendererComponent>(e).mesh = PrimitiveMesh::Cube; break;
        case CreateKind::Sphere: reg.emplace<MeshRendererComponent>(e).mesh = PrimitiveMesh::Sphere; break;
        case CreateKind::Plane: reg.emplace<MeshRendererComponent>(e).mesh = PrimitiveMesh::Plane; break;
        case CreateKind::Quad: reg.emplace<MeshRendererComponent>(e).mesh = PrimitiveMesh::Quad; break;
        case CreateKind::Sprite: reg.emplace<SpriteRendererComponent>(e); break;
        case CreateKind::Camera: {
            bool primaryExists = false;
            reg.view<CameraComponent>().each([&](ecs::Entity, CameraComponent& c) { primaryExists |= c.primary; });
            auto& cam = reg.emplace<CameraComponent>(e);
            cam.primary = !primaryExists;  // only one primary camera renders the game
            if (m_camera.mode2D) {
                cam.projection = Projection::Orthographic;
                t.position.z = 10.0f;
            }
            break;
        }
        case CreateKind::DirectionalLight:
            reg.emplace<DirectionalLightComponent>(e);
            t.rotation = math::Quat::fromEuler({math::radians(-50.0f), math::radians(35.0f), 0.0f});
            break;
        case CreateKind::Environment: reg.emplace<EnvironmentComponent>(e); break;
        case CreateKind::Tilemap: {
            // Cells stay aligned on the grid; the tileset is chosen in the inspector.
            auto& tm = reg.emplace<TilemapComponent>(e);
            tilemapEnsureLayout(tm);
            if (parent.isNull())
                t.position = {std::floor(t.position.x - static_cast<float>(tm.width) * 0.5f),
                              std::floor(t.position.y - static_cast<float>(tm.height) * 0.5f), 0.0f};
            break;
        }
        case CreateKind::Particles2D: reg.emplace<ParticleEmitter2DComponent>(e); break;
        case CreateKind::Light2D: reg.emplace<Light2DComponent>(e); break;
        case CreateKind::ParallaxLayer: reg.emplace<ParallaxComponent>(e); break;
        case CreateKind::PointLight:
            reg.emplace<PointLightComponent>(e);
            if (parent.isNull()) t.position.y += 2.0f;
            break;
        case CreateKind::SpotLight:
            reg.emplace<SpotLightComponent>(e);
            if (parent.isNull()) t.position.y += 3.0f;
            t.rotation = math::Quat::fromEuler({math::radians(-90.0f), 0.0f, 0.0f});  // pointing down
            break;
    }
    s.updateTransforms();
    m_selection.set(s.uuidOf(e));
    commit(std::string("Create ") + createKindName(kind));
    return e;
}

ecs::Entity EditorContext::createSprite(const std::string& uri, Vec3 position, ecs::Entity parent, const SpriteSheet* sheet) {
    Scene& s = scene();
    const std::string name = std::filesystem::path(uri.substr(uri.find("://") == std::string::npos ? 0 : uri.find("://") + 3)).stem().string();
    const ecs::Entity e = s.createEntity(name.empty() ? "Sprite" : name, parent);
    s.registry().get<TransformComponent>(e).position = position;
    {
        auto& sr = s.registry().emplace<SpriteRendererComponent>(e);
        if (sheet && !sheet->frames.empty()) {
            sr.sheet = uri;
            sr.frame = 0;
            sr.pixelArt = sheet->pixelArt;
            sr.size = {static_cast<float>(sheet->frames[0].w) / sheet->pixelsPerUnit,
                       static_cast<float>(sheet->frames[0].h) / sheet->pixelsPerUnit};
        } else {
            sr.texture = uri;
        }
    }  // the reference above is invalidated by the next emplace
    if (sheet && !sheet->clips.empty()) s.registry().emplace<SpriteAnimatorComponent>(e).clip = sheet->clips.front().name;
    s.updateTransforms();
    m_selection.set(s.uuidOf(e));
    commit("Create Sprite");
    return e;
}

ecs::Entity EditorContext::instantiateModel(const ModelAsset& model, Vec3 position, ecs::Entity parent) {
    Scene& s = scene();
    const ecs::Entity root = agr::instantiateModel(s, model, parent);
    if (root.isNull()) return root;
    s.registry().get<TransformComponent>(root).position = position;
    s.updateTransforms();
    m_selection.set(s.uuidOf(root));
    commit("Add Model");
    return root;
}

void EditorContext::deleteSelection() {
    Scene& s = scene();
    const auto entities = m_selection.entities(s);
    if (entities.empty()) return;
    for (ecs::Entity e : entities) s.destroyEntity(e);  // destroying a parent first is fine
    m_selection.clear();
    commit(entities.size() == 1 ? "Delete" : "Delete " + std::to_string(entities.size()) + " entities");
}

void EditorContext::duplicateSelection() {
    Scene& s = scene();
    std::vector<Uuid> copies;
    for (ecs::Entity e : m_selection.entities(s)) {
        bool ancestorSelected = false;
        for (ecs::Entity p = s.parent(e); !p.isNull(); p = s.parent(p)) ancestorSelected |= m_selection.contains(s.uuidOf(p));
        if (ancestorSelected) continue;  // duplicated with its ancestor
        copies.push_back(s.uuidOf(s.duplicate(e)));
    }
    if (copies.empty()) return;
    m_selection.assign(copies);
    s.updateTransforms();
    commit("Duplicate");
}

ecs::Entity EditorContext::groupSelection() {
    Scene& s = scene();
    std::vector<ecs::Entity> roots;  // selected entities whose ancestors are not selected
    for (ecs::Entity e : m_selection.entities(s)) {
        bool ancestorSelected = false;
        for (ecs::Entity p = s.parent(e); !p.isNull(); p = s.parent(p)) ancestorSelected |= m_selection.contains(s.uuidOf(p));
        if (!ancestorSelected) roots.push_back(e);
    }
    if (roots.empty()) return {};
    // Common parent when they all share one, otherwise the scene root.
    ecs::Entity parent = s.parent(roots.front());
    for (ecs::Entity e : roots)
        if (s.parent(e) != parent) parent = {};
    math::Vec3 center{};
    for (ecs::Entity e : roots) center += s.worldMatrix(e).translation();
    center = center / static_cast<float>(roots.size());
    const ecs::Entity group = s.createEntity("Group", parent);
    s.setParent(group, parent, roots.front(), false);  // where the first item was
    s.setWorldMatrix(group, math::Mat4::translate(center));
    for (ecs::Entity e : roots) s.setParent(e, group, {}, true);
    s.updateTransforms();
    m_selection.set(s.uuidOf(group));
    commit("Group");
    return group;
}

bool EditorContext::reparent(ecs::Entity child, ecs::Entity newParent, ecs::Entity before) {
    if (!scene().setParent(child, newParent, before, true)) return false;
    scene().updateTransforms();
    commit("Reparent");
    return true;
}

void EditorContext::rename(ecs::Entity e, const std::string& name) {
    if (!scene().valid(e) || name.empty()) return;
    scene().registry().get<NameComponent>(e).name = name;
    commit("Rename");
}

bool EditorContext::addComponent(ecs::Entity e, const std::string& name) {
    const ComponentInfo* info = ComponentRegistry::instance().find(name);
    if (!info || !scene().valid(e)) return false;
    auto& pool = scene().registry().ensurePool(info->id, info->makePool);
    if (pool.contains(e) || !pool.emplaceDefault(scene().registry(), e)) return false;
    commit("Add " + name);
    return true;
}

bool EditorContext::removeComponent(ecs::Entity e, const std::string& name) {
    const ComponentInfo* info = ComponentRegistry::instance().find(name);
    if (!info) return false;
    ecs::PoolBase* pool = scene().registry().findPool(info->id);
    if (!pool || !pool->contains(e)) return false;
    pool->remove(scene().registry(), e);
    commit("Remove " + name);
    return true;
}

void EditorContext::selectAll() {
    std::vector<Uuid> all;
    scene().traverse([&](ecs::Entity e, u32) { all.push_back(scene().uuidOf(e)); });
    m_selection.assign(std::move(all));
}

void EditorContext::focusSelection() {
    math::Aabb box;
    for (ecs::Entity e : m_selection.entities(scene())) {
        const math::Aabb b = worldBounds(scene(), e, {m_engine.service<SpriteSheetLibrary>(), m_engine.service<ModelLibrary>()});
        if (b.valid()) {
            box.expand(b.min);
            box.expand(b.max);
        }
    }
    m_camera.focus(box);
}

// ---------------------------------------------------------------- play mode
void EditorContext::play() {
    if (m_playState == PlayState::Paused) {
        pause();
        return;
    }
    if (m_playState != PlayState::Edit) return;
    m_playSnapshot = SceneSerializer::toString(scene(), -1);
    m_playSelection = m_selection.ids();
    m_playState = PlayState::Playing;
    m_engine.setTimeScale(1.0);
    AGR_LOG_INFO("Editor", "play");
}

void EditorContext::pause() {
    if (m_playState == PlayState::Playing) {
        m_playState = PlayState::Paused;
        m_engine.setTimeScale(0.0);
    } else if (m_playState == PlayState::Paused) {
        m_playState = PlayState::Playing;
        m_engine.setTimeScale(1.0);
    }
}

void EditorContext::stop() {
    if (m_playState == PlayState::Edit) return;
    if (Status s = SceneSerializer::fromString(scene(), m_playSnapshot); !s)
        AGR_LOG_ERROR("Editor", "could not restore the edited scene: {}", s.error().message);
    m_selection.assign(m_playSelection);
    m_playState = PlayState::Edit;
    m_engine.setTimeScale(0.0);
    scene().updateTransforms();
    AGR_LOG_INFO("Editor", "stop: scene restored");
}

void EditorContext::update() { m_selection.prune(scene()); }

} // namespace agr::editor
