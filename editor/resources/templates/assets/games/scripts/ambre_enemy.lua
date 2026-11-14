-- Ennemis des Ruines d'Ambre :
--   slime  : poursuit le héros sur le maillage de navigation en sautillant ;
--   bat    : vole droit sur lui en zigzaguant ;
--   archer : squelette qui garde ses distances et tire des flèches (annoncées par un éclat) ;
--   brute  : golem de pierre lent qui charge en ligne droite ; contre un mur il est étourdi et
--            prend double dégâts ;
--   minion : petit slime invoqué par le boss ;
--   boss   : écrasement annoncé par une zone rouge (sautez ou fuyez), sbires ; furieux à mi-vie :
--            plus rapide et pluie de rochers.
-- Rangés dans leur zone (« Area N/Enemies ») : positions du monde via worldPosition(), positions
-- locales (setPosition) = monde - décalage de la zone.
local G = require("assets://games/scripts/common.lua")

local HP = { slime = 2, bat = 1, minion = 1, archer = 2, brute = 6, boss = 30 }
local COLOR = { slime = { 0.5, 1, 0.45 }, minion = { 0.5, 1, 0.45 }, bat = { 0.75, 0.55, 1 }, archer = { 0.95, 0.92, 0.8 },
                brute = { 0.7, 0.68, 0.65 }, boss = { 1, 0.5, 0.3 } }

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
    self.summon, self.slamIn, self.rocksIn, self.phase, self.phaseTime = 6, 4, 5, "chase", 0
    self.shootIn, self.chargeIn = 1.5 + math.random(), 2
    if self.kind ~= "bat" then self.entity:stopNavigation() end
    if self.sprite then
        agr.stopTweens(self.sprite)
        self.sprite:setScale(1, 1, 1)
        self.sprite:setPosition(0, 0, 0)
    end
    if self.telegraph then self.telegraph:set("SpriteRenderer", "color", { 1, 0.2, 0.1, 0 }) end
    if self.kind == "boss" then self.entity:set("NavAgent", "speed", 3.2) end
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
    elseif self.kind == "archer" then
        self:archerUpdate(dt, d, x, z, hx, hz)
    elseif self.kind == "brute" then
        self:bruteUpdate(dt, d, x, z, hx, hz)
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
    local base = { 1, 1, 1, 1 }
    if self.kind == "boss" then base = self.furious and { 1, 0.25, 0.2, 1 } or { 1, 0.45, 0.35, 1 }
    elseif self.phase == "windup" then base = { 1, 0.55, 0.3, 1 }
    elseif self.phase == "stunned" then base = { 0.7, 0.7, 1, 1 } end
    self:tint(self.flash > 0 and { 1, 0.25, 0.25, 1 } or base)
    local reach = self.kind == "boss" and 2.0 or (self.kind == "brute" and 1.3 or 0.9)
    if self.kind ~= "archer" and self.phase ~= "stunned" and d < reach and math.abs(hy - y) < 1.6 and self.cooldown <= 0 then
        hero:script():damage(self.kind == "brute" and 2 or 1, self.entity)
        self.cooldown = 1
    end
end

-- Archer : reste entre 5 et 9 m, tire toutes les 2,5 s environ (éclat orange 0,5 s avant).
function Enemy:archerUpdate(dt, d, x, z, hx, hz)
    self.repath = self.repath - dt
    if self.repath <= 0 then
        self.repath = 0.6
        if d < 5 then
            local nx, nz = (x - hx) / math.max(d, 0.01), (z - hz) / math.max(d, 0.01)
            self.entity:navigateTo(x + nx * 4, 0, z + nz * 4)
        elseif d > 9 and d < 16 then
            self.entity:navigateTo(hx, 0, hz)
        else
            self.entity:stopNavigation()
        end
    end
    if self.phase == "windup" then
        self.phaseTime = self.phaseTime + dt
        if self.phaseTime >= 0.5 then
            self.phase = "chase"
            self:shoot(x, z, hx, hz)
        end
    else
        self.shootIn = self.shootIn - dt
        if self.shootIn <= 0 and d < 14 then
            self.phase, self.phaseTime = "windup", 0
            self.shootIn = 2.2 + math.random() * 0.8
            if self.sprite then agr.tween(self.sprite, { scale = { 1, 1, 1 } }, 0.5, { from = { 1.15, 0.9, 1 }, ease = "outBack" }) end
        end
    end
