-- Plateforme mobile (paquet officiel moving-platform). Sur l'entité plateforme (RigidBody2D / RigidBody
-- cinématique pour porter les personnages). Les points : entités nommées "<nom>_Point1", "<nom>_Point2"...
-- (placées dans la scène, hors de la plateforme), sinon un aller-retour de `distance` vers la droite.
local Platform = {
    properties = { speed = 2, pause = 0.5, loop = false, distance = 4, startDelay = 0 },
}

function Platform:onStart()
    local x, y, z = self.entity:position()
    self.points = {}
    local name = self.entity:name()
    for i = 1, 32 do
        local p = agr.find(name .. "_Point" .. i)
        if not p then break end
        local px, py, pz = p:worldPosition()
        self.points[#self.points + 1] = { px, py, pz }
    end
    if #self.points < 2 then self.points = { { x, y, z }, { x + self.distance, y, z } } end
    self.index, self.dir, self.wait = 1, 1, self.startDelay
    self.entity:setPosition(self.points[1][1], self.points[1][2], self.points[1][3])
end

function Platform:onUpdate(dt)
    if self.wait > 0 then self.wait = self.wait - dt return end
    local nextIndex = self.index + self.dir
    if nextIndex > #self.points or nextIndex < 1 then
        if self.loop then nextIndex = 1 else self.dir = -self.dir nextIndex = self.index + self.dir end
    end
    local x, y, z = self.entity:position()
    local t = self.points[nextIndex]
    local dx, dy, dz = t[1] - x, t[2] - y, t[3] - z
    local d = math.sqrt(dx * dx + dy * dy + dz * dz)
    local step = self.speed * dt
    if d <= step then
        self.entity:setPosition(t[1], t[2], t[3])
        self.index, self.wait = nextIndex, self.pause
        if self.loop and nextIndex == 1 then self.dir = 1 end
    else
        self.entity:setPosition(x + dx / d * step, y + dy / d * step, z + dz / d * step)
    end
end

return Platform
