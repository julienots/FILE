-- Les Ruines d'Ambre (AGR ENGINE) : aventure-action 2.5D. Trois niveaux (ramasser les trois gemmes
-- d'ambre ouvre la grille de sortie, le portail mène plus loin), puis l'arène du boss. Menu titre,
-- pause (P ou bouton II), game over, victoire ; progression, pièces et meilleur temps sauvegardés.
local G = require("assets://games/scripts/common.lua")

local Game = { properties = { spacing = 70, levels = 4, pickupRadius = 1.0, portalRadius = 1.6 } }

local NAMES = { "ambre.area1", "ambre.area2", "ambre.area3", "ambre.area4" }

-- Positions de départ (locales) des enfants d'une entité, par nom.
local function homesOf(parent)
    local t = {}
    for _, e in ipairs(parent and parent:children() or {}) do t[e:name()] = { e:position() } end
    return t
end

function Game:onStart()
    self.state, self.level, self.coins, self.gems = "menu", 1, 0, 0
    self.time, self.messageTime, self.nextTime = 0, 0, 0
    self.homes, self.itemHomes = {}, {}
    for k = 1, self.levels do
        self.homes[k] = homesOf(self:path(k, "Enemies"))
        self.itemHomes[k] = homesOf(self:path(k, "Items"))
    end
    self.hero = agr.find("Hero")
    self:showMenu("title")
    self:hud()
end

-- Entité d'une zone : self:path(2, "Items") -> l'entité « Items » de « Area 2 ».
function Game:path(k, child)
    local area = agr.find("Area " .. k)
    return area and area:find(child)
end

