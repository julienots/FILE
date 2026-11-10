#include "agr/render/Renderer.hpp"

#include <algorithm>
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
using math::Vec4;

namespace {

using detail::FrameUniforms;

constexpr u32 kSlotFrame = 0;
constexpr u32 kSlotLights2D = 1;  // sprite pipelines: FrameData, Lights2D

struct Lights2DUniforms {
    Vec4 posRadius[Renderer::kMaxLights2D];
    Vec4 colorFalloff[Renderer::kMaxLights2D];
    i32 count[4];
};
static_assert(sizeof(Lights2DUniforms) == 528);

using detail::srgbToLinear;

} // namespace

Renderer::Renderer(rhi::Device& device, AssetManager& assets) : m_device(device), m_assets(assets) {}

Renderer::~Renderer() {
    for (auto& [uri, cached] : m_textureCache) m_assets.release(cached.asset);
    for (auto& [uri, cached] : m_pixelTextureCache) m_assets.release(cached.asset);
    for (auto& [uri, cached] : m_dataTextureCache) m_assets.release(cached.asset);
    releaseTargets();
    for (auto& m : m_meshes) {
        m_device.destroyBuffer(m.vertices);
        m_device.destroyBuffer(m.indices);
    }
    m_device.destroyBuffer(m_uniformBuffer);
    m_device.destroyBuffer(m_spriteVertices);
    m_device.destroyBuffer(m_spriteIndices);
    for (auto p : m_pbrPipelines) m_device.destroyPipeline(p);
    m_device.destroyPipeline(m_shadowPipeline);
    m_device.destroyPipeline(m_shadowMaskPipeline);
    m_device.destroyPipeline(m_skyPipeline);
    m_device.destroyBuffer(m_skyVertices);
    m_device.destroyBuffer(m_instanceBuffer);
    m_device.destroyTexture(m_flatNormal);
    m_device.destroyRenderTarget(m_shadowTarget);
    m_device.destroyPipeline(m_spritePipeline);
    m_device.destroyPipeline(m_spriteLitPipeline);
    m_device.destroyPipeline(m_spriteAdditivePipeline);
    m_device.destroyPipeline(m_overlayPipeline);
    m_device.destroyBuffer(m_overlayVertices);
    m_device.destroyBuffer(m_overlayIndices);
    m_device.destroyTexture(m_softParticle);
}

Status Renderer::initialize() {
    m_uniformAlign = std::max(16u, m_device.info().uniformBufferAlignment);
    m_glClipSpace = m_device.info().backend != rhi::Backend::Vulkan;
    if (Status s = createPipelines(); !s) return s;
    if (Status s = create3DPipelines(); !s) return s;
    if (Status s = createMeshes(); !s) return s;
    return {};
}

