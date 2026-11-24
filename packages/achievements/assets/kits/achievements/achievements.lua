-- Succès (paquet officiel achievements) :
--   local Achievements = require("assets://kits/achievements/achievements.lua")
--   local a = Achievements.new({
--       { id = "first_coin", name = "Premier sou", description = "Ramasser une pièce" },
--       { id = "rich", name = "Riche", description = "1000 pièces", goal = 1000 },
--       { id = "secret", name = "???", hidden = true },
--   })
--   a:onUnlock(function(def) agr.log("Succès : " .. def.name) end)
--   a:progress("rich", 25)   -- cumule ; débloque à l'objectif
--   a:unlock("first_coin")
--   agr.save.set("achievements", a:toTable()) ; a:fromTable(agr.save.get("achievements"))
local Achievements = {}
Achievements.__index = Achievements

function Achievements.new(definitions)
    local self = setmetatable({ defs = {}, order = {}, state = {}, listeners = {} }, Achievements)
    for _, d in ipairs(definitions or {}) do
        assert(d.id, "achievement without id")
        self.defs[d.id] = d
        self.order[#self.order + 1] = d.id
        self.state[d.id] = { value = 0, unlocked = false }
    end
    return self
end
function Achievements:onUnlock(fn) self.listeners[#self.listeners + 1] = fn end

local function check(self, id)
    local d, s = self.defs[id], self.state[id]
    if s.unlocked then return false end
    if s.value >= (d.goal or 1) then
        s.unlocked = true
        for _, fn in ipairs(self.listeners) do fn(d) end
        return true
    end
    return false
end
--- Adds `amount` (default 1) to the progress; returns true when this unlocked it.
function Achievements:progress(id, amount)
    local s = self.state[id]
    assert(s, "unknown achievement '" .. tostring(id) .. "'")
    if s.unlocked then return false end
    s.value = math.min(s.value + (amount or 1), self.defs[id].goal or 1)
    return check(self, id)
end
function Achievements:unlock(id)
    local s = self.state[id]
    assert(s, "unknown achievement '" .. tostring(id) .. "'")
    s.value = self.defs[id].goal or 1
    return check(self, id)
end
function Achievements:isUnlocked(id) return self.state[id] ~= nil and self.state[id].unlocked end
--- Progress 0..1 of one achievement.
function Achievements:ratio(id) return self.state[id].value / (self.defs[id].goal or 1) end
--- Fraction of all achievements unlocked (0..1).
function Achievements:completion()
    local n = 0
    for _, id in ipairs(self.order) do if self.state[id].unlocked then n = n + 1 end end
    return #self.order == 0 and 0 or n / #self.order
end
--- For a list screen: hidden ones show "???" until unlocked.
function Achievements:list()
    local out = {}
    for _, id in ipairs(self.order) do
        local d, s = self.defs[id], self.state[id]
        local visible = s.unlocked or not d.hidden
        out[#out + 1] = { id = id, name = visible and d.name or "???", description = visible and (d.description or "") or "",
                          unlocked = s.unlocked, ratio = s.value / (d.goal or 1) }
    end
    return out
end
function Achievements:toTable()
    local t = {}
    for id, s in pairs(self.state) do t[id] = { value = s.value, unlocked = s.unlocked } end
    return t
end
--- Restores saved progress (unknown ids ignored; no notification for already unlocked ones).
function Achievements:fromTable(t)
    for id, s in pairs(t or {}) do
        if self.state[id] then self.state[id] = { value = s.value or 0, unlocked = s.unlocked == true } end
    end
end

return Achievements
