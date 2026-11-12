-- Héros des Ruines d'Ambre. Déplacement : flèches / ZQSD / stick (manette ou tactile) ; saut :
-- Espace, bouton A ou « Saut » ; épée : J, K, bouton X ou « Épée » ; roulade (invulnérable) :
-- Maj, L, bouton B ou « Roulade ». Le sprite (enfant « Sprite ») est animé par sa machine à états
-- (paramètres speed et grounded) ; ombre au sol, arc d'épée, impacts, recul.
local G = require("assets://games/scripts/common.lua")

local Hero = {
    properties = { speed = 5.5, jumpSpeed = 7.5, maxHealth = 5, attackRange = 2.0, attackCooldown = 0.28,
                   dashSpeed = 15, dashTime = 0.18, dashCooldown = 0.7, fallLimit = -8 },
}

function Hero:onStart()
    self.game = agr.find("Game"):script()
    self.sprite = self.entity:find("Sprite")
    self.shadow = self.entity:find("Shadow")
    self.slash = self.entity:find("Slash")
    self.health, self.facing = self.maxHealth, 1
    self.cooldown, self.invulnerable, self.swing, self.dash, self.dashCd, self.knock = 0, 0, 0, 0, 0, 0
    self.dashX, self.dashZ = 1, 0
    self.spawnX, self.spawnY, self.spawnZ = self.entity:position()
    self.grounded, self.airSpeed = true, 0
end

function Hero:spawn(x, y, z)
    self.spawnX, self.spawnY, self.spawnZ = x, y, z
    self.entity:setPosition(x, y, z)
    self.entity:setVelocity(0, 0, 0)
    self.health = self.maxHealth
    self.invulnerable, self.dash, self.knock = 1, 0, 0
end

function Hero:button(name)
    local b = agr.find(name)
    local down = b ~= nil and b:isActive() and agr.ui.isPressed(b)
    self.held = self.held or {}
    local was = self.held[name]
    self.held[name] = down
    return down and not was
end

-- Petit écrasement du sprite (saut, atterrissage).
function Hero:squash(sx, sy)
    if not self.sprite then return end
    agr.tween(self.sprite, { scale = { 1, 1, 1 } }, 0.25, { from = { sx, sy, 1 }, ease = "outBack" })
end

