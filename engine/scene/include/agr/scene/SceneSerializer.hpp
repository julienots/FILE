#pragma once
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

    /// Binary scene format: the same content as the JSON format (components and fields
    /// by reflected name, entity references as UUIDs, prefab instances as references
    /// with overrides), written straight from the reflection data without a JSON tree.
    /// Many times faster to write and read: used for editor snapshots (undo, play
    /// mode) and for exported games. Unknown components and fields are skipped, so
    /// files stay readable across engine versions.
    static std::vector<u8> toBinary(const Scene& scene);
    static Status fromBinary(Scene& scene, std::span<const u8> data);
    /// True when `bytes` starts with the binary scene signature.
    static bool isBinary(std::string_view bytes);
    /// Loads either format, detected from the first bytes.
    static Status fromBytes(Scene& scene, std::string_view data);
};

} // namespace agr
