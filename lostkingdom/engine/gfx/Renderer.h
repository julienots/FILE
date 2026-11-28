// AGR Engine — forward renderer tuned for a 2.5D isometric-perspective camera on mobile GPUs.
#pragma once
#include "GL.h"
#include "Textures.h"
#include <vector>
#include <map>

namespace agr {

enum class QualityProfile { Ultra = 0, High, Medium, Low, Battery };
const char* profileName(QualityProfile p);

struct GraphicsSettings {
    QualityProfile profile = QualityProfile::High;
    float renderScale = 0.85f;
    int shadowRes = 2048;      // 0 = shadows off
    bool softShadows = true;
    bool bloom = true;
    bool fxaa = true;
    bool dof = true;           // cinematic depth of field + tilt-shift
    bool terrainNormals = true;
    bool chromatic = false;    // optional chromatic aberration
    bool ssao = true;          // screen-space ambient occlusion (contact shadows, corners, grass)
    int shadowInterval = 1;    // redraw the shadow map every N frames (sun moves slowly)
    int bloomStart = 0;        // 0: bloom chain from half resolution, 1: from quarter resolution
    bool simpleWater = false;
    bool sunShafts = false;
    bool ssr = false;          // 1.12.0 screen-space reflections on water (trees, bridges, houses, characters)    // 1.11.1 volumetric sunlight (shadow-map ray march in the composite pass)  // one normal octave, no shadow lookup, no point lights on water
    float ssaoStrength = 0.8f;
    float grassDensity = 1.f;
    float particleScale = 1.f;
    float drawDistance = 110.f;
    int maxLights = 8;
    int fpsCap = 60;
    static GraphicsSettings forProfile(QualityProfile p);
};

// First-launch quality guess from the GL_RENDERER string (Adreno / Mali / PowerVR / Xclipse...).
QualityProfile suggestProfileForGpu(const std::string& glRenderer);

struct PointLight { vec3 pos; float radius; vec3 color; float flicker; };

struct Environment {
    vec3 sunDir{0.4f, 0.8f, 0.3f};
    vec3 sunColor{1.f, 0.95f, 0.85f};
    float sunIntensity = 3.f;
    float shadowStrength = 1.f;
    vec3 skyAmbient{0.35f, 0.42f, 0.55f};
    vec3 groundAmbient{0.22f, 0.2f, 0.15f};
    vec3 fogColor{0.6f, 0.7f, 0.8f};
    float fogDensity = 0.004f, fogFalloff = 0.05f, fogBase = 0.f;
    float wetness = 0.f, snowCover = 0.f, night = 0.f;
    vec2 windDir{1, 0};
    float windStrength = 0.5f, gust = 0.5f;
    float cloudShadows = 0.6f;
    float lightning = 0.f;
    vec3 skyTop{0.25f, 0.45f, 0.8f}, skyHorizon{0.7f, 0.8f, 0.9f};
    float cloudCover = 0.4f;
    // post / grading
    float exposure = 1.f, bloomIntensity = 0.06f, vignette = 0.35f, chromatic = 0.f;
    float saturation = 1.08f, contrast = 1.05f, temperature = 0.f, grain = 0.015f;
    vec3 lift{0, 0, 0}, gamma{1, 1, 1}, gain{1, 1, 1};
    float tiltShift = 0.f, dofAmount = 0.f, dofFocus = 25.f;
    vec3 fadeColor{0, 0, 0};
    float fade = 0.f;
    float damagePulse = 0.f;
    // 2.0.0 sky and post effects: aurora (cold regions at night), dust haze (desert, ash), rainbow, mirage shimmer, sand veil, lens flare
    float aurora = 0.f, dust = 0.f, rainbow = 0.f, heat = 0.f, sandVeil = 0.f, flare = 0.f;
    float scorch = 0.f;
    // 3.5.0 under the water: the surface level near the camera, caustics strength, camera below the surface (0..1),
    // the colour of that water and how deep the camera is
    float waterLevel = -1e9f, caustics = 0.f, underwater = 0.f, underDepth = 0.f;
    vec3 waterTint{0.1f, 0.35f, 0.42f};   // 3.4.0 the blazing midday sun: bleached highlights, ground heat haze, a starburst sun
    float paint = 1.f;      // 3.10.1 painterly grade: cool shadows, warm highlights, vibrance on muted colours (0 = off)
    float rift = 1.f, riftFlash = 0.f;   // 3.9.5 the sky breach: 0 closed .. 1 wide open; extra glow while it tears
    float space = 0.f;      // 3.2.0 outer space: starfield, nebulae, the planet below, a ringed giant (sky shader)
    float shafts = 0.35f;   // 1.11.1 sunlight scattering in the air (0 = off): light shafts through trees, eaves, smoke
    vec3 playerPos;
    float time = 0.f;  // animation time for shaders (s), owned by the game clock
    float grassPushRadius = 1.3f;   // > 0 also enables the see-through fade around the player
    // Grass benders (up to 8): xyz position, w = packPusher(strength -1..1, radius 0..4 m); w = 0: unused.
    vec4 pushers[8];
    static float packPusher(float strength, float radius) {
        float st = std::floor(std::min(1.f, std::fabs(strength)) * 64.f);
        if (st < 1.f) return 0.f;
        return (strength < 0 ? -1.f : 1.f) * (st + std::min(3.99f, std::max(0.f, radius)) / 4.f);
    }
};

// Shader variant flags for the lit uber-shader.
enum LitFlags : int {
    LIT_SKINNED = 1, LIT_INSTANCED = 2, LIT_WIND = 4, LIT_GRASS = 8, LIT_TERRAIN = 16,
    LIT_DISSOLVE = 32, LIT_OUTLINE = 64, LIT_DEPTH = 128, LIT_TEXTURED = 256,  // TEXTURED: albedo texture, UVs in the material bytes
    LIT_FAR = 512   // distant horizon relief: fragments closer than farClip.z to farClip.xy are discarded
};

struct DrawItem {
    const Mesh* mesh = nullptr;
    mat4 model;
    int flags = 0;
    const float* bones = nullptr;  // 16 floats per bone
    int boneCount = 0;
    vec4 tint{1, 1, 1, 0};         // rgb multiply, a snow
    vec4 fx{0, 0, 0, 0};           // flash, dissolve, rim, emissive boost
    vec3 dissolveColor{1.f, 0.5f, 0.1f};
    GLuint albedo = 0;             // LIT_TEXTURED: base colour texture (sRGB)
    bool castShadow = true;
    int instances = -1;            // 2.3.6 instanced: how many to draw (-1 = all)
    bool outline = false;
    vec4 outlineColor{0.05f, 0.03f, 0.02f, 1};
    float outlineWidth = 0.03f;
    float sortKey = 0;
    vec4 farClip{0, 0, 0, 0};      // LIT_FAR: x, z of the centre, z = radius drawn by the detailed chunks
};

// 3.0.1 shader effect (powers, spells, impacts): one procedural shape, additive, animated in the FX shader
enum FxShape { FX_DISC = 0, FX_CYLINDER = 1, FX_SPHERE = 2, FX_ARC = 3, FX_CONE = 4, FX_SHAPES = 5 };
struct FxItem {
    int shape = FX_DISC, style = 0;
    mat4 model;
    vec4 color{1, 1, 1, 1}, color2{1, 1, 1, 1};   // core, edge
    float age = 0, intensity = 1, seed = 0, time = 0;
};

struct WaterItem { const Mesh* mesh; mat4 model; vec3 deep, shallow; float frozen; bool lava; };

struct Camera {
    vec3 pos, target;
    float fovY = 32 * DEG, zNear = 1.f, zFar = 400.f;
    mat4 view, proj, viewProj;
    Frustum frustum;
    void update(float aspect);
};

struct RenderStats { int drawCalls = 0, triangles = 0, shadowCasters = 0; double cpuMs = 0; int triKind[8] = {}; };   // triKind: terrain, far, grass, foliage, models, skinned, props, other

class ParticleSystem;

class Renderer {
public:
    bool init(const GraphicsSettings& s);
    void shutdown();
    void applySettings(const GraphicsSettings& s);
    // Dynamic resolution: multiplier on settings.renderScale (1 = profile resolution). Rebuilds targets.
    void setDynamicScale(float k);
    float dynamicScale = 1.f;
    void resize(int w, int h);
    void begin(const Camera& cam, const Environment& env);
    void submit(const DrawItem& d) { items.push_back(d); }
    size_t itemCount() const { return items.size(); }
    void submitWater(const WaterItem& w) { waters.push_back(w); }
    void submitRibbon(const Mesh* m, float intensity) { ribbons.push_back({m, intensity}); }
    void submitFx(const FxItem& f) { fxItems.push_back(f); }
    void addLight(const PointLight& l) { lights.push_back(l); }
    void render(ParticleSystem* particles);
    // Draw an arbitrary set of items into an offscreen target (item icons, NPC portraits).
    void renderToTarget(RenderTarget& rt, const Camera& cam, const Environment& env, const std::vector<DrawItem>& list, vec4 clear);

