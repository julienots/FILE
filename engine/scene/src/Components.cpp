#include "agr/scene/Components.hpp"

#include "agr/scene/Reflection.hpp"

namespace agr {

void registerSceneComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<TransformComponent>()) return;

    reg.add<NameComponent>("Name")
        .category("Core")
        .userAddable(false)
        .field("name", &NameComponent::name);

    reg.add<StateComponent>("State")
        .category("Core")
        .userAddable(false)
        .field("active", &StateComponent::active, {.tooltip = "Inactive entities and their children are skipped by systems."})
        .field("layer", &StateComponent::layer, {.rangeMin = 0, .rangeMax = 31, .speed = 1})
        .field("hiddenInEditor", &StateComponent::hiddenInEditor, {.flags = FieldFlag_Hidden})
        .field("lockedInEditor", &StateComponent::lockedInEditor, {.flags = FieldFlag_Hidden});

    reg.add<TransformComponent>("Transform")
        .category("Core")
        .userAddable(false)
        .description("Position, rotation and scale relative to the parent entity.")
        .field("position", &TransformComponent::position)
        .field("rotation", &TransformComponent::rotation)
        .field("scale", &TransformComponent::scale, {.speed = 0.01f});

    // Hierarchy / world transform / id are handled by the Scene itself.
}

} // namespace agr
