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

/// Sprites lit by up to 16 point or directional lights (Light2D) plus the scene ambient. Variant
/// AGR_NORMAL_MAP: per-pixel normals from a normal map (OpenGL convention, green up) in the
/// sprite's frame, rebuilt from screen derivatives (rotation and flips need no vertex data);
/// each light shades the relief from its height above the plane (flat areas are unchanged).
inline constexpr const char* kSpriteLitFragment = R"(
layout(std140) uniform Lights2D {
    vec4 u_lightPosRadius[16];     // xy position, z height above the sprites (normal maps), w radius
    vec4 u_lightColorFalloff[16];  // rgb linear color * intensity, w falloff exponent
    vec4 u_lightShadow[16];        // shadow row (-1: none), softness (texels), strength, penetration / radius
    vec4 u_lightDirShadow[16];     // directional: across min, 1 / across range, along min, 1 / along range
    ivec4 u_lightCount;
};
uniform sampler2D u_texture;
uniform sampler2D u_shadow2D;  // polar shadow maps: one row per shadowed light, distance / radius in r (high) g (low)
uniform sampler2D u_normalMap;
in vec2 v_uv;
in vec4 v_color;
in vec3 v_worldPos;
layout(location = 0) out vec4 o_color;
void main() {
    vec4 c = texture(u_texture, v_uv) * v_color;
    vec3 light = u_ambient.rgb;
#ifdef AGR_NORMAL_MAP
    vec3 tn = texture(u_normalMap, v_uv).xyz * 2.0 - 1.0;
    // World directions of increasing u and v (dp/du, dp/dv) from screen derivatives: dividing by
    // the uv Jacobian makes them independent of the screen orientation (Vulkan flips y).
    vec3 dp1 = dFdx(v_worldPos), dp2 = dFdy(v_worldPos);
    vec2 du1 = dFdx(v_uv), du2 = dFdy(v_uv);
    float det = du1.x * du2.y - du1.y * du2.x;
    det = abs(det) < 1e-12 ? 1e-12 : det;
    vec3 T = (dp1 * du2.y - dp2 * du1.y) / det;
    vec3 B = (dp2 * du1.x - dp1 * du2.x) / det;
    T = dot(T, T) > 0.0 ? normalize(T) : vec3(1.0, 0.0, 0.0);
    B = dot(B, B) > 0.0 ? normalize(B) : vec3(0.0, -1.0, 0.0);
    // v grows down the image while the normal map's green points up.
    vec3 n = normalize(T * tn.x - B * tn.y + vec3(0.0, 0.0, 1.0) * max(tn.z, 0.0));
#endif
    for (int i = 0; i < 16; ++i) {
        if (i >= u_lightCount.x) break;
        // w < 0: directional light, xy = direction it travels (no radius, no falloff).
        bool directional = u_lightPosRadius[i].w < 0.0;
        vec2 d = directional ? u_lightPosRadius[i].xy : v_worldPos.xy - u_lightPosRadius[i].xy;
        float dist = directional ? 0.0 : length(d) / u_lightPosRadius[i].w;
        float att = clamp(1.0 - dist, 0.0, 1.0);
        float lit = 1.0;
        if (directional && u_lightShadow[i].x > -0.5) {
            // Parallel shadow map: first occluder along the light on this pixel's line.
            vec2 dirL = u_lightPosRadius[i].xy, perp = vec2(-dirL.y, dirL.x);
            vec4 ds = u_lightDirShadow[i];
            float u = (dot(v_worldPos.xy, perp) - ds.x) * ds.y;
            float a = (dot(v_worldPos.xy, dirL) - ds.z) * ds.w;
            float v = (u_lightShadow[i].x + 0.5) / 8.0;
            float spread = u_lightShadow[i].y / 512.0;
            float sum = 0.0;
            for (int k = -2; k <= 2; ++k) {
                vec4 t = texture(u_shadow2D, vec2(u + float(k) * spread * 0.5, v));
                float stored = (t.r * 255.0 * 256.0 + t.g * 255.0) / 65535.0;
                sum += a <= stored + 0.002 + u_lightShadow[i].w ? 1.0 : 0.0;
            }
            lit = mix(1.0, sum / 5.0, u_lightShadow[i].z);
        } else if (u_lightShadow[i].x > -0.5 && att > 0.0) {
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
#ifdef AGR_NORMAL_MAP
        // Relative to a flat surface: flat areas light exactly like a sprite without a normal
        // map, slopes facing the light brighten (up to 2x), slopes facing away darken.
        vec3 L = normalize(vec3(-d, u_lightPosRadius[i].z));
        lit *= clamp(dot(n, L) / max(L.z, 0.25), 0.0, 2.0);
#endif
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
    vec4 u_fog;        // height fog: density, base height, falloff, sun scattering
    vec4 u_fogColor;   // rgb (linear when HDR), 1 = height fog on
    vec4 u_shafts;     // sun on screen (uv), intensity, decay per sample
    vec4 u_sun;        // sun color (linear) * intensity, 1 = sun in front of the camera
    vec4 u_camera;     // camera position, distance given to the sky
    vec4 u_sunDir;     // direction toward the sun, 1 = light shafts on
    mat4 u_invViewProj;
    vec4 u_dof;        // focus distance, focus range, largest blur radius (pixels), 1 = near blur
    vec4 u_dof2;       // 1 = far blur, -, -, -
    vec4 u_exposure;   // 1 = auto exposure, target, min, max
    vec4 u_adapt;      // blend toward the measure this frame (0..1), 1 = history valid, -, -
    mat4 u_prevViewProj;  // last frame's view-projection, without jitter (TAA)
    vec4 u_taa;        // weight of the new frame, 1 = history valid, -, -
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
uniform sampler2D u_exposureTex;  // 1 x 1: adapted average log luminance (auto exposure)
layout(location = 0) out vec4 o_color;
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec3 c = texture(u_scene, uv).rgb;
    if (u_ao.z > 0.5) c *= texture(u_aoTex, uv).r;  // before tone mapping, like light
    if (u_bloom.z > 0.0) c += texture(u_bloomTex, uv).rgb * u_bloom.z;
    if (u_exposure.x > 0.5) {
        float average = exp(texture(u_exposureTex, vec2(0.5)).r);
        c *= clamp(u_exposure.y / max(average, 1e-4), u_exposure.z, u_exposure.w);
    }
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

/// Depth of field (gather): each pixel averages 32 taps on a golden-angle disc of the largest
/// blur radius; a tap counts when its own circle of confusion reaches this pixel, and a tap
/// behind the pixel cannot blur more than the pixel itself (no background halo over a sharp
/// foreground). Circle of confusion: 0 inside the focus zone, growing over `range` beyond it.
inline constexpr const char* kDof = R"(
uniform sampler2D u_source;  // scene color
uniform sampler2D u_depth;   // scene depth
layout(location = 0) out vec4 o_color;
float distanceAt(vec2 uv) {
    float d = texture(u_depth, uv).r;
    float z = u_ao.w > 0.5 ? d * 2.0 - 1.0 : d;
    vec2 ndc = uv * 2.0 - 1.0;
    if (u_ao.w < 0.5) ndc.y = -ndc.y;
    vec4 p = u_invViewProj * vec4(ndc, z, 1.0);
    return d >= 0.99999 ? 1e6 : length(p.xyz / p.w - u_camera.xyz);
}
float coc(float dist) {
    float halfRange = u_dof.y * 0.5;
    float far = u_dof2.x > 0.5 ? clamp((dist - (u_dof.x + halfRange)) / max(u_dof.y, 1e-3), 0.0, 1.0) : 0.0;
    float near = u_dof.w > 0.5 ? clamp(((u_dof.x - halfRange) - dist) / max(u_dof.y, 1e-3), 0.0, 1.0) : 0.0;
    return max(far, near) * u_dof.z;  // pixels
}
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec3 center = texture(u_source, uv).rgb;
    float centerDist = distanceAt(uv);
    float centerCoc = coc(centerDist);
    vec3 sum = center;
    float total = 1.0;
    const int kTaps = 32;
    for (int i = 1; i < kTaps; ++i) {
        float r = sqrt(float(i) / float(kTaps - 1)) * u_dof.z;  // uniform over the disc
        float a = float(i) * 2.39996323;                          // golden angle
        vec2 o = vec2(cos(a), sin(a)) * r;
        vec2 suv = (floor(gl_FragCoord.xy + o) + 0.5) * u_texel.xy;  // texel centers: color and depth of the same texel
        float sd = distanceAt(suv);
        float sc = coc(sd);
        if (sd > centerDist) sc = min(sc, centerCoc);  // behind: no halo over a sharp foreground
        float w = clamp(sc - r + 0.5, 0.0, 1.0);
        sum += texture(u_source, suv).rgb * w;
        total += w;
    }
    o_color = vec4(sum / total, 1.0);
}
)";

/// Screen-space reflections: the reflected view ray marched in view space through the scene
/// depth (32 steps, dithered start, 5 refinement steps), added where the opaque pass wrote a
/// reflectivity in the scene alpha. Fades at the screen edges, with distance and for rays
/// toward the camera. u_dof2: -, intensity, max distance, thickness.
inline constexpr const char* kSsr = R"(
uniform sampler2D u_source;  // scene color, reflectivity in alpha
uniform sampler2D u_depth;   // scene depth
layout(location = 0) out vec4 o_color;
vec2 uvToNdc(vec2 uv) {
    vec2 n = uv * 2.0 - 1.0;
    if (u_ao.w < 0.5) n.y = -n.y;
    return n;
}
vec2 ndcToUv(vec2 n) {
    if (u_ao.w < 0.5) n.y = -n.y;
    return n * 0.5 + 0.5;
}
float rawDepth(vec2 uv) { return texture(u_depth, uv).r; }
vec3 viewPos(vec2 uv) {
    float d = rawDepth(uv);
    float z = u_ao.w > 0.5 ? d * 2.0 - 1.0 : d;
    vec4 p = u_invProj * vec4(uvToNdc(uv), z, 1.0);
    return p.xyz / p.w;
}
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec4 scene = texture(u_source, uv);
    float reflectivity = scene.a * u_dof2.y;
    if (reflectivity < 0.004 || rawDepth(uv) >= 0.99999) {
        o_color = vec4(scene.rgb, 1.0);
        return;
    }
    vec3 P = viewPos(uv);
    vec2 px = u_texel.xy;
    vec3 r = viewPos(uv + vec2(px.x, 0.0)) - P, l = P - viewPos(uv - vec2(px.x, 0.0));
    vec3 t = viewPos(uv + vec2(0.0, px.y)) - P, b = P - viewPos(uv - vec2(0.0, px.y));
    vec3 N = normalize(cross(abs(r.z) < abs(l.z) ? r : l, abs(t.z) < abs(b.z) ? t : b));
    if (dot(N, P) > 0.0) N = -N;  // toward the camera
    vec3 V = normalize(P);
    vec3 R = reflect(V, N);
    float towardCamera = clamp(R.z * 2.0, 0.0, 1.0);  // rays coming back rarely find their target
    if (towardCamera >= 1.0) {
        o_color = vec4(scene.rgb, 1.0);
        return;
    }
    const int kSteps = 32;
    float maxDist = u_dof2.z, thickness = u_dof2.w;
    float stepLen = maxDist / float(kSteps);
    vec2 cell = mod(floor(gl_FragCoord.xy), 4.0);
    float dither = (cell.x + 4.0 * cell.y + 0.5) / 16.0;
    float tPrev = 0.0, tHit = -1.0;
    vec3 start = P + N * (0.01 - 0.002 * P.z);  // off the surface (depth precision)
    for (int i = 1; i <= kSteps; ++i) {
        float tt = (float(i) - 0.5 + dither * 0.5) * stepLen;
        vec3 S = start + R * tt;
        if (S.z > -0.01) break;  // behind the camera
        vec4 c = u_proj * vec4(S, 1.0);
        vec2 suv = ndcToUv(c.xy / c.w);
        if (suv.x < 0.0 || suv.y < 0.0 || suv.x > 1.0 || suv.y > 1.0) break;
        if (rawDepth(suv) >= 0.99999) {  // the sky: nothing to hit here
            tPrev = tt;
            continue;
        }
        float sceneZ = viewPos(suv).z;
        float behind = sceneZ - S.z;  // > 0: the ray point is behind the visible surface
        float bias = 0.01 - 0.003 * S.z;  // depth precision (16-bit depth far away)
        if (behind > bias && behind < thickness + stepLen * abs(R.z)) {
            tHit = tt;
            break;
        }
        tPrev = tt;
    }
    if (tHit < 0.0) {
        o_color = vec4(scene.rgb, 1.0);
        return;
    }
    // Refine between the last miss and the hit.
    float lo = tPrev, hi = tHit;
    for (int k = 0; k < 5; ++k) {
        float mid = (lo + hi) * 0.5;
        vec3 S = start + R * mid;
        vec4 c = u_proj * vec4(S, 1.0);
        vec2 suv = ndcToUv(c.xy / c.w);
        if (viewPos(suv).z - S.z > 0.0) hi = mid;
        else lo = mid;
    }
    vec3 H = start + R * hi;
    vec4 hc = u_proj * vec4(H, 1.0);
    vec2 huv = ndcToUv(hc.xy / hc.w);
    vec2 edge = smoothstep(vec2(0.0), vec2(0.08), huv) * smoothstep(vec2(0.0), vec2(0.08), 1.0 - huv);
    float fade = edge.x * edge.y * (1.0 - smoothstep(0.6, 1.0, hi / maxDist)) * (1.0 - towardCamera);
    vec3 reflected = texture(u_source, huv).rgb;
    o_color = vec4(scene.rgb + reflected * clamp(reflectivity, 0.0, 1.0) * fade, 1.0);
}
)";

