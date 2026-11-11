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
    float grassDensity = 1.f;
    float particleScale = 1.f;
    float drawDistance = 110.f;
    int maxLights = 8;
    int fpsCap = 60;
    static GraphicsSettings forProfile(QualityProfile p);
};

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
    vec3 playerPos;
    float time = 0.f;  // animation time for shaders (s), owned by the game clock
    float grassPushRadius = 1.3f;
};

// Shader variant flags for the lit uber-shader.
enum LitFlags : int {
    LIT_SKINNED = 1, LIT_INSTANCED = 2, LIT_WIND = 4, LIT_GRASS = 8, LIT_TERRAIN = 16,
    LIT_DISSOLVE = 32, LIT_OUTLINE = 64, LIT_DEPTH = 128
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
    bool castShadow = true;
    bool outline = false;
    vec4 outlineColor{0.05f, 0.03f, 0.02f, 1};
    float outlineWidth = 0.03f;
    float sortKey = 0;
};

struct WaterItem { const Mesh* mesh; mat4 model; vec3 deep, shallow; float frozen; bool lava; };

struct Camera {
    vec3 pos, target;
    float fovY = 32 * DEG, zNear = 1.f, zFar = 400.f;
    mat4 view, proj, viewProj;
    Frustum frustum;
    void update(float aspect);
};

struct RenderStats { int drawCalls = 0, triangles = 0, shadowCasters = 0; double cpuMs = 0; };

class ParticleSystem;

class Renderer {
public:
    bool init(const GraphicsSettings& s);
    void shutdown();
    void applySettings(const GraphicsSettings& s);
    void resize(int w, int h);
    void begin(const Camera& cam, const Environment& env);
    void submit(const DrawItem& d) { items.push_back(d); }
    void submitWater(const WaterItem& w) { waters.push_back(w); }
    void submitRibbon(const Mesh* m, float intensity) { ribbons.push_back({m, intensity}); }
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
    int width = 1, height = 1;
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
    Shader water, lava, sky, particleAlpha, particleAdd, ribbon, bloomDown, bloomUp, composite, fxaa, blit, ui;
    GLuint frameUbo = 0;
    RenderTarget shadowRT, sceneRT, ldrRT, blurRT;
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
