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
};

enum class UIHAlign : i32 { Left = 0, Center = 1, Right = 2 };
enum class UIVAlign : i32 { Top = 0, Middle = 1, Bottom = 2 };

/// Text (UTF-8) with a TrueType font. `fontSize` is the line height in canvas units.
struct UITextComponent {
    std::string text = "Text";
    std::string font = "assets://fonts/Inter-Regular.ttf";
    float fontSize = 32.0f;
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    UIHAlign hAlign = UIHAlign::Center;
    UIVAlign vAlign = UIVAlign::Middle;
    bool wrap = true;          ///< Breaks lines at the rectangle's width.
    float lineSpacing = 1.0f;
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
    math::Vec4 trackColor{0.16f, 0.18f, 0.22f, 1.0f};
    math::Vec4 fillColor{0.98f, 0.55f, 0.16f, 1.0f};
    math::Vec4 handleColor{1.0f, 1.0f, 1.0f, 1.0f};
    ecs::Entity target;
    std::string message;
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

/// Registers the reflection data (idempotent).
void registerUIComponents();

} // namespace agr
