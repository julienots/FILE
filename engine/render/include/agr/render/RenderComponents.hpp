#pragma once
#include <string>

#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

enum class Projection : i32 { Perspective = 0, Orthographic = 1 };

/// Renders the scene from the entity's world transform (looks down -Z).
struct CameraComponent {
    Projection projection = Projection::Perspective;
    float fieldOfView = math::radians(60.0f);  ///< Vertical, radians.
    float orthographicSize = 5.0f;             ///< Half of the visible height, world units.
    float nearPlane = 0.1f;
    float farPlane = 500.0f;
    math::Color clearColor{0.055f, 0.063f, 0.075f, 1.0f};  ///< sRGB. Default: AGR graphite-950.
    bool primary = true;                                   ///< The first active primary camera renders.
};

/// Sun-like light shining along the entity's forward axis (-Z).
struct DirectionalLightComponent {
    math::Color color{1.0f, 0.97f, 0.92f, 1.0f};
    float intensity = 1.0f;
};

/// Scene-wide ambient light (the first active one is used).
struct EnvironmentComponent {
    math::Color ambientColor{0.55f, 0.6f, 0.7f, 1.0f};
    float ambientIntensity = 0.35f;
};

enum class PrimitiveMesh : i32 { Cube = 0, Sphere = 1, Plane = 2, Quad = 3 };

/// Draws a built-in mesh (imported meshes arrive in phase 5).
struct MeshRendererComponent {
    PrimitiveMesh mesh = PrimitiveMesh::Cube;
    std::string texture;               ///< Asset URI, e.g. "assets://textures/crate.png". Empty = none.
    math::Color color{1.0f, 1.0f, 1.0f, 1.0f};  ///< sRGB tint.
    math::Vec2 uvScale{1.0f, 1.0f};    ///< Texture tiling.
    bool visible = true;
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
};

/// Registers reflection data for the render components (idempotent).
void registerRenderComponents();

} // namespace agr
