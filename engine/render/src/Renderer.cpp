#include "agr/render/Renderer.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cmath>
#include <cstring>

#include "Shaders.inl"
#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr {

using math::Mat4;
using math::Vec3;
using math::Vec4;

namespace {

struct FrameUniforms {
    Mat4 viewProj;
    Vec4 cameraPos;
    Vec4 lightDir;
    Vec4 lightColor;
    Vec4 ambient;
};
static_assert(sizeof(FrameUniforms) == 128);

struct ObjectUniforms {
    Mat4 model;
    Mat4 normalMatrix;
    Vec4 color;
    Vec4 uvScale;
};
static_assert(sizeof(ObjectUniforms) == 160);

constexpr u32 kSlotFrame = 0;
constexpr u32 kSlotObject = 1;

float srgbToLinear(float c) { return std::pow(std::clamp(c, 0.0f, 1.0f), 2.2f); }
Vec3 srgbToLinear(Vec4 c) { return {srgbToLinear(c.x), srgbToLinear(c.y), srgbToLinear(c.z)}; }

u32 packColor(Vec4 c) {
    auto b = [](float v) { return static_cast<u32>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return b(c.x) | (b(c.y) << 8) | (b(c.z) << 16) | (b(c.w) << 24);
}

} // namespace

Renderer::Renderer(rhi::Device& device, AssetManager& assets) : m_device(device), m_assets(assets) {}

Renderer::~Renderer() {
    for (auto& [uri, cached] : m_textureCache) m_assets.release(cached.asset);
    for (auto& [uri, cached] : m_pixelTextureCache) m_assets.release(cached.asset);
    releaseTargets();
    for (auto& m : m_meshes) {
        m_device.destroyBuffer(m.vertices);
        m_device.destroyBuffer(m.indices);
    }
    m_device.destroyBuffer(m_uniformBuffer);
    m_device.destroyBuffer(m_spriteVertices);
    m_device.destroyBuffer(m_spriteIndices);
    m_device.destroyPipeline(m_meshPipeline);
    m_device.destroyPipeline(m_spritePipeline);
}

Status Renderer::initialize() {
    m_uniformAlign = std::max(16u, m_device.info().uniformBufferAlignment);
    m_glClipSpace = m_device.info().backend != rhi::Backend::Vulkan;
    if (Status s = createPipelines(); !s) return s;
    if (Status s = createMeshes(); !s) return s;
    return {};
}

Status Renderer::createPipelines() {
    rhi::PipelineDesc mesh;
    mesh.debugName = "agr.mesh.lit";
    mesh.shader.vertex = std::string(shaders::kFrameBlock) + shaders::kObjectBlock + shaders::kMeshVertex;
    mesh.shader.fragment = std::string(shaders::kFrameBlock) + shaders::kObjectBlock + shaders::kMeshFragment;
    mesh.layout.stride = sizeof(MeshVertex);
    mesh.layout.attributes = {{0, rhi::VertexFormat::Float3, offsetof(MeshVertex, position)},
                              {1, rhi::VertexFormat::Float3, offsetof(MeshVertex, normal)},
                              {2, rhi::VertexFormat::Float2, offsetof(MeshVertex, uv)}};
    mesh.uniformBlocks = {"FrameData", "ObjectData"};
    mesh.samplers = {"u_albedo"};
    auto mp = m_device.createPipeline(mesh);
    if (!mp) return mp.error();
    m_meshPipeline = mp.value();

    rhi::PipelineDesc sprite;
    sprite.debugName = "agr.sprite";
    sprite.shader.vertex = std::string(shaders::kFrameBlock) + shaders::kSpriteVertex;
    sprite.shader.fragment = shaders::kSpriteFragment;
    sprite.layout.stride = sizeof(SpriteVertex);
    sprite.layout.attributes = {{0, rhi::VertexFormat::Float3, offsetof(SpriteVertex, position)},
                                {1, rhi::VertexFormat::Float2, offsetof(SpriteVertex, uv)},
                                {2, rhi::VertexFormat::UByte4Norm, offsetof(SpriteVertex, color)}};
    sprite.cull = rhi::CullMode::None;
    sprite.blend = rhi::BlendMode::Alpha;
    sprite.depthWrite = false;  // sorted back to front; still hidden behind opaque 3D geometry
    sprite.uniformBlocks = {"FrameData"};
    sprite.samplers = {"u_texture"};
    auto sp = m_device.createPipeline(sprite);
    if (!sp) return sp.error();
    m_spritePipeline = sp.value();
    return {};
}

Status Renderer::createMeshes() {
    for (int i = 0; i < 4; ++i) {
        const MeshData data = makePrimitive(static_cast<PrimitiveMesh>(i));
        auto vb = m_device.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Static,
                                         data.vertices.size() * sizeof(MeshVertex), data.vertices.data(), "agr.primitive.vb"});
        auto ib = m_device.createBuffer({rhi::BufferUsage::Index, rhi::BufferMemory::Static,
                                         data.indices.size() * sizeof(u16), data.indices.data(), "agr.primitive.ib"});
        if (!vb) return vb.error();
        if (!ib) return ib.error();
        m_meshes[i] = {vb.value(), ib.value(), static_cast<u32>(data.indices.size()), data.bounds};
    }
    return {};
}

