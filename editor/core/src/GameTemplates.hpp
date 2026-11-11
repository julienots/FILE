#pragma once
// Playable game templates (Phase 9): one builder per game. Scenes reference the original art
// and Lua scripts shipped in assets/games/ (copied into every new project). See docs/TEMPLATES.md.
#include <string>

#include "TemplateHelpers.hpp"
#include "agr/physics2d/Physics2DComponents.hpp"
#include "agr/physics3d/Physics3DComponents.hpp"
#include "agr/script/ScriptComponent.hpp"

namespace agr::editor::games {

using math::Vec3;
using math::Vec4;
using math::Vec2;

/// Small helpers that keep the builders readable.
struct Builder {
    Scene& scene;
    ecs::Registry& reg;
    explicit Builder(Scene& s) : scene(s), reg(s.registry()) {}

    ecs::Entity entity(const std::string& name, math::Vec3 pos = {}, ecs::Entity parent = {}) {
        const auto e = scene.createEntity(name, parent);
        reg.get<TransformComponent>(e).position = pos;
        return e;
    }
    ecs::Entity camera2D(float halfHeight, math::Vec4 clear, math::Vec3 pos = {0.0f, 0.0f, 10.0f}) {
        const auto e = entity("Main Camera", pos);
        auto& c = reg.emplace<CameraComponent>(e);
        c.projection = Projection::Orthographic;
        c.orthographicSize = halfHeight;
        c.scaleMode = ScaleMode::FixedHeight;
        c.clearColor = clear;
        return e;
    }
    ecs::Entity camera3D(Vec3 eye, Vec3 target, float fovDegrees = 50.0f) {
        const auto e = entity("Main Camera");
        auto& c = reg.emplace<CameraComponent>(e);
        c.fieldOfView = math::radians(fovDegrees);
        templates::lookAt(scene, e, eye, target);
        return e;
    }
    ecs::Entity sprite(const std::string& name, const std::string& texture, math::Vec3 pos, math::Vec2 size, i32 layer = 0,
                       ecs::Entity parent = {}) {
        const auto e = entity(name, pos, parent);
        auto& s = reg.emplace<SpriteRendererComponent>(e);
        s.texture = texture;
        s.size = size;
        s.sortingLayer = layer;
        return e;
    }
    SpriteRendererComponent& spriteOf(ecs::Entity e) { return reg.get<SpriteRendererComponent>(e); }
    void script(ecs::Entity e, const std::string& uri, const std::string& properties = {}) {
        auto& s = reg.emplace<ScriptComponent>(e);
        s.script = uri;
        s.properties = properties;
    }
    RigidBody2DComponent& body2D(ecs::Entity e, BodyType2D type, bool fixedRotation = true) {
        auto& b = reg.emplace<RigidBody2DComponent>(e);
        b.type = type;
        b.fixedRotation = fixedRotation;
        return b;
    }
    Collider2DComponent& box2D(ecs::Entity e, math::Vec2 size, math::Vec2 offset = {}, bool trigger = false) {
        auto& c = reg.emplace<Collider2DComponent>(e);
        c.size = size;
        c.offset = offset;
        c.isTrigger = trigger;
        return c;
    }
    Collider2DComponent& circle2D(ecs::Entity e, float radius, bool trigger = false, math::Vec2 offset = {}) {
        auto& c = reg.emplace<Collider2DComponent>(e);
        c.shape = ColliderShape2D::Circle;
        c.radius = radius;
        c.isTrigger = trigger;
        c.offset = offset;
        return c;
    }
    ecs::Entity mesh(const std::string& name, PrimitiveMesh m, Vec3 pos, Vec3 scale, math::Vec4 color, ecs::Entity parent = {}) {
        const auto e = entity(name, pos, parent);
        reg.get<TransformComponent>(e).scale = scale;
        auto& r = reg.emplace<MeshRendererComponent>(e);
        r.mesh = m;
        r.color = color;
        return e;
    }
    void hide(ecs::Entity e) { reg.get<StateComponent>(e).active = false; }

    // ---- interface (1920 x 1080 reference)
    ecs::Entity hud() { return templates::uiCanvas(scene); }
    /// Text box: anchor + position + size like uiElement, aligned left / center / right.
    ecs::Entity label(const std::string& name, ecs::Entity parent, math::Vec2 anchor, math::Vec2 pos, math::Vec2 size, const std::string& text,
                      float fontSize, UIHAlign align = UIHAlign::Left, math::Vec4 color = {1.0f, 0.97f, 0.9f, 1.0f}) {
        const auto e = templates::uiElement(scene, name, parent, anchor, pos, size);
        auto& t = reg.emplace<UITextComponent>(e);
        t.text = text;
        t.fontSize = fontSize;
        t.hAlign = align;
        t.color = color;
        return e;
    }
    ecs::Entity panel(const std::string& name, ecs::Entity parent, math::Vec2 anchor, math::Vec2 pos, math::Vec2 size, math::Vec4 color = {0.05f, 0.06f, 0.08f, 0.62f}) {
        const auto e = templates::uiPanel(scene, name, parent, anchor, pos, size);
        reg.get<UIImageComponent>(e).color = color;
        return e;
    }
    /// Button whose click calls `message(button)` on `target`'s script (empty: only onClick).
    ecs::Entity button(const std::string& name, ecs::Entity parent, math::Vec2 anchor, math::Vec2 pos, math::Vec2 size, const std::string& text,
                       ecs::Entity target = {}, const std::string& message = {}) {
        const auto e = templates::uiButton(scene, name, parent, anchor, pos, size, text);
        auto& b = reg.get<UIButtonComponent>(e);
        b.target = target;
        b.message = message;
        reg.get<UIImageComponent>(e).color = {0.16f, 0.18f, 0.24f, 0.85f};
        return e;
    }
    /// Text of the button's label.
    UITextComponent& buttonText(ecs::Entity button) { return reg.get<UITextComponent>(scene.children(button).front()); }
};

/// RPG HUD (health, level, quest, dialog, Attack / Talk buttons), shared by RPG 2D and 3D.
void rpgHud(Builder& b, ecs::Entity hero);

void buildPlatformer(Scene& scene);
void buildRpg2D(Scene& scene);
void buildRpg3D(Scene& scene);
void buildTowerDefense(Scene& scene);
void buildCards(Scene& scene);
void buildFarm(Scene& scene);
void buildStrategy(Scene& scene);
void buildSurvival(Scene& scene);
void buildRacing(Scene& scene);
void buildIdle(Scene& scene);
void buildTycoon(Scene& scene);
void buildArena(Scene& scene);

} // namespace agr::editor::games
