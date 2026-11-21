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
    vec4 u_flags;        // alpha cutoff, unlit, toon, -
    vec4 u_toon;         // toon: bands, softness, specular size, rim width
    vec4 u_toonShadow;   // toon: shadow tint (linear rgb), rim strength
    vec4 u_rimColor;     // toon: rim color (linear rgb); outline width in pixels
    vec4 u_outlineColor; // outline color (linear rgb), 1: extrude from the object center (built-in meshes)
    vec4 u_custom[8];    // custom shader parameters (.agrshader)
};
)";

inline constexpr const char* kLightBlock = R"(
layout(std140) uniform LightData {
    vec4 u_lightPosRange[8];   // world position, range
    vec4 u_lightColorType[8];  // linear rgb * intensity, type (0 point, 1 spot)
    vec4 u_lightSpotDir[8];    // direction the light travels, cos(outer angle)
    vec4 u_lightSpotInner[8];  // cos(inner angle), shadow slot (-1: none), -, -
    ivec4 u_lightCount;
    // Local light shadow atlas (u_spotShadowMap, 4 x 2 tiles): spots in tiles 0-1, point cube faces in 2-7.
    mat4 u_spotShadowMatrix[2];
    vec4 u_spotShadowParams;   // atlas texel (u, v), PCF on, depth bias
    vec4 u_spotShadowRect[2];
    mat4 u_pointShadowMatrix[6];  // cube faces +x, -x, +y, -y, +z, -z
    vec4 u_pointShadowRect[6];
    mat4 u_decalMatrix[16];  // world -> unit box (count: u_lightCount.y)
    vec4 u_decalColor[16];   // linear rgb, opacity
    vec4 u_decalAxis[16];    // projection direction (world), texture slot
    vec4 u_decalFade[16];    // normal fade, upper fade, lower fade, emission
    vec4 u_probeMin[8];      // reflection probes (count: u_lightCount.z): box min, blend distance
    vec4 u_probeMax[8];      // box max, intensity
    vec4 u_probePos[8];      // capture point, 1 = box projection
    vec4 u_probeRow[8];      // atlas row
    vec4 u_probeAtlasInfo;   // tile width (uv), tile height (uv), levels
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
    v_normal = mat3(i_normal0.xyz, i_normal1.xyz, i_normal2.xyz) * normal;
    v_tangent = vec4(mat3(model) * tangent, a_tangent.w);
    v_uv = a_uv * u_uvTransform.xy + u_uvTransform.zw;
    v_color = i_color;
#ifdef AGR_CUSTOM_VERTEX
    Vertex vtx;
    vtx.position = world.xyz;
    vtx.normal = normalize(v_normal);
    vtx.uv = v_uv;
    vtx.color = v_color;
    vtx.localPosition = position;
    vertex(vtx);
    world = vec4(vtx.position, 1.0);
    v_normal = vtx.normal;
    v_uv = vtx.uv;
    v_color = vtx.color;
#endif
    v_worldPos = world.xyz;
    gl_Position = u_viewProj * world;
}
)";

/// Water: grid displaced by 3 directional waves (vertex), Fresnel sky reflection + body color.
/// MaterialData: u_baseColor = shallow (a = opacity), u_emissive = deep (a = wave height),
/// u_pbr = wave length, speed, direction (radians), reflectivity.
inline constexpr const char* kWaterVertex = R"(
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
void main() {
    mat4 model = mat4(i_model0, i_model1, i_model2, i_model3);
    vec4 world = model * vec4(a_position, 1.0);
    float amp = u_emissive.w;
    float t = u_time.x * u_pbr.y;
    vec3 n = vec3(0.0, 1.0, 0.0);
    // Three waves: the main direction and two at +-40 degrees, shorter.
    for (int i = 0; i < 3; ++i) {
        float angle = u_pbr.z + (i == 0 ? 0.0 : (i == 1 ? 0.7 : -0.7));
        vec2 dir = vec2(cos(angle), sin(angle));
        float len = max(u_pbr.x, 0.1) * (i == 0 ? 1.0 : 0.47 * float(i));
        float k = 6.2831853 / len;
        float a = amp * (i == 0 ? 0.6 : 0.2);
        float phase = k * dot(dir, world.xz) - t * sqrt(9.81 * k);
        world.y += a * sin(phase);
        float d = a * k * cos(phase);
        n.x -= dir.x * d;
        n.z -= dir.y * d;
    }
    v_worldPos = world.xyz;
    v_normal = normalize(n);
    gl_Position = u_viewProj * world;
}
)";

