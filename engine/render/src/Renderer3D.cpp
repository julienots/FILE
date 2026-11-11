// 3D part of the renderer: PBR pipelines, mesh collection with automatic
// instancing, lights, directional shadow map, procedural sky.
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <format>

#include "RenderUniforms.hpp"
#include "Shaders.inl"
#include "Shaders3D.inl"
#include "agr/core/Log.hpp"
#include "agr/render/Renderer.hpp"

namespace agr {

using math::Mat4;
using math::Vec3;
using math::Vec4;
using detail::FrameUniforms;
using detail::LightUniforms;
using detail::MaterialUniforms;

namespace {

constexpr u32 kSlotFrame = 0, kSlotMaterial = 1, kSlotLights = 2;
constexpr u32 kSlotSkin = 3, kSlotShadowSkin = 2;  // SkinData: after the other blocks of each pipeline
constexpr usize kPaletteBytes = kMaxSkinJoints * sizeof(math::Mat4);
constexpr u32 kShadowUnit = 5, kEnvUnit = 6, kSplatUnit = 7;

u64 mix64(u64 h, u64 v) {
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return h;
}
u64 bits(float f) { return std::bit_cast<u32>(f); }

// Pipeline ids: 0..5 standard PBR (alphaMode * 2 + doubleSided), 6 terrain, 7 water,
// kCustomBase + set * kCustomStride + variant for custom shaders (see Renderer.hpp).
constexpr u32 kCustomBase = 1000, kCustomStride = 8, kCustomUnit = 7;
u32 pipelineVariant(u32 p) { return p < 6 ? p : p >= kCustomBase ? (p - kCustomBase) % kCustomStride : 0u; }
bool pipelineMasked(u32 p) { return pipelineVariant(p) / 2 == 1; }
u32 customSetOf(u32 p) { return p >= kCustomBase ? (p - kCustomBase) / kCustomStride : ~0u; }

rhi::VertexLayout modelVertexLayout() {
    rhi::VertexLayout l;
    l.stride = sizeof(ModelVertex);
    l.attributes = {{0, rhi::VertexFormat::Float3, offsetof(ModelVertex, position)},
                    {1, rhi::VertexFormat::Float3, offsetof(ModelVertex, normal)},
                    {2, rhi::VertexFormat::Float4, offsetof(ModelVertex, tangent)},
                    {3, rhi::VertexFormat::Float2, offsetof(ModelVertex, uv)}};
    return l;
}

rhi::VertexLayout skinnedVertexLayout() {
    rhi::VertexLayout l = modelVertexLayout();
    l.stride = sizeof(SkinnedModelVertex);
    l.attributes.push_back({12, rhi::VertexFormat::Float4, offsetof(SkinnedModelVertex, skin) + offsetof(ModelSkinWeights, joints)});
    l.attributes.push_back({13, rhi::VertexFormat::Float4, offsetof(SkinnedModelVertex, skin) + offsetof(ModelSkinWeights, weights)});
    return l;
}

rhi::VertexLayout instanceLayout(bool full) {
    rhi::VertexLayout l;
    l.stride = 128;
    for (u32 i = 0; i < 4; ++i) l.attributes.push_back({4 + i, rhi::VertexFormat::Float4, i * 16});
    if (full) {
        for (u32 i = 0; i < 3; ++i) l.attributes.push_back({8 + i, rhi::VertexFormat::Float3, 64 + i * 16});
        l.attributes.push_back({11, rhi::VertexFormat::Float4, 112});
    }
    return l;
}

const MaterialData& defaultMaterial() {
    static const MaterialData m;
    return m;
}

} // namespace

Status Renderer::create3DPipelines() {
    const std::string common = std::string(shaders::kFrameBlock) + shaders::kMaterialBlock;
    for (u32 skinned = 0; skinned < 2; ++skinned) {
        const std::string skinDefine = skinned ? "#define AGR_SKINNED 1\n" : "";
        for (u32 alpha = 0; alpha < 3; ++alpha) {
            for (u32 twoSided = 0; twoSided < 2; ++twoSided) {
                rhi::PipelineDesc p;
                p.debugName = skinned ? "agr.pbr.skinned" : "agr.pbr";
                const std::string defines = skinDefine + (alpha == 1 ? "#define AGR_ALPHA_MASK 1\n" : "");
                p.shader.vertex = defines + common + (skinned ? shaders::kSkinBlock : "") + shaders::kPbrVertex;
                p.shader.fragment = defines + common + shaders::kLightBlock + shaders::kToneMapping + shaders::kEnvBlock + shaders::kPbrFragment;
                p.layout = skinned ? skinnedVertexLayout() : modelVertexLayout();
                p.instanceLayout = instanceLayout(true);
                p.cull = twoSided ? rhi::CullMode::None : rhi::CullMode::Back;
                if (alpha == 2) {  // blend: sorted back to front, no depth write
                    p.blend = rhi::BlendMode::Alpha;
                    p.depthWrite = false;
                }
                p.uniformBlocks = {"FrameData", "MaterialData", "LightData"};
                if (skinned) p.uniformBlocks.push_back("SkinData");
                p.samplers = {"u_baseColorMap", "u_metalRoughMap", "u_normalMap", "u_occlusionMap", "u_emissiveMap", "u_shadowMap", "u_envMap"};
                auto r = m_device.createPipeline(p);
                if (!r) return r.error();
                (skinned ? m_pbrSkinnedPipelines : m_pbrPipelines)[alpha * 2 + twoSided] = r.value();
            }
        }
        {  // terrain: opaque PBR blending 4 layers through a splat map
            rhi::PipelineDesc p;
            p.debugName = "agr.terrain";
            p.shader.vertex = common + shaders::kPbrVertex;
            p.shader.fragment = std::string("#define AGR_TERRAIN 1\n") + common + shaders::kLightBlock + shaders::kToneMapping +
                                shaders::kEnvBlock + shaders::kTerrainBlock + shaders::kPbrFragment;
            p.layout = modelVertexLayout();
            p.instanceLayout = instanceLayout(true);
            p.cull = rhi::CullMode::Back;
            p.uniformBlocks = {"FrameData", "MaterialData", "LightData"};
            p.samplers = {"u_baseColorMap", "u_metalRoughMap", "u_normalMap", "u_occlusionMap", "u_emissiveMap", "u_shadowMap", "u_envMap", "u_splatMap"};
            auto r = m_device.createPipeline(p);
            if (!r) return r.error();
            m_terrainPipeline = r.value();
        }
        {  // water: transparent, waves in the vertex shader, sky reflection (u_envMap in unit 0)
            rhi::PipelineDesc p;
            p.debugName = "agr.water";
            p.shader.vertex = common + shaders::kWaterVertex;
            p.shader.fragment = common + shaders::kToneMapping + shaders::kEnvBlock + shaders::kWaterFragment;
            p.layout = modelVertexLayout();
            p.instanceLayout = instanceLayout(true);
            p.cull = rhi::CullMode::None;
            p.blend = rhi::BlendMode::Alpha;
            p.depthWrite = false;
            p.uniformBlocks = {"FrameData", "MaterialData"};
            p.samplers = {"u_envMap"};
            auto r = m_device.createPipeline(p);
            if (!r) return r.error();
            m_waterPipeline = r.value();
        }
        for (u32 mask = 0; mask < 2; ++mask) {
            rhi::PipelineDesc p;
            p.debugName = mask ? "agr.shadow.mask" : "agr.shadow";
            const std::string defines = skinDefine + (mask ? "#define AGR_SHADOW_MASK 1\n" : "");
            p.shader.vertex = defines + common + (skinned ? shaders::kSkinBlock : "") + shaders::kShadowVertex;
            p.shader.fragment = defines + common + shaders::kShadowFragment;
            p.layout = skinned ? skinnedVertexLayout() : modelVertexLayout();
            p.instanceLayout = instanceLayout(false);
            p.instanceLayout.stride = 128;
            p.cull = rhi::CullMode::None;  // thin and open meshes still cast
            p.colorOutput = false;
            p.uniformBlocks = {"FrameData", "MaterialData"};
            if (skinned) p.uniformBlocks.push_back("SkinData");
            p.samplers = {"u_baseColorMap"};
            auto r = m_device.createPipeline(p);
            if (!r) return r.error();
            if (skinned) m_shadowSkinnedPipelines[mask] = r.value();
            else (mask ? m_shadowMaskPipeline : m_shadowPipeline) = r.value();
        }
    }
    {
        rhi::PipelineDesc p;
        p.debugName = "agr.sky";
        p.shader.vertex = std::string(shaders::kFrameBlock) + shaders::kSkyVertex;
        p.shader.fragment = std::string(shaders::kFrameBlock) + shaders::kToneMapping + shaders::kEnvBlock + shaders::kSkyFragment;
        p.layout.stride = sizeof(math::Vec2);
        p.layout.attributes = {{0, rhi::VertexFormat::Float2, 0}};
        p.cull = rhi::CullMode::None;
        p.depthWrite = false;
        p.depthCompare = rhi::CompareOp::LessEqual;
        p.uniformBlocks = {"FrameData"};
        p.samplers = {"u_envMap"};
        auto r = m_device.createPipeline(p);
        if (!r) return r.error();
        m_skyPipeline = r.value();
        const math::Vec2 tri[3] = {{-1.0f, -1.0f}, {3.0f, -1.0f}, {-1.0f, 3.0f}};  // covers the screen
        auto vb = m_device.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Static, sizeof tri, tri, "agr.sky.vb"});
        if (!vb) return vb.error();
        m_skyVertices = vb.value();
    }
    {
        const u8 flat[4] = {128, 128, 255, 255};
        rhi::TextureDesc td;
        td.format = rhi::TextureFormat::RGBA8;
        td.generateMips = false;
        td.data = flat;
        td.debugName = "agr.normal.flat";
        auto t = m_device.createTexture(td);
        if (!t) return t.error();
        m_flatNormal = t.value();
    }
    return {};
}

