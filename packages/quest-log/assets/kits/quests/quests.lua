-- Journal de quêtes (paquet officiel quest-log).
--   local Quests = require("assets://kits/quests/quests.lua")
--   local log = Quests.new({
--       wolves = { title = "Chasse", objectives = { { id = "kill", text = "Loups", count = 5 } }, reward = { gold = 50 } },
--       den    = { title = "La tanière", requires = { "wolves" }, objectives = { { id = "find", text = "Trouver la tanière" } } },
--   })
--   log.onCompleted = function(id, quest) agr.log("Quête finie : " .. quest.title) end
--   log:start("wolves") ; log:progress("wolves", "kill", 1)
local Quests = {}
Quests.__index = Quests

function Quests.new(definitions)
    return setmetatable({ defs = definitions, state = {} }, Quests)
end

function Quests:status(id)
    local s = self.state[id]
    return s and s.status or (self:available(id) and "available" or "locked")
end

function Quests:available(id)
    local def = self.defs[id]
    if not def then return false end
    for _, req in ipairs(def.requires or {}) do
        if not self.state[req] or self.state[req].status ~= "completed" then return false end
    end
    return true
end

function Quests:start(id)
    if self.state[id] or not self:available(id) then return false end
    local counts = {}
    for _, o in ipairs(self.defs[id].objectives or {}) do counts[o.id] = 0 end
    self.state[id] = { status = "active", counts = counts }
    if self.onStarted then self.onStarted(id, self.defs[id]) end
    return true
end

-- Avance un objectif ; la quête se termine quand tous sont atteints. Rend vrai si elle vient de finir.
function Quests:progress(id, objective, amount)
    local s = self.state[id]
    if not s or s.status ~= "active" then return false end
    local def = self.defs[id]
    for _, o in ipairs(def.objectives or {}) do
        if o.id == objective then s.counts[o.id] = math.min(o.count or 1, s.counts[o.id] + (amount or 1)) end
    end
    for _, o in ipairs(def.objectives or {}) do
        if s.counts[o.id] < (o.count or 1) then return false end
    end
    s.status = "completed"
    if self.onCompleted then self.onCompleted(id, def) end
    return true
end

function Quests:count(id, objective)
    local s = self.state[id]
    return s and s.counts[objective] or 0
end

function Quests:active()
    local out = {}
    for id, s in pairs(self.state) do
        if s.status == "active" then out[#out + 1] = id end
    end
    table.sort(out)
    return out
end

-- Progression seule (à passer à save-system), et restauration.
function Quests:save() return self.state end
function Quests:restore(state) self.state = state or {} end

return Quests
