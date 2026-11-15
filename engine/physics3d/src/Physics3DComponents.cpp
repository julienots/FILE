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
        .field("isTrigger", &ColliderComponent::isTrigger, {.tooltip = "Detects overlaps without colliding"})
        .field("layer", &ColliderComponent::layer, {.tooltip = "Collision layer (0..31)", .flags = FieldFlag_Layer, .rangeMin = 0.0f, .rangeMax = 31.0f})
        .field("collidesWith", &ColliderComponent::collidesWith, {.tooltip = "Layers this collides with", .flags = FieldFlag_LayerMask});
    reg.add<Joint3DComponent>("Joint3D")
        .category("Physics 3D")
        .description("Joint between this body and another one (or the world): fixed, hinge, distance / spring, slider, ball.")
        .field("type", &Joint3DComponent::type, {.enumEntries = {{"Fixed", 0}, {"Hinge", 1}, {"Distance", 2}, {"Slider", 3}, {"Ball", 4}}})
        .field("connectedBody", &Joint3DComponent::connectedBody, {.tooltip = "Other body (none = a fixed point of the world)"})
        .field("anchor", &Joint3DComponent::anchor, {.tooltip = "Joint point, local to this entity", .speed = 0.01f})
        .field("autoConnectedAnchor", &Joint3DComponent::autoConnectedAnchor, {.tooltip = "The connected anchor is the same world point as the anchor when the joint is created"})
        .field("connectedAnchor", &Joint3DComponent::connectedAnchor, {.tooltip = "Local to the connected body (world point without one)", .speed = 0.01f})
        .field("axis", &Joint3DComponent::axis, {.tooltip = "Hinge / slider axis, local to this entity", .speed = 0.01f})
        .field("length", &Joint3DComponent::length, {.tooltip = "Distance: rest length (0 = distance when created)", .rangeMin = 0.0f, .rangeMax = 10000.0f, .speed = 0.01f})
        .field("springFrequency", &Joint3DComponent::springFrequency, {.tooltip = "Hz, 0 = rigid", .rangeMin = 0.0f, .rangeMax = 120.0f, .speed = 0.05f})
        .field("springDamping", &Joint3DComponent::springDamping, {.tooltip = "Damping ratio (1 = no bounce)", .rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("enableLimits", &Joint3DComponent::enableLimits)
        .field("lowerAngle", &Joint3DComponent::lowerAngle, {.tooltip = "Hinge", .flags = FieldFlag_Angle, .rangeMin = -6.2831853f, .rangeMax = 6.2831853f, .speed = 0.01f})
        .field("upperAngle", &Joint3DComponent::upperAngle, {.tooltip = "Hinge", .flags = FieldFlag_Angle, .rangeMin = -6.2831853f, .rangeMax = 6.2831853f, .speed = 0.01f})
        .field("lowerLimit", &Joint3DComponent::lowerLimit, {.tooltip = "Slider translation / Distance min length (m)", .speed = 0.01f})
        .field("upperLimit", &Joint3DComponent::upperLimit, {.tooltip = "Slider translation / Distance max length (m)", .speed = 0.01f})
        .field("enableMotor", &Joint3DComponent::enableMotor)
        .field("motorSpeed", &Joint3DComponent::motorSpeed, {.tooltip = "Hinge rad/s, slider m/s", .speed = 0.01f})
        .field("maxMotorForce", &Joint3DComponent::maxMotorForce, {.tooltip = "N (N·m for a hinge)", .rangeMin = 0.0f, .rangeMax = 1e9f, .speed = 1.0f})
        .field("breakForce", &Joint3DComponent::breakForce, {.tooltip = "N; 0 = unbreakable", .rangeMin = 0.0f, .rangeMax = 1e9f, .speed = 1.0f})
        .field("collideConnected", &Joint3DComponent::collideConnected, {.tooltip = "The two bodies still collide with each other"})
        .field("broken", &Joint3DComponent::broken, {.tooltip = "The joint broke (untick to rebuild it)", .flags = FieldFlag_Transient});
    reg.add<CharacterController3DComponent>("CharacterController3D")
        .category("Physics 3D")
        .description("Character capsule moved by its velocity: slides along walls, climbs steps and slopes, pushes bodies. Feet at the entity origin.")
        .field("radius", &CharacterController3DComponent::radius, {.rangeMin = 0.05f, .rangeMax = 50.0f, .speed = 0.01f})
        .field("height", &CharacterController3DComponent::height, {.tooltip = "Feet to head", .rangeMin = 0.1f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("maxSlope", &CharacterController3DComponent::maxSlope, {.tooltip = "Steepest walkable slope", .flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = 1.5533430f, .speed = 0.01f})
        .field("stepHeight", &CharacterController3DComponent::stepHeight, {.tooltip = "Steps climbed without jumping", .rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("gravityScale", &CharacterController3DComponent::gravityScale, {.rangeMin = -10.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("mass", &CharacterController3DComponent::mass, {.tooltip = "kg, for pushing bodies", .rangeMin = 0.1f, .rangeMax = 100000.0f, .speed = 0.5f})
        .field("pushForce", &CharacterController3DComponent::pushForce, {.tooltip = "Strongest push on dynamic bodies (N)", .rangeMin = 0.0f, .rangeMax = 1e7f, .speed = 1.0f})
        .field("layer", &CharacterController3DComponent::layer, {.tooltip = "Collision layer (0..31)", .flags = FieldFlag_Layer, .rangeMin = 0.0f, .rangeMax = 31.0f})
        .field("collidesWith", &CharacterController3DComponent::collidesWith, {.tooltip = "Layers this collides with", .flags = FieldFlag_LayerMask});
}

} // namespace agr
