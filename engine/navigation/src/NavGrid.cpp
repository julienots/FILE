#include "agr/navigation/NavGrid.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

namespace agr {

using math::Vec2;

void NavGrid::reset(Vec2 origin, float cellSize, u32 width, u32 height) {
    m_origin = origin;
    m_cell = std::max(cellSize, 1e-3f);
    m_width = width;
    m_height = height;
    m_blocked.assign(static_cast<usize>(width) * height, 0);
}

void NavGrid::setBlocked(i32 x, i32 y, bool b) {
    if (x < 0 || y < 0 || x >= static_cast<i32>(m_width) || y >= static_cast<i32>(m_height)) return;
    m_blocked[static_cast<usize>(y) * m_width + static_cast<usize>(x)] = b ? 1 : 0;
}

bool NavGrid::blocked(i32 x, i32 y) const {
    if (x < 0 || y < 0 || x >= static_cast<i32>(m_width) || y >= static_cast<i32>(m_height)) return true;
    return m_blocked[static_cast<usize>(y) * m_width + static_cast<usize>(x)] != 0;
}

u32 NavGrid::blockedCount() const { return static_cast<u32>(std::count(m_blocked.begin(), m_blocked.end(), u8{1})); }

Vec2 NavGrid::cellCenter(i32 x, i32 y) const {
    return {m_origin.x + (static_cast<float>(x) + 0.5f) * m_cell, m_origin.y + (static_cast<float>(y) + 0.5f) * m_cell};
}

bool NavGrid::cellOf(Vec2 p, i32& x, i32& y) const {
    x = static_cast<i32>(std::floor((p.x - m_origin.x) / m_cell));
    y = static_cast<i32>(std::floor((p.y - m_origin.y) / m_cell));
    return x >= 0 && y >= 0 && x < static_cast<i32>(m_width) && y < static_cast<i32>(m_height);
}

void NavGrid::blockRect(Vec2 mn, Vec2 mx, float margin) {
    mn = mn - Vec2{margin, margin};
    mx = mx + Vec2{margin, margin};
    i32 x0, y0, x1, y1;
    cellOf(mn, x0, y0);
    cellOf(mx, x1, y1);
    for (i32 y = std::max(y0, 0); y <= std::min(y1, static_cast<i32>(m_height) - 1); ++y)
        for (i32 x = std::max(x0, 0); x <= std::min(x1, static_cast<i32>(m_width) - 1); ++x) {
            const Vec2 c = cellCenter(x, y);
            if (c.x >= mn.x && c.x <= mx.x && c.y >= mn.y && c.y <= mx.y) setBlocked(x, y, true);
        }
}

void NavGrid::blockCircle(Vec2 center, float radius, float margin) {
    const float r = radius + margin;
    i32 x0, y0, x1, y1;
    cellOf(center - Vec2{r, r}, x0, y0);
    cellOf(center + Vec2{r, r}, x1, y1);
    for (i32 y = std::max(y0, 0); y <= std::min(y1, static_cast<i32>(m_height) - 1); ++y)
        for (i32 x = std::max(x0, 0); x <= std::min(x1, static_cast<i32>(m_width) - 1); ++x)
            if (math::length(cellCenter(x, y) - center) <= r) setBlocked(x, y, true);
}

bool NavGrid::lineOfSight(Vec2 a, Vec2 b) const {
    // Samples at a quarter of a cell: conservative against corners.
    const float len = math::length(b - a);
    const int steps = std::max(1, static_cast<int>(std::ceil(len / (m_cell * 0.25f))));
    for (int i = 0; i <= steps; ++i) {
        const Vec2 p = a + (b - a) * (static_cast<float>(i) / static_cast<float>(steps));
        i32 x, y;
        if (!cellOf(p, x, y) || blocked(x, y)) return false;
    }
    return true;
}

bool NavGrid::nearestFree(i32& x, i32& y, i32 radius) const {
    if (!blocked(x, y)) return true;
    for (i32 r = 1; r <= radius; ++r) {
        i32 bestX = 0, bestY = 0;
        float best = 1e30f;
        for (i32 dy = -r; dy <= r; ++dy)
            for (i32 dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r || blocked(x + dx, y + dy)) continue;
                const float d = static_cast<float>(dx * dx + dy * dy);
                if (d < best) best = d, bestX = x + dx, bestY = y + dy;
            }
        if (best < 1e30f) {
            x = bestX;
            y = bestY;
            return true;
        }
    }
    return false;
}

