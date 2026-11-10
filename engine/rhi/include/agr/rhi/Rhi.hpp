#pragma once
// AGR Render Hardware Interface.
//
// A thin, backend-neutral GPU API. The renderer only talks to rhi::Device;
// it never sees OpenGL or Vulkan. Design constraints:
//  * every object is a generational handle (stale handles are detected),
//  * uniforms live in uniform buffers bound to numbered slots (std140), and
//    textures are bound to numbered units: this maps 1:1 onto Vulkan
//    descriptor sets and onto GL uniform-block / sampler bindings,
//  * per-draw data is written once per frame into one large uniform buffer and
//    selected with bindUniformBuffer(slot, buffer, offset, size).
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "agr/core/Handle.hpp"
#include "agr/core/Result.hpp"
#include "agr/math/Math.hpp"

namespace agr::rhi {

struct BufferTag;
struct TextureTag;
struct PipelineTag;
struct RenderTargetTag;
using BufferHandle = Handle<BufferTag>;
using TextureHandle = Handle<TextureTag>;
using PipelineHandle = Handle<PipelineTag>;
using RenderTargetHandle = Handle<RenderTargetTag>;

enum class Backend : u8 { OpenGL, OpenGLES, Vulkan };
const char* enumName(Backend backend);

// ------------------------------------------------------------------- buffers
enum class BufferUsage : u8 { Vertex, Index, Uniform };
enum class BufferMemory : u8 {
    Static,   ///< Written once (meshes).
    Dynamic,  ///< Rewritten every frame (sprite batches, per-draw uniforms).
};

struct BufferDesc {
    BufferUsage usage = BufferUsage::Vertex;
    BufferMemory memory = BufferMemory::Static;
    usize size = 0;
    const void* initialData = nullptr;
    const char* debugName = "";
};

// ------------------------------------------------------------------ textures
enum class TextureFormat : u8 {
    RGBA8,      ///< Linear data (normal maps, masks, UI rendered in gamma space).
    RGBA8_sRGB, ///< Color textures: hardware converts to linear when sampling.
    R8,
    RGBA16F,
    Depth24Stencil8,
    Depth16,    ///< Sampleable depth (shadow maps): supported everywhere (Vulkan, GL 3.3, GLES 3.0).
};
inline bool isDepthFormat(TextureFormat f) { return f == TextureFormat::Depth24Stencil8 || f == TextureFormat::Depth16; }
u32 bytesPerPixel(TextureFormat format);

enum class Filter : u8 { Nearest, Linear };
enum class Wrap : u8 { Repeat, Clamp, Mirror };

struct SamplerDesc {
    Filter minFilter = Filter::Linear;
    Filter magFilter = Filter::Linear;
    bool mipmaps = true;  ///< Trilinear when the texture has mip levels.
    Wrap wrapU = Wrap::Repeat;
    Wrap wrapV = Wrap::Repeat;
};

struct TextureDesc {
    u32 width = 1;
    u32 height = 1;
    TextureFormat format = TextureFormat::RGBA8_sRGB;
    bool generateMips = true;
    SamplerDesc sampler;
    const void* data = nullptr;  ///< Tightly packed rows, top row first. May be null.
    const char* debugName = "";
};

// ----------------------------------------------------------------- pipelines
enum class VertexFormat : u8 { Float2, Float3, Float4, UByte4Norm };

struct VertexAttribute {
    u32 location;
    VertexFormat format;
    u32 offset;
};

struct VertexLayout {
    u32 stride = 0;
    std::vector<VertexAttribute> attributes;
};

enum class Topology : u8 { Triangles, Lines };
enum class CullMode : u8 { None, Back, Front };
enum class BlendMode : u8 { Opaque, Alpha, PremultipliedAlpha, Additive };
enum class CompareOp : u8 { Never, Less, LessEqual, Equal, GreaterEqual, Greater, Always };

/// Shaders are written in the AGR GLSL dialect: GLSL 3.30 / GLSL ES 3.00 common
/// subset *without* the #version line. The backend prepends the right header and
/// defines AGR_GLES on mobile. Vertex inputs and fragment outputs must use
/// layout(location = N); uniform blocks are std140.
struct ShaderSource {
    std::string vertex;
    std::string fragment;
};

struct PipelineDesc {
    ShaderSource shader;
    VertexLayout layout;
    /// Optional second vertex stream advanced once per instance (stride 0 = none);
    /// bound with Device::bindInstanceBuffer. Locations must not overlap `layout`.
    VertexLayout instanceLayout;
    Topology topology = Topology::Triangles;
    CullMode cull = CullMode::Back;
    BlendMode blend = BlendMode::Opaque;
    bool depthTest = true;
    bool depthWrite = true;
    CompareOp depthCompare = CompareOp::LessEqual;
    /// false: depth-only pipeline (shadow maps), used with targets that have no color.
    bool colorOutput = true;
    /// uniformBlocks[i] is bound to uniform slot i; samplers[i] to texture unit i.
    std::vector<std::string> uniformBlocks;
    std::vector<std::string> samplers;
    const char* debugName = "";
};

// ------------------------------------------------------------ render targets
struct RenderTargetDesc {
    u32 width = 1;
    u32 height = 1;
    bool color = true;  ///< false = depth-only target (shadow maps).
    TextureFormat colorFormat = TextureFormat::RGBA8;
    bool depth = true;
    /// Depth is a Depth16 texture kept after the pass and readable in shaders
    /// (renderTargetDepthTexture); single-sampled targets only.
    bool sampleableDepth = false;
    u32 samples = 1;  ///< > 1 = MSAA; resolve with blit().
    const char* debugName = "";
};

struct ClearValues {
    bool clearColor = true;
    math::Vec4 color{0, 0, 0, 1};
    bool clearDepth = true;
    float depth = 1.0f;
};

enum class IndexType : u8 { U16, U32 };

// --------------------------------------------------------------- device info
struct DeviceInfo {
    Backend backend = Backend::OpenGL;
    std::string apiVersion;
    std::string renderer;
    std::string vendor;
    u32 maxTextureSize = 2048;
    u32 maxSamples = 1;
    u32 uniformBufferAlignment = 256;
    u32 maxUniformBlockSize = 16384;
    bool gpuTimers = false;        ///< GPU frame time measurement available.
    bool softwareRenderer = false; ///< llvmpipe, SwiftShader... (affects profile selection).
    /// True when render-target textures are stored bottom row first (OpenGL): sample them
    /// with v flipped. False on Vulkan (negative-height viewport stores the top row first).
    bool renderTargetsFlipY = false;
};

struct FrameStats {
    u32 drawCalls = 0;
    u64 triangles = 0;
    u64 vertices = 0;
    u32 pipelineBinds = 0;
    u32 textureBinds = 0;
    u64 uploadedBytes = 0;
    f64 gpuTimeMs = -1.0;  ///< Latest available measurement (a few frames old); -1 = unavailable.
    /// CPU time spent blocked on the GPU or the presentation engine (frame fences,
    /// swapchain acquire / present). Vulkan only: with OpenGL the equivalent blocking
    /// happens inside SwapBuffers (PlatformLayer::present) or arbitrary driver calls.
    f64 cpuWaitMs = 0.0;
};

struct MemoryStats {
    u64 bufferBytes = 0;
    u64 textureBytes = 0;
    u32 buffers = 0;
    u32 textures = 0;
    u32 pipelines = 0;
    u32 renderTargets = 0;
};

/// Graphics device. All calls must happen on the thread owning the context.
class Device {
public:
    virtual ~Device() = default;

