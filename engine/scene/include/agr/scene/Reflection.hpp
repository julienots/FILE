#pragma once
// Runtime reflection of components. One description drives:
//   * the editor inspector (widgets, ranges, tooltips),
//   * scene serialization (JSON today, binary for shipping builds later),
//   * scripting bindings and the visual logic editor (later phases).
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "agr/core/Uuid.hpp"
#include "agr/ecs/Registry.hpp"
#include "agr/math/Math.hpp"

namespace agr {

enum class FieldType : u8 { Bool, Int32, UInt32, Float, Vec2, Vec3, Vec4, Quat, String, Entity, Enum };
const char* enumName(FieldType type);

enum FieldFlags : u32 {
    FieldFlag_None = 0,
    FieldFlag_Color = 1u << 0,        ///< Vec3/Vec4 edited with a color picker.
    FieldFlag_ReadOnly = 1u << 1,     ///< Visible but not editable in the inspector.
    FieldFlag_Hidden = 1u << 2,       ///< Not shown in the inspector.
    FieldFlag_Transient = 1u << 3,    ///< Not serialized.
    FieldFlag_Angle = 1u << 4,        ///< Float stored in radians, shown in degrees.
    FieldFlag_AssetPath = 1u << 5,    ///< String holding an asset URI (drag & drop, preview).
};

struct EnumEntry {
    std::string name;
    i32 value;
};

struct FieldInfo {
    std::string name;         ///< Serialized key (stable, camelCase).
    std::string displayName;  ///< Inspector label.
    std::string tooltip;
    FieldType type = FieldType::Float;
    u32 flags = FieldFlag_None;
    float rangeMin = 0.0f;
    float rangeMax = 0.0f;    ///< rangeMin == rangeMax means "unbounded".
    float speed = 0.1f;       ///< Drag speed in the inspector.
    std::vector<EnumEntry> enumEntries;
    /// Returns the address of the field inside a component instance.
    std::function<void*(void* component)> access;

    bool hasRange() const { return rangeMin != rangeMax; }
    template <typename T>
    T& ref(void* component) const { return *static_cast<T*>(access(component)); }
};

struct ComponentInfo {
    std::string name;      ///< Unique serialized name, e.g. "Transform".
    std::string category;  ///< "Core", "Rendering", "Physics"... (Add Component menu)
    std::string description;
    ecs::ComponentTypeId id = 0;
    std::vector<FieldInfo> fields;
    bool userAddable = true;    ///< Listed in the editor "Add Component" menu.
    bool serializable = true;
    std::function<std::unique_ptr<ecs::PoolBase>()> makePool;

    const FieldInfo* field(std::string_view fieldName) const;
};

namespace detail {
template <typename M>
constexpr FieldType deduceFieldType() {
    using T = std::remove_cvref_t<M>;
    if constexpr (std::is_same_v<T, bool>) return FieldType::Bool;
    else if constexpr (std::is_same_v<T, i32>) return FieldType::Int32;
    else if constexpr (std::is_same_v<T, u32>) return FieldType::UInt32;
    else if constexpr (std::is_same_v<T, float>) return FieldType::Float;
    else if constexpr (std::is_same_v<T, math::Vec2>) return FieldType::Vec2;
    else if constexpr (std::is_same_v<T, math::Vec3>) return FieldType::Vec3;
    else if constexpr (std::is_same_v<T, math::Vec4>) return FieldType::Vec4;
    else if constexpr (std::is_same_v<T, math::Quat>) return FieldType::Quat;
    else if constexpr (std::is_same_v<T, std::string>) return FieldType::String;
    else if constexpr (std::is_same_v<T, ecs::Entity>) return FieldType::Entity;
    else if constexpr (std::is_enum_v<T>) {
        static_assert(sizeof(T) == sizeof(i32), "reflected enums must have a 32-bit underlying type");
        return FieldType::Enum;
    } else {
        static_assert(sizeof(T) == 0, "unsupported reflected field type");
        return FieldType::Float;
    }
}
} // namespace detail

/// Options passed when declaring a field.
struct FieldOptions {
    std::string displayName;
    std::string tooltip;
    u32 flags = FieldFlag_None;
    float rangeMin = 0.0f, rangeMax = 0.0f, speed = 0.1f;
    std::vector<EnumEntry> enumEntries;
};

template <typename C>
class ComponentBuilder {
public:
    explicit ComponentBuilder(ComponentInfo& info) : m_info(info) {}

    template <typename M>
    ComponentBuilder& field(std::string name, M C::*member, FieldOptions opts = {}) {
        FieldInfo f;
        f.name = std::move(name);
        f.displayName = opts.displayName.empty() ? prettify(f.name) : std::move(opts.displayName);
        f.tooltip = std::move(opts.tooltip);
        f.type = detail::deduceFieldType<M>();
        f.flags = opts.flags;
        f.rangeMin = opts.rangeMin;
        f.rangeMax = opts.rangeMax;
        f.speed = opts.speed;
        f.enumEntries = std::move(opts.enumEntries);
        f.access = [member](void* c) -> void* { return &(static_cast<C*>(c)->*member); };
        m_info.fields.push_back(std::move(f));
        return *this;
    }
    ComponentBuilder& category(std::string c) { m_info.category = std::move(c); return *this; }
    ComponentBuilder& description(std::string d) { m_info.description = std::move(d); return *this; }
    ComponentBuilder& userAddable(bool v) { m_info.userAddable = v; return *this; }
    ComponentBuilder& serializable(bool v) { m_info.serializable = v; return *this; }

    /// "linearDamping" -> "Linear Damping"
    static std::string prettify(const std::string& s) {
        std::string out;
        for (usize i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (i == 0) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            else if (std::isupper(static_cast<unsigned char>(c))) out += ' ';
            out += c;
        }
        return out;
    }

private:
    ComponentInfo& m_info;
};

/// Process-wide catalogue of reflected component types.
class ComponentRegistry {
public:
    static ComponentRegistry& instance();
    /// Redirects instance() to the host's registry (used inside plugin libraries).
    static void setInstanceOverride(ComponentRegistry* host);

    template <typename C>
    ComponentBuilder<C> add(std::string name) {
        const ecs::ComponentTypeId id = ecs::componentTypeId<C>();
        auto& info = m_byId[id];
        info = ComponentInfo{};
        info.name = std::move(name);
        info.category = "General";
        info.id = id;
        info.makePool = [] { return std::make_unique<ecs::Pool<C>>(); };
        m_byName[info.name] = id;
        return ComponentBuilder<C>(info);
    }

    const ComponentInfo* find(ecs::ComponentTypeId id) const;
    const ComponentInfo* find(std::string_view name) const;
    template <typename C>
    const ComponentInfo* find() const { return find(ecs::componentTypeId<C>()); }

    /// Unregisters a component (plugins call this in onUnload).
    void remove(ecs::ComponentTypeId id);

    /// Sorted by category then name (stable editor menus).
    std::vector<const ComponentInfo*> all() const;

private:
    std::unordered_map<ecs::ComponentTypeId, ComponentInfo> m_byId;
    std::unordered_map<std::string, ecs::ComponentTypeId> m_byName;
};

} // namespace agr
