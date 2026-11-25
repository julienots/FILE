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
    vec4 uPushers[8];  // grass benders: xyz position, w = sign(strength) * (floor(|strength| * 64) + radius / 4)
    vec4 uWater;       // 3.5.0 x water surface level near the camera, y caustics strength (0 = off), z camera under water
};
const float PI = 3.14159265;
// 3.0.2 integer hash: exact on every GPU (the classic fract(sin(x) * 43758) breaks into bands and lines on
// mobile GPUs, whose sin() loses its precision on large arguments - world coordinates, screen pixels, time)
float hash12(vec2 p) {
    uvec2 q = uvec2(ivec2(floor(p)) + ivec2(1048576));
    uint h = (q.x * 1597334673u) ^ (q.y * 3812015801u);
    h ^= h >> 16u; h *= 2246822519u; h ^= h >> 13u; h *= 3266489917u; h ^= h >> 16u;
    return float(h & 0x00FFFFFFu) / 16777216.0;
}
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
#ifdef TEXTURED
out vec2 vUV;
#endif

// Gust fronts rolling across the land along the wind direction (0..1): grass bends in waves.
float windWave(vec3 wp) {
    vec2 d = normalize(uWind.xy + vec2(1e-4));
    float along = dot(wp.xz, d) * 0.09 - uCamPos.w * 1.25;
    float across = dot(wp.xz, vec2(-d.y, d.x)) * 0.035;
    float w = sin(along + sin(across) * 1.6) * 0.5 + 0.5;
    return w * w * w * clamp(uWind.z * 1.4, 0.0, 1.0);
}

