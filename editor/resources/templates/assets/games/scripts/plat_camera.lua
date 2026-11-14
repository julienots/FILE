-- Caméra du jeu de plateforme : suit le héros (un peu devant lui) dans les limites du niveau
-- courant, fixées par plat_game.lua (minX, maxX, minY, maxY).
local Cam = { properties = { target = "Hero", smoothing = 6, minX = 0, maxX = 100, minY = 0, maxY = 10 } }

function Cam:onStart()
    self.t = agr.find(self.target)
    self.look = 0
end

function Cam:snap()
    local gx, gy = self:goal()
    local _, _, z = self.entity:position()
    self.entity:setPosition(gx, gy, z)
end

function Cam:goal()
    local tx, ty = self.t:position()
    local s = self.t:script()
    self.look = self.look + (((s and s.facing) or 1) * 2 - self.look) * 0.04
    return math.max(self.minX, math.min(self.maxX, tx + self.look)), math.max(self.minY, math.min(self.maxY, ty + 1.5))
end

function Cam:onUpdate(dt)
    if not self.t or not self.t:isValid() then return end
    local gx, gy = self:goal()
    local x, y, z = self.entity:position()
    local k = math.min(1, self.smoothing * dt)
    if math.abs(gy - y) > 20 then return self:snap() end
    self.entity:setPosition(x + (gx - x) * k, y + (gy - y) * k * 0.8, z)
end

return Cam
