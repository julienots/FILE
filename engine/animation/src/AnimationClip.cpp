#include "agr/animation/AnimationClip.hpp"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace agr {

using json = nlohmann::json;

namespace {

constexpr const char* kInterpNames[] = {"constant", "linear", "smooth", "custom"};

Interpolation interpolationFromName(const std::string& s) {
    for (u8 i = 0; i < 4; ++i)
        if (s == kInterpNames[i]) return static_cast<Interpolation>(i);
    return Interpolation::Smooth;
}

/// Automatic tangent of key i: Catmull-Rom slope, flattened at local extremes and
/// clamped so the curve does not overshoot its neighbours.
float autoTangent(const std::vector<Keyframe>& k, usize i) {
    if (k.size() < 2 || i == 0 || i + 1 >= k.size()) return 0.0f;
    const Keyframe &a = k[i - 1], &b = k[i], &c = k[i + 1];
    const float d0 = b.value - a.value, d1 = c.value - b.value;
    if (d0 * d1 <= 0.0f) return 0.0f;  // extreme: flat
    const float dt = c.time - a.time;
    if (dt <= 0.0f) return 0.0f;
    float slope = (c.value - a.value) / dt;
    // Clamp (monotone cubic): at most 3x the secant slope on each side.
    const float s0 = d0 / std::max(1e-6f, b.time - a.time), s1 = d1 / std::max(1e-6f, c.time - b.time);
    const float limit = 3.0f * std::min(std::fabs(s0), std::fabs(s1));
    if (std::fabs(slope) > limit) slope = std::copysign(limit, slope);
    return slope;
}

} // namespace

const char* interpolationName(Interpolation i) { return kInterpNames[std::min<u8>(static_cast<u8>(i), 3)]; }

float Curve::evaluate(float t) const {
    if (keys.empty()) return 0.0f;
    if (keys.size() == 1 || t <= keys.front().time) return keys.front().value;
    if (t >= keys.back().time) return keys.back().value;
    // Last key with time <= t.
    const auto it = std::upper_bound(keys.begin(), keys.end(), t, [](float v, const Keyframe& k) { return v < k.time; });
    const usize i = static_cast<usize>(it - keys.begin()) - 1;
    const Keyframe &a = keys[i], &b = keys[i + 1];
    const float span = b.time - a.time;
    if (span <= 0.0f) return b.value;
    const float u = (t - a.time) / span;
    switch (a.interpolation) {
        case Interpolation::Constant: return a.value;
        case Interpolation::Linear: return a.value + (b.value - a.value) * u;
        case Interpolation::Smooth:
        case Interpolation::Custom: {
            const float m0 = (a.interpolation == Interpolation::Custom ? a.outTangent : autoTangent(keys, i)) * span;
            const float m1 = (b.interpolation == Interpolation::Custom ? b.inTangent : autoTangent(keys, i + 1)) * span;
            const float u2 = u * u, u3 = u2 * u;
            return (2 * u3 - 3 * u2 + 1) * a.value + (u3 - 2 * u2 + u) * m0 + (-2 * u3 + 3 * u2) * b.value + (u3 - u2) * m1;
        }
    }
    return a.value;
}

const std::string& Curve::evaluateText(float t) const {
    static const std::string empty;
    if (keys.empty()) return empty;
    const Keyframe* current = &keys.front();
    for (const Keyframe& k : keys) {
        if (k.time > t) break;
        current = &k;
    }
    return current->text;
}

Keyframe& Curve::setKey(float time, float value, Interpolation interpolation) {
    for (Keyframe& k : keys)
        if (std::fabs(k.time - time) < 1e-4f) {
            k.value = value;
            return k;
        }
    Keyframe k;
    k.time = time;
    k.value = value;
    k.interpolation = interpolation;
    const auto it = std::upper_bound(keys.begin(), keys.end(), time, [](float v, const Keyframe& key) { return v < key.time; });
    return *keys.insert(it, k);
}

void Curve::removeKey(usize index) {
    if (index < keys.size()) keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(index));
}

