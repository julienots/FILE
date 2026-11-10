#include "agr/assets/Model.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <ufbx.h>

#include "agr/core/Log.hpp"

namespace agr {

using math::Vec2;
using math::Vec3;
using math::Vec4;

namespace {

std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string extensionOf(std::string_view uri) {
    const auto slash = uri.find_last_of('/');
    const auto dot = uri.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return {};
    return lower(uri.substr(dot));
}

/// "scheme://a/b/model.gltf" + "../tex/x.png" -> "scheme://a/tex/x.png".
std::string resolveRelative(std::string_view modelUri, std::string_view relative) {
    if (relative.find("://") != std::string_view::npos) return std::string(relative);
    std::string rel(relative);
    std::replace(rel.begin(), rel.end(), '\\', '/');
    const auto schemeEnd = modelUri.find("://");
    const std::string scheme = schemeEnd == std::string_view::npos ? "" : std::string(modelUri.substr(0, schemeEnd + 3));
    std::string path(schemeEnd == std::string_view::npos ? modelUri : modelUri.substr(schemeEnd + 3));
    const auto slash = path.find_last_of('/');
    path = slash == std::string::npos ? "" : path.substr(0, slash + 1);
    if (!rel.empty() && rel[0] == '/') {  // absolute path written by the exporter: keep the file name only
        rel = rel.substr(rel.find_last_of('/') + 1);
    }
    std::vector<std::string> parts;
    auto split = [&](const std::string& s) {
        usize start = 0;
        while (start <= s.size()) {
            const usize end = s.find('/', start);
            const std::string part = s.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (part == "..") {
                if (!parts.empty()) parts.pop_back();
            } else if (!part.empty() && part != ".") {
                parts.push_back(part);
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
    };
    split(path);
    split(rel);
    std::string out = scheme;
    for (usize i = 0; i < parts.size(); ++i) out += (i ? "/" : "") + parts[i];
    return out;
}

std::string embeddedUri(std::string_view modelUri, usize index) { return std::string(modelUri) + "#image" + std::to_string(index); }

math::Color linearToSrgb(const float* c, float alpha) {
    return {linearToSrgbChannel(c[0]), linearToSrgbChannel(c[1]), linearToSrgbChannel(c[2]), alpha};
}

void finishMesh(ModelMesh& mesh, bool hadNormals, bool hadTangents) {
    if (!hadNormals) generateNormals(mesh);
    if (!hadTangents) generateTangents(mesh);
    computeBounds(mesh);
}

// =================================================================== glTF
struct GltfDeleter {
    void operator()(cgltf_data* d) const { cgltf_free(d); }
};
using GltfPtr = std::unique_ptr<cgltf_data, GltfDeleter>;

/// Parses a glTF / GLB document and loads its buffers (GLB chunk, data URIs, external .bin files).
Result<GltfPtr> parseGltf(std::span<const u8> bytes, std::string_view uri, VirtualFileSystem* vfs) {
    cgltf_options options{};
    cgltf_data* raw = nullptr;
    const cgltf_result r = cgltf_parse(&options, bytes.data(), bytes.size(), &raw);
    if (r != cgltf_result_success) return makeError(Error::Code::ParseError, "invalid glTF file (cgltf error " + std::to_string(r) + ")");
    GltfPtr data(raw);
    for (cgltf_size i = 0; i < data->buffers_count; ++i) {
        cgltf_buffer& b = data->buffers[i];
        if (b.data) continue;
        if (!b.uri) {
            if (i == 0 && data->bin && data->bin_size >= b.size) {
                b.data = const_cast<void*>(data->bin);
                b.data_free_method = cgltf_data_free_method_none;
                continue;
            }
            return makeError(Error::Code::ParseError, "glTF buffer " + std::to_string(i) + " has no data");
        }
        if (std::strncmp(b.uri, "data:", 5) == 0) {
            const char* comma = std::strchr(b.uri, ',');
            if (!comma || !std::strstr(b.uri, ";base64")) return makeError(Error::Code::ParseError, "unsupported glTF data URI");
            if (cgltf_load_buffer_base64(&options, b.size, comma + 1, &b.data) != cgltf_result_success)
                return makeError(Error::Code::ParseError, "invalid base64 glTF buffer");
            b.data_free_method = cgltf_data_free_method_memory_free;
            continue;
        }
        if (!vfs) return makeError(Error::Code::NotFound, "glTF external buffer needs a file system");
        std::string rel = b.uri;
        cgltf_decode_uri(rel.data());
        rel.resize(std::strlen(rel.c_str()));
        auto file = vfs->readBytes(resolveRelative(uri, rel));
        if (!file) return makeError(Error::Code::NotFound, "glTF buffer '" + rel + "': " + file.error().message);
        if (file.value().size() < b.size) return makeError(Error::Code::ParseError, "glTF buffer '" + rel + "' is truncated");
        void* mem = std::malloc(b.size);
        if (!mem) return makeError(Error::Code::Unknown, "glTF buffer allocation failed");
        std::memcpy(mem, file.value().data(), b.size);
        b.data = mem;
        b.data_free_method = cgltf_data_free_method_memory_free;
    }
    if (const cgltf_result v = cgltf_validate(data.get()); v != cgltf_result_success)
        return makeError(Error::Code::ParseError, "glTF validation failed (cgltf error " + std::to_string(v) + ")");
    return data;
}

std::string gltfTextureUri(const cgltf_data* data, const cgltf_texture_view& view, std::string_view uri, ModelData& out) {
    if (!view.texture || !view.texture->image) return {};
    if (view.texcoord != 0) out.warnings.push_back("texture uses a second UV set (only TEXCOORD_0 is supported)");
    const cgltf_image* image = view.texture->image;
    if (image->uri && std::strncmp(image->uri, "data:", 5) != 0) {
        std::string rel = image->uri;
        cgltf_decode_uri(rel.data());
        rel.resize(std::strlen(rel.c_str()));
        return resolveRelative(uri, rel);
    }
    return embeddedUri(uri, cgltf_image_index(data, image));
}

MaterialData gltfMaterial(const cgltf_data* data, const cgltf_material& m, std::string_view uri, ModelData& out) {
    MaterialData mat;
    mat.name = m.name ? m.name : "";
    if (m.has_pbr_metallic_roughness) {
        const auto& pbr = m.pbr_metallic_roughness;
        mat.baseColor = linearToSrgb(pbr.base_color_factor, pbr.base_color_factor[3]);
        mat.baseColorTexture = gltfTextureUri(data, pbr.base_color_texture, uri, out);
        mat.metallic = pbr.metallic_factor;
        mat.roughness = pbr.roughness_factor;
        mat.metallicRoughnessTexture = gltfTextureUri(data, pbr.metallic_roughness_texture, uri, out);
        if (pbr.base_color_texture.has_transform) {
            mat.uvScale = {pbr.base_color_texture.transform.scale[0], pbr.base_color_texture.transform.scale[1]};
            mat.uvOffset = {pbr.base_color_texture.transform.offset[0], pbr.base_color_texture.transform.offset[1]};
        }
    } else if (m.has_pbr_specular_glossiness) {
        mat.baseColor = linearToSrgb(m.pbr_specular_glossiness.diffuse_factor, m.pbr_specular_glossiness.diffuse_factor[3]);
        mat.baseColorTexture = gltfTextureUri(data, m.pbr_specular_glossiness.diffuse_texture, uri, out);
        mat.roughness = 1.0f - m.pbr_specular_glossiness.glossiness_factor;
        out.warnings.push_back("material '" + mat.name + "': specular-glossiness converted approximately to metallic-roughness");
    }
    mat.normalTexture = gltfTextureUri(data, m.normal_texture, uri, out);
    mat.normalScale = m.normal_texture.texture ? m.normal_texture.scale : 1.0f;
    mat.occlusionTexture = gltfTextureUri(data, m.occlusion_texture, uri, out);
    mat.occlusionStrength = m.occlusion_texture.texture ? m.occlusion_texture.scale : 1.0f;
    mat.emissive = linearToSrgb(m.emissive_factor, 1.0f);
    mat.emissiveIntensity = m.has_emissive_strength ? m.emissive_strength.emissive_strength : 1.0f;
    mat.emissiveTexture = gltfTextureUri(data, m.emissive_texture, uri, out);
    mat.alphaMode = m.alpha_mode == cgltf_alpha_mode_mask ? AlphaMode::Mask : m.alpha_mode == cgltf_alpha_mode_blend ? AlphaMode::Blend : AlphaMode::Opaque;
    mat.alphaCutoff = m.alpha_cutoff;
    mat.doubleSided = m.double_sided;
    mat.unlit = m.unlit;
    if (m.has_transmission || m.has_clearcoat || m.has_sheen || m.has_volume || m.has_iridescence)
        out.warnings.push_back("material '" + mat.name + "': advanced glTF extensions (transmission, clearcoat...) are ignored");
    return mat;
}

Result<ModelMesh> gltfMesh(const cgltf_data* data, const cgltf_mesh& m, ModelData& out) {
    ModelMesh mesh;
    mesh.name = m.name ? m.name : "";
    bool allNormals = true, allTangents = true;
    for (cgltf_size p = 0; p < m.primitives_count; ++p) {
        const cgltf_primitive& prim = m.primitives[p];
        if (prim.type != cgltf_primitive_type_triangles) {
            out.warnings.push_back("mesh '" + mesh.name + "': non-triangle primitive skipped");
            continue;
        }
        const cgltf_accessor *pos = nullptr, *nrm = nullptr, *tan = nullptr, *uv = nullptr, *joints = nullptr, *weights = nullptr;
        for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
            const cgltf_attribute& at = prim.attributes[a];
            if (at.type == cgltf_attribute_type_position) pos = at.data;
            else if (at.type == cgltf_attribute_type_normal) nrm = at.data;
            else if (at.type == cgltf_attribute_type_tangent) tan = at.data;
            else if (at.type == cgltf_attribute_type_texcoord && at.index == 0) uv = at.data;
            else if (at.type == cgltf_attribute_type_joints && at.index == 0) joints = at.data;
            else if (at.type == cgltf_attribute_type_weights && at.index == 0) weights = at.data;
        }
        if (!pos) return makeError(Error::Code::ParseError, "mesh '" + mesh.name + "' has a primitive without positions");
        if (prim.targets_count) out.warnings.push_back("mesh '" + mesh.name + "': morph targets are not imported");
        allNormals &= nrm != nullptr;
        allTangents &= tan != nullptr;
        const u32 base = static_cast<u32>(mesh.vertices.size());
        const usize count = pos->count;
        mesh.vertices.resize(base + count);
        for (usize i = 0; i < count; ++i) {
            ModelVertex& v = mesh.vertices[base + i];
            cgltf_accessor_read_float(pos, i, &v.position.x, 3);
            if (nrm) cgltf_accessor_read_float(nrm, i, &v.normal.x, 3);
            if (tan) cgltf_accessor_read_float(tan, i, &v.tangent.x, 4);
            else v.tangent = {1, 0, 0, 1};
            if (uv) cgltf_accessor_read_float(uv, i, &v.uv.x, 2);
        }
        if (joints && weights) {
            // Skinned primitive: earlier primitives without skinning get a neutral entry.
            mesh.skinWeights.resize(base);
            for (usize i = 0; i < count; ++i) {
                ModelSkinWeights w;
                cgltf_uint j[4] = {0, 0, 0, 0};
                cgltf_accessor_read_uint(joints, i, j, 4);
                cgltf_accessor_read_float(weights, i, &w.weights.x, 4);
                const float sum = w.weights.x + w.weights.y + w.weights.z + w.weights.w;
                if (sum > 1e-6f) w.weights = w.weights * (1.0f / sum);
                else w.weights = {1.0f, 0.0f, 0.0f, 0.0f};
                w.joints = {static_cast<float>(j[0]), static_cast<float>(j[1]), static_cast<float>(j[2]), static_cast<float>(j[3])};
                mesh.skinWeights.push_back(w);
            }
        } else if (!mesh.skinWeights.empty()) {
            mesh.skinWeights.resize(base + count);
        }
        ModelSubmesh sub;
        sub.firstIndex = static_cast<u32>(mesh.indices.size());
        if (prim.indices) {
            for (cgltf_size i = 0; i < prim.indices->count; ++i) {
                const cgltf_size idx = cgltf_accessor_read_index(prim.indices, i);
                if (idx >= count) return makeError(Error::Code::ParseError, "mesh '" + mesh.name + "' has an index out of range");
                mesh.indices.push_back(base + static_cast<u32>(idx));
            }
        } else {
            for (usize i = 0; i < count; ++i) mesh.indices.push_back(base + static_cast<u32>(i));
        }
        sub.indexCount = static_cast<u32>(mesh.indices.size()) - sub.firstIndex;
        sub.material = prim.material ? static_cast<i32>(cgltf_material_index(data, prim.material)) : -1;
        if (sub.indexCount % 3) return makeError(Error::Code::ParseError, "mesh '" + mesh.name + "': index count is not a multiple of 3");
        if (sub.indexCount) mesh.submeshes.push_back(sub);
    }
    if (!allNormals) out.warnings.push_back("mesh '" + mesh.name + "': normals generated");
    finishMesh(mesh, allNormals, allTangents);
    return mesh;
}

void gltfNode(const cgltf_data* data, const cgltf_node* node, i32 parent, ModelData& out, std::unordered_map<const cgltf_node*, i32>& index) {
    ModelNode n;
    n.name = node->name ? node->name : "";
    n.parent = parent;
    if (node->has_matrix) {
        math::Mat4 m;
        std::memcpy(&m.c[0].x, node->matrix, sizeof(float) * 16);
        math::decompose(m, n.position, n.rotation, n.scale);
    } else {
        if (node->has_translation) n.position = {node->translation[0], node->translation[1], node->translation[2]};
        if (node->has_rotation) n.rotation = {node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]};
        if (node->has_scale) n.scale = {node->scale[0], node->scale[1], node->scale[2]};
    }
    n.mesh = node->mesh ? static_cast<i32>(cgltf_mesh_index(data, node->mesh)) : -1;
    n.skin = node->skin && node->mesh ? static_cast<i32>(cgltf_skin_index(data, node->skin)) : -1;
    const i32 self = static_cast<i32>(out.nodes.size());
    index[node] = self;
    out.nodes.push_back(std::move(n));
    for (cgltf_size c = 0; c < node->children_count; ++c) gltfNode(data, node->children[c], self, out, index);
}

// =================================================================== OBJ
struct ObjKey {
    i32 v, t, n;
    bool operator==(const ObjKey&) const = default;
};
struct ObjKeyHash {
    usize operator()(const ObjKey& k) const {
        return std::hash<u64>{}((static_cast<u64>(static_cast<u32>(k.v)) << 32) ^ (static_cast<u64>(static_cast<u32>(k.t)) << 16) ^
                                static_cast<u64>(static_cast<u32>(k.n)));
    }
};

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

std::vector<std::string_view> tokens(std::string_view line) {
    std::vector<std::string_view> out;
    usize i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        const usize start = i;
        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        if (i > start) out.push_back(line.substr(start, i - start));
    }
    return out;
}

/// Locale-independent number parsing ("1.5e-3"). Not std::from_chars: Android's libc++
/// has no floating-point overloads.
float toFloat(std::string_view s) {
    char buf[64];
    const usize n = std::min(s.size(), sizeof(buf) - 1);
    std::memcpy(buf, s.data(), n);
    buf[n] = '\0';
    // strtof follows the C locale, which the engine never changes ('.' decimal point).
    return std::strtof(buf, nullptr);
}

/// MTL options ("-bm 0.5 file.png"): the file name is the last token.
std::string mapFile(const std::vector<std::string_view>& t, std::string_view uri) {
    return t.size() > 1 ? resolveRelative(uri, t.back()) : std::string();
}

void parseMtl(std::string_view text, std::string_view mtlUri, std::vector<MaterialData>& mats,
              std::unordered_map<std::string, i32>& byName, ModelData& out) {
    MaterialData* cur = nullptr;
    usize pos = 0;
    while (pos < text.size()) {
        const usize end = std::min(text.find('\n', pos), text.size());
        const std::string_view line = trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty() || line[0] == '#') continue;
        const auto t = tokens(line);
        const std::string key = lower(t[0]);
        if (key == "newmtl" && t.size() > 1) {
            byName[std::string(t[1])] = static_cast<i32>(mats.size());
            mats.emplace_back().name = std::string(t[1]);
            cur = &mats.back();
            cur->roughness = 0.8f;
        } else if (!cur) {
            continue;
        } else if (key == "kd" && t.size() >= 4) {
            cur->baseColor = {toFloat(t[1]), toFloat(t[2]), toFloat(t[3]), cur->baseColor.w};
        } else if (key == "map_kd") {
            cur->baseColorTexture = mapFile(t, mtlUri);
        } else if ((key == "map_bump" || key == "bump" || key == "norm") && t.size() > 1) {
            cur->normalTexture = mapFile(t, mtlUri);
        } else if (key == "ke" && t.size() >= 4) {
            cur->emissive = {toFloat(t[1]), toFloat(t[2]), toFloat(t[3]), 1.0f};
        } else if (key == "map_ke") {
            cur->emissiveTexture = mapFile(t, mtlUri);
            if (cur->emissive.x == 0 && cur->emissive.y == 0 && cur->emissive.z == 0) cur->emissive = {1, 1, 1, 1};
        } else if ((key == "d" || key == "tr") && t.size() > 1) {
            const float opacity = key == "d" ? toFloat(t[1]) : 1.0f - toFloat(t[1]);
            cur->baseColor.w = std::clamp(opacity, 0.0f, 1.0f);
            if (cur->baseColor.w < 0.999f) cur->alphaMode = AlphaMode::Blend;
        } else if (key == "ns" && t.size() > 1) {
            // Blinn-Phong exponent -> roughness (Walter et al. approximation).
            cur->roughness = std::clamp(std::sqrt(2.0f / (toFloat(t[1]) + 2.0f)), 0.04f, 1.0f);
        } else if (key == "pr" && t.size() > 1) {
            cur->roughness = std::clamp(toFloat(t[1]), 0.0f, 1.0f);
        } else if (key == "pm" && t.size() > 1) {
            cur->metallic = std::clamp(toFloat(t[1]), 0.0f, 1.0f);
        } else if (key == "map_pr" || key == "map_pm") {
            out.warnings.push_back("material '" + cur->name + "': separate roughness / metallic maps are not supported (use a packed glTF texture)");
        }
    }
}

