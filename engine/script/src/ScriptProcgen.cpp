// agr.procgen: procedural generation built into the engine (Godot has none): dungeons (rooms and
// corridors), caves (cellular automata, one connected region), Wave Function Collapse (a map that
// follows the adjacencies of a small hand-made example), and tilemap access from scripts
// (entity:tile / setTile / setTiles). Grids are Lua tables of rows: grid[y][x], 1-based.
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <random>

#include "ScriptInternal.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr {
namespace {

using Grid = std::vector<std::vector<i32>>;  // [y][x]

void pushGrid(lua_State* L, const Grid& g) {
    lua_createtable(L, static_cast<int>(g.size()), 0);
    for (usize y = 0; y < g.size(); ++y) {
        lua_createtable(L, static_cast<int>(g[y].size()), 0);
        for (usize x = 0; x < g[y].size(); ++x) {
            lua_pushinteger(L, g[y][x]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(x + 1));
        }
        lua_rawseti(L, -2, static_cast<lua_Integer>(y + 1));
    }
}

Grid checkGrid(lua_State* L, int idx) {
    luaL_checktype(L, idx, LUA_TTABLE);
    Grid g;
    const lua_Integer h = luaL_len(L, idx);
    for (lua_Integer y = 1; y <= h; ++y) {
        lua_rawgeti(L, idx, y);
        luaL_checktype(L, -1, LUA_TTABLE);
        std::vector<i32> row;
        const lua_Integer w = luaL_len(L, -1);
        for (lua_Integer x = 1; x <= w; ++x) {
            lua_rawgeti(L, -1, x);
            row.push_back(static_cast<i32>(luaL_checkinteger(L, -1)));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        if (!g.empty() && row.size() != g[0].size()) luaL_error(L, "grid rows must have the same length");
        g.push_back(std::move(row));
    }
    if (g.empty() || g[0].empty()) luaL_error(L, "empty grid");
    return g;
}

double optField(lua_State* L, int idx, const char* name, double fallback) {
    if (lua_type(L, idx) != LUA_TTABLE) return fallback;
    lua_getfield(L, idx, name);
    const double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : fallback;
    lua_pop(L, 1);
    return v;
}

int dims(lua_State* L, int idx) {
    const lua_Integer v = luaL_checkinteger(L, idx);
    if (v < 3 || v > 1024) luaL_error(L, "size must be between 3 and 1024");
    return static_cast<int>(v);
}

/// Keeps only the largest 4-connected region of `value` (others become `other`).
void keepLargest(Grid& g, i32 value, i32 other) {
    const int h = static_cast<int>(g.size()), w = static_cast<int>(g[0].size());
    std::vector<int> label(static_cast<usize>(w * h), -1);
    int best = -1, bestSize = 0, next = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (g[y][x] != value || label[static_cast<usize>(y * w + x)] >= 0) continue;
            int size = 0;
            std::queue<std::pair<int, int>> q;
            q.push({x, y});
            label[static_cast<usize>(y * w + x)] = next;
            while (!q.empty()) {
                auto [cx, cy] = q.front();
                q.pop();
                ++size;
                const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& o : d) {
                    const int nx = cx + o[0], ny = cy + o[1];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h || g[ny][nx] != value || label[static_cast<usize>(ny * w + nx)] >= 0) continue;
                    label[static_cast<usize>(ny * w + nx)] = next;
                    q.push({nx, ny});
                }
            }
            if (size > bestSize) bestSize = size, best = next;
            ++next;
        }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (g[y][x] == value && label[static_cast<usize>(y * w + x)] != best) g[y][x] = other;
}

