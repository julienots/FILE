#include "agr/physics2d/Physics2DComponents.hpp"

#include "agr/scene/Reflection.hpp"

namespace agr {

void registerPhysics2DComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<RigidBody2DComponent>()) return;
    reg.add<RigidBody2DComponent>("RigidBody2D")
        .category("Physics 2D")
        .description("2D physics body (Box2D): moved by gravity, forces and collisions.")
        .field("type", &RigidBody2DComponent::type, {.enumEntries = {{"Static", 0}, {"Kinematic", 1}, {"Dynamic", 2}}})
        .field("gravityScale", &RigidBody2DComponent::gravityScale, {.rangeMin = -10.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("linearDamping", &RigidBody2DComponent::linearDamping, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("angularDamping", &RigidBody2DComponent::angularDamping, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("fixedRotation", &RigidBody2DComponent::fixedRotation, {.tooltip = "Never rotates (characters)"})
        .field("bullet", &RigidBody2DComponent::bullet, {.tooltip = "Continuous collision for fast bodies"})
        .field("initialVelocity", &RigidBody2DComponent::initialVelocity, {.speed = 0.05f});
    reg.add<Collider2DComponent>("Collider2D")
        .category("Physics 2D")
        .description("Collision shape (box, circle, capsule). Static if no RigidBody2D on the entity or its parents.")
        .field("shape", &Collider2DComponent::shape, {.enumEntries = {{"Box", 0}, {"Circle", 1}, {"Capsule", 2}}})
        .field("size", &Collider2DComponent::size, {.tooltip = "Box / capsule size (world units, before scale)", .speed = 0.01f})
        .field("radius", &Collider2DComponent::radius, {.tooltip = "Circle radius", .rangeMin = 0.001f, .rangeMax = 1000.0f, .speed = 0.01f})
        .field("offset", &Collider2DComponent::offset, {.speed = 0.01f})
        .field("density", &Collider2DComponent::density, {.rangeMin = 0.0f, .rangeMax = 10000.0f, .speed = 0.01f})
        .field("friction", &Collider2DComponent::friction, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("restitution", &Collider2DComponent::restitution, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("isTrigger", &Collider2DComponent::isTrigger, {.tooltip = "Detects overlaps without colliding"});
    reg.add<TilemapCollider2DComponent>("TilemapCollider2D")
        .category("Physics 2D")
        .description("Static collision for the painted cells of the Tilemap.")
        .field("friction", &TilemapCollider2DComponent::friction, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("restitution", &TilemapCollider2DComponent::restitution, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f});
}

} // namespace agr
