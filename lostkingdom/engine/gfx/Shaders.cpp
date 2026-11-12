// AGR Engine — GLSL ES 3.00 shader library.
// One "lit" uber-shader is compiled into variants (static / skinned / instanced / wind /
// grass / terrain / depth-only / outline) at start-up; the other programs are water & lava,
// sky, particles, ribbons (weapon trails), post-processing (bloom chain, DoF, grading) and UI.
#include "Shaders.h"

namespace agr {
namespace shaders {

const char* FRAME_UBO = R"(
layout(std140) uniform Frame {
    mat4 uViewProj;
    mat4 uView;
    mat4 uShadowMat;
    vec4 uCamPos;      // xyz camera, w time (s)
    vec4 uSunDir;      // xyz towards sun, w intensity
    vec4 uSunColor;    // rgb, w shadow strength
    vec4 uSkyColor;    // rgb ambient from above, w exposure
    vec4 uGroundColor; // rgb ambient from below, w wetness (rain)
    vec4 uFogColor;    // rgb, w density
    vec4 uFogParams;   // x height falloff, y fog base height, z snow cover, w night factor
    vec4 uWind;        // xy direction, z strength, w gust
    vec4 uPlayer;      // xyz player position, w grass push radius
    vec4 uMisc;        // x light count, y shadow texel, z cloud shadows, w lightning flash
    vec4 uLightPos[8]; // xyz, w radius
    vec4 uLightCol[8]; // rgb * intensity, w flicker phase
};
const float PI = 3.14159265;
)";

// ----------------------------------------------------------------------------------------------
const char* LIT_VS = R"(
in vec3 aPos;
in vec4 aNormal;
in vec4 aColor;
in vec4 aMat;
in vec4 aSkin;
#ifdef INSTANCED
in vec4 iPosRot;
in float iScale;
in vec4 iTint;
#endif
uniform mat4 uModel;
#ifdef SKINNED
uniform mat4 uBones[32];
#endif
#ifdef OUTLINE
uniform vec4 uOutline; // x width (m at 10 m distance)
#endif
out vec3 vWorld;
out vec3 vNormal;
out vec4 vColor;
out vec4 vMat;
out vec4 vSkin;
out vec4 vShadow;

vec3 windOffset(vec3 wp, float weight) {
    float t = uCamPos.w;
    float phase = dot(wp.xz, vec2(0.13, 0.17));
    float sway = sin(t * 1.3 + phase) * 0.6 + sin(t * 2.7 + phase * 1.7) * 0.3;
    float gust = 0.55 + 0.45 * sin(t * 0.37 + wp.x * 0.021 + wp.z * 0.013) * uWind.w;
    vec2 off = uWind.xy * (sway * 0.35 + gust) * uWind.z * weight;
    off += vec2(sin(t * 7.3 + wp.x * 3.1 + wp.z * 1.7), cos(t * 6.1 + wp.z * 2.9)) * 0.04 * weight * uWind.z;
    return vec3(off.x, -dot(off, off) * 0.35, off.y);
}

void main() {
    vec3 p = aPos;
    vec3 n = aNormal.xyz;
#ifdef SKINNED
    int b0 = int(aSkin.x + 0.5), b1 = int(aSkin.y + 0.5);
    float w = aSkin.z / 255.0;
    mat4 sk = uBones[b0] * w + uBones[b1] * (1.0 - w);
    p = (sk * vec4(p, 1.0)).xyz;
    n = mat3(sk) * n;
#endif
#ifdef INSTANCED
    float c = cos(iPosRot.w), s = sin(iPosRot.w);
    vec3 lp = vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z) * iScale;
    n = vec3(c * n.x + s * n.z, n.y, -s * n.x + c * n.z);
    vec3 wp = lp + iPosRot.xyz;
#else
    vec3 wp = (uModel * vec4(p, 1.0)).xyz;
    n = mat3(uModel) * n;
#endif
#ifdef WIND
    float wweight = aMat.a;
  #ifdef INSTANCED
    wweight *= iScale;
  #endif
    wp += windOffset(wp, wweight);
  #ifdef GRASS
    vec2 d = wp.xz - uPlayer.xz;
    float dist = length(d);
    float push = (1.0 - smoothstep(0.0, uPlayer.w, dist)) * aMat.a * step(abs(wp.y - uPlayer.y), 2.5);
    wp.xz += d / max(dist, 0.001) * push * 0.55;
    wp.y -= push * 0.25;
  #endif
#endif
    n = normalize(n);
#ifdef OUTLINE
    float vd = length(wp - uCamPos.xyz);
    wp += n * uOutline.x * (vd / 10.0);
#endif
    vWorld = wp;
    vNormal = n;
    vColor = aColor;
#ifdef INSTANCED
    vColor.rgb *= iTint.rgb * 2.0;
#endif
    vMat = aMat;
    vSkin = aSkin / 255.0;
    vShadow = uShadowMat * vec4(wp, 1.0);
    gl_Position = uViewProj * vec4(wp, 1.0);
}
)";

// Shared lighting / fog code for lit fragment shaders.
const char* LIGHTING = R"(
uniform mediump sampler2DShadow uShadowMap;
uniform sampler2D uNoiseTex;

float shadowPCF(vec4 sc, float NdL) {
    vec3 p = sc.xyz / sc.w;
    if (p.x <= 0.001 || p.x >= 0.999 || p.y <= 0.001 || p.y >= 0.999 || p.z >= 1.0) return 1.0;
    float bias = mix(0.0025, 0.0006, clamp(NdL, 0.0, 1.0));
    float t = uMisc.y;
    float s = 0.0;
#ifdef SHADOW_SOFT
    // 12-tap rotated Poisson disc + hardware 2x2 PCF per tap = soft penumbra
    const vec2 k[12] = vec2[12](vec2(-0.326,-0.406), vec2(-0.840,-0.074), vec2(-0.696,0.457), vec2(-0.203,0.621),
                               vec2(0.962,-0.195), vec2(0.473,-0.480), vec2(0.519,0.767), vec2(0.185,-0.893),
                               vec2(0.507,0.064), vec2(0.896,0.412), vec2(-0.322,-0.933), vec2(-0.792,-0.598));
    float a = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831;
    mat2 r = mat2(cos(a), sin(a), -sin(a), cos(a));
    for (int i = 0; i < 12; i++) s += texture(uShadowMap, vec3(p.xy + r * k[i] * t * 2.2, p.z - bias));
    s /= 12.0;
#else
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) s += texture(uShadowMap, vec3(p.xy + vec2(x, y) * t * 1.25, p.z - bias));
    s /= 9.0;