rhi::TextureHandle Renderer::resolveTexture(const std::string& uri, bool pixelArt) {
    if (uri.empty()) return m_assets.whiteTexture();
    TextureCache& cache = pixelArt ? m_pixelTextureCache : m_textureCache;
    auto it = cache.find(std::string_view(uri));  // no allocation on the hot path
    if (it == cache.end()) {
        TextureLoadOptions o;
        o.pixelArt = pixelArt;
        it = cache.emplace(uri, CachedTexture{m_assets.loadTexture(uri, o), 0}).first;
    }
    it->second.lastUsedFrame = m_frame;
    return m_assets.gpuTexture(it->second.asset);
}

void Renderer::sweepTextureCache() {
    // Release textures no component referenced for ~10 s at 60 FPS.
    constexpr u64 kUnusedFrames = 600;
    for (TextureCache* cache : {&m_textureCache, &m_pixelTextureCache}) {
        for (auto it = cache->begin(); it != cache->end();) {
            if (m_frame - it->second.lastUsedFrame > kUnusedFrames) {
                m_assets.release(it->second.asset);
                it = cache->erase(it);
            } else {
                ++it;
            }
        }
    }
}

void Renderer::releaseTargets() {
    m_device.destroyRenderTarget(m_sceneTarget);
    m_device.destroyRenderTarget(m_resolveTarget);
    m_sceneTarget = m_resolveTarget = {};
    m_targetW = m_targetH = m_targetSamples = 0;
}

void Renderer::ensureTargets(u32 w, u32 h, u32 samples, bool needResolve) {
    const bool haveResolve = static_cast<bool>(m_resolveTarget);
    if (m_sceneTarget && w == m_targetW && h == m_targetH && samples == m_targetSamples && haveResolve == needResolve) return;
    releaseTargets();
    auto scene = m_device.createRenderTarget({w, h, rhi::TextureFormat::RGBA8, true, samples, "agr.scene"});
    if (!scene) {
        AGR_LOG_ERROR("Render", "cannot create scene target: {}", scene.error().message);
        return;
    }
    m_sceneTarget = scene.value();
    if (needResolve) {
        auto resolve = m_device.createRenderTarget({w, h, rhi::TextureFormat::RGBA8, false, 1, "agr.resolve"});
        if (resolve) m_resolveTarget = resolve.value();
    }
    m_targetW = w;
    m_targetH = h;
    m_targetSamples = samples;
}

u32 Renderer::pushUniforms(const void* data, usize size) {
    const usize offset = m_uniformStaging.size();
    m_uniformStaging.resize(offset + alignUp(size, m_uniformAlign));
    std::memcpy(m_uniformStaging.data() + offset, data, size);
    return static_cast<u32>(offset);
}

