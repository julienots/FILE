#include "agr/ui/UISystem.hpp"

#include "agr/runtime/Localization.hpp"

#include <algorithm>
#include <cmath>

#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/scene/Components.hpp"

namespace agr {

namespace {

using math::Vec2;
using math::Vec4;

bool contains(Vec4 r, Vec2 p) { return p.x >= r.x && p.y >= r.y && p.x < r.x + r.z && p.y < r.y + r.w; }
Vec4 intersect(Vec4 a, Vec4 b) {
    const float x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.x + a.z, b.x + b.z), y1 = std::min(a.y + a.w, b.y + b.w);
    return {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}
bool interactiveKind(ecs::Registry& reg, ecs::Entity e) {
    return reg.has<UIButtonComponent>(e) || reg.has<UISliderComponent>(e) || reg.has<UIToggleComponent>(e) || reg.has<UIInputFieldComponent>(e) ||
           reg.has<UIDropdownComponent>(e) || reg.has<UITabsComponent>(e) || reg.has<UIItemListComponent>(e) || reg.has<UIColorPickerComponent>(e);
}
bool interactableNow(ecs::Registry& reg, ecs::Entity e) {
    if (const auto* b = reg.tryGet<UIButtonComponent>(e)) return b->interactable;
    if (const auto* d = reg.tryGet<UIDropdownComponent>(e)) return d->interactable;
    if (reg.has<UITabsComponent>(e)) return true;
    if (const auto* l = reg.tryGet<UIItemListComponent>(e)) return l->interactable;
    if (const auto* c = reg.tryGet<UIColorPickerComponent>(e)) return c->interactable;
    if (const auto* s = reg.tryGet<UISliderComponent>(e)) return s->interactable;
    if (const auto* t = reg.tryGet<UIToggleComponent>(e)) return t->interactable;
    if (const auto* f = reg.tryGet<UIInputFieldComponent>(e)) return f->interactable;
    return false;
}
/// Number of UTF-8 code points.
usize utf8Length(const std::string& s) {
    usize n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}
/// Byte offset of code point `index` (the end if past it).
usize utf8Offset(const std::string& s, usize index) {
    usize n = 0;
    for (usize i = 0; i < s.size(); ++i)
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            if (n == index) return i;
            ++n;
        }
    return s.size();
}
/// Code point at `index` as a string ("" past the end).
std::string utf8At(const std::string& s, usize index) {
    const usize a = utf8Offset(s, index), b = utf8Offset(s, index + 1);
    return s.substr(a, b - a);
}
void utf8PopBack(std::string& s) {
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0) != 0x80) break;
    }
}

Vec4 mul(Vec4 a, Vec4 b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }

/// "A;B;C" -> {"A", "B", "C"} (empty items kept: their place counts).
std::vector<std::string> splitList(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ';') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty() || !out.empty()) out.push_back(cur);
    return out;
}

Vec4 hsvToRgb(float h, float s, float v, float a) {
    h = h - std::floor(h);
    const float i = std::floor(h * 6.0f), f = h * 6.0f - i;
    const float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (static_cast<int>(i) % 6) {
        case 0: return {v, t, p, a};
        case 1: return {q, v, p, a};
        case 2: return {p, v, t, a};
        case 3: return {p, q, v, a};
        case 4: return {t, p, v, a};
        default: return {v, p, q, a};
    }
}
math::Vec3 rgbToHsv(Vec4 c, float keepHue) {
    const float mx = std::max({c.x, c.y, c.z}), mn = std::min({c.x, c.y, c.z}), d = mx - mn;
    float h = keepHue;
    if (d > 1e-6f) {
        if (mx == c.x) h = (c.y - c.z) / d / 6.0f;
        else if (mx == c.y) h = ((c.z - c.x) / d + 2.0f) / 6.0f;
        else h = ((c.x - c.y) / d + 4.0f) / 6.0f;
        if (h < 0) h += 1.0f;
    }
    return {h, mx > 1e-6f ? d / mx : 0.0f, mx};
}

bool isWhite(Vec4 c) { return c.x == 1.0f && c.y == 1.0f && c.z == 1.0f; }

std::optional<Vec4> parseColor(const std::string& v) {
    static const std::pair<const char*, Vec4> named[] = {
        {"white", {1, 1, 1, 1}},          {"black", {0, 0, 0, 1}},       {"red", {0.95f, 0.25f, 0.2f, 1}},   {"green", {0.3f, 0.85f, 0.35f, 1}},
        {"blue", {0.3f, 0.55f, 1.0f, 1}}, {"yellow", {1, 0.86f, 0.2f, 1}}, {"orange", {1, 0.58f, 0.15f, 1}}, {"purple", {0.7f, 0.4f, 1.0f, 1}},
        {"gray", {0.6f, 0.6f, 0.6f, 1}},  {"cyan", {0.3f, 0.9f, 0.95f, 1}}};
    for (const auto& [n, c] : named)
        if (v == n) return c;
    if (v.size() != 7 && v.size() != 9) return std::nullopt;
    if (v[0] != '#') return std::nullopt;
    auto hex = [&](usize i) -> int {
        auto d = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
        const int a = d(v[i]), b = d(v[i + 1]);
        return a < 0 || b < 0 ? -1 : a * 16 + b;
    };
    const int r = hex(1), g = hex(3), b = hex(5), a = v.size() == 9 ? hex(7) : 255;
    if (r < 0 || g < 0 || b < 0 || a < 0) return std::nullopt;
    return Vec4{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
}

/// Rich text: removes the tags and returns the style of every remaining code point.
/// Tags: [b] [i] [u] [s] [color=red|#rrggbb] [url=id] and their closing [/...]; "[[" = "[".
template <typename Style>
std::string parseRichText(const std::string& in, std::vector<Style>& styles, std::vector<std::string>& links) {
    std::string out;
    std::vector<Vec4> colors;
    int bold = 0, underline = 0, italic = 0, strike = 0;
    i32 link = -1;
    auto current = [&] {
        Style st;
        st.color = colors.empty() ? Vec4{1, 1, 1, 1} : colors.back();
        st.hasColor = !colors.empty();
        st.bold = bold > 0, st.underline = underline > 0, st.italic = italic > 0, st.strike = strike > 0;
        st.link = link;
        return st;
    };
    for (usize i = 0; i < in.size();) {
        if (in[i] == '[') {
            if (i + 1 < in.size() && in[i + 1] == '[') {  // "[[" = a literal "["
                out += '[';
                styles.push_back(current());
                i += 2;
                continue;
            }
            const usize close = in.find(']', i);
            if (close != std::string::npos) {
                const std::string tag = in.substr(i + 1, close - i - 1);
                bool known = true;
                if (tag == "b") ++bold;
                else if (tag == "/b") bold = std::max(0, bold - 1);
                else if (tag == "u") ++underline;
                else if (tag == "/u") underline = std::max(0, underline - 1);
                else if (tag == "i") ++italic;
                else if (tag == "/i") italic = std::max(0, italic - 1);
                else if (tag == "s") ++strike;
                else if (tag == "/s") strike = std::max(0, strike - 1);
                else if (tag.rfind("img=", 0) == 0) {  // an em space the image is drawn on
                    links.push_back(tag.substr(4));
                    out += "\xE2\x80\x83";
                    Style st = current();
                    st.image = static_cast<i32>(links.size()) - 1;
                    styles.push_back(st);
                    i = close + 1;
                    continue;
                } else if (tag.rfind("url=", 0) == 0) {
                    links.push_back(tag.substr(4));
                    link = static_cast<i32>(links.size()) - 1;
                } else if (tag == "/url") {
                    link = -1;
                } else if (tag == "/color") {
                    if (!colors.empty()) colors.pop_back();
                } else if (tag.rfind("color=", 0) == 0) {
                    if (auto c = parseColor(tag.substr(6))) colors.push_back(*c);
                    else known = false;
                } else {
                    known = false;
                }
                if (known) {
                    i = close + 1;
                    continue;
                }
            }
        }
        // One code point (UTF-8) with the current style.
        const unsigned char c = static_cast<unsigned char>(in[i]);
        const usize n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        out.append(in, i, n);
        styles.push_back(current());
        i += n;
    }
    return out;
}

Vec4 anchoredRect(const UIRectComponent& r, Vec4 p, float s) {
    const float ax0 = p.x + p.z * r.anchorMin.x, ax1 = p.x + p.z * r.anchorMax.x;
    const float ay0 = p.y + p.w * r.anchorMin.y, ay1 = p.y + p.w * r.anchorMax.y;
    const float w = std::max(0.0f, (ax1 - ax0) + r.size.x * s), h = std::max(0.0f, (ay1 - ay0) + r.size.y * s);
    const float px = ax0 + (ax1 - ax0) * r.pivot.x + r.position.x * s;
    const float py = ay0 + (ay1 - ay0) * r.pivot.y + r.position.y * s;
    return {px - w * r.pivot.x, py - h * r.pivot.y, w, h};
}

float alignIn(UIAlign a, float start, float available, float size) {
    if (a == UIAlign::Center) return start + (available - size) * 0.5f;
    if (a == UIAlign::End) return start + available - size;
    return start;
}

bool isActive(Scene& scene, ecs::Entity e) {
    const auto* st = scene.registry().tryGet<StateComponent>(e);
    return !st || st->active;
}

} // namespace

UISystem::UISystem(VirtualFileSystem& vfs) : m_fonts(vfs), m_vfs(vfs) {}

const UITheme* UISystem::theme(const std::string& uri) {
    if (uri.empty()) return nullptr;
    const i64 modified = m_vfs.modifiedTime(uri);
    auto it = m_themes.find(uri);
    if (it != m_themes.end() && it->second.modified == modified) return it->second.theme.get();
    ThemeEntry& e = m_themes[uri];
    e.modified = modified;
    e.theme.reset();
    auto text = m_vfs.readText(uri);
    if (!text) {
        AGR_LOG_ERROR("UI", "theme '{}': {}", uri, text.error().message);
        return nullptr;
    }
    auto parsed = UITheme::fromJson(text.value());
    if (!parsed) {
        AGR_LOG_ERROR("UI", "theme '{}': {}", uri, parsed.error().message);
        return nullptr;
    }
    e.theme = std::make_unique<UITheme>(std::move(parsed.value()));
    return e.theme.get();
}

std::optional<TextLayout> UISystem::textLayout(const std::string& font, const std::string& text, float pixelSize, float width, bool wrap, UIHAlign align) {
    auto lay = m_fonts.layout(font, text, pixelSize, width, wrap, 1.0f, align);
    if (!lay) {
        if (m_reportedFonts.insert(font).second) AGR_LOG_ERROR("UI", "font '{}': {}", font, lay.error().message);
        return std::nullopt;
    }
    return std::move(lay.value());
}
UISystem::~UISystem() {
    if (m_shapeDevice) {
        if (m_discTexture) m_shapeDevice->destroyTexture(m_discTexture);
        if (m_softTexture) m_shapeDevice->destroyTexture(m_softTexture);
    }
}

void UISystem::ensureShapeTextures() {
    if (m_discTexture || !m_renderer) return;
    m_shapeDevice = &m_renderer->device();
    constexpr u32 kSize = 64;
    std::vector<u8> disc(kSize * kSize * 4, 255), soft(kSize * kSize * 4, 255);
    for (u32 y = 0; y < kSize; ++y)
        for (u32 x = 0; x < kSize; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - 32.0f, dy = static_cast<float>(y) + 0.5f - 32.0f;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = std::clamp(32.0f - d, 0.0f, 1.0f);  // 1-texel antialiased edge
            float t = std::clamp(1.0f - d / 32.0f, 0.0f, 1.0f);
            t = t * t * (3.0f - 2.0f * t);
            disc[(y * kSize + x) * 4 + 3] = static_cast<u8>(a * 255.0f + 0.5f);
            soft[(y * kSize + x) * 4 + 3] = static_cast<u8>(t * 255.0f + 0.5f);
        }
    auto make = [&](const std::vector<u8>& px, const char* name) {
        rhi::TextureDesc td;
        td.width = td.height = kSize;
        td.format = rhi::TextureFormat::RGBA8;
        td.generateMips = false;
        td.sampler = {rhi::Filter::Linear, rhi::Filter::Linear, false, rhi::Wrap::Clamp, rhi::Wrap::Clamp};
        td.data = px.data();
        td.debugName = name;
        auto t = m_shapeDevice->createTexture(td);
        return t ? t.value() : rhi::TextureHandle{};
    };
    m_discTexture = make(disc, "agr.ui.disc");
    m_softTexture = make(soft, "agr.ui.softdisc");
}

void UISystem::roundBox(OverlayDrawList& out, Vec4 rect, float radius, u32 color, bool soft) {
    if (rect.z <= 0.0f || rect.w <= 0.0f) return;
    const rhi::TextureHandle tex = soft ? m_softTexture : m_discTexture;
    const float r = std::min({radius, rect.z * 0.5f, rect.w * 0.5f});
    if (r <= 0.0f || !tex) {
        emit(out, {rect.x, rect.y}, {rect.x + rect.z, rect.y + rect.w}, {0, 0, 1, 1}, color, {});
        return;
    }
    // 9-slice of the disc: quarter discs in the corners, the middle row / column of texels elsewhere.
    const float xs[4] = {rect.x, rect.x + r, rect.x + rect.z - r, rect.x + rect.z};
    const float ys[4] = {rect.y, rect.y + r, rect.y + rect.w - r, rect.y + rect.w};
    const float us[4] = {0.0f, 0.5f, 0.5f, 1.0f};
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) {
            if (xs[x + 1] <= xs[x] || ys[y + 1] <= ys[y]) continue;
            emit(out, {xs[x], ys[y]}, {xs[x + 1], ys[y + 1]}, {us[x], us[y], us[x + 1], us[y + 1]}, color, tex);
        }
}

