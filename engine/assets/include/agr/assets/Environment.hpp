#pragma once
// Image-based lighting data computed on the CPU from an equirectangular (latitude /
// longitude) image: a mip chain prefiltered for increasing roughness (specular reflections)
// and order-2 spherical harmonics of the irradiance (diffuse light).
#include <array>
#include <string>
#include <vector>

#include "agr/assets/Image.hpp"
#include "agr/core/Result.hpp"
#include "agr/math/Math.hpp"

namespace agr {

/// Equirectangular convention (shared with the shaders): u = 0.5 + atan2(d.x, -d.z) / 2pi,
/// v = acos(d.y) / pi, top row (v = 0) looking up, the image center looking toward -Z.
math::Vec3 equirectDirection(float u, float v);
math::Vec2 equirectUv(math::Vec3 direction);

struct EnvironmentMap {
    u32 width = 0, height = 0;  ///< Level 0 (2:1).
    /// RGB float per level; level k is max(1, size >> k) and is blurred for roughness
    /// k / (levels - 1) (level 0: mirror, the sky itself).
    std::vector<std::vector<float>> levels;
    /// Irradiance / pi in SH9 (evaluateIrradiance(n) is the light a white Lambert surface with
    /// normal n reflects; a uniform sky of radiance L gives L).
    std::array<math::Vec3, 9> sh{};
    math::Vec3 average{0.0f};  ///< Mean radiance (solid-angle weighted).
};

/// Builds the chain from a linear image. The image is resampled to a 2:1 power-of-two
/// size, at most `maxWidth` wide; levels stop at 8 x 4 texels.
Result<EnvironmentMap> buildEnvironmentMap(const ImageF& equirect, u32 maxWidth = 2048);

/// Order-2 SH basis (9 values) for a unit direction; same constants as the shaders.
std::array<float, 9> shBasis(math::Vec3 n);
math::Vec3 evaluateIrradiance(const std::array<math::Vec3, 9>& sh, math::Vec3 n);

// ---- cube map skies (converted to the equirectangular layout used everywhere else)
/// Faces in the order +X, -X, +Y, -Y, +Z, -Z (OpenGL cube map convention, seen from inside).
using CubeFaces = std::array<ImageF, 6>;
enum class CubeLayout : u8 { None, HorizontalCross, VerticalCross, Strip6x1, Grid3x2 };
/// Layout of a single image holding the six faces (4:3 cross, 3:4 cross, 6:1 strip, 3:2 grid as
/// Godot's), None for an ordinary 2:1 equirectangular image.
CubeLayout detectCubeLayout(u32 width, u32 height);
/// Cuts the faces out of such an image.
Result<CubeFaces> splitCubeImage(const ImageF& image, CubeLayout layout);
/// Resamples six square faces into an equirectangular image `width` x `width / 2`.
Result<ImageF> cubeToEquirect(const CubeFaces& faces, u32 width);
/// Sky image from a URI: an equirectangular image, a cube layout image, or a .agrcube file
/// ({"px": uri, "nx": uri, "py": ..., "ny": ..., "pz": ..., "nz": ...}).
class VirtualFileSystem;
Result<ImageF> loadSkyImage(VirtualFileSystem& vfs, const std::string& uri);

/// Bilinear sample of a level (u wraps, v clamps).
math::Vec3 sampleEquirect(const std::vector<float>& rgb, u32 width, u32 height, math::Vec2 uv);

} // namespace agr
