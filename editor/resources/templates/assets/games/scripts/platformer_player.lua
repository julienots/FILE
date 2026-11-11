-- Joueur du modèle Plateforme 2D (AGR ENGINE).
-- Clavier / manette / zones tactiles : actions « move_x » et « jump » ; boutons Left / Right / Jump du HUD.
-- Sauter sur un slime le fait disparaître ; le toucher de côté coûte une vie. Tomber dans l'eau aussi.
-- Le drapeau bleu est un point de passage, le rouge l'arrivée.
local G = require("assets://games/scripts/common.lua")

local Player = {
    properties = { speed = 5, jumpSpeed = 13.5, lives = 3, fallLimit = -8, coinsTotal = 12, invulnerableTime = 1.5 },
}

function Player:onStart()
    self.startX, self.startY = self.entity:position()
    self.homes = G.homes("Enemies")
    self:restart()
end

function Player:restart()
    self.spawnX, self.spawnY = self.startX, self.startY
    self.coins, self.livesLeft = 0, self.lives
    self.state = "playing"
    self.invulnerable = 0
    self.facing = 1
    self.jumpHeld = false
    self.entity:setPosition(self.startX, self.startY)
    self.entity:setVelocity(0, 0)
    for _, c in ipairs(G.children("Coins")) do c:setActive(true) end
    for _, e in ipairs(G.children("Enemies")) do G.respawn(e, self.homes[e:name()]) end
    G.text("Message", "")
    G.show("Restart", false)
    self:hud()
end

function Player:hud()
    G.text("CoinsText", agr.tr("plat.coins", self.coins, self.coinsTotal))
    G.text("LivesText", agr.tr("plat.lives", self.livesLeft))
end

local function held(name)
    local b = agr.find(name)
    return b ~= nil and b:isActive() and agr.ui.isPressed(b)
end

function Player:onUpdate(dt)
    local e = self.entity
    local x, y = e:position()
    if self.state ~= "playing" then
        e:setVelocity(0, select(2, e:velocity()))
        e:setParameter("speed", 0)
        return
    end
    self.invulnerable = math.max(0, self.invulnerable - dt)
    e:set("SpriteRenderer", "color", { 1, 1, 1, (self.invulnerable > 0 and math.floor(self.invulnerable * 10) % 2 == 0) and 0.4 or 1 })

    local hit = agr.physics2d.raycast(x, y + 0.05, 0, -1, 0.15)
    self.grounded = hit ~= nil and hit.entity ~= e
    local move = agr.input.value("move_x")
    if held("Left") then move = -1 elseif held("Right") then move = 1 end
    local jumpButton = held("Jump")
    local jump = agr.input.pressed("jump") or agr.input.keyPressed("Up") or agr.input.keyPressed("W")
              or agr.input.keyPressed("Z") or (jumpButton and not self.jumpHeld)
    self.jumpHeld = jumpButton
    local _, vy = e:velocity()
    if jump and self.grounded then
        vy = self.jumpSpeed
        agr.audio.play("assets://audio/jump.wav", { volume = 0.6 })
    end
    e:setVelocity(move * self.speed, vy)
    if move ~= 0 then self.facing = move > 0 and 1 or -1 end
    e:set("SpriteRenderer", "flipX", self.facing < 0)
    e:setParameter("speed", math.abs(move))
    e:setParameter("grounded", self.grounded)

    if y < self.fallLimit then self:hurt(true) end
end

function Player:collect(kind, value)
    if kind == "coin" then
        self.coins = self.coins + value
        self:hud()
    end
end

function Player:hurt(fell)
    -- Also after a fall: the respawn teleport takes effect on the next physics step.
    if self.state ~= "playing" or self.invulnerable > 0 then return end
    self.livesLeft = self.livesLeft - 1
    agr.device.vibrate(0.15)
    if self.livesLeft <= 0 then
        self.state = "lost"
        G.text("Message", agr.tr("game.over"))
        G.show("Restart", true)
    elseif fell then
        self.entity:setPosition(self.spawnX, self.spawnY)
        self.entity:setVelocity(0, 0)
    else
        local _, vy = self.entity:velocity()
        self.entity:setVelocity(-self.facing * 6, 7)
    end
    self.invulnerable = self.invulnerableTime
    self:hud()
end

function Player:onCollision(other, began)
    if not began or self.state ~= "playing" or not other:name():find("^Slime") then return end
    local _, y = self.entity:position()
    local _, oy = other:position()
    if y > oy + 0.45 then  -- landed on it
        other:setActive(false)
        local vx = self.entity:velocity()
        self.entity:setVelocity(vx, self.jumpSpeed * 0.7)
        agr.audio.play("assets://audio/launch.wav", { volume = 0.6 })
    else
        self:hurt(false)
    end
end

function Player:reach(kind, x, y)
    if self.state ~= "playing" then return end
    if kind == "checkpoint" then
        if self.spawnX ~= x then
            self.spawnX, self.spawnY = x, y
            G.text("Message", agr.tr("plat.checkpoint"))
            agr.audio.play("assets://audio/coin.wav", { pitch = 1.5 })
        end
    elseif kind == "goal" then
        self.state = "won"
        local best = G.record("platformer.bestCoins", self.coins)
        G.text("Message", agr.tr("plat.won", self.coins, self.coinsTotal, best))
        G.show("Restart", true)
    end
end

return Player
