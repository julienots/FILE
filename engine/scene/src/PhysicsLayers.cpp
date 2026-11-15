#include "agr/scene/PhysicsLayers.hpp"

#include <nlohmann/json.hpp>

namespace agr {

using json = nlohmann::json;

std::string PhysicsLayers::displayName(u32 layer) const {
    if (layer >= kCount) return "?";
    if (!names[layer].empty()) return names[layer];
    return layer == 0 ? std::string("Default") : "Couche " + std::to_string(layer);
}

std::optional<u32> PhysicsLayers::find(std::string_view name) const {
    for (u32 i = 0; i < kCount; ++i)
        if (displayName(i) == name) return i;
    return std::nullopt;
}

std::string PhysicsLayers::toJson() const {
    json j;
    j["layers"] = json::array();
    for (const std::string& n : names) j["layers"].push_back(n);
    return j.dump(2);
}

Status PhysicsLayers::fromJson(std::string_view text) {
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, "physics layers: invalid JSON");
    const auto it = j.find("layers");
    if (it == j.end() || !it->is_array()) return makeError(Error::Code::ParseError, "physics layers: \"layers\" must be an array of names");
    std::array<std::string, kCount> read{};
    for (u32 i = 0; i < kCount && i < it->size(); ++i) {
        if (!(*it)[i].is_string()) return makeError(Error::Code::ParseError, "physics layers: layer " + std::to_string(i) + " is not a string");
        read[i] = (*it)[i].get<std::string>();
    }
    names = std::move(read);
    return {};
}

Status PhysicsLayers::load(const VirtualFileSystem& vfs, const std::string& uri) {
    m_loadedTime = vfs.modifiedTime(uri);
    if (!vfs.exists(uri)) {
        names = {};
        return {};
    }
    auto text = vfs.readText(uri);
    if (!text) return text.error();
    Status st = fromJson(text.value());
    if (!st) return makeError(st.error().code, uri + ": " + st.error().message);
    return {};
}

Status PhysicsLayers::save(VirtualFileSystem& vfs, const std::string& uri) const {
    Status st = vfs.writeText(uri, toJson());
    return st;
}

bool PhysicsLayers::refresh(const VirtualFileSystem& vfs, const std::string& uri) {
    const i64 t = vfs.exists(uri) ? vfs.modifiedTime(uri) : -1;
    if (t == m_loadedTime) return false;
    const auto before = names;
    if (!load(vfs, uri)) names = {};
    m_loadedTime = t;
    return before != names;
}

} // namespace agr
