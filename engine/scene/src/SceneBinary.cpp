// Binary scene format: the same content as the JSON format (entities in hierarchy
// order, components and fields by reflected name, entity references as UUIDs,
// prefab instances as references + overrides) written straight from the
// reflection data, with no JSON tree in between.
//
// Layout (little-endian):
//   magic "AGRSCNB\0", u32 version
//   str  scene name
//   u32  string count, then each string (u32 length + bytes): component / field names
//   u32  entity count, then per entity:
//        u64 uuid, u64 parent uuid (0: root), u8 kind
//        kind 0 (plain):  u32 component count, per component:
//                         u32 name index, u32 payload bytes, u32 field count,
//                         per field: u32 name index, u8 FieldType, value
//        kind 1 (prefab instance): u32 bytes of CBOR holding the JSON entry
//
// Every component carries its payload size and every field its type, so a
// reader skips unknown components and fields whose type changed: files stay
// readable across engine versions, exactly like the JSON format.
#include <algorithm>
#include <bit>
#include <cstring>
#include <unordered_map>

#include "agr/core/Log.hpp"
#include "agr/scene/Prefab.hpp"
#include "agr/scene/SceneSerializer.hpp"
#include "SerializerInternal.hpp"

namespace agr {

using ecs::Entity;
using namespace detail;

static_assert(std::endian::native == std::endian::little, "binary scenes assume a little-endian CPU (x86, ARM)");

namespace {

constexpr char kMagic[8] = {'A', 'G', 'R', 'S', 'C', 'N', 'B', '\0'};
constexpr u32 kBinaryVersion = 1;
constexpr u8 kPlain = 0, kPrefabInstance = 1;
constexpr u64 kMaxCells = 1ull << 26;  // same corrupt-file guard as the JSON reader

// ----------------------------------------------------------------- writer
class Writer {
public:
    std::vector<u8> bytes;

    template <typename T>
    void pod(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        const usize at = bytes.size();
        bytes.resize(at + sizeof(T));
        std::memcpy(bytes.data() + at, &v, sizeof(T));
    }
    void raw(const void* data, usize n) {
        const usize at = bytes.size();
        bytes.resize(at + n);
        if (n) std::memcpy(bytes.data() + at, data, n);
    }
    void str(std::string_view s) {
        pod(static_cast<u32>(s.size()));
        raw(s.data(), s.size());
    }
    usize reserveU32() {
        pod(u32{0});
        return bytes.size() - sizeof(u32);
    }
    void patchU32(usize at, u32 v) { std::memcpy(bytes.data() + at, &v, sizeof v); }
};

class StringTable {
public:
    u32 index(const std::string& s) {
        auto [it, inserted] = m_ids.try_emplace(s, static_cast<u32>(m_list.size()));
        if (inserted) m_list.push_back(&it->first);
        return it->second;
    }
    const std::vector<const std::string*>& list() const { return m_list; }

private:
    std::unordered_map<std::string, u32> m_ids;
    std::vector<const std::string*> m_list;
};

struct SceneWriter {
    const Scene& scene;
    ecs::Registry& registry;
    const ComponentRegistry& components;
    Writer body;
    StringTable strings;
    u32 entityCount = 0;

    void field(const FieldInfo& f, void* comp) {
        body.pod(strings.index(f.name));
        body.pod(static_cast<u8>(f.type));
        switch (f.type) {
            case FieldType::Bool: body.pod(static_cast<u8>(f.ref<bool>(comp) ? 1 : 0)); break;
            case FieldType::Int32: case FieldType::Enum: body.pod(f.ref<i32>(comp)); break;
            case FieldType::UInt32: body.pod(f.ref<u32>(comp)); break;
            case FieldType::Float: body.pod(f.ref<float>(comp)); break;
            case FieldType::Vec2: body.pod(f.ref<math::Vec2>(comp)); break;
            case FieldType::Vec3: body.pod(f.ref<math::Vec3>(comp)); break;
            case FieldType::Vec4: body.pod(f.ref<math::Vec4>(comp)); break;
            case FieldType::Quat: body.pod(f.ref<math::Quat>(comp)); break;
            case FieldType::String: body.str(f.ref<std::string>(comp)); break;
            case FieldType::Entity: {
                const Entity target = f.ref<Entity>(comp);
                body.pod(scene.valid(target) ? scene.uuidOf(target).value() : u64{0});
                break;
            }
            case FieldType::IntArray: {  // run-length encoded: tile maps are mostly runs
                const auto& v = f.ref<std::vector<i32>>(comp);
                const usize countAt = body.reserveU32();
                u32 runs = 0;
                for (usize i = 0; i < v.size(); ++runs) {
                    usize n = 1;
                    while (i + n < v.size() && v[i + n] == v[i]) ++n;
                    body.pod(v[i]);
                    body.pod(static_cast<u32>(n));
                    i += n;
                }
                body.patchU32(countAt, runs);
                break;
            }
        }
    }

