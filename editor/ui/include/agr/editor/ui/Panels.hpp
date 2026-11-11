#pragma once
#include <atomic>
#include <deque>
#include <thread>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "agr/editor/ui/EditorShared.hpp"
#include "agr/assets/Material.hpp"
#include "agr/animation/AnimationSystem.hpp"

namespace agr::editor {

class HierarchyPanel {
public:
    void draw(EditorShared& s);

private:
    void drawEntity(EditorShared& s, ecs::Entity e);
    void contextMenu(EditorShared& s, ecs::Entity e);
    void createMenu(EditorShared& s, ecs::Entity parent);
    std::vector<Uuid> m_visibleOrder;  // for shift-click ranges
    std::string m_filter;
    Uuid m_renaming;
    std::string m_renameBuffer;
    bool m_renameFocus = false;
    Uuid m_rangeAnchor;
    Uuid m_pendingRangeEnd;
};

/// Sphere lit by a sun and a sky, showing the material the inspector edits.
class MaterialPreview {
public:
    /// Inside the device frame, before the UI pass: renders EditorShared::previewMaterial.
    void render(EditorShared& s);
    /// Texture to display (null until rendered once); `flipY` for OpenGL render targets.
    rhi::TextureHandle texture(EditorShared& s) const;
    void release(EditorShared& s);
    static constexpr u32 kSize = 256;

private:
    std::unique_ptr<Renderer> m_renderer;  // own frame data: the scene view renders in the same frame
    std::unique_ptr<Scene> m_scene;
    ecs::Entity m_sphere;
    rhi::RenderTargetHandle m_target;
    bool m_rendered = false;
};

class InspectorPanel {
public:
    void draw(EditorShared& s, const MaterialPreview& preview);

private:
    void drawMaterialAsset(EditorShared& s, const MaterialPreview& preview);
    std::string m_materialUri;  // material being edited
    MaterialData m_material;
    std::unordered_map<u64, math::Vec3> m_eulerCache;  // stable Euler angles while editing a rotation
    std::string m_componentFilter;
};

class ViewportPanel {
public:
    enum class GizmoOp { Translate, Rotate, Scale };

    /// Before the UI is built: applies the size requested last frame (render target).
    void beginFrame(EditorShared& s);
    void draw(EditorShared& s, const FrameTime& time);
    /// Inside the device frame, before the UI pass: renders the scene into the target.
    void renderScene(EditorShared& s);
    void release(EditorShared& s);

    rhi::TextureHandle texture(EditorShared& s) const;
    /// Screen position (window pixels) of a world point in the last drawn viewport.
    bool worldToScreen(EditorShared& s, math::Vec3 world, ImVec2& out) const;
    /// Window position of a point of the game UI (pixels of the view, as in UIRect::screenRect).
    bool uiToScreen(math::Vec2 uiPixels, ImVec2& out) const;
    bool gameView = false;
    GizmoOp gizmoOp = GizmoOp::Translate;
    bool gizmoLocal = false;
    bool snap = false;
    float snapTranslate = 0.5f, snapRotate = 15.0f, snapScale = 0.1f;
    bool showGrid = true;
    bool showIcons = true;
    bool showColliders = true;  ///< Outlines of 2D / 3D colliders (green; triggers in amber).
    bool showNavigation = false;  ///< Navigation mesh, blocked 2D grid cells and agents' paths.
    float flySpeed = 6.0f;
    /// Game view device preview: 0 = free (panel size), else a phone / tablet format with its
    /// aspect ratio and simulated safe area.
    int devicePreset = 0;
    bool simulateTouch = false;  ///< Play mode: the mouse acts as a finger.

private:
    void handleCamera(EditorShared& s, const FrameTime& time, bool hovered);
    void drawOverlays(EditorShared& s, ImVec2 origin, ImVec2 size);
    void drawColliders(EditorShared& s, ImVec2 origin, ImVec2 size, const math::Mat4& viewProj);
    void drawNavigation(EditorShared& s, ImVec2 origin, ImVec2 size, const math::Mat4& viewProj);
    /// Terrain brushes; true while the tool owns the mouse (no picking / gizmo).
    bool handleTerrainSculpting(EditorShared& s, ImVec2 origin, ImVec2 size, bool hovered);
    bool m_sculpting = false;
    bool m_sculptChanged = false;
    void drawGizmo(EditorShared& s, ImVec2 origin, ImVec2 size);
    void handlePicking(EditorShared& s, ImVec2 origin, ImVec2 size);
    void handleDrop(EditorShared& s, ImVec2 origin, ImVec2 size);
    /// Game view in edit mode: selects, moves and resizes UI elements (UI builder). Returns
    /// true while it owns the left mouse button (no picking, no gizmo).
    bool handleUIEditing(EditorShared& s, ImVec2 origin, ImVec2 size, bool hovered);
    /// Tilemap brush: returns true while it owns the left mouse button (no picking, no gizmo).
    bool handleTilemapPainting(EditorShared& s, ImVec2 origin, ImVec2 size, bool hovered);
    math::Mat4 viewMatrix(EditorShared& s) const;
    math::Mat4 projectionGL(EditorShared& s, float aspect) const;