// 2.6.2 h > 0: height of the vertex above its root (grass blades). The blade BENDS around its root with its length
// kept: it leans at most ~55 degrees and its tip never goes below the ground (it used to shear sideways and sink
// into the soil in strong wind). h <= 0: tree foliage, a bounded sway so the crown never leaves its trunk.
vec3 windOffset(vec3 wp, float weight, float h) {
    float t = uCamPos.w;
    float phase = dot(wp.xz, vec2(0.13, 0.17));
    float sway = sin(t * 1.3 + phase) * 0.6 + sin(t * 2.7 + phase * 1.7) * 0.3;
    float gust = 0.55 + 0.45 * sin(t * 0.37 + wp.x * 0.021 + wp.z * 0.013) * uWind.w;
    gust += windWave(wp) * 0.9;                                   // travelling gust fronts
    vec2 off = uWind.xy * (sway * 0.35 + gust) * uWind.z * weight;
    off += vec2(sin(t * 7.3 + wp.x * 3.1 + wp.z * 1.7), cos(t * 6.1 + wp.z * 2.9)) * 0.04 * weight * uWind.z;
    float L = length(off);
    if (h > 0.0) {
        float m = h * 0.82;
        if (L > m) { off *= m / L; L = m; }
        return vec3(off.x, -(h - sqrt(max(h * h - L * L, 0.0))), off.y);
    }
    float m = 0.28 * weight + 0.02;
    if (L > m) { off *= m / L; L = m; }
    return vec3(off.x, -L * L * 0.3, off.y);
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
    float grassDown = 0.0;
#ifdef WIND
    float wweight = aMat.a;
    float bladeH = -1.0;
  #ifdef INSTANCED
    wweight *= iScale;
  #endif
  #if defined(GRASS) && defined(INSTANCED)
    bladeH = max(p.y * iScale, 0.001);
  #endif
    wp += windOffset(wp, wweight, bladeH);
  #ifdef GRASS
    float sheen = windWave(wp) * aMat.a;
    // grass bent by the hero, his trail (it stays down a moment, then springs back with a little wobble),
    // and nearby characters / the mount
    vec2 pushv = vec2(0.0);
    float down = 0.0;
    for (int i = 0; i < 8; i++) {
        vec4 P = uPushers[i];
        if (P.w == 0.0) continue;
        float aw = abs(P.w);
        float st = floor(aw) / 64.0 * sign(P.w);
        float r = fract(aw) * 4.0;
        vec2 d = wp.xz - P.xz;
        float dist = length(d);
        float k = (1.0 - smoothstep(r * 0.25, r, dist)) * step(abs(wp.y - P.y), 2.5);
        pushv += d / max(dist, 0.001) * k * st;
        down += k * max(st, 0.0);
    }
    float pl = length(pushv);
    if (pl > 1.0) pushv /= pl;
    grassDown = min(down, 1.0);
    // bend grows with the blade's height: tall meadow grass parts around the body, short grass just flattens
    float bendH = clamp(wweight * max(p.y, 0.75) * 1.15, 0.0, 1.4);
    wp.xz += pushv * bendH * 0.55;
    wp.y -= min(grassDown * bendH * 0.28, bladeH > 0.0 ? bladeH * 0.4 : 0.15);   // trodden, never into the soil (3.7.1 bent aside more than flattened)
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
#if defined(WIND) && defined(GRASS)
    vColor.rgb *= 1.0 + sheen * vec3(0.22, 0.2, 0.1);     // silvery-gold sheen as the gust passes (3.0.2 subtler)
    vColor.rgb *= 1.0 - 0.1 * grassDown;                    // trodden grass: a slightly darker path
#endif
    vMat = aMat;
#ifdef TEXTURED
    // 16-bit UVs packed little-endian in the four material bytes
    vUV = vec2(aMat.x * 255.0 + aMat.y * 65280.0, aMat.z * 255.0 + aMat.w * 65280.0) / 65535.0;
#endif
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
    float a = hash12(gl_FragCoord.xy) * 6.2831;
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

// 1.9.2 rain impacts: two layers of expanding rings in jittered cells -> normal offset (xz)
vec2 rainRipples(vec2 p, float t) {
    vec2 acc = vec2(0.0);
    for (int L = 0; L < 2; L++) {
        float sc = L == 0 ? 2.1 : 3.3;
        vec2 q = p * sc + float(L) * 17.3;
        vec2 cell = floor(q), f = fract(q) - 0.5;
        float h = hash12(cell);
        float ph = fract(t * (1.1 + 0.5 * h) + h);
        vec2 c = (vec2(h, fract(h * 7.3)) - 0.5) * 0.5;
        vec2 d = f - c;
        float r = length(d);
        float ring = sin((r - ph * 0.45) * 46.0) * (1.0 - ph) * smoothstep(0.45 * ph + 0.06, 0.45 * ph, r) * smoothstep(0.0, 0.05, r);
        acc += d / max(r, 1e-3) * ring * (L == 0 ? 1.0 : 0.6);
    }
    return acc;
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
#ifndef DBG_NOLIGHTS
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
#endif
    // Rim light improves character readability against the ground (2.5D).
    col += rim * pow(1.0 - NdV, 3.0) * (uSkyColor.rgb * 0.6 + uSunColor.rgb * 0.4) * (0.5 + 0.5 * shadow);
    return col;
}

// 3.5.0 caustics: the sunlight focused by the waves, a moving net of bright lines on whatever lies under the
// water near the camera (sea floor, rocks, wrecks, divers). Two warped wave layers, sharpened.
float causticNet(vec2 p, float t) {
    p += 0.55 * vec2(sin(p.y * 1.3 + t * 0.9), cos(p.x * 1.1 - t * 0.8));
    float w = sin(dot(p, vec2(0.8, 0.6)) * 1.9 + t * 1.2) + sin(dot(p, vec2(-0.5, 0.86)) * 2.3 - t * 1.4)
            + sin(dot(p, vec2(0.25, -0.97)) * 1.7 + t * 1.0) + sin(dot(p, vec2(-0.92, -0.38)) * 2.6 - t * 0.8);
    return pow(1.0 - abs(sin(w * 0.9)), 7.0);
}
float caustics(vec3 P, vec3 N) {
    if (uWater.y <= 0.0 || P.y > uWater.x - 0.05) return 0.0;
    float depth = uWater.x - P.y;
    float t = uCamPos.w;
    float c = causticNet(P.xz * 0.9, t) * 0.65 + causticNet(P.xz * 1.7 + 13.7, t * 1.3) * 0.45;
    return c * exp(-depth * 0.13) * smoothstep(0.05, 0.5, depth) * max(N.y * 0.75 + 0.25, 0.0) * uWater.y;
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
#ifdef TEXTURED
in vec2 vUV;
uniform sampler2D uAlbedoTex;
#endif
#ifdef TERRAIN
uniform mediump sampler2DArray uTerrainTex;  // albedo.rgb + height.a
uniform mediump sampler2DArray uTerrainNrm;  // tangent-space normal.rgb
#endif
#ifdef OUTLINE
uniform vec4 uOutlineColor;
#endif
#ifdef FAR
uniform vec4 uFar;   // xy centre, z radius covered by the detailed chunks
#endif
out vec4 fragColor;

#ifdef TERRAIN
// layers: 0 grass, 1 dirt, 2 rock, 3 snow, 4 sand, 5 mud, 6 cobble, 7 ash
// Explicit-gradient fetches: these are called inside per-pixel branches (only the layers present are
// read), where implicit derivatives are undefined on mobile GPUs.
vec4 sampleLayer(int l, vec2 uv, vec2 dx, vec2 dy) { return textureGrad(uTerrainTex, vec3(uv, float(l)), dx, dy); }
vec3 sampleNrm(int l, vec2 uv, vec2 dx, vec2 dy) { return textureGrad(uTerrainNrm, vec3(uv, float(l)), dx, dy).xyz * 2.0 - 1.0; }
#endif

void main() {
#ifdef OUTLINE
    fragColor = vec4(uOutlineColor.rgb * uSkyColor.w, 1.0);
    return;
#else
#ifdef FAR
    if (length(vWorld.xz - uFar.xy) < uFar.z) discard;
#endif
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos.xyz - vWorld);
    // vertex colours are authored in sRGB: convert to linear for lighting
    vec3 albedo = pow(max(vColor.rgb * uTint.rgb, vec3(0.0)), vec3(2.2));
    float ao = vColor.a;
    float rough = vMat.r, metal = vMat.g;
    float emis = vMat.b * 4.0;
    float wet = uGroundColor.w;
    float puddleK = 0.0;
#ifdef TEXTURED
    // AI-generated textures are bright and contain baked lighting: bring them into the game's albedo
    // range (procedural characters) and restore some saturation lost to the baked highlights.
    vec3 tx = texture(uAlbedoTex, vUV).rgb;
    albedo = pow(tx, vec3(2.2));
    float alum = dot(albedo, vec3(0.2126, 0.7152, 0.0722));
    // material guess from the painted colour: bright desaturated = steel, bright warm saturated = gold/brass
    float tmax = max(tx.r, max(tx.g, tx.b)), tmin = min(tx.r, min(tx.g, tx.b));
    float tsat = (tmax - tmin) / max(tmax, 1e-3);
    float steel = smoothstep(0.34, 0.58, tmax) * (1.0 - smoothstep(0.1, 0.3, tsat));
    float gold = smoothstep(0.42, 0.7, tmax) * smoothstep(0.35, 0.6, tsat) * step(tx.b, tx.r * 0.75);
    metal = max(steel * 0.85, gold * 0.9) * (1.0 - vSkin.w);   // vSkin.w = cloth flag from the importer
    rough = mix(0.8, 0.3, metal);
  #ifdef SKINNED
    vec3 dielectric = max(mix(vec3(alum), albedo, 1.3), 0.0) * 0.48;
  #else
    // 1.10.3 buildings (Tripo): their textures carry less baked light than the characters' - keep them brighter
    vec3 dielectric = max(mix(vec3(alum), albedo, 1.15), 0.0) * 0.74;
  #endif
    albedo = mix(dielectric, albedo * 0.85, metal) * uTint.rgb;   // metals: albedo is the specular colour
    emis = 0.0;
    if (!gl_FrontFacing) N = -N;   // double-sided imported meshes (capes)
#endif

#ifdef TERRAIN
    // Splat weights from vertex: mat.r rock, mat.g dirt, mat.b snow, mat.a sand, skin.x mud, skin.y cobble, skin.z ash
    float wRock = vMat.r, wDirt = vMat.g, wSnow = vMat.b, wSand = vMat.a, wMud = vSkin.x, wCob = vSkin.y, wAsh = vSkin.z;
    float wGrass = max(0.0, 1.0 - (wRock + wDirt + wSnow + wSand + wMud + wCob + wAsh));
    // Slope-driven rock override computed per pixel for crisp cliff edges.
    float slope = 1.0 - N.y;
    float cliff = smoothstep(0.28, 0.45, slope);
    // 2.4.0 freshly dug ground (vSkin.w): the steep walls of holes and craters are soil, not rock
    float dugK = vSkin.w;
    wDirt += cliff * dugK; cliff *= 1.0 - dugK;
    vec2 uv = vWorld.xz * 0.28;
    // anti-tiling for the dominant ground layers: a second, rotated & rescaled sample is blended in by
    // large-scale noise, so the texture never repeats visibly across a meadow
    float ws[8] = float[8](wGrass * (1.0 - cliff), wDirt * (1.0 - cliff), wRock + cliff, wSnow * (1.0 - cliff * 0.7), wSand * (1.0 - cliff), wMud * (1.0 - cliff), wCob, wAsh * (1.0 - cliff));
    // Mobile: only fetch the layers present at this pixel (usually 1-3 of 8; regions are coherent so the
    // branches are uniform over most of the screen).
    vec4 smp[8];
    for (int i = 0; i < 8; i++) smp[i] = vec4(0.0);
#ifdef DBG_ONELAYER
    for (int i = 1; i < 8; i++) ws[i] = 0.0;
#endif
    // 1.11.1 rock: true triplanar - the three projections are sampled and their COLOURS blended by the normal
    // (blending the coordinates, as before, smeared the texture into diagonal streaks on banks and slopes)
    vec2 uvT = vWorld.xz * 0.22, uvX = vWorld.zy * 0.22, uvZ = vWorld.xy * 0.22;
    vec2 tX = dFdx(uvT), tY = dFdy(uvT), xX = dFdx(uvX), xY = dFdy(uvX), zX = dFdx(uvZ), zY = dFdy(uvZ);
    float bxw = abs(N.x), bzw = abs(N.z);
    float bsum = bxw + bzw + 1e-4; bxw /= bsum; bzw /= bsum;
    bxw = smoothstep(0.2, 0.8, bxw); bzw = 1.0 - bxw;
    vec2 rockUV = cliff > 0.5 ? (bxw > 0.5 ? uvX : uvZ) : uvT;      // normal map: dominant projection
    vec2 rX = cliff > 0.5 ? (bxw > 0.5 ? xX : zX) : tX, rY = cliff > 0.5 ? (bxw > 0.5 ? xY : zY) : tY;
    vec2 dX = dFdx(uv), dY = dFdy(uv);   // gradients outside branches
    mat2 rot2 = mat2(0.8, -0.6, 0.6, 0.8);
    vec2 uv2 = rot2 * uv * 0.53 + vec2(0.37, 0.71);
    vec2 dX2 = rot2 * dX * 0.53, dY2 = rot2 * dY * 0.53;
    float tb = smoothstep(0.3, 0.7, texture(uNoiseTex, vWorld.xz * 0.031).r);
    if (ws[0] > 0.001) smp[0] = mix(sampleLayer(0, uv, dX, dY), sampleLayer(0, uv2, dX2, dY2), tb);
    if (ws[1] > 0.001) smp[1] = mix(sampleLayer(1, uv * 0.9, dX * 0.9, dY * 0.9), sampleLayer(1, uv2 * 0.9, dX2 * 0.9, dY2 * 0.9), tb);
    if (ws[2] > 0.001) {
        vec4 top = cliff < 0.99 ? sampleLayer(2, uvT, tX, tY) : vec4(0.0);
        vec4 side = vec4(0.0);
        if (cliff > 0.01) {
            // 3.0.2 a second, rotated and larger sample breaks the repeating strata on big mountain flanks
            float tbs = smoothstep(0.25, 0.75, texture(uNoiseTex, vWorld.xz * 0.017 + vWorld.y * 0.011).g);
            mat2 rs = mat2(0.6, -0.8, 0.8, 0.6);
            if (bxw > 0.01) side += mix(sampleLayer(2, uvX, xX, xY), sampleLayer(2, rs * uvX * 0.43 + 0.31, rs * xX * 0.43, rs * xY * 0.43), tbs * 0.7) * bxw;
            if (bzw > 0.01) side += mix(sampleLayer(2, uvZ, zX, zY), sampleLayer(2, rs * uvZ * 0.43 + 0.57, rs * zX * 0.43, rs * zY * 0.43), tbs * 0.7) * bzw;
        }
        smp[2] = mix(top, side, cliff);
    }
    if (ws[3] > 0.001) smp[3] = sampleLayer(3, uv * 0.7, dX * 0.7, dY * 0.7);
    if (ws[4] > 0.001) smp[4] = sampleLayer(4, uv * 0.8, dX * 0.8, dY * 0.8);
    if (ws[5] > 0.001) smp[5] = sampleLayer(5, uv, dX, dY);
    if (ws[6] > 0.001) smp[6] = sampleLayer(6, uv * 0.6, dX * 0.6, dY * 0.6);
    if (ws[7] > 0.001) smp[7] = sampleLayer(7, uv * 0.7, dX * 0.7, dY * 0.7);
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
        tn += (i == 2 ? sampleNrm(i, rockUV, rX, rY) : sampleNrm(i, uv, dX, dY)) * w;
  #endif
    }
    emis = 0.0;
    albedo = pow(alb, vec3(2.2)) * vColor.rgb * 2.0;  // texture sRGB -> linear; vertex colour is a regional tint around 0.5
    // macro variation breaks texture tiling at gameplay distance
    float macro = texture(uNoiseTex, vWorld.xz * 0.0045).g * 0.7 + texture(uNoiseTex, vWorld.xz * 0.019).r * 0.3;
    albedo *= 0.78 + macro * 0.44;
    albedo = mix(albedo, albedo * vec3(1.08, 1.0, 0.85), smoothstep(0.55, 0.75, macro) * wGrass);  // sun-dried patches
    // 3.6.1 fine detail near the camera: the 256 px ground layers are magnified there and read as a blur; a crisp
    // grain of short blades and soil, and darker gaps between the tufts (shaded earth), fading out with distance
    {
        float near = 1.0 - smoothstep(16.0, 60.0, length(vWorld - uCamPos.xyz));
        if (near > 0.0) {
            // the blotches of the magnified grass layer give way to its average colour (the blades carry the detail)
            vec3 flatG = pow(textureLod(uTerrainTex, vec3(0.5, 0.5, 0.0), 12.0).rgb, vec3(2.2)) * vColor.rgb * 2.0 * (0.78 + macro * 0.44);
            albedo = mix(albedo, flatG * 0.72, near * wGrass * 0.6);   // the shade at the foot of the blades
            float g1 = texture(uNoiseTex, vWorld.xz * 1.9).r, g2 = texture(uNoiseTex, vWorld.xz * vec2(4.7, 3.9) + 0.3).g;
            float grain = (g1 - 0.5) * 0.9 + (g2 - 0.5) * 0.7;
            albedo *= 1.0 + grain * near * (0.3 + 0.3 * wGrass);
            albedo *= 1.0 - wGrass * near * (0.3 + 0.2 * smoothstep(0.45, 0.75, g1));   // the depth of the sward: the ground under the blades is in their shade
        }
    }
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
    // 3.6.1 under the grass near the camera the magnified bumps of the ground lit up in soft blotches: flattened there
    tn = mix(tn, vec3(0.0, 0.0, 1.0), wGrass * (1.0 - smoothstep(10.0, 38.0, length(vWorld - uCamPos.xyz))) * 0.85);
    N = normalize(T * tn.x + B * tn.y + N * max(tn.z, 0.2));
    // 2.0.0 wind ripples on sand: long crests across the wind, a finer set across them (dunes and beaches)
    {
        float sandK = wSand * (1.0 - cliff) * smoothstep(0.7, 0.96, N.y);
        if (sandK > 0.02) {
            vec2 wd = normalize(uWind.xy + vec2(1e-3));
            float ph = dot(vWorld.xz, wd) * 3.1 + texture(uNoiseTex, vWorld.xz * 0.06).g * 7.0;
            float dr = cos(ph) * (0.6 + 0.4 * sin(ph * 0.5));
            float ph2 = dot(vWorld.xz, vec2(-wd.y, wd.x)) * 9.5 + texture(uNoiseTex, vWorld.xz * 0.27).r * 9.0;
            N = normalize(N + (vec3(wd.x, 0.0, wd.y) * dr * 0.11 + vec3(-wd.y, 0.0, wd.x) * cos(ph2) * 0.035) * sandK);
        }
    }
  #endif
    // dynamic snow cover from weather
    float snowFx = uFogParams.z * smoothstep(0.55, 0.85, N.y);
    // keep drift detail under fresh snow (height map + large-scale noise), cold blue in hollows
    float sdet = clamp(0.62 + hsum * 0.45 + (texture(uNoiseTex, vWorld.xz * 0.04).r - 0.5) * 0.35, 0.55, 1.1);
    albedo = mix(albedo, mix(vec3(0.26, 0.32, 0.44), vec3(0.5, 0.55, 0.63), sdet - 0.45) * sdet, snowFx);
    rough = mix(rough, 0.5, snowFx);
    // puddles: wet ground darkens and becomes glossy in low areas (height map valleys)
    // 1.9.2: puddles spread as the rain goes on (wetness builds up slowly), with soft noisy edges
    float pn = texture(uNoiseTex, vWorld.xz * 0.09).g;
    float puddle = wet * smoothstep(0.42 + 0.22 * wet + (pn - 0.5) * 0.25, 0.12, hsum) * smoothstep(0.8, 0.95, N.y) * (1.0 - snowFx);
    // ruts and dips of roads, paths and mud hold water first: large flat puddles from low-frequency noise
    float soft = clamp(wDirt + wCob * 0.8 + wMud * 1.3 + wGrass * 0.25, 0.0, 1.0);
    float spot = texture(uNoiseTex, vWorld.xz * 0.035).b * 0.65 + texture(uNoiseTex, vWorld.xz * 0.11).g * 0.35;
    float spots = smoothstep(0.66 - 0.16 * wet * soft, 0.6 - 0.16 * wet * soft, 1.0 - spot) * soft * smoothstep(0.9, 0.97, N.y);
    puddle = max(puddle, wet * wet * spots * (1.0 - snowFx));
    albedo *= mix(1.0, 0.55, wet * 0.6 + puddle * 0.4);
    rough = mix(rough, 0.08, max(wet * 0.4, puddle));
    if (puddle > 0.3) N = normalize(mix(N, vec3(0.0, 1.0, 0.0), puddle));
  #ifndef TERRAIN_SIMPLE
    // raindrops: rings in the puddles, a finer shimmer of impacts on all wet ground
    if (wet > 0.05) {
        // 3.0.2 only near the camera: further away the little rings alias into a grid of lines
        float rfade = 1.0 - smoothstep(10.0, 26.0, length(vWorld - uCamPos.xyz));
        if (rfade > 0.0) {
            vec2 rr = rainRipples(vWorld.xz, uCamPos.w);
            N = normalize(N + vec3(rr.x, 0.0, rr.y) * (puddle * 0.9 + wet * 0.12) * rfade);
        }
    }
  #endif
    puddleK = puddle;
#else
    // Hand-painted micro-variation for props & characters
    float nz = texture(uNoiseTex, vWorld.xz * 0.35 + vWorld.y * 0.21).r;
    albedo *= 0.9 + nz * 0.2;
    // Snow accumulates on upward faces (Frostheim & snowfall)
    float snow = max(uTint.a, uFogParams.z) * smoothstep(0.45, 0.8, N.y);
    albedo = mix(albedo, vec3(0.5, 0.55, 0.64) * (0.85 + nz * 0.3), snow);
    rough = mix(rough, 0.55, snow);
  #ifdef WIND
    // 3.0.2 rain-soaked grass and leaves: deeper, richer colours - no grey plastic gloss
    albedo *= mix(1.0, 0.8, wet);
    albedo = max(mix(vec3(dot(albedo, vec3(0.3, 0.59, 0.11))), albedo, 1.0 + 0.15 * wet), 0.0);
  #else
    albedo *= mix(1.0, 0.7, wet * (1.0 - metal) * (0.4 + 0.6 * N.y));
    rough = mix(rough, rough * 0.4, wet * smoothstep(0.0, 0.6, N.y));
  #endif
#endif

#if !defined(SKINNED) && !defined(TERRAIN) && !defined(GRASS)
    // see-through: geometry between the camera and the player is dithered away (3.6.0 not in first person: the
    // eye is in the hero's head, and the hands and weapon are right in front of it)
    if (uPlayer.w > 0.0 && length(uCamPos.xyz - uPlayer.xyz - vec3(0.0, 1.6, 0.0)) > 0.9) {
        vec3 a = uPlayer.xyz + vec3(0.0, 1.0, 0.0), b = uCamPos.xyz;
        vec3 ab = b - a;
        float t = clamp(dot(vWorld - a, ab) / dot(ab, ab), 0.0, 1.0);
        float d = length(vWorld - (a + ab * t));
        float occ = (1.0 - smoothstep(2.2, 4.2, d)) * step(0.06, t) * step(uPlayer.y + 1.2, vWorld.y);
        // 3.0.2 an organic dissolve in world space (the old screen-door grid drew lines at low resolution)
        float dn = texture(uNoiseTex, vWorld.xz * 0.55 + vec2(vWorld.y * 0.37, -vWorld.y * 0.21)).a;
        if (occ * 1.15 > dn) discard;
    }
#endif
#ifdef DISSOLVE
    float dn = texture(uNoiseTex, vWorld.xz * 0.45 + vec2(vWorld.y * 0.31)).a;
    if (dn < uFx.y) discard;
    float edge = 1.0 - smoothstep(uFx.y, uFx.y + 0.09, dn);
#endif

#ifdef DBG_NOSHADOW
    float sh = 1.0;
#else
    float sh = shadowPCF(vShadow, dot(N, uSunDir.xyz)) * cloudShadow(vWorld);
#endif
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
    col += albedo * uSunColor.rgb * trans * 0.42 * sh;   // 3.0.2 softer (sunsets turned the meadows red-orange)
#endif
#ifdef TERRAIN
    col += vec3(1.0, 0.28, 0.05) * emis;
  #ifndef TERRAIN_SIMPLE
    // 1.12.0 glitter: tiny facets of snow, frost and wet sand catch the sun (bloom turns them into sparkles)
    {
        float gk = max(max(wSnow, snowFx), wSand * (0.55 + 0.45 * wet)) * (1.0 - puddleK);
        if (gk > 0.05) {
            vec2 gp = vWorld.xz * 7.0, gc = floor(gp);
            float rnd = hash12(gc);
            vec2 ctr = vec2(fract(rnd * 7.0), fract(rnd * 13.0)) * 0.5 + 0.25;   // a point-like crystal inside the cell
            float dotk = smoothstep(0.42, 0.1, length(fract(gp) - ctr));
            vec3 fn = normalize(vec3(rnd - 0.5, 0.7, fract(rnd * 17.0) - 0.5));
            float facet = pow(max(dot(fn, normalize(uSunDir.xyz + V)), 0.0), 80.0) * step(0.88, fract(rnd * 31.0)) * dotk;
            col += uSunColor.rgb * facet * gk * sh * uSunDir.w * 10.0;
        }
    }
  #endif
    if (puddleK > 0.01) {
        // puddles mirror the sky (and the sun glint), stronger at grazing angles
        float pf = 0.04 + 0.96 * pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 5.0);
        vec3 pr = envColor(reflect(-V, N), 0.03) * (0.35 + 0.65 * sh);
        col = mix(col, pr, clamp(puddleK * (0.25 + pf * 0.75), 0.0, 0.85));
    }
