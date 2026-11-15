-- Unité du Duel des Tours. Son équipe est dans son nom (« Knight red 7 ») : le jeu la clone d'un
-- prototype puis la nomme. Elle cherche une cible (unité ennemie à portée de vue, sinon la tour
-- ennemie la plus proche ; le géant et le mini-tank ne visent que les tours), y va par le
-- maillage de navigation (les ponts) et l'attaque.
local G = require("assets://games/scripts/common.lua")

local STATS = {
    knight = { hp = 650, dmg = 75, range = 1.2, hit = 1.1, sight = 5.5 },
    archer = { hp = 250, dmg = 45, range = 5.0, hit = 1.0, sight = 6.0, ranged = true },
    giant = { hp = 1900, dmg = 120, range = 1.3, hit = 1.5, sight = 7.0, buildings = true },
    goblin = { hp = 160, dmg = 50, range = 1.0, hit = 0.9, sight = 5.0 },
    tank = { hp = 1100, dmg = 160, range = 4.5, hit = 2.0, sight = 6.5, buildings = true, ranged = true },
}

local Unit = { properties = { kind = "knight" } }

function Unit:onStart()
    self.game = agr.find("Game"):script()
    self.team = self.entity:name():match("^%a+ (%a+)") or "blue"
    self.stats = STATS[self.kind] or STATS.knight
    self.hp, self.maxHp = self.stats.hp, self.stats.hp
    self.cool, self.retarget, self.repath = 0.3, 0, 0
    self.fill = self.entity:find("HpFill")
    self.fillWidth = self.fill and select(1, self.fill:scale()) or 1
    if self.team == "red" then
        for _, part in ipairs({ "Body", "Turret", "HpFill" }) do
            local e = self.entity:find(part)
            if e then e:set("MeshRenderer", "color", { 0.95, 0.3, 0.25, 1 }) end
        end
    end
    agr.tween(self.entity, { scale = { 1.3, 1.3, 1.3 } }, 0.3, { from = { 0.2, 0.2, 0.2 }, ease = "outBack" })
    self.game:register(self.entity)
end

function Unit:radius() return self.kind == "giant" and 0.6 or 0.35 end

function Unit:pickTarget()
    local x, _, z = self.entity:position()
    local best, bestD = nil, math.huge
    if not self.stats.buildings then
        for _, u in ipairs(self.game:unitsOf(self.game:enemyOf(self.team))) do
            local ux, _, uz = u:position()
            local d = G.distance(x, z, ux, uz)
            if d < self.stats.sight and d < bestD then best, bestD = u, d end
        end
    end
    if not best then
        for _, t in ipairs(self.game:towersOf(self.game:enemyOf(self.team))) do
            local tx, _, tz = t:position()
            local d = G.distance(x, z, tx, tz)
            if d < bestD then best, bestD = t, d end
        end
    end
    return best
end

function Unit:onUpdate(dt)
    if self.dead then return end
    if self.game.state ~= "battle" then
        self.entity:stopNavigation()
        return
    end
    self.cool = self.cool - dt
    self.retarget = self.retarget - dt
    self.repath = self.repath - dt
    if self.retarget <= 0 or not (self.target and self.target:isValid() and self.target:isActive()) then
        self.target = self:pickTarget()
        self.retarget = 0.4
    end
    local t = self.target
    if not t then
        self.entity:stopNavigation()
        return
    end
    local x, _, z = self.entity:position()
    local tx, _, tz = t:position()
    local ts = t:script()
    local reach = self.stats.range + self:radius() + ((ts and ts.radius) and ts:radius() or 0.4)
    if G.distance(x, z, tx, tz) <= reach then
        self.entity:stopNavigation()
        self.entity:setRotation(0, math.deg(math.atan(tx - x, tz - z)), 0)
        if self.cool <= 0 then
            self.cool = self.stats.hit
            self.game:attack(self.entity, t, self.stats.dmg, self.stats.ranged)
        end
    elseif self.repath <= 0 then
        -- Une tour est un obstacle du maillage de navigation : viser un point devant elle.
        if ts and ts.isTower then
            local d = math.max(0.001, G.distance(x, z, tx, tz))
            local r = ts:radius() + 0.6
            tx, tz = tx + (x - tx) / d * r, tz + (z - tz) / d * r
        end
        self.entity:navigateTo(tx, 0, tz)
        self.repath = 0.5
    end
end

function Unit:damage(n)
    if self.dead then return end
    self.hp = self.hp - n
    if self.fill then
        local f = math.max(0, self.hp / self.maxHp)
        local _, sy, sz = self.fill:scale()
        self.fill:setScale(self.fillWidth * f, sy, sz)
    end
    if self.hp <= 0 then
        self.dead = true
        local x, y, z = self.entity:position()
        agr.fx.burst(x, y + 0.6, z, { count = 18, color = self.team == "blue" and { 0.4, 0.6, 1 } or { 1, 0.4, 0.3 }, speed = 4, life = 0.5 })
        self.entity:stopNavigation()
        self.game:unregister(self.entity)
        self.entity:destroy()
    end
end

return Unit
