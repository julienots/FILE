#include <doctest/doctest.h>

#include <cmath>
#include <cstring>
#include <filesystem>

#include "agr/assets/AssetManager.hpp"
#include "agr/assets/Image.hpp"
#include "agr/assets/Material.hpp"
#include "agr/assets/Model.hpp"
#include "agr/core/Uuid.hpp"

using namespace agr;
using math::Vec3;
namespace fs = std::filesystem;

namespace {

struct TempAssets {
    fs::path root = fs::temp_directory_path() / ("agr_models_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    TempAssets() {
        fs::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
    }
    ~TempAssets() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

std::vector<u8> pngBytes() {
    const std::vector<u8> px = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    return encodePng(2, 2, px).value();
}

/// Binary payload of a 1x1 quad on XY (+Z normal): positions, normals, uvs, u16 indices.
std::vector<u8> quadBuffer() {
    const float pos[12] = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    const float nrm[12] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
    const float uv[8] = {0, 1, 1, 1, 1, 0, 0, 0};
    const u16 idx[6] = {0, 1, 2, 0, 2, 3};
    std::vector<u8> b(140, 0);
    std::memcpy(b.data(), pos, 48);
    std::memcpy(b.data() + 48, nrm, 48);
    std::memcpy(b.data() + 96, uv, 32);
    std::memcpy(b.data() + 128, idx, 12);
    return b;
}

std::string quadJson(usize bufferLength, const std::string& bufferUri, bool withImage, usize imageLength) {
    std::string j = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":)" + std::to_string(bufferLength) +
                    (bufferUri.empty() ? "" : R"(,"uri":")" + bufferUri + "\"") + R"(}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":48},
 {"buffer":0,"byteOffset":96,"byteLength":32},{"buffer":0,"byteOffset":128,"byteLength":12})";
    if (withImage) j += R"(,{"buffer":0,"byteOffset":140,"byteLength":)" + std::to_string(imageLength) + "}";
    j += R"(],
"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
 {"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC2"},
 {"bufferView":3,"componentType":5123,"count":6,"type":"SCALAR"}],
"meshes":[{"name":"Quad","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]}],
"materials":[{"name":"Mat","pbrMetallicRoughness":{"baseColorFactor":[0.5,0.5,0.5,1.0],"metallicFactor":0.25,"roughnessFactor":0.75)";
    if (withImage) j += R"(,"baseColorTexture":{"index":0})";
    j += R"(},"alphaMode":"MASK","alphaCutoff":0.3,"doubleSided":true,"emissiveFactor":[1,0,0]}],)";
    if (withImage) j += R"("textures":[{"source":0}],"images":[{"bufferView":4,"mimeType":"image/png"}],)";
    j += R"("nodes":[{"name":"Root","translation":[1,2,3],"children":[1]},
 {"name":"Child","mesh":0,"matrix":[2,0,0,0, 0,2,0,0, 0,0,2,0, 5,0,0,1]}],
"scenes":[{"nodes":[0]}],"scene":0})";
    return j;
}

void append32(std::vector<u8>& out, u32 v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<u8>(v >> (8 * i)));
}

std::vector<u8> makeGlb() {
    std::vector<u8> bin = quadBuffer();
    const std::vector<u8> png = pngBytes();
    bin.insert(bin.end(), png.begin(), png.end());
    while (bin.size() % 4) bin.push_back(0);
    std::string json = quadJson(bin.size(), "", true, png.size());
    while (json.size() % 4) json.push_back(' ');
    std::vector<u8> glb;
    append32(glb, 0x46546C67);  // "glTF"
    append32(glb, 2);
    append32(glb, static_cast<u32>(12 + 8 + json.size() + 8 + bin.size()));
    append32(glb, static_cast<u32>(json.size()));
    append32(glb, 0x4E4F534A);  // JSON
    glb.insert(glb.end(), json.begin(), json.end());
    append32(glb, static_cast<u32>(bin.size()));
    append32(glb, 0x004E4942);  // BIN
    glb.insert(glb.end(), bin.begin(), bin.end());
    return glb;
}

