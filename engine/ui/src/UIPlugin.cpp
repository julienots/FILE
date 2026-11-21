#include "agr/ui/UIPlugin.hpp"

#include "agr/render/RenderPlugin.hpp"
#include "agr/runtime/Engine.hpp"

#include <cmath>

namespace agr {

PluginInfo UIPlugin::info() const {
    return {"UI", "1.0", "Game UI: canvases, anchors, images, text, buttons, sliders, layouts", "Superessence Studio", {}};
}

Status UIPlugin::onLoad(Engine& engine) {
    registerUIComponents();
    m_ui = std::make_unique<UISystem>(engine.vfs());
    m_ui->setLocalization(&engine.localization());
    m_ui->setTextInputHandler([&engine](bool on) { engine.platform().setTextInput(on); });
    m_ui->setClipboard([&engine] { return engine.platform().clipboardText(); },
                       [&engine](const std::string& t) { engine.platform().setClipboardText(t); });
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
    // Safe area in UI pixels (also used when drawing in edit mode).
    if (m_safeOverride) {
        m_ui->setSafeArea(*m_safeOverride);
    } else if (!m_viewport) {
        const math::Vec2 win = engine.platform().windowSize(), fb = engine.platform().framebufferSize();
        const math::Vec4 in = engine.platform().safeAreaInsets();
        const float kx = win.x > 0.0f ? fb.x / win.x : 1.0f, ky = win.y > 0.0f ? fb.y / win.y : 1.0f;
        m_ui->setSafeArea({in.x * kx, in.y * ky, in.z * kx, in.w * ky});
    } else {
        m_ui->setSafeArea({0.0f, 0.0f, 0.0f, 0.0f});
    }
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
    // Mouse wheel over a scroll view.
    if (in.wheel().x != 0.0f || in.wheel().y != 0.0f) m_ui->scrollAt(scene, toUI(in.mousePosition()), in.wheel());
    // Typing into the focused text field.
    if (m_ui->wantsText()) {
        const bool shift = in.keyDown(Key::LeftShift) || in.keyDown(Key::RightShift);
        const bool ctrl = in.keyDown(Key::LeftCtrl) || in.keyDown(Key::RightCtrl) || in.keyDown(Key::LeftSuper) || in.keyDown(Key::RightSuper);
        using TK = UISystem::TextKey;
        const std::pair<Key, TK> keys[] = {{Key::Left, TK::Left}, {Key::Right, TK::Right}, {Key::Up, TK::Up},   {Key::Down, TK::Down},
                                           {Key::Home, TK::Home}, {Key::End, TK::End},     {Key::Delete, TK::Delete}};
        for (const auto& [k, tk] : keys)
            if (in.keyPressed(k)) m_ui->processTextKey(scene, tk, shift, ctrl);
        if (ctrl) {
            if (in.keyPressed(Key::A)) m_ui->processTextKey(scene, TK::SelectAll);
            if (in.keyPressed(Key::C)) m_ui->processTextKey(scene, TK::Copy);
            if (in.keyPressed(Key::X)) m_ui->processTextKey(scene, TK::Cut);
            if (in.keyPressed(Key::V)) m_ui->processTextKey(scene, TK::Paste);
        }
        // Shortcut letters typed with Ctrl are not text.
        m_ui->processText(scene, ctrl ? std::string_view{} : in.textInput(), in.keyPressed(Key::Backspace) ? 1u : 0u, in.keyPressed(Key::Return));
    }
    // Keyboard / gamepad navigation (canvases with keyboardNavigation), not while typing.
    if (!m_ui->wantsText() && m_ui->navigationEnabled()) {
        const float sx = in.gamepadCount() > 0 ? in.gamepadAxis(0, GamepadAxis::LeftX) : 0.0f;
        const float sy = in.gamepadCount() > 0 ? in.gamepadAxis(0, GamepadAxis::LeftY) : 0.0f;
        const int stickDir = std::abs(sx) > 0.6f || std::abs(sy) > 0.6f ? (std::abs(sx) > std::abs(sy) ? (sx > 0 ? 1 : 3) : (sy > 0 ? 2 : 4)) : 0;
        const bool stickEdge = stickDir != 0 && stickDir != m_stickDir;
        m_stickDir = stickDir;
        const bool up = in.keyPressed(Key::Up) || in.anyGamepadPressed(GamepadButton::DpadUp) || (stickEdge && stickDir == 4);
        const bool down = in.keyPressed(Key::Down) || in.anyGamepadPressed(GamepadButton::DpadDown) || (stickEdge && stickDir == 2);
        const bool left = in.keyPressed(Key::Left) || in.anyGamepadPressed(GamepadButton::DpadLeft) || (stickEdge && stickDir == 3);
        const bool right = in.keyPressed(Key::Right) || in.anyGamepadPressed(GamepadButton::DpadRight) || (stickEdge && stickDir == 1);
        const bool onSlider = !m_ui->navFocus().isNull() && scene.registry().valid(m_ui->navFocus()) && scene.registry().has<UISliderComponent>(m_ui->navFocus());
        if (up) m_ui->navigate(scene, {0.0f, -1.0f});
        if (down) m_ui->navigate(scene, {0.0f, 1.0f});
        if (left) onSlider ? m_ui->navAdjust(scene, -1.0f) : m_ui->navigate(scene, {-1.0f, 0.0f});
        if (right) onSlider ? m_ui->navAdjust(scene, 1.0f) : m_ui->navigate(scene, {1.0f, 0.0f});
        if (in.keyPressed(Key::Return) || in.keyPressed(Key::Space) || in.anyGamepadPressed(GamepadButton::South)) m_ui->navActivate(scene);
    }
    for (const UIEvent& e : m_ui->takeEvents()) {
        if (e.type == UIEventType::Click) engine.events().publish(UIClickEvent{e.entity});
        else if (e.type == UIEventType::ValueChanged) engine.events().publish(UIValueChangedEvent{e.entity, e.value});
        else if (e.type == UIEventType::Link) engine.events().publish(UITextEvent{e.entity, e.text, false, true});
        else engine.events().publish(UITextEvent{e.entity, e.text, e.type == UIEventType::Submit});
    }
}

} // namespace agr
