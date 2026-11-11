-- Caméra 3D à la troisième personne : suit la cible avec un décalage (au-dessus, derrière),
-- regarde légèrement au-dessus d'elle. Q / A et R tournent autour.
local Follow = { properties = { target = "Hero", offsetY = 8, offsetZ = 9, smoothing = 5, lookHeight = 1.2, turnSpeed = 90 } }

function Follow:onStart()
    self.t = agr.find(self.target)
    self.yaw = 0
    if self.t then
        local x, y, z = self:goal()
        self.entity:setPosition(x, y, z)
        self:aim()
    end
end

function Follow:goal()
    local tx, ty, tz = self.t:position()
    local r = math.rad(self.yaw)
    return tx + math.sin(r) * self.offsetZ, ty + self.offsetY, tz + math.cos(r) * self.offsetZ
end

function Follow:aim()
    local pitch = -math.deg(math.atan(self.offsetY - self.lookHeight, self.offsetZ))
    self.entity:setRotation(pitch, self.yaw, 0)
end

function Follow:onUpdate(dt)
    if not self.t or not self.t:isValid() then return end
    if agr.input.keyDown("Q") or agr.input.keyDown("A") then self.yaw = self.yaw - self.turnSpeed * dt end
    if agr.input.keyDown("R") then self.yaw = self.yaw + self.turnSpeed * dt end
    local gx, gy, gz = self:goal()
    local x, y, z = self.entity:position()
    local k = math.min(1, self.smoothing * dt)
    self.entity:setPosition(x + (gx - x) * k, y + (gy - y) * k, z + (gz - z) * k)
    self:aim()
end

return Follow