#endif
    // fade out at the shadow map edges
    vec2 e = smoothstep(0.0, 0.06, p.xy) * smoothstep(1.0, 0.94, p.xy);
    return mix(1.0, s, e.x * e.y);
}

float cloudShadow(vec3 P) {
    vec2 uv = P.xz * 0.0035 + uWind.xy * uCamPos.w * 0.004;
    float c = texture(uNoiseTex, uv).b;
    return mix(1.0, smoothstep(0.25, 0.75, c) * 0.55 + 0.45, uMisc.z);
}

vec3 envColor(vec3 R, float rough) {
    float h = clamp(R.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 env = mix(uGroundColor.rgb * 0.8, uSkyColor.rgb * 1.35, smoothstep(0.35, 0.75, h));
    float sunSpot = pow(max(dot(R, uSunDir.xyz), 0.0), mix(400.0, 6.0, rough));
    return env + uSunColor.rgb * sunSpot * uSunDir.w * (1.0 - rough) * 0.7;
}

vec3 shade(vec3 albedo, vec3 N, vec3 V, vec3 P, float rough, float metal, float ao, float shadow, float wrap, float rim) {
    vec3 L = uSunDir.xyz;
    float NdL = dot(N, L);
    float diff = clamp((NdL + wrap) / (1.0 + wrap), 0.0, 1.0);
    vec3 H = normalize(L + V);
    float NdH = max(dot(N, H), 0.0), NdV = clamp(dot(N, V), 1e-3, 1.0), nl = max(NdL, 0.0);
    float a = max(rough * rough, 0.02), a2 = a * a;
    float dd = NdH * NdH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * dd * dd);
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float G = (NdV / (NdV * (1.0 - k) + k)) * (nl / (nl * (1.0 - k) + k));
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - clamp(dot(H, V), 0.0, 1.0), 5.0);
    vec3 spec = min(D * G * F / (4.0 * NdV * max(nl, 0.001) + 1e-4), vec3(24.0)) * nl;
    vec3 kd = (1.0 - F) * (1.0 - metal);
    vec3 sun = uSunColor.rgb * uSunDir.w * shadow;
    vec3 col = (kd * albedo * diff + spec) * sun;
    // Hemispheric ambient (sky / bounced ground) + approximated image-based specular.
    float up = N.y * 0.5 + 0.5;
    vec3 amb = mix(uGroundColor.rgb, uSkyColor.rgb, up);
    vec3 Fa = F0 + (max(vec3(1.0 - rough), F0) - F0) * pow(1.0 - NdV, 5.0);
    col += (albedo * kd * amb + envColor(reflect(-V, N), rough) * Fa * mix(1.0, 0.35, rough)) * ao;
    // Point lights (torches, fires, spells, lava)
    for (int i = 0; i < 8; i++) {
        if (float(i) >= uMisc.x) break;
        vec3 lv = uLightPos[i].xyz - P;
        float dist = length(lv);
        float att = clamp(1.0 - dist / uLightPos[i].w, 0.0, 1.0);
        if (att <= 0.0) continue;
        att *= att;
        vec3 l = lv / dist;
        float pl = clamp((dot(N, l) + 0.35) / 1.35, 0.0, 1.0);
        vec3 h = normalize(l + V);
        float sp = pow(max(dot(N, h), 0.0), mix(80.0, 6.0, rough)) * (1.0 - rough * 0.8);
        float flick = 1.0 + uLightCol[i].w * (sin(uCamPos.w * 13.0 + float(i) * 1.7) * 0.06 + sin(uCamPos.w * 23.0 + float(i)) * 0.04);
        col += uLightCol[i].rgb * att * flick * (albedo * pl * kd + sp * mix(F0, vec3(1.0), 0.3) * 2.0);
    }
    // Rim light improves character readability against the ground (2.5D).
    col += rim * pow(1.0 - NdV, 3.0) * (uSkyColor.rgb * 0.6 + uSunColor.rgb * 0.4) * (0.5 + 0.5 * shadow);
    return col;
}

vec3 applyFog(vec3 col, vec3 P) {
    vec3 d = P - uCamPos.xyz;
    float dist = length(d);
    float hf = exp(-max(P.y - uFogParams.y, 0.0) * uFogParams.x);
    float f = 1.0 - exp(-max(dist - 12.0, 0.0) * uFogColor.w * hf);
    vec3 vd = d / max(dist, 0.001);
    float sunAmt = pow(max(dot(vd, uSunDir.xyz), 0.0), 6.0);
    vec3 fc = uFogColor.rgb + uSunColor.rgb * sunAmt * 0.3 * uSunDir.w;
    return mix(col, fc, clamp(f, 0.0, 0.97));
}
)";

// ----------------------------------------------------------------------------------------------
const char* LIT_FS = R"(
in vec3 vWorld;
in vec3 vNormal;
in vec4 vColor;
in vec4 vMat;
in vec4 vSkin;
in vec4 vShadow;
uniform vec4 uTint;       // rgb multiply, a = snow on top faces
uniform vec4 uFx;         // x hit flash, y dissolve, z rim, w emissive boost
uniform vec4 uDissolveColor;
#ifdef TERRAIN
uniform mediump sampler2DArray uTerrainTex;  // albedo.rgb + height.a
uniform mediump sampler2DArray uTerrainNrm;  // tangent-space normal.rgb
#endif
#ifdef OUTLINE
uniform vec4 uOutlineColor;
#endif
out vec4 fragColor;

