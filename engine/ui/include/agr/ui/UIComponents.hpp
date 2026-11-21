#pragma once
#include <string>

#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

// Game UI. Coordinates are in canvas units with the origin at the top-left corner
// and Y pointing down; anchors (0, 0) = top-left of the parent, (1, 1) = bottom-right.

enum class UIScaleMode : i32 {
    ConstantPixelSize = 0,  ///< 1 unit = `pixelScale` pixels whatever the screen.
    ScaleWithScreen = 1,    ///< Units scale with the screen relative to the reference resolution.
};

/// Root of a UI: its descendants with a UIRect are laid out on the screen and drawn
/// over the scene (in sortOrder, then hierarchy order).
struct UICanvasComponent {
    UIScaleMode scaleMode = UIScaleMode::ScaleWithScreen;
    math::Vec2 referenceResolution{1920.0f, 1080.0f};
    float match = 0.5f;       ///< ScaleWithScreen: 0 = follow the width, 1 = the height (between: blend).
    float pixelScale = 1.0f;  ///< ConstantPixelSize.
    i32 sortOrder = 0;
    /// Children are laid out inside the safe area (away from notches and system bars).
    bool respectSafeArea = true;
    /// Arrow keys / gamepad D-pad move a highlight between its buttons, toggles, sliders and
    /// fields; Enter / Space / gamepad A activates (menus playable without a pointer).
    bool keyboardNavigation = false;
    /// Look of every control inside (.agrtheme): colors of buttons, sliders, toggles, bars,
    /// lists, tabs and tooltips, the font of the texts and the color of the white ones.
    std::string theme;
};

/// Rectangle of a UI element relative to its parent (Unity-like RectTransform).
/// With anchorMin == anchorMax the element has a fixed `size` placed at the anchor point;
/// with distinct anchors it stretches with the parent and `size` is added to the anchored area.
struct UIRectComponent {
    math::Vec2 anchorMin{0.5f, 0.5f};
    math::Vec2 anchorMax{0.5f, 0.5f};
    math::Vec2 pivot{0.5f, 0.5f};  ///< Point of the element placed at the anchor (0..1).
    math::Vec2 position{0.0f, 0.0f};
    math::Vec2 size{200.0f, 60.0f};
    /// Scale around the pivot (children too: they are laid out in the scaled rectangle) and rotation
    /// (radians, around the pivot; drawing and pointer hits follow it).
    math::Vec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
    /// Computed each frame: x, y, width, height in screen pixels (not saved).
    math::Vec4 screenRect{0.0f, 0.0f, 0.0f, 0.0f};
};

/// Colored or textured rectangle; optional 9-slice borders (texture pixels) keep corners
/// unstretched (panels, buttons).
struct UIImageComponent {
    std::string texture;  ///< Empty = plain color.
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec4 border{0.0f, 0.0f, 0.0f, 0.0f};  ///< 9-slice: left, top, right, bottom (texture pixels).
    float borderScale = 1.0f;                   ///< Border size on screen = border * borderScale (canvas units).
    bool preserveAspect = false;
    bool pixelArt = false;                      ///< Nearest filtering.
    bool blocksPointer = true;                  ///< false: clicks and touches go through (overlays, decorations).
    // Flat style box (images without a texture): rounded corners, outline, drop shadow.
    float cornerRadius = 0.0f;                  ///< Canvas units.
    float outlineWidth = 0.0f;                  ///< Canvas units, drawn inside the rectangle.
    math::Vec4 outlineColor{0.0f, 0.0f, 0.0f, 1.0f};
    float shadowSize = 0.0f;                    ///< Canvas units of soft shadow around the box, 0 = none.
    math::Vec4 shadowColor{0.0f, 0.0f, 0.0f, 0.45f};
    math::Vec2 shadowOffset{0.0f, 4.0f};        ///< Canvas units (+y = down).
};

enum class UIHAlign : i32 { Left = 0, Center = 1, Right = 2 };
enum class UIVAlign : i32 { Top = 0, Middle = 1, Bottom = 2 };

/// Text (UTF-8) with a TrueType font. `fontSize` is the line height in canvas units.
struct UITextComponent {
    std::string text = "Text";
    /// Key in the localization table (assets://localization.csv): when set, `text` is replaced by
    /// the translation in the current language.
    std::string localizationKey;
    std::string font = "assets://fonts/Inter-Regular.ttf";
    float fontSize = 32.0f;
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    UIHAlign hAlign = UIHAlign::Center;
    UIVAlign vAlign = UIVAlign::Middle;
    bool wrap = true;          ///< Breaks lines at the rectangle's width.
    float lineSpacing = 1.0f;
    /// Inline tags: [color=#ff8800] or [color=red]...[/color], [b]...[/b] (bold), [u]...[/u]
    /// (underline); write [[ for a literal [.
    bool richText = false;
};

