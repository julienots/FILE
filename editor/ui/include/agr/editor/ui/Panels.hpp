#pragma once
#include <future>
#include <optional>
#include <atomic>
#include <optional>
#include <filesystem>
#include <set>
#include <deque>
#include <thread>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "agr/editor/ui/EditorShared.hpp"
#include "agr/assets/Material.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/script/LogicGraph.hpp"
#include "agr/script/ScriptEngine.hpp"
#include "agr/render/ParticleCurves.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/assets/Image.hpp"
#include "agr/assets/ShaderGraph.hpp"
#include "agr/editor/Package.hpp"
#include "agr/editor/PackageRepository.hpp"
#include "agr/core/Profiler.hpp"
#include <algorithm>

struct ImGuiWindow;  // imgui_internal.h

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
    /// Inside the device frame, before the UI pass: renders `material` (nothing when empty).
    void render(EditorShared& s, const std::string& material);
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

/// `.agrshader` code editor: line numbers, live preview (scene and preview sphere) while
/// typing, compile errors with their line (click to jump), save / revert.
class ShaderEditorPanel {
public:
    void draw(EditorShared& s, const MaterialPreview& preview);
    const std::string& uri() const { return m_uri; }
    const Renderer::ShaderStatus& status() const { return m_status; }
    /// Replaces the code as if typed (automated tests); `compileNow` skips the typing delay.
    void edit(EditorShared& s, std::string text, bool compileNow);
    /// Discards unsaved changes (reloads the file).
    void revertChanges(EditorShared& s) { revert(s); }
    static constexpr const char* kPreviewMaterial = "memory://shader_preview.agrmat";

private:
    enum class After : u8 { None, Open, Close, Action };
    void open(EditorShared& s, const std::string& uri);
    void save(EditorShared& s);
    void revert(EditorShared& s);
    void compile(EditorShared& s);
    void drawCode(EditorShared& s, float height);
    void drawUnsavedPopup(EditorShared& s);
    std::string m_uri, m_text, m_saved;
    Renderer::ShaderStatus m_status;
    bool m_pendingCompile = false;
    f64 m_editTime = 0.0;
    int m_gotoLine = 0;          // 1-based line to move the cursor to (0 = none)
    int m_cursorLine = 1;
    bool m_followCursor = false;
    After m_after = After::None;
    std::string m_afterUri;
    bool m_openPopup = false;
};

/// Built-in Lua editor (ScriptEditorPanel.cpp): colors, syntax check, completion from the live API.
class ScriptEditorPanel {
public:
    void draw(EditorShared& s);
    const std::string& uri() const { return m_uri; }
    const std::optional<ScriptEngine::SyntaxError>& error() const { return m_error; }
    const std::vector<ScriptEngine::SyntaxError>& warnings() const { return m_warnings; }
    /// Replaces the code as if typed (automated tests) and checks it at once.
    void edit(EditorShared& s, std::string text);
    bool save(EditorShared& s);
    struct Token {
        usize begin, end;
        ImU32 color;
    };
    static std::vector<Token> tokenize(const std::string& text);
    /// Completion candidates at `cursor` (byte offset); wordStart = start of the word replaced.
    static std::vector<std::string> suggestions(EditorShared& s, const std::string& text, int cursor, int& wordStart);

private:
    void open(EditorShared& s, const std::string& uri);
    void check(EditorShared& s);
    void drawCode(EditorShared& s, float height);
    std::string m_uri, m_text, m_saved, m_afterUri, m_clickedSuggestion;
    std::optional<ScriptEngine::SyntaxError> m_error;
    std::vector<ScriptEngine::SyntaxError> m_warnings;  ///< Static checks (unknown names), when it compiles.
    bool m_pendingCheck = false, m_askSave = false, m_followCursor = false, m_refocus = false;
    bool m_hideUntilEdit = false, m_suggestionsDirty = false;
    f64 m_editTime = 0.0;
    int m_gotoLine = 0, m_cursorLine = 1, m_cursorCol = 0, m_cursorPos = 0, m_wordStart = 0, m_selected = 0, m_lastLength = -1;
    std::vector<std::string> m_suggestions;
    // Find / replace (Ctrl+F) in the file, find in every script of the project (Ctrl+Maj+F).
    bool m_findOpen = false, m_findFocus = false, m_findInFiles = false;
    std::string m_find, m_replace;
    int m_selectStart = -1, m_selectEnd = -1;  // selection to apply in the next callback
    int m_gotoAfterOpen = 0;                    // line to show once the requested file is open
    struct FileHit {
        std::string uri;
        int line;
        std::string text;
    };
    std::vector<FileHit> m_fileHits;
    void findNext(EditorShared& s, bool backwards);
    void searchFiles(EditorShared& s);

