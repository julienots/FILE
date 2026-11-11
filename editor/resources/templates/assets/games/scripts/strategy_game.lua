-- Stratégie en temps réel (AGR ENGINE).
-- Toucher / cliquer une unité bleue : la sélectionner (encore : la désélectionner) ; toucher le sol :
-- y envoyer la sélection. « Armée » sélectionne tous les soldats. Les ouvriers récoltent seuls les
-- cristaux. L'IA rouge produit des soldats et attaque. Détruire la base rouge pour gagner.
local G = require("assets://games/scripts/common.lua")

local Game = {
    properties = { crystals = 100, workerCost = 50, soldierCost = 80, baseHp = 60, redIncome = 6, attackSize = 4, pickRadius = 1.0 },
}

function Game:onStart()
    self.units = { blue = {}, red = {} }
    self:restart()
end

function Game:restart()
    for _, team in pairs(self.units) do for _, u in ipairs(team) do if u:isValid() then u:destroy() end end end
    self.units = { blue = {}, red = {} }
    self.bank = { blue = self.crystals, red = 60 }
    self.hpOf = { blue = self.baseHp, red = self.baseHp }
    self.selected = {}
    self.state = "playing"
    self.redTimer, self.waveTimer = 8, 60
    for _ = 1, 3 do self:spawn("blue", "worker") end
    for _ = 1, 2 do self:spawn("blue", "soldier") end
    for _ = 1, 2 do self:spawn("red", "worker") end
    self:spawn("red", "soldier")
    G.show("Restart", false)
    G.text("Message", "")
    G.text("Hint", agr.tr("strat.hint"))
    self:hud()
end

function Game:base(team) return agr.find(team == "blue" and "Blue Base" or "Red Base") end

function Game:spawn(team, kind)
    local proto = agr.find((team == "blue" and "Blue " or "Red ") .. (kind == "worker" and "Worker" or "Soldier"))
    local u = agr.clone(proto, false)
    u:setName(proto:name() .. " Unit")
    local bx, _, bz = self:base(team):position()
    local dir = team == "blue" and 1 or -1
    u:setPosition(bx + dir * 2.6, 0, bz + (math.random() - 0.5) * 4)
    return u
end

-- Appelés par les unités (strategy_unit.lua).
function Game:addUnit(team, u)
    table.insert(self.units[team], u)
    self:hud()
end
function Game:removeUnit(team, u)
    for i, e in ipairs(self.units[team]) do if e == u then table.remove(self.units[team], i) break end end
    self.selected[u:name() .. tostring(u)] = nil
    self:hud()
end
function Game:deposit(team, amount)
    self.bank[team] = self.bank[team] + amount
    self:hud()
end
function Game:hitBase(team, damage)
    if self.state ~= "playing" then return end
    self.hpOf[team] = math.max(0, self.hpOf[team] - damage)
    local base = self:base(team)
    if base then base:find("Roof"):set("MeshRenderer", "color", { 1, 0.6, 0.5, 1 }) end
    if self.hpOf[team] <= 0 then self:finish(team == "red") end
    self:hud()
end

function Game:finish(won)
    self.state = won and "won" or "lost"
    if won then
        local wins = agr.save.get("strategy.wins", 0) + 1
        agr.save.set("strategy.wins", wins)
        G.text("Message", agr.tr("strat.won", wins))
    else
        G.text("Message", agr.tr("strat.lost"))
    end
    G.show("Restart", true)
    for _, team in pairs(self.units) do for _, u in ipairs(team) do if u:isValid() then u:stopNavigation() end end end
end

function Game:count(team, kind)
    local n = 0
    for _, u in ipairs(self.units[team]) do
        local s = u:isValid() and u:script()
        if s and s.kind == kind then n = n + 1 end
    end
    return n
end

function Game:hud()
    G.text("CrystalsText", agr.tr("strat.crystals", math.floor(self.bank.blue)))
    G.text("ArmyText", agr.tr("strat.army.count", self:count("blue", "worker"), self:count("blue", "soldier")))
    G.text("BasesText", agr.tr("strat.bases", self.hpOf.blue, self.hpOf.red))
end

-- Boutons TrainWorker / TrainSoldier.
function Game:train(button)
    if self.state ~= "playing" then return end
    local kind = button:name() == "TrainWorker" and "worker" or "soldier"
    local cost = kind == "worker" and self.workerCost or self.soldierCost
    if self.bank.blue < cost then
        G.text("Message", agr.tr("strat.nocrystals"))
        self.messageTime = 2
        return
    end
    self.bank.blue = self.bank.blue - cost
    self:spawn("blue", kind)
    agr.audio.play("assets://audio/click.wav")
    self:hud()
end

function Game:setSelected(u, on)
    local s = u:script()
    if not s then return end
    s.selected = on
    local ring = u:find("Ring")
    if ring then ring:setActive(on) end
end

function Game:clearSelection()
    for _, u in ipairs(self.units.blue) do if u:isValid() then self:setSelected(u, false) end end
end

function Game:selectArmy()
    self:clearSelection()
    for _, u in ipairs(self.units.blue) do
        local s = u:isValid() and u:script()
        if s and s.kind == "soldier" then self:setSelected(u, true) end
    end
end

function Game:onUpdate(dt)
    self.messageTime = (self.messageTime or 0) - dt
    if self.messageTime <= 0 and self.state == "playing" then G.text("Message", "") end
    if self.state ~= "playing" then return end
    for _, team in pairs(self.units) do
        for i = #team, 1, -1 do if not team[i]:isValid() then table.remove(team, i) end end
    end
    -- IA rouge : revenu passif, production, vagues d'attaque.
    self.bank.red = self.bank.red + self.redIncome * dt
    self.redTimer = self.redTimer - dt
    if self.redTimer <= 0 then
        self.redTimer = 10
        if self.bank.red >= self.soldierCost then
            self.bank.red = self.bank.red - self.soldierCost
            self:spawn("red", "soldier")
        end
    end
    self.waveTimer = self.waveTimer - dt
    if self:count("red", "soldier") >= self.attackSize or self.waveTimer <= 0 then
        self.waveTimer = 60
        local bx, by, bz = self:base("blue"):position()
        for _, u in ipairs(self.units.red) do
            local s = u:isValid() and u:script()
            if s and s.kind == "soldier" then s:order(bx + 2.5, 0, bz + (math.random() - 0.5) * 3) end
        end
    end
    -- Sélection et ordres.
    local x, y, z = G.tappedWorld(0)
    if x then
        local best, bestD = nil, self.pickRadius
        for _, u in ipairs(self.units.blue) do
            if u:isValid() then
                local ux, _, uz = u:position()
                local d = G.distance(x, z, ux, uz)
                if d < bestD then best, bestD = u, d end
            end
        end
        if best then
            self:setSelected(best, not best:script().selected)
        else
            local k = 0
            for _, u in ipairs(self.units.blue) do
                local s = u:isValid() and u:script()
                if s and s.selected then  -- petite formation autour du point
                    s:order(x + (k % 3 - 1) * 0.9, 0, z + math.floor(k / 3) * 0.9)
                    k = k + 1
                end
            end
        end
    end
end

return Game
