#include "agr/ui/UIPlugin.hpp"

#include "agr/render/RenderPlugin.hpp"
#include "agr/runtime/Engine.hpp"

namespace agr {

PluginInfo UIPlugin::info() const {
    return {"UI", "1.0", "Game UI: canvases, anchors, images, text, buttons, sliders, layouts", "Superessence Studio", {}};
}

Status UIPlugin::onLoad(Engine& engine) {
    registerUIComponents();
    m_ui = std::make_unique<UISystem>(engine.vfs());
    engine.registerService<UIPlugin>(this);
    bindRenderer(engine);
    return {};
}

void UIPlugin::onUnload(Engine& engine) {
    if (m_renderer && m_renderer->overlaySource() == m_ui.get()) m_renderer->setOverlaySource(nullptr);
    m_renderer = nullptr;
    engine.registerService<UIPlugin>(nullptr);
    m_ui.reset();
}

void UIPlugin::bindRenderer(Engine& engine) {
    auto* rp = engine.service<RenderPlugin>();
    Renderer* r = rp ? rp->renderer() : nullptr;
    if (r == m_renderer) return;
    if (m_renderer && m_renderer->overlaySource() == m_ui.get()) m_renderer->setOverlaySource(nullptr);
    m_renderer = r;
    m_ui->setRenderer(r);
    if (r) r->setOverlaySource(m_ui.get());
}

void UIPlugin::setRunning(bool running) {
    m_running = running;
    if (!running && m_ui) m_ui->reset();
}

void UIPlugin::setViewport(math::Vec4 rect, u32 w, u32 h) { m_viewport = Viewport{rect, w, h}; }

void UIPlugin::onUpdate(Engine& engine, const FrameTime&) {
    bindRenderer(engine);
    if (!m_running) return;
    Scene& scene = engine.scene();
    const math::Vec2 window = engine.platform().windowSize();
    math::Vec2 origin{0.0f, 0.0f}, logical = window;
    math::Vec2 pixels = engine.platform().framebufferSize();
    if (m_viewport) {
        origin = {m_viewport->rect.x, m_viewport->rect.y};
        logical = {m_viewport->rect.z, m_viewport->rect.w};
        pixels = {static_cast<float>(m_viewport->width), static_cast<float>(m_viewport->height)};
    }
    if (pixels.x < 1.0f || pixels.y < 1.0f || logical.x < 1.0f || logical.y < 1.0f) return;
    m_ui->layout(scene, static_cast<u32>(pixels.x), static_cast<u32>(pixels.y));

    // Window coordinates -> UI pixels.
    const math::Vec2 k{pixels.x / logical.x, pixels.y / logical.y};
    auto toUI = [&](math::Vec2 p) { return math::Vec2{(p.x - origin.x) * k.x, (p.y - origin.y) * k.y}; };
    auto inside = [&](math::Vec2 p) { return p.x >= 0.0f && p.y >= 0.0f && p.x < pixels.x && p.y < pixels.y; };
    std::vector<UIPointer> pointers;
    const Input& in = engine.input();
    if (!in.touches().empty()) {
        for (const Touch& t : in.touches()) pointers.push_back({t.id + 1, toUI(t.position), t.phase != TouchPhase::Ended});
    } else {
        const math::Vec2 p = toUI(in.mousePosition());
        const bool down = in.mouseDown(MouseButton::Left);
        // Outside the game view the mouse is absent, unless it holds an element (slider drag).
        if (inside(p) || down || in.mouseReleased(MouseButton::Left)) pointers.push_back({0, p, down});
    }
    m_ui->processInput(scene, pointers);
    for (const UIEvent& e : m_ui->takeEvents()) {
        if (e.type == UIEventType::Click) engine.events().publish(UIClickEvent{e.entity});
        else engine.events().publish(UIValueChangedEvent{e.entity, e.value});
    }
}

} // namespace agr