    // Tabs: the active file lives in m_uri / m_text / m_saved; the others are kept here.
    struct OpenTab {
        std::string uri, text, saved;
        int cursorLine = 1;
    };
    std::vector<OpenTab> m_tabs;
    int m_activeTab = -1, m_closeTab = -1;
    bool m_selectTab = false;  ///< The active tab changed from code: select it in the tab bar.
    void stashActive();
    void activateTab(EditorShared& s, int index);
    void closeTab(EditorShared& s, int index);
    void drawTabs(EditorShared& s);

public:
    /// Tabs (automated tests).
    usize tabCount() const { return m_tabs.size(); }
    /// Functions defined in a Lua text: name and 1-based line ("function a.b:c(" or "x = function(").
    struct Definition {
        std::string name;
        int line;
    };
    static std::vector<Definition> definitions(const std::string& text);
    /// Goes to the definition of `word` (this file first, then every script); false if none.
    bool gotoDefinition(EditorShared& s, const std::string& word);
    int pendingLine() const { return m_gotoLine; }
    /// Find / replace helpers (automated tests).
    int replaceAll(EditorShared& s, const std::string& what, const std::string& with);
    const std::vector<FileHit>& searchProject(EditorShared& s, const std::string& what) {
        m_find = what;
        searchFiles(s);
        return m_fileHits;
    }
};

/// Touch input for the panels: long press = right click, vertical drag = scroll.
/// Call after ImGui::NewFrame (mobile mode).
class TouchSupport {
public:
    void update();
    float longPressSeconds = 0.55f;
    u32 longPresses = 0;  ///< Count (automated tests).

private:
    bool m_longPressed = false, m_scrolling = false;
    int m_rightPhase = 0;
    ::ImGuiWindow* m_scrollWindow = nullptr;
};
/// Larger scrollbars, grabs, paddings and touch margins (mobile mode).
void applyTouchStyle(float uiScale);

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
    // Touch gestures (two fingers: orbit / pan, pinch: zoom, three fingers: pan in 3D).
    bool m_multiTouch = false, m_hadTwo = false;
    math::Vec2 m_touchCenter{};
    float m_touchDistance = 0.0f;
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
    std::vector<std::pair<Uuid, math::Vec2>> m_uiGroupStart;  ///< Other selected UI elements moved along.
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
    void drawEffects(EditorShared& s, AudioSystem& audio);
    float m_level[2] = {0.0f, 0.0f};
    int m_bus = 2;  ///< Bus whose effects are shown (Effects).
    bool m_dirty = false;
};

/// Animation timeline: tracks of a clip (.agranim) of the selected Animator, keys on a
/// time ruler, key properties, curve view, events, preview by scrubbing in edit mode.
class AnimationPanel {
public:
    void draw(EditorShared& s, const FrameTime& time);
    /// Restores the values changed by the preview (play mode, panel closed, other entity).
    void endPreview(EditorShared& s);
    /// The timeline is showing animated values in the scene (edit mode).
    bool previewing() const { return m_previewing; }

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
    std::vector<KeyRef> m_keySel;         // selected keys (Ctrl+click, Ctrl+A)
    std::vector<CopiedKey> m_keyClipboard;
    float m_dragStartTime = 0.0f, m_dragApplied = 0.0f;
    int m_curveDrag = 0;              // curve editor: 0 none, 1 key, 2 in handle, 3 out handle
    float m_curveLo = 0.0f, m_curveHi = 1.0f;  // value range frozen while dragging
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
    void drawLayerBar(EditorShared& s);
    /// States, transitions and entry of the layer being edited (-1: the base layer).
    std::vector<AnimatorState>& states() { return m_layer < 0 ? m_ctrl.states : m_ctrl.layers[static_cast<usize>(m_layer)].states; }
    std::vector<AnimatorTransition>& transitions() {
        return m_layer < 0 ? m_ctrl.transitions : m_ctrl.layers[static_cast<usize>(m_layer)].transitions;
    }
    std::string& entry() { return m_layer < 0 ? m_ctrl.entry : m_ctrl.layers[static_cast<usize>(m_layer)].entry; }