/// agr.procgen.dungeon(w, h, {seed, rooms = 8, minRoom = 4, maxRoom = 9}) -> grid (1 floor, 0 wall), rooms
/// {{x, y, w, h, cx, cy}...}: non-overlapping rooms joined by L-shaped corridors in a chain.
int pg_dungeon(lua_State* L) {
    const int w = dims(L, 1), h = dims(L, 2);
    std::mt19937 rng(static_cast<u32>(optField(L, 3, "seed", 1)));
    const int count = static_cast<int>(optField(L, 3, "rooms", 8));
    const int minR = std::max(2, static_cast<int>(optField(L, 3, "minRoom", 4)));
    const int maxR = std::max(minR, static_cast<int>(optField(L, 3, "maxRoom", 9)));
    Grid g(static_cast<usize>(h), std::vector<i32>(static_cast<usize>(w), 0));
    struct Room {
        int x, y, w, h;
    };
    std::vector<Room> rooms;
    auto pick = [&](int a, int b) { return std::uniform_int_distribution<int>(a, std::max(a, b))(rng); };
    for (int attempt = 0; attempt < count * 30 && static_cast<int>(rooms.size()) < count; ++attempt) {
        Room r{0, 0, pick(minR, maxR), pick(minR, maxR)};
        if (r.w >= w - 2 || r.h >= h - 2) continue;
        r.x = pick(1, w - r.w - 1);
        r.y = pick(1, h - r.h - 1);
        bool overlap = false;
        for (const Room& o : rooms)
            if (r.x <= o.x + o.w && o.x <= r.x + r.w && r.y <= o.y + o.h && o.y <= r.y + r.h) overlap = true;  // one wall cell apart
        if (overlap) continue;
        rooms.push_back(r);
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x) g[y][x] = 1;
    }
    // Rooms sorted along x then chained: corridors rarely cross the whole map.
    std::sort(rooms.begin(), rooms.end(), [](const Room& a, const Room& b) { return a.x + a.w / 2 < b.x + b.w / 2; });
    for (usize i = 1; i < rooms.size(); ++i) {
        int x0 = rooms[i - 1].x + rooms[i - 1].w / 2, y0 = rooms[i - 1].y + rooms[i - 1].h / 2;
        const int x1 = rooms[i].x + rooms[i].w / 2, y1 = rooms[i].y + rooms[i].h / 2;
        const bool horizontalFirst = rng() & 1u;
        auto carveX = [&](int y) { for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x) g[y][x] = 1; };
        auto carveY = [&](int x) { for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y) g[y][x] = 1; };
        if (horizontalFirst) carveX(y0), carveY(x1);
        else carveY(x0), carveX(y1);
    }
    pushGrid(L, g);
    lua_createtable(L, static_cast<int>(rooms.size()), 0);
    for (usize i = 0; i < rooms.size(); ++i) {
        const Room& r = rooms[i];
        lua_createtable(L, 0, 6);
        const std::pair<const char*, int> f[] = {{"x", r.x + 1}, {"y", r.y + 1}, {"w", r.w}, {"h", r.h}, {"cx", r.x + r.w / 2 + 1}, {"cy", r.y + r.h / 2 + 1}};
        for (const auto& [k, v] : f) {
            lua_pushinteger(L, v);
            lua_setfield(L, -2, k);
        }
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 2;
}

/// agr.procgen.caves(w, h, {seed, fill = 0.45, steps = 5}) -> grid (1 floor, 0 rock), one connected cave.
int pg_caves(lua_State* L) {
    const int w = dims(L, 1), h = dims(L, 2);
    std::mt19937 rng(static_cast<u32>(optField(L, 3, "seed", 1)));
    const double fill = std::clamp(optField(L, 3, "fill", 0.45), 0.0, 1.0);
    const int steps = std::clamp(static_cast<int>(optField(L, 3, "steps", 5)), 0, 20);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    Grid g(static_cast<usize>(h), std::vector<i32>(static_cast<usize>(w), 0));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) g[y][x] = (x == 0 || y == 0 || x == w - 1 || y == h - 1 || u(rng) < fill) ? 0 : 1;
    for (int s = 0; s < steps; ++s) {
        Grid n = g;
        for (int y = 1; y < h - 1; ++y)
            for (int x = 1; x < w - 1; ++x) {
                int rock = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) rock += (dx || dy) && g[y + dy][x + dx] == 0;
                n[y][x] = rock >= 5 ? 0 : rock <= 3 ? 1 : g[y][x];  // 4-5 rule
            }
        g = std::move(n);
    }
    keepLargest(g, 1, 0);
    pushGrid(L, g);
    return 1;
}

