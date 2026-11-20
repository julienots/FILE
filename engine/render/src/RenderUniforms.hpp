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
    math::Mat4 shadowMatrix[4];   // world -> atlas uv + depth, per cascade
    math::Vec4 shadowRect[4];     // atlas uv bounds of each cascade (min u, min v, max u, max v)
    math::Vec4 shadowSplits;      // view depth where each cascade ends
    math::Vec4 shadowOffsets;     // normal offset (world units) per cascade
    math::Vec4 shadowView;        // camera forward (xyz)
    math::Vec4 shadowParams;      // cascade count (0 = off), depth bias, blend band (fraction), strength
    math::Vec4 shadowParams2;     // atlas texel size (uv), PCF radius, -, -
    math::Vec4 envSH[9];          // image sky: irradiance / pi (SH9, rgb)
    math::Vec4 envParams;         // image sky ready, intensity, rotation (radians), last mip level
    math::Vec4 time;              // seconds (wrapped every hour), -, 1 / render width, 1 / render height
};
static_assert(sizeof(FrameUniforms) == 848);

struct MaterialUniforms {
    math::Vec4 baseColor;
    math::Vec4 emissive;
    math::Vec4 pbr;
    math::Vec4 uvTransform;
    math::Vec4 flags;        // alpha cutoff, unlit, toon, -
    math::Vec4 toon;         // bands, softness, specular size, rim width
    math::Vec4 toonShadow;   // shadow tint (linear rgb), rim strength
    math::Vec4 rimColor;     // linear rgb, outline width (pixels)
    math::Vec4 outlineColor; // linear rgb, -
    math::Vec4 custom[8];    // custom shader parameters
};
static_assert(sizeof(MaterialUniforms) == 272);

inline constexpr u32 kMaxLights3D = 8;
inline constexpr u32 kMaxDecals = 16;
inline constexpr u32 kMaxDecalTextures = 4;
inline constexpr u32 kMaxSpotShadows = 2;
struct LightUniforms {
    math::Vec4 posRange[kMaxLights3D];
    math::Vec4 colorType[kMaxLights3D];
    math::Vec4 spotDir[kMaxLights3D];
    math::Vec4 spotInner[kMaxLights3D];  // cos(inner), shadow slot (-1: none), -, -
    i32 count[4];
    // Local light shadow atlas (4 x 2 tiles): spot lights in tiles 0-1, the point light's cube faces in 2-7.
    math::Mat4 spotShadowMatrix[kMaxSpotShadows];  // world -> atlas uv + depth
    math::Vec4 spotShadowParams;                   // atlas texel size (u, v), PCF on, depth bias
    math::Vec4 spotShadowRect[kMaxSpotShadows];    // atlas uv bounds of each spot tile
    math::Mat4 pointShadowMatrix[6];               // one per cube face (+x, -x, +y, -y, +z, -z)
    math::Vec4 pointShadowRect[6];                 // atlas uv bounds of each face
    // Decals (count[1]): world -> unit box [-0.5, 0.5]^3, tint + opacity, projection direction.
    math::Mat4 decalMatrix[kMaxDecals];
    math::Vec4 decalColor[kMaxDecals];   // linear rgb, opacity
    math::Vec4 decalAxis[kMaxDecals];    // world direction the decal projects along, texture slot (0..3)
    math::Vec4 decalFade[kMaxDecals];    // normal fade, upper fade, lower fade, emission
};
static_assert(sizeof(LightUniforms) == 528 + 128 + 16 + 32 + 384 + 96 + kMaxDecals * (64 + 48));

inline float srgbToLinear(float c) {
    c = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
inline math::Vec3 srgbToLinear(math::Vec4 c) { return {srgbToLinear(c.x), srgbToLinear(c.y), srgbToLinear(c.z)}; }

} // namespace agr::detail
