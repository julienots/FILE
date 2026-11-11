#include "UI.h"
#include <sstream>
#include <algorithm>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../third_party/stb_image.h"

namespace agr {

uint32_t utf8Next(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : 1;
    uint32_t cp = c & (0x3F >> n);
    for (int k = 0; k < n && i < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    return cp;
}

static bool loadPng(const std::string& path, Texture& t, bool gray) {
    std::vector<uint8_t> bytes;
    if (!platform::readAsset(path, bytes)) { AGR_ERR("missing asset %s", path.c_str()); return false; }
    int w, h, n;
    uint8_t* px = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &n, gray ? 1 : 4);
    if (!px) { AGR_ERR("png decode failed %s", path.c_str()); return false; }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (gray) t.create2D(w, h, GL_R8, GL_RED, GL_UNSIGNED_BYTE, px, true, false);
    else {
        // premultiply-free straight alpha; generate mips for downscaled icons
        t.create2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, px, true, false);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    stbi_image_free(px);
    return true;
}

bool UI::loadAtlas() {
    if (!loadPng("ui/ui.png", atlas, false)) return false;
    std::vector<uint8_t> txt;
    if (!platform::readAsset("ui/ui.txt", txt)) return false;
    std::istringstream in(std::string(txt.begin(), txt.end()));
    std::string name; float x, y, w, h, b;
    while (in >> name >> x >> y >> w >> h >> b) regions[name] = {x, y, w, h, b};
    return true;
}

bool UI::loadFont() {
    if (!loadPng("ui/font.png", font, true)) return false;
    std::vector<uint8_t> txt;
    if (!platform::readAsset("ui/font.txt", txt)) return false;
    std::istringstream in(std::string(txt.begin(), txt.end()));
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag; ls >> tag;
        if (tag == "font") {
            int idx; FontFace f; ls >> idx >> f.name >> f.size >> f.lineH >> f.asc;
            if ((int)fonts.size() <= idx) fonts.resize(idx + 1);
            fonts[idx] = f;
        } else if (tag == "g") {
            int fi; uint32_t cp; Glyph g; ls >> fi >> cp >> g.x >> g.y >> g.w >> g.h >> g.xoff >> g.yoff >> g.adv;
            fonts[fi].glyphs[cp] = g;
        } else if (tag == "k") {
            int fi; uint32_t a, b; float k; ls >> fi >> a >> b >> k;
            fonts[fi].kern[((uint64_t)a << 32) | b] = k;
        }
    }
    return !fonts.empty();
}

bool UI::init() {
    if (!loadAtlas() || !loadFont()) return false;
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)0);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UIVertex), (void*)8);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)12);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBindVertexArray(0);
    return true;
}

void UI::shutdown() {
    atlas.destroy(); font.destroy();
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ibo) glDeleteBuffers(1, &ibo);
    vao = vbo = ibo = 0;
}

void UI::begin(int sw, int sh, float dt_) {
    pixelW = sw; pixelH = sh;
    H = 720.f; scale = sh / H; W = sw / scale;
    dt = dt_; time += dt_;
    verts.clear(); idx.clear(); batches.clear(); clipStack.clear();
    curImage = 0;
    batches.push_back({0, 0, 0, {}, false});
    for (auto& kv : flashes) kv.second = std::max(0.f, kv.second - dt * 4.f);
}

void UI::ensureBatch(GLuint image) {
    Batch& b = batches.back();
    bool clipOn = !clipStack.empty();
    Rect clip = clipOn ? clipStack.back() : Rect{};
    bool sameClip = b.clipOn == clipOn && (!clipOn || (b.clip.x == clip.x && b.clip.y == clip.y && b.clip.w == clip.w && b.clip.h == clip.h));
    if ((image == 0 || b.image == image || b.image == 0) && sameClip) {
        if (image) b.image = image;
        return;
    }
    batches.push_back({image, (int)idx.size(), 0, clip, clipOn});
}

