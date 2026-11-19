#pragma once
#include <vector>

#include "agr/core/Types.hpp"
#include "agr/math/Geometry.hpp"
#include "agr/math/Math.hpp"

namespace agr {

/// Software occlusion culling: occluder shapes (simple boxes and quads placed on walls,
/// buildings, terrain features) are rasterized on the CPU into a small depth buffer, then
/// the screen rectangle of each object's bounding box is tested against it. An object is
/// hidden only when every pixel of its rectangle is covered by an occluder nearer than the
/// object's nearest point (conservative: when in doubt, it is drawn).
class OcclusionBuffer {
public:
    /// Starts a frame. `zeroToOne`: clip-space depth range of `viewProj` (Vulkan / D3D style),
    /// else -1..1 (OpenGL).
    void begin(const math::Mat4& viewProj, u32 width, u32 height, bool zeroToOne);
    /// World-space triangle (both sides occlude).
    void addTriangle(math::Vec3 a, math::Vec3 b, math::Vec3 c);
    /// Box of half extents `half` centered on the origin of `world`.
    void addBox(const math::Mat4& world, math::Vec3 half);
    /// Rectangle in the XY plane of `world`, half size `half`.
    void addQuad(const math::Mat4& world, math::Vec2 half);
    /// True when the world box is entirely hidden behind occluders.
    bool occluded(const math::Aabb& worldBounds) const;

    bool empty() const { return m_triangles == 0; }
    u32 width() const { return m_width; }
    u32 height() const { return m_height; }
    u32 triangles() const { return m_triangles; }
    /// Fraction of pixels covered by an occluder (tests, debug).
    float coverage() const;
    /// Depth of a pixel (normalized device z, 1 = nothing).
    float depthAt(u32 x, u32 y) const { return m_depth[static_cast<usize>(y) * m_width + x]; }

private:
    struct ClipVertex {
        float x, y, z, w;
    };
    void rasterize(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c);

    math::Mat4 m_viewProj{};
    u32 m_width = 0, m_height = 0;
    bool m_zeroToOne = false;
    u32 m_triangles = 0;
    std::vector<float> m_depth;
};

} // namespace agr
