#include "agr/render/RenderComponents.hpp"

#include <algorithm>

#include "agr/scene/Reflection.hpp"

namespace agr {

math::Vec2 orthographicHalfExtents(ScaleMode mode, float size, float refAspect, float aspect) {
    size = std::max(size, 1e-4f);
    refAspect = std::max(refAspect, 1e-3f);
    aspect = std::max(aspect, 1e-3f);
    float halfH = size;
    switch (mode) {
        case ScaleMode::FixedHeight: halfH = size; break;
        case ScaleMode::FixedWidth: halfH = size * refAspect / aspect; break;
        case ScaleMode::FitInside: halfH = std::max(size, size * refAspect / aspect); break;
        case ScaleMode::Crop: halfH = std::min(size, size * refAspect / aspect); break;
    }
    return {halfH * aspect, halfH};
}

void registerRenderComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<CameraComponent>()) return;

    reg.add<CameraComponent>("Camera")
        .category("Rendering")
        .description("Renders the scene from this entity, looking down its -Z axis.")
        .field("projection", &CameraComponent::projection,
               {.enumEntries = {{"Perspective", 0}, {"Orthographic", 1}}})
        .field("fieldOfView", &CameraComponent::fieldOfView,
               {.flags = FieldFlag_Angle, .rangeMin = math::radians(1.0f), .rangeMax = math::radians(170.0f), .speed = 0.01f})
        .field("orthographicSize", &CameraComponent::orthographicSize, {.rangeMin = 0.01f, .rangeMax = 10000.0f})
        .field("nearPlane", &CameraComponent::nearPlane, {.rangeMin = 0.001f, .rangeMax = 1000.0f, .speed = 0.01f})
        .field("farPlane", &CameraComponent::farPlane, {.rangeMin = 0.01f, .rangeMax = 100000.0f, .speed = 1.0f})
        .field("clearColor", &CameraComponent::clearColor, {.flags = FieldFlag_Color})
        .field("primary", &CameraComponent::primary)
        .field("scaleMode", &CameraComponent::scaleMode,
               {.tooltip = "Orthographic: how the view adapts to screen shapes (phones, tablets)",
                .enumEntries = {{"FixedHeight", 0}, {"FixedWidth", 1}, {"FitInside", 2}, {"Crop", 3}}})
        .field("referenceAspect", &CameraComponent::referenceAspect,
               {.tooltip = "Width / height of the design area (16:9 = 1.778)", .rangeMin = 0.2f, .rangeMax = 5.0f, .speed = 0.01f});

    reg.add<DirectionalLightComponent>("DirectionalLight")
        .category("Rendering")
        .description("Light shining along the entity's forward (-Z) axis, like the sun.")
        .field("color", &DirectionalLightComponent::color, {.flags = FieldFlag_Color})
        .field("intensity", &DirectionalLightComponent::intensity, {.rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f});

    reg.add<EnvironmentComponent>("Environment")
        .category("Rendering")
        .description("Ambient light of the scene.")
        .field("ambientColor", &EnvironmentComponent::ambientColor, {.flags = FieldFlag_Color})
        .field("ambientIntensity", &EnvironmentComponent::ambientIntensity, {.rangeMin = 0.0f, .rangeMax = 4.0f, .speed = 0.01f});

    reg.add<MeshRendererComponent>("MeshRenderer")
        .category("Rendering")
        .description("Draws a built-in mesh with a color and an optional texture.")
        .field("mesh", &MeshRendererComponent::mesh,
               {.enumEntries = {{"Cube", 0}, {"Sphere", 1}, {"Plane", 2}, {"Quad", 3}}})
        .field("texture", &MeshRendererComponent::texture, {.tooltip = "Asset URI, e.g. assets://textures/crate.png", .flags = FieldFlag_AssetPath})
        .field("color", &MeshRendererComponent::color, {.flags = FieldFlag_Color})
        .field("uvScale", &MeshRendererComponent::uvScale, {.speed = 0.01f})
        .field("visible", &MeshRendererComponent::visible);

    reg.add<SpriteRendererComponent>("SpriteRenderer")
        .category("Rendering")
        .description("Textured quad for 2D games, batched automatically.")
        .field("texture", &SpriteRendererComponent::texture, {.tooltip = "Asset URI, e.g. assets://sprites/hero.png", .flags = FieldFlag_AssetPath})
        .field("color", &SpriteRendererComponent::color, {.flags = FieldFlag_Color})
        .field("size", &SpriteRendererComponent::size, {.speed = 0.01f})
        .field("pivot", &SpriteRendererComponent::pivot, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("uvRect", &SpriteRendererComponent::uvRect, {.tooltip = "u0, v0, u1, v1 — region of a sprite sheet", .speed = 0.001f})
        .field("flipX", &SpriteRendererComponent::flipX)
        .field("flipY", &SpriteRendererComponent::flipY)
        .field("sortingLayer", &SpriteRendererComponent::sortingLayer, {.speed = 1.0f})
        .field("orderInLayer", &SpriteRendererComponent::orderInLayer, {.speed = 1.0f})
        .field("pixelArt", &SpriteRendererComponent::pixelArt)
        .field("visible", &SpriteRendererComponent::visible)
        .field("sheet", &SpriteRendererComponent::sheet, {.tooltip = "Sprite sheet (.agrsheet): texture and frames", .flags = FieldFlag_AssetPath})
        .field("frame", &SpriteRendererComponent::frame, {.tooltip = "Frame index in the sheet (-1 = none)", .rangeMin = -1, .rangeMax = 100000, .speed = 0.2f})
        .field("lit", &SpriteRendererComponent::lit, {.tooltip = "Lit by Light2D components"});

    reg.add<TilemapComponent>("Tilemap")
        .category("2D")
        .description("Grid of tiles from a tileset sprite sheet. Paint it with the Tilemap tool of the editor.")
        .field("tileset", &TilemapComponent::tileset, {.tooltip = "Sprite sheet whose frames are the tiles", .flags = FieldFlag_AssetPath})
        .field("cellSize", &TilemapComponent::cellSize, {.speed = 0.01f})
        .field("width", &TilemapComponent::width, {.rangeMin = 1, .rangeMax = 4096, .speed = 0.2f})
        .field("height", &TilemapComponent::height, {.rangeMin = 1, .rangeMax = 4096, .speed = 0.2f})
        .field("color", &TilemapComponent::color, {.flags = FieldFlag_Color})
        .field("sortingLayer", &TilemapComponent::sortingLayer, {.speed = 1.0f})
        .field("orderInLayer", &TilemapComponent::orderInLayer, {.speed = 1.0f})
        .field("lit", &TilemapComponent::lit)
        .field("dataWidth", &TilemapComponent::dataWidth, {.flags = FieldFlag_Hidden})
        .field("tiles", &TilemapComponent::tiles, {.flags = FieldFlag_Hidden});

    reg.add<Light2DComponent>("Light2D")
        .category("2D")
        .description("Point light for sprites and tilemaps marked 'lit'.")
        .field("color", &Light2DComponent::color, {.flags = FieldFlag_Color})
        .field("intensity", &Light2DComponent::intensity, {.rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("radius", &Light2DComponent::radius, {.rangeMin = 0.01f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("falloff", &Light2DComponent::falloff, {.rangeMin = 0.1f, .rangeMax = 8.0f, .speed = 0.01f});

    reg.add<ParallaxComponent>("Parallax")
        .category("2D")
        .description("Scrolls this entity and its children slower or faster than the world (backgrounds).")
        .field("factor", &ParallaxComponent::factor, {.tooltip = "1 = moves with the world, 0 = stays with the camera", .speed = 0.01f});

    reg.add<ParticleEmitter2DComponent>("ParticleEmitter2D")
        .category("2D")
        .description("Emits and simulates 2D particles (fire, smoke, sparks, magic...).")
        .field("emitting", &ParticleEmitter2DComponent::emitting)
        .field("loop", &ParticleEmitter2DComponent::loop)
        .field("duration", &ParticleEmitter2DComponent::duration, {.rangeMin = 0.05f, .rangeMax = 600.0f, .speed = 0.01f})
        .field("rate", &ParticleEmitter2DComponent::rate, {.rangeMin = 0.0f, .rangeMax = 10000.0f, .speed = 0.5f})
        .field("burst", &ParticleEmitter2DComponent::burst, {.rangeMin = 0, .rangeMax = 100000, .speed = 0.5f})
        .field("maxParticles", &ParticleEmitter2DComponent::maxParticles, {.rangeMin = 1, .rangeMax = 100000, .speed = 1.0f})
        .field("lifetimeMin", &ParticleEmitter2DComponent::lifetimeMin, {.rangeMin = 0.01f, .rangeMax = 60.0f, .speed = 0.01f})
        .field("lifetimeMax", &ParticleEmitter2DComponent::lifetimeMax, {.rangeMin = 0.01f, .rangeMax = 60.0f, .speed = 0.01f})
        .field("speedMin", &ParticleEmitter2DComponent::speedMin, {.speed = 0.01f})
        .field("speedMax", &ParticleEmitter2DComponent::speedMax, {.speed = 0.01f})
        .field("direction", &ParticleEmitter2DComponent::direction, {.flags = FieldFlag_Angle, .speed = 0.01f})
        .field("spread", &ParticleEmitter2DComponent::spread, {.flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = math::kPi, .speed = 0.01f})
        .field("spawnRadius", &ParticleEmitter2DComponent::spawnRadius, {.rangeMin = 0.0f, .rangeMax = 1000.0f, .speed = 0.01f})
        .field("gravity", &ParticleEmitter2DComponent::gravity, {.speed = 0.01f})
        .field("drag", &ParticleEmitter2DComponent::drag, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("startColor", &ParticleEmitter2DComponent::startColor, {.flags = FieldFlag_Color})
        .field("endColor", &ParticleEmitter2DComponent::endColor, {.flags = FieldFlag_Color})
        .field("startSize", &ParticleEmitter2DComponent::startSize, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.005f})
        .field("endSize", &ParticleEmitter2DComponent::endSize, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.005f})
        .field("angularVelocity", &ParticleEmitter2DComponent::angularVelocity, {.flags = FieldFlag_Angle, .speed = 0.01f})
        .field("texture", &ParticleEmitter2DComponent::texture, {.tooltip = "Empty = soft round particle", .flags = FieldFlag_AssetPath})
        .field("additive", &ParticleEmitter2DComponent::additive, {.tooltip = "Additive blending (fire, light, magic)"})
        .field("worldSpace", &ParticleEmitter2DComponent::worldSpace)
        .field("sortingLayer", &ParticleEmitter2DComponent::sortingLayer, {.speed = 1.0f})
        .field("orderInLayer", &ParticleEmitter2DComponent::orderInLayer, {.speed = 1.0f})
        .field("seed", &ParticleEmitter2DComponent::seed, {.tooltip = "0 = random, otherwise reproducible", .speed = 1.0f});
}

} // namespace agr