    virtual const DeviceInfo& info() const = 0;

    virtual Result<BufferHandle> createBuffer(const BufferDesc& desc) = 0;
    /// Dynamic buffers written from offset 0 are orphaned (no CPU/GPU stall).
    virtual Status updateBuffer(BufferHandle buffer, usize offset, const void* data, usize size) = 0;
    virtual void destroyBuffer(BufferHandle buffer) = 0;

    virtual Result<TextureHandle> createTexture(const TextureDesc& desc) = 0;
    /// Replaces the whole level-0 image (mips are regenerated when enabled).
    virtual Status updateTexture(TextureHandle texture, const void* data) = 0;
    virtual void destroyTexture(TextureHandle texture) = 0;

    /// Fails with the compiler / linker log when a shader is invalid.
    virtual Result<PipelineHandle> createPipeline(const PipelineDesc& desc) = 0;
    virtual void destroyPipeline(PipelineHandle pipeline) = 0;

    virtual Result<RenderTargetHandle> createRenderTarget(const RenderTargetDesc& desc) = 0;
    virtual void destroyRenderTarget(RenderTargetHandle target) = 0;
    /// Color texture of a single-sampled target (sampleable). Null for MSAA targets.
    virtual TextureHandle renderTargetTexture(RenderTargetHandle target) const = 0;
    /// Depth texture of a target created with sampleableDepth (null otherwise). Call
    /// prepareTextureForSampling on it after the pass that wrote it.
    virtual TextureHandle renderTargetDepthTexture(RenderTargetHandle target) const = 0;

