// AGR Engine — imported skinned models (.lkm) produced by tools/import_character.py.
// File layout (little endian):
//   "LKM1" or "LKM2" | u32 vertexCount | u32 indexCount | u32 jointCount | f32 height
//   jointCount x vec3 (joint rest positions, model space)
//   vertexCount x vec3 position | vertexCount x 3 x int8 normal | vertexCount x 2 x u16 uv
//   vertexCount x (u8 bone0, u8 bone1, u8 weight0 [, u8 flags in LKM2: 255 = cloth]) | indexCount x u32
// The texture is a separate PNG/JPEG. CPU data is kept so GPU objects can be rebuilt after a context loss.
#pragma once
#include "GL.h"
#include <vector>
#include <string>

namespace agr {

struct SkinnedModelData {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    std::vector<vec3> joints;
    float height = 0;
    std::vector<uint8_t> rgba;
    int texW = 0, texH = 0;
    bool loadLKM(const std::vector<uint8_t>& bytes);
    bool loadTexture(const std::vector<uint8_t>& encoded);
    bool valid() const { return !verts.empty() && !idx.empty(); }
};

}  // namespace agr
