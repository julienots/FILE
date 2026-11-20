// Reflection probes: a second Renderer captures the scene around each probe (one cube face
// per frame, linear light when RGBA16F targets exist), then a GPU pass prefilters the 6 faces
// into the probe's row of an equirectangular atlas (one tile per roughness level). The PBR
// shader picks the probe whose box holds the shaded point (agrProbeRadiance).
#include <algorithm>
#include <cstring>

#include "RenderUniforms.hpp"
#include "Shaders.inl"
#include "Shaders3D.inl"
#include "agr/core/Log.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/Renderer.hpp"

namespace agr {

using math::Mat4;
using math::Vec3;
using math::Vec4;

namespace {

constexpr u32 kAtlasW = detail::kProbeTileW * detail::kProbeLevels;
constexpr u32 kAtlasH = detail::kProbeTileH * detail::kMaxProbes;

// Cube faces: forward and up of each capture camera.
const Vec3 kFaceF[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
const Vec3 kFaceU[6] = {{0, 1, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, 1, 0}};

struct ProbeUniforms {
    Vec4 tile, params, atlas;
    Vec4 faceF[6], faceU[6];
};

u64 hashBytes(u64 h, const void* data, usize size) {
    const auto* b = static_cast<const u8*>(data);
    for (usize i = 0; i < size; ++i) h = (h ^ b[i]) * 1099511628211ull;
    return h;
}

} // namespace

void Renderer::releaseProbes() {
    for (rhi::RenderTargetHandle& t : m_probeFaces) {
        m_device.destroyRenderTarget(t);
        t = {};
    }
    m_device.destroyRenderTarget(m_probeAtlas);
    m_probeAtlas = {};
    m_device.destroyPipeline(m_probeConvertPipeline);
    m_probeConvertPipeline = {};
    m_probeRenderer.reset();
    m_probeSlots.clear();
}

void Renderer::updateReflectionProbes(Scene& scene) {
    m_probeConvertSlot = -1;
    m_stats.probeCapture = false;
    auto& reg = scene.registry();
    if (!reg.findPool<ReflectionProbeComponent>()) return;
    const u64 stamp = m_frame + 1;
    bool any = false;
    reg.view<ReflectionProbeComponent, WorldTransformComponent>().each([&](ecs::Entity e, ReflectionProbeComponent& p, WorldTransformComponent& w) {
        if (!p.enabled || !w.activeInHierarchy) return;
        auto it = std::find_if(m_probeSlots.begin(), m_probeSlots.end(), [&](const ProbeSlot& s) { return s.entity == e; });
        if (it == m_probeSlots.end()) {
            // A free row: never used, or its probe is gone (not seen last frame).
            it = std::find_if(m_probeSlots.begin(), m_probeSlots.end(), [&](const ProbeSlot& s) { return s.seenFrame + 1 < stamp; });
            if (it == m_probeSlots.end()) {
                if (m_probeSlots.size() >= detail::kMaxProbes) return;  // a 9th probe: ignored
                m_probeSlots.push_back({});
                it = m_probeSlots.end() - 1;
            }
            *it = ProbeSlot{e, 0, 6, false, stamp};
        }
        it->seenFrame = stamp;
        any = true;
        u64 h = 1469598103934665603ull;
        h = hashBytes(h, &w.matrix, sizeof w.matrix);
        h = hashBytes(h, &p.size, sizeof p.size);
        h = hashBytes(h, &p.maxDistance, sizeof p.maxDistance);
        h = hashBytes(h, &m_probeEpoch, sizeof m_probeEpoch);
        if (h != it->hash) {
            it->hash = h;
            it->facesDone = 0;  // (re)capture; the previous capture stays in use meanwhile
        } else if (it->facesDone >= 6 && p.update == ReflectionProbeComponent::Update::Always) {
            it->facesDone = 0;
        }
    });
    for (ProbeSlot& s : m_probeSlots)
        if (s.seenFrame != stamp) s.ready = false;
    if (!any) return;

    // Resources, created with the first probe.
    if (!m_probeAtlas) {
        m_probeLinear = m_device.info().halfFloatRenderTargets;
        const rhi::TextureFormat format = m_probeLinear ? rhi::TextureFormat::RGBA16F : rhi::TextureFormat::RGBA8;
        for (rhi::RenderTargetHandle& t : m_probeFaces) {
            rhi::RenderTargetDesc d;
            d.width = d.height = detail::kProbeFaceSize;
            d.colorFormat = format;
            d.depth = true;
            d.debugName = "agr.probe.face";
            auto r = m_device.createRenderTarget(d);
            if (!r) {
                AGR_LOG_ERROR("Render", "reflection probe target: {}", r.error().message);
                releaseProbes();
                return;
            }
            t = r.value();
        }
        rhi::RenderTargetDesc a;
        a.width = kAtlasW;
        a.height = kAtlasH;
        a.colorFormat = format;
        a.depth = false;
        a.debugName = "agr.probe.atlas";
        auto atlas = m_device.createRenderTarget(a);
        rhi::PipelineDesc p;
        p.debugName = "agr.probe.convert";
        p.shader.vertex = shaders::kPostVertex;
        p.shader.fragment = shaders::kProbeConvert;
        p.layout.stride = sizeof(math::Vec2);
        p.layout.attributes = {{0, rhi::VertexFormat::Float2, 0}};
        p.cull = rhi::CullMode::None;
        p.depthTest = false;
        p.depthWrite = false;
        p.uniformBlocks = {"ProbeData"};
        p.samplers = {"u_face0", "u_face1", "u_face2", "u_face3", "u_face4", "u_face5"};
        auto pipe = m_device.createPipeline(p);
        auto child = std::make_unique<Renderer>(m_device, m_assets);
        child->m_probeCaptureMode = true;
        child->m_probeLinear = m_probeLinear;
        const Status init = child->initialize();
        if (!atlas || !pipe || !init) {
            AGR_LOG_ERROR("Render", "reflection probes unavailable: {}",
                          !atlas ? atlas.error().message : !pipe ? pipe.error().message : init.error().message);
            if (atlas) m_device.destroyRenderTarget(atlas.value());
            if (pipe) m_device.destroyPipeline(pipe.value());
            releaseProbes();
            return;
        }
        m_probeAtlas = atlas.value();
        m_probeConvertPipeline = pipe.value();
        m_probeRenderer = std::move(child);
        m_device.beginPass(m_probeAtlas, {true, {0, 0, 0, 1}, false, 1.0f});  // defined content before the first capture
        m_device.endPass();
    }
    GraphicsProfile cp = m_profile;
    cp.msaaSamples = 1;
    cp.renderScale = 1.0f;
    cp.shadowMapSize = std::min(cp.shadowMapSize, 1024u);
    cp.shadowCascades = std::min(cp.shadowCascades, 2u);
    m_probeRenderer->setProfile(cp);
    m_probeRenderer->setModels(m_models);
    m_probeRenderer->setMaterials(m_materials);
    m_probeRenderer->setShaders(m_shaders);
    m_probeRenderer->setEnvironments(m_environments);
    m_probeRenderer->setParticleCurves(m_particleCurves);
    m_probeRenderer->setTime(m_time);

    // One face of one probe per frame (the renderer draws one view per device frame).
    for (usize i = 0; i < m_probeSlots.size(); ++i) {
        ProbeSlot& s = m_probeSlots[i];
        if (s.seenFrame != stamp || s.facesDone >= 6) continue;
        const auto& w = reg.get<WorldTransformComponent>(s.entity);
        const auto& p = reg.get<ReflectionProbeComponent>(s.entity);
        const u32 f = s.facesDone;
        const Vec3 pos = w.matrix.translation();
        RenderView rv;
        rv.position = pos;
        rv.view = Mat4::lookAt(pos, pos + kFaceF[f], kFaceU[f]);
        rv.fieldOfView = math::radians(90.0f);
        rv.nearPlane = 0.05f;
        rv.farPlane = std::max(p.maxDistance, 1.0f);
        rv.clearColor = {0, 0, 0, 1};
        rv.direct = true;
        rv.overlay = false;
        m_probeRenderer->renderView(scene, rv, m_probeFaces[f], detail::kProbeFaceSize, detail::kProbeFaceSize);
        m_stats.probeCapture = true;
        if (++s.facesDone == 6) m_probeConvertSlot = static_cast<i32>(i);
        break;
    }
}

void Renderer::pushProbeConversion() {
    m_probeConvertOffsets.clear();
    if (m_probeConvertSlot < 0 || !m_probeAtlas) return;
    const float row = static_cast<float>(m_probeConvertSlot);
    for (u32 level = 0; level < detail::kProbeLevels; ++level) {
        ProbeUniforms u{};
        const float tw = 1.0f / static_cast<float>(detail::kProbeLevels), th = 1.0f / static_cast<float>(detail::kMaxProbes);
        u.tile = Vec4(static_cast<float>(level) * tw, row * th, static_cast<float>(level + 1) * tw, (row + 1.0f) * th);
        u.params = Vec4(static_cast<float>(level) / static_cast<float>(detail::kProbeLevels - 1), m_probeLinear ? 1.0f : 0.0f,
                        m_glClipSpace ? 0.0f : 1.0f, m_probeExposure);
        u.atlas = Vec4(1.0f / static_cast<float>(kAtlasW), 1.0f / static_cast<float>(kAtlasH), 0.0f, 0.0f);
        for (u32 f = 0; f < 6; ++f) {
            u.faceF[f] = Vec4(kFaceF[f], 0.0f);
            u.faceU[f] = Vec4(kFaceU[f], 0.0f);
        }
        m_probeConvertOffsets.push_back(pushUniforms(&u, sizeof u));
    }
}

void Renderer::runProbeConversion() {
    if (!m_probeAtlas) return;
    if (!m_probeConvertOffsets.empty() && m_probeConvertSlot >= 0) {
        for (rhi::RenderTargetHandle f : m_probeFaces) m_device.prepareTextureForSampling(m_device.renderTargetTexture(f));
        for (u32 offset : m_probeConvertOffsets) {
            m_device.beginPass(m_probeAtlas, {false, {}, false, 1.0f});  // other tiles are kept
            m_device.bindPipeline(m_probeConvertPipeline);
            m_device.bindUniformBuffer(0, m_uniformBuffer, offset, sizeof(ProbeUniforms));
            for (u32 f = 0; f < 6; ++f) m_device.bindTexture(f, m_device.renderTargetTexture(m_probeFaces[f]));
            m_device.bindVertexBuffer(m_skyVertices);
            m_device.draw(3);
            m_device.endPass();
        }
        m_probeSlots[static_cast<usize>(m_probeConvertSlot)].ready = true;
        m_probeConvertOffsets.clear();
    }
    m_device.prepareTextureForSampling(m_device.renderTargetTexture(m_probeAtlas));
}

void Renderer::gatherProbes(ecs::Registry& reg, detail::LightUniforms& lights) {
    lights.count[2] = 0;
    if (m_probeCaptureMode || !m_probeAtlas) return;
    u32 n = 0;
    for (usize i = 0; i < m_probeSlots.size() && n < detail::kMaxProbes; ++i) {
        const ProbeSlot& s = m_probeSlots[i];
        if (!s.ready || !reg.valid(s.entity) || !reg.has<ReflectionProbeComponent>(s.entity) || !reg.has<WorldTransformComponent>(s.entity)) continue;
        const auto& p = reg.get<ReflectionProbeComponent>(s.entity);
        const Mat4& world = reg.get<WorldTransformComponent>(s.entity).matrix;
        const math::Aabb box = math::Aabb::fromCenterExtents(Vec3{0.0f}, p.size * 0.5f).transformed(world);
        lights.probeMin[n] = Vec4(box.min, std::max(p.blendDistance, 0.0f));
        lights.probeMax[n] = Vec4(box.max, std::max(p.intensity, 0.0f));
        lights.probePos[n] = Vec4(world.translation(), p.boxProjection ? 1.0f : 0.0f);
        lights.probeRow[n] = Vec4(static_cast<float>(i), 0.0f, 0.0f, 0.0f);
        ++n;
    }
    lights.probeAtlas = Vec4(1.0f / static_cast<float>(detail::kProbeLevels), 1.0f / static_cast<float>(detail::kMaxProbes),
                             static_cast<float>(detail::kProbeLevels), 0.0f);
    lights.count[2] = static_cast<i32>(n);
    m_stats.reflectionProbes = n;
}

} // namespace agr
