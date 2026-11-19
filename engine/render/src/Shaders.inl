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
    vec4 u_time;            // seconds (wrapped every hour), -, 1 / render width, 1 / render height
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
    vec4 u_lightShadow[16];        // shadow row (-1: none), softness (texels), strength, penetration / radius
    ivec4 u_lightCount;
};
uniform sampler2D u_texture;
uniform sampler2D u_shadow2D;  // polar shadow maps: one row per shadowed light, distance / radius in r (high) g (low)
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
        float dist = length(d) / u_lightPosRadius[i].w;
        float att = clamp(1.0 - dist, 0.0, 1.0);
        float lit = 1.0;
        if (u_lightShadow[i].x > -0.5 && att > 0.0) {
            // Polar shadow map: nearest occluder in this direction; 5 taps across directions soften the edge.
            float u = atan(d.y, d.x) / 6.2831853 + 0.5;
            float v = (u_lightShadow[i].x + 0.5) / 8.0;
            float spread = u_lightShadow[i].y / 512.0;
            float sum = 0.0;
            for (int k = -2; k <= 2; ++k) {
                vec4 t = texture(u_shadow2D, vec2(u + float(k) * spread * 0.5, v));
                float stored = (t.r * 255.0 * 256.0 + t.g * 255.0) / 65535.0;
                sum += dist <= stored + 0.002 + u_lightShadow[i].w ? 1.0 : 0.0;
            }
            lit = mix(1.0, sum / 5.0, u_lightShadow[i].z);
        }
        light += u_lightColorFalloff[i].rgb * pow(att, u_lightColorFalloff[i].w) * lit;
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
    vec4 u_ao;         // radius (world), intensity, 1 = ambient occlusion on, 1 = GL depth range (-1..1), 0 = Vulkan (0..1)
    mat4 u_proj;       // camera projection (SSAO)
    mat4 u_invProj;
};
)";

/// Screen-space ambient occlusion (half resolution) from the scene depth: view-space position
/// and normal rebuilt from depth, 12 samples in the normal hemisphere turned per pixel, range
/// check so that far objects do not darken near ones. Output: visibility in r (1 = open).
inline constexpr const char* kSsao = R"(
uniform sampler2D u_source;  // scene depth
layout(location = 0) out vec4 o_color;
// Target texture coordinates <-> clip space: identical on every backend (the Vulkan RHI flips
// its viewport to match OpenGL); only the depth range differs.
vec2 toNdc(vec2 uv) { return uv * 2.0 - 1.0; }
vec2 toUv(vec2 ndc) { return ndc * 0.5 + 0.5; }
vec3 viewPos(vec2 uv) {
    float d = texture(u_source, uv).r;
    float z = u_ao.w > 0.5 ? d * 2.0 - 1.0 : d;
    vec4 p = u_invProj * vec4(toNdc(uv), z, 1.0);
    return p.xyz / p.w;
}
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    if (texture(u_source, uv).r >= 0.99999) { o_color = vec4(1.0); return; }  // sky
    vec3 P = viewPos(uv);
    // Normal from the smaller depth step on each axis (no halo across silhouettes).
    vec2 px = u_texel.xy;
    vec3 r = viewPos(uv + vec2(px.x, 0.0)) - P, l = P - viewPos(uv - vec2(px.x, 0.0));
    vec3 t = viewPos(uv + vec2(0.0, px.y)) - P, b = P - viewPos(uv - vec2(0.0, px.y));
    vec3 dx = abs(r.z) < abs(l.z) ? r : l;
    vec3 dy = abs(t.z) < abs(b.z) ? t : b;
    vec3 N = normalize(cross(dx, dy));
    if (N.z < 0.0) N = -N;  // toward the camera (view space looks down -z)
    // Per-pixel rotation of a fixed spiral kernel, from a 4 x 4 pattern (dithered order): the
    // 4 x 4 blur then averages all 16 rotations exactly and the noise cancels out.
    vec2 cell = mod(floor(gl_FragCoord.xy), 4.0);
    float noise = (cell.x + 4.0 * cell.y + 0.5) / 16.0;  // each 4 x 4 window holds the 16 rotations once
    vec3 rv = vec3(cos(noise * 6.2831853), sin(noise * 6.2831853), 0.0);
    vec3 T = normalize(rv - N * dot(rv, N));
    vec3 B = cross(N, T);
    float radius = u_ao.x;
    float bias = 0.05 * radius - 0.004 * P.z;  // grows with distance: 16-bit depth steps
    float occlusion = 0.0;
    for (int i = 0; i < 12; ++i) {
        float fi = float(i);
        float a = fi * 2.3999632;                 // golden angle
        float h = mix(0.2, 1.0, (fi + 0.5) / 12.0);  // away from the tangent plane (depth precision)
        float rr = sqrt(1.0 - h * h);
        vec3 k = vec3(cos(a) * rr, sin(a) * rr, h) * mix(0.15, 1.0, (fi + 1.0) / 12.0);  // denser near the point
        vec3 S = P + (T * k.x + B * k.y + N * k.z) * radius;
        vec4 c = u_proj * vec4(S, 1.0);
        vec2 suv = toUv(c.xy / c.w);
        if (suv.x < 0.0 || suv.y < 0.0 || suv.x > 1.0 || suv.y > 1.0) continue;
        float sceneZ = viewPos(suv).z;
        float range = smoothstep(0.0, 1.0, radius / max(abs(P.z - sceneZ), 1e-4));
        occlusion += (sceneZ >= S.z + bias ? 1.0 : 0.0) * range;
    }
    float ao = clamp(1.0 - occlusion / 12.0 * u_ao.y, 0.0, 1.0);
    o_color = vec4(ao, ao, ao, 1.0);
}
)";

/// 4 x 4 box blur of the occlusion (removes the rotation noise).
inline constexpr const char* kSsaoBlur = R"(
uniform sampler2D u_source;
layout(location = 0) out vec4 o_color;
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    float sum = 0.0;
    for (int y = -2; y < 2; ++y)
        for (int x = -2; x < 2; ++x) sum += texture(u_source, uv + (vec2(float(x), float(y)) + 0.5) * u_texel.xy).r;
    float ao = sum / 16.0;
    o_color = vec4(ao, ao, ao, 1.0);
}
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
uniform sampler2D u_aoTex;
layout(location = 0) out vec4 o_color;
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec3 c = texture(u_scene, uv).rgb;
    if (u_ao.z > 0.5) c *= texture(u_aoTex, uv).r;  // before tone mapping, like light
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
