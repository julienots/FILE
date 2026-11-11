#pragma once
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "agr/render/Renderer.hpp"
#include "agr/ui/Font.hpp"
#include "agr/ui/UIComponents.hpp"

namespace agr {
class Localization;

/// A mouse or a finger, in pixels of the UI screen (origin top-left).
struct UIPointer {
    u64 id = 0;
    math::Vec2 position{0.0f, 0.0f};
    bool down = false;  ///< false on the frame a finger lifts (release), and for a hovering mouse.
};

enum class UIEventType : u8 { Click, ValueChanged };
struct UIEvent {
    UIEventType type = UIEventType::Click;
    ecs::Entity entity;
    float value = 0.0f;
};

/// Published on the engine event bus by UIPlugin (scripts receive them as callbacks).
struct UIClickEvent {
    ecs::Entity entity;
};
struct UIValueChangedEvent {
    ecs::Entity entity;
    float value = 0.0f;
};

struct UIStats {
    u32 canvases = 0;
    u32 elements = 0;  ///< Laid out elements (UIRect).
    u32 quads = 0;     ///< Last overlay.
    u32 texts = 0;
    f64 layoutMs = 0.0, buildMs = 0.0;
};

/// Game UI: lays out the UICanvas hierarchies for a screen size, handles pointers
/// (mouse and multi-touch: hover, press, click, slider drag) and produces the overlay
/// drawn by the Renderer. Main thread.
class UISystem final : public OverlaySource {
public:
    explicit UISystem(VirtualFileSystem& vfs);
    ~UISystem() override;

    /// Textures (images) and the glyph atlas device come from the renderer (null = headless).
    void setRenderer(Renderer* renderer);
    /// Texts with a localizationKey take their translation from it (null = keys ignored).
    void setLocalization(const Localization* localization) { m_localization = localization; }
    Renderer* renderer() const { return m_renderer; }

    /// Screen insets (left, top, right, bottom, in pixels of the UI screen) hidden by notches or
    /// system bars; canvases with respectSafeArea keep their children inside.
    void setSafeArea(math::Vec4 insets) { m_safeArea = insets; }
    math::Vec4 safeArea() const { return m_safeArea; }
    /// Computes UIRect::screenRect of every element for a width x height pixel screen.
    void layout(Scene& scene, u32 width, u32 height);
    /// Applies pointers (positions in the same pixels as the last layout). Produces events.
    void processInput(Scene& scene, std::span<const UIPointer> pointers);
    std::vector<UIEvent> takeEvents();
    /// True when a pointer is over a UI element (image, button, slider) or holds one:
    /// gameplay should then ignore it.
    bool pointerOverUI() const { return m_pointerOverUI; }
    bool isPointerOverUI(math::Vec2 position) const;
    /// Held down by at least one pointer (on-screen controls).
    bool isPressed(ecs::Entity e) const;
    bool isHovered(ecs::Entity e) const { return m_hovered.count(e.packed()) > 0; }
    /// Topmost element under a point that blocks pointers (image, button, slider), or null.
    ecs::Entity hitTest(math::Vec2 position) const;
    /// Topmost laid out element under a point, whatever its components (editor picking);
    /// canvases themselves are not returned.
    ecs::Entity elementAt(math::Vec2 position) const;
    /// The root canvas an element was laid out under (null if not laid out).
    ecs::Entity canvasOf(ecs::Entity element) const;
    /// Forgets pointer state (game stopped / scene changed).
    void reset();

    void buildOverlay(Scene& scene, u32 width, u32 height, OverlayDrawList& out) override;

    FontLibrary& fonts() { return m_fonts; }
    const UIStats& stats() const { return m_stats; }
    /// Scale factor (pixels per canvas unit) of a canvas in the last layout.
    float canvasScale(ecs::Entity canvas) const;

private:
    struct Element {
        ecs::Entity entity;
        math::Vec4 rect;  // pixels
        bool blocks = false;
        bool interactive = false;
    };
    struct CanvasPass {
        i32 sortOrder = 0;
        u32 hierarchyIndex = 0;
        ecs::Entity canvas;
        float scale = 1.0f;
        std::vector<Element> elements;  // painter's order
    };
    struct PendingText {
        usize element;
        usize canvas;
        TextLayout layout;
        math::Vec2 origin;
    };

    void layoutChildren(Scene& scene, ecs::Entity parent, math::Vec4 parentRect, float scale, CanvasPass& pass);
    void drawElement(Scene& scene, const Element& el, float scale, OverlayDrawList& out);
    void drawImage(const UIImageComponent& img, math::Vec4 rect, math::Vec4 tint, float scale, OverlayDrawList& out);
    void setSliderValue(Scene& scene, ecs::Entity e, float x);

    VirtualFileSystem& m_vfs;
    Renderer* m_renderer = nullptr;
    const Localization* m_localization = nullptr;
    FontLibrary m_fonts;
    Scene* m_layoutScene = nullptr;
    u64 m_layoutGeneration = 0;
    std::vector<CanvasPass> m_passes;
    std::unordered_map<u64, ecs::Entity> m_pressed;  // pointer id -> element
    std::unordered_set<u64> m_hovered;               // packed entities
    std::unordered_map<u64, bool> m_pointerDown;     // previous state per pointer
    std::vector<UIEvent> m_events;
    std::unordered_set<std::string> m_reportedFonts;
    std::vector<PendingText> m_texts;
    bool m_pointerOverUI = false;
    math::Vec4 m_safeArea{0.0f, 0.0f, 0.0f, 0.0f};
    UIStats m_stats;
};

} // namespace agr