// =================================================================== FBX
struct FbxDeleter {
    void operator()(ufbx_scene* s) const { ufbx_free_scene(s); }
};
using FbxPtr = std::unique_ptr<ufbx_scene, FbxDeleter>;

Result<FbxPtr> loadFbx(std::span<const u8> bytes) {
    ufbx_load_opts opts{};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;
    opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    opts.generate_missing_normals = true;
    ufbx_error error{};
    ufbx_scene* scene = ufbx_load_memory(bytes.data(), bytes.size(), &opts, &error);
    if (!scene) {
        char msg[512];
        ufbx_format_error(msg, sizeof msg, &error);
        return makeError(Error::Code::ParseError, std::string("invalid FBX file: ") + msg);
    }
    return FbxPtr(scene);
}

std::string fbxTextureUri(const ufbx_texture* tex, std::string_view uri) {
    if (!tex) return {};
    if (tex->content.size > 0) return embeddedUri(uri, tex->typed_id);
    const ufbx_string& name = tex->relative_filename.length ? tex->relative_filename : tex->filename;
    if (!name.length) return {};
    return resolveRelative(uri, std::string_view(name.data, name.length));
}

math::Color fbxColor(const ufbx_material_map& map, float factor, math::Color def) {
    if (!map.has_value) return def;
    return {static_cast<float>(map.value_vec4.x) * factor, static_cast<float>(map.value_vec4.y) * factor,
            static_cast<float>(map.value_vec4.z) * factor, 1.0f};
}

} // namespace

