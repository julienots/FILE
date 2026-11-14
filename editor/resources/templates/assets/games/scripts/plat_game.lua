-- Robo et les Cristaux (AGR ENGINE) : jeu de plateforme 2D complet.
--   5 niveaux (prairie, collines, grotte, lac, château) empilés dans la scène.
--   3 cœurs par essai ; chute ou coup = un cœur ; plus de cœurs = game over (réessayer le niveau).
--   3 cristaux cachés par niveau (les étoiles), pièces, point de passage, drapeau d'arrivée.
--   Menu, choix des niveaux, options (musique, effets, langue), pause, résultats, fin.
-- Sauvegarde : agr.save « plat » (table).
local G = require("assets://games/scripts/common.lua")

local Game = { properties = { levels = 5, levelStep = 40, width = 110, height = 18, maxHearts = 3 } }

local CONTROLS = { "Left", "Right", "Jump", "PauseButton" }
local MENU_BUTTONS = { "PlayButton", "LevelsButton", "OptionsButton", "ResumeButton", "RestartButton", "RetryButton", "NextButton",
                       "MenuButton", "BackButton", "MusicButton", "SfxButton", "LanguageButton" }
local VOLUMES = { 1, 0.75, 0.5, 0.25, 0 }
local LANGS = { "fr", "en", "es" }
-- Ambiance de chaque niveau : couleur du fond, teinte des décors (ciel, montagnes, collines).
local THEMES = {
    { clear = { 0.36, 0.62, 0.86, 1 }, tint = { { 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 } } },
    { clear = { 0.5, 0.7, 0.9, 1 }, tint = { { 1, 0.95, 0.85, 1 }, { 0.95, 0.9, 0.85, 1 }, { 1, 1, 0.8, 1 } } },
    { clear = { 0.08, 0.07, 0.12, 1 }, tint = { { 0.18, 0.16, 0.28, 1 }, { 0.22, 0.2, 0.3, 1 }, { 0.2, 0.22, 0.3, 1 } } },
    { clear = { 0.45, 0.7, 0.85, 1 }, tint = { { 0.85, 0.95, 1, 1 }, { 0.75, 0.9, 1, 1 }, { 0.7, 0.95, 0.9, 1 } } },
    { clear = { 0.35, 0.2, 0.25, 1 }, tint = { { 1, 0.6, 0.5, 1 }, { 0.6, 0.4, 0.45, 1 }, { 0.5, 0.35, 0.4, 1 } } },
}

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

local function count(mask)
    local n = 0
    for i = 0, 2 do if (mask & (1 << i)) ~= 0 then n = n + 1 end end
    return n
end

function Game:onStart()
    self.state, self.level, self.time, self.messageTime = "menu", 1, 0, 0
    self.pending, self.bullets = {}, {}
    self.homes = {}
    for k = 1, self.levels do
        self.homes[k] = {}
        for _, group in ipairs({ "Items", "Enemies", "Props" }) do self.homes[k][group] = homesOf(self:path(k, group)) end
    end
    self.hero = agr.find("Hero")
    self.camera = agr.find("Main Camera")
    self:load()
    self:applySettings()
    self:theme(1)
    self:showMenu("title")
    self.hearts = self.maxHearts
    self:hud()
end

function Game:load()
    local s = agr.save.get("plat", {})
    self.data = { unlocked = G.clamp(s.unlocked or 1, 1, self.levels), crystals = s.crystals or {}, best = s.best or {},
                  bank = s.bank or 0, seenIntro = s.seenIntro or false, finished = s.finished or false,
                  music = s.music or 1, sfx = s.sfx or 1 }
end

function Game:save() agr.save.set("plat", self.data) end
function Game:crystalsOf(k) return self.data.crystals[tostring(k)] or 0 end

function Game:totalCrystals()
    local n = 0
    for k = 1, self.levels do n = n + count(self:crystalsOf(k)) end
    return n
end

function Game:applySettings()
    agr.audio.setBusVolume("Music", self.data.music)
    agr.audio.setBusVolume("Effects", self.data.sfx)
end

function Game:path(k, child)
    local level = agr.find("Level " .. k)
    return level and level:find(child)
end

function Game:origin(k) return -10, -5 - (k - 1) * self.levelStep end
function Game:killY() local _, oy = self:origin(self.level); return oy - 1.5 end

function Game:theme(k)
    local t = THEMES[k]
    self.camera:set("Camera", "clearColor", t.clear)
    for i, name in ipairs({ "Sky", "Mountains", "Hills" }) do
        local e = agr.find(name)
        if e then e:set("SpriteRenderer", "color", t.tint[i]) end
    end