#else
    float nightOnly = vSkin.w > 0.5 ? uFogParams.w : 1.0;
    col += albedo * emis * (1.0 + uFx.w) * nightOnly;
#endif
#ifdef DISSOLVE
    col += uDissolveColor.rgb * edge * 6.0;
#endif
    col += albedo * uSunColor.rgb * uSunDir.w * caustics(vWorld, N) * (0.35 + 0.65 * sh) * 1.4;   // 3.5.0 light dancing under the water
    col = mix(col, vec3(1.5, 1.42, 1.35), uFx.x);  // hit flash (readable, not blinding)
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
uniform sampler2D uReflTex;     // 1.12.0 opaque scene (half res) for screen-space reflections
uniform highp sampler2D uSceneDepth;
uniform vec4 uSsr;              // x on, y near, z far
out vec4 fragColor;

float ssrLinZ(float z) { float n = uSsr.y, f = uSsr.z; z = z * 2.0 - 1.0; return 2.0 * n * f / (f + n - z * (f - n)); }
// March the reflected ray in world space, test it against the scene depth; rgb = reflected colour, a = confidence
vec4 traceSSR(vec3 P, vec3 R) {
    float stepLen = 0.6;
    vec3 q = P;
    for (int i = 0; i < 22; i++) {
        q += R * stepLen;
        stepLen *= 1.18;
        vec4 c = uViewProj * vec4(q, 1.0);
        if (c.w <= 0.0) break;
        vec3 ndc = c.xyz / c.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;
        if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) break;
        float sceneZ = ssrLinZ(texture(uSceneDepth, uv).r), rayZ = ssrLinZ(ndc.z * 0.5 + 0.5);
        if (rayZ > sceneZ + 0.05 && rayZ - sceneZ < max(1.2, stepLen * 2.0)) {
            // refine between the last two points
            vec3 a = q - R * stepLen / 1.18, b = q;
            for (int k = 0; k < 4; k++) {
                vec3 m = 0.5 * (a + b);
                vec4 cm = uViewProj * vec4(m, 1.0);
                vec3 nm = cm.xyz / cm.w;
                vec2 um = nm.xy * 0.5 + 0.5;
                if (ssrLinZ(nm.z * 0.5 + 0.5) > ssrLinZ(texture(uSceneDepth, um).r)) b = m; else a = m;
            }
            vec4 cb = uViewProj * vec4(b, 1.0);
            vec2 ub = cb.xy / cb.w * 0.5 + 0.5;
            vec2 edge = smoothstep(vec2(0.0), vec2(0.08), ub) * smoothstep(vec2(0.0), vec2(0.08), 1.0 - ub);
            float travelled = length(b - P);
            float conf = edge.x * edge.y * (1.0 - smoothstep(10.0, 40.0, travelled));   // near objects mirror clearly
            if (texture(uSceneDepth, ub).r > 0.9999) conf = 0.0;   // the sky is already in the environment colour
            return vec4(texture(uReflTex, ub).rgb / max(uSkyColor.w, 1e-3), conf);
        }
    }
    return vec4(0.0);
}

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
#ifdef WATER_SIMPLE
    vec3 dn = sampleN(uv1) * 1.4;                      // low profiles: one normal octave
#else
    vec3 dn = sampleN(uv1) + sampleN(uv2) * 0.7;
#endif
    // rain ripples: expanding rings in a cell grid
    float wet = uGroundColor.w;
    if (wet > 0.01) {
        vec2 cell = floor(vWorld.xz * 1.6);
        vec2 f = fract(vWorld.xz * 1.6) - 0.5;
        float h = hash12(cell);
        float ph = fract(t * 1.3 + h);
        float r = length(f - (vec2(h, fract(h * 7.3)) - 0.5) * 0.4);
        float ring = sin((r - ph * 0.5) * 40.0) * (1.0 - ph) * smoothstep(0.5 * ph + 0.05, 0.5 * ph, r);
        dn.xy += vec2(ring) * 0.5 * wet * (1.0 - smoothstep(14.0, 34.0, length(vWorld - uCamPos.xyz)));   // 3.0.2 near only (no grid far away)
    }
    vec3 N = normalize(vec3(dn.x, 1.0, dn.y));
    float frozen = uWaterShallow.a;
    N = normalize(mix(N, vec3(0.0, 1.0, 0.0), frozen * 0.8));
    float NdV = clamp(dot(N, V), 0.0, 1.0);
    float fres = 0.02 + 0.98 * pow(1.0 - NdV, 5.0);
    fres = fres * 0.85 + 0.07;
    vec3 R = reflect(-V, N);
    vec3 refl = envColor(R, 0.05);
    float ssrHit = 0.0;
#ifndef WATER_SIMPLE
    if (uSsr.x > 0.5 && frozen < 0.5 && uSsr.w < 2.5) {
        // trees, bridges, houses and characters mirrored in the water (rippled by the waves)
        vec3 Rr = normalize(reflect(-V, normalize(vec3(dn.x * 0.35, 1.0, dn.y * 0.35))));
        vec4 h = traceSSR(vWorld + vec3(0.0, 0.02, 0.0), Rr);
        refl = mix(refl, h.rgb, h.a);
        ssrHit = h.a;
    }
#endif
    float absorb = 1.0 - exp(-depth * 0.38);
    vec3 body = mix(uWaterShallow.rgb, uWaterDeep.rgb, absorb);
#ifdef WATER_SIMPLE
    float sh = cloudShadow(vWorld);                    // no shadow-map lookup on low profiles
#else
    float sh = shadowPCF(vShadow, 1.0) * cloudShadow(vWorld);
#endif
    sh = mix(1.0, sh, uSunColor.w);
    vec3 amb = mix(uGroundColor.rgb, uSkyColor.rgb, 0.8);
    body *= amb + uSunColor.rgb * uSunDir.w * 0.55 * sh;
    vec3 H = normalize(uSunDir.xyz + V);
    float spec = pow(max(dot(N, H), 0.0), 700.0) * 5.0 * sh * uSunDir.w * (1.0 - 0.6 * wet);
    // (stylised: mirrored objects read a bit stronger than physical Fresnel so they show from the high camera)
    vec3 col = mix(body, refl, clamp(fres + ssrHit * 0.15, 0.0, 1.0)) + uSunColor.rgb * spec;
    // shoreline foam
    float fn = texture(uNoiseTex, vWorld.xz * 0.22 + flow * t * 0.08 + t * 0.02).r;
    float foam = smoothstep(0.55, 0.0, depth) * smoothstep(0.35, 0.65, fn + 0.25 * sin(t * 2.0 + depth * 10.0));
    foam += smoothstep(0.6, 1.0, length(flow) * fn) * 0.5;  // rapids
    // current streaks: noise stretched along the flow direction, drifting downstream
    float fl = length(flow);
    if (fl > 0.02) {
        vec2 fd = flow / fl;
        vec2 suv = vec2(dot(vWorld.xz, fd) * 0.06 - t * fl * 0.12, dot(vWorld.xz, vec2(-fd.y, fd.x)) * 0.5);
        float streak = smoothstep(0.66, 0.86, texture(uNoiseTex, suv).r);
        foam += streak * 0.18 * clamp(fl * 3.0, 0.0, 1.0);
    }
    // 2.0.0 swash: bands of foam running up the beach with the waves (shallow open water only)
    float swash = smoothstep(0.8, 0.0, depth) * smoothstep(0.82, 0.96, sin(depth * 8.0 - t * 1.1 + fn * 2.0) * 0.5 + 0.5) * 0.45 * (1.0 - clamp(length(flow) * 4.0, 0.0, 1.0));
    foam += swash;
    col = mix(col, vec3(0.95) * (amb + uSunColor.rgb * uSunDir.w * 0.6), clamp(foam, 0.0, 1.0) * (1.0 - frozen));
    // 2.0.0 lagoon glow: sunlit shallow water scatters its own colour (turquoise over white sand)
    col += uWaterShallow.rgb * uSunColor.rgb * uSunDir.w * sh * (1.0 - absorb) * smoothstep(0.0, 0.5, depth) * (0.18 + 0.32 * pow(max(dot(-V, uSunDir.xyz), 0.0), 2.0)) * (1.0 - frozen);
#ifndef WATER_SIMPLE
    // 1.11.2 caustics: the waves focus sunlight into a moving net of bright lines on the bed of shallow water
    {
        vec2 cp = vWorld.xz * 0.32 + N.xz * 0.35 + flow * t * 0.12;
        float c1 = texture(uNoiseTex, cp + vec2(t * 0.031, t * 0.019)).g;
        float c2 = texture(uNoiseTex, cp * 1.37 - vec2(t * 0.024, -t * 0.033)).g;
        float ca = pow(1.0 - abs(c1 - c2), 14.0);
        float vis = (1.0 - absorb) * smoothstep(0.04, 0.4, depth) * sh * uSunDir.w * (1.0 - frozen) * (1.0 - fres);
        col += uSunColor.rgb * ca * vis * 0.75;
    }