end

function Enemy:shoot(x, z, hx, hz)
    local arrow = agr.clone(agr.find("Arrow Prototype"), false)
    arrow:setName("Arrow")
    local yaw = math.deg(math.atan(-(hz - z), hx - x))
    arrow:setRotation(0, yaw, 0)
    local a = math.rad(yaw)
    arrow:setPosition(x + math.cos(a) * 0.8, 1.0, z - math.sin(a) * 0.8)
    agr.audio.play("assets://audio/swing.wav", { volume = 0.5, pitch = 1.5 })
end

-- Brute : marche vers le héros ; à moins de 7 m, se prépare (0,7 s) puis charge en ligne droite.
-- Contre un mur ou un pilier : étourdie 1,6 s (dégâts doublés).
function Enemy:bruteUpdate(dt, d, x, z, hx, hz)
    self.phaseTime = self.phaseTime + dt
    if self.phase == "chase" then
        self.repath = self.repath - dt
        self.chargeIn = self.chargeIn - dt
        if self.repath <= 0 then
            self.entity:navigateTo(hx, 0, hz)
            self.repath = 0.5
        end
        if self.sprite then self.sprite:setPosition(0, math.abs(math.sin(self.t * 4)) * 0.08, 0) end
        if d < 7 and self.chargeIn <= 0 then
            self.phase, self.phaseTime = "windup", 0
            self.entity:stopNavigation()
            self.cx, self.cz = (hx - x) / math.max(d, 0.01), (hz - z) / math.max(d, 0.01)
            agr.audio.play("assets://audio/slam.wav", { volume = 0.35, pitch = 1.6 })
        end
    elseif self.phase == "windup" then
        if self.sprite then self.sprite:setPosition(math.sin(self.phaseTime * 50) * 0.06, 0, 0) end
        if self.phaseTime >= 0.7 then self.phase, self.phaseTime = "charge", 0 end
    elseif self.phase == "charge" then
        local step = 11 * dt
        local hit = agr.physics3d.raycast(x, 0.8, z, self.cx, 0, self.cz, step + 0.9)
        local hero = agr.find("Hero")
        if hit and hit.entity ~= hero then
            self.phase, self.phaseTime = "stunned", 0
            agr.fx.shake(0.35, 0.3)
            agr.audio.play("assets://audio/slam.wav", { volume = 0.6, pitch = 1.2 })
            agr.fx.burst(x + self.cx, 1.2, z + self.cz, { count = 20, color = { 0.8, 0.75, 0.6 }, speed = 4, life = 0.5, additive = false })
            agr.fx.popup(x, 3, z, "?!", { color = { 0.8, 0.85, 1 }, size = 56, duration = 1.2 })
        elseif self.phaseTime > 0.9 then
            self.phase, self.phaseTime, self.chargeIn = "chase", 0, 2.5
        else
            local lx, ly, lz = self.entity:position()
            self.entity:setPosition(lx + self.cx * step, ly, lz + self.cz * step)
            if self.sprite then self.sprite:setScale(1.15, 0.9, 1) end
            if d < 1.3 and self.cooldown <= 0 then
                hero:script():damage(2, self.entity)
                self.cooldown = 1
            end
        end
    elseif self.phase == "stunned" then
        if self.sprite then self.sprite:setScale(1, 1, 1) end
        if self.phaseTime >= 1.6 then self.phase, self.phaseTime, self.chargeIn = "chase", 0, 2.5 end
    end
end