rhi::TextureHandle Renderer::resolveDataTexture(std::string_view uri) {
    if (uri.empty()) return m_assets.whiteTexture();
    auto it = m_dataTextureCache.find(uri);
    if (it == m_dataTextureCache.end()) {
        TextureLoadOptions o;
        o.srgb = false;
        it = m_dataTextureCache.emplace(std::string(uri), CachedTexture{m_assets.loadTexture(uri, o), 0}).first;
    }
    it->second.lastUsedFrame = m_frame;
    return m_assets.gpuTexture(it->second.asset);
}

rhi::TextureHandle Renderer::shadowMap() const {
    return m_shadowsActive ? m_device.renderTargetDepthTexture(m_shadowTarget) : rhi::TextureHandle{};
}

u32 Renderer::materialSlot(u64 key, const MaterialData& m, math::Vec2 uvScale) {
    if (auto it = m_materialSlots.find(key); it != m_materialSlots.end()) return it->second;
    MaterialUniforms u;
    u.baseColor = Vec4(detail::srgbToLinear(m.baseColor), m.baseColor.w);
    u.emissive = Vec4(detail::srgbToLinear(m.emissive) * m.emissiveIntensity, 0.0f);
    u.pbr = Vec4(m.metallic, m.roughness, m.normalTexture.empty() ? 0.0f : m.normalScale,
                 m.occlusionTexture.empty() ? 0.0f : m.occlusionStrength);
    u.uvTransform = Vec4(m.uvScale.x * uvScale.x, m.uvScale.y * uvScale.y, m.uvOffset.x, m.uvOffset.y);
    u.flags = Vec4(m.alphaCutoff, m.unlit ? 1.0f : 0.0f, 0.0f, 0.0f);
    FrameMaterial fm;
    fm.pipeline = static_cast<u32>(m.alphaMode) * 2 + (m.doubleSided ? 1u : 0u);
    if (const u32 set = m.shader.empty() ? ~0u : customShaderSet(m.shader); set != ~0u) {
        const CustomShader& cs = m_shaders->get(m.shader)->parsed;  // usable set: the asset exists
        for (const ShaderParam& p : cs.params) {
            if (p.type == ShaderParamType::Texture) {
                const auto it = m.shaderTextures.find(p.name);
                const std::string& uri = it != m.shaderTextures.end() && !it->second.empty() ? it->second : p.defaultTexture;
                fm.custom[p.slot] = uri.empty() ? m_assets.whiteTexture() : p.srgb ? resolveTexture(uri, false) : resolveDataTexture(uri);
                continue;
            }
            const auto it = m.shaderParams.find(p.name);
            Vec4 v = it != m.shaderParams.end() ? it->second : p.defaultValue;
            if (p.type == ShaderParamType::Color) v = Vec4(detail::srgbToLinear(v), v.w);
            u.custom[p.slot] = v;
        }
        fm.customCount = cs.textureCount;
        fm.pipeline += kCustomBase + set * kCustomStride;
    }
    fm.uniformOffset = pushUniforms(&u, sizeof u);
    fm.textures[0] = resolveTexture(m.baseColorTexture, false);
    fm.textures[1] = resolveDataTexture(m.metallicRoughnessTexture);
    fm.textures[2] = m.normalTexture.empty() ? m_flatNormal : resolveDataTexture(m.normalTexture);
    fm.textures[3] = resolveDataTexture(m.occlusionTexture);
    fm.textures[4] = m.emissiveTexture.empty() ? m_assets.whiteTexture() : resolveTexture(m.emissiveTexture, false);
    const u32 slot = static_cast<u32>(m_frameMaterials.size());
    m_frameMaterials.push_back(fm);
    m_materialSlots.emplace(key, slot);
    return slot;
}

