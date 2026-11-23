#pragma once
#include <array>
#include <memory>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/assets/AssetManager.hpp"
#include "agr/math/Geometry.hpp"
#include "agr/render/EnvironmentLibrary.hpp"
#include "agr/render/OcclusionBuffer.hpp"
#include "agr/render/ParticleCurves.hpp"
#include "agr/render/GraphicsProfile.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/render/Primitives.hpp"
#include "agr/render/ShaderLibrary.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/rhi/Rhi.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/scene/Terrain.hpp"

namespace agr {

namespace detail {
struct FrameUniforms;
struct LightUniforms;
}

/// A camera independent of the scene (editor viewports, previews, split screen).
struct RenderView {
    math::Mat4 view = math::Mat4::identity();  ///< World -> view space (camera looks down -Z).
    math::Vec3 position{0.0f};
    Projection projection = Projection::Perspective;
    float fieldOfView = math::radians(60.0f);
    float orthographicSize = 5.0f;
    float nearPlane = 0.1f;
    float farPlane = 500.0f;
    math::Color clearColor{0.055f, 0.063f, 0.075f, 1.0f};
    ScaleMode scaleMode = ScaleMode::FixedHeight;
    float referenceAspect = 16.0f / 9.0f;
    bool editorView = false;  ///< Skips entities flagged hiddenInEditor.
    /// Renders directly into the target: no render scale, no MSAA (thumbnails, previews;
    /// avoids reallocating the off-screen targets shared with the main view).
    bool direct = false;
    /// Draws the overlay (game UI) on top of the scene when an overlay source is set.
    bool overlay = true;

    /// Projection matrix for an aspect ratio, in the clip space of the given backend.
    math::Mat4 projectionMatrix(float aspect, bool glClipSpace) const;
    /// View of the scene's first active primary camera, if any.
    static std::optional<RenderView> fromPrimaryCamera(Scene& scene);
};

/// Screen-space geometry drawn on top of the scene (game UI), in pixels of the
/// output with the origin at the top-left corner. Same vertex layout as sprites.
struct OverlayVertex {
    math::Vec3 position;  ///< x, y in pixels; z unused (0).
    math::Vec2 uv;
    u32 color;            ///< RGBA8, sRGB.
};
struct OverlayCommand {
    rhi::TextureHandle texture;  ///< Null = white.
    u32 firstIndex = 0;
    u32 indexCount = 0;
};
struct OverlayDrawList {
    std::vector<OverlayVertex> vertices;
    std::vector<u32> indices;
    std::vector<OverlayCommand> commands;

