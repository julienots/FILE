#pragma once
// FABRIK (Aristidou & Lasenby 2011): forward and backward reaching on joint positions, with a
// bend limit per joint and an optional pole. Shared by the 2D and 3D chain IK.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "agr/math/Math.hpp"

namespace agr::fabrik {

inline math::Vec3 safeNormalize(math::Vec3 v, math::Vec3 fallback) {
    const float l = math::length(v);
    return l > 1e-6f ? v * (1.0f / l) : fallback;
}

/// Turns `dir` toward `ref` so that the angle between them is at most `maxAngle`.
inline math::Vec3 limitAngle(math::Vec3 dir, math::Vec3 ref, float maxAngle) {
    const float c = std::clamp(math::dot(dir, ref), -1.0f, 1.0f);
    const float angle = std::acos(c);
    if (angle <= maxAngle) return dir;
    math::Vec3 axis = math::cross(ref, dir);
    if (math::length(axis) < 1e-6f) {  // opposite: any perpendicular
        axis = math::cross(ref, math::Vec3{0, 1, 0});
        if (math::length(axis) < 1e-6f) axis = math::cross(ref, math::Vec3{1, 0, 0});
    }
    return safeNormalize(math::Quat::fromAxisAngle(math::normalize(axis), maxAngle) * ref, ref);
}

/// Bend limit of a joint (radians): toward the front (the pole side, else up) and sideways. Equal
/// values = a round cone; different ones = an elliptical cone (a knee that bends forward a lot
/// and barely sideways).
struct JointLimit {
    float front = 3.1416f, side = 3.1416f;
    bool round() const { return std::fabs(front - side) < 1e-5f; }
};

/// Turns `dir` toward `ref` so that it stays in the elliptical cone of half angles `front` (along
/// `frontAxis`, perpendicular to ref) and `side` (along ref x frontAxis).
inline math::Vec3 limitEllipse(math::Vec3 dir, math::Vec3 ref, math::Vec3 frontAxis, const JointLimit& lim) {
    const float c = std::clamp(math::dot(dir, ref), -1.0f, 1.0f);
    const float angle = std::acos(c);
    if (angle <= std::min(lim.front, lim.side)) return dir;
    const math::Vec3 sideAxis = math::cross(ref, frontAxis);
    math::Vec3 perp = dir - ref * c;
    if (math::length(perp) < 1e-6f) perp = frontAxis;
    perp = math::normalize(perp);
    const float cf = math::dot(perp, frontAxis), cs = math::dot(perp, sideAxis);
    // Radius of the ellipse in that direction: a b / sqrt((b cos)^2 + (a sin)^2).
    const float a = std::max(lim.front, 1e-4f), b = std::max(lim.side, 1e-4f);
    const float maxAngle = a * b / std::sqrt((b * cf) * (b * cf) + (a * cs) * (a * cs));
    if (angle <= maxAngle) return dir;
    return safeNormalize(ref * std::cos(maxAngle) + perp * std::sin(maxAngle), ref);
}

/// "30, 60x15, 90" (degrees) -> one limit per joint: a number = a round cone, "FxS" = front x side
/// (elliptical cone); empty entries keep `fallback`.
inline std::vector<JointLimit> parseJointLimits(const std::string& text, float fallback) {
    std::vector<JointLimit> out;
    if (text.empty()) return out;
    std::string cur;
    auto deg = [](const std::string& s) { return std::clamp(std::strtof(s.c_str(), nullptr), 0.0f, 180.0f) * 0.017453292f; };
    for (char c : text + ",") {
        if (c == ',' || c == ';') {
            JointLimit l{fallback, fallback};
            if (!cur.empty()) {
                const usize x = cur.find_first_of("xX");
                if (x == std::string::npos) l.front = l.side = deg(cur);
                else l.front = deg(cur.substr(0, x)), l.side = deg(cur.substr(x + 1));
            }
            out.push_back(l);
            cur.clear();
        } else if (c != ' ') {
            cur += c;
        }
    }
    return out;
}

/// Solves in place; `p[0]` stays where it is. Returns the final distance to the target.
/// `limits[k]`: bend limit of joint k + 1 (between bone k and bone k + 1), else `maxAngle`.
inline float solve(std::vector<math::Vec3>& p, math::Vec3 target, int iterations, float maxAngle, const math::Vec3* pole,
                   const std::vector<JointLimit>& limits = {}) {
    const usize n = p.size();
    if (n < 2) return 0.0f;
    std::vector<float> len(n - 1);
    float total = 0.0f;
    for (usize i = 0; i + 1 < n; ++i) total += (len[i] = math::length(p[i + 1] - p[i]));
    const math::Vec3 root = p[0];
    const float tolerance = std::max(total * 1e-4f, 1e-5f);
    // The direction the root bone had: the reference of its bend limit.
    const math::Vec3 rootRef = safeNormalize(p[1] - p[0], {0, 1, 0});
    if (math::length(target - root) >= total) {  // out of reach: stretched toward it
        const math::Vec3 d = safeNormalize(target - root, rootRef);
        for (usize i = 0; i + 1 < n; ++i) p[i + 1] = p[i] + d * len[i];
    } else {
        for (int it = 0; it < iterations; ++it) {
            p[n - 1] = target;  // backward
            for (usize i = n - 1; i-- > 0;) p[i] = p[i + 1] + safeNormalize(p[i] - p[i + 1], {0, -1, 0}) * len[i];
            p[0] = root;  // forward, with the bend limit
            for (usize i = 0; i + 1 < n; ++i) {
                math::Vec3 d = safeNormalize(p[i + 1] - p[i], rootRef);
                const math::Vec3 ref = i == 0 ? d : safeNormalize(p[i] - p[i - 1], d);
                const JointLimit limit = i > 0 && i - 1 < limits.size() ? limits[i - 1] : JointLimit{maxAngle, maxAngle};
                if (i > 0 && limit.round() && limit.front < 3.14159f) {
                    d = limitAngle(d, ref, limit.front);
                } else if (i > 0 && !limit.round()) {
                    // Front: toward the pole (else world up), made perpendicular to the parent bone.
                    const math::Vec3 want = pole ? *pole - p[i] : math::Vec3{0, 1, 0};
                    math::Vec3 front = want - ref * math::dot(want, ref);
                    if (math::length(front) < 1e-5f) front = math::cross(ref, math::Vec3{1, 0, 0});
                    if (math::length(front) < 1e-5f) front = math::cross(ref, math::Vec3{0, 0, 1});
                    d = limitEllipse(d, ref, math::normalize(front), limit);
                }
                p[i + 1] = p[i] + d * len[i];
            }
            if (math::length(p[n - 1] - target) < tolerance) break;
        }
    }
    if (pole)  // each inner joint turns about the line through its neighbors toward the pole
        for (usize i = 1; i + 1 < n; ++i) {
            const math::Vec3 axis = safeNormalize(p[i + 1] - p[i - 1], {0, 1, 0});
            auto project = [&](math::Vec3 v) { return v - axis * math::dot(v, axis); };
            const math::Vec3 j = project(p[i] - p[i - 1]), q = project(*pole - p[i - 1]);
            if (math::length(j) < 1e-5f || math::length(q) < 1e-5f) continue;
            const math::Vec3 jn = math::normalize(j), qn = math::normalize(q);
            const float sign = math::dot(math::cross(jn, qn), axis) < 0.0f ? -1.0f : 1.0f;
            const float angle = std::acos(std::clamp(math::dot(jn, qn), -1.0f, 1.0f)) * sign;
            p[i] = p[i - 1] + math::Quat::fromAxisAngle(axis, angle) * (p[i] - p[i - 1]);
        }
    return math::length(p[n - 1] - target);
}

} // namespace agr::fabrik