void UISystem::roundBox(OverlayDrawList& out, Vec4 rect, Vec4 radii, u32 color, bool soft) {
    if (rect.z <= 0.0f || rect.w <= 0.0f) return;
    if (radii.x == radii.y && radii.y == radii.z && radii.z == radii.w) return roundBox(out, rect, radii.x, color, soft);
    const rhi::TextureHandle tex = soft ? m_softTexture : m_discTexture;
    const float lim = std::min(rect.z, rect.w) * 0.5f;
    const float tl = std::clamp(radii.x, 0.0f, lim), tr = std::clamp(radii.y, 0.0f, lim), br = std::clamp(radii.z, 0.0f, lim),
                bl = std::clamp(radii.w, 0.0f, lim);
    const float x0 = rect.x, x1 = rect.x + rect.z, y0 = rect.y, y1 = rect.y + rect.w;
    auto fill = [&](float ax, float ay, float bx, float by) {
        if (bx > ax && by > ay) emit(out, {ax, ay}, {bx, by}, {0, 0, 1, 1}, color, {});
    };
    auto corner = [&](float ax, float ay, float r, Vec4 uv) {
        if (r <= 0.0f) return;
        if (tex) emit(out, {ax, ay}, {ax + r, ay + r}, uv, color, tex);
        else fill(ax, ay, ax + r, ay + r);
    };
    // Quarter discs (texture quadrants), then bands that cover the rest exactly once.
    corner(x0, y0, tl, {0.0f, 0.0f, 0.5f, 0.5f});
    corner(x1 - tr, y0, tr, {0.5f, 0.0f, 1.0f, 0.5f});
    corner(x1 - br, y1 - br, br, {0.5f, 0.5f, 1.0f, 1.0f});
    corner(x0, y1 - bl, bl, {0.0f, 0.5f, 0.5f, 1.0f});
    const float top = std::max(tl, tr), bottom = std::max(bl, br);
    fill(x0 + tl, y0, x1 - tr, y0 + top);            // top band between the corners
    fill(x0, y0 + tl, x0 + tl, y0 + top);            // under a smaller top-left corner
    fill(x1 - tr, y0 + tr, x1, y0 + top);            // under a smaller top-right corner
    fill(x0, y0 + top, x1, y1 - bottom);             // middle
    fill(x0 + bl, y1 - bottom, x1 - br, y1);         // bottom band
    fill(x0, y1 - bottom, x0 + bl, y1 - bl);         // above a smaller bottom-left corner
    fill(x1 - br, y1 - bottom, x1, y1 - br);         // above a smaller bottom-right corner
}

void UISystem::setRenderer(Renderer* renderer) { m_renderer = renderer; }

float UISystem::canvasScale(ecs::Entity canvas) const {
    for (const CanvasPass& p : m_passes)
        if (p.canvas == canvas) return p.scale;
    return 1.0f;
}