std::string base64(const std::vector<u8>& data) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (usize i = 0; i < data.size(); i += 3) {
        const u32 n = (u32(data[i]) << 16) | (i + 1 < data.size() ? u32(data[i + 1]) << 8 : 0) | (i + 2 < data.size() ? data[i + 2] : 0);
        out += t[(n >> 18) & 63];
        out += t[(n >> 12) & 63];
        out += i + 1 < data.size() ? t[(n >> 6) & 63] : '=';
        out += i + 2 < data.size() ? t[n & 63] : '=';
    }
    return out;
}

void checkQuadModel(const ModelData& m) {
    REQUIRE(m.meshes.size() == 1);
    const ModelMesh& mesh = m.meshes[0];
    CHECK(mesh.name == "Quad");
    CHECK(mesh.vertices.size() == 4);
    CHECK(mesh.indices == std::vector<u32>{0, 1, 2, 0, 2, 3});
    REQUIRE(mesh.submeshes.size() == 1);
    CHECK(mesh.submeshes[0].material == 0);
    CHECK(mesh.bounds.max.x == doctest::Approx(1.0f));
    // Generated tangents follow +U (+X here), bitangent sign keeps the frame right-handed.
    for (const ModelVertex& v : mesh.vertices) {
        CHECK(v.tangent.x == doctest::Approx(1.0f));
        CHECK(std::fabs(v.tangent.w) == doctest::Approx(1.0f));
        const Vec3 b = math::cross(v.normal, Vec3{v.tangent.x, v.tangent.y, v.tangent.z}) * v.tangent.w;
        CHECK(b.y == doctest::Approx(1.0f));  // texture "up" (v decreasing) is +Y
    }
    REQUIRE(m.nodes.size() == 2);
    CHECK(m.nodes[0].name == "Root");
    CHECK(m.nodes[0].parent == -1);
    CHECK(m.nodes[0].position.z == doctest::Approx(3.0f));
    CHECK(m.nodes[1].parent == 0);
    CHECK(m.nodes[1].mesh == 0);
    CHECK(m.nodes[1].position.x == doctest::Approx(5.0f));  // from the matrix
    CHECK(m.nodes[1].scale.y == doctest::Approx(2.0f));
    REQUIRE(m.materials.size() == 1);
    const MaterialData& mat = m.materials[0];
    CHECK(mat.name == "Mat");
    CHECK(mat.baseColor.x == doctest::Approx(0.7354f).epsilon(0.001));  // linear 0.5 -> sRGB
    CHECK(mat.metallic == doctest::Approx(0.25f));
    CHECK(mat.roughness == doctest::Approx(0.75f));
    CHECK(mat.alphaMode == AlphaMode::Mask);
    CHECK(mat.alphaCutoff == doctest::Approx(0.3f));
    CHECK(mat.doubleSided);
    CHECK(mat.emissive.x == doctest::Approx(1.0f));
}

}  // namespace