/// Temporal anti-aliasing: the history (last output) is fetched where this pixel was last frame
/// (reprojected through the depth), clamped to the 3 x 3 neighborhood of the new frame (no
/// ghosting of moved objects), then blended with the new, jittered frame.
inline constexpr const char* kTaa = R"(
uniform sampler2D u_source;   // new frame (jittered)
uniform sampler2D u_depth;    // its depth
uniform sampler2D u_history;  // last output
layout(location = 0) out vec4 o_color;
vec2 uvToNdc(vec2 uv) {
    vec2 n = uv * 2.0 - 1.0;
    if (u_ao.w < 0.5) n.y = -n.y;
    return n;
}
vec2 ndcToUv(vec2 n) {
    if (u_ao.w < 0.5) n.y = -n.y;
    return n * 0.5 + 0.5;
}
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec4 cur = texture(u_source, uv);
    if (u_taa.y < 0.5) {
        o_color = cur;
        return;
    }
    float d = texture(u_depth, uv).r;
    float z = u_ao.w > 0.5 ? d * 2.0 - 1.0 : d;
    vec4 w = u_invViewProj * vec4(uvToNdc(uv), z, 1.0);
    w /= w.w;
    vec4 pc = u_prevViewProj * vec4(w.xyz, 1.0);
    vec2 puv = ndcToUv(pc.xy / pc.w);
    if (pc.w <= 0.0 || puv.x < 0.0 || puv.y < 0.0 || puv.x > 1.0 || puv.y > 1.0) {
        o_color = cur;
        return;
    }
    vec3 mn = cur.rgb, mx = cur.rgb;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) {
            vec3 c = texture(u_source, uv + vec2(float(x), float(y)) * u_texel.xy).rgb;
            mn = min(mn, c);
            mx = max(mx, c);
        }
    vec3 h = clamp(texture(u_history, puv).rgb, mn, mx);
    o_color = vec4(mix(h, cur.rgb, u_taa.x), cur.a);  // alpha (SSR reflectivity) from the new frame
}
)";