#endif
    // ice
    vec3 ice = vec3(0.75, 0.88, 0.97) * (amb + uSunColor.rgb * uSunDir.w * sh * 0.7) + uSunColor.rgb * spec * 0.4;
    float crack = smoothstep(0.02, 0.0, abs(texture(uNoiseTex, vWorld.xz * 0.05).r - 0.5));
    ice *= 1.0 - crack * 0.3;
    col = mix(col, ice, frozen);
    // point lights reflect on water
#ifndef WATER_SIMPLE
    for (int i = 0; i < 8; i++) {
        if (float(i) >= uMisc.x) break;
        vec3 lv = uLightPos[i].xyz - vWorld;
        float d = length(lv);
        float att = clamp(1.0 - d / uLightPos[i].w, 0.0, 1.0);
        col += uLightCol[i].rgb * att * att * (0.15 + pow(max(dot(N, normalize(normalize(lv) + V)), 0.0), 60.0) * 2.0);
    }
#endif
    col += vec3(uMisc.w) * 0.6;
    float alpha = mix(clamp(0.45 + absorb * 0.6 + fres * 0.3 + foam, 0.0, 1.0), 0.97, frozen);
    alpha *= smoothstep(0.0, 0.08, depth);
    col = applyFog(col, vWorld);
    fragColor = vec4(col * uSkyColor.w, alpha);
    if (uSsr.w > 0.5 && uSsr.w < 2.5) fragColor = uSsr.w > 1.5 ? vec4(refl, 1.0) : vec4(vec3(ssrHit), 1.0);   // LK_SSR_DEBUG=1 confidence, 2 reflection
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
uniform vec4 uSkyFx;       // 2.0.0 x aurora, y dust haze (desert, ash), z rainbow
out vec4 fragColor;

float hash13(vec3 p) { p = fract(p * 0.1031); p += dot(p, p.zyx + 31.32); return fract((p.x + p.y) * p.z); }

// layered cloud density: three octaves from the tileable noise texture
float cloudDens(vec2 p, float cover) {
    float c = texture(uNoiseTex, p).b * 0.55 + texture(uNoiseTex, p * 2.31 + 0.37).g * 0.30 + texture(uNoiseTex, p * 5.07 + 0.11).r * 0.15;
    return smoothstep(1.0 - cover, 1.18 - cover * 0.55, c);
}


// ---- 3.2.0 outer space: nebulae, three layers of stars, the galactic band, the planet seen from orbit (oceans,
// continents, clouds, a blue atmosphere on its rim, city lights on the night side), a ringed gas giant, the sun's corona
float vnoise(vec2 p) { return texture(uNoiseTex, p).r; }
float fbmN(vec2 p) { float a = 0.0, w = 0.5; for (int i = 0; i < 5; i++) { a += w * texture(uNoiseTex, p).g; p = p * 2.03 + vec2(0.37, 0.11); w *= 0.5; } return a; }
vec2 sphereUV(vec3 n) { return vec2(atan(n.z, n.x) / 6.2831853 + 0.5, asin(clamp(n.y, -1.0, 1.0)) / 3.14159265 + 0.5); }
vec3 spaceSky(vec3 d, vec3 S, float t) {
    // deep background with a faint violet gradient
    vec3 col = mix(vec3(0.004, 0.003, 0.012), vec3(0.012, 0.008, 0.03), d.y * 0.5 + 0.5);
    // nebulae: two colourful clouds with dark dust lanes
    vec2 np = vec2(atan(d.z, d.x) * 0.9, d.y * 1.6);
    float n1 = fbmN(np * 0.35 + vec2(0.13, 0.71)), n2 = fbmN(np * 0.55 + vec2(4.2, 1.3) + n1 * 0.3);
    float lane = smoothstep(0.45, 0.7, fbmN(np * 1.3 + 9.0));
    vec3 neb = vec3(0.55, 0.12, 0.65) * smoothstep(0.42, 0.85, n1) + vec3(0.05, 0.45, 0.6) * smoothstep(0.5, 0.9, n2) + vec3(0.9, 0.35, 0.2) * smoothstep(0.62, 0.95, n1 * n2 * 1.6);
    col += neb * 0.32 * (1.0 - 0.75 * lane);
    // the galactic band
    vec3 gn = normalize(vec3(0.2, 0.9, 0.38));
    float gb = dot(d, gn);
    vec2 gp = vec2(atan(d.z, d.x) * 1.3, d.y * 3.0);
    float mw = exp(-gb * gb * 9.0) * (0.4 + 0.6 * vnoise(gp * 1.7)) * (1.0 - 0.6 * smoothstep(0.4, 0.65, texture(uNoiseTex, gp * 0.55 + 0.2).g));
    col += mix(vec3(0.45, 0.5, 0.95), vec3(1.0, 0.78, 0.62), smoothstep(0.5, 0.9, vnoise(gp * 0.4))) * mw * 0.55;
    // stars: three layers, coloured, some twinkling
    for (int l = 0; l < 3; l++) {
        float sc = 140.0 + float(l) * 170.0;
        vec3 cell = floor(d * sc);
        float hs = hash13(cell + float(l) * 17.0);
        vec3 f = fract(d * sc) - 0.5;
        float th = l == 0 ? 0.985 : (l == 1 ? 0.992 : 0.996);
        float st = step(th, hs) * smoothstep(0.45, 0.0, length(f)) * (0.6 + 0.4 * sin(t * (1.5 + hs * 5.0) + hs * 80.0));
        vec3 c = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.78, 0.55), fract(hs * 73.1));
        if (fract(hs * 13.7) > 0.93) c = vec3(1.0, 0.45, 0.4);
        col += c * st * (l == 0 ? 3.5 : 1.6) * (1.0 + mw * 2.0);
    }
    // the sun: disc, corona streamers, wide glow
    float sd = max(dot(d, S), 0.0);
    float ang = atan(dot(d, normalize(cross(S, vec3(0.0, 1.0, 0.0)))), dot(d, normalize(cross(S, cross(S, vec3(0.0, 1.0, 0.0))))));
    float streamers = 0.6 + 0.4 * sin(ang * 9.0 + t * 0.2) * sin(ang * 5.0 - t * 0.13);
    col += vec3(1.0, 0.92, 0.8) * (pow(sd, 2400.0) * 60.0 + pow(sd, 260.0) * 3.0 * streamers + pow(sd, 18.0) * 0.25);
    // a ringed gas giant far away
    vec3 gc = normalize(vec3(-0.75, 0.32, -0.55));
    float gr = 0.16, gd = dot(d, gc);
    vec3 rn = normalize(vec3(0.25, 1.0, 0.15));
    {   // rings (behind and in front of the giant)
        float dn = dot(d, rn), dg = dot(gc, rn);
        if (abs(dn) > 1e-3) {
            vec3 hit = d * (dg / dn) - gc;
            float rr = length(hit) / gr;
            if (dg / dn > 0.0 && rr > 1.35 && rr < 2.35) {
                float band = 0.55 + 0.45 * sin(rr * 38.0) * sin(rr * 11.0);
                vec3 rc = mix(vec3(0.85, 0.75, 0.6), vec3(0.6, 0.52, 0.45), band) * smoothstep(1.35, 1.45, rr) * smoothstep(2.35, 2.2, rr);
                bool behind = gd > sqrt(max(0.0, 1.0 - gr * gr)) && dot(d * (dg / dn), gc) > 1.0;
                if (!behind) col = mix(col, rc * (0.25 + 0.75 * max(dot(rn, S), 0.15)), 0.75 * smoothstep(1.35, 1.5, rr));
            }
        }
    }
    float cosR = sqrt(1.0 - gr * gr);
    if (gd > cosR) {
        vec3 p = d * gd - gc; vec3 nrm = normalize(p + gc * (1.0 - cosR) * 0.0 + (d * gd - gc));
        nrm = normalize(d * (gd - sqrt(max(0.0, gd * gd - cosR * cosR))) - gc);
        float lat = dot(nrm, rn);
        float bands = sin(lat * 26.0 + fbmN(vec2(lat * 3.0, atan(nrm.z, nrm.x)) * 0.6) * 4.0);
        vec3 gcol = mix(vec3(0.85, 0.62, 0.42), vec3(0.95, 0.85, 0.7), bands * 0.5 + 0.5);
        float lit = max(dot(nrm, S), 0.0);
        col = gcol * (0.04 + 0.96 * lit) + vec3(0.9, 0.6, 0.4) * pow(1.0 - max(dot(nrm, -d), 0.0), 3.0) * 0.3 * lit;
    }
    // the planet below: the hero stands far above it
    vec3 pc = normalize(vec3(0.42, -0.55, -0.62));   // low over one side of the horizon
    float pr = 0.6, pd = dot(d, pc), cosP = sqrt(1.0 - pr * pr);
    float rim = smoothstep(cosP - 0.06, cosP, pd);
    if (pd > cosP - 0.06) {
        vec3 atm = vec3(0.3, 0.6, 1.0) * (1.0 - smoothstep(cosP, cosP - 0.06, pd)) * (0.25 + 0.75 * max(dot(normalize(d - pc * pd), S) * 0.5 + 0.5, 0.0));
        col += atm * 0.9 * (1.0 - step(cosP, pd));
    }
    if (pd > cosP) {
        float dist = pd - sqrt(max(0.0, pd * pd - cosP * cosP));
        vec3 nrm = normalize(d * dist - pc);
        // the planet's own frame: its axis is tilted away from the viewer, so the side facing the asteroids shows
        // oceans and continents (not a pole); it turns slowly
        vec3 ax = normalize(vec3(0.9, 0.15, -0.4)), e1 = normalize(cross(ax, vec3(0.0, 1.0, 0.0))), e2 = cross(ax, e1);
        float rotA = t * 0.004;
        vec3 ln = vec3(dot(nrm, e1) * cos(rotA) - dot(nrm, e2) * sin(rotA), dot(nrm, ax), dot(nrm, e1) * sin(rotA) + dot(nrm, e2) * cos(rotA));
        vec2 uv = sphereUV(ln);
        float land = fbmN(uv * vec2(4.0, 2.0) + vec2(0.31, 0.77)) + 0.25 * fbmN(uv * vec2(11.0, 5.5) + 3.1) - 0.12;
        float clouds = smoothstep(0.58, 0.82, fbmN(uv * vec2(6.0, 3.0) + vec2(t * 0.0015, 0.4))) * 0.6;
        vec3 ocean = mix(vec3(0.01, 0.05, 0.16), vec3(0.03, 0.16, 0.3), smoothstep(0.3, 0.5, land));
        vec3 ground = mix(vec3(0.12, 0.26, 0.08), vec3(0.42, 0.34, 0.2), smoothstep(0.62, 0.78, land));
        vec3 surf = mix(ocean, ground, smoothstep(0.5, 0.53, land));
        surf = mix(surf, vec3(0.85, 0.9, 0.95), smoothstep(0.86, 0.95, abs(ln.y)));   // polar caps, on its own axis
        float lit = dot(nrm, S);
        vec3 day = mix(surf, vec3(0.9), clouds) * (0.015 + 0.85 * max(lit, 0.0));
        float spec = pow(max(dot(reflect(-S, nrm), -d), 0.0), 60.0) * (1.0 - smoothstep(0.5, 0.53, land)) * 0.5 * step(0.0, lit) * (1.0 - clouds);
        vec3 nightLights = vec3(1.0, 0.72, 0.38) * smoothstep(0.53, 0.56, land) * step(0.65, vnoise(uv * 48.0)) * (1.0 - clouds) * smoothstep(0.05, -0.15, lit) * 0.7;
        float fres = pow(1.0 - max(dot(nrm, -d), 0.0), 3.0);
        col = day + vec3(1.0) * spec + nightLights + vec3(0.3, 0.55, 1.0) * fres * (0.15 + 0.6 * max(lit + 0.25, 0.0));
    }
    return col;
}

