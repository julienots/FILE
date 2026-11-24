#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "agr/core/Types.hpp"
#include "agr/ecs/Entity.hpp"
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
    /// Height fog: denser near `fogHeight`, thinning upward (valleys, mornings, swamps), lit by
    /// the sun when looking toward it. Uses the scene depth (MSAA off, like ambientOcclusion).
    bool heightFog = false;
    float fogDensity = 0.05f;
    float fogHeight = 0.0f;         ///< World height where the fog has `fogDensity`.
    float fogFalloff = 0.3f;        ///< How fast it thins with height (per world unit).
    math::Color fogColor{0.68f, 0.74f, 0.84f, 1.0f};
    float fogSunScatter = 0.8f;     ///< Glow of the fog around the sun.
    /// Point and spot lights (the 8 nearest) light up the fog around them: halos, lamp posts in
    /// the mist, light seen through the air of a cave. 0 = off.
    float fogLightScatter = 1.0f;
    /// Light shafts (god rays) from the sun through openings: trees, windows, clouds of objects.
    bool lightShafts = false;
    float shaftIntensity = 0.6f;
    float shaftDecay = 0.96f;       ///< 0.9 = short rays, 0.99 = long rays.
    /// Depth of field: sharp around `focusDistance` (from the camera), blurred in front of and
    /// behind the focus zone like a camera lens (bokeh). Uses the scene depth (MSAA off).
    bool depthOfField = false;
    float focusDistance = 10.0f;    ///< World units from the camera.
    float focusRange = 4.0f;        ///< Depth of the sharp zone; the blur grows over the same distance beyond it.
    float dofBlur = 0.012f;         ///< Largest blur radius, as a fraction of the image height.
    bool dofNear = true;            ///< Blur what is in front of the focus zone.
    bool dofFar = true;             ///< Blur what is behind it.
    /// Screen-space reflections: smooth and metallic opaque surfaces reflect what is on screen
    /// (wet floors, polished metal, calm water), traced in the scene depth (MSAA off). Where
    /// the ray leaves the screen, the sky / reflection probes stay.
    /// Temporal anti-aliasing (perspective cameras): the projection moves by a fraction of a
    /// pixel each frame and the frames are blended along the camera motion; smooths edges and
    /// thin details better than FXAA, slight blur in motion.
    bool temporalAA = false;
    /// Motion blur from the camera's movement (turning, running, vehicles): each pixel is smeared
    /// along its motion on screen since the last frame. Static objects only (no per-object blur).
    bool motionBlur = false;
    float motionBlurIntensity = 0.5f;  ///< Fraction of the frame's motion (shutter), 0..1.
    float taaBlend = 0.1f;          ///< Weight of the new frame (lower = smoother, more ghosting).
    bool screenSpaceReflections = false;
    float ssrIntensity = 1.0f;      ///< 0..2.
    float ssrMaxDistance = 12.0f;   ///< World units a reflected ray travels at most.
    float ssrThickness = 0.4f;      ///< World depth of the surfaces a ray can hit (behind them: no hit).
    /// Auto exposure (HDR): brightness adapts to the average light seen, like an eye coming out
    /// of a cave. Multiplies the Environment exposure.
    bool autoExposure = false;
    float exposureTarget = 0.4f;    ///< Average brightness aimed at (0.4 = neutral).
    float exposureMin = 0.3f;       ///< Exposure limits (darkest scene: at most exposureMax).
    float exposureMax = 4.0f;
    float exposureSpeed = 2.0f;     ///< Adaptation speed (per second).
    /// Custom full-screen effect: a `.agrshader` with `void post(inout Post p)` (CRT, pixelation,
    /// outlines, distortion...), run after tone mapping, before FXAA. "" = none.
    std::string shader;
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
    /// Morph target (blend shape) weights of an imported mesh: targets 0..3 in `morph`, 4..7 in
    /// `morph2` (0 = rest shape, 1 = full target). Animated by imported clips and the timeline.
    math::Vec4 morph{0.0f, 0.0f, 0.0f, 0.0f};
    math::Vec4 morph2{0.0f, 0.0f, 0.0f, 0.0f};
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