void Renderer::uploadUniforms() {
    const usize size = m_uniformStaging.size();
    if (size > m_uniformCapacity) {
        m_device.destroyBuffer(m_uniformBuffer);
        m_uniformCapacity = std::max<usize>(64 * 1024, std::bit_ceil(size));
        auto ub = m_device.createBuffer({rhi::BufferUsage::Uniform, rhi::BufferMemory::Dynamic, m_uniformCapacity, nullptr, "agr.uniforms"});
        if (!ub) {
            AGR_LOG_ERROR("Render", "uniform buffer allocation failed: {}", ub.error().message);
            m_uniformCapacity = 0;
            return;
        }
        m_uniformBuffer = ub.value();
    }
    (void)m_device.updateBuffer(m_uniformBuffer, 0, m_uniformStaging.data(), size);
}

void Renderer::render(Scene& scene, u32 width, u32 height) {
    Stopwatch cpu;
    ++m_frame;
    m_stats = {};
    if (width == 0 || height == 0) return;
    auto& reg = scene.registry();

    // ------------------------------------------------------------ camera
    ecs::Entity cameraEntity{};
    reg.view<CameraComponent, WorldTransformComponent>().each([&](ecs::Entity e, CameraComponent& c, WorldTransformComponent&) {
        if (!c.primary || !scene.isActiveInHierarchy(e)) return;
        if (cameraEntity.isNull() || e.index < cameraEntity.index) cameraEntity = e;  // deterministic choice
    });

    const u32 samples = std::clamp<u32>(m_profile.msaaSamples, 1, m_device.info().maxSamples);
    const float scale = std::clamp(m_profile.renderScale, 0.25f, 2.0f);
    const u32 renderW = std::max(1u, static_cast<u32>(std::lround(static_cast<float>(width) * scale)));
    const u32 renderH = std::max(1u, static_cast<u32>(std::lround(static_cast<float>(height) * scale)));

    if (cameraEntity.isNull()) {
        // Nothing to render from: clear to the AGR background so the frame is defined.
        m_device.beginFrame(width, height);
        m_device.beginPass({}, {true, {0.055f, 0.063f, 0.075f, 1.0f}, true, 1.0f});
        m_device.endPass();
        m_device.endFrame();
        m_stats.drawCalls = 0;
        m_stats.gpuTimeMs = m_device.frameStats().gpuTimeMs;
        m_stats.cpuMs = cpu.elapsedMilliseconds();
        return;
    }
    m_stats.hasCamera = true;
    const CameraComponent& cam = reg.get<CameraComponent>(cameraEntity);
    Vec3 camPos, camScale;
    math::Quat camRot;
    math::decompose(reg.get<WorldTransformComponent>(cameraEntity).matrix, camPos, camRot, camScale);
    const Mat4 view = math::inverse(Mat4::trs(camPos, camRot, Vec3{1.0f}));
    const float aspect = static_cast<float>(renderW) / static_cast<float>(renderH);
    const auto range = m_glClipSpace ? Mat4::DepthRange::NegativeOneToOne : Mat4::DepthRange::ZeroToOne;
    const float nearZ = std::max(cam.nearPlane, 1e-4f), farZ = std::max(cam.farPlane, nearZ + 1e-3f);
    Mat4 proj;
    if (cam.projection == Projection::Orthographic) {
        const float hh = std::max(cam.orthographicSize, 1e-4f), hw = hh * aspect;
        proj = Mat4::orthographic(-hw, hw, -hh, hh, nearZ, farZ, range);
    } else {
        proj = Mat4::perspective(std::clamp(cam.fieldOfView, 0.01f, 3.1f), aspect, nearZ, farZ, range);
    }
    const Mat4 viewProj = proj * view;
    const math::Frustum frustum = math::Frustum::fromMatrix(viewProj, range);

    // ------------------------------------------------------------ lights
    FrameUniforms frame;
    frame.viewProj = viewProj;
    frame.cameraPos = Vec4(camPos, 1.0f);
    frame.lightDir = Vec4(0, -1, 0, 0);
    frame.lightColor = Vec4(0, 0, 0, 0);
    {
        bool found = false;
        reg.view<DirectionalLightComponent, WorldTransformComponent>().each(
            [&](ecs::Entity e, DirectionalLightComponent& l, WorldTransformComponent& w) {
                if (found || !scene.isActiveInHierarchy(e)) return;
                found = true;
                frame.lightDir = Vec4(math::normalize(w.matrix.transformVector({0, 0, -1})), 0.0f);
                frame.lightColor = Vec4(srgbToLinear(l.color) * l.intensity, 0.0f);
            });
        EnvironmentComponent env;
        bool envFound = false;
        reg.view<EnvironmentComponent>().each([&](ecs::Entity e, EnvironmentComponent& c) {
            if (!envFound && scene.isActiveInHierarchy(e)) {
                env = c;
                envFound = true;
            }
        });
        frame.ambient = Vec4(srgbToLinear(env.ambientColor) * env.ambientIntensity, 0.0f);
    }
    m_uniformStaging.clear();
    pushUniforms(&frame, sizeof frame);

    // ------------------------------------------------------------ meshes
    m_meshDraws.clear();
    reg.view<MeshRendererComponent, WorldTransformComponent>().each(
        [&](ecs::Entity, MeshRendererComponent& mr, WorldTransformComponent& w) {
            if (!mr.visible || !w.activeInHierarchy) return;
            const int type = std::clamp(static_cast<int>(mr.mesh), 0, 3);
            if (!frustum.intersects(m_meshes[type].bounds.transformed(w.matrix))) {
                ++m_stats.meshesCulled;
                return;
            }
            ObjectUniforms obj;
            obj.model = w.matrix;
            obj.normalMatrix = math::transpose(math::inverse(w.matrix));
            obj.color = Vec4(srgbToLinear(mr.color), mr.color.w);
            obj.uvScale = Vec4(mr.uvScale.x, mr.uvScale.y, 0, 0);
            m_meshDraws.push_back({static_cast<PrimitiveMesh>(type), resolveTexture(mr.texture, false),
                                   pushUniforms(&obj, sizeof obj)});
        });
    m_stats.meshesVisible = static_cast<u32>(m_meshDraws.size());
    std::sort(m_meshDraws.begin(), m_meshDraws.end(), [](const MeshDraw& a, const MeshDraw& b) {
        if (a.mesh != b.mesh) return a.mesh < b.mesh;
        return a.texture.index < b.texture.index;
    });

    // ------------------------------------------------------------ sprites
    m_spriteItems.clear();
    std::string lastUri;
    bool lastPixelArt = false, lastHandleValid = false;
    rhi::TextureHandle lastHandle;
    reg.view<SpriteRendererComponent, WorldTransformComponent>().each(
        [&](ecs::Entity, SpriteRendererComponent& s, WorldTransformComponent& w) {
            if (!s.visible || !w.activeInHierarchy) return;
            SpriteItem item;
            const float x0 = -s.pivot.x * s.size.x, x1 = (1.0f - s.pivot.x) * s.size.x;
            const float y0 = -s.pivot.y * s.size.y, y1 = (1.0f - s.pivot.y) * s.size.y;
            item.corners[0] = w.matrix.transformPoint({x0, y0, 0});
            item.corners[1] = w.matrix.transformPoint({x1, y0, 0});
            item.corners[2] = w.matrix.transformPoint({x1, y1, 0});
            item.corners[3] = w.matrix.transformPoint({x0, y1, 0});
            math::Aabb box;
            for (const Vec3& c : item.corners) box.expand(c);
            if (!frustum.intersects(box)) {
                ++m_stats.spritesCulled;
                return;
            }
            item.layer = s.sortingLayer;
            item.order = s.orderInLayer;
            item.depth = (view.transformPoint(box.center())).z;  // more negative = farther
            item.uv = s.uvRect;
            if (s.flipX) std::swap(item.uv.x, item.uv.z);
            if (s.flipY) std::swap(item.uv.y, item.uv.w);
            item.color = packColor(s.color);
            // Neighbouring sprites usually share a texture: a string compare beats a hash lookup.
            if (s.texture != lastUri || s.pixelArt != lastPixelArt || !lastHandleValid) {
                lastUri = s.texture;
                lastPixelArt = s.pixelArt;
                lastHandle = resolveTexture(s.texture, s.pixelArt);
                lastHandleValid = true;
            }
            item.texture = lastHandle;
            m_spriteItems.push_back(item);
        });
    m_stats.spritesVisible = static_cast<u32>(m_spriteItems.size());
    // Painter's order: layer, order, then back to front; texture only breaks ties.
    // Small keys are sorted instead of the 80-byte items.
    m_spriteKeys.clear();
    m_spriteKeys.reserve(m_spriteItems.size());
    for (u32 i = 0; i < m_spriteItems.size(); ++i) {
        const SpriteItem& it = m_spriteItems[i];
        m_spriteKeys.push_back({it.layer, it.order, it.depth, it.texture.index, i});
    }
    std::sort(m_spriteKeys.begin(), m_spriteKeys.end(), [](const SortKey& a, const SortKey& b) {
        if (a.layer != b.layer) return a.layer < b.layer;
        if (a.order != b.order) return a.order < b.order;
        if (a.depth != b.depth) return a.depth < b.depth;
        if (a.texture != b.texture) return a.texture < b.texture;
        return a.index < b.index;  // deterministic
    });

    m_stats.collectMs = cpu.elapsedMilliseconds();
    Stopwatch submit;
    m_device.beginFrame(width, height);
    uploadUniforms();

    // ------------------------------------------------------------ passes
    const bool offscreen = samples > 1 || renderW != width || renderH != height;
    if (offscreen) ensureTargets(renderW, renderH, samples, samples > 1 && (renderW != width || renderH != height));
    const rhi::RenderTargetHandle target = offscreen ? m_sceneTarget : rhi::RenderTargetHandle{};
    m_device.beginPass(target, {true, cam.clearColor, true, 1.0f});
    if (m_uniformCapacity > 0) {
        drawMeshes(m_meshDraws);
        drawSprites(m_spriteItems);
    }
    m_device.endPass();
    if (offscreen && m_sceneTarget) {
        if (m_resolveTarget) {
            m_device.blit(m_sceneTarget, m_resolveTarget, rhi::Filter::Nearest);  // MSAA resolve
            m_device.blit(m_resolveTarget, {}, rhi::Filter::Linear);              // upscale
        } else {
            m_device.blit(m_sceneTarget, {}, rhi::Filter::Linear);
        }
    }
    m_device.endFrame();
    m_stats.submitMs = submit.elapsedMilliseconds();

    if (m_frame % 300 == 0) sweepTextureCache();
    const rhi::FrameStats& ds = m_device.frameStats();
    m_stats.drawCalls = ds.drawCalls;
    m_stats.triangles = ds.triangles;
    m_stats.gpuTimeMs = ds.gpuTimeMs;
    m_stats.gpuWaitMs = ds.cpuWaitMs;
    m_stats.renderWidth = renderW;
    m_stats.renderHeight = renderH;
    m_stats.msaaSamples = samples;
    const rhi::MemoryStats mem = m_device.memoryStats();
    m_stats.gpuMemoryBytes = mem.bufferBytes + mem.textureBytes;
    m_stats.cpuMs = cpu.elapsedMilliseconds();
}