#ifdef TERRAIN
// layers: 0 grass, 1 dirt, 2 rock, 3 snow, 4 sand, 5 mud, 6 cobble, 7 ash
vec4 sampleLayer(int l, vec2 uv) { return texture(uTerrainTex, vec3(uv, float(l))); }
vec3 sampleNrm(int l, vec2 uv) { return texture(uTerrainNrm, vec3(uv, float(l))).xyz * 2.0 - 1.0; }
#endif

void main() {
#ifdef OUTLINE
    fragColor = vec4(uOutlineColor.rgb * uSkyColor.w, 1.0);
    return;
#else
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos.xyz - vWorld);
    // vertex colours are authored in sRGB: convert to linear for lighting
    vec3 albedo = pow(max(vColor.rgb * uTint.rgb, vec3(0.0)), vec3(2.2));
    float ao = vColor.a;
    float rough = vMat.r, metal = vMat.g;
    float emis = vMat.b * 4.0;
    float wet = uGroundColor.w;

#ifdef TERRAIN
    // Splat weights from vertex: mat.r rock, mat.g dirt, mat.b snow, mat.a sand, skin.x mud, skin.y cobble, skin.z ash
    float wRock = vMat.r, wDirt = vMat.g, wSnow = vMat.b, wSand = vMat.a, wMud = vSkin.x, wCob = vSkin.y, wAsh = vSkin.z;
    float wGrass = max(0.0, 1.0 - (wRock + wDirt + wSnow + wSand + wMud + wCob + wAsh));
    // Slope-driven rock override computed per pixel for crisp cliff edges.
    float slope = 1.0 - N.y;
    float cliff = smoothstep(0.28, 0.45, slope);
    vec2 uv = vWorld.xz * 0.28;
    vec4 g = sampleLayer(0, uv), dI = sampleLayer(1, uv * 0.9), sn = sampleLayer(3, uv * 0.7), sa = sampleLayer(4, uv * 0.8);
    // triplanar-ish rock: project on the dominant horizontal axis for steep faces
    vec2 ruv = abs(N.x) > abs(N.z) ? vWorld.zy * 0.22 : vWorld.xy * 0.22;
    vec4 ro = sampleLayer(2, mix(vWorld.xz * 0.22, ruv, cliff));
    vec4 mu = sampleLayer(5, uv), co = sampleLayer(6, uv * 0.6), as = sampleLayer(7, uv * 0.7);
    float ws[8] = float[8](wGrass * (1.0 - cliff), wDirt * (1.0 - cliff), wRock + cliff, wSnow * (1.0 - cliff * 0.7), wSand * (1.0 - cliff), wMud * (1.0 - cliff), wCob, wAsh * (1.0 - cliff));
    vec4 smp[8] = vec4[8](g, dI, ro, sn, sa, mu, co, as);
    // height-based blending: the layer whose height sticks out wins the transition
    float best = 0.0;
    for (int i = 0; i < 8; i++) best = max(best, ws[i] + smp[i].a * 0.5);
    float sum = 0.0;
    vec3 alb = vec3(0.0);
    float hsum = 0.0;
    float wv[8];
    for (int i = 0; i < 8; i++) {
        float w = max(ws[i] + smp[i].a * 0.5 - best + 0.2, 0.0) * step(0.001, ws[i]);
        wv[i] = w;
        sum += w;
    }
    vec3 tn = vec3(0.0);
    float rgh = 0.0;
    const float layerRough[8] = float[8](0.85, 0.9, 0.75, 0.45, 0.9, 0.55, 0.7, 0.95);
    for (int i = 0; i < 8; i++) {
        float w = wv[i] / max(sum, 1e-4);
        if (w < 0.004) continue;
        alb += smp[i].rgb * w;
        rgh += layerRough[i] * w;
        hsum += smp[i].a * w;
  #ifndef TERRAIN_SIMPLE
        tn += sampleNrm(i, i == 2 ? mix(vWorld.xz * 0.22, ruv, cliff) : uv) * w;
  #endif
    }
    emis = 0.0;
    albedo = pow(alb, vec3(2.2)) * vColor.rgb * 2.0;  // texture sRGB -> linear; vertex colour is a regional tint around 0.5
    // macro variation breaks texture tiling at gameplay distance
    float macro = texture(uNoiseTex, vWorld.xz * 0.0045).g * 0.7 + texture(uNoiseTex, vWorld.xz * 0.019).r * 0.3;
    albedo *= 0.78 + macro * 0.44;
    albedo = mix(albedo, albedo * vec3(1.08, 1.0, 0.85), smoothstep(0.55, 0.75, macro) * wGrass);  // sun-dried patches
    // volcanic veins glowing through the ash
    float vn = texture(uNoiseTex, vWorld.xz * 0.033).g;
    float vein = smoothstep(0.012, 0.0, abs(vn - 0.5)) * smoothstep(0.3, 0.6, wAsh) * smoothstep(0.55, 0.7, texture(uNoiseTex, vWorld.xz * 0.006).b);
    emis += vein * (0.6 + 0.4 * sin(uCamPos.w * 1.7 + vWorld.x * 0.1)) * 1.6;
    rough = rgh;
    metal = 0.0;
    ao = mix(0.55, 1.0, hsum) * vColor.a;
  #ifndef TERRAIN_SIMPLE
    // world-aligned tangent frame (terrain is a heightfield)
    vec3 T = normalize(vec3(1.0, 0.0, 0.0) - N * N.x);
    vec3 B = cross(N, T);
    N = normalize(T * tn.x + B * tn.y + N * max(tn.z, 0.2));
  #endif
    // dynamic snow cover from weather
    float snowFx = uFogParams.z * smoothstep(0.55, 0.85, N.y);
    // keep drift detail under fresh snow (height map + large-scale noise), cold blue in hollows
    float sdet = clamp(0.62 + hsum * 0.45 + (texture(uNoiseTex, vWorld.xz * 0.04).r - 0.5) * 0.35, 0.55, 1.1);
    albedo = mix(albedo, mix(vec3(0.26, 0.32, 0.44), vec3(0.5, 0.55, 0.63), sdet - 0.45) * sdet, snowFx);
    rough = mix(rough, 0.5, snowFx);
    // puddles: wet ground darkens and becomes glossy in low areas (height map valleys)
    float puddle = wet * smoothstep(0.55, 0.2, hsum) * smoothstep(0.8, 0.95, N.y) * (1.0 - snowFx);
    albedo *= mix(1.0, 0.55, wet * 0.6 + puddle * 0.4);
    rough = mix(rough, 0.08, max(wet * 0.4, puddle));
    if (puddle > 0.3) N = normalize(mix(N, vec3(0.0, 1.0, 0.0), puddle));
