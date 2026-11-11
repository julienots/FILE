#pragma once
#include <functional>
#include <string>
#include <unordered_map>

#include <imgui.h>

#include "agr/assets/AssetManager.hpp"
#include "agr/editor/EditorContext.hpp"
#include "agr/editor/Picking.hpp"
#include "agr/editor/ui/Theme.hpp"
#include "agr/platform/SdlPlatform.hpp"
#include "agr/render/RenderPlugin.hpp"
#include "agr/twod/TwoDPlugin.hpp"
#include "agr/animation/AnimationPlugin.hpp"
#include "agr/scene/Terrain.hpp"
#include "agr/audio/AudioPlugin.hpp"
#include "agr/ui/UIPlugin.hpp"

namespace agr::editor {

/// Drag & drop payload identifiers.
inline constexpr const char* kPayloadEntity = "AGR_ENTITY";  ///< u64 Uuid
inline constexpr const char* kPayloadAsset = "AGR_ASSET";    ///< null-terminated "assets://..." URI

/// Tilemap painting (inspector palette + viewport). Painting applies to the primary
/// selection when it has a Tilemap, in edit mode only.
struct TilemapBrush {
    enum class Tool : u8 { Paint, Erase, Rect, Fill, Pick };
    bool active = false;
    Tool tool = Tool::Paint;
    i32 tile = 0;  ///< Frame of the tileset painted by Paint / Rect / Fill.
};

/// Terrain sculpting / painting (inspector settings + viewport), on the primary selection.
struct TerrainTool {
    bool active = false;
    terrain::BrushSettings settings;
};

/// State shared by every panel of the editor window.
struct EditorShared {
    Engine& engine;
    EditorContext& ctx;
    SdlPlatform* platform = nullptr;
    RenderPlugin* render = nullptr;
    TwoDPlugin* twod = nullptr;
    AudioPlugin* audio = nullptr;
    UIPlugin* ui = nullptr;
    AnimationPlugin* animation = nullptr;
    EditorFonts fonts;
    TilemapBrush brush;
    TerrainTool terrainTool;

    rhi::Device* device() const { return render ? render->device() : nullptr; }
    Renderer* renderer() const { return render ? render->renderer() : nullptr; }
    AssetManager* assets() const { return render ? render->assets() : nullptr; }
    SpriteSheetLibrary* sheets() const { return render ? render->spriteSheets() : nullptr; }
    ModelLibrary* models() const { return render ? render->models() : nullptr; }
    MaterialLibrary* materials() const { return render ? render->materials() : nullptr; }
    PickAssets pickAssets() const { return {sheets(), models()}; }

    // --- status bar
    std::string status;
    f64 statusTime = 0.0;
    void notify(std::string message);

    // --- "unsaved changes" guard for destructive navigation (open / new scene, quit)
    std::function<void()> pendingAction;
    bool openUnsavedPopup = false;
    /// Runs `action` now, or after asking to save when the scene has unsaved changes.
    void runGuarded(std::function<void()> action);

    // --- requests between panels
    bool openSaveAsPopup = false;
    bool renameSelection = false;  ///< Hierarchy starts inline rename of the primary selection.
    bool focusViewport = false;
    bool showMixer = false;        ///< Audio mixer panel.
    bool showInput = false;        ///< Input actions panel.
    bool showLocalization = false; ///< Localization table panel.
    bool showAnimation = false;    ///< Animation timeline panel.
    bool showStateMachine = false; ///< State machine graph panel.
    std::string assetFolder;       ///< Current folder of the asset browser, relative to assets://
    std::string inspectedAsset;    ///< Asset shown by the inspector when no entity is selected (.agrmat).
    std::string previewMaterial;   ///< Material the preview renders this frame (set by the inspector).

    // --- thumbnails (asset browser, inspector): kept loaded until releaseThumbnails()
    std::unordered_map<std::string, TextureAsset> thumbnails;
    /// Texture to display for an image URI (white while loading, checkerboard on error).
    ImTextureID thumbnail(const std::string& uri);
    void releaseThumbnails();
};

/// Image asset extensions the engine can import today.
bool isImageFile(const std::string& name);
bool isPrefabFile(const std::string& name);

} // namespace agr::editor
