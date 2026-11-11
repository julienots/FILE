#pragma once
// 2D navigation on a grid of cells (XY plane): A* with 8 directions, no corner cutting,
// paths smoothed by line of sight. Built from tilemaps and static 2D colliders.
#include <vector>

#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

class NavGrid {
public:
    /// `origin`: world position of the lower-left corner of cell (0, 0).
    void reset(math::Vec2 origin, float cellSize, u32 width, u32 height);
    bool empty() const { return m_width == 0 || m_height == 0; }
    u32 width() const { return m_width; }
    u32 height() const { return m_height; }
    float cellSize() const { return m_cell; }
    math::Vec2 origin() const { return m_origin; }

    void setBlocked(i32 x, i32 y, bool blocked);
    bool blocked(i32 x, i32 y) const;  ///< Outside the grid counts as blocked.
    /// Blocks every cell whose center is within the rectangle (world units), grown by `margin`.
    void blockRect(math::Vec2 min, math::Vec2 max, float margin);
    /// Blocks every cell whose center is within `radius + margin` of `center`.
    void blockCircle(math::Vec2 center, float radius, float margin);
    u32 blockedCount() const;

    math::Vec2 cellCenter(i32 x, i32 y) const;
    bool cellOf(math::Vec2 p, i32& x, i32& y) const;
    /// True when the straight segment crosses only free cells.
    bool lineOfSight(math::Vec2 a, math::Vec2 b) const;

    /// Path from `from` to `to` (both moved to the nearest free cell within `snapRadius`
    /// cells when they fall on an obstacle). The first point is `from`, the last the reachable
    /// goal. False when no path exists (out is cleared).
    bool findPath(math::Vec2 from, math::Vec2 to, std::vector<math::Vec2>& out, bool smooth = true, i32 snapRadius = 3) const;
    u32 lastExpanded() const { return m_lastExpanded; }

private:
    bool nearestFree(i32& x, i32& y, i32 radius) const;
    math::Vec2 m_origin{0.0f, 0.0f};
    float m_cell = 1.0f;
    u32 m_width = 0, m_height = 0;
    std::vector<u8> m_blocked;
    mutable u32 m_lastExpanded = 0;
};

} // namespace agr