#else
    // Hand-painted micro-variation for props & characters
    float nz = texture(uNoiseTex, vWorld.xz * 0.35 + vWorld.y * 0.21).r;
    albedo *= 0.9 + nz * 0.2;
    // Snow accumulates on upward faces (Frostheim & snowfall)
    float snow = max(uTint.a, uFogParams.z) * smoothstep(0.45, 0.8, N.y);
    albedo = mix(albedo, vec3(0.5, 0.55, 0.64) * (0.85 + nz * 0.3), snow);
    rough = mix(rough, 0.55, snow);
    albedo *= mix(1.0, 0.7, wet * (1.0 - metal) * (0.4 + 0.6 * N.y));
    rough = mix(rough, rough * 0.4, wet * smoothstep(0.0, 0.6, N.y));
#endif

#if !defined(SKINNED) && !defined(TERRAIN) && !defined(GRASS)
    // see-through: geometry between the camera and the player is dithered away
    if (uPlayer.w > 0.0) {
        vec3 a = uPlayer.xyz + vec3(0.0, 1.0, 0.0), b = uCamPos.xyz;
        vec3 ab = b - a;
        float t = clamp(dot(vWorld - a, ab) / dot(ab, ab), 0.0, 1.0);
        float d = length(vWorld - (a + ab * t));
        float occ = (1.0 - smoothstep(2.2, 4.2, d)) * step(0.06, t) * step(uPlayer.y + 1.2, vWorld.y);
        float dither = fract(dot(floor(gl_FragCoord.xy), vec2(0.5, 0.25)) + 0.125);
        if (occ * 0.85 > dither) discard;
    }
#endif
#ifdef DISSOLVE
    float dn = texture(uNoiseTex, vWorld.xz * 0.45 + vec2(vWorld.y * 0.31)).a;
    if (dn < uFx.y) discard;
    float edge = 1.0 - smoothstep(uFx.y, uFx.y + 0.09, dn);
#endif

    float sh = shadowPCF(vShadow, dot(N, uSunDir.xyz)) * cloudShadow(vWorld);
    sh = mix(1.0, sh, uSunColor.w);
#ifdef WIND
    float wrap = 0.5;  // foliage: light passes through leaves (translucency approximation)
#else
    float wrap = 0.12 + uFx.z * 0.25;
#endif
    vec3 col = shade(albedo, N, V, vWorld, rough, metal, ao, sh, wrap, uFx.z);
#ifdef WIND
    // back-lit leaves glow
    float trans = pow(max(dot(-V, uSunDir.xyz), 0.0), 3.0) * vMat.a;
    col += albedo * uSunColor.rgb * trans * 0.6 * sh;
#endif
#ifdef TERRAIN
    col += vec3(1.0, 0.28, 0.05) * emis;
#else
    float nightOnly = vSkin.w > 0.5 ? uFogParams.w : 1.0;
    col += albedo * emis * (1.0 + uFx.w) * nightOnly;
#endif
#ifdef DISSOLVE
    col += uDissolveColor.rgb * edge * 6.0;
#endif
    col = mix(col, vec3(3.0, 2.9, 2.7), uFx.x);
    // lightning flash
    col += albedo * uMisc.w * 2.0 * max(N.y, 0.2);
    col = applyFog(col, vWorld);
    fragColor = vec4(col * uSkyColor.w, 1.0);
#endif
}
)";

const char* DEPTH_FS = R"(
in vec3 vWorld;
uniform vec4 uFx;
uniform sampler2D uNoiseTex;
out vec4 fragColor;
void main() {
#ifdef DISSOLVE
    float dn = texture(uNoiseTex, vWorld.xz * 0.45 + vec2(vWorld.y * 0.31)).a;
    if (dn < uFx.y) discard;
#endif
    fragColor = vec4(1.0);
}
)";

// ----------------------------------------------------------------------------------------------
// Water / lava. Vertex alpha holds the depth of the water column (precomputed from the terrain),
// so no depth-buffer copy is needed for shoreline foam and depth absorption.
const char* WATER_VS = R"(
in vec3 aPos;
in vec4 aNormal;
in vec4 aColor;
in vec4 aMat;
in vec4 aSkin;
uniform mat4 uModel;
out vec3 vWorld;
out vec4 vColor;
out vec2 vFlow;
out vec4 vShadow;
void main() {
    vec3 wp = (uModel * vec4(aPos, 1.0)).xyz;
    float t = uCamPos.w;
    float depth = aColor.a * 8.0;
    float amp = clamp(depth * 0.25, 0.0, 1.0);
#ifndef LAVA
    // two Gerstner-ish waves
    wp.y += (sin(dot(wp.xz, vec2(0.21, 0.13)) + t * 1.4) * 0.06 + sin(dot(wp.xz, vec2(-0.11, 0.31)) + t * 1.9) * 0.04) * amp;
#else
    wp.y += sin(dot(wp.xz, vec2(0.15, 0.1)) + t * 0.6) * 0.04 * amp;
#endif
    vWorld = wp;
    vColor = aColor;
    vFlow = (aMat.xy * 2.0 - 1.0) * aMat.z * 3.0;  // river flow vector
    vShadow = uShadowMat * vec4(wp, 1.0);
    gl_Position = uViewProj * vec4(wp, 1.0);
}
)";