// ------------------------------------------------------------------ layout
void UISystem::layout(Scene& scene, u32 width, u32 height) {
    Stopwatch sw;
    auto& reg = scene.registry();
    if (&scene != m_layoutScene || scene.generation() != m_layoutGeneration) {
        reset();
        m_layoutScene = &scene;
        m_layoutGeneration = scene.generation();
    }
    m_passes.clear();
    if (m_localization)  // translated texts follow the current language (and table edits)
        reg.view<UITextComponent>().each([&](ecs::Entity, UITextComponent& t) {
            if (!t.localizationKey.empty()) {
                const std::string& tr = m_localization->tr(t.localizationKey);
                if (t.text != tr) t.text = tr;
            }
        });
    // Text fields show their text (or placeholder) through their UIText.
    reg.view<UIInputFieldComponent>().each([&](ecs::Entity e, UIInputFieldComponent& f) {
        auto* t = reg.tryGet<UITextComponent>(e);
        if (!t) return;
        t->localizationKey.clear();
        const std::string shown = f.text.empty() ? f.placeholder : f.password ? std::string(utf8Length(f.text), '*') : f.text;
        if (t->text != shown) t->text = shown;
        t->color = f.text.empty() ? f.placeholderColor : f.textColor;
    });
    m_scroll.clear();
    const Vec4 screen{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    reg.view<UICanvasComponent>().each([&](ecs::Entity e, UICanvasComponent& c) {
        if (!scene.isActiveInHierarchy(e)) return;
        for (ecs::Entity p = scene.parent(e); !p.isNull(); p = scene.parent(p))
            if (reg.has<UICanvasComponent>(p)) return;  // nested canvas: laid out by its root
        CanvasPass pass;
        pass.canvas = e;
        pass.sortOrder = c.sortOrder;
        pass.hierarchyIndex = e.index;
        pass.theme = theme(c.theme);
        if (c.scaleMode == UIScaleMode::ConstantPixelSize) {
            pass.scale = std::max(0.01f, c.pixelScale);
        } else {
            const float rw = std::max(1.0f, c.referenceResolution.x), rh = std::max(1.0f, c.referenceResolution.y);
            const float lw = std::log2(std::max(1.0f, screen.z) / rw), lh = std::log2(std::max(1.0f, screen.w) / rh);
            pass.scale = std::exp2(math::lerp(lw, lh, std::clamp(c.match, 0.0f, 1.0f)));
        }
        Element root{e, screen, false, false, screen};
        if (auto* r = reg.tryGet<UIRectComponent>(e)) r->screenRect = screen;
        const auto* img = reg.tryGet<UIImageComponent>(e);
        root.blocks = img && img->blocksPointer && img->color.w > 0.0f;
        pass.elements.push_back(root);
        Vec4 content = screen;
        if (c.respectSafeArea) {
            const Vec4 in = m_safeArea;
            content = {in.x, in.y, std::max(0.0f, screen.z - in.x - in.z), std::max(0.0f, screen.w - in.y - in.w)};
        }
        layoutChildren(scene, e, content, pass.scale, pass, screen, Affine{});
        m_passes.push_back(std::move(pass));
    });
    std::stable_sort(m_passes.begin(), m_passes.end(), [](const CanvasPass& a, const CanvasPass& b) {
        return a.sortOrder != b.sortOrder ? a.sortOrder < b.sortOrder : a.hierarchyIndex < b.hierarchyIndex;
    });
    layoutPopup();
    layoutDialog(scene, width, height);
    m_stats.canvases = static_cast<u32>(m_passes.size());
    m_stats.elements = 0;
    for (const CanvasPass& p : m_passes) m_stats.elements += static_cast<u32>(p.elements.size());
    m_stats.layoutMs = sw.elapsedMilliseconds();
}

void UISystem::layoutChildren(Scene& scene, ecs::Entity parent, Vec4 pr, float s, CanvasPass& pass, Vec4 clip, const Affine& xform) {
    auto& reg = scene.registry();
    std::vector<ecs::Entity> kids;
    scene.forEachChild(parent, [&](ecs::Entity c) {
        if (reg.has<UIRectComponent>(c) && isActive(scene, c)) kids.push_back(c);
    });
    if (kids.empty()) return;
    if (auto* tabs = reg.tryGet<UITabsComponent>(parent)) {
        // Only the current page is shown, in the area under the tabs.
        tabs->current = std::clamp(tabs->current, 0, static_cast<i32>(kids.size()) - 1);
        kids = {kids[static_cast<usize>(tabs->current)]};
        const float header = std::min(tabs->headerHeight * s, pr.w);
        pr = {pr.x, pr.y + header, pr.z, pr.w - header};
    }
    std::vector<Vec4> rects(kids.size());
    if (const auto* lay = reg.tryGet<UILayoutComponent>(parent)) {
        const Vec4 content{pr.x + lay->padding.x * s, pr.y + lay->padding.y * s, std::max(0.0f, pr.z - (lay->padding.x + lay->padding.z) * s),
                           std::max(0.0f, pr.w - (lay->padding.y + lay->padding.w) * s)};
        const float sp = lay->spacing * s;
        if (lay->type == UILayoutType::Grid) {
            const float cw = std::max(1.0f, lay->cellSize.x * s), ch = std::max(1.0f, lay->cellSize.y * s);
            const usize cols = std::max<usize>(1, static_cast<usize>(std::floor((content.z + sp) / (cw + sp))));
            const usize used = std::min(cols, kids.size());
            const float rowWidth = static_cast<float>(used) * cw + static_cast<float>(used - 1) * sp;
            const float x0 = alignIn(lay->childAlignment, content.x, content.z, rowWidth);
            for (usize i = 0; i < kids.size(); ++i)
                rects[i] = {x0 + static_cast<float>(i % cols) * (cw + sp), content.y + static_cast<float>(i / cols) * (ch + sp), cw, ch};
        } else {
            const bool vertical = lay->type == UILayoutType::Vertical;
            float cursor = vertical ? content.y : content.x;
            for (usize i = 0; i < kids.size(); ++i) {
                const auto& r = reg.get<UIRectComponent>(kids[i]);
                if (vertical) {
                    const float w = lay->expandChildren ? content.z : r.size.x * s, h = r.size.y * s;
                    rects[i] = {alignIn(lay->childAlignment, content.x, content.z, w), cursor, w, h};
                    cursor += h + sp;
                } else {
                    const float w = r.size.x * s, h = lay->expandChildren ? content.w : r.size.y * s;
                    rects[i] = {cursor, alignIn(lay->childAlignment, content.y, content.w, h), w, h};
                    cursor += w + sp;
                }
            }
        }
    } else {
        for (usize i = 0; i < kids.size(); ++i) rects[i] = anchoredRect(reg.get<UIRectComponent>(kids[i]), pr, s);
    }
    Vec4 childClip = clip;
    if (auto* sv = reg.tryGet<UIScrollViewComponent>(parent)) {
        // The children are the content: measure it, clamp the scroll, move it, clip it.
        float w = 0.0f, h = 0.0f;
        for (const Vec4& r : rects) {
            w = std::max(w, r.x + r.z - pr.x);
            h = std::max(h, r.y + r.w - pr.y);
        }
        if (const auto* lay = reg.tryGet<UILayoutComponent>(parent)) {
            w += lay->padding.z * s;
            h += lay->padding.w * s;
        }
        const float maxX = sv->horizontal ? std::max(0.0f, (w - pr.z) / s) : 0.0f, maxY = sv->vertical ? std::max(0.0f, (h - pr.w) / s) : 0.0f;
        sv->scroll = {std::clamp(sv->scroll.x, 0.0f, maxX), std::clamp(sv->scroll.y, 0.0f, maxY)};
        for (Vec4& r : rects) {
            r.x -= sv->scroll.x * s;
            r.y -= sv->scroll.y * s;
        }
        m_scroll[parent.packed()] = {pr, {w, h}, s};
        childClip = intersect(clip, pr);
    }
    for (usize i = 0; i < kids.size(); ++i) {
        const ecs::Entity c = kids[i];
        const auto& rc = reg.get<UIRectComponent>(c);
        // Scale around the pivot (children laid out in the scaled rectangle, sizes scaled by the mean).
        Vec4 rect = rects[i];
        float childScale = s;
        if (rc.scale.x != 1.0f || rc.scale.y != 1.0f) {
            const float sx = std::max(rc.scale.x, 0.0f), sy = std::max(rc.scale.y, 0.0f);
            const float px = rect.x + rect.z * rc.pivot.x, py = rect.y + rect.w * rc.pivot.y;
            rect = {px - rect.z * sx * rc.pivot.x, py - rect.w * sy * rc.pivot.y, rect.z * sx, rect.w * sy};
            childScale = s * (sx + sy) * 0.5f;
        }
        Affine xf = xform;
        if (rc.rotation != 0.0f) {  // rotation around the pivot, after the parents'
            const float px = rect.x + rect.z * rc.pivot.x, py = rect.y + rect.w * rc.pivot.y;
            const float cs = std::cos(rc.rotation), sn = std::sin(rc.rotation);
            const Affine r{cs, sn, -sn, cs, px - cs * px + sn * py, py - sn * px - cs * py};
            xf = xform * r;
        }
        reg.get<UIRectComponent>(c).screenRect = rect;
        Element el{c, rect, false, false, childClip, xf, childScale};
        const auto* img = reg.tryGet<UIImageComponent>(c);
        el.interactive = interactiveKind(reg, c);
        el.blocks = el.interactive || reg.has<UIScrollViewComponent>(c) || (img && img->blocksPointer && img->color.w > 0.0f);
        if (auto* list = reg.tryGet<UIItemListComponent>(c)) {  // keep the scroll inside the content
            const float content = static_cast<float>(listRows(*list).size()) * list->itemHeight * childScale;
            list->scroll = std::clamp(list->scroll, 0.0f, std::max(0.0f, (content - rect.w) / std::max(childScale, 0.01f)));
        }
        pass.elements.push_back(el);
        layoutChildren(scene, c, rect, childScale, pass, childClip, xf);
    }
}

// ------------------------------------------------------------------- input
ecs::Entity UISystem::hitTest(Vec2 p) const {
    for (auto pass = m_passes.rbegin(); pass != m_passes.rend(); ++pass)
        for (auto el = pass->elements.rbegin(); el != pass->elements.rend(); ++el)
            if (el->blocks && contains(el->rect, localPoint(*el, p)) && contains(el->clip, localPoint(*el, p))) return el->entity;
    return {};
}

ecs::Entity UISystem::elementAt(Vec2 p) const {
    for (auto pass = m_passes.rbegin(); pass != m_passes.rend(); ++pass)
        for (auto el = pass->elements.rbegin(); el != pass->elements.rend(); ++el)
            if (el->entity != pass->canvas && contains(el->rect, localPoint(*el, p))) return el->entity;
    return {};
}

ecs::Entity UISystem::canvasOf(ecs::Entity element) const {
    for (const CanvasPass& pass : m_passes)
        for (const Element& el : pass.elements)
            if (el.entity == element) return pass.canvas;
    return {};
}

bool UISystem::isPointerOverUI(Vec2 p) const { return !hitTest(p).isNull(); }

bool UISystem::isPressed(ecs::Entity e) const {
    for (auto& [id, pressed] : m_pressed)
        if (pressed == e) return true;
    return false;
}

void UISystem::setSliderValue(Scene& scene, ecs::Entity e, Vec2 position) {
    auto* s = scene.registry().tryGet<UISliderComponent>(e);
    auto* r = scene.registry().tryGet<UIRectComponent>(e);
    if (!s || !r || !s->interactable) return;
    const Vec4 rect = r->screenRect;
    float t;
    if (s->vertical) {  // bottom = min
        const float handle = rect.z * 0.5f;
        t = std::clamp((rect.y + rect.w - handle - position.y) / std::max(1.0f, rect.w - 2.0f * handle), 0.0f, 1.0f);
    } else {
        const float handle = rect.w * 0.5f;
        t = std::clamp((position.x - rect.x - handle) / std::max(1.0f, rect.z - 2.0f * handle), 0.0f, 1.0f);
    }
    float v = s->minValue + t * (s->maxValue - s->minValue);
    if (s->wholeNumbers) v = std::round(v);
    if (v != s->value) {
        s->value = v;
        m_events.push_back({UIEventType::ValueChanged, e, v, {}});
    }
}

void UISystem::processInput(Scene& scene, std::span<const UIPointer> pointers) {
    auto& reg = scene.registry();
    m_hovered.clear();
    m_pointerOverUI = false;
    std::unordered_set<u64> seen;
    if (!m_focusedField.isNull() && (!reg.valid(m_focusedField) || !reg.has<UIInputFieldComponent>(m_focusedField))) focusField({});
    if (!m_openDropdown.isNull() && (!reg.valid(m_openDropdown) || !reg.has<UIDropdownComponent>(m_openDropdown) || !elementOf(m_openDropdown)))
        m_openDropdown = {};
    ecs::Entity tooltipHit;
    for (const UIPointer& p : pointers) {
        seen.insert(p.id);
        const bool wasDown = m_pointerDown[p.id];
        if (dialogPointer(scene, p, wasDown)) {  // a modal dialog takes every pointer
            m_pointerDown[p.id] = p.down;
            continue;
        }
        if (popupPointer(scene, p, wasDown)) {
            m_pointerDown[p.id] = p.down;
            continue;
        }
        const ecs::Entity hit = hitTest(p.position);
        if (!p.down && !hit.isNull()) {  // tooltip: the element (or a parent) with one under a resting pointer
            for (ecs::Entity e = hit; !e.isNull() && reg.valid(e); e = scene.parent(e)) {
                if (reg.has<UITooltipComponent>(e)) {
                    tooltipHit = e;
                    m_tooltipPos = p.position;
                    break;
                }
                if (reg.has<UICanvasComponent>(e)) break;
            }
        }
        ecs::Entity top = hit;
        if (!top.isNull()) m_pointerOverUI = true;
        // Images and texts inside a control (icons, labels) forward the press to it.
        for (ecs::Entity e = top; !e.isNull() && reg.valid(e); e = scene.parent(e)) {
            if (interactiveKind(reg, e)) {
                top = e;
                break;
            }
            if (reg.has<UICanvasComponent>(e)) break;
        }
        const bool topInteractive = !top.isNull() && interactiveKind(reg, top);
        auto pressedIt = m_pressed.find(p.id);
        if (pressedIt != m_pressed.end() && !reg.valid(pressedIt->second)) {
            m_pressed.erase(pressedIt);
            pressedIt = m_pressed.end();
        }
        PointerState& ps = m_pointerState[p.id];
        if (p.down && !wasDown) {
            m_navVisible = false;  // the pointer takes over from keyboard navigation
            ps = {p.position, p.position, hit.isNull() ? ecs::Entity{} : scrollViewOf(scene, hit), false};
            if (!m_focusedField.isNull() && top != m_focusedField) focusField({});
            if (!m_focusedField.isNull() && top == m_focusedField && !m_caretStops.empty()) {
                m_caret = m_anchor = caretAt(p.position);
                m_selectingText = true;
                m_caretClock.restart();
            }
            if (topInteractive && interactableNow(reg, top)) {
                m_pressed[p.id] = top;
                if (reg.has<UISliderComponent>(top)) setSliderValue(scene, top, p.position);
                if (reg.has<UIColorPickerComponent>(top))
                    if (const Element* el = elementOf(top)) {
                        m_pickerZone[p.id] = pickerZone(*el, localPoint(*el, p.position));
                        setPickerValue(scene, top, p.position, m_pickerZone[p.id]);
                    }
            }
        } else if (p.down) {
            const bool picker = pressedIt != m_pressed.end() && reg.has<UIColorPickerComponent>(pressedIt->second);
            const bool slider = pressedIt != m_pressed.end() && (reg.has<UISliderComponent>(pressedIt->second) || picker);
            const auto* reorderList = pressedIt != m_pressed.end() ? reg.tryGet<UIItemListComponent>(pressedIt->second) : nullptr;
            const bool reordering = reorderList && reorderList->reorderable;  // drags move items (wheel scrolls)
            if (!ps.dragging && !ps.scrollView.isNull() && !slider && !reordering && math::length(p.position - ps.down) > 12.0f) {
                ps.dragging = true;  // a drag scrolls the view and cancels the press inside it
                if (pressedIt != m_pressed.end()) {
                    m_pressed.erase(pressedIt);
                    pressedIt = m_pressed.end();
                }
            }
            if (m_selectingText && pressedIt != m_pressed.end() && pressedIt->second == m_focusedField && !m_caretStops.empty())
                m_caret = caretAt(p.position);
            if (ps.dragging && reg.valid(ps.scrollView)) scrollBy(scene, ps.scrollView, ps.last - p.position);
            else if (picker) setPickerValue(scene, pressedIt->second, p.position, m_pickerZone[p.id]);
            else if (slider) setSliderValue(scene, pressedIt->second, p.position);
            ps.last = p.position;
        } else if (!p.down && wasDown) {
            m_selectingText = false;
            // A rich text link released where it was pressed.
            auto inBox = [](const Vec4& r, Vec2 q) { return q.x >= r.x && q.y >= r.y && q.x <= r.x + r.z && q.y <= r.y + r.w; };
            for (const LinkBox& lb : m_linkBoxes)
                if (inBox(lb.rect, p.position) && inBox(lb.rect, ps.down) && reg.valid(lb.entity)) {
                    m_events.push_back({UIEventType::Link, lb.entity, 0.0f, lb.id});
                    break;
                }
            if (pressedIt != m_pressed.end()) {
                const ecs::Entity pressed = pressedIt->second;
                if (pressed == top && !ps.dragging && interactableNow(reg, pressed) && !reg.has<UISliderComponent>(pressed)) {
                    if (const auto* tabs = reg.tryGet<UITabsComponent>(pressed)) {  // the tab under the pointer
                        const Element* el = elementOf(pressed);
                        const usize n = splitList(tabs->tabs).size();
                        const float scale = el ? canvasScale(canvasOf(pressed)) : 1.0f;
                        if (el && n > 0 && p.position.y < el->rect.y + tabs->headerHeight * scale)
                            selectOption(scene, pressed, std::clamp(static_cast<i32>((p.position.x - el->rect.x) / std::max(1.0f, el->rect.z) * static_cast<float>(n)), 0,
                                                                    static_cast<i32>(n) - 1));
                    } else if (auto* tl = reg.tryGet<UIItemListComponent>(pressed); tl && tl->reorderable && tl->tree &&
                                                                                    listRowAt(pressed, ps.down) >= 0 && listRowAt(pressed, p.position) >= 0 &&
                                                                                    listRowAt(pressed, ps.down) != listRowAt(pressed, p.position)) {
                        // Tree: before / inside / after the row under the pointer (quarter, half, quarter).
                        const Element* el = elementOf(pressed);
                        const float sc = el ? std::max(el->scale, 0.01f) : 1.0f, rowH = std::max(tl->itemHeight * sc, 1.0f);
                        const Vec2 lp = el ? localPoint(*el, p.position) : p.position;
                        const float within = el ? std::fmod(lp.y - el->rect.y + tl->scroll * sc, rowH) / rowH : 0.5f;
                        const i32 section = within < 0.25f ? -1 : within > 0.75f ? 1 : 0;
                        i32 to = 0;
                        if (moveTreeItem(*tl, listRowAt(pressed, ps.down), listRowAt(pressed, p.position), section, &to))
                            m_events.push_back({UIEventType::ValueChanged, pressed, static_cast<float>(to), {}});
                    } else if (auto* list = reg.tryGet<UIItemListComponent>(pressed); list && list->reorderable && !list->tree &&
                                                                                 listRowAt(pressed, ps.down) >= 0 && listRowAt(pressed, p.position) >= 0 &&
                                                                                 listRowAt(pressed, ps.down) != listRowAt(pressed, p.position)) {
                        // Dropped on another row: the item moves there.
                        const i32 from = listRowAt(pressed, ps.down), to = listRowAt(pressed, p.position);
                        std::vector<std::string> items = splitList(list->items);
                        if (from < static_cast<i32>(items.size()) && to < static_cast<i32>(items.size())) {
                            std::string moved = items[static_cast<usize>(from)];
                            items.erase(items.begin() + from);
                            items.insert(items.begin() + to, std::move(moved));
                            std::string joined;
                            for (usize k = 0; k < items.size(); ++k) joined += (k ? ";" : "") + items[k];
                            list->items = joined;
                            list->selected = to;
                            m_events.push_back({UIEventType::ValueChanged, pressed, static_cast<float>(to), {}});
                        }
                    } else if (reg.has<UIItemListComponent>(pressed)) {
                        clickList(scene, pressed, p.position);
                    } else if (!reg.has<UIColorPickerComponent>(pressed)) {
                        activate(scene, pressed);
                    }
                }
                m_pressed.erase(pressedIt);
            }
            ps.dragging = false;
        }
        if (m_pressed.count(p.id) || ps.dragging) m_pointerOverUI = true;
        if (!p.down && topInteractive) m_hovered.insert(top.packed());
        m_pointerDown[p.id] = p.down;
    }
    if (tooltipHit != m_tooltipEntity) {
        m_tooltipEntity = tooltipHit;
        m_tooltipClock.restart();
    }
    if (const auto* tip = tooltipHit.isNull() ? nullptr : reg.tryGet<UITooltipComponent>(tooltipHit))
        m_tooltipVisible = m_tooltipClock.elapsedSeconds() >= tip->delay && !tip->text.empty();
    else
        m_tooltipVisible = false;
    // Pointers that disappeared (finger lost, window left): released without a click.
    for (auto it = m_pointerDown.begin(); it != m_pointerDown.end();) {
        if (!seen.count(it->first)) {
            m_pressed.erase(it->first);
            m_pointerState.erase(it->first);
            it = m_pointerDown.erase(it);
        } else {
            ++it;
        }
    }
}

void UISystem::activate(Scene& scene, ecs::Entity e) {
    auto& reg = scene.registry();
    if (reg.has<UIButtonComponent>(e)) m_events.push_back({UIEventType::Click, e, 0.0f, {}});
    else if (reg.has<UIToggleComponent>(e)) clickToggle(scene, e);
    else if (reg.has<UIInputFieldComponent>(e)) focusField(e);
    else if (const auto* d = reg.tryGet<UIDropdownComponent>(e)) {
        if (m_navVisible) {  // keyboard / gamepad: the next option
            const i32 n = static_cast<i32>(splitList(d->options).size());
            if (n > 0) selectOption(scene, e, (d->selected + 1) % n);
        } else {
            openDropdown(m_openDropdown == e ? ecs::Entity{} : e);
            layoutPopup();
        }
    } else if (const auto* t = reg.tryGet<UITabsComponent>(e)) {  // keyboard / gamepad: the next tab
        const i32 n = static_cast<i32>(splitList(t->tabs).size());
        if (n > 0) selectOption(scene, e, (t->current + 1) % n);
    }
}

void UISystem::selectOption(Scene& scene, ecs::Entity e, i32 index) {
    auto& reg = scene.registry();
    if (auto* d = reg.tryGet<UIDropdownComponent>(e)) {
        const i32 n = static_cast<i32>(splitList(d->options).size());
        if (n == 0) return;
        index = std::clamp(index, 0, n - 1);
        if (d->selected != index) {
            d->selected = index;
            m_events.push_back({UIEventType::ValueChanged, e, static_cast<float>(index), {}});
        }
    } else if (auto* t = reg.tryGet<UITabsComponent>(e)) {
        const i32 n = static_cast<i32>(splitList(t->tabs).size());
        if (n == 0) return;
        index = std::clamp(index, 0, n - 1);
        if (t->current != index) {
            t->current = index;
            m_events.push_back({UIEventType::ValueChanged, e, static_cast<float>(index), {}});
        }
    }
}

void UISystem::layoutPopup() {
    m_popupCount = 0;
    if (m_openDropdown.isNull() || !m_layoutScene || !m_layoutScene->valid(m_openDropdown)) return;
    const auto* d = m_layoutScene->registry().tryGet<UIDropdownComponent>(m_openDropdown);
    const Element* el = elementOf(m_openDropdown);
    if (!d || !el) {
        m_openDropdown = {};
        return;
    }
    m_popupCount = static_cast<i32>(splitList(d->options).size());
    m_popupItemHeight = std::max(8.0f, el->rect.w);
    m_popupScale = 1.0f;
    m_popupTheme = nullptr;
    for (const CanvasPass& pass : m_passes)
        for (const Element& e : pass.elements)
            if (e.entity == m_openDropdown) {
                m_popupScale = pass.scale;
                m_popupTheme = pass.theme;
            }
    const float h = m_popupItemHeight * static_cast<float>(m_popupCount);
    float y = el->rect.y + el->rect.w;
    float screenH = 0.0f;
    for (const CanvasPass& pass : m_passes)
        if (!pass.elements.empty()) screenH = std::max(screenH, pass.elements.front().rect.y + pass.elements.front().rect.w);
    if (y + h > screenH && el->rect.y - h >= 0.0f) y = el->rect.y - h;  // no room below: above
    m_popupRect = {el->rect.x, y, el->rect.z, h};
}

bool UISystem::popupPointer(Scene& scene, const UIPointer& p, bool wasDown) {
    if (m_openDropdown.isNull() || m_popupCount <= 0) return false;
    const bool inside = contains(m_popupRect, p.position);
    const i32 item = inside ? std::clamp(static_cast<i32>((p.position.y - m_popupRect.y) / m_popupItemHeight), 0, m_popupCount - 1) : -1;
    if (inside) m_pointerOverUI = true;
    if (p.down && !wasDown) {
        m_popupPress[p.id] = item;  // outside: the press closes the list and goes nowhere else
        if (!inside) {
            m_openDropdown = {};
            m_pointerOverUI = true;
        }
        return true;
    }
    auto it = m_popupPress.find(p.id);
    if (!p.down && wasDown && it != m_popupPress.end()) {
        const i32 pressed = it->second;
        m_popupPress.erase(it);
        if (pressed >= 0 && pressed == item) {
            const ecs::Entity dropdown = m_openDropdown;
            m_openDropdown = {};
            selectOption(scene, dropdown, item);
        }
        return true;
    }
    if (!p.down) m_popupHover = item;
    return inside || it != m_popupPress.end();
}

void UISystem::clickToggle(Scene& scene, ecs::Entity e) {
    auto& reg = scene.registry();
    auto& t = reg.get<UIToggleComponent>(e);
    if (!t.group.empty()) {
        if (t.isOn) return;  // a radio button is switched off by choosing another one
        reg.view<UIToggleComponent>().each([&](ecs::Entity o, UIToggleComponent& other) {
            if (o != e && other.group == t.group && other.isOn) {
                other.isOn = false;
                m_events.push_back({UIEventType::ValueChanged, o, 0.0f, {}});
            }
        });
    }
    auto& tt = reg.get<UIToggleComponent>(e);
    tt.isOn = !tt.isOn;
    m_events.push_back({UIEventType::ValueChanged, e, tt.isOn ? 1.0f : 0.0f, {}});
}

void UISystem::focusField(ecs::Entity field) {
    if (field == m_focusedField) return;
    const bool had = !m_focusedField.isNull();
    m_focusedField = field;
    m_caret = m_anchor = static_cast<usize>(-1);  // the end of the text (clamped on use)
    m_caretStops.clear();
    m_selectingText = false;
    m_undo.clear(), m_redo.clear();
    m_fieldScroll = 0.0f;
    m_typingGroup = false;
    m_caretClock.restart();
    if (m_textInputHandler && had != !field.isNull()) m_textInputHandler(!field.isNull());
}

void UISystem::recordUndo(const std::string& before, usize caret, usize anchor, bool typing) {
    if (m_suppressUndo) return;
    m_redo.clear();
    if (!(typing && m_typingGroup && !m_undo.empty())) {
        m_undo.push_back({before, caret, anchor});
        if (m_undo.size() > 100) m_undo.erase(m_undo.begin());
    }
    m_typingGroup = typing;
}

void UISystem::processText(Scene& scene, std::string_view typed, u32 backspaces, bool enter) {
    if (m_focusedField.isNull()) return;
    auto& reg = scene.registry();
    if (!reg.valid(m_focusedField)) {
        focusField({});
        return;
    }
    auto* f = reg.tryGet<UIInputFieldComponent>(m_focusedField);
    if (!f || !f->interactable) return;
    const std::string before = f->text;
    usize len = utf8Length(f->text);
    m_caret = std::min(m_caret, len), m_anchor = std::min(m_anchor, len);
    const usize caretBefore = m_caret, anchorBefore = m_anchor;
    auto eraseRange = [&](usize from, usize to) {  // code points [from, to)
        if (to <= from) return;
        const usize a = utf8Offset(f->text, from), b = utf8Offset(f->text, to);
        f->text.erase(a, b - a);
        len -= to - from;
        m_caret = m_anchor = from;
    };
    const bool hadSelection = m_caret != m_anchor;
    if (hadSelection && (backspaces > 0 || !typed.empty() || (enter && f->multiline))) {
        eraseRange(std::min(m_caret, m_anchor), std::max(m_caret, m_anchor));
        if (backspaces > 0) --backspaces;  // the first backspace removes the selection
    }
    for (u32 i = 0; i < backspaces && m_caret > 0; ++i) eraseRange(m_caret - 1, m_caret);
    auto insert = [&](const std::string& ch) {
        f->text.insert(utf8Offset(f->text, m_caret), ch);
        ++len;
        m_anchor = ++m_caret;
    };
    for (usize i = 0; i < typed.size();) {
        const unsigned char c = static_cast<unsigned char>(typed[i]);
        const usize n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        const std::string ch(typed.substr(i, n));
        i += n;
        if (c < 0x20 || c == 0x7F) continue;  // control characters
        if (f->numeric && !(n == 1 && (std::isdigit(c) || c == '-' || c == '.'))) continue;
        if (f->maxLength > 0 && len >= f->maxLength) break;
        insert(ch);
    }
    if (enter && f->multiline && (f->maxLength == 0 || len < f->maxLength)) {  // a new line, no submit
        insert("\n");
        enter = false;
    }
    if (f->text != before) {
        // Letters typed in a row are one undo step; a space, a deletion or a caret jump ends it.
        const bool typing = backspaces == 0 && !hadSelection && !typed.empty() && typed.find(' ') == std::string_view::npos;
        recordUndo(before, caretBefore, anchorBefore, typing);
        m_events.push_back({UIEventType::TextChanged, m_focusedField, 0.0f, f->text});
    }
    if (enter) {
        m_events.push_back({UIEventType::Submit, m_focusedField, 0.0f, f->text});
        focusField({});
    }
    m_caretClock.restart();
}

void UISystem::processTextKey(Scene& scene, TextKey key, bool shift, bool word) {
    if (m_focusedField.isNull() || !scene.registry().valid(m_focusedField)) return;
    auto* f = scene.registry().tryGet<UIInputFieldComponent>(m_focusedField);
    if (!f) return;
    const usize len = utf8Length(f->text);
    m_caret = std::min(m_caret, len), m_anchor = std::min(m_anchor, len);
    const usize selA = std::min(m_caret, m_anchor), selB = std::max(m_caret, m_anchor);
    auto isSpace = [&](usize i) {
        const std::string c = utf8At(f->text, i);
        return c == " " || c == "\n" || c == "\t";
    };
    auto move = [&](usize to) {
        m_caret = std::min(to, len);
        if (!shift) m_anchor = m_caret;
    };
    auto lineStart = [&](usize i) {
        while (i > 0 && utf8At(f->text, i - 1) != "\n") --i;
        return i;
    };
    auto lineEnd = [&](usize i) {
        while (i < len && utf8At(f->text, i) != "\n") ++i;
        return i;
    };
    const std::string before = f->text;
    const usize caretBefore = m_caret, anchorBefore = m_anchor;
    switch (key) {
        case TextKey::Left:
            if (!shift && selA != selB) move(selA);
            else if (word) {
                usize i = m_caret;
                while (i > 0 && isSpace(i - 1)) --i;
                while (i > 0 && !isSpace(i - 1)) --i;
                move(i);
            } else {
                move(m_caret > 0 ? m_caret - 1 : 0);
            }
            break;
        case TextKey::Right:
            if (!shift && selA != selB) move(selB);
            else if (word) {
                usize i = m_caret;
                while (i < len && !isSpace(i)) ++i;
                while (i < len && isSpace(i)) ++i;
                move(i);
            } else {
                move(m_caret + 1);
            }
            break;
        case TextKey::Home: move(f->multiline && !word ? lineStart(m_caret) : 0); break;
        case TextKey::End: move(f->multiline && !word ? lineEnd(m_caret) : len); break;
        case TextKey::Up:
        case TextKey::Down: {
            if (!f->multiline) {
                move(key == TextKey::Up ? 0 : len);
                break;
            }
            // Same x on the previous / next drawn line (wrapped lines included).
            if (m_caretStops.size() == len + 1) {
                const CaretStop cur = m_caretStops[m_caret];
                const float targetY = key == TextKey::Up ? cur.y - cur.h * 0.5f : cur.y + cur.h * 1.5f;
                if (targetY < m_caretStops.front().y) move(0);
                else if (targetY > m_caretStops.back().y + m_caretStops.back().h) move(len);
                else move(caretAt({cur.x, targetY}));
            } else {  // not drawn yet: by text lines
                const usize start = lineStart(m_caret), col = m_caret - start;
                if (key == TextKey::Up) {
                    if (start == 0) move(0);
                    else {
                        const usize prev = lineStart(start - 1);
                        move(std::min(prev + col, start - 1));
                    }
                } else {
                    const usize end = lineEnd(m_caret);
                    if (end >= len) move(len);
                    else move(std::min(end + 1 + col, lineEnd(end + 1)));
                }
            }
            break;
        }
        case TextKey::Delete:
            if (selA != selB) {
                f->text.erase(utf8Offset(f->text, selA), utf8Offset(f->text, selB) - utf8Offset(f->text, selA));
                m_caret = m_anchor = selA;
            } else if (m_caret < len) {
                f->text.erase(utf8Offset(f->text, m_caret), utf8Offset(f->text, m_caret + 1) - utf8Offset(f->text, m_caret));
                m_anchor = m_caret;
            }
            break;
        case TextKey::SelectAll:
            m_anchor = 0;
            m_caret = len;
            break;
        case TextKey::Copy:
        case TextKey::Cut:
            if (selA != selB && !f->password) {
                const usize a = utf8Offset(f->text, selA), b = utf8Offset(f->text, selB);
                const std::string part = f->text.substr(a, b - a);
                if (m_clipboardSet) m_clipboardSet(part);
                m_clipboard = part;
                if (key == TextKey::Cut) {
                    f->text.erase(a, b - a);
                    m_caret = m_anchor = selA;
                }
            }
            break;
        case TextKey::Paste: {
            std::string clip = m_clipboardGet ? m_clipboardGet() : std::string{};
            if (clip.empty()) clip = m_clipboard;  // no system clipboard (headless)
            if (!f->multiline) std::replace(clip.begin(), clip.end(), '\n', ' ');
            std::string typed;
            for (char c : clip)  // processText drops control characters: keep new lines for multiline fields
                if (c != '\r') typed += c;
            const std::string pasteBefore = f->text;
            m_suppressUndo = true;
            if (f->multiline) {
                usize start = 0;
                for (usize i = 0; i <= typed.size(); ++i)
                    if (i == typed.size() || typed[i] == '\n') {
                        processText(scene, std::string_view(typed).substr(start, i - start), 0, i < typed.size());
                        start = i + 1;
                    }
            } else {
                processText(scene, typed, 0, false);
            }
            m_suppressUndo = false;
            if (f->text != pasteBefore) recordUndo(pasteBefore, caretBefore, anchorBefore, false);
            return;  // processText reported the change
        }
        case TextKey::Undo:
        case TextKey::Redo: {
            auto& from = key == TextKey::Undo ? m_undo : m_redo;
            auto& to = key == TextKey::Undo ? m_redo : m_undo;
            if (from.empty()) break;
            to.push_back({f->text, m_caret, m_anchor});
            TextSnapshot s = std::move(from.back());
            from.pop_back();
            f->text = std::move(s.text);
            const usize n = utf8Length(f->text);
            m_caret = std::min(s.caret, n), m_anchor = std::min(s.anchor, n);
            m_typingGroup = false;
            if (f->text != before) m_events.push_back({UIEventType::TextChanged, m_focusedField, 0.0f, f->text});
            m_caretClock.restart();
            return;
        }
    }
    if (f->text != before) {
        recordUndo(before, caretBefore, anchorBefore, false);
        m_events.push_back({UIEventType::TextChanged, m_focusedField, 0.0f, f->text});
    } else if (m_caret != caretBefore) {
        m_typingGroup = false;  // moving the caret starts a new undo step
    }
    m_caretClock.restart();
}

usize UISystem::caretAt(Vec2 p) const {
    if (m_caretStops.empty()) return static_cast<usize>(-1);
    // The line under the point (nearest line), then the nearest stop on it.
    float bestLineDist = 1e30f, lineY = m_caretStops.front().y;
    for (const CaretStop& s : m_caretStops) {
        const float d = p.y < s.y ? s.y - p.y : p.y > s.y + s.h ? p.y - s.y - s.h : 0.0f;
        if (d < bestLineDist - 1e-3f) bestLineDist = d, lineY = s.y;
    }
    usize best = 0;
    float bestDx = 1e30f;
    for (usize i = 0; i < m_caretStops.size(); ++i) {
        if (std::fabs(m_caretStops[i].y - lineY) > 0.5f) continue;
        const float dx = std::fabs(m_caretStops[i].x - p.x);
        if (dx < bestDx) bestDx = dx, best = i;
    }
    return best;
}

void UISystem::computeCaretStops(const PendingText& pt, const std::string& text) {
    const usize count = utf8Length(text);
    std::vector<bool> newline(count, false);
    for (usize i = 0, cp = 0; i < text.size(); ++i)
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
            newline[cp] = text[i] == '\n';
            ++cp;
        }
    const TextLayout& l = pt.layout;
    const float lineH = l.lines ? l.size.y / static_cast<float>(l.lines) : pt.pixelSize;
    std::vector<const TextGlyph*> bySource(count, nullptr);
    for (const TextGlyph& g : l.glyphs)
        if (g.source < count && !bySource[g.source]) bySource[g.source] = &g;
    m_caretStops.assign(count + 1, {pt.origin.x, pt.origin.y, lineH});
    float penX = 0.0f;
    i32 penLine = 0;
    for (usize i = 0; i < count; ++i) {
        if (const TextGlyph* g = bySource[i]) {
            const i32 line = static_cast<i32>(std::floor(g->baseline / std::max(lineH, 1.0f)));
            if (line != penLine) penX = g->min.x, penLine = line;
            m_caretStops[i] = {pt.origin.x + penX, pt.origin.y + static_cast<float>(penLine) * lineH, lineH};
            penX += g->advance;
        } else {
            m_caretStops[i] = {pt.origin.x + penX, pt.origin.y + static_cast<float>(penLine) * lineH, lineH};
            if (newline[i]) penX = 0.0f, ++penLine;
        }
    }
    m_caretStops[count] = {pt.origin.x + penX, pt.origin.y + static_cast<float>(penLine) * lineH, lineH};
}

