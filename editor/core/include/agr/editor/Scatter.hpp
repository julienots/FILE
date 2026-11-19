#pragma once
// Procedural scattering of props (trees, rocks, bushes...) for scene templates: deterministic
// dart throwing with weighted items, minimum spacing, exclusion rectangles and per-prop
// variations (size, rotation, tint). Same seed, same scene, on every platform.
#include <string>
#include <utility>
#include <vector>

#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::editor::templates {

struct ScatterItem {
    std::string model;          ///< .glb with a "Model" > "Skin" node pair (tools/generate_duel_art.py props).
    std::string name;           ///< Entity name ("Tree"); numbered by the scatter.
    float weight = 1.0f;        ///< Relative frequency.
    float minScale = 1.0f, maxScale = 1.0f;
    float radius = 0.5f;        ///< Footprint at scale 1 (spacing test).
    float outlineWidth = 0.0f;  ///< Cartoon outline in pixels (0: none).
    bool castShadows = true;
    float tint = 0.08f;         ///< Random brightness / hue variation of the instance color (0: none).
};

struct ScatterSettings {
    u32 seed = 1;
    math::Vec2 min{-10.0f, -10.0f}, max{10.0f, 10.0f};  ///< Area on the ground (x, z).
    float height = 0.0f;                                ///< Ground height (y).
    std::vector<std::pair<math::Vec2, math::Vec2>> exclude;  ///< Rectangles (min, max) kept free.
    float spacing = 0.3f;    ///< Extra gap between footprints.
    u32 maxCount = 200;
    u32 attempts = 4000;
    bool toon = true;
};

/// Places props under `parent` (each: entity "<name> <n>" > "Model" > "Skin" with a MeshRenderer).
/// Returns the created roots, in placement order.
std::vector<ecs::Entity> scatter(Scene& scene, ecs::Entity parent, const ScatterSettings& settings, const std::vector<ScatterItem>& items);

} // namespace agr::editor::templates