inline constexpr const char* kWaterFragment = R"(
in vec3 v_worldPos;
in vec3 v_normal;
layout(location = 0) out vec4 o_color;
void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos);
    if (dot(N, V) < 0.0) N = -N;  // seen from below
    float NoV = clamp(dot(N, V), 0.0, 1.0);
    float fresnel = 0.02 + 0.98 * pow(1.0 - NoV, 5.0);
    vec3 R = reflect(-V, N);
    vec3 sky;
    if (u_envParams.x > 0.5) sky = agrEnvRadiance(R, 0.04);
    else if (u_skyColor.w > 0.5) sky = mix(u_horizonColor.rgb, u_skyColor.rgb, clamp(R.y, 0.0, 1.0));
    else sky = u_ambient.rgb * 2.0;
    vec3 body = mix(u_baseColor.rgb, u_emissive.rgb, clamp(1.0 - NoV, 0.0, 1.0)) * (u_ambient.rgb + max(dot(N, -u_lightDir.xyz), 0.0) * u_lightColor.rgb * 0.5 + 0.2);
    vec3 color = mix(body, sky * u_pbr.w, fresnel);
    // Sun glint (tight specular).
    vec3 H = normalize(V - u_lightDir.xyz);
    color += u_lightColor.rgb * pow(max(dot(N, H), 0.0), 400.0) * 2.0;
    float alpha = mix(u_baseColor.a, 1.0, fresnel);
    if (u_fogColor.w > 0.5) {
        float f = clamp((length(u_cameraPos.xyz - v_worldPos) - u_fogParams.x) / max(u_fogParams.y - u_fogParams.x, 1e-3), 0.0, 1.0);
        color = mix(color, u_fogColor.rgb, f);
    }
    o_color = vec4(agrToneMap(color), alpha);
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
uniform sampler2D u_spotShadowMap;  // local light shadow atlas
uniform sampler2D u_decalMap0;
uniform sampler2D u_decalMap1;
uniform sampler2D u_decalMap2;
uniform sampler2D u_probeAtlas;  // reflection probes: rows = probes, columns = roughness levels
in vec3 v_worldPos;
in vec3 v_normal;
in vec4 v_tangent;
in vec2 v_uv;
in vec4 v_color;
layout(location = 0) out vec4 o_color;

const float PI = 3.14159265;

// Depth test of point c (atlas uv + depth) inside one tile, 3 x 3 PCF by profile.
float atlasShadow(vec3 c, vec4 rect) {
    if (c.z >= 1.0) return 1.0;
    float depth = c.z - u_spotShadowParams.w;
    vec2 lo = rect.xy + u_spotShadowParams.xy * 0.5;
    vec2 hi = rect.zw - u_spotShadowParams.xy * 0.5;
    int radius = int(u_spotShadowParams.z);
    float lit = 0.0, taps = 0.0;
    for (int y = -radius; y <= radius; ++y)
        for (int x = -radius; x <= radius; ++x) {
            float stored = texture(u_spotShadowMap, clamp(c.xy + vec2(float(x), float(y)) * u_spotShadowParams.xy, lo, hi)).r;
            lit += depth <= stored ? 1.0 : 0.0;
            taps += 1.0;
        }
    return lit / taps;
}
// Lit fraction (0..1) of the point for point light i (the shadowed one, u_lightSpotInner[i].z = 1):
// the cube face is chosen by the main axis of the light -> point direction.
float pointShadow(int i, vec3 N, vec3 L) {
    if (u_lightSpotInner[i].z < 0.5) return 1.0;
    vec3 d = v_worldPos - u_lightPosRange[i].xyz;
    vec3 a = abs(d);
    int f = a.x >= a.y && a.x >= a.z ? (d.x > 0.0 ? 0 : 1) : (a.y >= a.z ? (d.y > 0.0 ? 2 : 3) : (d.z > 0.0 ? 4 : 5));
    float NoL = clamp(dot(N, L), 0.0, 1.0);
    vec3 p = v_worldPos + N * (0.004 + 0.012 * length(d)) * (1.0 - NoL * 0.5);
    vec4 s = u_pointShadowMatrix[f] * vec4(p, 1.0);
    if (s.w <= 0.0) return 1.0;
    return atlasShadow(s.xyz / s.w, u_pointShadowRect[f]);
}
float spotShadow(int i, vec3 N, vec3 L);
float localShadow(int i, vec3 N, vec3 L) {
    return u_lightColorType[i].w > 0.5 ? spotShadow(i, N, L) : pointShadow(i, N, L);
}

