-- Les Ruines d'Ambre (AGR ENGINE) : aventure-action 2.5D. Trois niveaux (ramasser les trois gemmes
-- d'ambre ouvre la grille de sortie, le portail mène plus loin), puis l'arène du boss. Menu titre,
-- pause (P ou bouton II), game over, victoire ; progression, pièces et meilleur temps sauvegardés.
local G = require("assets://games/scripts/common.lua")

local Game = { properties = { spacing = 70, levels = 4, pickupRadius = 1.0, portalRadius = 1.6 } }

local CONTROLS = { "Jump", "Attack", "Dash", "Stick", "PauseButton" }
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
    self.homes, self.itemHomes, self.gateHome = {}, {}, {}
    for k = 1, self.levels do
        self.gateHome[k] = { self:path(k, "Gate"):position() }
        self.homes[k] = homesOf(self:path(k, "Enemies"))
        self.itemHomes[k] = homesOf(self:path(k, "Items"))
    end
    self.hero = agr.find("Hero")
    self:decorate()
    self:showMenu("title")
    self:hud()
end

-- Animations d'ambiance : gemmes et cœurs qui flottent, pièces qui tournent, portails qui pulsent.
function Game:decorate()
    for k = 1, self.levels do
        local items = self:path(k, "Items")
        for i, item in ipairs(items and items:children() or {}) do
            local sprite = item:find("Sprite")
            if sprite then
                local name = item:name()
                local o = { loop = "yoyo", ease = "inOutSine", delay = (i % 5) * 0.13 }
                if name:find("^Coin") then
                    agr.tween(sprite, { scale = { 0.15, 1, 1 } }, 0.45, o)
                else
                    agr.tween(sprite, { position = { 0, 0.3, 0 } }, 0.9, o)
                end
            end
        end
        local portal = self:path(k, "Portal")
        if portal then agr.tween(portal, { scale = { 1.85, 1.85, 1.85 } }, 1.2, { loop = "yoyo", ease = "inOutSine" }) end
    end
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
    for _, name in ipairs(CONTROLS) do G.show(name, false) end
    local menu = agr.find("Menu")
    if menu then agr.tween(menu, { uiPosition = { 0, 0 } }, 0.5, { from = { 0, 90 }, ease = "outBack", unscaled = true }) end
end

function Game:hideMenu()
    G.show("Menu", false)
    for _, name in ipairs(CONTROLS) do G.show(name, true) end
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
    agr.time.setScale(1)
    agr.fx.fade(0, 0.3)
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
    if gate then
        agr.stopTweens(gate)
        gate:setActive(true)
        gate:setPosition(self.gateHome[k][1], self.gateHome[k][2], self.gateHome[k][3])
    end
    self.gateOpening = false
    self.hero:script():spawn((k - 1) * self.spacing - 13, 0.05, 0)
    G.show("BossBar", k == self.levels)
    self:bossHealth(1)
    self.state = "playing"
    self:hideMenu()
    agr.time.setScale(1)
    agr.fx.fade(0, 0.7, 0, 0, 0)
    for i = 1, 3 do self:gemSlot(i, false) end
    G.text("LevelText", agr.tr(NAMES[k]))
    self:say(k == self.levels and agr.tr("ambre.boss") or agr.tr("ambre.goal"), 3)
    self:hud()
end

-- La grille s'enfonce dans le sol (1,2 s) puis disparaît ; le portail devient franchissable.
function Game:openGate()
    local gate = self:path(self.level, "Gate")
    if gate and gate:isActive() and not self.gateOpening then
        self.gateOpening = true
        local x, y, z = gate:position()
        agr.audio.play("assets://audio/slam.wav", { volume = 0.5, pitch = 1.4 })
        agr.fx.shake(0.3, 1.2)
        local wx, _, wz = gate:worldPosition()
        agr.fx.burst(wx, 0.3, wz, { count = 40, color = { 0.8, 0.65, 0.45 }, speed = 3, life = 1.0, size = 0.3, spread = "up", gravity = -3, additive = false })
        agr.tween(gate, { position = { x, -1.3, z } }, 1.2, { ease = "inOutQuad", onDone = function()
            gate:setActive(false)
            gate:setPosition(x, y, z)
            self.gateOpening = false
        end })
        self:say(agr.tr("ambre.gateOpen"), 2.5)
    end
end

function Game:levelComplete()
    if self.level >= self.levels then
        self.state = "victory"
        agr.audio.play("assets://audio/portal.wav", { volume = 0.8, pitch = 1.2 })
        agr.fx.flash(1, 0.85, 0.5, 0.8, 1.2)
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
    agr.fx.fade(1, 1.2, 0.1, 0.06, 0.02)
    agr.audio.play("assets://audio/portal.wav", { volume = 0.8 })
    agr.save.set("ambre.level", self.level + 1)
    agr.save.set("ambre.coins", self.coins)
    agr.save.set("ambre.time", self.time)
    agr.audio.play("assets://audio/coin.wav", { pitch = 1.5 })
    self:say(agr.tr("ambre.cleared"), 1.5)
end

-- Mort du héros : ralenti et assombrissement, puis le menu de game over.
function Game:heroDied()
    self.state = "dying"
    self.dyingTime = 1.2
    agr.time.setScale(0.3)
    agr.fx.fade(0.6, 1.2, 0.25, 0, 0)
    local x, y, z = self.hero:position()
    agr.fx.burst(x, y + 0.8, z, { count = 40, color = { 1, 0.4, 0.3 }, speed = 5, life = 0.9, size = 0.3 })
end