std::vector<UISystem::ListRow> UISystem::listRows(const UIItemListComponent& list) {
    std::vector<ListRow> all;
    const auto items = splitList(list.items);
    std::vector<i32> folded;
    if (list.tree) {
        std::string cur;
        for (char c : list.collapsed + ",") {
            if (c == ',') {
                if (!cur.empty()) folded.push_back(std::atoi(cur.c_str()));
                cur.clear();
            } else if (c != ' ') {
                cur += c;
            }
        }
    }
    for (usize i = 0; i < items.size(); ++i) {
        ListRow r;
        r.index = static_cast<i32>(i);
        usize skip = 0;
        if (list.tree)
            while (skip < items[i].size() && items[i][skip] == '>') ++skip;
        r.depth = static_cast<i32>(skip);
        r.label = items[i].substr(skip);
        r.folded = std::find(folded.begin(), folded.end(), r.index) != folded.end();
        all.push_back(std::move(r));
    }
    for (usize i = 0; i + 1 < all.size(); ++i) all[i].hasChildren = all[i + 1].depth > all[i].depth;
    std::vector<ListRow> shown;
    i32 hideDeeperThan = 1 << 30;
    for (ListRow& r : all) {
        if (r.depth > hideDeeperThan) continue;
        hideDeeperThan = r.folded && r.hasChildren ? r.depth : 1 << 30;
        shown.push_back(std::move(r));
    }
    return shown;
}

bool UISystem::moveTreeItem(UIItemListComponent& list, i32 from, i32 target, i32 section, i32* newIndex) {
    std::vector<std::string> items = splitList(list.items);
    const i32 n = static_cast<i32>(items.size());
    if (from < 0 || from >= n || target < 0 || target >= n) return false;
    auto depth = [&](i32 i) {
        i32 d = 0;
        while (d < static_cast<i32>(items[static_cast<usize>(i)].size()) && items[static_cast<usize>(i)][static_cast<usize>(d)] == '>') ++d;
        return d;
    };
    auto subtreeEnd = [&](i32 i) {  // one past the last descendant
        i32 j = i + 1;
        while (j < n && depth(j) > depth(i)) ++j;
        return j;
    };
    const i32 end = subtreeEnd(from);
    if (target >= from && target < end) return false;  // into itself or its own branch
    const i32 newDepth = section == 0 ? depth(target) + 1 : depth(target);
    const i32 delta = newDepth - depth(from);
    // Old indices in their new order.
    std::vector<i32> order;
    for (i32 i = 0; i < n; ++i)
        if (i < from || i >= end) order.push_back(i);
    const i32 insertAfterOld = section < 0 ? -1 : section == 0 ? subtreeEnd(target) - 1 : subtreeEnd(target) - 1;
    usize pos = 0;
    if (section < 0) {
        pos = static_cast<usize>(std::find(order.begin(), order.end(), target) - order.begin());
    } else {
        pos = static_cast<usize>(std::find(order.begin(), order.end(), insertAfterOld) - order.begin()) + 1;
    }
    std::vector<i32> moved;
    for (i32 i = from; i < end; ++i) moved.push_back(i);
    order.insert(order.begin() + static_cast<std::ptrdiff_t>(pos), moved.begin(), moved.end());
    std::vector<i32> newOf(static_cast<usize>(n));
    std::string joined;
    for (usize k = 0; k < order.size(); ++k) {
        const i32 old = order[k];
        newOf[static_cast<usize>(old)] = static_cast<i32>(k);
        std::string label = items[static_cast<usize>(old)];
        if (old >= from && old < end) {
            const i32 d = depth(old);
            label = std::string(static_cast<usize>(std::max(0, d + delta)), '>') + label.substr(static_cast<usize>(d));
        }
        joined += (k ? ";" : "") + label;
    }
    list.items = joined;
    // Folded items keep their state under their new index.
    std::string folded, cur;
    for (char c : list.collapsed + ",") {
        if (c == ',') {
            if (!cur.empty()) {
                const i32 old = std::atoi(cur.c_str());
                if (old >= 0 && old < n) folded += (folded.empty() ? "" : ",") + std::to_string(newOf[static_cast<usize>(old)]);
            }
            cur.clear();
        } else if (c != ' ') {
            cur += c;
        }
    }
    list.collapsed = folded;
    list.selected = newOf[static_cast<usize>(from)];
    if (newIndex) *newIndex = list.selected;
    return true;
}