void Renderer::drawMeshes(const std::vector<MeshDraw>& draws) {
    if (draws.empty()) return;
    m_device.bindPipeline(m_meshPipeline);
    m_device.bindUniformBuffer(kSlotFrame, m_uniformBuffer, 0, sizeof(FrameUniforms));
    int boundMesh = -1;
    for (const MeshDraw& d : draws) {
        const int type = static_cast<int>(d.mesh);
        if (type != boundMesh) {
            m_device.bindVertexBuffer(m_meshes[type].vertices);
            m_device.bindIndexBuffer(m_meshes[type].indices, rhi::IndexType::U16);
            boundMesh = type;
        }
        m_device.bindTexture(0, d.texture);
        m_device.bindUniformBuffer(kSlotObject, m_uniformBuffer, d.uniformOffset, sizeof(ObjectUniforms));
        m_device.drawIndexed(m_meshes[type].indexCount);
    }
}

void Renderer::drawSprites(const std::vector<SpriteItem>& items) {
    if (items.empty()) return;

    // Grow GPU buffers (indices are a fixed quad pattern, rebuilt only on growth).
    const u32 count = static_cast<u32>(items.size());
    if (count > m_spriteCapacity) {
        m_device.destroyBuffer(m_spriteVertices);
        m_device.destroyBuffer(m_spriteIndices);
        m_spriteCapacity = std::max(1024u, std::bit_ceil(count));
        std::vector<u32> indices(static_cast<usize>(m_spriteCapacity) * 6);
        for (u32 q = 0; q < m_spriteCapacity; ++q) {
            const u32 v = q * 4;
            const u32 pattern[6] = {v, v + 1, v + 2, v, v + 2, v + 3};
            std::copy(pattern, pattern + 6, indices.begin() + static_cast<std::ptrdiff_t>(q) * 6);
        }
        auto vb = m_device.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Dynamic,
                                         static_cast<usize>(m_spriteCapacity) * 4 * sizeof(SpriteVertex), nullptr, "agr.sprites.vb"});
        auto ib = m_device.createBuffer({rhi::BufferUsage::Index, rhi::BufferMemory::Static,
                                         indices.size() * sizeof(u32), indices.data(), "agr.sprites.ib"});
        if (!vb || !ib) {
            AGR_LOG_ERROR("Render", "sprite buffer allocation failed");
            m_spriteCapacity = 0;
            return;
        }
        m_spriteVertices = vb.value();
        m_spriteIndices = ib.value();
    }

    m_spriteStaging.clear();
    m_spriteStaging.reserve(static_cast<usize>(count) * 4);
    for (const SortKey& key : m_spriteKeys) {
        const SpriteItem& s = items[key.index];
        const float u0 = s.uv.x, v0 = s.uv.y, u1 = s.uv.z, v1 = s.uv.w;
        m_spriteStaging.push_back({s.corners[0], {u0, v1}, s.color});
        m_spriteStaging.push_back({s.corners[1], {u1, v1}, s.color});
        m_spriteStaging.push_back({s.corners[2], {u1, v0}, s.color});
        m_spriteStaging.push_back({s.corners[3], {u0, v0}, s.color});
    }
    (void)m_device.updateBuffer(m_spriteVertices, 0, m_spriteStaging.data(), m_spriteStaging.size() * sizeof(SpriteVertex));

    m_device.bindPipeline(m_spritePipeline);
    m_device.bindUniformBuffer(kSlotFrame, m_uniformBuffer, 0, sizeof(FrameUniforms));
    m_device.bindVertexBuffer(m_spriteVertices);
    m_device.bindIndexBuffer(m_spriteIndices, rhi::IndexType::U32);
    u32 batchStart = 0;
    auto textureAt = [&](u32 i) { return items[m_spriteKeys[i].index].texture; };
    for (u32 i = 1; i <= count; ++i) {
        if (i == count || textureAt(i) != textureAt(batchStart)) {
            m_device.bindTexture(0, textureAt(batchStart));
            m_device.drawIndexed((i - batchStart) * 6, batchStart * 6);
            ++m_stats.spriteBatches;
            batchStart = i;
        }
    }
}

} // namespace agr
