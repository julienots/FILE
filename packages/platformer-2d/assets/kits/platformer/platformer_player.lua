-- Personnage de plateforme 2D (paquet officiel platformer-2d).
local P = {
    properties = { maxSpeed = 6, accel = 40, decel = 50, jumpSpeed = 11, coyoteTime = 0.1, jumpBuffer = 0.12, cutJump = 0.45, fallGravity = 1.6 },
}

function P:onStart()
    self.coyote, self.buffer = 0, 0
    self.facing = 1
end

local function approach(v, target, step)
    if v < target then return math.min(v + step, target) end
    return math.max(v - step, target)
end

function P:grounded()
    local x, y = self.entity:worldPosition()
    local hit = agr.physics2d.raycast(x, y, 0, -1, 0.6)
    return hit ~= nil and hit.entity ~= self.entity
end

function P:onUpdate(dt)
    local vx, vy = self.entity:velocity()
    local input = agr.input.axis("horizontal")
    if agr.input.gamepadCount() > 0 then input = input + agr.input.gamepadAxis("LeftX") end
    input = math.max(-1, math.min(1, input))
    if input ~= 0 then self.facing = input > 0 and 1 or -1 end
    vx = approach(vx, input * self.maxSpeed, (input ~= 0 and self.accel or self.decel) * dt)

    local onGround = self:grounded()
    self.coyote = onGround and self.coyoteTime or math.max(0, self.coyote - dt)
    local jumpPressed = agr.input.keyPressed("Space") or agr.input.keyPressed("Up") or agr.input.gamepadPressed("South")
    local jumpHeld = agr.input.keyDown("Space") or agr.input.keyDown("Up") or agr.input.gamepadDown("South")
    self.buffer = jumpPressed and self.jumpBuffer or math.max(0, self.buffer - dt)
    if self.buffer > 0 and self.coyote > 0 then
        vy = self.jumpSpeed
        self.buffer, self.coyote = 0, 0
        self.entity:emit("jumped")
    end
    if not jumpHeld and vy > 0 then vy = vy * (1 - self.cutJump) end   -- relâcher tôt : saut plus court
    if vy < 0 then vy = vy - 9.81 * (self.fallGravity - 1) * dt end   -- chute plus franche
    self.entity:setVelocity(vx, vy)
    self.entity:setScale(self.facing, 1, 1)
end

return P
