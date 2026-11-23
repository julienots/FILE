-- Personnage vu de dessus (paquet officiel top-down-2d) : flèches / WASD / ZQSD ou manette,
-- Maj pour courir, Espace pour une esquive.
local TopDown = {
    properties = { speed = 4, runSpeed = 7, acceleration = 30, dashSpeed = 14, dashTime = 0.15, dashCooldown = 0.6, faceMovement = true },
    signals = { dashed = {} },
}

function TopDown:onStart()
    self.vx, self.vy = 0, 0
    self.dashLeft, self.cooldown = 0, 0
    self.lastX, self.lastY = 1, 0
end

function TopDown:onUpdate(dt)
    local ix = agr.input.axis("horizontal")
    local iy = agr.input.axis("vertical")
    if agr.input.gamepadCount() > 0 then
        local gx, gy = agr.input.gamepadAxis("LeftX"), -agr.input.gamepadAxis("LeftY")
        if math.abs(gx) + math.abs(gy) > math.abs(ix) + math.abs(iy) then ix, iy = gx, gy end
    end
    local len = math.sqrt(ix * ix + iy * iy)
    if len > 1 then ix, iy = ix / len, iy / len end
    if len > 0.1 then self.lastX, self.lastY = ix / math.max(len, 1e-6), iy / math.max(len, 1e-6) end

    self.cooldown = math.max(0, self.cooldown - dt)
    if self.dashLeft > 0 then
        self.dashLeft = self.dashLeft - dt
        self.vx, self.vy = self.lastX * self.dashSpeed, self.lastY * self.dashSpeed
    else
        if agr.input.keyPressed("Space") and self.cooldown == 0 then
            self.dashLeft, self.cooldown = self.dashTime, self.dashCooldown
            self.entity:emit("dashed")
        end
        local max = agr.input.keyDown("LeftShift") and self.runSpeed or self.speed
        local step = self.acceleration * dt
        local tx, ty = ix * max, iy * max
        self.vx = self.vx + math.max(-step, math.min(step, tx - self.vx))
        self.vy = self.vy + math.max(-step, math.min(step, ty - self.vy))
    end
    self.entity:translate(self.vx * dt, self.vy * dt, 0)
    if self.faceMovement and (math.abs(self.vx) + math.abs(self.vy)) > 0.05 then
        self.entity:setRotation(0, 0, math.deg(math.atan(self.vy, self.vx)))
    end
end

return TopDown
