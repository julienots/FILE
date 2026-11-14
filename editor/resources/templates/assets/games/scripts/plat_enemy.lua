-- Ennemis du jeu de plateforme (propriété kind) :
--   slime  : fait des allers-retours ; on peut l'écraser ;
--   spiky  : hérisson, comme le slime mais ses piquants blessent si on saute dessus ;
--   bat    : chauve-souris qui vole en vagues ; on peut l'écraser ;
--   fish   : poisson qui saute hors de l'eau à intervalles ;
--   cannon : canon fixe qui tire des boulets quand le héros approche (on peut monter dessus).
local G = require("assets://games/scripts/common.lua")

local Enemy = { properties = { kind = "slime", level = 1, speed = 1.6, range = 2.5, dir = -1, jump = 9, period = 2.6 } }

function Enemy:onStart()
    self.isEnemy = true
    self.game = agr.find("Game"):script()
    self.x0, self.y0 = self.entity:position()
    self.stompHeight = (self.kind == "bat" or self.kind == "fish") and 0.2 or 0.45
    self:reset()
end

function Enemy:reset()
    self.dead, self.t, self.moveDir, self.vy, self.wait = false, 0, -1, 0, 1 + (self.x0 % 2)
    self.entity:setPosition(self.x0, self.y0)
    self.entity:setVelocity(0, 0)
    self.entity:setScale(1, 1, 1)
    if self.kind == "spiky" then self.speed, self.range = 1.2, 3 end
end

function Enemy:onUpdate(dt)
    local g = self.game
    if self.dead or not g or g.state ~= "playing" or g.level ~= self.level then
        if self.kind == "slime" or self.kind == "spiky" then
            local _, vy = self.entity:velocity()
            self.entity:setVelocity(0, vy)
        elseif self.kind ~= "cannon" then
            self.entity:setVelocity(0, 0)
        end
        return
    end
    self.t = self.t + dt
    local x, y = self.entity:position()
    local _, vy = self.entity:velocity()
    if self.kind == "slime" or self.kind == "spiky" then
        local vx = self.entity:velocity()
        if x < self.x0 - self.range then self.moveDir = 1 elseif x > self.x0 + self.range then self.moveDir = -1
        elseif self.moving and math.abs(vx) < 0.05 then self.moveDir = -self.moveDir end
        self.moving = true
        self.entity:setVelocity(self.moveDir * self.speed, vy)
        self.entity:set("SpriteRenderer", "flipX", self.moveDir > 0)
        if self.kind == "slime" then
            local h = math.abs(math.sin(self.t * 6))
            self.entity:setScale(1.08 - 0.12 * h, 0.92 + 0.12 * h, 1)
        end
    elseif self.kind == "bat" then
        if x < self.x0 - 2.5 then self.moveDir = 1 elseif x > self.x0 + 2.5 then self.moveDir = -1 end
        self.entity:setVelocity(self.moveDir * 2, math.cos(self.t * 2.5) * 1.6)
        self.entity:set("SpriteRenderer", "flipX", self.moveDir > 0)
        local f = math.abs(math.sin(self.t * 12))
        self.entity:setScale(0.85 + 0.3 * f, 1.05 - 0.15 * f, 1)
    elseif self.kind == "fish" then
        if self.wait > 0 then
            self.wait = self.wait - dt
            self.entity:setVelocity(0, 0)
            if self.wait <= 0 then
                self.vy = self.jump
                local hx = agr.find("Hero"):position()
                if math.abs(hx - x) < 10 then agr.audio.play("assets://audio/jump.wav", { volume = 0.3, pitch = 0.6 }) end
                agr.fx.burst(x, y + 1, 0, { count = 12, color = { 0.6, 0.85, 1 }, speed = 3, life = 0.4, spread = "up", gravity = -6, additive = false })
            end
        else
            self.vy = self.vy - 15 * dt
            if y < self.y0 and self.vy < 0 then
                self.entity:setPosition(self.x0, self.y0)
                self.entity:setVelocity(0, 0)
                self.wait = 1.6
            else
                self.entity:setVelocity(0, self.vy)
                self.entity:set("SpriteRenderer", "flipY", self.vy < 0)
            end
        end
    elseif self.kind == "cannon" then
        self.entity:set("SpriteRenderer", "flipX", self.dir < 0)
        local hx, hy = agr.find("Hero"):position()
        self.cool = (self.cool or 1) - dt
        if self.cool <= 0 and math.abs(hx - x) < 13 and math.abs(hy - y) < 6 then
            self.cool = self.period
            local b = agr.clone(agr.find("Bullet Prototype"), false)
            b:setName("Bullet")
            b:setPosition(x + self.dir * 0.8, y + 0.55)
            b:setScale(self.dir, 1, 1)
            table.insert(self.game.bullets, b)
            agr.tween(self.entity, { scale = { 1, 1, 1 } }, 0.25, { from = { 0.8, 1.15, 1 }, ease = "outBack" })
            agr.fx.burst(x + self.dir * 0.9, y + 0.55, 0, { count = 10, color = { 0.8, 0.8, 0.8 }, speed = 2, life = 0.4, additive = false, gravity = 1 })
            if math.abs(hx - x) < 10 then agr.audio.play("assets://audio/launch.wav", { volume = 0.4, pitch = 1.3 }) end
        end
    end
end

-- Écrasé par le héros.
function Enemy:stomped()
    if self.dead then return end
    self.dead = true
    local x, y = self.entity:position()
    agr.audio.play("assets://audio/hit.wav", { volume = 0.7 })
    agr.fx.burst(x, y + 0.3, 0, { count = 22, color = self.kind == "bat" and { 0.7, 0.55, 1 } or { 0.5, 1, 0.45 }, speed = 5, life = 0.5, size = 0.18 })
    agr.fx.shake(0.2, 0.2)
    agr.fx.hitStop(0.05)
    self.entity:setVelocity(0, 0)
    self.game:enemyDefeated(self.kind, x, y)
    local e = self.entity
    agr.tween(e, { scale = { 1.5, 0.1, 1 } }, 0.15, { onDone = function() if e:isValid() then e:setActive(false) end end })
end

return Enemy