const char* WATER_FS = R"(
in vec3 vWorld;
in vec4 vColor;
in vec2 vFlow;
in vec4 vShadow;
uniform vec4 uWaterDeep;    // rgb deep colour
uniform vec4 uWaterShallow; // rgb shallow colour, a = frozen (ice) amount
out vec4 fragColor;

vec3 sampleN(vec2 uv) {
    // derivative of noise as normal (tileable noise texture)
    // central differences over +-2 texels: smooth gradient, no 8-bit contour banding
    float e = 2.0 / 256.0;
    float hx = texture(uNoiseTex, uv + vec2(e, 0.0)).g - texture(uNoiseTex, uv - vec2(e, 0.0)).g;
    float hy = texture(uNoiseTex, uv + vec2(0.0, e)).g - texture(uNoiseTex, uv - vec2(0.0, e)).g;
    return vec3(-hx, -hy, 0.0) * 7.0;
}

void main() {
    float t = uCamPos.w;
    float depth = vColor.a * 8.0;
    vec3 V = normalize(uCamPos.xyz - vWorld);
#ifdef LAVA
    vec2 uv = vWorld.xz * 0.06 + vFlow * t * 0.02;
    float n1 = texture(uNoiseTex, uv).g, n2 = texture(uNoiseTex, uv * 2.3 - t * 0.01).r;
    float crust = smoothstep(0.45, 0.62, n1 * 0.7 + n2 * 0.5);
    float pulse = 0.75 + 0.25 * sin(t * 1.5 + n1 * 6.0);
    vec3 hot = mix(vec3(4.5, 1.2, 0.15), vec3(6.0, 3.2, 0.6), n2) * pulse;
    vec3 crustCol = vec3(0.08, 0.05, 0.045) + vec3(0.6, 0.12, 0.02) * (1.0 - crust) * 0.4;
    vec3 col = mix(hot, crustCol, crust);
    col = applyFog(col, vWorld);
    fragColor = vec4(col * uSkyColor.w, 1.0);
#else
    vec2 flow = vFlow;
    vec2 uv1 = vWorld.xz * 0.045 + flow * t * 0.05 + vec2(t * 0.011, t * 0.007);
    vec2 uv2 = vWorld.xz * 0.071 + flow * t * 0.07 - vec2(t * 0.009, -t * 0.013);
    vec3 dn = sampleN(uv1) + sampleN(uv2) * 0.7;
    // rain ripples: expanding rings in a cell grid
    float wet = uGroundColor.w;
    if (wet > 0.01) {
        vec2 cell = floor(vWorld.xz * 1.6);
        vec2 f = fract(vWorld.xz * 1.6) - 0.5;
        float h = fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5);
        float ph = fract(t * 1.3 + h);
        float r = length(f - (vec2(h, fract(h * 7.3)) - 0.5) * 0.4);
        float ring = sin((r - ph * 0.5) * 40.0) * (1.0 - ph) * smoothstep(0.5 * ph + 0.05, 0.5 * ph, r);
        dn.xy += vec2(ring) * 0.5 * wet;
    }
    vec3 N = normalize(vec3(dn.x, 1.0, dn.y));
    float frozen = uWaterShallow.a;
    N = normalize(mix(N, vec3(0.0, 1.0, 0.0), frozen * 0.8));
    float NdV = clamp(dot(N, V), 0.0, 1.0);
    float fres = 0.02 + 0.98 * pow(1.0 - NdV, 5.0);
    fres = fres * 0.85 + 0.04;
    vec3 R = reflect(-V, N);
    vec3 refl = envColor(R, 0.05);
    float absorb = 1.0 - exp(-depth * 0.55);
    vec3 body = mix(uWaterShallow.rgb, uWaterDeep.rgb, absorb);
    float sh = shadowPCF(vShadow, 1.0) * cloudShadow(vWorld);
    sh = mix(1.0, sh, uSunColor.w);
    vec3 amb = mix(uGroundColor.rgb, uSkyColor.rgb, 0.8);
    body *= amb + uSunColor.rgb * uSunDir.w * 0.55 * sh;
    vec3 H = normalize(uSunDir.xyz + V);
    float spec = pow(max(dot(N, H), 0.0), 700.0) * 5.0 * sh * uSunDir.w * (1.0 - 0.6 * wet);
    vec3 col = mix(body, refl, fres) + uSunColor.rgb * spec;
    // shoreline foam
    float fn = texture(uNoiseTex, vWorld.xz * 0.22 + flow * t * 0.08 + t * 0.02).r;
    float foam = smoothstep(0.55, 0.0, depth) * smoothstep(0.35, 0.65, fn + 0.25 * sin(t * 2.0 + depth * 10.0));
    foam += smoothstep(0.6, 1.0, length(flow) * fn) * 0.5;  // rapids
    col = mix(col, vec3(0.95) * (amb + uSunColor.rgb * uSunDir.w * 0.6), clamp(foam, 0.0, 1.0) * (1.0 - frozen));
    // ice
    vec3 ice = vec3(0.75, 0.88, 0.97) * (amb + uSunColor.rgb * uSunDir.w * sh * 0.7) + uSunColor.rgb * spec * 0.4;
    float crack = smoothstep(0.02, 0.0, abs(texture(uNoiseTex, vWorld.xz * 0.05).r - 0.5));
    ice *= 1.0 - crack * 0.3;
    col = mix(col, ice, frozen);
    // point lights reflect on water
    for (int i = 0; i < 8; i++) {
        if (float(i) >= uMisc.x) break;
        vec3 lv = uLightPos[i].xyz - vWorld;
        float d = length(lv);
        float att = clamp(1.0 - d / uLightPos[i].w, 0.0, 1.0);
        col += uLightCol[i].rgb * att * att * (0.15 + pow(max(dot(N, normalize(normalize(lv) + V)), 0.0), 60.0) * 2.0);
    }
    col += vec3(uMisc.w) * 0.6;
    float alpha = mix(clamp(0.45 + absorb * 0.6 + fres * 0.3 + foam, 0.0, 1.0), 0.97, frozen);
    alpha *= smoothstep(0.0, 0.08, depth);
    col = applyFog(col, vWorld);
    fragColor = vec4(col * uSkyColor.w, alpha);