void UI::quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, vec4 c, float mode, float soft) {
    if (verts.size() > 65000) return;
    Color8 k = toColor8(c.xyz(), c.w);
    uint16_t b = (uint16_t)verts.size();
    verts.push_back({x0 * scale, y0 * scale, k.r, k.g, k.b, k.a, u0, v0, mode, soft});
    verts.push_back({x1 * scale, y0 * scale, k.r, k.g, k.b, k.a, u1, v0, mode, soft});
    verts.push_back({x1 * scale, y1 * scale, k.r, k.g, k.b, k.a, u1, v1, mode, soft});
    verts.push_back({x0 * scale, y1 * scale, k.r, k.g, k.b, k.a, u0, v1, mode, soft});
    uint16_t q[6] = {b, (uint16_t)(b + 1), (uint16_t)(b + 2), b, (uint16_t)(b + 2), (uint16_t)(b + 3)};
    idx.insert(idx.end(), q, q + 6);
    batches.back().count = (int)idx.size() - batches.back().start;
}

void UI::rect(Rect r, vec4 c) { ensureBatch(0); quad(r.x, r.y, r.x + r.w, r.y + r.h, 0, 0, 0, 0, c, 0); }

void UI::rectGrad(Rect r, vec4 top, vec4 bot) {
    ensureBatch(0);
    rect({r.x, r.y, r.w, r.h * 0.5f}, lerp(top, bot, 0.25f));
    rect({r.x, r.y + r.h * 0.5f, r.w, r.h * 0.5f}, lerp(top, bot, 0.75f));
}

void UI::sprite(const std::string& name, Rect r, vec4 c) {
    auto it = regions.find(name);
    if (it == regions.end()) return;
    const UIRegion& g = it->second;
    ensureBatch(0);
    float aw = (float)atlas.w, ah = (float)atlas.h;
    quad(r.x, r.y, r.x + r.w, r.y + r.h, g.x / aw, g.y / ah, (g.x + g.w) / aw, (g.y + g.h) / ah, c, 1);
}

void UI::spriteRot(const std::string& name, vec2 c, vec2 s, float ang, vec4 col) {
    auto it = regions.find(name);
    if (it == regions.end()) return;
    const UIRegion& g = it->second;
    ensureBatch(0);
    float aw = (float)atlas.w, ah = (float)atlas.h;
    vec2 corners[4] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
    float uv[4][2] = {{g.x / aw, g.y / ah}, {(g.x + g.w) / aw, g.y / ah}, {(g.x + g.w) / aw, (g.y + g.h) / ah}, {g.x / aw, (g.y + g.h) / ah}};
    Color8 k = toColor8(col.xyz(), col.w);
    uint16_t b = (uint16_t)verts.size();
    for (int i = 0; i < 4; i++) {
        vec2 p = rotate(corners[i] * s, ang) + c;
        verts.push_back({p.x * scale, p.y * scale, k.r, k.g, k.b, k.a, uv[i][0], uv[i][1], 1, 0});
    }
    uint16_t q[6] = {b, (uint16_t)(b + 1), (uint16_t)(b + 2), b, (uint16_t)(b + 2), (uint16_t)(b + 3)};
    idx.insert(idx.end(), q, q + 6);
    batches.back().count = (int)idx.size() - batches.back().start;
}

void UI::nine(const std::string& name, Rect r, vec4 c, float bs) {
    auto it = regions.find(name);
    if (it == regions.end()) return;
    const UIRegion& g = it->second;
    if (g.border <= 0) { sprite(name, r, c); return; }
    ensureBatch(0);
    float aw = (float)atlas.w, ah = (float)atlas.h;
    float b = g.border;                                   // texels
    float d = std::min(b * bs, std::min(r.w, r.h) * 0.5f);  // units on screen
    float xs[4] = {r.x, r.x + d, r.x + r.w - d, r.x + r.w};
    float ys[4] = {r.y, r.y + d, r.y + r.h - d, r.y + r.h};
    float us[4] = {g.x / aw, (g.x + b) / aw, (g.x + g.w - b) / aw, (g.x + g.w) / aw};
    float vs[4] = {g.y / ah, (g.y + b) / ah, (g.y + g.h - b) / ah, (g.y + g.h) / ah};
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) quad(xs[i], ys[j], xs[i + 1], ys[j + 1], us[i], vs[j], us[i + 1], vs[j + 1], c, 1);
}

