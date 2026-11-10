#include "agr/script/ScriptComponent.hpp"

#include "agr/scene/Reflection.hpp"

namespace agr {

void registerScriptComponent() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<ScriptComponent>()) return;
    reg.add<ScriptComponent>("Script")
        .category("Scripting")
        .description("Lua script: onStart, onUpdate, onFixedUpdate, onCollision, onTrigger, onDestroy.")
        .field("script", &ScriptComponent::script, {.tooltip = "Lua file, e.g. assets://scripts/player.lua", .flags = FieldFlag_AssetPath})
        .field("properties", &ScriptComponent::properties, {.tooltip = "Values of the script's properties (JSON)", .flags = FieldFlag_Hidden})
        .field("enabled", &ScriptComponent::enabled);
}

} // namespace agr
