#include "agr/editor/ui/EditorShared.hpp"

#include "agr/scene/Prefab.hpp"

#include "agr/editor/ui/ImGuiRenderer.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace agr::editor {

void EditorShared::notify(std::string message) {
    AGR_LOG_INFO("Editor", "{}", message);
    status = std::move(message);
    statusTime = ImGui::GetTime();
}

void EditorShared::runGuarded(std::function<void()> action) {
    if (ctx.dirty()) {
        pendingAction = std::move(action);
        openUnsavedPopup = true;
    } else {
        action();
    }
}

ImTextureID EditorShared::thumbnail(const std::string& uri) {
    AssetManager* am = assets();
    if (!am) return ImTextureID_Invalid;
    auto it = thumbnails.find(uri);
    if (it == thumbnails.end()) {
        // Linear (non-sRGB) format: the UI shader outputs sampled values unchanged.
        it = thumbnails.emplace(uri, am->loadTexture(uri, {.srgb = false})).first;
    }
    return ImGuiRenderer::toImTexture(am->gpuTexture(it->second));
}

void EditorShared::releaseThumbnails() {
    if (AssetManager* am = assets())
        for (auto& [uri, asset] : thumbnails) am->release(asset);
    thumbnails.clear();
}

bool isImageFile(const std::string& name) {
    std::string ext = std::filesystem::path(name).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".psd" ||
           ext == ".gif" || ext == ".hdr";
}

bool isPrefabFile(const std::string& name) { return std::filesystem::path(name).extension() == prefab::kExtension; }

} // namespace agr::editor