void UI::image(GLuint tex, Rect r, vec4 uv, vec4 c) {
    ensureBatch(tex);
    quad(r.x, r.y, r.x + r.w, r.y + r.h, uv.x, uv.y, uv.z, uv.w, c, 4);
}

void UI::imageRot(GLuint tex, vec2 c, vec2 s, float ang, vec4 uvr, vec4 col) {
    ensureBatch(tex);
    vec2 corners[4] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
    float uv[4][2] = {{uvr.x, uvr.y}, {uvr.z, uvr.y}, {uvr.z, uvr.w}, {uvr.x, uvr.w}};
    Color8 k = toColor8(col.xyz(), col.w);
    uint16_t b = (uint16_t)verts.size();
    for (int i = 0; i < 4; i++) {
        vec2 p = rotate(corners[i] * s, ang) + c;
        verts.push_back({p.x * scale, p.y * scale, k.r, k.g, k.b, k.a, uv[i][0], uv[i][1], 4, 0});
    }
    uint16_t q[6] = {b, (uint16_t)(b + 1), (uint16_t)(b + 2), b, (uint16_t)(b + 2), (uint16_t)(b + 3)};
    idx.insert(idx.end(), q, q + 6);
    batches.back().count = (int)idx.size() - batches.back().start;
}

float UI::textWidth(const std::string& s, float size, int fi) const {
    const FontFace& f = fonts[fi];
    float k = size / f.size, w = 0, best = 0;
    uint32_t prev = 0;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = utf8Next(s, i);
        if (cp == '\n') { best = std::max(best, w); w = 0; continue; }
        auto it = f.glyphs.find(cp);
        if (it == f.glyphs.end()) it = f.glyphs.find('?');
        if (it == f.glyphs.end()) continue;
        auto kk = f.kern.find(((uint64_t)prev << 32) | cp);
        if (kk != f.kern.end()) w += kk->second * k;
        w += it->second.adv * k;
        prev = cp;
    }
    return std::max(best, w);
}

// Splits text into lines for a max width (word wrap, explicit \n).
static std::vector<std::string> wrapLines(const UI& ui, const std::string& s, float size, int font, float maxW) {
    std::vector<std::string> out;
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string cand = line.empty() ? word : line + " " + word;
        if (maxW > 0 && !line.empty() && ui.textWidth(cand, size, font) > maxW) { out.push_back(line); line = word; }
        else line = cand;
        word.clear();
    };
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == ' ') flushWord();
        else if (c == '\n') { flushWord(); out.push_back(line); line.clear(); }
        else word += c;
    }
    flushWord();
    if (!line.empty() || out.empty()) out.push_back(line);
    return out;
}

float UI::textHeight(const std::string& s, float size, int fi, float maxW) const {
    auto lines = wrapLines(*this, s, size, fi, maxW);
    return lines.size() * fonts[fi].lineH * size / fonts[fi].size * 0.92f;
}