    std::string m_uri;
    AnimatorController m_ctrl;
    int m_layer = -1;
    std::string m_maskText;
    bool m_dirty = false;
    int m_state = -1, m_transition = -1;
    int m_dragState = -1;
    ImVec2 m_pan{240.0f, 50.0f};
    std::string m_newName = "controller";
};

/// Plugins and packages: engine plugins (unload / reload dynamic ones), project plugins,
/// installed packages, local repository, package creation.
class PackagesPanel {
public:
    void draw(EditorShared& s);
    /// Loads the project's plugins listed in its settings (called when a project opens).
    void loadProjectPlugins(EditorShared& s);

private:
    std::filesystem::path repository(EditorShared& s) const;
    void drawPlugins(EditorShared& s);
    void drawInstalled(EditorShared& s);
    void drawRepository(EditorShared& s);
    void drawCreate(EditorShared& s);
    void drawOnline(EditorShared& s);
    std::string m_onlineUrl;
    std::string m_onlineFilter;
    std::future<Result<OnlineIndex>> m_indexJob;
    std::optional<OnlineIndex> m_online;
    std::string m_onlineError;
    std::future<Result<std::vector<std::string>>> m_onlineInstall;
    std::string m_onlineResult;

    std::string m_uninstall;
    UninstallPlan m_uninstallPlan;
    bool m_removeModified = false;
    std::vector<RepositoryEntry> m_repo;
    bool m_repoScanned = false;
    std::filesystem::path m_installPath;
    InstallPlan m_installPlan;
    std::string m_installError;
    bool m_replaceChanged = false;
    PackageManifest m_new{"mon-paquet", "1.0.0", "", "", {}, {}, ""};
    std::string m_deps;
    std::set<std::string> m_selection;
};

/// Names of the 32 collision layers (assets://physics_layers.json), saved on each change.
class PhysicsLayersPanel {
public:
    void draw(EditorShared& s);
};

/// CPU profiler: scope tree (phases, plugins, scripts, renderer, agr.profile) and history.
class ProfilerPanel {
public:
    void draw(EditorShared& s);  ///< Samples every frame, draws when visible.
    usize rowCount() const { return m_rows.size(); }
    bool hasScope(const std::string& name) const {
        return std::any_of(m_rows.begin(), m_rows.end(), [&](const auto& r) { return r.name == name; });
    }

private:
    std::vector<Profiler::ScopeStats> m_rows;
    std::vector<float> m_history = std::vector<float>(240, 0.0f);
    usize m_head = 0;
    bool m_paused = false;
    std::string m_filter;
};

/// Lua script debugger: breakpoints (click the line numbers), pause, and when a script
/// stops: the code at the current line, the call stack, the variables of each frame,
/// watch expressions and continue / step over / into / out / stop.
class ScriptDebuggerPanel {
public:
    /// Normal frames (game running or not): keeps the engine's breakpoints in sync.
    void draw(EditorShared& s);
    /// A script stopped: draws the break view; returns the action once chosen.
    std::optional<ScriptDebugAction> drawBreak(EditorShared& s, const ScriptBreak& b);
    void beginBreak(const ScriptBreak& b);
    void toggleBreakpoint(const std::string& uri, int line);
    bool hasBreakpoint(const std::string& uri, int line) const { return m_breakpoints.contains({uri, line}); }
    usize breakpointCount() const { return m_breakpoints.size(); }
    bool breakOnError = true;

private:
    void sync(ScriptEngine* scripts);
    void openFile(EditorShared& s, const std::string& uri);
    /// Code view with clickable line numbers; `current` is highlighted and scrolled to.
    void drawSource(EditorShared& s, int current, float height);

