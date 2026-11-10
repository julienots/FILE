#include "agr/ui/Font.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include "agr/core/Log.hpp"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace agr {

namespace {

/// Next code point of a UTF-8 string (U+FFFD for invalid sequences).
u32 nextCodepoint(std::string_view s, usize& i) {
    const auto byte = [&](usize k) { return static_cast<u8>(s[k]); };
    const u8 c = byte(i++);
    if (c < 0x80) return c;
    u32 cp = 0;
    int extra = 0;
    if ((c & 0xE0) == 0xC0) cp = c & 0x1F, extra = 1;
    else if ((c & 0xF0) == 0xE0) cp = c & 0x0F, extra = 2;
    else if ((c & 0xF8) == 0xF0) cp = c & 0x07, extra = 3;
    else return 0xFFFD;
    for (int k = 0; k < extra; ++k) {
        if (i >= s.size() || (byte(i) & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (byte(i++) & 0x3F);
    }
    return cp > 0x10FFFF ? 0xFFFD : cp;
}

struct Glyph {
    math::Vec4 uv{0, 0, 0, 0};
    float x0 = 0, y0 = 0, w = 0, h = 0;  // bitmap offset from the pen (baseline) and size, pixels
    float advance = 0;
    int index = 0;
    bool valid = false;  // false: atlas was full
};

struct Font {
    std::vector<u8> data;
    stbtt_fontinfo info{};
    int ascent = 0, descent = 0, lineGap = 0;
};

} // namespace

struct FontLibrary::Impl {
    VirtualFileSystem& vfs;
    rhi::Device* device = nullptr;
    std::unordered_map<std::string, u32> fontIndex;  // uri -> fonts index
    std::vector<std::unique_ptr<Font>> fonts;
    std::unordered_map<u64, Glyph> glyphs;
    std::vector<u8> atlas = std::vector<u8>(static_cast<usize>(kAtlasSize) * kAtlasSize * 4, 0);
    u32 shelfX = 1, shelfY = 1, shelfH = 0;
    bool dirty = false, overflow = false;
    rhi::TextureHandle texture;
    FontStats stats;
    std::vector<u8> scratch;

    explicit Impl(VirtualFileSystem& v) : vfs(v) {
        for (usize i = 0; i < atlas.size(); i += 4) atlas[i] = atlas[i + 1] = atlas[i + 2] = 255;  // white, alpha = coverage
    }
    ~Impl() {
        if (device && texture) device->destroyTexture(texture);
    }

    Result<u32> font(std::string_view uri) {
        if (auto it = fontIndex.find(std::string(uri)); it != fontIndex.end()) return it->second;
        auto bytes = vfs.readBytes(uri);
        if (!bytes) return bytes.error();
        auto f = std::make_unique<Font>();
        f->data = std::move(bytes.value());
        const int offset = stbtt_GetFontOffsetForIndex(f->data.data(), 0);
        if (offset < 0 || !stbtt_InitFont(&f->info, f->data.data(), offset))
            return makeError(Error::Code::ParseError, std::string(uri) + ": not a TrueType font");
        stbtt_GetFontVMetrics(&f->info, &f->ascent, &f->descent, &f->lineGap);
        if (f->ascent - f->descent <= 0) return makeError(Error::Code::ParseError, std::string(uri) + ": invalid font metrics");
        const u32 index = static_cast<u32>(fonts.size());
        fonts.push_back(std::move(f));
        fontIndex.emplace(std::string(uri), index);
        stats.fonts = static_cast<u32>(fonts.size());
        return index;
    }

    const Glyph& glyph(u32 fontId, u32 px, u32 codepoint) {
        const u64 key = (static_cast<u64>(fontId) << 48) | (static_cast<u64>(px) << 32) | codepoint;
        if (auto it = glyphs.find(key); it != glyphs.end()) return it->second;
        Font& f = *fonts[fontId];
        Glyph g;
        g.index = stbtt_FindGlyphIndex(&f.info, static_cast<int>(codepoint));
        const float scale = static_cast<float>(px) / static_cast<float>(f.ascent - f.descent);
        int adv = 0, lsb = 0;
        stbtt_GetGlyphHMetrics(&f.info, g.index, &adv, &lsb);
        g.advance = static_cast<float>(adv) * scale;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBox(&f.info, g.index, scale, scale, &x0, &y0, &x1, &y1);
        const int w = x1 - x0, h = y1 - y0;
        g.valid = true;
        if (w > 0 && h > 0) {
            // Shelf packing with a 1-pixel gap (bilinear filtering).
            if (shelfX + static_cast<u32>(w) + 1 > kAtlasSize) {
                shelfX = 1;
                shelfY += shelfH + 1;
                shelfH = 0;
            }
            if (static_cast<u32>(w) + 2 > kAtlasSize || shelfY + static_cast<u32>(h) + 1 > kAtlasSize) {
                overflow = true;
                g.valid = false;
                return glyphs.emplace(key, g).first->second;  // retried after resetAtlas()
            }
            scratch.assign(static_cast<usize>(w) * h, 0);
            stbtt_MakeGlyphBitmap(&f.info, scratch.data(), w, h, w, scale, scale, g.index);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    atlas[((static_cast<usize>(shelfY) + y) * kAtlasSize + shelfX + x) * 4 + 3] = scratch[static_cast<usize>(y) * w + x];
            const float inv = 1.0f / static_cast<float>(kAtlasSize);
            g.uv = {static_cast<float>(shelfX) * inv, static_cast<float>(shelfY) * inv, static_cast<float>(shelfX + w) * inv,
                    static_cast<float>(shelfY + h) * inv};
            g.x0 = static_cast<float>(x0);
            g.y0 = static_cast<float>(y0);
            g.w = static_cast<float>(w);
            g.h = static_cast<float>(h);
            shelfX += static_cast<u32>(w) + 1;
            shelfH = std::max(shelfH, static_cast<u32>(h));
            dirty = true;
            ++stats.glyphs;
            stats.atlasUsage = static_cast<float>(shelfY + shelfH) / static_cast<float>(kAtlasSize);
        }
        return glyphs.emplace(key, g).first->second;
    }
};

FontLibrary::FontLibrary(VirtualFileSystem& vfs) : m(std::make_unique<Impl>(vfs)) {}
FontLibrary::~FontLibrary() = default;

void FontLibrary::setDevice(rhi::Device* device) {
    if (m->device == device) return;
    if (m->device && m->texture) m->device->destroyTexture(m->texture);
    m->texture = {};
    m->device = device;
    m->dirty = true;
}

Result<TextLayout> FontLibrary::layout(std::string_view fontUri, std::string_view text, float pixelSize, float boxWidth, bool wrap,
                                       float lineSpacing, UIHAlign align) {
    auto id = m->font(fontUri);
    if (!id) return id.error();
    const u32 fontId = id.value();
    const Font& f = *m->fonts[fontId];
    const u32 px = static_cast<u32>(std::clamp(std::lround(pixelSize), 1L, 1000L));
    const float scale = static_cast<float>(px) / static_cast<float>(f.ascent - f.descent);
    const float ascent = std::round(static_cast<float>(f.ascent) * scale);
    const float lineHeight = std::max(1.0f, std::round(static_cast<float>(px) * std::max(0.1f, lineSpacing)));

    struct Item {
        u32 cp;
        const Glyph* g;
        float x;  // pen position within the line, before alignment
    };
    TextLayout out;
    std::vector<Item> line;
    float y = 0.0f;
    auto emitLine = [&](usize count, float width) {
        float offset = 0.0f;
        const float box = std::max(boxWidth, 0.0f);
        if (align == UIHAlign::Center) offset = std::round((box - width) * 0.5f);
        else if (align == UIHAlign::Right) offset = std::round(box - width);
        const float baseline = y + ascent;
        for (usize i = 0; i < count; ++i) {
            const Item& it = line[i];
            if (!it.g->valid || it.g->w <= 0.0f) continue;
            const float gx = std::round(offset + it.x + it.g->x0), gy = baseline + it.g->y0;
            out.glyphs.push_back({{gx, gy}, {gx + it.g->w, gy + it.g->h}, it.g->uv});
        }
        out.size.x = std::max(out.size.x, width);
        y += lineHeight;
        ++out.lines;
    };

    usize i = 0;
    float pen = 0.0f;
    int prevGlyph = -1;
    usize lastSpace = SIZE_MAX;  // index in `line` of the last space (break opportunity)
    auto lineWidth = [&](usize count) {
        // Trailing spaces do not count.
        while (count > 0 && line[count - 1].cp == ' ') --count;
        return count == 0 ? 0.0f : line[count - 1].x + line[count - 1].g->advance;
    };
    auto breakLine = [&](usize count, usize resumeFrom) {
        emitLine(count, lineWidth(count));
        std::vector<Item> rest(line.begin() + static_cast<std::ptrdiff_t>(resumeFrom), line.end());
        line = std::move(rest);
        // Re-flow the carried-over items from the start of the new line.
        pen = 0.0f;
        prevGlyph = -1;
        for (Item& it : line) {
            if (prevGlyph >= 0) pen += static_cast<float>(stbtt_GetGlyphKernAdvance(&f.info, prevGlyph, it.g->index)) * scale;
            it.x = pen;
            pen += it.g->advance;
            prevGlyph = it.g->index;
        }
        lastSpace = SIZE_MAX;
    };
    while (i < text.size()) {
        const u32 cp = nextCodepoint(text, i);
        if (cp == '\r') continue;
        if (cp == '\n') {
            emitLine(line.size(), lineWidth(line.size()));
            line.clear();
            pen = 0.0f;
            prevGlyph = -1;
            lastSpace = SIZE_MAX;
            continue;
        }
        const Glyph& g = m->glyph(fontId, px, cp == '\t' ? ' ' : cp);
        if (prevGlyph >= 0) pen += static_cast<float>(stbtt_GetGlyphKernAdvance(&f.info, prevGlyph, g.index)) * scale;
        if (wrap && boxWidth > 0.0f && cp != ' ' && pen + g.advance > boxWidth && !line.empty()) {
            if (lastSpace != SIZE_MAX) breakLine(lastSpace, lastSpace + 1);  // at the last space
            else breakLine(line.size(), line.size());                        // word longer than the box
            if (prevGlyph >= 0) pen += static_cast<float>(stbtt_GetGlyphKernAdvance(&f.info, prevGlyph, g.index)) * scale;
        }
        line.push_back({cp, &g, pen});
        if (cp == ' ') lastSpace = line.size() - 1;
        pen += g.advance;
        prevGlyph = g.index;
    }
    if (!line.empty() || out.lines == 0) emitLine(line.size(), lineWidth(line.size()));
    out.size.y = y;
    return out;
}

void FontLibrary::flush() {
    if (!m->device || !m->dirty) return;
    if (!m->texture) {
        rhi::TextureDesc td;
        td.width = td.height = kAtlasSize;
        td.format = rhi::TextureFormat::RGBA8;
        td.generateMips = false;
        td.sampler = {rhi::Filter::Linear, rhi::Filter::Linear, false, rhi::Wrap::Clamp, rhi::Wrap::Clamp};
        td.data = m->atlas.data();
        td.debugName = "agr.ui.glyphs";
        auto t = m->device->createTexture(td);
        if (!t) {
            AGR_LOG_ERROR("UI", "glyph atlas: {}", t.error().message);
            return;
        }
        m->texture = t.value();
    } else if (Status s = m->device->updateTexture(m->texture, m->atlas.data()); !s) {
        AGR_LOG_ERROR("UI", "glyph atlas upload: {}", s.error().message);
    }
    m->dirty = false;
}

rhi::TextureHandle FontLibrary::texture() const { return m->texture; }
bool FontLibrary::overflowed() const { return m->overflow; }

void FontLibrary::resetAtlas() {
    m->glyphs.clear();
    for (usize i = 3; i < m->atlas.size(); i += 4) m->atlas[i] = 0;
    m->shelfX = m->shelfY = 1;
    m->shelfH = 0;
    m->overflow = false;
    m->dirty = true;
    m->stats.glyphs = 0;
    m->stats.atlasUsage = 0.0f;
    ++m->stats.atlasResets;
}

const FontStats& FontLibrary::stats() const { return m->stats; }

} // namespace agr
