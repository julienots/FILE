#pragma once
#include <string>

#include <nlohmann/json_fwd.hpp>

#include "agr/core/Result.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Text scene format (.agrscene, JSON). Human readable and merge friendly:
/// entities are written parents-first in hierarchy order, components by their
/// reflected name, entity references as persistent UUIDs.
class SceneSerializer {
public:
    static constexpr int kFormatVersion = 1;

    static nlohmann::ordered_json toJson(const Scene& scene);
    static std::string toString(const Scene& scene, int indent = 2);

    /// Replaces the content of `scene`. Unknown components are skipped with a warning.
    static Status fromJson(Scene& scene, const nlohmann::ordered_json& json);
    static Status fromString(Scene& scene, std::string_view text);

    /// Serializes one subtree (used by prefabs and copy / paste).
    static nlohmann::ordered_json entityToJson(const Scene& scene, ecs::Entity root);
    /// Instantiates a subtree produced by entityToJson with fresh UUIDs.
    static Result<ecs::Entity> instantiate(Scene& scene, const nlohmann::ordered_json& json, ecs::Entity parent = {});
};

} // namespace agr
