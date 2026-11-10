#include "agr/physics3d/Physics3DComponents.hpp"

#include "agr/scene/Reflection.hpp"

namespace agr {

void registerPhysics3DComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<RigidBodyComponent>()) return;
    reg.add<RigidBodyComponent>("RigidBody")
        .category("Physics 3D")
        .description("3D physics body (Jolt): moved by gravity, forces and collisions.")
        .field("type", &RigidBodyComponent::type, {.enumEntries = {{"Static", 0}, {"Kinematic", 1}, {"Dynamic", 2}}})
        .field("mass", &RigidBodyComponent::mass, {.tooltip = "kg; 0 = from the colliders' density", .rangeMin = 0.0f, .rangeMax = 1e6f, .speed = 0.1f})
        .field("linearDamping", &RigidBodyComponent::linearDamping, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("angularDamping", &RigidBodyComponent::angularDamping, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("gravityScale", &RigidBodyComponent::gravityScale, {.rangeMin = -10.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("freezeRotation", &RigidBodyComponent::freezeRotation, {.tooltip = "Translation only (characters)"})
        .field("bullet", &RigidBodyComponent::bullet, {.tooltip = "Continuous collision for fast bodies"})
        .field("initialVelocity", &RigidBodyComponent::initialVelocity, {.speed = 0.05f});
    reg.add<ColliderComponent>("Collider")
        .category("Physics 3D")
        .description("3D collision shape (box, sphere, capsule). Static if no RigidBody on the entity or its parents.")
        .field("shape", &ColliderComponent::shape, {.enumEntries = {{"Box", 0}, {"Sphere", 1}, {"Capsule", 2}}})
        .field("size", &ColliderComponent::size, {.tooltip = "Box size (before scale)", .speed = 0.01f})
        .field("radius", &ColliderComponent::radius, {.rangeMin = 0.001f, .rangeMax = 1000.0f, .speed = 0.01f})
        .field("height", &ColliderComponent::height, {.tooltip = "Capsule total height", .rangeMin = 0.001f, .rangeMax = 1000.0f, .speed = 0.01f})
        .field("offset", &ColliderComponent::offset, {.speed = 0.01f})
        .field("density", &ColliderComponent::density, {.rangeMin = 0.001f, .rangeMax = 100000.0f, .speed = 1.0f})
        .field("friction", &ColliderComponent::friction, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("restitution", &ColliderComponent::restitution, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("isTrigger", &ColliderComponent::isTrigger, {.tooltip = "Detects overlaps without colliding"});
}

} // namespace agr
