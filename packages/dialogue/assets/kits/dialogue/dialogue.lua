-- Système de dialogues (paquet officiel dialogue). Module à charger avec require :
--   local Dialogue = require("assets://kits/dialogue/dialogue.lua")
--   local d = Dialogue.load("assets://kits/dialogue/example.json")
--   d:start("hello") ; d:current() -> { speaker, text, choices = { {text, next}, ... } } ; d:choose(1)
local Dialogue = {}
Dialogue.__index = Dialogue

function Dialogue.load(uri)
    local text = agr.file.read(uri)
    if not text then return nil, "dialogue introuvable : " .. tostring(uri) end
    local data, err = agr.json.decode(text)
    if not data then return nil, err end
    return setmetatable({ nodes = data.nodes or {}, vars = data.vars or {}, node = nil }, Dialogue)
end

local function allowed(self, cond)
    if not cond then return true end
    local v = self.vars[cond.var]
    if cond.equals ~= nil then return v == cond.equals end
    if cond.atLeast ~= nil then return (v or 0) >= cond.atLeast end
    return v ~= nil and v ~= false
end

local function apply(self, action)
    if not action then return end
    if action.set then for k, v in pairs(action.set) do self.vars[k] = v end end
    if action.add then for k, v in pairs(action.add) do self.vars[k] = (self.vars[k] or 0) + v end end
    if action.event then agr.events.emit(action.event, self) end
end

function Dialogue:start(id)
    self.node = self.nodes[id]
    if self.node then apply(self, self.node.action) end
    return self:current()
end

function Dialogue:current()
    local n = self.node
    if not n then return nil end
    local choices = {}
    for _, c in ipairs(n.choices or {}) do
        if allowed(self, c.condition) then choices[#choices + 1] = { text = c.text, next = c.next, action = c.action } end
    end
    return { speaker = n.speaker or "", text = n.text or "", choices = choices, next = n.next }
end

-- Choix i (ou la suite d'une réplique sans choix quand i est nil). Rend la nouvelle réplique, ou nil à la fin.
function Dialogue:choose(i)
    local cur = self:current()
    if not cur then return nil end
    local target = cur.next
    if i and cur.choices[i] then
        apply(self, cur.choices[i].action)
        target = cur.choices[i].next
    end
    if not target then self.node = nil return nil end
    return self:start(target)
end

function Dialogue:finished() return self.node == nil end

return Dialogue