function Game:gameOver()
    agr.time.setScale(1)
    self.state = "gameover"
    self.coins = self.levelCoins or self.coins
    self:clearMinions()
    G.show("BossBar", false)
    self:showMenu("gameover")
end

function Game:enemyDefeated(kind, x, y, z)
    if kind == "slime" or kind == "bat" then
        self.coins = self.coins + 1
        if x then agr.fx.popup(x, y + 2.2, z, "+1", { color = { 1, 0.85, 0.3 }, size = 46, duration = 0.8 }) end
        self:coinPop()
    end
    self:hud()
end

function Game:bossHealth(f)
    local fill = agr.find("BossFill")
    if fill then
        agr.stopTweens(fill)
        if f >= 1 then fill:set("UIRect", "size", { 900, 30 })
        else agr.tween(fill, { uiSize = { 900 * G.clamp(f, 0, 1), 30 } }, 0.25, { ease = "outQuad" }) end
    end
end

function Game:bossDefeated()
    for _, e in ipairs(self:path(self.levels, "Enemies"):children()) do
        if e:name() == "Minion" and e:script() and not e:script().dead then
            local x, y, z = e:worldPosition()
            e:script():die(x, y, z)
        end
    end
    G.show("BossBar", false)
    agr.fx.hitStop(0.5, 0.15)
    agr.fx.shake(1, 1.2)
    agr.fx.flash(1, 0.9, 0.6, 0.7, 0.8)
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

-- Cœurs : pleins / vides ; le cœur perdu ou regagné rebondit.
function Game:healthChanged(delta)
    local hs = self.hero:script()
    for i = 1, hs.maxHealth do
        local icon = agr.find("HeartIcon " .. i)
        if icon then
            local full = i <= hs.health
            icon:set("UIImage", "color", full and { 1, 1, 1, 1 } or { 0.25, 0.15, 0.15, 0.7 })
            local changed = (delta < 0 and i > hs.health and i <= hs.health - delta) or (delta > 0 and full and i > hs.health - delta)
            if changed then agr.tween(icon, { uiSize = { 66, 66 } }, 0.4, { from = { 100, 100 }, ease = "outBack", unscaled = true }) end
        end
    end
    self:hud()
end

function Game:gemSlot(i, lit)
    local icon = agr.find("GemIcon " .. i)
    if not icon then return end
    icon:set("UIImage", "color", lit and { 1, 1, 1, 1 } or { 0.25, 0.2, 0.15, 0.8 })
    if lit then agr.tween(icon, { uiSize = { 56, 56 } }, 0.5, { from = { 96, 96 }, ease = "outElastic" }) end
end

function Game:coinPop()
    local t = agr.find("CoinIcon")
    if t then agr.tween(t, { uiSize = { 50, 50 } }, 0.3, { from = { 70, 70 }, ease = "outBack" }) end
end

function Game:hud()
    local hs = self.hero and self.hero:script()
    if hs and hs.health then
        G.text("HealthText", agr.tr("ambre.health", math.max(0, hs.health), hs.maxHealth))
        for i = 1, hs.maxHealth do
            local icon = agr.find("HeartIcon " .. i)
            if icon then icon:set("UIImage", "color", i <= hs.health and { 1, 1, 1, 1 } or { 0.25, 0.15, 0.15, 0.7 }) end
        end
    end
    G.text("CoinsText", tostring(self.coins))
    G.text("GemsText", self.level < self.levels and agr.tr("ambre.gems", self.gems) or agr.tr("ambre.gems.boss"))
end

-- ---- frame
function Game:onUpdate(dt)
    self.messageTime = self.messageTime - dt
    if self.messageTime <= 0 then G.text("Message", "") end
    if agr.input.keyPressed("P") or agr.input.gamepadPressed("Start") then self:togglePause() end
    if self.state == "dying" then
        self.dyingTime = self.dyingTime - dt / 0.3  -- ralenti à 0,3 : temps réel
        if self.dyingTime <= 0 then self:gameOver() end
        return
    end
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
    local x, y, z = item:worldPosition()
    item:setActive(false)
    if name:find("^Gem") then
        self.gems = self.gems + 1
        agr.audio.play("assets://audio/gem.wav", { volume = 0.8 })
        agr.fx.burst(x, y + 0.4, z, { count = 30, color = { 1, 0.8, 0.35 }, speed = 4, life = 0.8, size = 0.35, spread = "up",
                                      gravity = -2, texture = "assets://games/sparkle.png" })
        agr.fx.popup(x, y + 1.4, z, agr.tr("ambre.gem", self.gems), { color = { 1, 0.8, 0.35 }, size = 56, duration = 1.1 })
        self:gemSlot(self.gems, true)
        if self.gems >= 3 then self:openGate() end
    elseif name:find("^Heart") then
        agr.audio.play("assets://audio/gem.wav", { volume = 0.6, pitch = 0.8 })
        agr.fx.burst(x, y + 0.4, z, { count = 20, color = { 1, 0.4, 0.5 }, speed = 3, life = 0.6, spread = "up", gravity = -1 })
        agr.fx.popup(x, y + 1.4, z, "+2", { color = { 1, 0.5, 0.55 }, size = 52 })
        self.hero:script():heal(2)
    else
        self.coins = self.coins + 1
        agr.audio.play("assets://audio/coin.wav", { volume = 0.5, pitch = 0.95 + math.random() * 0.1 })
        agr.fx.burst(x, y + 0.4, z, { count = 10, color = { 1, 0.85, 0.3 }, speed = 3, life = 0.4, size = 0.18, spread = "up" })
        self:coinPop()
    end
    self:hud()
end

return Game
