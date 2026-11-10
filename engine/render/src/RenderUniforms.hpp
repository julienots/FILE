#pragma once
// std140 uniform layouts shared by the renderer sources (must match Shaders*.inl).
#include <cmath>

#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr::detail {

struct FrameUniforms {
    math::Mat4 viewProj;
    math::Mat4 invViewProj;
    math::Vec4 cameraPos;
    math::Vec4 lightDir;
    math::Vec4 lightColor;
    math::Vec4 ambient;
    math::Vec4 skyColor;
    math::Vec4 groundColor;
    math::Vec4 horizonColor;
    math::Vec4 fogColor;
    math::Vec4 fogParams;
    math::Mat4 shadowMatrix;
    math::Vec4 shadowParams;
    math::Vec4 shadowParams2;
};
static_assert(sizeof(FrameUniforms) == 368);

struct MaterialUniforms {
    math::Vec4 baseColor;
    math::Vec4 emissive;
    math::Vec4 pbr;
    math::Vec4 uvTransform;
    math::Vec4 flags;
};
static_assert(sizeof(MaterialUniforms) == 80);

inline constexpr u32 kMaxLights3D = 8;
struct LightUniforms {
    math::Vec4 posRange[kMaxLights3D];
    math::Vec4 colorType[kMaxLights3D];
    math::Vec4 spotDir[kMaxLights3D];
    math::Vec4 spotInner[kMaxLights3D];
    i32 count[4];
};
static_assert(sizeof(LightUniforms) == 528);

inline float srgbToLinear(float c) {
    c = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
inline math::Vec3 srgbToLinear(math::Vec4 c) { return {srgbToLinear(c.x), srgbToLinear(c.y), srgbToLinear(c.z)}; }

} // namespace agr::detail
