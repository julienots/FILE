#pragma once
#include <optional>
#include <utility>

#include "agr/math/Geometry.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr {

/// Lays the tile data out for the current width x height (keeps existing tiles
/// at their (x, y) position after a resize; new cells are empty).
void tilemapEnsureLayout(TilemapComponent& tm);
/// Sets one cell. Returns true when the value changed.
bool tilemapSet(TilemapComponent& tm, i32 x, i32 y, i32 tile);
/// Fills the rectangle between two cells (inclusive). Returns changed cells.
u32 tilemapFillRect(TilemapComponent& tm, i32 x0, i32 y0, i32 x1, i32 y1, i32 tile);
/// 4-connected flood fill from (x, y) replacing its tile value. Returns changed cells.
u32 tilemapFloodFill(TilemapComponent& tm, i32 x, i32 y, i32 tile);
/// Cell under a world-space ray (tilemap plane = local XY). Coordinates may be
/// outside the map; check them with tilemapContains.
std::optional<std::pair<i32, i32>> tilemapCellAt(const math::Mat4& world, const TilemapComponent& tm, const math::Ray& ray);
inline bool tilemapContains(const TilemapComponent& tm, i32 x, i32 y) {
    return x >= 0 && y >= 0 && static_cast<u32>(x) < tm.width && static_cast<u32>(y) < tm.height;
}
/// World-space bounds of the whole map.
math::Aabb tilemapBounds(const math::Mat4& world, const TilemapComponent& tm);

} // namespace agr
