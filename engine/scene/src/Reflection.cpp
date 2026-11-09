#include "agr/scene/Reflection.hpp"

#include <algorithm>

namespace agr {

const char* enumName(FieldType type) {
    switch (type) {
        case FieldType::Bool: return "bool";
        case FieldType::Int32: return "int";
        case FieldType::UInt32: return "uint";
        case FieldType::Float: return "float";
        case FieldType::Vec2: return "vec2";
        case FieldType::Vec3: return "vec3";
        case FieldType::Vec4: return "vec4";
        case FieldType::Quat: return "quat";
        case FieldType::String: return "string";
        case FieldType::Entity: return "entity";
        case FieldType::Enum: return "enum";
    }
    return "?";
}

const FieldInfo* ComponentInfo::field(std::string_view fieldName) const {
    for (const auto& f : fields)
        if (f.name == fieldName) return &f;
    return nullptr;
}

namespace {
ComponentRegistry* g_override = nullptr;
}

ComponentRegistry& ComponentRegistry::instance() {
    static ComponentRegistry registry;
    return g_override ? *g_override : registry;
}

void ComponentRegistry::setInstanceOverride(ComponentRegistry* host) { g_override = host; }

void ComponentRegistry::remove(ecs::ComponentTypeId id) {
    auto it = m_byId.find(id);
    if (it == m_byId.end()) return;
    m_byName.erase(it->second.name);
    m_byId.erase(it);
}

const ComponentInfo* ComponentRegistry::find(ecs::ComponentTypeId id) const {
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &it->second;
}

const ComponentInfo* ComponentRegistry::find(std::string_view name) const {
    auto it = m_byName.find(std::string(name));
    return it == m_byName.end() ? nullptr : find(it->second);
}

std::vector<const ComponentInfo*> ComponentRegistry::all() const {
    std::vector<const ComponentInfo*> out;
    out.reserve(m_byId.size());
    for (const auto& [id, info] : m_byId) out.push_back(&info);
    std::sort(out.begin(), out.end(), [](auto* a, auto* b) {
        return a->category != b->category ? a->category < b->category : a->name < b->name;
    });
    return out;
}

} // namespace agr
