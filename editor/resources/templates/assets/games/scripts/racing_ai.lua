-- Voiture adverse : suit les points du circuit sur sa voie, ralentit dans les virages serrés.
local G = require("assets://games/scripts/common.lua")

local Rival = { properties = { speed = 19, lane = -1.5, turnRate = 160 } }

function Rival:onStart()
    self.race = agr.find("Race"):script()
    self.startX, self.startY, self.startZ = self.entity:position()
    local _, yaw = self.entity:rotation()
    self.startYaw = yaw
    self:reset()
end

function Rival:reset()
    self.entity:setPosition(self.startX, self.startY, self.startZ)
    self.yaw, self.current = self.startYaw, 0
    self.entity:setRotation(0, self.yaw, 0)
    self.car = { entity = self.entity, next = 2, lap = 0, lapStart = 0 }
end

function Rival:onUpdate(dt)
    if not self.race or self.race.state ~= "racing" or self.car.finished then
        self.current = 0
        return
    end
    local wps = self.race.wps
    local a, b = wps[self.car.next], wps[self.car.next % #wps + 1]
    -- Cible : le prochain point décalé sur la voie (perpendiculaire au segment suivant).
    local dx, dz = b.x - a.x, b.z - a.z
    local l = math.max(0.01, math.sqrt(dx * dx + dz * dz))
    local tx, tz = a.x - dz / l * self.lane, a.z + dx / l * self.lane
    local x, y, z = self.entity:position()
    local want = math.deg(math.atan(tx - x, tz - z))
    local diff = ((want - self.yaw + 180) % 360) - 180
    local turn = math.max(-self.turnRate * dt, math.min(self.turnRate * dt, diff))
    self.yaw = self.yaw + turn
    local target = self.speed * (math.abs(diff) > 25 and 0.75 or 1)
    self.current = self.current + (target - self.current) * math.min(1, 2 * dt)
    local r = math.rad(self.yaw)
    self.entity:setPosition(x + math.sin(r) * self.current * dt, y, z + math.cos(r) * self.current * dt)
    self.entity:setRotation(0, self.yaw, 0)
    self.race:advance(self.car)
end

return Rival