// ===================================================================== API
usize ModelData::vertexCount() const {
    usize n = 0;
    for (const auto& m : meshes) n += m.vertices.size();
    return n;
}
usize ModelData::triangleCount() const {
    usize n = 0;
    for (const auto& m : meshes) n += m.indices.size() / 3;
    return n;
}

std::string modelNodeName(const std::vector<ModelNode>& nodes, const std::vector<std::string>& meshNames, usize index) {
    const ModelNode& n = nodes[index];
    if (!n.name.empty()) return n.name;
    if (n.mesh >= 0 && static_cast<usize>(n.mesh) < meshNames.size() && !meshNames[static_cast<usize>(n.mesh)].empty())
        return meshNames[static_cast<usize>(n.mesh)];
    return "Node " + std::to_string(index);
}

std::string modelNodePath(const std::vector<ModelNode>& nodes, const std::vector<std::string>& meshNames, usize index) {
    std::string path = modelNodeName(nodes, meshNames, index);
    for (i32 p = nodes[index].parent; p >= 0; p = nodes[static_cast<usize>(p)].parent)
        path = modelNodeName(nodes, meshNames, static_cast<usize>(p)) + "/" + path;
    return path;
}

bool isModelFile(std::string_view name) {
    const std::string ext = extensionOf(name);
    return ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx";
}

