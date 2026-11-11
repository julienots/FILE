#include "agr/assets/Image.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-align"
    #pragma GCC diagnostic ignored "-Wshadow"
    #pragma GCC diagnostic ignored "-Wnull-dereference"
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    #pragma GCC diagnostic ignored "-Wunused-function"
    #pragma GCC diagnostic ignored "-Wsign-compare"
    #pragma GCC diagnostic ignored "-Wconversion"
    #if !defined(__clang__)
        #pragma GCC diagnostic ignored "-Wduplicated-branches"
    #endif
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include <stb_image_write.h>
#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

namespace agr {

Result<Image> decodeImage(std::span<const u8> encoded, std::string_view name) {
    if (encoded.empty()) return makeError(Error::Code::ParseError, "empty image data: '" + std::string(name) + "'");
    if (encoded.size() > static_cast<usize>(INT32_MAX)) return makeError(Error::Code::Unsupported, "image file too large");
    int w = 0, h = 0, channels = 0;
    stbi_uc* data = stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &w, &h, &channels, 4);
    if (!data) {
        return makeError(Error::Code::ParseError,
                         "cannot decode image '" + std::string(name) + "': " + stbi_failure_reason());
    }
    Image img;
    img.width = static_cast<u32>(w);
    img.height = static_cast<u32>(h);
    img.pixels.assign(data, data + static_cast<usize>(w) * static_cast<usize>(h) * 4);
    stbi_image_free(data);
    return img;
}

Result<ImageF> decodeImageLinear(std::span<const u8> encoded, std::string_view name) {
    if (encoded.empty()) return makeError(Error::Code::ParseError, "empty image data: '" + std::string(name) + "'");
    if (encoded.size() > static_cast<usize>(INT32_MAX)) return makeError(Error::Code::Unsupported, "image file too large");
    const int size = static_cast<int>(encoded.size());
    ImageF img;
    int w = 0, h = 0, channels = 0;
    if (stbi_is_hdr_from_memory(encoded.data(), size)) {
        float* data = stbi_loadf_from_memory(encoded.data(), size, &w, &h, &channels, 3);
        if (!data) return makeError(Error::Code::ParseError, "cannot decode image '" + std::string(name) + "': " + stbi_failure_reason());
        img.rgb.assign(data, data + static_cast<usize>(w) * static_cast<usize>(h) * 3);
        stbi_image_free(data);
        for (float& v : img.rgb) v = std::isfinite(v) ? std::max(v, 0.0f) : 0.0f;
    } else {
        stbi_uc* data = stbi_load_from_memory(encoded.data(), size, &w, &h, &channels, 3);
        if (!data) return makeError(Error::Code::ParseError, "cannot decode image '" + std::string(name) + "': " + stbi_failure_reason());
        float table[256];
        for (int i = 0; i < 256; ++i) {
            const float c = static_cast<float>(i) / 255.0f;
            table[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }
        const usize n = static_cast<usize>(w) * static_cast<usize>(h) * 3;
        img.rgb.resize(n);
        for (usize i = 0; i < n; ++i) img.rgb[i] = table[data[i]];
        stbi_image_free(data);
    }
    img.width = static_cast<u32>(w);
    img.height = static_cast<u32>(h);
    return img;
}

Image downscaleHalf(const Image& src) {
    Image dst;
    dst.width = std::max(1u, src.width / 2);
    dst.height = std::max(1u, src.height / 2);
    dst.pixels.resize(static_cast<usize>(dst.width) * dst.height * 4);
    for (u32 y = 0; y < dst.height; ++y) {
        for (u32 x = 0; x < dst.width; ++x) {
            const u32 x0 = std::min(x * 2, src.width - 1), x1 = std::min(x * 2 + 1, src.width - 1);
            const u32 y0 = std::min(y * 2, src.height - 1), y1 = std::min(y * 2 + 1, src.height - 1);
            for (u32 c = 0; c < 4; ++c) {
                auto at = [&](u32 px, u32 py) { return static_cast<u32>(src.pixels[(static_cast<usize>(py) * src.width + px) * 4 + c]); };
                dst.pixels[(static_cast<usize>(y) * dst.width + x) * 4 + c] =
                    static_cast<u8>((at(x0, y0) + at(x1, y0) + at(x0, y1) + at(x1, y1) + 2) / 4);
            }
        }
    }
    return dst;
}

u32 fitToMaxSize(Image& image, u32 maxSize) {
    u32 halvings = 0;
    maxSize = std::max(1u, maxSize);
    while (image.width > maxSize || image.height > maxSize) {
        image = downscaleHalf(image);
        ++halvings;
    }
    return halvings;
}

Result<ByteBuffer> encodePng(u32 width, u32 height, std::span<const u8> rgba) {
    if (width == 0 || height == 0 || rgba.size() < static_cast<usize>(width) * height * 4)
        return makeError(Error::Code::InvalidArgument, "encodePng: invalid image size");
    ByteBuffer out;
    auto write = [](void* ctx, void* data, int size) {
        auto* buffer = static_cast<ByteBuffer*>(ctx);
        const auto* bytes = static_cast<const u8*>(data);
        buffer->insert(buffer->end(), bytes, bytes + size);
    };
    const int ok = stbi_write_png_to_func(write, &out, static_cast<int>(width), static_cast<int>(height), 4, rgba.data(),
                                          static_cast<int>(width * 4));
    if (!ok) return makeError(Error::Code::Unknown, "PNG encoding failed");
    return out;
}

Result<ByteBuffer> encodeHdr(u32 width, u32 height, std::span<const float> rgb) {
    if (width == 0 || height == 0 || rgb.size() < static_cast<usize>(width) * height * 3)
        return makeError(Error::Code::InvalidArgument, "encodeHdr: invalid image size");
    // Radiance RGBE, flat (uncompressed) scanlines, top row first.
    const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y " + std::to_string(height) + " +X " + std::to_string(width) + "\n";
    ByteBuffer out(header.begin(), header.end());
    out.reserve(out.size() + static_cast<usize>(width) * height * 4);
    for (usize i = 0; i < static_cast<usize>(width) * height; ++i) {
        const float r = std::max(rgb[i * 3], 0.0f), g = std::max(rgb[i * 3 + 1], 0.0f), b = std::max(rgb[i * 3 + 2], 0.0f);
        const float m = std::max({r, g, b});
        if (!(m >= 1e-32f) || !std::isfinite(m)) {
            out.insert(out.end(), {0, 0, 0, 0});
            continue;
        }
        int e = 0;
        const float scale = std::frexp(m, &e) * 256.0f / m;
        out.push_back(static_cast<u8>(std::min(255.0f, r * scale)));
        out.push_back(static_cast<u8>(std::min(255.0f, g * scale)));
        out.push_back(static_cast<u8>(std::min(255.0f, b * scale)));
        out.push_back(static_cast<u8>(std::clamp(e + 128, 0, 255)));
    }
    return out;
}

} // namespace agr
