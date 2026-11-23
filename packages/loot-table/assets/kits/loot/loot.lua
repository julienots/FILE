-- Tables de butin (paquet officiel loot-table).
--   local Loot = require("assets://kits/loot/loot.lua")
--   local chest = Loot.new({
--       { item = "coin",   weight = 70, min = 5, max = 20 },
--       { item = "potion", weight = 25 },
--       { item = "sword",  weight = 5, rare = true },
--   }, { seed = 42, pity = 20 })       -- au plus 20 tirages sans objet rare
--   for _, drop in ipairs(chest:roll(3)) do print(drop.item, drop.count) end
local Loot = {}
Loot.__index = Loot

function Loot.new(entries, options)
    options = options or {}
    local total = 0
    for _, e in ipairs(entries) do total = total + (e.weight or 1) end
    assert(total > 0, "table de butin vide")
    return setmetatable({ entries = entries, total = total, state = (options.seed or math.floor(agr.time.now())) % 2147483647,
                          pity = options.pity, sinceRare = 0 }, Loot)
end

-- Générateur reproductible (Park-Miller) : la même graine donne les mêmes butins.
function Loot:random()
    if self.state <= 0 then self.state = self.state + 2147483646 end
    self.state = (self.state * 48271) % 2147483647
    return self.state / 2147483647
end

local function pick(self, onlyRare)
    local total = 0
    for _, e in ipairs(self.entries) do
        if not onlyRare or e.rare then total = total + (e.weight or 1) end
    end
    local r = self:random() * total
    for _, e in ipairs(self.entries) do
        if not onlyRare or e.rare then
            r = r - (e.weight or 1)
            if r <= 0 then return e end
        end
    end
    return self.entries[#self.entries]
end

function Loot:rollOne()
    local hasRare = false
    for _, e in ipairs(self.entries) do hasRare = hasRare or e.rare end
    local forced = hasRare and self.pity and self.sinceRare + 1 >= self.pity
    local e = pick(self, forced)
    if e.rare then self.sinceRare = 0 else self.sinceRare = self.sinceRare + 1 end
    local lo, hi = e.min or 1, e.max or e.min or 1
    return { item = e.item, count = lo + math.floor(self:random() * (hi - lo + 1)), rare = e.rare == true }
end

function Loot:roll(n)
    local out = {}
    for i = 1, n or 1 do out[i] = self:rollOne() end
    return out
end

-- Probabilité d'un objet par tirage (sans la pitié), pour l'afficher au joueur.
function Loot:chance(item)
    for _, e in ipairs(self.entries) do
        if e.item == item then return (e.weight or 1) / self.total end
    end
    return 0
end

return Loot