void computeBounds(ModelMesh& mesh) {
    mesh.bounds = {};
    for (const auto& v : mesh.vertices) mesh.bounds.expand(v.position);
}

void generateNormals(ModelMesh& mesh) {
    for (auto& v : mesh.vertices) v.normal = {};
    for (usize i = 0; i + 2 < mesh.indices.size(); i += 3) {
        ModelVertex& a = mesh.vertices[mesh.indices[i]];
        ModelVertex& b = mesh.vertices[mesh.indices[i + 1]];
        ModelVertex& c = mesh.vertices[mesh.indices[i + 2]];
        const Vec3 n = math::cross(b.position - a.position, c.position - a.position);  // length = 2 * area
        a.normal = a.normal + n;
        b.normal = b.normal + n;
        c.normal = c.normal + n;
    }
    for (auto& v : mesh.vertices) {
        v.normal = math::normalize(v.normal);
        if (math::lengthSquared(v.normal) < 0.5f) v.normal = {0, 1, 0};
    }
}

void generateTangents(ModelMesh& mesh) {
    std::vector<Vec3> tan(mesh.vertices.size()), bitan(mesh.vertices.size());
    for (usize i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const u32 ia = mesh.indices[i], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
        const ModelVertex &a = mesh.vertices[ia], &b = mesh.vertices[ib], &c = mesh.vertices[ic];
        const Vec3 e1 = b.position - a.position, e2 = c.position - a.position;
        // UV v grows downward in the engine: flip it so the bitangent points "up" in the texture.
        const float du1 = b.uv.x - a.uv.x, dv1 = -(b.uv.y - a.uv.y);
        const float du2 = c.uv.x - a.uv.x, dv2 = -(c.uv.y - a.uv.y);
        const float det = du1 * dv2 - du2 * dv1;
        if (std::fabs(det) < 1e-12f) continue;
        const float r = 1.0f / det;
        const Vec3 t = (e1 * dv2 - e2 * dv1) * r;
        const Vec3 bt = (e2 * du1 - e1 * du2) * r;
        for (u32 k : {ia, ib, ic}) {
            tan[k] = tan[k] + t;
            bitan[k] = bitan[k] + bt;
        }
    }
    for (usize i = 0; i < mesh.vertices.size(); ++i) {
        ModelVertex& v = mesh.vertices[i];
        const Vec3 n = v.normal;
        Vec3 t = tan[i] - n * math::dot(n, tan[i]);  // Gram-Schmidt
        if (math::lengthSquared(t) < 1e-12f) {
            // No usable UVs: any vector orthogonal to the normal.
            t = std::fabs(n.x) < 0.9f ? math::cross(n, Vec3{1, 0, 0}) : math::cross(n, Vec3{0, 1, 0});
        }
        t = math::normalize(t);
        const float w = math::dot(math::cross(n, t), bitan[i]) < 0.0f ? -1.0f : 1.0f;
        v.tangent = Vec4(t, w);
    }
}

