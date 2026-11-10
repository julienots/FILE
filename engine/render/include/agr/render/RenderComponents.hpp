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
};

/// Cone of light along the entity's forward axis (-Z).
struct SpotLightComponent {
    math::Color color{1.0f, 0.95f, 0.85f, 1.0f};
    float intensity = 10.0f;
    float range = 15.0f;
    float innerAngle = math::radians(20.0f);  ///< Half angle of the fully lit cone.
    float outerAngle = math::radians(30.0f);  ///< Half angle where the light reaches zero.
};

enum class SkyMode : i32 {
    CameraColor = 0,  ///< Background = the camera clear color; flat ambient light.
    Gradient = 1,     ///< Procedural sky (zenith, horizon, ground, sun disc); hemisphere ambient light.
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
    bool fog = false;
    math::Color fogColor{0.75f, 0.80f, 0.86f, 1.0f};
    float fogStart = 20.0f;
    float fogEnd = 150.0f;
    float exposure = 1.0f;  ///< Multiplier before tone mapping (ACES).
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
    u32 dataWidth = 16;                  ///< Width the tile data is laid out with (resizes are remapped).
    std::vector<i32> tiles;              ///< Frame index per cell (row-major from y = 0), -1 = empty.

    i32 tileAt(u32 x, u32 y) const {
        const usize i = static_cast<usize>(y) * dataWidth + x;
        return x < width && y < height && x < dataWidth && i < tiles.size() ? tiles[i] : -1;
    }
};

/// Point light for lit sprites and tilemaps (2D). Shadows are not implemented yet.
struct Light2DComponent {
    math::Color color{1.0f, 0.85f, 0.6f, 1.0f};
    float intensity = 1.5f;
    float radius = 5.0f;   ///< World units; no light beyond.
    float falloff = 1.5f;  ///< Exponent of (1 - distance / radius).
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

/// Registers reflection data for the render components (idempotent).
void registerRenderComponents();

} // namespace agr