/// Projects a texture onto the surfaces inside its box (bullet holes, stains, road marks,
/// footprints...): local -Y is the projection direction (the box hangs below the entity like a
/// light, `size` is its full size in local units, scaled by the transform). The texture replaces
/// the base color of the lit surfaces it covers before lighting, so it is lit and shadowed like
/// them. Up to 16 decals near the camera and 4 different textures per frame.
struct DecalComponent {
    std::string texture;                ///< Color + alpha image (empty: a plain colored square).
    math::Color color{1.0f, 1.0f, 1.0f, 1.0f};  ///< Tint; alpha = opacity.
    math::Vec3 size{2.0f, 2.0f, 2.0f};  ///< Box size (x, z: the image; y: projection depth).
    float normalFade = 0.3f;            ///< 0: projects on every slope; 1: only on surfaces facing the decal.
    float upperFade = 0.3f;             ///< Fraction of the box's upper half where the decal fades out.
    float lowerFade = 0.3f;             ///< Same for the lower half.
    float emission = 0.0f;              ///< Glow: the decal also adds its color as light (neon signs, lava).
    bool enabled = true;
};

/// Local reflections: the probe captures the scene around it (6 views, once or continuously)
/// and the shiny surfaces inside its box reflect that capture instead of the sky alone (a
/// metal ball reflects the room it is in, a wet floor the walls). Box projection makes the
/// reflections line up with the walls of a room the size of the box. Up to 8 probes.
struct ReflectionProbeComponent {
    enum class Update : i32 { Once = 0, Always = 1 };
    math::Vec3 size{10.0f, 6.0f, 10.0f};  ///< Box of influence (full size, local units, centered).
    float intensity = 1.0f;
    float blendDistance = 1.0f;          ///< Fades in from the box faces over this distance.
    bool boxProjection = true;           ///< Parallax correction (rooms, corridors); off for open areas.
    Update update = Update::Once;        ///< Once: when added or moved; Always: 6 frames per refresh.
    float maxDistance = 200.0f;          ///< Far plane of the capture.
    bool enabled = true;
    /// Indirect diffuse light (global illumination): surfaces in the box get the ambient light of
    /// the captured room (its most blurred level) instead of the sky's: a red wall tints what is
    /// next to it, a dark cellar stays dark under a bright sky. 0 = off.
    float diffuseGI = 1.0f;
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
    /// Sprite shader (.agrshader with `void sprite(inout Sprite s)`): outlines, dissolves, flashes,
    /// waves, palette swaps. Unlit sprites; empty = standard drawing.
    std::string shader;
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
    /// Animated tiles: "f0,f1,f2@fps;..." — every cell holding frame f0 cycles through the
    /// list at fps frames per second (all cells in step).
    std::string animatedTiles;
    /// Autotiling: "F;G;..." — each base frame starts a group of 16 frames laid out by a
    /// 4-neighbour bitmask (+y = 1, +x = 2, -y = 4, -x = 8). A cell holding any frame of a group
    /// is drawn with F + the mask of its neighbours of the same group: paint with F, the
    /// borders and corners follow. "F:47" = a 47-frame "blob" set using the 8 neighbours
    /// (inner corners too): frame F + index of the mask in blobTileMasks().
    /// "F:c" = a 16-frame **terrain transition** set by corners (sand over grass, water over
    /// shore): a corner belongs to the terrain when any of the 4 cells around it holds a frame of
    /// the set, and every painted cell touching it is drawn with F + its corner mask (up-left = 1,
    /// up-right = 2, down-left = 4, down-right = 8; F + 15 = all terrain). The other tile shows
    /// where no corner is terrain: paint the transition terrain over a base terrain.
    std::string autotile;

    i32 tileAt(u32 x, u32 y) const {
        const usize i = static_cast<usize>(y) * dataWidth + x;
        return x < width && y < height && x < dataWidth && i < tiles.size() ? tiles[i] : -1;
    }
};

/// The 47 distinct 8-neighbour masks of a blob tile set, increasing (bits: +y = 1, +x+y = 2,
/// +x = 4, +x-y = 8, -y = 16, -x-y = 32, -x = 64, -x+y = 128; a corner counts only with both of
/// its sides). Frame F + i of a "F:47" group is drawn for mask blobTileMasks()[i].
const std::array<u8, 47>& blobTileMasks();