Status Renderer::createPipelines() {
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
    auto lp = m_device.createPipeline(lit);
    if (!lp) return lp.error();
    m_spriteLitPipeline = lp.value();

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
    for (TextureCache* cache : {&m_textureCache, &m_pixelTextureCache, &m_dataTextureCache}) {
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
    Stopwatch cpu;
    ++m_frame;
    m_stats = {};
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
    const Mat4 viewProj = rv.projectionMatrix(aspect, m_glClipSpace) * view;
    const math::Frustum frustum = math::Frustum::fromMatrix(viewProj, range);
    auto* states = reg.findPool<StateComponent>();
    auto hiddenInEditor = [&](ecs::Entity e) {
        if (!rv.editorView || !states) return false;
        const StateComponent* st = states->tryGet(e);
        return st && st->hiddenInEditor;
    };

    // ------------------------------------------------------------ frame: light, environment
    FrameUniforms frame{};
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
        const bool gradient = env.sky == SkyMode::Gradient;
        const Vec3 sky = srgbToLinear(env.skyColor), ground = srgbToLinear(env.groundColor);
        // Sprites lit by 2D lights use one flat ambient: with a gradient sky, its average.
        frame.ambient = gradient ? Vec4((sky + ground) * 0.5f * env.ambientIntensity, 0.0f)
                                 : Vec4(srgbToLinear(env.ambientColor) * env.ambientIntensity, 0.0f);
        frame.skyColor = Vec4(sky, gradient ? 1.0f : 0.0f);
        frame.groundColor = Vec4(ground, env.ambientIntensity);
        frame.horizonColor = Vec4(srgbToLinear(env.horizonColor), env.sunDisc ? 1.0f : 0.0f);
        frame.fogColor = Vec4(srgbToLinear(env.fogColor), env.fog ? 1.0f : 0.0f);
        frame.fogParams = Vec4(env.fogStart, std::max(env.fogEnd, env.fogStart + 0.01f), std::max(env.exposure, 0.0f), 0.0f);
        m_drawSky = gradient && rv.projection == Projection::Perspective;
    }
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
        reg.view<Light2DComponent, WorldTransformComponent>().each([&](ecs::Entity e, Light2DComponent& l, WorldTransformComponent& w) {
            if (n >= kMaxLights2D || !w.activeInHierarchy || hiddenInEditor(e) || l.intensity <= 0.0f) return;
            const Vec3 pos = w.matrix.translation() + parallaxOf(e);
            const float radius = std::max(l.radius, 1e-3f);
            if (!frustum.intersects(math::Aabb::fromCenterExtents(pos, Vec3{radius}))) return;
            lights.posRadius[n] = Vec4(pos, radius);
            lights.colorFalloff[n] = Vec4(srgbToLinear(l.color) * l.intensity, std::max(l.falloff, 0.01f));
            ++n;
        });
        lights.count[0] = static_cast<i32>(n);
        m_stats.lights2D = n;
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
            m_spriteItems.push_back(item);
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
        const u32 color = packColor(tm.color);
        for (i32 y = cy0; y <= cy1; ++y) {
            for (i32 x = cx0; x <= cx1; ++x) {
                const i32 id = tm.tileAt(static_cast<u32>(x), static_cast<u32>(y));
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
                item.pipeline = tm.lit ? 1u : 0u;
                item.texture = tex;
                m_spriteItems.push_back(item);
                ++m_stats.tilesVisible;
            }
        }
    });

    // ------------------------------------------------------------ particles
    reg.view<ParticleEmitter2DComponent, ParticleState2DComponent, WorldTransformComponent>().each(
        [&](ecs::Entity e, ParticleEmitter2DComponent& em, ParticleState2DComponent& st, WorldTransformComponent& w) {
            if (!w.activeInHierarchy || hiddenInEditor(e) || st.particles.empty()) return;
            const Vec3 off = parallaxOf(e);
            const rhi::TextureHandle tex = em.texture.empty() ? m_softParticle : textureFor(em.texture, false);
            const float depth = view.transformPoint(w.matrix.translation() + off).z;
            const Vec4 c0 = em.startColor, c1 = em.endColor;
            for (const Particle2D& p : st.particles) {
                const float t = std::clamp(p.age / std::max(p.lifetime, 1e-4f), 0.0f, 1.0f);
                const float half = math::lerp(em.startSize, em.endSize, t) * 0.5f;
                const Vec3 center = (em.worldSpace ? Vec3{p.position.x, p.position.y, p.z} : w.matrix.transformPoint({p.position.x, p.position.y, 0.0f})) + off;
                const float cs = std::cos(p.rotation) * half, sn = std::sin(p.rotation) * half;
                const Vec3 dx{cs, sn, 0.0f}, dy{-sn, cs, 0.0f};
                SpriteItem item;
                item.corners[0] = center - dx - dy;
                item.corners[1] = center + dx - dy;
                item.corners[2] = center + dx + dy;
                item.corners[3] = center - dx + dy;
                item.layer = em.sortingLayer;
                item.order = em.orderInLayer;
                item.depth = depth;
                item.uv = {0, 0, 1, 1};
                item.color = packColor(Vec4{math::lerp(c0.x, c1.x, t), math::lerp(c0.y, c1.y, t), math::lerp(c0.z, c1.z, t), math::lerp(c0.w, c1.w, t)});
                item.pipeline = em.additive ? 2u : 0u;
                item.texture = tex;
                m_spriteItems.push_back(item);
                ++m_stats.particlesVisible;
            }
        });
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
            m_overlayFrameOffset = pushUniforms(&ui, sizeof ui);
            m_stats.overlayQuads = static_cast<u32>(m_overlay.indices.size() / 6);
        }
    }

    m_stats.collectMs = cpu.elapsedMilliseconds();
    Stopwatch submit;
    const rhi::FrameStats before = m_device.frameStats();
    uploadUniforms();
    uploadInstances();
    if (m_uniformCapacity > 0) drawShadowPass();

    // ------------------------------------------------------------ passes
    const bool offscreen = samples > 1 || renderW != width || renderH != height;
    if (offscreen) ensureTargets(renderW, renderH, samples, samples > 1 && (renderW != width || renderH != height));
    const rhi::RenderTargetHandle target = offscreen ? m_sceneTarget : outputTarget;
    m_device.beginPass(target, {true, rv.clearColor, true, 1.0f});
    if (m_uniformCapacity > 0) {
        drawMeshItems(m_opaqueItems, 0, false);
        drawSky();
        drawMeshItems(m_transparentItems, static_cast<u32>(m_opaqueItems.size()), false);
        drawSprites(m_spriteItems);
    }
    if (!offscreen) drawOverlay();  // same pass: the overlay ignores depth
    m_device.endPass();
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

    const rhi::PipelineHandle pipelines[3] = {m_spritePipeline, m_spriteLitPipeline, m_spriteAdditivePipeline};
    m_device.bindUniformBuffer(kSlotFrame, m_uniformBuffer, 0, sizeof(FrameUniforms));
    m_device.bindUniformBuffer(kSlotLights2D, m_uniformBuffer, m_lightsOffset, sizeof(Lights2DUniforms));
    m_device.bindVertexBuffer(m_spriteVertices);
    m_device.bindIndexBuffer(m_spriteIndices, rhi::IndexType::U32);
    u32 batchStart = 0;
    auto textureAt = [&](u32 i) { return items[m_spriteKeys[i].index].texture; };
    auto pipelineAt = [&](u32 i) { return items[m_spriteKeys[i].index].pipeline; };
    for (u32 i = 1; i <= count; ++i) {
        if (i == count || textureAt(i) != textureAt(batchStart) || pipelineAt(i) != pipelineAt(batchStart)) {
            m_device.bindPipeline(pipelines[std::min(pipelineAt(batchStart), 2u)]);
            m_device.bindTexture(0, textureAt(batchStart));
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

u32 packColor(math::Vec4 c) {
    auto b = [](float v) { return static_cast<u32>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return b(c.x) | (b(c.y) << 8) | (b(c.z) << 16) | (b(c.w) << 24);
}

} // namespace agr
