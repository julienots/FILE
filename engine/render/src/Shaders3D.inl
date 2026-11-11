// 3D shaders, AGR GLSL dialect: physically based shading (metallic-roughness,
// GGX), directional + point + spot lights, shadow map with PCF, hemisphere
// ambient, fog, ACES tone mapping. Prepended by kFrameBlock (Shaders.inl).
// Variants are selected with #defines: AGR_ALPHA_MASK, AGR_SHADOW_MASK.
namespace agr::shaders {

inline constexpr const char* kMaterialBlock = R"(
layout(std140) uniform MaterialData {
    vec4 u_baseColor;    // linear rgb, alpha
    vec4 u_emissive;     // linear rgb * intensity
    vec4 u_pbr;          // metallic, roughness, normal scale, occlusion strength
    vec4 u_uvTransform;  // scale xy, offset zw
    vec4 u_flags;        // alpha cutoff, unlit, -, -
};
)";

inline constexpr const char* kLightBlock = R"(
layout(std140) uniform LightData {
    vec4 u_lightPosRange[8];   // world position, range
    vec4 u_lightColorType[8];  // linear rgb * intensity, type (0 point, 1 spot)
    vec4 u_lightSpotDir[8];    // direction the light travels, cos(outer angle)
    vec4 u_lightSpotInner[8];  // cos(inner angle), -, -, -
    ivec4 u_lightCount;
};
)";

/// Shared: tone mapping and output encoding.
inline constexpr const char* kToneMapping = R"(
// u_fogParams.z = exposure; u_fogParams.w = 1 when post-processing tone maps the frame (HDR
// target): shaders then output linear light.
vec3 agrToneMap(vec3 c) {
    c *= u_fogParams.z;  // exposure
    if (u_fogParams.w > 0.5) return c;
    // ACES filmic curve (Narkowicz 2015 fit).
    vec3 m = clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
    return pow(m, vec3(1.0 / 2.2));
}
vec3 agrUnlit(vec3 c) {
    if (u_fogParams.w > 0.5) return c;
    return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
}
)";

/// Declarations of skinned vertex shaders (only in the skinned pipeline variants: the
/// shader translator does not evaluate #ifdef around declarations).
inline constexpr const char* kSkinBlock = R"(
layout(location = 12) in vec4 a_joints;
layout(location = 13) in vec4 a_weights;
layout(std140) uniform SkinData {
    mat4 u_joints[128];  // inverse(mesh world) * joint world * inverse bind
};
)";

inline constexpr const char* kPbrVertex = R"(
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_tangent;
layout(location = 3) in vec2 a_uv;
layout(location = 4) in vec4 i_model0;
layout(location = 5) in vec4 i_model1;
layout(location = 6) in vec4 i_model2;
layout(location = 7) in vec4 i_model3;
layout(location = 8) in vec4 i_normal0;
layout(location = 9) in vec4 i_normal1;
layout(location = 10) in vec4 i_normal2;
layout(location = 11) in vec4 i_color;
out vec3 v_worldPos;
out vec3 v_normal;
out vec4 v_tangent;
out vec2 v_uv;
out vec4 v_color;
void main() {
    mat4 model = mat4(i_model0, i_model1, i_model2, i_model3);
#ifdef AGR_SKINNED
    mat4 skin = a_weights.x * u_joints[int(a_joints.x)] + a_weights.y * u_joints[int(a_joints.y)] +
                a_weights.z * u_joints[int(a_joints.z)] + a_weights.w * u_joints[int(a_joints.w)];
    vec3 position = (skin * vec4(a_position, 1.0)).xyz;
    vec3 normal = mat3(skin) * a_normal;
    vec3 tangent = mat3(skin) * a_tangent.xyz;
#else
    vec3 position = a_position;
    vec3 normal = a_normal;
    vec3 tangent = a_tangent.xyz;
#endif
    vec4 world = model * vec4(position, 1.0);
    v_worldPos = world.xyz;
    v_normal = mat3(i_normal0.xyz, i_normal1.xyz, i_normal2.xyz) * normal;
    v_tangent = vec4(mat3(model) * tangent, a_tangent.w);
    v_uv = a_uv * u_uvTransform.xy + u_uvTransform.zw;
    v_color = i_color;
    gl_Position = u_viewProj * world;
}
)";

/// Declarations of the terrain variant of the PBR fragment (splat map: layer weights).
inline constexpr const char* kTerrainBlock = R"(
uniform sampler2D u_splatMap;
)";

