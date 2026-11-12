-- Ennemis des Ruines d'Ambre : slime (poursuit le héros sur le maillage de navigation en
-- sautillant), chauve-souris (vole droit sur lui en ondulant), sbire (petit slime invoqué) et le
-- boss (gros slime : poursuit, invoque des sbires, plus vite à mi-vie).
-- Les ennemis sont rangés dans leur zone (« Area N/Enemies ») : positions du monde via
-- worldPosition(), positions locales (setPosition) = monde - décalage de la zone.
local G = require("assets://games/scripts/common.lua")

local HP = { slime = 2, bat = 1, minion = 1, boss = 24 }

local Enemy = { properties = { kind = "slime", sight = 10, spacing = 70 } }

function Enemy:onStart()
    self.game = agr.find("Game"):script()
    self.sprite = self.entity:find("Sprite")
    local wx, _, wz = self.entity:worldPosition()
    local lx, _, lz = self.entity:position()
    self.ox, self.oz = wx - lx, wz - lz
    self.area = math.floor((wx + self.spacing / 2) / self.spacing) + 1
    self.life = HP[self.kind] or 2
    self.maxLife = self.life
    self.t, self.repath, self.cooldown, self.flash, self.summon = math.random() * 6, 0, 0, 0, 5
end

function Enemy:reset()
    self.life = self.maxLife
    self.summon, self.cooldown, self.repath = 5, 0, 0
    if self.kind ~= "bat" then self.entity:stopNavigation() end
end

function Enemy:onUpdate(dt)
    local game = self.game
    if not game or game.state ~= "playing" or game.level ~= self.area then
        if self.kind ~= "bat" then self.entity:stopNavigation() end
        return
    end
    local hero = agr.find("Hero")
    local x, y, z = self.entity:worldPosition()
    local hx, hy, hz = hero:position()
    local d = G.distance(x, z, hx, hz)
    self.t = self.t + dt
    self.cooldown = self.cooldown - dt
    self.flash = self.flash - dt
    if self.kind == "bat" then
        if d < self.sight then
            local speed = 3.2
            local nx, nz = (hx - x) / math.max(d, 0.01), (hz - z) / math.max(d, 0.01)
            local side = math.sin(self.t * 3) * 0.45  -- weaves left and right
            local wx, wz = x + (nx - nz * side) * speed * dt, z + (nz + nx * side) * speed * dt
            self.entity:setPosition(wx - self.ox, 1.2 + math.sin(self.t * 5) * 0.3, wz - self.oz)
        end
    else
        self.repath = self.repath - dt
        local sight = self.kind == "boss" and 30 or self.sight
        if d < sight and self.repath <= 0 then
            self.entity:navigateTo(hx, 0, hz)
            self.repath = 0.4
        end
        if self.sprite then
            local hop = math.abs(math.sin(self.t * (self.kind == "boss" and 4 or 8))) * (self.kind == "boss" and 0.4 or 0.25)
            self.sprite:setPosition(0, hop, 0)
        end
    end
    if self.kind == "boss" then
        self.summon = self.summon - dt
        if self.summon <= 0 then
            self.summon = self.life < self.maxLife / 2 and 4 or 7
            local proto = agr.find("Minion Prototype")
            local lx, _, lz = self.entity:position()
            for i = 1, 2 do
                local m = agr.clone(proto, self.entity:parent())
                m:setName("Minion")
                m:setPosition(lx + (i == 1 and 2 or -2), 0, lz + 1.5)
            end
            game:say(agr.tr("ambre.summon"), 1.5)
        end
    end
    if self.sprite then
        local tint = self.kind == "boss" and { 1, 0.45, 0.35, 1 } or { 1, 1, 1, 1 }
        self.sprite:set("SpriteRenderer", "color", self.flash > 0 and { 1, 0.3, 0.3, 1 } or tint)
    end
    local reach = self.kind == "boss" and 2.2 or 0.9
    if d < reach and math.abs(hy - y) < 1.6 and self.cooldown <= 0 then
        hero:script():damage(self.kind == "boss" and 2 or 1)
        self.cooldown = 1
    end
end

function Enemy:hit(n, from)
    if self.life <= 0 then return end
    self.life = self.life - n
    self.flash = 0.15
    local x, y, z = self.entity:position()
    local wx, _, wz = self.entity:worldPosition()
    local hx, _, hz = from:worldPosition()
    local d = math.max(0.01, G.distance(wx, wz, hx, hz))
    local push = self.kind == "boss" and 0.3 or 1.0
    self.entity:setPosition(x + (wx - hx) / d * push, y, z + (wz - hz) / d * push)
    self.repath = 0
    if self.kind == "boss" then self.game:bossHealth(math.max(0, self.life) / self.maxLife) end
    if self.life > 0 then return end
    agr.audio.play("assets://audio/coin.wav", { volume = 0.5, pitch = 0.7 })
    if self.kind ~= "bat" then self.entity:stopNavigation() end
    self.game:enemyDefeated(self.kind)
    if self.kind == "minion" then
        self.entity:destroy()
    else
        self.entity:setActive(false)
        if self.kind == "boss" then self.game:bossDefeated() end
    end
end

return Enemy
