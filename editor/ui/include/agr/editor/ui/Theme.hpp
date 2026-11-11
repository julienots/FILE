#pragma once
#include <string>

#include <imgui.h>

namespace agr::editor {

/// AGR brand palette (docs/brand/BRAND.md).
namespace palette {
inline constexpr ImU32 rgb(unsigned r, unsigned g, unsigned b, unsigned a = 255) { return IM_COL32(r, g, b, a); }
inline constexpr ImU32 kGraphite950 = rgb(0x0E, 0x10, 0x13);
inline constexpr ImU32 kGraphite900 = rgb(0x15, 0x18, 0x1D);
inline constexpr ImU32 kGraphite800 = rgb(0x1D, 0x21, 0x27);
inline constexpr ImU32 kGraphite700 = rgb(0x2A, 0x2F, 0x37);
inline constexpr ImU32 kText = rgb(0xE6, 0xE4, 0xDF);
inline constexpr ImU32 kTextMuted = rgb(0x9A, 0xA0, 0xA8);
inline constexpr ImU32 kAmber = rgb(0xF5, 0xA5, 0x24);
inline constexpr ImU32 kSelect = rgb(0x3D, 0x8B, 0xFD);
inline constexpr ImU32 kSuccess = rgb(0x3F, 0xB9, 0x50);
inline constexpr ImU32 kWarning = rgb(0xD2, 0x99, 0x22);
inline constexpr ImU32 kError = rgb(0xF8, 0x51, 0x49);
inline constexpr ImU32 kAxisX = rgb(0xE5, 0x48, 0x4D);
inline constexpr ImU32 kAxisY = rgb(0x46, 0xA7, 0x58);
inline constexpr ImU32 kAxisZ = rgb(0x3E, 0x63, 0xDD);
ImVec4 toVec4(ImU32 c);
}  // namespace palette

struct EditorFonts {
    ImFont* regular = nullptr;
    ImFont* semibold = nullptr;
    ImFont* mono = nullptr;
};

/// Loads Inter / JetBrains Mono from `fontsDir` (falls back to the ImGui default
/// font with a warning) and applies the AGR dark theme.
EditorFonts applyAgrTheme(const std::string& fontsDir, float uiScale);

/// Vector icons drawn with ImDrawList: crisp at any DPI, no icon font needed.
namespace icons {
enum class Kind { Play, Pause, Stop, Eye, EyeClosed, Lock, Unlock, Folder, File, Image, Scene, Entity, Camera, Light, Plus, Search, Model, Material, Prefab, Shader };
void draw(ImDrawList* dl, Kind kind, ImVec2 center, float size, ImU32 color);
/// Icon-sized button; returns true when clicked.
bool button(const char* id, Kind kind, float size, ImU32 color, const char* tooltip = nullptr, bool active = false);
/// The AGR mark (same geometry as docs/brand/agr-mark.svg).
void drawAgrMark(ImDrawList* dl, ImVec2 topLeft, float size);
}  // namespace icons

} // namespace agr::editor
