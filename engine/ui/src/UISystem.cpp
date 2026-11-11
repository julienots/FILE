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

Vec4 mul(Vec4 a, Vec4 b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }

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

UISystem::UISystem(VirtualFileSystem& vfs) : m_vfs(vfs), m_fonts(vfs) {}
UISystem::~UISystem() = default;

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
    const Vec4 screen{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    reg.view<UICanvasComponent>().each([&](ecs::Entity e, UICanvasComponent& c) {
        if (!scene.isActiveInHierarchy(e)) return;
        for (ecs::Entity p = scene.parent(e); !p.isNull(); p = scene.parent(p))
            if (reg.has<UICanvasComponent>(p)) return;  // nested canvas: laid out by its root
        CanvasPass pass;
        pass.canvas = e;
        pass.sortOrder = c.sortOrder;
        pass.hierarchyIndex = e.index;
        if (c.scaleMode == UIScaleMode::ConstantPixelSize) {
            pass.scale = std::max(0.01f, c.pixelScale);
        } else {
            const float rw = std::max(1.0f, c.referenceResolution.x), rh = std::max(1.0f, c.referenceResolution.y);
            const float lw = std::log2(std::max(1.0f, screen.z) / rw), lh = std::log2(std::max(1.0f, screen.w) / rh);
            pass.scale = std::exp2(math::lerp(lw, lh, std::clamp(c.match, 0.0f, 1.0f)));
        }
        Element root{e, screen};
        if (auto* r = reg.tryGet<UIRectComponent>(e)) r->screenRect = screen;
        const auto* img = reg.tryGet<UIImageComponent>(e);
        root.blocks = img && img->color.w > 0.0f;
        pass.elements.push_back(root);
        Vec4 content = screen;
        if (c.respectSafeArea) {
            const Vec4 in = m_safeArea;
            content = {in.x, in.y, std::max(0.0f, screen.z - in.x - in.z), std::max(0.0f, screen.w - in.y - in.w)};
        }
        layoutChildren(scene, e, content, pass.scale, pass);
        m_passes.push_back(std::move(pass));
    });
    std::stable_sort(m_passes.begin(), m_passes.end(), [](const CanvasPass& a, const CanvasPass& b) {
        return a.sortOrder != b.sortOrder ? a.sortOrder < b.sortOrder : a.hierarchyIndex < b.hierarchyIndex;
    });
    m_stats.canvases = static_cast<u32>(m_passes.size());
    m_stats.elements = 0;
    for (const CanvasPass& p : m_passes) m_stats.elements += static_cast<u32>(p.elements.size());
    m_stats.layoutMs = sw.elapsedMilliseconds();
}

void UISystem::layoutChildren(Scene& scene, ecs::Entity parent, Vec4 pr, float s, CanvasPass& pass) {
    auto& reg = scene.registry();
    std::vector<ecs::Entity> kids;
    scene.forEachChild(parent, [&](ecs::Entity c) {
        if (reg.has<UIRectComponent>(c) && isActive(scene, c)) kids.push_back(c);
    });
    if (kids.empty()) return;
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
    for (usize i = 0; i < kids.size(); ++i) {
        const ecs::Entity c = kids[i];
        reg.get<UIRectComponent>(c).screenRect = rects[i];
        Element el{c, rects[i]};
        const auto* img = reg.tryGet<UIImageComponent>(c);
        const auto* btn = reg.tryGet<UIButtonComponent>(c);
        const auto* sld = reg.tryGet<UISliderComponent>(c);
        el.interactive = btn || sld;
        el.blocks = el.interactive || (img && img->color.w > 0.0f);
        pass.elements.push_back(el);
        layoutChildren(scene, c, rects[i], s, pass);
    }
}

// ------------------------------------------------------------------- input
ecs::Entity UISystem::hitTest(Vec2 p) const {
    for (auto pass = m_passes.rbegin(); pass != m_passes.rend(); ++pass)
        for (auto el = pass->elements.rbegin(); el != pass->elements.rend(); ++el)
            if (el->blocks && contains(el->rect, p)) return el->entity;
    return {};
}

