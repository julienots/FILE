#pragma once
#include <string>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

enum class Interpolation : u8 {
    Constant = 0,  ///< Holds the value until the next key (steps).
    Linear = 1,
    Smooth = 2,    ///< Cubic with automatic tangents (no overshoot at extremes).
    Custom = 3,    ///< Cubic with the key's own tangents (curve editor).
};
const char* interpolationName(Interpolation i);

/// One key of a curve. `interpolation` applies to the segment that starts at this key.
struct Keyframe {
    float time = 0.0f;
    float value = 0.0f;
    Interpolation interpolation = Interpolation::Smooth;
    float inTangent = 0.0f;   ///< Slope (value per second) arriving at the key (Custom).
    float outTangent = 0.0f;  ///< Slope leaving the key (Custom).
    std::string text;         ///< Value of string fields (always stepped).
};

/// A scalar function of time defined by keys sorted by time.
struct Curve {
    std::vector<Keyframe> keys;

    bool empty() const { return keys.empty(); }
    float evaluate(float time) const;
    /// String value at `time` (step), for string fields.
    const std::string& evaluateText(float time) const;
    /// Inserts or replaces (same time within 1e-4 s) a key; keeps the keys sorted.
    Keyframe& setKey(float time, float value, Interpolation interpolation = Interpolation::Smooth);
    void removeKey(usize index);
    void sort();
    float duration() const { return keys.empty() ? 0.0f : keys.back().time; }
};

/// Animates one channel of one reflected field of an entity relative to the animated one.
///  * target: path of names below the animator's entity ("" = itself, "Arm/Hand").
///  * component / field: reflection names ("Transform", "position").
///  * channel: vector / color component (0..3) or -1 for scalars, booleans, integers, enums
///    and strings. Rotations (quaternions) are animated as Euler angles in degrees (0..2).
struct AnimationTrack {
    std::string target;
    std::string component;
    std::string field;
    i32 channel = -1;
    Curve curve;

    /// "Arm/Hand: Transform.position.x"
    std::string label() const;
};

/// Sampled transform animation of one entity (imported skeletal / node animations).
/// Rotations are quaternions interpolated along the shortest arc.
struct TransformTrack {
    enum class Path : u8 { Translation, Rotation, Scale };
    std::string target;  ///< Path below the animated entity (as AnimationTrack::target).
    Path path = Path::Translation;
    bool step = false;
    std::vector<float> times;
    std::vector<math::Vec4> values;  ///< xyz, or quaternion xyzw.

    math::Vec4 sample(float time) const;
};
/// Blends two samples of the same path (lerp, or shortest-arc normalized lerp for rotations).
math::Vec4 blendTransform(TransformTrack::Path path, math::Vec4 a, math::Vec4 b, float t);

/// Named moment of a clip; scripts receive onAnimationEvent(name).
struct AnimationEventKey {
    float time = 0.0f;
    std::string name;
};

/// A property animation (.agranim): curves on component fields plus events.
struct AnimationClip {
    float duration = 1.0f;  ///< Seconds; keys past the end are ignored when looping.
    bool loop = true;
    std::vector<AnimationTrack> tracks;
    std::vector<TransformTrack> transformTracks;  ///< From imported models ("model.glb#Clip"); not saved in .agranim.
    std::vector<AnimationEventKey> events;

    AnimationTrack* findTrack(std::string_view target, std::string_view component, std::string_view field, i32 channel);
    const AnimationTrack* findTrack(std::string_view target, std::string_view component, std::string_view field, i32 channel) const;
    AnimationTrack& track(std::string_view target, std::string_view component, std::string_view field, i32 channel);
    /// Sets duration to the last key or event (at least 0.01 s).
    void fitDuration();

    std::string toJson() const;
    static Result<AnimationClip> fromJson(std::string_view json);
};

} // namespace agr