TEST_SUITE("models") {

TEST_CASE("materials: .agrmat round trip, defaults and errors") {
    MaterialData m;
    m.name = "Brushed steel";
    m.baseColor = {0.8f, 0.8f, 0.82f, 1.0f};
    m.metallic = 1.0f;
    m.roughness = 0.35f;
    m.normalTexture = "assets://tex/steel_n.png";
    m.alphaMode = AlphaMode::Blend;
    m.uvScale = {4.0f, 2.0f};
    auto back = parseMaterial(materialToJson(m));
    REQUIRE(back);
    CHECK(back.value() == m);
    auto partial = parseMaterial(R"({"format":"agr.material","version":1,"roughness":2.0})");
    REQUIRE(partial);
    CHECK(partial.value().roughness == 1.0f);  // clamped
    CHECK(partial.value().baseColor == math::Color{1, 1, 1, 1});
    CHECK_FALSE(parseMaterial("{}"));
    CHECK_FALSE(parseMaterial(R"({"format":"agr.material","version":99})"));
    CHECK_FALSE(parseMaterial(R"({"format":"agr.material","alphaMode":"glass"})"));
    CHECK(linearToSrgbChannel(srgbToLinearChannel(0.3f)) == doctest::Approx(0.3f));
}

TEST_CASE("glTF: GLB with embedded image, hierarchy and PBR material") {
    TempAssets t;
    const std::vector<u8> glb = makeGlb();
    REQUIRE(t.vfs.writeBytes("assets://models/quad.glb", glb.data(), glb.size()));
    auto model = importModel(t.vfs, "assets://models/quad.glb");
    REQUIRE_MESSAGE(model, (model ? "" : model.error().message));
    checkQuadModel(model.value());
    const std::string texture = model.value().materials[0].baseColorTexture;
    CHECK(texture == "assets://models/quad.glb#image0");
    auto png = readEmbeddedImage(t.vfs, texture);
    REQUIRE(png);
    auto img = decodeImage(png.value());
    REQUIRE(img);
    CHECK(img.value().width == 2);
    CHECK(img.value().pixels[0] == 255);  // top-left red
    CHECK_FALSE(readEmbeddedImage(t.vfs, "assets://models/quad.glb#image7"));
    // The asset manager loads embedded images like files (headless: CPU copy).
    JobSystem jobs(1);
    AssetManager assets(t.vfs, jobs, nullptr);
    const TextureAsset tex = assets.loadTexture(texture, {}, LoadMode::Blocking);
    REQUIRE(assets.texture(tex));
    CHECK(assets.texture(tex)->state == AssetState::Ready);
    REQUIRE(assets.cpuImage(tex));
    CHECK(assets.cpuImage(tex)->width == 2);
    assets.release(tex);
    CHECK(isModelFile("a/B.GLB"));
    CHECK_FALSE(isModelFile("a/b.png"));
}

TEST_CASE("glTF: .gltf with base64 or external buffers, errors") {
    TempAssets t;
    const std::vector<u8> bin = quadBuffer();
    const std::string embedded = quadJson(bin.size(), "data:application/octet-stream;base64," + base64(bin), false, 0);
    auto a = importGltf(std::span(reinterpret_cast<const u8*>(embedded.data()), embedded.size()), "assets://a.gltf", nullptr);
    REQUIRE_MESSAGE(a, (a ? "" : a.error().message));
    checkQuadModel(a.value());

    REQUIRE(t.vfs.writeBytes("assets://m/data/quad.bin", bin.data(), bin.size()));
    REQUIRE(t.vfs.writeText("assets://m/scenes/quad.gltf", quadJson(bin.size(), "../data/quad.bin", false, 0)));
    auto b = importModel(t.vfs, "assets://m/scenes/quad.gltf");
    REQUIRE_MESSAGE(b, (b ? "" : b.error().message));
    checkQuadModel(b.value());

    REQUIRE(t.vfs.writeText("assets://m/broken.gltf", quadJson(bin.size(), "missing.bin", false, 0)));
    CHECK_FALSE(importModel(t.vfs, "assets://m/broken.gltf"));
    REQUIRE(t.vfs.writeText("assets://m/garbage.glb", "not a model"));
    CHECK_FALSE(importModel(t.vfs, "assets://m/garbage.glb"));
    CHECK_FALSE(importModel(t.vfs, "assets://m/nothing.gltf"));
    CHECK_FALSE(importModel(t.vfs, "assets://m/scene.blend"));
}

TEST_CASE("OBJ: polygons, negative indices, groups of materials, generated normals") {
    TempAssets t;
    REQUIRE(t.vfs.writeText("assets://obj/mats.mtl", R"(
newmtl Red
Kd 1 0 0
Ns 98
map_Kd textures/red.png
newmtl Glass
Kd 0.8 0.9 1
d 0.4
map_Bump -bm 0.5 textures/glass_n.png
)"));
    REQUIRE(t.vfs.writeText("assets://obj/two.obj", R"(# two quads
mtllib mats.mtl
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
vt 0 0
vt 1 0
vt 1 1
vt 0 1
usemtl Red
f 1/1 2/2 3/3 4/4
v 0 0 -1
v 1 0 -1
v 1 1 -1
usemtl Glass
f -3/1 -1/3 -2/2
)"));
    auto model = importModel(t.vfs, "assets://obj/two.obj");
    REQUIRE_MESSAGE(model, (model ? "" : model.error().message));
    const ModelData& m = model.value();
    REQUIRE(m.meshes.size() == 1);
    const ModelMesh& mesh = m.meshes[0];
    CHECK(mesh.indices.size() == 9);  // quad fan (2 triangles) + 1 triangle
    REQUIRE(mesh.submeshes.size() == 2);
    CHECK(mesh.submeshes[0].indexCount == 6);
    CHECK(mesh.submeshes[1].material == 1);
    CHECK(mesh.vertices[0].uv.y == doctest::Approx(1.0f));  // OBJ v = 0 at the bottom
    CHECK(mesh.vertices[0].normal.z == doctest::Approx(1.0f));
    REQUIRE(m.materials.size() == 2);
    CHECK(m.materials[0].baseColor.x == 1.0f);
    CHECK(m.materials[0].baseColorTexture == "assets://obj/textures/red.png");
    CHECK(m.materials[0].roughness == doctest::Approx(0.1414f).epsilon(0.01));
    CHECK(m.materials[1].alphaMode == AlphaMode::Blend);
    CHECK(m.materials[1].baseColor.w == doctest::Approx(0.4f));
    CHECK(m.materials[1].normalTexture == "assets://obj/textures/glass_n.png");
    REQUIRE(m.nodes.size() == 1);

    REQUIRE(t.vfs.writeText("assets://obj/bad.obj", "v 0 0 0\nf 1 2 3\n"));
    CHECK_FALSE(importModel(t.vfs, "assets://obj/bad.obj"));
    REQUIRE(t.vfs.writeText("assets://obj/empty.obj", "# nothing\n"));
    CHECK_FALSE(importModel(t.vfs, "assets://obj/empty.obj"));
}

TEST_CASE("FBX: ASCII file converted to meters, +Y up") {
    // 1 m quad in centimeters (UnitScaleFactor 1), node 1 m above the origin.
    const std::string fbx = R"(; FBX 7.4.0 project file
FBXHeaderExtension:  {
	FBXHeaderVersion: 1003
	FBXVersion: 7400
}
GlobalSettings:  {
	Version: 1000
	Properties70:  {
		P: "UpAxis", "int", "Integer", "",1
		P: "UpAxisSign", "int", "Integer", "",1
		P: "FrontAxis", "int", "Integer", "",2
		P: "FrontAxisSign", "int", "Integer", "",1
		P: "CoordAxis", "int", "Integer", "",0
		P: "CoordAxisSign", "int", "Integer", "",1
		P: "UnitScaleFactor", "double", "Number", "",1
	}
}
Objects:  {
	Geometry: 1000, "Geometry::Quad", "Mesh" {
		Vertices: *12 {
			a: 0,0,0,100,0,0,100,100,0,0,100,0
		}
		PolygonVertexIndex: *4 {
			a: 0,1,2,-4
		}
	}
	Model: 2000, "Model::Quad", "Mesh" {
		Version: 232
		Properties70:  {
			P: "Lcl Translation", "Lcl Translation", "", "A",0,100,0
		}
	}
	Material: 3000, "Material::Red", "" {
		Properties70:  {
			P: "DiffuseColor", "Color", "", "A",1,0,0
		}
	}
}
Connections:  {
	C: "OO",1000,2000
	C: "OO",2000,0
	C: "OO",3000,2000
}
)";
    auto model = importFbx(std::span(reinterpret_cast<const u8*>(fbx.data()), fbx.size()), "assets://quad.fbx");
    REQUIRE_MESSAGE(model, (model ? "" : model.error().message));
    const ModelData& m = model.value();
    REQUIRE(m.meshes.size() == 1);
    CHECK(m.meshes[0].indices.size() == 6);
    CHECK(m.meshes[0].bounds.max.x == doctest::Approx(1.0f));
    CHECK(m.meshes[0].vertices[0].normal.z == doctest::Approx(1.0f));
    REQUIRE(m.nodes.size() == 1);
    CHECK(m.nodes[0].name == "Quad");
    CHECK(m.nodes[0].position.y == doctest::Approx(1.0f));
    CHECK(m.nodes[0].mesh == 0);
    REQUIRE(m.materials.size() == 1);
    CHECK(m.materials[0].baseColor.x == doctest::Approx(1.0f));
    CHECK(m.materials[0].baseColor.y == doctest::Approx(0.0f));
    CHECK_FALSE(importFbx(std::span<const u8>(), "assets://x.fbx"));
}

}  // TEST_SUITE
