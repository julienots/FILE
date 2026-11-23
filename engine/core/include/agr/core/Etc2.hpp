#pragma once
// ETC2 texture compression (RGBA8 ETC2 + EAC alpha, 16 bytes per 4x4 block, 8 bits per pixel)
// and the KTX 1 container. ETC2 is the compressed format every OpenGL ES 3.0 device samples
// natively: exported Android games use 4x less texture memory than RGBA8.
//  * encodeEtc2Rgba: ETC1-compatible individual / differential modes (both flips, every table)
//    for the color, a fitted EAC table per block for the alpha;
//  * decodeEtc2Rgba: the whole format (individual, differential, T, H and planar color modes)
//    for GPUs without ETC2 and for tools.
#include <span>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::etc2 {

constexpr u32 kGlRgba8Etc2Eac = 0x9278;       ///< GL_COMPRESSED_RGBA8_ETC2_EAC
constexpr u32 kGlSrgb8Alpha8Etc2Eac = 0x9279; ///< GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC

/// Bytes of one level of `width` x `height` (blocks of 4x4, partial blocks padded).
inline usize levelBytes(u32 width, u32 height) { return static_cast<usize>((width + 3) / 4) * ((height + 3) / 4) * 16; }

/// RGBA8 pixels (top row first) -> blocks.
std::vector<u8> encodeEtc2Rgba(const u8* rgba, u32 width, u32 height);
/// Blocks -> RGBA8 pixels (top row first).
void decodeEtc2Rgba(const u8* blocks, u32 width, u32 height, u8* rgba);

/// A compressed texture with its mip levels (level 0 first).
struct CompressedImage {
    u32 width = 0, height = 0;
    u32 glFormat = kGlRgba8Etc2Eac;
    std::vector<std::vector<u8>> levels;
};
/// Compresses RGBA8 pixels with a full mip chain (box-filtered down to 1x1).
CompressedImage compressWithMips(const u8* rgba, u32 width, u32 height);

std::vector<u8> writeKtx(const CompressedImage& image);
bool isKtx(std::span<const u8> bytes);
Result<CompressedImage> readKtx(std::span<const u8> bytes);

} // namespace agr::etc2