bool NavGrid::findPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, bool smooth, i32 snapRadius) const {
    out.clear();
    m_lastExpanded = 0;
    if (empty()) return false;
    i32 sx, sy, gx, gy;
    cellOf(from, sx, sy);
    cellOf(to, gx, gy);
    sx = std::clamp(sx, 0, static_cast<i32>(m_width) - 1);
    sy = std::clamp(sy, 0, static_cast<i32>(m_height) - 1);
    gx = std::clamp(gx, 0, static_cast<i32>(m_width) - 1);
    gy = std::clamp(gy, 0, static_cast<i32>(m_height) - 1);
    const bool goalSnapped = blocked(gx, gy);
    if (!nearestFree(sx, sy, snapRadius) || !nearestFree(gx, gy, snapRadius)) return false;

    const usize n = static_cast<usize>(m_width) * m_height;
    std::vector<float> g(n, 1e30f);
    std::vector<i32> parent(n, -1);
    std::vector<u8> closed(n, 0);
    auto index = [&](i32 x, i32 y) { return static_cast<usize>(y) * m_width + static_cast<usize>(x); };
    auto heuristic = [&](i32 x, i32 y) {  // octile distance
        const float dx = static_cast<float>(std::abs(x - gx)), dy = static_cast<float>(std::abs(y - gy));
        return (dx + dy) + (1.41421356f - 2.0f) * std::min(dx, dy);
    };
    using Node = std::pair<float, i32>;  // f, cell index
    std::priority_queue<Node, std::vector<Node>, std::greater<>> open;
    const usize start = index(sx, sy), goal = index(gx, gy);
    g[start] = 0.0f;
    open.push({heuristic(sx, sy), static_cast<i32>(start)});
    bool found = false;
    while (!open.empty()) {
        const usize cur = static_cast<usize>(open.top().second);
        open.pop();
        if (closed[cur]) continue;
        closed[cur] = 1;
        ++m_lastExpanded;
        if (cur == goal) {
            found = true;
            break;
        }
        const i32 cx = static_cast<i32>(cur % m_width), cy = static_cast<i32>(cur / m_width);
        for (i32 dy = -1; dy <= 1; ++dy)
            for (i32 dx = -1; dx <= 1; ++dx) {
                if ((dx == 0 && dy == 0) || blocked(cx + dx, cy + dy)) continue;
                if (dx != 0 && dy != 0 && (blocked(cx + dx, cy) || blocked(cx, cy + dy))) continue;  // no corner cutting
                const usize nb = index(cx + dx, cy + dy);
                if (closed[nb]) continue;
                const float cost = g[cur] + ((dx != 0 && dy != 0) ? 1.41421356f : 1.0f);
                if (cost < g[nb]) {
                    g[nb] = cost;
                    parent[nb] = static_cast<i32>(cur);
                    open.push({cost + heuristic(cx + dx, cy + dy), static_cast<i32>(nb)});
                }
            }
    }
    if (!found) return false;
    std::vector<Vec2> cells;
    for (i32 c = static_cast<i32>(goal); c >= 0; c = parent[static_cast<usize>(c)])
        cells.push_back(cellCenter(static_cast<i32>(static_cast<u32>(c) % m_width), static_cast<i32>(static_cast<u32>(c) / m_width)));
    std::reverse(cells.begin(), cells.end());
    // Exact end points when they are free; cell centers otherwise.
    i32 fx, fy;
    if (cellOf(from, fx, fy) && !blocked(fx, fy)) cells.front() = from;
    if (!goalSnapped) cells.back() = to;
    if (!smooth || cells.size() <= 2) {
        out = std::move(cells);
        return true;
    }
    // String pulling: keep a point only when the next ones are not directly visible.
    out.push_back(cells.front());
    usize anchor = 0;
    for (usize i = 2; i < cells.size(); ++i)
        if (!lineOfSight(cells[anchor], cells[i])) {
            out.push_back(cells[i - 1]);
            anchor = i - 1;
        }
    out.push_back(cells.back());
    return true;
}

} // namespace agr