    rhi::RenderTargetHandle m_target;
    u32 m_width = 0, m_height = 0;        // current render target
    u32 m_wantWidth = 0, m_wantHeight = 0; // requested by the UI
    bool m_visible = false;
    bool m_gizmoWasUsing = false;
    bool m_flying = false;
    ImVec2 m_pressPos{};
    bool m_pressed = false;
    std::vector<std::pair<ecs::Entity, ImVec2>> m_iconHits;
    bool m_painting = false;          // tilemap stroke in progress
    bool m_strokeChanged = false;
    std::pair<i32, i32> m_strokeStart{}, m_strokeLast{};
    ImVec2 m_lastOrigin{}, m_lastSize{};
    bool m_touching = false;
    // UI builder drag: 0 none, 1 move, 2 resize (m_uiHandle = edge signs)
    int m_uiDrag = 0;
    math::Vec2 m_uiHandle{0.0f, 0.0f};
    ImVec2 m_uiDragStart{};
    math::Vec2 m_uiStartPos{}, m_uiStartSize{};
    bool m_uiChanged = false;
};

class AssetBrowserPanel {
public:
    void draw(EditorShared& s);
    void importFiles(EditorShared& s, const std::vector<std::string>& paths);

private:
    void drawFolderTree(EditorShared& s, const std::string& folder);
    std::string m_filter;
    std::string m_pendingDelete;
    std::string m_renameTarget, m_renameBuffer;
    // "Slice as grid" dialog (image -> .agrsheet)
    std::string m_sliceTarget;
    int m_sliceW = 32, m_sliceH = 32, m_sliceMargin = 0, m_sliceSpacing = 0;
    bool m_slicePixelArt = true;
    // "Create atlas" dialog (images of the current folder -> one texture + .agrsheet)
    bool m_atlasOpen = false;
    std::string m_atlasName = "atlas";
    float m_atlasPpu = 100.0f;
    bool m_atlasPixelArt = false;
    float m_tileSize = 84.0f;
    std::mutex m_importMutex;
    std::vector<std::string> m_importQueue;  // filled by the native file dialog callback
};

class ConsolePanel {
public:
    void draw(EditorShared& s);

private:
    std::vector<RingBufferSink::Entry> m_entries;
    u64 m_seen = ~0ull;
    bool m_showInfo = true, m_showWarn = true, m_showError = true, m_showDebug = false;
    bool m_autoScroll = true;
    std::string m_filter;
};

/// Input actions (assets://input.agrinput): actions, their bindings, key / button capture,
/// live values while the game runs. Every change is saved.
class InputPanel {
public:
    void draw(EditorShared& s);

private:
    int m_selected = 0;
    int m_capture = -1;  ///< Binding index waiting for a key / button, -1 = none.
    std::string m_newName;
};

/// Localization table (assets://localization.csv): keys x languages, editable cells, preview
/// language for the edited scene's texts. Changes are saved when a cell is left.
class LocalizationPanel {
public:
    void draw(EditorShared& s);

private:
    std::string m_newKey, m_newLanguage, m_filter;
};

/// Audio mixer: output state, bus volumes and mutes, master level meter.
class MixerPanel {
public:
    void draw(EditorShared& s, const FrameTime& time);

private:
    float m_level[2] = {0.0f, 0.0f};
};

/// Animation timeline: tracks of a clip (.agranim) of the selected Animator, keys on a
/// time ruler, key properties, curve view, events, preview by scrubbing in edit mode.
class AnimationPanel {
public:
    void draw(EditorShared& s, const FrameTime& time);
    /// Restores the values changed by the preview (play mode, panel closed, other entity).
    void endPreview(EditorShared& s);

private:
    void loadClip(EditorShared& s, const std::string& uri);
    bool saveClip(EditorShared& s);
    void applyPreview(EditorShared& s);
    void keyTracks(EditorShared& s, bool selectedOnly);
    void addTrackPopup(EditorShared& s);

