#pragma once
// AGR math library: column-major matrices, right-handed coordinates (+Y up,
// camera looks down -Z), radians everywhere. Matches the GL / Vulkan convention
// used by the renderer (clip-space depth range selectable per backend).
#include <algorithm>
#include <cmath>
#include <numbers>

namespace agr::math {

inline constexpr float kPi = std::numbers::pi_v<float>;
inline constexpr float kEpsilon = 1e-6f;

constexpr float radians(float degrees) { return degrees * (kPi / 180.0f); }
constexpr float degrees(float radians) { return radians * (180.0f / kPi); }
template <typename T> constexpr T lerp(T a, T b, float t) { return a + (b - a) * t; }
inline bool nearlyEqual(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

// --------------------------------------------------------------------- Vec2
struct Vec2 {
    float x = 0, y = 0;
    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}
    constexpr explicit Vec2(float s) : x(s), y(s) {}
    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(Vec2 o) const { return {x * o.x, y * o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    constexpr Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    constexpr Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    constexpr Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    constexpr bool operator==(const Vec2&) const = default;
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
};
constexpr Vec2 operator*(float s, Vec2 v) { return v * s; }
constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
constexpr float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline float length(Vec2 v) { return std::sqrt(dot(v, v)); }
inline Vec2 normalize(Vec2 v) { const float l = length(v); return l > kEpsilon ? v / l : Vec2{}; }

// --------------------------------------------------------------------- Vec3
struct Vec3 {
    float x = 0, y = 0, z = 0;
    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    constexpr explicit Vec3(float s) : x(s), y(s), z(s) {}
    constexpr Vec3(Vec2 v, float z_) : x(v.x), y(v.y), z(z_) {}
    constexpr Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(Vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
    constexpr Vec3 operator/(Vec3 o) const { return {x / o.x, y / o.y, z / o.z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3& operator-=(Vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    constexpr bool operator==(const Vec3&) const = default;
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
    constexpr Vec2 xy() const { return {x, y}; }

    static constexpr Vec3 zero() { return {0, 0, 0}; }
    static constexpr Vec3 one() { return {1, 1, 1}; }
    static constexpr Vec3 up() { return {0, 1, 0}; }
    static constexpr Vec3 right() { return {1, 0, 0}; }
    static constexpr Vec3 forward() { return {0, 0, -1}; }
};
constexpr Vec3 operator*(float s, Vec3 v) { return v * s; }
constexpr float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
constexpr float lengthSquared(Vec3 v) { return dot(v, v); }
inline Vec3 normalize(Vec3 v) { const float l = length(v); return l > kEpsilon ? v / l : Vec3{}; }
inline Vec3 min(Vec3 a, Vec3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline Vec3 max(Vec3 a, Vec3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline bool nearlyEqual(Vec3 a, Vec3 b, float eps = 1e-4f) {
    return nearlyEqual(a.x, b.x, eps) && nearlyEqual(a.y, b.y, eps) && nearlyEqual(a.z, b.z, eps);
}

// --------------------------------------------------------------------- Vec4
struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    constexpr Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vec4(Vec3 v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    constexpr Vec4 operator+(Vec4 o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    constexpr Vec4 operator-(Vec4 o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    constexpr Vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    constexpr bool operator==(const Vec4&) const = default;
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
    constexpr Vec3 xyz() const { return {x, y, z}; }
};
constexpr float dot(Vec4 a, Vec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

/// Linear RGBA color.
using Color = Vec4;

// --------------------------------------------------------------------- Quat
struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
    constexpr Quat() = default;
    constexpr Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    static constexpr Quat identity() { return {}; }

    static Quat fromAxisAngle(Vec3 axis, float angle) {
        const Vec3 n = normalize(axis);
        const float s = std::sin(angle * 0.5f);
        return {n.x * s, n.y * s, n.z * s, std::cos(angle * 0.5f)};
    }
    /// Euler angles (radians) applied in Y (yaw), X (pitch), Z (roll) order.
    static Quat fromEuler(Vec3 e) {
        return fromAxisAngle({0, 1, 0}, e.y) * fromAxisAngle({1, 0, 0}, e.x) * fromAxisAngle({0, 0, 1}, e.z);
    }

    constexpr Quat operator*(const Quat& q) const {
        return {w * q.x + x * q.w + y * q.z - z * q.y, w * q.y - x * q.z + y * q.w + z * q.x,
                w * q.z + x * q.y - y * q.x + z * q.w, w * q.w - x * q.x - y * q.y - z * q.z};
    }
    constexpr Vec3 operator*(Vec3 v) const {
        const Vec3 u{x, y, z};
        const Vec3 t = cross(u, v) * 2.0f;
        return v + t * w + cross(u, t);
    }
    constexpr bool operator==(const Quat&) const = default;
    constexpr Quat conjugate() const { return {-x, -y, -z, w}; }

    /// Inverse of fromEuler (YXZ order). Returns radians.
    Vec3 toEuler() const {
        // Rotation matrix elements needed for YXZ decomposition.
        const float m12 = 2.0f * (y * z - w * x);
        const float pitch = std::asin(std::clamp(-m12, -1.0f, 1.0f));
        float yaw, roll;
        if (std::fabs(m12) < 0.9999f) {
            yaw = std::atan2(2.0f * (x * z + w * y), 1.0f - 2.0f * (x * x + y * y));
            roll = std::atan2(2.0f * (x * y + w * z), 1.0f - 2.0f * (x * x + z * z));
        } else {  // gimbal lock
            yaw = std::atan2(-2.0f * (x * z - w * y), 1.0f - 2.0f * (y * y + z * z));
            roll = 0.0f;
        }
        return {pitch, yaw, roll};
    }
};
constexpr float dot(Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline Quat normalize(Quat q) {
    const float l = std::sqrt(dot(q, q));
    return l > kEpsilon ? Quat{q.x / l, q.y / l, q.z / l, q.w / l} : Quat{};
}
inline Quat slerp(Quat a, Quat b, float t) {
    float c = dot(a, b);
    if (c < 0.0f) { b = {-b.x, -b.y, -b.z, -b.w}; c = -c; }
    if (c > 0.9995f) {
        return normalize(Quat{lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t), lerp(a.w, b.w, t)});
    }
    const float theta = std::acos(c);
    const float s = std::sin(theta);
    const float wa = std::sin((1.0f - t) * theta) / s, wb = std::sin(t * theta) / s;
    return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
}
inline bool nearlyEqual(Quat a, Quat b, float eps = 1e-4f) {
    // q and -q represent the same rotation.
    return std::fabs(std::fabs(dot(a, b)) - 1.0f) <= eps;
}

// --------------------------------------------------------------------- Mat4
/// Column-major 4x4 matrix: m[column][row].
struct Mat4 {
    Vec4 c[4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

    static constexpr Mat4 identity() { return {}; }
    Vec4& operator[](int i) { return c[i]; }
    const Vec4& operator[](int i) const { return c[i]; }
    const float* data() const { return &c[0].x; }

    constexpr Vec4 operator*(Vec4 v) const {
        return {c[0].x * v.x + c[1].x * v.y + c[2].x * v.z + c[3].x * v.w,
                c[0].y * v.x + c[1].y * v.y + c[2].y * v.z + c[3].y * v.w,
                c[0].z * v.x + c[1].z * v.y + c[2].z * v.z + c[3].z * v.w,
                c[0].w * v.x + c[1].w * v.y + c[2].w * v.z + c[3].w * v.w};
    }
    constexpr Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int i = 0; i < 4; ++i) r.c[i] = (*this) * o.c[i];
        return r;
    }
    constexpr Vec3 transformPoint(Vec3 p) const {
        const Vec4 r = (*this) * Vec4(p, 1.0f);
        return r.w != 0.0f && r.w != 1.0f ? Vec3{r.x / r.w, r.y / r.w, r.z / r.w} : r.xyz();
    }
    constexpr Vec3 transformVector(Vec3 v) const { return ((*this) * Vec4(v, 0.0f)).xyz(); }
    constexpr Vec3 translation() const { return c[3].xyz(); }

    static constexpr Mat4 translate(Vec3 t) { Mat4 m; m.c[3] = {t.x, t.y, t.z, 1}; return m; }
    static constexpr Mat4 scale(Vec3 s) {
        Mat4 m;
        m.c[0].x = s.x; m.c[1].y = s.y; m.c[2].z = s.z;
        return m;
    }
    static constexpr Mat4 rotate(Quat q) {
        const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
        Mat4 m;
        m.c[0] = {1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0};
        m.c[1] = {2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0};
        m.c[2] = {2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0};
        return m;
    }
    /// translation * rotation * scale
    static constexpr Mat4 trs(Vec3 t, Quat r, Vec3 s) {
        Mat4 m = rotate(r);
        m.c[0] = m.c[0] * s.x;
        m.c[1] = m.c[1] * s.y;
        m.c[2] = m.c[2] * s.z;
        m.c[3] = {t.x, t.y, t.z, 1};
        return m;
    }

    enum class DepthRange { NegativeOneToOne /*OpenGL*/, ZeroToOne /*Vulkan/D3D*/ };

    static Mat4 perspective(float fovY, float aspect, float zNear, float zFar,
                            DepthRange range = DepthRange::ZeroToOne) {
        const float f = 1.0f / std::tan(fovY * 0.5f);
        Mat4 m;
        m.c[0] = {f / aspect, 0, 0, 0};
        m.c[1] = {0, f, 0, 0};
        if (range == DepthRange::ZeroToOne) {
            m.c[2] = {0, 0, zFar / (zNear - zFar), -1};
            m.c[3] = {0, 0, (zNear * zFar) / (zNear - zFar), 0};
        } else {
            m.c[2] = {0, 0, (zFar + zNear) / (zNear - zFar), -1};
            m.c[3] = {0, 0, (2 * zFar * zNear) / (zNear - zFar), 0};
        }
        return m;
    }
    static Mat4 orthographic(float left, float right, float bottom, float top, float zNear, float zFar,
                             DepthRange range = DepthRange::ZeroToOne) {
        Mat4 m;
        m.c[0] = {2 / (right - left), 0, 0, 0};
        m.c[1] = {0, 2 / (top - bottom), 0, 0};
        if (range == DepthRange::ZeroToOne) {
            m.c[2] = {0, 0, -1 / (zFar - zNear), 0};
            m.c[3] = {-(right + left) / (right - left), -(top + bottom) / (top - bottom), -zNear / (zFar - zNear), 1};
        } else {
            m.c[2] = {0, 0, -2 / (zFar - zNear), 0};
            m.c[3] = {-(right + left) / (right - left), -(top + bottom) / (top - bottom), -(zFar + zNear) / (zFar - zNear), 1};
        }
        return m;
    }
    static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
        const Vec3 f = normalize(target - eye);
        const Vec3 s = normalize(cross(f, up));
        const Vec3 u = cross(s, f);
        Mat4 m;
        m.c[0] = {s.x, u.x, -f.x, 0};
        m.c[1] = {s.y, u.y, -f.y, 0};
        m.c[2] = {s.z, u.z, -f.z, 0};
        m.c[3] = {-dot(s, eye), -dot(u, eye), dot(f, eye), 1};
        return m;
    }
};

inline Mat4 transpose(const Mat4& m) {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.c[i][j] = m.c[j][i];
    return r;
}

/// General 4x4 inverse (cofactor expansion). Returns identity for singular matrices.
inline Mat4 inverse(const Mat4& mat) {
    const float* m = mat.data();
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(det) < 1e-12f) return Mat4::identity();
    const float invDet = 1.0f / det;
    Mat4 r;
    float* out = &r.c[0].x;
    for (int i = 0; i < 16; ++i) out[i] = inv[i] * invDet;
    return r;
}

/// Splits an affine matrix into translation / rotation / scale (no shear).
inline void decompose(const Mat4& m, Vec3& t, Quat& r, Vec3& s) {
    t = m.c[3].xyz();
    Vec3 cx = m.c[0].xyz(), cy = m.c[1].xyz(), cz = m.c[2].xyz();
    s = {length(cx), length(cy), length(cz)};
    if (dot(cross(cx, cy), cz) < 0.0f) s.x = -s.x;  // mirrored
    if (std::fabs(s.x) > kEpsilon) cx = cx / s.x;
    if (std::fabs(s.y) > kEpsilon) cy = cy / s.y;
    if (std::fabs(s.z) > kEpsilon) cz = cz / s.z;
    // Rotation matrix -> quaternion (Shepperd's method).
    const float trace = cx.x + cy.y + cz.z;
    if (trace > 0.0f) {
        const float k = std::sqrt(trace + 1.0f) * 2.0f;
        r = {(cy.z - cz.y) / k, (cz.x - cx.z) / k, (cx.y - cy.x) / k, 0.25f * k};
    } else if (cx.x > cy.y && cx.x > cz.z) {
        const float k = std::sqrt(1.0f + cx.x - cy.y - cz.z) * 2.0f;
        r = {0.25f * k, (cy.x + cx.y) / k, (cz.x + cx.z) / k, (cy.z - cz.y) / k};
    } else if (cy.y > cz.z) {
        const float k = std::sqrt(1.0f + cy.y - cx.x - cz.z) * 2.0f;
        r = {(cy.x + cx.y) / k, 0.25f * k, (cz.y + cy.z) / k, (cz.x - cx.z) / k};
    } else {
        const float k = std::sqrt(1.0f + cz.z - cx.x - cy.y) * 2.0f;
        r = {(cz.x + cx.z) / k, (cz.y + cy.z) / k, 0.25f * k, (cx.y - cy.x) / k};
    }
    r = normalize(r);
}

} // namespace agr::math
