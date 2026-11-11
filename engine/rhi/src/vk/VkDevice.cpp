// Vulkan 1.1 implementation of rhi::Device.
//
// Design (see docs/RENDERING.md, "Backend Vulkan"):
//  * Classic render passes (Vulkan 1.1 on Android rarely has dynamic rendering);
//    pipelines are created lazily per render-pass signature (color format,
//    depth format, sample count) and cached.
//  * Negative-height viewport: clip space, winding and texture orientation match
//    the OpenGL backend, so the renderer is backend-agnostic.
//  * The "backbuffer" is an engine-owned image blitted to the swapchain at
//    endFrame(): stable readback for screenshots / tests and free format
//    conversion (BGRA swapchains). Cost: one blit per frame (to be skipped later
//    when no readback is needed).
//  * Dynamic buffers keep one copy per frame in flight (ring); static resources
//    are uploaded through staging buffers.
//  * One descriptor set layout for every pipeline: 8 dynamic uniform buffers +
//    8 combined image samplers; sets are cached while bindings do not change.
//  * Destruction is deferred until the GPU finished the frames that used it.
#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <unordered_map>

#include "VkCommon.hpp"
#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/rhi/ShaderDialect.hpp"

namespace agr::rhi {

bool vulkanBackendCompiled() { return true; }

namespace vk {

const char* resultName(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        default: return "VkResult(other)";
    }
}

namespace {

constexpr u32 kFrames = 2;
constexpr u32 kSetsPerPool = 512;

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc = nullptr;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    u32 width = 0, height = 0, mips = 1, samples = 1;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct VkBufferRes {
    std::array<VkBuffer, kFrames> buffer{};
    std::array<VmaAllocation, kFrames> alloc{};
    std::array<void*, kFrames> mapped{};
    std::array<u64, kFrames> lastUsedFrame{};
    u32 copies = 1;
    u32 active = 0;
    u64 lastWriteFrame = ~0ull;
    usize size = 0;
    BufferUsage usage = BufferUsage::Vertex;
    BufferMemory memory = BufferMemory::Static;
};

struct VkTextureRes {
    Image img;
    VkSampler sampler = VK_NULL_HANDLE;
    TextureFormat format = TextureFormat::RGBA8;
    bool ownedByTarget = false;
    bool explicitMips = false;
    u64 bytes = 0;
};

struct VkPipelineRes {
    VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
    PipelineDesc state;  // shader sources dropped
    std::unordered_map<u64, VkPipeline> variants;
};

struct VkTargetRes {
    TextureHandle colorTexture;  // single-sampled color (sampleable)
    TextureHandle depthTexture;  // sampleable depth (shadow maps)
    bool hasColor = true;
    Image msaaColor;             // multisampled color
    Image depth;
    bool hasDepth = false;
    u32 width = 0, height = 0, samples = 1;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    u64 compat = 0;
    u64 bytes = 0;
    bool internal = false;  // the engine backbuffer
};

struct FrameData {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> descriptorPools;
    u32 poolIndex = 0;
    bool submitted = false;
    u64 frameIndex = 0;
    bool timestamps = false;
};

VkFormat toVk(TextureFormat f, VkFormat depthFormat) {
    switch (f) {
        case TextureFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA8_sRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureFormat::R8: return VK_FORMAT_R8_UNORM;
        case TextureFormat::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::Depth24Stencil8: return depthFormat;
        case TextureFormat::Depth16: return VK_FORMAT_D16_UNORM;  // mandatory: depth attachment + sampled
    }
    return VK_FORMAT_R8G8B8A8_UNORM;
}

VkFormat toVk(VertexFormat f) {
    switch (f) {
        case VertexFormat::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexFormat::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexFormat::UByte4Norm: return VK_FORMAT_R8G8B8A8_UNORM;
    }
    return VK_FORMAT_R32G32B32A32_SFLOAT;
}

VkCompareOp toVk(CompareOp op) {
    switch (op) {
        case CompareOp::Never: return VK_COMPARE_OP_NEVER;
        case CompareOp::Less: return VK_COMPARE_OP_LESS;
        case CompareOp::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
        case CompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
        case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_LESS_OR_EQUAL;
}

VkSamplerAddressMode toVk(Wrap w) {
    switch (w) {
        case Wrap::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case Wrap::Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case Wrap::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

bool hasStencil(VkFormat f) {
    return f == VK_FORMAT_D24_UNORM_S8_UINT || f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D16_UNORM_S8_UINT;
}

/// Render pass compatibility key: color format (UNDEFINED = depth-only), depth format,
/// samples, and whether depth is stored for sampling (bit 48).
u64 compatKey(VkFormat color, VkFormat depth, u32 samples, bool storeDepth = false) {
    return static_cast<u64>(color) | (static_cast<u64>(depth) << 20) | (static_cast<u64>(samples) << 40) |
           (static_cast<u64>(storeDepth) << 48);
}

const char* vendorName(u32 id) {
    switch (id) {
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "Intel";
        case 0x13B5: return "ARM";
        case 0x5143: return "Qualcomm";
        case 0x1010: return "Imagination";
        case 0x10005: return "Mesa";
        default: return "Unknown";
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
    auto* errors = static_cast<u64*>(user);
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ++*errors;
        AGR_LOG_ERROR("Vulkan", "{}", data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        AGR_LOG_WARN("Vulkan", "{}", data->pMessage);
    }
    return VK_FALSE;
}

#define AGR_VK_TRY(expr, what)                                                                        \
    do {                                                                                              \
        const VkResult agr_vk_r = (expr);                                                             \
        if (agr_vk_r != VK_SUCCESS)                                                                   \
            return makeError(Error::Code::Unsupported, std::string(what) + ": " + resultName(agr_vk_r)); \
    } while (0)

class VulkanDevice final : public Device {
public:
    ~VulkanDevice() override { shutdown(); }

    Status initialize(const VulkanPlatform& platform);

    const DeviceInfo& info() const override { return m_info; }
    u64 validationErrorCount() const override { return m_validationErrors; }

    // ---------------------------------------------------------------- buffers
    Result<BufferHandle> createBuffer(const BufferDesc& d) override {
        if (d.size == 0) return makeError(Error::Code::InvalidArgument, "buffer size must be > 0");
        VkBufferRes b;
        b.size = d.size;
        b.usage = d.usage;
        b.memory = d.memory;
        VkBufferUsageFlags usage = d.usage == BufferUsage::Vertex  ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                   : d.usage == BufferUsage::Index ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                                   : VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = d.size;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo ai{};
        ai.usage = VMA_MEMORY_USAGE_AUTO;
        if (d.memory == BufferMemory::Dynamic) {
            b.copies = kFrames;
            bi.usage = usage;
            ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        } else {
            b.copies = 1;
            bi.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        for (u32 i = 0; i < b.copies; ++i) {
            VmaAllocationInfo info{};
            if (vmaCreateBuffer(m_allocator, &bi, &ai, &b.buffer[i], &b.alloc[i], &info) != VK_SUCCESS) {
                for (u32 k = 0; k < i; ++k) vmaDestroyBuffer(m_allocator, b.buffer[k], b.alloc[k]);
                return makeError(Error::Code::Unknown, std::string("buffer allocation failed for '") + d.debugName + "'");
            }
            b.mapped[i] = info.pMappedData;
        }
        if (d.initialData) {
            if (d.memory == BufferMemory::Dynamic) {
                for (u32 i = 0; i < b.copies; ++i) {
                    std::memcpy(b.mapped[i], d.initialData, d.size);
                    vmaFlushAllocation(m_allocator, b.alloc[i], 0, VK_WHOLE_SIZE);
                }
            } else if (Status s = uploadToBuffer(b.buffer[0], 0, d.initialData, d.size); !s) {
                vmaDestroyBuffer(m_allocator, b.buffer[0], b.alloc[0]);
                return s.error();
            }
            m_stats.uploadedBytes += d.size;
        }
        m_bufferBytes += d.size * b.copies;
        return m_buffers.create(b);
    }

    Status updateBuffer(BufferHandle h, usize offset, const void* data, usize size) override {
        VkBufferRes* b = m_buffers.get(h);
        if (!b) return makeError(Error::Code::InvalidArgument, "invalid buffer handle");
        if (offset + size > b->size) return makeError(Error::Code::InvalidArgument, "buffer update out of range");
        if (size == 0) return {};
        m_stats.uploadedBytes += size;
        if (b->memory == BufferMemory::Static) {
            vkDeviceWaitIdle(m_device);  // rare: static data rewritten while frames may read it
            return uploadToBuffer(b->buffer[0], offset, data, size);
        }
        if (b->lastWriteFrame != m_frame) {
            // First write of this frame: move to the next copy (the GPU may still read the current one).
            const u32 next = (b->active + 1) % b->copies;
            waitForFrame(b->lastUsedFrame[next]);
            const bool partial = offset != 0 || size != b->size;
            if (partial) std::memcpy(b->mapped[next], b->mapped[b->active], b->size);
            b->active = next;
            b->lastWriteFrame = m_frame;
            m_descriptorKeyValid = false;  // buffer identity changed
        }
        std::memcpy(static_cast<u8*>(b->mapped[b->active]) + offset, data, size);
        vmaFlushAllocation(m_allocator, b->alloc[b->active], offset, size);
        return {};
    }

    void destroyBuffer(BufferHandle h) override {
        VkBufferRes* b = m_buffers.get(h);
        if (!b) return;
        const VkBufferRes copy = *b;
        m_bufferBytes -= copy.size * copy.copies;
        defer([this, copy] {
            for (u32 i = 0; i < copy.copies; ++i) vmaDestroyBuffer(m_allocator, copy.buffer[i], copy.alloc[i]);
        });
        m_buffers.release(h);
        for (auto& s : m_slots)
            if (s.buffer == h) s = {};
        if (m_vertexBuffer == h) m_vertexBuffer = {};
        if (m_instanceBuffer == h) m_instanceBuffer = {};
        if (m_indexBuffer == h) m_indexBuffer = {};
        m_descriptorKeyValid = false;
    }

    // --------------------------------------------------------------- textures
    Result<TextureHandle> createTexture(const TextureDesc& d) override {
        if (d.width == 0 || d.height == 0) return makeError(Error::Code::InvalidArgument, "texture size must be > 0");
        if (d.width > m_info.maxTextureSize || d.height > m_info.maxTextureSize)
            return makeError(Error::Code::Unsupported, "texture larger than GPU limit " + std::to_string(m_info.maxTextureSize));
        VkTextureRes t;
        t.format = d.format;
        const VkFormat format = toVk(d.format, m_depthFormat);
        const bool depth = isDepthFormat(d.format);
        if (!d.mipData.empty() && (!d.data || depth))
            return makeError(Error::Code::InvalidArgument, "explicit mip levels need level 0 data and a color format");
        u32 mips = 1;
        if (!d.mipData.empty()) {
            mips = 1 + static_cast<u32>(d.mipData.size());
            t.explicitMips = true;
        } else if (d.generateMips && !depth && canBlitLinear(format)) {
            while ((std::max(d.width, d.height) >> mips) > 0) ++mips;
        }
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (depth) usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        auto img = createImage(d.width, d.height, mips, 1, format, usage,
                               depth ? (VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0))
                                     : VK_IMAGE_ASPECT_COLOR_BIT);
        if (!img) return img.error();
        t.img = img.value();
        t.sampler = sampler(d.sampler, mips > 1);
        const u64 base = static_cast<u64>(d.width) * d.height * bytesPerPixel(d.format);
        t.bytes = mips > 1 ? base * 4 / 3 : base;
        if (t.explicitMips) {
            std::vector<const void*> levels{d.data};
            levels.insert(levels.end(), d.mipData.begin(), d.mipData.end());
            if (Status s = uploadImageLevels(t.img, levels, bytesPerPixel(d.format)); !s) {
                destroyImageNow(t.img);
                return s.error();
            }
            m_stats.uploadedBytes += t.bytes;
        } else if (d.data) {
            if (Status s = uploadImage(t.img, d.data, base); !s) {
                destroyImageNow(t.img);
                return s.error();
            }
            m_stats.uploadedBytes += base;
        } else if (!depth) {
            if (Status s = immediate([&](VkCommandBuffer c) { transition(c, t.img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); }); !s) {
                destroyImageNow(t.img);
                return s.error();
            }
        }
        m_textureBytes += t.bytes;
        return m_textures.create(t);
    }

    Status updateTexture(TextureHandle h, const void* data) override {
        VkTextureRes* t = m_textures.get(h);
        if (!t || !data) return makeError(Error::Code::InvalidArgument, "invalid texture or data");
        if (t->explicitMips) return makeError(Error::Code::Unsupported, "texture with explicit mip levels cannot be updated");
        vkDeviceWaitIdle(m_device);
        const u64 base = static_cast<u64>(t->img.width) * t->img.height * bytesPerPixel(t->format);
        m_stats.uploadedBytes += base;
        return uploadImage(t->img, data, base);
    }

    void destroyTexture(TextureHandle h) override {
        VkTextureRes* t = m_textures.get(h);
        if (!t) return;
        if (t->ownedByTarget) {
            AGR_LOG_WARN("RHI", "destroyTexture on a render-target texture ignored; destroy the target instead");
            return;
        }
        releaseTexture(h);
    }

    // -------------------------------------------------------------- pipelines
    Result<PipelineHandle> createPipeline(const PipelineDesc& d) override {
        if (d.uniformBlocks.size() > kMaxUniformSlots || d.samplers.size() > kMaxTextureUnits)
            return makeError(Error::Code::InvalidArgument, "too many uniform blocks or samplers");
        auto spirv = compileToSpirv(d);
        if (!spirv) return spirv.error();
        VkPipelineRes p;
        auto makeModule = [&](const std::vector<u32>& code, VkShaderModule& out) {
            VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            ci.codeSize = code.size() * sizeof(u32);
            ci.pCode = code.data();
            return vkCreateShaderModule(m_device, &ci, nullptr, &out);
        };
        if (makeModule(spirv.value().vertex, p.vs) != VK_SUCCESS || makeModule(spirv.value().fragment, p.fs) != VK_SUCCESS) {
            if (p.vs) vkDestroyShaderModule(m_device, p.vs, nullptr);
            return makeError(Error::Code::Unknown, "vkCreateShaderModule failed");
        }
        p.state = d;
        p.state.shader = {};
        // Create the variant for the backbuffer now so obvious errors surface early.
        const auto handle = m_pipelines.create(std::move(p));
        const u64 warmCompat = d.colorOutput ? m_backbufferCompat : compatKey(VK_FORMAT_UNDEFINED, VK_FORMAT_D16_UNORM, 1, true);
        if (!variant(*m_pipelines.get(handle), warmCompat)) {
            destroyPipeline(handle);
            return makeError(Error::Code::Unknown, std::string("vkCreateGraphicsPipelines failed for '") + d.debugName + "'");
        }
        return handle;
    }

    void destroyPipeline(PipelineHandle h) override {
        VkPipelineRes* p = m_pipelines.get(h);
        if (!p) return;
        VkPipelineRes copy = std::move(*p);
        defer([this, copy] {
            for (auto& [key, pipe] : copy.variants) vkDestroyPipeline(m_device, pipe, nullptr);
            vkDestroyShaderModule(m_device, copy.vs, nullptr);
            vkDestroyShaderModule(m_device, copy.fs, nullptr);
        });
        m_pipelines.release(h);
        if (m_pipeline == h) m_pipeline = {};
    }

    // --------------------------------------------------------- render targets
    Result<RenderTargetHandle> createRenderTarget(const RenderTargetDesc& d) override {
        return createTarget(d, false);
    }

    void destroyRenderTarget(RenderTargetHandle h) override {
        VkTargetRes* rt = m_targets.get(h);
        if (!rt) return;
        const VkTargetRes copy = *rt;
        if (!copy.internal) m_targetBytes -= copy.bytes;
        if (copy.colorTexture) releaseTexture(copy.colorTexture);
        if (copy.depthTexture) releaseTexture(copy.depthTexture);
        defer([this, copy] {
            vkDestroyFramebuffer(m_device, copy.framebuffer, nullptr);
            Image msaa = copy.msaaColor, depth = copy.depth;
            destroyImageNow(msaa);
            destroyImageNow(depth);
        });
        m_targets.release(h);
    }

    TextureHandle renderTargetTexture(RenderTargetHandle h) const override {
        const VkTargetRes* rt = m_targets.get(h);
        return rt && !rt->internal ? rt->colorTexture : TextureHandle{};
    }
    TextureHandle renderTargetDepthTexture(RenderTargetHandle h) const override {
        const VkTargetRes* rt = m_targets.get(h);
        return rt ? rt->depthTexture : TextureHandle{};
    }

    // ------------------------------------------------------------------ frame
    void beginFrame(u32 width, u32 height) override;
    void endFrame() override;
    void beginPass(RenderTargetHandle target, const ClearValues& clear) override;
    void endPass() override;

    void setViewport(i32 x, i32 y, u32 w, u32 h) override {
        // RHI viewports use a bottom-left origin (like OpenGL); flip to Vulkan with a
        // negative height so clip space and winding stay identical across backends.
        m_viewport = {static_cast<float>(x), static_cast<float>(m_passH) - static_cast<float>(y), static_cast<float>(w),
                      -static_cast<float>(h), 0.0f, 1.0f};
        if (m_inPass) vkCmdSetViewport(cmd(), 0, 1, &m_viewport);
    }

    void setScissor(bool enabled, i32 x, i32 y, u32 w, u32 h) override {
        if (enabled) {
            const i32 top = static_cast<i32>(m_passH) - y - static_cast<i32>(h);
            m_scissor = {{std::max(0, x), std::max(0, top)}, {w, h}};
        } else {
            m_scissor = {{0, 0}, {m_passW, m_passH}};
        }
        if (m_inPass) vkCmdSetScissor(cmd(), 0, 1, &m_scissor);
    }

    void bindPipeline(PipelineHandle h) override {
        if (h == m_pipeline) return;
        m_pipeline = h;
        m_pipelineDirty = true;
        ++m_stats.pipelineBinds;
    }
    void bindVertexBuffer(BufferHandle h, usize offset) override {
        m_vertexBuffer = h;
        m_vertexOffset = offset;
        m_vertexDirty = true;
    }
    void bindInstanceBuffer(BufferHandle h, usize offset) override {
        m_instanceBuffer = h;
        m_instanceOffset = offset;
    }
    void bindIndexBuffer(BufferHandle h, IndexType type, usize offset) override {
        m_indexBuffer = h;
        m_indexType = type;
        m_indexOffset = offset;
        m_indexDirty = true;
    }
    void bindUniformBuffer(u32 slot, BufferHandle h, usize offset, usize size) override {
        if (slot >= kMaxUniformSlots) return;
        AGR_ASSERT(offset % m_info.uniformBufferAlignment == 0, "uniform buffer offset not aligned");
        m_slots[slot] = {h, offset, size};
    }
    void bindTexture(u32 unit, TextureHandle h) override {
        if (unit >= kMaxTextureUnits || m_units[unit] == h) return;
        m_units[unit] = h;
        ++m_stats.textureBinds;
        VkTextureRes* t = m_textures.get(h);
        if (t && t->img.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            if (m_inPass) {
                AGR_LOG_ERROR("RHI", "texture bound inside a pass is not in a readable layout (bind it before beginPass)");
                ++m_validationErrors;
            }
            else if (m_recording) transition(cmd(), t->img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    void prepareTextureForSampling(TextureHandle h) override {
        VkTextureRes* t = m_textures.get(h);
        if (!t || t->img.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) return;
        if (!m_recording || m_inPass) {
            AGR_LOG_ERROR("RHI", "prepareTextureForSampling must be called inside a frame, outside a pass");
            ++m_validationErrors;
            return;
        }
        transition(cmd(), t->img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void draw(u32 vertexCount, u32 firstVertex) override {
        if (!prepareDraw(false)) return;
        vkCmdDraw(cmd(), vertexCount, 1, firstVertex, 0);
        count(vertexCount);
    }
    void drawIndexed(u32 indexCount, u32 firstIndex, i32 baseVertex) override {
        if (!prepareDraw(true)) return;
        vkCmdDrawIndexed(cmd(), indexCount, 1, firstIndex, baseVertex, 0);
        count(indexCount);
    }
    void drawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 baseVertex) override {
        if (instanceCount == 0 || !prepareDraw(true)) return;
        vkCmdDrawIndexed(cmd(), indexCount, instanceCount, firstIndex, baseVertex, 0);
        count(indexCount * instanceCount);
    }

    void blit(RenderTargetHandle src, RenderTargetHandle dst, Filter filter) override;
    Status readBackbuffer(u32 x, u32 y, u32 width, u32 height, std::vector<u8>& out) override;

    const FrameStats& frameStats() const override { return m_stats; }
    MemoryStats memoryStats() const override {
        MemoryStats m;
        m.bufferBytes = m_bufferBytes;
        m.textureBytes = m_textureBytes + m_targetBytes;
        m.buffers = static_cast<u32>(m_buffers.size());
        m.textures = static_cast<u32>(m_textures.size());
        m.pipelines = static_cast<u32>(m_pipelines.size());
        m.renderTargets = static_cast<u32>(m_targets.size()) - (m_backbuffer ? 1u : 0u);
        return m;
    }

private:
    struct SlotBinding {
        BufferHandle buffer;
        usize offset = 0, size = 0;
    };

    VkCommandBuffer cmd() const { return m_frames[m_frame % kFrames].cmd; }

    // --- helpers implemented below
    Result<Image> createImage(u32 w, u32 h, u32 mips, u32 samples, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect);
    void destroyImageNow(Image& img);
    void transition(VkCommandBuffer cmd, Image& img, VkImageLayout layout);
    Status immediate(const std::function<void(VkCommandBuffer)>& record);
    Status uploadToBuffer(VkBuffer dst, usize offset, const void* data, usize size);
    Status uploadImage(Image& img, const void* data, u64 bytes);
    /// Every mip level given by the caller (level i is max(1, size >> i)).
    Status uploadImageLevels(Image& img, const std::vector<const void*>& levels, u32 pixelBytes);
    bool canBlitLinear(VkFormat format) const;
    VkSampler sampler(const SamplerDesc& s, bool hasMips);
    VkRenderPass renderPass(u64 compat, bool clearColor, bool clearDepth);
    VkPipeline variant(VkPipelineRes& p, u64 compat);
    Result<RenderTargetHandle> createTarget(const RenderTargetDesc& d, bool internal);
    void releaseTexture(TextureHandle h);
    bool prepareDraw(bool indexed);
    void count(u32 vertices) {
        ++m_stats.drawCalls;
        m_stats.vertices += vertices;
        if (const VkPipelineRes* p = m_pipelines.get(m_pipeline); p && p->state.topology == Topology::Triangles)
            m_stats.triangles += vertices / 3;
    }
    Status createSwapchain(u32 width, u32 height);
    void releaseSurface() override;
    Status restoreSurface() override;
    void destroySwapchain(VkSwapchainKHR swapchain);
    void ensureBackbuffer(u32 w, u32 h);
    void waitForFrame(u64 frame);
    void defer(std::function<void()> fn) { m_deletions.push_back({m_frame, std::move(fn)}); }
    void runDeletions(bool all);
    void shutdown();

    // --- core objects
    VulkanPlatform m_platform;
    DeviceInfo m_info;
    FrameStats m_stats;
    u64 m_validationErrors = 0;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    bool m_surfaceLost = false;
    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties m_props{};
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    u32 m_queueFamily = 0;
    VmaAllocator m_allocator = nullptr;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
    VkQueryPool m_timestamps = VK_NULL_HANDLE;
    u64 m_timestampMask = ~0ull;

    // --- swapchain
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapExtent{};
    std::vector<VkImage> m_swapImages;
    std::vector<VkSemaphore> m_renderFinished;
    bool m_swapchainDirty = true;
    bool m_canBlitToSwapchain = true;

    // --- frames
    std::array<FrameData, kFrames> m_frames{};
    u64 m_frame = 0;
    i64 m_completedFrame = -1;
    bool m_recording = false;
    VkCommandPool m_immediatePool = VK_NULL_HANDLE;
    VkCommandBuffer m_immediateCmd = VK_NULL_HANDLE;
    VkFence m_immediateFence = VK_NULL_HANDLE;
    struct Deletion {
        u64 frame;
        std::function<void()> fn;
    };
    std::vector<Deletion> m_deletions;

    // --- resources
    HandlePool<VkBufferRes, BufferTag> m_buffers;
    HandlePool<VkTextureRes, TextureTag> m_textures;
    HandlePool<VkPipelineRes, PipelineTag> m_pipelines;
    HandlePool<VkTargetRes, RenderTargetTag> m_targets;
    std::unordered_map<u64, VkRenderPass> m_renderPasses;
    std::unordered_map<u64, VkSampler> m_samplers;
    u64 m_bufferBytes = 0, m_textureBytes = 0, m_targetBytes = 0;
    RenderTargetHandle m_backbuffer;
    u64 m_backbufferCompat = 0;
    BufferHandle m_dummyUniform;
    TextureHandle m_dummyTexture;
    VkBuffer m_dummyUniformBuffer = VK_NULL_HANDLE;  // cached: used for every unbound slot
    VkImageView m_dummyView = VK_NULL_HANDLE;
    VkSampler m_dummySampler = VK_NULL_HANDLE;

    // --- bound state
    bool m_inPass = false;
    u32 m_passW = 0, m_passH = 0;
    u64 m_passCompat = 0;
    VkViewport m_viewport{};
    VkRect2D m_scissor{};
    PipelineHandle m_pipeline;
    bool m_pipelineDirty = true;
    VkPipeline m_boundVkPipeline = VK_NULL_HANDLE;
    BufferHandle m_vertexBuffer, m_indexBuffer, m_instanceBuffer;
    usize m_vertexOffset = 0, m_indexOffset = 0, m_instanceOffset = 0;
    IndexType m_indexType = IndexType::U16;
    bool m_vertexDirty = true, m_indexDirty = true;
    std::array<SlotBinding, kMaxUniformSlots> m_slots{};
    std::array<TextureHandle, kMaxTextureUnits> m_units{};
    std::array<u64, kMaxUniformSlots * 2 + kMaxTextureUnits * 2> m_descriptorKey{};
    bool m_descriptorKeyValid = false;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    std::array<u32, kMaxUniformSlots> m_boundOffsets{};
};

// ============================================================ initialization
Status VulkanDevice::initialize(const VulkanPlatform& platform) {
    m_platform = platform;
    if (!platform.getInstanceProcAddr) return makeError(Error::Code::Unsupported, "no vkGetInstanceProcAddr from the platform");
    volkInitializeCustom(reinterpret_cast<PFN_vkGetInstanceProcAddr>(platform.getInstanceProcAddr));
    if (volkGetInstanceVersion() < VK_API_VERSION_1_1)
        return makeError(Error::Code::Unsupported, "Vulkan 1.1 loader not available");

    // ---- instance
    std::vector<const char*> extensions;
    for (const auto& e : platform.instanceExtensions) extensions.push_back(e.c_str());
    std::vector<const char*> layers;
    bool debugUtils = false;
    if (platform.validation) {
        u32 count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> props(count);
        vkEnumerateInstanceLayerProperties(&count, props.data());
        for (const auto& l : props)
            if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) layers.push_back("VK_LAYER_KHRONOS_validation");
        u32 extCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, exts.data());
        for (const auto& e : exts)
            if (std::strcmp(e.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) debugUtils = true;
        if (debugUtils) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if (layers.empty()) AGR_LOG_WARN("RHI", "Vulkan validation requested but VK_LAYER_KHRONOS_validation is not installed");
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = m_platform.appName.c_str();
    app.pEngineName = "AGR ENGINE";
    app.engineVersion = VK_MAKE_VERSION(AGR_VERSION_MAJOR, AGR_VERSION_MINOR, AGR_VERSION_PATCH);
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = static_cast<u32>(extensions.size());
    ici.ppEnabledExtensionNames = extensions.data();
    ici.enabledLayerCount = static_cast<u32>(layers.size());
    ici.ppEnabledLayerNames = layers.data();
    AGR_VK_TRY(vkCreateInstance(&ici, nullptr, &m_instance), "vkCreateInstance");
    volkLoadInstanceOnly(m_instance);

    if (debugUtils && !layers.empty()) {
        VkDebugUtilsMessengerCreateInfoEXT mi{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        mi.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        mi.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        mi.pfnUserCallback = debugCallback;
        mi.pUserData = &m_validationErrors;
        vkCreateDebugUtilsMessengerEXT(m_instance, &mi, nullptr, &m_messenger);
    }

    // ---- surface
    if (!platform.createSurface) return makeError(Error::Code::Unsupported, "platform cannot create a Vulkan surface");
    m_surface = reinterpret_cast<VkSurfaceKHR>(platform.createSurface(m_instance));
    if (!m_surface) return makeError(Error::Code::Unsupported, "Vulkan surface creation failed");

    // ---- physical device: graphics + present queue, swapchain, Vulkan 1.1
    u32 count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());
    int bestScore = -1;
    for (VkPhysicalDevice pd : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(pd, &props);
        if (props.apiVersion < VK_API_VERSION_1_1) continue;
        u32 extCount = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data());
        if (std::none_of(exts.begin(), exts.end(), [](auto& e) { return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0; }))
            continue;
        u32 qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, queues.data());
        for (u32 q = 0; q < qCount; ++q) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, q, m_surface, &present);
            if (!(queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !present) continue;
            const int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? 4
                              : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 3
                              : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU    ? 2
                                                                                           : 1;
            if (score > bestScore) {
                bestScore = score;
                m_physical = pd;
                m_queueFamily = q;
                const u32 bits = queues[q].timestampValidBits;
                m_timestampMask = bits >= 64 ? ~0ull : ((1ull << bits) - 1);
                m_info.gpuTimers = bits > 0;
            }
            break;
        }
    }
    if (!m_physical) return makeError(Error::Code::Unsupported, "no Vulkan 1.1 GPU with graphics + presentation support");
    vkGetPhysicalDeviceProperties(m_physical, &m_props);
    m_info.gpuTimers = m_info.gpuTimers && m_props.limits.timestampComputeAndGraphics;

    // ---- logical device
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = m_queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    const char* deviceExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = deviceExts;
    AGR_VK_TRY(vkCreateDevice(m_physical, &dci, nullptr, &m_device), "vkCreateDevice");
    volkLoadDevice(m_device);
    vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);

    VmaVulkanFunctions fns{};
    fns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    fns.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    VmaAllocatorCreateInfo aci{};
    aci.vulkanApiVersion = VK_API_VERSION_1_1;
    aci.physicalDevice = m_physical;
    aci.device = m_device;
    aci.instance = m_instance;
    aci.pVulkanFunctions = &fns;
    AGR_VK_TRY(vmaCreateAllocator(&aci, &m_allocator), "vmaCreateAllocator");

    // ---- capabilities
    for (VkFormat f : {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT}) {
        VkFormatProperties fp;
        vkGetPhysicalDeviceFormatProperties(m_physical, f, &fp);
        if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            m_depthFormat = f;
            break;
        }
    }
    if (m_depthFormat == VK_FORMAT_UNDEFINED) return makeError(Error::Code::Unsupported, "no depth format");
    const VkSampleCountFlags sc = m_props.limits.framebufferColorSampleCounts & m_props.limits.framebufferDepthSampleCounts;
    m_info.maxSamples = (sc & VK_SAMPLE_COUNT_8_BIT) ? 8 : (sc & VK_SAMPLE_COUNT_4_BIT) ? 4 : (sc & VK_SAMPLE_COUNT_2_BIT) ? 2 : 1;
    m_info.backend = Backend::Vulkan;
    m_info.halfFloatRenderTargets = true;  // R16G16B16A16_SFLOAT color attachments are mandatory in Vulkan
    m_info.apiVersion = "Vulkan " + std::to_string(VK_API_VERSION_MAJOR(m_props.apiVersion)) + "." +
                        std::to_string(VK_API_VERSION_MINOR(m_props.apiVersion)) + "." +
                        std::to_string(VK_API_VERSION_PATCH(m_props.apiVersion));
    m_info.renderer = m_props.deviceName;
    m_info.vendor = vendorName(m_props.vendorID);
    m_info.maxTextureSize = m_props.limits.maxImageDimension2D;
    m_info.uniformBufferAlignment = static_cast<u32>(m_props.limits.minUniformBufferOffsetAlignment);
    m_info.maxUniformBlockSize = m_props.limits.maxUniformBufferRange;
    m_info.softwareRenderer = m_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;

    // ---- descriptor layout shared by all pipelines
    std::array<VkDescriptorSetLayoutBinding, kMaxUniformSlots + kMaxTextureUnits> bindings{};
    for (u32 i = 0; i < bindings.size(); ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[i].descriptorType = i < kMaxUniformSlots ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                                                          : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = static_cast<u32>(bindings.size());
    dsl.pBindings = bindings.data();
    AGR_VK_TRY(vkCreateDescriptorSetLayout(m_device, &dsl, nullptr, &m_setLayout), "vkCreateDescriptorSetLayout");
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;
    AGR_VK_TRY(vkCreatePipelineLayout(m_device, &pl, nullptr, &m_pipelineLayout), "vkCreatePipelineLayout");
    VkPipelineCacheCreateInfo pci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    AGR_VK_TRY(vkCreatePipelineCache(m_device, &pci, nullptr, &m_pipelineCache), "vkCreatePipelineCache");

    // ---- per-frame objects
    for (FrameData& f : m_frames) {
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cp.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        cp.queueFamilyIndex = m_queueFamily;
        AGR_VK_TRY(vkCreateCommandPool(m_device, &cp, nullptr, &f.pool), "vkCreateCommandPool");
        VkCommandBufferAllocateInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cb.commandPool = f.pool;
        cb.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cb.commandBufferCount = 1;
        AGR_VK_TRY(vkAllocateCommandBuffers(m_device, &cb, &f.cmd), "vkAllocateCommandBuffers");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        AGR_VK_TRY(vkCreateFence(m_device, &fi, nullptr, &f.fence), "vkCreateFence");
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        AGR_VK_TRY(vkCreateSemaphore(m_device, &si, nullptr, &f.imageAvailable), "vkCreateSemaphore");
    }
    {
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cp.queueFamilyIndex = m_queueFamily;
        AGR_VK_TRY(vkCreateCommandPool(m_device, &cp, nullptr, &m_immediatePool), "vkCreateCommandPool");
        VkCommandBufferAllocateInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cb.commandPool = m_immediatePool;
        cb.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cb.commandBufferCount = 1;
        AGR_VK_TRY(vkAllocateCommandBuffers(m_device, &cb, &m_immediateCmd), "vkAllocateCommandBuffers");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        AGR_VK_TRY(vkCreateFence(m_device, &fi, nullptr, &m_immediateFence), "vkCreateFence");
    }
    if (m_info.gpuTimers) {
        VkQueryPoolCreateInfo qp{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qp.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qp.queryCount = kFrames * 2;
        if (vkCreateQueryPool(m_device, &qp, nullptr, &m_timestamps) != VK_SUCCESS) m_info.gpuTimers = false;
    }

    // ---- fallback resources and default backbuffer signature
    m_backbufferCompat = compatKey(VK_FORMAT_R8G8B8A8_UNORM, m_depthFormat, 1);
    const u8 zeros[256] = {};
    auto ubo = createBuffer({BufferUsage::Uniform, BufferMemory::Static, sizeof zeros, zeros, "agr.vk.dummy.ubo"});
    if (!ubo) return ubo.error();
    m_dummyUniform = ubo.value();
    const u8 white[4] = {255, 255, 255, 255};
    TextureDesc td;
    td.format = TextureFormat::RGBA8;
    td.generateMips = false;
    td.data = white;
    auto tex = createTexture(td);
    if (!tex) return tex.error();
    m_dummyTexture = tex.value();
    const VkBufferRes* dummyBuffer = m_buffers.get(m_dummyUniform);
    const VkTextureRes* dummyTexture = m_textures.get(m_dummyTexture);
    if (!dummyBuffer || !dummyTexture) return makeError(Error::Code::Unknown, "fallback resources missing");
    m_dummyUniformBuffer = dummyBuffer->buffer[0];
    m_dummyView = dummyTexture->img.view;
    m_dummySampler = dummyTexture->sampler;

    AGR_LOG_INFO("RHI", "Vulkan device: {} | {} ({}) | max texture {} | MSAA x{} | UBO align {} | GPU timers {} | validation {}",
                 m_info.apiVersion, m_info.renderer, m_info.vendor, m_info.maxTextureSize, m_info.maxSamples,
                 m_info.uniformBufferAlignment, m_info.gpuTimers ? "yes" : "no", m_messenger ? "on" : "off");
    return {};
}

void VulkanDevice::shutdown() {
    if (!m_device) {
        if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        if (m_messenger) vkDestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);
        if (m_instance) vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        m_surface = VK_NULL_HANDLE;
        return;
    }
    vkDeviceWaitIdle(m_device);
    std::vector<RenderTargetHandle> targets;
    m_targets.forEach([&](RenderTargetHandle h, VkTargetRes&) { targets.push_back(h); });
    for (auto h : targets) destroyRenderTarget(h);
    std::vector<TextureHandle> textures;
    m_textures.forEach([&](TextureHandle h, VkTextureRes&) { textures.push_back(h); });
    for (auto h : textures) releaseTexture(h);
    std::vector<BufferHandle> buffers;
    m_buffers.forEach([&](BufferHandle h, VkBufferRes&) { buffers.push_back(h); });
    for (auto h : buffers) destroyBuffer(h);
    std::vector<PipelineHandle> pipelines;
    m_pipelines.forEach([&](PipelineHandle h, VkPipelineRes&) { pipelines.push_back(h); });
    for (auto h : pipelines) destroyPipeline(h);
    runDeletions(true);
    for (auto& [k, rp] : m_renderPasses) vkDestroyRenderPass(m_device, rp, nullptr);
    for (auto& [k, s] : m_samplers) vkDestroySampler(m_device, s, nullptr);
    destroySwapchain(m_swapchain);
    for (FrameData& f : m_frames) {
        for (auto p : f.descriptorPools) vkDestroyDescriptorPool(m_device, p, nullptr);
        vkDestroySemaphore(m_device, f.imageAvailable, nullptr);
        vkDestroyFence(m_device, f.fence, nullptr);
        vkDestroyCommandPool(m_device, f.pool, nullptr);
    }
    vkDestroyFence(m_device, m_immediateFence, nullptr);
    vkDestroyCommandPool(m_device, m_immediatePool, nullptr);
    if (m_timestamps) vkDestroyQueryPool(m_device, m_timestamps, nullptr);
    vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_setLayout, nullptr);
    vmaDestroyAllocator(m_allocator);
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_messenger) vkDestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
    m_surface = VK_NULL_HANDLE;
}

// ================================================================== helpers
Result<Image> VulkanDevice::createImage(u32 w, u32 h, u32 mips, u32 samples, VkFormat format, VkImageUsageFlags usage,
                                        VkImageAspectFlags aspect) {
    Image img;
    img.width = w;
    img.height = h;
    img.mips = mips;
    img.samples = samples;
    img.format = format;
    img.aspect = aspect;
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent = {w, h, 1};
    ii.mipLevels = mips;
    ii.arrayLayers = 1;
    ii.samples = static_cast<VkSampleCountFlagBits>(samples);
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = usage;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    AGR_VK_TRY(vmaCreateImage(m_allocator, &ii, &ai, &img.image, &img.alloc, nullptr), "vmaCreateImage");
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = img.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    // Sampled depth/stencil views may only expose one aspect; attachments use both.
    vi.subresourceRange = {aspect, 0, mips, 0, 1};
    if (vkCreateImageView(m_device, &vi, nullptr, &img.view) != VK_SUCCESS) {
        vmaDestroyImage(m_allocator, img.image, img.alloc);
        return makeError(Error::Code::Unknown, "vkCreateImageView failed");
    }
    return img;
}

void VulkanDevice::destroyImageNow(Image& img) {
    if (img.view) vkDestroyImageView(m_device, img.view, nullptr);
    if (img.image) vmaDestroyImage(m_allocator, img.image, img.alloc);
    img = {};
}

void VulkanDevice::transition(VkCommandBuffer c, Image& img, VkImageLayout layout) {
    if (img.layout == layout) return;
    // Conservative full barrier: correct for every transition used by the engine.
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    b.oldLayout = img.layout;
    b.newLayout = layout;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img.image;
    b.subresourceRange = {img.aspect, 0, img.mips, 0, 1};
    vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &b);
    img.layout = layout;
}

Status VulkanDevice::immediate(const std::function<void(VkCommandBuffer)>& record) {
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResetCommandBuffer(m_immediateCmd, 0);
    vkBeginCommandBuffer(m_immediateCmd, &bi);
    record(m_immediateCmd);
    vkEndCommandBuffer(m_immediateCmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &m_immediateCmd;
    vkResetFences(m_device, 1, &m_immediateFence);
    AGR_VK_TRY(vkQueueSubmit(m_queue, 1, &si, m_immediateFence), "vkQueueSubmit (immediate)");
    AGR_VK_TRY(vkWaitForFences(m_device, 1, &m_immediateFence, VK_TRUE, UINT64_MAX), "vkWaitForFences (immediate)");
    return {};
}

Status VulkanDevice::uploadToBuffer(VkBuffer dst, usize offset, const void* data, usize size) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer staging;
    VmaAllocation alloc;
    VmaAllocationInfo info{};
    AGR_VK_TRY(vmaCreateBuffer(m_allocator, &bi, &ai, &staging, &alloc, &info), "staging buffer");
    std::memcpy(info.pMappedData, data, size);
    vmaFlushAllocation(m_allocator, alloc, 0, VK_WHOLE_SIZE);
    Status s = immediate([&](VkCommandBuffer c) {
        VkBufferCopy region{0, offset, size};
        vkCmdCopyBuffer(c, staging, dst, 1, &region);
    });
    vmaDestroyBuffer(m_allocator, staging, alloc);
    return s;
}

bool VulkanDevice::canBlitLinear(VkFormat format) const {
    VkFormatProperties fp;
    vkGetPhysicalDeviceFormatProperties(m_physical, format, &fp);
    const VkFormatFeatureFlags need = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    return (fp.optimalTilingFeatures & need) == need;
}

Status VulkanDevice::uploadImage(Image& img, const void* data, u64 bytes) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = bytes;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer staging;
    VmaAllocation alloc;
    VmaAllocationInfo info{};
    AGR_VK_TRY(vmaCreateBuffer(m_allocator, &bi, &ai, &staging, &alloc, &info), "staging buffer");
    std::memcpy(info.pMappedData, data, bytes);
    vmaFlushAllocation(m_allocator, alloc, 0, VK_WHOLE_SIZE);
    Status s = immediate([&](VkCommandBuffer c) {
        transition(c, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy region{};
        region.imageSubresource = {img.aspect, 0, 0, 1};
        region.imageExtent = {img.width, img.height, 1};
        vkCmdCopyBufferToImage(c, staging, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        // Mip chain: each level is blitted from the previous one.
        i32 w = static_cast<i32>(img.width), h = static_cast<i32>(img.height);
        for (u32 level = 1; level < img.mips; ++level) {
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = img.image;
            b.subresourceRange = {img.aspect, level - 1, 1, 0, 1};
            vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
            const i32 nw = std::max(1, w / 2), nh = std::max(1, h / 2);
            VkImageBlit blit{};
            blit.srcSubresource = {img.aspect, level - 1, 0, 1};
            blit.srcOffsets[1] = {w, h, 1};
            blit.dstSubresource = {img.aspect, level, 0, 1};
            blit.dstOffsets[1] = {nw, nh, 1};
            vkCmdBlitImage(c, img.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &blit, VK_FILTER_LINEAR);
            w = nw;
            h = nh;
        }
        // Levels 0..n-2 are TRANSFER_SRC, the last one TRANSFER_DST: move all to shader-read.
        for (u32 level = 0; level < img.mips; ++level) {
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            b.oldLayout = level + 1 < img.mips ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = img.image;
            b.subresourceRange = {img.aspect, level, 1, 0, 1};
            vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr,
                                 1, &b);
        }
        img.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    });
    vmaDestroyBuffer(m_allocator, staging, alloc);
    return s;
}

Status VulkanDevice::uploadImageLevels(Image& img, const std::vector<const void*>& levels, u32 pixelBytes) {
    std::vector<VkBufferImageCopy> regions;
    u64 total = 0;
    u32 w = img.width, h = img.height;
    for (usize level = 0; level < levels.size(); ++level) {
        VkBufferImageCopy r{};
        r.bufferOffset = total;
        r.imageSubresource = {img.aspect, static_cast<u32>(level), 0, 1};
        r.imageExtent = {w, h, 1};
        regions.push_back(r);
        total += static_cast<u64>(w) * h * pixelBytes;
        total = (total + 15) & ~u64{15};  // keep offsets aligned for every texel size
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
    }
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = total;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer staging;
    VmaAllocation alloc;
    VmaAllocationInfo info{};
    AGR_VK_TRY(vmaCreateBuffer(m_allocator, &bi, &ai, &staging, &alloc, &info), "staging buffer");
    for (usize level = 0; level < levels.size(); ++level) {
        const VkExtent3D e = regions[level].imageExtent;
        std::memcpy(static_cast<u8*>(info.pMappedData) + regions[level].bufferOffset, levels[level],
                    static_cast<usize>(e.width) * e.height * pixelBytes);
    }
    vmaFlushAllocation(m_allocator, alloc, 0, VK_WHOLE_SIZE);
    Status s = immediate([&](VkCommandBuffer c) {
        transition(c, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkCmdCopyBufferToImage(c, staging, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<u32>(regions.size()), regions.data());
        transition(c, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });
    vmaDestroyBuffer(m_allocator, staging, alloc);
    return s;
}

VkSampler VulkanDevice::sampler(const SamplerDesc& s, bool hasMips) {
    const bool useMips = hasMips && s.mipmaps;
    const u64 key = static_cast<u64>(s.minFilter) | (static_cast<u64>(s.magFilter) << 4) | (static_cast<u64>(useMips) << 8) |
                    (static_cast<u64>(s.wrapU) << 12) | (static_cast<u64>(s.wrapV) << 16);
    if (auto it = m_samplers.find(key); it != m_samplers.end()) return it->second;
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = s.magFilter == Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.minFilter = s.minFilter == Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.mipmapMode = s.minFilter == Filter::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = toVk(s.wrapU);
    si.addressModeV = toVk(s.wrapV);
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = useMips ? VK_LOD_CLAMP_NONE : 0.0f;
    VkSampler out = VK_NULL_HANDLE;
    vkCreateSampler(m_device, &si, nullptr, &out);
    m_samplers[key] = out;
    return out;
}

VkRenderPass VulkanDevice::renderPass(u64 compat, bool clearColor, bool clearDepth) {
    const u64 key = compat | (static_cast<u64>(clearColor) << 50) | (static_cast<u64>(clearDepth) << 51);
    if (auto it = m_renderPasses.find(key); it != m_renderPasses.end()) return it->second;
    const auto color = static_cast<VkFormat>(compat & 0xFFFFF);
    const auto depth = static_cast<VkFormat>((compat >> 20) & 0xFFFFF);
    const auto samples = static_cast<VkSampleCountFlagBits>((compat >> 40) & 0xFF);
    const bool storeDepth = (compat >> 48) & 1;
    VkAttachmentDescription att[2]{};
    att[0].format = color;
    att[0].samples = samples;
    att[0].loadOp = clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    att[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // Depth is never stored: tile-based GPUs keep it on chip (like glInvalidateFramebuffer).
    att[1].format = depth;
    att[1].samples = samples;
    att[1].loadOp = clearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    att[1].storeOp = storeDepth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[1].stencilLoadOp = clearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    att[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    att[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    const bool hasColor = color != VK_FORMAT_UNDEFINED;
    const bool hasDepth = depth != VK_FORMAT_UNDEFINED;
    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{hasColor ? 1u : 0u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = hasColor ? 1 : 0;
    sub.pColorAttachments = hasColor ? &colorRef : nullptr;
    sub.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = (hasColor ? 1u : 0u) + (hasDepth ? 1u : 0u);
    rp.pAttachments = hasColor ? att : att + 1;
    rp.subpassCount = 1;
    rp.pSubpasses = &sub;
    VkRenderPass out = VK_NULL_HANDLE;
    if (vkCreateRenderPass(m_device, &rp, nullptr, &out) != VK_SUCCESS) AGR_LOG_ERROR("RHI", "vkCreateRenderPass failed");
    m_renderPasses[key] = out;
    return out;
}

VkPipeline VulkanDevice::variant(VkPipelineRes& p, u64 compat) {
    if (auto it = p.variants.find(compat); it != p.variants.end()) return it->second;
    const PipelineDesc& d = p.state;
    const bool hasDepth = ((compat >> 20) & 0xFFFFF) != 0;
    const auto samples = static_cast<VkSampleCountFlagBits>((compat >> 40) & 0xFF);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, p.vs, "main", nullptr};
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, p.fs, "main", nullptr};
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    if (d.layout.stride) {
        bindings.push_back({0, d.layout.stride, VK_VERTEX_INPUT_RATE_VERTEX});
        for (const auto& a : d.layout.attributes) attrs.push_back({a.location, 0, toVk(a.format), a.offset});
    }
    if (d.instanceLayout.stride) {
        bindings.push_back({1, d.instanceLayout.stride, VK_VERTEX_INPUT_RATE_INSTANCE});
        for (const auto& a : d.instanceLayout.attributes) attrs.push_back({a.location, 1, toVk(a.format), a.offset});
    }
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = static_cast<u32>(bindings.size());
    vi.pVertexBindingDescriptions = bindings.data();
    vi.vertexAttributeDescriptionCount = static_cast<u32>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = d.topology == Topology::Lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = d.cull == CullMode::None ? VK_CULL_MODE_NONE : d.cull == CullMode::Back ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // valid because of the negative-height viewport
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = samples;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = d.depthTest && hasDepth;
    ds.depthWriteEnable = d.depthWrite && hasDepth;
    ds.depthCompareOp = toVk(d.depthCompare);
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
    switch (d.blend) {
        case BlendMode::Opaque: break;
        case BlendMode::Alpha:
            blend.blendEnable = VK_TRUE;
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        case BlendMode::PremultipliedAlpha:
            blend.blendEnable = VK_TRUE;
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        case BlendMode::Additive:
            blend.blendEnable = VK_TRUE;
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            break;
    }
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = (compat & 0xFFFFF) != 0 ? 1 : 0;  // depth-only passes have no color attachment
    cb.pAttachments = &blend;
    const VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;
    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dy;
    gp.layout = m_pipelineLayout;
    gp.renderPass = renderPass(compat, true, true);  // compatible with every load-op variant
    VkPipeline out = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &gp, nullptr, &out) != VK_SUCCESS) {
        AGR_LOG_ERROR("RHI", "vkCreateGraphicsPipelines failed for '{}'", d.debugName);
        return VK_NULL_HANDLE;
    }
    p.variants[compat] = out;
    return out;
}

Result<RenderTargetHandle> VulkanDevice::createTarget(const RenderTargetDesc& d, bool internal) {
    if (d.width == 0 || d.height == 0) return makeError(Error::Code::InvalidArgument, "render target size must be > 0");
    VkTargetRes rt;
    rt.width = d.width;
    rt.height = d.height;
    rt.samples = std::clamp<u32>(d.samples, 1, m_info.maxSamples);
    rt.hasDepth = d.depth || d.sampleableDepth;
    rt.hasColor = d.color;
    rt.internal = internal;
    if (d.sampleableDepth && rt.samples > 1)
        return makeError(Error::Code::InvalidArgument, "sampleable depth requires a single-sampled target");
    if (!rt.hasColor && !rt.hasDepth) return makeError(Error::Code::InvalidArgument, "render target without color nor depth");
    rt.colorFormat = rt.hasColor ? toVk(d.colorFormat, m_depthFormat) : VK_FORMAT_UNDEFINED;
    VkImageView colorView = VK_NULL_HANDLE;
    if (!rt.hasColor) {
        // depth-only target
    } else if (rt.samples > 1) {
        auto img = createImage(d.width, d.height, 1, rt.samples, rt.colorFormat,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        if (!img) return img.error();
        rt.msaaColor = img.value();
        colorView = rt.msaaColor.view;
    } else {
        auto img = createImage(d.width, d.height, 1, 1, rt.colorFormat,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               VK_IMAGE_ASPECT_COLOR_BIT);
        if (!img) return img.error();
        VkTextureRes t;
        t.img = img.value();
        t.format = d.colorFormat;
        t.ownedByTarget = true;
        t.sampler = sampler({Filter::Linear, Filter::Linear, false, Wrap::Clamp, Wrap::Clamp}, false);
        t.bytes = static_cast<u64>(d.width) * d.height * bytesPerPixel(d.colorFormat);
        m_textureBytes += t.bytes;
        rt.colorTexture = m_textures.create(t);
        colorView = t.img.view;
    }
    VkFormat depthFormat = m_depthFormat;
    VkImageView depthView = VK_NULL_HANDLE;
    if (d.sampleableDepth) {
        depthFormat = VK_FORMAT_D16_UNORM;
        auto img = createImage(d.width, d.height, 1, 1, depthFormat,
                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
        if (!img) {
            if (rt.colorTexture) releaseTexture(rt.colorTexture);
            return img.error();
        }
        VkTextureRes t;
        t.img = img.value();
        t.format = TextureFormat::Depth16;
        t.ownedByTarget = true;
        t.sampler = sampler({Filter::Nearest, Filter::Nearest, false, Wrap::Clamp, Wrap::Clamp}, false);
        t.bytes = static_cast<u64>(d.width) * d.height * 2;
        m_textureBytes += t.bytes;
        rt.depthTexture = m_textures.create(t);
        depthView = t.img.view;
    } else if (rt.hasDepth) {
        auto img = createImage(d.width, d.height, 1, rt.samples, m_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                               VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil(m_depthFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0));
        if (!img) {
            if (rt.colorTexture) releaseTexture(rt.colorTexture);
            destroyImageNow(rt.msaaColor);
            return img.error();
        }
        rt.depth = img.value();
        depthView = rt.depth.view;
    }
    rt.compat = compatKey(rt.colorFormat, rt.hasDepth ? depthFormat : VK_FORMAT_UNDEFINED, rt.samples, d.sampleableDepth);
    VkImageView views[2];
    u32 viewCount = 0;
    if (rt.hasColor) views[viewCount++] = colorView;
    if (rt.hasDepth) views[viewCount++] = depthView;
    VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fi.renderPass = renderPass(rt.compat, true, true);
    fi.attachmentCount = viewCount;
    fi.pAttachments = views;
    fi.width = d.width;
    fi.height = d.height;
    fi.layers = 1;
    if (vkCreateFramebuffer(m_device, &fi, nullptr, &rt.framebuffer) != VK_SUCCESS) {
        if (rt.colorTexture) releaseTexture(rt.colorTexture);
        if (rt.depthTexture) releaseTexture(rt.depthTexture);
        destroyImageNow(rt.msaaColor);
        destroyImageNow(rt.depth);
        return makeError(Error::Code::Unknown, "vkCreateFramebuffer failed");
    }
    const u64 pixels = static_cast<u64>(d.width) * d.height * rt.samples;
    rt.bytes = (rt.samples > 1 && rt.hasColor ? pixels * bytesPerPixel(d.colorFormat) : 0) +
               (rt.hasDepth && !d.sampleableDepth ? pixels * 4 : 0);
    if (!internal) m_targetBytes += rt.bytes;
    return m_targets.create(rt);
}

void VulkanDevice::releaseTexture(TextureHandle h) {
    VkTextureRes* t = m_textures.get(h);
    if (!t) return;
    const Image img = t->img;
    m_textureBytes -= t->bytes;
    defer([this, img] {
        Image copy = img;
        destroyImageNow(copy);
    });
    for (auto& u : m_units)
        if (u == h) u = {};
    m_textures.release(h);
    m_descriptorKeyValid = false;
}

void VulkanDevice::waitForFrame(u64 frame) {
    if (frame == 0 && m_completedFrame < 0 && m_frame == 0) return;
    if (static_cast<i64>(frame) <= m_completedFrame) return;
    FrameData& f = m_frames[frame % kFrames];
    if (f.submitted && f.frameIndex == frame) {
        vkWaitForFences(m_device, 1, &f.fence, VK_TRUE, UINT64_MAX);
        m_completedFrame = std::max(m_completedFrame, static_cast<i64>(frame));
    }
}

void VulkanDevice::runDeletions(bool all) {
    std::vector<Deletion> keep;
    for (auto& d : m_deletions) {
        if (all || static_cast<i64>(d.frame) <= m_completedFrame) d.fn();
        else keep.push_back(std::move(d));
    }
    m_deletions.swap(keep);
}

// =================================================================== swapchain
void VulkanDevice::destroySwapchain(VkSwapchainKHR swapchain) {
    if (swapchain) vkDestroySwapchainKHR(m_device, swapchain, nullptr);
    for (VkSemaphore s : m_renderFinished) vkDestroySemaphore(m_device, s, nullptr);
    m_renderFinished.clear();
    m_swapImages.clear();
    if (swapchain == m_swapchain) m_swapchain = VK_NULL_HANDLE;
}

void VulkanDevice::releaseSurface() {
    if (!m_surface) return;
    vkDeviceWaitIdle(m_device);
    if (m_swapchain) destroySwapchain(m_swapchain);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
    AGR_LOG_INFO("RHI", "Vulkan surface released (background)");
}

Status VulkanDevice::restoreSurface() {
    if (m_surface) return {};
    if (!m_platform.createSurface) return makeError(Error::Code::Unsupported, "no surface factory");
    m_surface = reinterpret_cast<VkSurfaceKHR>(m_platform.createSurface(m_instance));
    if (!m_surface) return makeError(Error::Code::Unsupported, "Vulkan surface recreation failed");
    VkBool32 present = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_physical, m_queueFamily, m_surface, &present);
    if (!present) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
        return makeError(Error::Code::Unsupported, "the new surface cannot be presented by the queue in use");
    }
    m_swapchainDirty = true;  // created at the next frame
    m_surfaceLost = false;
    AGR_LOG_INFO("RHI", "Vulkan surface restored");
    return {};
}

Status VulkanDevice::createSwapchain(u32 width, u32 height) {
    if (!m_surface) return makeError(Error::Code::Unsupported, "no surface (application in background)");
    vkDeviceWaitIdle(m_device);
    VkSurfaceCapabilitiesKHR caps;
    AGR_VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical, m_surface, &caps), "surface capabilities");
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == 0xFFFFFFFFu) {
        extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0) return makeError(Error::Code::Unsupported, "window minimized");
    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &formatCount, formats.data());
    if (formats.empty()) return makeError(Error::Code::Unsupported, "no surface format");
    // Shaders output gamma-encoded values: pick a UNORM (not sRGB) format.
    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats)
        if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            break;
        }
    u32 modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical, m_surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical, m_surface, &modeCount, modes.data());
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;  // always available, vsync
    if (!m_platform.vsync) {
        for (VkPresentModeKHR m : {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR})
            if (std::find(modes.begin(), modes.end(), m) != modes.end()) {
                mode = m;
                break;
            }
    }
    u32 imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);
    m_canBlitToSwapchain = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;
    if (!m_canBlitToSwapchain) return makeError(Error::Code::Unsupported, "swapchain images cannot be transfer destinations");

    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = m_surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = chosen.format;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                                                                                         : caps.currentTransform;
    sci.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
                             ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
                             : static_cast<VkCompositeAlphaFlagBitsKHR>(caps.supportedCompositeAlpha & -caps.supportedCompositeAlpha);
    sci.presentMode = mode;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = m_swapchain;
    VkSwapchainKHR fresh = VK_NULL_HANDLE;
    AGR_VK_TRY(vkCreateSwapchainKHR(m_device, &sci, nullptr, &fresh), "vkCreateSwapchainKHR");
    destroySwapchain(m_swapchain);
    m_swapchain = fresh;
    m_swapFormat = chosen.format;
    m_swapExtent = extent;
    u32 count = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr);
    m_swapImages.resize(count);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, m_swapImages.data());
    m_renderFinished.resize(count);
    for (auto& s : m_renderFinished) {
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(m_device, &si, nullptr, &s);
    }
    m_swapchainDirty = false;
    return {};
}