Result<ModelData> importGltf(std::span<const u8> bytes, std::string_view uri, VirtualFileSystem* vfs) {
    auto parsed = parseGltf(bytes, uri, vfs);
    if (!parsed) return parsed.error();
    const cgltf_data* data = parsed.value().get();
    ModelData out;
    for (cgltf_size i = 0; i < data->materials_count; ++i) out.materials.push_back(gltfMaterial(data, data->materials[i], uri, out));
    for (cgltf_size i = 0; i < data->meshes_count; ++i) {
        auto mesh = gltfMesh(data, data->meshes[i], out);
        if (!mesh) return mesh.error();
        out.meshes.push_back(std::move(mesh.value()));
    }
    const cgltf_scene* scene = data->scene ? data->scene : (data->scenes_count ? &data->scenes[0] : nullptr);
    std::unordered_map<const cgltf_node*, i32> nodeIndex;
    if (scene) {
        for (cgltf_size i = 0; i < scene->nodes_count; ++i) gltfNode(data, scene->nodes[i], -1, out, nodeIndex);
    } else {
        for (cgltf_size i = 0; i < data->nodes_count; ++i)
            if (!data->nodes[i].parent) gltfNode(data, &data->nodes[i], -1, out, nodeIndex);
    }
    // Skins: joints as node indices, inverse bind matrices (identity when absent).
    for (cgltf_size si = 0; si < data->skins_count; ++si) {
        const cgltf_skin& sk = data->skins[si];
        ModelSkin skin;
        skin.name = sk.name ? sk.name : "";
        for (cgltf_size j = 0; j < sk.joints_count; ++j) {
            auto it = nodeIndex.find(sk.joints[j]);
            skin.joints.push_back(it == nodeIndex.end() ? -1 : it->second);
            math::Mat4 ib = math::Mat4::identity();
            if (sk.inverse_bind_matrices) cgltf_accessor_read_float(sk.inverse_bind_matrices, j, &ib.c[0].x, 16);
            skin.inverseBind.push_back(ib);
        }
        if (std::find(skin.joints.begin(), skin.joints.end(), -1) != skin.joints.end())
            out.warnings.push_back("skin '" + skin.name + "' uses joints outside the scene: they stay in their rest pose");
        if (skin.joints.size() > kMaxSkinJoints)
            out.warnings.push_back("skin '" + skin.name + "' has " + std::to_string(skin.joints.size()) + " joints; only the first " +
                                   std::to_string(kMaxSkinJoints) + " are animated");
        out.skins.push_back(std::move(skin));
    }
    // Animations of node transforms (morph-target weights are not supported).
    bool cubic = false, morph = false;
    for (cgltf_size ai = 0; ai < data->animations_count; ++ai) {
        const cgltf_animation& an = data->animations[ai];
        ModelAnimation anim;
        anim.name = an.name && *an.name ? an.name : "Animation " + std::to_string(ai);
        for (cgltf_size ci = 0; ci < an.channels_count; ++ci) {
            const cgltf_animation_channel& ch = an.channels[ci];
            if (!ch.target_node || !ch.sampler) continue;
            auto it = nodeIndex.find(ch.target_node);
            if (it == nodeIndex.end()) continue;
            ModelAnimationChannel c;
            c.node = it->second;
            switch (ch.target_path) {
                case cgltf_animation_path_type_translation: c.path = ModelAnimationChannel::Path::Translation; break;
                case cgltf_animation_path_type_rotation: c.path = ModelAnimationChannel::Path::Rotation; break;
                case cgltf_animation_path_type_scale: c.path = ModelAnimationChannel::Path::Scale; break;
                default: morph = true; continue;
            }
            const cgltf_animation_sampler& sm = *ch.sampler;
            c.step = sm.interpolation == cgltf_interpolation_type_step;
            const bool spline = sm.interpolation == cgltf_interpolation_type_cubic_spline;
            cubic |= spline;
            const cgltf_size keys = sm.input->count;
            const int comps = c.path == ModelAnimationChannel::Path::Rotation ? 4 : 3;
            for (cgltf_size k = 0; k < keys; ++k) {
                float t = 0.0f;
                cgltf_accessor_read_float(sm.input, k, &t, 1);
                math::Vec4 v{0.0f, 0.0f, 0.0f, 1.0f};
                // Cubic spline outputs are (in-tangent, value, out-tangent): keep the values.
                cgltf_accessor_read_float(sm.output, spline ? k * 3 + 1 : k, &v.x, static_cast<cgltf_size>(comps));
                c.times.push_back(t);
                c.values.push_back(v);
                anim.duration = std::max(anim.duration, t);
            }
            if (!c.times.empty()) anim.channels.push_back(std::move(c));
        }
        if (!anim.channels.empty()) out.animations.push_back(std::move(anim));
    }
    if (cubic) out.warnings.push_back("cubic spline animation curves are played as linear between their keys");
    if (morph) out.warnings.push_back("morph target (blend shape) animations are not imported");
    if (out.nodes.empty())  // a file with meshes but no scene: one node per mesh
        for (usize i = 0; i < out.meshes.size(); ++i) out.nodes.push_back({out.meshes[i].name, -1, {}, math::Quat::identity(), Vec3{1.0f}, static_cast<i32>(i)});
    if (data->cameras_count || data->lights_count) out.warnings.push_back("cameras and lights of the file are not imported");
    return out;
}

