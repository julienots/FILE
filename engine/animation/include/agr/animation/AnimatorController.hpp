#pragma once
#include <string>
#include <string_view>
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
/// One motion of a blend state: a clip and the parameter value where it plays alone.
/// In a 2D blend state (blend space), `position` is the point of the two parameters where it plays alone.
struct BlendMotion {
    std::string clip;
    float threshold = 0.0f;
    math::Vec2 position{0.0f, 0.0f};
};

struct AnimatorState {
    std::string name;
    std::string clip;        ///< .agranim asset, optional.
    std::string spriteClip;  ///< Sprite sheet clip name, optional.
    float speed = 1.0f;
    math::Vec2 editorPosition{0.0f, 0.0f};  ///< Node position in the graph editor.
    /// Blend state (1D blend tree, e.g. Idle / Walk / Run by "speed"): when `motions` is not
    /// empty, the two motions around the parameter's value are mixed, their phases kept in step.
    std::string blendParameter;
    /// 2D blend state (blend space, e.g. strafing by "moveX" / "moveY"): motions are placed at
    /// `position`; the ones around the point of the two parameters are mixed (gradient band
    /// interpolation), their phases kept in step.
    std::string blendParameterY;
    std::vector<BlendMotion> motions;  ///< 1D: sorted by threshold when loaded.
    bool isBlend() const { return !motions.empty(); }
    bool isBlend2D() const { return isBlend() && !blendParameterY.empty(); }
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

/// An extra state machine played on top of the base one (upper body that shoots while the legs
/// run, a face, a hit reaction): its values replace the ones below with `weight` (0..1, changed
/// by scripts at run time), only on the objects of its `mask`.
struct AnimatorLayer {
    std::string name;
    float weight = 1.0f;
    /// Additive: the layer adds its clips' change from their first frame (breathing, recoil, a
    /// lean, a hit flinch) on top of the layers below instead of replacing them.
    bool additive = false;
    /// Paths below the animated entity ("Torso", "Hips/Spine") whose subtree the layer animates;
    /// empty = everything.
    std::vector<std::string> mask;
    std::vector<AnimatorState> states;
    std::vector<AnimatorTransition> transitions;
    std::string entry;

    /// True when a track targeting `path` is inside the mask.
    bool masks(std::string_view path) const;
};

/// State machine asset (.agrctrl).
struct AnimatorController {
    std::vector<AnimatorParameter> parameters;
    std::vector<AnimatorState> states;
    std::vector<AnimatorTransition> transitions;
    std::string entry;  ///< Initial state (default: the first one).
    std::vector<AnimatorLayer> layers;  ///< Played over the base layer (the states above), in order.

    const AnimatorState* findState(std::string_view name) const;
    const AnimatorParameter* findParameter(std::string_view name) const;
    /// Checks references (states, parameters); returns the problems found.
    std::vector<std::string> validate() const;

    std::string toJson() const;
    static Result<AnimatorController> fromJson(std::string_view json);
};

} // namespace agr