    void clear() {
        vertices.clear();
        indices.clear();
        commands.clear();
    }
    bool empty() const { return indices.empty(); }
    /// Axis-aligned quad from `min` to `max` (pixels) with texture coordinates `uv` (u0, v0, u1, v1).
    void quad(math::Vec2 min, math::Vec2 max, math::Vec4 uv, u32 color, rhi::TextureHandle texture);
    /// Any convex quad (corners in order), one color per corner (gradients, rotated elements).
    void quad4(const math::Vec2 corners[4], const math::Vec2 uvs[4], const u32 colors[4], rhi::TextureHandle texture);
};
/// Produces the overlay of a view (implemented by the UI module).
class OverlaySource {
public:
    virtual ~OverlaySource() = default;
    /// Called once per rendered view, before any pass of the frame (texture uploads are allowed).
    virtual void buildOverlay(Scene& scene, u32 width, u32 height, OverlayDrawList& out) = 0;
};

/// Packs an RGBA color (0..1, sRGB) into the RGBA8 vertex format of sprites and overlays.
u32 packColor(math::Vec4 color);

struct RenderStats {
    u32 drawCalls = 0;
    u64 triangles = 0;
    u32 meshesVisible = 0;
    u32 meshesCulled = 0;
    u32 spritesVisible = 0;
    u32 spritesCulled = 0;
    u32 spriteBatches = 0;
    u32 tilesVisible = 0;
    u32 particlesVisible = 0;
    u32 overlayQuads = 0;    ///< Game UI quads (overlay).
    u32 lights2D = 0;
    u32 lights = 0;          ///< Point / spot lights shading this frame.
    u32 decals = 0;          ///< Decals projected this frame (<= 16).
    u32 reflectionProbes = 0;  ///< Probes with a finished capture, used by this frame's shading.
    bool probeCapture = false; ///< A probe face was captured this frame.
    u32 meshBatches = 0;     ///< 3D draw calls (instanced batches) in the main pass.
    u32 outlineBatches = 0;  ///< Outline draw calls (instanced batches of outlined materials).
    bool ambientOcclusion = false;  ///< SSAO computed this frame.
    u32 shadowCasters = 0;   ///< Mesh parts drawn into the shadow map (all cascades).
    u32 shadowCascades = 0;  ///< Directional shadow cascades rendered this frame.
    u32 spotShadows = 0;     ///< Spot lights with a shadow map this frame (at most 2).
    u32 pointShadows = 0;    ///< Point lights with cube shadows this frame (at most 1).
    u32 lodMeshes = 0;       ///< Meshes drawn with a simplified level of detail.
    u32 shadowedLights2D = 0;  ///< 2D lights with shadows this frame.
    u32 occluderEdges2D = 0;   ///< Occluder edges tested by them.
    u32 rangeCulled = 0;     ///< Meshes hidden by their visibility range.
    u32 skins2D = 0;              ///< Bone-deformed 2D skins drawn.
    u32 trails = 0;               ///< Trail ribbons drawn.
    u32 normalMappedSprites = 0;  ///< Lit sprites drawn with a normal map.
    bool atmosphere = false;      ///< Height fog / light shafts pass this frame.
    bool depthOfField = false;    ///< Depth of field pass this frame.
    bool screenSpaceReflections = false;  ///< SSR pass this frame.
    u32 morphedMeshes = 0;        ///< Meshes drawn with blended morph targets.
    bool temporalAA = false;      ///< TAA pass this frame.
    u32 shadedSprites = 0;        ///< Sprites drawn with a sprite shader.
    bool motionBlur = false;      ///< Motion blur pass this frame.
    bool autoExposure = false;    ///< Auto exposure measured this frame.
    u32 occluders = 0;       ///< Occluder shapes rasterized this frame.
    u32 occlusionCulled = 0; ///< Meshes in the view hidden behind occluders.
    u32 skinnedMeshes = 0;   ///< Meshes deformed by a skeleton this frame.
    u32 terrains = 0;        ///< Terrains drawn (or casting shadows) this frame.
    u32 blobShadows = 0;     ///< Blob shadows drawn this frame.
    u32 waterSurfaces = 0;
    bool postProcessing = false;  ///< PostProcess active this frame.
    bool hdr = false;             ///< The frame was shaded into an HDR (half float) target.
    bool imageSky = false;        ///< An image sky lit and filled the background.
    bool environmentLoading = false;  ///< The Environment's sky image is still loading.
    u32 bloomLevels = 0;
    u32 modelsLoading = 0;   ///< Mesh renderers waiting for their model.
    u32 renderWidth = 0, renderHeight = 0;  ///< Internal 3D resolution.
    u32 msaaSamples = 1;
    f64 cpuMs = 0.0;         ///< Total CPU time of render().
    f64 collectMs = 0.0;     ///< Scene traversal, culling, sorting, uniform packing (engine code).
    f64 submitMs = 0.0;      ///< Upload + draw submission (mostly graphics driver time), includes gpuWaitMs.
    f64 gpuWaitMs = 0.0;     ///< Part of submitMs spent blocked on the GPU (Vulkan only).
    f64 frameWaitMs = 0.0;   ///< render() only: CPU time blocked on the GPU or presentation over the whole
                             ///< device frame (beginFrame .. endFrame). In OpenGL the wait is in SwapBuffers (not measured).
    f64 gpuTimeMs = -1.0;    ///< -1 when the device has no GPU timers.
    u64 gpuMemoryBytes = 0;  ///< Buffers + textures + render targets.
    bool hasCamera = false;
};

/// Forward renderer: PBR meshes (built-in and imported) with directional, point and
/// spot lights, a directional shadow map, procedural sky, fog and ACES tone mapping,
/// plus batched 2D sprites, tilemaps and particles. Render scale, MSAA, shadow
/// quality and light count come from the graphics profile.
///
/// Frame structure:
///   1. collect visible mesh parts (camera frustum) and shadow casters (light frustum),
///      sprites, lights; one uniform buffer upload and one instance buffer upload
///   2. shadow pass (depth only) when a directional light casts shadows
///   3. main pass: opaque and alpha-tested meshes sorted by pipeline / material / mesh and
///      merged into instanced draws, sky, transparent meshes back to front, sprites
///   4. MSAA resolve and/or upscale to the backbuffer when rendering off-screen
class Renderer {
public:
    Renderer(rhi::Device& device, AssetManager& assets);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Status initialize();
    void setProfile(const GraphicsProfile& profile) { m_profile = profile; }
    const GraphicsProfile& profile() const { return m_profile; }

