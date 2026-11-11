#include "agr/editor/EditorContext.hpp"
#include "agr/scene/Prefab.hpp"
#include "agr/scene/Terrain.hpp"

#include <cctype>

#include "agr/animation/AnimationSystem.hpp"
#include "agr/audio/AudioComponents.hpp"
#include "agr/ui/UIComponents.hpp"

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
        case CreateKind::UICanvas: return "Canvas";
        case CreateKind::UIPanel: return "Panel";
        case CreateKind::UIText: return "Text";
        case CreateKind::UIButton: return "Button";
        case CreateKind::UISlider: return "Slider";
        case CreateKind::UIColumn: return "Column";
        case CreateKind::AudioSource: return "Audio Source";
        case CreateKind::Terrain: return "Terrain";
    }
    return "Entity";
}

EditorContext::EditorContext(Engine& engine) : m_engine(engine) {
    registerRenderComponents();
    registerSpriteAnimatorComponent();
    registerAudioComponents();
    registerUIComponents();
    registerAnimationComponents();
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
    m_engine.prefabs().clear();  // another project's prefabs share URIs
    m_engine.reloadGameSettings();  // input actions and texts of this project
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

ecs::Entity EditorContext::uiParentFor(ecs::Entity parent) {
    Scene& s = scene();
    auto& reg = s.registry();
    // Under the given parent if it is (inside) a canvas, else under the first canvas, else a new one.
    for (ecs::Entity p = parent; !p.isNull(); p = s.parent(p))
        if (reg.has<UICanvasComponent>(p)) return parent;
    ecs::Entity canvas;
    reg.view<UICanvasComponent>().each([&](ecs::Entity c, UICanvasComponent&) {
        if (canvas.isNull()) canvas = c;
    });
    if (!canvas.isNull()) return canvas;
    canvas = s.createEntity("Canvas");
    reg.emplace<UICanvasComponent>(canvas);
    return canvas;
}

ecs::Entity EditorContext::create(CreateKind kind, ecs::Entity parent) {
    Scene& s = scene();
    auto& reg = s.registry();
    const bool ui = kind >= CreateKind::UIPanel && kind <= CreateKind::UIColumn;
    if (ui) parent = uiParentFor(parent);
    const ecs::Entity e = s.createEntity(createKindName(kind), parent);
    auto uiRect = [&](math::Vec2 size) -> UIRectComponent& {
        auto& r = reg.emplace<UIRectComponent>(e);
        r.size = size;
        return r;
    };
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
        case CreateKind::Terrain: {
            auto& tc = reg.emplace<TerrainComponent>(e);
            // The project's terrain textures when it has them (projects created from the templates do).
            const char* layers[] = {"assets://terrain/grass.png", "assets://terrain/rock.png", "assets://terrain/dirt.png", "assets://terrain/sand.png"};
            std::string* slots[] = {&tc.layer0, &tc.layer1, &tc.layer2, &tc.layer3};
            for (int i = 0; i < 4; ++i)
                if (m_engine.vfs().exists(layers[i])) *slots[i] = layers[i];
            terrain::ensureData(tc);
            break;
        }
        case CreateKind::SpotLight:
            reg.emplace<SpotLightComponent>(e);
            if (parent.isNull()) t.position.y += 3.0f;
            t.rotation = math::Quat::fromEuler({math::radians(-90.0f), 0.0f, 0.0f});  // pointing down
            break;
        case CreateKind::UICanvas: reg.emplace<UICanvasComponent>(e); break;
        case CreateKind::UIPanel: {
            uiRect({400, 300});
            reg.emplace<UIImageComponent>(e).color = {0.08f, 0.09f, 0.11f, 0.85f};
            break;
        }
        case CreateKind::UIText: {
            uiRect({400, 60});
            reg.emplace<UITextComponent>(e);
            break;
        }
        case CreateKind::UIButton: {
            uiRect({260, 70});
            reg.emplace<UIImageComponent>(e).color = {0.98f, 0.55f, 0.16f, 1.0f};
            reg.emplace<UIButtonComponent>(e);
            const ecs::Entity label = s.createEntity("Label", e);
            auto& lr = reg.emplace<UIRectComponent>(label);
            lr.anchorMin = {0, 0};
            lr.anchorMax = {1, 1};
            lr.size = {0, 0};
            auto& lt = reg.emplace<UITextComponent>(label);
            lt.text = "Button";
            lt.color = {0.07f, 0.07f, 0.08f, 1.0f};
            break;
        }
        case CreateKind::UISlider: {
            uiRect({360, 36});
            reg.emplace<UISliderComponent>(e);
            break;
        }
        case CreateKind::UIColumn: {
            uiRect({320, 400});
            reg.emplace<UIImageComponent>(e).color = {0.08f, 0.09f, 0.11f, 0.6f};
            reg.emplace<UILayoutComponent>(e);
            break;
        }
        case CreateKind::AudioSource: reg.emplace<AudioSourceComponent>(e); break;
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
    // Animated model: an Animator playing its first clip ("<model>#<clip>").
    if (!model.animationNames.empty()) s.registry().emplace<AnimatorComponent>(root).clip = model.uri + "#" + model.animationNames.front();
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

void EditorContext::update() {
    m_selection.prune(scene());
    // Prefab files changed by another tool: refresh the open scene's instances (once a second).
    m_prefabPollTimer += 1.0 / 60.0;
    if (m_prefabPollTimer >= 1.0) {
        m_prefabPollTimer = 0.0;
        reloadChangedPrefabs();
    }
}

// ------------------------------------------------------------------ prefabs
Result<std::string> EditorContext::createPrefab(ecs::Entity e) {
    Scene& s = scene();
    if (!s.valid(e)) return makeError(Error::Code::InvalidArgument, "no entity");
    if (m_playState != PlayState::Edit) return makeError(Error::Code::InvalidArgument, "stop the game to create a prefab");
    // File name from the entity name, made unique.
    std::string base;
    for (char c : s.nameOf(e)) base += (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') ? c : '_';
    if (base.empty()) base = "prefab";
    std::string uri = "assets://prefabs/" + base + prefab::kExtension;
    for (int i = 2; m_engine.vfs().exists(uri); ++i) uri = "assets://prefabs/" + base + "_" + std::to_string(i) + prefab::kExtension;
    auto saved = m_engine.prefabs().save(uri, prefab::fromEntity(s, e));
    if (!saved) return saved.error();
    if (Status st = prefab::link(s, e, uri); !st) return st.error();
    commit("Create Prefab");
    return uri;
}

Result<ecs::Entity> EditorContext::instantiatePrefab(const std::string& uri, Vec3 position, ecs::Entity parent) {
    Scene& s = scene();
    auto e = prefab::instantiate(s, uri, parent);
    if (!e) return e.error();
    s.registry().get<TransformComponent>(e.value()).position = position;
    s.updateTransforms();
    m_selection.set(s.uuidOf(e.value()));
    commit("Add Prefab");
    return e.value();
}

Status EditorContext::applyPrefab(ecs::Entity root) {
    Scene& s = scene();
    if (m_playState != PlayState::Edit) return makeError(Error::Code::InvalidArgument, "stop the game to apply a prefab");
    const auto* pic = s.registry().tryGet<PrefabInstanceComponent>(root);
    if (!pic) return makeError(Error::Code::InvalidArgument, "not a prefab instance");
    const std::string uri = pic->prefab;
    auto content = prefab::applyContent(s, root);
    if (!content) return content.error();
    auto previous = m_engine.prefabs().save(uri, content.value());
    if (!previous) return previous.error();
    if (Status st = prefab::link(s, root, uri); !st) return st;
    prefab::refreshAll(s, uri, previous.value(), root);
    commit("Apply Prefab");
    return {};
}

Status EditorContext::revertPrefab(ecs::Entity root) {
    Status st = prefab::revert(scene(), root);
    if (st) commit("Revert Prefab");
    return st;
}

void EditorContext::unpackPrefab(ecs::Entity root) {
    if (!prefab::isInstanceRoot(scene(), root)) return;
    prefab::unpack(scene(), root);
    commit("Unpack Prefab");
}

u32 EditorContext::reloadChangedPrefabs() {
    const auto changed = m_engine.prefabs().reloadChanged();
    u32 n = 0;
    for (const auto& [uri, previous] : changed) n += prefab::refreshAll(scene(), uri, previous);
    if (n > 0 && m_playState == PlayState::Edit) commit("Prefab Updated");
    return n;
}

} // namespace agr::editor
