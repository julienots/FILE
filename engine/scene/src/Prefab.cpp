#include "agr/scene/Prefab.hpp"

#include <algorithm>
#include <unordered_set>

#include "SerializerInternal.hpp"
#include "agr/core/Log.hpp"
#include "agr/scene/Components.hpp"

namespace agr {

using detail::json;
using ecs::Entity;

namespace {

PrefabResolver g_resolver;
constexpr int kMaxDepth = 16;
thread_local std::vector<std::string> g_expanding;  // prefabs being expanded (cycle detection)

/// Id of an entity inside an instance: a hash of the instance root id and the entity's id in
/// the prefab file (stable across loads, unique per instance).
Uuid deriveId(Uuid root, std::string_view fileId) {
    u64 h = root.value() ^ 0x9e3779b97f4a7c15ull;
    for (char c : fileId) {
        h ^= static_cast<u8>(c);
        h *= 0x100000001b3ull;
    }
    h += 0x9e3779b97f4a7c15ull;
    h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ull;
    h = (h ^ (h >> 27)) * 0x94d049bb133111ebull;
    h ^= h >> 31;
    return Uuid(h ? h : 1);
}

bool validPrefab(const json* j) {
    return j && j->is_object() && j->value("format", "") == "agr.prefab" && j->contains("entities") && (*j)["entities"].is_array() &&
           !(*j)["entities"].empty() && (*j)["entities"][0].is_object() && (*j)["entities"][0].contains("id") &&
           (*j)["entities"][0]["id"].is_string();
}

void preorder(const Scene& scene, Entity e, std::vector<Entity>& out) {
    out.push_back(e);
    for (Entity c : scene.children(e)) preorder(scene, c, out);
}

/// Expands a prefab (no overrides) with the given root id; nested instances included.
Result<Entity> expand(Scene& scene, const json& base, Entity parent, Uuid rootId, int depth, const std::string& uri) {
    if (depth > kMaxDepth) return makeError(Error::Code::InvalidArgument, "prefab '" + uri + "' nests too deeply");
    if (std::find(g_expanding.begin(), g_expanding.end(), uri) != g_expanding.end())
        return makeError(Error::Code::InvalidArgument, "prefab '" + uri + "' contains an instance of itself");
    g_expanding.push_back(uri);
    struct Pop {
        ~Pop() { g_expanding.pop_back(); }
    } pop;
    detail::LoadContext local{scene};
    local.depth = depth;
    const std::string firstId = base["entities"][0]["id"].get<std::string>();
    Entity root{};
    Status s = detail::createEntries(local, base["entities"], parent,
                                     [&](const std::string& fid) { return fid == firstId ? rootId : deriveId(rootId, fid); }, &root);
    detail::resolveRefs(local);
    if (!s) {
        if (scene.valid(root)) scene.destroyEntity(root);
        return s.error();
    }
    if (root.isNull()) return makeError(Error::Code::ParseError, "prefab '" + uri + "' is empty");
    auto& reg = scene.registry();
    if (auto* pic = reg.tryGet<PrefabInstanceComponent>(root)) *pic = PrefabInstanceComponent{uri, {}};
    else reg.emplace<PrefabInstanceComponent>(root, PrefabInstanceComponent{uri, {}});
    return root;
}

/// Pairs the entities of two expansions of `base` (ids made from `from` and from `to`).
std::vector<std::pair<Uuid, Uuid>> idPairs(const json& base, const std::string& uri, Uuid from, Uuid to, int depth) {
    Scene a, b;
    auto ra = expand(a, base, {}, from, depth, uri);
    auto rb = expand(b, base, {}, to, depth, uri);
    std::vector<std::pair<Uuid, Uuid>> out;
    if (!ra || !rb) return out;
    std::vector<Entity> la, lb;
    preorder(a, ra.value(), la);
    preorder(b, rb.value(), lb);
    if (la.size() != lb.size()) return out;
    for (usize i = 0; i < la.size(); ++i) out.emplace_back(a.uuidOf(la[i]), b.uuidOf(lb[i]));
    return out;
}

/// Replaces every string found in `table` (ids and references), recursively.
json translate(const json& j, const std::unordered_map<std::string, std::string>& table) {
    if (j.is_string()) {
        auto it = table.find(j.get<std::string>());
        return it == table.end() ? j : json(it->second);
    }
    if (j.is_array()) {
        json out = json::array();
        for (const auto& x : j) out.push_back(translate(x, table));
        return out;
    }
    if (j.is_object()) {
        json out = json::object();
        for (const auto& [k, v] : j.items()) {
            auto it = table.find(k);
            out[it == table.end() ? k : it->second] = translate(v, table);
        }
        return out;
    }
    return j;
}

json computeOverrides(const Scene& scene, Entity root, const json& base, const std::string& uri) {
    Scene tmp;
    auto troot = expand(tmp, base, {}, scene.uuidOf(root), 1, uri);
    if (!troot) return nullptr;
    std::unordered_map<std::string, Entity> expanded;
    std::vector<Entity> baseOrder;
    preorder(tmp, troot.value(), baseOrder);
    for (Entity e : baseOrder) expanded[tmp.uuidOf(e).toString()] = e;

    json ov = json::object();
    std::unordered_set<std::string> present;
    std::function<void(Entity)> walk = [&](Entity e) {
        const std::string key = scene.uuidOf(e).toString();
        auto it = expanded.find(key);
        if (it == expanded.end()) {  // added to this instance (with its subtree)
            if (!ov.contains("added")) ov["added"] = json::array();
            detail::collectSubtree(scene, e, ov["added"], true, false);
            return;
        }
        present.insert(key);
        const json mine = detail::componentsToJson(scene, e);
        const json theirs = detail::componentsToJson(tmp, it->second);
        for (const auto& [name, fields] : mine.items()) {
            if (!theirs.contains(name)) {
                ov["components"][key][name] = fields;
                continue;
            }
            for (const auto& [f, v] : fields.items())
                if (!theirs[name].contains(f) || theirs[name][f] != v) ov["components"][key][name][f] = v;
        }
        for (const auto& [name, fields] : theirs.items()) {
            (void)fields;
            if (!mine.contains(name)) ov["removedComponents"][key].push_back(name);
        }
        for (Entity c : scene.children(e)) walk(c);
    };
    walk(root);
    for (Entity e : baseOrder) {
        const std::string key = tmp.uuidOf(e).toString();
        if (present.contains(key)) continue;
        const Entity p = tmp.parent(e);
        if (!p.isNull() && present.contains(tmp.uuidOf(p).toString())) ov["removed"].push_back(key);  // top-most removed only
    }
    return ov;
}

/// Applies differences to a freshly expanded instance. `lookup` already knows the ids of the
/// expansion; added entities get `deriveRoot`-derived ids when the instance was re-identified.
void applyOverrides(Scene& scene, Entity root, const json& ov, detail::LoadContext& lc, Uuid deriveRoot) {
    if (!ov.is_object()) return;
    auto inside = [&](Entity e) { return scene.valid(e) && (e == root || scene.isAncestorOf(root, e)); };
    if (ov.contains("removed") && ov["removed"].is_array())
        for (const auto& id : ov["removed"])
            if (id.is_string())
                if (Entity e = lc.lookup(id.get<std::string>()); inside(e) && e != root) scene.destroyEntity(e);
    if (ov.contains("added") && ov["added"].is_array()) {
        detail::IdFn idFor = deriveRoot.isNull() ? detail::IdFn([](const std::string& id) { return Uuid::fromString(id); })
                                                 : detail::IdFn([&](const std::string& id) { return deriveId(deriveRoot, id); });
        const bool strict = lc.strictParents;
        lc.strictParents = false;
        if (Status s = detail::createEntries(lc, ov["added"], root, idFor, nullptr); !s)
            AGR_LOG_WARN("Prefab", "added entities of an instance could not be restored: {}", s.error().message);
        lc.strictParents = strict;
    }
    auto& reg = scene.registry();
    if (ov.contains("removedComponents") && ov["removedComponents"].is_object())
        for (const auto& [id, names] : ov["removedComponents"].items()) {
            const Entity e = lc.lookup(id);
            if (!inside(e) || !names.is_array()) continue;
            for (const auto& n : names) {
                if (!n.is_string()) continue;
                const std::string name = n.get<std::string>();
                if (name == "Name" || name == "Transform" || name == "State") continue;
                if (const ComponentInfo* info = ComponentRegistry::instance().find(name))
                    if (ecs::PoolBase* pool = reg.findPool(info->id); pool && pool->contains(e)) pool->remove(reg, e);
            }
        }
    if (ov.contains("components") && ov["components"].is_object())
        for (const auto& [id, comps] : ov["components"].items()) {
            const Entity e = lc.lookup(id);
            if (!inside(e) || !comps.is_object()) continue;
            for (const auto& [name, fields] : comps.items()) detail::readComponent(lc, e, name, fields);
        }
}

Status rebuild(Scene& scene, Entity root, const json& ov) {
    auto& reg = scene.registry();
    const auto* pic = reg.tryGet<PrefabInstanceComponent>(root);
    if (!pic) return makeError(Error::Code::InvalidArgument, "not a prefab instance");
    const std::string uri = pic->prefab;
    const json* base = resolvePrefab(uri);
    if (!validPrefab(base)) return makeError(Error::Code::NotFound, "prefab '" + uri + "' is missing or invalid");
    // Place and outside references to restore after the rebuild.
    const Entity parent = scene.parent(root);
    Entity next{};
    {
        const std::vector<Entity> siblings = parent.isNull() ? scene.roots() : scene.children(parent);
        for (usize i = 0; i + 1 < siblings.size(); ++i)
            if (siblings[i] == root) next = siblings[i + 1];
    }
    std::vector<Entity> subtree;
    preorder(scene, root, subtree);
    std::unordered_set<u64> inSubtree;
    for (Entity e : subtree) inSubtree.insert(e.packed());
    struct Repair {
        Entity owner;
        ecs::ComponentTypeId component;
        const FieldInfo* field;
        Uuid target;
    };
    std::vector<Repair> repairs;
    for (const ComponentInfo* info : ComponentRegistry::instance().all()) {
        ecs::PoolBase* pool = reg.findPool(info->id);
        if (!pool) continue;
        for (const FieldInfo& f : info->fields) {
            if (f.type != FieldType::Entity) continue;
            for (Entity owner : pool->entities()) {
                if (inSubtree.contains(owner.packed())) continue;
                const Entity target = f.ref<Entity>(pool->getRaw(owner));
                if (inSubtree.contains(target.packed())) repairs.push_back({owner, info->id, &f, scene.uuidOf(target)});
            }
        }
    }
    const Uuid id = scene.uuidOf(root);
    scene.destroyEntity(root);
    json item = {{"id", id.toString()}, {"prefab", uri}};
    if (!ov.is_null() && !ov.empty()) item["overrides"] = ov;
    detail::LoadContext ctx{scene};
    Entity created{};
    Status s = detail::loadInstanceEntry(ctx, item, parent, id, &created);
    detail::resolveRefs(ctx);
    if (!s) return s;
    if (scene.valid(created) && !next.isNull()) scene.setParent(created, parent, next, false);
    for (const Repair& r : repairs)
        if (ecs::PoolBase* pool = reg.findPool(r.component); pool && pool->contains(r.owner))
            r.field->ref<Entity>(pool->getRaw(r.owner)) = scene.findByUuid(r.target);
    return {};
}

} // namespace

// ------------------------------------------------------------------ serializer hooks
namespace detail {

bool writeInstanceEntry(const Scene& scene, Entity e, json& entry) {
    const auto* pic = const_cast<Scene&>(scene).registry().tryGet<PrefabInstanceComponent>(e);
    if (!pic) return false;
    if (!pic->unresolved.empty()) {  // prefab missing when loaded: keep what the file said
        json saved = json::parse(pic->unresolved, nullptr, false);
        entry["prefab"] = pic->prefab;
        if (saved.is_object() && saved.contains("overrides")) entry["overrides"] = saved["overrides"];
        return true;
    }
    const json* base = resolvePrefab(pic->prefab);
    if (!validPrefab(base)) return false;  // written in full: nothing is lost
    json ov = computeOverrides(scene, e, *base, pic->prefab);
    if (ov.is_null()) return false;
    entry["prefab"] = pic->prefab;
    if (!ov.empty()) entry["overrides"] = std::move(ov);
    return true;
}

Status loadInstanceEntry(LoadContext& ctx, const json& item, Entity parent, Uuid actualId, Entity* created) {
    const std::string uri = item["prefab"].get<std::string>();
    Uuid original = Uuid::fromString(item["id"].get<std::string>());
    if (original.isNull()) original = actualId;
    const json* base = resolvePrefab(uri);
    const json ov = item.contains("overrides") ? item["overrides"] : json::object();
    Result<Entity> root = makeError(Error::Code::NotFound, "prefab '" + uri + "' not found or invalid");
    if (validPrefab(base)) root = expand(ctx.scene, *base, parent, actualId, ctx.depth + 1, uri);
    if (!root) {
        // Placeholder keeping the saved data until the prefab is available again.
        AGR_LOG_WARN("Prefab", "{}: placeholder kept", root.error().message);
        const Entity e = ctx.scene.createEntity("Prefab introuvable", parent, actualId);
        const std::string key = original.toString();
        if (ov.contains("components") && ov["components"].contains(key))
            for (const char* name : {"Name", "Transform"})
                if (ov["components"][key].contains(name)) readComponent(ctx, e, name, ov["components"][key][name]);
        ctx.scene.registry().emplace<PrefabInstanceComponent>(e, PrefabInstanceComponent{uri, item.dump()});
        *created = e;
        return {};
    }
    LoadContext lc{ctx.scene};
    lc.depth = ctx.depth + 1;
    Uuid deriveRoot{};
    if (original != actualId) {
        // Re-identified (nested in a prefab, pasted, duplicated): ids written relative to the
        // original root map to this instance's ids.
        for (const auto& [from, to] : idPairs(*base, uri, original, actualId, ctx.depth + 1)) {
            const Entity e = ctx.scene.findByUuid(to);
            lc.byFileId[from.toString()] = e;
            ctx.byFileId[from.toString()] = e;  // references from the enclosing file
        }
        deriveRoot = actualId;
    }
    applyOverrides(ctx.scene, root.value(), ov, lc, deriveRoot);
    resolveRefs(lc);
    if (!deriveRoot.isNull())
        for (const auto& [k, v] : lc.byFileId) ctx.byFileId.emplace(k, v);  // added entities too
    *created = root.value();
    return {};
}

} // namespace detail

// ------------------------------------------------------------------ components / resolver
void registerPrefabComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<PrefabInstanceComponent>()) return;
    reg.add<PrefabInstanceComponent>("PrefabInstance")
        .category("Core")
        .description("Root of a prefab instance (see the Prefab section of the inspector).")
        .userAddable(false)
        .serializable(false)
        .field("prefab", &PrefabInstanceComponent::prefab, {.flags = FieldFlag_ReadOnly | FieldFlag_AssetPath})
        .field("unresolved", &PrefabInstanceComponent::unresolved, {.flags = FieldFlag_Hidden | FieldFlag_Transient});
}

