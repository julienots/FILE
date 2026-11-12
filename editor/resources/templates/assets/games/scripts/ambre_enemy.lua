-- Ennemis des Ruines d'Ambre :
--   slime  : poursuit le héros sur le maillage de navigation en sautillant ;
--   bat    : vole droit sur lui en zigzaguant ;
--   minion : petit slime invoqué par le boss ;
--   boss   : gros slime ; prépare un écrasement (zone rouge au sol : sautez ou fuyez !), invoque
--            des sbires, devient furieux à mi-vie.
-- Rangés dans leur zone (« Area N/Enemies ») : positions du monde via worldPosition(), positions
-- locales (setPosition) = monde - décalage de la zone.
local G = require("assets://games/scripts/common.lua")

local HP = { slime = 2, bat = 1, minion = 1, boss = 24 }
local COLOR = { slime = { 0.5, 1, 0.45 }, minion = { 0.5, 1, 0.45 }, bat = { 0.75, 0.55, 1 }, boss = { 1, 0.5, 0.3 } }

local Enemy = { properties = { kind = "slime", sight = 10, spacing = 70, slamRadius = 3.2 } }

function Enemy:onStart()
    self.game = agr.find("Game"):script()
    self.sprite = self.entity:find("Sprite")
    self.shadow = self.entity:find("Shadow")
    self.telegraph = self.entity:find("Telegraph")
    local wx, _, wz = self.entity:worldPosition()
    local lx, _, lz = self.entity:position()
    self.ox, self.oz = wx - lx, wz - lz
    self.area = math.floor((wx + self.spacing / 2) / self.spacing) + 1
    self:reset()
end

function Enemy:reset()
    self.life = HP[self.kind] or 2
    self.maxLife = self.life
    self.dead, self.furious = false, false
    self.t, self.repath, self.cooldown, self.flash, self.stun = math.random() * 6, 0, 0, 0, 0
    self.summon, self.slamIn, self.phase, self.phaseTime = 6, 4, "chase", 0
    if self.kind ~= "bat" then self.entity:stopNavigation() end
    if self.sprite then
        agr.stopTweens(self.sprite)
        self.sprite:setScale(1, 1, 1)
        self.sprite:setPosition(0, 0, 0)
    end
    if self.telegraph then self.telegraph:set("SpriteRenderer", "color", { 1, 0.2, 0.1, 0 }) end
end

function Enemy:tint(c)
    if self.sprite then self.sprite:set("SpriteRenderer", "color", c) end
end

function Enemy:onUpdate(dt)
    local game = self.game
    if self.dead or not game or game.state ~= "playing" or game.level ~= self.area then
        if self.kind ~= "bat" and not self.dead then self.entity:stopNavigation() end
        return
    end
    local hero = agr.find("Hero")
    local x, y, z = self.entity:worldPosition()
    local hx, hy, hz = hero:position()
    local d = G.distance(x, z, hx, hz)
    self.t = self.t + dt
    self.cooldown = self.cooldown - dt
    self.flash = self.flash - dt
    self.stun = self.stun - dt
    if self.kind == "bat" then
        if d < self.sight and self.stun <= 0 then
            local speed = 3.2
            local nx, nz = (hx - x) / math.max(d, 0.01), (hz - z) / math.max(d, 0.01)
            local side = math.sin(self.t * 3) * 0.45
            local wx, wz = x + (nx - nz * side) * speed * dt, z + (nz + nx * side) * speed * dt
            self.entity:setPosition(wx - self.ox, 1.2 + math.sin(self.t * 5) * 0.3, wz - self.oz)
        end
        if self.sprite then  -- battements d'ailes
            local f = math.abs(math.sin(self.t * 14))
            self.sprite:setScale(0.8 + 0.35 * f, 1.1 - 0.2 * f, 1)
        end
        if self.shadow then self.shadow:setPosition(0, -y + 0.02, 0) end
    elseif self.kind == "boss" then
        self:bossUpdate(dt, d, hx, hz)
    else
        self.repath = self.repath - dt
        if self.stun <= 0 and d < self.sight and self.repath <= 0 then
            self.entity:navigateTo(hx, 0, hz)
            self.repath = 0.4
        end
        if self.sprite then  -- sautille en s'écrasant
            local h = math.abs(math.sin(self.t * 7))
            self.sprite:setPosition(0, h * 0.3, 0)
            self.sprite:setScale(1.15 - 0.25 * h, 0.85 + 0.25 * h, 1)
        end
    end
    local base = self.kind == "boss" and (self.furious and { 1, 0.25, 0.2, 1 } or { 1, 0.45, 0.35, 1 }) or { 1, 1, 1, 1 }
    self:tint(self.flash > 0 and { 1, 0.25, 0.25, 1 } or base)
    local reach = self.kind == "boss" and 2.0 or 0.9
    if d < reach and math.abs(hy - y) < 1.6 and self.cooldown <= 0 then
        hero:script():damage(1, self.entity)
        self.cooldown = 1
    end
end