// Image sky (equirectangular, prefiltered mips): shared by the PBR and sky fragments.
inline constexpr const char* kEnvBlock = R"(
uniform sampler2D u_envMap;
vec3 agrEnvRotate(vec3 d) {
    float c = cos(u_envParams.z), s = sin(u_envParams.z);
    return vec3(c * d.x - s * d.z, d.y, s * d.x + c * d.z);  // world -> image: rotation by -angle around Y
}
vec2 agrEnvUv(vec3 d) {
    return vec2(0.5 + atan(d.x, -d.z) * 0.15915494, acos(clamp(d.y, -1.0, 1.0)) * 0.31830989);
}
vec3 agrEnvRadiance(vec3 dir, float roughness) {
    return textureLod(u_envMap, agrEnvUv(agrEnvRotate(normalize(dir))), roughness * u_envParams.w).rgb * u_envParams.y;
}
vec3 agrEnvIrradiance(vec3 n) {
    n = agrEnvRotate(n);
    vec3 e = u_envSH[0].rgb * 0.282095
           + u_envSH[1].rgb * (0.488603 * n.y) + u_envSH[2].rgb * (0.488603 * n.z) + u_envSH[3].rgb * (0.488603 * n.x)
           + u_envSH[4].rgb * (1.092548 * n.x * n.y) + u_envSH[5].rgb * (1.092548 * n.y * n.z)
           + u_envSH[6].rgb * (0.315392 * (3.0 * n.z * n.z - 1.0)) + u_envSH[7].rgb * (1.092548 * n.x * n.z)
           + u_envSH[8].rgb * (0.546274 * (n.x * n.x - n.y * n.y));
    return max(e, vec3(0.0)) * u_envParams.y;
}
)";

inline constexpr const char* kPbrFragment = R"(
uniform sampler2D u_baseColorMap;
uniform sampler2D u_metalRoughMap;
uniform sampler2D u_normalMap;
uniform sampler2D u_occlusionMap;
uniform sampler2D u_emissiveMap;
uniform sampler2D u_shadowMap;
in vec3 v_worldPos;
in vec3 v_normal;
in vec4 v_tangent;
in vec2 v_uv;
in vec4 v_color;
layout(location = 0) out vec4 o_color;

const float PI = 3.14159265;

float distributionGGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}
float visibilitySmithGGX(float NoV, float NoL, float a) {
    float a2 = a * a;
    float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(gv + gl, 1e-5);
}
vec3 fresnelSchlick(vec3 f0, float VoH) {
    float f = pow(1.0 - VoH, 5.0);
    return f0 + (1.0 - f0) * f;
}
// Radiance reflected toward the viewer for light of unit intensity from L. Lambert
// is not divided by PI: a white surface facing a light of intensity 1 shows white.
vec3 shade(vec3 N, vec3 V, vec3 L, vec3 diffuseColor, vec3 f0, float a) {
    float NoL = clamp(dot(N, L), 0.0, 1.0);
    if (NoL <= 0.0) return vec3(0.0);
    vec3 H = normalize(V + L);
    float NoV = abs(dot(N, V)) + 1e-4;
    float NoH = clamp(dot(N, H), 0.0, 1.0);
    float VoH = clamp(dot(V, H), 0.0, 1.0);
    vec3 F = fresnelSchlick(f0, VoH);
    vec3 specular = distributionGGX(NoH, a) * visibilitySmithGGX(NoV, NoL, a) * F * PI;
    return (diffuseColor * (1.0 - F) + specular) * NoL;
}
vec3 hemisphere(vec3 n) {
    if (u_skyColor.w > 0.5) return mix(u_groundColor.rgb, u_skyColor.rgb, n.y * 0.5 + 0.5) * u_groundColor.w;
    return u_ambient.rgb;
}
// Lit fraction (0..1) of the point in cascade i of the shadow atlas.
float cascadeLit(int i, vec3 N, float NoL) {
    // Normal offset: pushes the lookup along the normal, more at grazing angles.
    vec3 p = v_worldPos + N * u_shadowOffsets[i] * (1.0 - NoL * 0.5);
    vec4 s = u_shadowMatrix[i] * vec4(p, 1.0);
    vec3 c = s.xyz / s.w;
    vec4 rect = u_shadowRect[i];
    if (c.x <= rect.x || c.x >= rect.z || c.y <= rect.y || c.y >= rect.w || c.z >= 1.0) return 1.0;
    float depth = c.z - u_shadowParams.y;
    int radius = int(u_shadowParams2.y);
    // PCF taps stay inside the cascade's tile.
    vec2 lo = rect.xy + vec2(u_shadowParams2.x * 0.5);
    vec2 hi = rect.zw - vec2(u_shadowParams2.x * 0.5);
    float lit = 0.0;
    float taps = 0.0;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            float stored = texture(u_shadowMap, clamp(c.xy + vec2(float(x), float(y)) * u_shadowParams2.x, lo, hi)).r;
            lit += depth <= stored ? 1.0 : 0.0;
            taps += 1.0;
        }
    }
    return lit / taps;
}
float shadowFactor(vec3 N, vec3 L) {
    int count = int(u_shadowParams.x + 0.5);
    if (count == 0) return 1.0;
    float viewDepth = dot(v_worldPos - u_cameraPos.xyz, u_shadowView.xyz);
    int i = 0;
    while (i < count && viewDepth > u_shadowSplits[i]) ++i;
    if (i >= count) return 1.0;  // beyond the shadow distance
    float NoL = clamp(dot(N, L), 0.0, 1.0);
    float lit = cascadeLit(i, N, NoL);
    // Blend into the next cascade near the end of this one (no visible seam).
    float start = i == 0 ? 0.0 : u_shadowSplits[i - 1];
    float band = (u_shadowSplits[i] - start) * u_shadowParams.z;
    float t = band > 0.0 ? clamp((viewDepth - (u_shadowSplits[i] - band)) / band, 0.0, 1.0) : 0.0;
    if (t > 0.0) lit = mix(lit, i + 1 < count ? cascadeLit(i + 1, N, NoL) : 1.0, t);
    return mix(1.0, lit, u_shadowParams.w);
}
void main() {
#ifdef AGR_TERRAIN
    // Terrain: 4 layers (units 0, 1, 3, 4) blended by the splat map; tiling per layer in u_emissive.
    vec4 sw = texture(u_splatMap, v_uv);
    sw /= max(sw.r + sw.g + sw.b + sw.a, 1e-4);
    vec4 base = (texture(u_baseColorMap, v_uv * u_emissive.x) * sw.r + texture(u_metalRoughMap, v_uv * u_emissive.y) * sw.g +
                 texture(u_occlusionMap, v_uv * u_emissive.z) * sw.b + texture(u_emissiveMap, v_uv * u_emissive.w) * sw.a) *
                u_baseColor * v_color;
#else
    vec4 base = texture(u_baseColorMap, v_uv) * u_baseColor * v_color;
#endif
#ifdef AGR_ALPHA_MASK
    if (base.a < u_flags.x) discard;
    base.a = 1.0;
#endif
    if (u_flags.y > 0.5) {  // unlit
        o_color = vec4(agrUnlit(base.rgb), base.a);
        return;
    }
#ifdef AGR_TERRAIN
    vec4 mr = vec4(1.0);
#else
    vec4 mr = texture(u_metalRoughMap, v_uv);
#endif
    float metallic = clamp(u_pbr.x * mr.b, 0.0, 1.0);
    float roughness = clamp(u_pbr.y * mr.g, 0.045, 1.0);
    float a = roughness * roughness;

    vec3 N = normalize(v_normal);
    if (!gl_FrontFacing) N = -N;  // double-sided materials
    vec3 T = v_tangent.xyz - N * dot(N, v_tangent.xyz);
    if (dot(T, T) > 1e-8) {
        T = normalize(T);
        vec3 B = cross(N, T) * (v_tangent.w < 0.0 ? -1.0 : 1.0);
        vec3 tn = texture(u_normalMap, v_uv).xyz * 2.0 - 1.0;
        tn.xy *= u_pbr.z;
        N = normalize(T * tn.x + B * tn.y + N * max(tn.z, 1e-3));
    }
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos);
    vec3 diffuseColor = base.rgb * (1.0 - metallic);
    vec3 f0 = mix(vec3(0.04), base.rgb, metallic);

    vec3 color = vec3(0.0);
    if (dot(u_lightColor.rgb, u_lightColor.rgb) > 0.0) {
        vec3 L = -u_lightDir.xyz;
        color += shade(N, V, L, diffuseColor, f0, a) * u_lightColor.rgb * shadowFactor(N, L);
    }
    for (int i = 0; i < 8; ++i) {
        if (i >= u_lightCount.x) break;
        vec3 d = u_lightPosRange[i].xyz - v_worldPos;
        float dist2 = max(dot(d, d), 1e-4);
        vec3 L = d * inversesqrt(dist2);
        // Inverse square falloff, smoothly windowed to zero at the range.
        float r = dist2 / (u_lightPosRange[i].w * u_lightPosRange[i].w);
        float window = clamp(1.0 - r * r, 0.0, 1.0);
        float atten = window * window / (dist2 + 1.0);
        if (u_lightColorType[i].w > 0.5) {
            float cd = dot(-L, u_lightSpotDir[i].xyz);
            atten *= smoothstep(u_lightSpotDir[i].w, u_lightSpotInner[i].x, cd);
        }
        if (atten > 0.0) color += shade(N, V, L, diffuseColor, f0, a) * u_lightColorType[i].rgb * atten;
    }
    // Ambient: hemisphere light + split-sum environment BRDF approximation (Karis, mobile).
    float NoV = clamp(dot(N, V), 1e-4, 1.0);
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 rr = roughness * c0 + c1;
    float a004 = min(rr.x * rr.x, exp2(-9.28 * NoV)) * rr.x + rr.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + rr.zw;
    vec3 envSpecular = f0 * ab.x + ab.y;
    float occlusion = mix(1.0, texture(u_occlusionMap, v_uv).r, u_pbr.w);
    if (u_envParams.x > 0.5) {  // image sky: SH irradiance + prefiltered reflections
        vec3 R = reflect(-V, N);
        color += (diffuseColor * agrEnvIrradiance(N) + envSpecular * agrEnvRadiance(R, roughness)) * occlusion * u_groundColor.w;
    } else {
        color += (diffuseColor * hemisphere(N) + envSpecular * hemisphere(reflect(-V, N))) * occlusion;
    }
