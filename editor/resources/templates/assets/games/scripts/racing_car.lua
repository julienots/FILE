-- Voiture du joueur (arcade) : gaz / frein (action move_y, flèches, ZQSD, boutons Gaz / Frein),
-- direction (move_x, boutons < >). Le corps physique garde la gravité et les collisions ; la
-- vitesse et le cap sont pilotés par ce script. Plus lente hors de la route.
local Car = {
    properties = { maxSpeed = 26, accel = 12, brakePower = 24, drag = 4, turnRate = 100, offRoadFactor = 0.45, roadHalfWidth = 4.3 },
}

function Car:onStart()
    self.race = agr.find("Race"):script()
    self.startX, self.startY, self.startZ = self.entity:position()
    local _, yaw = self.entity:rotation()
    self.startYaw = yaw
    self:reset()
end

function Car:reset()
    self.entity:setPosition(self.startX, self.startY, self.startZ)
    self.entity:setVelocity(0, 0, 0)
    self.yaw, self.speed = self.startYaw, 0
    self.entity:setRotation(0, self.yaw, 0)
    self.car = { entity = self.entity, next = 2, lap = 0, lapStart = 0, player = true }
end

local function held(name)
    local b = agr.find(name)
    return b ~= nil and agr.ui.isPressed(b)
end

function Car:onUpdate(dt)
    local e = self.entity
    local _, vy = e:velocity()
    if not self.race or self.race.state ~= "racing" then
        self.speed = 0
        e:setVelocity(0, vy, 0)
        return
    end
    local throttle, steer = agr.input.value("move_y"), agr.input.value("move_x")
    if held("Gas") then throttle = 1 elseif held("Brake") then throttle = -1 end
    if held("Left") then steer = -1 elseif held("Right") then steer = 1 end
    local x, _, z = e:position()
    self.offRoad = self.race:distanceToTrack(x, z) > self.roadHalfWidth
    local max = self.maxSpeed * (self.offRoad and self.offRoadFactor or 1)
    if throttle > 0 then self.speed = self.speed + self.accel * throttle * dt
    elseif throttle < 0 then self.speed = self.speed + self.brakePower * throttle * dt
    else self.speed = self.speed - math.min(math.abs(self.speed), self.drag * dt) * (self.speed > 0 and 1 or -1) end
    if self.speed > max then self.speed = math.max(max, self.speed - 25 * dt) end
    self.speed = math.max(-6, self.speed)
    local grip = math.max(-1, math.min(1, self.speed / 8))
    self.yaw = self.yaw + steer * self.turnRate * dt * grip
    local r = math.rad(self.yaw)
    e:setVelocity(math.sin(r) * self.speed, vy, math.cos(r) * self.speed)
    e:setRotation(0, self.yaw, 0)
    self.race:advance(self.car)
end

return Car
