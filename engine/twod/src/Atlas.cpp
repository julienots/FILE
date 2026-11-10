#include "agr/twod/Atlas.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <filesystem>
#include <numeric>

namespace agr {

namespace {

/// Copies `src` at (x, y) and extrudes its border into the padding ring.
void blitExtruded(Image& dst, const Image& src, u32 x, u32 y, u32 pad) {
    for (i64 dy = -static_cast<i64>(pad); dy < static_cast<i64>(src.height + pad); ++dy) {
        const u32 sy = static_cast<u32>(std::clamp<i64>(dy, 0, static_cast<i64>(src.height) - 1));
        for (i64 dx = -static_cast<i64>(pad); dx < static_cast<i64>(src.width + pad); ++dx) {
            const u32 sx = static_cast<u32>(std::clamp<i64>(dx, 0, static_cast<i64>(src.width) - 1));
            const usize di = (static_cast<usize>(y + dy) * dst.width + static_cast<usize>(x + dx)) * 4;
            std::memcpy(&dst.pixels[di], &src.pixels[(static_cast<usize>(sy) * src.width + sx) * 4], 4);
        }
    }
}

struct Placement {
    u32 x, y;
};

/// Shelf packing into a width x height box; returns false when it does not fit.
bool tryPack(const std::vector<AtlasInput>& in, const std::vector<usize>& order, u32 width, u32 height, u32 pad,
             std::vector<Placement>& out) {
    out.assign(in.size(), {0, 0});
    u32 x = 0, y = 0, shelfH = 0;
    for (usize idx : order) {
        const u32 w = in[idx].image.width + 2 * pad, h = in[idx].image.height + 2 * pad;
        if (w > width) return false;
        if (x + w > width) {  // next shelf
            y += shelfH;
            x = 0;
            shelfH = 0;
        }
        if (y + h > height) return false;
        out[idx] = {x + pad, y + pad};
        x += w;
        shelfH = std::max(shelfH, h);
    }
    return true;
}

} // namespace

Result<AtlasResult> packAtlas(const std::vector<AtlasInput>& inputs, u32 maxSize, u32 pad) {
    if (inputs.empty()) return makeError(Error::Code::InvalidArgument, "no image to pack");
    u64 area = 0;
    u32 widest = 0;
    for (const auto& in : inputs) {
        if (in.image.empty()) return makeError(Error::Code::InvalidArgument, "image '" + in.name + "' is empty");
        area += static_cast<u64>(in.image.width + 2 * pad) * (in.image.height + 2 * pad);
        widest = std::max(widest, in.image.width + 2 * pad);
    }
    std::vector<usize> order(inputs.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](usize a, usize b) { return inputs[a].image.height > inputs[b].image.height; });

    std::vector<Placement> placement;
    u32 width = std::bit_ceil(std::max<u32>(widest, static_cast<u32>(std::ceil(std::sqrt(static_cast<double>(area))))));
    u32 height = width;
    // Try w x h, w x 2h, 2w x 2h... until it fits or exceeds the limit.
    for (int attempt = 0; attempt < 32 && width <= maxSize && height <= maxSize; ++attempt) {
        for (u32 h = std::max(64u, width / 2); h <= std::min(height * 2, maxSize); h *= 2) {
            if (tryPack(inputs, order, width, h, pad, placement)) {
                AtlasResult r;
                r.atlas.width = width;
                r.atlas.height = h;
                r.atlas.pixels.assign(static_cast<usize>(width) * h * 4, 0);
                for (usize i = 0; i < inputs.size(); ++i) {
                    blitExtruded(r.atlas, inputs[i].image, placement[i].x, placement[i].y, pad);
                    r.frames.push_back({inputs[i].name, placement[i].x, placement[i].y, inputs[i].image.width, inputs[i].image.height, {0.5f, 0.5f}});
                }
                return r;
            }
        }
        width *= 2;
        height *= 2;
    }
    return makeError(Error::Code::Unsupported, "images do not fit in a " + std::to_string(maxSize) + " x " + std::to_string(maxSize) + " atlas");
}

Result<SpriteSheet> buildAtlasFiles(VirtualFileSystem& vfs, const std::vector<std::string>& uris, const std::string& outImageUri,
                                    const std::string& outSheetUri, float pixelsPerUnit, bool pixelArt) {
    std::vector<AtlasInput> inputs;
    for (const std::string& uri : uris) {
        auto bytes = vfs.readBytes(uri);
        if (!bytes) return bytes.error();
        auto img = decodeImage(bytes.value(), uri);
        if (!img) return img.error();
        const auto slash = uri.find_last_of('/');
        inputs.push_back({std::filesystem::path(uri.substr(slash == std::string::npos ? 0 : slash + 1)).stem().string(), std::move(img.value())});
    }
    auto packed = packAtlas(inputs);
    if (!packed) return packed.error();
    auto png = encodePng(packed.value().atlas.width, packed.value().atlas.height, packed.value().atlas.pixels);
    if (!png) return png.error();
    if (Status s = vfs.writeBytes(outImageUri, png.value().data(), png.value().size()); !s) return s.error();
    SpriteSheet sheet;
    sheet.texture = outImageUri;
    sheet.textureWidth = packed.value().atlas.width;
    sheet.textureHeight = packed.value().atlas.height;
    sheet.pixelsPerUnit = pixelsPerUnit;
    sheet.pixelArt = pixelArt;
    sheet.frames = std::move(packed.value().frames);
    if (Status s = vfs.writeText(outSheetUri, sheet.toJson()); !s) return s.error();
    return sheet;
}

Result<SpriteSheet> buildGridSheetFile(VirtualFileSystem& vfs, const std::string& imageUri, u32 cw, u32 ch, const std::string& outSheetUri,
                                       u32 margin, u32 spacing, bool pixelArt) {
    if (cw == 0 || ch == 0) return makeError(Error::Code::InvalidArgument, "cell size must be > 0");
    auto bytes = vfs.readBytes(imageUri);
    if (!bytes) return bytes.error();
    auto img = decodeImage(bytes.value(), imageUri);
    if (!img) return img.error();
    const std::string stem = std::filesystem::path(imageUri.substr(imageUri.find_last_of('/') + 1)).stem().string();
    SpriteSheet sheet = SpriteSheet::grid(imageUri, img.value().width, img.value().height, cw, ch, margin, spacing, stem);
    if (sheet.frames.empty()) return makeError(Error::Code::InvalidArgument, "the cell size is larger than the image");
    sheet.pixelArt = pixelArt;
    if (Status s = vfs.writeText(outSheetUri, sheet.toJson()); !s) return s.error();
    return sheet;
}

} // namespace agr