Result<ModelData> importObj(std::string_view text, std::string_view uri, VirtualFileSystem* vfs) {
    ModelData out;
    std::unordered_map<std::string, i32> materialIndex;
    std::vector<Vec3> positions, normals;
    std::vector<Vec2> uvs;
    ModelMesh mesh;
    mesh.name = std::string(uri.substr(uri.find_last_of('/') == std::string_view::npos ? 0 : uri.find_last_of('/') + 1));
    std::unordered_map<ObjKey, u32, ObjKeyHash> vertexOf;
    i32 currentMaterial = -1;
    bool anyNormal = true;
    auto startSubmesh = [&] {
        if (!mesh.submeshes.empty() && mesh.submeshes.back().indexCount == 0) {
            mesh.submeshes.back().material = currentMaterial;
            return;
        }
        mesh.submeshes.push_back({static_cast<u32>(mesh.indices.size()), 0, currentMaterial});
    };
    startSubmesh();
    usize pos = 0;
    u32 lineNo = 0;
    while (pos < text.size()) {
        const usize end = std::min(text.find('\n', pos), text.size());
        const std::string_view line = trim(text.substr(pos, end - pos));
        pos = end + 1;
        ++lineNo;
        if (line.empty() || line[0] == '#') continue;
        const auto t = tokens(line);
        const std::string_view key = t[0];
        if (key == "v" && t.size() >= 4) {
            positions.push_back({toFloat(t[1]), toFloat(t[2]), toFloat(t[3])});
        } else if (key == "vn" && t.size() >= 4) {
            normals.push_back({toFloat(t[1]), toFloat(t[2]), toFloat(t[3])});
        } else if (key == "vt" && t.size() >= 3) {
            uvs.push_back({toFloat(t[1]), 1.0f - toFloat(t[2])});  // OBJ: v = 0 at the bottom
        } else if (key == "f" && t.size() >= 4) {
            std::vector<u32> poly;
            for (usize k = 1; k < t.size(); ++k) {
                ObjKey idx{0, 0, 0};
                i32* fields[3] = {&idx.v, &idx.t, &idx.n};
                usize field = 0, start = 0;
                const std::string_view tok = t[k];
                for (usize c = 0; c <= tok.size() && field < 3; ++c) {
                    if (c == tok.size() || tok[c] == '/') {
                        if (c > start) std::from_chars(tok.data() + start, tok.data() + c, *fields[field]);
                        ++field;
                        start = c + 1;
                    }
                }
                auto fix = [](i32 i, usize count) { return i < 0 ? static_cast<i32>(count) + i + 1 : i; };  // negative = relative
                idx.v = fix(idx.v, positions.size());
                idx.t = idx.t ? fix(idx.t, uvs.size()) : 0;
                idx.n = idx.n ? fix(idx.n, normals.size()) : 0;
                if (idx.v < 1 || static_cast<usize>(idx.v) > positions.size() || static_cast<usize>(idx.t) > uvs.size() ||
                    static_cast<usize>(idx.n) > normals.size() || idx.t < 0 || idx.n < 0)
                    return makeError(Error::Code::ParseError, "OBJ line " + std::to_string(lineNo) + ": index out of range");
                anyNormal &= idx.n != 0;
                auto [it, inserted] = vertexOf.emplace(idx, static_cast<u32>(mesh.vertices.size()));
                if (inserted) {
                    ModelVertex v{};
                    v.position = positions[static_cast<usize>(idx.v - 1)];
                    if (idx.n) v.normal = normals[static_cast<usize>(idx.n - 1)];
                    if (idx.t) v.uv = uvs[static_cast<usize>(idx.t - 1)];
                    mesh.vertices.push_back(v);
                }
                poly.push_back(it->second);
            }
            for (usize k = 1; k + 1 < poly.size(); ++k) {  // fan triangulation
                mesh.indices.push_back(poly[0]);
                mesh.indices.push_back(poly[k]);
                mesh.indices.push_back(poly[k + 1]);
            }
            mesh.submeshes.back().indexCount = static_cast<u32>(mesh.indices.size()) - mesh.submeshes.back().firstIndex;
        } else if (key == "usemtl" && t.size() > 1) {
            auto it = materialIndex.find(std::string(t[1]));
            currentMaterial = it == materialIndex.end() ? -1 : it->second;
            if (it == materialIndex.end()) out.warnings.push_back("unknown material '" + std::string(t[1]) + "'");
            startSubmesh();
        } else if (key == "mtllib" && t.size() > 1) {
            const std::string mtlUri = resolveRelative(uri, line.substr(line.find(t[1])));
            auto mtl = vfs ? vfs->readText(mtlUri) : Result<std::string>(makeError(Error::Code::NotFound, "no file system"));
            if (mtl) parseMtl(mtl.value(), mtlUri, out.materials, materialIndex, out);
            else out.warnings.push_back("material library '" + mtlUri + "' not found");
        } else if (key == "l" || key == "p") {
            out.warnings.push_back("OBJ lines and points are ignored");
        }
    }
    std::erase_if(mesh.submeshes, [](const ModelSubmesh& s) { return s.indexCount == 0; });
    if (mesh.indices.empty()) return makeError(Error::Code::ParseError, "OBJ file without faces");
    // A vertex shared by faces with and without normals would be ambiguous: regenerate all.
    if (!anyNormal) out.warnings.push_back("normals generated");
    finishMesh(mesh, anyNormal, false);
    out.meshes.push_back(std::move(mesh));
    out.nodes.push_back({out.meshes[0].name, -1, {}, math::Quat::identity(), Vec3{1.0f}, 0});
    return out;
}