/// Camera motion blur: the pixel's world position (depth) projected with last frame's camera gives
/// its motion on screen; 12 samples along it (scaled by the shutter u_taa.z, at most u_taa.w px).
inline constexpr const char* kMotionBlur = R"(
uniform sampler2D u_source;
uniform sampler2D u_depth;
layout(location = 0) out vec4 o_color;
vec2 uvToNdc(vec2 uv) {
    vec2 n = uv * 2.0 - 1.0;
    if (u_ao.w < 0.5) n.y = -n.y;
    return n;
}
vec2 ndcToUv(vec2 n) {
    if (u_ao.w < 0.5) n.y = -n.y;
    return n * 0.5 + 0.5;
}
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec4 cur = texture(u_source, uv);
    float d = texture(u_depth, uv).r;
    float z = u_ao.w > 0.5 ? d * 2.0 - 1.0 : d;
    vec4 w = u_invViewProj * vec4(uvToNdc(uv), z, 1.0);
    w /= w.w;
    vec4 pc = u_prevViewProj * vec4(w.xyz, 1.0);
    if (u_taa.z <= 0.0 || pc.w <= 0.0) {
        o_color = cur;
        return;
    }
    vec2 velocity = (uv - ndcToUv(pc.xy / pc.w)) * u_taa.z;  // uv units this frame moved
    float lengthPx = length(velocity / u_texel.xy);
    if (lengthPx < 0.5) {
        o_color = cur;
        return;
    }
    velocity *= min(1.0, u_taa.w / lengthPx);
    vec3 sum = cur.rgb;
    const int kSamples = 12;
    for (int i = 1; i < kSamples; ++i) {
        float t = float(i) / float(kSamples - 1) - 0.5;
        sum += texture(u_source, uv + velocity * t).rgb;
    }
    o_color = vec4(sum / float(kSamples), cur.a);
}
)";

