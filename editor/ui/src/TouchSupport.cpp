// Touch screens (Android, or --mobile on a desktop): a long press acts as a right click
// (context menus), a vertical finger drag scrolls the panel under the finger. Pinch and
// two-finger gestures of the scene view are in ViewportPanel.
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "agr/editor/ui/Panels.hpp"

namespace agr::editor {

void TouchSupport::update() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;
    const float unit = ImGui::GetFontSize() / 15.0f;  // follows the UI scale
    // Right click injected over the next two frames (events are processed by NewFrame).
    if (m_rightPhase == 1) {
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
        m_rightPhase = 2;
        return;
    }
    if (m_rightPhase == 2) {
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
        m_rightPhase = 0;
        return;
    }
    if (!io.MouseDown[ImGuiMouseButton_Left]) {
        m_longPressed = m_scrolling = false;
        m_scrollWindow = nullptr;
        return;
    }
    const ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
    const float moved = std::sqrt(drag.x * drag.x + drag.y * drag.y);
    if (!m_longPressed && !m_scrolling && io.MouseDownDuration[ImGuiMouseButton_Left] > longPressSeconds && moved < 10.0f * unit) {
        m_longPressed = true;
        ImGui::ClearActiveID();  // the held item is not clicked
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        m_rightPhase = 1;
        ++longPresses;
        return;
    }
    if (!m_scrolling && !m_longPressed && moved > 14.0f * unit && std::fabs(drag.y) > 1.5f * std::fabs(drag.x)) {
        ImGuiWindow* w = g.HoveredWindow;  // the nearest scrollable window under the finger
        while (w && w->ScrollMax.y <= 0.0f && (w->Flags & ImGuiWindowFlags_ChildWindow) && w->ParentWindow) w = w->ParentWindow;
        if (w && w->ScrollMax.y > 0.0f) {
            m_scrolling = true;
            m_scrollWindow = w;
        }
    }
    if (m_scrolling && m_scrollWindow) {
        ImGui::ClearActiveID();  // no click, no drag & drop: the list scrolls
        ImGui::SetScrollY(m_scrollWindow, m_scrollWindow->Scroll.y - io.MouseDelta.y);
    }
}

void applyTouchStyle(float uiScale) {
    ImGuiStyle& s = ImGui::GetStyle();
    s.ScrollbarSize = 22.0f * uiScale;
    s.GrabMinSize = 18.0f * uiScale;
    s.FramePadding.y = std::max(s.FramePadding.y, 7.0f * uiScale);
    s.ItemSpacing.y = std::max(s.ItemSpacing.y, 8.0f * uiScale);
    s.TouchExtraPadding = {5.0f * uiScale, 5.0f * uiScale};
}

} // namespace agr::editor