// Lit fraction (0..1) of the point for spot light i, from its tile of the spot shadow atlas.
float spotShadow(int i, vec3 N, vec3 L) {
    float slot = u_lightSpotInner[i].y;
    if (slot < -0.5) return 1.0;
    int k = int(slot + 0.5);
    // Normal offset grows with the distance to the light (perspective texels get larger).
    float dist = length(u_lightPosRange[i].xyz - v_worldPos);
    float NoL = clamp(dot(N, L), 0.0, 1.0);
    vec3 p = v_worldPos + N * (0.004 + 0.012 * dist) * (1.0 - NoL * 0.5);
    vec4 s = u_spotShadowMatrix[k] * vec4(p, 1.0);
    if (s.w <= 0.0) return 1.0;
    vec3 c = s.xyz / s.w;
    vec4 rect = u_spotShadowRect[k];
    if (c.x <= rect.x || c.x >= rect.z || c.y <= rect.y || c.y >= rect.w) return 1.0;  // outside the cone
    return atlasShadow(c, rect);
}

// Decals: each one projects its image along its axis onto the point if it lies in its box.
// Texture derivatives are taken before any branch (uniform control flow), then textureGrad.
void agrApplyDecals(inout vec3 albedo, inout vec3 emissive, vec3 geomN) {
    for (int i = 0; i < 16; ++i) {
        if (i >= u_lightCount.y) break;
        vec3 local = (u_decalMatrix[i] * vec4(v_worldPos, 1.0)).xyz;
        vec2 uv = vec2(local.x + 0.5, local.z + 0.5);
        vec2 dx = dFdx(uv), dy = dFdy(uv);
        vec3 a = abs(local);
        if (a.x > 0.5 || a.y > 0.5 || a.z > 0.5) continue;
        int slot = int(u_decalAxis[i].w + 0.5);
        vec4 t = slot >= 3 ? vec4(1.0) : slot == 0 ? textureGrad(u_decalMap0, uv, dx, dy)
               : slot == 1 ? textureGrad(u_decalMap1, uv, dx, dy)
                           : textureGrad(u_decalMap2, uv, dx, dy);
        vec4 fade = u_decalFade[i];
        float facing = dot(geomN, -u_decalAxis[i].xyz);
        float limit = fade.x * 0.95;  // cos of the steepest slope kept
        float nf = fade.x > 0.0 ? smoothstep(limit - 0.05, limit + 0.05, facing) : 1.0;
        float h = local.y * 2.0;  // -1 (bottom) .. 1 (top of the box)
        float ff = h > 0.0 ? (fade.y > 0.0 ? clamp((1.0 - h) / fade.y, 0.0, 1.0) : 1.0)
                           : (fade.z > 0.0 ? clamp((1.0 + h) / fade.z, 0.0, 1.0) : 1.0);
        float k = t.a * u_decalColor[i].a * nf * ff;
        vec3 c = t.rgb * u_decalColor[i].rgb;
        albedo = mix(albedo, c, k);
        emissive += c * k * fade.w;
    }
}

