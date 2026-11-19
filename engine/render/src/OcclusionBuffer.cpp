#include "agr/render/OcclusionBuffer.hpp"

#include <algorithm>
#include <cmath>

namespace agr {

using math::Mat4;
using math::Vec2;
using math::Vec3;
using math::Vec4;

void OcclusionBuffer::begin(const Mat4& viewProj, u32 width, u32 height, bool zeroToOne) {
    m_viewProj = viewProj;
    m_width = std::max(width, 1u);
    m_height = std::max(height, 1u);
    m_zeroToOne = zeroToOne;
    m_triangles = 0;
    m_depth.assign(static_cast<usize>(m_width) * m_height, 1.0f);
}

void OcclusionBuffer::addTriangle(Vec3 a, Vec3 b, Vec3 c) {
    auto clip = [&](Vec3 p) {
        const Vec4 v = m_viewProj * Vec4(p.x, p.y, p.z, 1.0f);
        return ClipVertex{v.x, v.y, v.z, v.w};
    };
    // Near plane: z >= 0 (zero-to-one) or z >= -w (OpenGL). Sutherland-Hodgman on one plane.
    auto dist = [&](const ClipVertex& v) { return m_zeroToOne ? v.z : v.z + v.w; };
    const ClipVertex in[3] = {clip(a), clip(b), clip(c)};
    ClipVertex poly[4];
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        const ClipVertex& p = in[i];
        const ClipVertex& q = in[(i + 1) % 3];
        const float dp = dist(p), dq = dist(q);
        if (dp >= 0.0f) poly[n++] = p;
        if ((dp >= 0.0f) != (dq >= 0.0f)) {
            const float t = dp / (dp - dq);
            poly[n++] = {p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t, p.z + (q.z - p.z) * t, p.w + (q.w - p.w) * t};
        }
    }
    if (n < 3) return;
    ++m_triangles;
    for (int i = 1; i + 1 < n; ++i) rasterize(poly[0], poly[i], poly[i + 1]);
}

void OcclusionBuffer::rasterize(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c) {
    struct S {
        float x, y, z;
    };
    auto screen = [&](const ClipVertex& v) {
        const float iw = 1.0f / std::max(v.w, 1e-6f);
        float z = v.z * iw;
        if (!m_zeroToOne) z = z * 0.5f + 0.5f;  // stored as 0..1 in both conventions
        return S{(v.x * iw * 0.5f + 0.5f) * static_cast<float>(m_width), (v.y * iw * 0.5f + 0.5f) * static_cast<float>(m_height), z};
    };
    const S p0 = screen(a), p1 = screen(b), p2 = screen(c);
    const float area = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    if (std::fabs(area) < 1e-8f) return;
    const float invArea = 1.0f / area;
    const int x0 = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
    const int x1 = std::min(static_cast<int>(m_width) - 1, static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
    const int y0 = std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
    const int y1 = std::min(static_cast<int>(m_height) - 1, static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));
    for (int y = y0; y <= y1; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        float* row = m_depth.data() + static_cast<usize>(y) * m_width;
        for (int x = x0; x <= x1; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            // Barycentrics from edge functions; the sign of the area handles both windings.
            const float w0 = ((p1.x - px) * (p2.y - py) - (p1.y - py) * (p2.x - px)) * invArea;
            const float w1 = ((p2.x - px) * (p0.y - py) - (p2.y - py) * (p0.x - px)) * invArea;
            const float w2 = 1.0f - w0 - w1;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            const float z = w0 * p0.z + w1 * p1.z + w2 * p2.z;  // NDC depth is linear in screen space
            if (z < row[x]) row[x] = std::max(z, 0.0f);
        }
    }
}

void OcclusionBuffer::addBox(const Mat4& world, Vec3 h) {
    Vec3 c[8];
    for (int i = 0; i < 8; ++i)
        c[i] = world.transformPoint(Vec3((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z));
    static constexpr int kFaces[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
    for (const auto& f : kFaces) {
        addTriangle(c[f[0]], c[f[1]], c[f[2]]);
        addTriangle(c[f[0]], c[f[2]], c[f[3]]);
    }
}

void OcclusionBuffer::addQuad(const Mat4& world, Vec2 h) {
    const Vec3 a = world.transformPoint(Vec3(-h.x, -h.y, 0.0f)), b = world.transformPoint(Vec3(h.x, -h.y, 0.0f));
    const Vec3 c = world.transformPoint(Vec3(h.x, h.y, 0.0f)), d = world.transformPoint(Vec3(-h.x, h.y, 0.0f));
    addTriangle(a, b, c);
    addTriangle(a, c, d);
}

bool OcclusionBuffer::occluded(const math::Aabb& b) const {
    if (m_triangles == 0 || !b.valid()) return false;
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f, minZ = 1e30f;
    for (int i = 0; i < 8; ++i) {
        const Vec3 p((i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y, (i & 4) ? b.max.z : b.min.z);
        const Vec4 v = m_viewProj * Vec4(p.x, p.y, p.z, 1.0f);
        if ((m_zeroToOne ? v.z : v.z + v.w) <= 0.0f || v.w <= 1e-6f) return false;  // crosses the near plane
        const float iw = 1.0f / v.w;
        float z = v.z * iw;
        if (!m_zeroToOne) z = z * 0.5f + 0.5f;
        const float sx = (v.x * iw * 0.5f + 0.5f) * static_cast<float>(m_width);
        const float sy = (v.y * iw * 0.5f + 0.5f) * static_cast<float>(m_height);
        minX = std::min(minX, sx);
        maxX = std::max(maxX, sx);
        minY = std::min(minY, sy);
        maxY = std::max(maxY, sy);
        minZ = std::min(minZ, z);
    }
    const int x0 = std::max(0, static_cast<int>(std::floor(minX))), x1 = std::min(static_cast<int>(m_width) - 1, static_cast<int>(std::floor(maxX)));
    const int y0 = std::max(0, static_cast<int>(std::floor(minY))), y1 = std::min(static_cast<int>(m_height) - 1, static_cast<int>(std::floor(maxY)));
    if (x0 > x1 || y0 > y1) return false;  // off screen: left to frustum culling
    for (int y = y0; y <= y1; ++y) {
        const float* row = m_depth.data() + static_cast<usize>(y) * m_width;
        for (int x = x0; x <= x1; ++x)
            if (row[x] >= minZ) return false;  // a pixel where the object may show
    }
    return true;
}

float OcclusionBuffer::coverage() const {
    if (m_depth.empty()) return 0.0f;
    usize covered = 0;
    for (float d : m_depth) covered += d < 1.0f ? 1 : 0;
    return static_cast<float>(covered) / static_cast<float>(m_depth.size());
}

} // namespace agr