/// agr.procgen.wfc(sample, w, h, {seed, attempts = 10}) -> grid or nil: every pair of neighbors in the
/// result also appears next to each other (same direction) in the sample; tiles are as frequent.
int pg_wfc(lua_State* L) {
    const Grid sample = checkGrid(L, 1);
    const int w = dims(L, 2), h = dims(L, 3);
    const u32 seed = static_cast<u32>(optField(L, 4, "seed", 1));
    const int attempts = std::clamp(static_cast<int>(optField(L, 4, "attempts", 10)), 1, 100);
    // Tiles, weights, allowed neighbors per direction (0 +x, 1 -x, 2 +y, 3 -y).
    std::map<i32, usize> index;
    std::vector<i32> tiles;
    std::vector<double> weight;
    for (const auto& row : sample)
        for (i32 v : row) {
            auto [it, inserted] = index.try_emplace(v, tiles.size());
            if (inserted) tiles.push_back(v), weight.push_back(0.0);
            weight[it->second] += 1.0;
        }
    const usize T = tiles.size();
    std::vector<std::vector<std::vector<bool>>> allow(4, std::vector<std::vector<bool>>(T, std::vector<bool>(T, false)));
    const int sh = static_cast<int>(sample.size()), sw = static_cast<int>(sample[0].size());
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x)
            for (int d = 0; d < 4; ++d) {
                const int nx = x + dx[d], ny = y + dy[d];
                if (nx < 0 || ny < 0 || nx >= sw || ny >= sh) continue;
                allow[d][index[sample[y][x]]][index[sample[ny][nx]]] = true;
            }
    for (int attempt = 0; attempt < attempts; ++attempt) {
        std::mt19937 rng(seed + static_cast<u32>(attempt) * 7919u);
        std::vector<std::vector<bool>> wave(static_cast<usize>(w * h), std::vector<bool>(T, true));
        bool failed = false;
        auto propagate = [&](usize start) {
            std::vector<usize> stack{start};
            while (!stack.empty() && !failed) {
                const usize c = stack.back();
                stack.pop_back();
                const int cx = static_cast<int>(c % static_cast<usize>(w)), cy = static_cast<int>(c / static_cast<usize>(w));
                for (int d = 0; d < 4; ++d) {
                    const int nx = cx + dx[d], ny = cy + dy[d];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    const usize n = static_cast<usize>(ny * w + nx);
                    bool changed = false, any = false;
                    for (usize t = 0; t < T; ++t) {
                        if (!wave[n][t]) continue;
                        bool ok = false;
                        for (usize s = 0; s < T && !ok; ++s) ok = wave[c][s] && allow[d][s][t];
                        if (!ok) wave[n][t] = false, changed = true;
                        else any = true;
                    }
                    if (!any) failed = true;
                    if (changed) stack.push_back(n);
                }
            }
        };
        for (;;) {
            // Cell with the fewest options left (ties broken randomly).
            usize best = SIZE_MAX, bestCount = T + 1;
            u32 bestNoise = 0;
            for (usize c = 0; c < wave.size(); ++c) {
                const usize n = static_cast<usize>(std::count(wave[c].begin(), wave[c].end(), true));
                if (n <= 1) continue;
                const u32 noise = rng();
                if (n < bestCount || (n == bestCount && noise < bestNoise)) best = c, bestCount = n, bestNoise = noise;
            }
            if (best == SIZE_MAX) break;  // every cell decided
            double total = 0.0;
            for (usize t = 0; t < T; ++t) total += wave[best][t] ? weight[t] : 0.0;
            double r = std::uniform_real_distribution<double>(0.0, total)(rng);
            usize chosen = 0;
            for (usize t = 0; t < T; ++t) {
                if (!wave[best][t]) continue;
                chosen = t;
                r -= weight[t];
                if (r <= 0.0) break;
            }
            for (usize t = 0; t < T; ++t) wave[best][t] = t == chosen;
            propagate(best);
            if (failed) break;
        }
        if (failed) continue;
        Grid g(static_cast<usize>(h), std::vector<i32>(static_cast<usize>(w), 0));
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const auto& cell = wave[static_cast<usize>(y * w + x)];
                g[y][x] = tiles[static_cast<usize>(std::find(cell.begin(), cell.end(), true) - cell.begin())];
            }
        pushGrid(L, g);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