#ifndef AGR_TERRAIN
    color += u_emissive.rgb * texture(u_emissiveMap, v_uv).rgb;
#endif
    if (u_fogColor.w > 0.5) {
        float f = clamp((length(u_cameraPos.xyz - v_worldPos) - u_fogParams.x) / max(u_fogParams.y - u_fogParams.x, 1e-3), 0.0, 1.0);
        color = mix(color, u_fogColor.rgb, f);
    }
    o_color = vec4(agrToneMap(color), base.a);
}
)";

inline constexpr const char* kShadowVertex = R"(
layout(location = 0) in vec3 a_position;
layout(location = 3) in vec2 a_uv;
layout(location = 4) in vec4 i_model0;
layout(location = 5) in vec4 i_model1;
layout(location = 6) in vec4 i_model2;
layout(location = 7) in vec4 i_model3;
out vec2 v_uv;
void main() {
    v_uv = a_uv * u_uvTransform.xy + u_uvTransform.zw;
#ifdef AGR_SKINNED
    mat4 skin = a_weights.x * u_joints[int(a_joints.x)] + a_weights.y * u_joints[int(a_joints.y)] +
                a_weights.z * u_joints[int(a_joints.z)] + a_weights.w * u_joints[int(a_joints.w)];
    vec4 position = skin * vec4(a_position, 1.0);
#else
    vec4 position = vec4(a_position, 1.0);
#endif
    gl_Position = u_viewProj * (mat4(i_model0, i_model1, i_model2, i_model3) * position);
}
)";

inline constexpr const char* kShadowFragment = R"(
uniform sampler2D u_baseColorMap;
in vec2 v_uv;
void main() {
#ifdef AGR_SHADOW_MASK
    if (texture(u_baseColorMap, v_uv).a * u_baseColor.a < u_flags.x) discard;
#endif
}
)";

inline constexpr const char* kSkyVertex = R"(
layout(location = 0) in vec2 a_position;
out vec2 v_ndc;
void main() {
    v_ndc = a_position;
    gl_Position = vec4(a_position, 1.0, 1.0);  // on the far plane: only background pixels pass
}
)";

inline constexpr const char* kSkyFragment = R"(
in vec2 v_ndc;
layout(location = 0) out vec4 o_color;
void main() {
    vec4 p = u_invViewProj * vec4(v_ndc, 1.0, 1.0);
    vec3 dir = normalize(p.xyz / p.w - u_cameraPos.xyz);
    vec3 color = dir.y >= 0.0 ? mix(u_horizonColor.rgb, u_skyColor.rgb, pow(clamp(dir.y, 0.0, 1.0), 0.6))
                              : mix(u_horizonColor.rgb, u_groundColor.rgb, pow(clamp(-dir.y * 3.0, 0.0, 1.0), 0.5));
    if (u_envParams.x > 0.5) color = agrEnvRadiance(dir, 0.0);  // the image holds its own sun
    else if (u_horizonColor.w > 0.5 && dot(u_lightColor.rgb, u_lightColor.rgb) > 0.0) {
        float s = dot(dir, -u_lightDir.xyz);
        color += u_lightColor.rgb * (smoothstep(0.9993, 0.9997, s) * 6.0 + pow(max(s, 0.0), 48.0) * 0.25);
    }
    // Horizon haze for the procedural sky (an image sky carries its own).
    if (u_fogColor.w > 0.5 && u_envParams.x < 0.5) color = mix(color, u_fogColor.rgb, clamp(1.0 - abs(dir.y) * 4.0, 0.0, 1.0));
    o_color = vec4(agrToneMap(color), 1.0);
}
)";

} // namespace agr::shaders