float UI::text(const std::string& s, float x, float y, float size, vec4 c, int fi, int align, float maxW, bool outline) {
    if (fi < 0 || fi >= (int)fonts.size()) return 0;
    const FontFace& f = fonts[fi];
    float k = size / f.size;
    float lh = f.lineH * k * 0.92f;
    auto lines = maxW > 0 || s.find('\n') != std::string::npos ? wrapLines(*this, s, size, fi, maxW) : std::vector<std::string>{s};
    ensureBatch(0);
    float fw = (float)font.w, fh = (float)font.h;
    // SDF softness: smaller text needs a wider smoothing band (in distance units)
    float soft = clampf(0.12f / (size * scale / 28.f), 0.03f, 0.25f);
    float yy = y;
    for (auto& ln : lines) {
        float lw = textWidth(ln, size, fi);
        float xx = x - (align == A_CENTER ? lw * 0.5f : (align == A_RIGHT ? lw : 0));
        uint32_t prev = 0;
        for (size_t i = 0; i < ln.size();) {
            uint32_t cp = utf8Next(ln, i);
            auto it = f.glyphs.find(cp);
            if (it == f.glyphs.end()) it = f.glyphs.find('?');
            if (it == f.glyphs.end()) continue;
            auto kk = f.kern.find(((uint64_t)prev << 32) | cp);
            if (kk != f.kern.end()) xx += kk->second * k;
            const Glyph& g = it->second;
            if (g.w > 0) {
                float x0 = xx + g.xoff * k, y0 = yy + g.yoff * k;
                quad(x0, y0, x0 + g.w * k, y0 + g.h * k, g.x / fw, g.y / fh, (g.x + g.w) / fw, (g.y + g.h) / fh, c, outline ? 3.f : 2.f, soft);
            }
            xx += g.adv * k;
            prev = cp;
        }
        yy += lh;
    }
    return yy - y;
}

void UI::bar(Rect r, float frac, vec4 col, float ghost, vec4 ghostCol) {
    frac = saturate(frac);
    nine("bar_frame", r, vec4(1, 1, 1, 1), r.h / 32.f);
    Rect in = {r.x + r.h * 0.22f, r.y + r.h * 0.22f, r.w - r.h * 0.44f, r.h * 0.56f};
    if (ghost > frac) nine("bar_fill", {in.x, in.y, in.w * saturate(ghost), in.h}, ghostCol, in.h / 24.f);
    if (frac > 0.005f) nine("bar_fill", {in.x, in.y, std::max(in.h, in.w * frac), in.h}, col, in.h / 24.f);
}

void UI::circle(vec2 c, float r, vec4 col, int seg) { ring(c, 0, r, 0, TAU, col, seg); }

void UI::ring(vec2 c, float r0, float r1, float a0, float a1, vec4 col, int seg) {
    ensureBatch(0);
    Color8 k = toColor8(col.xyz(), col.w);
    for (int i = 0; i < seg; i++) {
        float t0 = a0 + (a1 - a0) * i / seg, t1 = a0 + (a1 - a0) * (i + 1) / seg;
        vec2 p[4] = {c + vec2(std::cos(t0), std::sin(t0)) * r0, c + vec2(std::cos(t0), std::sin(t0)) * r1,
                     c + vec2(std::cos(t1), std::sin(t1)) * r1, c + vec2(std::cos(t1), std::sin(t1)) * r0};
        uint16_t b = (uint16_t)verts.size();
        for (auto& q : p) verts.push_back({q.x * scale, q.y * scale, k.r, k.g, k.b, k.a, 0, 0, 0, 0});
        uint16_t qi[6] = {b, (uint16_t)(b + 1), (uint16_t)(b + 2), b, (uint16_t)(b + 2), (uint16_t)(b + 3)};
        idx.insert(idx.end(), qi, qi + 6);
    }
    batches.back().count = (int)idx.size() - batches.back().start;
}

void UI::pushClip(Rect r) {
    if (!clipStack.empty()) {
        Rect p = clipStack.back();
        float x0 = std::max(p.x, r.x), y0 = std::max(p.y, r.y), x1 = std::min(p.x + p.w, r.x + r.w), y1 = std::min(p.y + p.h, r.y + r.h);
        r = {x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0)};
    }
    clipStack.push_back(r);
    batches.push_back({curImage, (int)idx.size(), 0, r, true});
}