end

-- ---- menus (comme Les Ruines d'Ambre)
function Game:place(list, top)
    for _, name in ipairs(MENU_BUTTONS) do G.show(name, false) end
    for i = 1, self.levels do G.show("LevelButton " .. i, false) end
    for i, name in ipairs(list) do
        local b = agr.find(name)
        b:setActive(true)
        b:set("UIRect", "position", { 0, (top or 400) + (i - 1) * 100 })
    end
end

function Game:label(button, text)
    local l = agr.find(button):find("Label")
    if l then l:setText(text) end
end

function Game:showMenu(page, from)
    self.page = page
    self.menuFrom = from or self.menuFrom
    G.show("Menu", true)
    G.show("HintPanel", false)
    for _, name in ipairs(CONTROLS) do G.show(name, false) end
    local title, text = agr.tr("plat.title"), ""
    if page == "title" then
        text = agr.tr("plat.tagline") .. "\n\n" .. agr.tr("plat.progress", self:totalCrystals(), self.levels * 3, self.data.bank)
        self:label("PlayButton", self.data.unlocked > 1 and agr.tr("plat.continueAt", self.data.unlocked) or agr.tr("plat.play"))
        self:place({ "PlayButton", "LevelsButton", "OptionsButton" }, 430)
    elseif page == "story" then
        title, text = agr.tr("plat.intro.title"), agr.tr("plat.intro")
        self:place({ "NextButton" }, 640)
    elseif page == "levels" then
        title, text = agr.tr("plat.levels"), agr.tr("plat.levels.help")
        self:place({ "BackButton" }, 730)
        for k = 1, self.levels do
            local b = agr.find("LevelButton " .. k)
            local open = k <= self.data.unlocked
            b:setActive(true)
            b:set("UIRect", "position", { (k % 2 == 1 and k < self.levels) and -235 or (k == self.levels and 0 or 235), 290 + math.floor((k - 1) / 2) * 135 })
            b:set("UIButton", "interactable", open)
            self:label("LevelButton " .. k, open and (k .. ". " .. agr.tr("plat.level" .. k) .. "\n" .. stars(self:crystalsOf(k)))
                                                 or (k .. ". " .. agr.tr("plat.locked")))
        end
    elseif page == "options" then
        title = agr.tr("plat.options")
        self:label("MusicButton", agr.tr("plat.music", math.floor(self.data.music * 100 + 0.5)))
        self:label("SfxButton", agr.tr("plat.sfx", math.floor(self.data.sfx * 100 + 0.5)))
        self:label("LanguageButton", agr.tr("plat.language"))
        self:place({ "MusicButton", "SfxButton", "LanguageButton", "BackButton" }, 330)
    elseif page == "pause" then
        title = agr.tr("plat.paused")
        text = agr.tr("plat.level" .. self.level) .. "\n" .. agr.tr("plat.pauseInfo", count(self.found), self.coins, self:clock(self.time))
        self:place({ "ResumeButton", "RestartButton", "OptionsButton", "MenuButton" }, 330)
    elseif page == "gameover" then
        title, text = agr.tr("plat.gameover.title"), agr.tr("plat.gameover")
        self:place({ "RetryButton", "MenuButton" }, 430)
    elseif page == "results" then
        title = agr.tr("plat.cleared", agr.tr("plat.level" .. self.level))
        text = agr.tr("plat.results", self:clock(self.time), self.coins, self.coinsTotal, stars(self.found))
        if self.newBest then text = text .. "\n" .. agr.tr("plat.newCrystals") end
        self:place({ "NextButton", "RestartButton", "MenuButton" }, 430)
    elseif page == "victory" then
        title = agr.tr("plat.victory.title")
        text = agr.tr("plat.ending") .. "\n\n" .. agr.tr("plat.progress", self:totalCrystals(), self.levels * 3, self.data.bank)
        self:place({ "LevelsButton", "MenuButton" }, 560)
    end
    G.text("MenuTitle", title)
    G.text("MenuText", text)
    agr.tween(agr.find("Menu"), { uiPosition = { 0, 0 } }, 0.45, { from = { 0, 70 }, ease = "outBack", unscaled = true })
end

function Game:hideMenu()
    G.show("Menu", false)
    for _, name in ipairs(CONTROLS) do G.show(name, true) end
end

function Game:pressed(button)
    local name = button:name()
    agr.audio.play("assets://audio/click.wav", { volume = 0.6 })
    if name == "PlayButton" then
        if not self.data.seenIntro then
            self.data.seenIntro = true
            self:save()
            self.afterStory = function() self:startLevel(1) end
            self:showMenu("story")
        else
            self:startLevel(self.data.unlocked)
        end
    elseif name == "LevelsButton" then self:showMenu("levels", self.page == "victory" and "title" or self.page)
    elseif name == "OptionsButton" then self:showMenu("options", self.page)
    elseif name == "BackButton" then self:showMenu(self.menuFrom == "pause" and "pause" or "title")
    elseif name == "NextButton" then
        if self.page == "results" then self:startLevel(self.level + 1)
        else
            local after = self.afterStory
            self.afterStory = nil
            if after then after() else self:showMenu("title") end
        end
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
        local current, i = agr.locale.get(), 1
        for j, l in ipairs(LANGS) do if l == current then i = j end end
        agr.locale.set(LANGS[i % #LANGS + 1])
        self:showMenu("options")
        self:hud()
    elseif name:find("^LevelButton") then
        local k = tonumber(name:match("%d+"))
        if k and k <= self.data.unlocked then self:startLevel(k) end
    end
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
    self:clearBullets()
    self.menuFrom = "title"
    self:showMenu("title")
end

-- ---- niveaux
function Game:clearBullets()
    for _, e in ipairs(self.bullets or {}) do if e:isValid() then e:destroy() end end
    self.bullets = {}
end

function Game:startLevel(k)
    self.level, self.time, self.coins, self.found = k, 0, 0, 0
    self.hearts = self.maxHearts
    self.coinsTotal = 0
    self.pending = {}
    for _, group in ipairs({ "Items", "Enemies", "Props" }) do
        for _, e in ipairs(self:path(k, group):children()) do
            G.respawn(e, self.homes[k][group][e:name()])
            if group == "Items" and e:name():find("^Coin") then self.coinsTotal = self.coinsTotal + 1 end
        end
    end
    local start = self:path(k, "Start")
    local x, y = start:position()
    self.spawnX, self.spawnY = x, y
    self.hero:script():spawn(x, y)
    local ox, oy = self:origin(k)
    local cam = self.camera:script()
    cam.minX, cam.maxX = ox + 9.8, ox + self.width - 9.8
    cam.minY, cam.maxY = oy + 5.5, oy + self.height - 5.5
    cam:snap()
    self:theme(k)
    self.state = "playing"
    self:hideMenu()
    agr.time.setScale(1)
    agr.fx.fade(0, 0.6, 0, 0, 0)
    G.text("LevelText", agr.tr("plat.level" .. k))
    self:say(agr.tr("plat.go"), 2)
    for i = 1, 3 do self:crystalIcon(i, false) end
    self:hud()
end

function Game:respawnLater(e, seconds)
    table.insert(self.pending, { e = e, t = seconds, level = self.level })
end

function Game:checkpoint(x, y)
    if self.spawnX == x then return end
    self.spawnX, self.spawnY = x, y + 0.1
    self:say(agr.tr("plat.checkpoint"), 1.5)
    agr.audio.play("assets://audio/coin.wav", { pitch = 1.5 })
    agr.fx.burst(x, y + 1.5, 0, { count = 24, color = { 0.6, 0.85, 1 }, speed = 4, life = 0.6, spread = "up", texture = "assets://games/sparkle.png" })
end

function Game:collect(kind, value, e)
    local x, y = e:position()
    if kind == "coin" then
        self.coins = self.coins + 1
        self.data.bank = self.data.bank + 1
        local icon = agr.find("CoinIcon")
        if icon then agr.tween(icon, { uiSize = { 50, 50 } }, 0.3, { from = { 70, 70 }, ease = "outBack" }) end
    elseif kind == "crystal" then
        local i = tonumber(e:name():match("%d+")) or 1
        self.found = self.found | (1 << (i - 1))
        self:crystalIcon(i, true)
        self:say(agr.tr("plat.crystal", count(self.found)), 1.5)
        agr.fx.burst(x, y, 0, { count = 30, color = { 0.8, 0.6, 1 }, speed = 4, life = 0.8, spread = "up", texture = "assets://games/sparkle.png" })
    elseif kind == "heart" then
        self.hearts = math.min(self.maxHearts, self.hearts + 1)
        agr.fx.popup(x, y + 0.5, 0, "+1", { color = { 1, 0.5, 0.55 }, size = 48 })
    end
    self:hud()
end

function Game:crystalIcon(i, lit)
    local icon = agr.find("CrystalIcon " .. i)
    if not icon then return end
    icon:set("UIImage", "color", lit and { 1, 1, 1, 1 } or { 0.2, 0.2, 0.3, 0.8 })
    if lit then agr.tween(icon, { uiSize = { 58, 58 } }, 0.5, { from = { 96, 96 }, ease = "outElastic" }) end
end

function Game:enemyDefeated(kind, x, y)
    self.data.bank = self.data.bank + 1
    agr.fx.popup(x, y + 1, 0, "+1", { color = { 1, 0.9, 0.4 }, size = 44 })
    self:hud()
end

-- Un cœur perdu : chute (retour au point de passage) ou coup (recul).
function Game:playerHurt(fell)
    if self.state ~= "playing" then return end
    self.hearts = self.hearts - 1
    agr.device.vibrate(0.15)
    agr.audio.play("assets://audio/hurt.wav", { volume = 0.7 })
    agr.fx.shake(0.45, 0.35)
    agr.fx.flash(1, 0.15, 0.1, 0.4, 0.3)
    local icon = agr.find("HeartIcon " .. (self.hearts + 1))
    if icon then agr.tween(icon, { uiSize = { 58, 58 } }, 0.4, { from = { 90, 90 }, ease = "outBack", unscaled = true }) end
    self:hud()
    if self.hearts <= 0 then
        self.state = "dying"
        self.dyingTime = 1.0
        agr.time.setScale(0.4)
        agr.fx.fade(0.6, 1, 0.2, 0, 0)
        return
    end
    if fell then
        self.hero:script():spawn(self.spawnX, self.spawnY)
        self.hero:script().invulnerable = 1.5
        agr.fx.fade(0, 0.5, 0, 0, 0)
    end
end

function Game:levelComplete()
    if self.state ~= "playing" then return end
    local k = self.level
    local before = self:crystalsOf(k)
    self.data.crystals[tostring(k)] = before | self.found
    self.newBest = (before | self.found) ~= before
    local best = self.data.best[tostring(k)]
    if not best or self.time < best then self.data.best[tostring(k)] = self.time end
    agr.audio.play("assets://audio/portal.wav", { volume = 0.8 })
    local x, y = self.hero:position()
    agr.fx.burst(x, y + 1.5, 0, { count = 80, color = { 1, 0.85, 0.3 }, speed = 7, life = 1.2, size = 0.3, spread = "up", texture = "assets://games/sparkle.png" })
    self:clearBullets()
    if k >= self.levels then
        self.data.finished = true
        self:save()
        self.state = "victory"
        agr.fx.flash(1, 0.9, 0.6, 0.7, 1.2)
        self:showMenu("victory")
    else
        self.data.unlocked = math.max(self.data.unlocked, k + 1)
        self:save()
        self.state = "results"
        self:showMenu("results")
    end
end

-- ---- HUD
function Game:clock(t) return string.format("%d:%02d", math.floor(t / 60), math.floor(t % 60)) end

function Game:say(text, seconds)
    G.text("Message", text)
    self.messageTime = seconds or 2
end

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

function Game:hud()
    for i = 1, self.maxHearts do
        local icon = agr.find("HeartIcon " .. i)
        if icon then icon:set("UIImage", "color", i <= (self.hearts or 0) and { 1, 1, 1, 1 } or { 0.25, 0.15, 0.15, 0.7 }) end
    end
    G.text("CoinsText", tostring(self.data.bank))
    G.text("LivesText", agr.tr("plat.lives", math.max(0, self.hearts or 0)))
end

function Game:onUpdate(dt)
    self.messageTime = self.messageTime - dt
    if self.messageTime <= 0 then G.text("Message", "") end
    if agr.input.keyPressed("P") or agr.input.keyPressed("Escape") or agr.input.gamepadPressed("Start") then self:togglePause() end
    for i = #self.pending, 1, -1 do
        local p = self.pending[i]
        p.t = p.t - dt
        if p.t <= 0 then
            if p.e:isValid() and p.level == self.level then G.respawn(p.e, self.homes[p.level]["Props"][p.e:name()]) end
            table.remove(self.pending, i)
        end
    end
    if self.state == "dying" then
        self.dyingTime = self.dyingTime - dt / 0.4
        if self.dyingTime <= 0 then
            agr.time.setScale(1)
            self.state = "gameover"
            self:clearBullets()
            self:showMenu("gameover")
        end
        return
    end
    if self.state ~= "playing" then return end
    self.time = self.time + dt
    G.text("TimeText", self:clock(self.time))
end

return Game
