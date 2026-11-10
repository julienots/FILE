// Built-in shaders, AGR GLSL dialect (see rhi::ShaderSource).
// Lighting is computed in linear space; output is gamma-encoded (sRGB) for an
// 8-bit target. PBR replaces the Blinn-Phong model in phase 5.
namespace agr::shaders {

inline constexpr const char* kFrameBlock = R"(
layout(std140) uniform FrameData {
    mat4 u_viewProj;
    vec4 u_cameraPos;
    vec4 u_lightDir;    // direction the light travels, normalized
    vec4 u_lightColor;  // linear rgb * intensity
    vec4 u_ambient;     // linear rgb * intensity
};
)";

inline constexpr const char* kObjectBlock = R"(
layout(std140) uniform ObjectData {
    mat4 u_model;
    mat4 u_normalMatrix;
    vec4 u_color;    // linear rgb, alpha
    vec4 u_uvScale;  // xy
};
)";

inline constexpr const char* kMeshVertex = R"(
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_worldPos;
void main() {
    vec4 world = u_model * vec4(a_position, 1.0);
    v_worldPos = world.xyz;
    v_normal = mat3(u_normalMatrix) * a_normal;
    v_uv = a_uv * u_uvScale.xy;
    gl_Position = u_viewProj * world;
}
)";

inline constexpr const char* kMeshFragment = R"(
uniform sampler2D u_albedo;
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_worldPos;
layout(location = 0) out vec4 o_color;
void main() {
    vec4 base = texture(u_albedo, v_uv) * u_color;
    vec3 n = normalize(v_normal);
    vec3 l = -u_lightDir.xyz;
    float diffuse = max(dot(n, l), 0.0);
    vec3 viewDir = normalize(u_cameraPos.xyz - v_worldPos);
    vec3 h = normalize(viewDir + l);
    float specular = diffuse > 0.0 ? pow(max(dot(n, h), 0.0), 32.0) * 0.2 : 0.0;
    vec3 lit = base.rgb * (u_ambient.rgb + u_lightColor.rgb * diffuse) + u_lightColor.rgb * specular;
    o_color = vec4(pow(clamp(lit, 0.0, 1.0), vec3(1.0 / 2.2)), base.a);
}
)";

inline constexpr const char* kSpriteVertex = R"(
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
out vec2 v_uv;
out vec4 v_color;
void main() {
    v_uv = a_uv;
    v_color = vec4(pow(a_color.rgb, vec3(2.2)), a_color.a);
    gl_Position = u_viewProj * vec4(a_position, 1.0);
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