    std::set<std::pair<std::string, int>> m_breakpoints;
    ScriptEngine* m_synced = nullptr;
    bool m_dirty = true;
    std::string m_file;
    std::vector<std::string> m_lines;
    i64 m_fileTime = 0;
    std::string m_openInput;
    usize m_frame = 0;  ///< Selected stack frame.
    bool m_scroll = false;
    std::vector<std::string> m_watches;
    std::string m_watchInput;
};

/// Particle effects: presets (fire, smoke, explosion...), over-lifetime size curve and color /
/// alpha gradient (.agrparticle, previewed live), flipbook, main emitter settings, restart.
class ParticlePanel {
public:
    void draw(EditorShared& s, const std::string& templateAssetsDir);

private:
    bool curveEditor(std::vector<ParticleCurves::Key>& keys);
    bool gradientEditor(std::vector<ParticleCurves::ColorKey>& keys);

    std::string m_uri;             ///< Curves being edited.
    ParticleCurves m_curves;
    bool m_dirty = false;
    int m_dragKey = -1, m_selectedStop = 0, m_dragStop = -1;
    bool m_pendingCommit = false;
};

/// Sprite sheet editor (.agrsheet): the texture with its frames (zoom, pan), frames drawn,
/// moved and resized with the mouse, grid and automatic (transparency) slicing, trimming,
/// pivots, clips with a live preview.
class SpriteSheetPanel {
public:
    void draw(EditorShared& s);
    bool open(EditorShared& s, const std::string& uri);
    bool save(EditorShared& s);
    /// Replaces the frames by the islands of opaque pixels; false when the image cannot be read.
    bool autoSlice(EditorShared& s, int alphaThreshold, int minSize, int mergeDistance);
    const SpriteSheet& sheet() const { return m_sheet; }
    const std::string& uri() const { return m_uri; }
    bool dirty() const { return m_dirty; }

private:
    bool loadPixels(EditorShared& s);
    void drawCanvas(EditorShared& s);
    void drawProperties(EditorShared& s);
    void trimFrame(SpriteFrame& f) const;

    std::string m_uri;
    SpriteSheet m_sheet;
    bool m_dirty = false;
    int m_selected = -1;
    float m_zoom = 0.0f;  ///< 0: fit on the next draw.
    float m_panX = 0.0f, m_panY = 0.0f;
    enum class Drag { None, Create, Move, Resize, Pan } m_drag = Drag::None;
    float m_dragX = 0.0f, m_dragY = 0.0f;  ///< Texel where the drag started.
    SpriteFrame m_dragOrigin;
    Image m_pixels;  ///< Decoded texture (slicing, trimming), loaded on demand.
    std::string m_pixelsUri;
    int m_gridW = 32, m_gridH = 32, m_margin = 0, m_spacing = 0;
    int m_alpha = 8, m_minSize = 2, m_merge = 0;
    int m_clip = -1;
    std::string m_clipFrames;  ///< Text of the selected clip's frames.
    std::string m_status;
};

/// Integrated documentation: docs/*.md rendered as Markdown, full-text search, links.
class DocsPanel {
public:
    void setDirectory(const std::string& docsDir) {
        m_dir = docsDir;
        m_scanned = false;
    }
    void draw(EditorShared& s);
    bool openDoc(const std::string& file);  ///< "LOGIC.md"
    usize documentCount() {
        if (!m_scanned) scan(m_dir);
        return m_docs.size();
    }
    const std::string& currentFile() const {
        static const std::string none;
        return m_current >= 0 && m_current < static_cast<int>(m_docs.size()) ? m_docs[static_cast<usize>(m_current)].file : none;
    }

private:
    struct Doc {
        std::string file, title, text, lower;
    };
    void scan(const std::string& docsDir);
    void render(EditorShared& s, const std::string& text);
    void inlineText(EditorShared& s, const std::string& text, float indent);
    void followLink(EditorShared& s, const std::string& link);

    std::string m_dir;
    std::vector<Doc> m_docs;
    bool m_scanned = false, m_scrollTop = false;
    int m_current = -1;
    std::string m_search;
};

