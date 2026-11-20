#pragma once
#include <string>
#include <vector>

#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

enum class Projection : i32 { Perspective = 0, Orthographic = 1 };

/// How an orthographic camera adapts to screens of different shapes (phones!).
/// The "design area" is 2*orthographicSize high and referenceAspect times as wide.
enum class ScaleMode : i32 {
    FixedHeight = 0,  ///< Same visible height everywhere; width varies with the screen.
    FixedWidth = 1,   ///< Same visible width everywhere; height varies.
    FitInside = 2,    ///< The whole design area is always visible (bands of extra world on some screens).
    Crop = 3,         ///< The screen is always filled; part of the design area may be cut.
};

/// Renders the scene from the entity's world transform (looks down -Z).
struct CameraComponent {
    Projection projection = Projection::Perspective;
    float fieldOfView = math::radians(60.0f);  ///< Vertical, radians.
    float orthographicSize = 5.0f;             ///< Half of the visible height, world units.
    float nearPlane = 0.1f;
    float farPlane = 500.0f;
    math::Color clearColor{0.055f, 0.063f, 0.075f, 1.0f};  ///< sRGB. Default: AGR graphite-950.
    bool primary = true;                                   ///< The first active primary camera renders.
    ScaleMode scaleMode = ScaleMode::FixedHeight;          ///< Orthographic cameras only.
    float referenceAspect = 16.0f / 9.0f;                  ///< Width / height of the design area.
};

/// Visible orthographic half extents for a screen aspect ratio (see ScaleMode).
math::Vec2 orthographicHalfExtents(ScaleMode mode, float orthographicSize, float referenceAspect, float screenAspect);

/// Sun-like light shining along the entity's forward axis (-Z). The first active
/// one lights the scene and, when castShadows is on, casts shadows (shadow map).
struct DirectionalLightComponent {
    math::Color color{1.0f, 0.97f, 0.92f, 1.0f};
    float intensity = 1.0f;
    bool castShadows = true;
    float shadowDistance = 40.0f;  ///< Shadows are drawn up to this distance from the camera.
    float shadowStrength = 1.0f;   ///< 0 = no darkening, 1 = full shadow.
};

/// Light radiating in every direction from the entity, up to `range` meters.
struct PointLightComponent {
    math::Color color{1.0f, 0.95f, 0.85f, 1.0f};
    float intensity = 5.0f;
    float range = 10.0f;
    /// Shadows in every direction (6 shadow views): the nearest shadowed point light gets them;
    /// profiles without shadows skip them.
    bool castShadows = false;
};

/// Cone of light along the entity's forward axis (-Z).
struct SpotLightComponent {
    math::Color color{1.0f, 0.95f, 0.85f, 1.0f};
    float intensity = 10.0f;
    float range = 15.0f;
    float innerAngle = math::radians(20.0f);  ///< Half angle of the fully lit cone.
    float outerAngle = math::radians(30.0f);  ///< Half angle where the light reaches zero.
    /// Shadows of the objects in the cone (the 2 nearest shadowed spot lights get them; half the
    /// directional shadow map resolution; profiles without shadows skip them).
    bool castShadows = false;
};

enum class SkyMode : i32 {
    CameraColor = 0,  ///< Background = the camera clear color; flat ambient light.
    Gradient = 1,     ///< Procedural sky (zenith, horizon, ground, sun disc); hemisphere ambient light.
    Image = 2,        ///< Equirectangular image (.hdr, .png...): background, diffuse and reflected light.
};

/// Scene-wide ambient light, sky, fog and exposure (the first active one is used).
struct EnvironmentComponent {
    math::Color ambientColor{0.55f, 0.6f, 0.7f, 1.0f};  ///< Flat ambient (CameraColor sky).
    float ambientIntensity = 0.35f;
    SkyMode sky = SkyMode::CameraColor;
    math::Color skyColor{0.30f, 0.50f, 0.82f, 1.0f};
    math::Color horizonColor{0.78f, 0.84f, 0.90f, 1.0f};
    math::Color groundColor{0.30f, 0.28f, 0.26f, 1.0f};
    bool sunDisc = true;
    std::string skyTexture;     ///< Image sky: equirectangular (2:1) image, e.g. assets://sky/day.hdr.
    float skyIntensity = 1.0f;  ///< Image sky brightness (background and lighting).
    float skyRotation = 0.0f;   ///< Image sky: turn around the vertical axis (radians).
    bool fog = false;
    math::Color fogColor{0.75f, 0.80f, 0.86f, 1.0f};
    float fogStart = 20.0f;
    float fogEnd = 150.0f;
    float exposure = 1.0f;  ///< Multiplier before tone mapping (ACES).
};

