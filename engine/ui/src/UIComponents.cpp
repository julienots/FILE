#include "agr/ui/UIComponents.hpp"

#include "agr/scene/Reflection.hpp"

namespace agr {

void registerUIComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<UICanvasComponent>()) return;
    reg.add<UICanvasComponent>("UICanvas")
        .category("UI")
        .description("Root of a game UI drawn over the scene; adapts to every screen size.")
        .field("scaleMode", &UICanvasComponent::scaleMode, {.enumEntries = {{"ConstantPixelSize", 0}, {"ScaleWithScreen", 1}}})
        .field("referenceResolution", &UICanvasComponent::referenceResolution, {.tooltip = "Design resolution (ScaleWithScreen)", .speed = 1.0f})
        .field("match", &UICanvasComponent::match, {.tooltip = "0 = follow the width, 1 = follow the height", .rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("pixelScale", &UICanvasComponent::pixelScale, {.tooltip = "Pixels per unit (ConstantPixelSize)", .rangeMin = 0.1f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("sortOrder", &UICanvasComponent::sortOrder, {.tooltip = "Higher canvases are drawn on top"});
    reg.add<UIRectComponent>("UIRect")
        .category("UI")
        .description("Position and size of a UI element relative to its parent (anchors, pivot).")
        .field("anchorMin", &UIRectComponent::anchorMin, {.tooltip = "0,0 = top-left of the parent; 1,1 = bottom-right", .rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("anchorMax", &UIRectComponent::anchorMax, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("pivot", &UIRectComponent::pivot, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("position", &UIRectComponent::position, {.speed = 1.0f})
        .field("size", &UIRectComponent::size, {.tooltip = "Size (or extra size when the anchors are apart)", .speed = 1.0f})
        .field("screenRect", &UIRectComponent::screenRect, {.flags = FieldFlag_Hidden | FieldFlag_Transient | FieldFlag_ReadOnly});
    reg.add<UIImageComponent>("UIImage")
        .category("UI")
        .description("Colored or textured rectangle, with optional 9-slice borders.")
        .field("texture", &UIImageComponent::texture, {.tooltip = "Empty = plain color", .flags = FieldFlag_AssetPath})
        .field("color", &UIImageComponent::color, {.flags = FieldFlag_Color})
        .field("border", &UIImageComponent::border, {.tooltip = "9-slice borders in texture pixels: left, top, right, bottom", .speed = 1.0f})
        .field("borderScale", &UIImageComponent::borderScale, {.rangeMin = 0.01f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("preserveAspect", &UIImageComponent::preserveAspect)
        .field("pixelArt", &UIImageComponent::pixelArt, {.tooltip = "Sharp pixels (nearest filtering)"});
    reg.add<UITextComponent>("UIText")
        .category("UI")
        .description("Text with a TrueType font, aligned in its rectangle.")
        .field("text", &UITextComponent::text)
        .field("font", &UITextComponent::font, {.tooltip = "TrueType font (.ttf)", .flags = FieldFlag_AssetPath})
        .field("fontSize", &UITextComponent::fontSize, {.tooltip = "Line height (canvas units)", .rangeMin = 1.0f, .rangeMax = 500.0f, .speed = 0.5f})
        .field("color", &UITextComponent::color, {.flags = FieldFlag_Color})
        .field("hAlign", &UITextComponent::hAlign, {.enumEntries = {{"Left", 0}, {"Center", 1}, {"Right", 2}}})
        .field("vAlign", &UITextComponent::vAlign, {.enumEntries = {{"Top", 0}, {"Middle", 1}, {"Bottom", 2}}})
        .field("wrap", &UITextComponent::wrap, {.tooltip = "Break lines at the rectangle's width"})
        .field("lineSpacing", &UITextComponent::lineSpacing, {.rangeMin = 0.5f, .rangeMax = 3.0f, .speed = 0.01f});
    reg.add<UIButtonComponent>("UIButton")
        .category("UI")
        .description("Clickable / touchable element: calls onClick() on its script and a function of the target's script.")
        .field("interactable", &UIButtonComponent::interactable)
        .field("normalColor", &UIButtonComponent::normalColor, {.flags = FieldFlag_Color})
        .field("hoverColor", &UIButtonComponent::hoverColor, {.flags = FieldFlag_Color})
        .field("pressedColor", &UIButtonComponent::pressedColor, {.flags = FieldFlag_Color})
        .field("disabledColor", &UIButtonComponent::disabledColor, {.flags = FieldFlag_Color})
        .field("target", &UIButtonComponent::target, {.tooltip = "Entity whose script receives the message"})
        .field("message", &UIButtonComponent::message, {.tooltip = "Function of the target's script, called as target:message(button)"});
    reg.add<UISliderComponent>("UISlider")
        .category("UI")
        .description("Horizontal slider: calls onValueChanged(value) on its script and a function of the target's script.")
        .field("value", &UISliderComponent::value, {.speed = 0.01f})
        .field("minValue", &UISliderComponent::minValue, {.speed = 0.01f})
        .field("maxValue", &UISliderComponent::maxValue, {.speed = 0.01f})
        .field("wholeNumbers", &UISliderComponent::wholeNumbers)
        .field("interactable", &UISliderComponent::interactable)
        .field("trackColor", &UISliderComponent::trackColor, {.flags = FieldFlag_Color})
        .field("fillColor", &UISliderComponent::fillColor, {.flags = FieldFlag_Color})
        .field("handleColor", &UISliderComponent::handleColor, {.flags = FieldFlag_Color})
        .field("target", &UISliderComponent::target)
        .field("message", &UISliderComponent::message, {.tooltip = "Called as target:message(slider, value)"});
    reg.add<UILayoutComponent>("UILayout")
        .category("UI")
        .description("Arranges the child elements in a column, a row or a grid.")
        .field("type", &UILayoutComponent::type, {.enumEntries = {{"Vertical", 0}, {"Horizontal", 1}, {"Grid", 2}}})
        .field("spacing", &UILayoutComponent::spacing, {.rangeMin = 0.0f, .rangeMax = 1000.0f, .speed = 0.5f})
        .field("padding", &UILayoutComponent::padding, {.tooltip = "left, top, right, bottom", .speed = 0.5f})
        .field("childAlignment", &UILayoutComponent::childAlignment, {.enumEntries = {{"Start", 0}, {"Center", 1}, {"End", 2}}})
        .field("expandChildren", &UILayoutComponent::expandChildren, {.tooltip = "Children fill the width (column) or height (row)"})
        .field("cellSize", &UILayoutComponent::cellSize, {.tooltip = "Grid cell size", .speed = 1.0f});
}

} // namespace agr
