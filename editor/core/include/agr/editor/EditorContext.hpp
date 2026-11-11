#pragma once
#include <optional>
#include <string>

#include "agr/editor/EditorCamera.hpp"
#include "agr/editor/Project.hpp"
#include "agr/editor/Selection.hpp"
#include "agr/editor/UndoStack.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/runtime/Engine.hpp"

namespace agr {
struct SpriteSheet;
struct ModelAsset;
}

namespace agr::editor {

enum class PlayState : u8 { Edit, Playing, Paused };

/// What "Create" menus can add.
enum class CreateKind : u8 {
    Empty, Cube, Sphere, Plane, Quad, Sprite, Camera, DirectionalLight, Environment,
    Tilemap, Particles2D, Light2D, ParallaxLayer, PointLight, SpotLight,
    UICanvas, UIPanel, UIText, UIButton, UISlider, UIColumn, AudioSource, Terrain, Water
};
const char* createKindName(CreateKind kind);

/// The editing session: open project, current scene, selection, undo history and
/// play mode. Every operation used by the UI lives here, so it is unit tested
/// without any UI.
class EditorContext {
public:
    explicit EditorContext(Engine& engine);

    Engine& engine() { return m_engine; }
    Scene& scene() { return m_engine.scene(); }
    Selection& selection() { return m_selection; }
    UndoStack& undoStack() { return m_undo; }
    EditorCamera& camera() { return m_camera; }

    // ------------------------------------------------------------- project
    /// Opens a project: mounts assets:// on its folder and loads the startup scene.
    Status openProject(Project project);
    bool hasProject() const { return m_project.has_value(); }
    Project& project() { return *m_project; }

    // -------------------------------------------------------------- scenes
    /// Project-relative path of the open scene ("scenes/main.agrscene"); empty when unsaved.
    const std::string& scenePath() const { return m_scenePath; }
    Status loadScene(const std::string& projectRelativePath);
    Status saveScene();                                   ///< To scenePath() (fails if empty)
    Status saveSceneAs(const std::string& projectRelativePath);
    void newScene(SceneTemplate tmpl);
    bool dirty() const { return m_undo.dirty(); }

    // ------------------------------------------------------------- editing
    /// Records the current scene in the undo history. Call after each user action.
    void commit(const std::string& label);
    bool undo();
    bool redo();

    ecs::Entity create(CreateKind kind, ecs::Entity parent = {});
    /// Parent for a new UI element: `parent` when it is inside a canvas, else the first
    /// canvas of the scene, else a new canvas.
    ecs::Entity uiParentFor(ecs::Entity parent);
    /// Sprite showing an image asset, placed at `position`. With `sheet` (the loaded
    /// .agrsheet `uri` points to), the sprite shows its first frame at native size and
    /// gets a SpriteAnimator playing the first clip when the sheet has clips.
    ecs::Entity createSprite(const std::string& uri, math::Vec3 position, ecs::Entity parent = {}, const SpriteSheet* sheet = nullptr);
    /// Adds a loaded model's hierarchy (root at `position`), selected, as one undo step.
    ecs::Entity instantiateModel(const ModelAsset& model, math::Vec3 position, ecs::Entity parent = {});
    void deleteSelection();
    void duplicateSelection();
    /// Creates an empty "Group" at the center of the selection and moves the selected
    /// entities under it (world positions kept). Also used as a folder in the hierarchy.
    ecs::Entity groupSelection();
    bool reparent(ecs::Entity child, ecs::Entity newParent, ecs::Entity before = {});
    void rename(ecs::Entity e, const std::string& name);
    /// Adds a reflected component by name (Inspector "Add Component").
    bool addComponent(ecs::Entity e, const std::string& componentName);
    bool removeComponent(ecs::Entity e, const std::string& componentName);
    void selectAll();

    // ----------------------------------------------------------- prefabs
    /// Saves `e` and its subtree as assets://prefabs/<name>.agrprefab (unique name) and makes
    /// `e` its first instance. Returns the prefab URI.
    Result<std::string> createPrefab(ecs::Entity e);
    /// New instance of a prefab at `position`, selected, one undo step.
    Result<ecs::Entity> instantiatePrefab(const std::string& uri, math::Vec3 position, ecs::Entity parent = {});
    /// Writes the instance's changes into its prefab and updates the other instances.
    Status applyPrefab(ecs::Entity instanceRoot);
    /// Drops the instance's changes (position and name kept).
    Status revertPrefab(ecs::Entity instanceRoot);
    void unpackPrefab(ecs::Entity instanceRoot);
    /// Reloads prefabs modified on disk and updates their instances (polled by update()).
    u32 reloadChangedPrefabs();

    /// Frames the selection with the editor camera.
    void focusSelection();

    // ----------------------------------------------------------- play mode
    PlayState playState() const { return m_playState; }
    /// Saves the edited scene in memory and starts the simulation.
    void play();
    void pause();   ///< Toggles pause while playing.
    /// Stops the simulation and restores the scene as it was before play().
    void stop();

    /// Called every frame by the editor application.
    void update();

private:
    Engine& m_engine;
    f64 m_prefabPollTimer = 0.0;
    std::optional<Project> m_project;
    std::string m_scenePath;
    Selection m_selection;
    UndoStack m_undo;
    EditorCamera m_camera;
    PlayState m_playState = PlayState::Edit;
    std::string m_playSnapshot;
    std::vector<Uuid> m_playSelection;
};

} // namespace agr::editor