#endif
}
)";

// ----------------------------------------------------------------------------------------------
const char* SKY_VS = R"(
in vec3 aPos;
out vec3 vDir;
uniform mat4 uInvViewProj;
void main() {
    vec4 p = vec4(aPos.xy, 1.0, 1.0);
    vec4 w = uInvViewProj * p;
    vDir = w.xyz / w.w - uCamPos.xyz;
    gl_Position = vec4(aPos.xy, 0.9999, 1.0);
}
)";

const char* SKY_FS = R"(
in vec3 vDir;
uniform sampler2D uNoiseTex;
uniform vec4 uSkyTop;      // zenith colour
uniform vec4 uSkyHorizon;  // horizon colour, a = cloud cover
out vec4 fragColor;
void main() {
    vec3 d = normalize(vDir);
    float h = clamp(d.y, -0.2, 1.0);
    vec3 col = mix(uSkyHorizon.rgb, uSkyTop.rgb, pow(max(h, 0.0), 0.6));
    col = mix(col, uFogColor.rgb, smoothstep(0.15, -0.1, h));
    float sd = max(dot(d, uSunDir.xyz), 0.0);
    col += uSunColor.rgb * (pow(sd, 900.0) * 30.0 + pow(sd, 12.0) * 0.4) * uSunDir.w;
    // moon on the opposite side at night
    float md = max(dot(d, -uSunDir.xyz), 0.0);
    col += vec3(0.8, 0.85, 1.0) * pow(md, 2000.0) * 8.0 * uFogParams.w;
    // stars
    vec2 suv = d.xz / (d.y + 0.3) * 40.0;
    float st = texture(uNoiseTex, suv * 0.05).r;
    col += vec3(pow(st, 40.0) * 30.0) * uFogParams.w * smoothstep(0.0, 0.3, h);
    // clouds on a plane
    if (d.y > 0.0) {
        vec2 cuv = d.xz / (d.y + 0.05) * 0.25 + uWind.xy * uCamPos.w * 0.003;
        float c = texture(uNoiseTex, cuv).b * 0.65 + texture(uNoiseTex, cuv * 2.7).g * 0.35;
        float cover = uSkyHorizon.a;
        float cl = smoothstep(1.0 - cover, 1.15 - cover * 0.6, c) * smoothstep(0.0, 0.25, d.y);
        vec3 cc = mix(uSkyHorizon.rgb * 1.1, uSunColor.rgb * uSunDir.w + uSkyTop.rgb * 0.4, 0.5) ;
        col = mix(col, cc, cl * 0.9);
    }
    col += vec3(uMisc.w) * 0.8;
    fragColor = vec4(col * uSkyColor.w, 1.0);
}
)";

// ----------------------------------------------------------------------------------------------
// Particles: camera-facing quads, instanced. Per instance: pos.xyz+size, color rgba, (rot, frame, stretch, soft).
const char* PARTICLE_VS = R"(
in vec3 aPos;          // unit quad corner in xy (-0.5..0.5)
in vec4 iP0;           // xyz pos, w size
in vec4 iP1;           // rgba colour (float)
in vec4 iP2;           // x rotation, y atlas frame, z stretch (velocity aligned), w unused
in vec4 iP3;           // xyz velocity dir (for stretched sparks)
uniform vec3 uCamRight;
uniform vec3 uCamUp;
out vec2 vUV;
out vec4 vColor;
out vec3 vWorld;
void main() {
    vec2 c = aPos.xy;
    float r = iP2.x;
    vec2 rc = vec2(c.x * cos(r) - c.y * sin(r), c.x * sin(r) + c.y * cos(r));
    vec3 right = uCamRight, up = uCamUp;
    if (iP2.z > 0.0) {
        // velocity-stretched (sparks, rain streaks)
        vec3 vdir = iP3.xyz;
        vec4 a = uViewProj * vec4(iP0.xyz, 1.0), b = uViewProj * vec4(iP0.xyz + vdir, 1.0);
        vec2 sd = normalize((b.xy / b.w - a.xy / a.w) + 1e-5);
        vec3 vr = normalize(uCamRight * sd.x + uCamUp * sd.y);
        up = vr * (1.0 + iP2.z);
        right = normalize(cross(vr, normalize(uCamPos.xyz - iP0.xyz)));
        rc = c;
    }
    vec3 wp = iP0.xyz + (right * rc.x + up * rc.y) * iP0.w;
    vWorld = wp;
    float fr = iP2.y;
    vec2 cell = vec2(mod(fr, 4.0), floor(fr / 4.0));
    vUV = (cell + (c + 0.5)) / 4.0;
    vColor = iP1;
    gl_Position = uViewProj * vec4(wp, 1.0);
}
)";

const char* PARTICLE_FS = R"(
in vec2 vUV;
in vec4 vColor;
in vec3 vWorld;
uniform sampler2D uAtlas;
out vec4 fragColor;
void main() {
    vec4 t = texture(uAtlas, vUV);
    vec4 c = vColor * t;
#ifdef ADDITIVE
    vec3 rgb = c.rgb * c.a;
    // fog fades additive particles to nothing
    float dist = length(vWorld - uCamPos.xyz);
    rgb *= exp(-max(dist - 30.0, 0.0) * uFogColor.w * 0.5);
    fragColor = vec4(rgb * uSkyColor.w, 0.0);
#else
    vec3 rgb = c.rgb * mix(uGroundColor.rgb + uSkyColor.rgb, vec3(1.0), 0.35) * 0.9 + c.rgb * uSunColor.rgb * uSunDir.w * 0.35;
    rgb = applyFog(rgb, vWorld);
    fragColor = vec4(rgb * uSkyColor.w, c.a);
#endif
}
)";