    std::string m_uri;          // clip being edited
    AnimationClip m_clip;
    bool m_dirty = false;
    Uuid m_entity;              // animated root
    float m_time = 0.0f;
    bool m_playing = false;
    bool m_previewing = false;
    std::vector<std::pair<AnimationTrack, float>> m_restore;  // values before the preview
    int m_track = -1;           // selected track
    int m_key = -1;             // selected key of the selected track
    int m_event = -1;           // selected event
    bool m_draggingKey = false, m_scrubbing = false;
    float m_zoom = 200.0f;      // pixels per second
    std::string m_newClipName = "clip";
    // add-track popup
    int m_addTarget = 0, m_addComponent = 0, m_addField = 0;
};

/// State machine (.agrctrl) graph of the selected Animator: states, transitions,
/// conditions, parameters.
class StateMachinePanel {
public:
    void draw(EditorShared& s);

private:
    void load(EditorShared& s, const std::string& uri);
    bool save(EditorShared& s);
    void drawProperties(EditorShared& s);

    std::string m_uri;
    AnimatorController m_ctrl;
    bool m_dirty = false;
    int m_state = -1, m_transition = -1;
    int m_dragState = -1;
    ImVec2 m_pan{240.0f, 50.0f};
    std::string m_newName = "controller";
};

/// Export window: target (PC folder, Android APK / AAB), game settings (identity, version,
/// orientation, Android levels and ABIs), signing (keystore creation, passwords asked here
/// and never stored), tool checks, and the export itself on a background thread with its log.
class ExportPanel {
public:
    ~ExportPanel();
    void draw(EditorShared& s, const std::string& editorDirectory);
    bool open = false;
    bool busy() const { return m_running.load(); }

private:
    void start(EditorShared& s, const std::string& editorDirectory);
    int m_target = 1;  // 0 PC, 1 Android debug APK, 2 Android release APK, 3 Android App Bundle
    std::string m_output;
    std::string m_storePassword, m_keyPassword;
    std::string m_newAlias = "game", m_newOwner, m_newPassword;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::mutex m_mutex;
    std::vector<std::string> m_log;  // guarded by m_mutex
    std::string m_result;            // guarded by m_mutex
    bool m_resultOk = false;
    std::string m_resultPath;
};

class StatsPanel {
public:
    void draw(EditorShared& s, const FrameTime& time);

private:
    std::vector<float> m_frameMs = std::vector<float>(180, 0.0f);
    usize m_head = 0;
};

} // namespace agr::editor