// ---- 3.9.4 the Rift: a huge tear across the sky, a breach into another universe. A jagged crack (zig-zag
// segments at three scales) whose lips burn white-hot with a cyan / magenta chromatic fringe; through it a
// swirling nebula, its stars and a strange planet; glowing veins split the sky around it, sparks drift off it,
// and a violet halo darkens the sky nearby. It breathes slowly. Always there, by day and by night.
float riftZig(float x, float n, float s) { float i = floor(x * n), f = fract(x * n); return mix(hash13(vec3(i, s, 1.7)), hash13(vec3(i + 1.0, s, 1.7)), f) - 0.5; }
vec3 riftSky(vec3 d, vec3 col, float t, float night) {
    const vec3 R = vec3(0.596, 0.298, -0.745);   // low over the horizon, so the gameplay camera sees it
    float c = dot(d, R);
    if (c < 0.42) return col;
    vec3 e1 = normalize(cross(vec3(0.0, 1.0, 0.0), R)), e2 = cross(R, e1);
    vec2 p = vec2(dot(d, e1), dot(d, e2)) / c;
    const float ca = 0.9394, sa = 0.3429;   // the crack runs at a slant
    p = vec2(ca * p.x - sa * p.y, sa * p.x + ca * p.y);
    const float L = 0.6;
    float x = p.x, ax = abs(x) / L;
    float env = pow(max(1.0 - ax * ax, 0.0), 0.65);
    float cy = 0.1 * riftZig(x, 3.0, 1.0) + 0.05 * riftZig(x, 8.0, 2.0) + 0.016 * riftZig(x, 23.0, 3.0) + 0.005 * riftZig(x, 61.0, 5.0);
    float w = (0.1 * (0.6 + 0.7 * (riftZig(x, 6.0, 4.0) + 0.5)) + 0.004) * env * (0.93 + 0.07 * sin(t * 0.9 + x * 4.0));
    float pulse = 0.86 + 0.1 * sin(t * 1.3) + 0.04 * sin(t * 7.1 + x * 9.0);
    float dy = p.y - cy;
    float e = abs(dy) - w;          // < 0 inside the breach
    float eo = max(e, 0.0);
    float endFade = smoothstep(1.08, 0.75, ax);
    // halo: the sky around the wound darkens and turns violet
    float halo = exp(-eo * 7.0) * endFade;
    vec3 haloC = mix(vec3(0.55, 0.12, 1.0), vec3(0.1, 0.65, 1.0), 0.5 + 0.5 * sin(x * 5.0 + t * 0.5));
    col = mix(col, col * 0.45 + haloC * 0.1, halo * (0.5 + 0.3 * (1.0 - night)));
    col += haloC * halo * (0.16 + 0.12 * night) * pulse;
    // veins: ridged noise lines cracking the sky near the breach
    vec2 vp = p * 2.1 + vec2(0.13, 0.41);
    float v1 = abs(texture(uNoiseTex, vp).g - 0.5), v2 = abs(texture(uNoiseTex, vp * 2.7 + 0.3).r - 0.5);
    float near = exp(-eo * 28.0) * smoothstep(1.0, 0.55, ax);
    float veins = (smoothstep(0.01, 0.0, v1) + 0.5 * smoothstep(0.006, 0.0, v2)) * near;
    col += mix(vec3(0.95, 0.4, 1.0), vec3(0.45, 0.95, 1.0), smoothstep(0.0, 0.016, v1)) * veins * 1.1 * pulse;
    // inside: the other universe
    if (e < 0.004) {
        vec2 q = vec2(x, dy) * 1.7;
        float rr = length(q);
        float ang = t * 0.04 + 0.35 / (rr + 0.25);
        q = vec2(cos(ang) * q.x - sin(ang) * q.y, sin(ang) * q.x + cos(ang) * q.y);
        float n1 = fbmN(q * 0.9 + vec2(3.1, 1.7));
        float n2 = fbmN(q * 1.9 + n1 * 0.7 + vec2(t * 0.004, 5.3));
        vec3 cos3 = vec3(0.01, 0.002, 0.03)
                  + vec3(0.75, 0.1, 0.85) * smoothstep(0.45, 0.9, n1)
                  + vec3(0.05, 0.6, 0.95) * smoothstep(0.45, 0.9, n2) * 0.85
                  + vec3(1.0, 0.55, 0.2) * smoothstep(0.55, 0.95, n1 * n2 * 1.8) * 1.2;
        vec2 sg = p * 150.0; vec2 sc = floor(sg);
        float hs = hash13(vec3(sc, 9.3));
        cos3 += mix(vec3(0.7, 0.85, 1.0), vec3(1.0, 0.75, 0.9), fract(hs * 37.0)) * step(0.955, hs) * smoothstep(0.4, 0.0, length(fract(sg) - 0.5)) * (0.6 + 0.4 * sin(t * 3.0 + hs * 50.0)) * 3.0;
        // a strange planet beyond, lit from inside the breach
        vec2 pc = vec2(0.1, cy + 0.01);
        float pr = 0.07, pdist = length(p - pc);
        if (pdist < pr) {
            vec3 nn = vec3((p - pc) / pr, 0.0); nn.z = sqrt(max(1.0 - dot(nn.xy, nn.xy), 0.0));
            float lit = max(dot(nn, normalize(vec3(-0.6, 0.5, 0.6))), 0.0);
            float bands = 0.5 + 0.5 * sin(nn.y * 18.0 + fbmN(nn.xy * 0.6 + 2.0) * 5.0);
            vec3 pcol = mix(vec3(0.1, 0.75, 0.6), vec3(0.85, 0.95, 0.5), bands) * (0.06 + 1.1 * lit);
            cos3 = mix(cos3, pcol * 1.6 + vec3(0.4, 0.9, 1.0) * pow(1.0 - nn.z, 3.0) * 0.8, smoothstep(pr, pr - 0.003, pdist));
        } else cos3 += vec3(0.3, 0.8, 1.0) * exp(-(pdist - pr) * 60.0) * 0.5;
        col = mix(col, cos3 * 0.6, smoothstep(0.004, -0.004, e));
    }
    // the lips: white-hot core, a cyan inner and a magenta outer fringe, a wide glow
    float lip = (env * endFade + 0.15) * (1.0 + 0.8 * (1.0 - night));   // brighter by day, against a bright sky
    col += vec3(0.88, 0.96, 1.0) * exp(-abs(e) * 320.0) * 1.6 * pulse * lip;
    col += vec3(0.25, 0.9, 1.0) * exp(-abs(e + 0.004) * 260.0) * 0.8 * lip;
    col += vec3(1.0, 0.2, 0.85) * exp(-abs(e - 0.006) * 200.0) * 0.9 * lip;
    col += mix(vec3(0.7, 0.25, 1.0), vec3(0.3, 0.8, 1.0), 0.5 + 0.5 * sin(x * 7.0 - t)) * exp(-eo * 40.0) * step(0.0, e) * 0.4 * pulse * endFade;
    // sparks drifting away from the breach
    vec2 kg = vec2(x * 60.0, (p.y - cy) * 60.0 - sign(dy) * t * 0.6);
    vec2 kc = floor(kg);
    float kh = hash13(vec3(kc, 4.1));
    col += vec3(0.8, 0.9, 1.0) * step(0.975, kh) * smoothstep(0.3, 0.0, length(fract(kg) - 0.5)) * exp(-eo * 9.0) * endFade * (0.5 + 0.5 * sin(t * 4.0 + kh * 40.0)) * 2.0;
    return col;
}

