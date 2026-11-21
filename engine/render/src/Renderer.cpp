#include "agr/render/Renderer.hpp"
#include "agr/render/Skin2D.hpp"
#include "agr/core/Profiler.hpp"
#include <optional>

#include <algorithm>
#include <chrono>
#include <functional>
#include <bit>
#include <cstddef>
#include <cmath>
#include <cstring>

#include "RenderUniforms.hpp"
#include "Shaders.inl"
#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr {

using math::Mat4;
using math::Vec3;
using math::Vec2;
using math::Vec4;

namespace {

using detail::FrameUniforms;

constexpr u32 kSlotFrame = 0;
constexpr u32 kSlotLights2D = 1;  // sprite pipelines: FrameData, Lights2D

struct Lights2DUniforms {
    Vec4 posRadius[Renderer::kMaxLights2D];
    Vec4 colorFalloff[Renderer::kMaxLights2D];
    Vec4 shadow[Renderer::kMaxLights2D];  // row (-1: none), softness, strength
    i32 count[4];
};
static_assert(sizeof(Lights2DUniforms) == 784);

/// Distance from `o` along unit direction `dir` to segment [a, b], or a negative value.
float raySegment(Vec2 o, Vec2 dir, Vec2 a, Vec2 b) {
    const Vec2 e = b - a, w = a - o;
    const float den = dir.x * e.y - dir.y * e.x;
    if (std::fabs(den) < 1e-9f) return -1.0f;
    const float t = (w.x * e.y - w.y * e.x) / den;  // along the ray
    const float s = (w.x * dir.y - w.y * dir.x) / den;  // along the segment
    return s >= 0.0f && s <= 1.0f ? t : -1.0f;
}

using detail::srgbToLinear;

} // namespace

Renderer::Renderer(rhi::Device& device, AssetManager& assets) : m_device(device), m_assets(assets) {}

Renderer::~Renderer() {
    for (auto& [uri, cached] : m_textureCache) m_assets.release(cached.asset);
    for (auto& [uri, cached] : m_pixelTextureCache) m_assets.release(cached.asset);
    for (auto& [uri, cached] : m_dataTextureCache) m_assets.release(cached.asset);
    for (auto& [uri, cached] : m_pixelDataTextureCache) m_assets.release(cached.asset);
    releaseProbes();
    releaseTargets();
    for (auto& m : m_meshes) {
        m_device.destroyBuffer(m.vertices);
        m_device.destroyBuffer(m.indices);
    }
    m_device.destroyBuffer(m_uniformBuffer);
    m_device.destroyBuffer(m_spriteVertices);
    m_device.destroyBuffer(m_spriteIndices);
    for (auto& [key, g] : m_terrains) releaseTerrain(g);
    for (auto& [key, g] : m_morphs) m_device.destroyBuffer(g.buffer);
    for (auto& [n, w] : m_waterGrids) {
        m_device.destroyBuffer(w.vertices);
        m_device.destroyBuffer(w.indices);
    }
    m_device.destroyPipeline(m_waterPipeline);
    m_device.destroyPipeline(m_blobPipeline);
    m_device.destroyPipeline(m_terrainPipeline);
    for (auto p : m_pbrPipelines) m_device.destroyPipeline(p);
    for (auto p : m_pbrSkinnedPipelines) m_device.destroyPipeline(p);
    for (CustomPipelines& set : m_customSets) destroyCustomPipelines(set);
    for (auto p : m_shadowSkinnedPipelines) m_device.destroyPipeline(p);
    for (auto p : m_outlinePipelines) m_device.destroyPipeline(p);
    m_device.destroyPipeline(m_shadowPipeline);
    m_device.destroyPipeline(m_shadowMaskPipeline);
    m_device.destroyPipeline(m_skyPipeline);
    m_device.destroyBuffer(m_skyVertices);
    m_device.destroyBuffer(m_instanceBuffer);
    m_device.destroyTexture(m_flatNormal);
    m_device.destroyTexture(m_shadow2D);
    m_device.destroyRenderTarget(m_shadowTarget);
    m_device.destroyRenderTarget(m_spotShadowTarget);
    m_device.destroyPipeline(m_spritePipeline);
    m_device.destroyPipeline(m_spriteLitPipeline);
    m_device.destroyPipeline(m_spriteLitNormalPipeline);
    m_device.destroyPipeline(m_spriteAdditivePipeline);
    m_device.destroyPipeline(m_overlayPipeline);
    for (auto p : m_postPipelines) m_device.destroyPipeline(p);
    releasePostTargets();
    m_device.destroyBuffer(m_overlayVertices);
    m_device.destroyBuffer(m_overlayIndices);
    m_device.destroyTexture(m_softParticle);
}

Status Renderer::initialize() {
    m_uniformAlign = std::max(16u, m_device.info().uniformBufferAlignment);
    m_glClipSpace = m_device.info().backend != rhi::Backend::Vulkan;
    if (Status s = createPipelines(); !s) return s;
    if (Status s = create3DPipelines(); !s) return s;
    if (Status s = createPostPipelines(); !s) return s;
    if (Status s = createMeshes(); !s) return s;
    return {};
}

Status Renderer::createPipelines() {
    rhi::PipelineDesc sprite;
    sprite.debugName = "agr.sprite";
    sprite.shader.vertex = std::string(shaders::kFrameBlock) + shaders::kSpriteVertex;
    sprite.shader.fragment = std::string(shaders::kFrameBlock) + shaders::kSpriteFragment;
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

    rhi::PipelineDesc additive = sprite;
    additive.debugName = "agr.sprite.additive";
    additive.blend = rhi::BlendMode::Additive;
    auto ap = m_device.createPipeline(additive);
    if (!ap) return ap.error();
    m_spriteAdditivePipeline = ap.value();

    rhi::PipelineDesc overlay = sprite;
    overlay.debugName = "agr.overlay";
    overlay.depthTest = false;  // drawn last, over everything
    auto op = m_device.createPipeline(overlay);
    if (!op) return op.error();
    m_overlayPipeline = op.value();

    rhi::PipelineDesc lit = sprite;
    lit.debugName = "agr.sprite.lit";
    lit.shader.fragment = std::string(shaders::kFrameBlock) + shaders::kSpriteLitFragment;
    lit.uniformBlocks = {"FrameData", "Lights2D"};
    lit.samplers = {"u_texture", "u_shadow2D", "u_normalMap"};
    auto lp = m_device.createPipeline(lit);
    if (!lp) return lp.error();
    m_spriteLitPipeline = lp.value();
    rhi::PipelineDesc litNormal = lit;
    litNormal.debugName = "agr.sprite.lit.normal";
    litNormal.shader.fragment = std::string("#define AGR_NORMAL_MAP 1\n") + shaders::kFrameBlock + shaders::kSpriteLitFragment;
    auto lnp = m_device.createPipeline(litNormal);
    if (!lnp) return lnp.error();
    m_spriteLitNormalPipeline = lnp.value();

    // Default particle: soft white disc (procedural, no asset needed).
    constexpr u32 kSize = 64;
    std::vector<u8> pixels(kSize * kSize * 4);
    for (u32 y = 0; y < kSize; ++y)
        for (u32 x = 0; x < kSize; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / kSize * 2.0f - 1.0f, dy = (static_cast<float>(y) + 0.5f) / kSize * 2.0f - 1.0f;
            const float a = std::clamp(1.0f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
            u8* px = &pixels[(y * kSize + x) * 4];
            px[0] = px[1] = px[2] = 255;
            px[3] = static_cast<u8>(std::lround(a * a * 255.0f));
        }
    rhi::TextureDesc td;
    td.width = td.height = kSize;
    td.format = rhi::TextureFormat::RGBA8;
    td.data = pixels.data();
    td.sampler = {rhi::Filter::Linear, rhi::Filter::Linear, true, rhi::Wrap::Clamp, rhi::Wrap::Clamp};
    td.debugName = "agr.particle.soft";
    auto soft = m_device.createTexture(td);
    if (!soft) return soft.error();
    m_softParticle = soft.value();
    return {};
}

Status Renderer::createMeshes() {
    for (int i = 0; i < 4; ++i) {
        const MeshData data = makePrimitive(static_cast<PrimitiveMesh>(i));
        auto vb = m_device.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Static,
                                         data.vertices.size() * sizeof(MeshVertex), data.vertices.data(), "agr.primitive.vb"});
        auto ib = m_device.createBuffer({rhi::BufferUsage::Index, rhi::BufferMemory::Static,
                                         data.indices.size() * sizeof(u32), data.indices.data(), "agr.primitive.ib"});
        if (!vb) return vb.error();
        if (!ib) return ib.error();
        m_meshes[i] = {vb.value(), ib.value(), static_cast<u32>(data.indices.size()), data.bounds};
    }
    return {};
}

rhi::TextureHandle Renderer::resolveTexture(std::string_view uri, bool pixelArt) {
    const TextureInfo* info = nullptr;
    return resolveTexture(uri, pixelArt, info);
}

rhi::TextureHandle Renderer::resolveTexture(std::string_view uri, bool pixelArt, const TextureInfo*& info) {
    info = nullptr;
    if (uri.empty()) return m_assets.whiteTexture();
    TextureCache& cache = pixelArt ? m_pixelTextureCache : m_textureCache;
    auto it = cache.find(uri);  // no allocation on the hot path
    if (it == cache.end()) {
        TextureLoadOptions o;
        o.pixelArt = pixelArt;
        it = cache.emplace(std::string(uri), CachedTexture{m_assets.loadTexture(uri, o), 0}).first;
    }
    it->second.lastUsedFrame = m_frame;
    info = m_assets.texture(it->second.asset);
    return m_assets.gpuTexture(it->second.asset);
}

