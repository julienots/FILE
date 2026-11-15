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
    return reg.has<UIButtonComponent>(e) || reg.has<UISliderComponent>(e) || reg.has<UIToggleComponent>(e) || reg.has<UIInputFieldComponent>(e);
}
bool interactableNow(ecs::Registry& reg, ecs::Entity e) {
    if (const auto* b = reg.tryGet<UIButtonComponent>(e)) return b->interactable;
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
void utf8PopBack(std::string& s) {
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0) != 0x80) break;
    }
}

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

UISystem::UISystem(VirtualFileSystem& vfs) : m_fonts(vfs) {}
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
        layoutChildren(scene, e, content, pass.scale, pass, screen);
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

void UISystem::layoutChildren(Scene& scene, ecs::Entity parent, Vec4 pr, float s, CanvasPass& pass, Vec4 clip) {
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
        reg.get<UIRectComponent>(c).screenRect = rects[i];
        Element el{c, rects[i], false, false, childClip};
        const auto* img = reg.tryGet<UIImageComponent>(c);
        el.interactive = interactiveKind(reg, c);
        el.blocks = el.interactive || reg.has<UIScrollViewComponent>(c) || (img && img->blocksPointer && img->color.w > 0.0f);
        pass.elements.push_back(el);
        layoutChildren(scene, c, rects[i], s, pass, childClip);
    }
}

// ------------------------------------------------------------------- input
ecs::Entity UISystem::hitTest(Vec2 p) const {
    for (auto pass = m_passes.rbegin(); pass != m_passes.rend(); ++pass)
        for (auto el = pass->elements.rbegin(); el != pass->elements.rend(); ++el)
            if (el->blocks && contains(el->rect, p) && contains(el->clip, p)) return el->entity;
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
        m_events.push_back({UIEventType::ValueChanged, e, v, {}});
    }
}

void UISystem::processInput(Scene& scene, std::span<const UIPointer> pointers) {
    auto& reg = scene.registry();
    m_hovered.clear();
    m_pointerOverUI = false;
    std::unordered_set<u64> seen;
    if (!m_focusedField.isNull() && (!reg.valid(m_focusedField) || !reg.has<UIInputFieldComponent>(m_focusedField))) focusField({});
    for (const UIPointer& p : pointers) {
        seen.insert(p.id);
        const bool wasDown = m_pointerDown[p.id];
        const ecs::Entity hit = hitTest(p.position);
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
            if (topInteractive && interactableNow(reg, top)) {
                m_pressed[p.id] = top;
                if (reg.has<UISliderComponent>(top)) setSliderValue(scene, top, p.position.x);
            }
        } else if (p.down) {
            const bool slider = pressedIt != m_pressed.end() && reg.has<UISliderComponent>(pressedIt->second);
            if (!ps.dragging && !ps.scrollView.isNull() && !slider && math::length(p.position - ps.down) > 12.0f) {
                ps.dragging = true;  // a drag scrolls the view and cancels the press inside it
                if (pressedIt != m_pressed.end()) {
                    m_pressed.erase(pressedIt);
                    pressedIt = m_pressed.end();
                }
            }
            if (ps.dragging && reg.valid(ps.scrollView)) scrollBy(scene, ps.scrollView, ps.last - p.position);
            else if (slider) setSliderValue(scene, pressedIt->second, p.position.x);
            ps.last = p.position;
        } else if (!p.down && wasDown) {
            if (pressedIt != m_pressed.end()) {
                const ecs::Entity pressed = pressedIt->second;
                if (pressed == top && !ps.dragging && interactableNow(reg, pressed) && !reg.has<UISliderComponent>(pressed)) activate(scene, pressed);
                m_pressed.erase(pressedIt);
            }
            ps.dragging = false;
        }
        if (m_pressed.count(p.id) || ps.dragging) m_pointerOverUI = true;
        if (!p.down && topInteractive) m_hovered.insert(top.packed());
        m_pointerDown[p.id] = p.down;
    }
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
    m_caretClock.restart();
    if (m_textInputHandler && had != !field.isNull()) m_textInputHandler(!field.isNull());
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
    for (u32 i = 0; i < backspaces; ++i) utf8PopBack(f->text);
    for (usize i = 0; i < typed.size();) {
        const unsigned char c = static_cast<unsigned char>(typed[i]);
        const usize n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        const std::string ch(typed.substr(i, n));
        i += n;
        if (c < 0x20 || c == 0x7F) continue;  // control characters
        if (f->numeric && !(n == 1 && (std::isdigit(c) || c == '-' || c == '.'))) continue;
        if (f->maxLength > 0 && utf8Length(f->text) >= f->maxLength) break;
        f->text += ch;
    }
    if (f->text != before) m_events.push_back({UIEventType::TextChanged, m_focusedField, 0.0f, f->text});
    if (enter) {
        m_events.push_back({UIEventType::Submit, m_focusedField, 0.0f, f->text});
        focusField({});
    }
    m_caretClock.restart();
}

