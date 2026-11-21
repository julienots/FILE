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

std::vector<SpriteFrame> autoSliceFrames(const Image& img, u8 alphaThreshold, u32 minSize, u32 mergeDistance, const std::string& namePrefix) {
    struct Box {
        u32 x0, y0, x1, y1;  // inclusive
    };
    std::vector<Box> boxes;
    const u32 w = img.width, h = img.height;
    if (w == 0 || h == 0 || img.pixels.size() < static_cast<usize>(w) * h * 4) return {};
    std::vector<u8> seen(static_cast<usize>(w) * h, 0);
    auto opaque = [&](u32 x, u32 y) { return img.pixels[(static_cast<usize>(y) * w + x) * 4 + 3] > alphaThreshold; };
    std::vector<std::pair<u32, u32>> stack;
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x) {
            const usize i = static_cast<usize>(y) * w + x;
            if (seen[i] || !opaque(x, y)) continue;
            Box b{x, y, x, y};
            seen[i] = 1;
            stack.push_back({x, y});
            while (!stack.empty()) {  // flood fill (8 neighbours)
                const auto [cx, cy] = stack.back();
                stack.pop_back();
                b.x0 = std::min(b.x0, cx), b.y0 = std::min(b.y0, cy), b.x1 = std::max(b.x1, cx), b.y1 = std::max(b.y1, cy);
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const i64 nx = static_cast<i64>(cx) + dx, ny = static_cast<i64>(cy) + dy;
                        if (nx < 0 || ny < 0 || nx >= static_cast<i64>(w) || ny >= static_cast<i64>(h)) continue;
                        const usize ni = static_cast<usize>(ny) * w + static_cast<usize>(nx);
                        if (seen[ni] || !opaque(static_cast<u32>(nx), static_cast<u32>(ny))) continue;
                        seen[ni] = 1;
                        stack.push_back({static_cast<u32>(nx), static_cast<u32>(ny)});
                    }
            }
            boxes.push_back(b);
        }
    // Merge boxes closer than mergeDistance (a sword next to its hero, the dot of an "i").
    const i64 md = mergeDistance;
    for (bool merged = true; merged;) {
        merged = false;
        for (usize a = 0; a < boxes.size() && !merged; ++a)
            for (usize c = a + 1; c < boxes.size(); ++c) {
                const Box& p = boxes[a];
                const Box& q = boxes[c];
                const i64 gapX = std::max<i64>({0, static_cast<i64>(q.x0) - static_cast<i64>(p.x1) - 1, static_cast<i64>(p.x0) - static_cast<i64>(q.x1) - 1});
                const i64 gapY = std::max<i64>({0, static_cast<i64>(q.y0) - static_cast<i64>(p.y1) - 1, static_cast<i64>(p.y0) - static_cast<i64>(q.y1) - 1});
                if (gapX <= md && gapY <= md && (md > 0 || (gapX == 0 && gapY == 0))) {
                    boxes[a] = {std::min(p.x0, q.x0), std::min(p.y0, q.y0), std::max(p.x1, q.x1), std::max(p.y1, q.y1)};
                    boxes.erase(boxes.begin() + static_cast<std::ptrdiff_t>(c));
                    merged = true;
                    break;
                }
            }
    }
    std::erase_if(boxes, [&](const Box& b) { return b.x1 - b.x0 + 1 < minSize && b.y1 - b.y0 + 1 < minSize; });
    // Rows: a box starts a new row when it lies below the previous row's band.
    std::sort(boxes.begin(), boxes.end(), [](const Box& a, const Box& b) { return a.y0 < b.y0 || (a.y0 == b.y0 && a.x0 < b.x0); });
    std::vector<std::vector<Box>> rows;
    u32 rowBottom = 0;
    for (const Box& b : boxes) {
        if (rows.empty() || b.y0 > rowBottom) {
            rows.push_back({});
            rowBottom = b.y1;
        }
        rows.back().push_back(b);
        rowBottom = std::max(rowBottom, b.y1);
    }
    std::vector<SpriteFrame> frames;
    for (auto& row : rows) {
        std::sort(row.begin(), row.end(), [](const Box& a, const Box& b) { return a.x0 < b.x0; });
        for (const Box& b : row) {
            SpriteFrame f;
            f.name = namePrefix + "_" + std::to_string(frames.size());
            f.x = b.x0, f.y = b.y0, f.w = b.x1 - b.x0 + 1, f.h = b.y1 - b.y0 + 1;
            frames.push_back(std::move(f));
        }
    }
    return frames;
}

Result<SpriteSheet> buildAutoSliceSheetFile(VirtualFileSystem& vfs, const std::string& imageUri, const std::string& outSheetUri, u8 alphaThreshold,
                                            u32 minSize, u32 mergeDistance, bool pixelArt) {
    auto bytes = vfs.readBytes(imageUri);
    if (!bytes) return bytes.error();
    auto img = decodeImage(bytes.value(), imageUri);
    if (!img) return img.error();
    const std::string stem = std::filesystem::path(imageUri.substr(imageUri.find_last_of('/') + 1)).stem().string();
    SpriteSheet sheet;
    sheet.texture = imageUri;
    sheet.textureWidth = img.value().width;
    sheet.textureHeight = img.value().height;
    sheet.pixelArt = pixelArt;
    sheet.frames = autoSliceFrames(img.value(), alphaThreshold, minSize, mergeDistance, stem);
    if (sheet.frames.empty()) return makeError(Error::Code::InvalidArgument, "no opaque pixels to slice");
    if (Status s = vfs.writeText(outSheetUri, sheet.toJson()); !s) return s.error();
    return sheet;
}

} // namespace agr