    void components_(Entity e) {
        struct Present {
            const ComponentInfo* info;
            void* data;
        };
        std::vector<Present> list;
        list.reserve(16);
        registry.eachPool([&](ecs::PoolBase& pool) {
            void* data = pool.getRaw(e);
            if (!data) return;
            const ComponentInfo* info = components.find(pool.typeId());
            if (info && info->serializable) list.push_back({info, data});
        });
        // Same order as the JSON writer, so loading adds components in the same order.
        std::sort(list.begin(), list.end(), [](const Present& a, const Present& b) {
            return a.info->category != b.info->category ? a.info->category < b.info->category : a.info->name < b.info->name;
        });
        body.pod(static_cast<u32>(list.size()));
        for (const Present& p : list) {
            body.pod(strings.index(p.info->name));
            const usize sizeAt = body.reserveU32();
            const usize countAt = body.reserveU32();
            u32 fields = 0;
            for (const auto& f : p.info->fields) {
                if (f.flags & FieldFlag_Transient) continue;
                field(f, p.data);
                ++fields;
            }
            body.patchU32(countAt, fields);
            body.patchU32(sizeAt, static_cast<u32>(body.bytes.size() - (sizeAt + sizeof(u32))));
        }
    }

    void subtree(Entity e) {
        ++entityCount;
        const Entity parent = scene.parent(e);
        body.pod(scene.uuidOf(e).value());
        body.pod(parent.isNull() ? u64{0} : scene.uuidOf(parent).value());
        detail::json entry;
        if (registry.tryGet<PrefabInstanceComponent>(e)) {
            entry["id"] = scene.uuidOf(e).toString();
            entry["parent"] = parent.isNull() ? detail::json(nullptr) : detail::json(scene.uuidOf(parent).toString());
        }
        if (!entry.is_null() && writeInstanceEntry(scene, e, entry)) {  // prefab instance: reference + overrides, as in JSON
            body.pod(kPrefabInstance);
            const std::vector<u8> cbor = detail::json::to_cbor(entry);
            body.pod(static_cast<u32>(cbor.size()));
            body.raw(cbor.data(), cbor.size());
            return;
        }
        body.pod(kPlain);
        components_(e);
        for (Entity c : scene.children(e)) subtree(c);
    }
};

// ----------------------------------------------------------------- reader
class Reader {
public:
    Reader(const u8* data, usize size) : m_p(data), m_end(data + size) {}
    bool ok() const { return m_ok; }
    usize remaining() const { return static_cast<usize>(m_end - m_p); }
    const u8* cursor() const { return m_p; }