Result<ModelData> importFbx(std::span<const u8> bytes, std::string_view uri) {
    auto loaded = loadFbx(bytes);
    if (!loaded) return loaded.error();
    const ufbx_scene* scene = loaded.value().get();
    ModelData out;
    for (usize i = 0; i < scene->materials.count; ++i) {
        const ufbx_material* m = scene->materials.data[i];
        MaterialData mat;
        mat.name = std::string(m->name.data, m->name.length);
        const float baseFactor = m->pbr.base_factor.has_value ? static_cast<float>(m->pbr.base_factor.value_real) : 1.0f;
        mat.baseColor = fbxColor(m->pbr.base_color, baseFactor, mat.baseColor);
        mat.baseColorTexture = fbxTextureUri(m->pbr.base_color.texture, uri);
        if (m->pbr.roughness.has_value) mat.roughness = std::clamp(static_cast<float>(m->pbr.roughness.value_real), 0.0f, 1.0f);
        if (m->pbr.metalness.has_value) mat.metallic = std::clamp(static_cast<float>(m->pbr.metalness.value_real), 0.0f, 1.0f);
        mat.normalTexture = fbxTextureUri(m->pbr.normal_map.texture, uri);
        const float emissionFactor = m->pbr.emission_factor.has_value ? static_cast<float>(m->pbr.emission_factor.value_real) : 1.0f;
        mat.emissive = fbxColor(m->pbr.emission_color, emissionFactor, mat.emissive);
        mat.emissiveTexture = fbxTextureUri(m->pbr.emission_color.texture, uri);
        if (m->pbr.opacity.has_value && m->pbr.opacity.value_real < 0.999) {
            mat.baseColor.w = static_cast<float>(m->pbr.opacity.value_real);
            mat.alphaMode = AlphaMode::Blend;
        }
        out.materials.push_back(std::move(mat));
    }
    std::vector<u32> tri;
    for (usize mi = 0; mi < scene->meshes.count; ++mi) {
        const ufbx_mesh* m = scene->meshes.data[mi];
        ModelMesh mesh;
        mesh.name = std::string(m->name.data, m->name.length);
        tri.resize(m->max_face_triangles * 3);
        std::unordered_map<std::string, u32> dedup;
        for (usize p = 0; p < m->material_parts.count; ++p) {
            const ufbx_mesh_part& part = m->material_parts.data[p];
            ModelSubmesh sub;
            sub.firstIndex = static_cast<u32>(mesh.indices.size());
            sub.material = part.index < m->materials.count ? static_cast<i32>(m->materials.data[part.index]->typed_id) : -1;
            for (usize f = 0; f < part.face_indices.count; ++f) {
                const ufbx_face face = m->faces.data[part.face_indices.data[f]];
                const u32 n = ufbx_triangulate_face(tri.data(), tri.size(), m, face);
                for (u32 k = 0; k < n * 3; ++k) {
                    const u32 ix = tri[k];
                    ModelVertex v{};
                    const ufbx_vec3 p3 = m->vertex_position.values.data[m->vertex_position.indices.data[ix]];
                    v.position = {static_cast<float>(p3.x), static_cast<float>(p3.y), static_cast<float>(p3.z)};
                    if (m->vertex_normal.exists) {
                        const ufbx_vec3 n3 = m->vertex_normal.values.data[m->vertex_normal.indices.data[ix]];
                        v.normal = math::normalize(Vec3{static_cast<float>(n3.x), static_cast<float>(n3.y), static_cast<float>(n3.z)});
                    }
                    if (m->vertex_uv.exists) {
                        const ufbx_vec2 t2 = m->vertex_uv.values.data[m->vertex_uv.indices.data[ix]];
                        v.uv = {static_cast<float>(t2.x), 1.0f - static_cast<float>(t2.y)};  // FBX: v = 0 at the bottom
                    }
                    const std::string key(reinterpret_cast<const char*>(&v), sizeof v);
                    auto [it, inserted] = dedup.emplace(key, static_cast<u32>(mesh.vertices.size()));
                    if (inserted) mesh.vertices.push_back(v);
                    mesh.indices.push_back(it->second);
                }
            }
            sub.indexCount = static_cast<u32>(mesh.indices.size()) - sub.firstIndex;
            if (sub.indexCount) mesh.submeshes.push_back(sub);
        }
        finishMesh(mesh, m->vertex_normal.exists, false);
        out.meshes.push_back(std::move(mesh));
    }
    // Hierarchy, parents first (depth-first from the root, which itself is not kept).
    std::vector<std::pair<const ufbx_node*, i32>> stack;
    for (usize c = scene->root_node->children.count; c-- > 0;) stack.emplace_back(scene->root_node->children.data[c], -1);
    while (!stack.empty()) {
        const auto [node, parent] = stack.back();
        stack.pop_back();
        ModelNode n;
        n.name = std::string(node->name.data, node->name.length);
        n.parent = parent;
        const ufbx_transform& t = node->local_transform;
        n.position = {static_cast<float>(t.translation.x), static_cast<float>(t.translation.y), static_cast<float>(t.translation.z)};
        n.rotation = {static_cast<float>(t.rotation.x), static_cast<float>(t.rotation.y), static_cast<float>(t.rotation.z),
                      static_cast<float>(t.rotation.w)};
        n.scale = {static_cast<float>(t.scale.x), static_cast<float>(t.scale.y), static_cast<float>(t.scale.z)};
        n.mesh = node->mesh ? static_cast<i32>(node->mesh->typed_id) : -1;
        const i32 self = static_cast<i32>(out.nodes.size());
        out.nodes.push_back(std::move(n));
        for (usize c = node->children.count; c-- > 0;) stack.emplace_back(node->children.data[c], self);
    }
    if (scene->skin_deformers.count) out.warnings.push_back("skins are not imported yet (meshes are imported in their bind pose)");
    if (scene->anim_stacks.count > 1 || (scene->anim_stacks.count == 1 && scene->anim_layers.count && scene->anim_curves.count))
        out.warnings.push_back("animations are not imported yet");
    return out;
}

