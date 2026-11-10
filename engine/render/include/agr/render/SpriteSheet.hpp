#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"
#include "agr/math/Math.hpp"

namespace agr {

struct SpriteFrame {
    std::string name;
    u32 x = 0, y = 0, w = 0, h = 0;  ///< Pixels, origin top-left of the texture.
    math::Vec2 pivot{0.5f, 0.5f};
};

struct SpriteClip {
    std::string name;
    std::vector<u32> frames;  ///< Indices into SpriteSheet::frames.
    float fps = 12.0f;
    bool loop = true;
};

/// A texture cut into named frames, plus animation clips (.agrsheet, JSON).
/// The texture size is stored in the file so UVs are known before the image loads.
struct SpriteSheet {
    static constexpr int kFormatVersion = 1;
    std::string texture;  ///< Asset URI of the image.
    u32 textureWidth = 0, textureHeight = 0;
    float pixelsPerUnit = 100.0f;  ///< Native world size of a frame = pixels / pixelsPerUnit.
    bool pixelArt = false;         ///< Nearest filtering.
    std::vector<SpriteFrame> frames;
    std::vector<SpriteClip> clips;

    bool validFrame(i32 frame) const { return frame >= 0 && static_cast<usize>(frame) < frames.size(); }
    /// u0, v0, u1, v1 with v = 0 at the top (same convention as SpriteRendererComponent::uvRect).
    math::Vec4 uv(u32 frame) const;
    math::Vec2 frameWorldSize(u32 frame) const;
    i32 findFrame(std::string_view name) const;
    const SpriteClip* findClip(std::string_view name) const;

    static Result<SpriteSheet> parse(std::string_view json);
    std::string toJson() const;

    /// Cuts a texture into a regular grid (row-major from the top-left).
    static SpriteSheet grid(std::string texture, u32 textureWidth, u32 textureHeight, u32 cellWidth, u32 cellHeight,
                            u32 margin = 0, u32 spacing = 0, const std::string& namePrefix = "frame");
};

/// Loads and caches sprite sheets by URI (engine service). Sheets are small JSON
/// files read synchronously on first use; the texture itself streams through the
/// AssetManager like any other texture.
class SpriteSheetLibrary {
public:
    explicit SpriteSheetLibrary(VirtualFileSystem& vfs) : m_vfs(vfs) {}
    /// Null when the file is missing or invalid (the error is logged once).
    const SpriteSheet* get(const std::string& uri);
    /// Re-reads sheets whose file changed. Returns the number reloaded.
    u32 reloadChanged();
    void invalidate(const std::string& uri) { m_cache.erase(uri); }
    void clear() { m_cache.clear(); }

private:
    struct Entry {
        bool valid = false;
        SpriteSheet sheet;
        i64 modifiedTime = 0;
    };
    Entry load(const std::string& uri);
    VirtualFileSystem& m_vfs;
    std::unordered_map<std::string, Entry> m_cache;
};

} // namespace agr
