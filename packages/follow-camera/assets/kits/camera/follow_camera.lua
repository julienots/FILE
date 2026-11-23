-- Caméra de suivi (paquet officiel follow-camera). Sur l'entité caméra.
local Cam = {
    properties = { target = "Player", smooth = 6, lookAhead = 1.5, deadZone = 0.3, offsetY = 1, offsetZ = 0,
                   minX = -1000, maxX = 1000, minY = -1000, maxY = 1000, shakeDecay = 1.5, shakeAmount = 0.4 },
}

function Cam:onStart()
    self.t = agr.find(self.target)
    self.trauma = 0
    self.lastTx = nil
    local x, y, z = self.entity:position()
    self.z = z
    self.x, self.y = x, y
end

-- Autres scripts : agr.find("Camera"):script():shake(0.6)
function Cam:shake(amount) self.trauma = math.min(1, self.trauma + (amount or 0.5)) end

function Cam:onUpdate(dt)
    if not (self.t and self.t:isValid()) then self.t = agr.find(self.target) return end
    local tx, ty = self.t:worldPosition()
    local vx = self.lastTx and (tx - self.lastTx) / math.max(dt, 1e-4) or 0
    self.lastTx = tx
    local ahead = math.max(-1, math.min(1, vx / 5)) * self.lookAhead
    local gx, gy = tx + ahead, ty + self.offsetY
    if math.abs(gx - self.x) > self.deadZone or math.abs(gy - self.y) > self.deadZone then
        local k = 1 - math.exp(-self.smooth * dt)
        self.x = self.x + (gx - self.x) * k
        self.y = self.y + (gy - self.y) * k
    end
    self.x = math.max(self.minX, math.min(self.maxX, self.x))
    self.y = math.max(self.minY, math.min(self.maxY, self.y))
    local sx, sy = 0, 0
    if self.trauma > 0 then
        local s = self.trauma * self.trauma * self.shakeAmount
        sx, sy = (math.random() * 2 - 1) * s, (math.random() * 2 - 1) * s
        self.trauma = math.max(0, self.trauma - self.shakeDecay * dt)
    end
    self.entity:setPosition(self.x + sx, self.y + sy, self.z + self.offsetZ)
end

return Cam
