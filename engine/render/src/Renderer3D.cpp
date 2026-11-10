// 3D part of the renderer: PBR pipelines, mesh collection with automatic
// instancing, lights, directional shadow map, procedural sky.
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstring>

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
constexpr u32 kShadowUnit = 5;

u64 mix64(u64 h, u64 v) {
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return h;
}
u64 bits(float f) { return std::bit_cast<u32>(f); }

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
                p.shader.fragment = defines + common + shaders::kLightBlock + shaders::kToneMapping + shaders::kPbrFragment;
                p.layout = skinned ? skinnedVertexLayout() : modelVertexLayout();
                p.instanceLayout = instanceLayout(true);
                p.cull = twoSided ? rhi::CullMode::None : rhi::CullMode::Back;
                if (alpha == 2) {  // blend: sorted back to front, no depth write
                    p.blend = rhi::BlendMode::Alpha;
                    p.depthWrite = false;
                }
                p.uniformBlocks = {"FrameData", "MaterialData", "LightData"};
                if (skinned) p.uniformBlocks.push_back("SkinData");
                p.samplers = {"u_baseColorMap", "u_metalRoughMap", "u_normalMap", "u_occlusionMap", "u_emissiveMap", "u_shadowMap"};
                auto r = m_device.createPipeline(p);
                if (!r) return r.error();
                (skinned ? m_pbrSkinnedPipelines : m_pbrPipelines)[alpha * 2 + twoSided] = r.value();
            }
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
        p.shader.fragment = std::string(shaders::kFrameBlock) + shaders::kToneMapping + shaders::kSkyFragment;
        p.layout.stride = sizeof(math::Vec2);
        p.layout.attributes = {{0, rhi::VertexFormat::Float2, 0}};
        p.cull = rhi::CullMode::None;
        p.depthWrite = false;
        p.depthCompare = rhi::CompareOp::LessEqual;
        p.uniformBlocks = {"FrameData"};
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
    fm.uniformOffset = pushUniforms(&u, sizeof u);
    fm.textures[0] = resolveTexture(m.baseColorTexture, false);
    fm.textures[1] = resolveDataTexture(m.metallicRoughnessTexture);
    fm.textures[2] = m.normalTexture.empty() ? m_flatNormal : resolveDataTexture(m.normalTexture);
    fm.textures[3] = resolveDataTexture(m.occlusionTexture);
    fm.textures[4] = m.emissiveTexture.empty() ? m_assets.whiteTexture() : resolveTexture(m.emissiveTexture, false);
    fm.pipeline = static_cast<u32>(m.alphaMode) * 2 + (m.doubleSided ? 1u : 0u);
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
    m_shadowItems.clear();
    m_instances.clear();
    m_frameMaterials.clear();
    m_materialSlots.clear();
    m_shadowsActive = false;

    // ------------------------------------------------------------ shadow setup
    const DirectionalLightComponent* sun = nullptr;
    reg.view<DirectionalLightComponent, WorldTransformComponent>().each(
        [&](ecs::Entity, DirectionalLightComponent& l, WorldTransformComponent& w) {
            if (sun || !w.activeInHierarchy) return;
            sun = &l;
        });
    Mat4 lightViewProj;
    math::Frustum lightFrustum;
    const bool wantShadows = sun && sun->castShadows && sun->intensity > 0.0f && m_profile.shadowMapSize > 0 &&
                             rv.projection == Projection::Perspective &&
                             frame.lightColor.x + frame.lightColor.y + frame.lightColor.z > 0.0f;
    if (wantShadows) {
        const u32 size = std::clamp<u32>(m_profile.shadowMapSize, 256, std::min<u32>(4096, m_device.info().maxTextureSize));
        if (!m_shadowTarget || m_shadowSize != size) {
            m_device.destroyRenderTarget(m_shadowTarget);
            m_shadowTarget = {};
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
            // Bounding sphere of the camera frustum slice [near, shadowDistance]: its size does not
            // change when the camera turns, so shadows do not swim.
            RenderView slice = rv;
            slice.farPlane = std::clamp(sun->shadowDistance, rv.nearPlane + 0.01f, rv.farPlane);
            const Mat4 invSlice = math::inverse(slice.projectionMatrix(aspect, m_glClipSpace) * view);
            const float zNear = m_glClipSpace ? -1.0f : 0.0f;
            Vec3 corners[8];
            Vec3 center{};
            for (int i = 0; i < 8; ++i) {
                const Vec4 c = invSlice * Vec4((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : zNear, 1.0f);
                corners[i] = Vec3{c.x, c.y, c.z} / c.w;
                center = center + corners[i] * 0.125f;
            }
            float radius = 0.0f;
            for (const Vec3& c : corners) radius = std::max(radius, math::length(c - center));
            radius = std::ceil(radius * 16.0f) / 16.0f;
            const Vec3 L = math::normalize(Vec3{frame.lightDir.x, frame.lightDir.y, frame.lightDir.z});
            const float back = std::max(radius, 30.0f);  // casters between the light and the visible area
            const Vec3 up = std::fabs(L.y) > 0.99f ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
            const Mat4 lightView = Mat4::lookAt(center - L * (radius + back), center, up);
            Mat4 lightProj = Mat4::orthographic(-radius, radius, -radius, radius, 0.0f, 2.0f * radius + back, range);
            // Snap to whole shadow-map texels (stable edges while the camera moves).
            const Vec4 origin = lightProj * (lightView * Vec4(0, 0, 0, 1));
            const float half = static_cast<float>(m_shadowSize) * 0.5f;
            lightProj.c[3].x += (std::round(origin.x * half) - origin.x * half) / half;
            lightProj.c[3].y += (std::round(origin.y * half) - origin.y * half) / half;
            lightViewProj = lightProj * lightView;
            lightFrustum = math::Frustum::fromMatrix(lightViewProj, range);
            // Clip space -> texture space: GL stores render targets bottom row first.
            Mat4 bias;
            bias.c[0] = {0.5f, 0, 0, 0};
            bias.c[1] = {0, m_device.info().renderTargetsFlipY ? 0.5f : -0.5f, 0, 0};
            bias.c[2] = {0, 0, m_glClipSpace ? 0.5f : 1.0f, 0};
            bias.c[3] = {0.5f, 0.5f, m_glClipSpace ? 0.5f : 0.0f, 1};
            frame.shadowMatrix = bias * lightViewProj;
            const float worldTexel = 2.0f * radius / static_cast<float>(m_shadowSize);
            frame.shadowParams = Vec4(1.0f, 0.0008f, worldTexel * 1.5f, std::clamp(sun->shadowStrength, 0.0f, 1.0f));
            frame.shadowParams2 = Vec4(1.0f / static_cast<float>(m_shadowSize), m_profile.softShadows ? 1.0f : 0.0f, 0, 0);
            m_shadowsActive = true;
        }
    }
    m_frameOffset = pushUniforms(&frame, sizeof frame);  // first push: offset 0 (sprites rely on it)
    if (m_shadowsActive) {
        FrameUniforms shadowFrame = frame;
        shadowFrame.viewProj = lightViewProj;
        m_shadowFrameOffset = pushUniforms(&shadowFrame, sizeof shadowFrame);
    }

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
        const bool caster = m_shadowsActive && mr.castShadows && lightFrustum.intersects(worldBounds);
        if (!visible) ++m_stats.meshesCulled;
        else ++m_stats.meshesVisible;
        if (!visible && !caster) return;

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
            if (caster && !blend) m_shadowItems.push_back(item);
        }
    });
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
    std::sort(m_shadowItems.begin(), m_shadowItems.end(), [](const MeshItem& a, const MeshItem& b) {
        const bool am = a.pipeline / 2 == 1, bm = b.pipeline / 2 == 1;  // alpha-tested casters need their material
        if (am != bm) return am < bm;
        if (am && a.material != b.material) return a.material < b.material;
        if (a.vertices.index != b.vertices.index) return a.vertices.index < b.vertices.index;
        if (a.firstIndex != b.firstIndex) return a.firstIndex < b.firstIndex;
        if (a.skinOffset != b.skinOffset) return a.skinOffset < b.skinOffset;
        return a.instance < b.instance;
    });
    // Transparent: back to front.
    std::sort(m_transparentItems.begin(), m_transparentItems.end(), [](const MeshItem& a, const MeshItem& b) {
        if (a.depth != b.depth) return a.depth > b.depth;
        return a.instance < b.instance;
    });
    m_stats.shadowCasters = static_cast<u32>(m_shadowItems.size());

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
    // Instance records in draw order: opaque, transparent, shadow casters.
    m_instanceStaging.clear();
    for (const auto* list : {&m_opaqueItems, &m_transparentItems, &m_shadowItems})
        for (const MeshItem& it : *list) m_instanceStaging.push_back(m_instances[it.instance]);
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

