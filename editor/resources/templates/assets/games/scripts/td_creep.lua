-- Créature du tower defense : suit le chemin (enfants de « Path »), meurt sous les tirs.
local G = require("assets://games/scripts/common.lua")

local Creep = {}

function Creep:onStart()
    self.game = agr.find("Game"):script()
    local spec = self.game.creepSpec
    self.hp, self.maxHp, self.speed, self.reward = spec.hp, spec.hp, spec.speed, spec.reward
    self.target = 2
    self.t = math.random() * 6
    self.game:addCreep(self.entity)
end

-- Avancement le long du chemin (les tours visent la créature la plus avancée).
function Creep:progress() return self.target end

function Creep:onUpdate(dt)
    if self.game.state ~= "playing" then return end
    local p = self.game.path[self.target]
    local x, y = self.entity:position()
    local d = G.distance(x, y, p.x, p.y)
    local step = self.speed * dt
    if d <= step then
        self.entity:setPosition(p.x, p.y, 0)
        self.target = self.target + 1
        if self.target > #self.game.path then
            self.game:leak(self.entity)
            self.entity:destroy()
        end
        return
    end
    self.entity:setPosition(x + (p.x - x) / d * step, y + (p.y - y) / d * step, 0)
    if math.abs(p.x - x) > 0.01 then self.entity:set("SpriteRenderer", "flipX", p.x > x) end
    self.t = self.t + dt * 10
    self.entity:setScale(1 + math.sin(self.t) * 0.06, 1 - math.sin(self.t) * 0.06)
    local k = self.hp / self.maxHp
    self.entity:set("SpriteRenderer", "color", { 1, 0.5 + 0.5 * k, 0.5 + 0.5 * k, 1 })
end

function Creep:hit(damage)
    if self.dead then return end
    self.hp = self.hp - damage
    if self.hp <= 0 then
        self.dead = true
        self.game:killed(self.entity, self.reward)
        agr.audio.play("assets://audio/coin.wav", { volume = 0.4, pitch = 1.2 })
        self.entity:destroy()
    end
end

return Creep