void Renderer::collect3D(Scene& scene, const RenderView& rv, const Mat4& view, const Mat4& /*viewProj*/, const math::Frustum& frustum,
                         float aspect, const std::function<bool(ecs::Entity)>& hidden, detail::FrameUniforms& frame) {
    auto& reg = scene.registry();
    const auto range = m_glClipSpace ? Mat4::DepthRange::NegativeOneToOne : Mat4::DepthRange::ZeroToOne;
    m_opaqueItems.clear();
    m_transparentItems.clear();
    for (auto& list : m_shadowItems) list.clear();
    m_instances.clear();
    m_frameMaterials.clear();
    m_materialSlots.clear();
    m_shadowsActive = false;
    m_shadowCascades = 0;

    // ------------------------------------------------------------ shadow setup
    const DirectionalLightComponent* sun = nullptr;
    reg.view<DirectionalLightComponent, WorldTransformComponent>().each(
        [&](ecs::Entity, DirectionalLightComponent& l, WorldTransformComponent& w) {
            if (sun || !w.activeInHierarchy) return;
            sun = &l;
        });
    std::array<Mat4, kMaxCascades> lightViewProj{};
    std::array<math::Frustum, kMaxCascades> lightFrustum{};
    const bool wantShadows = sun && sun->castShadows && sun->intensity > 0.0f && m_profile.shadowMapSize > 0 &&
                             rv.projection == Projection::Perspective &&
                             frame.lightColor.x + frame.lightColor.y + frame.lightColor.z > 0.0f;
    if (wantShadows) {
        // One depth atlas: a single map, or 2 x 2 tiles for 2 to 4 cascades (at most 4096 texels).
        const u32 cascades = std::clamp<u32>(m_profile.shadowCascades, 1, kMaxCascades);
        const u32 maxAtlas = std::min<u32>(4096, m_device.info().maxTextureSize);
        const u32 size = std::clamp<u32>(cascades > 1 ? m_profile.shadowMapSize * 2 : m_profile.shadowMapSize, 256, maxAtlas);
        if (!m_shadowTarget || m_shadowSize != size) {
            m_device.destroyRenderTarget(m_shadowTarget);
            m_shadowTarget = {};
            m_shadowSize = 0;
            auto rt = m_device.createRenderTarget({.width = size, .height = size, .color = false, .depth = true, .sampleableDepth = true,
                                                   .debugName = "agr.shadowmap"});
            if (rt) {
                m_shadowTarget = rt.value();
                m_shadowSize = size;
            } else {
                AGR_LOG_ERROR("Render", "shadow map creation failed: {}", rt.error().message);
            }
        }
        if (m_shadowTarget) {
            const u32 tile = cascades > 1 ? m_shadowSize / 2 : m_shadowSize;
            const float tileUv = static_cast<float>(tile) / static_cast<float>(m_shadowSize);
            // Split distances: mix of logarithmic (detail near the camera) and uniform.
            const float nearZ = std::max(rv.nearPlane, 0.05f);
            const float farZ = std::clamp(sun->shadowDistance, nearZ + 0.01f, rv.farPlane);
            std::array<float, kMaxCascades + 1> splits{};
            splits[0] = nearZ;
            for (u32 i = 1; i <= cascades; ++i) {
                const float f = static_cast<float>(i) / static_cast<float>(cascades);
                const float logSplit = nearZ * std::pow(farZ / nearZ, f);
                const float uniformSplit = nearZ + (farZ - nearZ) * f;
                splits[i] = 0.75f * logSplit + 0.25f * uniformSplit;
            }
            splits[cascades] = farZ;
            const Vec3 L = math::normalize(Vec3{frame.lightDir.x, frame.lightDir.y, frame.lightDir.z});
            const Vec3 up = std::fabs(L.y) > 0.99f ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
            const float zNear = m_glClipSpace ? -1.0f : 0.0f;
            // Clip space -> texture space: GL stores render targets bottom row first.
            const bool flipY = m_device.info().renderTargetsFlipY;
            frame.shadowSplits = Vec4(1e30f, 1e30f, 1e30f, 1e30f);
            for (u32 c = 0; c < cascades; ++c) {
                // Bounding sphere of the camera frustum slice: its size does not change when the
                // camera turns, so shadows do not swim.
                RenderView slice = rv;
                slice.nearPlane = splits[c];
                slice.farPlane = splits[c + 1];
                const Mat4 invSlice = math::inverse(slice.projectionMatrix(aspect, m_glClipSpace) * view);
                Vec3 corners[8];
                Vec3 center{};
                for (int i = 0; i < 8; ++i) {
                    const Vec4 p = invSlice * Vec4((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : zNear, 1.0f);
                    corners[i] = Vec3{p.x, p.y, p.z} / p.w;
                    center = center + corners[i] * 0.125f;
                }
                float radius = 0.0f;
                for (const Vec3& corner : corners) radius = std::max(radius, math::length(corner - center));
                radius = std::ceil(radius * 16.0f) / 16.0f;
                const float back = std::max(radius, 30.0f);  // casters between the light and the visible area
                const Mat4 lightView = Mat4::lookAt(center - L * (radius + back), center, up);
                Mat4 lightProj = Mat4::orthographic(-radius, radius, -radius, radius, 0.0f, 2.0f * radius + back, range);
                // Snap to whole shadow-map texels (stable edges while the camera moves).
                const Vec4 origin = lightProj * (lightView * Vec4(0, 0, 0, 1));
                const float half = static_cast<float>(tile) * 0.5f;
                lightProj.c[3].x += (std::round(origin.x * half) - origin.x * half) / half;
                lightProj.c[3].y += (std::round(origin.y * half) - origin.y * half) / half;
                lightViewProj[c] = lightProj * lightView;
                lightFrustum[c] = math::Frustum::fromMatrix(lightViewProj[c], range);
                // Tile of the atlas: viewport (bottom-left origin) and where it lands in uv space.
                const u32 col = cascades > 1 ? c & 1u : 0u, row = cascades > 1 ? c >> 1 : 0u;
                m_cascadeViewports[c] = Vec4(static_cast<float>(col * tile), static_cast<float>(row * tile), static_cast<float>(tile),
                                             static_cast<float>(tile));
                const float u0 = static_cast<float>(col) * tileUv;
                const float v0 = flipY ? static_cast<float>(row) * tileUv : 1.0f - static_cast<float>(row + 1) * tileUv;
                Mat4 bias;
                bias.c[0] = {0.5f * tileUv, 0, 0, 0};
                bias.c[1] = {0, (flipY ? 0.5f : -0.5f) * tileUv, 0, 0};
                bias.c[2] = {0, 0, m_glClipSpace ? 0.5f : 1.0f, 0};
                bias.c[3] = {u0 + 0.5f * tileUv, v0 + 0.5f * tileUv, m_glClipSpace ? 0.5f : 0.0f, 1};
                frame.shadowMatrix[c] = bias * lightViewProj[c];
                frame.shadowRect[c] = Vec4(u0, v0, u0 + tileUv, v0 + tileUv);
                frame.shadowSplits[static_cast<int>(c)] = splits[c + 1];
                const float worldTexel = 2.0f * radius / static_cast<float>(tile);
                frame.shadowOffsets[static_cast<int>(c)] = worldTexel * 1.5f;
            }
            const Vec3 forward = -Vec3{view.c[0].z, view.c[1].z, view.c[2].z};  // third row of the view matrix
            frame.shadowView = Vec4(forward, 0.0f);
            frame.shadowParams = Vec4(static_cast<float>(cascades), 0.0008f, cascades > 1 ? 0.1f : 0.0f, std::clamp(sun->shadowStrength, 0.0f, 1.0f));
            frame.shadowParams2 = Vec4(1.0f / static_cast<float>(m_shadowSize), m_profile.softShadows ? 1.0f : 0.0f, 0, 0);
            m_shadowCascades = cascades;
            m_shadowsActive = true;
        }
    }
    m_frameOffset = pushUniforms(&frame, sizeof frame);  // first push: offset 0 (sprites rely on it)
    for (u32 c = 0; c < m_shadowCascades; ++c) {
        FrameUniforms shadowFrame = frame;
        shadowFrame.viewProj = lightViewProj[c];
        m_shadowFrameOffsets[c] = pushUniforms(&shadowFrame, sizeof shadowFrame);
    }
    m_stats.shadowCascades = m_shadowCascades;

    // ------------------------------------------------------------ meshes
    if (m_frame % 300 == 0)  // forget the joints of skinned meshes that no longer exist
        for (auto it = m_skinJoints.begin(); it != m_skinJoints.end();) {
            const ecs::Entity owner{static_cast<u32>(it->first), static_cast<u32>(it->first >> 32)};
            it = scene.valid(owner) ? std::next(it) : m_skinJoints.erase(it);
        }
    const Vec3 camPos = rv.position;
    const Vec3 camForward = -Vec3{view.c[0].z, view.c[1].z, view.c[2].z};  // third row of the view matrix
    reg.view<MeshRendererComponent, WorldTransformComponent>().each([&](ecs::Entity e, MeshRendererComponent& mr, WorldTransformComponent& w) {
        if (!mr.visible || !w.activeInHierarchy || hidden(e)) return;
        // Geometry: built-in mesh or a mesh of an imported model.
        rhi::BufferHandle vb, ib;
        math::Aabb bounds;
        const std::vector<ModelSubmesh>* submeshes = nullptr;
        static thread_local std::vector<ModelSubmesh> primitiveSubmesh(1);
        const ModelAsset* model = nullptr;
        bool skinnedMesh = false;
        if (!mr.model.empty()) {
            model = m_models ? m_models->get(mr.model) : nullptr;
            if (!model || model->state != AssetState::Ready || mr.meshIndex < 0 || static_cast<usize>(mr.meshIndex) >= model->meshes.size()) {
                if (model && model->state == AssetState::Loading) ++m_stats.modelsLoading;
                return;
            }
            const ModelAsset::Mesh& mesh = model->meshes[static_cast<usize>(mr.meshIndex)];
            vb = mesh.vertices;
            ib = mesh.indices;
            bounds = mesh.bounds;
            submeshes = &mesh.submeshes;
            skinnedMesh = mesh.skinned;
        } else {
            const GpuMesh& mesh = m_meshes[std::clamp(static_cast<int>(mr.mesh), 0, 3)];
            vb = mesh.vertices;
            ib = mesh.indices;
            bounds = mesh.bounds;
            primitiveSubmesh[0] = {0, mesh.indexCount, -1};
            submeshes = &primitiveSubmesh;
        }
        if (!vb || !ib) return;
        math::Aabb worldBounds = bounds.transformed(w.matrix);
        u32 skinOffset = ~0u;
        if (skinnedMesh) skinOffset = skinPalette(scene, e, mr, *model, w.matrix, bounds, worldBounds);
        const bool visible = frustum.intersects(worldBounds);
        u32 casterMask = 0;  // cascades whose light volume contains the mesh
        if (m_shadowsActive && mr.castShadows)
            for (u32 c = 0; c < m_shadowCascades; ++c)
                if (lightFrustum[c].intersects(worldBounds)) casterMask |= 1u << c;
        if (!visible) ++m_stats.meshesCulled;
        else ++m_stats.meshesVisible;
        if (!visible && !casterMask) return;

        InstanceData inst;
        inst.model = w.matrix;
        const Mat4 normalMatrix = math::transpose(math::inverse(w.matrix));
        for (int i = 0; i < 3; ++i) inst.normal[i] = Vec4(normalMatrix.c[i].x, normalMatrix.c[i].y, normalMatrix.c[i].z, 0.0f);
        inst.color = Vec4(detail::srgbToLinear(mr.color), mr.color.w);
        const u32 instance = static_cast<u32>(m_instances.size());
        m_instances.push_back(inst);
        const Vec3 center = worldBounds.center();
        const float depth = math::dot(center - camPos, camForward);

        const MaterialData* override = mr.material.empty() || !m_materials ? nullptr : m_materials->get(mr.material);
        for (const ModelSubmesh& sub : *submeshes) {
            if (sub.indexCount == 0) continue;
            const MaterialData* mat;
            u64 key;
            const u64 uvKey = mix64(bits(mr.uvScale.x), bits(mr.uvScale.y));
            if (override) {
                mat = override;
                key = mix64(reinterpret_cast<u64>(override), uvKey);
            } else if (model && sub.material >= 0 && static_cast<usize>(sub.material) < model->materials.size()) {
                mat = &model->materials[static_cast<usize>(sub.material)];
                key = mix64(reinterpret_cast<u64>(mat), uvKey);
            } else {
                // Built-in material from the component fields.
                static thread_local MaterialData inline_;
                inline_ = defaultMaterial();
                inline_.baseColorTexture = mr.texture;
                inline_.metallic = std::clamp(mr.metallic, 0.0f, 1.0f);
                inline_.roughness = std::clamp(mr.roughness, 0.0f, 1.0f);
                mat = &inline_;
                key = mix64(mix64(std::hash<std::string>{}(mr.texture), mix64(bits(inline_.metallic), bits(inline_.roughness))), uvKey);
            }
            const u32 slot = materialSlot(key, *mat, mr.uvScale);
            MeshItem item;
            item.vertices = vb;
            item.indices = ib;
            item.firstIndex = sub.firstIndex;
            item.indexCount = sub.indexCount;
            item.material = slot;
            item.pipeline = m_frameMaterials[slot].pipeline;
            item.instance = instance;
            item.depth = depth;
            item.skinOffset = skinOffset;
            const bool blend = mat->alphaMode == AlphaMode::Blend;
            if (visible) (blend ? m_transparentItems : m_opaqueItems).push_back(item);
            if (!blend)
                for (u32 c = 0; c < m_shadowCascades; ++c)
                    if (casterMask & (1u << c)) m_shadowItems[c].push_back(item);
        }
    });
    // ------------------------------------------------------------ terrains
    reg.view<TerrainComponent, WorldTransformComponent>().each([&](ecs::Entity e, TerrainComponent& t, WorldTransformComponent& w) {
        if (!w.activeInHierarchy || hidden(e)) return;
        TerrainGpu* g = terrainGpu(e, t);
        if (!g) return;
        const math::Aabb worldBounds = terrain::localBounds(t).transformed(w.matrix);
        const bool visible = frustum.intersects(worldBounds);
        u32 casterMask = 0;
        if (m_shadowsActive && t.castShadows)
            for (u32 c = 0; c < m_shadowCascades; ++c)
                if (lightFrustum[c].intersects(worldBounds)) casterMask |= 1u << c;
        if (visible) ++m_stats.meshesVisible;
        else ++m_stats.meshesCulled;
        if (!visible && !casterMask) return;
        InstanceData inst;
        inst.model = w.matrix;
        const Mat4 normalMatrix = math::transpose(math::inverse(w.matrix));
        for (int i = 0; i < 3; ++i) inst.normal[i] = Vec4(normalMatrix.c[i].x, normalMatrix.c[i].y, normalMatrix.c[i].z, 0.0f);
        inst.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        const u32 instance = static_cast<u32>(m_instances.size());
        m_instances.push_back(inst);
        // Material: layers in units 0, 1, 3, 4 (sRGB), splat map in unit 7, tiling in u_emissive.
        MaterialUniforms u;
        u.baseColor = Vec4(detail::srgbToLinear(t.color), 1.0f);
        u.emissive = t.tiling;
        u.pbr = Vec4(0.0f, std::clamp(t.roughness, 0.045f, 1.0f), 0.0f, 0.0f);
        u.uvTransform = Vec4(1.0f, 1.0f, 0.0f, 0.0f);
        u.flags = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        FrameMaterial fm;
        fm.uniformOffset = pushUniforms(&u, sizeof u);
        auto layer = [&](const std::string& uri) { return uri.empty() ? m_assets.whiteTexture() : resolveTexture(uri, false); };
        fm.textures[0] = layer(t.layer0);
        fm.textures[1] = layer(t.layer1);
        fm.textures[2] = m_flatNormal;
        fm.textures[3] = layer(t.layer2);
        fm.textures[4] = layer(t.layer3);
        fm.pipeline = kTerrainPipeline;
        fm.splat = g->splat;
        const u32 slot = static_cast<u32>(m_frameMaterials.size());
        m_frameMaterials.push_back(fm);
        MeshItem item;
        item.vertices = g->vertices;
        item.indices = g->indices;
        item.indexCount = g->indexCount;
        item.material = slot;
        item.pipeline = kTerrainPipeline;
        item.instance = instance;
        item.depth = math::dot(worldBounds.center() - camPos, camForward);
        if (visible) m_opaqueItems.push_back(item);
        for (u32 c = 0; c < m_shadowCascades; ++c)
            if (casterMask & (1u << c)) m_shadowItems[c].push_back(item);
        ++m_stats.terrains;
    });
    // ------------------------------------------------------------ water
    reg.view<WaterComponent, WorldTransformComponent>().each([&](ecs::Entity e, WaterComponent& wc, WorldTransformComponent& w) {
        if (!w.activeInHierarchy || hidden(e)) return;
        const u32 res = std::clamp<u32>(wc.resolution, 2, 512);
        const WaterGrid* grid = waterGrid(res);
        if (!grid) return;
        const Mat4 model = w.matrix * Mat4::trs(Vec3{0.0f}, math::Quat::identity(), {std::max(wc.size.x, 0.01f), 1.0f, std::max(wc.size.y, 0.01f)});
        const float amp = std::max(wc.waveHeight, 0.0f);
        const math::Aabb bounds = math::Aabb{{-0.5f, -amp, -0.5f}, {0.5f, amp, 0.5f}}.transformed(model);
        if (!frustum.intersects(bounds)) {
            ++m_stats.meshesCulled;
            return;
        }
        ++m_stats.meshesVisible;
        InstanceData inst;
        inst.model = model;
        for (int i = 0; i < 3; ++i) inst.normal[i] = Vec4(i == 0 ? 1.0f : 0.0f, i == 1 ? 1.0f : 0.0f, i == 2 ? 1.0f : 0.0f, 0.0f);
        inst.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        const u32 instance = static_cast<u32>(m_instances.size());
        m_instances.push_back(inst);
        MaterialUniforms u;
        u.baseColor = Vec4(detail::srgbToLinear(wc.shallowColor), std::clamp(wc.opacity, 0.0f, 1.0f));
        u.emissive = Vec4(detail::srgbToLinear(wc.deepColor), amp);
        u.pbr = Vec4(wc.waveLength, wc.waveSpeed, wc.waveDirection, std::max(wc.reflectivity, 0.0f));
        u.uvTransform = Vec4(1.0f, 1.0f, 0.0f, 0.0f);
        u.flags = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        FrameMaterial fm;
        fm.uniformOffset = pushUniforms(&u, sizeof u);
        fm.textures[0] = m_envTexture ? m_envTexture : m_assets.whiteTexture();
        for (int i = 1; i < 5; ++i) fm.textures[i] = m_assets.whiteTexture();
        fm.pipeline = kWaterPipeline;
        const u32 slot = static_cast<u32>(m_frameMaterials.size());
        m_frameMaterials.push_back(fm);
        MeshItem item;
        item.vertices = grid->vertices;
        item.indices = grid->indices;
        item.indexCount = grid->indexCount;
        item.material = slot;
        item.pipeline = kWaterPipeline;
        item.instance = instance;
        item.depth = math::dot(bounds.center() - camPos, camForward);
        m_transparentItems.push_back(item);
        ++m_stats.waterSurfaces;
    });

    if (m_frame % 300 == 0)  // GPU data of terrains that no longer exist
        for (auto it = m_terrains.begin(); it != m_terrains.end();) {
            if (m_frame > it->second.lastFrame + 300) {
                releaseTerrain(it->second);
                it = m_terrains.erase(it);
            } else {
                ++it;
            }
        }

    // Opaque: group identical draws (instancing) and limit state changes.
    auto byState = [](const MeshItem& a, const MeshItem& b) {
        if (a.pipeline != b.pipeline) return a.pipeline < b.pipeline;
        if (a.material != b.material) return a.material < b.material;
        if (a.vertices.index != b.vertices.index) return a.vertices.index < b.vertices.index;
        if (a.firstIndex != b.firstIndex) return a.firstIndex < b.firstIndex;
        if (a.skinOffset != b.skinOffset) return a.skinOffset < b.skinOffset;
        return a.instance < b.instance;
    };
    std::sort(m_opaqueItems.begin(), m_opaqueItems.end(), byState);
    m_stats.shadowCasters = 0;
    for (auto& list : m_shadowItems) {
        std::sort(list.begin(), list.end(), [this](const MeshItem& a, const MeshItem& b) {
            const u32 ak = shadowKey(a.pipeline), bk = shadowKey(b.pipeline);  // alpha-tested casters need their material
            if (ak != bk) return ak < bk;
            if ((ak & 1u) && a.material != b.material) return a.material < b.material;
            if (a.vertices.index != b.vertices.index) return a.vertices.index < b.vertices.index;
            if (a.firstIndex != b.firstIndex) return a.firstIndex < b.firstIndex;
            if (a.skinOffset != b.skinOffset) return a.skinOffset < b.skinOffset;
            return a.instance < b.instance;
        });
        m_stats.shadowCasters += static_cast<u32>(list.size());
    }
    // Transparent: back to front.
    std::sort(m_transparentItems.begin(), m_transparentItems.end(), [](const MeshItem& a, const MeshItem& b) {
        if (a.depth != b.depth) return a.depth > b.depth;
        return a.instance < b.instance;
    });

    // ------------------------------------------------------------ point / spot lights
    struct Candidate {
        float distance;
        Vec4 posRange, colorType, spotDir, spotInner;
    };
    std::vector<Candidate> candidates;
    reg.view<PointLightComponent, WorldTransformComponent>().each([&](ecs::Entity e, PointLightComponent& l, WorldTransformComponent& w) {
        if (!w.activeInHierarchy || hidden(e) || l.intensity <= 0.0f || l.range <= 0.0f) return;
        const Vec3 p = w.matrix.translation();
        if (!frustum.intersects(math::Aabb::fromCenterExtents(p, Vec3{l.range}))) return;
        candidates.push_back({math::length(p - camPos), Vec4(p, l.range), Vec4(detail::srgbToLinear(l.color) * l.intensity, 0.0f), Vec4(0, 0, -1, -1),
                              Vec4(1, 0, 0, 0)});
    });
    reg.view<SpotLightComponent, WorldTransformComponent>().each([&](ecs::Entity e, SpotLightComponent& l, WorldTransformComponent& w) {
        if (!w.activeInHierarchy || hidden(e) || l.intensity <= 0.0f || l.range <= 0.0f) return;
        const Vec3 p = w.matrix.translation();
        if (!frustum.intersects(math::Aabb::fromCenterExtents(p, Vec3{l.range}))) return;
        const float outer = std::clamp(l.outerAngle, 0.001f, math::radians(89.0f));
        const float inner = std::clamp(l.innerAngle, 0.0f, outer - 0.0005f);
        const Vec3 dir = math::normalize(w.matrix.transformVector({0, 0, -1}));
        candidates.push_back({math::length(p - camPos), Vec4(p, l.range), Vec4(detail::srgbToLinear(l.color) * l.intensity, 1.0f),
                              Vec4(dir, std::cos(outer)), Vec4(std::cos(inner), 0, 0, 0)});
    });
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
    LightUniforms lights{};
    const u32 maxLights = std::min<u32>(m_profile.maxLights, detail::kMaxLights3D);
    u32 n = 0;
    for (const Candidate& c : candidates) {
        if (n >= maxLights) break;
        lights.posRange[n] = c.posRange;
        lights.colorType[n] = c.colorType;
        lights.spotDir[n] = c.spotDir;
        lights.spotInner[n] = c.spotInner;
        ++n;
    }
    lights.count[0] = static_cast<i32>(n);
    m_stats.lights = n;
    m_lights3DOffset = pushUniforms(&lights, sizeof lights);
}

void Renderer::uploadInstances() {
    // Instance records in draw order: opaque, transparent, shadow casters of each cascade.
    m_instanceStaging.clear();
    for (const auto* list : {&m_opaqueItems, &m_transparentItems})
        for (const MeshItem& it : *list) m_instanceStaging.push_back(m_instances[it.instance]);
    for (const auto& list : m_shadowItems)
        for (const MeshItem& it : list) m_instanceStaging.push_back(m_instances[it.instance]);
    if (m_instanceStaging.empty()) return;
    const usize bytes = m_instanceStaging.size() * sizeof(InstanceData);
    if (bytes > m_instanceCapacity) {
        m_device.destroyBuffer(m_instanceBuffer);
        m_instanceCapacity = std::max<usize>(64 * 1024, std::bit_ceil(bytes));
        auto b = m_device.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Dynamic, m_instanceCapacity, nullptr, "agr.instances"});
        if (!b) {
            AGR_LOG_ERROR("Render", "instance buffer allocation failed: {}", b.error().message);
            m_instanceCapacity = 0;
            m_instanceBuffer = {};
            return;
        }
        m_instanceBuffer = b.value();
    }
    (void)m_device.updateBuffer(m_instanceBuffer, 0, m_instanceStaging.data(), bytes);
}