void setPrefabResolver(PrefabResolver resolver) { g_resolver = std::move(resolver); }
const json* resolvePrefab(const std::string& uri) { return g_resolver ? g_resolver(uri) : nullptr; }

// ------------------------------------------------------------------ library
PrefabLibrary::Entry PrefabLibrary::load(const std::string& uri) {
    Entry e;
    e.modified = m_vfs.modifiedTime(uri);
    auto text = m_vfs.readText(uri);
    if (!text) {
        AGR_LOG_ERROR("Prefab", "cannot read '{}': {}", uri, text.error().message);
        return e;
    }
    e.content = json::parse(text.value(), nullptr, false);
    e.valid = validPrefab(&e.content);
    if (!e.valid) AGR_LOG_ERROR("Prefab", "'{}' is not a valid prefab file", uri);
    return e;
}

const json* PrefabLibrary::get(const std::string& uri) {
    if (uri.empty()) return nullptr;
    auto it = m_entries.find(uri);
    if (it == m_entries.end()) it = m_entries.emplace(uri, load(uri)).first;
    return it->second.valid ? &it->second.content : nullptr;
}

Result<json> PrefabLibrary::save(const std::string& uri, const json& content) {
    if (!validPrefab(&content)) return makeError(Error::Code::InvalidArgument, "invalid prefab content");
    json previous = nullptr;
    if (m_entries.contains(uri) || m_vfs.exists(uri))
        if (const json* old = get(uri)) previous = *old;
    if (Status s = m_vfs.writeText(uri, content.dump(2)); !s) return s.error();
    Entry e;
    e.content = content;
    e.valid = true;
    e.modified = m_vfs.modifiedTime(uri);
    m_entries[uri] = std::move(e);
    return previous;
}

