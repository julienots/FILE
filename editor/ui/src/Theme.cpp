#include <array>
#include "agr/editor/ui/Theme.hpp"

#include <cmath>
#include <filesystem>

#include "agr/core/Log.hpp"

namespace agr::editor {

ImVec4 palette::toVec4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

EditorFonts applyAgrTheme(const std::string& fontsDir, float uiScale) {
    ImGuiIO& io = ImGui::GetIO();
    EditorFonts fonts;
    namespace fs = std::filesystem;
    auto load = [&](const char* file, float size) -> ImFont* {
        const fs::path path = fs::path(fontsDir) / file;
        if (!fs::exists(path)) {
            AGR_LOG_WARN("Editor", "font '{}' not found: using the default ImGui font", path.string());
            return nullptr;
        }
        return io.Fonts->AddFontFromFileTTF(path.string().c_str(), size * uiScale);
    };
    fonts.regular = load("Inter-Regular.ttf", 15.0f);
    fonts.semibold = load("Inter-SemiBold.ttf", 15.0f);
    fonts.mono = load("JetBrainsMono-Regular.ttf", 14.0f);
    if (!fonts.regular) fonts.regular = io.Fonts->AddFontDefault();
    if (!fonts.semibold) fonts.semibold = fonts.regular;
    if (!fonts.mono) fonts.mono = fonts.regular;
    io.FontDefault = fonts.regular;

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.ChildRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.PopupRounding = 4.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding = 3.0f;
    s.TabRounding = 3.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowPadding = {8, 8};
    s.FramePadding = {7, 4};
    s.ItemSpacing = {8, 5};
    s.ItemInnerSpacing = {6, 4};
    s.IndentSpacing = 14.0f;
    s.ScrollbarSize = 12.0f;
    s.GrabMinSize = 9.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.SeparatorTextBorderSize = 1.0f;
    s.DockingSeparatorSize = 2.0f;
    s.ScaleAllSizes(uiScale);

    using namespace palette;
    ImVec4* c = s.Colors;
    const ImVec4 bg = toVec4(kGraphite950), panel = toVec4(kGraphite900), field = toVec4(kGraphite800),
                 border = toVec4(kGraphite700), text = toVec4(kText), muted = toVec4(kTextMuted), amber = toVec4(kAmber),
                 select = toVec4(kSelect);
    auto alpha = [](ImVec4 v, float a) { return ImVec4(v.x, v.y, v.z, a); };
    auto lighten = [](ImVec4 v, float k) { return ImVec4(v.x + k, v.y + k, v.z + k, v.w); };
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = muted;
    c[ImGuiCol_WindowBg] = panel;
    c[ImGuiCol_ChildBg] = alpha(panel, 0.0f);
    c[ImGuiCol_PopupBg] = alpha(field, 0.98f);
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_BorderShadow] = alpha(bg, 0.0f);
    c[ImGuiCol_FrameBg] = field;
    c[ImGuiCol_FrameBgHovered] = lighten(field, 0.04f);
    c[ImGuiCol_FrameBgActive] = lighten(field, 0.07f);
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = bg;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg] = bg;
    c[ImGuiCol_ScrollbarBg] = alpha(bg, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = border;
    c[ImGuiCol_ScrollbarGrabHovered] = lighten(border, 0.08f);
    c[ImGuiCol_ScrollbarGrabActive] = muted;
    c[ImGuiCol_CheckMark] = amber;
    c[ImGuiCol_SliderGrab] = amber;
    c[ImGuiCol_SliderGrabActive] = lighten(amber, 0.08f);
    c[ImGuiCol_Button] = field;
    c[ImGuiCol_ButtonHovered] = lighten(field, 0.06f);
    c[ImGuiCol_ButtonActive] = lighten(field, 0.10f);
    c[ImGuiCol_Header] = alpha(select, 0.30f);
    c[ImGuiCol_HeaderHovered] = alpha(select, 0.20f);
    c[ImGuiCol_HeaderActive] = alpha(select, 0.40f);
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = alpha(amber, 0.6f);
    c[ImGuiCol_SeparatorActive] = amber;
    c[ImGuiCol_ResizeGrip] = alpha(border, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = alpha(amber, 0.6f);
    c[ImGuiCol_ResizeGripActive] = amber;
    c[ImGuiCol_Tab] = bg;
    c[ImGuiCol_TabHovered] = field;
    c[ImGuiCol_TabSelected] = panel;
    c[ImGuiCol_TabSelectedOverline] = amber;
    c[ImGuiCol_TabDimmed] = bg;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_TabDimmedSelectedOverline] = alpha(amber, 0.0f);
    c[ImGuiCol_DockingPreview] = alpha(amber, 0.5f);
    c[ImGuiCol_DockingEmptyBg] = bg;
    c[ImGuiCol_PlotLines] = amber;
    c[ImGuiCol_PlotHistogram] = amber;
    c[ImGuiCol_TableHeaderBg] = field;
    c[ImGuiCol_TableBorderStrong] = border;
    c[ImGuiCol_TableBorderLight] = alpha(border, 0.6f);
    c[ImGuiCol_TextSelectedBg] = alpha(select, 0.35f);
    c[ImGuiCol_DragDropTarget] = amber;
    c[ImGuiCol_NavCursor] = amber;
    c[ImGuiCol_ModalWindowDimBg] = alpha(bg, 0.7f);
    return fonts;
}