    /// Complete frame: renders the scene's primary camera into the backbuffer
    /// (begins and ends the device frame).
    void render(Scene& scene, u32 width, u32 height);
    /// Renders `view` into `target` (null = backbuffer) of width x height pixels.
    /// Must be called between Device::beginFrame and Device::endFrame; a render target
    /// must have a depth buffer. One view per device frame per Renderer (its frame data
    /// lives in shared dynamic buffers): create another Renderer for extra views.
    void renderView(Scene& scene, const RenderView& view, rhi::RenderTargetHandle target, u32 width, u32 height);
    bool glClipSpace() const { return m_glClipSpace; }
    /// Occlusion culling with the scene's Occluder components (on by default).
    void setOcclusionCulling(bool enabled) { m_occlusionCulling = enabled; }
    bool occlusionCulling() const { return m_occlusionCulling; }
    /// The occlusion depth buffer of the last frame (debug views, tests).
    const OcclusionBuffer& occlusionBuffer() const { return m_occlusion; }
    rhi::Device& device() { return m_device; }
    const RenderStats& stats() const { return m_stats; }

    /// Cached texture for a component asset URI (shared between components).
    rhi::TextureHandle resolveTexture(std::string_view uri, bool pixelArt);
    /// Same, with the texture's information (size; state Loading / Failed); null info for uri "".
    rhi::TextureHandle resolveTexture(std::string_view uri, bool pixelArt, const TextureInfo*& info);
    /// Linear (non-sRGB) texture: normal, metallic-roughness and occlusion maps.
    rhi::TextureHandle resolveDataTexture(std::string_view uri, bool pixelArt = false);
    /// Imported models and .agrmat materials used by mesh renderers.
    void setModels(ModelLibrary* models) { m_models = models; }
    void setMaterials(MaterialLibrary* materials) { m_materials = materials; }
    void setParticleCurves(ParticleCurvesLibrary* curves) { m_particleCurves = curves; }
    /// Custom surface shaders (`.agrshader`) referenced by materials.
    void setShaders(ShaderLibrary* shaders) { m_shaders = shaders; }
    struct ShaderStatus {
        bool usable = false;                       ///< Parsed and compiled: materials use it.
        std::vector<ShaderDiagnostic> diagnostics;  ///< Parse and compile errors (user line numbers).
    };
    /// Compiles the standard variant of a custom shader if needed (opaque, one-sided,
    /// with its vertex hook in the shadow pass) and returns its state. Editor validation.
    ShaderStatus shaderStatus(const std::string& uri);
    /// Sky images (Environment sky = Image). Without it, image skies fall back to the gradient.
    void setEnvironments(EnvironmentLibrary* environments) { m_environments = environments; }
    /// Re-captures every reflection probe (after static geometry changed; moved probes and
    /// probes in Always mode are re-captured on their own).
    void invalidateReflectionProbes() { ++m_probeEpoch; }
    /// Atlas of the probe captures (debug views): rows = probes, columns = roughness levels.
    rhi::RenderTargetHandle reflectionProbeAtlas() const { return m_probeAtlas; }
    /// Depth texture of the last shadow pass (null when shadows are off): an atlas of the
    /// cascades, 2 x 2 tiles when there is more than one. Debug views.
    rhi::TextureHandle shadowMap() const;
    /// Game UI drawn over every view with RenderView::overlay (null = none).
    void setOverlaySource(OverlaySource* source) { m_overlaySource = source; }
    /// Clock of animated materials (water), in seconds; the render plugin advances it.
    void setTime(f64 seconds) { m_time = seconds; }
    OverlaySource* overlaySource() const { return m_overlaySource; }
    /// Sprite sheets used by sprites (sheet + frame) and tilemaps.
    void setSpriteSheets(SpriteSheetLibrary* sheets) { m_sheets = sheets; }
    static constexpr u32 kMaxLights2D = 16;
    static constexpr u32 kShadowRows2D = 8, kShadowBins2D = 512;  // polar shadow maps of 2D lights
    rhi::TextureHandle m_shadow2D;
    std::vector<u8> m_shadow2DPixels;
    std::vector<float> m_shadow2DBins;
    /// Number of distinct texture URIs referenced recently by components.
    usize cachedTextureCount() const { return m_textureCache.size() + m_pixelTextureCache.size() + m_dataTextureCache.size() + m_pixelDataTextureCache.size(); }

private:
    struct GpuMesh {
        rhi::BufferHandle vertices, indices;
        u32 indexCount = 0;
        math::Aabb bounds;
    };
    /// Material resolved for this frame: uniforms, textures, pipeline.
    struct FrameMaterial {
        u32 uniformOffset = 0;
        rhi::TextureHandle textures[5];  // base color, metallic-roughness, normal, occlusion, emissive
        u32 pipeline = 0;                // alphaMode * 2 + doubleSided, or kTerrainPipeline
        rhi::TextureHandle splat;        // terrain layer weights
        rhi::TextureHandle custom[kMaxShaderTextures];  // custom shader textures (units 7..)
        u32 customCount = 0;
        bool outline = false;            // material draws an outline (inverted hull pass)
    };
    /// Per-instance vertex stream of the 3D pipelines (locations 4..11).
    struct InstanceData {
        math::Mat4 model;
        math::Vec4 normal[3];  ///< Columns of the normal matrix (xyz).
        math::Vec4 color;      ///< Linear tint.
    };
    static_assert(sizeof(InstanceData) == 128);
    /// One submesh of one entity.
    struct MeshItem {
        rhi::BufferHandle vertices, indices;
        u32 firstIndex = 0, indexCount = 0;
        u32 material = 0;  // index into m_frameMaterials
        u32 pipeline = 0;
        u32 instance = 0;  // index into m_instances
        float depth = 0.0f;
        u32 skinOffset = ~0u;  // joint palette in the uniform buffer (skinned meshes only)
    };
    struct SpriteVertex {
        math::Vec3 position;
        math::Vec2 uv;
        u32 color;  ///< RGBA8, sRGB
    };
    struct SpriteItem {
        i32 layer, order;
        float depth;
        rhi::TextureHandle texture;
        math::Vec3 corners[4];  // bl, br, tr, tl
        math::Vec4 uv;          // u0, v0, u1, v1
        u32 color;
        u32 pipeline = 0;       // 0 = alpha, 1 = lit (2D lights), 2 = additive, 3 = lit with a normal map
        rhi::TextureHandle normalMap{};
    };
    struct CachedTexture {
        TextureAsset asset;
        u64 lastUsedFrame = 0;
    };
    struct SortKey {
        i32 layer, order;
        float depth;
        u32 pipeline;
        u32 texture;
        u32 index;
    };
    /// Heterogeneous lookup: find by std::string_view without allocating.
    struct StringHash {
        using is_transparent = void;
        usize operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
    };
    using TextureCache = std::unordered_map<std::string, CachedTexture, StringHash, std::equal_to<>>;