// Ribbon trails (weapon swings, magic projectiles): position + colour per vertex, additive.
const char* RIBBON_VS = R"(
in vec3 aPos;
in vec4 aColor;
in vec4 aMat;
out vec4 vColor;
out vec2 vUV;
void main() {
    vColor = aColor;
    vUV = aMat.xy;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";
const char* RIBBON_FS = R"(
in vec4 vColor;
in vec2 vUV;
uniform sampler2D uNoiseTex;
uniform float uIntensity;
out vec4 fragColor;
void main() {
    float edge = 1.0 - abs(vUV.y * 2.0 - 1.0);
    float n = texture(uNoiseTex, vec2(vUV.x * 2.0 - uCamPos.w * 1.5, vUV.y * 0.5)).r;
    float a = vColor.a * pow(edge, 1.5) * (0.6 + 0.6 * n) * vUV.x;
    fragColor = vec4(vColor.rgb * a * uIntensity, 0.0);
}
)";

// ----------------------------------------------------------------------------------------------
// Post-processing
const char* FULLSCREEN_VS = R"(
in vec3 aPos;
out vec2 vUV;
void main() {
    vUV = aPos.xy * 0.5 + 0.5;
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
}
)";

// Bloom downsample (13-tap, with brightness threshold on the first pass + Karis average to stop fireflies)
const char* BLOOM_DOWN_FS = R"(
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThreshold;  // < 0 = no threshold
uniform float uHdrScale;
out vec4 fragColor;
vec3 S(vec2 o) {
    vec3 c = texture(uSrc, vUV + o * uTexel).rgb * uHdrScale;
    // guard: a single NaN/Inf pixel would otherwise smear across the whole bloom chain
    return (any(isnan(c)) || any(isinf(c))) ? vec3(0.0) : min(c, vec3(64.0));
}
float karis(vec3 c) { return 1.0 / (1.0 + max(c.r, max(c.g, c.b))); }
void main() {
    vec3 a = S(vec2(-2, 2)), b = S(vec2(0, 2)), c = S(vec2(2, 2));
    vec3 d = S(vec2(-2, 0)), e = S(vec2(0, 0)), f = S(vec2(2, 0));
    vec3 g = S(vec2(-2, -2)), h = S(vec2(0, -2)), i = S(vec2(2, -2));
    vec3 j = S(vec2(-1, 1)), k = S(vec2(1, 1)), l = S(vec2(-1, -1)), m = S(vec2(1, -1));
    vec3 col;
    if (uThreshold >= 0.0) {
        vec3 g0 = (a + b + d + e) * 0.25, g1 = (b + c + e + f) * 0.25, g2 = (d + e + g + h) * 0.25, g3 = (e + f + h + i) * 0.25, g4 = (j + k + l + m) * 0.25;
        col = g0 * karis(g0) * 0.125 + g1 * karis(g1) * 0.125 + g2 * karis(g2) * 0.125 + g3 * karis(g3) * 0.125 + g4 * karis(g4) * 0.5;
        col /= (karis(g0) * 0.125 + karis(g1) * 0.125 + karis(g2) * 0.125 + karis(g3) * 0.125 + karis(g4) * 0.5) ;
        float br = max(col.r, max(col.g, col.b));
        float soft = clamp(br - uThreshold + 0.5, 0.0, 1.0);
        soft = soft * soft * 0.5;
        float contrib = max(soft, br - uThreshold) / max(br, 1e-4);
        col *= contrib;
    } else {
        col = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    }
    fragColor = vec4(col, 1.0);
}
)";

const char* BLOOM_UP_FS = R"(
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uRadius;
out vec4 fragColor;
void main() {
    vec2 o = uTexel * uRadius;
    vec3 c = texture(uSrc, vUV).rgb * 4.0;
    c += (texture(uSrc, vUV + vec2(-o.x, 0)).rgb + texture(uSrc, vUV + vec2(o.x, 0)).rgb + texture(uSrc, vUV + vec2(0, o.y)).rgb + texture(uSrc, vUV + vec2(0, -o.y)).rgb) * 2.0;
    c += texture(uSrc, vUV + vec2(-o.x, o.y)).rgb + texture(uSrc, vUV + vec2(o.x, o.y)).rgb + texture(uSrc, vUV + vec2(-o.x, -o.y)).rgb + texture(uSrc, vUV + vec2(o.x, -o.y)).rgb;
    fragColor = vec4(c / 16.0, 1.0);
}
)";

// Final composite: exposure, bloom, depth of field / tilt-shift, ACES tone mapping,
// colour grading (lift / gamma / gain, saturation, contrast, temperature), vignette,
// optional chromatic aberration, film grain, screen fades.
const char* COMPOSITE_FS = R"(
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uBlur;     // half/quarter res blurred scene for DoF
uniform sampler2D uDepth;
uniform vec4 uPost0;  // x exposure, y bloom intensity, z vignette, w chromatic aberration
uniform vec4 uPost1;  // x saturation, y contrast, z temperature (-1..1), w grain
uniform vec4 uLift;   // rgb lift, a dof amount
uniform vec4 uGamma;  // rgb gamma, a dof focus distance (m)
uniform vec4 uGain;   // rgb gain, a tilt-shift strength
uniform vec4 uFade;   // rgb fade colour, a amount
uniform vec4 uProj;   // x near, y far, z hdr scale, w damage vignette
uniform float uTime;
uniform float uAlphaFromScene;
out vec4 fragColor;

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float linDepth(float z) { float n = uProj.x, f = uProj.y; z = z * 2.0 - 1.0; return 2.0 * n * f / (f + n - z * (f - n)); }