i32 UISystem::listRowAt(ecs::Entity e, Vec2 position) const {
    const Element* el = elementOf(e);
    if (!el || !m_layoutScene) return -1;
    const auto* list = m_layoutScene->registry().tryGet<UIItemListComponent>(e);
    if (!list) return -1;
    const Vec2 p = localPoint(*el, position);
    const float s = std::max(el->scale, 0.01f), rowH = list->itemHeight * s;
    const i32 row = static_cast<i32>(std::floor((p.y - el->rect.y + list->scroll * s) / std::max(rowH, 1.0f)));
    const auto rows = listRows(*list);
    return row >= 0 && row < static_cast<i32>(rows.size()) ? rows[static_cast<usize>(row)].index : -1;
}

std::vector<i32> UISystem::listSelection(const UIItemListComponent& list) {
    std::vector<i32> out;
    if (!list.multiSelect) {
        if (list.selected >= 0) out.push_back(list.selected);
        return out;
    }
    std::string cur;
    for (char c : list.selection + ",") {
        if (c == ',') {
            if (!cur.empty()) out.push_back(std::atoi(cur.c_str()));
            cur.clear();
        } else if (c >= '0' && c <= '9') {
            cur += c;
        }
    }
    return out;
}

void UISystem::clickList(Scene& scene, ecs::Entity e, Vec2 position) {
    auto* list = scene.registry().tryGet<UIItemListComponent>(e);
    const Element* el = elementOf(e);
    if (!list || !el || !list->interactable) return;
    const Vec2 p = localPoint(*el, position);
    const float s = std::max(el->scale, 0.01f), rowH = list->itemHeight * s;
    const auto rows = listRows(*list);
    const i32 row = static_cast<i32>(std::floor((p.y - el->rect.y + list->scroll * s) / std::max(rowH, 1.0f)));
    if (row < 0 || row >= static_cast<i32>(rows.size())) return;
    const ListRow& r = rows[static_cast<usize>(row)];
    const float indent = 28.0f * s;
    if (list->tree && r.hasChildren && p.x < el->rect.x + indent * static_cast<float>(r.depth + 1)) {  // the arrow: fold / unfold
        std::string cur, out;
        bool removed = false;
        for (char c : list->collapsed + ",") {
            if (c == ',') {
                if (!cur.empty()) {
                    if (std::atoi(cur.c_str()) == r.index) removed = true;
                    else out += (out.empty() ? "" : ",") + cur;
                }
                cur.clear();
            } else if (c != ' ') {
                cur += c;
            }
        }
        if (!removed) out += (out.empty() ? "" : ",") + std::to_string(r.index);
        list->collapsed = out;
        return;
    }
    const f64 now = m_uiClock.elapsedSeconds();
    if (list->multiSelect) {
        std::vector<i32> sel = listSelection(*list);
        if (m_shift && list->selected >= 0) {  // range between the anchor and this row (in display order)
            i32 a = -1, b = -1;
            for (usize k = 0; k < rows.size(); ++k) {
                if (rows[k].index == list->selected) a = static_cast<i32>(k);
                if (rows[k].index == r.index) b = static_cast<i32>(k);
            }
            if (a < 0) a = b;
            if (!m_ctrl) sel.clear();
            for (i32 k = std::min(a, b); k <= std::max(a, b); ++k)
                if (std::find(sel.begin(), sel.end(), rows[static_cast<usize>(k)].index) == sel.end()) sel.push_back(rows[static_cast<usize>(k)].index);
        } else if (m_ctrl) {
            if (auto it = std::find(sel.begin(), sel.end(), r.index); it != sel.end()) sel.erase(it);
            else sel.push_back(r.index);
            list->selected = r.index;
        } else {
            sel = {r.index};
            list->selected = r.index;
        }
        std::sort(sel.begin(), sel.end());
        std::string text;
        for (i32 v : sel) text += (text.empty() ? "" : ",") + std::to_string(v);
        if (text != list->selection) {
            list->selection = text;
            m_events.push_back({UIEventType::ValueChanged, e, static_cast<float>(r.index), {}});
        }
        m_lastListClick = {e, r.index, now};
        return;
    }
    if (list->selected != r.index) {
        list->selected = r.index;
        m_events.push_back({UIEventType::ValueChanged, e, static_cast<float>(r.index), {}});
    }
    if (m_lastListClick.entity == e && m_lastListClick.row == r.index && now - m_lastListClick.time < 0.4) {
        m_events.push_back({UIEventType::Submit, e, static_cast<float>(r.index), r.label});
        m_lastListClick = {};
    } else {
        m_lastListClick = {e, r.index, now};
    }
}

i32 UISystem::pickerZone(const Element& el, Vec2 local) const {
    const Vec4 r = el.rect;
    const float sq = std::min(r.w, r.z * 0.72f);
    if (local.x < r.x + sq) return 0;
    const float bar = sq * 0.12f, gap = sq * 0.06f;
    if (local.x < r.x + sq + gap + bar + gap * 0.5f) return 1;
    return 2;
}

void UISystem::setPickerValue(Scene& scene, ecs::Entity e, Vec2 position, i32 zone) {
    auto* c = scene.registry().tryGet<UIColorPickerComponent>(e);
    const Element* el = elementOf(e);
    if (!c || !el || !c->interactable) return;
    const Vec2 p = localPoint(*el, position);
    const Vec4 r = el->rect;
    const float sq = std::max(1.0f, std::min(r.w, r.z * 0.72f));
    if (c->hsvOf.x != c->color.x || c->hsvOf.y != c->color.y || c->hsvOf.z != c->color.z || c->hsvOf.w != c->color.w)
        c->hsv = rgbToHsv(c->color, c->hsv.x);
    const float ty = std::clamp((p.y - r.y) / sq, 0.0f, 1.0f);
    float alpha = c->color.w;
    if (zone == 0) {
        c->hsv.y = std::clamp((p.x - r.x) / sq, 0.0f, 1.0f);
        c->hsv.z = 1.0f - ty;
    } else if (zone == 1) {
        c->hsv.x = std::min(ty, 0.9999f);
    } else if (c->alpha) {
        alpha = 1.0f - ty;
    }
    const Vec4 next = hsvToRgb(c->hsv.x, c->hsv.y, c->hsv.z, alpha);
    if (next.x != c->color.x || next.y != c->color.y || next.z != c->color.z || next.w != c->color.w) {
        c->color = next;
        m_events.push_back({UIEventType::ValueChanged, e, next.w, {}});
    }
    c->hsvOf = c->color;
}

void UISystem::layoutDialog(Scene& scene, u32 width, u32 height) {
    m_dialog = {};
    m_dialogButtons.clear();
    auto& reg = scene.registry();
    reg.view<UIDialogComponent>().each([&](ecs::Entity e, UIDialogComponent& d) {
        if (!d.open || !scene.isActiveInHierarchy(e)) return;
        if (m_dialog.isNull() || e.index > m_dialog.index) m_dialog = e;  // the last created is in front
    });
    if (m_dialog.isNull()) return;
    const auto& d = reg.get<UIDialogComponent>(m_dialog);
    m_dialogScale = 1.0f;
    m_dialogTheme = nullptr;
    m_dialogScreen = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    for (ecs::Entity p = m_dialog; !p.isNull(); p = scene.parent(p))
        if (reg.has<UICanvasComponent>(p)) {
            for (const CanvasPass& pass : m_passes)
                if (pass.canvas == p) m_dialogScale = pass.scale, m_dialogTheme = pass.theme;
            break;
        }
    const float s = m_dialogScale;
    const float w = std::min(d.size.x * s, m_dialogScreen.z * 0.95f), h = std::min(d.size.y * s, m_dialogScreen.w * 0.95f);
    m_dialogRect = {(m_dialogScreen.z - w) * 0.5f, (m_dialogScreen.w - h) * 0.5f, w, h};
    const auto buttons = splitList(d.buttons);
    const usize n = std::max<usize>(buttons.size(), 1);
    const float pad = 24.0f * s, bh = std::min(64.0f * s, h * 0.3f);
    const float bw = std::min(220.0f * s, (w - pad * static_cast<float>(n + 1)) / static_cast<float>(n));
    float x = m_dialogRect.x + w - pad - bw * static_cast<float>(buttons.size()) - pad * static_cast<float>(buttons.size() - (buttons.empty() ? 0 : 1));
    for (usize i = 0; i < buttons.size(); ++i) {
        m_dialogButtons.push_back({x, m_dialogRect.y + h - pad - bh, bw, bh});
        x += bw + pad;
    }
}

bool UISystem::dialogPointer(Scene& scene, const UIPointer& p, bool wasDown) {
    if (m_dialog.isNull() || !scene.valid(m_dialog)) return false;
    m_pointerOverUI = true;
    i32 button = -1;
    for (usize i = 0; i < m_dialogButtons.size(); ++i)
        if (contains(m_dialogButtons[i], p.position)) button = static_cast<i32>(i);
    if (!p.down) m_dialogHover = button;
    if (p.down && !wasDown) m_dialogPress[p.id] = button;
    auto it = m_dialogPress.find(p.id);
    if (!p.down && wasDown && it != m_dialogPress.end()) {
        const i32 pressed = it->second;
        m_dialogPress.erase(it);
        if (pressed >= 0 && pressed == button) {
            auto* d = scene.registry().tryGet<UIDialogComponent>(m_dialog);
            if (d) {
                d->open = false;
                d->result = pressed;
                m_events.push_back({UIEventType::ValueChanged, m_dialog, static_cast<float>(pressed), {}});
            }
            const ecs::Entity closed = m_dialog;
            m_dialog = {};
            layoutDialog(scene, static_cast<u32>(m_dialogScreen.z), static_cast<u32>(m_dialogScreen.w));  // another dialog under it?
            (void)closed;
        }
    }
    return true;
}

ecs::Entity UISystem::scrollViewOf(Scene& scene, ecs::Entity e) const {
    for (ecs::Entity p = e; !p.isNull() && scene.valid(p); p = scene.parent(p)) {
        if (scene.registry().has<UIScrollViewComponent>(p) || scene.registry().has<UIItemListComponent>(p)) return p;
        if (scene.registry().has<UICanvasComponent>(p)) break;
    }
    return {};
}

void UISystem::scrollBy(Scene& scene, ecs::Entity view, Vec2 pixels) {
    if (auto* list = scene.registry().tryGet<UIItemListComponent>(view)) {
        if (const Element* el = elementOf(view)) {
            const float s = std::max(el->scale, 0.01f);
            const float content = static_cast<float>(listRows(*list).size()) * list->itemHeight * s;
            list->scroll = std::clamp(list->scroll + pixels.y / s, 0.0f, std::max(0.0f, (content - el->rect.w) / s));
        }
        return;
    }
    auto* sv = scene.registry().tryGet<UIScrollViewComponent>(view);
    auto it = m_scroll.find(view.packed());
    if (!sv || it == m_scroll.end()) return;
    const ScrollInfo& info = it->second;
    const float s = std::max(0.01f, info.scale);
    if (sv->horizontal) sv->scroll.x = std::clamp(sv->scroll.x + pixels.x / s, 0.0f, std::max(0.0f, (info.content.x - info.view.z) / s));
    if (sv->vertical) sv->scroll.y = std::clamp(sv->scroll.y + pixels.y / s, 0.0f, std::max(0.0f, (info.content.y - info.view.w) / s));
}

bool UISystem::scrollAt(Scene& scene, Vec2 position, Vec2 wheel) {
    if (wheel.x == 0.0f && wheel.y == 0.0f) return false;
    const ecs::Entity hit = hitTest(position);
    const ecs::Entity view = hit.isNull() ? ecs::Entity{} : scrollViewOf(scene, hit);
    if (view.isNull()) return false;
    if (const auto* list = scene.registry().tryGet<UIItemListComponent>(view)) {
        const Element* el = elementOf(view);
        scrollBy(scene, view, {0.0f, -wheel.y * list->itemHeight * (el ? el->scale : 1.0f)});
        return true;
    }
    const auto& sv = scene.registry().get<UIScrollViewComponent>(view);
    const float s = m_scroll.count(view.packed()) ? m_scroll[view.packed()].scale : 1.0f;
    // Wheel up (positive) shows what is above: the scroll offset decreases.
    scrollBy(scene, view, {-wheel.x * sv.wheelSpeed * s, -wheel.y * sv.wheelSpeed * s});
    return true;
}

const UISystem::Element* UISystem::elementOf(ecs::Entity e) const {
    for (const CanvasPass& pass : m_passes)
        for (const Element& el : pass.elements)
            if (el.entity == e) return &el;
    return nullptr;
}

bool UISystem::navigationEnabled() const {
    if (!m_layoutScene) return false;
    for (const CanvasPass& pass : m_passes)
        if (const auto* c = m_layoutScene->registry().tryGet<UICanvasComponent>(pass.canvas); c && c->keyboardNavigation) return true;
    return false;
}

void UISystem::navigate(Scene& scene, Vec2 dir) {
    auto& reg = scene.registry();
    struct Cand {
        ecs::Entity e;
        Vec2 center;
        i32 order;
    };
    std::vector<Cand> cands;
    for (const CanvasPass& pass : m_passes) {
        const auto* c = reg.tryGet<UICanvasComponent>(pass.canvas);
        if (!c || !c->keyboardNavigation) continue;
        for (const Element& el : pass.elements) {
            if (!el.interactive || !interactableNow(reg, el.entity) || el.rect.z <= 0.0f) continue;
            const Vec2 center{el.rect.x + el.rect.z * 0.5f, el.rect.y + el.rect.w * 0.5f};
            if (!contains(el.clip, center) && scrollViewOf(scene, el.entity).isNull()) continue;
            cands.push_back({el.entity, center, pass.sortOrder});
        }
    }
    if (cands.empty()) return;
    i32 topOrder = cands.front().order;
    for (const Cand& c : cands) topOrder = std::max(topOrder, c.order);
    const Cand* current = nullptr;
    for (const Cand& c : cands)
        if (c.e == m_navFocus) current = &c;
    if (!current || !m_navVisible) {  // first press: the top-left control of the topmost canvas
        const Cand* best = nullptr;
        for (const Cand& c : cands)
            if (c.order == topOrder && (!best || c.center.y < best->center.y - 1.0f || (std::abs(c.center.y - best->center.y) <= 1.0f && c.center.x < best->center.x)))
                best = &c;
        if (best) m_navFocus = best->e;  // always found: cands is not empty
        m_navVisible = true;
        return;
    }
    const Cand* best = nullptr;
    float bestScore = 1e30f;
    for (const Cand& c : cands) {
        if (c.e == current->e || c.order != current->order) continue;
        const Vec2 v = c.center - current->center;
        const float along = v.x * dir.x + v.y * dir.y;
        if (along <= 1.0f) continue;
        const float across = std::abs(v.x * dir.y - v.y * dir.x);
        const float score = along + across * 2.0f;
        if (score < bestScore) {
            bestScore = score;
            best = &c;
        }
    }
    if (best) m_navFocus = best->e;
    m_navVisible = true;
    // Keep it visible inside its scroll view.
    if (const ecs::Entity view = scrollViewOf(scene, m_navFocus); !view.isNull() && m_scroll.count(view.packed())) {
        const Element* el = elementOf(m_navFocus);
        const Vec4 v = m_scroll[view.packed()].view;
        if (el) {
            if (el->rect.y < v.y) scrollBy(scene, view, {0.0f, el->rect.y - v.y});
            else if (el->rect.y + el->rect.w > v.y + v.w) scrollBy(scene, view, {0.0f, el->rect.y + el->rect.w - v.y - v.w});
        }
    }
}

