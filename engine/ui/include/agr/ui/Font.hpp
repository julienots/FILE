#pragma once
#include <memory>
#include <string_view>
#include <vector>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"
#include "agr/rhi/Rhi.hpp"
#include "agr/ui/UIComponents.hpp"

namespace agr {

struct TextGlyph {
    math::Vec2 min, max;  ///< Pixels, relative to the top-left corner of the text block.
    math::Vec4 uv;        ///< u0, v0, u1, v1 in the glyph atlas.
};

struct TextLayout {
    std::vector<TextGlyph> glyphs;
    math::Vec2 size{0.0f, 0.0f};  ///< Width of the widest line, total height.
    u32 lines = 0;
};

struct FontStats {
    u32 fonts = 0;
    u32 glyphs = 0;         ///< Rasterized glyphs in the atlas.
    float atlasUsage = 0;   ///< 0..1 of the atlas height used.
    u32 atlasResets = 0;
};

/// TrueType fonts (stb_truetype) rasterized on demand, per pixel size, into one
/// shared glyph atlas texture. Text layout: UTF-8, kerning, line breaks ("\n"),
/// word wrapping and horizontal alignment.
class FontLibrary {
public:
    static constexpr u32 kAtlasSize = 1024;

    explicit FontLibrary(VirtualFileSystem& vfs);
    ~FontLibrary();
    FontLibrary(const FontLibrary&) = delete;
    FontLibrary& operator=(const FontLibrary&) = delete;

    /// GPU device for the atlas texture (null: layout and rasterization only, e.g. tests).
    void setDevice(rhi::Device* device);

    /// Lays out `text` with a line height of `pixelSize` pixels. Lines are aligned in
    /// `boxWidth`; with `wrap`, they break at spaces to fit it.
    Result<TextLayout> layout(std::string_view fontUri, std::string_view text, float pixelSize, float boxWidth, bool wrap,
                              float lineSpacing, UIHAlign align);

    /// Uploads the glyphs rasterized since the last call (outside of render passes).
    void flush();
    rhi::TextureHandle texture() const;
    /// The atlas filled up during a layout: some glyphs are missing. resetAtlas() and lay out again.
    bool overflowed() const;
    void resetAtlas();
    const FontStats& stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace agr