// ---- tilemap cells from scripts (cell (1, 1) = the bottom-left one, as stored)
TilemapComponent& tilemapOf(lua_State* L, ecs::Entity e) {
    auto* tm = sceneOf(L).registry().tryGet<TilemapComponent>(e);
    if (!tm) luaL_error(L, "the entity has no Tilemap");
    if (tm->tiles.size() != static_cast<usize>(tm->width) * tm->height) tm->tiles.assign(static_cast<usize>(tm->width) * tm->height, -1);
    return *tm;
}
/// entity:tile(x, y) -> frame or -1 (empty / outside).
int e_tile(lua_State* L) {
    TilemapComponent& tm = tilemapOf(L, checkEntity(L, 1));
    const lua_Integer x = luaL_checkinteger(L, 2) - 1, y = luaL_checkinteger(L, 3) - 1;
    lua_pushinteger(L, x < 0 || y < 0 || x >= tm.width || y >= tm.height ? -1 : tm.tiles[static_cast<usize>(y * tm.width + x)]);
    return 1;
}
/// entity:setTile(x, y, frame) (-1 clears).
int e_setTile(lua_State* L) {
    TilemapComponent& tm = tilemapOf(L, checkEntity(L, 1));
    const lua_Integer x = luaL_checkinteger(L, 2) - 1, y = luaL_checkinteger(L, 3) - 1;
    if (x < 0 || y < 0 || x >= tm.width || y >= tm.height) return 0;
    tm.tiles[static_cast<usize>(y * tm.width + x)] = static_cast<i32>(luaL_checkinteger(L, 4));
    return 0;
}
/// entity:setTiles(grid[, mapping]): resizes the tilemap to the grid and fills it; mapping[value] =
/// frame (missing values: the value itself; -1 = empty). grid[1] is the bottom row.
int e_setTiles(lua_State* L) {
    TilemapComponent& tm = tilemapOf(L, checkEntity(L, 1));
    const Grid g = checkGrid(L, 2);
    tm.width = static_cast<u32>(g[0].size());
    tm.height = static_cast<u32>(g.size());
    tm.tiles.assign(static_cast<usize>(tm.width) * tm.height, -1);
    for (usize y = 0; y < g.size(); ++y)
        for (usize x = 0; x < g[y].size(); ++x) {
            i32 frame = g[y][x];
            if (lua_type(L, 3) == LUA_TTABLE) {
                lua_rawgeti(L, 3, g[y][x]);
                if (lua_isinteger(L, -1)) frame = static_cast<i32>(lua_tointeger(L, -1));
                lua_pop(L, 1);
            }
            tm.tiles[y * tm.width + x] = frame;
        }
    return 0;
}
/// entity:tilemapSize() -> width, height
int e_tilemapSize(lua_State* L) {
    TilemapComponent& tm = tilemapOf(L, checkEntity(L, 1));
    lua_pushinteger(L, tm.width);
    lua_pushinteger(L, tm.height);
    return 2;
}

} // namespace

void openProcgenBindings(lua_State* L) {
    static const luaL_Reg procgen[] = {{"dungeon", pg_dungeon}, {"caves", pg_caves}, {"wfc", pg_wfc}, {nullptr, nullptr}};
    addAgrLibrary(L, "procgen", procgen);
    static const luaL_Reg methods[] = {{"tile", e_tile}, {"setTile", e_setTile}, {"setTiles", e_setTiles}, {"tilemapSize", e_tilemapSize}, {nullptr, nullptr}};
    addEntityMethods(L, methods);
}

} // namespace agr
