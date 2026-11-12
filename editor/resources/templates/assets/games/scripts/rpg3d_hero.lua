-- Héros du RPG 3D (AGR ENGINE) : mêmes règles que le RPG 2D (quête, niveaux, sauvegarde),
-- déplacement en 3D relatif à la caméra (ZQSD / flèches / joystick), épée : Espace, J ou bouton,
-- parler : E ou bouton près de l'ancien. Q / R font tourner la caméra.
local G = require("assets://games/scripts/common.lua")

local Hero = {
    properties = { speed = 5, baseHp = 10, attackRange = 2.2, attackCooldown = 0.4, talkRange = 2.6, fallLimit = -20 },
}

function Hero:onStart()
    self.startX, self.startY, self.startZ = self.entity:position()
    self.level = agr.save.get("rpg3d.level", 1)
    self.xp = agr.save.get("rpg3d.xp", 0)
    self.quest = agr.save.get("rpg3d.quest", "none")
    self.gems, self.dead = 0, {}
    self.homes = G.homes("Enemies")
    self:restart()
end

function Hero:maxHp() return self.baseHp + (self.level - 1) * 2 end
function Hero:xpNeeded() return 5 * self.level end
function Hero:damageDealt() return 1 + math.floor((self.level - 1) / 2) end

function Hero:restart()
    self.hp = self:maxHp()
    self.state = "playing"
    self.cooldown, self.invulnerable, self.dialogTime, self.messageTime, self.swing = 0, 0, 0, 0, 0
    self.entity:setPosition(self.startX, self.startY, self.startZ)
    self.entity:setVelocity(0, 0, 0)
    for _, e in ipairs(G.children("Enemies")) do G.respawn(e, self.homes[e:name()]) end
    self.dead = {}
    G.show("Restart", false)
    G.show("Dialog", false)
    G.text("Message", "")
    self:hud()
end

function Hero:save()
    agr.save.set("rpg3d.level", self.level)
    agr.save.set("rpg3d.xp", self.xp)
    agr.save.set("rpg3d.quest", self.quest)
end

function Hero:hud()
    G.text("HpText", agr.tr("rpg.hp", math.max(0, self.hp), self:maxHp()))
    local fill = agr.find("HpFill")
    if fill then fill:set("UIRect", "size", { 552 * math.max(0, self.hp) / self:maxHp(), 26 }) end
    G.text("LevelText", agr.tr("rpg.level", self.level, self.xp, self:xpNeeded()))
    if self.quest == "none" then G.text("QuestText", agr.tr("rpg3d.quest.none"))
    elseif self.quest == "active" then G.text("QuestText", agr.tr("rpg3d.quest.active", math.min(self.gems, 3)))
    else G.text("QuestText", agr.tr("rpg3d.quest.done")) end
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
    local _, vy = e:velocity()
    if self.state ~= "playing" then
        e:setVelocity(0, vy, 0)
        return
    end
    self.cooldown = self.cooldown - dt
    self.invulnerable = math.max(0, self.invulnerable - dt)

    -- Déplacement relatif à la caméra (son axe avant projeté au sol).
    local mx, my = agr.input.value("move_x"), agr.input.value("move_y")
    local cam = agr.find("Main Camera")
    local fx, _, fz = 0, 0, -1
    if cam then fx, _, fz = cam:forward() end
    local fl = math.max(0.001, math.sqrt(fx * fx + fz * fz))
    fx, fz = fx / fl, fz / fl
    local dx, dz = fx * my - fz * mx, fz * my + fx * mx  -- avant * y + droite * x (droite = (-fz, fx))
    local len = math.sqrt(dx * dx + dz * dz)
    if len > 1 then dx, dz = dx / len, dz / len end
    e:setVelocity(dx * self.speed, vy, dz * self.speed)
    if len > 0.1 then e:setRotation(0, math.deg(math.atan(dx, dz)), 0) end

    -- Épée : un petit mouvement de l'arme.
    self.swing = math.max(0, self.swing - dt)
    local sword = e:find("Sword")
    if sword then sword:setRotation(-self.swing * 300, 0, 0) end

    if agr.input.pressed("jump") or agr.input.keyPressed("J") or self:buttonPressed("Attack") then self:attack() end
    local talk = self:buttonPressed("Talk")
    local x, y, z = e:position()
    local sage = agr.find("Sage")
    local near = false
    if sage then
        local sx, sy, sz = sage:position()
        near = G.distance(x, z, sx, sz) < self.talkRange
    end
    G.text("Hint", near and agr.tr("rpg.hint.talk") or "")
    if near and (agr.input.keyPressed("E") or talk) then sage:script():talk(self) end
    if y < self.fallLimit then e:setPosition(self.startX, self.startY + 1, self.startZ) end

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
    self.cooldown, self.swing = self.attackCooldown, 0.25
    agr.audio.play("assets://audio/swing.wav", { volume = 0.7, pitch = 0.9 + math.random() * 0.2 })
    local x, _, z = self.entity:position()
    for _, enemy in ipairs(G.children("Enemies")) do
        if enemy:isActive() then
            local ex, _, ez = enemy:position()
            if G.distance(x, z, ex, ez) < self.attackRange then
                local s = enemy:script()
                if s then s:hit(self:damageDealt(), self) end
            end
        end
    end
end

function Hero:damage(n)
    if self.state ~= "playing" or self.invulnerable > 0 then return end
    self.hp = self.hp - n
    self.invulnerable = 0.8
    agr.device.vibrate(0.1)
    agr.audio.play("assets://audio/hurt.wav", { volume = 0.6 })
    agr.fx.shake(0.4, 0.35)
    agr.fx.flash(1, 0.1, 0.05, 0.35, 0.3)
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