void Renderer::drawMeshItems(const std::vector<MeshItem>& items, u32 firstInstance, bool shadow) {
    if (items.empty() || !m_instanceBuffer) return;
    m_device.bindUniformBuffer(kSlotFrame, m_uniformBuffer, shadow ? m_shadowFrameOffset : m_frameOffset, sizeof(FrameUniforms));
    if (!shadow) {
        m_device.bindUniformBuffer(kSlotLights, m_uniformBuffer, m_lights3DOffset, sizeof(LightUniforms));
        m_device.bindTexture(kShadowUnit, m_shadowsActive ? m_device.renderTargetDepthTexture(m_shadowTarget) : m_assets.whiteTexture());
    }
    u32 boundPipeline = ~0u, boundMaterial = ~0u;
    rhi::BufferHandle boundVb;
    usize i = 0;
    while (i < items.size()) {
        const MeshItem& first = items[i];
        usize end = i + 1;
        const bool masked = first.pipeline / 2 == 1;
        while (end < items.size() && items[end].vertices == first.vertices && items[end].firstIndex == first.firstIndex &&
               items[end].indexCount == first.indexCount && items[end].skinOffset == first.skinOffset && first.skinOffset == ~0u &&
               (shadow ? (!masked || items[end].material == first.material) && (items[end].pipeline / 2 == 1) == masked
                       : items[end].material == first.material && items[end].pipeline == first.pipeline))
            ++end;
        const bool skinned = first.skinOffset != ~0u;
        const u32 pipeline = (shadow ? (masked ? 1u : 0u) : first.pipeline) + (skinned ? 100u : 0u);
        if (pipeline != boundPipeline) {
            rhi::PipelineHandle h;
            if (shadow) h = skinned ? m_shadowSkinnedPipelines[masked ? 1 : 0] : (masked ? m_shadowMaskPipeline : m_shadowPipeline);
            else h = skinned ? m_pbrSkinnedPipelines[first.pipeline] : m_pbrPipelines[first.pipeline];
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
    drawMeshItems(m_shadowItems, static_cast<u32>(m_opaqueItems.size() + m_transparentItems.size()), true);
    m_device.endPass();
    m_device.prepareTextureForSampling(m_device.renderTargetDepthTexture(m_shadowTarget));
}

void Renderer::drawSky() {
    if (!m_drawSky) return;
    m_device.bindPipeline(m_skyPipeline);
    m_device.bindUniformBuffer(kSlotFrame, m_uniformBuffer, m_frameOffset, sizeof(FrameUniforms));
    m_device.bindVertexBuffer(m_skyVertices);
    m_device.draw(3);
}

} // namespace agr
