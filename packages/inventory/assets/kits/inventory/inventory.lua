-- Inventaire (paquet officiel inventory). Module :
--   local Inventory = require("assets://kits/inventory/inventory.lua")
--   local inv = Inventory.new(20, "assets://kits/inventory/items.json")
--   inv:add("potion", 3) ; inv:count("potion") ; inv:remove("potion", 1) ; inv:save("bag") ; inv:load("bag")
local Inventory = {}
Inventory.__index = Inventory

function Inventory.new(slots, itemsUri)
    local defs = {}
    if itemsUri then
        local text = agr.file.read(itemsUri)
        local data = text and agr.json.decode(text)
        if data then defs = data end
    end
    return setmetatable({ slots = {}, size = slots or 20, defs = defs, listeners = {} }, Inventory)
end

function Inventory:maxStack(id) return (self.defs[id] and self.defs[id].stack) or 99 end
function Inventory:onChange(fn) self.listeners[#self.listeners + 1] = fn end
local function changed(self, id) for _, fn in ipairs(self.listeners) do fn(id, self:count(id)) end end

-- Ajoute `n` objets ; rend le nombre qui n'a pas trouvé de place.
function Inventory:add(id, n)
    n = n or 1
    local max = self:maxStack(id)
    for i = 1, self.size do
        local s = self.slots[i]
        if s and s.id == id and s.n < max then
            local put = math.min(n, max - s.n)
            s.n, n = s.n + put, n - put
            if n == 0 then break end
        end
    end
    for i = 1, self.size do
        if n == 0 then break end
        if not self.slots[i] then
            local put = math.min(n, max)
            self.slots[i] = { id = id, n = put }
            n = n - put
        end
    end
    changed(self, id)
    return n
end

function Inventory:count(id)
    local total = 0
    for i = 1, self.size do
        local s = self.slots[i]
        if s and s.id == id then total = total + s.n end
    end
    return total
end

-- Retire `n` objets si possible (tout ou rien) ; rend true / false.
function Inventory:remove(id, n)
    n = n or 1
    if self:count(id) < n then return false end
    for i = self.size, 1, -1 do
        local s = self.slots[i]
        if s and s.id == id then
            local take = math.min(n, s.n)
            s.n, n = s.n - take, n - take
            if s.n == 0 then self.slots[i] = nil end
            if n == 0 then break end
        end
    end
    changed(self, id)
    return true
end

function Inventory:transfer(other, id, n)
    n = math.min(n or 1, self:count(id))
    if n <= 0 then return 0 end
    local left = other:add(id, n)
    self:remove(id, n - left)
    return n - left
end

function Inventory:value()
    local v = 0
    for i = 1, self.size do
        local s = self.slots[i]
        if s then v = v + ((self.defs[s.id] and self.defs[s.id].value) or 0) * s.n end
    end
    return v
end

function Inventory:save(key)
    local list = {}
    for i = 1, self.size do
        local s = self.slots[i]
        if s then list[#list + 1] = { slot = i, id = s.id, n = s.n } end
    end
    agr.save.set(key, list)
end

function Inventory:load(key)
    self.slots = {}
    for _, s in ipairs(agr.save.get(key, {})) do self.slots[s.slot] = { id = s.id, n = s.n } end
end

return Inventory