// Reflection probes: radiance seen along R from the probe whose box holds the point (the most
// inside one), blended with `fallback` (the sky) near its faces. Box projection: the ray is
// traced to the box walls, the capture is read in the direction capture point -> hit point.
vec3 agrProbeRadiance(vec3 R, float roughness, vec3 fallback) {
    int best = -1;
    float bestW = -1.0;
    for (int i = 0; i < 8; ++i) {
        if (i >= u_lightCount.z) break;
        vec3 inside = min(v_worldPos - u_probeMin[i].xyz, u_probeMax[i].xyz - v_worldPos);
        float d = min(inside.x, min(inside.y, inside.z));
        if (d < 0.0) continue;
        float w = u_probeMin[i].w > 0.0 ? clamp(d / u_probeMin[i].w, 0.0, 1.0) : 1.0;
        if (w > bestW) {
            bestW = w;
            best = i;
        }
    }
    if (best < 0 || bestW <= 0.0) return fallback;
    vec3 dir = R;
    if (u_probePos[best].w > 0.5) {
        vec3 r = vec3(abs(R.x) < 1e-5 ? 1e-5 : R.x, abs(R.y) < 1e-5 ? 1e-5 : R.y, abs(R.z) < 1e-5 ? 1e-5 : R.z);
        vec3 t1 = (u_probeMax[best].xyz - v_worldPos) / r;
        vec3 t2 = (u_probeMin[best].xyz - v_worldPos) / r;
        vec3 tf = max(t1, t2);
        float t = min(tf.x, min(tf.y, tf.z));
        dir = v_worldPos + R * t - u_probePos[best].xyz;
    }
    dir = normalize(dir);
    vec2 e = vec2(0.5 + atan(dir.x, -dir.z) * 0.15915494, acos(clamp(dir.y, -1.0, 1.0)) * 0.31830989);
    e = clamp(e, vec2(0.004, 0.008), vec2(0.996, 0.992));  // half a texel inside the tile
    float levels = u_probeAtlasInfo.z;
    float lv = clamp(roughness, 0.0, 1.0) * (levels - 1.0);
    float l0 = floor(lv);
    float l1 = min(l0 + 1.0, levels - 1.0);
    float row = u_probeRow[best].x;
    vec3 c0 = textureLod(u_probeAtlas, vec2((l0 + e.x) * u_probeAtlasInfo.x, (row + e.y) * u_probeAtlasInfo.y), 0.0).rgb;
    vec3 c1 = textureLod(u_probeAtlas, vec2((l1 + e.x) * u_probeAtlasInfo.x, (row + e.y) * u_probeAtlasInfo.y), 0.0).rgb;
    return mix(fallback, mix(c0, c1, lv - l0) * u_probeMax[best].w, bestW);
}

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
// ---- Stylized (toon) shading: banded light, tinted shadows, sharp highlight, rim light.
// Light steps of x in 0..1: `bands` levels, soft edges of width u_toon.y around each step.
float agrToonRamp(float x) {
    float bands = max(u_toon.x, 1.0);
    float w = max(u_toon.y, 0.001);
    float t = clamp(x, 0.0, 1.0) * bands;
    float b = min(floor(t), bands - 1.0);
    return clamp((b + smoothstep(0.5 - w, 0.5 + w, t - b)) / bands, 0.0, 1.0);
}
vec3 agrToonShade(vec3 albedo, vec3 N, vec3 V, float occlusion) {
    vec3 tint = u_toonShadow.rgb;
    vec3 color = vec3(0.0);
    float sunLit = 0.0;
    if (dot(u_lightColor.rgb, u_lightColor.rgb) > 0.0) {
        vec3 L = -u_lightDir.xyz;
        float NoL = dot(N, L);
        // Half Lambert: the terminator sits at NoL = 0. Cast shadows join the shadow band, but
        // fade out where the light grazes the surface: there the shadow map is unreliable and its
        // filtered values would cross the hard step as speckles (the ramp alone decides there).
        float shadow = mix(1.0, smoothstep(0.25, 0.75, shadowFactor(N, L)), smoothstep(0.0, 0.25, NoL));
        sunLit = agrToonRamp(NoL * 0.5 + 0.5) * shadow;
        sunLit = min(sunLit, agrToonRamp((NoL * 0.5 + 0.5) * shadow));
        color += albedo * u_lightColor.rgb * mix(tint, vec3(1.0), sunLit);
        if (u_toon.z > 0.0) {
            float NoH = clamp(dot(N, normalize(L + V)), 0.0, 1.0);
            float edge = 1.0 - u_toon.z * u_toon.z * 0.08;  // size 1: wide highlight, 0.25: a small spot
            float w = max(u_toon.y, 0.002) * 0.05;
            color += u_lightColor.rgb * smoothstep(edge - w, edge + w, NoH) * sunLit;
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (i >= u_lightCount.x) break;
        vec3 d = u_lightPosRange[i].xyz - v_worldPos;
        float dist2 = max(dot(d, d), 1e-4);
        vec3 L = d * inversesqrt(dist2);
        float r = dist2 / (u_lightPosRange[i].w * u_lightPosRange[i].w);
        float window = clamp(1.0 - r * r, 0.0, 1.0);
        float atten = window * window / (dist2 + 1.0);
        if (u_lightColorType[i].w > 0.5) atten *= smoothstep(u_lightSpotDir[i].w, u_lightSpotInner[i].x, dot(-L, u_lightSpotDir[i].xyz));
        if (atten > 0.0) color += albedo * u_lightColorType[i].rgb * atten * agrToonRamp(max(dot(N, L), 0.0) * localShadow(i, N, L));
    }
    // Flat ambient (hemisphere or image sky irradiance): no reflections, readable colors. It
    // mostly fills the shadow side: on the lit side the sun alone shows the true base color.
    vec3 ambient = u_envParams.x > 0.5 ? agrEnvIrradiance(N) * u_groundColor.w : hemisphere(N);
    color += albedo * ambient * occlusion * (1.0 - 0.7 * sunLit);
    if (u_toonShadow.w > 0.0) {
        float NoV = clamp(dot(N, V), 0.0, 1.0);
        float w = max(u_toon.w, 0.001);
        float rim = smoothstep(1.0 - w, 1.0 - w + max(u_toon.y, 0.01) + 0.02, 1.0 - NoV);
        color += u_rimColor.rgb * rim * u_toonShadow.w * (0.35 + 0.65 * sunLit);
    }
    // Sun + ambient can exceed 1: scale the color down as a whole instead of clipping each
    // channel, so lit surfaces keep their hue and saturation instead of washing out to white.
    return color / max(1.0, max(color.r, max(color.g, color.b)));
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
#ifndef AGR_CUSTOM_SURFACE
#ifdef AGR_ALPHA_MASK
    if (base.a < u_flags.x) discard;
    base.a = 1.0;
#endif
    if (u_flags.y > 0.5) {  // unlit
        o_color = vec4(agrUnlit(base.rgb), base.a);
        return;
    }
#endif
#ifdef AGR_TERRAIN
    vec4 mr = vec4(1.0);
#else
    vec4 mr = texture(u_metalRoughMap, v_uv);
#endif
    float metallic = clamp(u_pbr.x * mr.b, 0.0, 1.0);
    float roughness = clamp(u_pbr.y * mr.g, 0.045, 1.0);

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
    float occlusion = mix(1.0, texture(u_occlusionMap, v_uv).r, u_pbr.w);
#ifdef AGR_TERRAIN
    vec3 emissive = vec3(0.0);
#else
    vec3 emissive = u_emissive.rgb * texture(u_emissiveMap, v_uv).rgb;
#endif
#ifdef AGR_CUSTOM_SURFACE
    Surface s;
    s.baseColor = base.rgb;
    s.alpha = base.a;
    s.metallic = metallic;
    s.roughness = roughness;
    s.normal = N;
    s.emissive = emissive;
    s.occlusion = occlusion;
    s.worldPos = v_worldPos;
    s.viewDir = V;
    s.uv = v_uv;
    s.color = v_color;
    surface(s);
    base = vec4(s.baseColor, s.alpha);
    metallic = clamp(s.metallic, 0.0, 1.0);
    roughness = clamp(s.roughness, 0.045, 1.0);
    N = dot(s.normal, s.normal) > 1e-12 ? normalize(s.normal) : N;
    emissive = s.emissive;
    occlusion = clamp(s.occlusion, 0.0, 1.0);
#ifdef AGR_ALPHA_MASK
    if (base.a < u_flags.x) discard;
    base.a = 1.0;
#endif
    if (u_flags.y > 0.5) {  // unlit
        o_color = vec4(agrUnlit(base.rgb + emissive), base.a);
        return;
    }
#endif
    if (u_lightCount.y > 0) {
        vec3 albedo = base.rgb;
        agrApplyDecals(albedo, emissive, normalize(gl_FrontFacing ? v_normal : -v_normal));
        base.rgb = albedo;
    }
    if (u_flags.z > 0.5) {  // toon: vivid colors, no filmic curve (exposure kept)
        vec3 toon = agrToonShade(base.rgb, N, V, occlusion) + emissive;
        if (u_fogColor.w > 0.5) {
            float f = clamp((length(u_cameraPos.xyz - v_worldPos) - u_fogParams.x) / max(u_fogParams.y - u_fogParams.x, 1e-3), 0.0, 1.0);
            toon = mix(toon, u_fogColor.rgb, f);
        }
        o_color = vec4(agrUnlit(toon * u_fogParams.z), base.a);
        return;
    }
    float a = roughness * roughness;
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
        if (atten > 0.0) color += shade(N, V, L, diffuseColor, f0, a) * u_lightColorType[i].rgb * atten * localShadow(i, N, L);
    }
    // Ambient: hemisphere light + split-sum environment BRDF approximation (Karis, mobile).
    float NoV = clamp(dot(N, V), 1e-4, 1.0);
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 rr = roughness * c0 + c1;
    float a004 = min(rr.x * rr.x, exp2(-9.28 * NoV)) * rr.x + rr.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + rr.zw;
    vec3 envSpecular = f0 * ab.x + ab.y;
    vec3 R = reflect(-V, N);
    if (u_envParams.x > 0.5) {  // image sky: SH irradiance + prefiltered reflections
        vec3 reflected = agrProbeRadiance(R, roughness, agrEnvRadiance(R, roughness) * u_groundColor.w);
        color += (diffuseColor * agrEnvIrradiance(N) * u_groundColor.w + envSpecular * reflected) * occlusion;
    } else {
        color += (diffuseColor * hemisphere(N) + envSpecular * agrProbeRadiance(R, roughness, hemisphere(R))) * occlusion;
    }
    color += emissive;
    if (u_fogColor.w > 0.5) {
        float f = clamp((length(u_cameraPos.xyz - v_worldPos) - u_fogParams.x) / max(u_fogParams.y - u_fogParams.x, 1e-3), 0.0, 1.0);
        color = mix(color, u_fogColor.rgb, f);
    }
#ifdef AGR_OPAQUE
    // Opaque surfaces: alpha = how much they mirror the screen (screen-space reflections):
    // specular reflectance x smoothness^2, dimmed by occlusion. Nothing else reads it.
    float smoothness = 1.0 - roughness;
    o_color = vec4(agrToneMap(color), clamp(dot(envSpecular, vec3(0.3333)) * smoothness * smoothness * occlusion, 0.0, 1.0));
#else
    o_color = vec4(agrToneMap(color), base.a);
#endif
}
)";

