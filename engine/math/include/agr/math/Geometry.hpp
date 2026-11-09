#pragma once
// Geometric primitives used by culling, picking and physics queries.
#include <limits>

#include "agr/math/Math.hpp"

namespace agr::math {

struct Ray {
    Vec3 origin;
    Vec3 direction;  ///< Normalized.
    constexpr Vec3 at(float t) const { return origin + direction * t; }
};

struct Aabb {
    Vec3 min{std::numeric_limits<float>::max()};
    Vec3 max{-std::numeric_limits<float>::max()};

    static constexpr Aabb fromCenterExtents(Vec3 c, Vec3 e) { return {c - e, c + e}; }
    bool valid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
    constexpr Vec3 center() const { return (min + max) * 0.5f; }
    constexpr Vec3 extents() const { return (max - min) * 0.5f; }
    void expand(Vec3 p) { min = math::min(min, p); max = math::max(max, p); }
    bool contains(Vec3 p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
    }
    bool intersects(const Aabb& o) const {
        return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y && min.z <= o.max.z &&
               max.z >= o.min.z;
    }
    /// Bounds of this box after an affine transform (Arvo's method).
    Aabb transformed(const Mat4& m) const {
        Aabb r;
        r.min = r.max = m.translation();
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                const float a = m.c[j][i] * min[j], b = m.c[j][i] * max[j];
                r.min[i] += std::min(a, b);
                r.max[i] += std::max(a, b);
            }
        }
        return r;
    }
};

/// Slab test. Returns the entry distance or a negative value when missed.
inline float intersect(const Ray& ray, const Aabb& box) {
    float tmin = 0.0f, tmax = std::numeric_limits<float>::max();
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(ray.direction[i]) < kEpsilon) {
            if (ray.origin[i] < box.min[i] || ray.origin[i] > box.max[i]) return -1.0f;
            continue;
        }
        const float inv = 1.0f / ray.direction[i];
        float t0 = (box.min[i] - ray.origin[i]) * inv, t1 = (box.max[i] - ray.origin[i]) * inv;
        if (t0 > t1) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmin > tmax) return -1.0f;
    }
    return tmin;
}

/// Plane: dot(normal, p) + d = 0.
struct Plane {
    Vec3 normal{0, 1, 0};
    float d = 0;
    float distance(Vec3 p) const { return dot(normal, p) + d; }
    Plane normalized() const { const float l = length(normal); return {normal / l, d / l}; }
};

/// View frustum extracted from a view-projection matrix (Gribb/Hartmann).
struct Frustum {
    Plane planes[6];  // left, right, bottom, top, near, far

    static Frustum fromMatrix(const Mat4& vp, Mat4::DepthRange range = Mat4::DepthRange::ZeroToOne) {
        auto row = [&](int r) { return Vec4{vp.c[0][r], vp.c[1][r], vp.c[2][r], vp.c[3][r]}; };
        const Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
        auto mk = [](Vec4 v) { return Plane{{v.x, v.y, v.z}, v.w}.normalized(); };
        Frustum f;
        f.planes[0] = mk(r3 + r0);
        f.planes[1] = mk(r3 - r0);
        f.planes[2] = mk(r3 + r1);
        f.planes[3] = mk(r3 - r1);
        f.planes[4] = range == Mat4::DepthRange::ZeroToOne ? mk(r2) : mk(r3 + r2);
        f.planes[5] = mk(r3 - r2);
        return f;
    }
    /// Conservative test: false only when the box is fully outside.
    bool intersects(const Aabb& box) const {
        for (const Plane& p : planes) {
            const Vec3 positive{p.normal.x >= 0 ? box.max.x : box.min.x, p.normal.y >= 0 ? box.max.y : box.min.y,
                                p.normal.z >= 0 ? box.max.z : box.min.z};
            if (p.distance(positive) < 0.0f) return false;
        }
        return true;
    }
};

} // namespace agr::math
