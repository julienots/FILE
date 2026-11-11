#pragma once
// Building blocks shared by the scene templates (SceneTemplates.cpp, GameTemplates.cpp).
#include <string>

#include "agr/audio/AudioComponents.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/ui/UIComponents.hpp"

namespace agr::editor::templates {

using math::Quat;
using math::Vec3;

// ------------------------------------------------------------------ UI helpers
inline ecs::Entity uiCanvas(Scene& scene) {
    const auto c = scene.createEntity("HUD");
    auto& cv = scene.registry().emplace<UICanvasComponent>(c);
    cv.referenceResolution = {1920.0f, 1080.0f};
    cv.match = 0.5f;
    return c;
}

inline ecs::Entity uiElement(Scene& scene, const std::string& name, ecs::Entity parent, math::Vec2 anchor, math::Vec2 position, math::Vec2 size) {
    const auto e = scene.createEntity(name, parent);
    auto& r = scene.registry().emplace<UIRectComponent>(e);
    r.anchorMin = r.anchorMax = r.pivot = anchor;
    r.position = position;
    r.size = size;
    return e;
}

/// Text filling its parent.
inline ecs::Entity uiLabel(Scene& scene, const std::string& name, ecs::Entity parent, const std::string& text, float fontSize, math::Vec4 color) {
    const auto e = scene.createEntity(name, parent);
    auto& r = scene.registry().emplace<UIRectComponent>(e);
    r.anchorMin = {0.0f, 0.0f};
    r.anchorMax = {1.0f, 1.0f};
    r.size = {0.0f, 0.0f};
    auto& t = scene.registry().emplace<UITextComponent>(e);
    t.text = text;
    t.fontSize = fontSize;
    t.color = color;
    return e;
}

inline ecs::Entity uiPanel(Scene& scene, const std::string& name, ecs::Entity parent, math::Vec2 anchor, math::Vec2 position, math::Vec2 size) {
    const auto e = uiElement(scene, name, parent, anchor, position, size);
    scene.registry().emplace<UIImageComponent>(e).color = {0.05f, 0.06f, 0.08f, 0.62f};
    return e;
}

inline ecs::Entity uiButton(Scene& scene, const std::string& name, ecs::Entity parent, math::Vec2 anchor, math::Vec2 position, math::Vec2 size,
                     const std::string& label) {
    const auto e = uiElement(scene, name, parent, anchor, position, size);
    scene.registry().emplace<UIImageComponent>(e).color = {1.0f, 1.0f, 1.0f, 0.22f};
    auto& b = scene.registry().emplace<UIButtonComponent>(e);
    b.pressedColor = {2.2f, 1.6f, 0.6f, 2.2f};  // brighter, warmer while held
    uiLabel(scene, "Label", e, label, size.y * 0.42f, {1.0f, 1.0f, 1.0f, 0.9f});
    return e;
}

inline ecs::Entity audioSource(Scene& scene, const std::string& name, const std::string& clip, AudioBus bus, float volume) {
    const auto e = scene.createEntity(name);
    auto& a = scene.registry().emplace<AudioSourceComponent>(e);
    a.clip = clip;
    a.bus = bus;
    a.volume = volume;
    a.loop = true;
    a.stream = true;
    return e;
}

inline void lookAt(Scene& scene, ecs::Entity e, Vec3 eye, Vec3 target) {
    auto& t = scene.registry().get<TransformComponent>(e);
    Vec3 s;
    math::decompose(math::inverse(math::Mat4::lookAt(eye, target, Vec3::up())), t.position, t.rotation, s);
}

inline ecs::Entity sun(Scene& scene) {
    const auto light = scene.createEntity("Sun Light");
    scene.registry().emplace<DirectionalLightComponent>(light).intensity = 1.1f;
    scene.registry().emplace<EnvironmentComponent>(light);
    scene.registry().get<TransformComponent>(light).rotation =
        Quat::fromEuler({math::radians(-50.0f), math::radians(35.0f), 0.0f});
    return light;
}


} // namespace agr::editor::templates