/// Animated water surface (local XZ plane, centered on the entity): waves, sky reflection
/// (Fresnel), sun highlight, transparency from shallow to deep color. Visual only.
struct WaterComponent {
    math::Vec2 size{40.0f, 40.0f};
    u32 resolution = 96;            ///< Grid cells per side (vertex waves need enough of them).
    math::Color shallowColor{0.20f, 0.55f, 0.60f, 1.0f};
    math::Color deepColor{0.02f, 0.12f, 0.22f, 1.0f};
    float opacity = 0.75f;          ///< Seen from above; grazing angles reflect the sky.
    float waveHeight = 0.15f;       ///< Meters.
    float waveLength = 6.0f;        ///< Meters, longest wave.
    float waveSpeed = 1.0f;
    float waveDirection = 0.0f;     ///< Radians around Y.
    float reflectivity = 1.0f;
};

/// Full-screen effects of the camera's image (first active one in the scene). Rendering
/// then goes through an HDR target (when the GPU supports it) and a composite pass.
struct PostProcessComponent {
    bool bloom = true;              ///< Glow around bright areas (emissive materials, sun on metal...).
    float bloomIntensity = 0.6f;
    float bloomThreshold = 1.0f;    ///< Linear brightness above which pixels glow.
    float bloomRadius = 1.0f;       ///< Spread of the glow.
    bool fxaa = true;               ///< Anti-aliasing filter on edges (cheap; complements or replaces MSAA).
    bool filmicToneMapping = true;  ///< ACES curve (3D); off: colors are kept as they are (2D, flat art).
    float contrast = 1.0f;
    float saturation = 1.0f;
    math::Color colorFilter{1.0f, 1.0f, 1.0f, 1.0f};
    float vignette = 0.2f;          ///< Darkened corners, 0..1.
    /// Screen-space ambient occlusion: contact shadows in corners, creases and under objects
    /// (half resolution; turns MSAA off for this camera, FXAA smooths the edges instead).
    bool ambientOcclusion = false;
    float aoRadius = 0.5f;          ///< World units around each point.
    float aoIntensity = 1.0f;       ///< 0..2.
};

enum class PrimitiveMesh : i32 { Cube = 0, Sphere = 1, Plane = 2, Quad = 3 };

/// Draws a built-in mesh or one mesh of an imported model with PBR materials.
/// Material choice: `material` (.agrmat) if set, else the model's own materials,
/// else a built-in material made of texture / metallic / roughness. `color` tints
/// all of them (per instance: instancing still applies).
struct MeshRendererComponent {
    PrimitiveMesh mesh = PrimitiveMesh::Cube;
    std::string texture;               ///< Built-in material: base color texture. Empty = none.
    math::Color color{1.0f, 1.0f, 1.0f, 1.0f};  ///< sRGB tint.
    math::Vec2 uvScale{1.0f, 1.0f};    ///< Texture tiling.
    bool visible = true;
    std::string model;                 ///< Imported model (.gltf, .glb, .obj, .fbx). Empty = built-in mesh.
    i32 meshIndex = 0;                 ///< Mesh of the model.
    i32 skin = -1;                     ///< Skin of the model deforming the mesh (-1 = none); joints are found by name below the model's root.
    std::string material;              ///< Optional .agrmat replacing every material of the mesh.
    float metallic = 0.0f;             ///< Built-in material.
    float roughness = 0.5f;            ///< Built-in material.
    bool castShadows = true;
    /// Cartoon lighting (ShadingModel::Toon) for every material of the mesh, with the toon
    /// defaults of MaterialData; an .agrmat set to Toon keeps its own toon settings.
    bool toon = false;
    /// Automatic level of detail of imported models: simplified versions are used when the object
    /// is small on screen. > 1 keeps the detail longer, < 1 simplifies sooner, 0 = always full detail.
    float lodBias = 1.0f;
    /// Hidden beyond this distance from the camera (shadows included); 0 = always drawn.
    float visibilityRange = 0.0f;
    float outlineWidth = 0.0f;         ///< Outline in pixels for every material of the mesh (0: the material's).
    math::Color outlineColor{0.1f, 0.07f, 0.12f, 1.0f};
};