function Game:enemies()
    local list = {}
    local group = self:path(self.level, "Enemies")
    if group then
        for _, e in ipairs(group:children()) do
            if e:isActive() then list[#list + 1] = e end
        end
    end
    return list
end

-- ---- menus
function Game:showMenu(kind)
    self.menuKind = kind
    G.show("Menu", true)
    local saved = agr.save.get("ambre.level", 1)
    local best = agr.save.get("ambre.bestTime")
    local text
    if kind == "title" then
        text = agr.tr("ambre.intro")
        if best then text = text .. "\n" .. agr.tr("ambre.best", self:clock(best)) end
    elseif kind == "pause" then text = agr.tr("ambre.paused")
    elseif kind == "gameover" then text = agr.tr("ambre.gameover", agr.tr(NAMES[self.level]))
    else text = agr.tr("ambre.victory", self:clock(self.time), self.coins) end
    G.text("MenuTitle", kind == "victory" and agr.tr("ambre.victory.title") or (kind == "gameover" and agr.tr("ambre.gameover.title") or "Les Ruines d'Ambre"))
    G.text("MenuText", text)
    G.show("PlayButton", kind ~= "pause")
    G.show("ContinueButton", (kind == "title" and saved > 1) or kind == "gameover")
    G.show("ResumeButton", kind == "pause")
    G.show("MenuButton", kind ~= "title")
    for _, name in ipairs({ "Jump", "Attack", "PauseButton" }) do G.show(name, false) end
end

function Game:hideMenu()
    G.show("Menu", false)
    for _, name in ipairs({ "Jump", "Attack", "PauseButton" }) do G.show(name, true) end
end

function Game:newGame()
    self.coins, self.time = 0, 0
    agr.save.set("ambre.level", 1)
    agr.save.set("ambre.coins", 0)
    self:startLevel(1)
end

function Game:continueGame()
    if self.menuKind == "gameover" then
        self:startLevel(self.level)
        return
    end
    self.coins = agr.save.get("ambre.coins", 0)
    self.time = agr.save.get("ambre.time", 0)
    self:startLevel(G.clamp(agr.save.get("ambre.level", 1), 1, self.levels))
end

function Game:togglePause()
    if self.state == "playing" then
        self.state = "paused"
        self:showMenu("pause")
    elseif self.state == "paused" then
        self.state = "playing"
        self:hideMenu()
    end
end

function Game:toMenu()
    self.state = "menu"
    self:clearMinions()
    G.show("BossBar", false)
    self:showMenu("title")
end

-- ---- levels
function Game:clearMinions()
    local group = self:path(self.levels, "Enemies")
    if not group then return end
    for _, e in ipairs(group:children()) do
        if e:name() == "Minion" then e:destroy() end
    end
end

function Game:startLevel(k)
    self.level, self.gems = k, 0
    self.levelCoins = self.coins
    agr.save.set("ambre.level", k)
    local items = self:path(k, "Items")
    for _, e in ipairs(items and items:children() or {}) do G.respawn(e, self.itemHomes[k][e:name()]) end
    self:clearMinions()
    local foes = self:path(k, "Enemies")
    for _, e in ipairs(foes and foes:children() or {}) do G.respawn(e, self.homes[k][e:name()]) end
    local gate = self:path(k, "Gate")
    if gate then gate:setActive(true) end
    self.hero:script():spawn((k - 1) * self.spacing - 13, 0.05, 0)
    G.show("BossBar", k == self.levels)
    self:bossHealth(1)
    self.state = "playing"
    self:hideMenu()
    G.text("LevelText", agr.tr(NAMES[k]))
    self:say(k == self.levels and agr.tr("ambre.boss") or agr.tr("ambre.goal"), 3)
    self:hud()
end

function Game:openGate()
    local gate = self:path(self.level, "Gate")
    if gate and gate:isActive() then
        gate:setActive(false)
        agr.audio.play("assets://audio/coin.wav", { pitch = 0.5 })
        self:say(agr.tr("ambre.gateOpen"), 2.5)
    end
end

function Game:levelComplete()
    if self.level >= self.levels then
        self.state = "victory"
        local best, isNew = G.record("ambre.bestTime", self.time, false)
        agr.save.set("ambre.level", 1)
        agr.save.set("ambre.coins", 0)
        G.show("BossBar", false)
        self:showMenu("victory")
        if isNew then G.text("MenuText", agr.tr("ambre.victory", self:clock(self.time), self.coins) .. "\n" .. agr.tr("ambre.record")) end
        self.bestTime = best
        return
    end
    self.state = "levelcomplete"
    self.nextTime = 1.5
    agr.save.set("ambre.level", self.level + 1)
    agr.save.set("ambre.coins", self.coins)
    agr.save.set("ambre.time", self.time)
    agr.audio.play("assets://audio/coin.wav", { pitch = 1.5 })
    self:say(agr.tr("ambre.cleared"), 1.5)
end

function Game:heroDied()
    self.state = "gameover"
    self.coins = self.levelCoins or self.coins
    self:clearMinions()
    G.show("BossBar", false)
    self:showMenu("gameover")
end

function Game:enemyDefeated(kind)
    if kind == "slime" or kind == "bat" then self.coins = self.coins + 1 end
    self:hud()
end

function Game:bossHealth(f)
    local fill = agr.find("BossFill")
    if fill then fill:set("UIRect", "size", { 900 * G.clamp(f, 0, 1), 30 }) end
end

function Game:bossDefeated()
    self:clearMinions()
    G.show("BossBar", false)
    self:openGate()
    self:say(agr.tr("ambre.bossDown"), 3)
end

-- ---- HUD
function Game:clock(t)
    return string.format("%d:%02d", math.floor(t / 60), math.floor(t % 60))
end

function Game:say(text, seconds)
    G.text("Message", text)
    self.messageTime = seconds or 2
end

function Game:hud()
    local hs = self.hero and self.hero:script()
    if hs and hs.health then G.text("HealthText", agr.tr("ambre.health", math.max(0, hs.health), hs.maxHealth)) end
    G.text("GemsText", self.level < self.levels and agr.tr("ambre.gems", self.gems) or agr.tr("ambre.gems.boss"))
    G.text("CoinsText", agr.tr("ambre.coins", self.coins))
end

-- ---- frame
function Game:onUpdate(dt)
    self.messageTime = self.messageTime - dt
    if self.messageTime <= 0 then G.text("Message", "") end
    if agr.input.keyPressed("P") or agr.input.gamepadPressed("Start") then self:togglePause() end
    if self.state == "levelcomplete" then
        self.nextTime = self.nextTime - dt
        if self.nextTime <= 0 then self:startLevel(self.level + 1) end
        return
    end
    if self.state ~= "playing" then return end
    self.time = self.time + dt
    local hx, hy, hz = self.hero:position()
    local items = self:path(self.level, "Items")
    for _, item in ipairs(items and items:children() or {}) do
        if item:isActive() then
            local ix, iy, iz = item:worldPosition()
            if G.distance(hx, hz, ix, iz) < self.pickupRadius and math.abs(hy - iy) < 1.5 then self:collect(item) end
        end
    end
    local portal = self:path(self.level, "Portal")
    local gate = self:path(self.level, "Gate")
    if portal and gate and not gate:isActive() then
        local px, _, pz = portal:worldPosition()
        if G.distance(hx, hz, px, pz) < self.portalRadius then self:levelComplete() end
    end
end

function Game:collect(item)
    local name = item:name()
    item:setActive(false)
    if name:find("^Gem") then
        self.gems = self.gems + 1
        agr.audio.play("assets://audio/coin.wav", { pitch = 1.3 })
        if self.gems >= 3 then self:openGate() else self:say(agr.tr("ambre.gem", self.gems), 1.5) end
    elseif name:find("^Heart") then
        self.hero:script():heal(2)
        agr.audio.play("assets://audio/coin.wav", { pitch = 0.9 })
    else
        self.coins = self.coins + 1
        agr.audio.play("assets://audio/coin.wav", { volume = 0.6 })
    end
    self:hud()
end

return Game
