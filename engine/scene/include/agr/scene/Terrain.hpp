#pragma once
// Heightmap terrain: a square grid of heights centered on its entity (local XZ plane, Y up),
// painted with up to 4 texture layers. Data shared by the renderer, the physics, the
// navigation and the editor brushes.
#include <string>
#include <vector>

#include "agr/core/Types.hpp"
#include "agr/math/Geometry.hpp"
#include "agr/math/Math.hpp"

namespace agr {

struct TerrainComponent {
    float size = 64.0f;          ///< Width and depth in meters (local units), centered on the entity.
    u32 resolution = 129;        ///< Vertices per side (2..1025): spacing = size / (resolution - 1).
    std::vector<i32> heights;    ///< Millimeters, resolution x resolution, row-major from -Z; empty = flat.
    std::vector<i32> splat;      ///< Layer weights per vertex packed as RGBA8 (r = layer 0); empty = layer 0.
    std::string layer0, layer1, layer2, layer3;  ///< Textures (grass, rock, dirt, sand...).
    math::Vec4 tiling{24.0f, 24.0f, 24.0f, 24.0f};  ///< Texture repeats across the terrain, per layer.
    math::Color color{1.0f, 1.0f, 1.0f, 1.0f};
    float roughness = 0.92f;
    bool castShadows = true;
    bool collision = true;       ///< Static collision mesh (3D physics) and navigation source.
};
void registerTerrainComponents();

namespace terrain {

u32 resolution(const TerrainComponent& t);  ///< Clamped to 2..1025.
float spacing(const TerrainComponent& t);
/// Sizes `heights` and `splat` for the resolution (keeps what fits; new cells flat / layer 0).
void ensureData(TerrainComponent& t);
/// Height (local units) of grid vertex (x, z); 0 when out of range or no data.
float sample(const TerrainComponent& t, i32 x, i32 z);
/// Bilinear height at a local position (x, z); clamped at the edges.
float heightAt(const TerrainComponent& t, float x, float z);
math::Vec3 normalAt(const TerrainComponent& t, float x, float z);
/// Local position of grid vertex (x, z).
math::Vec3 vertex(const TerrainComponent& t, i32 x, i32 z);
/// Triangles of the whole grid (local space, counter-clockwise from above). Optional UVs (0..1).
void buildMesh(const TerrainComponent& t, std::vector<math::Vec3>& positions, std::vector<u32>& indices,
               std::vector<math::Vec3>* normals = nullptr, std::vector<math::Vec2>* uvs = nullptr);
math::Aabb localBounds(const TerrainComponent& t);
/// Changes when heights, splat or size change (cache invalidation).
u64 heightsHash(const TerrainComponent& t);
u64 splatHash(const TerrainComponent& t);
/// First hit of a local-space ray with the surface (ray march + refinement).
bool raycast(const TerrainComponent& t, const math::Ray& localRay, float maxDistance, float& distance);

enum class Brush : u8 { Raise, Lower, Smooth, Flatten, Paint };
struct BrushSettings {
    Brush brush = Brush::Raise;
    float radius = 4.0f;     ///< Local units.
    float strength = 1.0f;   ///< Raise / lower: meters per application at the center; smooth / paint: 0..1.
    float targetHeight = 0.0f;  ///< Flatten.
    u32 layer = 1;           ///< Paint.
};
/// Applies a brush centered at local (x, z) with a smooth falloff. Returns false if nothing changed.
bool applyBrush(TerrainComponent& t, math::Vec2 center, const BrushSettings& b);

} // namespace terrain
} // namespace agr
