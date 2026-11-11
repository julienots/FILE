-- Unité de stratégie : ouvrier (récolte : mine -> base) ou soldat (attaque l'ennemi le plus proche
-- à portée de vue, obéit aux ordres de déplacement en combattant ce qu'il croise).
local G = require("assets://games/scripts/common.lua")

local Unit = { properties = { team = "blue", kind = "soldier", hp = 6, damage = 1, range = 1.5, sight = 5, cargo = 10, mineTime = 2 } }

function Unit:onStart()
    self.game = agr.find("Game"):script()
    self.life = self.hp
    self.cooldown, self.repath, self.timer = 0, 0, 0
    self.task = self.kind == "worker" and "toMine" or "idle"
    self.game:addUnit(self.team, self.entity)
end

function Unit:onDestroy()
    if self.game and self.entity:isValid() then self.game:removeUnit(self.team, self.entity) end
end

function Unit:enemyTeam() return self.team == "blue" and "red" or "blue" end

function Unit:order(x, y, z)
    self.dest = { x, y, z }
    self.entity:navigateTo(x, y, z)
end

function Unit:nearestEnemy(radius)
    local x, _, z = self.entity:position()
    local best, bestD = nil, radius
    for _, u in ipairs(self.game.units[self:enemyTeam()]) do
        if u:isValid() then
            local ux, _, uz = u:position()
            local d = G.distance(x, z, ux, uz)
            if d < bestD then best, bestD = u, d end
        end
    end
    local base = self.game:base(self:enemyTeam())
    local bx, _, bz = base:position()
    local d = G.distance(x, z, bx, bz) - 1.5
    if d < bestD then best, bestD = base, d end
    return best, bestD
end

function Unit:nearestMine()
    local x, _, z = self.entity:position()
    local best, bestD = nil, math.huge
    for _, m in ipairs(G.children("Mines")) do
        local mx, _, mz = m:position()
        local d = G.distance(x, z, mx, mz)
        if d < bestD then best, bestD = m, d end
    end
    return best
end

function Unit:onUpdate(dt)
    if self.game.state ~= "playing" then return end
    self.cooldown = self.cooldown - dt
    self.repath = self.repath - dt
    local x, _, z = self.entity:position()
    if self.kind == "worker" then return self:work(dt, x, z) end

    local target, d = self:nearestEnemy(self.dest and self.range + 0.3 or self.sight)
    if target then
        if d <= self.range then
            self.entity:stopNavigation()
            if self.cooldown <= 0 then
                self.cooldown = 1
                if target:name():find("Base$") then self.game:hitBase(self:enemyTeam(), self.damage)
                else
                    local s = target:script()
                    if s then s:hit(self.damage) end
                end
                agr.audio.play("assets://audio/click.wav", { volume = 0.25, pitch = 0.8 })
            end
            return
        elseif self.repath <= 0 then
            local tx, ty, tz = target:position()
            self.entity:navigateTo(tx, 0, tz)
            self.repath = 0.6
            return
        end
    end
    if self.dest then
        if self.entity:navigationStatus() == "arrived" then self.dest = nil
        elseif self.repath <= 0 and self.entity:navigationStatus() == "idle" then
            self.entity:navigateTo(self.dest[1], self.dest[2], self.dest[3])
            self.repath = 1
        end
    end
end

function Unit:work(dt, x, z)
    if self.dest then  -- un ordre du joueur interrompt la récolte
        if self.entity:navigationStatus() == "arrived" then
            self.dest = nil
            self.task = "toMine"
        end
        return
    end
    if self.task == "toMine" then
        local m = self:nearestMine()
        local mx, my, mz = m:position()
        if G.distance(x, z, mx, mz) < 1.6 then
            self.entity:stopNavigation()
            self.task, self.timer = "mining", self.mineTime
        elseif self.repath <= 0 then
            self.entity:navigateTo(mx, 0, mz)
            self.repath = 1
        end
    elseif self.task == "mining" then
        self.timer = self.timer - dt
        if self.timer <= 0 then self.task = "toBase" end
    elseif self.task == "toBase" then
        local bx, _, bz = self.game:base(self.team):position()
        if G.distance(x, z, bx, bz) < 2.8 then
            self.game:deposit(self.team, self.cargo)
            self.task = "toMine"
        elseif self.repath <= 0 then
            self.entity:navigateTo(bx + (self.team == "blue" and 2 or -2), 0, bz)
            self.repath = 1
        end
    end
end

function Unit:hit(n)
    self.life = self.life - n
    if self.life <= 0 then
        self.game:removeUnit(self.team, self.entity)
        self.entity:destroy()
    end
end

return Unit