ecs::Entity UISystem::scrollViewOf(Scene& scene, ecs::Entity e) const {
    for (ecs::Entity p = e; !p.isNull() && scene.valid(p); p = scene.parent(p)) {
        if (scene.registry().has<UIScrollViewComponent>(p)) return p;
        if (scene.registry().has<UICanvasComponent>(p)) break;
    }
    return {};
}

void UISystem::scrollBy(Scene& scene, ecs::Entity view, Vec2 pixels) {
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
        m_navFocus = best->e;
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
    if (x0 == mn.x && y0 == mn.y && x1 == mx.x && y1 == mx.y) {
        out.quad(mn, mx, uv, color, texture);
        return;
    }
    const float w = std::max(1e-6f, mx.x - mn.x), h = std::max(1e-6f, mx.y - mn.y);
    const float u0 = uv.x + (uv.z - uv.x) * (x0 - mn.x) / w, u1 = uv.x + (uv.z - uv.x) * (x1 - mn.x) / w;
    const float v0 = uv.y + (uv.w - uv.y) * (y0 - mn.y) / h, v1 = uv.y + (uv.w - uv.y) * (y1 - mn.y) / h;
    out.quad({x0, y0}, {x1, y1}, {u0, v0, u1, v1}, color, texture);
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
        emit(out, {r.x, ty}, {r.x + r.z, ty + trackH}, {0, 0, 1, 1}, packColor(mul(s->trackColor, dim)), {});
        emit(out, {r.x, ty}, {hx, ty + trackH}, {0, 0, 1, 1}, packColor(mul(s->fillColor, dim)), {});
        const float hs = r.w * 0.5f;
        emit(out, {hx - hs, r.y}, {hx + hs, r.y + r.w}, {0, 0, 1, 1}, packColor(mul(s->handleColor, dim)), {});
    }
    if (const auto* t = reg.tryGet<UIToggleComponent>(el.entity)) {  // box at the left, mark when on
        const Vec4 r = el.rect;
        const float box = std::min(r.w, r.z) * 0.8f, bx = r.x + (r.w - box) * 0.5f, by = r.y + (r.w - box) * 0.5f;
        const Vec4 dim = t->interactable ? (isHovered(el.entity) ? Vec4{1.2f, 1.2f, 1.2f, 1} : Vec4{1, 1, 1, 1}) : Vec4{1, 1, 1, 0.5f};
        emit(out, {bx, by}, {bx + box, by + box}, {0, 0, 1, 1}, packColor(mul(t->boxColor, dim)), {});
        if (t->isOn) {
            const float in = box * 0.22f;
            emit(out, {bx + in, by + in}, {bx + box - in, by + box - in}, {0, 0, 1, 1}, packColor(mul(t->checkColor, dim)), {});
        }
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
            const Element& el = pass.elements[ei];
            m_clip = el.clip;
            drawElement(scene, el, pass.scale, out);
            float caretX = el.rect.x + 4.0f * pass.scale;
            for (; ti < m_texts.size() && m_texts[ti].canvas == ci && m_texts[ti].element == ei; ++ti) {
                const PendingText& pt = m_texts[ti];
                const u32 color = packColor(reg.get<UITextComponent>(el.entity).color);
                for (const TextGlyph& g : pt.layout.glyphs) {
                    emit(out, pt.origin + g.min, pt.origin + g.max, g.uv, color, m_fonts.texture());
                    caretX = std::max(caretX, pt.origin.x + g.max.x + 2.0f);
                }
            }
            if (el.entity == m_focusedField && std::fmod(m_caretClock.elapsedSeconds(), 1.0) < 0.55) {  // blinking caret
                const auto* f = reg.tryGet<UIInputFieldComponent>(el.entity);
                if (f && f->text.empty()) caretX = el.rect.x + 4.0f * pass.scale;
                const float h = el.rect.w * 0.6f, y = el.rect.y + (el.rect.w - h) * 0.5f;
                emit(out, {caretX, y}, {caretX + std::max(2.0f, 2.0f * pass.scale), y + h}, {0, 0, 1, 1}, packColor(f ? f->textColor : Vec4{1, 1, 1, 1}), {});
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
    }
    m_stats.quads = static_cast<u32>(out.indices.size() / 6);
    m_stats.texts = static_cast<u32>(m_texts.size());
    m_stats.buildMs = sw.elapsedMilliseconds();
}

} // namespace agr
