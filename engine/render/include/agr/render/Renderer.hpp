#pragma once
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/assets/AssetManager.hpp"
#include "agr/math/Geometry.hpp"
#include "agr/render/EnvironmentLibrary.hpp"
#include "agr/render/GraphicsProfile.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/render/Primitives.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/rhi/Rhi.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/scene/Terrain.hpp"

namespace agr {

namespace detail {
struct FrameUniforms;
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
    u32 meshBatches = 0;     ///< 3D draw calls (instanced batches) in the main pass.
    u32 shadowCasters = 0;   ///< Mesh parts drawn into the shadow map (all cascades).
    u32 shadowCascades = 0;  ///< Directional shadow cascades rendered this frame.
    u32 skinnedMeshes = 0;   ///< Meshes deformed by a skeleton this frame.
    u32 terrains = 0;        ///< Terrains drawn (or casting shadows) this frame.
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
    rhi::Device& device() { return m_device; }
    const RenderStats& stats() const { return m_stats; }

    /// Cached texture for a component asset URI (shared between components).
    rhi::TextureHandle resolveTexture(std::string_view uri, bool pixelArt);
    /// Same, with the texture's information (size; state Loading / Failed); null info for uri "".
    rhi::TextureHandle resolveTexture(std::string_view uri, bool pixelArt, const TextureInfo*& info);
    /// Linear (non-sRGB) texture: normal, metallic-roughness and occlusion maps.
    rhi::TextureHandle resolveDataTexture(std::string_view uri);
    /// Imported models and .agrmat materials used by mesh renderers.
    void setModels(ModelLibrary* models) { m_models = models; }
    void setMaterials(MaterialLibrary* materials) { m_materials = materials; }
    /// Sky images (Environment sky = Image). Without it, image skies fall back to the gradient.
    void setEnvironments(EnvironmentLibrary* environments) { m_environments = environments; }
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
    /// Number of distinct texture URIs referenced recently by components.
    usize cachedTextureCount() const { return m_textureCache.size() + m_pixelTextureCache.size() + m_dataTextureCache.size(); }

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
        u32 pipeline = 0;       // 0 = alpha, 1 = lit (2D lights), 2 = additive
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
    u32 materialSlot(u64 key, const MaterialData& material, math::Vec2 uvScale);
    /// Uploads the joint palette of a skinned mesh entity; returns its uniform offset and
    /// the world bounds covering the deformed mesh.
    u32 skinPalette(Scene& scene, ecs::Entity e, const MeshRendererComponent& mr, const ModelAsset& model, const math::Mat4& world,
                    math::Aabb bindBounds, math::Aabb& worldBounds);
    void uploadInstances();
    void drawShadowPass();
    void drawMeshItems(const std::vector<MeshItem>& items, u32 firstInstance, bool shadow, u32 cascade = 0);
    void drawSky();
    void ensureTargets(u32 renderW, u32 renderH, u32 samples, bool needResolve);
    void releaseTargets();
    u32 pushUniforms(const void* data, usize size);
    void uploadUniforms();
    void drawSprites(const std::vector<SpriteItem>& items);
    void drawOverlay();
    // Post-processing (PostProcess component): HDR scene target, bloom chain, composite, FXAA.
    struct PostPass {
        enum Kind : u8 { Prefilter, Down, Up, Composite, Fxaa } kind;
        rhi::RenderTargetHandle source, target;  // target null = output
        u32 uniformOffset = 0;
    };
    Status createPostPipelines();
    bool ensurePostTargets(u32 renderW, u32 renderH, u32 samples, u32 outW, u32 outH, bool hdr, bool bloom, bool fxaa);
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

    rhi::PipelineHandle m_spritePipeline, m_spriteLitPipeline, m_spriteAdditivePipeline, m_overlayPipeline;
    OverlaySource* m_overlaySource = nullptr;
    rhi::PipelineHandle m_postPipelines[5];  // PostPass::Kind
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
    f64 m_time = 0.0;
    static constexpr u32 kWaterPipeline = 7;
    rhi::PipelineHandle m_waterPipeline;
    struct WaterGrid {
        rhi::BufferHandle vertices, indices;
        u32 indexCount = 0;
    };
    std::unordered_map<u32, WaterGrid> m_waterGrids;  // by resolution
    const WaterGrid* waterGrid(u32 resolution);
    TerrainGpu* terrainGpu(ecs::Entity e, TerrainComponent& t);
    void releaseTerrain(TerrainGpu& g);
    rhi::PipelineHandle m_shadowSkinnedPipelines[2];  // [alpha mask]
    /// Joint entities of skinned meshes (resolved by name, revalidated every frame).
    std::unordered_map<u64, std::vector<ecs::Entity>> m_skinJoints;
    std::vector<math::Mat4> m_palette;
    rhi::PipelineHandle m_shadowPipeline, m_shadowMaskPipeline, m_skyPipeline;
    rhi::BufferHandle m_skyVertices;
    rhi::TextureHandle m_flatNormal;        // (0.5, 0.5, 1): no normal map
    ModelLibrary* m_models = nullptr;
    MaterialLibrary* m_materials = nullptr;
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
    u32 m_shadowSize = 0;     // atlas size (texels)
    u32 m_shadowCascades = 0; // cascades this frame
    std::array<math::Vec4, kMaxCascades> m_cascadeViewports{};  // x, y, w, h in the atlas (bottom-left origin)
    bool m_shadowsActive = false;
    bool m_drawSky = false;
    u32 m_frameOffset = 0, m_lights3DOffset = 0;
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
    std::vector<SortKey> m_spriteKeys;
    std::vector<SpriteItem> m_spriteItems;
};

} // namespace agr