    Status createPipelines();
    Status create3DPipelines();
    Status createMeshes();
    /// 3D part of a frame (Renderer3D.cpp).
    void collect3D(Scene& scene, const RenderView& rv, const math::Mat4& view, const math::Mat4& viewProj, const math::Frustum& frustum,
                   float aspect, const std::function<bool(ecs::Entity)>& hidden, detail::FrameUniforms& frame);
    /// `radialOutline`: the outline extrudes from the object center (built-in meshes).
    u32 materialSlot(u64 key, const MaterialData& material, math::Vec2 uvScale, bool radialOutline = false);
    /// Uploads the joint palette of a skinned mesh entity; returns its uniform offset and
    /// the world bounds covering the deformed mesh.
    u32 skinPalette(Scene& scene, ecs::Entity e, const MeshRendererComponent& mr, const ModelAsset& model, const math::Mat4& world,
                    math::Aabb bindBounds, math::Aabb& worldBounds);
    void uploadInstances();
    void gatherDecals(ecs::Registry& reg, const math::Frustum& frustum, math::Vec3 camPos, detail::LightUniforms& lights);
    void drawShadowPass();
    void drawMeshItems(const std::vector<MeshItem>& items, u32 firstInstance, bool shadow, u32 cascade = 0);
    void drawSky();
    void drawOutlines(const std::vector<MeshItem>& items, u32 firstInstance);
    void ensureTargets(u32 renderW, u32 renderH, u32 samples, bool needResolve);
    void releaseTargets();
    u32 pushUniforms(const void* data, usize size);
    void uploadUniforms();
    void drawSprites(const std::vector<SpriteItem>& items);
    void drawGpuParticles();
    struct GpuParticleDraw {
        u32 uniformOffset = 0;
        u32 count = 0;
        rhi::TextureHandle texture;
        bool additive = false;
    };
    std::vector<GpuParticleDraw> m_gpuParticleDraws;
    rhi::PipelineHandle m_gpuParticlePipeline, m_gpuParticleAdditivePipeline;
    rhi::BufferHandle m_gpuParticleVertices, m_gpuParticleIndices;
    void drawOverlay();
    // Post-processing (PostProcess component): HDR scene target, bloom chain, composite, FXAA.
    struct PostPass {
        enum Kind : u8 { Prefilter, Down, Up, Composite, Fxaa, Ao, AoBlur, Atmosphere, Dof, Luminance, Ssr, Taa, MotionBlur } kind;
        rhi::RenderTargetHandle source, target;  // target null = output
        u32 uniformOffset = 0;
    };
    Status createPostPipelines();
    bool ensurePostTargets(u32 renderW, u32 renderH, u32 samples, u32 outW, u32 outH, bool hdr, bool bloom, bool fxaa, bool ao, bool atmos);
    void releasePostTargets();
    void runPost(rhi::RenderTargetHandle output);
    void sweepTextureCache();

