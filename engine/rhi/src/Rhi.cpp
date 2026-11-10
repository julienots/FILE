#include "agr/rhi/Rhi.hpp"

namespace agr::rhi {

const char* enumName(Backend backend) {
    switch (backend) {
        case Backend::OpenGL: return "OpenGL";
        case Backend::OpenGLES: return "OpenGL ES";
        case Backend::Vulkan: return "Vulkan";
    }
    return "?";
}

u32 bytesPerPixel(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA8:
        case TextureFormat::RGBA8_sRGB:
        case TextureFormat::Depth24Stencil8: return 4;
        case TextureFormat::R8: return 1;
        case TextureFormat::RGBA16F: return 8;
        case TextureFormat::Depth16: return 2;
    }
    return 4;
}

#if !defined(AGR_RHI_VULKAN)
bool vulkanBackendCompiled() { return false; }
Result<std::unique_ptr<Device>> createVulkanDevice(const VulkanPlatform&) {
    return makeError(Error::Code::Unsupported, "engine built without the Vulkan backend (AGR_ENABLE_VULKAN=OFF)");
}
#endif

} // namespace agr::rhi
