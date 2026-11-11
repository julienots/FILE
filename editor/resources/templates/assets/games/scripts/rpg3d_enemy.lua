-- Golem : garde son territoire, poursuit le héros sur le maillage de navigation quand il approche,
-- frappe au contact. Vaincu, il donne de l'expérience et revient plus tard.
local G = require("assets://games/scripts/common.lua")

local Golem = { properties = { hp = 4, sight = 12, leash = 25, damage = 2, xp = 3, respawn = 25, reach = 1.6 } }

function Golem:onStart()
    self.homeX, self.homeY, self.homeZ = self.entity:position()
    self.life = self.hp
    self.repath, self.hitCooldown, self.flash, self.t = 0, 0, 0, math.random() * 6
end

function Golem:reset()
    self.entity:stopNavigation()
    self.life = self.hp
end

function Golem:onUpdate(dt)
    local hero = agr.find("Hero")
    local hs = hero and hero:script()
    self.t = self.t + dt
    self.hitCooldown = self.hitCooldown - dt
    self.flash = self.flash - dt
    local s = 1.0 + (self.flash > 0 and 0.12 or 0)
    self.entity:setScale(s, s, s)
    if not hs or hs.state ~= "playing" then
        self.entity:stopNavigation()
        return
    end
    local x, y, z = self.entity:position()
    local hx, hy, hz = hero:position()
    local d = G.distance(x, z, hx, hz)
    local fromHome = G.distance(x, z, self.homeX, self.homeZ)
    self.repath = self.repath - dt
    if d < self.sight and fromHome < self.leash then
        if self.repath <= 0 then
            self.entity:navigateTo(hx, hy, hz)
            self.repath = 0.5
        end
    elseif self.repath <= 0 then
        self.entity:navigateTo(self.homeX, self.homeY, self.homeZ)
        self.repath = 2
    end
    if d < self.reach and self.hitCooldown <= 0 then
        hs:damage(self.damage)
        self.hitCooldown = 1.2
    end
end

function Golem:hit(n, hero)
    self.life = self.life - n
    self.flash = 0.15
    self.repath = 0
    if self.life <= 0 then
        self.entity:stopNavigation()
        self.entity:setActive(false)
        hero:defeated(self.entity, self.xp, self.respawn)
        agr.audio.play("assets://audio/launch.wav", { volume = 0.6, pitch = 0.6 })
    end
end

return Golem