u32 Renderer::skinPalette(Scene& scene, ecs::Entity e, const MeshRendererComponent& mr, const ModelAsset& model, const Mat4& world,
                          math::Aabb bindBounds, math::Aabb& worldBounds) {
    m_palette.assign(kMaxSkinJoints, Mat4::identity());
    ++m_stats.skinnedMeshes;
    if (mr.skin < 0 || static_cast<usize>(mr.skin) >= model.skins.size()) return pushUniforms(m_palette.data(), kPaletteBytes);  // bind pose
    const ModelSkin& skin = model.skins[static_cast<usize>(mr.skin)];
    // Joints: entities named like the model's nodes, found below the model root (the
    // entity instantiateModel created, an ancestor of this mesh). Cached, revalidated.
    auto& joints = m_skinJoints[e.packed()];
    bool valid = joints.size() == skin.joints.size();
    for (usize i = 0; valid && i < joints.size(); ++i) valid = joints[i].isNull() || scene.valid(joints[i]);
    if (!valid) {
        joints.assign(skin.joints.size(), ecs::Entity{});
        std::vector<std::string> meshNames;
        for (const auto& m : model.meshes) meshNames.push_back(m.name);
        // Depth of this mesh's node gives the model root.
        i32 meshNode = -1;
        for (usize n = 0; n < model.nodes.size() && meshNode < 0; ++n)
            if (model.nodes[n].mesh == mr.meshIndex && model.nodes[n].skin == mr.skin) meshNode = static_cast<i32>(n);
        ecs::Entity root = e;
        for (i32 p = meshNode; p >= 0 && !root.isNull(); p = model.nodes[static_cast<usize>(p)].parent) root = scene.parent(root);
        if (!root.isNull() && meshNode >= 0) {
            for (usize j = 0; j < skin.joints.size(); ++j) {
                if (skin.joints[j] < 0) continue;
                const std::string path = modelNodePath(model.nodes, meshNames, static_cast<usize>(skin.joints[j]));
                ecs::Entity cur = root;
                usize start = 0;
                while (!cur.isNull() && start <= path.size()) {
                    const usize slash = path.find('/', start);
                    const std::string name = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
                    ecs::Entity found;
                    if (cur.isNull()) break;
                    scene.forEachChild(cur, [&](ecs::Entity c) {
                        if (found.isNull() && scene.nameOf(c) == name) found = c;
                    });
                    cur = found;
                    if (slash == std::string::npos) break;
                    start = slash + 1;
                }
                joints[j] = cur;
            }
        }
    }
    // Palette: inverse(mesh world) * joint world * inverse bind; bounds from the joint positions.
    const Mat4 invWorld = math::inverse(world);
    auto* worlds = scene.registry().findPool<WorldTransformComponent>();
    const Vec3 ext = bindBounds.extents();
    const float pad = std::max({ext.x, ext.y, ext.z});
    math::Aabb deformed = worldBounds;
    const usize count = std::min(joints.size(), kMaxSkinJoints);
    for (usize j = 0; j < count; ++j) {
        const WorldTransformComponent* jw = joints[j].isNull() || !worlds ? nullptr : worlds->tryGet(joints[j]);
        if (!jw) continue;
        m_palette[j] = invWorld * jw->matrix * skin.inverseBind[j];
        const Vec3 p = jw->matrix.translation();
        deformed.expand(p - Vec3{pad});
        deformed.expand(p + Vec3{pad});
    }
    worldBounds = deformed;
    return pushUniforms(m_palette.data(), kPaletteBytes);
}

