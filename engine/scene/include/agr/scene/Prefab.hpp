#pragma once
// Prefabs: reusable entity hierarchies saved as .agrprefab files.
//
// An instance in a scene is stored as a reference to its prefab plus its differences
// (changed fields, added / removed components and entities). Loading expands the prefab
// and re-applies the differences, so editing a prefab updates every instance while keeping
// what each instance changed. Entities inside an instance get ids derived from the instance
// root id: references to them stay valid across saves, loads and prefab edits.
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Marks the root of a prefab instance. Added and removed by the prefab functions.
struct PrefabInstanceComponent {
    std::string prefab;      ///< URI of the .agrprefab file.
    std::string unresolved;  ///< Saved entry kept verbatim while the prefab file is missing (no data loss).
};
void registerPrefabComponents();

/// Returns the content of a prefab (null if unknown or invalid). Set by the engine / editor;
/// used whenever scenes are loaded or saved.
using PrefabResolver = std::function<const nlohmann::ordered_json*(const std::string& uri)>;
void setPrefabResolver(PrefabResolver resolver);
const nlohmann::ordered_json* resolvePrefab(const std::string& uri);

/// Prefab files read through the virtual file system, cached, reloaded when modified.
class PrefabLibrary {
public:
    explicit PrefabLibrary(VirtualFileSystem& vfs) : m_vfs(vfs) {}
    /// Content of a prefab, loaded on first use. Null if missing or invalid (error logged once).
    const nlohmann::ordered_json* get(const std::string& uri);
    /// Writes a prefab; returns the previous content (null JSON if new) so instances can be refreshed.
    Result<nlohmann::ordered_json> save(const std::string& uri, const nlohmann::ordered_json& content);
    /// Prefabs modified on disk since loaded: reloaded; returns (uri, previous content) pairs.
    std::vector<std::pair<std::string, nlohmann::ordered_json>> reloadChanged();
    void clear() { m_entries.clear(); }
    /// Makes this library the global resolver used by the serializer.
    void install();

private:
    struct Entry {
        nlohmann::ordered_json content;
        bool valid = false;
        i64 modified = 0;
    };
    Entry load(const std::string& uri);
    VirtualFileSystem& m_vfs;
    std::unordered_map<std::string, Entry> m_entries;
};

namespace prefab {

constexpr const char* kExtension = ".agrprefab";

/// Creates an instance of a prefab (through the resolver) under `parent`.
Result<ecs::Entity> instantiate(Scene& scene, const std::string& uri, ecs::Entity parent = {});
/// File content for a prefab made from `root` and its subtree (root written in full; instances
/// inside stay instances, which makes nested prefabs).
nlohmann::ordered_json fromEntity(const Scene& scene, ecs::Entity root);
/// After saving fromEntity(root) as `uri`: turns `root` into an instance of it (ids of the
/// subtree become instance ids; components and references are kept).
Status link(Scene& scene, ecs::Entity root, const std::string& uri);
/// Content to write back to the instance's prefab (Apply): keeps the prefab's entity ids so
/// other instances keep their changes.
Result<nlohmann::ordered_json> applyContent(const Scene& scene, ecs::Entity instanceRoot);
/// Differences of an instance from its prefab (empty object: none; the root position always differs).
nlohmann::ordered_json overrides(const Scene& scene, ecs::Entity instanceRoot);
/// Number of changed fields, components and entities (root Transform and Name excluded).
u32 overrideCount(const Scene& scene, ecs::Entity instanceRoot);

bool isInstanceRoot(const Scene& scene, ecs::Entity e);
/// The nearest instance root containing `e` (itself included), null if none.
ecs::Entity instanceRootOf(const Scene& scene, ecs::Entity e);

/// Rebuilds an instance from the current prefab content, keeping its differences from
/// `previous` (the content it was made from). Keeps the root id, its place in the hierarchy
/// and references from outside the instance.
Status refresh(Scene& scene, ecs::Entity instanceRoot, const nlohmann::ordered_json& previous);
/// refresh() for every instance of `uri` in the scene except `except`; returns how many.
u32 refreshAll(Scene& scene, const std::string& uri, const nlohmann::ordered_json& previous, ecs::Entity except = {});
/// Drops the differences, keeping only the root transform and name.
Status revert(Scene& scene, ecs::Entity instanceRoot);
/// Breaks the link: the entities become ordinary ones.
void unpack(Scene& scene, ecs::Entity instanceRoot);

} // namespace prefab
} // namespace agr