void UISystem::navActivate(Scene& scene) {
    if (!m_navVisible || m_navFocus.isNull() || !scene.valid(m_navFocus) || !interactableNow(scene.registry(), m_navFocus)) return;
    activate(scene, m_navFocus);
}

void UISystem::navAdjust(Scene& scene, float steps) {
    if (!m_navVisible || m_navFocus.isNull() || !scene.valid(m_navFocus)) return;
    auto* s = scene.registry().tryGet<UISliderComponent>(m_navFocus);
    if (!s || !s->interactable) return;
    const float step = s->wholeNumbers ? 1.0f : (s->maxValue - s->minValue) * 0.1f;
    const float v = std::clamp(s->value + steps * step, std::min(s->minValue, s->maxValue), std::max(s->minValue, s->maxValue));
    if (v != s->value) {
        s->value = v;
        m_events.push_back({UIEventType::ValueChanged, m_navFocus, v, {}});
    }
}

std::vector<UIEvent> UISystem::takeEvents() {
    std::vector<UIEvent> out;
    out.swap(m_events);
    return out;
}

void UISystem::reset() {
    focusField({});
    m_openDropdown = {};
    m_popupPress.clear();
    m_dialogPress.clear();
    m_pickerZone.clear();
    m_lastListClick = {};
    m_tooltipEntity = {};
    m_tooltipVisible = false;
    m_navFocus = {};
    m_navVisible = false;
    m_pointerState.clear();
    m_pressed.clear();
    m_hovered.clear();
    m_pointerDown.clear();
    m_events.clear();
    m_pointerOverUI = false;
}

// ----------------------------------------------------------------- drawing
/// Quad clipped to m_clip (scroll views): texture coordinates follow the cut.
void UISystem::emit(OverlayDrawList& out, Vec2 mn, Vec2 mx, Vec4 uv, u32 color, rhi::TextureHandle texture) const {
    const Vec4 c = m_clip;
    const float x0 = std::max(mn.x, c.x), y0 = std::max(mn.y, c.y), x1 = std::min(mx.x, c.x + c.z), y1 = std::min(mx.y, c.y + c.w);
    if (x1 <= x0 || y1 <= y0) return;
    const float w = std::max(1e-6f, mx.x - mn.x), h = std::max(1e-6f, mx.y - mn.y);
    const float u0 = uv.x + (uv.z - uv.x) * (x0 - mn.x) / w, u1 = uv.x + (uv.z - uv.x) * (x1 - mn.x) / w;
    const float v0 = uv.y + (uv.w - uv.y) * (y0 - mn.y) / h, v1 = uv.y + (uv.w - uv.y) * (y1 - mn.y) / h;
    if (!m_xform.identity()) {  // rotated element: clipped in its own frame, then turned
        const Vec2 p[4] = {m_xform.apply({x0, y0}), m_xform.apply({x1, y0}), m_xform.apply({x1, y1}), m_xform.apply({x0, y1})};
        const Vec2 t[4] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
        const u32 cs[4] = {color, color, color, color};
        out.quad4(p, t, cs, texture);
        return;
    }
    out.quad({x0, y0}, {x1, y1}, {u0, v0, u1, v1}, color, texture);
}

void UISystem::emitGradient(OverlayDrawList& out, Vec2 mn, Vec2 mx, const Vec4 colors[4]) const {
    // Corners: top-left, top-right, bottom-right, bottom-left (not clipped: small controls).
    const Vec2 p[4] = {m_xform.apply(mn), m_xform.apply({mx.x, mn.y}), m_xform.apply(mx), m_xform.apply({mn.x, mx.y})};
    const Vec2 t[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const u32 cs[4] = {packColor(colors[0]), packColor(colors[1]), packColor(colors[2]), packColor(colors[3])};
    out.quad4(p, t, cs, {});
}
void UISystem::drawImage(const UIImageComponent& img, Vec4 rect, Vec4 tint, float scale, OverlayDrawList& out) {
    const Vec4 color = mul(img.color, tint);
    if (color.w <= 0.0f || rect.z <= 0.0f || rect.w <= 0.0f) return;
    rhi::TextureHandle tex;
    float texW = 0.0f, texH = 0.0f;
    if (!img.texture.empty() && m_renderer) {
        const TextureInfo* info = nullptr;
        tex = m_renderer->resolveTexture(img.texture, img.pixelArt, info);
        if (info && info->state == AssetState::Loading) return;  // no white flash while loading
        if (info) {
            texW = static_cast<float>(std::max(1u, info->sourceWidth ? info->sourceWidth : info->width));
            texH = static_cast<float>(std::max(1u, info->sourceHeight ? info->sourceHeight : info->height));
        }
    }
    const u32 packed = packColor(color);
    const bool anyCorner = img.cornerRadii.x > 0.0f || img.cornerRadii.y > 0.0f || img.cornerRadii.z > 0.0f || img.cornerRadii.w > 0.0f;
    if (img.texture.empty() && (img.cornerRadius > 0.0f || anyCorner || img.outlineWidth > 0.0f || img.shadowSize > 0.0f)) {
        // Flat style box: shadow, then outline (the whole box), then the fill inset by the outline.
        ensureShapeTextures();
        const float base = std::max(img.cornerRadius, 0.0f);
        const Vec4 radii{(img.cornerRadii.x < 0.0f ? base : img.cornerRadii.x) * scale, (img.cornerRadii.y < 0.0f ? base : img.cornerRadii.y) * scale,
                         (img.cornerRadii.z < 0.0f ? base : img.cornerRadii.z) * scale, (img.cornerRadii.w < 0.0f ? base : img.cornerRadii.w) * scale};
        auto grow = [](Vec4 r, float d) { return Vec4{std::max(r.x + d, 0.0f), std::max(r.y + d, 0.0f), std::max(r.z + d, 0.0f), std::max(r.w + d, 0.0f)}; };
        if (img.shadowSize > 0.0f) {
            const Vec4 sc = mul(img.shadowColor, Vec4{1, 1, 1, tint.w});
            const float s = img.shadowSize * scale;
            if (sc.w > 0.0f)
                roundBox(out, {rect.x + img.shadowOffset.x * scale - s, rect.y + img.shadowOffset.y * scale - s, rect.z + 2.0f * s, rect.w + 2.0f * s},
                         grow(radii, 2.0f * s), packColor(sc), true);
        }
        Vec4 inner = rect;
        Vec4 innerRadii = radii;
        if (img.outlineWidth > 0.0f) {
            const Vec4 oc = mul(img.outlineColor, Vec4{1, 1, 1, tint.w});
            if (oc.w > 0.0f) roundBox(out, rect, radii, packColor(oc), false);
            const float w = std::min(img.outlineWidth * scale, std::min(rect.z, rect.w) * 0.5f);
            inner = {rect.x + w, rect.y + w, rect.z - 2.0f * w, rect.w - 2.0f * w};
            innerRadii = grow(radii, -w);
        }
        roundBox(out, inner, innerRadii, packed, false);
        return;
    }
    const bool sliced = texW > 0.0f && (img.border.x > 0 || img.border.y > 0 || img.border.z > 0 || img.border.w > 0);
    if (!sliced) {
        Vec4 r = rect;
        if (img.preserveAspect && texW > 0.0f && texH > 0.0f) {
            const float k = std::min(rect.z / texW, rect.w / texH);
            const float w = texW * k, h = texH * k;
            r = {rect.x + (rect.z - w) * 0.5f, rect.y + (rect.w - h) * 0.5f, w, h};
        }
        emit(out, {r.x, r.y}, {r.x + r.z, r.y + r.w}, {0, 0, 1, 1}, packed, tex);
        return;
    }
    // 9-slice: corners keep their size, edges stretch in one direction, the center in both.
    const float k = img.borderScale * scale;
    float l = img.border.x * k, t = img.border.y * k, r = img.border.z * k, b = img.border.w * k;
    const float fx = (l + r) > rect.z ? rect.z / (l + r) : 1.0f, fy = (t + b) > rect.w ? rect.w / (t + b) : 1.0f;
    l *= fx, r *= fx, t *= fy, b *= fy;
    const float xs[4] = {rect.x, rect.x + l, rect.x + rect.z - r, rect.x + rect.z};
    const float ys[4] = {rect.y, rect.y + t, rect.y + rect.w - b, rect.y + rect.w};
    const float us[4] = {0.0f, img.border.x / texW, 1.0f - img.border.z / texW, 1.0f};
    const float vs[4] = {0.0f, img.border.y / texH, 1.0f - img.border.w / texH, 1.0f};
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) {
            if (xs[x + 1] <= xs[x] || ys[y + 1] <= ys[y]) continue;
            emit(out, {xs[x], ys[y]}, {xs[x + 1], ys[y + 1]}, {us[x], vs[y], us[x + 1], vs[y + 1]}, packed, tex);
        }
}

