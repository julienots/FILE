#pragma once
#include <span>
#include <string_view>
#include <vector>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"

namespace agr {

/// CPU image, always RGBA8, top row first.
struct Image {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> pixels;

    bool empty() const { return pixels.empty(); }
    usize byteSize() const { return pixels.size(); }
};

/// Decodes PNG, JPEG, TGA, BMP, PSD (flattened), GIF (first frame), HDR (tone-clamped).
/// WEBP and SVG are not supported yet (planned: libwebp, nanosvg).
Result<Image> decodeImage(std::span<const u8> encoded, std::string_view nameForErrors = "");

/// Halves width and height with a 2x2 box filter (odd edges are clamped).
Image downscaleHalf(const Image& src);
/// Repeatedly halves until both dimensions are <= maxSize. Returns the number of halvings.
u32 fitToMaxSize(Image& image, u32 maxSize);

/// PNG encoding (screenshots, thumbnails, generated assets).
Result<ByteBuffer> encodePng(u32 width, u32 height, std::span<const u8> rgba);

} // namespace agr
