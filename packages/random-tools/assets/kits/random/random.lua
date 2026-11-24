-- Hasard (paquet officiel random-tools) : local Random = require("assets://kits/random/random.lua")
--   local rng = Random.new(1234)          -- graine : mêmes tirages à chaque partie
--   rng:float() rng:range(1, 6) rng:pick(liste) rng:weighted({ commun = 70, rare = 25, epique = 5 })
--   rng:shuffle(t) ; local bag = rng:bag({ "I", "O", "T", "S", "Z", "J", "L" }) ; bag:next()
--   Random.noise2(x, y, graine)           -- bruit lisse 0..1 (terrains, nuages)
local Random = {}
Random.__index = Random

--- Seeded generator (xorshift32): independent of math.random, the same on every platform.
function Random.new(seed)
    local s = math.floor(seed or (agr and agr.time and agr.time.now and agr.time.now() * 1000) or 1) & 0xFFFFFFFF
    if s == 0 then s = 0x9E3779B9 end
    return setmetatable({ s = s }, Random)
end
function Random:next32()
    local x = self.s
    x = x ~ ((x << 13) & 0xFFFFFFFF)
    x = x ~ (x >> 17)
    x = x ~ ((x << 5) & 0xFFFFFFFF)
    self.s = x & 0xFFFFFFFF
    return self.s
end
--- 0 <= value < 1
function Random:float() return self:next32() / 4294967296 end
--- Integer in [a, b].
function Random:range(a, b) return a + math.floor(self:float() * (b - a + 1)) end
function Random:pick(list) return list[self:range(1, #list)] end
--- Key of `weights` ({ key = weight }) drawn by weight; keys sorted for reproducibility.
function Random:weighted(weights)
    local keys, total = {}, 0
    for k, w in pairs(weights) do keys[#keys + 1] = k total = total + w end
    table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
    local r = self:float() * total
    for _, k in ipairs(keys) do
        r = r - weights[k]
        if r < 0 then return k end
    end
    return keys[#keys]
end
function Random:shuffle(t)
    for i = #t, 2, -1 do
        local j = self:range(1, i)
        t[i], t[j] = t[j], t[i]
    end
    return t
end
--- Point uniformly inside a disc of `radius`.
function Random:inCircle(radius)
    local a, r = self:float() * 2 * math.pi, math.sqrt(self:float()) * (radius or 1)
    return math.cos(a) * r, math.sin(a) * r
end
--- Bag: every item once in a random order, then a new shuffled round (no long droughts).
function Random:bag(items)
    local rng = self
    local bag = { items = items, queue = {} }
    function bag:next()
        if #self.queue == 0 then
            for i, v in ipairs(self.items) do self.queue[i] = v end
            rng:shuffle(self.queue)
        end
        return table.remove(self.queue)
    end
    return bag
end

local function hash2(x, y, seed)
    local h = (x * 374761393 + y * 668265263 + (seed or 0) * 1442695041) & 0xFFFFFFFF
    h = ((h ~ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return (h ~ (h >> 16)) / 4294967295
end
local function smooth(t) return t * t * (3 - 2 * t) end
--- Smooth value noise in 0..1.
function Random.noise2(x, y, seed)
    local x0, y0 = math.floor(x), math.floor(y)
    local fx, fy = smooth(x - x0), smooth(y - y0)
    local a, b = hash2(x0, y0, seed), hash2(x0 + 1, y0, seed)
    local c, d = hash2(x0, y0 + 1, seed), hash2(x0 + 1, y0 + 1, seed)
    local top, bottom = a + (b - a) * fx, c + (d - c) * fx
    return top + (bottom - top) * fy
end
function Random.noise1(x, seed) return Random.noise2(x, 0.5, seed) end

return Random
