#include "agr/scene/SceneSerializer.hpp"

#include <nlohmann/json.hpp>

#include "agr/core/Log.hpp"
#include "agr/scene/Reflection.hpp"

namespace agr {

using json = nlohmann::ordered_json;
using ecs::Entity;

namespace {

json writeField(const Scene& scene, const FieldInfo& f, void* comp) {
    switch (f.type) {
        case FieldType::Bool: return f.ref<bool>(comp);
        case FieldType::Int32: return f.ref<i32>(comp);
        case FieldType::UInt32: return f.ref<u32>(comp);
        case FieldType::Float: return f.ref<float>(comp);
        case FieldType::Vec2: { auto& v = f.ref<math::Vec2>(comp); return json::array({v.x, v.y}); }
        case FieldType::Vec3: { auto& v = f.ref<math::Vec3>(comp); return json::array({v.x, v.y, v.z}); }
        case FieldType::Vec4: { auto& v = f.ref<math::Vec4>(comp); return json::array({v.x, v.y, v.z, v.w}); }
        case FieldType::Quat: { auto& q = f.ref<math::Quat>(comp); return json::array({q.x, q.y, q.z, q.w}); }
        case FieldType::String: return f.ref<std::string>(comp);
        case FieldType::Entity: {
            const Entity target = f.ref<Entity>(comp);
            return scene.valid(target) ? json(scene.uuidOf(target).toString()) : json(nullptr);
        }
        case FieldType::IntArray: {
            // Run-length encoded as [value, count, value, count, ...]: tile maps are mostly runs.
            const auto& v = f.ref<std::vector<i32>>(comp);
            json rle = json::array();
            for (usize i = 0; i < v.size();) {
                usize n = 1;
                while (i + n < v.size() && v[i + n] == v[i]) ++n;
                rle.push_back(v[i]);
                rle.push_back(n);
                i += n;
            }
            json out = json::object();
            out["rle"] = std::move(rle);
            return out;
        }
        case FieldType::Enum: {
            const i32 value = f.ref<i32>(comp);
            for (const auto& e : f.enumEntries)
                if (e.value == value) return e.name;
            return value;
        }
    }
    return nullptr;
}

template <typename V, int N>
bool readFloats(const json& j, V& out) {
    if (!j.is_array() || j.size() != N) return false;
    float tmp[N];
    for (int i = 0; i < N; ++i) {
        if (!j[i].is_number()) return false;  // all-or-nothing: never half-assign
        tmp[i] = j[i].get<float>();
    }
    for (int i = 0; i < N; ++i) (&out.x)[i] = tmp[i];
    return true;
}

/// Entity references are resolved after every entity exists.
struct PendingRef {
    Entity owner;
    ecs::ComponentTypeId component;
    const FieldInfo* field;
    Uuid target;
};

bool readField(const FieldInfo& f, void* comp, const json& j, Entity owner, ecs::ComponentTypeId cid,
               std::vector<PendingRef>& refs) {
    switch (f.type) {
        case FieldType::Bool: if (!j.is_boolean()) return false; f.ref<bool>(comp) = j.get<bool>(); return true;
        case FieldType::Int32: if (!j.is_number_integer()) return false; f.ref<i32>(comp) = j.get<i32>(); return true;
        case FieldType::UInt32: if (!j.is_number_unsigned() && !j.is_number_integer()) return false; f.ref<u32>(comp) = j.get<u32>(); return true;
        case FieldType::Float: if (!j.is_number()) return false; f.ref<float>(comp) = j.get<float>(); return true;
        case FieldType::Vec2: return readFloats<math::Vec2, 2>(j, f.ref<math::Vec2>(comp));
        case FieldType::Vec3: return readFloats<math::Vec3, 3>(j, f.ref<math::Vec3>(comp));
        case FieldType::Vec4: return readFloats<math::Vec4, 4>(j, f.ref<math::Vec4>(comp));
        case FieldType::Quat: return readFloats<math::Quat, 4>(j, f.ref<math::Quat>(comp));
        case FieldType::String: if (!j.is_string()) return false; f.ref<std::string>(comp) = j.get<std::string>(); return true;
        case FieldType::Entity:
            f.ref<Entity>(comp) = Entity{};
            if (j.is_null()) return true;
            if (!j.is_string()) return false;
            refs.push_back({owner, cid, &f, Uuid::fromString(j.get<std::string>())});
            return true;
        case FieldType::IntArray: {
            std::vector<i32> out;
            if (j.is_array()) {  // plain array also accepted (hand-written files)
                for (const auto& x : j) {
                    if (!x.is_number_integer()) return false;
                    out.push_back(x.get<i32>());
                }
            } else if (j.is_object() && j.contains("rle") && j["rle"].is_array() && j["rle"].size() % 2 == 0) {
                const json& r = j["rle"];
                for (usize i = 0; i < r.size(); i += 2) {
                    if (!r[i].is_number_integer() || !r[i + 1].is_number_unsigned()) return false;
                    const u64 count = r[i + 1].get<u64>();
                    if (count > (1u << 26) || out.size() + count > (1u << 26)) return false;  // 64 M cells: corrupt file guard
                    out.insert(out.end(), static_cast<usize>(count), r[i].get<i32>());
                }
            } else {
                return false;
            }
            f.ref<std::vector<i32>>(comp) = std::move(out);
            return true;
        }
        case FieldType::Enum:
            if (j.is_number_integer()) { f.ref<i32>(comp) = j.get<i32>(); return true; }
            if (j.is_string()) {
                for (const auto& e : f.enumEntries)
                    if (e.name == j.get<std::string>()) { f.ref<i32>(comp) = e.value; return true; }
            }
            return false;
    }
    return false;
}

json componentsToJson(const Scene& scene, Entity e) {
    auto& registry = const_cast<Scene&>(scene).registry();
    json comps = json::object();
    // Iterate through the reflection catalogue for a deterministic order.
    for (const ComponentInfo* info : ComponentRegistry::instance().all()) {
        if (!info->serializable) continue;
        ecs::PoolBase* pool = registry.findPool(info->id);
        if (!pool || !pool->contains(e)) continue;
        void* comp = pool->getRaw(e);
        json fields = json::object();
        for (const auto& f : info->fields) {
            if (f.flags & FieldFlag_Transient) continue;
            fields[f.name] = writeField(scene, f, comp);
        }
        comps[info->name] = std::move(fields);
    }
    return comps;
}

void collectSubtree(const Scene& scene, Entity e, json& out, bool includeParent) {
    json entry;
    entry["id"] = scene.uuidOf(e).toString();
    if (includeParent) {
        const Entity p = scene.parent(e);
        entry["parent"] = p.isNull() ? json(nullptr) : json(scene.uuidOf(p).toString());
    }
    entry["components"] = componentsToJson(scene, e);
    out.push_back(std::move(entry));
    for (Entity c : scene.children(e)) collectSubtree(scene, c, out, true);
}

/// Creates entities from an "entities" array. `remap` = true assigns new UUIDs.
Status loadEntities(Scene& scene, const json& entities, bool remap, Entity rootParent, Entity* firstCreated) {
    if (!entities.is_array()) return makeError(Error::Code::ParseError, "'entities' must be an array");
    std::unordered_map<std::string, Entity> byFileId;
    std::vector<PendingRef> refs;
    auto& registry = scene.registry();
    const auto& catalogue = ComponentRegistry::instance();

    for (const json& item : entities) {
        if (!item.is_object() || !item.contains("id") || !item["id"].is_string())
            return makeError(Error::Code::ParseError, "entity entry without a valid 'id'");
        const std::string fileId = item["id"].get<std::string>();
        Entity parent = rootParent;
        if (item.contains("parent") && item["parent"].is_string()) {
            auto it = byFileId.find(item["parent"].get<std::string>());
            if (it == byFileId.end())
                return makeError(Error::Code::ParseError, "entity " + fileId + " references a parent defined later or missing");
            parent = it->second;
        }
        const Uuid id = remap ? Uuid{} : Uuid::fromString(fileId);
        const Entity e = scene.createEntity("Entity", parent, id);
        byFileId[fileId] = e;
        if (firstCreated && firstCreated->isNull()) *firstCreated = e;

        if (!item.contains("components")) continue;
        for (const auto& [compName, fields] : item["components"].items()) {
            const ComponentInfo* info = catalogue.find(compName);
            if (!info) {
                AGR_LOG_WARN("Serializer", "unknown component '{}' on entity {} skipped", compName, fileId);
                continue;
            }
            ecs::PoolBase& pool = registry.ensurePool(info->id, info->makePool);
            void* comp = pool.emplaceDefault(registry, e);
            if (!comp) {
                AGR_LOG_WARN("Serializer", "component '{}' is not default constructible", compName);
                continue;
            }
            for (const auto& f : info->fields) {
                if ((f.flags & FieldFlag_Transient) || !fields.contains(f.name)) continue;
                if (!readField(f, comp, fields[f.name], e, info->id, refs))
                    AGR_LOG_WARN("Serializer", "field '{}.{}' has an invalid value; default kept", compName, f.name);
            }
        }
    }

    // Resolve entity references (to ids inside the file first, then the scene).
    for (const PendingRef& r : refs) {
        Entity target{};
        auto it = byFileId.find(r.target.toString());
        if (it != byFileId.end()) target = it->second;
        else if (!remap) target = scene.findByUuid(r.target);
        if (void* comp = registry.findPool(r.component)->getRaw(r.owner)) r.field->ref<Entity>(comp) = target;
    }
    return {};
}

} // namespace

json SceneSerializer::toJson(const Scene& scene) {
    json root;
    root["format"] = "agr.scene";
    root["version"] = kFormatVersion;
    root["name"] = scene.name();
    json entities = json::array();
    for (Entity r : scene.roots()) collectSubtree(scene, r, entities, true);
    root["entities"] = std::move(entities);
    return root;
}

std::string SceneSerializer::toString(const Scene& scene, int indent) { return toJson(scene).dump(indent); }

Status SceneSerializer::fromJson(Scene& scene, const json& j) {
    if (!j.is_object() || j.value("format", "") != "agr.scene")
        return makeError(Error::Code::ParseError, "not an AGR scene file");
    const int version = j.value("version", 0);
    if (version < 1 || version > kFormatVersion)
        return makeError(Error::Code::VersionMismatch, "unsupported scene version " + std::to_string(version));
    scene.clear();
    scene.setName(j.value("name", std::string("Untitled")));
    if (!j.contains("entities")) return {};
    Status s = loadEntities(scene, j["entities"], false, Entity{}, nullptr);
    if (s) scene.updateTransforms();
    return s;
}

Status SceneSerializer::fromString(Scene& scene, std::string_view text) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded()) return makeError(Error::Code::ParseError, "invalid JSON");
    return fromJson(scene, j);
}

json SceneSerializer::entityToJson(const Scene& scene, Entity root) {
    json out;
    out["format"] = "agr.entity";
    out["version"] = kFormatVersion;
    json entities = json::array();
    if (scene.valid(root)) {
        // The root is written without parent so it can be re-parented anywhere.
        json tmp = json::array();
        collectSubtree(scene, root, tmp, true);
        tmp[0].erase("parent");
        entities = std::move(tmp);
    }
    out["entities"] = std::move(entities);
    return out;
}

Result<Entity> SceneSerializer::instantiate(Scene& scene, const json& j, Entity parent) {
    if (!j.is_object() || j.value("format", "") != "agr.entity" || !j.contains("entities"))
        return makeError(Error::Code::ParseError, "not an AGR entity snippet");
    Entity first{};
    Status s = loadEntities(scene, j["entities"], true, parent, &first);
    if (!s) return s.error();
    if (first.isNull()) return makeError(Error::Code::ParseError, "snippet is empty");
    return first;
}

} // namespace agr
