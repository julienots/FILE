-- Recherche de chemin sur grille (paquet officiel grid-pathfinding) :
--   local Grid = require("assets://kits/pathfinding/grid.lua")
--   local g = Grid.new(20, 15, { diagonal = true })
--   g:setBlocked(5, 3, true) ; g:setCost(6, 3, 3)         -- mur, boue
--   local path = g:find(1, 1, 10, 8)                       -- { {x=1,y=1}, ..., {x=10,y=8} } ou nil
--   local zone = g:reachable(4, 4, 5)                      -- cases à 5 de coût au plus : zone["x,y"] = coût
local Grid = {}
Grid.__index = Grid

function Grid.new(width, height, options)
    options = options or {}
    return setmetatable({ w = width, h = height, diagonal = options.diagonal or false, cost = {}, blocked = {} }, Grid)
end
local function key(x, y) return x .. "," .. y end
function Grid:inside(x, y) return x >= 1 and y >= 1 and x <= self.w and y <= self.h end
function Grid:setBlocked(x, y, b) self.blocked[key(x, y)] = b or nil end
function Grid:isBlocked(x, y) return not self:inside(x, y) or self.blocked[key(x, y)] == true end
function Grid:setCost(x, y, c) self.cost[key(x, y)] = c end
function Grid:costAt(x, y) return self.cost[key(x, y)] or 1 end

function Grid:neighbors(x, y)
    local out = {}
    local dirs = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} }
    if self.diagonal then
        for _, d in ipairs({ {1, 1}, {1, -1}, {-1, 1}, {-1, -1} }) do dirs[#dirs + 1] = d end
    end
    for _, d in ipairs(dirs) do
        local nx, ny = x + d[1], y + d[2]
        if not self:isBlocked(nx, ny) then
            local diag = d[1] ~= 0 and d[2] ~= 0
            -- No corner cutting: both sides of a diagonal step must be free.
            if not diag or (not self:isBlocked(x + d[1], y) and not self:isBlocked(x, y + d[2])) then
                out[#out + 1] = { nx, ny, self:costAt(nx, ny) * (diag and 1.41421356 or 1) }
            end
        end
    end
    return out
end

-- Binary heap on f scores.
local function push(heap, node)
    heap[#heap + 1] = node
    local i = #heap
    while i > 1 do
        local p = i // 2
        if heap[p].f <= heap[i].f then break end
        heap[p], heap[i] = heap[i], heap[p]
        i = p
    end
end
local function pop(heap)
    local top = heap[1]
    heap[1] = heap[#heap]
    heap[#heap] = nil
    local i = 1
    while true do
        local l, r, m = i * 2, i * 2 + 1, i
        if heap[l] and heap[l].f < heap[m].f then m = l end
        if heap[r] and heap[r].f < heap[m].f then m = r end
        if m == i then break end
        heap[m], heap[i] = heap[i], heap[m]
        i = m
    end
    return top
end

function Grid:heuristic(ax, ay, bx, by)
    local dx, dy = math.abs(ax - bx), math.abs(ay - by)
    if self.diagonal then return math.max(dx, dy) + 0.41421356 * math.min(dx, dy) end
    return dx + dy
end

--- Shortest path from (sx, sy) to (tx, ty), both included; nil when unreachable.
function Grid:find(sx, sy, tx, ty)
    if self:isBlocked(sx, sy) or self:isBlocked(tx, ty) then return nil end
    local open, g, came = {}, { [key(sx, sy)] = 0 }, {}
    push(open, { x = sx, y = sy, f = self:heuristic(sx, sy, tx, ty) })
    local closed = {}
    while #open > 0 do
        local n = pop(open)
        local k = key(n.x, n.y)
        if n.x == tx and n.y == ty then
            local path = { { x = tx, y = ty } }
            while came[k] do
                local p = came[k]
                table.insert(path, 1, { x = p[1], y = p[2] })
                k = key(p[1], p[2])
            end
            return path, g[key(tx, ty)]
        end
        if not closed[k] then
            closed[k] = true
            for _, nb in ipairs(self:neighbors(n.x, n.y)) do
                local nk = key(nb[1], nb[2])
                local cost = g[k] + nb[3]
                if not g[nk] or cost < g[nk] then
                    g[nk] = cost
                    came[nk] = { n.x, n.y }
                    push(open, { x = nb[1], y = nb[2], f = cost + self:heuristic(nb[1], nb[2], tx, ty) })
                end
            end
        end
    end
    return nil
end

--- Cells reachable from (sx, sy) with a total cost <= budget: { ["x,y"] = cost }.
function Grid:reachable(sx, sy, budget)
    local best = { [key(sx, sy)] = 0 }
    local open = {}
    push(open, { x = sx, y = sy, f = 0 })
    while #open > 0 do
        local n = pop(open)
        if n.f <= best[key(n.x, n.y)] then
            for _, nb in ipairs(self:neighbors(n.x, n.y)) do
                local c = n.f + nb[3]
                local nk = key(nb[1], nb[2])
                if c <= budget and (not best[nk] or c < best[nk]) then
                    best[nk] = c
                    push(open, { x = nb[1], y = nb[2], f = c })
                end
            end
        end
    end
    return best
end

--- True when no blocked cell lies on the straight line between two cells (Bresenham).
function Grid:lineOfSight(ax, ay, bx, by)
    local dx, dy = math.abs(bx - ax), -math.abs(by - ay)
    local sx, sy = ax < bx and 1 or -1, ay < by and 1 or -1
    local err = dx + dy
    local x, y = ax, ay
    while true do
        if self:isBlocked(x, y) then return false end
        if x == bx and y == by then return true end
        local e2 = 2 * err
        if e2 >= dy then err = err + dy x = x + sx end
        if e2 <= dx then err = err + dx y = y + sy end
    end
end

return Grid
