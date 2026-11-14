// AGR Engine — UI renderer & immediate-mode widgets.
// Virtual coordinates: the screen is 720 units tall; width = 720 * aspect. One batch, three textures
// (atlas, SDF font, dynamic image). Text uses signed distance fields with outline/shadow.
#pragma once
#include "GL.h"
#include "../core/Platform.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace agr {

struct UIRegion { float x, y, w, h; float border; };
struct Glyph { float x, y, w, h, xoff, yoff, adv; };
struct FontFace { std::string name; float size, lineH, asc; std::unordered_map<uint32_t, Glyph> glyphs; std::unordered_map<uint64_t, float> kern; };

enum FontId { F_TITLE = 0, F_SERIF = 1, F_BODY = 2, F_BOLD = 3 };
enum Align { A_LEFT = 0, A_CENTER = 1, A_RIGHT = 2 };

struct UIVertex { float x, y; uint8_t r, g, b, a; float u, v, mode, soft; };

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && py >= y && px <= x + w && py <= y + h; }
    Rect inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
};

inline vec4 rgba(uint32_t hex, float a = 1.f) { return vec4(hexColor(hex), a); }

class UI {
public:
    bool init();
    void shutdown();
    void begin(int screenW, int screenH, float dt);
    void end(Shader& sh);

    float W = 1280, H = 720;     // virtual size
    float scale = 1;             // pixels per unit
    float dt = 0.016f;
    float time = 0;

    // --- drawing ----------------------------------------------------------------------
    void rect(Rect r, vec4 c);
    void rectGrad(Rect r, vec4 top, vec4 bottom);
    void sprite(const std::string& region, Rect r, vec4 c = vec4(1, 1, 1, 1));
    void spriteRot(const std::string& region, vec2 center, vec2 size, float angle, vec4 c = vec4(1, 1, 1, 1));
    void nine(const std::string& region, Rect r, vec4 c = vec4(1, 1, 1, 1), float borderScale = 1.f);
    void image(GLuint tex, Rect r, vec4 uv = vec4(0, 0, 1, 1), vec4 c = vec4(1, 1, 1, 1));
    void imageRot(GLuint tex, vec2 center, vec2 size, float angle, vec4 uv, vec4 c);
    // circular textured disc (minimap): uvCenter/uvRadius in texture space, rotated by angle
    void imageCircle(GLuint tex, vec2 center, float radius, vec2 uvCenter, float uvRadius, float angle, vec4 c, int seg = 40);
    // text: returns height used. maxW > 0 enables word wrapping.
    float text(const std::string& s, float x, float y, float size, vec4 c, int font = F_BODY, int align = A_LEFT, float maxW = 0, bool outline = false);
    float textWidth(const std::string& s, float size, int font = F_BODY) const;
    float textHeight(const std::string& s, float size, int font, float maxW) const;
    void bar(Rect r, float frac, vec4 col, float ghost = -1, vec4 ghostCol = vec4(1, 1, 1, 0.5f));
    void circle(vec2 c, float r, vec4 col, int seg = 24);
    void ring(vec2 c, float r0, float r1, float a0, float a1, vec4 col, int seg = 32);
    bool hasRegion(const std::string& n) const { return regions.count(n) > 0; }

    // --- clipping ---------------------------------------------------------------------
    void pushClip(Rect r);
    void popClip();

    // --- input ------------------------------------------------------------------------
    // consumed: owned by a widget (a button fired, a slider is dragged, a HUD control holds it).
    // blocked: the finger touched a modal UI (dialogue, menu): it must not drive the world (camera, joystick),
    //          but the menu's own buttons still see it. Both flags last for the finger's whole life.
    struct Finger { int id; float x, y, sx, sy; bool down, pressed, released, consumed; float t; bool blocked = false; };
    void onTouch(const TouchEvent& e);       // pixels → stored in virtual units
    std::vector<Finger> fingers;
    bool button(Rect r, int id = -1);        // true on release inside (tap)
    bool held(Rect r) const;                 // finger currently inside
    bool pressedIn(Rect r);                  // a finger went down inside this frame (consumes it)
    // vertical scroll area: returns scroll offset (persistent per key)
    float scrollArea(const std::string& key, Rect r, float contentH);
    bool anyFingerOver(Rect r) const;
    void consumeAll();                       // modal UIs: block world input (sets `blocked`, buttons still work)
    // Test support: when enabled, every labelled widget drawn this frame is recorded (rect + label).
    bool recordWidgets = false;
    std::vector<std::pair<Rect, std::string>> widgets, lastWidgets;   // this frame / previous frame
    void tag(Rect r, const std::string& label) { if (recordWidgets) widgets.push_back({r, label}); }
    const Rect* findWidget(const std::string& label) const { for (auto& w : lastWidgets) if (w.second == label) return &w.first; return nullptr; }
    bool fingerConsumed(int id) const;
    void endInput();                          // clears per-frame flags

    // Fancy primitive widgets shared by all menus
    bool fancyButton(Rect r, const std::string& label, bool enabled = true, const std::string& style = "button", float textSize = 26);
    bool iconButton(Rect r, const std::string& icon, const std::string& base = "round", vec4 tint = vec4(1, 1, 1, 1), bool enabled = true);
    // same look, no input (for icons whose taps are handled elsewhere, e.g. the HUD shortcuts)
    void iconDraw(Rect r, const std::string& icon, const std::string& base = "round", vec4 tint = vec4(1, 1, 1, 1), bool enabled = true);
    void panel(Rect r, const std::string& title = "", bool parchment = false);
    float tapFlash(int key) const;

    std::vector<FontFace> fonts;

private:
    std::unordered_map<std::string, UIRegion> regions;
    Texture atlas, font;
    std::vector<UIVertex> verts;
    std::vector<uint16_t> idx;
    struct Batch { GLuint image; int start, count; Rect clip; bool clipOn; };
    std::vector<Batch> batches;
    std::vector<Rect> clipStack;
    GLuint vao = 0, vbo = 0, ibo = 0;
    GLuint curImage = 0;
    std::unordered_map<std::string, float> scrolls, scrollVel;
    std::unordered_map<int, float> flashes;
    int pixelW = 1, pixelH = 1;
    void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, vec4 c, float mode, float soft = 0);
    void ensureBatch(GLuint image);
    bool loadAtlas();
    bool loadFont();
};

// UTF-8 decoding helper
uint32_t utf8Next(const std::string& s, size_t& i);

}  // namespace agr
