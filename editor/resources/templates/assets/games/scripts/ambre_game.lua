-- Les Ruines d'Ambre (AGR ENGINE) : aventure-action 2.5D complète.
--   6 salles : tutoriel, pièges à pics, gouffre à sauter, énigme des dalles, épreuve, boss.
--   Dans chaque salle : 3 gemmes d'ambre ouvrent la grille, le portail mène plus loin.
--   3 étoiles par salle : terminée, toutes les pièces au sol, en moins du temps de référence.
--   Boutique entre les salles (cœurs, épée, roulade, potion de garde), pièces gardées d'une partie
--   à l'autre ; choix des salles débloquées ; options (musique, effets, langue) ; histoire.
-- Sauvegarde : agr.save « ambre » (table).
local G = require("assets://games/scripts/common.lua")

local Game = { properties = { spacing = 70, levels = 6, pickupRadius = 1.0, portalRadius = 1.6 } }

local CONTROLS = { "Jump", "Attack", "Dash", "Stick", "PauseButton" }
local PAR = { 60, 70, 75, 90, 100, 150 }  -- secondes : temps de référence (3e étoile)
local MENU_BUTTONS = { "PlayButton", "LevelsButton", "OptionsButton", "ResumeButton", "RestartButton", "RetryButton", "NextButton",
                       "MenuButton", "BackButton", "MusicButton", "SfxButton", "LanguageButton" }
local UPGRADES = {
    heart = { costs = { 10, 20, 35 } },  -- +1 cœur max par niveau (5 -> 8)
    sword = { costs = { 15, 30 } },      -- +1 dégât par niveau (1 -> 3)
    dash = { costs = { 10, 20 } },       -- recharge de la roulade 0,7 -> 0,5 -> 0,35 s
}
local DASH = { 0.7, 0.5, 0.35 }
local POTION_COST = 8
local VOLUMES = { 1, 0.75, 0.5, 0.25, 0 }
local LANGS = { "fr", "en", "es" }

-- Positions de départ (locales) des enfants d'une entité, par nom.
local function homesOf(parent)
    local t = {}
    for _, e in ipairs(parent and parent:children() or {}) do t[e:name()] = { e:position() } end
    return t
end

local function stars(mask)
    local s = ""
    for i = 0, 2 do s = s .. ((mask & (1 << i)) ~= 0 and "★" or "☆") end
    return s
end

local function starCount(mask)
    local n = 0
    for i = 0, 2 do if (mask & (1 << i)) ~= 0 then n = n + 1 end end
    return n
end

function Game:onStart()
    self.state, self.level, self.gems, self.time = "menu", 1, 0, 0
    self.messageTime, self.nextTime = 0, 0
    self.arrows, self.arrowSeq = {}, 0
    self.homes, self.itemHomes, self.propHomes, self.gateHome = {}, {}, {}, {}
    for k = 1, self.levels do
        self.homes[k] = homesOf(self:path(k, "Enemies"))
        self.itemHomes[k] = homesOf(self:path(k, "Items"))
        self.propHomes[k] = homesOf(self:path(k, "Props"))
        self.gateHome[k] = { self:path(k, "Gate"):position() }
    end
    self.hero = agr.find("Hero")
    self:load()
    self:applySettings()
    self:decorate()
    self:showMenu("title")
    self.hero:script():applyUpgrades(5 + self.data.upgrades.heart, 1, DASH[1], false)
    self.hero:script().health = self.hero:script().maxHealth
    self:healthChanged(0)
end

-- ---- sauvegarde
function Game:load()
    local s = agr.save.get("ambre", {})
    self.data = {
        unlocked = G.clamp(s.unlocked or 1, 1, self.levels),
        bank = s.bank or 0,
        upgrades = s.upgrades or { heart = 0, sword = 0, dash = 0 },
        stars = s.stars or {},
        best = s.best or {},
        bestRun = s.bestRun,
        seenIntro = s.seenIntro or false,
        finished = s.finished or false,
        music = s.music or 1,
        sfx = s.sfx or 1,
    }
    for _, key in ipairs({ "heart", "sword", "dash" }) do self.data.upgrades[key] = self.data.upgrades[key] or 0 end
end

function Game:save()
    agr.save.set("ambre", self.data)
end

