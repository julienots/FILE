#pragma once
#include <string>
#include <string_view>

#include "agr/core/Result.hpp"
#include "agr/math/Math.hpp"

namespace agr {

/// Look of the controls of a canvas (.agrtheme, UICanvas.theme): when set, these colors replace
/// the ones of each control's component, the font replaces the texts' font, and white texts
/// take the `text` color.
struct UITheme {
    std::string font;  ///< Empty: the texts keep their font.
    math::Vec4 text{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec4 panel{0.11f, 0.12f, 0.15f, 0.98f};       ///< Lists, tab headers, tooltips.
    math::Vec4 button{1.0f, 1.0f, 1.0f, 1.0f};          ///< Tints of the buttons' image.
    math::Vec4 buttonHover{0.92f, 0.92f, 0.92f, 1.0f};
    math::Vec4 buttonPressed{0.75f, 0.75f, 0.75f, 1.0f};
    math::Vec4 buttonDisabled{0.6f, 0.6f, 0.6f, 0.5f};
    math::Vec4 accent{0.98f, 0.55f, 0.16f, 1.0f};       ///< Slider fills, checks, bars, selections, active tab.
    math::Vec4 track{0.16f, 0.18f, 0.22f, 1.0f};        ///< Slider tracks, check boxes, bar backgrounds.
    math::Vec4 handle{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec4 field{0.16f, 0.18f, 0.22f, 1.0f};        ///< Drop-down boxes.

    std::string toJson() const;
    static Result<UITheme> fromJson(std::string_view json);
};

} // namespace agr
