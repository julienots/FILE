#pragma once
#include <functional>
#include <string>
#include <unordered_map>

#include <imgui.h>

#include "agr/assets/AssetManager.hpp"
#include "agr/editor/EditorContext.hpp"
#include "agr/editor/ui/Theme.hpp"
#include "agr/platform/SdlPlatform.hpp"
#include "agr/render/RenderPlugin.hpp"

namespace agr::editor {

/// Drag & drop payload identifiers.
inline constexpr const char* kPayloadEntity = "AGR_ENTITY";  ///< u64 Uuid
inline constexpr const char* kPayloadAsset = "AGR_ASSET";    ///< null-terminated "assets://..." URI

/// State shared by every panel of the editor window.
struct EditorShared {
    Engine& engine;
    EditorContext& ctx;
    SdlPlatform* platform = nullptr;
    RenderPlugin* render = nullptr;
    EditorFonts fonts;

    rhi::Device* device() const { return render ? render->device() : nullptr; }
    Renderer* renderer() const { return render ? render->renderer() : nullptr; }
    AssetManager* assets() const { return render ? render->assets() : nullptr; }

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
    std::string assetFolder;       ///< Current folder of the asset browser, relative to assets://

    // --- thumbnails (asset browser, inspector): kept loaded until releaseThumbnails()
    std::unordered_map<std::string, TextureAsset> thumbnails;
    /// Texture to display for an image URI (white while loading, checkerboard on error).
    ImTextureID thumbnail(const std::string& uri);
    void releaseThumbnails();
};

/// Image asset extensions the engine can import today.
bool isImageFile(const std::string& name);

} // namespace agr::editor
