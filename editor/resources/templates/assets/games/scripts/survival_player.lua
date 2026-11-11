-- Survie (AGR ENGINE). Déplacement : ZQSD / flèches / joystick tactile.
-- Action (Espace, E ou bouton) : frapper un zombie proche, manger des baies, couper du bois,
-- ou remettre du bois dans le feu. Feu (C ou bouton) : allumer un feu de camp (5 bois).
-- La faim baisse sans cesse ; à zéro, la santé baisse. La nuit, les zombies viennent ; le feu
-- les brûle. Survivre 3 nuits.
local G = require("assets://games/scripts/common.lua")

local Player = {
    properties = { speed = 4, maxHealth = 10, hungerRate = 0.8, dayLength = 60, nightLength = 45, nights = 3,
                   fireCost = 5, fireFuel = 60, reach = 1.6 },
}

function Player:onStart()
    self.startX, self.startY = self.entity:position()
    self:restart()
end

function Player:restart()
    if self.fires then for _, f in ipairs(self.fires) do if f.e:isValid() then f.e:destroy() end end end
    if self.zombies then for _, z in ipairs(self.zombies) do if z:isValid() then z:destroy() end end end
    self.fires, self.zombies = {}, {}
    self.health, self.hunger, self.wood = self.maxHealth, 100, 2
    self.day, self.night, self.clock = 1, false, 0
    self.survived, self.spawnTimer, self.toSpawn = 0, 0, 0
    self.cooldown, self.invulnerable, self.messageTime = 0, 0, 0
    self.state = "playing"
    self.bushes, self.trees = {}, {}
    for _, b in ipairs(G.children("Bushes")) do self.bushes[#self.bushes + 1] = { e = b, regrow = 0 } end
    for _, t in ipairs(G.children("Trees")) do
        t:setActive(true)
        self.trees[#self.trees + 1] = { e = t, hits = 0, regrow = 0 }
    end
    for _, b in ipairs(self.bushes) do b.e:set("SpriteRenderer", "color", { 1, 1, 1, 1 }) end
    self.entity:setPosition(self.startX, self.startY)
    self.entity:setVelocity(0, 0)
    G.show("Restart", false)
    self:say(agr.tr("surv.intro"), 6)
    self:hud()
end

function Player:say(text, seconds)
    G.text("Message", text)
    self.messageTime = seconds or 2.5
end

function Player:hud()
    G.text("HealthText", agr.tr("surv.health", math.max(0, math.ceil(self.health)), self.maxHealth))
    G.text("HungerText", agr.tr("surv.hunger", math.floor(self.hunger)))
    G.text("WoodText", agr.tr("surv.wood", self.wood, self.fireCost))
    local hf = agr.find("HealthFill")
    if hf then hf:set("UIRect", "size", { 552 * math.max(0, self.health) / self.maxHealth, 20 }) end
    local gf = agr.find("HungerFill")
    if gf then gf:set("UIRect", "size", { 552 * self.hunger / 100, 20 }) end
    G.text("TimeText", agr.tr(self.night and "surv.night" or "surv.day", self.day))
end

function Player:edge(name)
    local b = agr.find(name)
    local down = b ~= nil and agr.ui.isPressed(b)
    self.buttons = self.buttons or {}
    local was = self.buttons[name]
    self.buttons[name] = down
    return down and not was
end

function Player:onUpdate(dt)
    self.messageTime = self.messageTime - dt
    if self.messageTime <= 0 and self.state == "playing" then G.text("Message", "") end
    local e = self.entity
    if self.state ~= "playing" then
        e:setVelocity(0, 0)
        return
    end
    -- Mouvement.
    local mx, my = agr.input.value("move_x"), agr.input.value("move_y")
    local len = math.sqrt(mx * mx + my * my)
    if len > 1 then mx, my = mx / len, my / len end
    e:setVelocity(mx * self.speed, my * self.speed)
    if mx ~= 0 then e:set("SpriteRenderer", "flipX", mx < 0) end
    self.cooldown = self.cooldown - dt
    self.invulnerable = math.max(0, self.invulnerable - dt)

    -- Faim et santé.
    self.hunger = math.max(0, self.hunger - self.hungerRate * dt)
    if self.hunger <= 0 then self.health = self.health - 0.5 * dt end
    if self.health <= 0 then return self:die() end

    -- Temps : jour puis nuit.
    self.clock = self.clock + dt
    if not self.night and self.clock >= self.dayLength then
        self.night, self.clock = true, 0
        self.toSpawn, self.spawnTimer = 3 + 2 * self.day, 1
        self:say(agr.tr("surv.nightfalls"))
    elseif self.night and self.clock >= self.nightLength then
        self:dawn()
    end
    local night = agr.find("Night")
    if night then
        local a = self.night and 0.55 or math.max(0, (self.clock - self.dayLength + 10) / 10) * 0.55
        night:set("SpriteRenderer", "color", { 0.02, 0.02, 0.1, a })
    end
    if self.night and self.toSpawn > 0 then
        self.spawnTimer = self.spawnTimer - dt
        if self.spawnTimer <= 0 then
            self.spawnTimer = 4
            self.toSpawn = self.toSpawn - 1
            self:spawnZombie()
        end
    end

    -- Feux : le bois brûle.
    for i = #self.fires, 1, -1 do
        local f = self.fires[i]
        f.fuel = f.fuel - dt
        if f.fuel <= 0 then
            f.e:destroy()
            table.remove(self.fires, i)
        else
            f.e:setScale(0.6 + 0.4 * math.min(1, f.fuel / 20), 0.6 + 0.4 * math.min(1, f.fuel / 20), 1)
        end
    end
    -- Ressources qui repoussent.
    for _, b in ipairs(self.bushes) do
        if b.regrow > 0 then
            b.regrow = b.regrow - dt
            if b.regrow <= 0 then b.e:set("SpriteRenderer", "color", { 1, 1, 1, 1 }) end
        end
    end
    for _, t in ipairs(self.trees) do
        if t.regrow > 0 then
            t.regrow = t.regrow - dt
            if t.regrow <= 0 then t.e:setActive(true) end
        end
    end
    for i = #self.zombies, 1, -1 do if not self.zombies[i]:isValid() then table.remove(self.zombies, i) end end

    if agr.input.pressed("jump") or agr.input.keyPressed("E") or self:edge("Action") then self:act() end
    if agr.input.keyPressed("C") or self:edge("Craft") then self:craft() end
    self:hud()
end

function Player:spawnZombie()
    local z = agr.clone(agr.find("Zombie Prototype"), false)
    z:setName("Zombie")
    local edges = { { -16, math.random(-10, 10) }, { 16, math.random(-10, 10) }, { math.random(-16, 16), 10 }, { math.random(-16, 16), -10 } }
    local p = edges[math.random(1, 4)]
    z:setPosition(p[1], p[2], 0)
    table.insert(self.zombies, z)
end

function Player:dawn()
    self.night, self.clock = false, 0
    self.survived = self.survived + 1
    for _, z in ipairs(self.zombies) do if z:isValid() then z:destroy() end end
    self.zombies = {}
    if self.survived >= self.nights then
        self.state = "won"
        local best = G.record("survival.best", self.survived)
        G.text("Message", agr.tr("surv.won", self.survived))
        G.show("Restart", true)
        return
    end
    self.day = self.day + 1
    self:say(agr.tr("surv.dawn", self.survived))
end

function Player:nearest(list, maxDistance, filter)
    local x, y = self.entity:position()
    local best, bestD = nil, maxDistance
    for _, item in ipairs(list) do
        local e = item.e or item
        if e:isValid() and e:isActive() and (not filter or filter(item)) then
            local ex, ey = e:position()
            local d = G.distance(x, y + 0.4, ex, ey + 0.4)
            if d < bestD then best, bestD = item, d end
        end
    end
    return best
end

function Player:act()
    if self.cooldown > 0 then return end
    self.cooldown = 0.35
    local z = self:nearest(self.zombies, self.reach)
    if z then
        local s = z:script()
        if s then s:hit(1, self.entity) end
        agr.audio.play("assets://audio/click.wav", { pitch = 0.6 })
        return
    end
    local f = self:nearest(self.fires, 1.5)
    if f and self.wood > 0 then
        self.wood = self.wood - 1
        f.fuel = math.min(self.fireFuel * 2, f.fuel + 20)
        return
    end
    local b = self:nearest(self.bushes, 1.4, function(b) return b.regrow <= 0 end)
    if b then
        self.hunger = math.min(100, self.hunger + 25)
        b.regrow = 30
        b.e:set("SpriteRenderer", "color", { 0.55, 0.65, 0.55, 1 })
        agr.audio.play("assets://audio/coin.wav", { pitch = 0.8 })
        return
    end
    local t = self:nearest(self.trees, self.reach)
    if t then
        t.hits = t.hits + 1
        agr.audio.play("assets://audio/click.wav", { pitch = 0.5 })
        if t.hits >= 3 then
            t.hits, t.regrow = 0, 45
            t.e:setActive(false)
            self.wood = self.wood + 2
        end
    end
end

function Player:craft()
    if self.wood < self.fireCost then return self:say(agr.tr("surv.nowood", self.fireCost)) end
    self.wood = self.wood - self.fireCost
    local x, y = self.entity:position()
    local fire = agr.clone(agr.find("Campfire Prototype"), false)
    fire:setName("Campfire")
    fire:setPosition(x + 0.9, y, 0)
    table.insert(self.fires, { e = fire, fuel = self.fireFuel })
    agr.audio.play("assets://audio/launch.wav", { volume = 0.5 })
end

-- Zombies : distance au feu le plus proche (ils y brûlent).
function Player:fireNear(x, y, radius)
    for _, f in ipairs(self.fires) do
        local fx, fy = f.e:position()
        if G.distance(x, y, fx, fy) < radius then return fx, fy end
    end
end

function Player:damage(n)
    if self.state ~= "playing" or self.invulnerable > 0 then return end
    self.health = self.health - n
    self.invulnerable = 0.8
    agr.device.vibrate(0.1)
    if self.health <= 0 then self:die() end
end

function Player:die()
    self.state = "dead"
    G.text("Message", agr.tr("surv.dead", self.survived))
    G.show("Restart", true)
end

return Player