/// Parsed Tilemap.animatedTiles / autotile for a tileset of `frameCount` frames.
struct TileRules {
    std::vector<i32> group;                   ///< Frame -> base frame of its autotile group, -1 = none.
    std::vector<u8> blob;                     ///< Frame -> 1 when its group is a 47-tile (8 neighbours) set.
    std::vector<i32> cornerSets;              ///< Base frames of "F:c" corner transition sets.
    std::vector<i32> anim;                    ///< Frame -> index in `sequences`, -1 = static.
    std::vector<std::vector<i32>> sequences;  ///< Frames of each animation.
    std::vector<float> fps;
    bool empty() const { return sequences.empty() && group.empty(); }

    static TileRules parse(const TilemapComponent& tm, usize frameCount);
    /// Frame drawn for cell (x, y) at `time` seconds (-1 = empty).
    i32 resolve(const TilemapComponent& tm, u32 x, u32 y, double time) const;
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
    /// Directional (sun/moon): lights everything in the direction of the entity's local +X axis,
    /// with no radius and no falloff (radius, falloff and shadows are ignored).
    bool directional = false;
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
/// When a sub-emitter bursts: where a particle dies, or where it hits a collider / the ground.
enum class SubEmitTrigger : i32 { Death = 0, Collision = 1 };

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
    /// Where particles start: inside the spawnRadius disc (0), on its circle (1), in a spawnSize
    /// rectangle (2), or along a spawnSize.x long line (3, waterfalls, rain from a ledge).
    i32 spawnShape = 0;
    math::Vec2 spawnSize{1.0f, 1.0f};
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
    float velocityStretch = 0.0f;   ///< Stretches particles along their motion: + speed x this (seconds) in length (sparks, rain).
    // Collisions (world-space emitters): the ground plane and the ParticleCollider shapes.
    bool collisions = false;
    bool groundPlane = true;        ///< Collide with the horizontal plane y = groundHeight (world).
    float groundHeight = 0.0f;
    float bounce = 0.4f;            ///< Speed kept along the normal after a hit (0 = stops, 1 = elastic).
    float collisionFriction = 0.2f; ///< Speed lost along the surface at each hit.
    bool dieOnCollision = false;
    bool affectedByAttractors = true;  ///< ParticleAttractor pull / push (world-space emitters).
    /// Turbulence: a swirling, divergence-free noise field (world units / s^2) that wanders over
    /// time — smoke curls, magic swirls, leaves. 0 = off.
    float turbulence = 0.0f;
    float turbulenceScale = 1.0f;   ///< Size of the swirls (world units).
    float turbulenceSpeed = 0.5f;   ///< How fast the field changes.
    /// Fraction of the emitter's own velocity given to new particles (world space): a moving
    /// torch or rocket leaves a trail that keeps going.
    float inheritVelocity = 0.0f;
    /// Another emitter of the same kind (usually not emitting itself) that bursts `subEmitCount`
    /// particles where a particle dies or hits something (explosions throwing sparks, splashes).
    ecs::Entity subEmitter{};
    SubEmitTrigger subEmitOn = SubEmitTrigger::Death;
    i32 subEmitCount = 8;
    bool lit = false;               ///< Lit by Light2D (and their shadows) like lit sprites; not with additive.
    bool additive = false;
    bool worldSpace = true;         ///< Particles stay where emitted when the emitter moves.
    i32 sortingLayer = 0;
    i32 orderInLayer = 0;
    u32 seed = 0;                   ///< 0 = random; otherwise deterministic.
    /// Simulated on the GPU (see ParticleEmitter3D::gpu): hundreds of thousands of particles for
    /// almost no CPU. Uses rate, maxParticles, lifetime, speed, direction / spread, spawn shape,
    /// gravity, drag, colors, sizes, rotation, texture, additive, emitting, loop / duration, seed;
    /// particles follow the emitter. Not with curves, flipbook, collisions, attractors, turbulence,
    /// sub-emitters, bursts, lit, velocity stretch; drawn after the other sprites (no sorting layer).
    bool gpu = false;
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
    math::Vec3 lastEmitterPos{0.0f};  ///< For inheritVelocity.
    float clock = 0.0f;               ///< Seconds since start (turbulence field time).
    double gpuTime = 0.0;             ///< GPU emitters (see ParticleState3DComponent).
    double gpuSpawnStart = 0.0, gpuSpawnEnd = 1e30;
    bool gpuWasEmitting = true;
    u32 gpuAlive = 0;
};