    // ------------------------------------------------------- frame commands
    virtual void beginFrame(u32 backbufferWidth, u32 backbufferHeight) = 0;
    virtual void endFrame() = 0;

    /// Null target = backbuffer.
    virtual void beginPass(RenderTargetHandle target, const ClearValues& clear) = 0;
    virtual void endPass() = 0;
    virtual void setViewport(i32 x, i32 y, u32 width, u32 height) = 0;
    virtual void setScissor(bool enabled, i32 x = 0, i32 y = 0, u32 width = 0, u32 height = 0) = 0;

    virtual void bindPipeline(PipelineHandle pipeline) = 0;
    virtual void bindVertexBuffer(BufferHandle buffer, usize offset = 0) = 0;
    virtual void bindIndexBuffer(BufferHandle buffer, IndexType type, usize offset = 0) = 0;
    /// Per-instance vertex stream of pipelines with an instanceLayout.
    virtual void bindInstanceBuffer(BufferHandle buffer, usize offset = 0) = 0;
    virtual void bindUniformBuffer(u32 slot, BufferHandle buffer, usize offset, usize size) = 0;
    virtual void bindTexture(u32 unit, TextureHandle texture) = 0;
    /// Must be called outside a pass before sampling a render-target texture in a later
    /// pass (Vulkan layout transition; no-op on OpenGL).
    virtual void prepareTextureForSampling(TextureHandle texture) { (void)texture; }

    virtual void draw(u32 vertexCount, u32 firstVertex = 0) = 0;
    virtual void drawIndexed(u32 indexCount, u32 firstIndex = 0, i32 baseVertex = 0) = 0;
    /// Instance i reads the instance stream at offset + i * instanceLayout.stride.
    virtual void drawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 firstIndex = 0, i32 baseVertex = 0) = 0;

    /// Copies (and resolves MSAA / rescales) `src` into `dst` (null = backbuffer).
    virtual void blit(RenderTargetHandle src, RenderTargetHandle dst, Filter filter) = 0;
    /// Reads RGBA8 pixels of the backbuffer, top row first. For screenshots and tests.
    virtual Status readBackbuffer(u32 x, u32 y, u32 width, u32 height, std::vector<u8>& outRgba) = 0;

    virtual const FrameStats& frameStats() const = 0;
    virtual MemoryStats memoryStats() const = 0;
    /// Errors reported by API validation (Vulkan validation layers when enabled). 0 otherwise.
    virtual u64 validationErrorCount() const { return 0; }
};

/// Resolves OpenGL entry points (SDL_GL_GetProcAddress, eglGetProcAddress...).
using GLProcLoader = std::function<void*(const char* name)>;

/// Creates the OpenGL 3.3 core / OpenGL ES 3.0 backend on the current context.
Result<std::unique_ptr<Device>> createOpenGLDevice(const GLProcLoader& loader, bool gles);

/// What the platform layer provides to create a Vulkan device (no Vulkan types
/// leak into this header: handles are passed as void* / u64).
struct VulkanPlatform {
    std::string appName = "AGR Application";
    std::vector<std::string> instanceExtensions;  ///< Required by the window system (surface).
    void* getInstanceProcAddr = nullptr;          ///< PFN_vkGetInstanceProcAddr of the loader.
    /// Creates the window surface for a VkInstance; returns a VkSurfaceKHR (0 on failure).
    std::function<u64(void* instance)> createSurface;
    bool vsync = true;
    bool validation = false;  ///< Enables VK_LAYER_KHRONOS_validation when installed.
};

/// True when the engine was built with the Vulkan backend.
bool vulkanBackendCompiled();
/// Creates the Vulkan 1.1 backend (fails cleanly when no suitable GPU / driver exists).
Result<std::unique_ptr<Device>> createVulkanDevice(const VulkanPlatform& platform);

} // namespace agr::rhi