// ------------------------------------------------------------------ terrain GPU data
Renderer::TerrainGpu* Renderer::terrainGpu(ecs::Entity e, TerrainComponent& t) {
    terrain::ensureData(t);
    TerrainGpu& g = m_terrains[e.packed()];
    g.lastFrame = m_frame;
    const u64 hh = terrain::heightsHash(t);
    if (!g.vertices || hh != g.heightsHash) {
        std::vector<Vec3> positions, normals;
        std::vector<math::Vec2> uvs;
        std::vector<u32> indices;
        terrain::buildMesh(t, positions, indices, &normals, &uvs);
        std::vector<ModelVertex> verts(positions.size());
        for (usize i = 0; i < verts.size(); ++i) verts[i] = {positions[i], normals[i], Vec4(1.0f, 0.0f, 0.0f, 1.0f), uvs[i]};
        const usize vbytes = verts.size() * sizeof(ModelVertex), ibytes = indices.size() * sizeof(u32);
        if (g.vertices && g.indexCount == indices.size()) {  // same resolution: update in place (sculpting)
            (void)m_device.updateBuffer(g.vertices, 0, verts.data(), vbytes);
        } else {
            m_device.destroyBuffer(g.vertices);
            m_device.destroyBuffer(g.indices);
            auto vb = m_device.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Dynamic, vbytes, verts.data(), "agr.terrain.vb"});
            auto ib = m_device.createBuffer({rhi::BufferUsage::Index, rhi::BufferMemory::Static, ibytes, indices.data(), "agr.terrain.ib"});
            if (!vb || !ib) {
                AGR_LOG_ERROR("Render", "terrain buffers: {}", !vb ? vb.error().message : ib.error().message);
                g.vertices = vb ? vb.value() : rhi::BufferHandle{};
                g.indices = ib ? ib.value() : rhi::BufferHandle{};
                return nullptr;
            }
            g.vertices = vb.value();
            g.indices = ib.value();
            g.indexCount = static_cast<u32>(indices.size());
        }
        g.heightsHash = hh;
    }
    const u64 sh = terrain::splatHash(t);
    const u32 n = terrain::resolution(t);
    if (!g.splat || sh != g.splatHash || g.splatSize != n) {
        // RGBA8 weights, little-endian packing: byte 0 = layer 0.
        static_assert(std::endian::native == std::endian::little);
        if (g.splat && g.splatSize == n) {
            (void)m_device.updateTexture(g.splat, t.splat.data());
        } else {
            m_device.destroyTexture(g.splat);
            rhi::TextureDesc d;
            d.width = d.height = n;
            d.format = rhi::TextureFormat::RGBA8;
            d.generateMips = false;
            d.sampler = {rhi::Filter::Linear, rhi::Filter::Linear, false, rhi::Wrap::Clamp, rhi::Wrap::Clamp};
            d.data = t.splat.data();
            d.debugName = "agr.terrain.splat";
            auto tex = m_device.createTexture(d);
            g.splat = tex ? tex.value() : rhi::TextureHandle{};
            g.splatSize = n;
        }
        g.splatHash = sh;
    }
    return &g;
}