enum class EmitterShape3D : i32 { Point = 0, Sphere = 1, Box = 2, Cone = 3, Ring = 4 };

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
    bool emitFromSurface = false;   ///< Sphere / box: start on the surface instead of inside (shells, auras).
    bool alignToVelocity = false;   ///< Mesh particles: point their local -Z along their motion (arrows, sparks, debris).
    float shapeRadius = 0.3f;       ///< Sphere, cone base, ring (circle in the local XZ plane, emits within coneAngle of +Y).
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
    float velocityStretch = 0.0f;   ///< Stretches particles along their motion: + speed x this (seconds) in length (sparks, rain).
    // Collisions (world-space emitters): the ground plane and the ParticleCollider shapes.
    bool collisions = false;
    bool groundPlane = true;        ///< Collide with the horizontal plane y = groundHeight (world).
    float groundHeight = 0.0f;
    float bounce = 0.4f;            ///< Speed kept along the normal after a hit (0 = stops, 1 = elastic).
    float collisionFriction = 0.2f; ///< Speed lost along the surface at each hit.
    bool dieOnCollision = false;
    bool affectedByAttractors = true;  ///< ParticleAttractor pull / push (world-space emitters).
    /// Turbulence: a swirling, divergence-free noise field (world units / s^2) that wanders over
    /// time — smoke curls, magic swirls, leaves. 0 = off.
    float turbulence = 0.0f;
    float turbulenceScale = 1.0f;   ///< Size of the swirls (world units).
    float turbulenceSpeed = 0.5f;   ///< How fast the field changes.
    /// Fraction of the emitter's own velocity given to new particles (world space): a moving
    /// torch or rocket leaves a trail that keeps going.
    float inheritVelocity = 0.0f;
    /// Lit by the scene: ambient, the directional light (half-Lambert facing the camera) and
    /// point / spot lights in range, evaluated per particle (smoke and dust in a dark cave).
    bool lit = false;
    /// Mesh particles: each particle is a small lit 3D mesh (debris, rocks, leaves, coins) that
    /// tumbles with its rotation, instead of a camera-facing billboard. `particleMesh` = a built-in
    /// shape, or `particleModel` (first mesh of an imported model) when set; size = scale.
    bool meshParticles = false;
    PrimitiveMesh particleMesh = PrimitiveMesh::Cube;
    std::string particleModel;
    float meshMetallic = 0.0f;
    float meshRoughness = 0.6f;
    bool meshCastShadows = true;
    /// Mesh particles: a material (.agrmat: textures, PBR values, custom shader, transparency)
    /// instead of the built-in one; the particle color still tints it.
    std::string particleMaterial;
    /// Another emitter of the same kind (usually not emitting itself) that bursts `subEmitCount`
    /// particles where a particle dies or hits something (explosions throwing sparks, splashes).
    ecs::Entity subEmitter{};
    SubEmitTrigger subEmitOn = SubEmitTrigger::Death;
    i32 subEmitCount = 8;
    bool additive = false;
    bool worldSpace = true;
    u32 seed = 0;                   ///< 0 = random.
    /// Simulated on the GPU (in the vertex shader, from each particle's index and the time): hundreds
    /// of thousands of particles (rain, snow, dust, sparks, stars) for almost no CPU. Uses rate,
    /// maxParticles, lifetime, speed, shape, gravity, drag, colors, sizes, rotation, texture,
    /// additive, emitting, loop / duration and seed; the particles follow the emitter (local space).
    /// Not with: curves, flipbook, collisions, attractors, turbulence, sub-emitters, bursts, lit,
    /// mesh particles, velocity stretch.
    bool gpu = false;
};
/// Particle slots of a GPU emitter: enough for rate x the longest lifetime, at most maxParticles
/// (and 4 194 304). Slot i is reborn every slots / rate seconds.
template <typename Emitter>
inline u32 gpuParticleCount(const Emitter& em) {
    const double rate = em.rate > 0.0f ? em.rate : 0.0;
    const double lifeMax = em.lifetimeMax > em.lifetimeMin ? em.lifetimeMax : em.lifetimeMin;
    const double cap = em.maxParticles < (4u << 20) ? em.maxParticles : (4u << 20);
    const double need = static_cast<double>(static_cast<u64>(rate * lifeMax)) + 2.0;
    return static_cast<u32>(need < cap ? need : cap);
}
/// Advances a GPU emitter's clock and birth window (no particle is simulated on the CPU).
template <typename Emitter, typename State>
void advanceGpuEmitter(const Emitter& em, State& st, float dt, u32 seed) {
    if (!st.started) {
        st.started = true;
        st.gpuTime = 0.0;
        st.gpuSpawnStart = 0.0;
        st.gpuSpawnEnd = em.emitting ? 1e30 : -1.0;
        st.gpuWasEmitting = em.emitting;
        st.rng = (seed & 0xffffffu) | 1u;
    }
    st.particles.clear();
    st.gpuTime += dt;
    if (em.emitting != st.gpuWasEmitting) {
        st.gpuWasEmitting = em.emitting;
        if (em.emitting) st.gpuSpawnStart = st.gpuTime, st.gpuSpawnEnd = 1e30;
        else st.gpuSpawnEnd = st.gpuTime;
    }
    const double end = em.loop ? st.gpuSpawnEnd : std::min(st.gpuSpawnEnd, st.gpuSpawnStart + std::max(em.duration, 0.01f));
    const float life = 0.5f * (em.lifetimeMin + std::max(em.lifetimeMin, em.lifetimeMax));
    const double emitting = std::clamp(std::min(st.gpuTime, end) - std::max(st.gpuSpawnStart, st.gpuTime - life), 0.0, 1e9);
    st.gpuAlive = static_cast<u32>(std::min<double>(gpuParticleCount(em), std::ceil(emitting * em.rate - 1e-6)));
}
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
    math::Vec3 lastEmitterPos{0.0f};  ///< For inheritVelocity.
    float clock = 0.0f;               ///< Seconds since start (turbulence field time).
    // GPU emitters: particle time, and the window in which particles may be born.
    double gpuTime = 0.0;
    double gpuSpawnStart = 0.0, gpuSpawnEnd = 1e30;
    bool gpuWasEmitting = true;
    u32 gpuAlive = 0;                 ///< Estimate (statistics).
};

