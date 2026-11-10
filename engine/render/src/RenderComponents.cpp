#include "agr/render/RenderComponents.hpp"

#include "agr/scene/Reflection.hpp"

namespace agr {

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
        .field("primary", &CameraComponent::primary);

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
        .field("visible", &SpriteRendererComponent::visible);
}

} // namespace agr
