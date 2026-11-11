-- Tycoon de rue commerçante (AGR ENGINE). Toucher une parcelle vide : construire une boutique ;
-- toucher une boutique : l'améliorer (service plus rapide, prix plus élevé, 5 niveaux).
-- Les clients arrivent plus souvent quand la réputation est bonne ; une file trop longue les fait
-- repartir mécontents. Objectif : 2000 pièces.
local G = require("assets://games/scripts/common.lua")

local Tycoon = {
    properties = { money = 250, buildCost = 100, goal = 2000, dayLength = 90, patience = 12, plotRadius = 1.2, streetY = -3.0 },
}

function Tycoon:onStart()
    self.shops = {}
    for _, p in ipairs(G.children("Plots")) do
        self.shops[#self.shops + 1] = { plot = p, building = p:find("Shop"), level = 0, queue = {}, timer = 0 }
    end
    self.customers = {}
    self:restart()
end

function Tycoon:restart()
    for _, c in ipairs(self.customers) do if c:isValid() then c:destroy() end end
    self.customers = {}
    for _, s in ipairs(self.shops) do
        s.level, s.queue, s.timer = 0, {}, 0
        s.building:setActive(false)
    end
    self.moneyNow, self.reputation, self.day, self.clock, self.spawnTimer = self.money, 3, 1, 0, 1
    self.served, self.unhappy = 0, 0
    self.state = "playing"
    G.show("Restart", false)
    G.text("Message", "")
    G.text("Hint", agr.tr("tycoon.hint", self.buildCost))
    self:hud()
end

function Tycoon:hud()
    G.text("MoneyText", agr.tr("tycoon.money", math.floor(self.moneyNow)))
    G.text("ReputationText", agr.tr("tycoon.reputation", string.rep("★", math.floor(self.reputation + 0.5)) .. string.rep("☆", 5 - math.floor(self.reputation + 0.5))))
    G.text("DayText", agr.tr("tycoon.day", self.day, self.served, self.unhappy))
    G.text("Goal", agr.tr("tycoon.goal", self.goal))
end

function Tycoon:serviceTime(s) return 4 * 0.8 ^ (s.level - 1) end
function Tycoon:price(s) return 8 + 4 * (s.level - 1) end
function Tycoon:upgradeCost(s) return 120 * s.level end

function Tycoon:openShops()
    local t = {}
    for _, s in ipairs(self.shops) do if s.level > 0 then t[#t + 1] = s end end
    return t
end

function Tycoon:tapShop(s)
    if s.level == 0 then
        if self.moneyNow < self.buildCost then return self:say(agr.tr("tycoon.nomoney")) end
        self.moneyNow = self.moneyNow - self.buildCost
        s.level = 1
        s.building:setActive(true)
    elseif s.level < 5 then
        local cost = self:upgradeCost(s)
        if self.moneyNow < cost then return self:say(agr.tr("tycoon.nomoney")) end
        self.moneyNow = self.moneyNow - cost
        s.level = s.level + 1
        s.building:setScale(1 + (s.level - 1) * 0.08, 1 + (s.level - 1) * 0.08, 1)
        self:say(agr.tr("tycoon.upgraded", s.level, self:price(s)))
    end
    agr.audio.play("assets://audio/click.wav")
    self:hud()
end

function Tycoon:say(text)
    G.text("Message", text)
    self.messageTime = 2.5
end

-- Appelés par les clients (tycoon_customer.lua).
function Tycoon:pickShop()
    local open = self:openShops()
    if #open == 0 then return nil end
    table.sort(open, function(a, b) return #a.queue < #b.queue end)  -- la file la plus courte
    return open[1]
end
function Tycoon:arrive(c, shop) table.insert(shop.queue, c) end
function Tycoon:giveUp(c, shop)
    for i, e in ipairs(shop.queue) do if e == c then table.remove(shop.queue, i) break end end
    self.unhappy = self.unhappy + 1
    self.reputation = math.max(0, self.reputation - 0.3)
    self:hud()
end
function Tycoon:addCustomer(c) table.insert(self.customers, c) end
function Tycoon:removeCustomer(c)
    for i, e in ipairs(self.customers) do if e == c then table.remove(self.customers, i) return end end
end

function Tycoon:onUpdate(dt)
    self.messageTime = (self.messageTime or 0) - dt
    if self.messageTime <= 0 and self.state == "playing" then G.text("Message", "") end
    if self.state ~= "playing" then return end
    self.clock = self.clock + dt
    if self.clock >= self.dayLength then
        self.clock, self.day = 0, self.day + 1
        self:hud()
    end
    -- Arrivées : plus fréquentes avec une bonne réputation (et seulement si une boutique est ouverte).
    self.spawnTimer = self.spawnTimer - dt
    if self.spawnTimer <= 0 and #self:openShops() > 0 then
        self.spawnTimer = 6 / (0.5 + self.reputation * 0.5) * (0.7 + math.random() * 0.6)
        local c = agr.clone(agr.find("Customer Prototype"), false)
        c:setName("Customer")
        c:setPosition(-15, self.streetY, 0)
    end
    -- Service : le premier de chaque file est servi au bout du temps de service.
    for _, s in ipairs(self.shops) do
        local first = s.queue[1]
        if first and first:isValid() then
            local cs = first:script()
            if cs and cs.state == "waiting" then
                s.timer = s.timer + dt
                if s.timer >= self:serviceTime(s) then
                    s.timer = 0
                    table.remove(s.queue, 1)
                    self.moneyNow = self.moneyNow + self:price(s)
                    self.served = self.served + 1
                    self.reputation = math.min(5, self.reputation + 0.1)
                    cs:served()
                    agr.audio.play("assets://audio/coin.wav", { volume = 0.5 })
                    self:hud()
                end
            end
        elseif first then
            table.remove(s.queue, 1)
        end
    end
    if self.moneyNow >= self.goal then
        self.state = "won"
        local best = G.record("tycoon.bestDays", self.day, false)
        G.text("Message", agr.tr("tycoon.won", self.day, best))
        G.show("Restart", true)
        return
    end
    local x, y = G.tappedWorld()
    if x then
        for _, s in ipairs(self.shops) do
            local px, py = s.plot:position()
            if G.distance(x, y, px, py) < self.plotRadius then
                self:tapShop(s)
                break
            end
        end
    end
end

return Tycoon