/// Clickable element. Tints the UIImage of the entity by state. A click calls
/// `onClick()` on the entity's script and `message(sender)` on the target's script.
struct UIButtonComponent {
    bool interactable = true;
    math::Vec4 normalColor{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec4 hoverColor{0.92f, 0.92f, 0.92f, 1.0f};
    math::Vec4 pressedColor{0.75f, 0.75f, 0.75f, 1.0f};
    math::Vec4 disabledColor{0.6f, 0.6f, 0.6f, 0.5f};
    ecs::Entity target;
    std::string message;
};

/// Horizontal slider drawn from colors (track, fill, handle); dragging changes `value`.
/// A change calls `onValueChanged(value)` on the entity's script and `message(sender, value)`
/// on the target's script.
struct UISliderComponent {
    float value = 0.5f;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    bool wholeNumbers = false;
    bool interactable = true;
    bool vertical = false;  ///< Bottom = minValue, top = maxValue.
    math::Vec4 trackColor{0.16f, 0.18f, 0.22f, 1.0f};
    math::Vec4 fillColor{0.98f, 0.55f, 0.16f, 1.0f};
    math::Vec4 handleColor{1.0f, 1.0f, 1.0f, 1.0f};
    ecs::Entity target;
    std::string message;
};

/// Check box / switch. Clicking flips `isOn` (toggles sharing a non-empty `group` act as radio
/// buttons: one on at a time). A change calls onValueChanged(1 or 0) on the entity's script and
/// target:message(toggle, value). Drawn as a box (with a mark when on) at the left of the
/// element; add a UIText child for its label.
struct UIToggleComponent {
    bool isOn = false;
    bool interactable = true;
    std::string group;
    math::Vec4 boxColor{0.16f, 0.18f, 0.22f, 1.0f};
    math::Vec4 checkColor{0.98f, 0.55f, 0.16f, 1.0f};
    ecs::Entity target;
    std::string message;
};

/// Text entry field (with a UIText on the same entity showing the text or the placeholder).
/// Clicking focuses it (on-screen keyboard on phones); typing calls onTextChanged(text), Enter
/// calls onSubmit(text) and target:message(field, text).
struct UIInputFieldComponent {
    std::string text;
    std::string placeholder = "Saisir...";
    u32 maxLength = 64;
    bool numeric = false;   ///< Digits, '-' and '.' only.
    bool password = false;  ///< Shown as '*'.
    bool multiline = false; ///< Enter starts a new line (no submit); set the UIText to wrap, top aligned.
    bool interactable = true;
    math::Vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec4 placeholderColor{1.0f, 1.0f, 1.0f, 0.4f};
    ecs::Entity target;
    std::string message;
};

/// Scrolling area: its children are the content, moved by `scroll` (canvas units) and clipped to
/// the element. Mouse wheel, or drag with a finger / the mouse (a drag cancels the press of a
/// button inside). Add a UILayout to stack the content.
struct UIScrollViewComponent {
    bool vertical = true;
    bool horizontal = false;
    math::Vec2 scroll{0.0f, 0.0f};
    float wheelSpeed = 60.0f;
    bool showScrollbar = true;
    math::Vec4 scrollbarColor{1.0f, 1.0f, 1.0f, 0.35f};
};

enum class UILayoutType : i32 { Vertical = 0, Horizontal = 1, Grid = 2 };
enum class UIAlign : i32 { Start = 0, Center = 1, End = 2 };

/// Arranges the children (with a UIRect) in a column, a row or a grid; their anchors and
/// positions are ignored, their sizes are kept (or stretched with expandChildren).
struct UILayoutComponent {
    UILayoutType type = UILayoutType::Vertical;
    float spacing = 10.0f;
    math::Vec4 padding{10.0f, 10.0f, 10.0f, 10.0f};  ///< left, top, right, bottom
    UIAlign childAlignment = UIAlign::Start;          ///< Across the main axis (and along it for Grid rows).
    bool expandChildren = false;                      ///< Children fill the cross axis.
    math::Vec2 cellSize{100.0f, 100.0f};              ///< Grid.
};

enum class UIFillDirection : i32 { LeftToRight = 0, RightToLeft = 1, BottomToTop = 2, TopToBottom = 3 };

/// Progress / health / loading bar: the fill covers (value - min) / (max - min) of the element.
struct UIProgressBarComponent {
    float value = 0.5f;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    UIFillDirection direction = UIFillDirection::LeftToRight;
    math::Vec4 backgroundColor{0.16f, 0.18f, 0.22f, 1.0f};
    math::Vec4 fillColor{0.35f, 0.85f, 0.35f, 1.0f};
    float inset = 3.0f;  ///< Canvas units between the frame and the fill.
};

/// Drop-down list (option button): shows the selected option; a click opens the list of
/// `options` (separated by ';') over everything; choosing one calls onValueChanged(index) on
/// the entity's script and target:message(dropdown, index).
struct UIDropdownComponent {
    std::string options = "Option 1;Option 2;Option 3";
    i32 selected = 0;
    bool interactable = true;
    std::string font = "assets://fonts/Inter-Regular.ttf";
    float fontSize = 28.0f;
    math::Vec4 boxColor{0.16f, 0.18f, 0.22f, 1.0f};
    math::Vec4 listColor{0.11f, 0.12f, 0.15f, 0.98f};
    math::Vec4 highlightColor{0.98f, 0.55f, 0.16f, 1.0f};
    math::Vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
    ecs::Entity target;
    std::string message;
};

/// Tab container: a row of tabs (`tabs` separated by ';') over pages; page i is the i-th child
/// with a UIRect, laid out in the area under the tabs; only the current one is shown. A click on
/// a tab calls onValueChanged(index) and target:message(tabs, index).
struct UITabsComponent {
    std::string tabs = "Tab 1;Tab 2";
    i32 current = 0;
    float headerHeight = 56.0f;
    std::string font = "assets://fonts/Inter-Regular.ttf";
    float fontSize = 26.0f;
    math::Vec4 headerColor{0.11f, 0.12f, 0.15f, 1.0f};
    math::Vec4 activeColor{0.98f, 0.55f, 0.16f, 1.0f};
    math::Vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
    ecs::Entity target;
    std::string message;
};

/// Text shown in a small box near the pointer after it rests `delay` seconds on the element.
struct UITooltipComponent {
    std::string text = "Tooltip";
    float delay = 0.6f;
};

/// List of items (Godot's ItemList and Tree): one row per item of `items` (';'-separated). With
/// `tree`, leading '>' give the depth ("Fruits;>Apple;>Pear;Vegetables;>Leek") and a click on a
/// row's arrow folds / unfolds it. Wheel or drag scrolls it. A click selects a row
/// (onValueChanged(index), index in `items`); a double click activates it (onSubmit(label)).
struct UIItemListComponent {
    std::string items = "Item 1;Item 2;Item 3";
    i32 selected = -1;
    /// Several items at once: Ctrl+click adds / removes one, Shift+click a range from `selected`.
    bool multiSelect = false;
    /// Drag an item onto another row to move it there (lists, not trees): inventories, playlists,
    /// priority orders. onValueChanged(new index) after the move.
    bool reorderable = false;
    std::string selection;           ///< multiSelect: selected indices, comma-separated ("1,4,5").
    bool tree = false;
    std::string collapsed;           ///< Tree: indices of the folded items, comma-separated.
    float itemHeight = 48.0f;
    float scroll = 0.0f;             ///< Canvas units.
    bool interactable = true;
    std::string font = "assets://fonts/Inter-Regular.ttf";
    float fontSize = 26.0f;
    math::Vec4 backgroundColor{0.11f, 0.12f, 0.15f, 0.95f};
    math::Vec4 selectedColor{0.98f, 0.55f, 0.16f, 1.0f};
    math::Vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
    ecs::Entity target;
    std::string message;
};

/// Modal dialog box: while `open`, a dimmed layer covers the screen and blocks the rest of the
/// UI, and a centered panel shows `title`, `text` and its `buttons` (';'-separated). A button
/// closes it (open = false, result = its index): onValueChanged(index), target:message(index).
struct UIDialogComponent {
    bool open = false;
    std::string title = "Titre";
    std::string text = "Message";
    std::string buttons = "OK";
    math::Vec2 size{640.0f, 320.0f};  ///< Canvas units.
    i32 result = -1;                  ///< Index of the last button pressed.
    std::string font = "assets://fonts/Inter-Regular.ttf";
    float fontSize = 28.0f;
    math::Vec4 panelColor{0.11f, 0.12f, 0.15f, 1.0f};
    math::Vec4 dimColor{0.0f, 0.0f, 0.0f, 0.55f};
    math::Vec4 buttonColor{0.98f, 0.55f, 0.16f, 1.0f};
    math::Vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
    ecs::Entity target;
    std::string message;
};

/// Color picker: a saturation / brightness square, a hue bar and an alpha bar (with `alpha`).
/// Pressing or dragging in them changes `color` (onValueChanged with the alpha).
struct UIColorPickerComponent {
    math::Vec4 color{1.0f, 0.5f, 0.2f, 1.0f};
    bool alpha = true;
    bool interactable = true;
    ecs::Entity target;
    std::string message;
    math::Vec3 hsv{0.0f, 0.0f, 0.0f};    ///< Internal: hue kept when the color is gray.
    math::Vec4 hsvOf{-1.0f, 0.0f, 0.0f, 0.0f};  ///< Internal: the color `hsv` was computed from.
};

/// Registers the reflection data (idempotent).
void registerUIComponents();

} // namespace agr
