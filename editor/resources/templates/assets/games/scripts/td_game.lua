-- Partie de tower defense (AGR ENGINE) : vagues, or, vies, construction et amélioration des tours.
-- Toucher / cliquer un emplacement vert : construire ; toucher une tour : l'améliorer (3 niveaux).
-- Bouton « Vague suivante », N ou Espace : lancer la vague.
local G = require("assets://games/scripts/common.lua")

local Game = {
    properties = { gold = 100, lives = 10, waves = 10, towerCost = 50, upgradeCost = 40, slotRadius = 0.9 },
}

function Game:onStart()
    self.path = {}
    for _, p in ipairs(G.children("Path")) do
        local x, y = p:position()
        table.insert(self.path, { x = x, y = y })
    end
    self.creeps, self.towers = {}, {}
    self:restart()
end

function Game:restart()
    for _, c in ipairs(self.creeps) do if c:isValid() then c:destroy() end end
    for _, t in pairs(self.towers) do if t:isValid() then t:destroy() end end
    self.creeps, self.towers = {}, {}
    self.goldNow, self.livesNow, self.wave, self.kills = self.gold, self.lives, 0, 0
    self.spawning = nil
    self.state = "playing"
    self.creepSpec = { hp = 3, speed = 1.6, reward = 5 }
    G.show("Restart", false)
    G.show("NextWave", true)
    G.text("Message", "")
    G.text("Hint", agr.tr("td.hint", self.towerCost, self.upgradeCost))
    self:hud()
end

function Game:hud()
    G.text("GoldText", agr.tr("td.gold", self.goldNow))
    G.text("LivesText", agr.tr("td.lives", self.livesNow))
    G.text("WaveText", agr.tr("td.wave", self.wave, self.waves))
end

function Game:nextWave()
    if self.state ~= "playing" or self.spawning or self.wave >= self.waves then return end
    self.wave = self.wave + 1
    self.creepSpec = { hp = 1 + self.wave * 2, speed = 1.5 + self.wave * 0.1, reward = 5 + self.wave }
    self.spawning = { left = 6 + self.wave * 2, timer = 0 }
    G.text("Message", "")
    agr.audio.play("assets://audio/launch.wav", { volume = 0.5 })
    self:hud()
end

-- Appelés par les créatures (td_creep.lua).
function Game:addCreep(c) table.insert(self.creeps, c) end
function Game:removeCreep(c)
    for i, e in ipairs(self.creeps) do
        if e == c then
            table.remove(self.creeps, i)
            return
        end
    end
end
function Game:killed(c, reward)
    self:removeCreep(c)
    self.goldNow = self.goldNow + reward
    self.kills = self.kills + 1
    self:hud()
end
function Game:leak(c)
    self:removeCreep(c)
    if self.state ~= "playing" then return end
    self.livesNow = self.livesNow - 1
    agr.device.vibrate(0.1)
    agr.fx.shake(0.35, 0.3)
    agr.fx.flash(1, 0.1, 0.05, 0.3, 0.3)
    agr.audio.play("assets://audio/hurt.wav", { volume = 0.5 })
    if self.livesNow <= 0 then
        self.state = "lost"
        G.text("Message", agr.tr("td.lost", self.wave))
        G.show("Restart", true)
        G.show("NextWave", false)
    end
    self:hud()
end

function Game:slotAt(x, y)
    local best, bestD = nil, self.slotRadius
    for _, s in ipairs(G.children("Slots")) do
        local sx, sy = s:position()
        local d = G.distance(x, y, sx, sy)
        if d < bestD then best, bestD = s, d end
    end
    return best
end

function Game:build(slot)
    local name = slot:name()
    local tower = self.towers[name]
    if tower and tower:isValid() then  -- amélioration
        local s = tower:script()
        if not s or s.level >= 3 then return end
        if self.goldNow < self.upgradeCost then
            G.text("Message", agr.tr("td.nogold"))
            return
        end
        self.goldNow = self.goldNow - self.upgradeCost
        s:upgrade()
    else
        if self.goldNow < self.towerCost then
            G.text("Message", agr.tr("td.nogold"))
            return
        end
        self.goldNow = self.goldNow - self.towerCost
        local x, y = slot:position()
        tower = agr.clone(agr.find("Tower Prototype"), false)
        tower:setName("Tower " .. name)
        tower:setPosition(x, y, 0)
        self.towers[name] = tower
    end
    agr.audio.play("assets://audio/click.wav")
    self:hud()
end

function Game:onUpdate(dt)
    if self.state ~= "playing" then return end
    if self.spawning then
        self.spawning.timer = self.spawning.timer - dt
        if self.spawning.timer <= 0 then
            local c = agr.clone(agr.find("Creep Prototype"), false)
            c:setName("Creep")
            c:setPosition(self.path[1].x, self.path[1].y, 0)
            self.spawning.left = self.spawning.left - 1
            self.spawning.timer = 0.9
            if self.spawning.left <= 0 then self.spawning = nil end
        end
    end
    for i = #self.creeps, 1, -1 do  -- créatures détruites autrement
        if not self.creeps[i]:isValid() then table.remove(self.creeps, i) end
    end
    if not self.spawning and #self.creeps == 0 and self.wave >= self.waves then
        self.state = "won"
        local best = G.record("td.bestKills", self.kills)
        G.text("Message", agr.tr("td.won", self.livesNow, best))
        G.show("Restart", true)
        G.show("NextWave", false)
        return
    end
    local x, y = G.tappedWorld()
    if x then
        local slot = self:slotAt(x, y)
        if slot then self:build(slot) end
    end
    if agr.input.keyPressed("N") or agr.input.pressed("jump") then self:nextWave() end
end

return Game