/// Soft round shadow on the ground under the entity: the cheap "blob" shadow of mobile games,
/// readable at any distance and independent of the shadow map. Drawn on the horizontal plane
/// y = groundHeight (world) below the entity's origin; it shrinks and fades as the entity rises.
struct BlobShadowComponent {
    float radius = 0.5f;               ///< World units, on the ground.
    float opacity = 0.45f;
    float softness = 0.6f;             ///< Fraction of the radius that fades out (0: hard disc).
    float groundHeight = 0.0f;         ///< World height of the ground the shadow lies on.
    float fadeHeight = 4.0f;           ///< Height above the ground where the shadow has faded out.
    math::Color color{0.12f, 0.09f, 0.18f, 1.0f};
};

/// Hides what is behind it from the renderer (occlusion culling): a simple box or rectangle
/// placed on walls, buildings, rocks or hills. Objects entirely behind occluders are not drawn
/// (their shadows still are). The shape should stay inside the visible geometry, or objects
/// seen around its edges could be hidden. Not drawn itself.
struct OccluderComponent {
    enum class Shape : i32 { Box = 0, Quad = 1 };
    Shape shape = Shape::Box;
    math::Vec3 size{1.0f, 1.0f, 1.0f};  ///< Full size in local units (Quad: x and y, in the XY plane).
    bool enabled = true;
};

/// Textured quad in the entity's XY plane. Sorted by layer, then order, then depth.
struct SpriteRendererComponent {
    std::string texture;
    math::Color color{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec2 size{1.0f, 1.0f};         ///< World units.
    math::Vec2 pivot{0.5f, 0.5f};        ///< 0..1, (0,0) = bottom-left.
    math::Vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};  ///< u0, v0, u1, v1 (v = 0 is the top row): sprite sheets.
    bool flipX = false;
    bool flipY = false;
    i32 sortingLayer = 0;
    i32 orderInLayer = 0;
    bool pixelArt = false;               ///< Nearest filtering for crisp pixels.
    bool visible = true;
    std::string sheet;                   ///< Optional sprite sheet (.agrsheet): overrides texture / uvRect, and a
                                         ///< valid frame overrides pivot (frames of a clip may differ).
    i32 frame = -1;                      ///< Frame of the sheet (-1 = none).
    bool lit = false;                    ///< Affected by 2D lights (Light2D).
    /// Normal map of the texture (same layout, OpenGL convention: green up), lit sprites only:
    /// the lights shade the relief (bricks, folds, scales) from their height (Light2D.height).
    std::string normalMap;
};

/// Root of a 2D skeleton: the Bone2D entities below it deform the Skin2D parts below it.
struct Skeleton2DComponent {
    bool showBones = true;  ///< Editor: draws the bones over the scene view.
};

/// A bone of a 2D skeleton. It points along its local +X axis; its children (other bones,
/// sprites attached rigidly) follow it. The rest pose is the pose the skins were drawn in:
/// in the editor (edit mode) it follows the bone; animations and IK move the bone away from it.
struct Bone2DComponent {
    float length = 1.0f;                ///< World units along +X (weights, IK, editor drawing).
    bool restSet = false;               ///< False: the current local transform is the rest pose.
    math::Vec3 restPosition{0.0f};
    math::Quat restRotation{};
    math::Vec3 restScale{1.0f};
};