ecs::Entity UISystem::elementAt(Vec2 p) const {
    for (auto pass = m_passes.rbegin(); pass != m_passes.rend(); ++pass)
        for (auto el = pass->elements.rbegin(); el != pass->elements.rend(); ++el)
            if (el->entity != pass->canvas && contains(el->rect, p)) return el->entity;
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

void UISystem::setSliderValue(Scene& scene, ecs::Entity e, float x) {
    auto* s = scene.registry().tryGet<UISliderComponent>(e);
    auto* r = scene.registry().tryGet<UIRectComponent>(e);
    if (!s || !r || !s->interactable) return;
    const Vec4 rect = r->screenRect;
    const float handle = rect.w * 0.5f;
    const float t = std::clamp((x - rect.x - handle) / std::max(1.0f, rect.z - 2.0f * handle), 0.0f, 1.0f);
    float v = s->minValue + t * (s->maxValue - s->minValue);
    if (s->wholeNumbers) v = std::round(v);
    if (v != s->value) {
        s->value = v;
        m_events.push_back({UIEventType::ValueChanged, e, v});
    }
}

void UISystem::processInput(Scene& scene, std::span<const UIPointer> pointers) {
    auto& reg = scene.registry();
    m_hovered.clear();
    m_pointerOverUI = false;
    std::unordered_set<u64> seen;
    for (const UIPointer& p : pointers) {
        seen.insert(p.id);
        const bool wasDown = m_pointerDown[p.id];
        const ecs::Entity top = hitTest(p.position);
        if (!top.isNull()) m_pointerOverUI = true;
        const bool topInteractive = !top.isNull() && (reg.has<UIButtonComponent>(top) || reg.has<UISliderComponent>(top));
        auto pressedIt = m_pressed.find(p.id);
        if (pressedIt != m_pressed.end() && !reg.valid(pressedIt->second)) {
            m_pressed.erase(pressedIt);
            pressedIt = m_pressed.end();
        }
        if (p.down && !wasDown) {
            if (topInteractive) {
                const auto* b = reg.tryGet<UIButtonComponent>(top);
                const auto* s = reg.tryGet<UISliderComponent>(top);
                if ((b && b->interactable) || (s && s->interactable)) {
                    m_pressed[p.id] = top;
                    if (s) setSliderValue(scene, top, p.position.x);
                }
            }
        } else if (p.down && pressedIt != m_pressed.end()) {
            if (reg.has<UISliderComponent>(pressedIt->second)) setSliderValue(scene, pressedIt->second, p.position.x);
        } else if (!p.down && wasDown && pressedIt != m_pressed.end()) {
            const ecs::Entity pressed = pressedIt->second;
            const auto* b = reg.tryGet<UIButtonComponent>(pressed);
            if (b && b->interactable && pressed == top) m_events.push_back({UIEventType::Click, pressed, 0.0f});
            m_pressed.erase(pressedIt);
        }
        if (m_pressed.count(p.id)) m_pointerOverUI = true;
        if (!p.down && topInteractive) m_hovered.insert(top.packed());
        m_pointerDown[p.id] = p.down;
    }
    // Pointers that disappeared (finger lost, window left): released without a click.
    for (auto it = m_pointerDown.begin(); it != m_pointerDown.end();) {
        if (!seen.count(it->first)) {
            m_pressed.erase(it->first);
            it = m_pointerDown.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<UIEvent> UISystem::takeEvents() {
    std::vector<UIEvent> out;
    out.swap(m_events);
    return out;
}

void UISystem::reset() {
    m_pressed.clear();
    m_hovered.clear();
    m_pointerDown.clear();
    m_events.clear();
    m_pointerOverUI = false;
}

// ----------------------------------------------------------------- drawing
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
    const bool sliced = texW > 0.0f && (img.border.x > 0 || img.border.y > 0 || img.border.z > 0 || img.border.w > 0);
    if (!sliced) {
        Vec4 r = rect;
        if (img.preserveAspect && texW > 0.0f && texH > 0.0f) {
            const float k = std::min(rect.z / texW, rect.w / texH);
            const float w = texW * k, h = texH * k;
            r = {rect.x + (rect.z - w) * 0.5f, rect.y + (rect.w - h) * 0.5f, w, h};
        }
        out.quad({r.x, r.y}, {r.x + r.z, r.y + r.w}, {0, 0, 1, 1}, packed, tex);
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
            out.quad({xs[x], ys[y]}, {xs[x + 1], ys[y + 1]}, {us[x], vs[y], us[x + 1], vs[y + 1]}, packed, tex);
        }
}

void UISystem::drawElement(Scene& scene, const Element& el, float scale, OverlayDrawList& out) {
    auto& reg = scene.registry();
    Vec4 tint{1, 1, 1, 1};
    if (const auto* b = reg.tryGet<UIButtonComponent>(el.entity)) {
        if (!b->interactable) tint = b->disabledColor;
        else if (isPressed(el.entity)) tint = b->pressedColor;
        else if (isHovered(el.entity)) tint = b->hoverColor;
        else tint = b->normalColor;
    }
    if (const auto* img = reg.tryGet<UIImageComponent>(el.entity)) drawImage(*img, el.rect, tint, scale, out);
    if (const auto* s = reg.tryGet<UISliderComponent>(el.entity)) {
        const Vec4 r = el.rect;
        const float handle = r.w * 0.5f;
        const float range = s->maxValue - s->minValue;
        const float t = range != 0.0f ? std::clamp((s->value - s->minValue) / range, 0.0f, 1.0f) : 0.0f;
        const float hx = r.x + handle + t * std::max(0.0f, r.z - 2.0f * handle);
        const float trackH = std::max(2.0f, r.w * 0.3f), ty = r.y + (r.w - trackH) * 0.5f;
        const Vec4 dim = s->interactable ? Vec4{1, 1, 1, 1} : Vec4{1, 1, 1, 0.5f};
        out.quad({r.x, ty}, {r.x + r.z, ty + trackH}, {0, 0, 1, 1}, packColor(mul(s->trackColor, dim)), {});
        out.quad({r.x, ty}, {hx, ty + trackH}, {0, 0, 1, 1}, packColor(mul(s->fillColor, dim)), {});
        const float hs = r.w * 0.5f;
        out.quad({hx - hs, r.y}, {hx + hs, r.y + r.w}, {0, 0, 1, 1}, packColor(mul(s->handleColor, dim)), {});
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
        for (usize ci = 0; ci < m_passes.size(); ++ci) {
            const CanvasPass& pass = m_passes[ci];
            for (usize ei = 0; ei < pass.elements.size(); ++ei) {
                const Element& el = pass.elements[ei];
                const auto* t = reg.tryGet<UITextComponent>(el.entity);
                if (!t || t->text.empty() || t->color.w <= 0.0f) continue;
                auto lay = m_fonts.layout(t->font, t->text, t->fontSize * pass.scale, el.rect.z, t->wrap, t->lineSpacing, t->hAlign);
                if (!lay) {
                    if (m_reportedFonts.insert(t->font).second) AGR_LOG_ERROR("UI", "font '{}': {}", t->font, lay.error().message);
                    continue;
                }
                Vec2 origin{el.rect.x, el.rect.y};
                if (t->vAlign == UIVAlign::Middle) origin.y += (el.rect.w - lay.value().size.y) * 0.5f;
                else if (t->vAlign == UIVAlign::Bottom) origin.y += el.rect.w - lay.value().size.y;
                origin = {std::round(origin.x), std::round(origin.y)};
                m_texts.push_back({ei, ci, std::move(lay.value()), origin});
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

    // Pass 2: geometry in painter's order (element, then its text, then its children).
    usize ti = 0;
    for (usize ci = 0; ci < m_passes.size(); ++ci) {
        const CanvasPass& pass = m_passes[ci];
        for (usize ei = 0; ei < pass.elements.size(); ++ei) {
            drawElement(scene, pass.elements[ei], pass.scale, out);
            for (; ti < m_texts.size() && m_texts[ti].canvas == ci && m_texts[ti].element == ei; ++ti) {
                const PendingText& pt = m_texts[ti];
                const u32 color = packColor(reg.get<UITextComponent>(pass.elements[ei].entity).color);
                for (const TextGlyph& g : pt.layout.glyphs)
                    out.quad(pt.origin + g.min, pt.origin + g.max, g.uv, color, m_fonts.texture());
            }
        }
    }
    m_stats.quads = static_cast<u32>(out.indices.size() / 6);
    m_stats.texts = static_cast<u32>(m_texts.size());
    m_stats.buildMs = sw.elapsedMilliseconds();
}

} // namespace agr
