#include "agr/render/SpriteSheet.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "agr/core/Log.hpp"

namespace agr {

using json = nlohmann::ordered_json;

math::Vec4 SpriteSheet::uv(u32 frame) const {
    if (frame >= frames.size() || textureWidth == 0 || textureHeight == 0) return {0, 0, 1, 1};
    const SpriteFrame& f = frames[frame];
    const float iw = 1.0f / static_cast<float>(textureWidth), ih = 1.0f / static_cast<float>(textureHeight);
    return {static_cast<float>(f.x) * iw, static_cast<float>(f.y) * ih, static_cast<float>(f.x + f.w) * iw,
            static_cast<float>(f.y + f.h) * ih};
}

math::Vec2 SpriteSheet::frameWorldSize(u32 frame) const {
    if (frame >= frames.size()) return {1, 1};
    const float ppu = std::max(pixelsPerUnit, 1e-3f);
    return {static_cast<float>(frames[frame].w) / ppu, static_cast<float>(frames[frame].h) / ppu};
}

i32 SpriteSheet::findFrame(std::string_view name) const {
    for (usize i = 0; i < frames.size(); ++i)
        if (frames[i].name == name) return static_cast<i32>(i);
    return -1;
}

const SpriteClip* SpriteSheet::findClip(std::string_view name) const {
    for (const auto& c : clips)
        if (c.name == name) return &c;
    return nullptr;
}

Result<SpriteSheet> SpriteSheet::parse(std::string_view text) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object() || j.value("format", "") != "agr.spritesheet")
        return makeError(Error::Code::ParseError, "not an AGR sprite sheet");
    if (j.value("version", 0) > kFormatVersion) return makeError(Error::Code::VersionMismatch, "sprite sheet from a newer engine");
    SpriteSheet s;
    try {
        s.texture = j.value("texture", std::string());
        s.textureWidth = j.value("textureWidth", 0u);
        s.textureHeight = j.value("textureHeight", 0u);
        s.pixelsPerUnit = j.value("pixelsPerUnit", 100.0f);
        s.pixelArt = j.value("pixelArt", false);
        if (j.contains("frames")) {
            for (const auto& f : j["frames"]) {
                SpriteFrame fr;
                fr.name = f.value("name", std::string());
                fr.x = f.value("x", 0u);
                fr.y = f.value("y", 0u);
                fr.w = f.value("w", 0u);
                fr.h = f.value("h", 0u);
                if (f.contains("pivot") && f["pivot"].is_array() && f["pivot"].size() == 2)
                    fr.pivot = {f["pivot"][0].get<float>(), f["pivot"][1].get<float>()};
                s.frames.push_back(std::move(fr));
            }
        }
        if (j.contains("clips")) {
            for (const auto& c : j["clips"]) {
                SpriteClip clip;
                clip.name = c.value("name", std::string());
                clip.fps = c.value("fps", 12.0f);
                clip.loop = c.value("loop", true);
                for (const auto& f : c.value("frames", json::array())) {
                    if (f.is_number_unsigned()) clip.frames.push_back(f.get<u32>());
                    else if (f.is_string()) {  // frames may be referenced by name
                        const i32 idx = s.findFrame(f.get<std::string>());
                        if (idx < 0) return makeError(Error::Code::ParseError, "clip '" + clip.name + "' references unknown frame '" + f.get<std::string>() + "'");
                        clip.frames.push_back(static_cast<u32>(idx));
                    }
                }
                s.clips.push_back(std::move(clip));
            }
        }
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("sprite sheet: ") + e.what());
    }
    if (s.texture.empty()) return makeError(Error::Code::ParseError, "sprite sheet without texture");
    if (s.textureWidth == 0 || s.textureHeight == 0) return makeError(Error::Code::ParseError, "sprite sheet without texture size");
    for (const auto& f : s.frames)
        if (f.w == 0 || f.h == 0 || f.x + f.w > s.textureWidth || f.y + f.h > s.textureHeight)
            return makeError(Error::Code::ParseError, "frame '" + f.name + "' is outside the texture");
    for (const auto& c : s.clips)
        for (u32 f : c.frames)
            if (f >= s.frames.size()) return makeError(Error::Code::ParseError, "clip '" + c.name + "' has an invalid frame index");
    return s;
}

std::string SpriteSheet::toJson() const {
    json j;
    j["format"] = "agr.spritesheet";
    j["version"] = kFormatVersion;
    j["texture"] = texture;
    j["textureWidth"] = textureWidth;
    j["textureHeight"] = textureHeight;
    j["pixelsPerUnit"] = pixelsPerUnit;
    j["pixelArt"] = pixelArt;
    json fr = json::array();
    for (const auto& f : frames) {
        json o;
        o["name"] = f.name;
        o["x"] = f.x;
        o["y"] = f.y;
        o["w"] = f.w;
        o["h"] = f.h;
        if (f.pivot.x != 0.5f || f.pivot.y != 0.5f) o["pivot"] = {f.pivot.x, f.pivot.y};
        fr.push_back(std::move(o));
    }
    j["frames"] = std::move(fr);
    json cl = json::array();
    for (const auto& c : clips) cl.push_back({{"name", c.name}, {"fps", c.fps}, {"loop", c.loop}, {"frames", c.frames}});
    j["clips"] = std::move(cl);
    return j.dump(2);
}

SpriteSheet SpriteSheet::grid(std::string texture, u32 tw, u32 th, u32 cw, u32 ch, u32 margin, u32 spacing, const std::string& prefix) {
    SpriteSheet s;
    s.texture = std::move(texture);
    s.textureWidth = tw;
    s.textureHeight = th;
    s.pixelsPerUnit = static_cast<float>(std::max(cw, ch));  // one cell = one world unit by default
    if (cw == 0 || ch == 0) return s;
    u32 index = 0;
    for (u32 y = margin; y + ch <= th; y += ch + spacing)
        for (u32 x = margin; x + cw <= tw; x += cw + spacing)
            s.frames.push_back({prefix + "_" + std::to_string(index++), x, y, cw, ch, {0.5f, 0.5f}});
    return s;
}

SpriteSheetLibrary::Entry SpriteSheetLibrary::load(const std::string& uri) {
    Entry e;
    e.modifiedTime = m_vfs.modifiedTime(uri);
    auto text = m_vfs.readText(uri);
    if (!text) {
        AGR_LOG_ERROR("Render", "sprite sheet '{}': {}", uri, text.error().message);
        return e;
    }
    auto sheet = SpriteSheet::parse(text.value());
    if (!sheet) {
        AGR_LOG_ERROR("Render", "sprite sheet '{}': {}", uri, sheet.error().message);
        return e;
    }
    e.sheet = std::move(sheet.value());
    e.valid = true;
    return e;
}

const SpriteSheet* SpriteSheetLibrary::get(const std::string& uri) {
    if (uri.empty()) return nullptr;
    auto it = m_cache.find(uri);
    if (it == m_cache.end()) it = m_cache.emplace(uri, load(uri)).first;
    return it->second.valid ? &it->second.sheet : nullptr;
}

u32 SpriteSheetLibrary::reloadChanged() {
    u32 n = 0;
    for (auto& [uri, entry] : m_cache) {
        const i64 t = m_vfs.modifiedTime(uri);
        if (t != 0 && t != entry.modifiedTime) {
            entry = load(uri);
            ++n;
            AGR_LOG_INFO("Render", "hot reload sprite sheet '{}'", uri);
        }
    }
    return n;
}

} // namespace agr
