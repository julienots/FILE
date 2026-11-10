#include "agr/render/Primitives.hpp"

#include <cmath>

namespace agr {

using math::Vec2;
using math::Vec3;

namespace {

void addFace(MeshData& m, Vec3 n, Vec3 t, float halfSize, bool centreOnOrigin) {
    const Vec3 b = math::cross(n, t);  // guarantees counter-clockwise winding seen from +n
    const Vec3 c = centreOnOrigin ? Vec3{} : n * halfSize;
    const auto base = static_cast<u32>(m.vertices.size());
    const math::Vec4 tangent(t, 1.0f);
    m.vertices.push_back({c + (-t - b) * halfSize, n, tangent, {0, 1}});
    m.vertices.push_back({c + (t - b) * halfSize, n, tangent, {1, 1}});
    m.vertices.push_back({c + (t + b) * halfSize, n, tangent, {1, 0}});
    m.vertices.push_back({c + (-t + b) * halfSize, n, tangent, {0, 0}});
    for (u32 i : {0u, 1u, 2u, 0u, 2u, 3u}) m.indices.push_back(base + i);
}

/// One submesh covering every index (default material slot).
void finish(MeshData& m) {
    m.submeshes = {{0, static_cast<u32>(m.indices.size()), -1}};
    computeBounds(m);
}

} // namespace

MeshData makeCube() {
    MeshData m;
    addFace(m, {1, 0, 0}, {0, 0, -1}, 0.5f, false);
    addFace(m, {-1, 0, 0}, {0, 0, 1}, 0.5f, false);
    addFace(m, {0, 1, 0}, {1, 0, 0}, 0.5f, false);
    addFace(m, {0, -1, 0}, {1, 0, 0}, 0.5f, false);
    addFace(m, {0, 0, 1}, {1, 0, 0}, 0.5f, false);
    addFace(m, {0, 0, -1}, {-1, 0, 0}, 0.5f, false);
    finish(m);
    return m;
}

MeshData makePlane() {
    MeshData m;
    addFace(m, {0, 1, 0}, {1, 0, 0}, 0.5f, true);
    finish(m);
    return m;
}

MeshData makeQuad() {
    MeshData m;
    addFace(m, {0, 0, 1}, {1, 0, 0}, 0.5f, true);
    finish(m);
    return m;
}

MeshData makeSphere(u32 segments, u32 rings) {
    MeshData m;
    segments = std::max(segments, 3u);
    rings = std::max(rings, 2u);
    for (u32 i = 0; i <= rings; ++i) {
        const float theta = math::kPi * static_cast<float>(i) / static_cast<float>(rings);
        for (u32 j = 0; j <= segments; ++j) {
            const float phi = 2.0f * math::kPi * static_cast<float>(j) / static_cast<float>(segments);
            const Vec3 n{std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
            m.vertices.push_back({n * 0.5f, n, {}, {static_cast<float>(j) / segments, static_cast<float>(i) / rings}});
        }
    }
    const u32 stride = segments + 1;
    for (u32 i = 0; i < rings; ++i) {
        for (u32 j = 0; j < segments; ++j) {
            const u32 a = i * stride + j;
            const u32 b = a + stride;
            for (u32 idx : {a, a + 1, b, a + 1, b + 1, b}) m.indices.push_back(idx);
        }
    }
    generateTangents(m);
    finish(m);
    return m;
}

MeshData makePrimitive(PrimitiveMesh type) {
    switch (type) {
        case PrimitiveMesh::Cube: return makeCube();
        case PrimitiveMesh::Sphere: return makeSphere();
        case PrimitiveMesh::Plane: return makePlane();
        case PrimitiveMesh::Quad: return makeQuad();
    }
    return makeCube();
}

} // namespace agr