std::vector<std::pair<std::string, json>> PrefabLibrary::reloadChanged() {
    std::vector<std::pair<std::string, json>> changed;
    for (auto& [uri, entry] : m_entries) {
        const i64 t = m_vfs.modifiedTime(uri);
        if (t == 0 || t == entry.modified) continue;
        json previous = entry.valid ? entry.content : json(nullptr);
        entry = load(uri);
        AGR_LOG_INFO("Prefab", "hot reload '{}'", uri);
        changed.emplace_back(uri, std::move(previous));
    }
    return changed;
}

void PrefabLibrary::install() {
    setPrefabResolver([this](const std::string& uri) { return get(uri); });
}

// ------------------------------------------------------------------ operations
namespace prefab {

Result<Entity> instantiate(Scene& scene, const std::string& uri, Entity parent) {
    if (!validPrefab(resolvePrefab(uri))) return makeError(Error::Code::NotFound, "prefab '" + uri + "' is missing or invalid");
    const Uuid id = Uuid::generate();
    detail::LoadContext ctx{scene};
    Entity created{};
    const json item = {{"id", id.toString()}, {"prefab", uri}};
    if (Status s = detail::loadInstanceEntry(ctx, item, parent, id, &created); !s) return s.error();
    detail::resolveRefs(ctx);
    return created;
}

json fromEntity(const Scene& scene, Entity root) {
    json entities = json::array();
    if (scene.valid(root)) {
        detail::collectSubtree(scene, root, entities, true, true);
        entities[0].erase("parent");
    }
    return {{"format", "agr.prefab"}, {"version", 1}, {"entities", std::move(entities)}};
}

Status link(Scene& scene, Entity root, const std::string& uri) {
    const json* base = resolvePrefab(uri);
    if (!validPrefab(base)) return makeError(Error::Code::NotFound, "prefab '" + uri + "' is missing or invalid");
    Scene tmp;
    auto troot = expand(tmp, *base, {}, scene.uuidOf(root), 1, uri);
    if (!troot) return troot.error();
    std::vector<Entity> mine, theirs;
    preorder(scene, root, mine);
    preorder(tmp, troot.value(), theirs);
    if (mine.size() != theirs.size())
        return makeError(Error::Code::InvalidArgument, "the hierarchy does not match prefab '" + uri + "'");
    for (usize i = 1; i < mine.size(); ++i)
        if (!scene.setUuid(mine[i], tmp.uuidOf(theirs[i])))
            return makeError(Error::Code::Unknown, "id collision while linking prefab '" + uri + "'");
    auto& reg = scene.registry();
    reg.remove<PrefabInstanceComponent>(root);
    for (usize i = 1; i < mine.size(); ++i)  // nested instances: their markers come from the prefab
        if (const auto* pic = tmp.registry().tryGet<PrefabInstanceComponent>(theirs[i])) {
            if (auto* own = reg.tryGet<PrefabInstanceComponent>(mine[i])) *own = *pic;
            else reg.emplace<PrefabInstanceComponent>(mine[i], *pic);
        }
    reg.emplace<PrefabInstanceComponent>(root, PrefabInstanceComponent{uri, {}});
    return {};
}

Result<json> applyContent(const Scene& scene, Entity root) {
    const auto* pic = const_cast<Scene&>(scene).registry().tryGet<PrefabInstanceComponent>(root);
    if (!pic) return makeError(Error::Code::InvalidArgument, "not a prefab instance");
    const json* base = resolvePrefab(pic->prefab);
    if (!validPrefab(base)) return makeError(Error::Code::NotFound, "prefab '" + pic->prefab + "' is missing or invalid");
    // Instance ids of the prefab's own entities -> their ids in the file.
    const Uuid n = scene.uuidOf(root);
    const std::string firstId = (*base)["entities"][0]["id"].get<std::string>();
    std::unordered_map<std::string, std::string> table;
    for (const auto& item : (*base)["entities"]) {
        const std::string fid = item["id"].get<std::string>();
        table[(fid == firstId ? n : deriveId(n, fid)).toString()] = fid;
    }
    json content = fromEntity(scene, root);
    for (auto& entry : content["entities"]) {
        // Nested instances among the prefab's entities: overrides go back to the file's id space.
        if (entry.contains("prefab") && entry.contains("overrides")) {
            auto it = table.find(entry["id"].get<std::string>());
            const json* nested = resolvePrefab(entry["prefab"].get<std::string>());
            if (it != table.end() && validPrefab(nested)) {
                std::unordered_map<std::string, std::string> back;
                for (const auto& [actual, original] :
                     idPairs(*nested, entry["prefab"].get<std::string>(), Uuid::fromString(entry["id"].get<std::string>()),
                             Uuid::fromString(it->second), 1))
                    back[actual.toString()] = original.toString();
                entry["overrides"] = translate(entry["overrides"], back);
            }
        }
    }
    json entities = json::array();
    for (const auto& entry : content["entities"]) {
        json copy = json::object();
        for (const auto& [k, v] : entry.items()) copy[k] = (k == "overrides") ? v : translate(v, table);
        entities.push_back(std::move(copy));
    }
    content["entities"] = std::move(entities);
    return content;
}

json overrides(const Scene& scene, Entity root) {
    const auto* pic = const_cast<Scene&>(scene).registry().tryGet<PrefabInstanceComponent>(root);
    if (!pic) return json::object();
    const json* base = resolvePrefab(pic->prefab);
    if (!validPrefab(base)) return json::object();
    json ov = computeOverrides(scene, root, *base, pic->prefab);
    return ov.is_null() ? json::object() : ov;
}

u32 overrideCount(const Scene& scene, Entity root) {
    const json ov = overrides(scene, root);
    u32 n = 0;
    const std::string rootKey = scene.uuidOf(root).toString();
    if (ov.contains("components"))
        for (const auto& [id, comps] : ov["components"].items())
            for (const auto& [name, fields] : comps.items())
                if (!(id == rootKey && (name == "Transform" || name == "Name"))) n += static_cast<u32>(fields.size());
    if (ov.contains("removedComponents"))
        for (const auto& [id, names] : ov["removedComponents"].items()) n += static_cast<u32>(names.size());
    if (ov.contains("removed")) n += static_cast<u32>(ov["removed"].size());
    if (ov.contains("added")) n += static_cast<u32>(ov["added"].size());
    return n;
}

bool isInstanceRoot(const Scene& scene, Entity e) {
    return scene.valid(e) && const_cast<Scene&>(scene).registry().tryGet<PrefabInstanceComponent>(e) != nullptr;
}

Entity instanceRootOf(const Scene& scene, Entity e) {
    for (Entity c = e; scene.valid(c); c = scene.parent(c))
        if (isInstanceRoot(scene, c)) return c;
    return {};
}

Status refresh(Scene& scene, Entity root, const json& previous) {
    const auto* pic = scene.registry().tryGet<PrefabInstanceComponent>(root);
    if (!pic) return makeError(Error::Code::InvalidArgument, "not a prefab instance");
    json ov = json::object();
    if (validPrefab(&previous)) {
        ov = computeOverrides(scene, root, previous, pic->prefab);
        if (ov.is_null()) ov = json::object();
    }
    return rebuild(scene, root, ov);
}

u32 refreshAll(Scene& scene, const std::string& uri, const json& previous, Entity except) {
    std::vector<Uuid> roots;
    scene.registry().view<PrefabInstanceComponent>().each([&](Entity e, PrefabInstanceComponent& p) {
        if (p.prefab == uri && e != except && p.unresolved.empty()) roots.push_back(scene.uuidOf(e));
    });
    u32 n = 0;
    for (Uuid id : roots) {
        const Entity e = scene.findByUuid(id);  // earlier rebuilds may have recreated nested ones
        if (!scene.valid(e)) continue;
        if (Status s = refresh(scene, e, previous); s) ++n;
        else AGR_LOG_WARN("Prefab", "instance '{}' not refreshed: {}", scene.nameOf(e), s.error().message);
    }
    scene.updateTransforms();
    return n;
}

Status revert(Scene& scene, Entity root) {
    json ov = overrides(scene, root);
    const std::string key = scene.uuidOf(root).toString();
    json kept = json::object();
    if (ov.contains("components") && ov["components"].contains(key))
        for (const char* name : {"Transform", "Name"})
            if (ov["components"][key].contains(name)) kept["components"][key][name] = ov["components"][key][name];
    Status s = rebuild(scene, root, kept);
    scene.updateTransforms();
    return s;
}

void unpack(Scene& scene, Entity root) {
    if (isInstanceRoot(scene, root)) scene.registry().remove<PrefabInstanceComponent>(root);
}

} // namespace prefab
} // namespace agr