/// Blob shadow: radial falloff on the unit plane. u_baseColor = color (linear), opacity;
/// u_pbr.x = softness (fraction of the radius that fades out).
inline constexpr const char* kBlobFragment = R"(
in vec3 v_worldPos;
in vec3 v_normal;
in vec4 v_tangent;
in vec2 v_uv;
in vec4 v_color;
layout(location = 0) out vec4 o_color;
void main() {
    float r = length(v_uv * 2.0 - 1.0);
    float soft = clamp(u_pbr.x, 0.001, 1.0);
    float a = u_baseColor.a * (1.0 - smoothstep(1.0 - soft, 1.0, r));
    if (a <= 0.003) discard;
    o_color = vec4(agrUnlit(u_baseColor.rgb), a);
}
)";

/// Outline (inverted hull): back faces pushed out along the normal by a constant number
/// of pixels in clip space, drawn in the outline color; front faces of the object hide
/// all but the silhouette. Imported models need smooth normals; built-in meshes are pushed
/// out from their center instead (closed hull on a cube).
inline constexpr const char* kOutlineVertex = R"(
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 4) in vec4 i_model0;
layout(location = 5) in vec4 i_model1;
layout(location = 6) in vec4 i_model2;
layout(location = 7) in vec4 i_model3;
layout(location = 8) in vec4 i_normal0;
layout(location = 9) in vec4 i_normal1;
layout(location = 10) in vec4 i_normal2;
void main() {
    mat4 model = mat4(i_model0, i_model1, i_model2, i_model3);
#ifdef AGR_SKINNED
    mat4 skin = a_weights.x * u_joints[int(a_joints.x)] + a_weights.y * u_joints[int(a_joints.y)] +
                a_weights.z * u_joints[int(a_joints.z)] + a_weights.w * u_joints[int(a_joints.w)];
    vec3 position = (skin * vec4(a_position, 1.0)).xyz;
    vec3 normal = mat3(skin) * a_normal;
#else
    vec3 position = a_position;
    vec3 normal = a_normal;
#endif
    vec4 clip = u_viewProj * (model * vec4(position, 1.0));
    // Built-in meshes (cube...) have split normals at hard edges: pushing out from the center
    // keeps the hull closed there. Imported models use their (smooth) normals.
    vec3 dir3 = u_outlineColor.w > 0.5 && dot(position, position) > 1e-8 ? mat3(model) * position
                                                                         : mat3(i_normal0.xyz, i_normal1.xyz, i_normal2.xyz) * normal;
    vec3 n = dir3;
    vec2 dir = (u_viewProj * vec4(n, 0.0)).xy;
    float len = length(dir);
    if (len > 1e-6) clip.xy += dir / len * u_rimColor.w * 2.0 * u_time.zw * clip.w;
    gl_Position = clip;
}
)";

