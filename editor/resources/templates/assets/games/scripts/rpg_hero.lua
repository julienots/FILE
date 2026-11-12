-- Héros du modèle RPG 2D (AGR ENGINE).
-- Déplacement : flèches / ZQSD / stick / joystick tactile (actions move_x, move_y).
-- Épée : Espace, J ou bouton « Épée ». Parler : E ou bouton « Parler » près du sage.
-- Niveau, expérience et quête sont gardés dans la sauvegarde locale (agr.save).
local G = require("assets://games/scripts/common.lua")

local Hero = {
    properties = { speed = 4.2, baseHp = 10, attackRange = 1.5, attackCooldown = 0.35, talkRange = 1.8 },
}

function Hero:onStart()
    self.startX, self.startY = self.entity:position()
    self.level = agr.save.get("rpg2d.level", 1)
    self.xp = agr.save.get("rpg2d.xp", 0)
    self.quest = agr.save.get("rpg2d.quest", "none")  -- none, active, done
    self.gems = 0
    self.dead = {}  -- ennemis vaincus : { entité, temps avant retour }
    self.homes = G.homes("Enemies")
    self:restart()
end

function Hero:maxHp() return self.baseHp + (self.level - 1) * 2 end
function Hero:xpNeeded() return 5 * self.level end
function Hero:damageDealt() return 1 + math.floor((self.level - 1) / 2) end

function Hero:restart()
    self.hp = self:maxHp()
    self.state = "playing"
    self.cooldown, self.invulnerable, self.dialogTime, self.messageTime = 0, 0, 0, 0
    self.facing = 1
    self.entity:setPosition(self.startX, self.startY)
    self.entity:setVelocity(0, 0)
    for _, e in ipairs(G.children("Enemies")) do G.respawn(e, self.homes[e:name()]) end
    self.dead = {}
    G.show("Restart", false)
    G.show("Dialog", false)
    G.text("Message", "")
    self:hud()
end

function Hero:save()
    agr.save.set("rpg2d.level", self.level)
    agr.save.set("rpg2d.xp", self.xp)
    agr.save.set("rpg2d.quest", self.quest)
end

function Hero:hud()
    G.text("HpText", agr.tr("rpg.hp", math.max(0, self.hp), self:maxHp()))
    local fill = agr.find("HpFill")
    if fill then fill:set("UIRect", "size", { 552 * math.max(0, self.hp) / self:maxHp(), 26 }) end
    G.text("LevelText", agr.tr("rpg.level", self.level, self.xp, self:xpNeeded()))
    if self.quest == "none" then G.text("QuestText", agr.tr("rpg.quest.none"))
    elseif self.quest == "active" then G.text("QuestText", agr.tr("rpg.quest.active", math.min(self.gems, 3)))
    else G.text("QuestText", agr.tr("rpg.quest.done")) end
end

function Hero:say(text, seconds)
    G.show("Dialog", true)
    G.text("DialogText", text)
    self.dialogTime = seconds or 5
end

function Hero:flash(text)
    G.text("Message", text)
    self.messageTime = 2.5
end

-- Bouton tactile : vrai à l'image où il est enfoncé.
function Hero:buttonPressed(name)
    local b = agr.find(name)
    local down = b ~= nil and agr.ui.isPressed(b)
    self.buttons = self.buttons or {}
    local was = self.buttons[name]
    self.buttons[name] = down
    return down and not was
end

function Hero:onUpdate(dt)
    local e = self.entity
    self.dialogTime = self.dialogTime - dt
    if self.dialogTime <= 0 then G.show("Dialog", false) end
    self.messageTime = self.messageTime - dt
    if self.messageTime <= 0 and self.state == "playing" then G.text("Message", "") end
    if self.state ~= "playing" then
        e:setVelocity(0, 0)
        return
    end
    self.cooldown = self.cooldown - dt
    self.invulnerable = math.max(0, self.invulnerable - dt)
    e:set("SpriteRenderer", "color", self.invulnerable > 0 and { 1, 0.5, 0.5, 1 } or { 1, 1, 1, 1 })

    local mx, my = agr.input.value("move_x"), agr.input.value("move_y")
    local len = math.sqrt(mx * mx + my * my)
    if len > 1 then mx, my = mx / len, my / len end
    e:setVelocity(mx * self.speed, my * self.speed)
    if mx ~= 0 then
        self.facing = mx > 0 and 1 or -1
        e:set("SpriteRenderer", "flipX", self.facing < 0)
    end

    local attack = self:buttonPressed("Attack")
    if agr.input.pressed("jump") or agr.input.keyPressed("J") or attack then self:attack() end
    local talk = self:buttonPressed("Talk")
    local x, y = e:position()
    local sage = agr.find("Sage")
    local near = false
    if sage then
        local sx, sy = sage:position()
        near = G.distance(x, y, sx, sy) < self.talkRange
    end
    G.text("Hint", near and agr.tr("rpg.hint.talk") or "")
    if near and (agr.input.keyPressed("E") or talk) then sage:script():talk(self) end

    -- Les ennemis vaincus reviennent au bout d'un moment.
    for i = #self.dead, 1, -1 do
        local d = self.dead[i]
        d.time = d.time - dt
        if d.time <= 0 then
            G.respawn(d.entity, self.homes[d.entity:name()])
            table.remove(self.dead, i)
        end
    end
end

function Hero:attack()
    if self.cooldown > 0 then return end
    self.cooldown = self.attackCooldown
    agr.audio.play("assets://audio/swing.wav", { volume = 0.7, pitch = 0.9 + math.random() * 0.2 })
    local x, y = self.entity:position()
    for _, enemy in ipairs(G.children("Enemies")) do
        if enemy:isActive() then
            local ex, ey = enemy:position()
            if G.distance(x, y + 0.4, ex, ey) < self.attackRange then enemy:script():hit(self:damageDealt(), self) end
        end
    end
end

function Hero:damage(n)
    if self.state ~= "playing" or self.invulnerable > 0 then return end
    self.hp = self.hp - n
    self.invulnerable = 0.8
    agr.device.vibrate(0.1)
    agr.audio.play("assets://audio/hurt.wav", { volume = 0.6 })
    agr.fx.shake(0.35, 0.3)
    agr.fx.flash(1, 0.1, 0.05, 0.35, 0.3)
    local x, y = self.entity:position()
    agr.fx.popup(x, y + 1.2, 0, "-" .. n, { color = { 1, 0.35, 0.3 }, size = 52 })
    if self.hp <= 0 then
        self.state = "dead"
        G.text("Message", agr.tr("rpg.dead"))
        G.show("Restart", true)
    end
    self:hud()
end

function Hero:defeated(enemy, xp, respawn)
    table.insert(self.dead, { entity = enemy, time = respawn })
    self:gainXp(xp)
end

function Hero:gainXp(n)
    self.xp = self.xp + n
    while self.xp >= self:xpNeeded() do
        self.xp = self.xp - self:xpNeeded()
        self.level = self.level + 1
        self.hp = self:maxHp()
        self:flash(agr.tr("rpg.levelup", self.level))
        agr.audio.play("assets://audio/coin.wav", { pitch = 1.4 })
    end
    self:save()
    self:hud()
end

function Hero:collect(kind)
    if kind == "gem" then
        self.gems = self.gems + 1
        self:hud()
    end
end

function Hero:setQuest(q)
    self.quest = q
    self:save()
    self:hud()
end

return Hero
