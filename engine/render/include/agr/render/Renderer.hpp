#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/assets/AssetManager.hpp"
#include "agr/math/Geometry.hpp"
#include "agr/render/GraphicsProfile.hpp"
#include "agr/render/Primitives.hpp"
#include "agr/rhi/Rhi.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

struct RenderStats {
    u32 drawCalls = 0;
    u64 triangles = 0;
    u32 meshesVisible = 0;
    u32 meshesCulled = 0;
    u32 spritesVisible = 0;
    u32 spritesCulled = 0;
    u32 spriteBatches = 0;
    u32 renderWidth = 0, renderHeight = 0;  ///< Internal 3D resolution.
    u32 msaaSamples = 1;
    f64 cpuMs = 0.0;         ///< Total CPU time of render().
    f64 collectMs = 0.0;     ///< Scene traversal, culling, sorting, uniform packing (engine code).
    f64 submitMs = 0.0;      ///< Upload + draw submission (mostly graphics driver time).
    f64 gpuTimeMs = -1.0;    ///< -1 when the device has no GPU timers.
    u64 gpuMemoryBytes = 0;  ///< Buffers + textures + render targets.
    bool hasCamera = false;
};

/// Forward renderer (phase 2): lit built-in meshes + batched sprites, frustum
/// culling, render scale and MSAA from the graphics profile.
///
/// Frame structure:
///   1. collect visible meshes / sprites from the scene, cull against the camera frustum
///   2. write frame + per-object uniforms into one buffer (single upload)
///   3. pass: opaque meshes (sorted by mesh then texture), then sprites
///      (sorted by layer, order, depth back-to-front, texture) in as few batches as possible
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

    /// Renders the scene's primary camera into the backbuffer (width x height pixels).
    void render(Scene& scene, u32 width, u32 height);
    const RenderStats& stats() const { return m_stats; }

    /// Cached texture for a component asset URI (shared between components).
    rhi::TextureHandle resolveTexture(const std::string& uri, bool pixelArt);
    /// Number of distinct texture URIs referenced recently by components.
    usize cachedTextureCount() const { return m_textureCache.size() + m_pixelTextureCache.size(); }

private:
    struct GpuMesh {
        rhi::BufferHandle vertices, indices;
        u32 indexCount = 0;
        math::Aabb bounds;
    };
    struct MeshDraw {
        PrimitiveMesh mesh;
        rhi::TextureHandle texture;
        u32 uniformOffset;
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
    };
    struct CachedTexture {
        TextureAsset asset;
        u64 lastUsedFrame = 0;
    };
    struct SortKey {
        i32 layer, order;
        float depth;
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
    Status createMeshes();
    void ensureTargets(u32 renderW, u32 renderH, u32 samples, bool needResolve);
    void releaseTargets();
    u32 pushUniforms(const void* data, usize size);
    void uploadUniforms();
    void drawMeshes(const std::vector<MeshDraw>& draws);
    void drawSprites(const std::vector<SpriteItem>& items);
    void sweepTextureCache();

    rhi::Device& m_device;
    AssetManager& m_assets;
    GraphicsProfile m_profile = graphicsProfile(GraphicsTier::High);
    RenderStats m_stats;
    u64 m_frame = 0;
    bool m_glClipSpace = true;

    rhi::PipelineHandle m_meshPipeline, m_spritePipeline;
    GpuMesh m_meshes[4];

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
    std::vector<SortKey> m_spriteKeys;
    std::vector<MeshDraw> m_meshDraws;
    std::vector<SpriteItem> m_spriteItems;
};

} // namespace agr