void UISystem::drawElement(Scene& scene, const Element& el, float scale, const UITheme* theme, OverlayDrawList& out) {
    auto& reg = scene.registry();
    const UITheme* th = theme;
    auto quad = [&](float x0, float y0, float x1, float y1, Vec4 c) { emit(out, {x0, y0}, {x1, y1}, {0, 0, 1, 1}, packColor(c), {}); };
    Vec4 tint{1, 1, 1, 1};
    if (const auto* b = reg.tryGet<UIButtonComponent>(el.entity)) {
        if (!b->interactable) tint = th ? th->buttonDisabled : b->disabledColor;
        else if (isPressed(el.entity)) tint = th ? th->buttonPressed : b->pressedColor;
        else if (isHovered(el.entity)) tint = th ? th->buttonHover : b->hoverColor;
        else tint = th ? th->button : b->normalColor;
    }
    if (const auto* img = reg.tryGet<UIImageComponent>(el.entity)) drawImage(*img, el.rect, tint, scale, out);
    if (const auto* s = reg.tryGet<UISliderComponent>(el.entity)) {
        const Vec4 r = el.rect;
        const float range = s->maxValue - s->minValue;
        const float t = range != 0.0f ? std::clamp((s->value - s->minValue) / range, 0.0f, 1.0f) : 0.0f;
        const Vec4 dim = s->interactable ? Vec4{1, 1, 1, 1} : Vec4{1, 1, 1, 0.5f};
        const Vec4 track = mul(th ? th->track : s->trackColor, dim), fill = mul(th ? th->accent : s->fillColor, dim),
                   knob = mul(th ? th->handle : s->handleColor, dim);
        if (s->vertical) {  // bottom = min
            const float handle = r.z * 0.5f;
            const float hy = r.y + r.w - handle - t * std::max(0.0f, r.w - 2.0f * handle);
            const float trackW = std::max(2.0f, r.z * 0.3f), tx = r.x + (r.z - trackW) * 0.5f;
            quad(tx, r.y, tx + trackW, r.y + r.w, track);
            quad(tx, hy, tx + trackW, r.y + r.w, fill);
            quad(r.x, hy - handle, r.x + r.z, hy + handle, knob);
        } else {
            const float handle = r.w * 0.5f;
            const float hx = r.x + handle + t * std::max(0.0f, r.z - 2.0f * handle);
            const float trackH = std::max(2.0f, r.w * 0.3f), ty = r.y + (r.w - trackH) * 0.5f;
            quad(r.x, ty, r.x + r.z, ty + trackH, track);
            quad(r.x, ty, hx, ty + trackH, fill);
            quad(hx - handle, r.y, hx + handle, r.y + r.w, knob);
        }
    }
    if (const auto* t = reg.tryGet<UIToggleComponent>(el.entity)) {  // box at the left, mark when on
        const Vec4 r = el.rect;
        const float box = std::min(r.w, r.z) * 0.8f, bx = r.x + (r.w - box) * 0.5f, by = r.y + (r.w - box) * 0.5f;
        const Vec4 dim = t->interactable ? (isHovered(el.entity) ? Vec4{1.2f, 1.2f, 1.2f, 1} : Vec4{1, 1, 1, 1}) : Vec4{1, 1, 1, 0.5f};
        quad(bx, by, bx + box, by + box, mul(th ? th->track : t->boxColor, dim));
        if (t->isOn) {
            const float in = box * 0.22f;
            quad(bx + in, by + in, bx + box - in, by + box - in, mul(th ? th->accent : t->checkColor, dim));
        }
    }
    if (const auto* p = reg.tryGet<UIProgressBarComponent>(el.entity)) {
        const Vec4 r = el.rect;
        quad(r.x, r.y, r.x + r.z, r.y + r.w, th ? th->track : p->backgroundColor);
        const float range = p->maxValue - p->minValue;
        const float t = range != 0.0f ? std::clamp((p->value - p->minValue) / range, 0.0f, 1.0f) : 0.0f;
        const float in = std::min(p->inset * scale, std::min(r.z, r.w) * 0.5f);
        const float x0 = r.x + in, y0 = r.y + in, x1 = r.x + r.z - in, y1 = r.y + r.w - in;
        const Vec4 fill = th ? th->accent : p->fillColor;
        if (t > 0.0f) switch (p->direction) {
            case UIFillDirection::LeftToRight: quad(x0, y0, x0 + (x1 - x0) * t, y1, fill); break;
            case UIFillDirection::RightToLeft: quad(x1 - (x1 - x0) * t, y0, x1, y1, fill); break;
            case UIFillDirection::BottomToTop: quad(x0, y1 - (y1 - y0) * t, x1, y1, fill); break;
            case UIFillDirection::TopToBottom: quad(x0, y0, x1, y0 + (y1 - y0) * t, fill); break;
        }
    }
    if (const auto* d = reg.tryGet<UIDropdownComponent>(el.entity)) {  // box and a small arrow at the right
        const Vec4 r = el.rect;
        const Vec4 dim = d->interactable ? (isHovered(el.entity) || m_openDropdown == el.entity ? Vec4{1.15f, 1.15f, 1.15f, 1} : Vec4{1, 1, 1, 1})
                                         : Vec4{1, 1, 1, 0.5f};
        quad(r.x, r.y, r.x + r.z, r.y + r.w, mul(th ? th->field : d->boxColor, dim));
        const float a = r.w * 0.16f, cx = r.x + r.z - r.w * 0.5f, cy = r.y + r.w * 0.5f;
        const Vec4 arrow = th ? th->text : d->textColor;
        for (int i = 0; i < 4; ++i) {  // a stepped triangle pointing down
            const float w = a * (1.0f - static_cast<float>(i) / 4.0f), y = cy - a * 0.5f + static_cast<float>(i) * a / 4.0f;
            quad(cx - w, y, cx + w, y + a / 4.0f, arrow);
        }
    }
    if (const auto* tb = reg.tryGet<UITabsComponent>(el.entity)) {  // header: background, the current tab, a line under it
        const Vec4 r = el.rect;
        const float h = std::min(tb->headerHeight * scale, r.w);
        quad(r.x, r.y, r.x + r.z, r.y + h, th ? th->panel : tb->headerColor);
        const usize n = splitList(tb->tabs).size();
        if (n > 0) {
            const float tw = r.z / static_cast<float>(n);
            const float x = r.x + tw * static_cast<float>(std::clamp(tb->current, 0, static_cast<i32>(n) - 1));
            Vec4 active = th ? th->accent : tb->activeColor;
            Vec4 soft = active;
            soft.w *= 0.25f;
            quad(x, r.y, x + tw, r.y + h, soft);
            const float line = std::max(2.0f, 4.0f * scale);
            quad(x, r.y + h - line, x + tw, r.y + h, active);
        }
    }
    if (const auto* list = reg.tryGet<UIItemListComponent>(el.entity)) {  // background, selection, tree arrows
        const Vec4 r = el.rect;
        quad(r.x, r.y, r.x + r.z, r.y + r.w, th ? th->panel : list->backgroundColor);
        m_clip = intersect(el.clip, r);  // rows and their texts stay inside
        const float rowH = list->itemHeight * scale, indent = 28.0f * scale;
        const auto rows = listRows(*list);
        const Vec4 accent = th ? th->accent : list->selectedColor;
        const Vec4 arrow = th ? th->text : list->textColor;
        const std::vector<i32> selection = listSelection(*list);
        for (usize i = 0; i < rows.size(); ++i) {
            const float y = r.y + static_cast<float>(i) * rowH - list->scroll * scale;
            if (y + rowH < r.y || y > r.y + r.w) continue;
            if (std::find(selection.begin(), selection.end(), rows[i].index) != selection.end()) quad(r.x, y, r.x + r.z, y + rowH, {accent.x, accent.y, accent.z, accent.w * 0.45f});
            if (list->tree && rows[i].hasChildren) {  // stepped triangle: right when folded, down when open
                const float a = std::min(rowH * 0.18f, indent * 0.35f), cx = r.x + indent * (static_cast<float>(rows[i].depth) + 0.5f), cy = y + rowH * 0.5f;
                for (int k = 0; k < 4; ++k) {
                    const float t = static_cast<float>(k) / 4.0f, w = a * (1.0f - t);
                    if (rows[i].folded) quad(cx - a * 0.5f + t * a, cy - w, cx - a * 0.5f + (t + 0.25f) * a, cy + w, arrow);
                    else quad(cx - w, cy - a * 0.5f + t * a, cx + w, cy - a * 0.5f + (t + 0.25f) * a, arrow);
                }
            }
        }
    }
    if (auto* cp = reg.tryGet<UIColorPickerComponent>(el.entity)) {  // square, hue bar, alpha bar, markers
        if (cp->hsvOf.x != cp->color.x || cp->hsvOf.y != cp->color.y || cp->hsvOf.z != cp->color.z || cp->hsvOf.w != cp->color.w) {
            cp->hsv = rgbToHsv(cp->color, cp->hsv.x);
            cp->hsvOf = cp->color;
        }
        const Vec4 r = el.rect;
        const float sq = std::min(r.w, r.z * 0.72f), bar = sq * 0.12f, gap = sq * 0.06f;
        const Vec4 hue = hsvToRgb(cp->hsv.x, 1, 1, 1);
        const Vec4 sat[4] = {{1, 1, 1, 1}, hue, hue, {1, 1, 1, 1}};
        emitGradient(out, {r.x, r.y}, {r.x + sq, r.y + sq}, sat);
        const Vec4 dark[4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 0, 1}};
        emitGradient(out, {r.x, r.y}, {r.x + sq, r.y + sq}, dark);
        const float hx = r.x + sq + gap;
        for (int k = 0; k < 6; ++k) {
            const float y0 = r.y + sq * static_cast<float>(k) / 6.0f, y1 = r.y + sq * static_cast<float>(k + 1) / 6.0f;
            const Vec4 a = hsvToRgb(static_cast<float>(k) / 6.0f, 1, 1, 1), b = hsvToRgb(static_cast<float>(k + 1) / 6.0f, 1, 1, 1);
            const Vec4 g[4] = {a, a, b, b};
            emitGradient(out, {hx, y0}, {hx + bar, y1}, g);
        }
        const float mark = std::max(2.0f, 3.0f * scale);
        const Vec4 white{1, 1, 1, 1}, black{0, 0, 0, 1};
        const float hy = r.y + cp->hsv.x * sq;
        quad(hx - mark, hy - mark * 0.5f, hx + bar + mark, hy + mark * 0.5f, white);
        if (cp->alpha) {
            const float ax = hx + bar + gap;
            quad(ax, r.y, ax + bar, r.y + sq, {0.5f, 0.5f, 0.5f, 1});
            for (int k = 0; k < 8; ++k)  // checkerboard under the alpha
                quad(ax + (k % 2) * bar * 0.5f, r.y + sq * static_cast<float>(k) / 8.0f, ax + (k % 2 + 1) * bar * 0.5f, r.y + sq * static_cast<float>(k + 1) / 8.0f,
                     {0.8f, 0.8f, 0.8f, 1});
            const Vec4 opaque{cp->color.x, cp->color.y, cp->color.z, 1}, clear{cp->color.x, cp->color.y, cp->color.z, 0};
            const Vec4 g[4] = {opaque, opaque, clear, clear};
            emitGradient(out, {ax, r.y}, {ax + bar, r.y + sq}, g);
            const float ay = r.y + (1.0f - cp->color.w) * sq;
            quad(ax - mark, ay - mark * 0.5f, ax + bar + mark, ay + mark * 0.5f, white);
        }
        const float mx = r.x + cp->hsv.y * sq, my = r.y + (1.0f - cp->hsv.z) * sq, m = std::max(4.0f, 6.0f * scale);
        quad(mx - m, my - mark * 0.5f, mx + m, my + mark * 0.5f, cp->hsv.z > 0.5f ? black : white);
        quad(mx - mark * 0.5f, my - m, mx + mark * 0.5f, my + m, cp->hsv.z > 0.5f ? black : white);
    }
    if (const auto* sv = reg.tryGet<UIScrollViewComponent>(el.entity); sv && sv->showScrollbar) {
        auto it = m_scroll.find(el.entity.packed());
        if (it != m_scroll.end() && sv->vertical && it->second.content.y > el.rect.w + 1.0f) {
            const Vec4 r = el.rect;
            const float total = it->second.content.y, barH = std::max(20.0f, r.w * r.w / total);
            const float maxScroll = (total - r.w) / std::max(0.01f, it->second.scale);
            const float t = maxScroll > 0.0f ? sv->scroll.y / maxScroll : 0.0f;
            const float bw = std::max(3.0f, 5.0f * scale), y = r.y + t * (r.w - barH);
            emit(out, {r.x + r.z - bw - 2.0f, y}, {r.x + r.z - 2.0f, y + barH}, {0, 0, 1, 1}, packColor(sv->scrollbarColor), {});
        }
    }
}

