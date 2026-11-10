#pragma once
#include <volk.h>

#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wnull-dereference"
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    #if defined(__clang__)
        #pragma GCC diagnostic ignored "-Wnullability-completeness"
    #endif
#endif
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>
#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

#include <string>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/rhi/Rhi.hpp"

namespace agr::rhi::vk {

const char* resultName(VkResult r);

/// Compiles an AGR dialect pipeline to SPIR-V (vertex + fragment).
struct SpirvPair {
    std::vector<u32> vertex;
    std::vector<u32> fragment;
};
Result<SpirvPair> compileToSpirv(const PipelineDesc& desc);

} // namespace agr::rhi::vk