/// Auto exposure: average log luminance of a 16 x 16 grid of the scene, blended toward the
/// previous frame's value (u_history) by u_adapt.x. Output in r of a 1 x 1 target.
inline constexpr const char* kLuminance = R"(
uniform sampler2D u_source;   // scene color (linear)
uniform sampler2D u_history;  // previous adapted value
layout(location = 0) out vec4 o_color;
void main() {
    float sum = 0.0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            vec3 c = textureLod(u_source, (vec2(float(x), float(y)) + 0.5) / 16.0, 0.0).rgb;
            sum += log(max(dot(c, vec3(0.2126, 0.7152, 0.0722)), 1e-4));
        }
    float measured = sum / 256.0;
    float previous = texture(u_history, vec2(0.5)).r;
    float v = u_adapt.y > 0.5 ? mix(previous, measured, u_adapt.x) : measured;
    o_color = vec4(v, 0.0, 0.0, 1.0);
}
)";

/// Atmosphere (before bloom and tone mapping): height fog integrated along each view ray (denser
/// low down, thinning with height), light scattered around the sun, and light shafts: from each
/// pixel toward the sun on screen, the share of open sky (depth = far) seen through the scene.
inline constexpr const char* kAtmosphere = R"(
uniform sampler2D u_source;  // scene color
uniform sampler2D u_depth;   // scene depth
layout(location = 0) out vec4 o_color;
bool isSky(float d) { return d >= 0.99999; }
vec3 worldAt(vec2 uv, float d) {
    float z = u_ao.w > 0.5 ? d * 2.0 - 1.0 : d;
    vec2 ndc = uv * 2.0 - 1.0;
    if (u_ao.w < 0.5) ndc.y = -ndc.y;  // Vulkan: framebuffer row 0 is the top (clip y = +1)
    vec4 p = u_invViewProj * vec4(ndc, z, 1.0);
    return p.xyz / p.w;
}
void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.zw;
    vec3 col = texture(u_source, uv).rgb;
    float d = texture(u_depth, uv).r;
    bool sky = isSky(d);
    vec3 cam = u_camera.xyz;
    vec3 far = worldAt(uv, sky ? 0.5 : d);  // the sky: any point along the ray gives its direction
    vec3 dir = normalize(far - cam);
    float dist = sky ? u_camera.w : length(far - cam);
    float toSun = max(dot(dir, u_sunDir.xyz), 0.0);
    if (u_fogColor.w > 0.5) {
        float b = max(u_fog.z, 1e-4);
        float y0 = cam.y - u_fog.y;
        float dy = dir.y;
        // Integral of density * exp(-b * height) along the ray.
        float amount = abs(dy * dist) < 1e-3 ? exp(-b * y0) * dist : exp(-b * y0) * (1.0 - exp(-b * dy * dist)) / (b * dy);
        float f = clamp(1.0 - exp(-u_fog.x * max(amount, 0.0)), 0.0, 1.0);
        vec3 fogCol = u_fogColor.rgb + u_sun.rgb * pow(toSun, 8.0) * u_fog.w;
        col = mix(col, fogCol, f);
    }
    if (u_sunDir.w > 0.5 && u_sun.w > 0.5) {
        const int kSteps = 40;
        vec2 delta = (u_shafts.xy - uv) / float(kSteps);
        vec2 p = uv;
        float weight = 1.0, sum = 0.0, total = 0.0;
        for (int i = 0; i < kSteps; ++i) {
            p += delta;
            float open = (p.x < 0.0 || p.y < 0.0 || p.x > 1.0 || p.y > 1.0) ? 0.0 : (isSky(texture(u_depth, p).r) ? 1.0 : 0.0);
            sum += open * weight;
            total += weight;
            weight *= u_shafts.w;
        }
        float shafts = sum / max(total, 1e-4);
        // Radial falloff around the sun on screen (aspect-corrected), and only when the camera
        // looks roughly toward it; occluders leave dark streaks in the open sky light.
        vec2 rel = (u_shafts.xy - uv) * vec2(u_texel.w / u_texel.z, 1.0);
        float radial = exp(-length(rel) * 2.2);
        vec3 forward = normalize(worldAt(vec2(0.5), 0.5) - cam);
        float facing = clamp(dot(forward, u_sunDir.xyz) * 1.4, 0.0, 1.0);
        col += u_sun.rgb * shafts * u_shafts.z * radial * facing * 0.6;
    }
    o_color = vec4(col, 1.0);
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
