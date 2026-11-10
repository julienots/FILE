#pragma once
#include <vector>

#include "agr/assets/Model.hpp"
#include "agr/math/Geometry.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr {

/// Built-in meshes use the same layout as imported models (position, normal,
/// tangent, uv: 48 bytes), with one submesh.
using MeshVertex = ModelVertex;
using MeshData = ModelMesh;

/// Unit-sized primitives centred on the origin:
/// Cube 1x1x1, Sphere diameter 1, Plane 1x1 on XZ facing +Y, Quad 1x1 on XY facing +Z.
MeshData makePrimitive(PrimitiveMesh type);
MeshData makeCube();
MeshData makeSphere(u32 segments = 32, u32 rings = 16);
MeshData makePlane();
MeshData makeQuad();

} // namespace agr
