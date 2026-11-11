-- Caméra 2D qui suit une entité sur les deux axes, avec amorti et limites (bords de la carte).
local Follow = {
    properties = { target = "Hero", smoothing = 6, minX = -1000, maxX = 1000, minY = -1000, maxY = 1000 },
}

function Follow:onStart()
    self.t = agr.find(self.target)
    if self.t then  -- démarre centrée sur la cible
        local tx, ty = self.t:position()
        local _, _, z = self.entity:position()
        self.entity:setPosition(math.max(self.minX, math.min(self.maxX, tx)), math.max(self.minY, math.min(self.maxY, ty)), z)
    end
end

function Follow:onUpdate(dt)
    if not self.t or not self.t:isValid() then return end
    local tx, ty = self.t:position()
    local x, y, z = self.entity:position()
    local k = math.min(1, self.smoothing * dt)
    x = x + (math.max(self.minX, math.min(self.maxX, tx)) - x) * k
    y = y + (math.max(self.minY, math.min(self.maxY, ty)) - y) * k
    self.entity:setPosition(x, y, z)
end

return Follow