-- Boss : poursuite, puis : préparation (zone rouge qui grandit), écrasement (dégâts si le héros est
-- dans la zone et au sol), récupération (vulnérable). Furieux : pluie de rochers.
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
        if self.furious then
            self.rocksIn = self.rocksIn - dt
            if self.rocksIn <= 0 then
                self.rocksIn = 6
                self:rockRain(hx, hz)
            end
        end
        if self.slamIn <= 0 and d < 9 then
            self.phase, self.phaseTime = "windup", 0
            self.entity:stopNavigation()
            if self.telegraph then
                agr.tween(self.telegraph, { alpha = 0.6 }, 0.9, { from = 0 })
                agr.tween(self.telegraph, { scale = { 1, 1, 1 } }, 0.9, { from = { 0.2, 0.2, 1 }, ease = "outCubic" })
            end
            if self.sprite then agr.tween(self.sprite, { scale = { 0.85, 1.35, 1 }, position = { 0, 1.2, 0 } }, 0.9, { ease = "outQuad" }) end
        elseif self.summon <= 0 then
            self.summon = self.furious and 6 or 9
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

-- Trois rochers tombent autour du héros, annoncés par des cercles rouges.
function Enemy:rockRain(hx, hz)
    local group = self.entity:parent()
    for i = 1, 3 do
        local rock = group:find("Boulder " .. i)
        if rock then
            local a, r = math.random() * math.pi * 2, (i == 1) and 0 or (1.5 + math.random() * 2.5)
            local wx, wz = hx + math.cos(a) * r, hz + math.sin(a) * r
            rock:setPosition(wx - self.ox, 0, wz - self.oz)
            rock:setActive(true)
            local zone, stone = rock:find("Zone"), rock:find("Sprite")
            agr.tween(zone, { alpha = 0.55 }, 1.0, { from = 0 })
            agr.tween(stone, { position = { 0, 0, 0 } }, 1.0, { from = { 0, 12, 0 }, ease = "inQuad", onDone = function()
                if self.game.state ~= "playing" then
                    rock:setActive(false)
                    return
                end
                agr.fx.burst(wx, 0.4, wz, { count = 24, color = { 0.6, 0.5, 0.4 }, speed = 5, life = 0.5, additive = false })
                agr.fx.shake(0.25, 0.25)
                agr.audio.play("assets://audio/slam.wav", { volume = 0.4, pitch = 1.3 })
                local hero = agr.find("Hero")
                local px, py, pz = hero:position()
                if G.distance(px, pz, wx, wz) < 1.3 and py < 1 then hero:script():damage(1, rock) end
                agr.tween(zone, { alpha = 0 }, 0.2, { onDone = function() rock:setActive(false) end })
            end })
        end
    end
    self.game:say(agr.tr("ambre.rocks"), 1.5)
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
    if self.phase == "stunned" then n = n * 2 end
    self.life = self.life - n
    self.flash = 0.12
    local x, y, z = self.entity:position()
    local wx, wy, wz = self.entity:worldPosition()
    local hx, _, hz = from:worldPosition()
    local d = math.max(0.01, G.distance(wx, wz, hx, hz))
    local push = (self.kind == "boss" or self.kind == "brute") and 0.2 or 1.1
    if self.phase ~= "charge" then self.entity:setPosition(x + (wx - hx) / d * push, y, z + (wz - hz) / d * push) end
    if self.kind == "slime" or self.kind == "minion" or self.kind == "archer" then self.entity:stopNavigation() end
    self.stun, self.repath = (self.kind == "boss" or self.kind == "brute") and 0 or 0.3, 0.3
    local c = COLOR[self.kind] or { 1, 1, 1 }
    agr.fx.burst(wx, wy + 0.6, wz, { count = 14, color = c, speed = 4.5, life = 0.45, size = 0.2 })
    agr.fx.popup(wx, wy + (self.kind == "boss" and 3.5 or 1.8), wz, "-" .. n, { color = n > 1 and { 1, 0.8, 0.3 } or { 1, 0.95, 0.8 }, size = n > 1 and 64 or 54 })
    if self.sprite and self.kind ~= "boss" and self.kind ~= "brute" then
        agr.tween(self.sprite, { scale = { 1, 1, 1 } }, 0.3, { from = { 1.45, 0.6, 1 }, ease = "outElastic" })
    end
    if self.kind == "boss" then
        self.game:bossHealth(math.max(0, self.life) / self.maxLife)
        if not self.furious and self.life <= self.maxLife / 2 then
            self.furious = true
            self.rocksIn = 2
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