/// Obstacle for particles of emitters with `collisions` (world space): a sphere (diameter
/// size.x) or a box (size), turned and scaled with the entity. 2D particles use its XY section.
struct ParticleColliderComponent {
    enum class Shape : i32 { Sphere = 0, Box = 1 };
    Shape shape = Shape::Sphere;
    math::Vec3 size{1.0f, 1.0f, 1.0f};
};

/// Pulls (strength > 0) or pushes (< 0) the particles within `radius` of the entity (world-space
/// emitters with affectedByAttractors): vortexes, magnets, black holes, wind gusts.
struct ParticleAttractorComponent {
    float strength = 6.0f;  ///< World units / s^2 at the center, fading to 0 at the radius.
    float radius = 3.0f;
    bool enabled = true;
};

/// Ribbon left behind the entity as it moves (projectiles, swords, comets, dashes): points are
/// recorded every `minDistance` and fade over `lifetime`; width and color follow `curves`
/// (.agrparticle: size = width multiplier, gradient over the age of the point), or `color`.
/// Faces the camera (in 2D: lies in the XY plane).
struct TrailComponent {
    bool emitting = true;
    float lifetime = 0.4f;          ///< Seconds a point stays.
    float minDistance = 0.08f;      ///< World units between recorded points.
    float width = 0.3f;
    math::Color color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string curves;             ///< Optional .agrparticle (width curve, color gradient over the age).
    std::string texture;            ///< Empty = plain; u runs along the trail, v across.
    bool additive = false;
    i32 sortingLayer = 0;
    i32 orderInLayer = 0;
    u32 maxPoints = 128;
};
struct TrailPoint {
    math::Vec3 position;
    float time = 0.0f;
};
/// Runtime points of a trail (not serialized).
struct TrailStateComponent {
    std::vector<TrailPoint> points;  ///< Oldest first; the last one follows the entity.
    float time = 0.0f;
};

/// Registers reflection data for the render components (idempotent).
void registerRenderComponents();

} // namespace agr
