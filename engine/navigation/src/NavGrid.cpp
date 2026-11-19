#include "agr/navigation/NavGrid.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

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
    // Exact grid traversal (Amanatides & Woo) of every cell the segment touches.
    // When the segment passes exactly through a cell corner, both side cells must
    // be free too: the same "no corner cutting" rule as the A* search.
    const float ax = (a.x - m_origin.x) / m_cell, ay = (a.y - m_origin.y) / m_cell;
    const float bx = (b.x - m_origin.x) / m_cell, by = (b.y - m_origin.y) / m_cell;
    i32 x = static_cast<i32>(std::floor(ax)), y = static_cast<i32>(std::floor(ay));
    const i32 ex = static_cast<i32>(std::floor(bx)), ey = static_cast<i32>(std::floor(by));
    if (blocked(x, y) || blocked(ex, ey)) return false;
    const float dx = bx - ax, dy = by - ay;
    const i32 sx = dx > 0.0f ? 1 : (dx < 0.0f ? -1 : 0);
    const i32 sy = dy > 0.0f ? 1 : (dy < 0.0f ? -1 : 0);
    constexpr float kInf = 1e30f;
    const float tdx = sx != 0 ? 1.0f / std::abs(dx) : kInf;
    const float tdy = sy != 0 ? 1.0f / std::abs(dy) : kInf;
    float tx = sx > 0 ? (static_cast<float>(x + 1) - ax) * tdx : (sx < 0 ? (ax - static_cast<float>(x)) * tdx : kInf);
    float ty = sy > 0 ? (static_cast<float>(y + 1) - ay) * tdy : (sy < 0 ? (ay - static_cast<float>(y)) * tdy : kInf);
    constexpr float kCornerEps = 1e-5f;
    i32 budget = std::abs(ex - x) + std::abs(ey - y) + 2;  // hard bound against float drift
    while ((x != ex || y != ey) && budget-- > 0) {
        if (std::abs(tx - ty) <= kCornerEps) {
            if (blocked(x + sx, y) || blocked(x, y + sy)) return false;
            x += sx;
            y += sy;
            tx += tdx;
            ty += tdy;
        } else if (tx < ty) {
            x += sx;
            tx += tdx;
        } else {
            y += sy;
            ty += tdy;
        }
        if (blocked(x, y)) return false;
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

    // Per-thread scratch reused between queries: no allocation and no O(cells)
    // clear per search. A generation stamp marks which entries are current.
    struct OpenNode {
        float f, g;
        i32 cell;
    };
    struct Scratch {
        std::vector<float> g;
        std::vector<i32> parent;
        std::vector<u32> seen;    ///< == gen: g / parent valid for this search
        std::vector<u32> closed;  ///< == gen: expanded in this search
        std::vector<OpenNode> heap;
        u32 gen = 0;
    };
    thread_local Scratch sc;
    const usize n = static_cast<usize>(m_width) * m_height;
    if (sc.seen.size() < n) {
        sc.g.resize(n);
        sc.parent.resize(n);
        sc.seen.assign(n, 0);
        sc.closed.assign(n, 0);
        sc.gen = 0;
    }
    if (++sc.gen == 0) {  // wrapped: reset the stamps once every 4 billion searches
        std::fill(sc.seen.begin(), sc.seen.end(), 0u);
        std::fill(sc.closed.begin(), sc.closed.end(), 0u);
        sc.gen = 1;
    }
    const u32 gen = sc.gen;
    float* g = sc.g.data();
    i32* parent = sc.parent.data();
    u32* seen = sc.seen.data();
    u32* closed = sc.closed.data();
    const u8* blockedCells = m_blocked.data();
    const i32 w = static_cast<i32>(m_width), h = static_cast<i32>(m_height);

    auto heuristic = [&](i32 x, i32 y) {  // octile distance
        const float dx = static_cast<float>(std::abs(x - gx)), dy = static_cast<float>(std::abs(y - gy));
        return (dx + dy) + (1.41421356f - 2.0f) * std::min(dx, dy);
    };
    auto& open = sc.heap;
    open.clear();
    // Min-heap on f; among equal f, the deepest node (largest g) first. On open
    // ground this follows one optimal line instead of flooding every tie.
    const auto cmp = [](const OpenNode& a, const OpenNode& b) { return a.f > b.f || (a.f == b.f && a.g < b.g); };
    const i32 start = sy * w + sx, goal = gy * w + gx;
    g[start] = 0.0f;
    parent[start] = -1;
    seen[start] = gen;
    open.push_back({heuristic(sx, sy), 0.0f, start});
    bool found = false;
    // Neighbour order: 4 straight moves first, then the diagonals that need them.
    static constexpr i32 kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static constexpr i32 kDy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!open.empty()) {
        std::pop_heap(open.begin(), open.end(), cmp);
        const i32 cur = open.back().cell;
        open.pop_back();
        if (closed[cur] == gen) continue;
        closed[cur] = gen;
        ++m_lastExpanded;
        if (cur == goal) {
            found = true;
            break;
        }
        const i32 cx = cur % w, cy = cur / w;
        const float gc = g[cur];
        bool freeStraight[4];
        for (int k = 0; k < 8; ++k) {
            const i32 nx = cx + kDx[k], ny = cy + kDy[k];
            bool free = nx >= 0 && ny >= 0 && nx < w && ny < h && blockedCells[ny * w + nx] == 0;
            if (k < 4) {
                freeStraight[k] = free;
            } else {
                // No corner cutting: both orthogonal neighbours must be free.
                free = free && freeStraight[kDx[k] > 0 ? 0 : 1] && freeStraight[kDy[k] > 0 ? 2 : 3];
            }
            if (!free) continue;
            const i32 nb = ny * w + nx;
            if (closed[nb] == gen) continue;
            const float cost = gc + (k >= 4 ? 1.41421356f : 1.0f);
            if (seen[nb] != gen || cost < g[nb]) {
                seen[nb] = gen;
                g[nb] = cost;
                parent[nb] = cur;
                open.push_back({cost + heuristic(nx, ny), cost, nb});
                std::push_heap(open.begin(), open.end(), cmp);
            }
        }
    }
    if (!found) return false;
    std::vector<Vec2> cells;
    for (i32 c = goal; c >= 0; c = parent[c])
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