void VulkanDevice::ensureBackbuffer(u32 w, u32 h) {
    if (const VkTargetRes* bb = m_targets.get(m_backbuffer); bb && bb->width == w && bb->height == h) return;
    if (m_backbuffer) destroyRenderTarget(m_backbuffer);
    auto bb = createTarget({.width = w, .height = h, .colorFormat = TextureFormat::RGBA8, .depth = true, .samples = 1, .debugName = "agr.vk.backbuffer"}, true);
    if (!bb) {
        AGR_LOG_ERROR("RHI", "backbuffer creation failed: {}", bb.error().message);
        m_backbuffer = {};
        return;
    }
    m_backbuffer = bb.value();
    m_swapchainDirty = true;  // window size changed
}

// ================================================================ frame flow
void VulkanDevice::beginFrame(u32 width, u32 height) {
    if (m_recording) {
        AGR_LOG_ERROR("RHI", "beginFrame called twice without endFrame");
        return;
    }
    FrameData& f = m_frames[m_frame % kFrames];
    f64 waited = 0.0;
    if (f.submitted) {
        Stopwatch wait;
        vkWaitForFences(m_device, 1, &f.fence, VK_TRUE, UINT64_MAX);
        waited = wait.elapsedMilliseconds();
        m_completedFrame = std::max(m_completedFrame, static_cast<i64>(f.frameIndex));
        if (f.timestamps) {
            u64 ts[2] = {};
            if (vkGetQueryPoolResults(m_device, m_timestamps, (m_frame % kFrames) * 2, 2, sizeof ts, ts, sizeof(u64),
                                      VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
                const u64 delta = (ts[1] - ts[0]) & m_timestampMask;
                m_stats.gpuTimeMs = static_cast<f64>(delta) * m_props.limits.timestampPeriod / 1e6;
            }
        }
    }
    runDeletions(false);
    for (auto pool : f.descriptorPools) vkResetDescriptorPool(m_device, pool, 0);
    f.poolIndex = 0;
    const f64 gpu = m_stats.gpuTimeMs;
    const u64 frameIndex = m_stats.frameIndex + 1;
    m_stats = {};
    m_stats.gpuTimeMs = gpu;
    m_stats.cpuWaitMs = waited;
    m_stats.frameIndex = frameIndex;

    ensureBackbuffer(std::max(1u, width), std::max(1u, height));
    vkResetCommandPool(m_device, f.pool, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd, &bi);
    f.timestamps = m_info.gpuTimers;
    if (f.timestamps) {
        vkCmdResetQueryPool(f.cmd, m_timestamps, (m_frame % kFrames) * 2, 2);
        vkCmdWriteTimestamp(f.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_timestamps, (m_frame % kFrames) * 2);
    }
    m_recording = true;
    m_set = VK_NULL_HANDLE;
    m_descriptorKeyValid = false;
    m_boundVkPipeline = VK_NULL_HANDLE;
    m_pipelineDirty = m_vertexDirty = m_indexDirty = true;
}

void VulkanDevice::endFrame() {
    if (!m_recording) return;
    if (m_inPass) endPass();
    FrameData& f = m_frames[m_frame % kFrames];
    VkCommandBuffer c = f.cmd;
    VkTargetRes* bb = m_targets.get(m_backbuffer);

    u32 imageIndex = ~0u;
    if (bb) {
        if (m_surfaceLost && !m_surface) (void)restoreSurface();
        if (m_surface && (m_swapchainDirty || !m_swapchain)) {
            if (Status s = createSwapchain(bb->width, bb->height); !s) AGR_LOG_WARN("RHI", "swapchain: {}", s.error().message);
        }
        if (m_swapchain) {
            Stopwatch wait;
            VkResult r = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, f.imageAvailable, VK_NULL_HANDLE, &imageIndex);
            m_stats.cpuWaitMs += wait.elapsedMilliseconds();
            if (r == VK_ERROR_SURFACE_LOST_KHR) {  // the window went away: recreate everything next frame
                imageIndex = ~0u;
                releaseSurface();
                m_surfaceLost = true;
            } else if (r == VK_ERROR_OUT_OF_DATE_KHR) {
                imageIndex = ~0u;
                m_swapchainDirty = true;
            } else if (r == VK_SUBOPTIMAL_KHR) {
                m_swapchainDirty = true;
            } else if (r != VK_SUCCESS) {
                AGR_LOG_ERROR("RHI", "vkAcquireNextImageKHR: {}", resultName(r));
                imageIndex = ~0u;
            }
        }
    }
    if (imageIndex != ~0u && bb) {
        VkTextureRes* color = m_textures.get(bb->colorTexture);
        transition(c, color->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        Image swapImg;
        swapImg.image = m_swapImages[imageIndex];
        swapImg.layout = VK_IMAGE_LAYOUT_UNDEFINED;  // contents are fully overwritten
        transition(c, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageBlit region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.srcOffsets[1] = {static_cast<i32>(bb->width), static_cast<i32>(bb->height), 1};
        region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstOffsets[1] = {static_cast<i32>(m_swapExtent.width), static_cast<i32>(m_swapExtent.height), 1};
        const bool same = bb->width == m_swapExtent.width && bb->height == m_swapExtent.height;
        vkCmdBlitImage(c, color->img.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapImg.image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, same ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
        transition(c, swapImg, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    if (f.timestamps)
        vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timestamps, (m_frame % kFrames) * 2 + 1);
    vkEndCommandBuffer(c);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &c;
    if (imageIndex != ~0u) {
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &f.imageAvailable;
        si.pWaitDstStageMask = &waitStage;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &m_renderFinished[imageIndex];
    }
    vkResetFences(m_device, 1, &f.fence);
    if (VkResult r = vkQueueSubmit(m_queue, 1, &si, f.fence); r != VK_SUCCESS)
        AGR_LOG_ERROR("RHI", "vkQueueSubmit: {}", resultName(r));
    f.submitted = true;
    f.frameIndex = m_frame;

    if (imageIndex != ~0u) {
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &m_renderFinished[imageIndex];
        pi.swapchainCount = 1;
        pi.pSwapchains = &m_swapchain;
        pi.pImageIndices = &imageIndex;
        // Presentation may block (vsync, compositor, software WSI): accounted as waiting.
        Stopwatch wait;
        const VkResult r = vkQueuePresentKHR(m_queue, &pi);
        m_stats.cpuWaitMs += wait.elapsedMilliseconds();
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) m_swapchainDirty = true;
        else if (r != VK_SUCCESS) AGR_LOG_ERROR("RHI", "vkQueuePresentKHR: {}", resultName(r));
    }
    ++m_frame;
    m_recording = false;
}

void VulkanDevice::beginPass(RenderTargetHandle target, const ClearValues& clear) {
    if (!m_recording) {
        AGR_LOG_ERROR("RHI", "beginPass outside beginFrame / endFrame");
        return;
    }
    if (m_inPass) endPass();
    VkTargetRes* rt = m_targets.get(target ? target : m_backbuffer);
    if (!rt) {
        AGR_LOG_ERROR("RHI", "beginPass with an invalid render target");
        return;
    }
    VkCommandBuffer c = cmd();
    if (rt->hasColor) {
        Image& color = rt->samples > 1 ? rt->msaaColor : m_textures.get(rt->colorTexture)->img;
        transition(c, color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    if (rt->depthTexture) transition(c, m_textures.get(rt->depthTexture)->img, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    else if (rt->hasDepth) transition(c, rt->depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkClearValue values[2]{};
    u32 clearCount = 0;
    if (rt->hasColor) values[clearCount++].color = {{clear.color.x, clear.color.y, clear.color.z, clear.color.w}};
    if (rt->hasDepth) values[clearCount++].depthStencil = {clear.depth, 0};
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass = renderPass(rt->compat, clear.clearColor, clear.clearDepth);
    bi.framebuffer = rt->framebuffer;
    bi.renderArea = {{0, 0}, {rt->width, rt->height}};
    bi.clearValueCount = clearCount;
    bi.pClearValues = values;
    vkCmdBeginRenderPass(c, &bi, VK_SUBPASS_CONTENTS_INLINE);
    m_inPass = true;
    m_passW = rt->width;
    m_passH = rt->height;
    m_passCompat = rt->compat;
    m_boundVkPipeline = VK_NULL_HANDLE;
    m_pipelineDirty = true;
    setViewport(0, 0, rt->width, rt->height);
    setScissor(false, 0, 0, 0, 0);
}

void VulkanDevice::endPass() {
    if (!m_inPass) return;
    vkCmdEndRenderPass(cmd());
    m_inPass = false;
}

bool VulkanDevice::prepareDraw(bool indexed) {
    if (!m_inPass) {
        AGR_LOG_ERROR("RHI", "draw outside of a pass");
        return false;
    }
    VkCommandBuffer c = cmd();
    VkPipelineRes* p = m_pipelines.get(m_pipeline);
    if (!p) {
        AGR_LOG_ERROR("RHI", "draw without a valid pipeline");
        return false;
    }
    const VkPipeline vp = variant(*p, m_passCompat);
    if (!vp) return false;
    if (vp != m_boundVkPipeline) {
        vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, vp);
        m_boundVkPipeline = vp;
    }
    VkBufferRes* vb = m_buffers.get(m_vertexBuffer);
    if (!vb) {
        AGR_LOG_ERROR("RHI", "draw without a valid vertex buffer");
        return false;
    }
    vb->lastUsedFrame[vb->active] = m_frame;
    const VkDeviceSize vOffset = m_vertexOffset;
    vkCmdBindVertexBuffers(c, 0, 1, &vb->buffer[vb->active], &vOffset);
    if (p->state.instanceLayout.stride) {
        VkBufferRes* inst = m_buffers.get(m_instanceBuffer);
        if (!inst) {
            AGR_LOG_ERROR("RHI", "instanced pipeline drawn without a valid instance buffer");
            return false;
        }
        inst->lastUsedFrame[inst->active] = m_frame;
        const VkDeviceSize iOffset = m_instanceOffset;
        vkCmdBindVertexBuffers(c, 1, 1, &inst->buffer[inst->active], &iOffset);
    }
    if (indexed) {
        VkBufferRes* ib = m_buffers.get(m_indexBuffer);
        if (!ib) {
            AGR_LOG_ERROR("RHI", "indexed draw without a valid index buffer");
            return false;
        }
        ib->lastUsedFrame[ib->active] = m_frame;
        vkCmdBindIndexBuffer(c, ib->buffer[ib->active], m_indexOffset,
                             m_indexType == IndexType::U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    // Descriptor set: reused while buffers / ranges / textures are unchanged; only
    // the dynamic offsets differ between draws (per-object uniforms).
    std::array<u64, kMaxUniformSlots * 2 + kMaxTextureUnits * 2> key{};
    std::array<VkDescriptorBufferInfo, kMaxUniformSlots> bufInfos{};
    std::array<VkDescriptorImageInfo, kMaxTextureUnits> imgInfos{};
    std::array<u32, kMaxUniformSlots> offsets{};
    for (u32 i = 0; i < kMaxUniformSlots; ++i) {
        VkBufferRes* b = m_buffers.get(m_slots[i].buffer);
        if (b) {
            b->lastUsedFrame[b->active] = m_frame;
            bufInfos[i] = {b->buffer[b->active], 0, std::max<usize>(m_slots[i].size, 16)};
            offsets[i] = static_cast<u32>(m_slots[i].offset);
        } else {
            bufInfos[i] = {m_dummyUniformBuffer, 0, 256};
        }
        key[i * 2] = reinterpret_cast<u64>(bufInfos[i].buffer);
        key[i * 2 + 1] = bufInfos[i].range;
    }
    for (u32 i = 0; i < kMaxTextureUnits; ++i) {
        const VkTextureRes* t = m_textures.get(m_units[i]);
        if (t && t->img.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            imgInfos[i] = {t->sampler, t->img.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        else
            imgInfos[i] = {m_dummySampler, m_dummyView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        key[kMaxUniformSlots * 2 + i * 2] = reinterpret_cast<u64>(imgInfos[i].imageView);
        key[kMaxUniformSlots * 2 + i * 2 + 1] = reinterpret_cast<u64>(imgInfos[i].sampler);
    }
    bool rebind = false;
    if (!m_descriptorKeyValid || key != m_descriptorKey || !m_set) {
        FrameData& f = m_frames[m_frame % kFrames];
        VkDescriptorSet set = VK_NULL_HANDLE;
        for (;;) {
            if (f.poolIndex == f.descriptorPools.size()) {
                const VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kSetsPerPool * kMaxUniformSlots},
                                                      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kSetsPerPool * kMaxTextureUnits}};
                VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
                pi.maxSets = kSetsPerPool;
                pi.poolSizeCount = 2;
                pi.pPoolSizes = sizes;
                VkDescriptorPool pool;
                if (vkCreateDescriptorPool(m_device, &pi, nullptr, &pool) != VK_SUCCESS) {
                    AGR_LOG_ERROR("RHI", "descriptor pool allocation failed");
                    return false;
                }
                f.descriptorPools.push_back(pool);
            }
            VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            ai.descriptorPool = f.descriptorPools[f.poolIndex];
            ai.descriptorSetCount = 1;
            ai.pSetLayouts = &m_setLayout;
            const VkResult r = vkAllocateDescriptorSets(m_device, &ai, &set);
            if (r == VK_SUCCESS) break;
            ++f.poolIndex;  // pool exhausted: move to the next one
        }
        std::array<VkWriteDescriptorSet, kMaxUniformSlots + kMaxTextureUnits> writes{};
        for (u32 i = 0; i < writes.size(); ++i) {
            writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[i].dstSet = set;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            if (i < kMaxUniformSlots) {
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                writes[i].pBufferInfo = &bufInfos[i];
            } else {
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[i].pImageInfo = &imgInfos[i - kMaxUniformSlots];
            }
        }
        vkUpdateDescriptorSets(m_device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
        m_set = set;
        m_descriptorKey = key;
        m_descriptorKeyValid = true;
        rebind = true;
    }
    if (rebind || offsets != m_boundOffsets || m_pipelineDirty) {
        vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_set, kMaxUniformSlots,
                                offsets.data());
        m_boundOffsets = offsets;
        m_pipelineDirty = false;
    }
    return true;
}

void VulkanDevice::blit(RenderTargetHandle src, RenderTargetHandle dst, Filter filter) {
    if (!m_recording || m_inPass) {
        AGR_LOG_ERROR("RHI", "blit must be recorded inside a frame and outside a pass");
        return;
    }
    VkTargetRes* s = m_targets.get(src);
    VkTargetRes* d = m_targets.get(dst ? dst : m_backbuffer);
    if (!s || !d) return;
    VkCommandBuffer c = cmd();
    Image& dstImg = m_textures.get(d->colorTexture)->img;
    if (s->samples > 1) {
        if (s->width != d->width || s->height != d->height) {
            AGR_LOG_ERROR("RHI", "MSAA resolve requires equal sizes; resolve into a same-size target first");
            return;
        }
        transition(c, s->msaaColor, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        transition(c, dstImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageResolve region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.extent = {s->width, s->height, 1};
        vkCmdResolveImage(c, s->msaaColor.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImg.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        return;
    }
    Image& srcImg = m_textures.get(s->colorTexture)->img;
    transition(c, srcImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(c, dstImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageBlit region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[1] = {static_cast<i32>(s->width), static_cast<i32>(s->height), 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstOffsets[1] = {static_cast<i32>(d->width), static_cast<i32>(d->height), 1};
    const bool same = s->width == d->width && s->height == d->height;
    vkCmdBlitImage(c, srcImg.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                   &region, (filter == Filter::Linear && !same) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
}

Status VulkanDevice::readBackbuffer(u32 x, u32 y, u32 width, u32 height, std::vector<u8>& out) {
    if (m_recording) return makeError(Error::Code::InvalidArgument, "readBackbuffer must be called after endFrame");
    VkTargetRes* bb = m_targets.get(m_backbuffer);
    if (!bb) return makeError(Error::Code::InvalidArgument, "no frame rendered yet");
    if (width == 0 || height == 0 || x + width > bb->width || y + height > bb->height)
        return makeError(Error::Code::InvalidArgument, "read rectangle outside the backbuffer");
    vkDeviceWaitIdle(m_device);
    const usize bytes = static_cast<usize>(width) * height * 4;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = bytes;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer buffer;
    VmaAllocation alloc;
    VmaAllocationInfo info{};
    AGR_VK_TRY(vmaCreateBuffer(m_allocator, &bi, &ai, &buffer, &alloc, &info), "readback buffer");
    Image& img = m_textures.get(bb->colorTexture)->img;
    Status s = immediate([&](VkCommandBuffer c) {
        transition(c, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageOffset = {static_cast<i32>(x), static_cast<i32>(y), 0};  // top-left origin, like the RHI contract
        region.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(c, img.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
    });
    if (s) {
        vmaInvalidateAllocation(m_allocator, alloc, 0, VK_WHOLE_SIZE);
        out.assign(static_cast<const u8*>(info.pMappedData), static_cast<const u8*>(info.pMappedData) + bytes);
    }
    vmaDestroyBuffer(m_allocator, buffer, alloc);
    return s;
}

} // namespace
} // namespace vk

Result<std::unique_ptr<Device>> createVulkanDevice(const VulkanPlatform& platform) {
    auto device = std::make_unique<vk::VulkanDevice>();
    if (Status s = device->initialize(platform); !s) return s.error();
    return std::unique_ptr<Device>(std::move(device));
}

} // namespace agr::rhi
