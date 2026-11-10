#pragma once
#include <optional>

#include "agr/runtime/Plugin.hpp"
#include "agr/ui/UISystem.hpp"

namespace agr {

/// Game UI: UICanvas hierarchies drawn over the scene by the Renderer, driven by the
/// mouse and multi-touch. Static plugin, registered after RenderPlugin and before
/// ScriptPlugin (scripts then see this frame's UI state: pointerOverUI, isPressed):
///     engine.plugins().addStatic(std::make_unique<UIPlugin>());
/// Clicks and slider changes are published as UIClickEvent / UIValueChangedEvent.
class UIPlugin final : public Plugin {
public:
    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onUpdate(Engine& engine, const FrameTime& time) override;

    UISystem& ui() { return *m_ui; }
    /// Pointer interaction (always in a game; in the editor, in Play mode).
    void setRunning(bool running);
    bool running() const { return m_running; }
    /// Editor: the game is shown in `windowRect` (x, y, w, h in window coordinates) at
    /// pixelWidth x pixelHeight; pointers are made relative to it.
    void setViewport(math::Vec4 windowRect, u32 pixelWidth, u32 pixelHeight);
    void clearViewport() { m_viewport.reset(); }
    /// Editor device preview: simulated safe-area insets in pixels of the view (none = the platform's).
    void setSafeAreaOverride(std::optional<math::Vec4> insets) { m_safeOverride = insets; }

private:
    struct Viewport {
        math::Vec4 rect;
        u32 width, height;
    };
    void bindRenderer(Engine& engine);

    std::unique_ptr<UISystem> m_ui;
    Renderer* m_renderer = nullptr;
    std::optional<Viewport> m_viewport;
    std::optional<math::Vec4> m_safeOverride;
    bool m_running = true;
};

} // namespace agr