void UI::popClip() {
    if (clipStack.empty()) return;
    clipStack.pop_back();
    batches.push_back({0, (int)idx.size(), 0, clipStack.empty() ? Rect{} : clipStack.back(), !clipStack.empty()});
}

void UI::end(Shader& sh) {
    if (idx.empty()) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    sh.use();
    sh.set("uScreen", vec2((float)pixelW, (float)pixelH));
    sh.set("uAtlas", 0); sh.set("uFont", 1); sh.set("uImage", 2);
    atlas.bind(0); font.bind(1);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(UIVertex), verts.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 2, idx.data(), GL_STREAM_DRAW);
    for (auto& b : batches) {
        if (b.count <= 0) continue;
        if (b.clipOn) {
            glEnable(GL_SCISSOR_TEST);
            glScissor((int)(b.clip.x * scale), (int)(pixelH - (b.clip.y + b.clip.h) * scale), (int)std::max(0.f, b.clip.w * scale), (int)std::max(0.f, b.clip.h * scale));
        } else glDisable(GL_SCISSOR_TEST);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, b.image);
        glDrawElements(GL_TRIANGLES, b.count, GL_UNSIGNED_SHORT, (void*)(intptr_t)(b.start * 2));
    }
    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// ------------------------------------------------------------------------------------------- input
void UI::onTouch(const TouchEvent& e) {
    float x = e.x / scale, y = e.y / scale;
    auto it = std::find_if(fingers.begin(), fingers.end(), [&](const Finger& f) { return f.id == e.id; });
    if (e.type == TouchEvent::Down) {
        if (it != fingers.end()) fingers.erase(it);
        fingers.push_back({e.id, x, y, x, y, true, true, false, false, 0});
    } else if (it != fingers.end()) {
        it->x = x; it->y = y;
        if (e.type == TouchEvent::Up || e.type == TouchEvent::Cancel) { it->down = false; it->released = e.type == TouchEvent::Up; }
    }
}

void UI::endInput() {
    for (auto& f : fingers) { f.pressed = false; f.t += dt; }
    fingers.erase(std::remove_if(fingers.begin(), fingers.end(), [](const Finger& f) { return !f.down; }), fingers.end());
}

bool UI::button(Rect r, int key) {
    if (!clipStack.empty()) {
        Rect c = clipStack.back();
        // a button partially scrolled out of view is only active in its visible part
        float x0 = std::max(c.x, r.x), y0 = std::max(c.y, r.y), x1 = std::min(c.x + c.w, r.x + r.w), y1 = std::min(c.y + c.h, r.y + r.h);
        if (x1 <= x0 || y1 <= y0) return false;
        r = {x0, y0, x1 - x0, y1 - y0};
    }
    for (auto& f : fingers) {
        if (f.released && !f.consumed && r.contains(f.x, f.y) && r.contains(f.sx, f.sy)) {
            float drag = std::fabs(f.x - f.sx) + std::fabs(f.y - f.sy);
            if (drag > 22) continue;  // was a scroll gesture
            f.consumed = true;
            if (key >= 0) flashes[key] = 1.f;
            return true;
        }
    }
    return false;
}

bool UI::held(Rect r) const {
    for (auto& f : fingers) if (f.down && r.contains(f.x, f.y) && r.contains(f.sx, f.sy)) return true;
    return false;
}

bool UI::pressedIn(Rect r) {
    for (auto& f : fingers) if (f.pressed && !f.consumed && r.contains(f.x, f.y)) { f.consumed = true; return true; }
    return false;
}

bool UI::anyFingerOver(Rect r) const {
    for (auto& f : fingers) if (f.down && r.contains(f.x, f.y)) return true;
    return false;
}

void UI::consumeAll() { for (auto& f : fingers) f.consumed = true; }
bool UI::fingerConsumed(int id) const {
    for (auto& f : fingers) if (f.id == id) return f.consumed;
    return false;
}