void Curve::sort() {
    std::stable_sort(keys.begin(), keys.end(), [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
}

math::Vec4 TransformTrack::sample(float t) const {
    if (times.empty()) return path == Path::Rotation ? math::Vec4{0, 0, 0, 1} : math::Vec4{0, 0, 0, 0};
    if (t <= times.front() || times.size() == 1) return values.front();
    if (t >= times.back()) return values.back();
    const auto it = std::upper_bound(times.begin(), times.end(), t);
    const usize i = static_cast<usize>(it - times.begin()) - 1;
    if (step) return values[i];
    const float span = times[i + 1] - times[i];
    const float u = span > 0.0f ? (t - times[i]) / span : 0.0f;
    return blendTransform(path, values[i], values[i + 1], u);
}

math::Vec4 blendTransform(TransformTrack::Path path, math::Vec4 a, math::Vec4 b, float t) {
    if (path != TransformTrack::Path::Rotation) return a + (b - a) * t;
    // Shortest arc: q and -q are the same rotation.
    const float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0.0f) b = b * -1.0f;
    const float cosTheta = std::fabs(d);
    math::Vec4 r;
    if (cosTheta > 0.9995f) {
        r = a + (b - a) * t;  // nearly identical: normalized lerp
    } else {
        const float theta = std::acos(std::clamp(cosTheta, -1.0f, 1.0f));
        const float sinTheta = std::sin(theta);
        r = a * (std::sin((1.0f - t) * theta) / sinTheta) + b * (std::sin(t * theta) / sinTheta);
    }
    const float len = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
    return len > 0.0f ? r * (1.0f / len) : math::Vec4{0, 0, 0, 1};
}

std::string AnimationTrack::label() const {
    static const char* xyzw[] = {"x", "y", "z", "w"};
    std::string s = target.empty() ? "" : target + ": ";
    s += component + "." + field;
    if (channel >= 0 && channel < 4) s += std::string(".") + xyzw[channel];
    return s;
}

AnimationTrack* AnimationClip::findTrack(std::string_view target, std::string_view component, std::string_view field, i32 channel) {
    for (AnimationTrack& t : tracks)
        if (t.target == target && t.component == component && t.field == field && t.channel == channel) return &t;
    return nullptr;
}

const AnimationTrack* AnimationClip::findTrack(std::string_view target, std::string_view component, std::string_view field, i32 channel) const {
    return const_cast<AnimationClip*>(this)->findTrack(target, component, field, channel);
}

AnimationTrack& AnimationClip::track(std::string_view target, std::string_view component, std::string_view field, i32 channel) {
    if (AnimationTrack* t = findTrack(target, component, field, channel)) return *t;
    AnimationTrack t;
    t.target = std::string(target);
    t.component = std::string(component);
    t.field = std::string(field);
    t.channel = channel;
    tracks.push_back(std::move(t));
    return tracks.back();
}

void AnimationClip::fitDuration() {
    float end = 0.01f;
    for (const AnimationTrack& t : tracks) end = std::max(end, t.curve.duration());
    for (const AnimationEventKey& e : events) end = std::max(end, e.time);
    for (const TransformTrack& t : transformTracks)
        if (!t.times.empty()) end = std::max(end, t.times.back());
    duration = end;
}

std::string AnimationClip::toJson() const {
    json j;
    j["version"] = 1;
    j["duration"] = duration;
    j["loop"] = loop;
    json tracksJ = json::array();
    for (const AnimationTrack& t : tracks) {
        json tj;
        tj["target"] = t.target;
        tj["component"] = t.component;
        tj["field"] = t.field;
        tj["channel"] = t.channel;
        json keys = json::array();
        for (const Keyframe& k : t.curve.keys) {
            json kj{{"t", k.time}, {"v", k.value}, {"interp", interpolationName(k.interpolation)}};
            if (k.interpolation == Interpolation::Custom) {
                kj["in"] = k.inTangent;
                kj["out"] = k.outTangent;
            }
            if (!k.text.empty()) kj["s"] = k.text;
            keys.push_back(std::move(kj));
        }
        tj["keys"] = std::move(keys);
        tracksJ.push_back(std::move(tj));
    }
    j["tracks"] = std::move(tracksJ);
    json ev = json::array();
    for (const AnimationEventKey& e : events) ev.push_back({{"t", e.time}, {"name", e.name}});
    j["events"] = std::move(ev);
    return j.dump(2);
}

Result<AnimationClip> AnimationClip::fromJson(std::string_view text) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, "animation: invalid JSON");
    if (j.value("version", 1) > 1) return makeError(Error::Code::VersionMismatch, "animation: made by a newer version of AGR");
    AnimationClip c;
    try {
        c.duration = std::max(0.01f, j.value("duration", 1.0f));
        c.loop = j.value("loop", true);
        for (const json& tj : j.value("tracks", json::array())) {
            AnimationTrack t;
            t.target = tj.value("target", "");
            t.component = tj.value("component", "");
            t.field = tj.value("field", "");
            t.channel = std::clamp(tj.value("channel", -1), -1, 3);
            if (t.component.empty() || t.field.empty()) return makeError(Error::Code::ParseError, "animation: track without component / field");
            for (const json& kj : tj.value("keys", json::array())) {
                Keyframe k;
                k.time = std::max(0.0f, kj.value("t", 0.0f));
                k.value = kj.value("v", 0.0f);
                k.interpolation = interpolationFromName(kj.value("interp", "smooth"));
                k.inTangent = kj.value("in", 0.0f);
                k.outTangent = kj.value("out", 0.0f);
                k.text = kj.value("s", "");
                t.curve.keys.push_back(std::move(k));
            }
            t.curve.sort();
            c.tracks.push_back(std::move(t));
        }
        for (const json& ej : j.value("events", json::array())) c.events.push_back({std::max(0.0f, ej.value("t", 0.0f)), ej.value("name", "")});
        std::sort(c.events.begin(), c.events.end(), [](const AnimationEventKey& a, const AnimationEventKey& b) { return a.time < b.time; });
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("animation: ") + e.what());
    }
    return c;
}

} // namespace agr