/// Visual shader editor (.agrshadergraph): node canvas compiled to the .agrshader next to the
/// graph, live preview (scene and sphere), undo / redo, generated code.
class ShaderGraphPanel {
public:
    void draw(EditorShared& s, const MaterialPreview& preview);
    void open(EditorShared& s, const std::string& uri);
    bool save(EditorShared& s);
    bool undo(EditorShared& s);
    bool redo(EditorShared& s);
    int addNode(EditorShared& s, const std::string& type, ImVec2 canvasPos);
    ShaderGraph& graph() { return m_graph; }
    const ShaderGraphCompileResult& result() const { return m_result; }
    const Renderer::ShaderStatus& status() const { return m_status; }
    const std::string& uri() const { return m_uri; }
    bool dirty() const { return m_dirty; }
    void changed(EditorShared& s);  ///< After editing graph(): marks dirty, recompiles, previews.
    void snapshot();                ///< Before editing graph(): undo point.
    static std::string newGraph(ShaderGraph::Target target);
    static constexpr const char* kPreviewMaterial = "memory://shadergraph_preview.agrmat";

private:
    struct PinPos {
        int node;
        std::string port;
        bool output;
        ShaderPortType type;
        ImVec2 pos;
    };
    struct Drag {
        int node = 0;
        std::string port;
        bool output = false;
        bool active = false;
    };
    struct Pending {
        int node = 0;
        std::string port;
        bool output = false;
    };
    void drawCanvas(EditorShared& s);
    void recompile(EditorShared& s);
    std::string m_uri;
    ShaderGraph m_graph;
    ShaderGraphCompileResult m_result;
    Renderer::ShaderStatus m_status;
    std::vector<std::string> m_undo, m_redo;
    bool m_dirty = false, m_moved = false, m_panned = false;
    int m_selected = 0;
    ImVec2 m_pan{40.0f, 40.0f};
    float m_zoom = 1.0f;
    std::vector<PinPos> m_pins;
    Drag m_drag;
    Pending m_pendingFrom;
    ImVec2 m_menuPos{0.0f, 0.0f};
    std::string m_filter;
};

/// Visual logic editor (.agrlogic): node canvas, links, add menu, variables, compile status,
/// generated Lua, undo / redo, attach to the selected entity.
class LogicPanel {
public:
    void draw(EditorShared& s);
    void open(EditorShared& s, const std::string& uri);
    bool save(EditorShared& s);
    bool undo();
    bool redo();
    Status attachToSelection(EditorShared& s);
    /// Adds a node at a canvas position (connected to a pending dropped link, if any).
    int addNode(const std::string& type, ImVec2 canvasPos);
    logic::Graph& graph() { return m_graph; }
    const logic::CompileResult& result() const { return m_result; }
    const std::string& uri() const { return m_uri; }
    bool dirty() const { return m_dirty; }
    void changed();  ///< After editing graph(): marks dirty and recompiles.
    void snapshot();  ///< Before editing graph(): undo point.
    static std::string defaultGraph();

private:
    struct PinPos {
        int node;
        std::string pin;
        bool output;
        logic::PinKind kind;
        ImVec2 pos;
    };
    struct Drag {
        int node = 0;
        std::string pin;
        bool output = false;
        logic::PinKind kind = logic::PinKind::Any;
        bool active = false;
    };
    struct Pending {
        int node = 0;
        std::string pin;
        bool output = false;
    };
    void drawSidebar(EditorShared& s);
    void drawCanvas(EditorShared& s);
    void recompile();

    std::string m_uri;
    logic::Graph m_graph;
    logic::CompileResult m_result;
    bool m_dirty = false, m_moved = false, m_panned = false;
    std::vector<std::string> m_undo, m_redo;
    int m_selected = 0;
    ImVec2 m_pan{40.0f, 40.0f};
    float m_zoom = 1.0f;
    std::vector<PinPos> m_pins;
    Drag m_drag;
    Pending m_pendingFrom;
    ImVec2 m_menuPos{0, 0};
    std::string m_search;
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
    std::string m_patchBase;  // game.agrpak (or an earlier patch) the next patch is made against
    bool m_patch = false;     // the next start() exports a patch
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
