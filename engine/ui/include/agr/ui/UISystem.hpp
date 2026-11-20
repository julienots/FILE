#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "agr/core/Time.hpp"
#include "agr/render/Renderer.hpp"
#include "agr/ui/Font.hpp"
#include "agr/ui/UIComponents.hpp"
#include "agr/ui/UITheme.hpp"

namespace agr {
class Localization;

/// A mouse or a finger, in pixels of the UI screen (origin top-left).
struct UIPointer {
    u64 id = 0;
    math::Vec2 position{0.0f, 0.0f};
    bool down = false;  ///< false on the frame a finger lifts (release), and for a hovering mouse.
};

enum class UIEventType : u8 { Click, ValueChanged, TextChanged, Submit };
struct UIEvent {
    UIEventType type = UIEventType::Click;
    ecs::Entity entity;
    float value = 0.0f;
    std::string text;  ///< TextChanged / Submit
};

/// Published on the engine event bus by UIPlugin (scripts receive them as callbacks).
struct UIClickEvent {
    ecs::Entity entity;
};
struct UIValueChangedEvent {
    ecs::Entity entity;
    float value = 0.0f;
};
/// Text field edited (submitted = Enter).
struct UITextEvent {
    ecs::Entity entity;
    std::string text;
    bool submitted = false;
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

    // --- scroll views, text fields, keyboard / gamepad navigation
    /// Mouse wheel at a point (UI pixels): scrolls the topmost scroll view under it.
    bool scrollAt(Scene& scene, math::Vec2 position, math::Vec2 wheel);
    /// Typing for the focused text field: UTF-8 text, backspaces, Enter.
    void processText(Scene& scene, std::string_view typed, u32 backspaces, bool enter);
    ecs::Entity focusedField() const { return m_focusedField; }
    bool wantsText() const { return !m_focusedField.isNull(); }
    void focusField(ecs::Entity field);  ///< null = none (hides the on-screen keyboard)
    /// Called with true / false when a field gains / loses focus (on-screen keyboard).
    void setTextInputHandler(std::function<void(bool)> handler) { m_textInputHandler = std::move(handler); }
    /// Canvases with keyboardNavigation: move the highlight (dx, dy), activate it, adjust a slider.
    bool navigationEnabled() const;
    void navigate(Scene& scene, math::Vec2 direction);
    void navActivate(Scene& scene);
    void navAdjust(Scene& scene, float steps);
    ecs::Entity navFocus() const { return m_navVisible ? m_navFocus : ecs::Entity{}; }
    /// Drop-down whose list is open (null = none); opening one closes the other.
    ecs::Entity openDropdown() const { return m_openDropdown; }
    void openDropdown(ecs::Entity dropdown) {
        m_openDropdown = dropdown;
        m_popupHover = -1;
    }
    /// Selects an option of a drop-down / a tab of a tab container (events like a click).
    void selectOption(Scene& scene, ecs::Entity e, i32 index);
    /// Element whose tooltip is shown (null = none).
    ecs::Entity tooltipShown() const { return m_tooltipVisible ? m_tooltipEntity : ecs::Entity{}; }
    /// The theme of a canvas (.agrtheme), loaded through the VFS; null if none or invalid.
    const UITheme* theme(const std::string& uri);
    void setNavFocus(ecs::Entity e) {
        m_navFocus = e;
        m_navVisible = !e.isNull();
    }

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
        math::Vec4 clip{-1e9f, -1e9f, 2e9f, 2e9f};  // visible part (scroll views), pixels
    };
    struct ScrollInfo {
        math::Vec4 view;
        math::Vec2 content;  // pixels
        float scale = 1.0f;
    };
    struct PointerState {
        math::Vec2 down, last;
        ecs::Entity scrollView;
        bool dragging = false;
    };
    struct CanvasPass {
        i32 sortOrder = 0;
        u32 hierarchyIndex = 0;
        ecs::Entity canvas;
        float scale = 1.0f;
        const UITheme* theme = nullptr;
        std::vector<Element> elements;  // painter's order
    };
    struct RichStyle {
        math::Vec4 color{1, 1, 1, 1};
        bool hasColor = false, bold = false, underline = false;
    };
    struct PendingText {
        usize element;
        usize canvas;
        TextLayout layout;
        math::Vec2 origin;
        math::Vec4 color{1, 1, 1, 1};
        std::vector<RichStyle> styles;  // rich text: per code point
        float pixelSize = 0.0f;
    };
    struct ThemeEntry {
        std::unique_ptr<UITheme> theme;
        i64 modified = 0;
    };

    void layoutChildren(Scene& scene, ecs::Entity parent, math::Vec4 parentRect, float scale, CanvasPass& pass, math::Vec4 clip);
    void emit(OverlayDrawList& out, math::Vec2 min, math::Vec2 max, math::Vec4 uv, u32 color, rhi::TextureHandle texture) const;
    void clickToggle(Scene& scene, ecs::Entity e);
    void activate(Scene& scene, ecs::Entity e);  ///< click / toggle / focus, as a pointer release would
    ecs::Entity scrollViewOf(Scene& scene, ecs::Entity e) const;
    void scrollBy(Scene& scene, ecs::Entity view, math::Vec2 pixels);
    const Element* elementOf(ecs::Entity e) const;
    void drawElement(Scene& scene, const Element& el, float scale, const UITheme* theme, OverlayDrawList& out);
    void drawImage(const UIImageComponent& img, math::Vec4 rect, math::Vec4 tint, float scale, OverlayDrawList& out);
    void setSliderValue(Scene& scene, ecs::Entity e, math::Vec2 position);
    /// Pointer input while a drop-down list is open; true when the list took the pointer.
    bool popupPointer(Scene& scene, const UIPointer& p, bool wasDown);
    void layoutPopup();
    std::optional<TextLayout> textLayout(const std::string& font, const std::string& text, float pixelSize, float width, bool wrap, UIHAlign align);

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
    std::unordered_map<u64, ScrollInfo> m_scroll;           // packed scroll view -> last layout
    std::unordered_map<u64, PointerState> m_pointerState;   // pointer id
    ecs::Entity m_focusedField, m_navFocus;
    bool m_navVisible = false;
    std::function<void(bool)> m_textInputHandler;
    math::Vec4 m_clip{-1e9f, -1e9f, 2e9f, 2e9f};            // while drawing
    Stopwatch m_caretClock;
    math::Vec4 m_safeArea{0.0f, 0.0f, 0.0f, 0.0f};
    // Drop-down list drawn over everything.
    ecs::Entity m_openDropdown;
    math::Vec4 m_popupRect{0, 0, 0, 0};
    float m_popupItemHeight = 0.0f;
    i32 m_popupCount = 0, m_popupHover = -1;
    std::unordered_map<u64, i32> m_popupPress;  // pointer id -> pressed item (-1: press swallowed)
    const UITheme* m_popupTheme = nullptr;
    float m_popupScale = 1.0f;
    // Tooltip.
    ecs::Entity m_tooltipEntity;
    math::Vec2 m_tooltipPos{0.0f, 0.0f};
    Stopwatch m_tooltipClock;
    bool m_tooltipVisible = false;
    struct OverlayText {
        TextLayout layout;
        math::Vec2 origin;
        math::Vec4 color;
    };
    std::vector<OverlayText> m_overlayTexts;  // popup and tooltip texts, drawn last
    std::unordered_map<std::string, ThemeEntry> m_themes;
    VirtualFileSystem& m_vfs;
    UIStats m_stats;
};

} // namespace agr