-- Boss : poursuite, puis toutes les quelques secondes : préparation (zone rouge qui grandit),
-- écrasement (dégâts si le héros est dans la zone et au sol), récupération (il est vulnérable).
function Enemy:bossUpdate(dt, d, hx, hz)
    self.phaseTime = self.phaseTime + dt
    if self.phase == "chase" then
        self.repath = self.repath - dt
        if self.repath <= 0 then
            self.entity:navigateTo(hx, 0, hz)
            self.repath = 0.4
        end
        if self.sprite then self.sprite:setPosition(0, math.abs(math.sin(self.t * 4)) * 0.35, 0) end
        self.slamIn = self.slamIn - dt
        self.summon = self.summon - dt
        if self.slamIn <= 0 and d < 9 then
            self.phase, self.phaseTime = "windup", 0
            self.entity:stopNavigation()
            if self.telegraph then
                agr.tween(self.telegraph, { alpha = 0.6 }, 0.9, { from = 0 })
                agr.tween(self.telegraph, { scale = { 1, 1, 1 } }, 0.9, { from = { 0.2, 0.2, 1 }, ease = "outCubic" })
            end
            if self.sprite then agr.tween(self.sprite, { scale = { 0.85, 1.35, 1 }, position = { 0, 1.2, 0 } }, 0.9, { ease = "outQuad" }) end
        elseif self.summon <= 0 then
            self.summon = self.furious and 5 or 8
            self:summonMinions()
        end
    elseif self.phase == "windup" and self.phaseTime >= 0.9 then
        self.phase, self.phaseTime = "recover", 0
        self:slam()
    elseif self.phase == "recover" and self.phaseTime >= (self.furious and 0.6 or 1.0) then
        self.phase = "chase"
        self.slamIn = self.furious and 3 or 4.5
    end
end

function Enemy:slam()
    local x, _, z = self.entity:worldPosition()
    agr.audio.play("assets://audio/slam.wav", { volume = 0.9 })
    agr.fx.shake(0.75, 0.6)
    agr.fx.burst(x, 0.2, z, { count = 60, color = { 0.85, 0.6, 0.4 }, endColor = { 0.5, 0.35, 0.25, 0 }, speed = 9, life = 0.7,
                              size = 0.4, spread = "ring", gravity = -4, additive = false })
    if self.sprite then agr.tween(self.sprite, { scale = { 1, 1, 1 }, position = { 0, 0, 0 } }, 0.35, { from = { 1.4, 0.6, 1 }, ease = "outElastic" }) end
    if self.telegraph then agr.tween(self.telegraph, { alpha = 0 }, 0.3) end
    local hero = agr.find("Hero")
    local hx, hy, hz = hero:position()
    if G.distance(x, z, hx, hz) < self.slamRadius and hy < 0.5 then hero:script():damage(2, self.entity) end
end

function Enemy:summonMinions()
    local proto = agr.find("Minion Prototype")
    local lx, _, lz = self.entity:position()
    for i = 1, 2 do
        local m = agr.clone(proto, self.entity:parent())
        m:setName("Minion")
        m:setPosition(lx + (i == 1 and 2 or -2), 0, lz + 1.5)
        local wx, _, wz = m:worldPosition()
        agr.fx.burst(wx, 0.4, wz, { count = 16, color = { 0.5, 1, 0.45 }, speed = 3, life = 0.5, spread = "up" })
    end
    self.game:say(agr.tr("ambre.summon"), 1.5)
end

function Enemy:hit(n, from)
    if self.dead then return end
    self.life = self.life - n
    self.flash = 0.12
    local x, y, z = self.entity:position()
    local wx, wy, wz = self.entity:worldPosition()
    local hx, _, hz = from:worldPosition()
    local d = math.max(0.01, G.distance(wx, wz, hx, hz))
    local push = self.kind == "boss" and 0.25 or 1.1
    self.entity:setPosition(x + (wx - hx) / d * push, y, z + (wz - hz) / d * push)
    if self.kind ~= "bat" and self.kind ~= "boss" then self.entity:stopNavigation() end
    self.stun, self.repath = self.kind == "boss" and 0 or 0.3, 0.3
    local c = COLOR[self.kind] or { 1, 1, 1 }
    agr.fx.burst(wx, wy + 0.6, wz, { count = 14, color = c, speed = 4.5, life = 0.45, size = 0.2 })
    agr.fx.popup(wx, wy + (self.kind == "boss" and 3.5 or 1.6), wz, "-" .. n, { color = { 1, 0.95, 0.8 }, size = 54 })
    if self.sprite and self.kind ~= "boss" then
        agr.tween(self.sprite, { scale = { 1, 1, 1 } }, 0.3, { from = { 1.45, 0.6, 1 }, ease = "outElastic" })
    end
    if self.kind == "boss" then
        self.game:bossHealth(math.max(0, self.life) / self.maxLife)
        if not self.furious and self.life <= self.maxLife / 2 then
            self.furious = true
            self.entity:set("NavAgent", "speed", 4.4)
            agr.fx.popup(wx, wy + 4.5, wz, agr.tr("ambre.fury"), { color = { 1, 0.4, 0.2 }, size = 72, duration = 1.4 })
            agr.fx.shake(0.5, 0.5)
        end
    end
    if self.life > 0 then return end
    self:die(wx, wy, wz)
end

function Enemy:die(wx, wy, wz)
    self.dead = true
    if self.kind ~= "bat" then self.entity:stopNavigation() end
    local c = COLOR[self.kind] or { 1, 1, 1 }
    local boss = self.kind == "boss"
    agr.fx.burst(wx, wy + 0.6, wz, { count = boss and 120 or 30, color = c, endColor = { 1, 0.9, 0.5, 0 }, speed = boss and 9 or 6,
                                     life = boss and 1.2 or 0.6, size = boss and 0.45 or 0.25, texture = "assets://games/sparkle.png" })
    agr.audio.play("assets://audio/coin.wav", { volume = 0.5, pitch = 0.7 })
    self.game:enemyDefeated(self.kind, wx, wy, wz)
    local entity, kind = self.entity, self.kind
    if self.sprite then
        agr.tween(self.sprite, { scale = { 1.6, 0.05, 1 } }, boss and 0.8 or 0.2, { ease = "inBack", onDone = function()
            if not entity:isValid() then return end
            if kind == "minion" then entity:destroy() else entity:setActive(false) end
        end })
    elseif kind == "minion" then entity:destroy() else entity:setActive(false) end
    if boss then self.game:bossDefeated() end
end

return Enemy
