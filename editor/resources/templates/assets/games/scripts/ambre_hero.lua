-- Héros des Ruines d'Ambre. Déplacement : flèches / ZQSD / stick / joystick tactile ; saut :
-- Espace ou bouton « Saut » ; épée : J, K ou bouton « Épée ». Le sprite (enfant « Sprite ») est
-- animé par sa machine à états (paramètres speed et grounded).
local G = require("assets://games/scripts/common.lua")

local Hero = { properties = { speed = 5.5, jumpSpeed = 7, maxHealth = 5, attackRange = 1.8, attackCooldown = 0.35, fallLimit = -8 } }

function Hero:onStart()
    self.game = agr.find("Game"):script()
    self.sprite = self.entity:find("Sprite")
    self.health, self.facing = self.maxHealth, 1
    self.cooldown, self.invulnerable, self.swing = 0, 0, 0
    self.spawnX, self.spawnY, self.spawnZ = self.entity:position()
end

function Hero:spawn(x, y, z)
    self.spawnX, self.spawnY, self.spawnZ = x, y, z
    self.entity:setPosition(x, y, z)
    self.entity:setVelocity(0, 0, 0)
    self.health = self.maxHealth
    self.invulnerable = 1
end

local function edge(self, name)
    local b = agr.find(name)
    local down = b ~= nil and b:isActive() and agr.ui.isPressed(b)
    self.held = self.held or {}
    local was = self.held[name]
    self.held[name] = down
    return down and not was
end

function Hero:onUpdate(dt)
    local e = self.entity
    local _, vy = e:velocity()
    if not self.game or self.game.state ~= "playing" then
        e:setVelocity(0, math.min(vy, 0), 0)
        if self.sprite then self.sprite:setParameter("speed", 0) end
        return
    end
    self.cooldown = self.cooldown - dt
    self.invulnerable = math.max(0, self.invulnerable - dt)
    self.swing = math.max(0, self.swing - dt)
    local x, y, z = e:position()
    -- Starts just under the feet (a ray starting inside the capsule would hit the hero itself).
    local hit = agr.physics3d.raycast(x, y - 0.03, z, 0, -1, 0, 0.2)
    self.grounded = hit ~= nil and hit.entity ~= e
    local mx, mz = agr.input.value("move_x"), -agr.input.value("move_y")
    local len = math.sqrt(mx * mx + mz * mz)
    if len > 1 then mx, mz = mx / len, mz / len end
    if (agr.input.pressed("jump") or edge(self, "Jump")) and self.grounded then
        vy = self.jumpSpeed
        agr.audio.play("assets://audio/jump.wav", { volume = 0.6 })
    end
    e:setVelocity(mx * self.speed, vy, mz * self.speed)
    if mx ~= 0 then self.facing = mx > 0 and 1 or -1 end
    if agr.input.keyPressed("J") or agr.input.keyPressed("K") or edge(self, "Attack") then self:attack() end
    if self.sprite then
        self.sprite:set("SpriteRenderer", "flipX", self.facing < 0)
        self.sprite:setParameter("speed", len > 0.1 and 1 or 0)
        self.sprite:setParameter("grounded", self.grounded)
        local blink = self.invulnerable > 0 and math.floor(self.invulnerable * 12) % 2 == 0
        self.sprite:set("SpriteRenderer", "color", blink and { 1, 1, 1, 0.35 } or (self.swing > 0 and { 1, 0.9, 0.6, 1 } or { 1, 1, 1, 1 }))
        local s = 1 + self.swing * 0.6
        self.sprite:setScale(s * 1, 1, 1)
    end
    if y < self.fallLimit then
        self:damage(1, true)
        e:setPosition(self.spawnX, self.spawnY + 0.5, self.spawnZ)
        e:setVelocity(0, 0, 0)
    end
end

function Hero:attack()
    if self.cooldown > 0 then return end
    self.cooldown, self.swing = self.attackCooldown, 0.2
    agr.audio.play("assets://audio/click.wav", { volume = 0.7, pitch = 0.7 })
    local x, _, z = self.entity:position()
    for _, foe in ipairs(self.game:enemies()) do
        if foe:isValid() and foe:isActive() then
            local fx, _, fz = foe:worldPosition()
            local s = foe:script()
            local reach = self.attackRange + (s and s.kind == "boss" and 1.2 or 0)
            if s and G.distance(x, z, fx, fz) < reach then s:hit(1, self.entity) end
        end
    end
end

function Hero:damage(n, ignoreInvulnerability)
    if self.game.state ~= "playing" or (self.invulnerable > 0 and not ignoreInvulnerability) then return end
    self.health = self.health - n
    self.invulnerable = 1.2
    agr.device.vibrate(0.12)
    agr.audio.play("assets://audio/launch.wav", { volume = 0.5, pitch = 0.6 })
    self.game:hud()
    if self.health <= 0 then self.game:heroDied() end
end

function Hero:heal(n)
    self.health = math.min(self.maxHealth, self.health + n)
    self.game:hud()
end

return Hero
