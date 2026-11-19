-- Duel des Tours : combat de cartes en temps réel (AGR ENGINE).
-- Élixir qui se recharge, main de 4 cartes tirées d'un paquet de 6, cartes glissées sur sa moitié
-- de l'arène (ou touchées puis l'arène touchée), unités qui trouvent leur chemin par les ponts,
-- adversaire IA, 3 minutes avec double élixir à la fin, prolongation en mort subite, couronnes,
-- trophées sauvegardés.
local G = require("assets://games/scripts/common.lua")

local CARDS = {
    knight = { cost = 3, unit = "Knight", count = 1, color = { 0.45, 0.6, 1.0, 1 } },
    archer = { cost = 3, unit = "Archer", count = 2, color = { 0.95, 0.5, 0.75, 1 } },
    giant = { cost = 5, unit = "Giant", count = 1, color = { 0.95, 0.7, 0.35, 1 } },
    goblin = { cost = 2, unit = "Goblin", count = 3, color = { 0.5, 0.9, 0.4, 1 } },
    tank = { cost = 4, unit = "Tank", count = 1, color = { 0.65, 0.68, 0.72, 1 } },
    fireball = { cost = 4, spell = true, radius = 2.5, damage = 320, towerDamage = 130, color = { 1.0, 0.45, 0.2, 1 } },
}
local DECK = { "knight", "archer", "giant", "goblin", "fireball", "tank" }
local MAX_ELIXIR, REGEN, REGEN_DOUBLE = 10, 2.8, 1.4
local MATCH_TIME, OVERTIME = 180, 60
local TEAM_COLOR = { blue = { 0.4, 0.6, 1.0 }, red = { 1.0, 0.4, 0.3 } }

local Game = {}

-- ---- outils
local function shuffle(t)
    for i = #t, 2, -1 do
        local j = math.random(i)
        t[i], t[j] = t[j], t[i]
    end
    return t
end

local function copy(t)
    local r = {}
    for i, v in ipairs(t) do r[i] = v end
    return r
end

function Game:enemyOf(team) return team == "blue" and "red" or "blue" end

function Game:unitsOf(team) return self.units[team] end