const Renderer::WaterGrid* Renderer::waterGrid(u32 n) {
    if (auto it = m_waterGrids.find(n); it != m_waterGrids.end()) return &it->second;
    std::vector<ModelVertex> verts;
    std::vector<u32> idx;
    verts.reserve(static_cast<usize>(n + 1) * (n + 1));
    for (u32 z = 0; z <= n; ++z)
        for (u32 x = 0; x <= n; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(n), v = static_cast<float>(z) / static_cast<float>(n);
            verts.push_back({{u - 0.5f, 0.0f, v - 0.5f}, {0, 1, 0}, Vec4(1, 0, 0, 1), {u, v}});
        }
    for (u32 z = 0; z < n; ++z)
        for (u32 x = 0; x < n; ++x) {
            const u32 a = z * (n + 1) + x, b = a + 1, c = a + n + 1, d = c + 1;
            idx.insert(idx.end(), {a, c, b, b, c, d});
        }
    auto vb = m_device.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Static, verts.size() * sizeof(ModelVertex), verts.data(), "agr.water.vb"});
    auto ib = m_device.createBuffer({rhi::BufferUsage::Index, rhi::BufferMemory::Static, idx.size() * sizeof(u32), idx.data(), "agr.water.ib"});
    if (!vb || !ib) {
        if (vb) m_device.destroyBuffer(vb.value());
        if (ib) m_device.destroyBuffer(ib.value());
        AGR_LOG_ERROR("Render", "water grid buffers could not be created");
        return nullptr;
    }
    return &(m_waterGrids[n] = WaterGrid{vb.value(), ib.value(), static_cast<u32>(idx.size())});
}