void main() {
    vec3 d = normalize(vDir);
    if (uSkyFx.w > 0.5) { fragColor = vec4(spaceSky(d, normalize(uSunDir.xyz), uCamPos.w) * uSkyColor.w, 1.0); return; }   // 3.2.0 outer space
    float h = clamp(d.y, -0.2, 1.0);
    float night = uFogParams.w;
    float sun = uSunDir.w;                         // sunlight intensity (also dims at night)
    vec3 S = normalize(uSunDir.xyz);
    float sd = max(dot(d, S), 0.0);
    float low = 1.0 - smoothstep(0.02, 0.5, S.y);  // 1 at sunrise/sunset
    // ---- atmosphere: zenith to horizon, warm scattering around a low sun
    vec3 col = mix(uSkyHorizon.rgb, uSkyTop.rgb, pow(max(h, 0.0), 0.55));
    col += uSunColor.rgb * pow(sd, 4.0) * 0.16 * (0.3 + low) * (1.0 - night);                       // aureole
    col += vec3(1.0, 0.42, 0.14) * pow(sd, 2.2) * low * smoothstep(0.55, 0.0, h) * 0.38 * (1.0 - night);   // sunset band hugging the horizon
    col = mix(col, uFogColor.rgb, smoothstep(0.15, -0.1, h));
    // desert / ash dust: a warm brown haze thickening towards the horizon
    float dust = uSkyFx.y;
    if (dust > 0.001) col = mix(col, mix(vec3(0.85, 0.58, 0.32), uFogColor.rgb, 0.4) * (0.35 + 0.65 * (1.0 - night)), dust * smoothstep(0.42, -0.05, h) * 0.62 * (1.0 - night * 0.55));
    // ---- sun disc with limb glow, moon with maria
    col += uSunColor.rgb * (pow(sd, 1100.0) * 34.0 + pow(sd, 12.0) * 0.35 + pow(sd, 90.0) * 0.5) * sun;
    vec3 M = -S;
    float md = dot(d, M);
    if (night > 0.02 && md > 0.9) {
        float disc = smoothstep(0.99925, 0.99945, md);
        vec2 mp = (d.xz - M.xz) * 26.0;
        float maria = texture(uNoiseTex, mp * 0.5 + 0.3).r;
        vec3 mooncol = mix(vec3(0.62, 0.66, 0.78), vec3(0.95, 0.96, 1.0), smoothstep(0.35, 0.7, maria));
        col += mooncol * disc * 5.5 * night;
        col += vec3(0.55, 0.65, 1.0) * (pow(max(md, 0.0), 80.0) * 0.16 + smoothstep(0.986, 0.9856, md) * smoothstep(0.9847, 0.9856, md) * 0.04) * night;   // halo and a faint 22 degree ring
    }
    // ---- stars (hashed cells, twinkling), the Milky Way band with dust lanes, a shooting star now and then
    if (night > 0.02 && h > -0.02) {
        float horizonFade = smoothstep(0.0, 0.3, h);
        vec3 cell = floor(d * 180.0);
        float hs = hash13(cell);
        vec3 f = fract(d * 180.0) - 0.5;
        float star = step(0.9935, hs) * smoothstep(0.42, 0.0, length(f)) * (0.55 + 0.45 * sin(uCamPos.w * (2.0 + hs * 4.0) + hs * 60.0));
        vec3 sc = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.85, 0.65), fract(hs * 91.7));
        col += sc * star * 3.2 * night * horizonFade;
        vec3 gn = normalize(vec3(0.35, 0.78, 0.52));
        float gb = dot(d, gn);
        vec2 gp = vec2(atan(d.z, d.x) * 1.3, d.y * 3.0);
        float lane = smoothstep(0.38, 0.62, texture(uNoiseTex, gp * 0.55 + 0.2).g);
        float mw = exp(-gb * gb * 14.0) * (0.35 + 0.65 * texture(uNoiseTex, gp * 1.7).r) * (1.0 - 0.55 * lane);
        col += mix(vec3(0.32, 0.38, 0.7), vec3(0.8, 0.62, 0.55), smoothstep(0.55, 0.9, texture(uNoiseTex, gp * 0.4).b)) * mw * 0.32 * night * horizonFade;
        float ts = floor(uCamPos.w / 7.0), tf = fract(uCamPos.w / 7.0);
        if (hash13(vec3(ts, 3.1, 1.7)) > 0.45 && tf < 0.12) {
            vec3 a0 = normalize(vec3(hash13(vec3(ts, 1.0, 0.0)) - 0.5, 0.45 + 0.35 * hash13(vec3(ts, 2.0, 0.0)), hash13(vec3(ts, 5.0, 0.0)) - 0.5));
            vec3 dirm = normalize(vec3(hash13(vec3(ts, 7.0, 0.0)) - 0.5, -0.35, hash13(vec3(ts, 8.0, 0.0)) - 0.5));
            vec3 head = normalize(a0 + dirm * tf * 2.2);
            vec3 tail = normalize(a0 + dirm * max(tf - 0.05, 0.0) * 2.2);
            vec3 ax = head - tail; float len = length(ax) + 1e-5;
            float tt = clamp(dot(d - tail, ax) / (len * len), 0.0, 1.0);
            float dist = length(d - (tail + ax * tt));
            col += vec3(1.0, 0.95, 0.85) * smoothstep(0.004, 0.0, dist) * tt * (1.0 - tf / 0.12) * 4.0 * night;
        }
    }
    // ---- aurora: shimmering curtains of green, cyan and violet (cold regions at night)
    float au = uSkyFx.x;
    if (au > 0.01 && d.y > 0.04) {
        vec3 acc = vec3(0.0);
        for (int i = 0; i < 6; i++) {
            float fi = float(i);
            float t = (0.55 + 0.11 * fi) / (d.y + 0.12);
            vec2 p = d.xz * t * 0.5 + vec2(uCamPos.w * 0.012, fi * 0.13);
            float n = texture(uNoiseTex, p * 0.31).g + 0.28 * sin(p.x * 6.0 + uCamPos.w * 0.35 + fi);
            float cur = smoothstep(0.52, 0.74, n) * smoothstep(0.98, 0.7, n + 0.2 * texture(uNoiseTex, p * 1.9).r);
            vec3 ac = mix(vec3(0.1, 1.0, 0.45), vec3(0.15, 0.75, 1.0), fi / 5.0);
            ac = mix(ac, vec3(0.75, 0.25, 1.0), smoothstep(0.55, 1.0, fi / 5.0 + 0.25 * sin(uCamPos.w * 0.2 + p.x)));
            acc += ac * cur * (1.0 - fi * 0.12);
        }
        col += acc * 0.34 * au * smoothstep(0.04, 0.4, d.y) * night;
    }
    // ---- 3.6.0 volumetric cumulus: the view ray crosses a slab of cloud (flat bases, rounded towers that thin out
    // towards the top) in 11 jittered steps; each step looks once towards the sun (Beer's law + "powder" darkening
    // of the thick cores), so the clouds get bright sunlit tops, dark grey bellies, silver edges against the sun
    // and pink-orange undersides at sunset. They drift with the wind and fade into the haze at the horizon.
    if (d.y > 0.0) {
        float cover = uSkyHorizon.a;
        vec2 wdir = uWind.xy * uCamPos.w * 0.0022;
        float fade = smoothstep(0.0, 0.05, d.y);
        if (fade > 0.0 && cover > 0.02) {
            const float H0 = 1.0, H1 = 1.55;
            float t0 = H0 / d.y, t1 = min(H1 / d.y, t0 + 3.0);
            const int N = 11;
            float dt = (t1 - t0) / float(N);
            float jit = hash12(gl_FragCoord.xy);
            float T = 1.0;
            vec3 acc = vec3(0.0);
            float hg = 0.25 + 0.75 * pow(sd, 6.0) + 0.6 * pow(sd, 40.0);   // forward scattering: bright towards the sun
            vec3 amb = mix(uSkyHorizon.rgb, uSkyTop.rgb, 0.4) * 0.9 + vec3(0.03);
            for (int i = 0; i < N; i++) {
                float t = t0 + (float(i) + jit) * dt;
                vec3 p = d * t;
                float h = clamp((p.y - H0) / (H1 - H0), 0.0, 1.0);
                vec2 q = p.xz * 0.11 + wdir;
                float base = clamp((texture(uNoiseTex, q).b * 0.65 + texture(uNoiseTex, q * 2.3 + 0.37).g * 0.35 - 0.5) * 2.8 + 0.5, 0.0, 1.0);   // the noise is narrow: stretched
                float det = texture(uNoiseTex, q * 6.1 + vec2(0.11, uCamPos.w * 0.002)).r;
                float prof = smoothstep(0.0, 0.12, h) * (1.0 - smoothstep(0.25 + 0.6 * base, 1.0, h));   // flat base, rounded top
                float thr = 0.85 - cover * 0.75;   // fair-weather cumulus on a clear day, a grey lid when overcast
                float dens = clamp((base - thr) * 2.6 - (det - 0.5) * 0.5 * (0.5 + h), 0.0, 1.0) * prof;
                if (dens < 0.01) continue;
                // light march: one sample towards the sun
                vec2 ql = q + S.xz * 0.05;
                float hl = clamp(h + S.y * 0.45, 0.0, 1.0);
                float bl = clamp((texture(uNoiseTex, ql).b * 0.65 + texture(uNoiseTex, ql * 2.3 + 0.37).g * 0.35 - 0.5) * 2.8 + 0.5, 0.0, 1.0);
                float dl = clamp((bl - thr) * 2.6, 0.0, 1.0) * (1.0 - smoothstep(0.25 + 0.6 * bl, 1.0, hl));
                float beer = exp(-dl * 2.6 - dens * 0.6);
                float powder = 1.0 - exp(-dens * 5.0);
                vec3 sunL = uSunColor.rgb * min(sun, 1.6) * beer * mix(1.0, powder, 0.5) * hg * 1.6;
                vec3 sunset = vec3(1.0, 0.45, 0.22) * low * (1.0 - h) * 0.5 * (1.0 - night);
                vec3 Lc = sunL * 1.15 + amb * (0.28 + 0.62 * h) + sunset * beer;
                float a = 1.0 - exp(-dens * dt * 5.0);
                acc += T * a * Lc;
                T *= 1.0 - a;
                if (T < 0.04) break;
            }
            float haze = exp(-t0 * 0.045);   // far clouds melt into the horizon haze
            vec3 cl = col * T + acc * (1.0 - night * 0.75);
            cl = mix(mix(col, uSkyHorizon.rgb * (1.0 - night * 0.7), 1.0 - T), cl, haze);
            float k = fade * (1.0 - night * 0.6);
            col = mix(col, cl, k);
        }
        vec2 cir = d.xz / (d.y + 0.18) * vec2(0.09, 0.55) + wdir * 0.4 + vec2(0.0, 7.0);
        float ci = smoothstep(0.55, 0.85, texture(uNoiseTex, cir).r * 0.7 + texture(uNoiseTex, cir * 3.1).g * 0.3);
        vec3 cic = mix(uSkyHorizon.rgb * 1.15, uSunColor.rgb * min(sun, 1.3), 0.35 + 0.4 * low);
        col = mix(col, cic, ci * 0.28 * (1.0 - cover * 0.5) * smoothstep(0.05, 0.4, d.y) * (1.0 - night * 0.8));
    }
    // ---- rainbow after a shower, opposite the sun
    float rb = uSkyFx.z;
    if (rb > 0.01 && S.y > 0.02 && S.y < 0.7 && d.y > -0.02) {
        float ang = acos(clamp(dot(d, -S), -1.0, 1.0));
        float hue = (ang - 0.693) / 0.048;
        if (hue > 0.0 && hue < 1.0) {
            vec3 sp = clamp(vec3(abs(hue * 6.0 - 3.0) - 1.0, 2.0 - abs(hue * 6.0 - 2.0), 2.0 - abs(hue * 6.0 - 4.0)), 0.0, 1.0);
            col += sp.zyx * smoothstep(0.0, 0.18, hue) * smoothstep(1.0, 0.82, hue) * rb * 0.34 * smoothstep(-0.02, 0.2, d.y);
        }
    }
    col = riftSky(d, col, uCamPos.w, night);   // 3.9.4
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
// 3.0.1 shader effects (powers, spells, impacts): procedural meshes (disc, cylinder, sphere, arc, cone)
// shaded with animated noise, fresnel and procedural runes; additive HDR (they feed the bloom).
// uShape: 0 disc (xz, r <= 1), 1 cylinder (y 0..1, r 1), 2 sphere (r 1), 3 arc (xz, r 0.7..1, angle 0..pi), 4 cone
// uP: x age 0..1, y intensity, z seed, w seconds since the effect started
const char* FX_VS = R"(
in vec3 aPos;
uniform mat4 uModel;
uniform int uShape;
out vec3 vLocal;
out vec3 vWorld;
out vec3 vNrm;
void main() {
    vLocal = aPos;
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    vec3 nl = aPos;
    if (uShape == 1 || uShape == 4) nl.y = 0.0;
    if (uShape == 0 || uShape == 3) nl = vec3(0.0, 1.0, 0.0);
    vNrm = normalize(mat3(uModel) * nl + vec3(1e-5));
    gl_Position = uViewProj * w;
}
)";

