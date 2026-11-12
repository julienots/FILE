-- Chauve-souris : rôde autour de son nid, poursuit le héros quand il est proche (recherche de chemin
-- sur la grille de navigation), le blesse au contact. Vaincue, elle donne de l'expérience et revient.
local G = require("assets://games/scripts/common.lua")

local Enemy = {
    properties = { hp = 3, sight = 6, damage = 1, xp = 2, respawn = 20, reach = 0.8 },
}

function Enemy:onStart()
    self.homeX, self.homeY = self.entity:position()
    self.life = self.hp
    self.repath, self.wander, self.hitCooldown, self.flash, self.t = 0, 0, 0, 0, math.random() * 6
end

function Enemy:reset()
    if not self.homeX then self:onStart() end
    self.entity:setActive(true)
    self.entity:stopNavigation()
    self.entity:setPosition(self.homeX, self.homeY)
    self.life = self.hp
end

function Enemy:onUpdate(dt)
    local hero = agr.find("Hero")
    local hs = hero and hero:script()
    self.t = self.t + dt
    self.hitCooldown = self.hitCooldown - dt
    self.flash = self.flash - dt
    self.entity:setScale(1, 1 + math.sin(self.t * 12) * 0.08)  -- battement d'ailes
    self.entity:set("SpriteRenderer", "color", self.flash > 0 and { 1, 0.4, 0.4, 1 } or { 1, 1, 1, 1 })
    if not hs or hs.state ~= "playing" then
        self.entity:stopNavigation()
        return
    end
    local x, y = self.entity:position()
    local hx, hy = hero:position()
    hy = hy + 0.5  -- vise le torse
    local d = G.distance(x, y, hx, hy)
    if d < self.sight then
        self.repath = self.repath - dt
        if self.repath <= 0 then
            self.entity:navigateTo(hx, hy)
            self.repath = 0.4
        end
    else
        self.wander = self.wander - dt
        if self.wander <= 0 then
            self.entity:navigateTo(self.homeX + (math.random() - 0.5) * 4, self.homeY + (math.random() - 0.5) * 4)
            self.wander = 2 + math.random() * 2
        end
    end
    if d < self.reach and self.hitCooldown <= 0 then
        hs:damage(self.damage)
        self.hitCooldown = 1
    end
end

function Enemy:hit(n, hero)
    self.life = self.life - n
    self.flash = 0.2
    local x, y = self.entity:position()
    agr.audio.play("assets://audio/hit.wav", { volume = 0.7 })
    agr.fx.burst(x, y + 0.3, 0, { count = 14, color = { 0.6, 1, 0.5 }, speed = 4, life = 0.4, size = 0.18 })
    agr.fx.popup(x, y + 1, 0, "-" .. n, { color = { 1, 0.95, 0.8 }, size = 48 })
    agr.fx.hitStop(0.05)
    agr.fx.shake(0.15, 0.15)
    local hx, hy = hero.entity:position()
    local d = math.max(0.01, G.distance(x, y, hx, hy))
    self.entity:setPosition(x + (x - hx) / d * 0.6, y + (y - hy) / d * 0.6)  -- recul
    self.repath = 0
    if self.life <= 0 then
        self.entity:stopNavigation()
        self.entity:setActive(false)
        hero:defeated(self.entity, self.xp, self.respawn)
        agr.audio.play("assets://audio/launch.wav", { volume = 0.5, pitch = 1.3 })
    end
end

return Enemy
