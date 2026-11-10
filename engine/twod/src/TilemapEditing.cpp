#include "agr/twod/TilemapEditing.hpp"

#include <cmath>
#include <vector>

namespace agr {

void tilemapEnsureLayout(TilemapComponent& tm) {
    const usize needed = static_cast<usize>(tm.width) * tm.height;
    if (tm.dataWidth == tm.width && tm.tiles.size() == needed) return;
    std::vector<i32> out(needed, -1);
    const u32 oldW = tm.dataWidth ? tm.dataWidth : tm.width;
    const usize oldH = oldW ? tm.tiles.size() / oldW : 0;
    for (usize y = 0; y < std::min<usize>(oldH, tm.height); ++y)
        for (u32 x = 0; x < std::min(oldW, tm.width); ++x) out[y * tm.width + x] = tm.tiles[y * oldW + x];
    tm.tiles = std::move(out);
    tm.dataWidth = tm.width;
}

bool tilemapSet(TilemapComponent& tm, i32 x, i32 y, i32 tile) {
    if (!tilemapContains(tm, x, y)) return false;
    tilemapEnsureLayout(tm);
    i32& cell = tm.tiles[static_cast<usize>(y) * tm.width + static_cast<usize>(x)];
    if (cell == tile) return false;
    cell = tile;
    return true;
}

u32 tilemapFillRect(TilemapComponent& tm, i32 x0, i32 y0, i32 x1, i32 y1, i32 tile) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    u32 changed = 0;
    for (i32 y = std::max(0, y0); y <= std::min(static_cast<i32>(tm.height) - 1, y1); ++y)
        for (i32 x = std::max(0, x0); x <= std::min(static_cast<i32>(tm.width) - 1, x1); ++x) changed += tilemapSet(tm, x, y, tile) ? 1u : 0u;
    return changed;
}

u32 tilemapFloodFill(TilemapComponent& tm, i32 x, i32 y, i32 tile) {
    if (!tilemapContains(tm, x, y)) return 0;
    tilemapEnsureLayout(tm);
    const i32 target = tm.tileAt(static_cast<u32>(x), static_cast<u32>(y));
    if (target == tile) return 0;
    u32 changed = 0;
    std::vector<std::pair<i32, i32>> stack{{x, y}};
    while (!stack.empty()) {
        const auto [cx, cy] = stack.back();
        stack.pop_back();
        if (!tilemapContains(tm, cx, cy) || tm.tileAt(static_cast<u32>(cx), static_cast<u32>(cy)) != target) continue;
        tm.tiles[static_cast<usize>(cy) * tm.width + static_cast<usize>(cx)] = tile;
        ++changed;
        stack.push_back({cx + 1, cy});
        stack.push_back({cx - 1, cy});
        stack.push_back({cx, cy + 1});
        stack.push_back({cx, cy - 1});
    }
    return changed;
}

std::optional<std::pair<i32, i32>> tilemapCellAt(const math::Mat4& world, const TilemapComponent& tm, const math::Ray& ray) {
    if (tm.cellSize.x <= 0.0f || tm.cellSize.y <= 0.0f) return std::nullopt;
    const math::Mat4 toLocal = math::inverse(world);
    const math::Vec3 o = toLocal.transformPoint(ray.origin);
    const math::Vec3 d = toLocal.transformVector(ray.direction);
    if (std::fabs(d.z) < 1e-8f) return std::nullopt;
    const float t = -o.z / d.z;
    if (t < 0.0f) return std::nullopt;
    const math::Vec3 p = o + d * t;
    return std::make_pair(static_cast<i32>(std::floor(p.x / tm.cellSize.x)), static_cast<i32>(std::floor(p.y / tm.cellSize.y)));
}

math::Aabb tilemapBounds(const math::Mat4& world, const TilemapComponent& tm) {
    const math::Aabb local{{0.0f, 0.0f, 0.0f}, {tm.cellSize.x * static_cast<float>(tm.width), tm.cellSize.y * static_cast<float>(tm.height), 0.0f}};
    return local.transformed(world);
}

} // namespace agr