const char* FX_FS = R"(
in vec3 vLocal;
in vec3 vWorld;
in vec3 vNrm;
uniform sampler2D uNoiseTex;
uniform vec4 uCol;    // core colour, a: opacity
uniform vec4 uCol2;   // edge colour
uniform vec4 uP;      // age 0..1, intensity, seed, time (s)
uniform int uShape;
uniform int uStyle;
out vec4 fragColor;
float nz(vec2 uv) { return texture(uNoiseTex, uv).r; }
float fbmN(vec2 uv) { return nz(uv) * 0.6 + nz(uv * 2.3 + 0.37) * 0.3 + nz(uv * 5.1 + 0.71) * 0.1; }
float line(vec2 p, vec2 a, vec2 b, float w) { vec2 pa = p - a, ba = b - a; float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0); return exp(-pow(length(pa - ba * h) / w, 2.0)); }
void main() {
    float age = uP.x, t = uP.w, seed = uP.z;
    vec3 V = normalize(uCamPos.xyz - vWorld);
    float a = 0.0;
    vec3 col = uCol.rgb;
    if (uShape == 0) {
        float r = length(vLocal.xz), ang = atan(vLocal.z, vLocal.x);
        if (uStyle == 0) {            // shockwave ring
            float R = mix(0.05, 1.0, 1.0 - pow(1.0 - age, 2.5));
            float n = fbmN(vec2(ang * 0.8 + seed, r * 2.0 - t * 1.5));
            float w = 0.05 + 0.09 * age;
            float ring = exp(-pow((r - R + 0.06 * (n - 0.5)) / w, 2.0));
            float inner = smoothstep(R, 0.0, r) * 0.15 * n;
            a = (ring * 1.5 + inner) * (1.0 - age);
            col = mix(uCol2.rgb, uCol.rgb, ring);
        } else if (uStyle == 1) {     // magic circle: rings, runes, hexagram, drawn in then fading
            float rot = t * 0.9 + seed;
            float ang2 = ang + rot;
            float rings = exp(-pow((r - 0.96) / 0.022, 2.0)) + exp(-pow((r - 0.83) / 0.016, 2.0)) + 0.8 * exp(-pow((r - 0.47) / 0.018, 2.0)) + 0.7 * exp(-pow((r - 0.22) / 0.014, 2.0));
            float band = step(0.85, r) * step(r, 0.94);
            float cells = 28.0;
            float seg = fract((ang2 + 3.14159) * cells / 6.2831853), cell = floor((ang2 + 3.14159) * cells / 6.2831853);
            float h = hash12(vec2(cell, seed * 13.0));
            float rv = (r - 0.85) / 0.09;
            float glyph = band * step(0.18, seg) * step(seg, 0.82) * step(0.35, fract(h * 5.0 + floor(rv * 3.0) * 0.37)) * step(abs(rv - 0.5), 0.4);
            float star = 0.0;
            for (int i = 0; i < 6; i++) {
                float aa = float(i) * 1.0471976 - rot * 1.6;
                star = max(star, line(vLocal.xz, vec2(cos(aa), sin(aa)) * 0.83, vec2(cos(aa + 2.0943951), sin(aa + 2.0943951)) * 0.83, 0.011));
            }
            float sweep = clamp(age * 5.0 - (ang + 3.14159) / 6.2831853, 0.0, 1.0);   // the circle is traced in a sweep
            float fadeOut = 1.0 - smoothstep(0.78, 1.0, age);
            a = (rings + glyph * 1.3 + star * 0.9) * sweep * fadeOut + smoothstep(1.0, 0.0, r) * 0.05 * fadeOut * smoothstep(0.0, 0.2, age);
            a *= step(r, 1.0);
            col = mix(uCol2.rgb, uCol.rgb, clamp(rings + glyph, 0.0, 1.0));
        } else if (uStyle == 2) {     // burning / cracked ground
            float n = fbmN(vLocal.xz * 0.9 + seed);
            float cracks = 1.0 - smoothstep(0.0, 0.06, abs(n - 0.5));
            float fall = smoothstep(1.0, 0.25, r);
            a = (cracks * 1.6 + fall * 0.35 * n) * fall * (1.0 - age);
            col = mix(uCol2.rgb, uCol.rgb, cracks);
        } else {                      // sun rays bursting out
            float rays = pow(abs(sin(ang * 8.0 + seed + t * 0.6)), 16.0);
            float R = age * 1.15;
            a = rays * smoothstep(R, R * 0.2, r) * smoothstep(0.0, 0.08, r) * (1.0 - age) * 1.6 + exp(-pow((r - R * 0.9) / 0.05, 2.0)) * (1.0 - age);
        }
    } else if (uShape == 1 || uShape == 4) {
        float ang = atan(vLocal.z, vLocal.x), v = vLocal.y;
        float facing = abs(dot(normalize(vNrm), V));
        if (uStyle == 0) {            // energy beam: white-hot core, flowing noise, glowing edge
            float n = fbmN(vec2(ang * 0.5, v * 3.0 - t * 7.0) + seed);
            float core = pow(facing, 4.0), edge = pow(1.0 - facing, 2.0);
            float ends = smoothstep(0.0, 0.03, v) * smoothstep(1.0, 0.94, v);
            float life = smoothstep(0.0, 0.06, age) * (1.0 - smoothstep(0.55, 1.0, age));
            a = (core * 1.6 + n * 0.7 * facing + edge * 0.25) * ends * life;
            col = mix(uCol2.rgb, mix(uCol.rgb, vec3(1.0), core * 0.5), facing);
        } else {                      // 1: pillar of light, 2: vortex (spiralling streaks)
            float spiral = uStyle == 2 ? ang * 3.0 + v * 7.0 - t * 9.0 : ang;
            float n = fbmN(vec2(spiral * 0.32, v * 1.6 - t * (uStyle == 2 ? 1.6 : 2.6)) + seed);
            float streak = smoothstep(0.45, 0.88, n);
            float fadeV = (1.0 - v) * smoothstep(0.0, 0.06, v);
            float life = smoothstep(0.0, 0.1, age) * (1.0 - smoothstep(0.55, 1.0, age));
            a = (streak * 1.2 + pow(facing, 2.0) * 0.12) * fadeV * life;
            col = mix(uCol2.rgb, uCol.rgb, streak);
        }
    } else if (uShape == 2) {
        vec3 N = normalize(vNrm);
        float facing = abs(dot(N, V)), fres = pow(1.0 - facing, 2.5);
        vec2 suv = vec2(atan(vLocal.z, vLocal.x) * 0.3183, vLocal.y * 0.5 + 0.5);
        if (uStyle == 0) {            // shield / aura shell: fresnel rim, flowing cells
            float n = fbmN(suv * vec2(3.0, 2.0) + vec2(t * 0.15, -t * 0.3) + seed);
            float cells = smoothstep(0.42, 0.5, abs(fract(suv.x * 12.0) - 0.5)) + smoothstep(0.42, 0.5, abs(fract(suv.y * 8.0 + t * 0.2) - 0.5));
            float life = smoothstep(0.0, 0.1, age) * (1.0 - smoothstep(0.8, 1.0, age));
            a = (fres * 1.25 + n * 0.08 + cells * 0.3 * fres) * life;
            col = mix(uCol.rgb, uCol2.rgb, fres);
        } else if (uStyle == 1) {     // explosion: hot core, burning away as it grows
            float n = fbmN(suv * vec2(2.0, 1.5) + vec2(0.0, -t * 0.8) + seed);
            float burn = smoothstep(age * 1.1 - 0.15, age * 1.1, n);
            float hot = pow(facing, 1.5);
            a = (hot * 1.15 + 0.25) * burn * (1.0 - age * 0.6);
            col = mix(uCol2.rgb, mix(uCol.rgb, vec3(1.0, 0.95, 0.85), hot * 0.35), hot);
        } else {                      // energy orb (projectiles): bright core, swirling surface
            float core = pow(facing, 3.0);
            float n = fbmN(suv * 3.0 + vec2(t * 0.6, t * 0.9) + seed);
            a = core * 1.5 + fres * 0.7 + n * 0.3 * facing;
            col = mix(uCol2.rgb, mix(uCol.rgb, vec3(1.0), core * 0.45), core);
        }
    } else {                          // arc: a blade slash sweeping through the air
        float r = length(vLocal.xz), ang = atan(vLocal.z, vLocal.x);
        float u = clamp(ang / 3.14159, 0.0, 1.0);
        float head = age * 1.5;
        float trail = smoothstep(head - 0.75, head, u) * step(u, head);
        float w = (r - 0.7) / 0.3;
        float thick = smoothstep(0.0, 0.35, w) * smoothstep(1.0, 0.75, w);
        float edgeL = exp(-pow((w - 0.86) / 0.07, 2.0));
        float n = fbmN(vec2(u * 3.0 - t * 2.0, w) + seed);
        a = (thick * 0.75 * (0.6 + 0.6 * n) + edgeL * 1.6) * trail * (1.0 - smoothstep(0.7, 1.0, age));
        col = mix(uCol2.rgb, uCol.rgb, edgeL);
    }
    float dist = length(vWorld - uCamPos.xyz);
    a *= exp(-max(dist - 35.0, 0.0) * uFogColor.w * 0.5);
    a = max(a, 0.0) * uCol.a;
    fragColor = vec4(col * a * uP.y, 0.0);
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
// Screen-space ambient occlusion from depth only (mobile friendly: half resolution, 10 samples).
// View-space position from linear depth, normal from screen derivatives, golden-angle hemisphere samples
// rotated per pixel by the noise texture; range check avoids dark halos around silhouettes.
const char* SSAO_FS = R"(
in vec2 vUV;
uniform sampler2D uDepth;
uniform sampler2D uNoiseTex;
uniform vec4 uProjAo;    // near, far, tan(fovX/2), tan(fovY/2)
uniform vec4 uAoParams;  // radius (m), intensity, fade distance (m)
out vec4 fragColor;
float linZ(float d) { float n = uProjAo.x, f = uProjAo.y; float z = d * 2.0 - 1.0; return 2.0 * n * f / (f + n - z * (f - n)); }
void main() {
    float d = texture(uDepth, vUV).r;
    if (d >= 0.99999) { fragColor = vec4(1.0); return; }
    float z = linZ(d);
    vec3 P = vec3((vUV * 2.0 - 1.0) * uProjAo.zw * z, -z);
    vec3 N = normalize(cross(dFdx(P), dFdy(P)));
    if (dot(N, -P) < 0.0) N = -N;
    vec3 T = normalize(abs(N.y) < 0.9 ? cross(N, vec3(0.0, 1.0, 0.0)) : cross(N, vec3(1.0, 0.0, 0.0)));
    vec3 Bt = cross(N, T);
    float ang = texture(uNoiseTex, gl_FragCoord.xy / 64.0).r * 6.2831853;
    float R = uAoParams.x * (1.0 + z * 0.01);
    float occ = 0.0;
    for (int i = 0; i < 10; i++) {
        float fi = float(i);
        float a = ang + fi * 2.3999632;
        float r = sqrt((fi + 0.5) / 10.0);
        float h = fract(fi * 0.618034 + 0.31) * 0.75 + 0.25;
        vec3 dir = (T * cos(a) + Bt * sin(a)) * r + N * h;
        vec3 S = P + dir * R * (0.25 + 0.75 * r);
        vec2 suv = (S.xy / -S.z) / uProjAo.zw * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
        float sz = linZ(texture(uDepth, suv).r);
        float diff = -S.z - sz;                                  // > 0: something in front of the sample
        float range = smoothstep(0.0, 1.0, R / max(abs(z - sz), 1e-3));
        occ += step(0.03 + z * 0.003, diff) * range;   // 3.0.4 a little more bias: flat ground never shades itself
    }
    float ao = clamp(1.0 - occ / 10.0 * uAoParams.y, 0.0, 1.0);
    ao = mix(ao, 1.0, smoothstep(uAoParams.z * 0.6, uAoParams.z, z));
    fragColor = vec4(ao, ao, ao, 1.0);
}
)";

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
uniform sampler2D uAO;
uniform vec4 uAoOn;   // x strength (0 = off), yz AO texel size
uniform mediump sampler2DShadow uShadowMap;   // 1.11.1 light shafts
uniform mat4 uInvVP, uShadowM;
uniform vec3 uEye, uSunC;
uniform vec4 uSunS;   // xyz direction towards the sun, w amount (0 = off)
uniform vec4 uFx0;    // 2.0.0 x heat shimmer (mirage), y sand veil (sandstorm)
uniform vec4 uSunScr; // 2.0.0 lens flare: xy sun position on screen (uv), z aspect, w amount
uniform vec4 uSharp;  // 3.0.2 x sharpening amount, yz scene texel size
uniform vec4 uUnder;  // 3.5.0 x camera under water (0..1), y depth below the surface (m), z daylight
uniform vec3 uUnderC; // 3.5.0 the colour of the water
out vec4 fragColor;

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float linDepth(float z) { float n = uProj.x, f = uProj.y; z = z * 2.0 - 1.0; return 2.0 * n * f / (f + n - z * (f - n)); }