/// A textured mesh deformed by the bones of the enclosing Skeleton2D (smooth bending of
/// bodies, tails, cloth, faces): a grid of columns x rows cells over the picture, whose points
/// follow the nearby bones (automatic weights from the distance to each bone at rest).
/// Without a Skeleton2D above it, it is drawn like a sprite.
struct Skin2DComponent {
    std::string texture;
    std::string normalMap;               ///< Lit skins: relief (see SpriteRenderer.normalMap).
    math::Color color{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec2 size{1.0f, 1.0f};         ///< World units, in the entity's XY plane.
    math::Vec2 pivot{0.5f, 0.5f};
    math::Vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
    i32 columns = 8;                     ///< Grid resolution: more cells = smoother bends.
    i32 rows = 4;
    float stiffness = 4.0f;              ///< Exponent of the distance weights: higher = rigid parts, sharper joints.
    i32 sortingLayer = 0;
    i32 orderInLayer = 0;
    bool pixelArt = false;
    bool lit = false;
    bool visible = true;
};

/// Grid of tiles drawn from a sprite sheet (the tileset). Cell (x, y) covers
/// [x, x+1] x [y, y+1] * cellSize in the entity's local XY plane; y grows upward.
struct TilemapComponent {
    std::string tileset;                 ///< .agrsheet whose frames are the tiles.
    math::Vec2 cellSize{1.0f, 1.0f};
    u32 width = 16, height = 10;         ///< In cells.
    math::Color color{1.0f, 1.0f, 1.0f, 1.0f};
    i32 sortingLayer = 0;
    i32 orderInLayer = 0;
    bool lit = false;
    std::string normalMap;               ///< Normal map laid out like the tileset's texture (lit tilemaps).
    u32 dataWidth = 16;                  ///< Width the tile data is laid out with (resizes are remapped).
    std::vector<i32> tiles;              ///< Frame index per cell (row-major from y = 0), -1 = empty.