    template <typename T>
    T pod() {
        T v{};
        if (!need(sizeof(T))) return v;
        std::memcpy(&v, m_p, sizeof(T));
        m_p += sizeof(T);
        return v;
    }
    std::string_view str() {
        const u32 n = pod<u32>();
        if (!need(n)) return {};
        std::string_view s(reinterpret_cast<const char*>(m_p), n);
        m_p += n;
        return s;
    }
    const u8* skip(usize n) {
        if (!need(n)) return nullptr;
        const u8* at = m_p;
        m_p += n;
        return at;
    }

private:
    bool need(usize n) {
        if (!m_ok || remaining() < n) {
            m_ok = false;
            return false;
        }
        return true;
    }
    const u8* m_p;
    const u8* m_end;
    bool m_ok = true;
};

struct PendingUuidRef {
    Entity owner;
    ecs::ComponentTypeId component;
    const FieldInfo* field;
    u64 target;
};

/// Reads one field value of type `type` into `f` (when `f` is non-null and of
/// the same type) or skips it. False when the stream is corrupt.
bool readBinaryField(Reader& r, FieldType type, const FieldInfo* f, void* comp, Entity owner, ecs::ComponentTypeId cid,
                     std::vector<PendingUuidRef>& refs) {
    const bool use = f && f->type == type;
    switch (type) {
        case FieldType::Bool: { const u8 v = r.pod<u8>(); if (use) f->ref<bool>(comp) = v != 0; break; }
        case FieldType::Int32: case FieldType::Enum: { const i32 v = r.pod<i32>(); if (use) f->ref<i32>(comp) = v; break; }
        case FieldType::UInt32: { const u32 v = r.pod<u32>(); if (use) f->ref<u32>(comp) = v; break; }
        case FieldType::Float: { const float v = r.pod<float>(); if (use) f->ref<float>(comp) = v; break; }
        case FieldType::Vec2: { const auto v = r.pod<math::Vec2>(); if (use) f->ref<math::Vec2>(comp) = v; break; }
        case FieldType::Vec3: { const auto v = r.pod<math::Vec3>(); if (use) f->ref<math::Vec3>(comp) = v; break; }
        case FieldType::Vec4: { const auto v = r.pod<math::Vec4>(); if (use) f->ref<math::Vec4>(comp) = v; break; }
        case FieldType::Quat: { const auto v = r.pod<math::Quat>(); if (use) f->ref<math::Quat>(comp) = v; break; }
        case FieldType::String: { const std::string_view v = r.str(); if (use && r.ok()) f->ref<std::string>(comp).assign(v); break; }
        case FieldType::Entity: {
            const u64 v = r.pod<u64>();
            if (use) {
                f->ref<Entity>(comp) = Entity{};
                if (v != 0) refs.push_back({owner, cid, f, v});
            }
            break;
        }
        case FieldType::IntArray: {
            const u32 runs = r.pod<u32>();
            if (!r.ok() || static_cast<usize>(runs) * 8 > r.remaining()) return false;
            std::vector<i32> out;
            for (u32 i = 0; i < runs; ++i) {
                const i32 value = r.pod<i32>();
                const u32 count = r.pod<u32>();
                if (!use) continue;
                if (out.size() + count > kMaxCells) return false;
                out.insert(out.end(), count, value);
            }
            if (use && r.ok()) f->ref<std::vector<i32>>(comp) = std::move(out);
            break;
        }
        default:
            return false;  // unknown type tag: cannot skip safely
    }
    return r.ok();
}

} // namespace

bool SceneSerializer::isBinary(std::string_view bytes) {
    return bytes.size() >= sizeof kMagic && std::memcmp(bytes.data(), kMagic, sizeof kMagic) == 0;
}

std::vector<u8> SceneSerializer::toBinary(const Scene& scene) {
    SceneWriter w{scene, const_cast<Scene&>(scene).registry(), ComponentRegistry::instance(), {}, {}, 0};
    w.body.bytes.reserve(64 + w.registry.aliveCount() * 160);
    for (Entity r : scene.roots()) w.subtree(r);

    Writer out;
    out.bytes.reserve(w.body.bytes.size() + 4096);
    out.raw(kMagic, sizeof kMagic);
    out.pod(kBinaryVersion);
    out.str(scene.name());
    out.pod(static_cast<u32>(w.strings.list().size()));
    for (const std::string* s : w.strings.list()) out.str(*s);
    out.pod(w.entityCount);
    out.raw(w.body.bytes.data(), w.body.bytes.size());
    return std::move(out.bytes);
}

Status SceneSerializer::fromBinary(Scene& scene, std::span<const u8> data) {
    const auto corrupt = [] { return makeError(Error::Code::ParseError, "truncated or corrupt binary scene"); };
    if (!isBinary(std::string_view(reinterpret_cast<const char*>(data.data()), data.size())))
        return makeError(Error::Code::ParseError, "not an AGR binary scene");
    Reader r(data.data() + sizeof kMagic, data.size() - sizeof kMagic);
    const u32 version = r.pod<u32>();
    if (!r.ok()) return corrupt();
    if (version < 1 || version > kBinaryVersion)
        return makeError(Error::Code::VersionMismatch, "unsupported binary scene version " + std::to_string(version));
    const std::string_view name = r.str();
    const u32 stringCount = r.pod<u32>();
    if (!r.ok() || static_cast<usize>(stringCount) * 4 > r.remaining()) return corrupt();
    std::vector<std::string_view> strings(stringCount);
    for (auto& s : strings) s = r.str();
    if (!r.ok()) return corrupt();

    scene.clear();
    scene.setName(name.empty() ? std::string("Untitled") : std::string(name));

    const ComponentRegistry& components = ComponentRegistry::instance();
    auto& registry = scene.registry();
    // Name lookups are done once per string, not once per entity.
    std::vector<const ComponentInfo*> infoOf(stringCount, nullptr);
    std::vector<u8> infoResolved(stringCount, 0);
    std::unordered_map<u64, const FieldInfo*> fieldOf;  // (component string, field string) -> field
    const auto info = [&](u32 idx) -> const ComponentInfo* {
        if (!infoResolved[idx]) {
            infoResolved[idx] = 1;
            infoOf[idx] = components.find(strings[idx]);
            if (!infoOf[idx]) AGR_LOG_WARN("Serializer", "unknown component '{}' skipped", strings[idx]);
        }
        return infoOf[idx];
    };
    const auto field = [&](u32 compIdx, const ComponentInfo* ci, u32 fieldIdx) -> const FieldInfo* {
        const u64 key = (static_cast<u64>(compIdx) << 32) | fieldIdx;
        auto [it, inserted] = fieldOf.try_emplace(key, nullptr);
        if (inserted) {
            const FieldInfo* f = ci->field(strings[fieldIdx]);
            it->second = f && !(f->flags & FieldFlag_Transient) ? f : nullptr;
        }
        return it->second;
    };

    LoadContext ctx{scene};
    std::unordered_map<u64, Entity> byId;
    std::vector<PendingUuidRef> refs;
    const u32 entityCount = r.pod<u32>();
    if (!r.ok()) return corrupt();
    byId.reserve(entityCount);
    Status status{};
    for (u32 n = 0; n < entityCount && status; ++n) {
        const u64 id = r.pod<u64>();
        const u64 parentId = r.pod<u64>();
        const u8 kind = r.pod<u8>();
        if (!r.ok()) return corrupt();
        Entity parent{};
        if (parentId != 0) {
            if (auto it = byId.find(parentId); it != byId.end()) parent = it->second;
            else parent = ctx.lookup(Uuid(parentId).toString());
            if (parent.isNull()) {
                status = makeError(Error::Code::ParseError, "entity " + Uuid(id).toString() + " references a parent defined later or missing");
                break;
            }
        }
        Entity e{};
        if (kind == kPrefabInstance) {
            const u32 size = r.pod<u32>();
            const u8* cbor = r.skip(size);
            if (!r.ok()) return corrupt();
            const detail::json item = detail::json::from_cbor(cbor, cbor + size, true, false);
            if (item.is_discarded() || !item.is_object()) return corrupt();
            status = loadInstanceEntry(ctx, item, parent, id == 0 ? Uuid::generate() : Uuid(id), &e);
            if (!status) break;
        } else if (kind == kPlain) {
            e = scene.createEntity("Entity", parent, Uuid(id));
            const u32 compCount = r.pod<u32>();
            for (u32 c = 0; c < compCount && r.ok(); ++c) {
                const u32 nameIdx = r.pod<u32>();
                const u32 payload = r.pod<u32>();
                if (!r.ok() || nameIdx >= stringCount || payload > r.remaining()) return corrupt();
                const u8* payloadEnd = r.cursor() + payload;
                const ComponentInfo* ci = info(nameIdx);
                void* comp = nullptr;
                if (ci) {
                    ecs::PoolBase& pool = registry.ensurePool(ci->id, ci->makePool);
                    comp = pool.contains(e) ? pool.getRaw(e) : pool.emplaceDefault(registry, e);
                    if (!comp) AGR_LOG_WARN("Serializer", "component '{}' is not default constructible", ci->name);
                }
                if (!comp) {
                    r.skip(payload);
                    continue;
                }
                const u32 fieldCount = r.pod<u32>();
                for (u32 k = 0; k < fieldCount; ++k) {
                    const u32 fieldIdx = r.pod<u32>();
                    const auto type = static_cast<FieldType>(r.pod<u8>());
                    if (!r.ok() || fieldIdx >= stringCount) return corrupt();
                    const FieldInfo* f = field(nameIdx, ci, fieldIdx);
                    if (f && f->type != type)
                        AGR_LOG_WARN("Serializer", "field '{}.{}' has an invalid value; default kept", ci->name, f->name);
                    if (!readBinaryField(r, type, f, comp, e, ci->id, refs)) return corrupt();
                }
                if (r.cursor() != payloadEnd) return corrupt();
            }
            if (!r.ok()) return corrupt();
        } else {
            return corrupt();
        }
        byId[id] = e;
    }
    // Entity references, once every entity exists.
    for (const PendingUuidRef& ref : refs) {
        if (!scene.valid(ref.owner)) continue;
        Entity target{};
        if (auto it = byId.find(ref.target); it != byId.end()) target = it->second;
        else target = ctx.lookup(Uuid(ref.target).toString());
        if (ecs::PoolBase* pool = registry.findPool(ref.component))
            if (void* comp = pool->getRaw(ref.owner)) ref.field->ref<Entity>(comp) = target;
    }
    resolveRefs(ctx);  // references collected by prefab instances
    if (status) scene.updateTransforms();
    return status;
}

Status SceneSerializer::fromBytes(Scene& scene, std::string_view data) {
    if (isBinary(data)) return fromBinary(scene, std::span(reinterpret_cast<const u8*>(data.data()), data.size()));
    return fromString(scene, data);
}

} // namespace agr
