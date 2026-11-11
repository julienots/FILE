-- Caméra de poursuite : derrière la voiture, un peu au-dessus, suit son cap en douceur.
local Chase = { properties = { target = "Player Car", distance = 8, height = 3.5, pitch = -14, smoothing = 6 } }

function Chase:onStart()
    self.t = agr.find(self.target)
    self:place(1)
end

function Chase:place(k)
    if not self.t then return end
    local tx, ty, tz = self.t:position()
    local _, yaw = self.t:rotation()
    self.yaw = self.yaw and self.yaw + (((yaw - self.yaw + 180) % 360) - 180) * k or yaw
    local r = math.rad(self.yaw)
    local gx, gy, gz = tx - math.sin(r) * self.distance, ty + self.height, tz - math.cos(r) * self.distance
    local x, y, z = self.entity:position()
    self.entity:setPosition(x + (gx - x) * k, y + (gy - y) * k, z + (gz - z) * k)
    self.entity:setRotation(self.pitch, self.yaw + 180, 0)
end

function Chase:onUpdate(dt) self:place(math.min(1, self.smoothing * dt)) end

return Chase
