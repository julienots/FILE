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
        .description("Collision shape (box, circle, capsule, polygon, chain). Static if no RigidBody2D on the entity or its parents.")
        .field("shape", &Collider2DComponent::shape, {.enumEntries = {{"Box", 0}, {"Circle", 1}, {"Capsule", 2}, {"Polygon", 3}, {"Chain", 4}}})
        .field("size", &Collider2DComponent::size, {.tooltip = "Box / capsule size (world units, before scale)", .speed = 0.01f})
        .field("radius", &Collider2DComponent::radius, {.tooltip = "Circle radius", .rangeMin = 0.001f, .rangeMax = 1000.0f, .speed = 0.01f})
        .field("offset", &Collider2DComponent::offset, {.speed = 0.01f})
        .field("density", &Collider2DComponent::density, {.rangeMin = 0.0f, .rangeMax = 10000.0f, .speed = 0.01f})
        .field("friction", &Collider2DComponent::friction, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("restitution", &Collider2DComponent::restitution, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("isTrigger", &Collider2DComponent::isTrigger, {.tooltip = "Detects overlaps without colliding"})
        .field("layer", &Collider2DComponent::layer, {.tooltip = "Collision layer (0..31)", .flags = FieldFlag_Layer, .rangeMin = 0.0f, .rangeMax = 31.0f})
        .field("collidesWith", &Collider2DComponent::collidesWith, {.tooltip = "Layers this collides with", .flags = FieldFlag_LayerMask})
        .field("points", &Collider2DComponent::points, {.tooltip = "Polygon / Chain vertices (local units)", .speed = 0.01f})
        .field("loop", &Collider2DComponent::loop, {.tooltip = "Chain: closes the outline"})
        .field("oneWay", &Collider2DComponent::oneWay, {.tooltip = "Solid only from above (local +Y): jump through it from below"});
    reg.add<CharacterController2DComponent>("CharacterController2D")
        .category("Physics 2D")
        .description("Character moved by its velocity: slides on walls and slopes, stands on floors, follows moving platforms (move and slide).")
        .field("size", &CharacterController2DComponent::size, {.tooltip = "Capsule width x height", .speed = 0.01f})
        .field("offset", &CharacterController2DComponent::offset, {.speed = 0.01f})
        .field("velocity", &CharacterController2DComponent::velocity, {.tooltip = "m/s, set by scripts; walls and floors remove the part going into them", .speed = 0.05f})
        .field("gravityScale", &CharacterController2DComponent::gravityScale, {.rangeMin = -10.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("maxSlope", &CharacterController2DComponent::maxSlope, {.tooltip = "Steepest floor", .flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = 1.55f})
        .field("snapDistance", &CharacterController2DComponent::snapDistance, {.tooltip = "Stays on the floor going down slopes", .rangeMin = 0.0f, .rangeMax = 2.0f, .speed = 0.01f})
        .field("pushForce", &CharacterController2DComponent::pushForce, {.tooltip = "> 0: pushes dynamic bodies; 0: they block", .rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("layer", &CharacterController2DComponent::layer, {.flags = FieldFlag_Layer, .rangeMin = 0.0f, .rangeMax = 31.0f})
        .field("collidesWith", &CharacterController2DComponent::collidesWith, {.flags = FieldFlag_LayerMask})
        .field("onFloor", &CharacterController2DComponent::onFloor, {.flags = FieldFlag_ReadOnly | FieldFlag_Transient})
        .field("onWall", &CharacterController2DComponent::onWall, {.flags = FieldFlag_ReadOnly | FieldFlag_Transient})
        .field("onCeiling", &CharacterController2DComponent::onCeiling, {.flags = FieldFlag_ReadOnly | FieldFlag_Transient})
        .field("floorNormal", &CharacterController2DComponent::floorNormal, {.flags = FieldFlag_ReadOnly | FieldFlag_Transient});
    reg.add<TilemapCollider2DComponent>("TilemapCollider2D")
        .category("Physics 2D")
        .description("Static collision for the painted cells of the Tilemap.")
        .field("oneWayTiles", &TilemapCollider2DComponent::oneWayTiles, {.tooltip = "Tiles that are one-way platforms (frame indices: \"12,13\")"})
        .field("slopeTiles", &TilemapCollider2DComponent::slopeTiles, {.tooltip = "Slope tiles: \"20:/,21:\\\" (/ rises to the right)"})
        .field("emptyTiles", &TilemapCollider2DComponent::emptyTiles, {.tooltip = "Tiles with no collision (decor): \"4,5,9\""})
        .field("triggerTiles", &TilemapCollider2DComponent::triggerTiles, {.tooltip = "Tiles that are triggers (spikes, water, ladders): \"30,31\" -> onTrigger"})
        .field("shapeTiles", &TilemapCollider2DComponent::shapeTiles, {.tooltip = "Own box per tile, cell fractions from bottom-left: \"7:0,0,1,0.5; 8:0.4,0,0.2,1\""})
        .field("friction", &TilemapCollider2DComponent::friction, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("restitution", &TilemapCollider2DComponent::restitution, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("layer", &TilemapCollider2DComponent::layer, {.tooltip = "Collision layer (0..31)", .flags = FieldFlag_Layer, .rangeMin = 0.0f, .rangeMax = 31.0f})
        .field("collidesWith", &TilemapCollider2DComponent::collidesWith, {.tooltip = "Layers this collides with", .flags = FieldFlag_LayerMask});
    reg.add<Joint2DComponent>("Joint2D")
        .category("Physics 2D")
        .description("Joint between this body and another one (or the world): fixed, hinge, distance / spring, slider.")
        .field("type", &Joint2DComponent::type, {.enumEntries = {{"Fixed", 0}, {"Hinge", 1}, {"Distance", 2}, {"Slider", 3}}})
        .field("connectedBody", &Joint2DComponent::connectedBody, {.tooltip = "Other body (none = a fixed point of the world)"})
        .field("anchor", &Joint2DComponent::anchor, {.tooltip = "Joint point, local to this entity", .speed = 0.01f})
        .field("autoConnectedAnchor", &Joint2DComponent::autoConnectedAnchor, {.tooltip = "The connected anchor is the same world point as the anchor when the joint is created"})
        .field("connectedAnchor", &Joint2DComponent::connectedAnchor, {.tooltip = "Local to the connected body (world point without one)", .speed = 0.01f})
        .field("axis", &Joint2DComponent::axis, {.tooltip = "Slider direction, local to this entity", .speed = 0.01f})
        .field("length", &Joint2DComponent::length, {.tooltip = "Distance: rest length (0 = distance when created)", .rangeMin = 0.0f, .rangeMax = 10000.0f, .speed = 0.01f})
        .field("springFrequency", &Joint2DComponent::springFrequency, {.tooltip = "Hz, 0 = rigid", .rangeMin = 0.0f, .rangeMax = 120.0f, .speed = 0.05f})
        .field("springDamping", &Joint2DComponent::springDamping, {.tooltip = "Damping ratio (1 = no bounce)", .rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("enableLimits", &Joint2DComponent::enableLimits)
        .field("lowerAngle", &Joint2DComponent::lowerAngle, {.tooltip = "Hinge", .flags = FieldFlag_Angle, .rangeMin = -6.2831853f, .rangeMax = 6.2831853f, .speed = 0.01f})
        .field("upperAngle", &Joint2DComponent::upperAngle, {.tooltip = "Hinge", .flags = FieldFlag_Angle, .rangeMin = -6.2831853f, .rangeMax = 6.2831853f, .speed = 0.01f})
        .field("lowerLimit", &Joint2DComponent::lowerLimit, {.tooltip = "Slider translation / Distance min length (m)", .speed = 0.01f})
        .field("upperLimit", &Joint2DComponent::upperLimit, {.tooltip = "Slider translation / Distance max length (m)", .speed = 0.01f})
        .field("enableMotor", &Joint2DComponent::enableMotor)
        .field("motorSpeed", &Joint2DComponent::motorSpeed, {.tooltip = "Hinge rad/s, slider m/s", .speed = 0.01f})
        .field("maxMotorForce", &Joint2DComponent::maxMotorForce, {.tooltip = "N (N·m for a hinge)", .rangeMin = 0.0f, .rangeMax = 1e9f, .speed = 1.0f})
        .field("breakForce", &Joint2DComponent::breakForce, {.tooltip = "N; 0 = unbreakable", .rangeMin = 0.0f, .rangeMax = 1e9f, .speed = 1.0f})
        .field("collideConnected", &Joint2DComponent::collideConnected, {.tooltip = "The two bodies still collide with each other"})
        .field("broken", &Joint2DComponent::broken, {.tooltip = "The joint broke (untick to rebuild it)", .flags = FieldFlag_Transient});
}

} // namespace agr