    Shader* lit(int flags);
    Shader& uiShader() { return ui; }
    Texture noiseTex, terrainTex, terrainNrm, particleTex;
    Mesh quadMesh;  // unit quad (-0.5..0.5) for particles
    Mesh fsMesh;    // fullscreen triangle pair
    GraphicsSettings settings;
    RenderStats stats;
    // Per-pass GPU timing (glFinish between passes): debug only, enabled by the LK_GPU_PROFILE env variable.
    bool gpuProfile = false; int gpuSkipMask = 0; bool gpuSkipProps = false;   // debug: skip item classes to measure them
    double gpuAcc[12] = {}; int gpuFrames = 0;
    double gpuT0 = 0;
    void gpuMark(int pass);
    std::string gpuReport();
    int width = 1, height = 1;
    int shadowFrame = 0;
    Camera camera;
    Environment env;
    std::vector<PointLight> lights;

private:
    void updateFrameUBO(const Camera& cam, const Environment& env, const mat4& shadowMat, bool withShadow);
    void drawItem(const DrawItem& d, bool depthPass, bool outlinePass);
    void createTargets();
    void destroyTargets();
    void shadowPass();
    void postProcess();

    std::map<int, Shader> litVariants;
    Shader water, lava, sky, particleAlpha, particleAdd, ribbon, bloomDown, bloomUp, composite, fxaa, blit, ui, ssao, fxShader;
    Mesh fxMesh[FX_SHAPES];
    std::vector<FxItem> fxItems;
    void buildFxMeshes();
    GLuint frameUbo = 0;
    RenderTarget shadowRT, sceneRT, ldrRT, blurRT, aoRT;
    RenderTarget reflRT, depthCopyRT;   // 1.12.0 copies of the opaque scene (colour, half res) and depth for water reflections
    std::vector<RenderTarget> bloomChain;
    GLenum hdrFormat = GL_RGBA16F;
    float hdrScale = 1.f;  // when the HDR target is RGB10A2 we store colour / 4
    mat4 shadowMatrix;
    struct Ribbon { const Mesh* m; float intensity; };
    std::vector<DrawItem> items;
    std::vector<WaterItem> waters;
    std::vector<Ribbon> ribbons;
    bool shadowsThisFrame = false;
};

}  // namespace agr
