#pragma once
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "agr/editor/ui/EditorShared.hpp"
#include "agr/assets/Material.hpp"

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
    bool gameView = false;
    GizmoOp gizmoOp = GizmoOp::Translate;
    bool gizmoLocal = false;
    bool snap = false;
    float snapTranslate = 0.5f, snapRotate = 15.0f, snapScale = 0.1f;
    bool showGrid = true;
    bool showIcons = true;
    bool showColliders = true;  ///< Outlines of 2D / 3D colliders (green; triggers in amber).
    float flySpeed = 6.0f;

private:
    void handleCamera(EditorShared& s, const FrameTime& time, bool hovered);
    void drawOverlays(EditorShared& s, ImVec2 origin, ImVec2 size);
    void drawColliders(EditorShared& s, ImVec2 origin, ImVec2 size, const math::Mat4& viewProj);
    void drawGizmo(EditorShared& s, ImVec2 origin, ImVec2 size);
    void handlePicking(EditorShared& s, ImVec2 origin, ImVec2 size);
    void handleDrop(EditorShared& s, ImVec2 origin, ImVec2 size);
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

class StatsPanel {
public:
    void draw(EditorShared& s, const FrameTime& time);

private:
    std::vector<float> m_frameMs = std::vector<float>(180, 0.0f);
    usize m_head = 0;
};

} // namespace agr::editor