inline constexpr const char* kOutlineFragment = R"(
layout(location = 0) out vec4 o_color;
void main() {
    o_color = vec4(agrUnlit(u_outlineColor.rgb), 1.0);
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
    mat4 model = mat4(i_model0, i_model1, i_model2, i_model3);
    vec4 world = model * position;
#ifdef AGR_CUSTOM_VERTEX
#ifdef AGR_SKINNED
    vec3 localNormal = mat3(skin) * a_normal;
#else
    vec3 localNormal = a_normal;
#endif
    Vertex vtx;
    vtx.position = world.xyz;
    vtx.normal = normalize(mat3(model) * localNormal);
    vtx.uv = v_uv;
    vtx.color = vec4(1.0);
    vtx.localPosition = position.xyz;
    vertex(vtx);
    world = vec4(vtx.position, 1.0);
    v_uv = vtx.uv;
#endif
    gl_Position = u_viewProj * world;
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

/// Custom shaders (.agrshader): the hook API, then the user's code after `#line 1 1`.
inline constexpr const char* kCustomBlock = R"(
struct Surface {
    vec3 baseColor;
    float alpha;
    float metallic;
    float roughness;
    vec3 normal;
    vec3 emissive;
    float occlusion;
    vec3 worldPos;
    vec3 viewDir;
    vec2 uv;
    vec4 color;
};
struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 color;
    vec3 localPosition;
};
#define TIME (u_time.x)
void surface(inout Surface s);
void vertex(inout Vertex v);
)";

