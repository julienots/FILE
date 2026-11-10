#pragma once
// Translation of the AGR GLSL dialect (see rhi::ShaderSource) for each backend.
#include <string>
#include <unordered_map>

#include "agr/core/Result.hpp"
#include "agr/rhi/Rhi.hpp"

namespace agr::rhi {

/// Descriptor binding layout shared by every Vulkan pipeline (set 0):
/// uniform slot i -> binding i, texture unit i -> binding kTextureBindingBase + i.
inline constexpr u32 kMaxUniformSlots = 8;
inline constexpr u32 kMaxTextureUnits = 8;
inline constexpr u32 kTextureBindingBase = kMaxUniformSlots;

enum class ShaderStage : u8 { Vertex, Fragment };

/// Rewrites a dialect shader into Vulkan GLSL 4.50:
///  * `#version 450` header and `#define AGR_VULKAN 1`;
///  * `layout(std140) uniform Name` -> explicit set / binding from desc.uniformBlocks;
///  * `uniform sampler2D name;` -> explicit set / binding from desc.samplers;
///  * vertex outputs get locations in declaration order, fragment inputs reuse the
///    location of the vertex output with the same name (`varyings` is filled by the
///    vertex pass and read by the fragment pass).
/// Declarations must be on one line (dialect rule). Errors name the offending symbol.
Result<std::string> translateForVulkan(const std::string& source, ShaderStage stage, const PipelineDesc& desc,
                                       std::unordered_map<std::string, u32>& varyings);

} // namespace agr::rhi
