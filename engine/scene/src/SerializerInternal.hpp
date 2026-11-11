#pragma once
// Internal building blocks shared by the scene serializer and prefabs (not a public API).
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "agr/scene/Reflection.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::detail {

using json = nlohmann::ordered_json;

struct PendingRef {
    ecs::Entity owner;
    ecs::ComponentTypeId component;
    const FieldInfo* field;
    std::string target;  ///< File id or UUID string.
};

struct LoadContext {
    Scene& scene;
    bool remap = false;          ///< Snippet pasted: ids not found in the snippet resolve to nothing.
    bool strictParents = true;   ///< A missing parent is an error (scene files) or attaches to the root parent.
    int depth = 0;               ///< Prefab nesting depth (cycle guard).
    std::unordered_map<std::string, ecs::Entity> byFileId;
    std::vector<PendingRef> refs;
    ecs::Entity lookup(const std::string& key) const;
};

/// Maps a file id to the UUID to give the created entity (null = fresh).
using IdFn = std::function<Uuid(const std::string& fileId)>;

json componentsToJson(const Scene& scene, ecs::Entity e);
/// Appends `e` and its subtree. Prefab instances are written as references with their overrides,
/// except the root itself when `plainRoot`.
void collectSubtree(const Scene& scene, ecs::Entity e, json& out, bool includeParent, bool plainRoot);
bool readField(const FieldInfo& f, void* comp, const json& j, ecs::Entity owner, ecs::ComponentTypeId cid, std::vector<PendingRef>& refs);
/// Adds (or updates) a component from its JSON fields; references go to ctx.refs.
bool readComponent(LoadContext& ctx, ecs::Entity e, const std::string& compName, const json& fields);
Status createEntries(LoadContext& ctx, const json& entities, ecs::Entity rootParent, const IdFn& idFor, ecs::Entity* firstCreated);
void resolveRefs(LoadContext& ctx);

// Implemented in Prefab.cpp.
bool writeInstanceEntry(const Scene& scene, ecs::Entity e, json& entry);
Status loadInstanceEntry(LoadContext& ctx, const json& item, ecs::Entity parent, Uuid actualId, ecs::Entity* created);

} // namespace agr::detail
