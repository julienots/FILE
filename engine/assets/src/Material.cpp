#include "agr/assets/Material.hpp"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace agr {

using json = nlohmann::json;

namespace {

constexpr int kVersion = 1;

json color(const math::Color& c) { return {c.x, c.y, c.z, c.w}; }
math::Color readColor(const json& j, math::Color def) {
    if (!j.is_array() || j.size() < 3) return def;
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j.size() > 3 ? j[3].get<float>() : 1.0f};
}
math::Vec2 readVec2(const json& j, math::Vec2 def) {
    if (!j.is_array() || j.size() != 2) return def;
    return {j[0].get<float>(), j[1].get<float>()};
}
const char* alphaName(AlphaMode m) {
    switch (m) {
        case AlphaMode::Opaque: return "opaque";
        case AlphaMode::Mask: return "mask";
        case AlphaMode::Blend: return "blend";
    }
    return "opaque";
}

} // namespace

float srgbToLinearChannel(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgbChannel(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

Result<MaterialData> parseMaterial(std::string_view text) {
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object() || j.value("format", "") != "agr.material")
        return makeError(Error::Code::ParseError, "not an AGR material (.agrmat)");
    if (j.value("version", 0) > kVersion) return makeError(Error::Code::VersionMismatch, "material from a newer engine");
    MaterialData m;
    try {
        m.name = j.value("name", std::string());
        m.baseColor = readColor(j.value("baseColor", json()), m.baseColor);
        m.baseColorTexture = j.value("baseColorTexture", std::string());
        m.metallic = std::clamp(j.value("metallic", m.metallic), 0.0f, 1.0f);
        m.roughness = std::clamp(j.value("roughness", m.roughness), 0.0f, 1.0f);
        m.metallicRoughnessTexture = j.value("metallicRoughnessTexture", std::string());
        m.normalTexture = j.value("normalTexture", std::string());
        m.normalScale = j.value("normalScale", m.normalScale);
        m.occlusionTexture = j.value("occlusionTexture", std::string());
        m.occlusionStrength = std::clamp(j.value("occlusionStrength", m.occlusionStrength), 0.0f, 1.0f);
        m.emissive = readColor(j.value("emissive", json()), m.emissive);
        m.emissiveIntensity = std::max(0.0f, j.value("emissiveIntensity", m.emissiveIntensity));
        m.emissiveTexture = j.value("emissiveTexture", std::string());
        const std::string alpha = j.value("alphaMode", std::string("opaque"));
        if (alpha == "mask") m.alphaMode = AlphaMode::Mask;
        else if (alpha == "blend") m.alphaMode = AlphaMode::Blend;
        else if (alpha != "opaque") return makeError(Error::Code::ParseError, "unknown alphaMode '" + alpha + "'");
        m.alphaCutoff = std::clamp(j.value("alphaCutoff", m.alphaCutoff), 0.0f, 1.0f);
        m.doubleSided = j.value("doubleSided", false);
        m.unlit = j.value("unlit", false);
        m.uvScale = readVec2(j.value("uvScale", json()), m.uvScale);
        m.uvOffset = readVec2(j.value("uvOffset", json()), m.uvOffset);
    } catch (const std::exception& e) {
        return makeError(Error::Code::ParseError, std::string("material: ") + e.what());
    }
    return m;
}

std::string materialToJson(const MaterialData& m) {
    json j;
    j["format"] = "agr.material";
    j["version"] = kVersion;
    j["name"] = m.name;
    j["baseColor"] = color(m.baseColor);
    j["baseColorTexture"] = m.baseColorTexture;
    j["metallic"] = m.metallic;
    j["roughness"] = m.roughness;
    j["metallicRoughnessTexture"] = m.metallicRoughnessTexture;
    j["normalTexture"] = m.normalTexture;
    j["normalScale"] = m.normalScale;
    j["occlusionTexture"] = m.occlusionTexture;
    j["occlusionStrength"] = m.occlusionStrength;
    j["emissive"] = color(m.emissive);
    j["emissiveIntensity"] = m.emissiveIntensity;
    j["emissiveTexture"] = m.emissiveTexture;
    j["alphaMode"] = alphaName(m.alphaMode);
    j["alphaCutoff"] = m.alphaCutoff;
    j["doubleSided"] = m.doubleSided;
    j["unlit"] = m.unlit;
    j["uvScale"] = {m.uvScale.x, m.uvScale.y};
    j["uvOffset"] = {m.uvOffset.x, m.uvOffset.y};
    return j.dump(2);
}

} // namespace agr
