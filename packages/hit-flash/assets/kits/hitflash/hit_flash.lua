-- Clignotement au coup (paquet officiel hit-flash). Sur une entité avec SpriteRenderer.
--   agr.find("Enemy"):script():flash()       -- ou relié au signal "damaged" du paquet health
local Flash = {
    properties = { color = "#ff4040", duration = 0.25, blinks = 3, shake = 0.08 },
}

local function parse(hex)
    local r, g, b = hex:match("#?(%x%x)(%x%x)(%x%x)")
    if not r then return { 1, 0.25, 0.25, 1 } end
    return { tonumber(r, 16) / 255, tonumber(g, 16) / 255, tonumber(b, 16) / 255, 1 }
end

function Flash:onStart()
    self.base = self.entity:get("SpriteRenderer", "color")
    self.time = 0
    local x, y, z = self.entity:position()
    self.home = { x, y, z }
    self.entity:connect("damaged", function() self:flash() end, self.entity)
end

function Flash:flash()
    if self.time <= 0 then
        local x, y, z = self.entity:position()
        self.home = { x, y, z }
    end
    self.time = self.duration
end

function Flash:onUpdate(dt)
    if self.time <= 0 then return end
    self.time = self.time - dt
    if self.time <= 0 then
        self.entity:set("SpriteRenderer", "color", self.base)
        self.entity:setPosition(self.home[1], self.home[2], self.home[3])
        return
    end
    local phase = math.floor((1 - self.time / self.duration) * self.blinks * 2)
    self.entity:set("SpriteRenderer", "color", phase % 2 == 0 and parse(self.color) or self.base)
    local s = self.shake * (self.time / self.duration)
    self.entity:setPosition(self.home[1] + (math.random() * 2 - 1) * s, self.home[2] + (math.random() * 2 - 1) * s, self.home[3])
end

return Flash