void main() {
    vec2 uv = vUV;
    vec3 col;
    if (uPost0.w > 0.0) {
        vec2 dir = (uv - 0.5) * uPost0.w * 0.012;
        col = vec3(texture(uScene, uv + dir).r, texture(uScene, uv).g, texture(uScene, uv - dir).b);
    } else col = texture(uScene, uv).rgb;
    col *= uProj.z;
    // Depth of field: cinematic focus + tilt-shift (miniature diorama feel of the 2.5D view)
    float dofAmt = uLift.a;
    float tilt = uGain.a * smoothstep(0.18, 0.5, abs(uv.y - 0.47));
    float coc = tilt;
    if (dofAmt > 0.0) {
        float d = linDepth(texture(uDepth, uv).r);
        coc = max(coc, clamp(abs(d - uGamma.a) / max(uGamma.a, 1.0) * 1.6, 0.0, 1.0) * dofAmt);
    }
    if (coc > 0.01) col = mix(col, texture(uBlur, uv).rgb * uProj.z, clamp(coc, 0.0, 1.0));
    col += texture(uBloom, uv).rgb * uPost0.y;
    col *= uPost0.x;
    // white balance
    col *= vec3(1.0 + uPost1.z * 0.1, 1.0, 1.0 - uPost1.z * 0.1);
    col = aces(col);
    // grading in display space
    col = pow(max(col, 0.0), vec3(1.0 / 2.2));
    col = uGain.rgb * (col + uLift.rgb * (1.0 - col));
    col = pow(max(col, 0.0), 1.0 / uGamma.rgb);
    float l = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(vec3(l), col, uPost1.x);
    col = (col - 0.5) * uPost1.y + 0.5;
    // vignette (+ red pulse when badly hurt)
    vec2 vc = uv - 0.5;
    float vig = 1.0 - dot(vc, vc) * uPost0.z * 1.6;
    col *= clamp(vig, 0.0, 1.0);
    col = mix(col, vec3(0.55, 0.0, 0.0), uProj.w * smoothstep(0.15, 0.6, length(vc)));
    // grain
    float n = fract(sin(dot(uv * (uTime + 1.37), vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    col += n * uPost1.w;
    col = mix(col, uFade.rgb, uFade.a);
    float outA = uAlphaFromScene > 0.5 ? texture(uScene, uv).a : dot(clamp(col, 0.0, 1.0), vec3(0.299, 0.587, 0.114));
    fragColor = vec4(clamp(col, 0.0, 1.0), outA);
}
)";

// FXAA 3.11-style (quality preset low) operating on the LDR image, luma in alpha.
const char* FXAA_FS = R"(
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uTexel;
out vec4 fragColor;
void main() {
    vec3 rgbM = texture(uSrc, vUV).rgb;
    float lM = texture(uSrc, vUV).a;
    float lNW = texture(uSrc, vUV + vec2(-1, -1) * uTexel).a, lNE = texture(uSrc, vUV + vec2(1, -1) * uTexel).a;
    float lSW = texture(uSrc, vUV + vec2(-1, 1) * uTexel).a, lSE = texture(uSrc, vUV + vec2(1, 1) * uTexel).a;
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    if (lMax - lMin < max(0.0312, lMax * 0.125)) { fragColor = vec4(rgbM, 1.0); return; }
    vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), ((lNW + lSW) - (lNE + lSE)));
    float red = max((lNW + lNE + lSW + lSE) * 0.03125, 1.0 / 128.0);
    float rcp = 1.0 / (min(abs(dir.x), abs(dir.y)) + red);
    dir = clamp(dir * rcp, vec2(-8.0), vec2(8.0)) * uTexel;
    vec3 a = 0.5 * (texture(uSrc, vUV + dir * (1.0 / 3.0 - 0.5)).rgb + texture(uSrc, vUV + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 b = a * 0.5 + 0.25 * (texture(uSrc, vUV + dir * -0.5).rgb + texture(uSrc, vUV + dir * 0.5).rgb);
    float lB = dot(b, vec3(0.299, 0.587, 0.114));
    fragColor = vec4((lB < lMin || lB > lMax) ? a : b, 1.0);
}
)";

const char* BLIT_FS = R"(
in vec2 vUV;
uniform sampler2D uSrc;
out vec4 fragColor;
void main() { fragColor = vec4(texture(uSrc, vUV).rgb, 1.0); }
)";

// ----------------------------------------------------------------------------------------------
// UI: textured quads + SDF text, all in one batch. aMat.x selects the mode.
const char* UI_VS = R"(
in vec2 aPos;      // pixels
in vec4 aColor;
in vec4 aMat;      // uv.xy, mode, sdf softness
uniform vec2 uScreen;
out vec4 vColor;
out vec2 vUV;
out float vMode;
out float vSoft;
void main() {
    vColor = aColor;
    vUV = aMat.xy;
    vMode = aMat.z;
    vSoft = aMat.w;
    vec2 p = aPos.xy / uScreen * 2.0 - 1.0;
    gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
}
)";
const char* UI_FS = R"(
in vec4 vColor;
in vec2 vUV;
in float vMode;
in float vSoft;
uniform sampler2D uAtlas;   // icons / frames
uniform sampler2D uFont;    // SDF glyphs
uniform sampler2D uImage;   // dynamic images (minimap, portraits)
out vec4 fragColor;
void main() {
    if (vMode < 0.5) {
        fragColor = vColor;  // solid
    } else if (vMode < 1.5) {
        fragColor = texture(uAtlas, vUV) * vColor;
    } else if (vMode < 2.5) {
        float d = texture(uFont, vUV).r;
        float w = max(vSoft, 0.02);
        float a = smoothstep(0.5 - w, 0.5 + w, d);
        fragColor = vec4(vColor.rgb, vColor.a * a);
    } else if (vMode < 3.5) {
        // SDF text with dark outline / shadow for readability over the world
        float d = texture(uFont, vUV).r;
        float w = max(vSoft, 0.02);
        float fill = smoothstep(0.5 - w, 0.5 + w, d);
        float outl = smoothstep(0.3 - w, 0.3 + w, d);
        vec3 c = mix(vec3(0.03, 0.02, 0.01), vColor.rgb, fill);
        fragColor = vec4(c, vColor.a * outl);
    } else {
        fragColor = texture(uImage, vUV) * vColor;
    }
}
)";

}  // namespace shaders
}  // namespace agr