void Renderer::releaseTerrain(TerrainGpu& g) {
    m_device.destroyBuffer(g.vertices);
    m_device.destroyBuffer(g.indices);
    m_device.destroyTexture(g.splat);
    g = {};
}

void Renderer::drawMeshItems(const std::vector<MeshItem>& items, u32 firstInstance, bool shadow, u32 cascade) {
    if (items.empty() || !m_instanceBuffer) return;
    m_device.bindUniformBuffer(kSlotFrame, m_uniformBuffer, shadow ? m_shadowFrameOffsets[cascade] : m_frameOffset, sizeof(FrameUniforms));
    if (!shadow) {
        m_device.bindUniformBuffer(kSlotLights, m_uniformBuffer, m_lights3DOffset, sizeof(LightUniforms));
        m_device.bindTexture(kShadowUnit, m_shadowsActive ? m_device.renderTargetDepthTexture(m_shadowTarget) : m_assets.whiteTexture());
        m_device.bindTexture(kEnvUnit, m_envTexture ? m_envTexture : m_assets.whiteTexture());
    }
    u32 boundPipeline = ~0u, boundMaterial = ~0u;
    rhi::BufferHandle boundVb;
    usize i = 0;
    while (i < items.size()) {
        const MeshItem& first = items[i];
        usize end = i + 1;
        const bool masked = pipelineMasked(first.pipeline);
        const u32 firstShadowKey = shadow ? shadowKey(first.pipeline) : 0u;
        while (end < items.size() && items[end].vertices == first.vertices && items[end].firstIndex == first.firstIndex &&
               items[end].indexCount == first.indexCount && items[end].skinOffset == first.skinOffset && first.skinOffset == ~0u &&
               (shadow ? (!masked || items[end].material == first.material) && shadowKey(items[end].pipeline) == firstShadowKey
                       : items[end].material == first.material && items[end].pipeline == first.pipeline))
            ++end;
        const bool skinned = first.skinOffset != ~0u;
        const u32 pipeline = (shadow ? firstShadowKey : first.pipeline) | (skinned ? 0x80000000u : 0u);
        if (pipeline != boundPipeline) {
            rhi::PipelineHandle h;
            const u32 set = customSetOf(first.pipeline);
            if (set != ~0u && (!shadow || firstShadowKey >= kCustomBase))  // custom shader (falls back below)
                h = customPipeline(set, shadow ? (masked ? 1u : 0u) : pipelineVariant(first.pipeline), skinned, shadow);
            if (!h) {
                if (shadow) h = skinned ? m_shadowSkinnedPipelines[masked ? 1 : 0] : (masked ? m_shadowMaskPipeline : m_shadowPipeline);
                else if (first.pipeline == kTerrainPipeline) h = m_terrainPipeline;
                else if (first.pipeline == kWaterPipeline) h = m_waterPipeline;
                else {
                    const u32 v = pipelineVariant(first.pipeline);
                    h = skinned ? m_pbrSkinnedPipelines[v] : m_pbrPipelines[v];
                }
            }
            m_device.bindPipeline(h);
            boundPipeline = pipeline;
            boundMaterial = ~0u;
        }
        if (skinned) m_device.bindUniformBuffer(shadow ? kSlotShadowSkin : kSlotSkin, m_uniformBuffer, first.skinOffset, kPaletteBytes);
        if (first.material != boundMaterial) {  // opaque casters ignore it, but the block must be bound
            const FrameMaterial& m = m_frameMaterials[first.material];
            m_device.bindUniformBuffer(kSlotMaterial, m_uniformBuffer, m.uniformOffset, sizeof(MaterialUniforms));
            const u32 textures = shadow ? 1u : 5u;
            for (u32 t = 0; t < textures; ++t) m_device.bindTexture(t, m.textures[t]);
            if (!shadow && m.pipeline == kTerrainPipeline) m_device.bindTexture(kSplatUnit, m.splat ? m.splat : m_assets.whiteTexture());
            if (!shadow)
                for (u32 t = 0; t < m.customCount; ++t) m_device.bindTexture(kCustomUnit + t, m.custom[t] ? m.custom[t] : m_assets.whiteTexture());
            boundMaterial = first.material;
        }
        if (first.vertices != boundVb) {
            m_device.bindVertexBuffer(first.vertices);
            m_device.bindIndexBuffer(first.indices, rhi::IndexType::U32);
            boundVb = first.vertices;
        }
        m_device.bindInstanceBuffer(m_instanceBuffer, static_cast<usize>(firstInstance + i) * sizeof(InstanceData));
        m_device.drawIndexedInstanced(first.indexCount, static_cast<u32>(end - i), first.firstIndex);
        if (!shadow) ++m_stats.meshBatches;
        i = end;
    }
}

void Renderer::drawShadowPass() {
    if (!m_shadowsActive) return;
    m_device.beginPass(m_shadowTarget, {false, {}, true, 1.0f});
    u32 first = static_cast<u32>(m_opaqueItems.size() + m_transparentItems.size());
    for (u32 c = 0; c < m_shadowCascades; ++c) {
        const Vec4& vp = m_cascadeViewports[c];
        m_device.setViewport(static_cast<i32>(vp.x), static_cast<i32>(vp.y), static_cast<u32>(vp.z), static_cast<u32>(vp.w));
        drawMeshItems(m_shadowItems[c], first, true, c);
        first += static_cast<u32>(m_shadowItems[c].size());
    }
    m_device.endPass();
    m_device.prepareTextureForSampling(m_device.renderTargetDepthTexture(m_shadowTarget));
}