    rhi::Device& m_device;
    AssetManager& m_assets;
    GraphicsProfile m_profile = graphicsProfile(GraphicsTier::High);
    RenderStats m_stats;
    u64 m_frame = 0;
    u64 m_lastViewFrame = 0;
    bool m_warnedSecondView = false;
    bool m_glClipSpace = true;
    OcclusionBuffer m_occlusion;
    std::vector<math::Vec3> m_skinPoints;
    std::vector<math::Vec3> m_trailScratch;  ///< Scratch: points of the 2D skin being collected.
    bool m_occlusionCulling = true;

    rhi::PipelineHandle m_spritePipeline, m_spriteLitPipeline, m_spriteLitNormalPipeline, m_spriteAdditivePipeline, m_overlayPipeline;
    OverlaySource* m_overlaySource = nullptr;
    rhi::PipelineHandle m_postPipelines[13];  // PostPass::Kind
    static constexpr usize kPostUniformBytes = 33 * 16 + 4 * 64;  // PostData block (Shaders.inl)
    rhi::RenderTargetHandle m_dofTarget;                 // scene color with depth of field
    rhi::RenderTargetHandle m_ssrTarget;                 // scene color with screen-space reflections
    rhi::RenderTargetHandle m_motionTarget;               // scene color with motion blur
    bool m_prevViewProjValid = false;
    rhi::RenderTargetHandle m_taaTargets[2];             // temporal AA output / history (ping-pong)
    u32 m_taaIndex = 0;
    bool m_taaValid = false;
    math::Mat4 m_prevViewProj;                           // unjittered, last frame (TAA reprojection)
    rhi::RenderTargetHandle m_lumTargets[2];             // 1 x 1 adapted log luminance (ping-pong)
    u32 m_lumIndex = 0;                                  // target written this frame
    bool m_lumValid = false;                             // the other target holds last frame's value
    f64 m_lastPostClock = -1.0;
    bool m_postReadDepth = false;                        // this frame needs a sampleable post scene depth
    bool m_postSceneReadable = false;                    // the post scene target has one
    rhi::RenderTargetHandle m_aoTarget, m_aoBlurTarget;  // half resolution
    bool m_postAo = false;
    rhi::RenderTargetHandle m_atmosTarget;  // scene color with fog and light shafts
    bool m_postAtmos = false;
    rhi::RenderTargetHandle m_postScene, m_postResolve, m_postLdr;
    std::vector<rhi::RenderTargetHandle> m_bloomLevels;
    std::vector<std::pair<u32, u32>> m_bloomSizes;
    u32 m_postW = 0, m_postH = 0, m_postSamples = 0, m_postOutW = 0, m_postOutH = 0;
    bool m_postHdr = false, m_postBloom = false, m_postFxaa = false;
    std::vector<PostPass> m_postPasses;
    OverlayDrawList m_overlay;
    rhi::BufferHandle m_overlayVertices, m_overlayIndices;
    usize m_overlayVertexCapacity = 0, m_overlayIndexCapacity = 0;
    u32 m_overlayFrameOffset = 0;
    rhi::PipelineHandle m_pbrPipelines[6];  // [alphaMode * 2 + doubleSided]
    rhi::PipelineHandle m_pbrSkinnedPipelines[6];
    static constexpr u32 kTerrainPipeline = 6;
    rhi::PipelineHandle m_terrainPipeline;
    struct TerrainGpu {
        rhi::BufferHandle vertices, indices;
        u32 indexCount = 0;
        rhi::TextureHandle splat;
        u32 splatSize = 0;
        u64 heightsHash = 0, splatHash = 0;
        u64 lastFrame = 0;
    };
    std::unordered_map<u64, TerrainGpu> m_terrains;  // by entity
    struct DynamicMeshGpu {  // DynamicMeshComponent (soft bodies, procedural meshes)
        rhi::BufferHandle vertices, indices;
        usize vertexCount = 0, indexCount = 0;
        u32 version = ~0u;
        u64 lastFrame = 0;
    };
    std::unordered_map<u64, DynamicMeshGpu> m_dynamicMeshes;  // by entity
    std::vector<ModelVertex> m_dynamicScratch;
    /// Vertex buffer with an entity's morph targets blended in (rewritten each frame it is drawn).
    struct MorphGpu {
        rhi::BufferHandle buffer;
        usize bytes = 0;
        u64 lastFrame = 0;
    };
    std::unordered_map<u64, MorphGpu> m_morphs;  // by entity
    std::vector<u8> m_morphScratch;
    rhi::BufferHandle morphedVertices(ecs::Entity e, const ModelAsset::Mesh& mesh, const float weights[8]);
    f64 m_time = 0.0;
    static constexpr u32 kWaterPipeline = 7;
    rhi::PipelineHandle m_waterPipeline;
    static constexpr u32 kBlobPipeline = 8;
    rhi::PipelineHandle m_blobPipeline;
    struct WaterGrid {
        rhi::BufferHandle vertices, indices;
        u32 indexCount = 0;
    };
    std::unordered_map<u32, WaterGrid> m_waterGrids;  // by resolution
    const WaterGrid* waterGrid(u32 resolution);
    TerrainGpu* terrainGpu(ecs::Entity e, TerrainComponent& t);
    void releaseTerrain(TerrainGpu& g);
    rhi::PipelineHandle m_shadowSkinnedPipelines[2];  // [alpha mask]
    rhi::PipelineHandle m_outlinePipelines[2];        // [skinned]
    /// Joint entities of skinned meshes (resolved by name, revalidated every frame).
    std::unordered_map<u64, std::vector<ecs::Entity>> m_skinJoints;
    std::vector<math::Mat4> m_palette;
    rhi::PipelineHandle m_shadowPipeline, m_shadowMaskPipeline, m_skyPipeline;
    rhi::BufferHandle m_skyVertices;
    rhi::TextureHandle m_flatNormal;        // (0.5, 0.5, 1): no normal map
    ModelLibrary* m_models = nullptr;
    MaterialLibrary* m_materials = nullptr;
    ParticleCurvesLibrary* m_particleCurves = nullptr;
    ShaderLibrary* m_shaders = nullptr;
    /// Sprite shaders: pipeline per .agrshader (rebuilt when the file changes); items use
    /// pipeline id 4 + index in m_spriteShaderPipelines.
    struct SpriteShader {
        u32 index = 0;
        u64 version = 0;
        bool ok = false;
    };
    std::unordered_map<std::string, SpriteShader> m_spriteShaders;
    std::vector<rhi::PipelineHandle> m_spriteShaderPipelines;
    i32 spriteShaderPipeline(const std::string& uri);  ///< -1: standard drawing
    // Custom shaders: pipeline ids kCustomPipelineBase + set * kCustomStride + variant
    // (alphaMode * 2 + doubleSided); compiled on first use, rebuilt when the source changes.
    static constexpr u32 kCustomPipelineBase = 1000, kCustomStride = 8;
    struct CustomPipelines {
        std::string uri;
        u64 version = 0;
        bool usable = false;
        bool hasVertex = false;
        u32 textureCount = 0;
        rhi::PipelineHandle pbr[2][6];     // [skinned][variant]
        bool tried[2][6]{};
        rhi::PipelineHandle shadow[2][2];  // [skinned][alpha mask] (vertex hook only)
        bool shadowTried[2][2]{};
        std::vector<ShaderDiagnostic> diagnostics;
    };
    std::vector<CustomPipelines> m_customSets;
    std::unordered_map<std::string, u32> m_customIndex;
    /// Index of the shader's pipeline set, ~0u when it cannot be used (missing, errors).
    u32 customShaderSet(const std::string& uri);
    /// Cached or newly compiled pipeline; null on failure (the set becomes unusable).
    rhi::PipelineHandle customPipeline(u32 set, u32 variant, bool skinned, bool shadow);
    void destroyCustomPipelines(CustomPipelines& set);
    /// Shadow pass grouping: standard casters (0 / 1 = alpha mask) or a custom vertex hook.
    u32 shadowKey(u32 pipeline) const;
    EnvironmentLibrary* m_environments = nullptr;
    rhi::TextureHandle m_envTexture;  // image sky of this frame (null: none)
    rhi::TextureHandle m_softParticle;  // default particle texture (procedural)
    SpriteSheetLibrary* m_sheets = nullptr;
    std::unordered_map<u32, math::Vec3> m_parallax;  // entity index -> world offset
    std::vector<math::Vec4> m_tileUvs;
    u32 m_lightsOffset = 0;
    GpuMesh m_meshes[4];
    // 3D frame data
    static constexpr u32 kMaxCascades = 4;
    std::vector<MeshItem> m_opaqueItems, m_transparentItems;
    std::array<std::vector<MeshItem>, kMaxCascades> m_shadowItems;  // casters per cascade
    std::vector<InstanceData> m_instances, m_instanceStaging;
    std::vector<FrameMaterial> m_frameMaterials;
    std::unordered_map<u64, u32> m_materialSlots;
    rhi::BufferHandle m_instanceBuffer;
    usize m_instanceCapacity = 0;
    rhi::RenderTargetHandle m_shadowTarget;
    // Local light shadows: one atlas of 4 x 2 square tiles; tiles 0-1 for the shadowed spot lights,
    // tiles 2-7 for the 6 cube faces of the shadowed point light.
    static constexpr u32 kSpotShadowSlots = 2;
    rhi::RenderTargetHandle m_spotShadowTarget;
    u32 m_spotShadowSize = 0;  // tile size
    u32 m_spotShadowCount = 0;
    bool m_pointShadow = false;
    ecs::Entity m_pointShadowEntity{};
    std::array<math::Mat4, 6> m_pointShadowMatrix{};
    std::array<math::Vec4, 6> m_pointShadowRect{};
    std::array<math::Vec4, kSpotShadowSlots> m_spotShadowRect{};
    std::array<std::vector<MeshItem>, 6> m_pointShadowItems;
    std::array<u32, 6> m_pointFrameOffsets{};
    std::array<ecs::Entity, kSpotShadowSlots> m_spotShadowEntities{};
    std::array<math::Mat4, kSpotShadowSlots> m_spotShadowMatrix{};
    std::array<std::vector<MeshItem>, kSpotShadowSlots> m_spotShadowItems;
    std::array<u32, kSpotShadowSlots> m_spotFrameOffsets{};
    u32 m_shadowSize = 0;     // atlas size (texels)
    u32 m_shadowCascades = 0; // cascades this frame
    std::array<math::Vec4, kMaxCascades> m_cascadeViewports{};  // x, y, w, h in the atlas (bottom-left origin)
    bool m_shadowsActive = false;
    bool m_drawSky = false;
    u32 m_frameOffset = 0, m_lights3DOffset = 0;
    std::array<rhi::TextureHandle, 3> m_decalTextures{};  ///< Decal images of this frame (units 12..14).
    // Reflection probes (ReflectionProbes.cpp): a second renderer captures one face per frame
    // into m_probeFaces; when 6 are done, a GPU pass prefilters them into the probe's atlas row.
    struct ProbeSlot {
        ecs::Entity entity;
        u64 hash = 0;
        u32 facesDone = 6;  ///< 0..5: capture in progress; 6: idle.
        bool ready = false;
        u64 seenFrame = 0;
    };
    std::unique_ptr<Renderer> m_probeRenderer;
    bool m_probeCaptureMode = false;  ///< This renderer captures probe faces (linear output, no probes).
    bool m_probeLinear = false;       ///< Faces and atlas in RGBA16F (else display-encoded RGBA8).
    std::array<rhi::RenderTargetHandle, 6> m_probeFaces{};
    rhi::RenderTargetHandle m_probeAtlas;
    rhi::PipelineHandle m_probeConvertPipeline;
    std::vector<ProbeSlot> m_probeSlots;
    std::vector<u32> m_probeConvertOffsets;  ///< Uniforms of this frame's conversion passes (one per level).
    i32 m_probeConvertSlot = -1;
    u64 m_probeEpoch = 0;
    float m_probeExposure = 1.0f;
    void updateReflectionProbes(Scene& scene);
    void pushProbeConversion();
    void runProbeConversion();
    void gatherProbes(ecs::Registry& reg, detail::LightUniforms& lights);
    void releaseProbes();
    std::array<u32, kMaxCascades> m_shadowFrameOffsets{};

    rhi::BufferHandle m_uniformBuffer;
    usize m_uniformCapacity = 0;
    std::vector<u8> m_uniformStaging;
    u32 m_uniformAlign = 256;

    rhi::BufferHandle m_spriteVertices, m_spriteIndices;
    u32 m_spriteCapacity = 0;
    std::vector<SpriteVertex> m_spriteStaging;

    rhi::RenderTargetHandle m_sceneTarget, m_resolveTarget;
    u32 m_targetW = 0, m_targetH = 0, m_targetSamples = 0;

    TextureCache m_textureCache;       ///< Linear filtering.
    TextureCache m_pixelTextureCache;  ///< pixelArt = true (nearest filtering).
    TextureCache m_dataTextureCache;   ///< Linear data (normal / metallic-roughness / occlusion maps).
    TextureCache m_pixelDataTextureCache;  ///< Linear data with nearest filtering (normal maps of pixel art).
    std::vector<SortKey> m_spriteKeys;
    std::vector<SpriteItem> m_spriteItems;
};

} // namespace agr