void Renderer::sweepTextureCache() {
    // Release textures no component referenced for ~10 s at 60 FPS.
    constexpr u64 kUnusedFrames = 600;
    for (TextureCache* cache : {&m_textureCache, &m_pixelTextureCache, &m_dataTextureCache, &m_pixelDataTextureCache}) {
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
    auto scene = m_device.createRenderTarget({.width = w, .height = h, .colorFormat = rhi::TextureFormat::RGBA8, .depth = true, .samples = samples, .debugName = "agr.scene"});
    if (!scene) {
        AGR_LOG_ERROR("Render", "cannot create scene target: {}", scene.error().message);
        return;
    }
    m_sceneTarget = scene.value();
    if (needResolve) {
        auto resolve = m_device.createRenderTarget({.width = w, .height = h, .colorFormat = rhi::TextureFormat::RGBA8, .depth = false, .samples = 1, .debugName = "agr.resolve"});
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

Mat4 RenderView::projectionMatrix(float aspect, bool glClipSpace) const {
    const auto range = glClipSpace ? Mat4::DepthRange::NegativeOneToOne : Mat4::DepthRange::ZeroToOne;
    const float nearZ = std::max(nearPlane, 1e-4f), farZ = std::max(farPlane, nearZ + 1e-3f);
    if (projection == Projection::Orthographic) {
        const math::Vec2 he = orthographicHalfExtents(scaleMode, orthographicSize, referenceAspect, aspect);
        return Mat4::orthographic(-he.x, he.x, -he.y, he.y, nearZ, farZ, range);
    }
    return Mat4::perspective(std::clamp(fieldOfView, 0.01f, 3.1f), aspect, nearZ, farZ, range);
}

std::optional<RenderView> RenderView::fromPrimaryCamera(Scene& scene) {
    auto& reg = scene.registry();
    ecs::Entity cameraEntity{};
    reg.view<CameraComponent, WorldTransformComponent>().each([&](ecs::Entity e, CameraComponent& c, WorldTransformComponent&) {
        if (!c.primary || !scene.isActiveInHierarchy(e)) return;
        if (cameraEntity.isNull() || e.index < cameraEntity.index) cameraEntity = e;  // deterministic choice
    });
    if (cameraEntity.isNull()) return std::nullopt;
    const CameraComponent& cam = reg.get<CameraComponent>(cameraEntity);
    Vec3 pos, scale;
    math::Quat rot;
    math::decompose(reg.get<WorldTransformComponent>(cameraEntity).matrix, pos, rot, scale);
    RenderView v;
    v.view = math::inverse(Mat4::trs(pos, rot, Vec3{1.0f}));  // camera scale is ignored
    v.position = pos;
    v.projection = cam.projection;
    v.fieldOfView = cam.fieldOfView;
    v.orthographicSize = cam.orthographicSize;
    v.scaleMode = cam.scaleMode;
    v.referenceAspect = cam.referenceAspect;
    v.nearPlane = cam.nearPlane;
    v.farPlane = cam.farPlane;
    v.clearColor = cam.clearColor;
    return v;
}

void Renderer::render(Scene& scene, u32 width, u32 height) {
    if (width == 0 || height == 0) return;
    const std::optional<RenderView> view = RenderView::fromPrimaryCamera(scene);
    m_device.beginFrame(width, height);
    if (view) {
        renderView(scene, *view, {}, width, height);
    } else {
        // Nothing to render from: clear to the AGR background so the frame is defined.
        Stopwatch cpu;
        m_stats = {};
        m_device.beginPass({}, {true, {0.055f, 0.063f, 0.075f, 1.0f}, true, 1.0f});
        m_device.endPass();
        m_stats.cpuMs = cpu.elapsedMilliseconds();
    }
    m_device.endFrame();
    m_stats.gpuTimeMs = m_device.frameStats().gpuTimeMs;
    m_stats.frameWaitMs = m_device.frameStats().cpuWaitMs;
}

void Renderer::renderView(Scene& scene, const RenderView& rv, rhi::RenderTargetHandle outputTarget, u32 width, u32 height) {
    // Per-frame GPU data (uniforms, instances, sprites) is written once per device frame:
    // a second view in the same frame would overwrite the first one's data on Vulkan.
    const u64 deviceFrame = m_device.frameStats().frameIndex;
    if (deviceFrame != 0 && deviceFrame == m_lastViewFrame) {
        if (!m_warnedSecondView) AGR_LOG_ERROR("Render", "renderView called twice in one frame on the same Renderer: use one Renderer per view");
        m_warnedSecondView = true;
        return;
    }
    m_lastViewFrame = deviceFrame;
    if (!m_probeCaptureMode && !rv.direct) updateReflectionProbes(scene);  // before this view's uniforms (own renderer)
    Stopwatch cpu;
    AGR_PROFILE_SCOPE("Renderer");
    std::optional<ProfileScope> preparation(std::in_place, "Préparation (culling, lots, lumières)");  // early returns close it
    ++m_frame;
    {
        const bool captured = m_stats.probeCapture;  // set by updateReflectionProbes above
        m_stats = {};
        m_stats.probeCapture = captured;
    }
    if (width == 0 || height == 0) return;
    auto& reg = scene.registry();
    m_stats.hasCamera = true;

    const u32 samples = rv.direct ? 1u : std::clamp<u32>(m_profile.msaaSamples, 1, m_device.info().maxSamples);
    const float scale = rv.direct ? 1.0f : std::clamp(m_profile.renderScale, 0.25f, 2.0f);
    const u32 renderW = std::max(1u, static_cast<u32>(std::lround(static_cast<float>(width) * scale)));
    const u32 renderH = std::max(1u, static_cast<u32>(std::lround(static_cast<float>(height) * scale)));

    const Mat4 view = rv.view;
    const float aspect = static_cast<float>(renderW) / static_cast<float>(renderH);
    const auto range = m_glClipSpace ? Mat4::DepthRange::NegativeOneToOne : Mat4::DepthRange::ZeroToOne;
    // Post-processing: first active PostProcess component (not for direct previews).
    PostProcessComponent post;
    bool postActive = false;
    if (!rv.direct)
        reg.view<PostProcessComponent>().each([&](ecs::Entity e, PostProcessComponent& c) {
            if (!postActive && scene.isActiveInHierarchy(e)) {
                post = c;
                postActive = true;
            }
        });
    // Temporal AA: the projection moves inside the pixel every frame (Halton 2, 3 over 8 frames).
    const bool taa = postActive && post.temporalAA && m_profile.taa && rv.projection == Projection::Perspective;
    Mat4 proj = rv.projectionMatrix(aspect, m_glClipSpace);
    const Mat4 unjitteredViewProj = proj * view;
    if (taa) {
        auto halton = [](u32 i, u32 b) {
            float f = 1.0f, r = 0.0f;
            for (; i > 0; i /= b) {
                f /= static_cast<float>(b);
                r += f * static_cast<float>(i % b);
            }
            return r;
        };
        const u32 k = static_cast<u32>(m_frame % 8) + 1;
        const float jx = (halton(k, 2) - 0.5f) * 2.0f / static_cast<float>(renderW), jy = (halton(k, 3) - 0.5f) * 2.0f / static_cast<float>(renderH);
        proj.c[2].x += jx;  // clip x += jx * view z, w = -view z: a constant NDC offset
        proj.c[2].y += jy;
    }
    const Mat4 viewProj = proj * view;
    const math::Frustum frustum = math::Frustum::fromMatrix(viewProj, range);
    auto* states = reg.findPool<StateComponent>();
    auto hiddenInEditor = [&](ecs::Entity e) {
        if (!rv.editorView || !states) return false;
        const StateComponent* st = states->tryGet(e);
        return st && st->hiddenInEditor;
    };

    // ------------------------------------------------------------ frame: light, environment
    FrameUniforms frame{};
    frame.time = Vec4(static_cast<float>(std::fmod(m_time, 3600.0)), 0.0f, 1.0f / static_cast<float>(renderW),
                      1.0f / static_cast<float>(renderH));
    frame.viewProj = viewProj;
    frame.invViewProj = math::inverse(viewProj);
    frame.cameraPos = Vec4(rv.position, 1.0f);
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
        // Image sky: the environment map once loaded (the gradient stands in while it loads).
        m_envTexture = {};
        const EnvironmentAsset* image = nullptr;
        if (env.sky == SkyMode::Image && m_environments) {
            image = m_environments->get(env.skyTexture);
            if (image && image->state == AssetState::Loading) m_stats.environmentLoading = true;
            if (image && (image->state != AssetState::Ready || !image->texture)) image = nullptr;
        }
        const bool gradient = env.sky == SkyMode::Gradient || (env.sky == SkyMode::Image && !image);
        const Vec3 sky = srgbToLinear(env.skyColor), ground = srgbToLinear(env.groundColor);
        // Sprites lit by 2D lights use one flat ambient: with a gradient sky, its average.
        frame.ambient = gradient ? Vec4((sky + ground) * 0.5f * env.ambientIntensity, 0.0f)
                                 : Vec4(srgbToLinear(env.ambientColor) * env.ambientIntensity, 0.0f);
        if (image) {
            m_envTexture = image->texture;
            const float intensity = std::max(env.skyIntensity, 0.0f);
            for (usize i = 0; i < 9; ++i) frame.envSH[i] = Vec4(image->sh[i], 0.0f);
            frame.envParams = Vec4(1.0f, intensity, env.skyRotation, static_cast<float>(image->levels > 0 ? image->levels - 1 : 0));
            frame.ambient = Vec4(image->average * intensity * env.ambientIntensity, 0.0f);
            m_stats.imageSky = true;
        }
        frame.skyColor = Vec4(sky, gradient ? 1.0f : 0.0f);
        frame.groundColor = Vec4(ground, env.ambientIntensity);
        frame.horizonColor = Vec4(srgbToLinear(env.horizonColor), env.sunDisc ? 1.0f : 0.0f);
        frame.fogColor = Vec4(srgbToLinear(env.fogColor), env.fog ? 1.0f : 0.0f);
        frame.fogParams = Vec4(env.fogStart, std::max(env.fogEnd, env.fogStart + 0.01f), std::max(env.exposure, 0.0f), 0.0f);
        m_drawSky = (gradient || image) && rv.projection == Projection::Perspective;
    }
    const bool postHdr = postActive && m_device.info().halfFloatRenderTargets;
    frame.fogParams.w = postHdr || (m_probeCaptureMode && m_probeLinear) ? 1.0f : 0.0f;  // shaders output linear light; the composite pass tone maps
    if (!m_probeCaptureMode) m_probeExposure = frame.fogParams.z;
    m_uniformStaging.clear();
    collect3D(scene, rv, view, viewProj, frustum, aspect, hiddenInEditor, frame);  // pushes the frame uniforms first

    // ------------------------------------------------------------ parallax
    // Offsets applied at draw time to Parallax entities and their descendants
    // (innermost Parallax wins). Transforms are left untouched.
    m_parallax.clear();
    {
        std::vector<std::pair<u32, ecs::Entity>> roots;  // (depth, entity)
        reg.view<ParallaxComponent>().each([&](ecs::Entity e, ParallaxComponent&) {
            u32 depth = 0;
            for (ecs::Entity p = scene.parent(e); !p.isNull(); p = scene.parent(p)) ++depth;
            roots.emplace_back(depth, e);
        });
        std::sort(roots.begin(), roots.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [depth, e] : roots) {
            const math::Vec2 f = reg.get<ParallaxComponent>(e).factor;
            const Vec3 offset{rv.position.x * (1.0f - f.x), rv.position.y * (1.0f - f.y), 0.0f};
            std::vector<ecs::Entity> stack{e};
            while (!stack.empty()) {
                const ecs::Entity x = stack.back();
                stack.pop_back();
                m_parallax[x.index] = offset;
                for (ecs::Entity c : scene.children(x)) stack.push_back(c);
            }
        }
    }
    auto parallaxOf = [&](ecs::Entity e) -> Vec3 {
        if (m_parallax.empty()) return {};
        auto it = m_parallax.find(e.index);
        return it == m_parallax.end() ? Vec3{} : it->second;
    };

    // ------------------------------------------------------------ 2D lights
    {
        Lights2DUniforms lights{};
        u32 n = 0;
        // Occluder edges in world XY (boxes; tilemaps: one box per run of filled cells in a row).
        struct Edge {
            Vec2 a, b;
        };
        std::vector<Edge> edges;
        bool occludersCollected = false;
        auto collectOccluders = [&] {
            occludersCollected = true;
            auto addBox = [&](const Mat4& m, Vec2 mn, Vec2 mx) {
                Vec2 c[4];
                const Vec2 local[4] = {{mn.x, mn.y}, {mx.x, mn.y}, {mx.x, mx.y}, {mn.x, mx.y}};
                for (int k = 0; k < 4; ++k) {
                    const Vec3 p = m.transformPoint({local[k].x, local[k].y, 0.0f});
                    c[k] = {p.x, p.y};
                }
                for (int k = 0; k < 4; ++k) edges.push_back({c[k], c[(k + 1) % 4]});
            };
            reg.view<LightOccluder2DComponent, WorldTransformComponent>().each(
                [&](ecs::Entity e, LightOccluder2DComponent& o, WorldTransformComponent& w) {
                    if (!w.activeInHierarchy) return;
                    if (const auto* tm = reg.tryGet<TilemapComponent>(e)) {
                        for (u32 y = 0; y < tm->height; ++y)
                            for (u32 x = 0; x < tm->width;) {
                                if (tm->tileAt(x, y) < 0) {
                                    ++x;
                                    continue;
                                }
                                u32 end = x;
                                while (end < tm->width && tm->tileAt(end, y) >= 0) ++end;
                                addBox(w.matrix, {static_cast<float>(x) * tm->cellSize.x, static_cast<float>(y) * tm->cellSize.y},
                                       {static_cast<float>(end) * tm->cellSize.x, static_cast<float>(y + 1) * tm->cellSize.y});
                                x = end;
                            }
                        return;
                    }
                    addBox(w.matrix, o.offset - o.size * 0.5f, o.offset + o.size * 0.5f);
                });
        };
        u32 shadowRows = 0;
        if (m_shadow2DBins.size() != kShadowBins2D) m_shadow2DBins.resize(kShadowBins2D);
        if (m_shadow2DPixels.size() != kShadowBins2D * kShadowRows2D * 4) m_shadow2DPixels.assign(kShadowBins2D * kShadowRows2D * 4, 255);
        reg.view<Light2DComponent, WorldTransformComponent>().each([&](ecs::Entity e, Light2DComponent& l, WorldTransformComponent& w) {
            if (n >= kMaxLights2D || !w.activeInHierarchy || hiddenInEditor(e) || l.intensity <= 0.0f) return;
            if (l.directional) {
                // Direction in xy (w < 0 flags it for the shader); no radius, falloff or shadows.
                Vec3 d = w.matrix.transformVector({1.0f, 0.0f, 0.0f});
                const float len = std::sqrt(d.x * d.x + d.y * d.y);
                d = len > 1e-6f ? Vec3{d.x / len, d.y / len, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
                lights.posRadius[n] = Vec4(d.x, d.y, std::max(l.height, 0.0f), -1.0f);
                lights.colorFalloff[n] = Vec4(srgbToLinear(l.color) * l.intensity, 1.0f);
                lights.shadow[n] = Vec4(-1.0f, 0.0f, 0.0f, 0.0f);
                ++n;
                return;
            }
            const Vec3 pos = w.matrix.translation() + parallaxOf(e);
            const float radius = std::max(l.radius, 1e-3f);
            if (!frustum.intersects(math::Aabb::fromCenterExtents(pos, Vec3{radius}))) return;
            lights.posRadius[n] = Vec4(pos.x, pos.y, std::max(l.height, 0.0f), radius);
            lights.colorFalloff[n] = Vec4(srgbToLinear(l.color) * l.intensity, std::max(l.falloff, 0.01f));
            lights.shadow[n] = Vec4(-1.0f, 0.0f, 0.0f, 0.0f);
            if (l.castShadows && shadowRows < kShadowRows2D) {
                if (!occludersCollected) collectOccluders();
                // Polar shadow map: for each of 512 directions, the nearest occluder distance / radius.
                const Vec2 o{pos.x, pos.y};
                std::fill(m_shadow2DBins.begin(), m_shadow2DBins.end(), 1.0f);
                constexpr float kTwoPi = 6.2831853f;
                for (const Edge& ed : edges) {
                    // Skip edges out of reach.
                    const Vec2 lo{std::min(ed.a.x, ed.b.x), std::min(ed.a.y, ed.b.y)}, hi{std::max(ed.a.x, ed.b.x), std::max(ed.a.y, ed.b.y)};
                    if (lo.x > o.x + radius || hi.x < o.x - radius || lo.y > o.y + radius || hi.y < o.y - radius) continue;
                    ++m_stats.occluderEdges2D;
                    // Directions covered by the edge (shorter arc), then a ray per bin.
                    float a0 = std::atan2(ed.a.y - o.y, ed.a.x - o.x), a1 = std::atan2(ed.b.y - o.y, ed.b.x - o.x);
                    float span = a1 - a0;
                    if (span > 3.14159265f) span -= kTwoPi;
                    if (span < -3.14159265f) span += kTwoPi;
                    if (span < 0.0f) {
                        a0 += span;
                        span = -span;
                    }
                    const float binWidth = kTwoPi / static_cast<float>(kShadowBins2D);
                    const i32 first = static_cast<i32>(std::floor((a0 + 3.14159265f) / binWidth));
                    const i32 last = static_cast<i32>(std::ceil((a0 + span + 3.14159265f) / binWidth));
                    for (i32 b = first; b <= last; ++b) {
                        const i32 bin = ((b % static_cast<i32>(kShadowBins2D)) + static_cast<i32>(kShadowBins2D)) % static_cast<i32>(kShadowBins2D);
                        const float ang = (static_cast<float>(bin) + 0.5f) * binWidth - 3.14159265f;
                        const float t = raySegment(o, {std::cos(ang), std::sin(ang)}, ed.a, ed.b);
                        if (t >= 0.0f) m_shadow2DBins[static_cast<usize>(bin)] = std::min(m_shadow2DBins[static_cast<usize>(bin)], t / radius);
                    }
                }
                // Row `shadowRows`, top row first: 16-bit distance in R (high) and G (low).
                u8* row = &m_shadow2DPixels[static_cast<usize>(shadowRows) * kShadowBins2D * 4];
                for (u32 b = 0; b < kShadowBins2D; ++b) {
                    const u32 q = static_cast<u32>(std::clamp(m_shadow2DBins[b], 0.0f, 1.0f) * 65535.0f + 0.5f);
                    row[b * 4 + 0] = static_cast<u8>(q >> 8);
                    row[b * 4 + 1] = static_cast<u8>(q & 255);
                    row[b * 4 + 2] = 0;
                    row[b * 4 + 3] = 255;
                }
                lights.shadow[n] = Vec4(static_cast<float>(shadowRows), std::clamp(l.shadowSoftness, 0.0f, 4.0f) * 4.0f,
                                        std::clamp(l.shadowStrength, 0.0f, 1.0f), std::max(l.shadowPenetration, 0.0f) / radius);
                ++shadowRows;
            }
            ++n;
        });
        if (shadowRows > 0) {
            if (!m_shadow2D) {
                rhi::TextureDesc td;
                td.width = kShadowBins2D;
                td.height = kShadowRows2D;
                td.format = rhi::TextureFormat::RGBA8;
                td.generateMips = false;
                td.sampler = {rhi::Filter::Nearest, rhi::Filter::Nearest, false, rhi::Wrap::Repeat, rhi::Wrap::Clamp};
                td.data = m_shadow2DPixels.data();
                td.debugName = "agr.shadow2d";
                if (auto t = m_device.createTexture(td)) m_shadow2D = t.value();
            } else {
                (void)m_device.updateTexture(m_shadow2D, m_shadow2DPixels.data());
            }
        }
        lights.count[0] = static_cast<i32>(n);
        m_stats.lights2D = n;
        m_stats.shadowedLights2D = shadowRows;
        m_lightsOffset = pushUniforms(&lights, sizeof lights);
    }

    // ------------------------------------------------------------ sprites
    m_spriteItems.clear();
    std::string lastUri;
    bool lastPixelArt = false, lastHandleValid = false;
    rhi::TextureHandle lastHandle;
    auto textureFor = [&](std::string_view uri, bool pixelArt) {
        // Neighbouring sprites usually share a texture: a string compare beats a hash lookup.
        if (uri != lastUri || pixelArt != lastPixelArt || !lastHandleValid) {
            lastUri = std::string(uri);
            lastPixelArt = pixelArt;
            lastHandle = resolveTexture(uri, pixelArt);
            lastHandleValid = true;
        }
        return lastHandle;
    };
    reg.view<SpriteRendererComponent, WorldTransformComponent>().each(
        [&](ecs::Entity e, SpriteRendererComponent& s, WorldTransformComponent& w) {
            if (!s.visible || !w.activeInHierarchy || hiddenInEditor(e)) return;
            SpriteItem item;
            const Vec3 off = parallaxOf(e);
            // With a sprite sheet the frame owns the pivot (frames of one clip may differ).
            const SpriteSheet* sheet = (m_sheets && !s.sheet.empty()) ? m_sheets->get(s.sheet) : nullptr;
            const bool sheetFrame = sheet && sheet->validFrame(s.frame);
            const math::Vec2 pivot = sheetFrame ? sheet->frames[static_cast<u32>(s.frame)].pivot : s.pivot;
            const float x0 = -pivot.x * s.size.x, x1 = (1.0f - pivot.x) * s.size.x;
            const float y0 = -pivot.y * s.size.y, y1 = (1.0f - pivot.y) * s.size.y;
            item.corners[0] = w.matrix.transformPoint({x0, y0, 0}) + off;
            item.corners[1] = w.matrix.transformPoint({x1, y0, 0}) + off;
            item.corners[2] = w.matrix.transformPoint({x1, y1, 0}) + off;
            item.corners[3] = w.matrix.transformPoint({x0, y1, 0}) + off;
            math::Aabb box;
            for (const Vec3& c : item.corners) box.expand(c);
            if (!frustum.intersects(box)) {
                ++m_stats.spritesCulled;
                return;
            }
            item.layer = s.sortingLayer;
            item.order = s.orderInLayer;
            item.depth = (view.transformPoint(box.center())).z;  // more negative = farther
            std::string_view texture = s.texture;
            item.uv = s.uvRect;
            bool pixelArt = s.pixelArt;
            if (sheet) {
                texture = sheet->texture;
                pixelArt |= sheet->pixelArt;
                if (sheetFrame) item.uv = sheet->uv(static_cast<u32>(s.frame));
            }
            if (s.flipX) std::swap(item.uv.x, item.uv.z);
            if (s.flipY) std::swap(item.uv.y, item.uv.w);
            item.color = packColor(s.color);
            item.pipeline = s.lit ? 1u : 0u;
            item.texture = textureFor(texture, pixelArt);
            if (s.lit && !s.normalMap.empty()) {
                item.normalMap = resolveDataTexture(s.normalMap, pixelArt);
                item.pipeline = 3;
                ++m_stats.normalMappedSprites;
            }
            m_spriteItems.push_back(item);
        });

    // ------------------------------------------------------------ 2D skins
    // Bone-deformed grids: one quad per cell, batched like sprites (same texture, same depth).
    reg.view<Skin2DComponent, WorldTransformComponent>().each([&](ecs::Entity e, Skin2DComponent& sk, WorldTransformComponent& w) {
        if (!sk.visible || !w.activeInHierarchy || hiddenInEditor(e)) return;
        if (!skin2DPoints(scene, e, m_skinPoints)) return;
        const Vec3 off = parallaxOf(e);
        math::Aabb box;
        for (Vec3& p : m_skinPoints) {
            p = p + off;
            box.expand(p);
        }
        if (!frustum.intersects(box)) {
            ++m_stats.spritesCulled;
            return;
        }
        ++m_stats.skins2D;
        const i32 cols = std::clamp(sk.columns, 1, 64), rows = std::clamp(sk.rows, 1, 64);
        const rhi::TextureHandle tex = textureFor(sk.texture, sk.pixelArt);
        const rhi::TextureHandle normalMap = sk.lit && !sk.normalMap.empty() ? resolveDataTexture(sk.normalMap, sk.pixelArt) : rhi::TextureHandle{};
        const u32 color = packColor(sk.color);
        const float depth = view.transformPoint(box.center()).z;
        auto u = [&](i32 i) { return sk.uvRect.x + (sk.uvRect.z - sk.uvRect.x) * static_cast<float>(i) / static_cast<float>(cols); };
        auto v = [&](i32 j) { return sk.uvRect.w + (sk.uvRect.y - sk.uvRect.w) * static_cast<float>(j) / static_cast<float>(rows); };  // v = 0: top
        auto at = [&](i32 i, i32 j) { return m_skinPoints[static_cast<usize>(j * (cols + 1) + i)]; };
        for (i32 j = 0; j < rows; ++j)
            for (i32 i = 0; i < cols; ++i) {
                SpriteItem item;
                item.corners[0] = at(i, j);
                item.corners[1] = at(i + 1, j);
                item.corners[2] = at(i + 1, j + 1);
                item.corners[3] = at(i, j + 1);
                item.uv = {u(i), v(j + 1), u(i + 1), v(j)};
                item.layer = sk.sortingLayer;
                item.order = sk.orderInLayer;
                item.depth = depth;
                item.color = color;
                item.pipeline = normalMap ? 3u : sk.lit ? 1u : 0u;
                item.texture = tex;
                item.normalMap = normalMap;
                m_spriteItems.push_back(item);
            }
    });

    // ------------------------------------------------------------ tilemaps
    // Only cells inside the view are generated: cost follows the screen, not the map.
    const Mat4 invViewProj = math::inverse(viewProj);
    const float ndcNear = m_glClipSpace ? -1.0f : 0.0f;
    reg.view<TilemapComponent, WorldTransformComponent>().each([&](ecs::Entity e, TilemapComponent& tm, WorldTransformComponent& w) {
        if (!w.activeInHierarchy || hiddenInEditor(e) || tm.tiles.empty() || tm.cellSize.x <= 0.0f || tm.cellSize.y <= 0.0f) return;
        const SpriteSheet* sheet = m_sheets ? m_sheets->get(tm.tileset) : nullptr;
        if (!sheet || sheet->frames.empty()) return;
        const Mat4 local = Mat4::translate(parallaxOf(e)) * w.matrix;
        const Mat4 toLocal = math::inverse(local);
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        for (int i = 0; i < 8; ++i) {  // xy bounds of the view frustum in tilemap space (conservative)
            const Vec3 ndc{(i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : ndcNear};
            const Vec3 p = toLocal.transformPoint(invViewProj.transformPoint(ndc));
            minX = std::min(minX, p.x), maxX = std::max(maxX, p.x), minY = std::min(minY, p.y), maxY = std::max(maxY, p.y);
        }
        const i32 cx0 = std::max(0, static_cast<i32>(std::floor(minX / tm.cellSize.x)));
        const i32 cy0 = std::max(0, static_cast<i32>(std::floor(minY / tm.cellSize.y)));
        const i32 cx1 = std::min(static_cast<i32>(tm.width) - 1, static_cast<i32>(std::floor(maxX / tm.cellSize.x)));
        const i32 cy1 = std::min(static_cast<i32>(tm.height) - 1, static_cast<i32>(std::floor(maxY / tm.cellSize.y)));
        if (cx0 > cx1 || cy0 > cy1) return;
        m_tileUvs.resize(sheet->frames.size());
        for (u32 f = 0; f < sheet->frames.size(); ++f) m_tileUvs[f] = sheet->uv(f);
        const Vec3 o = local.translation();
        const Vec3 ax = local.transformVector({tm.cellSize.x, 0, 0}), ay = local.transformVector({0, tm.cellSize.y, 0});
        const float depth = view.transformPoint(o + ax * (static_cast<float>(tm.width) * 0.5f) + ay * (static_cast<float>(tm.height) * 0.5f)).z;
        const rhi::TextureHandle tex = textureFor(sheet->texture, sheet->pixelArt);
        const rhi::TextureHandle normalMap = tm.lit && !tm.normalMap.empty() ? resolveDataTexture(tm.normalMap, sheet->pixelArt) : rhi::TextureHandle{};
        const u32 color = packColor(tm.color);
        const TileRules rules = TileRules::parse(tm, sheet->frames.size());
        for (i32 y = cy0; y <= cy1; ++y) {
            for (i32 x = cx0; x <= cx1; ++x) {
                const i32 id = rules.empty() ? tm.tileAt(static_cast<u32>(x), static_cast<u32>(y))
                                             : rules.resolve(tm, static_cast<u32>(x), static_cast<u32>(y), m_time);
                if (id < 0 || static_cast<usize>(id) >= m_tileUvs.size()) continue;
                SpriteItem item;
                const Vec3 c = o + ax * static_cast<float>(x) + ay * static_cast<float>(y);
                item.corners[0] = c;
                item.corners[1] = c + ax;
                item.corners[2] = c + ax + ay;
                item.corners[3] = c + ay;
                item.layer = tm.sortingLayer;
                item.order = tm.orderInLayer;
                item.depth = depth;  // one depth for the whole map: tiles batch together
                item.uv = m_tileUvs[static_cast<usize>(id)];
                item.color = color;
                item.pipeline = normalMap ? 3u : tm.lit ? 1u : 0u;
                item.texture = tex;
                item.normalMap = normalMap;
                m_spriteItems.push_back(item);
                ++m_stats.tilesVisible;
            }
        }
    });

    // Over-lifetime curves and flipbook frames shared by 2D and 3D particles.
    auto curvesFor = [&](const std::string& uri) { return m_particleCurves && !uri.empty() ? m_particleCurves->get(uri) : nullptr; };
    auto particleLook = [&](const ParticleCurvesLut* lut, i32 cols, i32 rows, float cycles, float t, float startSize, float endSize, Vec4 c0, Vec4 c1,
                            float& size, Vec4& color, Vec4& uv) {
        size = lut && lut->hasSize ? startSize * lut->sizeAt(t) : math::lerp(startSize, endSize, t);
        color = lut && lut->hasColor ? Vec4(lut->colorAt(t)) : Vec4{math::lerp(c0.x, c1.x, t), math::lerp(c0.y, c1.y, t), math::lerp(c0.z, c1.z, t), math::lerp(c0.w, c1.w, t)};
        uv = {0, 0, 1, 1};
        if (cols > 1 || rows > 1) {
            const i32 c = std::max(cols, 1), r = std::max(rows, 1), frames = c * r;
            const i32 frame = std::min(static_cast<i32>(t * std::max(cycles, 0.0f) * static_cast<float>(frames)) % frames, frames - 1);
            const float fw = 1.0f / static_cast<float>(c), fh = 1.0f / static_cast<float>(r);
            const float u0 = static_cast<float>(frame % c) * fw, v0 = static_cast<float>(frame / c) * fh;  // row 0 = top of the image
            uv = {u0, v0, u0 + fw, v0 + fh};
        }
    };
    // ------------------------------------------------------------ particles
    reg.view<ParticleEmitter2DComponent, ParticleState2DComponent, WorldTransformComponent>().each(
        [&](ecs::Entity e, ParticleEmitter2DComponent& em, ParticleState2DComponent& st, WorldTransformComponent& w) {
            if (!w.activeInHierarchy || hiddenInEditor(e) || st.particles.empty()) return;
            const Vec3 off = parallaxOf(e);
            const rhi::TextureHandle tex = em.texture.empty() ? m_softParticle : textureFor(em.texture, false);
            const float depth = view.transformPoint(w.matrix.translation() + off).z;
            const Vec4 c0 = em.startColor, c1 = em.endColor;
            const ParticleCurvesLut* lut = curvesFor(em.curves);
            for (const Particle2D& p : st.particles) {
                const float t = std::clamp(p.age / std::max(p.lifetime, 1e-4f), 0.0f, 1.0f);
                float size;
                Vec4 color, uv;
                particleLook(lut, em.flipbookColumns, em.flipbookRows, em.flipbookCycles, t, em.startSize, em.endSize, c0, c1, size, color, uv);
                const float half = size * 0.5f;
                if (half <= 0.0f) continue;
                const Vec3 center = (em.worldSpace ? Vec3{p.position.x, p.position.y, p.z} : w.matrix.transformPoint({p.position.x, p.position.y, 0.0f})) + off;
                Vec3 dx, dy;
                Vec3 vel = em.worldSpace ? Vec3{p.velocity.x, p.velocity.y, 0.0f} : w.matrix.transformVector({p.velocity.x, p.velocity.y, 0.0f});
                vel.z = 0.0f;
                const float speed = math::length(vel);
                if (em.velocityStretch > 0.0f && speed > 1e-4f) {  // along the motion (sparks, rain)
                    const Vec3 dir = vel * (1.0f / speed);
                    dx = dir * (half + speed * em.velocityStretch * 0.5f);
                    dy = Vec3{-dir.y, dir.x, 0.0f} * half;
                } else {
                    const float cs = std::cos(p.rotation) * half, sn = std::sin(p.rotation) * half;
                    dx = {cs, sn, 0.0f};
                    dy = {-sn, cs, 0.0f};
                }
                SpriteItem item;
                item.corners[0] = center - dx - dy;
                item.corners[1] = center + dx - dy;
                item.corners[2] = center + dx + dy;
                item.corners[3] = center - dx + dy;
                item.layer = em.sortingLayer;
                item.order = em.orderInLayer;
                item.depth = depth;
                item.uv = uv;
                item.color = packColor(color);
                item.pipeline = em.additive ? 2u : 0u;
                item.texture = tex;
                m_spriteItems.push_back(item);
                ++m_stats.particlesVisible;
            }
        });
    // 3D particles: billboards facing the camera, sorted per particle with the sprites.
    {
        const Vec3 right{view.c[0].x, view.c[1].x, view.c[2].x}, up{view.c[0].y, view.c[1].y, view.c[2].y};
        // Lit particles: ambient + half-Lambert sun (normal toward the camera) + point / spot lights
        // in range, evaluated per particle on the CPU (gathered once, only when needed).
        struct ParticleLight {
            Vec3 pos, color, dir;
            float range, cosOuter, cosInner;  // cosOuter < -1.5: point light
        };
        std::vector<ParticleLight> plights;
        bool plightsGathered = false;
        const Vec3 toCamera = math::normalize(Vec3{view.c[0].z, view.c[1].z, view.c[2].z});
        auto lightAt = [&](Vec3 p) {
            Vec3 l{frame.ambient.x, frame.ambient.y, frame.ambient.z};
            const float sunWrap = 0.5f + 0.5f * math::dot(toCamera, Vec3{-frame.lightDir.x, -frame.lightDir.y, -frame.lightDir.z});
            l = l + Vec3{frame.lightColor.x, frame.lightColor.y, frame.lightColor.z} * sunWrap;
            for (const ParticleLight& pl : plights) {
                const Vec3 d = p - pl.pos;
                const float dist = math::length(d);
                if (dist >= pl.range) continue;
                float att = 1.0f - dist / pl.range;
                att *= att;
                if (pl.cosOuter > -1.5f) {
                    const float c = dist > 1e-5f ? math::dot(d * (1.0f / dist), pl.dir) : 1.0f;
                    att *= std::clamp((c - pl.cosOuter) / std::max(pl.cosInner - pl.cosOuter, 1e-4f), 0.0f, 1.0f);
                }
                l = l + pl.color * att;
            }
            return l;
        };
        reg.view<ParticleEmitter3DComponent, ParticleState3DComponent, WorldTransformComponent>().each(
            [&](ecs::Entity e, ParticleEmitter3DComponent& em, ParticleState3DComponent& st, WorldTransformComponent& w) {
                if (!w.activeInHierarchy || hiddenInEditor(e) || st.particles.empty() || em.meshParticles) return;  // meshes: 3D pass
                if (em.lit && !plightsGathered) {
                    plightsGathered = true;
                    reg.view<PointLightComponent, WorldTransformComponent>().each([&](ecs::Entity le, PointLightComponent& l, WorldTransformComponent& lw) {
                        if (lw.activeInHierarchy && l.intensity > 0.0f && l.range > 0.0f && !hiddenInEditor(le))
                            plights.push_back({lw.matrix.translation(), srgbToLinear(l.color) * l.intensity, Vec3{0.0f}, l.range, -2.0f, 1.0f});
                    });
                    reg.view<SpotLightComponent, WorldTransformComponent>().each([&](ecs::Entity le, SpotLightComponent& l, WorldTransformComponent& lw) {
                        if (lw.activeInHierarchy && l.intensity > 0.0f && l.range > 0.0f && !hiddenInEditor(le))
                            plights.push_back({lw.matrix.translation(), srgbToLinear(l.color) * l.intensity,
                                               math::normalize(lw.matrix.transformVector({0, 0, -1})), l.range, std::cos(l.outerAngle),
                                               std::cos(std::min(l.innerAngle, l.outerAngle))});
                    });
                }
                const rhi::TextureHandle tex = em.texture.empty() ? m_softParticle : textureFor(em.texture, false);
                const Vec4 c0 = em.startColor, c1 = em.endColor;
                const ParticleCurvesLut* lut = curvesFor(em.curves);
                for (const Particle3D& p : st.particles) {
                    const float t = std::clamp(p.age / std::max(p.lifetime, 1e-4f), 0.0f, 1.0f);
                    float size;
                    Vec4 color, uv;
                    particleLook(lut, em.flipbookColumns, em.flipbookRows, em.flipbookCycles, t, em.startSize, em.endSize, c0, c1, size, color, uv);
                    const float half = size * 0.5f;
                    if (half <= 0.0f) continue;
                    const Vec3 center = em.worldSpace ? p.position : w.matrix.transformPoint(p.position);
                    Vec3 dx, dy;
                    const Vec3 vel = em.worldSpace ? p.velocity : w.matrix.transformVector(p.velocity);
                    const float sx = math::dot(vel, right), sy = math::dot(vel, up), screenSpeed = std::sqrt(sx * sx + sy * sy);
                    if (em.velocityStretch > 0.0f && screenSpeed > 1e-4f) {  // stretched along the motion as seen on screen
                        const float ux = sx / screenSpeed, uy = sy / screenSpeed;
                        dx = (right * ux + up * uy) * (half + screenSpeed * em.velocityStretch * 0.5f);
                        dy = (up * ux - right * uy) * half;
                    } else {
                        const float cs = std::cos(p.rotation) * half, sn = std::sin(p.rotation) * half;
                        dx = right * cs + up * sn;
                        dy = up * cs - right * sn;
                    }
                    SpriteItem item;
                    item.corners[0] = center - dx - dy;
                    item.corners[1] = center + dx - dy;
                    item.corners[2] = center + dx + dy;
                    item.corners[3] = center - dx + dy;
                    item.layer = 0;
                    item.order = 0;
                    item.depth = view.transformPoint(center).z;
                    item.uv = uv;
                    if (em.lit) {
                        const Vec3 l = lightAt(center);
                        color = Vec4{color.x * l.x, color.y * l.y, color.z * l.z, color.w};
                    }
                    item.color = packColor(color);
                    item.pipeline = em.additive ? 2u : 0u;
                    item.texture = tex;
                    m_spriteItems.push_back(item);
                    ++m_stats.particlesVisible;
                }
            });
    }
    // Trails: one quad per segment between recorded points, facing the camera (in 2D: the XY plane).
    {
        const Vec3 camForward = math::normalize(Vec3{-view.c[0].z, -view.c[1].z, -view.c[2].z});
        reg.view<TrailComponent, TrailStateComponent, WorldTransformComponent>().each(
            [&](ecs::Entity e, TrailComponent& t, TrailStateComponent& st, WorldTransformComponent& w) {
                if (!w.activeInHierarchy || hiddenInEditor(e) || st.points.size() < 2 || t.width <= 0.0f) return;
                const ParticleCurvesLut* lut = curvesFor(t.curves);
                const rhi::TextureHandle tex = t.texture.empty() ? m_assets.whiteTexture() : textureFor(t.texture, false);
                const usize n = st.points.size();
                const float life = std::max(t.lifetime, 1e-3f);
                std::vector<Vec3>& side = m_trailScratch;
                side.assign(n, Vec3{0.0f});
                for (usize i = 0; i < n; ++i) {  // half width offset at each point, perpendicular to the trail
                    const Vec3 a = st.points[i > 0 ? i - 1 : 0].position, b = st.points[i + 1 < n ? i + 1 : n - 1].position;
                    Vec3 s = math::cross(b - a, camForward);
                    const float len = math::length(s);
                    if (len < 1e-6f) s = i > 0 ? side[i - 1] : Vec3{0.0f};
                    else s = s * (1.0f / len);
                    const float age = std::clamp((st.time - st.points[i].time) / life, 0.0f, 1.0f);
                    const float width = t.width * (lut && lut->hasSize ? lut->sizeAt(age) : 1.0f - age);
                    side[i] = len < 1e-6f ? s : s * (width * 0.5f);
                }
                math::Aabb box;
                for (const TrailPoint& p : st.points) box.expand(p.position);
                box.min = box.min - Vec3{t.width}, box.max = box.max + Vec3{t.width};
                if (!frustum.intersects(box)) return;
                const float depth = view.transformPoint(box.center()).z;
                for (usize i = 0; i + 1 < n; ++i) {
                    const float ageA = std::clamp((st.time - st.points[i].time) / life, 0.0f, 1.0f),
                                ageB = std::clamp((st.time - st.points[i + 1].time) / life, 0.0f, 1.0f);
                    const float mid = (ageA + ageB) * 0.5f;
                    Vec4 color = lut && lut->hasColor ? Vec4(lut->colorAt(mid)) : Vec4{1, 1, 1, 1.0f - mid};
                    color = {color.x * t.color.x, color.y * t.color.y, color.z * t.color.z, color.w * t.color.w};
                    const Vec3 a = st.points[i].position, b = st.points[i + 1].position;
                    SpriteItem item;
                    item.corners[0] = a - side[i];
                    item.corners[1] = b - side[i + 1];
                    item.corners[2] = b + side[i + 1];
                    item.corners[3] = a + side[i];
                    // u runs from the tail (0) to the head (1).
                    item.uv = {static_cast<float>(i) / static_cast<float>(n - 1), 0.0f, static_cast<float>(i + 1) / static_cast<float>(n - 1), 1.0f};
                    item.layer = t.sortingLayer;
                    item.order = t.orderInLayer;
                    item.depth = depth;
                    item.color = packColor(color);
                    item.pipeline = t.additive ? 2u : 0u;
                    item.texture = tex;
                    m_spriteItems.push_back(item);
                }
                ++m_stats.trails;
            });
    }
    m_stats.spritesVisible = static_cast<u32>(m_spriteItems.size());
    // Painter's order: layer, order, then back to front; texture only breaks ties.
    // Small keys are sorted instead of the 80-byte items.
    m_spriteKeys.clear();
    m_spriteKeys.reserve(m_spriteItems.size());
    for (u32 i = 0; i < m_spriteItems.size(); ++i) {
        const SpriteItem& it = m_spriteItems[i];
        m_spriteKeys.push_back({it.layer, it.order, it.depth, it.pipeline, it.texture.index, i});
    }
    std::sort(m_spriteKeys.begin(), m_spriteKeys.end(), [](const SortKey& a, const SortKey& b) {
        if (a.layer != b.layer) return a.layer < b.layer;
        if (a.order != b.order) return a.order < b.order;
        if (a.depth != b.depth) return a.depth < b.depth;
        if (a.pipeline != b.pipeline) return a.pipeline < b.pipeline;
        if (a.texture != b.texture) return a.texture < b.texture;
        return a.index < b.index;  // deterministic
    });

    // Overlay (game UI): built by its source, drawn in pixels with its own projection.
    m_overlay.clear();
    if (m_overlaySource && rv.overlay) {
        m_overlaySource->buildOverlay(scene, width, height, m_overlay);
        if (!m_overlay.empty()) {
            FrameUniforms ui = frame;
            ui.viewProj = Mat4::orthographic(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, -1.0f, 1.0f, range);
            ui.fogParams.w = 0.0f;  // the UI is drawn after post-processing, in display colors
            m_overlayFrameOffset = pushUniforms(&ui, sizeof ui);
            m_stats.overlayQuads = static_cast<u32>(m_overlay.indices.size() / 6);
        }
    }

    // Post passes: targets and their uniforms (before the uniform upload).
    m_postPasses.clear();
    if (postActive) {
        const bool bloom = post.bloom && m_profile.bloom && post.bloomIntensity > 0.0f;
        const bool fxaa = post.fxaa && m_profile.fxaa;
        const bool ao = post.ambientOcclusion && m_profile.ssao && post.aoIntensity > 0.0f;
        const bool atmos = m_profile.atmosphere && ((post.heightFog && post.fogDensity > 0.0f) || (post.lightShafts && post.shaftIntensity > 0.0f));
        const bool dof = m_profile.depthOfField && post.depthOfField && post.dofBlur > 0.0f && (post.dofNear || post.dofFar);
        const bool autoExposure = post.autoExposure && postHdr;
        const bool ssr = m_profile.ssr && post.screenSpaceReflections && post.ssrIntensity > 0.0f;
        const bool readDepth = ao || atmos || dof || ssr || taa;  // a single-sampled, sampleable scene depth
        m_postReadDepth = readDepth;
        if (ensurePostTargets(renderW, renderH, readDepth ? 1u : samples, width, height, postHdr, bloom, fxaa, ao, atmos)) {
            struct PostUniforms {
                Vec4 texel, bloom, grade, filter, ao;
                Mat4 proj, invProj;
                Vec4 fog, fogColor, shafts, sun, camera, sunDir;
                Mat4 invViewProj;
                Vec4 dof, dof2, exposure, adapt;
                Mat4 prevViewProj;
                Vec4 taa;
            };
            // Extra targets of this frame's optional passes (released with the others).
            auto extra = [&](rhi::RenderTargetHandle& t, u32 tw, u32 th, rhi::TextureFormat f, const char* name) {
                if (t) return;
                auto r = m_device.createRenderTarget({.width = tw, .height = th, .colorFormat = f, .depth = false, .debugName = name});
                if (r) t = r.value();
                else AGR_LOG_ERROR("Render", "post-processing target '{}': {}", name, r.error().message);
            };
            if (taa) {
                extra(m_taaTargets[0], renderW, renderH, postHdr ? rhi::TextureFormat::RGBA16F : rhi::TextureFormat::RGBA8, "agr.post.taa.a");
                extra(m_taaTargets[1], renderW, renderH, postHdr ? rhi::TextureFormat::RGBA16F : rhi::TextureFormat::RGBA8, "agr.post.taa.b");
            }
            if (ssr) extra(m_ssrTarget, renderW, renderH, postHdr ? rhi::TextureFormat::RGBA16F : rhi::TextureFormat::RGBA8, "agr.post.ssr");
            if (dof) extra(m_dofTarget, renderW, renderH, postHdr ? rhi::TextureFormat::RGBA16F : rhi::TextureFormat::RGBA8, "agr.post.dof");
            if (autoExposure) {
                extra(m_lumTargets[0], 1, 1, rhi::TextureFormat::RGBA16F, "agr.post.luminance.a");
                extra(m_lumTargets[1], 1, 1, rhi::TextureFormat::RGBA16F, "agr.post.luminance.b");
            }
            const bool lumReady = autoExposure && m_lumTargets[0] && m_lumTargets[1];
            float adaptBlend = 1.0f;
            if (lumReady) {
                const f64 now = std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
                const f64 dt = m_lastPostClock < 0.0 ? 0.0 : std::clamp(now - m_lastPostClock, 0.0, 0.5);
                m_lastPostClock = now;
                adaptBlend = 1.0f - std::exp(-static_cast<float>(dt) * std::max(post.exposureSpeed, 0.0f));
                m_lumIndex ^= 1u;  // write the other target; the previous one is the history
            } else {
                m_lumValid = false;
                m_lastPostClock = -1.0;
            }
            // Atmosphere: the sun (first directional light) on screen, the camera, the fog color.
            const Vec3 toSun = math::normalize(Vec3{-frame.lightDir.x, -frame.lightDir.y, -frame.lightDir.z});
            const Vec3 camPos{frame.cameraPos.x, frame.cameraPos.y, frame.cameraPos.z};
            const Vec4 sunClip = viewProj * Vec4(camPos + toSun * 1000.0f, 1.0f);
            const bool sunInFront = sunClip.w > 0.0f && frame.lightColor.x + frame.lightColor.y + frame.lightColor.z > 0.0f;
            const Vec2 sunUv = sunInFront ? Vec2{sunClip.x / sunClip.w * 0.5f + 0.5f, (m_glClipSpace ? 1.0f : -1.0f) * sunClip.y / sunClip.w * 0.5f + 0.5f} : Vec2{0.5f, 0.5f};
            const Vec3 fogLinear = postHdr ? detail::srgbToLinear(post.fogColor) : Vec3{post.fogColor.x, post.fogColor.y, post.fogColor.z};
            static_assert(sizeof(PostUniforms) == kPostUniformBytes);
            const Mat4 proj = rv.projectionMatrix(aspect, m_glClipSpace);
            auto add = [&](PostPass::Kind kind, rhi::RenderTargetHandle src, rhi::RenderTargetHandle dst, Vec2 srcSize, Vec2 dstSize) {
                PostUniforms u;
                u.texel = Vec4(1.0f / srcSize.x, 1.0f / srcSize.y, 1.0f / dstSize.x, 1.0f / dstSize.y);
                u.bloom = Vec4(std::max(0.0f, post.bloomThreshold), 0.5f, bloom ? post.bloomIntensity : 0.0f, std::clamp(post.bloomRadius, 0.25f, 4.0f));
                u.grade = Vec4(post.contrast, post.saturation, std::clamp(post.vignette, 0.0f, 1.0f), post.filmicToneMapping ? 1.0f : 0.0f);
                u.filter = Vec4(post.colorFilter.x, post.colorFilter.y, post.colorFilter.z, postHdr ? 1.0f : 0.0f);
                u.ao = Vec4(std::clamp(post.aoRadius, 0.05f, 5.0f), std::clamp(post.aoIntensity, 0.0f, 2.0f), ao ? 1.0f : 0.0f, m_glClipSpace ? 1.0f : 0.0f);
                u.proj = proj;
                u.invProj = math::inverse(proj);
                u.fog = Vec4(post.fogDensity, post.fogHeight, std::max(post.fogFalloff, 0.0f), std::max(post.fogSunScatter, 0.0f));
                u.fogColor = Vec4(fogLinear, post.heightFog ? 1.0f : 0.0f);
                u.shafts = Vec4(sunUv.x, sunUv.y, std::max(post.shaftIntensity, 0.0f), std::clamp(post.shaftDecay, 0.5f, 0.999f));
                u.sun = Vec4(frame.lightColor.x, frame.lightColor.y, frame.lightColor.z, sunInFront ? 1.0f : 0.0f);
                if (!postHdr) u.sun = Vec4(std::min(u.sun.x, 1.0f), std::min(u.sun.y, 1.0f), std::min(u.sun.z, 1.0f), u.sun.w);
                u.camera = Vec4(camPos, std::max(rv.farPlane, 1.0f));
                u.sunDir = Vec4(toSun, post.lightShafts ? 1.0f : 0.0f);
                u.invViewProj = math::inverse(viewProj);
                u.dof = Vec4(std::max(post.focusDistance, 0.0f), std::max(post.focusRange, 0.01f),
                             std::clamp(post.dofBlur, 0.0f, 0.05f) * static_cast<float>(renderH), post.dofNear ? 1.0f : 0.0f);
                u.dof2 = Vec4(post.dofFar ? 1.0f : 0.0f, std::clamp(post.ssrIntensity, 0.0f, 2.0f), std::max(post.ssrMaxDistance, 0.1f),
                              std::max(post.ssrThickness, 0.01f));
                u.exposure = Vec4(lumReady ? 1.0f : 0.0f, std::max(post.exposureTarget, 0.001f), std::max(post.exposureMin, 0.001f),
                                  std::max(post.exposureMax, std::max(post.exposureMin, 0.001f)));
                u.adapt = Vec4(adaptBlend, m_lumValid ? 1.0f : 0.0f, 0.0f, 0.0f);
                u.prevViewProj = m_prevViewProj;
                u.taa = Vec4(std::clamp(post.taaBlend, 0.02f, 1.0f), m_taaValid ? 1.0f : 0.0f, 0.0f, 0.0f);
                m_postPasses.push_back({kind, src, dst, pushUniforms(&u, sizeof u)});
            };
            const Vec2 renderSize{static_cast<float>(renderW), static_cast<float>(renderH)}, outSize{static_cast<float>(width), static_cast<float>(height)};
            rhi::RenderTargetHandle sceneColor = m_postResolve ? m_postResolve : m_postScene;
            const bool taaReady = taa && m_taaTargets[0] && m_taaTargets[1];
            if (taaReady) {  // first: every later pass sees the accumulated, anti-aliased image
                m_taaIndex ^= 1u;  // write one target, read the other as the history
                add(PostPass::Taa, sceneColor, m_taaTargets[m_taaIndex], renderSize, renderSize);
                sceneColor = m_taaTargets[m_taaIndex];
                m_stats.temporalAA = true;
            }
            if (ssr && m_ssrTarget) {  // reflections are fogged and blurred with the rest
                add(PostPass::Ssr, sceneColor, m_ssrTarget, renderSize, renderSize);
                sceneColor = m_ssrTarget;
                m_stats.screenSpaceReflections = true;
            }
            if (atmos && m_atmosTarget) {  // then: bloom, AO and the composite read the fogged scene
                add(PostPass::Atmosphere, sceneColor, m_atmosTarget, renderSize, renderSize);
                sceneColor = m_atmosTarget;
                m_stats.atmosphere = true;
            }
            if (dof && m_dofTarget) {
                add(PostPass::Dof, sceneColor, m_dofTarget, renderSize, renderSize);
                sceneColor = m_dofTarget;
                m_stats.depthOfField = true;
            }
            if (lumReady) {
                add(PostPass::Luminance, sceneColor, m_lumTargets[m_lumIndex], renderSize, Vec2{1.0f, 1.0f});
                m_lumValid = true;
                m_stats.autoExposure = true;
            }
            auto levelSize = [&](usize i) { return Vec2{static_cast<float>(m_bloomSizes[i].first), static_cast<float>(m_bloomSizes[i].second)}; };
            if (bloom && !m_bloomLevels.empty()) {
                add(PostPass::Prefilter, sceneColor, m_bloomLevels[0], renderSize, levelSize(0));
                for (usize i = 1; i < m_bloomLevels.size(); ++i) add(PostPass::Down, m_bloomLevels[i - 1], m_bloomLevels[i], levelSize(i - 1), levelSize(i));
                for (usize i = m_bloomLevels.size() - 1; i >= 1; --i) add(PostPass::Up, m_bloomLevels[i], m_bloomLevels[i - 1], levelSize(i), levelSize(i - 1));
            }
            if (ao) {
                const Vec2 half{static_cast<float>(std::max(1u, renderW / 2)), static_cast<float>(std::max(1u, renderH / 2))};
                add(PostPass::Ao, m_postScene, m_aoTarget, renderSize, half);  // source: the scene's depth
                add(PostPass::AoBlur, m_aoTarget, m_aoBlurTarget, half, half);
                m_stats.ambientOcclusion = true;
            }
            add(PostPass::Composite, sceneColor, fxaa ? m_postLdr : outputTarget, renderSize, outSize);
            if (fxaa) add(PostPass::Fxaa, m_postLdr, outputTarget, outSize, outSize);
            m_stats.postProcessing = true;
            m_taaValid = taaReady;
            m_stats.hdr = postHdr;
            m_stats.bloomLevels = bloom ? static_cast<u32>(m_bloomLevels.size()) : 0u;
        }
    }

    if (!postActive || !taa) m_taaValid = false;
    m_prevViewProj = unjitteredViewProj;  // TAA reprojection next frame
    m_stats.collectMs = cpu.elapsedMilliseconds();
    pushProbeConversion();
    Stopwatch submit;
    const rhi::FrameStats before = m_device.frameStats();
    uploadUniforms();
    uploadInstances();
    preparation.reset();
    {
        AGR_PROFILE_SCOPE("Ombres");
        if (m_uniformCapacity > 0) drawShadowPass();
    }
    if (m_uniformCapacity > 0) runProbeConversion();
    AGR_PROFILE_SCOPE("Passes (scène, post-traitement)");

    // ------------------------------------------------------------ passes
    const bool postPath = !m_postPasses.empty();
    const bool offscreen = !postPath && (samples > 1 || renderW != width || renderH != height);
    if (offscreen) ensureTargets(renderW, renderH, samples, samples > 1 && (renderW != width || renderH != height));
    const rhi::RenderTargetHandle target = postPath ? m_postScene : offscreen ? m_sceneTarget : outputTarget;
    m_device.beginPass(target, {true, rv.clearColor, true, 1.0f});
    if (m_uniformCapacity > 0) {
        drawMeshItems(m_opaqueItems, 0, false);
        drawOutlines(m_opaqueItems, 0);
        drawSky();
        drawMeshItems(m_transparentItems, static_cast<u32>(m_opaqueItems.size()), false);
        drawSprites(m_spriteItems);
    }
    if (!offscreen && !postPath) drawOverlay();  // same pass: the overlay ignores depth
    m_device.endPass();
    if (postPath) {
        if (m_postResolve) m_device.blit(m_postScene, m_postResolve, rhi::Filter::Nearest);  // MSAA resolve
        runPost(outputTarget);
        if (!m_overlay.empty()) {
            m_device.beginPass(outputTarget, {false, {}, false, 1.0f});
            drawOverlay();
            m_device.endPass();
        }
    }
    if (offscreen && m_sceneTarget) {
        if (m_resolveTarget) {
            m_device.blit(m_sceneTarget, m_resolveTarget, rhi::Filter::Nearest);  // MSAA resolve
            m_device.blit(m_resolveTarget, outputTarget, rhi::Filter::Linear);    // upscale
        } else {
            m_device.blit(m_sceneTarget, outputTarget, rhi::Filter::Linear);
        }
        if (!m_overlay.empty()) {
            // At full output resolution (crisp text), without MSAA.
            m_device.beginPass(outputTarget, {false, {}, false, 1.0f});
            drawOverlay();
            m_device.endPass();
        }
    }
    m_stats.submitMs = submit.elapsedMilliseconds();

    if (m_frame % 300 == 0) sweepTextureCache();
    const rhi::FrameStats& ds = m_device.frameStats();
    m_stats.drawCalls = ds.drawCalls - before.drawCalls;
    m_stats.triangles = ds.triangles - before.triangles;
    m_stats.gpuTimeMs = ds.gpuTimeMs;
    m_stats.gpuWaitMs = ds.cpuWaitMs - before.cpuWaitMs;  // only the waits inside the submit window
    m_stats.renderWidth = renderW;
    m_stats.renderHeight = renderH;
    m_stats.msaaSamples = samples;
    const rhi::MemoryStats mem = m_device.memoryStats();
    m_stats.gpuMemoryBytes = mem.bufferBytes + mem.textureBytes;
    m_stats.cpuMs = cpu.elapsedMilliseconds();
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

    const rhi::PipelineHandle pipelines[4] = {m_spritePipeline, m_spriteLitPipeline, m_spriteAdditivePipeline, m_spriteLitNormalPipeline};
    m_device.bindUniformBuffer(kSlotFrame, m_uniformBuffer, 0, sizeof(FrameUniforms));
    m_device.bindUniformBuffer(kSlotLights2D, m_uniformBuffer, m_lightsOffset, sizeof(Lights2DUniforms));
    m_device.bindVertexBuffer(m_spriteVertices);
    m_device.bindIndexBuffer(m_spriteIndices, rhi::IndexType::U32);
    u32 batchStart = 0;
    auto textureAt = [&](u32 i) { return items[m_spriteKeys[i].index].texture; };
    auto pipelineAt = [&](u32 i) { return items[m_spriteKeys[i].index].pipeline; };
    auto normalAt = [&](u32 i) { return items[m_spriteKeys[i].index].normalMap; };
    for (u32 i = 1; i <= count; ++i) {
        if (i == count || textureAt(i) != textureAt(batchStart) || pipelineAt(i) != pipelineAt(batchStart) || normalAt(i) != normalAt(batchStart)) {
            const u32 pipeline = std::min(pipelineAt(batchStart), 3u);
            m_device.bindPipeline(pipelines[pipeline]);
            m_device.bindTexture(0, textureAt(batchStart));
            if (pipeline == 1 || pipeline == 3) {
                m_device.bindTexture(1, m_shadow2D ? m_shadow2D : m_assets.whiteTexture());
                m_device.bindTexture(2, normalAt(batchStart) ? normalAt(batchStart) : m_flatNormal);
            }
            m_device.drawIndexed((i - batchStart) * 6, batchStart * 6);
            ++m_stats.spriteBatches;
            batchStart = i;
        }
    }
}

void Renderer::drawOverlay() {
    if (m_overlay.empty() || m_uniformCapacity == 0) return;
    const usize vbytes = m_overlay.vertices.size() * sizeof(OverlayVertex), ibytes = m_overlay.indices.size() * sizeof(u32);
    if (vbytes > m_overlayVertexCapacity || ibytes > m_overlayIndexCapacity) {
        m_device.destroyBuffer(m_overlayVertices);
        m_device.destroyBuffer(m_overlayIndices);
        m_overlayVertexCapacity = std::max<usize>(64 * 1024, std::bit_ceil(vbytes));
        m_overlayIndexCapacity = std::max<usize>(32 * 1024, std::bit_ceil(ibytes));
        auto vb = m_device.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Dynamic, m_overlayVertexCapacity, nullptr, "agr.overlay.vb"});
        auto ib = m_device.createBuffer({rhi::BufferUsage::Index, rhi::BufferMemory::Dynamic, m_overlayIndexCapacity, nullptr, "agr.overlay.ib"});
        if (!vb || !ib) {
            AGR_LOG_ERROR("Render", "overlay buffer allocation failed");
            m_overlayVertexCapacity = m_overlayIndexCapacity = 0;
            return;
        }
        m_overlayVertices = vb.value();
        m_overlayIndices = ib.value();
    }
    (void)m_device.updateBuffer(m_overlayVertices, 0, m_overlay.vertices.data(), vbytes);
    (void)m_device.updateBuffer(m_overlayIndices, 0, m_overlay.indices.data(), ibytes);
    m_device.bindPipeline(m_overlayPipeline);
    m_device.bindUniformBuffer(kSlotFrame, m_uniformBuffer, m_overlayFrameOffset, sizeof(FrameUniforms));
    m_device.bindVertexBuffer(m_overlayVertices);
    m_device.bindIndexBuffer(m_overlayIndices, rhi::IndexType::U32);
    for (const OverlayCommand& c : m_overlay.commands) {
        if (c.indexCount == 0) continue;
        m_device.bindTexture(0, c.texture ? c.texture : m_assets.whiteTexture());
        m_device.drawIndexed(c.indexCount, c.firstIndex);
    }
}

void OverlayDrawList::quad(math::Vec2 mn, math::Vec2 mx, math::Vec4 uv, u32 color, rhi::TextureHandle texture) {
    const u32 base = static_cast<u32>(vertices.size());
    vertices.push_back({{mn.x, mn.y, 0.0f}, {uv.x, uv.y}, color});
    vertices.push_back({{mx.x, mn.y, 0.0f}, {uv.z, uv.y}, color});
    vertices.push_back({{mx.x, mx.y, 0.0f}, {uv.z, uv.w}, color});
    vertices.push_back({{mn.x, mx.y, 0.0f}, {uv.x, uv.w}, color});
    if (commands.empty() || commands.back().texture != texture)
        commands.push_back({texture, static_cast<u32>(indices.size()), 0});
    for (u32 i : {0u, 1u, 2u, 0u, 2u, 3u}) indices.push_back(base + i);
    commands.back().indexCount += 6;
}

void OverlayDrawList::quad4(const math::Vec2 p[4], const math::Vec2 uv[4], const u32 c[4], rhi::TextureHandle texture) {
    const u32 base = static_cast<u32>(vertices.size());
    for (int i = 0; i < 4; ++i) vertices.push_back({{p[i].x, p[i].y, 0.0f}, uv[i], c[i]});
    if (commands.empty() || commands.back().texture != texture) commands.push_back({texture, static_cast<u32>(indices.size()), 0});
    for (u32 i : {0u, 1u, 2u, 0u, 2u, 3u}) indices.push_back(base + i);
    commands.back().indexCount += 6;
}

u32 packColor(math::Vec4 c) {
    auto b = [](float v) { return static_cast<u32>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return b(c.x) | (b(c.y) << 8) | (b(c.z) << 16) | (b(c.w) << 24);
}

Status Renderer::createPostPipelines() {
    const char* fragments[12] = {shaders::kBloomPrefilter, shaders::kBloomDownsample, shaders::kBloomUpsample, shaders::kComposite, shaders::kFxaa,
                                 shaders::kSsao,           shaders::kSsaoBlur,       shaders::kAtmosphere,     shaders::kDof,      shaders::kLuminance,
                                 shaders::kSsr,            shaders::kTaa};
    const char* names[12] = {"agr.post.prefilter", "agr.post.down", "agr.post.up",        "agr.post.composite", "agr.post.fxaa",
                             "agr.post.ssao",      "agr.post.ssao.blur", "agr.post.atmosphere", "agr.post.dof",   "agr.post.luminance", "agr.post.ssr", "agr.post.taa"};
    for (int k = 0; k < 12; ++k) {
        rhi::PipelineDesc p;
        p.debugName = names[k];
        p.shader.vertex = shaders::kPostVertex;
        p.shader.fragment = std::string(shaders::kPostBlock) + fragments[k];
        p.layout.stride = sizeof(math::Vec2);
        p.layout.attributes = {{0, rhi::VertexFormat::Float2, 0}};
        p.cull = rhi::CullMode::None;
        p.depthTest = false;
        p.depthWrite = false;
        p.blend = k == PostPass::Up ? rhi::BlendMode::Additive : rhi::BlendMode::Opaque;
        p.uniformBlocks = {"PostData"};
        p.samplers = k == PostPass::Composite                          ? std::vector<std::string>{"u_scene", "u_bloomTex", "u_aoTex", "u_exposureTex"}
                     : k == PostPass::Atmosphere || k == PostPass::Dof || k == PostPass::Ssr ? std::vector<std::string>{"u_source", "u_depth"}
                     : k == PostPass::Luminance                        ? std::vector<std::string>{"u_source", "u_history"}
                     : k == PostPass::Taa                              ? std::vector<std::string>{"u_source", "u_depth", "u_history"}
                                                                       : std::vector<std::string>{"u_source"};
        auto r = m_device.createPipeline(p);
        if (!r) return r.error();
        m_postPipelines[k] = r.value();
    }
    return {};
}

void Renderer::releasePostTargets() {
    for (rhi::RenderTargetHandle t : {m_postScene, m_postResolve, m_postLdr, m_aoTarget, m_aoBlurTarget, m_atmosTarget}) m_device.destroyRenderTarget(t);
    m_atmosTarget = {};
    m_taaValid = false;
    for (rhi::RenderTargetHandle* t : {&m_dofTarget, &m_ssrTarget, &m_taaTargets[0], &m_taaTargets[1], &m_lumTargets[0], &m_lumTargets[1]}) {
        m_device.destroyRenderTarget(*t);
        *t = {};
    }
    m_lumValid = false;
    for (rhi::RenderTargetHandle t : m_bloomLevels) m_device.destroyRenderTarget(t);
    m_postScene = m_postResolve = m_postLdr = {};
    m_bloomLevels.clear();
    m_bloomSizes.clear();
    m_postW = m_postH = m_postSamples = m_postOutW = m_postOutH = 0;
}

bool Renderer::ensurePostTargets(u32 w, u32 h, u32 samples, u32 outW, u32 outH, bool hdr, bool bloom, bool fxaa, bool ao, bool atmos) {
    if (m_postScene && w == m_postW && h == m_postH && samples == m_postSamples && outW == m_postOutW && outH == m_postOutH && hdr == m_postHdr &&
        bloom == m_postBloom && fxaa == m_postFxaa && ao == m_postAo && atmos == m_postAtmos && m_postReadDepth == m_postSceneReadable)
        return true;
    releasePostTargets();
    const rhi::TextureFormat color = hdr ? rhi::TextureFormat::RGBA16F : rhi::TextureFormat::RGBA8;
    auto make = [&](u32 tw, u32 th, rhi::TextureFormat f, bool depth, u32 s, const char* name, bool readDepth = false) -> rhi::RenderTargetHandle {
        auto t = m_device.createRenderTarget({.width = std::max(1u, tw), .height = std::max(1u, th), .colorFormat = f, .depth = depth,
                                              .sampleableDepth = readDepth, .samples = s, .debugName = name});
        if (!t) {
            AGR_LOG_ERROR("Render", "post-processing target '{}': {}", name, t.error().message);
            return {};
        }
        return t.value();
    };
    // SSAO reads the scene depth: a single-sampled, sampleable depth texture (the caller drops MSAA).
    m_postScene = make(w, h, color, true, samples, "agr.post.scene", m_postReadDepth && samples == 1);
    if (atmos) m_atmosTarget = make(w, h, color, false, 1, "agr.post.atmosphere");
    if (ao) {
        m_aoTarget = make(w / 2, h / 2, rhi::TextureFormat::RGBA8, false, 1, "agr.post.ao");
        m_aoBlurTarget = make(w / 2, h / 2, rhi::TextureFormat::RGBA8, false, 1, "agr.post.ao.blur");
    }
    if (samples > 1) m_postResolve = make(w, h, color, false, 1, "agr.post.resolve");
    if (fxaa) m_postLdr = make(outW, outH, rhi::TextureFormat::RGBA8, false, 1, "agr.post.ldr");
    if (bloom) {
        u32 bw = std::max(1u, w / 2), bh = std::max(1u, h / 2);
        for (int level = 0; level < 6 && std::min(bw, bh) >= 8; ++level) {
            m_bloomLevels.push_back(make(bw, bh, color, false, 1, "agr.post.bloom"));
            m_bloomSizes.push_back({bw, bh});
            bw = std::max(1u, bw / 2);
            bh = std::max(1u, bh / 2);
        }
    }
    const bool ok = m_postScene && (samples == 1 || m_postResolve) && (!fxaa || m_postLdr) && (!ao || (m_aoTarget && m_aoBlurTarget)) && (!atmos || m_atmosTarget) &&
                    std::all_of(m_bloomLevels.begin(), m_bloomLevels.end(), [](rhi::RenderTargetHandle t) { return static_cast<bool>(t); });
    if (!ok) {
        releasePostTargets();
        return false;  // the frame is drawn without post-processing
    }
    m_postW = w, m_postH = h, m_postSamples = samples, m_postOutW = outW, m_postOutH = outH;
    m_postHdr = hdr, m_postBloom = bloom, m_postFxaa = fxaa, m_postAo = ao, m_postAtmos = atmos;
    m_postSceneReadable = m_postReadDepth;
    return true;
}

void Renderer::runPost(rhi::RenderTargetHandle output) {
    rhi::TextureHandle bloom = m_bloomLevels.empty() ? m_assets.whiteTexture() : m_device.renderTargetTexture(m_bloomLevels[0]);
    const rhi::TextureHandle ao = m_postAo && m_aoBlurTarget ? m_device.renderTargetTexture(m_aoBlurTarget) : m_assets.whiteTexture();
    for (const PostPass& pass : m_postPasses) {
        const rhi::TextureHandle src = pass.kind == PostPass::Ao ? m_device.renderTargetDepthTexture(pass.source) : m_device.renderTargetTexture(pass.source);
        m_device.prepareTextureForSampling(src);
        const bool composite = pass.kind == PostPass::Composite;
        if (composite && !m_bloomLevels.empty()) m_device.prepareTextureForSampling(bloom);
        if (composite && m_postAo) m_device.prepareTextureForSampling(ao);
        const bool atmosPass = pass.kind == PostPass::Atmosphere || pass.kind == PostPass::Dof || pass.kind == PostPass::Ssr || pass.kind == PostPass::Taa;
        const rhi::TextureHandle sceneDepth = atmosPass ? m_device.renderTargetDepthTexture(m_postScene) : rhi::TextureHandle{};
        if (atmosPass) m_device.prepareTextureForSampling(sceneDepth);
        const bool lumPass = pass.kind == PostPass::Luminance, taaPass = pass.kind == PostPass::Taa;
        const rhi::TextureHandle history = lumPass   ? m_device.renderTargetTexture(m_lumTargets[m_lumIndex ^ 1u])
                                           : taaPass ? m_device.renderTargetTexture(m_taaTargets[m_taaIndex ^ 1u])
                                                     : rhi::TextureHandle{};
        if (lumPass || taaPass) m_device.prepareTextureForSampling(history);
        const bool exposurePass = composite && m_lumValid && m_lumTargets[m_lumIndex];
        const rhi::TextureHandle exposure = exposurePass ? m_device.renderTargetTexture(m_lumTargets[m_lumIndex]) : m_assets.whiteTexture();
        if (exposurePass) m_device.prepareTextureForSampling(exposure);
        // Upsampling adds into the target: keep its content (loaded); other passes overwrite every pixel.
        m_device.beginPass(pass.target ? pass.target : output, {false, {}, false, 1.0f});
        m_device.bindPipeline(m_postPipelines[pass.kind]);
        m_device.bindUniformBuffer(0, m_uniformBuffer, pass.uniformOffset, kPostUniformBytes);
        m_device.bindTexture(0, src);
        if (atmosPass) m_device.bindTexture(1, sceneDepth);
        if (lumPass) m_device.bindTexture(1, history);
        if (taaPass) m_device.bindTexture(2, history);
        if (composite) {
            m_device.bindTexture(1, bloom);
            m_device.bindTexture(2, ao);
            m_device.bindTexture(3, exposure);
        }
        m_device.bindVertexBuffer(m_skyVertices);
        m_device.draw(3);
        m_device.endPass();
    }
}

} // namespace agr