/// Shadow pass of a custom vertex hook: the normal attribute (separate block: the Vulkan
/// translator reads declarations without preprocessing).
inline constexpr const char* kShadowNormalBlock = R"(
layout(location = 1) in vec3 a_normal;
)";

/// Reflection probe: 6 captured faces -> one equirectangular tile of the probe atlas, GGX
/// prefiltered for the tile's roughness. Full-screen pass on the atlas; other tiles discarded.
inline constexpr const char* kProbeConvert = R"(
layout(std140) uniform ProbeData {
    vec4 u_tile;      // atlas uv rect: x0, y0, x1, y1
    vec4 u_params;    // roughness, 1 = linear faces (else tone mapped sRGB), 1 = face rows top-down (Vulkan), exposure
    vec4 u_atlas;     // 1 / atlas width, 1 / atlas height
    vec4 u_faceF[6];  // forward of each face
    vec4 u_faceU[6];  // up of each face
};
uniform sampler2D u_face0;
uniform sampler2D u_face1;
uniform sampler2D u_face2;
uniform sampler2D u_face3;
uniform sampler2D u_face4;
uniform sampler2D u_face5;
layout(location = 0) out vec4 o_color;
vec3 decode(vec3 c) {
    if (u_params.y > 0.5) return c;
    // Inverse of the display encoding: sRGB gamma, then the ACES fit (Narkowicz), then exposure.
    c = min(pow(clamp(c, 0.0, 1.0), vec3(2.2)), vec3(0.98));
    vec3 a = 2.43 * c - 2.51;
    vec3 b = 0.59 * c - 0.03;
    vec3 k = 0.14 * c;
    vec3 x = (-b - sqrt(max(b * b - 4.0 * a * k, vec3(0.0)))) / (2.0 * a);
    return max(x, vec3(0.0)) / max(u_params.w, 1e-3);
}
vec3 sampleCube(vec3 d) {
    int best = 0;
    float m = -2.0;
    for (int i = 0; i < 6; ++i) {
        float k = dot(d, u_faceF[i].xyz);
        if (k > m) {
            m = k;
            best = i;
        }
    }
    vec3 F = u_faceF[best].xyz;
    vec3 U = u_faceU[best].xyz;
    vec3 Rt = normalize(cross(F, U));
    vec2 uv = vec2(dot(d, Rt), dot(d, U)) / max(m, 1e-4) * 0.5 + 0.5;
    if (u_params.z > 0.5) uv.y = 1.0 - uv.y;
    uv = clamp(uv, vec2(0.002), vec2(0.998));
    vec3 c = best == 0 ? textureLod(u_face0, uv, 0.0).rgb
           : best == 1 ? textureLod(u_face1, uv, 0.0).rgb
           : best == 2 ? textureLod(u_face2, uv, 0.0).rgb
           : best == 3 ? textureLod(u_face3, uv, 0.0).rgb
           : best == 4 ? textureLod(u_face4, uv, 0.0).rgb
                       : textureLod(u_face5, uv, 0.0).rgb;
    return decode(c);
}
void main() {
    vec2 uv = gl_FragCoord.xy * u_atlas.xy;
    if (uv.x < u_tile.x || uv.y < u_tile.y || uv.x > u_tile.z || uv.y > u_tile.w) discard;
    vec2 e = (uv - u_tile.xy) / (u_tile.zw - u_tile.xy);
    float phi = (e.x - 0.5) * 6.2831853;
    float theta = e.y * 3.14159265;
    vec3 N = vec3(sin(theta) * sin(phi), cos(theta), -sin(theta) * cos(phi));
    float r = u_params.x;
    if (r <= 0.0) {
        o_color = vec4(sampleCube(N), 1.0);
        return;
    }
    // GGX prefiltering (split sum, N = V = R), golden-ratio sequence.
    float a = r * r;
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);
    vec3 sum = vec3(0.0);
    float total = 0.0;
    const int kSamples = 256;
    for (int i = 0; i < kSamples; ++i) {
        vec2 xi = vec2((float(i) + 0.5) / float(kSamples), fract(float(i) * 0.6180340));
        float ph = 6.2831853 * xi.y;
        float cosT = sqrt((1.0 - xi.x) / (1.0 + (a * a - 1.0) * xi.x));
        float sinT = sqrt(max(1.0 - cosT * cosT, 0.0));
        vec3 H = T * (sinT * cos(ph)) + B * (sinT * sin(ph)) + N * cosT;
        vec3 L = 2.0 * dot(N, H) * H - N;
        float NoL = dot(N, L);
        if (NoL > 0.0) {
            // Karis weighting: a tiny very bright spot (a lamp) caught by few samples would
            // leave sparkles; dividing by (1 + luma) evens the samples out.
            vec3 c = sampleCube(L);
            float w = NoL / (1.0 + dot(c, vec3(0.2126, 0.7152, 0.0722)));
            sum += c * w;
            total += w;
        }
    }
    o_color = vec4(sum / max(total, 1e-6), 1.0);
}
)";

} // namespace agr::shaders