function Game:towersOf(team)
    local list = {}
    for _, t in ipairs(G.children("Towers")) do
        if t:isActive() and t:name():lower():sub(1, #team) == team then list[#list + 1] = t end
    end
    return list
end

function Game:register(e)
    local team = e:name():match("^%a+ (%a+)") or "blue"
    local list = self.units[team]
    list[#list + 1] = e
end

function Game:unregister(e)
    for _, team in ipairs({ "blue", "red" }) do
        local list = self.units[team]
        for i = #list, 1, -1 do
            if list[i] == e then table.remove(list, i) end
        end
    end
end

-- ---- démarrage, menu
function Game:onStart()
    self.units = { blue = {}, red = {} }
    self.shotProto = agr.find("Shot Prototype")
    -- Effets : flèches (archères, tours), boulets (mini-tank), brûlure et gravats (modèles 3D cartoon).
    self.arrowProto = agr.find("Arrow Prototype") or self.shotProto
    self.ballProto = agr.find("Cannonball Prototype") or self.shotProto
    self.scorchProto = agr.find("Scorch Prototype")
    self.rubbleProto = agr.find("Rubble Prototype")
    self.towerHomes = G.homes("Towers")
    self.trophies = agr.save.get("duel.trophies") or 100
    self.frame = 0
    self.state = "menu"
    self:showMenu(agr.tr("duel.title"), agr.tr("duel.intro"))
end

function Game:showMenu(title, text)
    G.show("Menu", true)
    G.show("Showcase", true)
    self.cheer = 1.0
    G.show("Card Tray", false)
    G.text("MenuTitle", title)
    G.text("MenuText", text)
    G.text("Message", "")
    G.text("TrophiesText", agr.tr("duel.trophies", self.trophies))
    self:hideDrop()
end

function Game:clearUnits()
    for _, e in ipairs(G.children("Units")) do e:destroy() end
    self.units = { blue = {}, red = {} }
end

function Game:startBattle()
    self:clearUnits()
    for _, t in ipairs(G.children("Towers")) do G.respawn(t, self.towerHomes[t:name()]) end
    agr.nav.rebuild()
    self.elixir = { blue = 5, red = 5 }
    self.crowns = { blue = 0, red = 0 }
    self.time = MATCH_TIME
    self.overtime = false
    self.double = false
    self.decks = {}
    for _, team in ipairs({ "blue", "red" }) do
        local d = shuffle(copy(DECK))
        self.decks[team] = { hand = { d[1], d[2], d[3], d[4] }, queue = { d[5], d[6] } }
    end
    self.selected, self.dragging = nil, nil
    self.aiTimer, self.aiThreshold = 2.5, math.random(6, 9)
    self.spawned = 0
    self.state = "battle"
    G.show("Menu", false)
    G.show("Showcase", false)
    G.show("Card Tray", true)
    self:say(agr.tr("duel.fight"), 1.5)
    self:refreshCards()
    self:refreshHud()
end

function Game:say(text, seconds)
    G.text("Message", text)
    self.messageTime = seconds or 2
end

-- ---- cartes
function Game:cardName(kind) return agr.tr("duel.card." .. kind) end

function Game:refreshCards()
    local deck = self.decks.blue
    for i = 1, 4 do
        local kind = deck.hand[i]
        local card = CARDS[kind]
        local b = agr.find("Card " .. i)
        local label = b:find("Label")
        if label then label:setText(self:cardName(kind) .. "\n" .. card.cost) end
        b:set("UIImage", "color", card.color)
        local icon = b:find("Icon")
        if icon then icon:set("UIImage", "texture", "assets://games/duel/card_" .. kind .. ".png") end
        b:set("UIButton", "interactable", self.elixir.blue >= card.cost)
        local selected = self.selected == i
        b:set("UIButton", "normalColor", selected and { 1, 1, 1, 1 } or { 0.8, 0.8, 0.8, 1 })
        b:setScale(selected and 1.08 or 1, selected and 1.08 or 1, 1)
    end
    G.text("NextCard", agr.tr("duel.next") .. "\n" .. self:cardName(deck.queue[1]))
end

function Game:refreshHud()
    local e = self.elixir.blue
    for i = 1, 10 do
        local seg = agr.find("Elixir " .. i)
        local a = i <= math.floor(e) and 1 or (i == math.floor(e) + 1 and 0.25 + 0.5 * (e % 1) or 0.15)
        seg:set("UIImage", "color", { 0.85, 0.3, 0.95, a })
    end
    G.text("ElixirText", math.floor(e))
    local t = math.max(0, math.ceil(self.time))
    G.text("TimerText", (self.overtime and "+ " or "") .. string.format("%d:%02d", t // 60, t % 60))
    G.text("CrownsText", agr.tr("duel.crowns", self.crowns.blue, self.crowns.red))
end

-- Message des boutons de cartes : la carte est choisie, toucher l'arène la pose.
function Game:cardTapped(button)
    if self.state ~= "battle" then return end
    local i = tonumber(button:name():match("(%d+)$"))
    if not i then return end
    self.selected = self.selected ~= i and i or nil
    self.selectFrame = self.frame
    self:refreshCards()
    if self.selected then self:showZone(CARDS[self.decks.blue.hand[i]]) else self:hideDrop() end
end

-- Zone de déploiement d'une équipe : sa moitié, plus le couloir d'une tour de princesse ennemie tombée.
function Game:canDeploy(team, card, x, z)
    if not x then return false end
    if math.abs(z) > 7.4 or math.abs(x) > 14.8 then return false end
    if card.spell then return true end
    local sign = team == "blue" and 1 or -1
    local own = x * sign < -1.5
    if own then return true end
    local enemy = self:enemyOf(team)
    local lane = z < 0 and "Princess N" or "Princess S"
    local prefix = enemy == "red" and "Red " or "Blue "
    local t = agr.find(prefix .. lane)
    return (t == nil or not t:isActive()) and x * sign < 6
end

function Game:showZone(card)
    local zone = agr.find("Deploy Zone")
    zone:setActive(not card.spell)
end

function Game:hideDrop()
    G.show("Deploy Zone", false)
    G.show("Drop Marker", false)
end

function Game:moveMarker(card, x, z)
    local marker = agr.find("Drop Marker")
    if not x then
        marker:setActive(false)
        return
    end
    marker:setActive(true)
    marker:setPosition(x, 0.1, z)
    local r = card.spell and card.radius * 2 or 1.2
    marker:setScale(r, 0.1, r)
    local ok = self:canDeploy("blue", card, x, z)
    marker:set("MeshRenderer", "color", ok and { 1, 1, 1, 0.6 } or { 1, 0.2, 0.2, 0.6 })
end

-- Joue la carte n° i de la main d'une équipe en (x, z) ; renvoie vrai si elle a été posée.
function Game:play(team, i, x, z)
    local deck = self.decks[team]
    local kind = deck.hand[i]
    local card = CARDS[kind]
    if not kind or self.elixir[team] < card.cost or not self:canDeploy(team, card, x, z) then return false end
    self.elixir[team] = self.elixir[team] - card.cost
    deck.queue[#deck.queue + 1] = kind
    deck.hand[i] = table.remove(deck.queue, 1)
    if card.spell then
        self:fireball(team, card, x, z)
    else
        local proto = agr.find(card.unit .. " Prototype")
        local parent = agr.find("Units")
        for k = 1, card.count do
            local a = (k - 1) / card.count * math.pi * 2
            local off = card.count > 1 and 0.6 or 0
            local u = agr.clone(proto, parent)
            self.spawned = self.spawned + 1
            u:setName(card.unit .. " " .. team .. " " .. self.spawned)
            u:setPosition(x + math.cos(a) * off, 0, z + math.sin(a) * off)
            u:setRotation(0, team == "blue" and 90 or -90, 0)
        end
        agr.fx.burst(x, 0.3, z, { count = 14, color = TEAM_COLOR[team], speed = 2.5, life = 0.4 })
    end
    self:sfx("deploy", 0.55)
    if team == "blue" then self:refreshCards() end
    return true
end

function Game:fireball(team, card, x, z)
    local ball = agr.clone(self.shotProto, false)
    ball:setPosition(x - (team == "blue" and 6 or -6), 9, z)
    ball:setScale(1.1, 1.1, 1.1)
    ball:set("MeshRenderer", "color", card.color)
    agr.tween(ball, { position = { x, 0.3, z } }, 0.55, { ease = "inQuad", onDone = function()
        if ball:isValid() then ball:destroy() end
        if self.state ~= "battle" then return end
        agr.fx.burst(x, 0.5, z, { count = 60, color = { 1, 0.55, 0.2 }, endColor = { 0.4, 0.3, 0.3 }, speed = 6, life = 0.6, size = 0.35 })
        agr.fx.burst(x, 0.2, z, { count = 40, color = { 1, 0.85, 0.5 }, speed = 9, life = 0.35, size = 0.25, spread = "ring" })  -- onde de choc
        agr.fx.flash(1, 0.6, 0.2, 0.25, 0.15)
        self:scorch(x, z, card.radius)
        agr.fx.shake(0.35, 0.3)
        self:sfx("explosion", 0.8)
        local enemy = self:enemyOf(team)
        local victims = {}
        for _, u in ipairs(self.units[enemy]) do victims[#victims + 1] = { u, card.damage } end
        for _, t in ipairs(self:towersOf(enemy)) do victims[#victims + 1] = { t, card.towerDamage } end
        for _, v in ipairs(victims) do
            local e = v[1]
            if e:isValid() and e:isActive() then
                local ex, _, ez = e:position()
                local s = e:script()
                local reach = card.radius + (s and s.radius and s:radius() or 0)
                if G.distance(x, z, ex, ez) <= reach and s and s.damage then s:damage(v[2]) end
            end
        end
    end })
end

-- ---- combat
function Game:hit(target, dmg)
    if self.state ~= "battle" or not (target:isValid() and target:isActive()) then return end
    local s = target:script()
    if s and s.damage then s:damage(dmg) end
end

-- Sons du duel (tools/generate_duel_audio.py). Un même son ne repart pas plus d'une fois toutes les
-- 3 images : une grosse mêlée reste audible sans saturer. Hauteur légèrement variée.
function Game:sfx(name, volume)
    self.lastSfx = self.lastSfx or {}
    if self.lastSfx[name] and self.frame - self.lastSfx[name] < 3 then return end
    self.lastSfx[name] = self.frame
    agr.audio.play("assets://games/duel/" .. name .. ".wav", { volume = volume or 0.5, pitch = 0.92 + math.random() * 0.16 })
end

-- Petite gerbe d'étincelles là où un coup porte.
function Game:impact(x, y, z, heavy)
    agr.fx.burst(x, y, z, { count = heavy and 16 or 7, color = heavy and { 1, 0.7, 0.3 } or { 1, 1, 0.85 }, speed = heavy and 4 or 2.5,
                            life = 0.25, size = heavy and 0.22 or 0.12 })
end

-- Trace brûlée au sol (ombre ronde orange sombre) qui s'efface.
function Game:scorch(x, z, radius)
    if not self.scorchProto then return end
    local s = agr.clone(self.scorchProto, agr.find("Units"))
    s:setPosition(x, 0, z)
    s:set("BlobShadow", "radius", radius * 0.8)
    agr.tween(s, { ["BlobShadow.opacity"] = 0 }, 2.5, { from = 0.75, delay = 0.4, onDone = function() if s:isValid() then s:destroy() end end })
end

function Game:attack(src, target, dmg, ranged)
    if not ranged then
        self:hit(target, dmg)
        agr.tween(src, { scale = { 1.15, 1.15, 1.15 } }, 0.08, { ease = "outQuad", loop = "yoyo", times = 2 })
        local tx, ty, tz = target:worldPosition()
        self:impact(tx, ty + 0.8, tz, false)
        self:sfx("clash", 0.35)
        return
    end
    local x, y, z = src:worldPosition()
    local tx, ty, tz = target:worldPosition()
    local ss = src:script()
    local heavy = ss and ss.kind == "tank"
    local sy, ey = y + ((ss and ss.isTower) and 3.0 or 1.4), ty + 0.8
    local shot = agr.clone(heavy and self.ballProto or self.arrowProto, false)
    self:sfx(heavy and "cannon" or "bow", heavy and 0.5 or 0.3)
    shot:setPosition(x, sy, z)
    -- La flèche pointe vers sa cible (tangage puis lacet, en degrés).
    local dx, dy, dz = tx - x, ey - sy, tz - z
    shot:setRotation(-math.deg(math.atan(dy, math.max(1e-4, math.sqrt(dx * dx + dz * dz)))), math.deg(math.atan(dx, dz)), 0)
    local d = G.distance3(x, sy, z, tx, ey, tz)
    agr.tween(shot, { position = { tx, ey, tz } }, math.max(0.08, d / (heavy and 12 or 18)), { ease = "linear", onDone = function()
        if shot:isValid() then shot:destroy() end
        if self.state == "battle" and target:isValid() then self:impact(tx, ey, tz, heavy) end
        self:hit(target, dmg)
    end })
end

function Game:towerDestroyed(tower, team, king)
    -- Gravats à la place de la tour (nettoyés avec les unités à la partie suivante).
    if self.rubbleProto then
        local x, _, z = tower:position()
        local r = agr.clone(self.rubbleProto, agr.find("Units"))
        r:setPosition(x, 0, z)
        local k = king and 1.6 or 1.2
        agr.tween(r, { scale = { k, k, k } }, 0.35, { from = { 0.2, 0.2, 0.2 }, ease = "outBack" })
    end
    local winner = self:enemyOf(team)
    self.crowns[winner] = king and 3 or math.min(3, self.crowns[winner] + 1)
    self:say(agr.tr(team == "red" and "duel.towerDown" or "duel.towerLost"), 2)
    -- La tour du roi s'éveille quand une tour de princesse de son camp tombe.
    local kingTower = agr.find((team == "blue" and "Blue" or "Red") .. " King")
    local ks = kingTower and kingTower:isActive() and kingTower:script()
    if ks and ks.wake and not king then ks:wake() end
    self.navDirty = true
    self:refreshHud()
    if king or self.overtime then self:finish() end
end

function Game:finish()
    self.state = "over"
    self:hideDrop()
    for _, team in ipairs({ "blue", "red" }) do
        for _, u in ipairs(self.units[team]) do
            if u:isValid() then u:stopNavigation() end
        end
    end
    local b, r = self.crowns.blue, self.crowns.red
    local title, delta
    if b > r then
        title, delta = agr.tr("duel.win"), 30
        self:sfx("victory", 0.7)
        agr.fx.burst(12.5, 3, 0, { count = 120, color = { 1, 0.85, 0.3 }, speed = 7, life = 1.2 })
    elseif r > b then
        title, delta = agr.tr("duel.lose"), -20
    else
        title, delta = agr.tr("duel.draw"), 0
    end
    self.trophies = math.max(0, self.trophies + delta)
    agr.save.set("duel.trophies", self.trophies)
    local text = agr.tr("duel.result", b, r) .. "\n" .. agr.tr(delta >= 0 and "duel.gain" or "duel.loss", math.abs(delta))
    self:showMenu(title, text)
    G.text("CrownsText", agr.tr("duel.crowns", b, r))
end

-- ---- adversaire
function Game:affordable(team)
    local deck, list = self.decks[team], {}
    for i, kind in ipairs(deck.hand) do
        if CARDS[kind].cost <= self.elixir[team] then list[#list + 1] = i end
    end
    return list
end

function Game:handIndex(team, pred)
    for _, i in ipairs(self:affordable(team)) do
        if pred(self.decks[team].hand[i]) then return i end
    end
    return nil
end

function Game:thinkAI()
    local deck = self.decks.red
    -- Défense : une unité ennemie a passé la rivière.
    local threat, cluster = nil, 0
    for _, u in ipairs(self.units.blue) do
        local x, _, z = u:position()
        if x > -1 and (not threat or x > select(1, threat:position())) then threat = u end
        if x > 0 then cluster = cluster + 1 end
    end
    if threat then
        local tx, _, tz = threat:position()
        if cluster >= 3 then
            local i = self:handIndex("red", function(k) return k == "fireball" end)
            if i and self:play("red", i, tx, tz) then return end
        end
        local i = self:handIndex("red", function(k) return not CARDS[k].spell end)
        if i then
            local x = G.clamp(tx + 2.5, 2, 13)
            if self:play("red", i, x, G.clamp(tz, -7, 7)) then return end
        end
        return
    end
    -- Attaque quand l'élixir a atteint son seuil : un couloir au hasard, derrière la tour ou au pont.
    if self.elixir.red >= self.aiThreshold then
        local options = self:affordable("red")
        local pick = {}
        for _, i in ipairs(options) do
            if not CARDS[deck.hand[i]].spell then pick[#pick + 1] = i end
        end
        if #pick > 0 then
            local i = pick[math.random(#pick)]
            local z = math.random() < 0.5 and -4.5 or 4.5
            local x = CARDS[deck.hand[i]].unit == "Giant" and 11 or (math.random() < 0.5 and 2.5 or 10)
            self:play("red", i, x, z + (math.random() - 0.5))
            self.aiThreshold = math.random(5, 9)
        end
    end
end

-- ---- boucle
function Game:handleInput()
    local deck = self.decks.blue
    for _, g in ipairs(agr.input.gestures()) do
        if g.type == "dragBegin" then
            for i = 1, 4 do
                if agr.ui.isPressed(agr.find("Card " .. i)) then
                    self.dragging = i
                    self.selected = nil
                    self:refreshCards()
                    self:showZone(CARDS[deck.hand[i]])
                end
            end
        elseif g.type == "drag" and self.dragging then
            local x, _, z = agr.camera.screenToWorld(g.x, g.y, 0)
            self:moveMarker(CARDS[deck.hand[self.dragging]], x, z)
        elseif g.type == "dragEnd" and self.dragging then
            local i = self.dragging
            self.dragging = nil
            self:hideDrop()
            local x, _, z = agr.camera.screenToWorld(g.x, g.y, 0)
            if not agr.ui.pointerOverUI() and x and not self:play("blue", i, x, z) then self:say(agr.tr("duel.cannot"), 1) end
        elseif g.type == "tap" and self.selected and self.selectFrame ~= self.frame and not agr.ui.pointerOverUI() then
            local x, _, z = agr.camera.screenToWorld(g.x, g.y, 0)
            if self:play("blue", self.selected, x, z) then
                self.selected = nil
                self:hideDrop()
                self:refreshCards()
            else
                self:say(agr.tr("duel.cannot"), 1)
            end
        end
    end
end

function Game:onUpdate(dt)
    self.frame = self.frame + 1
    if self.messageTime then
        self.messageTime = self.messageTime - dt
        if self.messageTime <= 0 then
            self.messageTime = nil
            G.text("Message", "")
        end
    end
    if self.state ~= "battle" then
        -- Écran d'accueil : les personnages saluent chacun leur tour.
        self.cheer = (self.cheer or 1) - dt
        if self.cheer <= 0 then
            local poses = G.children("Showcase")
            if #poses > 0 then
                local p = poses[math.random(#poses)]
                if p:has("Animator") then p:setTrigger("attack") end
            end
            self.cheer = 1.6
        end
        return
    end
    if self.navDirty then
        self.navDirty = false
        agr.nav.rebuild()
    end

    local regen = self.double and REGEN_DOUBLE or REGEN
    local before = math.floor(self.elixir.blue)
    for _, team in ipairs({ "blue", "red" }) do
        self.elixir[team] = math.min(MAX_ELIXIR, self.elixir[team] + dt / regen)
    end
    if math.floor(self.elixir.blue) ~= before then self:refreshCards() end

    self.time = self.time - dt
    if not self.double and (self.overtime or self.time <= 60) then
        self.double = true
        self:say(agr.tr("duel.double"), 2)
    end
    if self.time <= 0 then
        if self.overtime or self.crowns.blue ~= self.crowns.red then
            self:finish()
            return
        end
        self.overtime = true
        self.time = OVERTIME
        self:say(agr.tr("duel.overtime"), 2.5)
    end

    self:handleInput()
    self.aiTimer = self.aiTimer - dt
    if self.aiTimer <= 0 then
        self.aiTimer = 0.6
        self:thinkAI()
    end
    self:refreshHud()
end

return Game