float UI::scrollArea(const std::string& key, Rect r, float contentH) {
    float& off = scrolls[key];
    float& vel = scrollVel[key];
    float maxOff = std::max(0.f, contentH - r.h);
    bool dragging = false;
    for (auto& f : fingers) {
        if (f.down && r.contains(f.sx, f.sy) && std::fabs(f.y - f.sy) > 12) {
            static std::unordered_map<int, float> lastY;
            auto it = lastY.find(f.id);
            float ly = (it == lastY.end() || f.t == 0) ? f.y : it->second;
            float dy = f.y - ly;
            off -= dy;
            vel = -dy / std::max(dt, 0.001f);
            lastY[f.id] = f.y;
            dragging = true;
        }
    }
    if (!dragging) { off += vel * dt; vel *= std::exp(-6.f * dt); }
    if (off < 0) { off = damp(off, 0, 14, dt); vel = 0; }
    if (off > maxOff) { off = damp(off, maxOff, 14, dt); vel = 0; }
    // scrollbar
    if (contentH > r.h) {
        float bh = std::max(30.f, r.h * r.h / contentH);
        float by = r.y + (r.h - bh) * saturate(off / std::max(1.f, maxOff));
        rect({r.x + r.w - 5, by, 3, bh}, vec4(0.85f, 0.7f, 0.4f, 0.5f));
    }
    return off;
}

float UI::tapFlash(int key) const { auto it = flashes.find(key); return it == flashes.end() ? 0.f : it->second; }

bool UI::fancyButton(Rect r, const std::string& label, bool enabled, const std::string& style, float ts) {
    bool h = enabled && held(r);
    std::string st = h ? style + "_hi" : style;
    if (!hasRegion(st)) st = style;
    float press = h ? 2.f : 0.f;
    Rect rr = {r.x, r.y + press, r.w, r.h};
    nine(st, rr, enabled ? vec4(1, 1, 1, 1) : vec4(0.5f, 0.5f, 0.5f, 0.8f), std::min(1.f, r.h / 64.f));
    text(label, rr.x + rr.w / 2, rr.y + rr.h / 2 - ts * 0.62f, ts, enabled ? rgba(0xf6e7c1) : rgba(0x9a8f7a), F_SERIF, A_CENTER, 0, true);
    return enabled && button(r);
}

bool UI::iconButton(Rect r, const std::string& icon, const std::string& base, vec4 tint, bool enabled) {
    bool h = enabled && held(r);
    std::string b = h && hasRegion(base + "_hi") ? base + "_hi" : base;
    float s = h ? 0.94f : 1.f;
    Rect rr = {r.x + r.w * (1 - s) / 2, r.y + r.h * (1 - s) / 2, r.w * s, r.h * s};
    sprite(b, rr, enabled ? vec4(1, 1, 1, 0.92f) : vec4(0.6f, 0.6f, 0.6f, 0.6f));
    Rect ir = rr.inset(rr.w * 0.24f);
    sprite("i_" + icon, ir, enabled ? tint : vec4(0.5f, 0.5f, 0.5f, 0.6f));
    return enabled && button(r);
}

void UI::panel(Rect r, const std::string& title, bool parchment) {
    // drop shadow
    rect({r.x + 6, r.y + 8, r.w, r.h}, vec4(0, 0, 0, 0.35f));
    nine(parchment ? "parchment" : "panel", r, vec4(1, 1, 1, 0.98f), 0.9f);
    if (!title.empty()) {
        float tw = std::max(260.f, textWidth(title, 30, F_SERIF) + 140);
        Rect rb = {r.x + r.w / 2 - tw / 2, r.y - 26, tw, 62};
        sprite("ribbon", rb);
        text(title, r.x + r.w / 2, r.y - 26 + 13, 28, rgba(0xf8e6b8), F_SERIF, A_CENTER, 0, true);
    }
}

}  // namespace agr