void UISystem::buildOverlay(Scene& scene, u32 width, u32 height, OverlayDrawList& out) {
    layout(scene, width, height);
    Stopwatch sw;
    if (m_renderer) m_fonts.setDevice(&m_renderer->device());
    auto& reg = scene.registry();

    // Pass 1: lay out the texts (rasterizes new glyphs); retried once with an empty atlas if it filled up.
    for (int attempt = 0; attempt < 2; ++attempt) {
        m_texts.clear();
        m_overlayTexts.clear();
        for (usize ci = 0; ci < m_passes.size(); ++ci) {
            const CanvasPass& pass = m_passes[ci];
            const UITheme* th = pass.theme;
            auto fontOf = [&](const std::string& f) -> const std::string& { return th && !th->font.empty() ? th->font : f; };
            for (usize ei = 0; ei < pass.elements.size(); ++ei) {
                const Element& el = pass.elements[ei];
                if (const auto* t = reg.tryGet<UITextComponent>(el.entity); t && !t->text.empty() && t->color.w > 0.0f) {
                    PendingText pt{ei, ci, {}, {}, t->color, {}, t->fontSize * pass.scale};
                    if (th && isWhite(t->color)) pt.color = Vec4{th->text.x, th->text.y, th->text.z, th->text.w * t->color.w};
                    const std::string shown = t->richText ? parseRichText(t->text, pt.styles, pt.links) : std::string();
                    auto lay = m_fonts.layout(fontOf(t->font), t->richText ? shown : t->text, pt.pixelSize, el.rect.z, t->wrap, t->lineSpacing, t->hAlign);
                    if (!lay) {
                        if (m_reportedFonts.insert(t->font).second) AGR_LOG_ERROR("UI", "font '{}': {}", t->font, lay.error().message);
                    } else {
                        Vec2 origin{el.rect.x, el.rect.y};
                        if (t->vAlign == UIVAlign::Middle) origin.y += (el.rect.w - lay.value().size.y) * 0.5f;
                        else if (t->vAlign == UIVAlign::Bottom) origin.y += el.rect.w - lay.value().size.y;
                        pt.origin = {std::round(origin.x), std::round(origin.y)};
                        pt.layout = std::move(lay.value());
                        m_texts.push_back(std::move(pt));
                    }
                }
                if (const auto* d = reg.tryGet<UIDropdownComponent>(el.entity)) {  // the selected option
                    const auto options = splitList(d->options);
                    if (!options.empty()) {
                        const float pad = 12.0f * pass.scale, px = d->fontSize * pass.scale;
                        const std::string& label = options[static_cast<usize>(std::clamp(d->selected, 0, static_cast<i32>(options.size()) - 1))];
                        if (auto lay = textLayout(fontOf(d->font), label, px, std::max(1.0f, el.rect.z - pad - el.rect.w), false, UIHAlign::Left)) {
                            const Vec2 o{std::round(el.rect.x + pad), std::round(el.rect.y + (el.rect.w - lay->size.y) * 0.5f)};
                            const Vec4 c = th ? th->text : d->textColor;
                            m_texts.push_back({ei, ci, std::move(*lay), o, d->interactable ? c : mul(c, {1, 1, 1, 0.5f}), {}, px});
                        }
                    }
                    if (el.entity == m_openDropdown && m_popupCount > 0) {  // the list, drawn over everything
                        for (usize i = 0; i < options.size(); ++i) {
                            const float y = m_popupRect.y + m_popupItemHeight * static_cast<float>(i);
                            if (auto lay = textLayout(fontOf(d->font), options[i], d->fontSize * pass.scale, m_popupRect.z, false, UIHAlign::Left))
                                m_overlayTexts.push_back({std::move(*lay), {std::round(m_popupRect.x + 12.0f * pass.scale), std::round(y + (m_popupItemHeight - lay->size.y) * 0.5f)},
                                                          th ? th->text : d->textColor});
                        }
                    }
                }
                if (const auto* list = reg.tryGet<UIItemListComponent>(el.entity)) {  // visible rows
                    const float rowH = list->itemHeight * el.scale, indent = 28.0f * el.scale, px = list->fontSize * el.scale;
                    const auto rows = listRows(*list);
                    for (usize i = 0; i < rows.size(); ++i) {
                        const float y = el.rect.y + static_cast<float>(i) * rowH - list->scroll * el.scale;
                        if (y + rowH < el.rect.y || y > el.rect.y + el.rect.w) continue;
                        const float x = el.rect.x + 12.0f * el.scale + (list->tree ? indent * static_cast<float>(rows[i].depth + 1) : 0.0f);
                        if (auto lay = textLayout(fontOf(list->font), rows[i].label, px, std::max(1.0f, el.rect.x + el.rect.z - x), false, UIHAlign::Left))
                            m_texts.push_back({ei, ci, std::move(*lay), {std::round(x), std::round(y + (rowH - lay->size.y) * 0.5f)}, th ? th->text : list->textColor, {}, px});
                    }
                }
                if (const auto* tb = reg.tryGet<UITabsComponent>(el.entity)) {  // tab names, centered in their tab
                    const auto names = splitList(tb->tabs);
                    const float h = std::min(tb->headerHeight * pass.scale, el.rect.w), tw = names.empty() ? 0.0f : el.rect.z / static_cast<float>(names.size());
                    for (usize i = 0; i < names.size(); ++i)
                        if (auto lay = textLayout(fontOf(tb->font), names[i], tb->fontSize * pass.scale, tw, false, UIHAlign::Center)) {
                            const Vec2 o{std::round(el.rect.x + tw * static_cast<float>(i)), std::round(el.rect.y + (h - lay->size.y) * 0.5f)};
                            m_texts.push_back({ei, ci, std::move(*lay), o, th ? th->text : tb->textColor, {}, tb->fontSize * pass.scale});
                        }
                }
            }
        }
        m_dialogTexts.clear();
        if (!m_dialog.isNull() && reg.valid(m_dialog))
            if (const auto* d = reg.tryGet<UIDialogComponent>(m_dialog)) {
                const UITheme* th = m_dialogTheme;
                const float s = m_dialogScale, pad = 24.0f * s;
                const std::string& font = th && !th->font.empty() ? th->font : d->font;
                const Vec4 tc = th ? th->text : d->textColor;
                auto title = textLayout(font, d->title, d->fontSize * 1.3f * s, m_dialogRect.z - pad * 2.0f, true, UIHAlign::Left);
                auto body = textLayout(font, d->text, d->fontSize * s, m_dialogRect.z - pad * 2.0f, true, UIHAlign::Left);
                // The panel grows to fit its texts above the buttons (kept centered on the screen).
                const float bh = m_dialogButtons.empty() ? 0.0f : m_dialogButtons[0].w;
                const float needed = pad + (title ? title->size.y + pad * 0.6f : 0.0f) + (body ? body->size.y : 0.0f) + pad + bh + pad;
                if (needed > m_dialogRect.w) {
                    const float grow = std::min(needed, m_dialogScreen.w * 0.95f) - m_dialogRect.w;
                    m_dialogRect.y -= grow * 0.5f;
                    m_dialogRect.w += grow;
                    for (Vec4& b : m_dialogButtons) b.y += grow * 0.5f;
                }
                const Vec4 r = m_dialogRect;
                float y = r.y + pad;
                if (title) {
                    const float h = title->size.y;
                    m_dialogTexts.push_back({std::move(*title), {std::round(r.x + pad), std::round(y)}, tc});
                    y += h + pad * 0.6f;
                }
                if (body) m_dialogTexts.push_back({std::move(*body), {std::round(r.x + pad), std::round(y)}, {tc.x, tc.y, tc.z, tc.w * 0.85f}});
                const auto labels = splitList(d->buttons);
                for (usize i = 0; i < labels.size() && i < m_dialogButtons.size(); ++i) {
                    const Vec4 b = m_dialogButtons[i];
                    if (auto lay = textLayout(font, labels[i], d->fontSize * s, b.z, false, UIHAlign::Center))
                        m_dialogTexts.push_back({std::move(*lay), {std::round(b.x), std::round(b.y + (b.w - lay->size.y) * 0.5f)}, {0.07f, 0.07f, 0.08f, 1.0f}});
                }
            }
        if (m_tooltipVisible && reg.valid(m_tooltipEntity)) {
            if (const auto* tip = reg.tryGet<UITooltipComponent>(m_tooltipEntity)) {
                float scale = 1.0f;
                const UITheme* th = nullptr;
                for (const CanvasPass& pass : m_passes)
                    if (pass.canvas == canvasOf(m_tooltipEntity)) scale = pass.scale, th = pass.theme;
                const std::string& font = th && !th->font.empty() ? th->font : std::string("assets://fonts/Inter-Regular.ttf");
                if (auto lay = textLayout(font, tip->text, 24.0f * scale, 420.0f * scale, true, UIHAlign::Left))
                    m_overlayTexts.push_back({std::move(*lay), {std::round(m_tooltipPos.x + 18.0f * scale), std::round(m_tooltipPos.y + 26.0f * scale)},
                                              th ? th->text : Vec4{1, 1, 1, 1}});
            }
        }
        if (!m_fonts.overflowed()) break;
        if (attempt == 0) {
            m_fonts.resetAtlas();
        } else {
            AGR_LOG_WARN("UI", "too many different characters / sizes for the glyph atlas: some text is missing");
            m_fonts.resetAtlas();
        }
    }
    m_fonts.flush();

    // Pass 2: geometry in painter's order (element, then its texts, then its children).
    m_linkBoxes.clear();
    auto drawText = [&](const PendingText& pt, ecs::Entity owner) {
        const u32 base = packColor(pt.color);
        const float boldOffset = std::max(1.0f, std::round(pt.pixelSize * 0.045f));
        const Vec4 linkColor{0.4f, 0.7f, 1.0f, 1.0f};
        for (const TextGlyph& g : pt.layout.glyphs) {
            const RichStyle* st = pt.styles.empty() || g.source >= pt.styles.size() ? nullptr : &pt.styles[g.source];
            if (st && st->image >= 0 && static_cast<usize>(st->image) < pt.links.size()) {  // inline image, one em, on the baseline
                if (m_renderer) {
                    const rhi::TextureHandle tex = m_renderer->resolveTexture(pt.links[static_cast<usize>(st->image)], false);
                    const float sz = pt.pixelSize * 0.9f, x = pt.origin.x + g.min.x + (g.advance - sz) * 0.5f, y = pt.origin.y + g.baseline - sz * 0.85f;
                    emit(out, {x, y}, {x + sz, y + sz}, {0, 0, 1, 1}, packColor({1, 1, 1, pt.color.w}), tex);
                }
                continue;
            }
            const bool link = st && st->link >= 0 && static_cast<usize>(st->link) < pt.links.size();
            const Vec4 c = st && st->hasColor ? st->color : link ? linkColor : Vec4{1, 1, 1, 1};
            const u32 color = st && (st->hasColor || link) ? packColor({c.x, c.y, c.z, c.w * pt.color.w}) : base;
            const Vec2 mn = pt.origin + g.min, mx = pt.origin + g.max;
            auto glyph = [&](Vec2 off) {
                const Vec4 clip = m_clip;
                const bool inside = mn.x + off.x >= clip.x && mx.x + off.x + (mx.y - mn.y) * 0.25f <= clip.x + clip.z && mn.y >= clip.y && mx.y <= clip.y + clip.w;
                if (st && st->italic && inside) {  // sheared quad: the top leans right
                    const float k = 0.2f;
                    const float baseY = pt.origin.y + g.baseline;
                    const Vec2 p[4] = {m_xform.apply({mn.x + off.x + (baseY - mn.y) * k, mn.y}), m_xform.apply({mx.x + off.x + (baseY - mn.y) * k, mn.y}),
                                       m_xform.apply({mx.x + off.x + (baseY - mx.y) * k, mx.y}), m_xform.apply({mn.x + off.x + (baseY - mx.y) * k, mx.y})};
                    const Vec2 t[4] = {{g.uv.x, g.uv.y}, {g.uv.z, g.uv.y}, {g.uv.z, g.uv.w}, {g.uv.x, g.uv.w}};
                    const u32 cs[4] = {color, color, color, color};
                    out.quad4(p, t, cs, m_fonts.texture());
                } else {
                    emit(out, mn + off, mx + off, g.uv, color, m_fonts.texture());
                }
            };
            glyph({0.0f, 0.0f});
            if (st && st->bold) glyph({boldOffset, 0.0f});
            const float t = std::max(1.0f, pt.pixelSize * 0.06f);
            if (st && (st->underline || link)) {
                const float y = pt.origin.y + g.baseline + std::max(1.0f, pt.pixelSize * 0.08f);
                emit(out, {pt.origin.x + g.min.x, y}, {pt.origin.x + g.min.x + g.advance, y + t}, {0, 0, 1, 1}, color, {});
            }
            if (st && st->strike) {
                const float y = pt.origin.y + g.baseline - pt.pixelSize * 0.28f;
                emit(out, {pt.origin.x + g.min.x, y}, {pt.origin.x + g.min.x + g.advance, y + t}, {0, 0, 1, 1}, color, {});
            }
            if (link) {  // clickable area: the glyph's cell on its line
                const Vec4 r{pt.origin.x + g.min.x, pt.origin.y + g.baseline - pt.pixelSize * 0.8f, std::max(g.advance, g.max.x - g.min.x), pt.pixelSize};
                const std::string& id = pt.links[static_cast<usize>(st->link)];
                if (!m_linkBoxes.empty() && m_linkBoxes.back().entity == owner && m_linkBoxes.back().id == id &&
                    std::fabs(m_linkBoxes.back().rect.y - r.y) < 0.5f) {
                    LinkBox& lb = m_linkBoxes.back();  // extend the box along the line
                    const float x1 = std::max(lb.rect.x + lb.rect.z, r.x + r.z);
                    lb.rect.x = std::min(lb.rect.x, r.x);
                    lb.rect.z = x1 - lb.rect.x;
                } else {
                    m_linkBoxes.push_back({owner, r, id});
                }
            }
        }
    };
    usize ti = 0;
    for (usize ci = 0; ci < m_passes.size(); ++ci) {
        const CanvasPass& pass = m_passes[ci];
        for (usize ei = 0; ei < pass.elements.size(); ++ei) {
            const Element& el = pass.elements[ei];
            m_clip = el.clip;
            m_xform = el.xform;
            drawElement(scene, el, el.scale, pass.theme, out);
            const auto* field = reg.tryGet<UIInputFieldComponent>(el.entity);
            const bool focusedField = field && el.entity == m_focusedField;
            bool stopsDrawn = false;
            const bool singleLine = field && !field->multiline;
            if (singleLine) m_clip = intersect(el.clip, el.rect);  // long text stays inside the field
            for (; ti < m_texts.size() && m_texts[ti].canvas == ci && m_texts[ti].element == ei; ++ti) {
                PendingText scrolled;
                const PendingText* ptp = &m_texts[ti];
                if (focusedField && singleLine && !field->text.empty()) {
                    // Scroll so that the caret stays inside the field (padding of 4 px).
                    computeCaretStops(*ptp, field->text);
                    const float pad = 4.0f * pass.scale, left = el.rect.x + pad, right = el.rect.x + el.rect.z - pad;
                    const float caretX = m_caretStops[std::min(m_caret, m_caretStops.size() - 1)].x - m_fieldScroll;
                    if (caretX > right) m_fieldScroll += caretX - right;
                    if (caretX < left) m_fieldScroll -= left - caretX;
                    m_fieldScroll = std::clamp(m_fieldScroll, 0.0f, std::max(0.0f, m_caretStops.back().x - right));
                    if (m_fieldScroll > 0.0f) {
                        scrolled = *ptp;
                        scrolled.origin.x -= m_fieldScroll;
                        ptp = &scrolled;
                    }
                }
                const PendingText& pt = *ptp;
                if (focusedField && !stopsDrawn && !field->text.empty()) {
                    // Caret stops for clicks and arrows, and the selection under the text.
                    computeCaretStops(pt, field->text);
                    stopsDrawn = true;
                    const usize n = m_caretStops.size() - 1;
                    const usize a = std::min(std::min(m_caret, m_anchor), n), b = std::min(std::max(m_caret, m_anchor), n);
                    const u32 sel = packColor({0.25f, 0.5f, 1.0f, 0.45f});
                    for (usize i = a; i < b; ++i) {
                        const CaretStop& s0 = m_caretStops[i];
                        const CaretStop& s1 = m_caretStops[i + 1];
                        const float x1 = std::fabs(s1.y - s0.y) < 0.5f && s1.x > s0.x ? s1.x : s0.x + std::max(4.0f, pt.pixelSize * 0.3f);
                        emit(out, {s0.x, s0.y}, {x1, s0.y + s0.h}, {0, 0, 1, 1}, sel, {});
                    }
                }
                drawText(pt, el.entity);
            }
            if (focusedField && !stopsDrawn) m_caretStops.clear();
            if (focusedField && std::fmod(m_caretClock.elapsedSeconds(), 1.0) < 0.55) {  // blinking caret
                float caretX = el.rect.x + 4.0f * pass.scale, h = el.rect.w * 0.6f, y = el.rect.y + (el.rect.w - h) * 0.5f;
                if (!m_caretStops.empty()) {
                    const CaretStop& s = m_caretStops[std::min(m_caret, m_caretStops.size() - 1)];
                    caretX = s.x;
                    if (field->multiline) h = s.h * 0.85f, y = s.y + s.h * 0.075f;
                }
                emit(out, {caretX, y}, {caretX + std::max(2.0f, 2.0f * pass.scale), y + h}, {0, 0, 1, 1}, packColor(field->textColor), {});
            }
            if (m_navVisible && el.entity == m_navFocus) {  // keyboard / gamepad highlight
                const Vec4 r = el.rect;
                const float t = std::max(2.0f, 3.0f * pass.scale);
                const u32 c = packColor({1.0f, 0.75f, 0.2f, 1.0f});
                emit(out, {r.x - t, r.y - t}, {r.x + r.z + t, r.y}, {0, 0, 1, 1}, c, {});
                emit(out, {r.x - t, r.y + r.w}, {r.x + r.z + t, r.y + r.w + t}, {0, 0, 1, 1}, c, {});
                emit(out, {r.x - t, r.y}, {r.x, r.y + r.w}, {0, 0, 1, 1}, c, {});
                emit(out, {r.x + r.z, r.y}, {r.x + r.z + t, r.y + r.w}, {0, 0, 1, 1}, c, {});
            }
        }
        m_clip = {-1e9f, -1e9f, 2e9f, 2e9f};
        m_xform = {};
    }
    // Over everything: the open drop-down list, then the tooltip.
    usize oi = 0;
    if (!m_openDropdown.isNull() && m_popupCount > 0 && reg.valid(m_openDropdown)) {
        if (const auto* d = reg.tryGet<UIDropdownComponent>(m_openDropdown)) {
            const UITheme* th = m_popupTheme;
            const Vec4 r = m_popupRect;
            emit(out, {r.x, r.y}, {r.x + r.z, r.y + r.w}, {0, 0, 1, 1}, packColor(th ? th->panel : d->listColor), {});
            const Vec4 hi = th ? th->accent : d->highlightColor;
            auto band = [&](i32 i, Vec4 c) {
                const float y = r.y + m_popupItemHeight * static_cast<float>(i);
                emit(out, {r.x, y}, {r.x + r.z, y + m_popupItemHeight}, {0, 0, 1, 1}, packColor(c), {});
            };
            if (d->selected >= 0 && d->selected < m_popupCount) band(d->selected, {hi.x, hi.y, hi.z, hi.w * 0.3f});
            if (m_popupHover >= 0 && m_popupHover < m_popupCount) band(m_popupHover, {hi.x, hi.y, hi.z, hi.w * 0.6f});
            for (; oi < m_overlayTexts.size() && oi < static_cast<usize>(m_popupCount); ++oi)
                drawText({0, 0, std::move(m_overlayTexts[oi].layout), m_overlayTexts[oi].origin, m_overlayTexts[oi].color, {}, 0.0f}, ecs::Entity{});
        }
    }
    for (; oi < m_overlayTexts.size(); ++oi) {  // tooltip: a box around its text
        OverlayText& ot = m_overlayTexts[oi];
        const float pad = 8.0f;
        Vec4 panel{0.08f, 0.09f, 0.11f, 0.95f};
        for (const CanvasPass& pass : m_passes)
            if (pass.canvas == canvasOf(m_tooltipEntity) && pass.theme) panel = pass.theme->panel;
        emit(out, {ot.origin.x - pad, ot.origin.y - pad}, {ot.origin.x + ot.layout.size.x + pad, ot.origin.y + ot.layout.size.y + pad}, {0, 0, 1, 1},
             packColor(panel), {});
        drawText({0, 0, std::move(ot.layout), ot.origin, ot.color, {}, 0.0f}, ecs::Entity{});
    }
    if (!m_dialog.isNull() && reg.valid(m_dialog))  // the modal dialog, over everything
        if (const auto* d = reg.tryGet<UIDialogComponent>(m_dialog)) {
            const UITheme* th = m_dialogTheme;
            const Vec4 sc = m_dialogScreen, r = m_dialogRect;
            emit(out, {sc.x, sc.y}, {sc.x + sc.z, sc.y + sc.w}, {0, 0, 1, 1}, packColor(d->dimColor), {});
            emit(out, {r.x, r.y}, {r.x + r.z, r.y + r.w}, {0, 0, 1, 1}, packColor(th ? th->panel : d->panelColor), {});
            const Vec4 bc = th ? th->accent : d->buttonColor;
            for (usize i = 0; i < m_dialogButtons.size(); ++i) {
                const Vec4 b = m_dialogButtons[i];
                const float k = static_cast<i32>(i) == m_dialogHover ? 1.15f : 1.0f;
                emit(out, {b.x, b.y}, {b.x + b.z, b.y + b.w}, {0, 0, 1, 1}, packColor({bc.x * k, bc.y * k, bc.z * k, bc.w}), {});
            }
            for (OverlayText& ot : m_dialogTexts) drawText({0, 0, std::move(ot.layout), ot.origin, ot.color, {}, 0.0f}, ecs::Entity{});
        }
    m_stats.quads = static_cast<u32>(out.indices.size() / 6);
    m_stats.texts = static_cast<u32>(m_texts.size());
    m_stats.buildMs = sw.elapsedMilliseconds();
}

} // namespace agr
