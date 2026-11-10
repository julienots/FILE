#pragma once
#include <string_view>

#include "agr/core/Result.hpp"
#include "agr/rhi/Rhi.hpp"

namespace agr {

/// Quality tiers, cheapest first.
enum class GraphicsTier : i32 { UltraMobile = 0, Low = 1, Medium = 2, High = 3, Ultra = 4 };

/// Every setting here is consumed by the renderer today. New costly features
/// (shadows, SSAO, bloom...) must add a field and stay off on low tiers.
struct GraphicsProfile {
    GraphicsTier tier = GraphicsTier::High;
    const char* name = "high";
    float renderScale = 1.0f;    ///< 3D resolution relative to the screen (upscaled with bilinear filtering).
    u32 msaaSamples = 4;         ///< Clamped to the device maximum.
    u32 maxTextureSize = 4096;   ///< Larger textures are downscaled at load time.
    u32 targetFps = 0;           ///< Frame cap (0 = vsync / uncapped).
    u64 uploadBudgetBytes = 16ull * 1024 * 1024;  ///< Texture bytes uploaded per frame at most.
};

GraphicsProfile graphicsProfile(GraphicsTier tier);
/// "ultra", "high", "medium", "low", "ultra-mobile".
Result<GraphicsTier> parseGraphicsTier(std::string_view name);
/// Heuristic choice from device capabilities. Documented in docs/RENDERING.md.
GraphicsTier recommendGraphicsTier(const rhi::DeviceInfo& device, bool mobilePlatform);

} // namespace agr