void Renderer::drawSky() {
    if (!m_drawSky) return;
    m_device.bindPipeline(m_skyPipeline);
    m_device.bindUniformBuffer(kSlotFrame, m_uniformBuffer, m_frameOffset, sizeof(FrameUniforms));
    m_device.bindTexture(0, m_envTexture ? m_envTexture : m_assets.whiteTexture());
    m_device.bindVertexBuffer(m_skyVertices);
    m_device.draw(3);
}

// ------------------------------------------------------------- custom shaders
u32 Renderer::shadowKey(u32 pipeline) const {
    const u32 masked = pipelineMasked(pipeline) ? 1u : 0u;
    const u32 set = customSetOf(pipeline);
    if (set < m_customSets.size() && m_customSets[set].usable && m_customSets[set].hasVertex)
        return kCustomBase + set * kCustomStride + masked;
    return masked;
}

void Renderer::destroyCustomPipelines(CustomPipelines& set) {
    for (auto& row : set.pbr)
        for (rhi::PipelineHandle& h : row) {
            m_device.destroyPipeline(h);
            h = {};
        }
    for (auto& row : set.shadow)
        for (rhi::PipelineHandle& h : row) {
            m_device.destroyPipeline(h);
            h = {};
        }
    for (auto& row : set.tried) std::fill(std::begin(row), std::end(row), false);
    for (auto& row : set.shadowTried) std::fill(std::begin(row), std::end(row), false);
}

u32 Renderer::customShaderSet(const std::string& uri) {
    static_assert(kCustomPipelineBase == kCustomBase && kCustomStride == Renderer::kCustomStride);
    if (!m_shaders) return ~0u;
    const ShaderAsset* asset = m_shaders->get(uri);
    u32 index;
    if (auto it = m_customIndex.find(uri); it != m_customIndex.end()) {
        index = it->second;
    } else {
        index = static_cast<u32>(m_customSets.size());
        m_customSets.emplace_back().uri = uri;
        m_customIndex.emplace(uri, index);
    }
    CustomPipelines& set = m_customSets[index];
    const u64 version = asset ? asset->version : ~0ull;
    if (set.version != version) {
        destroyCustomPipelines(set);
        set.version = version;
        set.usable = asset && asset->parsed.valid();
        set.hasVertex = asset && asset->parsed.hasVertex;
        set.textureCount = asset ? asset->parsed.textureCount : 0;
        set.diagnostics = asset ? asset->parsed.errors : std::vector<ShaderDiagnostic>{{0, "the file cannot be read"}};
    }
    return set.usable ? index : ~0u;
}

rhi::PipelineHandle Renderer::customPipeline(u32 index, u32 variant, bool skinned, bool shadow) {
    if (index >= m_customSets.size()) return {};
    CustomPipelines& set = m_customSets[index];
    rhi::PipelineHandle& slot = shadow ? set.shadow[skinned][variant & 1u] : set.pbr[skinned][variant];
    bool& tried = shadow ? set.shadowTried[skinned][variant & 1u] : set.tried[skinned][variant];
    if (slot || tried || !set.usable || !m_shaders) return slot;
    tried = true;
    const ShaderAsset* asset = m_shaders->get(set.uri);
    if (!asset || asset->version != set.version) return {};  // changed since this frame's lookup
    const CustomShader& cs = asset->parsed;
    const std::string common = std::string(shaders::kFrameBlock) + shaders::kMaterialBlock;
    const std::string skinDefine = skinned ? "#define AGR_SKINNED 1\n" : "";
    const std::string vertexDefine = cs.hasVertex ? "#define AGR_CUSTOM_VERTEX 1\n" : "";
    // The user's code follows `#line 1 1`: compiler messages about it carry its own line numbers.
    // Each part starts on its own line (preprocessor directives must start a line).
    const std::string vertexCode = "\n" + shaderParamDefines(cs, false) + "#line 1 1\n" + blankFunction(cs.code, "surface");
    rhi::PipelineDesc p;
    p.layout = skinned ? skinnedVertexLayout() : modelVertexLayout();
    p.uniformBlocks = {"FrameData", "MaterialData"};
    if (shadow) {
        const bool mask = (variant & 1u) != 0;
        p.debugName = "agr.custom.shadow";
        const std::string defines = skinDefine + vertexDefine + (mask ? "#define AGR_SHADOW_MASK 1\n" : "");
        p.shader.vertex = defines + common + (skinned ? shaders::kSkinBlock : "") + shaders::kCustomBlock + shaders::kShadowNormalBlock +
                          shaders::kShadowVertex + vertexCode;
        p.shader.fragment = defines + common + shaders::kShadowFragment;
        p.instanceLayout = instanceLayout(false);
        p.colorOutput = false;
        p.cull = rhi::CullMode::None;
        if (skinned) p.uniformBlocks.push_back("SkinData");
        p.samplers = {"u_baseColorMap"};
    } else {
        const u32 alpha = variant / 2, twoSided = variant & 1u;
        p.debugName = "agr.custom";
        const std::string defines = skinDefine + vertexDefine + (alpha == 1 ? "#define AGR_ALPHA_MASK 1\n" : "") +
                                    (cs.hasSurface ? "#define AGR_CUSTOM_SURFACE 1\n" : "");
        std::string samplers;
        for (u32 t = 0; t < cs.textureCount; ++t) samplers += std::format("uniform sampler2D u_customMap{};\n", t);
        p.shader.vertex = defines + common + (skinned ? shaders::kSkinBlock : "") + shaders::kCustomBlock + shaders::kPbrVertex +
                          (cs.hasVertex ? vertexCode : std::string());
        p.shader.fragment = defines + common + shaders::kLightBlock + shaders::kToneMapping + shaders::kEnvBlock + samplers +
                            shaders::kCustomBlock + shaders::kPbrFragment + "\n" + shaderParamDefines(cs, true) + "#line 1 1\n" +
                            blankFunction(cs.code, "vertex");
        p.instanceLayout = instanceLayout(true);
        p.cull = twoSided ? rhi::CullMode::None : rhi::CullMode::Back;
        if (alpha == 2) {
            p.blend = rhi::BlendMode::Alpha;
            p.depthWrite = false;
        }
        p.uniformBlocks.push_back("LightData");
        if (skinned) p.uniformBlocks.push_back("SkinData");
        p.samplers = {"u_baseColorMap", "u_metalRoughMap", "u_normalMap", "u_occlusionMap", "u_emissiveMap", "u_shadowMap", "u_envMap"};
        for (u32 t = 0; t < cs.textureCount; ++t) p.samplers.push_back(std::format("u_customMap{}", t));
    }
    auto r = m_device.createPipeline(p);
    if (!r) {
        set.usable = false;
        set.diagnostics = parseShaderLog(r.error().message);
        if (set.diagnostics.empty()) set.diagnostics.push_back({0, r.error().message});
        AGR_LOG_ERROR("Render", "shader '{}' does not compile ({} message(s)); its materials use the standard shader", set.uri,
                      set.diagnostics.size());
        for (const ShaderDiagnostic& d : set.diagnostics) AGR_LOG_ERROR("Render", "  line {}: {}", d.line, d.message);
        return {};
    }
    slot = r.value();
    return slot;
}

Renderer::ShaderStatus Renderer::shaderStatus(const std::string& uri) {
    const u32 index = customShaderSet(uri);
    if (index != ~0u) {
        customPipeline(index, 0, false, false);
        if (m_customSets[index].hasVertex) customPipeline(index, 0, false, true);
    }
    const auto it = m_customIndex.find(uri);
    if (it == m_customIndex.end()) return {false, {{0, "no shader library"}}};
    const CustomPipelines& set = m_customSets[it->second];
    return {set.usable, set.diagnostics};
}

} // namespace agr
