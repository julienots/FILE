-- Zombie : poursuit le survivant (recherche de chemin), le blesse au contact, recule devant le
-- feu et y brûle. Trois coups le détruisent.
local G = require("assets://games/scripts/common.lua")

local Zombie = { properties = { hp = 3, sight = 30, reach = 0.8, fireRadius = 2.5 } }

function Zombie:onStart()
    self.life = self.hp
    self.repath, self.hitCooldown, self.flash, self.burn = 0, 0, 0, 0
end

function Zombie:onUpdate(dt)
    local hero = agr.find("Survivor")
    local hs = hero and hero:script()
    if not hs or hs.state ~= "playing" then
        self.entity:stopNavigation()
        return
    end
    self.hitCooldown = self.hitCooldown - dt
    self.flash = self.flash - dt
    self.entity:set("SpriteRenderer", "color", self.flash > 0 and { 1, 0.5, 0.4, 1 } or { 1, 1, 1, 1 })
    local x, y = self.entity:position()
    local fx, fy = hs:fireNear(x, y, self.fireRadius)
    if fx then  -- le feu : recule et brûle
        self.burn = self.burn + dt
        if self.burn >= 1 then
            self.burn = 0
            self:hit(1, nil)
        end
        if not self.entity:isValid() then return end
        local d = math.max(0.1, G.distance(x, y, fx, fy))
        self.entity:navigateTo(x + (x - fx) / d * 3, y + (y - fy) / d * 3)
        self.repath = 0.5
        return
    end
    local hx, hy = hero:position()
    self.repath = self.repath - dt
    if self.repath <= 0 then
        self.entity:navigateTo(hx, hy)
        self.repath = 0.5
    end
    if G.distance(x, y, hx, hy) < self.reach and self.hitCooldown <= 0 then
        hs:damage(1)
        self.hitCooldown = 1
    end
end

function Zombie:hit(n, from)
    self.life = self.life - n
    self.flash = 0.2
    if from then
        local x, y = self.entity:position()
        local hx, hy = from:position()
        local d = math.max(0.01, G.distance(x, y, hx, hy))
        self.entity:setPosition(x + (x - hx) / d * 0.5, y + (y - hy) / d * 0.5)
    end
    if self.life <= 0 then
        agr.audio.play("assets://audio/launch.wav", { volume = 0.4, pitch = 0.7 })
        self.entity:destroy()
    end
end

return Zombie