    i32 tileAt(u32 x, u32 y) const {
        const usize i = static_cast<usize>(y) * dataWidth + x;
        return x < width && y < height && x < dataWidth && i < tiles.size() ? tiles[i] : -1;
    }
};

/// Point light for lit sprites and tilemaps (2D).
struct Light2DComponent {
    math::Color color{1.0f, 0.85f, 0.6f, 1.0f};
    float intensity = 1.5f;
    float radius = 5.0f;   ///< World units; no light beyond.
    float falloff = 1.5f;  ///< Exponent of (1 - distance / radius).
    /// Shadows of LightOccluder2D shapes (up to 8 shadowed lights per frame).
    bool castShadows = false;
    float shadowSoftness = 0.5f;  ///< Blur of the shadow edges, 0 = hard.
    float shadowStrength = 1.0f;  ///< 0..1: how dark the shadows are.
    /// World units the light enters occluders before the shadow starts: the faces of lit walls
    /// (tiles, crates) stay lit instead of turning black behind their edge.
    float shadowPenetration = 0.5f;
    /// Height of the light above the sprites, for normal-mapped sprites: low = grazing light
    /// (strong relief, long falloff), high = light from the front.
    float height = 1.0f;
};

/// Blocks 2D light (Light2D.castShadows): a box in the entity's XY plane, or, on an entity
/// with a Tilemap, every non-empty cell.
struct LightOccluder2DComponent {
    math::Vec2 size{1.0f, 1.0f};
    math::Vec2 offset{0.0f, 0.0f};
};

/// Makes the entity and its children scroll at a different speed than the
/// world when the camera moves: 1 = like the world, 0 = attached to the camera,
/// between = distant layer. Applied at render time (transforms are unchanged).
struct ParallaxComponent {
    math::Vec2 factor{0.5f, 0.5f};
};

/// 2D particle emitter, simulated by the 2D module (simulateParticles2D, TwoDPlugin).
struct ParticleEmitter2DComponent {
    bool emitting = true;
    bool loop = true;
    float duration = 2.0f;          ///< Seconds per cycle.
    float rate = 30.0f;             ///< Particles per second.
    u32 burst = 0;                  ///< Emitted at the start of each cycle.
    u32 maxParticles = 500;
    float lifetimeMin = 0.8f, lifetimeMax = 1.6f;
    float speedMin = 1.0f, speedMax = 3.0f;
    float direction = math::radians(90.0f);  ///< Radians, 0 = +X, 90° = up.
    float spread = math::radians(30.0f);     ///< Half angle.
    float spawnRadius = 0.0f;
    math::Vec2 gravity{0.0f, -2.0f};
    float drag = 0.0f;              ///< Velocity damping per second.
    math::Color startColor{1.0f, 1.0f, 1.0f, 1.0f};
    math::Color endColor{1.0f, 1.0f, 1.0f, 0.0f};
    float startSize = 0.25f, endSize = 0.05f;
    float angularVelocity = 0.0f;   ///< Radians per second (random sign).
    std::string texture;            ///< Empty = soft round particle.
    std::string curves;             ///< Over-lifetime size curve and color gradient (.agrparticle); empty = start / end values.
    i32 flipbookColumns = 1;        ///< Texture cut in columns x rows frames played over the lifetime (smoke, explosions).
    i32 flipbookRows = 1;
    float flipbookCycles = 1.0f;    ///< How many times the frames play during a particle's life.
    bool additive = false;
    bool worldSpace = true;         ///< Particles stay where emitted when the emitter moves.
    i32 sortingLayer = 0;
    i32 orderInLayer = 0;
    u32 seed = 0;                   ///< 0 = random; otherwise deterministic.
};

/// Runtime particles (not serialized), written by the simulation, read by the renderer.
struct Particle2D {
    math::Vec2 position, velocity;
    float z = 0.0f;
    float age = 0.0f, lifetime = 1.0f;
    float rotation = 0.0f, angularVelocity = 0.0f;
};
struct ParticleState2DComponent {
    std::vector<Particle2D> particles;
    float time = 0.0f;
    float emitAccumulator = 0.0f;
    u32 rng = 0;
    bool started = false;
};

enum class EmitterShape3D : i32 { Point = 0, Sphere = 1, Box = 2, Cone = 3 };

/// 3D particle emitter (camera-facing billboards), simulated by the 2D / effects module
/// (simulateParticles3D). Emits along its local +Y axis.
struct ParticleEmitter3DComponent {
    bool emitting = true;
    bool loop = true;
    float duration = 2.0f;
    float rate = 40.0f;             ///< Particles per second.
    u32 burst = 0;                  ///< Emitted at the start of each cycle.
    u32 maxParticles = 800;
    float lifetimeMin = 0.8f, lifetimeMax = 1.6f;
    float speedMin = 1.0f, speedMax = 3.0f;
    EmitterShape3D shape = EmitterShape3D::Cone;
    float shapeRadius = 0.3f;       ///< Sphere, cone base.
    math::Vec3 boxSize{1.0f, 0.1f, 1.0f};
    float coneAngle = math::radians(25.0f);  ///< Half angle around +Y (cone); sphere / box emit outward / up.
    math::Vec3 gravity{0.0f, 0.0f, 0.0f};    ///< World units / s^2.
    float drag = 0.0f;
    math::Color startColor{1.0f, 1.0f, 1.0f, 1.0f};
    math::Color endColor{1.0f, 1.0f, 1.0f, 0.0f};
    float startSize = 0.3f, endSize = 0.1f;
    float angularVelocity = 0.0f;   ///< Radians / s, random sign.
    std::string texture;            ///< Empty = soft round particle.
    std::string curves;             ///< Over-lifetime size curve and color gradient (.agrparticle); empty = start / end values.
    i32 flipbookColumns = 1;        ///< Texture cut in columns x rows frames played over the lifetime (smoke, explosions).
    i32 flipbookRows = 1;
    float flipbookCycles = 1.0f;    ///< How many times the frames play during a particle's life.
    bool additive = false;
    bool worldSpace = true;
    u32 seed = 0;                   ///< 0 = random.
};
struct Particle3D {
    math::Vec3 position, velocity;
    float age = 0.0f, lifetime = 1.0f;
    float rotation = 0.0f, angularVelocity = 0.0f;
};
struct ParticleState3DComponent {
    std::vector<Particle3D> particles;
    float time = 0.0f;
    float emitAccumulator = 0.0f;
    u32 rng = 0;
    bool started = false;
};

/// Registers reflection data for the render components (idempotent).
void registerRenderComponents();

} // namespace agr
