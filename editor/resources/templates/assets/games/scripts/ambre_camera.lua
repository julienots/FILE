-- Caméra 2.5D des Ruines d'Ambre : suit le héros en restant inclinée (même angle que les sprites),
-- sans sortir de la zone courante.
local Cam = { properties = { target = "Hero", height = 10, back = 12, smoothing = 5, spacing = 70 } }

function Cam:onStart()
    self.t = agr.find(self.target)
    self.entity:setRotation(-math.deg(math.atan(self.height, self.back)), 0, 0)
    if self.t then
        local x, y, z = self:goal()
        self.entity:setPosition(x, y, z)
    end
end

function Cam:goal()
    local tx, ty, tz = self.t:position()
    local area = math.floor((tx + self.spacing / 2) / self.spacing)
    local cx = area * self.spacing
    tx = math.max(cx - 7, math.min(cx + 12, tx))
    return tx, ty + self.height, math.max(-4, math.min(6, tz)) + self.back
end

function Cam:onUpdate(dt)
    if not self.t or not self.t:isValid() then return end
    local gx, gy, gz = self:goal()
    local x, y, z = self.entity:position()
    local k = math.min(1, self.smoothing * dt)
    if math.abs(gx - x) > 30 then k = 1 end  -- new level: jump there
    self.entity:setPosition(x + (gx - x) * k, y + (gy - y) * k, z + (gz - z) * k)
end

return Cam
