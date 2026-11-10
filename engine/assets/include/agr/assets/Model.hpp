#pragma once
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "agr/assets/Material.hpp"
#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"
#include "agr/math/Geometry.hpp"

namespace agr {

/// Vertex of imported and built-in 3D meshes: 48 bytes.
struct ModelVertex {
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec4 tangent;  ///< xyz = tangent (U direction), w = bitangent sign (+1 / -1).
    math::Vec2 uv;       ///< v = 0 is the top of the texture.
};
static_assert(sizeof(ModelVertex) == 48);

/// Range of indices drawn with one material.
struct ModelSubmesh {
    u32 firstIndex = 0;
    u32 indexCount = 0;
    i32 material = -1;  ///< Index into ModelData::materials; -1 = default material.
};

struct ModelMesh {
    std::string name;
    std::vector<ModelVertex> vertices;
    std::vector<u32> indices;  ///< Triangles, counter-clockwise front faces.
    std::vector<ModelSubmesh> submeshes;
    math::Aabb bounds;
};

/// Node of the model hierarchy (local transform relative to the parent).
struct ModelNode {
    std::string name;
    i32 parent = -1;  ///< Always lower than this node's index (parents first).
    math::Vec3 position{0.0f};
    math::Quat rotation = math::Quat::identity();
    math::Vec3 scale{1.0f};
    i32 mesh = -1;
};

/// CPU result of an import. Engine conventions: right-handed, +Y up, meters,
/// UV origin at the top-left. Texture URIs are resolved: external files relative
/// to the model, embedded images as "<model uri>#image<N>" (see readEmbeddedImage).
struct ModelData {
    std::vector<ModelMesh> meshes;
    std::vector<ModelNode> nodes;
    std::vector<MaterialData> materials;
    std::vector<std::string> warnings;  ///< Non-fatal: unsupported features that were skipped.

    usize vertexCount() const;
    usize triangleCount() const;
};

/// .gltf, .glb, .obj, .fbx (case-insensitive).
bool isModelFile(std::string_view name);

/// Reads and imports a model through the VFS (dispatch on the extension).
Result<ModelData> importModel(VirtualFileSystem& vfs, std::string_view uri);
/// glTF 2.0 (.gltf with embedded / external buffers, or .glb). `vfs` resolves external files.
Result<ModelData> importGltf(std::span<const u8> bytes, std::string_view uri, VirtualFileSystem* vfs);
/// Wavefront OBJ with its MTL library (read through `vfs` when given).
Result<ModelData> importObj(std::string_view text, std::string_view uri, VirtualFileSystem* vfs);
/// Autodesk FBX (binary and ASCII), converted to the engine axes and to meters.
Result<ModelData> importFbx(std::span<const u8> bytes, std::string_view uri);

/// Encoded bytes (PNG, JPEG...) of an image embedded in a model file, from a URI
/// "<model uri>#image<N>" produced by the importers.
Result<ByteBuffer> readEmbeddedImage(VirtualFileSystem& vfs, std::string_view uri);

/// Area-weighted smooth normals.
void generateNormals(ModelMesh& mesh);
/// Per-vertex tangents from the UV layout (needed by normal maps).
void generateTangents(ModelMesh& mesh);
/// Recomputes ModelMesh::bounds.
void computeBounds(ModelMesh& mesh);

} // namespace agr
