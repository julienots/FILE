#pragma once
#include <string>
#include <vector>

#include "agr/assets/Image.hpp"
#include "agr/render/SpriteSheet.hpp"

namespace agr {

struct AtlasInput {
    std::string name;
    Image image;
};

struct AtlasResult {
    Image atlas;
    std::vector<SpriteFrame> frames;  ///< In input order.
};

/// Packs images into one texture (shelf packing, tallest first). Each image is
/// surrounded by `padding` pixels that repeat its border (no color bleeding
/// with linear filtering or mipmaps). The atlas grows in powers of two up to
/// maxSize; fails when everything cannot fit.
Result<AtlasResult> packAtlas(const std::vector<AtlasInput>& inputs, u32 maxSize = 4096, u32 padding = 2);

/// Packs image files into `outImageUri` (PNG) and writes `outSheetUri` (.agrsheet)
/// whose frame names are the file names without extension.
Result<SpriteSheet> buildAtlasFiles(VirtualFileSystem& vfs, const std::vector<std::string>& imageUris, const std::string& outImageUri,
                                    const std::string& outSheetUri, float pixelsPerUnit = 100.0f, bool pixelArt = false);

/// Writes a sheet that cuts `imageUri` into a grid of cellWidth x cellHeight pixels.
Result<SpriteSheet> buildGridSheetFile(VirtualFileSystem& vfs, const std::string& imageUri, u32 cellWidth, u32 cellHeight,
                                       const std::string& outSheetUri, u32 margin = 0, u32 spacing = 0, bool pixelArt = false);

} // namespace agr