void main() {
    vec2 uv = vUV;
    vec2 uv0 = vUV;
    vec3 col;
    // 2.0.0 heat shimmer: the far ground wobbles (a mirage over the dunes); near things stay still
    if (uFx0.x > 0.0) {
        float dh = linDepth(texture(uDepth, uv).r);
        float m = smoothstep(30.0, 160.0, dh) * uFx0.x;
        float tt = uTime;
        float wob = sin(uv.y * 150.0 + tt * 2.3) + 0.6 * sin(uv.y * 330.0 - tt * 3.1 + uv.x * 25.0);
        uv.x += wob * 0.0016 * m;
        uv.y += sin(uv.x * 90.0 + tt * 1.7) * 0.0008 * m;
    }
    // 3.4.0 the blazing sun: hot air rising off the ground, from a dozen metres away, in slow vertical waves
    if (uFx0.z > 0.0) {
        float dh = linDepth(texture(uDepth, uv0).r);
        float m = smoothstep(10.0, 45.0, dh) * (1.0 - smoothstep(250.0, 380.0, dh)) * uFx0.z;
        float tt = uTime;
        float rise = sin(uv0.y * 210.0 + tt * 5.2 + sin(uv0.x * 40.0 + tt) * 2.0) * 0.6 + sin(uv0.y * 470.0 + tt * 7.7 + uv0.x * 60.0) * 0.4;
        uv.x += rise * 0.0011 * m;
        uv.y += sin(uv0.x * 120.0 - tt * 3.3) * 0.0006 * m;
    }
    // 3.5.0 under water the picture ripples gently
    if (uUnder.x > 0.0) {
        float tt = uTime;
        uv += vec2(sin(uv0.y * 24.0 + tt * 1.6) + 0.5 * sin(uv0.y * 61.0 - tt * 2.3), cos(uv0.x * 21.0 + tt * 1.3) + 0.5 * cos(uv0.x * 47.0 + tt * 2.1)) * 0.0022 * uUnder.x;
    }
    if (uPost0.w > 0.0) {
        vec2 dir = (uv - 0.5) * uPost0.w * 0.012;
        col = vec3(texture(uScene, uv + dir).r, texture(uScene, uv).g, texture(uScene, uv - dir).b);
    } else col = texture(uScene, uv).rgb;
    if (uSharp.x > 0.0) {   // 3.0.2 restores the detail lost to a reduced internal resolution and to FXAA
        vec3 nb = texture(uScene, uv + vec2(uSharp.y, 0.0)).rgb + texture(uScene, uv - vec2(uSharp.y, 0.0)).rgb +
                  texture(uScene, uv + vec2(0.0, uSharp.z)).rgb + texture(uScene, uv - vec2(0.0, uSharp.z)).rgb;
        col = max(col + (col - nb * 0.25) * uSharp.x, vec3(0.0));
    }
    col *= uProj.z;
    if (uAoOn.x > 0.0) {
        // 4 bilinear taps = soft 4x4 blur of the half-resolution AO
        vec2 o = uAoOn.yz * 0.75;
        float ao = 0.25 * (texture(uAO, uv + vec2(-o.x, -o.y)).r + texture(uAO, uv + vec2(o.x, -o.y)).r +
                           texture(uAO, uv + vec2(-o.x, o.y)).r + texture(uAO, uv + vec2(o.x, o.y)).r);
        col *= mix(1.0, ao, uAoOn.x);
    }
    // Depth of field: cinematic focus + tilt-shift (miniature diorama feel of the 2.5D view)
    float dofAmt = uLift.a;
    float tilt = uGain.a * smoothstep(0.18, 0.5, abs(uv.y - 0.47));
    float coc = tilt;
    if (dofAmt > 0.0) {
        float d = linDepth(texture(uDepth, uv).r);
        coc = max(coc, clamp(abs(d - uGamma.a) / max(uGamma.a, 1.0) * 1.6, 0.0, 1.0) * dofAmt);
    }
    if (coc > 0.01) col = mix(col, texture(uBlur, uv).rgb * uProj.z, clamp(coc, 0.0, 1.0));
    // 3.5.0 under water: the water absorbs red first, then everything, with distance; a blue-green murk
    // closes in at ~40 m, darker the deeper you go; shafts of sunlight slant down from the surface
    if (uUnder.x > 0.0) {
        float dh = linDepth(texture(uDepth, uv).r);
        float deep = exp(-uUnder.y * 0.07);
        vec3 wc = uUnderC * (0.35 + 0.65 * deep) * (0.25 + 0.75 * clamp(uUnder.z / 2.5, 0.0, 1.0));
        vec3 absorb = exp(-dh * vec3(0.11, 0.035, 0.025));
        vec3 uw = col * absorb;
        float murk = 1.0 - exp(-dh * 0.055);
        uw = mix(uw, wc, murk);
        float tt = uTime;
        float sx = uv0.x + uv0.y * 0.35;   // the shafts lean with the sun
        float rays = pow(0.5 + 0.5 * sin(sx * 31.0 + tt * 0.35 + sin(sx * 7.0 - tt * 0.2) * 2.0), 6.0) * 0.6 + pow(0.5 + 0.5 * sin(sx * 57.0 - tt * 0.5), 10.0) * 0.4;
        rays *= smoothstep(0.15, 1.0, uv0.y) * deep * clamp(uUnder.z / 2.5, 0.0, 1.0) * smoothstep(2.0, 18.0, dh);
        uw += uUnderC * vec3(1.4, 1.6, 1.5) * rays * 0.55;
        col = mix(col, uw, uUnder.x);
    }
    if (uSunS.w > 0.0) {
        // Volumetric sunlight: 14 shadow-map taps along the view ray (jittered, max 70 m). Where trees, roofs or
        // cliffs block the sun the air stays dark, elsewhere it glows: visible shafts in the haze.
        vec4 wp = uInvVP * vec4(uv * 2.0 - 1.0, texture(uDepth, uv).r * 2.0 - 1.0, 1.0);
        vec3 P = wp.xyz / wp.w, V = P - uEye;
        float L = min(length(V), 70.0); vec3 dir = V / max(length(V), 1e-3);
        float jit = hash12(gl_FragCoord.xy);   // 3.0.2 white noise (the interleaved gradient drew diagonal stripes in the haze)
        float lit = 0.0;
        for (int i = 0; i < 14; i++) {
            vec3 q = uEye + dir * (L * (float(i) + jit) / 14.0);
            vec4 s = uShadowM * vec4(q, 1.0);
            s.xyz /= s.w;
            bool inside = all(greaterThan(s.xy, vec2(0.0))) && all(lessThan(s.xy, vec2(1.0))) && s.z < 1.0;
            lit += inside ? texture(uShadowMap, vec3(s.xy, s.z - 0.002)) : 1.0;
        }
        lit /= 14.0;
        float mu = dot(dir, uSunS.xyz);
        float g = 0.55;   // Henyey-Greenstein forward scattering + a little isotropic
        float phase = 0.08 + (1.0 - g * g) / (12.566 * pow(1.0 + g * g - 2.0 * g * mu, 1.5));
        float air = 1.0 - exp(-L * 0.018);
        col += uSunC * (lit * lit) * phase * air * uSunS.w * 0.55;
    }
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
    // 3.4.0 the blazing sun: highlights burn to a warm white, the shade stays cool and deep
    if (uFx0.z > 0.0) {
        float lb = dot(clamp(col, 0.0, 1.0), vec3(0.2126, 0.7152, 0.0722));
        float nearK = 1.0 - smoothstep(50.0, 170.0, linDepth(texture(uDepth, uv0).r));   // 3.4.1 the distance stays readable (no white veil over the horizon)
        col = mix(col, vec3(1.0, 0.95, 0.84), smoothstep(0.62, 1.0, lb) * 0.38 * uFx0.z * nearK);
        col = mix(col, col * vec3(0.9, 0.95, 1.08), (1.0 - smoothstep(0.08, 0.4, lb)) * 0.5 * uFx0.z);
    }
    // split toning: cool shadows, warm highlights (cinematic separation, subtle)
    float lt = dot(clamp(col, 0.0, 1.0), vec3(0.2126, 0.7152, 0.0722));
    col += vec3(-0.02, 0.0, 0.035) * (1.0 - smoothstep(0.0, 0.45, lt)) + vec3(0.03, 0.012, -0.025) * smoothstep(0.55, 1.0, lt);
    // vignette (+ red pulse when badly hurt)
    vec2 vc = uv - 0.5;
    float vig = 1.0 - dot(vc, vc) * uPost0.z * 1.6;
    col *= clamp(vig, 0.0, 1.0);
    col = mix(col, vec3(0.55, 0.0, 0.0), uProj.w * smoothstep(0.15, 0.6, length(vc)));
    // grain
    // 2.0.0 sandstorm: a veil of ochre grit racing across the picture, in long horizontal streaks
    if (uFx0.y > 0.0) {
        float s1 = hash12(vec2(uv0.x * 5.0 - uTime * 2.4, uv0.y * 90.0));
        float s2 = hash12(vec2(uv0.x * 11.0 - uTime * 4.6, uv0.y * 160.0) + 4099.0);
        float streak = smoothstep(0.55, 1.0, s1) * 0.55 + smoothstep(0.7, 1.0, s2) * 0.45;
        vec3 sandc = vec3(0.80, 0.56, 0.32) * (0.55 + 0.45 * smoothstep(0.1, 0.9, dot(col, vec3(0.33))));
        col = mix(col, sandc, uFx0.y * (0.22 + 0.2 * streak));
        col += vec3(0.95, 0.7, 0.4) * streak * uFx0.y * 0.07;
    }
    // 2.0.0 lens flare: soft halo, ghosts mirrored through the centre, an anamorphic streak (only when the sun is in view and unobstructed)
    if (uSunScr.w > 0.0) {
        vec2 sp = uSunScr.xy;
        float vis = 0.0;
        for (int i = 0; i < 5; i++) {
            vec2 o = i == 0 ? vec2(0.0) : vec2(i == 1 ? 0.012 : (i == 2 ? -0.012 : 0.0), i == 3 ? 0.014 : (i == 4 ? -0.014 : 0.0));
            vec2 q = clamp(sp + o, vec2(0.002), vec2(0.998));
            vis += step(0.9999, texture(uDepth, q).r);
        }
        vis *= 0.2;
        vis *= smoothstep(-0.25, 0.05, sp.x) * smoothstep(1.25, 0.95, sp.x) * smoothstep(-0.25, 0.05, sp.y) * smoothstep(1.25, 0.95, sp.y);
        if (vis > 0.001) {
            vec2 asp = vec2(uSunScr.z, 1.0);
            vec2 p = (uv0 - 0.5) * asp, sP = (sp - 0.5) * asp;
            float ds = length(p - sP);
            vec3 fl = vec3(1.0, 0.82, 0.55) * (0.10 / (1.0 + ds * ds * 60.0));
            fl += vec3(0.35, 0.6, 1.0) * exp(-abs(p.y - sP.y) * 55.0) * exp(-abs(p.x - sP.x) * 2.2) * 0.10;
            for (int k = 0; k < 4; k++) {
                float g = k == 0 ? 0.35 : (k == 1 ? 0.7 : (k == 2 ? 1.25 : 1.9));
                float r = k == 0 ? 0.035 : (k == 1 ? 0.06 : (k == 2 ? 0.045 : 0.09));
                vec2 gp = mix(sP, -sP, g);
                float dg = length(p - gp);
                float ring = smoothstep(r, r * 0.82, dg) - smoothstep(r * 0.82, r * 0.6, dg) * 0.8;
                vec3 tint = k == 0 ? vec3(1.0, 0.6, 0.3) : (k == 1 ? vec3(0.4, 0.8, 1.0) : (k == 2 ? vec3(0.6, 1.0, 0.5) : vec3(1.0, 0.45, 0.7)));
                fl += tint * ring * 0.05;
            }
            // 3.4.0 a blinding starburst round a blazing sun: long thin rays that turn slowly, a white-hot core
            {   // (also on any clear day, stronger under the blazing sun)
                float kb = 0.45 + 0.55 * uFx0.z;
                vec2 dv = p - sP;
                float ang = atan(dv.y, dv.x);
                float rays = pow(abs(cos(ang * 6.0 + uTime * 0.05)), 60.0) + 0.6 * pow(abs(cos(ang * 9.0 - uTime * 0.03 + 0.7)), 90.0);
                float fall = exp(-ds * 4.5);
                fl += vec3(1.0, 0.9, 0.7) * rays * fall * 0.55 * kb;
                fl += vec3(1.0, 0.92, 0.75) * (0.35 / (1.0 + ds * ds * 900.0) + 0.12 / (1.0 + ds * ds * 25.0)) * kb;
            }
            col += fl * vis * uSunScr.w;
        }
    }
    float fr = floor(uTime * 24.0);
    float n = hash12(gl_FragCoord.xy + vec2(fr * 37.0, fr * 91.0)) - 0.5;   // 3.0.2 per-pixel film grain (no bands)
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
uniform float uSharpen;
out vec4 fragColor;
void main() {
    vec3 rgbM = texture(uSrc, vUV).rgb;
    float lM = texture(uSrc, vUV).a;
    float lNW = texture(uSrc, vUV + vec2(-1, -1) * uTexel).a, lNE = texture(uSrc, vUV + vec2(1, -1) * uTexel).a;
    float lSW = texture(uSrc, vUV + vec2(-1, 1) * uTexel).a, lSE = texture(uSrc, vUV + vec2(1, 1) * uTexel).a;
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    if (lMax - lMin < max(0.0312, lMax * 0.125)) {
        // flat area: contrast-adaptive sharpening (the scene is rendered below native resolution)
        vec3 n4 = texture(uSrc, vUV + vec2(0.0, -uTexel.y)).rgb + texture(uSrc, vUV + vec2(0.0, uTexel.y)).rgb
                + texture(uSrc, vUV + vec2(-uTexel.x, 0.0)).rgb + texture(uSrc, vUV + vec2(uTexel.x, 0.0)).rgb;
        float amt = uSharpen * (1.0 - smoothstep(0.0, 0.25, lMax - lMin));
        fragColor = vec4(clamp(rgbM + (rgbM - n4 * 0.25) * amt, 0.0, 1.0), 1.0);
        return;
    }
    vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), ((lNW + lSW) - (lNE + lSE)));
    float red = max((lNW + lNE + lSW + lSE) * 0.03125, 1.0 / 128.0);
    float rcp = 1.0 / (min(abs(dir.x), abs(dir.y)) + red);
    dir = clamp(dir * rcp, vec2(-2.5), vec2(2.5)) * uTexel;   // 3.6.1 a short span (was 8): dense grass is all edges, a long span smeared it
    vec3 a = 0.5 * (texture(uSrc, vUV + dir * (1.0 / 3.0 - 0.5)).rgb + texture(uSrc, vUV + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 b = a * 0.5 + 0.25 * (texture(uSrc, vUV + dir * -0.5).rgb + texture(uSrc, vUV + dir * 0.5).rgb);
    float lB = dot(b, vec3(0.299, 0.587, 0.114));
    fragColor = vec4(mix(rgbM, (lB < lMin || lB > lMax) ? a : b, 0.65), 1.0);   // 3.6.1 keep a third of the sharp pixel
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
