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
    mat4 u_shadowMatrix[4]; // world -> shadow atlas (uv, depth), per cascade
    vec4 u_shadowRect[4];   // atlas uv bounds of each cascade
    vec4 u_shadowSplits;    // view depth where each cascade ends
    vec4 u_shadowOffsets;   // normal offset (world) per cascade
    vec4 u_shadowView;      // camera forward
    vec4 u_shadowParams;    // cascade count (0 = off), depth bias, blend band (fraction), strength
    vec4 u_shadowParams2;   // atlas texel size (uv), PCF radius (0 or 1), -, -
    vec4 u_envSH[9];        // image sky: irradiance / pi, SH9
    vec4 u_envParams;       // image sky ready, intensity, rotation (radians), last mip level
    vec4 u_time;            // seconds (wrapped every hour)
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
    vec3 lit = c.rgb * light;
    o_color = vec4(u_fogParams.w > 0.5 ? lit : pow(clamp(lit, 0.0, 1.0), vec3(1.0 / 2.2)), c.a);
}
)";

inline constexpr const char* kSpriteFragment = R"(
uniform sampler2D u_texture;
in vec2 v_uv;
in vec4 v_color;
layout(location = 0) out vec4 o_color;
void main() {
    vec4 c = texture(u_texture, v_uv) * v_color;
    o_color = vec4(u_fogParams.w > 0.5 ? c.rgb : pow(c.rgb, vec3(1.0 / 2.2)), c.a);  // linear for the HDR post chain
}
)";

// ----------------------------------------------------------------- post-processing
// Full-screen triangle; texture coordinates come from gl_FragCoord (same convention for
// render targets on every backend).
inline constexpr const char* kPostVertex = R"(
layout(location = 0) in vec2 a_position;
void main() { gl_Position = vec4(a_position, 0.0, 1.0); }
)";

inline constexpr const char* kPostBlock = R"(
layout(std140) uniform PostData {
    vec4 u_texel;      // 1 / source size (xy), 1 / target size (zw)
    vec4 u_bloom;      // threshold, soft knee, intensity, upsample radius
    vec4 u_grade;      // contrast, saturation, vignette, 1 = filmic tone mapping (HDR input)
    vec4 u_filter;     // color filter (rgb), 1 = HDR input
};
)";

/// Bright pass + first half-resolution downsample (soft threshold, firefly clamp).
inline constexpr const char* kBloomPrefilter = R"(
uniform sampler2D u_source;
layout(location = 0) out vec4 o_color;
vec3 sampleBox(vec2 uv) {
    vec2 d = u_texel.xy;
    return 0.25 * (texture(u_source, uv + vec2(-d.x, -d.y)).rgb + texture(u_source, uv + vec2(d.x, -d.y)).rgb +
                   texture(u_source, uv + vec2(-d.x, d.y)).rgb + texture(u_source, uv + vec2(d.x, d.y)).rgb);
}
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec3 c = min(sampleBox(uv), vec3(64.0));
    float b = max(c.r, max(c.g, c.b));
    float knee = u_bloom.x * u_bloom.y;
    float soft = clamp(b - u_bloom.x + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-5);
    float contribution = max(soft, b - u_bloom.x) / max(b, 1e-5);
    o_color = vec4(c * contribution, 1.0);
}
)";

inline constexpr const char* kBloomDownsample = R"(
uniform sampler2D u_source;
layout(location = 0) out vec4 o_color;
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec2 d = u_texel.xy;
    vec3 c = 0.5 * texture(u_source, uv).rgb;
    c += 0.125 * (texture(u_source, uv + vec2(-d.x, -d.y)).rgb + texture(u_source, uv + vec2(d.x, -d.y)).rgb +
                  texture(u_source, uv + vec2(-d.x, d.y)).rgb + texture(u_source, uv + vec2(d.x, d.y)).rgb);
    o_color = vec4(c, 1.0);
}
)";

/// 3x3 tent filter of the smaller level, added (blending) to the larger one.
inline constexpr const char* kBloomUpsample = R"(
uniform sampler2D u_source;
layout(location = 0) out vec4 o_color;
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec2 d = u_texel.xy * u_bloom.w;
    vec3 c = 4.0 * texture(u_source, uv).rgb;
    c += 2.0 * (texture(u_source, uv + vec2(d.x, 0.0)).rgb + texture(u_source, uv - vec2(d.x, 0.0)).rgb +
                texture(u_source, uv + vec2(0.0, d.y)).rgb + texture(u_source, uv - vec2(0.0, d.y)).rgb);
    c += texture(u_source, uv + d).rgb + texture(u_source, uv - d).rgb + texture(u_source, uv + vec2(d.x, -d.y)).rgb +
         texture(u_source, uv + vec2(-d.x, d.y)).rgb;
    o_color = vec4(c / 16.0, 1.0);
}
)";

/// Scene + bloom, tone mapping (HDR), color grading, vignette; luma in alpha for FXAA.
inline constexpr const char* kComposite = R"(
uniform sampler2D u_scene;
uniform sampler2D u_bloomTex;
layout(location = 0) out vec4 o_color;
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec3 c = texture(u_scene, uv).rgb;
    if (u_bloom.z > 0.0) c += texture(u_bloomTex, uv).rgb * u_bloom.z;
    if (u_filter.w > 0.5) {  // HDR input: linear light -> display
        c *= u_filter.rgb;
        if (u_grade.w > 0.5) c = clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
        c = pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
    } else {
        c *= u_filter.rgb;
    }
    float luma = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(luma), c, u_grade.y);
    c = (c - 0.5) * u_grade.x + 0.5;
    vec2 v = uv - 0.5;
    c *= 1.0 - u_grade.z * smoothstep(0.35, 0.95, length(v) * 1.414);
    c = clamp(c, 0.0, 1.0);
    o_color = vec4(c, dot(c, vec3(0.299, 0.587, 0.114)));
}
)";

/// FXAA (Lottes, "FXAA 3.11 PC" reduced form): luma from alpha.
inline constexpr const char* kFxaa = R"(
uniform sampler2D u_source;
layout(location = 0) out vec4 o_color;
void main() {
    vec2 rcp = u_texel.xy;
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    float lumaNW = texture(u_source, uv + vec2(-1.0, -1.0) * rcp).a;
    float lumaNE = texture(u_source, uv + vec2(1.0, -1.0) * rcp).a;
    float lumaSW = texture(u_source, uv + vec2(-1.0, 1.0) * rcp).a;
    float lumaSE = texture(u_source, uv + vec2(1.0, 1.0) * rcp).a;
    vec4 center = texture(u_source, uv);
    float lumaM = center.a;
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    vec2 dir = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)), (lumaNW + lumaSW) - (lumaNE + lumaSE));
    float reduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 / 8.0), 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * rcp;
    vec3 a = 0.5 * (texture(u_source, uv + dir * (1.0 / 3.0 - 0.5)).rgb + texture(u_source, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 b = a * 0.5 + 0.25 * (texture(u_source, uv - dir * 0.5).rgb + texture(u_source, uv + dir * 0.5).rgb);
    float lumaB = dot(b, vec3(0.299, 0.587, 0.114));
    o_color = vec4((lumaB < lumaMin || lumaB > lumaMax) ? a : b, 1.0);
}
)";

} // namespace agr::shaders
