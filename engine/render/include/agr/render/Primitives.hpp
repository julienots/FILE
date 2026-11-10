#pragma once
#include <vector>

#include "agr/math/Geometry.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr {

/// Vertex of lit meshes: 32 bytes.
struct MeshVertex {
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec2 uv;  ///< v = 0 is the top of the texture.
};
static_assert(sizeof(MeshVertex) == 32);

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<u16> indices;  ///< Counter-clockwise front faces.
    math::Aabb bounds;
};

/// Unit-sized primitives centred on the origin:
/// Cube 1x1x1, Sphere diameter 1, Plane 1x1 on XZ facing +Y, Quad 1x1 on XY facing +Z.
MeshData makePrimitive(PrimitiveMesh type);
MeshData makeCube();
MeshData makeSphere(u32 segments = 32, u32 rings = 16);
MeshData makePlane();
MeshData makeQuad();

} // namespace agr