namespace icons {

void drawAgrMark(ImDrawList* dl, ImVec2 o, float size) {
    const float k = size / 64.0f;
    auto P = [&](float x, float y) { return ImVec2(o.x + x * k, o.y + y * k); };
    // Open "A" (two convex parts so the filled polygons stay convex) + amber bar.
    const ImVec2 left[] = {P(32, 5), P(32, 29.5f), P(17.5f, 59), P(5, 59)};
    const ImVec2 right[] = {P(32, 5), P(59, 59), P(46.5f, 59), P(32, 29.5f)};
    dl->AddConvexPolyFilled(left, 4, palette::kText);
    dl->AddConvexPolyFilled(right, 4, palette::kText);
    const ImVec2 bar[] = {P(15, 41), P(62, 41), P(57, 49), P(11, 49)};
    dl->AddConvexPolyFilled(bar, 4, palette::kAmber);
}

void draw(ImDrawList* dl, Kind kind, ImVec2 c, float s, ImU32 col) {
    const float h = s * 0.5f, t = std::max(1.0f, s * 0.09f);
    switch (kind) {
        case Kind::Play: dl->AddTriangleFilled({c.x - h * 0.6f, c.y - h * 0.75f}, {c.x - h * 0.6f, c.y + h * 0.75f}, {c.x + h * 0.75f, c.y}, col); break;
        case Kind::Pause:
            dl->AddRectFilled({c.x - h * 0.6f, c.y - h * 0.7f}, {c.x - h * 0.15f, c.y + h * 0.7f}, col, 1.0f);
            dl->AddRectFilled({c.x + h * 0.15f, c.y - h * 0.7f}, {c.x + h * 0.6f, c.y + h * 0.7f}, col, 1.0f);
            break;
        case Kind::Stop: dl->AddRectFilled({c.x - h * 0.6f, c.y - h * 0.6f}, {c.x + h * 0.6f, c.y + h * 0.6f}, col, 1.5f); break;
        case Kind::Eye:
        case Kind::EyeClosed: {
            // Almond outline + pupil; closed eye = lower lid only.
            const int n = 12;
            ImVec2 top[n + 1], bottom[n + 1];
            for (int i = 0; i <= n; ++i) {
                const float u = static_cast<float>(i) / n, x = c.x - h * 0.9f + u * h * 1.8f, y = std::sin(u * 3.14159f) * h * 0.55f;
                top[i] = {x, c.y - y};
                bottom[i] = {x, c.y + y};
            }
            if (kind == Kind::Eye) {
                dl->AddPolyline(top, n + 1, col, t);
                dl->AddPolyline(bottom, n + 1, col, t);
                dl->AddCircleFilled(c, h * 0.28f, col);
            } else {
                dl->AddPolyline(bottom, n + 1, col, t);
            }
            break;
        }
        case Kind::Lock:
        case Kind::Unlock: {
            dl->AddRectFilled({c.x - h * 0.6f, c.y - h * 0.1f}, {c.x + h * 0.6f, c.y + h * 0.75f}, col, 1.5f);
            const float shift = kind == Kind::Unlock ? h * 0.45f : 0.0f;
            dl->PathArcTo({c.x + shift, c.y - h * 0.15f}, h * 0.38f, 3.14159f, 2.0f * 3.14159f, 10);
            dl->PathStroke(col, t);
            dl->AddLine({c.x - h * 0.38f + shift, c.y - h * 0.15f}, {c.x - h * 0.38f + shift, c.y - h * 0.1f}, col, t);
            dl->AddLine({c.x + h * 0.38f + shift, c.y - h * 0.15f}, {c.x + h * 0.38f + shift, c.y - h * 0.1f}, col, t);
            break;
        }
        case Kind::Folder:
            dl->AddRectFilled({c.x - h * 0.9f, c.y - h * 0.55f}, {c.x - h * 0.1f, c.y - h * 0.3f}, col, 1.0f);
            dl->AddRectFilled({c.x - h * 0.9f, c.y - h * 0.35f}, {c.x + h * 0.9f, c.y + h * 0.65f}, col, 2.0f);
            break;
        case Kind::File:
        case Kind::Image:
        case Kind::Scene: {
            const ImVec2 a{c.x - h * 0.65f, c.y - h * 0.85f}, b{c.x + h * 0.65f, c.y + h * 0.85f};
            dl->AddRect(a, b, col, 2.0f, t);
            if (kind == Kind::Image) {
                dl->AddTriangleFilled({a.x + h * 0.15f, b.y - h * 0.2f}, {c.x, c.y - h * 0.1f}, {b.x - h * 0.15f, b.y - h * 0.2f}, col);
                dl->AddCircleFilled({c.x + h * 0.25f, c.y - h * 0.45f}, h * 0.13f, col);
            } else if (kind == Kind::Scene) {
                draw(dl, Kind::Entity, {c.x, c.y + h * 0.1f}, s * 0.55f, col);
            } else {
                for (int i = 0; i < 3; ++i)
                    dl->AddLine({a.x + h * 0.25f, c.y - h * 0.35f + i * h * 0.35f}, {b.x - h * 0.25f, c.y - h * 0.35f + i * h * 0.35f}, col, t);
            }
            break;
        }
        case Kind::Entity: {  // isometric cube
            const ImVec2 top{c.x, c.y - h * 0.8f}, l{c.x - h * 0.7f, c.y - h * 0.4f}, r{c.x + h * 0.7f, c.y - h * 0.4f},
                mid{c.x, c.y}, bl{c.x - h * 0.7f, c.y + h * 0.4f}, br{c.x + h * 0.7f, c.y + h * 0.4f}, bot{c.x, c.y + h * 0.8f};
            const ImVec2 outline[] = {top, r, br, bot, bl, l};
            dl->AddPolyline(outline, 6, col, t, ImDrawFlags_Closed);
            dl->AddLine(l, mid, col, t);
            dl->AddLine(r, mid, col, t);
            dl->AddLine(mid, bot, col, t);
            break;
        }
        case Kind::Camera:
            dl->AddRectFilled({c.x - h * 0.85f, c.y - h * 0.45f}, {c.x + h * 0.25f, c.y + h * 0.45f}, col, 2.0f);
            dl->AddTriangleFilled({c.x + h * 0.25f, c.y}, {c.x + h * 0.85f, c.y - h * 0.45f}, {c.x + h * 0.85f, c.y + h * 0.45f}, col);
            break;
        case Kind::Light:
            dl->AddCircleFilled(c, h * 0.38f, col);
            for (int i = 0; i < 8; ++i) {
                const float a = i * 3.14159f / 4.0f, ca = std::cos(a), sa = std::sin(a);
                dl->AddLine({c.x + ca * h * 0.6f, c.y + sa * h * 0.6f}, {c.x + ca * h * 0.9f, c.y + sa * h * 0.9f}, col, t);
            }
            break;
        case Kind::Model: {  // solid isometric cube (imported model)
            const ImVec2 top{c.x, c.y - h * 0.8f}, l{c.x - h * 0.75f, c.y - h * 0.4f}, r{c.x + h * 0.75f, c.y - h * 0.4f}, mid{c.x, c.y},
                bl{c.x - h * 0.75f, c.y + h * 0.45f}, br{c.x + h * 0.75f, c.y + h * 0.45f}, bot{c.x, c.y + h * 0.85f};
            const ImVec2 topFace[] = {top, r, mid, l};
            const ImVec2 leftFace[] = {l, mid, bot, bl};
            const ImVec2 rightFace[] = {mid, r, br, bot};
            dl->AddConvexPolyFilled(topFace, 4, col);
            dl->AddConvexPolyFilled(leftFace, 4, (col & 0x00FFFFFF) | (static_cast<ImU32>(((col >> 24) & 0xFF) * 0.75f) << 24));
            dl->AddConvexPolyFilled(rightFace, 4, (col & 0x00FFFFFF) | (static_cast<ImU32>(((col >> 24) & 0xFF) * 0.5f) << 24));
            break;
        }
        case Kind::Prefab: {  // outlined cube in prefab blue (reusable hierarchy)
            const ImU32 blue = (col & 0xFF000000u) | IM_COL32(110, 170, 255, 0);
            const ImVec2 top{c.x, c.y - h * 0.8f}, l{c.x - h * 0.75f, c.y - h * 0.4f}, r{c.x + h * 0.75f, c.y - h * 0.4f}, mid{c.x, c.y},
                bl{c.x - h * 0.75f, c.y + h * 0.45f}, br{c.x + h * 0.75f, c.y + h * 0.45f}, bot{c.x, c.y + h * 0.85f};
            const ImVec2 outline[] = {top, r, br, bot, bl, l};
            dl->AddPolyline(outline, 6, blue, t * 1.2f, ImDrawFlags_Closed);
            dl->AddLine(l, mid, blue, t);
            dl->AddLine(mid, r, blue, t);
            dl->AddLine(mid, bot, blue, t);
            break;
        }
        case Kind::Material:  // shaded sphere
            dl->AddCircleFilled(c, h * 0.8f, (col & 0x00FFFFFF) | (static_cast<ImU32>(((col >> 24) & 0xFF) * 0.55f) << 24), 24);
            dl->AddCircleFilled({c.x - h * 0.15f, c.y - h * 0.15f}, h * 0.55f, (col & 0x00FFFFFF) | (static_cast<ImU32>(((col >> 24) & 0xFF) * 0.8f) << 24), 24);
            dl->AddCircleFilled({c.x - h * 0.32f, c.y - h * 0.32f}, h * 0.18f, col, 12);
            break;
        case Kind::Shader:  // "</>"
            dl->AddPolyline(std::array<ImVec2, 3>{ImVec2{c.x - h * 0.3f, c.y - h * 0.5f}, ImVec2{c.x - h * 0.8f, c.y}, ImVec2{c.x - h * 0.3f, c.y + h * 0.5f}}.data(),
                            3, col, t * 1.3f, ImDrawFlags_None);
            dl->AddPolyline(std::array<ImVec2, 3>{ImVec2{c.x + h * 0.3f, c.y - h * 0.5f}, ImVec2{c.x + h * 0.8f, c.y}, ImVec2{c.x + h * 0.3f, c.y + h * 0.5f}}.data(),
                            3, col, t * 1.3f, ImDrawFlags_None);
            dl->AddLine({c.x + h * 0.15f, c.y - h * 0.7f}, {c.x - h * 0.15f, c.y + h * 0.7f}, col, t * 1.3f);
            break;
        case Kind::Plus:
            dl->AddLine({c.x - h * 0.6f, c.y}, {c.x + h * 0.6f, c.y}, col, t * 1.4f);
            dl->AddLine({c.x, c.y - h * 0.6f}, {c.x, c.y + h * 0.6f}, col, t * 1.4f);
            break;
        case Kind::Search:
            dl->AddCircle({c.x - h * 0.15f, c.y - h * 0.15f}, h * 0.45f, col, 16, t);
            dl->AddLine({c.x + h * 0.2f, c.y + h * 0.2f}, {c.x + h * 0.7f, c.y + h * 0.7f}, col, t * 1.3f);
            break;
    }
}

bool button(const char* id, Kind kind, float size, ImU32 color, const char* tooltip, bool active) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, {size, size});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (active || ImGui::IsItemHovered())
        dl->AddRectFilled(pos, {pos.x + size, pos.y + size}, active ? IM_COL32(245, 165, 36, 50) : IM_COL32(255, 255, 255, 20), 3.0f);
    draw(dl, kind, {pos.x + size * 0.5f, pos.y + size * 0.5f}, size * 0.62f, color);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

}  // namespace icons
}  // namespace agr::editor
