#pragma once
// Custom surface shaders (`.agrshader`): GLSL functions plugged into the PBR pipeline.
//
//     param color tint = (1.0, 0.5, 0.2, 1.0);
//     param float speed = 2.0 range(0, 10);
//     param texture noise = "assets://textures/noise.png";      (add `srgb` for color images)
//
//     void vertex(inout Vertex v) { v.position.y += sin(TIME * speed + v.position.x) * 0.1; }
//     void surface(inout Surface s) { s.baseColor *= tint.rgb; s.emissive += texture(noise, s.uv).rgb; }
//
// `param` lines declare material parameters (one vec4 slot each, at most kMaxShaderVectors, plus
// kMaxShaderTextures textures); materials override their values. The rest is GLSL: `surface`
// (fragment) and / or `vertex` (world-space vertex, also used by the shadow pass) plus helpers.
// See docs/SHADERS.md.
#include <string>
#include <string_view>
#include <vector>

#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

inline constexpr const char* kShaderExtension = ".agrshader";
inline constexpr u32 kMaxShaderVectors = 8;
inline constexpr u32 kMaxShaderTextures = 4;

enum class ShaderParamType : u8 { Float, Vec2, Vec3, Vec4, Color, Texture };

struct ShaderParam {
    std::string name;
    ShaderParamType type = ShaderParamType::Float;
    math::Vec4 defaultValue{0.0f, 0.0f, 0.0f, 0.0f};  ///< Color: sRGB as written (converted to linear on upload).
    std::string defaultTexture;                        ///< Texture: asset URI ("" = white).
    bool srgb = false;                                 ///< Texture: color image (`srgb` keyword); else data.
    float rangeMin = 0.0f, rangeMax = 0.0f;            ///< Float slider range; min == max: no range.
    u32 slot = 0;                                      ///< Vector slot (u_custom[slot]) or texture index.
    u32 line = 0;                                      ///< 1-based line of the declaration.
};

/// A compiler or parser message about a line of the user's file (line 0: not tied to a line).
struct ShaderDiagnostic {
    u32 line = 0;
    std::string message;
    bool operator==(const ShaderDiagnostic&) const = default;
};

struct CustomShader {
    std::vector<ShaderParam> params;
    /// The user's GLSL with `param` lines blanked: same line numbers as the file.
    std::string code;
    bool hasSurface = false;
    bool hasVertex = false;
    u32 vectorCount = 0, textureCount = 0;
    std::vector<ShaderDiagnostic> errors;  ///< Parse errors; empty when the file is usable.

    bool valid() const { return errors.empty(); }
    const ShaderParam* find(std::string_view name) const;
};

/// Parses a `.agrshader` source. Never fails as a whole: problems are in `errors`.
CustomShader parseCustomShader(std::string_view source);

/// `code` with the body of the top-level function `name` (e.g. "surface") replaced by blank
/// space, newlines kept. Used to compile each stage without the other stage's entry point.
std::string blankFunction(std::string_view code, std::string_view name);

/// GLSL `#define`s mapping parameter names to the uniforms holding them
/// (`u_custom[i]` components; textures to `u_customMap<i>` when `textures` is true).
std::string shaderParamDefines(const CustomShader& shader, bool textures);

/// Extracts the messages about source string 1 (the user's code, after `#line 1 1`) from a GL
/// (Mesa / drivers) or glslang compile log. Other messages get line 0.
std::vector<ShaderDiagnostic> parseShaderLog(std::string_view log);

/// Starting point for a new `.agrshader` file.
std::string defaultCustomShaderSource();

} // namespace agr