function Hero:onUpdate(dt)
    local e = self.entity
    local _, vy = e:velocity()
    self:updateStick()
    if not self.game or self.game.state ~= "playing" then
        e:setVelocity(0, math.min(vy, 0), 0)
        if self.sprite then self.sprite:setParameter("speed", 0) end
        return
    end
    self.cooldown = self.cooldown - dt
    self.invulnerable = math.max(0, self.invulnerable - dt)
    self.swing = math.max(0, self.swing - dt)
    self.dash = self.dash - dt
    self.dashCd = self.dashCd - dt
    self.knock = self.knock - dt
    local x, y, z = e:position()
    -- Sol : rayon depuis juste sous les pieds (un rayon parti de l'intérieur de la capsule la toucherait).
    local hit = agr.physics3d.raycast(x, y - 0.03, z, 0, -1, 0, 12)
    local wasGrounded = self.grounded
    self.grounded = hit ~= nil and hit.entity ~= e and hit.distance < 0.2
    if self.grounded and not wasGrounded and self.airSpeed < -4 then
        self:squash(1.3, 0.75)
        agr.fx.burst(x, 0.1, z, { count = 10, color = { 0.8, 0.7, 0.55 }, speed = 2.5, life = 0.4, size = 0.18, gravity = -2, additive = false })
    end
    self.airSpeed = vy
    if self.shadow then  -- l'ombre reste au sol et rapetisse en l'air
        local floor = hit and (y - 0.03 - hit.distance) or (y - 50)
        local h = math.max(0, y - floor)
        self.shadow:setPosition(0, -h + 0.02, 0)
        local k = math.max(0.4, 1 - h * 0.18)
        self.shadow:setScale(k, k, 1)
    end
    local mx, mz = agr.input.value("move_x"), -agr.input.value("move_y")
    local len = math.sqrt(mx * mx + mz * mz)
    if len > 1 then mx, mz, len = mx / len, mz / len, 1 end
    if (agr.input.keyPressed("Space") or agr.input.gamepadPressed("South") or self:button("Jump")) and self.grounded then
        vy = self.jumpSpeed
        self:squash(0.75, 1.3)
        agr.audio.play("assets://audio/jump.wav", { volume = 0.5 })
    end
    if (agr.input.keyPressed("LeftShift") or agr.input.keyPressed("L") or agr.input.gamepadPressed("East") or self:button("Dash"))
        and self.dashCd <= 0 then
        self.dash, self.dashCd = self.dashTime, self.dashCooldown
        if len > 0.1 then self.dashX, self.dashZ = mx / len, mz / len else self.dashX, self.dashZ = self.facing, 0 end
        self.invulnerable = math.max(self.invulnerable, self.dashTime + 0.08)
        self:squash(1.35, 0.7)
        agr.audio.play("assets://audio/swing.wav", { volume = 0.5, pitch = 1.5 })
        agr.fx.burst(x, 0.2, z, { count = 12, color = { 0.9, 0.85, 0.7 }, speed = 3, life = 0.35, size = 0.2, gravity = 0, additive = false })
    end
    if self.knock > 0 then
        -- recul : la physique garde la vitesse donnée par damage()
    elseif self.dash > 0 then
        e:setVelocity(self.dashX * self.dashSpeed, math.min(vy, 0), self.dashZ * self.dashSpeed)
    else
        e:setVelocity(mx * self.speed, vy, mz * self.speed)
    end
    if mx > 0.1 then self.facing = 1 elseif mx < -0.1 then self.facing = -1 end
    if agr.input.keyPressed("J") or agr.input.keyPressed("K") or agr.input.gamepadPressed("West") or self:button("Attack") then self:attack() end
    if self.sprite then
        self.sprite:set("SpriteRenderer", "flipX", self.facing < 0)
        self.sprite:setParameter("speed", len > 0.1 and 1 or 0)
        self.sprite:setParameter("grounded", self.grounded)
        local blink = self.invulnerable > 0 and self.dash <= 0 and math.floor(self.invulnerable * 14) % 2 == 0
        self.sprite:set("SpriteRenderer", "color", blink and { 1, 1, 1, 0.3 } or { 1, 1, 1, 1 })
    end
    if y < self.fallLimit then
        self:damage(1, nil, true)
        e:setPosition(self.spawnX, self.spawnY + 0.5, self.spawnZ)
        e:setVelocity(0, 0, 0)
    end
end

-- Le bouton du stick tactile suit le pouce (et le clavier / la manette).
function Hero:updateStick()
    local knob = agr.find("StickKnob")
    if knob then knob:set("UIRect", "position", { agr.input.value("move_x") * 70, -agr.input.value("move_y") * 70 }) end
end

function Hero:attack()
    if self.cooldown > 0 then return end
    self.cooldown, self.swing = self.attackCooldown, 0.18
    agr.audio.play("assets://audio/swing.wav", { volume = 0.7, pitch = 0.9 + math.random() * 0.2 })
    if self.slash then
        self.slash:setPosition(0.9 * self.facing, 0.75, 0)
        self.slash:setRotation(-38, 0, -90 * self.facing)
        agr.tween(self.slash, { alpha = 0 }, 0.22, { from = 1, ease = "inQuad" })
        agr.tween(self.slash, { scale = { 1, 1, 1 } }, 0.15, { from = { 0.5, 0.5, 1 }, ease = "outBack" })
    end
    if self.sprite then self:squash(1.2, 0.9) end
    local x, _, z = self.entity:position()
    local hits = 0
    for _, foe in ipairs(self.game:enemies()) do
        local s = foe:script()
        if s and not s.dead then
            local fx, _, fz = foe:worldPosition()
            local reach = self.attackRange + (s.kind == "boss" and 1.4 or 0)
            local inFront = (fx - x) * self.facing > -0.6 or s.kind == "boss"
            if inFront and G.distance(x, z, fx, fz) < reach then
                s:hit(1, self.entity)
                hits = hits + 1
            end
        end
    end
    if hits > 0 then
        agr.fx.hitStop(0.06)
        agr.fx.shake(0.2, 0.2)
        agr.audio.play("assets://audio/hit.wav", { volume = 0.8 })
    end
end

-- Dégâts reçus : clignotement, flash rouge, tremblement, recul depuis `from`.
function Hero:damage(n, from, ignoreInvulnerability)
    if self.game.state ~= "playing" or (self.invulnerable > 0 and not ignoreInvulnerability) then return end
    self.health = self.health - n
    self.invulnerable = 1.2
    agr.device.vibrate(0.15)
    agr.audio.play("assets://audio/hurt.wav", { volume = 0.7 })
    agr.fx.flash(1, 0.1, 0.05, 0.45, 0.35)
    agr.fx.shake(0.45, 0.4)
    agr.fx.hitStop(0.08)
    local x, y, z = self.entity:position()
    agr.fx.popup(x, y + 1.8, z, "-" .. n, { color = { 1, 0.35, 0.3 }, size = 60 })
    if from and from:isValid() then
        local fx, _, fz = from:worldPosition()
        local d = math.max(0.01, G.distance(x, z, fx, fz))
        self.entity:setVelocity((x - fx) / d * 8, 4, (z - fz) / d * 8)
        self.knock = 0.22
    end
    self.game:healthChanged(-n)
    if self.health <= 0 then self.game:heroDied() end
end

function Hero:heal(n)
    local before = self.health
    self.health = math.min(self.maxHealth, self.health + n)
    self.game:healthChanged(self.health - before)
end

return Hero
