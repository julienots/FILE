#pragma once
#include <string>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

enum class ParameterType : u8 { Float = 0, Int = 1, Bool = 2, Trigger = 3 };

struct AnimatorParameter {
    std::string name;
    ParameterType type = ParameterType::Float;
    float defaultValue = 0.0f;  ///< Bool / Trigger: 0 or 1.
};

enum class ConditionOp : u8 { Greater = 0, Less = 1, Equal = 2, NotEqual = 3, IsTrue = 4, IsFalse = 5 };
const char* conditionOpName(ConditionOp op);

struct TransitionCondition {
    std::string parameter;
    ConditionOp op = ConditionOp::IsTrue;  ///< Triggers: IsTrue (consumed by the transition).
    float value = 0.0f;
};

/// A state plays a property clip (.agranim) and / or a sprite clip (SpriteAnimator.clip).
struct AnimatorState {
    std::string name;
    std::string clip;        ///< .agranim asset, optional.
    std::string spriteClip;  ///< Sprite sheet clip name, optional.
    float speed = 1.0f;
    math::Vec2 editorPosition{0.0f, 0.0f};  ///< Node position in the graph editor.
};

/// Goes from `from` ("*" = any state) to `to` when every condition holds and, with
/// exitTime >= 0, once the source clip reached that normalized time (0..1 per loop).
/// `duration` crossfades the two clips' animated values (seconds).
struct AnimatorTransition {
    std::string from;
    std::string to;
    std::vector<TransitionCondition> conditions;
    float exitTime = -1.0f;
    float duration = 0.1f;
};

/// State machine asset (.agrctrl).
struct AnimatorController {
    std::vector<AnimatorParameter> parameters;
    std::vector<AnimatorState> states;
    std::vector<AnimatorTransition> transitions;
    std::string entry;  ///< Initial state (default: the first one).

    const AnimatorState* findState(std::string_view name) const;
    const AnimatorParameter* findParameter(std::string_view name) const;
    /// Checks references (states, parameters); returns the problems found.
    std::vector<std::string> validate() const;

    std::string toJson() const;
    static Result<AnimatorController> fromJson(std::string_view json);
};

} // namespace agr
