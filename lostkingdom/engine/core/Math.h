// AGR Engine — core math (vectors, quaternions, matrices, easing).
// Header-only, no dependencies. Right-handed, Y up. Matrices are column-major (GL convention).
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace agr {

constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = 6.28318530717958647692f;
constexpr float DEG = PI / 180.0f;

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float saturate(float v) { return clampf(v, 0.f, 1.f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float a, float b, float x) { float t = saturate((x - a) / (b - a)); return t * t * (3.f - 2.f * t); }
inline float sign(float v) { return v < 0.f ? -1.f : 1.f; }
inline float wrapAngle(float a) { while (a > PI) a -= TAU; while (a < -PI) a += TAU; return a; }
inline float approach(float cur, float target, float delta) { return cur < target ? std::min(cur + delta, target) : std::max(cur - delta, target); }
// Framerate-independent exponential smoothing: fraction of the way to target after dt with "rate" per second.
inline float damp(float cur, float target, float rate, float dt) { return lerpf(cur, target, 1.f - std::exp(-rate * dt)); }
inline float dampAngle(float cur, float target, float rate, float dt) { return cur + wrapAngle(target - cur) * (1.f - std::exp(-rate * dt)); }

// Easing curves used by animation authoring and UI.
inline float easeInOut(float t) { t = saturate(t); return t < .5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.f) / 2; }
inline float easeOut(float t) { t = saturate(t); return 1 - (1 - t) * (1 - t) * (1 - t); }
inline float easeIn(float t) { t = saturate(t); return t * t * t; }
inline float easeOutBack(float t) { t = saturate(t); const float c1 = 1.70158f, c3 = c1 + 1; return 1 + c3 * std::pow(t - 1, 3.f) + c1 * std::pow(t - 1, 2.f); }
inline float easeOutElastic(float t) { t = saturate(t); if (t == 0 || t == 1) return t; return std::pow(2.f, -10 * t) * std::sin((t * 10 - 0.75f) * (TAU / 3)) + 1; }

struct vec2 {
    float x = 0, y = 0;
    vec2() = default;
    constexpr vec2(float x_, float y_) : x(x_), y(y_) {}
    explicit constexpr vec2(float s) : x(s), y(s) {}
    vec2 operator+(vec2 o) const { return {x + o.x, y + o.y}; }
    vec2 operator-(vec2 o) const { return {x - o.x, y - o.y}; }
    vec2 operator*(float s) const { return {x * s, y * s}; }
    vec2 operator*(vec2 o) const { return {x * o.x, y * o.y}; }
    vec2 operator/(float s) const { return {x / s, y / s}; }
    vec2 operator-() const { return {-x, -y}; }
    vec2& operator+=(vec2 o) { x += o.x; y += o.y; return *this; }
    vec2& operator-=(vec2 o) { x -= o.x; y -= o.y; return *this; }
    vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};
inline float dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(vec2 a) { return std::sqrt(dot(a, a)); }
inline float length2(vec2 a) { return dot(a, a); }
inline vec2 normalize(vec2 a) { float l = length(a); return l > 1e-8f ? a / l : vec2(0, 0); }
inline vec2 lerp(vec2 a, vec2 b, float t) { return a + (b - a) * t; }
inline vec2 perp(vec2 a) { return {-a.y, a.x}; }
inline vec2 rotate(vec2 v, float a) { float c = std::cos(a), s = std::sin(a); return {v.x * c - v.y * s, v.x * s + v.y * c}; }

struct vec3 {
    float x = 0, y = 0, z = 0;
    vec3() = default;
    constexpr vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit constexpr vec3(float s) : x(s), y(s), z(s) {}
    vec3 operator+(vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    vec3 operator-(vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    vec3 operator*(vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
    vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    vec3 operator-() const { return {-x, -y, -z}; }
    vec3& operator+=(vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    vec3& operator-=(vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
    vec2 xz() const { return {x, z}; }
};
inline vec3 operator*(float s, vec3 v) { return v * s; }
inline float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 cross(vec3 a, vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(vec3 a) { return std::sqrt(dot(a, a)); }
inline float length2(vec3 a) { return dot(a, a); }
inline vec3 normalize(vec3 a) { float l = length(a); return l > 1e-8f ? a / l : vec3(0, 1, 0); }
inline vec3 lerp(vec3 a, vec3 b, float t) { return a + (b - a) * t; }
inline vec3 vmin(vec3 a, vec3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline vec3 vmax(vec3 a, vec3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline vec3 xz3(vec2 v, float y = 0) { return {v.x, y, v.y}; }
inline vec3 damp(vec3 c, vec3 t, float rate, float dt) { float k = 1.f - std::exp(-rate * dt); return c + (t - c) * k; }
inline float distXZ(vec3 a, vec3 b) { float dx = a.x - b.x, dz = a.z - b.z; return std::sqrt(dx * dx + dz * dz); }
inline float dist2XZ(vec3 a, vec3 b) { float dx = a.x - b.x, dz = a.z - b.z; return dx * dx + dz * dz; }

struct vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    vec4() = default;
    constexpr vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr vec4(vec3 v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    vec4 operator+(vec4 o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    vec4 operator-(vec4 o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    vec3 xyz() const { return {x, y, z}; }
};
inline vec4 lerp(vec4 a, vec4 b, float t) { return a + (b - a) * t; }

struct quat {
    float x = 0, y = 0, z = 0, w = 1;
    quat() = default;
    constexpr quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    static quat axisAngle(vec3 axis, float angle) {
        axis = normalize(axis); float s = std::sin(angle * .5f);
        return {axis.x * s, axis.y * s, axis.z * s, std::cos(angle * .5f)};
    }
    // Intrinsic rotation order Y (yaw) * X (pitch) * Z (roll), angles in radians.
    static quat euler(float pitchX, float yawY, float rollZ) {
        return axisAngle({0, 1, 0}, yawY) * axisAngle({1, 0, 0}, pitchX) * axisAngle({0, 0, 1}, rollZ);
    }
    quat operator*(const quat& q) const {
        return {w * q.x + x * q.w + y * q.z - z * q.y,
                w * q.y - x * q.z + y * q.w + z * q.x,
                w * q.z + x * q.y - y * q.x + z * q.w,
                w * q.w - x * q.x - y * q.y - z * q.z};
    }
    vec3 rotate(vec3 v) const {
        vec3 u{x, y, z};
        vec3 t = cross(u, v) * 2.f;
        return v + t * w + cross(u, t);
    }
    quat conj() const { return {-x, -y, -z, w}; }
};
inline float dot(quat a, quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline quat normalize(quat q) {
    float l = std::sqrt(dot(q, q));
    return l > 1e-8f ? quat{q.x / l, q.y / l, q.z / l, q.w / l} : quat{};
}
inline quat nlerp(quat a, quat b, float t) {
    if (dot(a, b) < 0) b = {-b.x, -b.y, -b.z, -b.w};
    return normalize(quat{lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t), lerpf(a.w, b.w, t)});
}
inline quat slerp(quat a, quat b, float t) {
    float d = dot(a, b);
    if (d < 0) { b = {-b.x, -b.y, -b.z, -b.w}; d = -d; }
    if (d > 0.9995f) return nlerp(a, b, t);
    float th = std::acos(d), s = std::sin(th);
    float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
    return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
}
// Shortest rotation taking unit vector a onto unit vector b.
inline quat fromTo(vec3 a, vec3 b) {
    a = normalize(a); b = normalize(b);
    float d = dot(a, b);
    if (d > 0.99999f) return {};
    if (d < -0.99999f) {
        vec3 ax = cross({1, 0, 0}, a);
        if (length2(ax) < 1e-6f) ax = cross({0, 1, 0}, a);
        return quat::axisAngle(ax, PI);
    }
    vec3 c = cross(a, b);
    return normalize(quat{c.x, c.y, c.z, 1 + d});
}

struct mat4 {
    float m[16];  // column-major
    mat4() { for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.f : 0.f; }
    float& operator()(int row, int col) { return m[col * 4 + row]; }
    float operator()(int row, int col) const { return m[col * 4 + row]; }
    mat4 operator*(const mat4& o) const {
        mat4 r;
        for (int c = 0; c < 4; c++)
            for (int rr = 0; rr < 4; rr++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += (*this)(rr, k) * o(k, c);
                r(rr, c) = s;
            }
        return r;
    }
    vec4 operator*(vec4 v) const {
        return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
                m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
                m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
                m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w};
    }
    vec3 transformPoint(vec3 p) const { vec4 r = (*this) * vec4(p, 1); return r.xyz(); }
    vec3 transformDir(vec3 p) const { vec4 r = (*this) * vec4(p, 0); return r.xyz(); }
    vec3 translation() const { return {m[12], m[13], m[14]}; }

    static mat4 translate(vec3 t) { mat4 r; r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
    static mat4 scale(vec3 s) { mat4 r; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r; }
    static mat4 rotate(quat q) {
        mat4 r;
        float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z, wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
        r.m[0] = 1 - 2 * (yy + zz); r.m[1] = 2 * (xy + wz); r.m[2] = 2 * (xz - wy);
        r.m[4] = 2 * (xy - wz); r.m[5] = 1 - 2 * (xx + zz); r.m[6] = 2 * (yz + wx);
        r.m[8] = 2 * (xz + wy); r.m[9] = 2 * (yz - wx); r.m[10] = 1 - 2 * (xx + yy);
        return r;
    }
    static mat4 trs(vec3 t, quat q, vec3 s) {
        mat4 r = rotate(q);
        r.m[0] *= s.x; r.m[1] *= s.x; r.m[2] *= s.x;
        r.m[4] *= s.y; r.m[5] *= s.y; r.m[6] *= s.y;
        r.m[8] *= s.z; r.m[9] *= s.z; r.m[10] *= s.z;
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }
    static mat4 perspective(float fovY, float aspect, float zn, float zf) {
        mat4 r; float f = 1.f / std::tan(fovY * .5f);
        r.m[0] = f / aspect; r.m[5] = f; r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1; r.m[14] = 2 * zf * zn / (zn - zf); r.m[15] = 0;
        return r;
    }
    static mat4 ortho(float l, float r_, float b, float t, float zn, float zf) {
        mat4 r;
        r.m[0] = 2 / (r_ - l); r.m[5] = 2 / (t - b); r.m[10] = -2 / (zf - zn);
        r.m[12] = -(r_ + l) / (r_ - l); r.m[13] = -(t + b) / (t - b); r.m[14] = -(zf + zn) / (zf - zn);
        return r;
    }
    static mat4 lookAt(vec3 eye, vec3 target, vec3 up) {
        vec3 f = normalize(target - eye), s = normalize(cross(f, up)), u = cross(s, f);
        mat4 r;
        r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
        r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
        return r;
    }
    mat4 inverse() const {
        const float* a = m; float inv[16];
        inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
        inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
        inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
        inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
        inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
        inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
        inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
        inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
        inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
        inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
        inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
        inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
        inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
        inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
        inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
        inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
        float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
        mat4 r; if (std::fabs(det) < 1e-12f) return r;
        det = 1.f / det; for (int i = 0; i < 16; i++) r.m[i] = inv[i] * det;
        return r;
    }
};

// Axis-aligned bounding box.
struct AABB {
    vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
    void add(vec3 p) { lo = vmin(lo, p); hi = vmax(hi, p); }
    void add(const AABB& b) { lo = vmin(lo, b.lo); hi = vmax(hi, b.hi); }
    bool valid() const { return lo.x <= hi.x; }
    vec3 center() const { return (lo + hi) * .5f; }
    vec3 extent() const { return (hi - lo) * .5f; }
};

// View frustum for culling (planes extracted from view-projection).
struct Frustum {
    vec4 p[6];
    void fromMatrix(const mat4& vp) {
        auto row = [&](int r) { return vec4(vp(r, 0), vp(r, 1), vp(r, 2), vp(r, 3)); };
        vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
        p[0] = r3 + r0; p[1] = r3 - r0; p[2] = r3 + r1; p[3] = r3 - r1; p[4] = r3 + r2; p[5] = r3 - r2;
        for (auto& pl : p) { float l = length(pl.xyz()); pl = pl * (1.f / l); }
    }
    bool visible(const AABB& b) const {
        vec3 c = b.center(), e = b.extent();
        for (auto& pl : p) {
            float d = pl.x * c.x + pl.y * c.y + pl.z * c.z + pl.w;
            float r = e.x * std::fabs(pl.x) + e.y * std::fabs(pl.y) + e.z * std::fabs(pl.z);
            if (d + r < 0) return false;
        }
        return true;
    }
    bool visibleSphere(vec3 c, float rad) const {
        for (auto& pl : p) if (pl.x * c.x + pl.y * c.y + pl.z * c.z + pl.w < -rad) return false;
        return true;
    }
};

// Packed colour helpers.
struct Color8 { uint8_t r, g, b, a; };
inline Color8 toColor8(vec3 c, float a = 1) {
    auto q = [](float v) { return (uint8_t)clampf(v * 255.f + .5f, 0, 255); };
    return {q(c.x), q(c.y), q(c.z), q(a)};
}
inline vec3 hexColor(uint32_t hex) { return {((hex >> 16) & 255) / 255.f, ((hex >> 8) & 255) / 255.f, (hex & 255) / 255.f}; }
inline vec3 mix3(vec3 a, vec3 b, float t) { return lerp(a, b, t); }

}  // namespace agr