Result<ModelData> importModel(VirtualFileSystem& vfs, std::string_view uri) {
    const std::string ext = extensionOf(uri);
    if (ext == ".obj") {
        auto text = vfs.readText(uri);
        if (!text) return text.error();
        return importObj(text.value(), uri, &vfs);
    }
    if (ext != ".gltf" && ext != ".glb" && ext != ".fbx")
        return makeError(Error::Code::Unsupported, "unsupported model format '" + ext + "' (glTF, GLB, OBJ, FBX)");
    auto bytes = vfs.readBytes(uri);
    if (!bytes) return bytes.error();
    if (ext == ".fbx") return importFbx(bytes.value(), uri);
    return importGltf(bytes.value(), uri, &vfs);
}

Result<ByteBuffer> readEmbeddedImage(VirtualFileSystem& vfs, std::string_view uri) {
    const auto hash = uri.find("#image");
    if (hash == std::string_view::npos) return makeError(Error::Code::InvalidArgument, "not an embedded image URI");
    const std::string_view container = uri.substr(0, hash);
    usize index = 0;
    const std::string_view num = uri.substr(hash + 6);
    if (std::from_chars(num.data(), num.data() + num.size(), index).ec != std::errc())
        return makeError(Error::Code::InvalidArgument, "invalid embedded image index");
    auto bytes = vfs.readBytes(container);
    if (!bytes) return bytes.error();
    if (extensionOf(container) == ".fbx") {
        auto scene = loadFbx(bytes.value());
        if (!scene) return scene.error();
        if (index >= scene.value()->textures.count) return makeError(Error::Code::NotFound, "no embedded texture " + std::to_string(index));
        const ufbx_blob& blob = scene.value()->textures.data[index]->content;
        if (!blob.size) return makeError(Error::Code::NotFound, "texture " + std::to_string(index) + " is not embedded");
        const auto* p = static_cast<const u8*>(blob.data);
        return ByteBuffer(p, p + blob.size);
    }
    auto parsed = parseGltf(bytes.value(), container, &vfs);
    if (!parsed) return parsed.error();
    const cgltf_data* data = parsed.value().get();
    if (index >= data->images_count) return makeError(Error::Code::NotFound, "no embedded image " + std::to_string(index));
    const cgltf_image& image = data->images[index];
    if (image.buffer_view) {
        const auto* p = static_cast<const u8*>(image.buffer_view->buffer->data) + image.buffer_view->offset;
        return ByteBuffer(p, p + image.buffer_view->size);
    }
    if (image.uri && std::strncmp(image.uri, "data:", 5) == 0) {
        const char* comma = std::strchr(image.uri, ',');
        if (!comma) return makeError(Error::Code::ParseError, "invalid image data URI");
        const usize len = std::strlen(comma + 1);
        usize padding = 0;
        if (len >= 1 && comma[len] == '=') ++padding;
        if (len >= 2 && comma[len - 1] == '=') ++padding;
        const usize size = len / 4 * 3 - padding;
        cgltf_options options{};
        void* out = nullptr;
        if (cgltf_load_buffer_base64(&options, size, comma + 1, &out) != cgltf_result_success)
            return makeError(Error::Code::ParseError, "invalid base64 image");
        const auto* p = static_cast<const u8*>(out);
        ByteBuffer result(p, p + size);
        std::free(out);
        return result;
    }
    return makeError(Error::Code::NotFound, "image " + std::to_string(index) + " is an external file");
}

} // namespace agr
