-- Contrôleur FPS (paquet officiel fps-controller).
-- Entité : CharacterController3D ; caméra en enfant nommée par `camera` (par défaut "Camera").
local FPS = {
    properties = { speed = 4.5, sprint = 7.5, jumpSpeed = 5.0, gravity = 18.0, sensitivity = 0.15, stickSensitivity = 140, camera = "Camera" },
}

function FPS:onStart()
    self.yaw, self.pitch = 0, 0
    self.vy = 0
    self.cam = self.entity:find(self.camera)
    self.lastX, self.lastY = agr.input.mousePosition()
end

function FPS:onUpdate(dt)
    -- Regard : souris (clic droit maintenu, pour ne pas gêner l'interface) ou stick droit.
    local mx, my = agr.input.mousePosition()
    if agr.input.mouseDown(2) and not agr.ui.pointerOverUI() then
        self.yaw = self.yaw - (mx - self.lastX) * self.sensitivity
        self.pitch = self.pitch - (my - self.lastY) * self.sensitivity
    end
    self.lastX, self.lastY = mx, my
    if agr.input.gamepadCount() > 0 then
        self.yaw = self.yaw - agr.input.gamepadAxis("RightX") * self.stickSensitivity * dt
        self.pitch = self.pitch - agr.input.gamepadAxis("RightY") * self.stickSensitivity * dt
    end
    self.pitch = math.max(-85, math.min(85, self.pitch))
    self.entity:setRotation(0, self.yaw, 0)
    if self.cam then self.cam:setRotation(self.pitch, 0, 0) end

    -- Déplacement dans le repère du regard (horizontal).
    local ix, iz = agr.input.axis("horizontal"), agr.input.axis("vertical")
    if agr.input.gamepadCount() > 0 then
        ix = ix + agr.input.gamepadAxis("LeftX")
        iz = iz - agr.input.gamepadAxis("LeftY")
    end
    local len = math.sqrt(ix * ix + iz * iz)
    if len > 1 then ix, iz = ix / len, iz / len end
    local r = math.rad(self.yaw)
    local fx, fz = -math.sin(r), -math.cos(r)   -- avant (-Z tourné)
    local rx, rz = math.cos(r), -math.sin(r)    -- droite
    local running = agr.input.keyDown("LeftShift") or agr.input.gamepadDown("West")
    local v = running and self.sprint or self.speed
    local vx = (fx * iz + rx * ix) * v
    local vz = (fz * iz + rz * ix) * v

    if self.entity:isGrounded() then
        self.vy = -1
        if agr.input.keyPressed("Space") or agr.input.gamepadPressed("South") then self.vy = self.jumpSpeed end
    else
        self.vy = self.vy - self.gravity * dt
    end
    self.entity:setVelocity(vx, self.vy, vz)
end

return FPS
