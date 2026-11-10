// Built-in shaders, AGR GLSL dialect (see rhi::ShaderSource).
// Lighting is computed in linear space; output is gamma-encoded (sRGB) for an
// 8-bit target. 3D shaders (PBR, shadows, sky) are in Shaders3D.inl.
namespace agr::shaders {

inline constexpr const char* kFrameBlock = R"(
layout(std140) uniform FrameData {
    mat4 u_viewProj;
    mat4 u_invViewProj;
    vec4 u_cameraPos;
    vec4 u_lightDir;      // direction the light travels, normalized
    vec4 u_lightColor;    // linear rgb * intensity
    vec4 u_ambient;       // flat ambient, linear rgb * intensity (sprites, CameraColor sky)
    vec4 u_skyColor;      // linear rgb; w = 1: gradient sky + hemisphere ambient
    vec4 u_groundColor;   // linear rgb; w = ambient intensity
    vec4 u_horizonColor;  // linear rgb; w = 1: sun disc
    vec4 u_fogColor;      // linear rgb; w = 1: fog enabled
    vec4 u_fogParams;     // start, end, exposure, -
    mat4 u_shadowMatrix;  // world -> shadow map (uv, depth)
    vec4 u_shadowParams;  // enabled, depth bias, normal offset (world), strength
    vec4 u_shadowParams2; // texel size (uv), PCF radius (0 or 1), -, -
};
)";

inline constexpr const char* kSpriteVertex = R"(
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
out vec2 v_uv;
out vec4 v_color;
out vec3 v_worldPos;
void main() {
    v_uv = a_uv;
    v_color = vec4(pow(a_color.rgb, vec3(2.2)), a_color.a);
    v_worldPos = a_position;
    gl_Position = u_viewProj * vec4(a_position, 1.0);
}
)";

/// Sprites lit by up to 16 point lights (Light2D) plus the scene ambient.
inline constexpr const char* kSpriteLitFragment = R"(
layout(std140) uniform Lights2D {
    vec4 u_lightPosRadius[16];     // xyz position, w radius
    vec4 u_lightColorFalloff[16];  // rgb linear color * intensity, w falloff exponent
    ivec4 u_lightCount;
};
uniform sampler2D u_texture;
in vec2 v_uv;
in vec4 v_color;
in vec3 v_worldPos;
layout(location = 0) out vec4 o_color;
void main() {
    vec4 c = texture(u_texture, v_uv) * v_color;
    vec3 light = u_ambient.rgb;
    for (int i = 0; i < 16; ++i) {
        if (i >= u_lightCount.x) break;
        vec2 d = v_worldPos.xy - u_lightPosRadius[i].xy;
        float att = clamp(1.0 - length(d) / u_lightPosRadius[i].w, 0.0, 1.0);
        light += u_lightColorFalloff[i].rgb * pow(att, u_lightColorFalloff[i].w);
    }
    o_color = vec4(pow(clamp(c.rgb * light, 0.0, 1.0), vec3(1.0 / 2.2)), c.a);
}
)";

inline constexpr const char* kSpriteFragment = R"(
uniform sampler2D u_texture;
in vec2 v_uv;
in vec4 v_color;
layout(location = 0) out vec4 o_color;
void main() {
    vec4 c = texture(u_texture, v_uv) * v_color;
    o_color = vec4(pow(c.rgb, vec3(1.0 / 2.2)), c.a);
}
)";

} // namespace agr::shaders
