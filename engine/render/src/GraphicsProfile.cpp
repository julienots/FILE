#include "agr/render/GraphicsProfile.hpp"

#include <string>

namespace agr {

GraphicsProfile graphicsProfile(GraphicsTier tier) {
    GraphicsProfile p;
    p.tier = tier;
    switch (tier) {
        case GraphicsTier::Ultra:
            p = {tier, "ultra", 1.0f, 8, 8192, 0, 64ull << 20, 4096, true, 8, true, true, 4};
            break;
        case GraphicsTier::High:
            p = {tier, "high", 1.0f, 4, 4096, 0, 32ull << 20, 2048, true, 8, true, true, 3};
            break;
        case GraphicsTier::Medium:
            p = {tier, "medium", 1.0f, 2, 2048, 0, 16ull << 20, 1024, true, 4, true, true, 2};
            break;
        case GraphicsTier::Low:
            p = {tier, "low", 0.75f, 1, 1024, 60, 8ull << 20, 1024, false, 4, false, true, 1};
            break;
        case GraphicsTier::UltraMobile:
            p = {tier, "ultra-mobile", 0.5f, 1, 512, 30, 4ull << 20, 0, false, 2, false, false, 1};
            break;
    }
    return p;
}

Result<GraphicsTier> parseGraphicsTier(std::string_view name) {
    if (name == "ultra") return GraphicsTier::Ultra;
    if (name == "high") return GraphicsTier::High;
    if (name == "medium") return GraphicsTier::Medium;
    if (name == "low") return GraphicsTier::Low;
    if (name == "ultra-mobile") return GraphicsTier::UltraMobile;
    return makeError(Error::Code::InvalidArgument, "unknown graphics profile '" + std::string(name) + "'");
}

GraphicsTier recommendGraphicsTier(const rhi::DeviceInfo& d, bool mobilePlatform) {
    // Software rasterizers (CI, VMs, remote desktops) cannot afford MSAA or full resolution.
    if (d.softwareRenderer) return GraphicsTier::Low;
    if (mobilePlatform || d.backend == rhi::Backend::OpenGLES) {
        if (d.maxTextureSize >= 8192 && d.maxSamples >= 4) return GraphicsTier::Medium;
        if (d.maxTextureSize >= 4096) return GraphicsTier::Low;
        return GraphicsTier::UltraMobile;
    }
    // Desktop hardware. Ultra is never chosen automatically: it is an explicit opt-in.
    return GraphicsTier::High;
}

} // namespace agr
