-- Caméra qui suit une entité horizontalement, avec un amorti et des limites (AGR ENGINE).
local Follow = {
    properties = {
        target = "Hero",   -- nom de l'entité suivie
        smoothing = 5,     -- plus grand = suit plus vite
        minX = -11,
        maxX = 11,
    },
}

function Follow:onStart()
    self.targetEntity = agr.find(self.target)
    if not self.targetEntity then agr.warn("camera_follow : entité '" .. self.target .. "' introuvable") end
end

function Follow:onUpdate(dt)
    local t = self.targetEntity
    if not t or not t:isValid() then return end
    local tx = t:position()
    local x, y, z = self.entity:position()
    local goal = math.max(self.minX, math.min(self.maxX, tx))
    x = x + (goal - x) * math.min(1, self.smoothing * dt)
    self.entity:setPosition(x, y, z)
end

return Follow
