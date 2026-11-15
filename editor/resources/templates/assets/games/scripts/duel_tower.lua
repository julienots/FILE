-- Tour du Duel des Tours : tire sur l'unité ennemie la plus proche à portée. La tour du roi ne
-- s'éveille que si elle est touchée ou si une tour de princesse de son camp tombe.
local G = require("assets://games/scripts/common.lua")

local Tower = { properties = { team = "blue", king = false, hp = 1400, range = 6.5, dmg = 90, hit = 0.8 } }

function Tower:onStart()
    self.game = agr.find("Game"):script()
    self.fill = self.entity:find("HpFill")
    self.fillWidth = self.fill and select(1, self.fill:scale()) or 1
    self:reset()
end

function Tower:reset()
    self.maxHp = self.hp
    self.life = self.hp
    self.awake = not self.king
    self.cool = 0.5
    self.destroyed = false
    if self.fill then
        local _, sy, sz = self.fill:scale()
        self.fill:setScale(self.fillWidth, sy, sz)
    end
end

Tower.isTower = true

function Tower:radius() return self.king and 1.25 or 1.0 end

function Tower:onUpdate(dt)
    if self.destroyed or self.game.state ~= "battle" or not self.awake then return end
    self.cool = self.cool - dt
    if self.cool > 0 then return end
    local x, _, z = self.entity:position()
    local best, bestD = nil, self.range + (self.king and 0.5 or 0)
    for _, u in ipairs(self.game:unitsOf(self.game:enemyOf(self.team))) do
        local ux, _, uz = u:position()
        local d = G.distance(x, z, ux, uz)
        if d < bestD then best, bestD = u, d end
    end
    if best then
        self.cool = self.hit
        self.game:attack(self.entity, best, self.dmg, true)
    end
end

function Tower:damage(n)
    if self.destroyed then return end
    self.life = self.life - n
    self.awake = true
    if self.fill then
        local _, sy, sz = self.fill:scale()
        self.fill:setScale(self.fillWidth * math.max(0, self.life / self.maxHp), sy, sz)
    end
    if self.life <= 0 then
        self.destroyed = true
        local x, _, z = self.entity:position()
        agr.fx.burst(x, 2, z, { count = 80, color = { 0.8, 0.75, 0.7 }, speed = 6, life = 1, size = 0.4, additive = false })
        agr.fx.shake(0.5, 0.5)
        agr.audio.play("assets://audio/slam.wav", { volume = 0.7 })
        -- Le jeu d'abord : désactiver l'entité détruit l'instance de ce script.
        self.game:towerDestroyed(self.entity, self.team, self.king)
        self.entity:setActive(false)
    end
end

-- Une tour de princesse tombe : le roi s'éveille.
function Tower:wake() self.awake = true end

return Tower
