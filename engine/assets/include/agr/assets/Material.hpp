#pragma once
#include <map>
#include <string>
#include <string_view>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

enum class AlphaMode : i32 {
    Opaque = 0,
    Mask = 1,   ///< Alpha test against alphaCutoff (foliage, fences).
    Blend = 2,  ///< Transparent: sorted back to front, no depth write.
};

/// Lighting model of a material.
enum class ShadingModel : i32 {
    Standard = 0,  ///< Physically based (GGX, metallic-roughness).
    Toon = 1,      ///< Stylized: banded light, tinted shadows, sharp highlight, rim light.
};

/// PBR material, metallic-roughness workflow (the glTF 2.0 model).
/// Stored as `.agrmat` JSON files ("agr.material", version 1); also produced by the
/// model importers for the materials embedded in glTF / OBJ / FBX files.
/// Colors are sRGB like every color edited in the engine; textures are asset URIs.
struct MaterialData {
    std::string name;
    math::Color baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    std::string baseColorTexture;          ///< sRGB color (alpha = opacity).
    float metallic = 0.0f;
    float roughness = 0.5f;
    std::string metallicRoughnessTexture;  ///< Linear: G = roughness, B = metallic (glTF packing).
    std::string normalTexture;             ///< Tangent-space normal map (linear, +Y up / OpenGL convention).
    float normalScale = 1.0f;
    std::string occlusionTexture;          ///< Linear: R = ambient occlusion.
    float occlusionStrength = 1.0f;
    math::Color emissive{0.0f, 0.0f, 0.0f, 1.0f};
    float emissiveIntensity = 1.0f;
    std::string emissiveTexture;
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool unlit = false;                    ///< Shows the base color as is (no lighting).
    // ---- Stylized shading (shading == Toon), outlines (any shading model)
    ShadingModel shading = ShadingModel::Standard;
    i32 toonBands = 1;                          ///< Light steps between shadow and full light (1..8).
    float toonSoftness = 0.04f;                 ///< Width of the band edges (0 = hard).
    math::Color shadowColor{0.55f, 0.5f, 0.78f, 1.0f};  ///< Tint of the shadowed side (x base color).
    float specularSize = 0.0f;                  ///< Sharp highlight size, 0 = none.
    float rimStrength = 0.0f;                   ///< Rim light on the silhouette, 0 = none.
    float rimWidth = 0.3f;
    math::Color rimColor{1.0f, 1.0f, 1.0f, 1.0f};
    float outlineWidth = 0.0f;                  ///< Outline thickness in pixels, 0 = none.
    math::Color outlineColor{0.1f, 0.07f, 0.12f, 1.0f};

    math::Vec2 uvScale{1.0f, 1.0f};
    math::Vec2 uvOffset{0.0f, 0.0f};
    /// Custom surface shader (`.agrshader`, empty = standard PBR) and the values of its
    /// parameters that differ from the shader's defaults (colors in sRGB, like other colors).
    std::string shader;
    std::map<std::string, math::Vec4> shaderParams;
    std::map<std::string, std::string> shaderTextures;

    bool operator==(const MaterialData&) const = default;
};

inline constexpr const char* kMaterialExtension = ".agrmat";

/// Parses an `.agrmat` document. Unknown keys are ignored; missing keys keep their defaults.
Result<MaterialData> parseMaterial(std::string_view json);
std::string materialToJson(const MaterialData& material);

/// sRGB <-> linear conversion of a color channel (exact piecewise sRGB curve).
float srgbToLinearChannel(float c);
float linearToSrgbChannel(float c);

} // namespace agr