function Game:starsOf(k) return self.data.stars[k] or self.data.stars[tostring(k)] or 0 end
function Game:setStars(k, mask)
    self.data.stars[tostring(k)] = mask
    self.data.stars[k] = nil
end

function Game:totalStars()
    local n = 0
    for k = 1, self.levels do n = n + starCount(self:starsOf(k)) end
    return n
end

-- ---- options
function Game:applySettings()
    agr.audio.setBusVolume("Music", self.data.music)
    agr.audio.setBusVolume("Effects", self.data.sfx)
end

-- Entité d'une zone : self:path(2, "Items") -> l'entité « Items » de « Area 2 ».
function Game:path(k, child)
    local area = agr.find("Area " .. k)
    return area and area:find(child)
end

function Game:enemies()
    local list = {}
    local group = self:path(self.level, "Enemies")
    for _, e in ipairs(group and group:children() or {}) do
        if e:isActive() and e:script() then list[#list + 1] = e end
    end
    return list
end

function Game:breakables()
    local list = {}
    local group = self:path(self.level, "Props")
    for _, e in ipairs(group and group:children() or {}) do
        local s = e:isActive() and e:script()
        if s and s.kind == "pot" and not s.broken then list[#list + 1] = e end
    end
    return list
end

function Game:registerArrow(e)
    self.arrowSeq = self.arrowSeq + 1
    self.arrows[self.arrowSeq] = e
    return self.arrowSeq
end

-- Animations d'ambiance : gemmes et cœurs qui flottent, pièces qui tournent, portails qui pulsent.
function Game:decorate()
    for k = 1, self.levels do
        local items = self:path(k, "Items")
        for i, item in ipairs(items and items:children() or {}) do
            local sprite = item:find("Sprite")
            if sprite then
                local o = { loop = "yoyo", ease = "inOutSine", delay = (i % 5) * 0.13 }
                if item:name():find("^Coin") then agr.tween(sprite, { scale = { 0.15, 1, 1 } }, 0.45, o)
                else agr.tween(sprite, { position = { 0, 0.3, 0 } }, 0.9, o) end
            end
        end
        local portal = self:path(k, "Portal")
        if portal then agr.tween(portal, { scale = { 1.85, 1.85, 1.85 } }, 1.2, { loop = "yoyo", ease = "inOutSine" }) end
    end
end

-- ---- menus
function Game:place(list, top)
    for _, name in ipairs(MENU_BUTTONS) do G.show(name, false) end
    for i = 1, self.levels do G.show("LevelButton " .. i, false) end
    for i, name in ipairs(list) do
        local b = agr.find(name)
        if b then
            b:setActive(true)
            b:set("UIRect", "position", { 0, (top or 400) + (i - 1) * 100 })
        end
    end
end

function Game:label(button, text)
    local b = agr.find(button)
    local l = b and b:find("Label")
    if l then l:setText(text) end
end

function Game:showMenu(page, from)
    self.page = page
    self.menuFrom = from or self.menuFrom
    G.show("Menu", true)
    G.show("Shop", false)
    for _, name in ipairs(CONTROLS) do G.show(name, false) end
    G.show("HintPanel", false)
    local title, text = agr.tr("ambre.title"), ""
    if page == "title" then
        text = agr.tr("ambre.tagline") .. "\n\n" .. agr.tr("ambre.progress", self:totalStars(), self.levels * 3, self.data.bank)
        if self.data.bestRun then text = text .. "\n" .. agr.tr("ambre.best", self:clock(self.data.bestRun)) end
        self:label("PlayButton", self.data.unlocked > 1 and agr.tr("ambre.continueAt", self.data.unlocked) or agr.tr("ambre.play"))
        self:place({ "PlayButton", "LevelsButton", "OptionsButton" }, 430)
    elseif page == "story" then
        title = agr.tr(self.storyKey .. ".title")
        text = agr.tr(self.storyKey)
        self:place({ "NextButton" }, 640)
    elseif page == "levels" then
        title = agr.tr("ambre.levels")
        text = agr.tr("ambre.levels.help")
        self:place({ "BackButton" }, 730)
        for k = 1, self.levels do
            local b = agr.find("LevelButton " .. k)
            local open = k <= self.data.unlocked
            b:setActive(true)
            b:set("UIRect", "position", { (k % 2 == 1) and -235 or 235, 280 + math.floor((k - 1) / 2) * 140 })
            b:set("UIButton", "interactable", open)
            self:label("LevelButton " .. k, open and (k .. ". " .. agr.tr("ambre.area" .. k .. ".short") .. "\n" .. stars(self:starsOf(k)))
                                                  or (k .. ". " .. agr.tr("ambre.locked")))
        end
    elseif page == "options" then
        title = agr.tr("ambre.options")
        self:label("MusicButton", agr.tr("ambre.music", math.floor(self.data.music * 100 + 0.5)))
        self:label("SfxButton", agr.tr("ambre.sfx", math.floor(self.data.sfx * 100 + 0.5)))
        self:label("LanguageButton", agr.tr("ambre.language"))
        self:place({ "MusicButton", "SfxButton", "LanguageButton", "BackButton" }, 330)
    elseif page == "pause" then
        title = agr.tr("ambre.paused")
        text = agr.tr("ambre.area" .. self.level) .. "\n" .. agr.tr("ambre.pauseInfo", self.gems, self:clock(self.time))
        self:place({ "ResumeButton", "RestartButton", "OptionsButton", "MenuButton" }, 330)
    elseif page == "gameover" then
        title = agr.tr("ambre.gameover.title")
        text = agr.tr("ambre.gameover", agr.tr("ambre.area" .. self.level))
        self:place({ "RetryButton", "MenuButton" }, 430)
    elseif page == "victory" then
        title = agr.tr("ambre.victory.title")
        text = agr.tr("ambre.ending") .. "\n\n" .. agr.tr("ambre.victory", self:clock(self.runTime or self.time), self:totalStars(), self.levels * 3)
        if self.newRecord then text = text .. "\n" .. agr.tr("ambre.record") end
        self:place({ "LevelsButton", "MenuButton" }, 560)
    end
    G.text("MenuTitle", title)
    G.text("MenuText", text)
    local menu = agr.find("Menu")
    agr.tween(menu, { uiPosition = { 0, 0 } }, 0.45, { from = { 0, 70 }, ease = "outBack", unscaled = true })
end

function Game:hideMenu()
    G.show("Menu", false)
    G.show("Shop", false)
    for _, name in ipairs(CONTROLS) do G.show(name, true) end
end

-- Tous les boutons des menus et de la boutique envoient « pressed ».
function Game:pressed(button)
    local name = button:name()
    agr.audio.play("assets://audio/click.wav", { volume = 0.6 })
    if name == "PlayButton" then
        if not self.data.seenIntro then
            self.storyKey, self.afterStory = "ambre.intro", function() self:newRun(1) end
            self.data.seenIntro = true
            self:save()
            self:showMenu("story")
        else
            self:newRun(self.data.unlocked)
        end
    elseif name == "LevelsButton" then self:showMenu("levels", self.page == "victory" and "title" or self.page)
    elseif name == "OptionsButton" then self:showMenu("options", self.page)
    elseif name == "BackButton" then self:showMenu(self.menuFrom == "pause" and "pause" or "title")
    elseif name == "NextButton" then
        local after = self.afterStory
        self.afterStory = nil
        if after then after() else self:showMenu("title") end
    elseif name == "ResumeButton" then self:togglePause()
    elseif name == "RestartButton" or name == "RetryButton" then self:startLevel(self.level)
    elseif name == "MenuButton" then self:toMenu()
    elseif name == "MusicButton" or name == "SfxButton" then
        local key = name == "MusicButton" and "music" or "sfx"
        local i = 1
        for j, v in ipairs(VOLUMES) do if math.abs(v - self.data[key]) < 0.01 then i = j end end
        self.data[key] = VOLUMES[i % #VOLUMES + 1]
        self:applySettings()
        self:save()
        self:showMenu("options")
    elseif name == "LanguageButton" then
        local current = agr.locale.get()
        local i = 1
        for j, l in ipairs(LANGS) do if l == current then i = j end end
        agr.locale.set(LANGS[i % #LANGS + 1])
        self:showMenu("options")
        self:hud()
    elseif name:find("^LevelButton") then
        local k = tonumber(name:match("%d+"))
        if k and k <= self.data.unlocked then self:newRun(k) end
    elseif name:find("^Buy") then self:buy(name)
    elseif name == "ShopContinue" then self:startLevel(self.level + 1)
    end
end

function Game:newRun(k)
    self.runTime = 0
    self:startLevel(k)
end

function Game:togglePause()
    if self.state == "playing" then
        self.state = "paused"
        self:showMenu("pause", "pause")
    elseif self.state == "paused" then
        self.state = "playing"
        self:hideMenu()
    end
end

function Game:toMenu()
    agr.time.setScale(1)
    agr.fx.fade(0, 0.3)
    self.state = "menu"
    self:clearTransients()
    G.show("BossBar", false)
    self.menuFrom = "title"
    self:showMenu("title")
end

-- ---- salles
function Game:clearTransients()
    local group = self:path(self.levels, "Enemies")
    for _, e in ipairs(group and group:children() or {}) do
        if e:name() == "Minion" then e:destroy()
        elseif e:name():find("^Boulder") then e:setActive(false) end
    end
    for id, a in pairs(self.arrows) do
        if a:isValid() then a:destroy() end
        self.arrows[id] = nil
    end
end

function Game:startLevel(k)
    self.level, self.gems, self.time = k, 0, 0
    self.coinItems, self.coinItemsTotal = 0, 0
    self.plates = { false, true, true, false }
    self.hints = {}
    local items = self:path(k, "Items")
    for _, e in ipairs(items and items:children() or {}) do
        G.respawn(e, self.itemHomes[k][e:name()])
        if e:name():find("^Coin") then self.coinItemsTotal = self.coinItemsTotal + 1 end
    end
    local props = self:path(k, "Props")
    for _, e in ipairs(props and props:children() or {}) do G.respawn(e, self.propHomes[k][e:name()]) end
    self:clearTransients()
    local foes = self:path(k, "Enemies")
    for _, e in ipairs(foes and foes:children() or {}) do
        if not e:name():find("^Boulder") then G.respawn(e, self.homes[k][e:name()]) end
    end
    local gate = self:path(k, "Gate")
    agr.stopTweens(gate)
    gate:setActive(true)
    gate:setPosition(self.gateHome[k][1], self.gateHome[k][2], self.gateHome[k][3])
    self.gateOpening = false
    if self:isPuzzle(k) then  -- la 3e gemme de la Forge n'apparaît qu'une fois l'énigme résolue
        local gem = items:find("Gem 3")
        if gem then gem:setActive(false) end
        self:updatePlates()
    end
    local hs = self.hero:script()
    local u = self.data.upgrades
    hs:applyUpgrades(5 + u.heart, 1 + u.sword, DASH[u.dash + 1], self.guardNext or false)
    self.guardNext = false
    hs:spawn((k - 1) * self.spacing - 13, 0.05, 0)
    G.show("BossBar", k == self.levels)
    self:bossHealth(1)
    self.state = "playing"
    self:hideMenu()
    agr.time.setScale(1)
    agr.fx.fade(0, 0.7, 0, 0, 0)
    for i = 1, 3 do self:gemSlot(i, false) end
    G.text("LevelText", agr.tr("ambre.area" .. k))
    self:say(k == self.levels and agr.tr("ambre.boss") or agr.tr("ambre.goal"), 3)
    self:healthChanged(0)
end

function Game:isPuzzle(k) return self:path(k, "Props"):find("Plate 1") ~= nil end

-- Énigme des dalles (2 x 2) : marcher sur une dalle inverse elle et ses deux voisines.
local NEIGHBOURS = { { 1, 2, 3 }, { 2, 1, 4 }, { 3, 1, 4 }, { 4, 2, 3 } }
function Game:pressPlate(i)
    if self.puzzleDone or not self.plates then return end
    for _, j in ipairs(NEIGHBOURS[i]) do self.plates[j] = not self.plates[j] end
    agr.audio.play("assets://audio/click.wav", { volume = 0.7, pitch = 0.6 })
    self:updatePlates()
    local all = self.plates[1] and self.plates[2] and self.plates[3] and self.plates[4]
    if all then
        local gem = self:path(self.level, "Items"):find("Gem 3")
        if gem and not gem:isActive() and self.gems < 3 then
            gem:setActive(true)
            local x, y, z = gem:worldPosition()
            agr.fx.burst(x, y + 0.5, z, { count = 50, color = { 1, 0.75, 0.3 }, speed = 5, life = 1, spread = "up", texture = "assets://games/sparkle.png" })
            agr.audio.play("assets://audio/portal.wav", { volume = 0.7, pitch = 1.4 })
            self:say(agr.tr("ambre.puzzleDone"), 2.5)
        end
    end
end

function Game:updatePlates()
    for i = 1, 4 do
        local p = self:path(self.level, "Props"):find("Plate " .. i)
        local s = p and p:script()
        if s then s:setLit(self.plates[i]) end
    end
end

-- Panneaux : un conseil à la fois en bas de l'écran.
function Game:showHint(key)
    self.hintKey = key
    G.show("HintPanel", true)
    G.text("Hint", agr.tr(key))
end

function Game:hideHint(key)
    if self.hintKey == key then
        self.hintKey = nil
        G.show("HintPanel", false)
    end
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

-- Étoiles : salle finie ; toutes les pièces au sol ; temps de référence battu.
function Game:levelComplete()
    local k = self.level
    local mask = 1
    if self.coinItems >= self.coinItemsTotal then mask = mask | 2 end
    if self.time <= PAR[k] then mask = mask | 4 end
    local before = self:starsOf(k)
    self:setStars(k, before | mask)
    self.lastMask, self.lastNew = mask, (before | mask) ~= before
    local bestTime = self.data.best[tostring(k)]
    if not bestTime or self.time < bestTime then self.data.best[tostring(k)] = self.time end
    self.runTime = (self.runTime or 0) + self.time
    self:clearTransients()
    if k >= self.levels then
        self.state = "victory"
        self.data.finished = true
        self.newRecord = not self.data.bestRun or self.runTime < self.data.bestRun
        if self.newRecord then self.data.bestRun = self.runTime end
        self:save()
        agr.audio.play("assets://audio/portal.wav", { volume = 0.8, pitch = 1.2 })
        agr.fx.flash(1, 0.85, 0.5, 0.8, 1.2)
        G.show("BossBar", false)
        self:showMenu("victory")
        return
    end
    self.data.unlocked = math.max(self.data.unlocked, k + 1)
    self:save()
    self.state = "levelcomplete"
    self.nextTime = 1.3
    agr.fx.fade(1, 1.1, 0.1, 0.06, 0.02)
    agr.audio.play("assets://audio/portal.wav", { volume = 0.8 })
    self:say(agr.tr("ambre.cleared"), 1.3)
end

-- ---- boutique
function Game:price(key)
    local level = self.data.upgrades[key]
    return UPGRADES[key].costs[level + 1]
end

function Game:openShop()
    self.state = "shop"
    agr.fx.fade(0, 0.4)
    G.show("Menu", false)
    G.show("Shop", true)
    for _, name in ipairs(CONTROLS) do G.show(name, false) end
    G.show("HintPanel", false)
    G.text("ShopTitle", agr.tr("ambre.shop.title", agr.tr("ambre.area" .. self.level .. ".short")))
    local results = agr.tr("ambre.results", self:clock(self.time), self.coinItems, self.coinItemsTotal, stars(self.lastMask))
    if self.lastNew then results = results .. "\n" .. agr.tr("ambre.newStars") end
    G.text("ShopResults", results)
    self:refreshShop()
    agr.tween(agr.find("Shop"), { uiPosition = { 0, 0 } }, 0.45, { from = { 0, 70 }, ease = "outBack", unscaled = true })
end

function Game:refreshShop()
    local u = self.data.upgrades
    G.text("ShopCoins", agr.tr("ambre.bank", self.data.bank))
    local function ware(button, key, label, value, nextValue)
        local cost = self:price(key)
        local b = agr.find(button)
        if cost then
            self:label(button, agr.tr(label, value, nextValue) .. "\n" .. agr.tr("ambre.cost", cost))
            b:set("UIButton", "interactable", self.data.bank >= cost)
        else
            self:label(button, agr.tr(label, value, value) .. "\n" .. agr.tr("ambre.max"))
            b:set("UIButton", "interactable", false)
        end
    end
    ware("BuyHeart", "heart", "ambre.buy.heart", 5 + u.heart, 6 + u.heart)
    ware("BuySword", "sword", "ambre.buy.sword", 1 + u.sword, 2 + u.sword)
    ware("BuyDash", "dash", "ambre.buy.dash", DASH[u.dash + 1], DASH[math.min(u.dash + 2, #DASH)])
    self:label("BuyPotion", agr.tr("ambre.buy.potion") .. "\n" .. (self.guardNext and agr.tr("ambre.owned") or agr.tr("ambre.cost", POTION_COST)))
    agr.find("BuyPotion"):set("UIButton", "interactable", not self.guardNext and self.data.bank >= POTION_COST)
end

function Game:buy(name)
    local key = ({ BuyHeart = "heart", BuySword = "sword", BuyDash = "dash" })[name]
    local cost = key and self:price(key) or POTION_COST
    if not cost or self.data.bank < cost or (name == "BuyPotion" and self.guardNext) then return end
    self.data.bank = self.data.bank - cost
    if key then self.data.upgrades[key] = self.data.upgrades[key] + 1 else self.guardNext = true end
    self:save()
    agr.audio.play("assets://audio/gem.wav", { volume = 0.7 })
    local b = agr.find(name)
    if b then agr.tween(b, { uiSize = { 540, 130 } }, 0.35, { from = { 600, 150 }, ease = "outBack", unscaled = true }) end
    self:refreshShop()
    self:hud()
end

-- ---- vie et mort
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
    self:clearTransients()
    G.show("BossBar", false)
    self:showMenu("gameover")
end

function Game:addCoins(n, x, y, z)
    self.data.bank = self.data.bank + n
    if x then agr.fx.popup(x, y + 1.2, z, "+" .. n, { color = { 1, 0.85, 0.3 }, size = 46, duration = 0.8 }) end
    self:coinPop()
    self:hud()
end

function Game:enemyDefeated(kind, x, y, z)
    local reward = ({ slime = 1, bat = 1, archer = 2, brute = 3 })[kind]
    if reward then self:addCoins(reward, x, y + 1, z) end
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
        local s = e:name() == "Minion" and e:script()
        if s and not s.dead then
            local x, y, z = e:worldPosition()
            s:die(x, y, z)
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
    for i = 1, 8 do
        local icon = agr.find("HeartIcon " .. i)
        if icon then
            icon:setActive(i <= hs.maxHealth)
            local full = i <= hs.health
            icon:set("UIImage", "color", full and { 1, 1, 1, 1 } or { 0.25, 0.15, 0.15, 0.7 })
            local changed = (delta < 0 and i > hs.health and i <= hs.health - delta) or (delta > 0 and full and i > hs.health - delta)
            if changed then agr.tween(icon, { uiSize = { 58, 58 } }, 0.4, { from = { 90, 90 }, ease = "outBack", unscaled = true }) end
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
        local text = agr.tr("ambre.health", math.max(0, hs.health), hs.maxHealth)
        if hs.guard then text = text .. " +" .. agr.tr("ambre.guard") end
        G.text("HealthText", text)
    end
    G.text("CoinsText", tostring(self.data and self.data.bank or 0))
    G.text("GemsText", self.level < self.levels and agr.tr("ambre.gems", self.gems) or agr.tr("ambre.gems.boss"))
end

-- ---- image
function Game:onUpdate(dt)
    self.messageTime = self.messageTime - dt
    if self.messageTime <= 0 then G.text("Message", "") end
    if agr.input.keyPressed("P") or agr.input.keyPressed("Escape") or agr.input.gamepadPressed("Start") then self:togglePause() end
    if self.state == "dying" then
        self.dyingTime = self.dyingTime - dt / 0.3  -- ralenti à 0,3 : temps réel
        if self.dyingTime <= 0 then self:gameOver() end
        return
    end
    if self.state == "levelcomplete" then
        self.nextTime = self.nextTime - dt
        if self.nextTime <= 0 then self:openShop() end
        return
    end
    if self.state ~= "playing" then return end
    self.time = self.time + dt
    G.text("TimeText", self:clock(self.time))
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
        self.coinItems = self.coinItems + 1
        agr.audio.play("assets://audio/coin.wav", { volume = 0.5, pitch = 0.95 + math.random() * 0.1 })
        agr.fx.burst(x, y + 0.4, z, { count = 10, color = { 1, 0.85, 0.3 }, speed = 3, life = 0.4, size = 0.18, spread = "up" })
        self:addCoins(1)
        if self.coinItems == self.coinItemsTotal then self:say(agr.tr("ambre.allCoins"), 1.5) end
    end
    self:hud()
end

return Game
