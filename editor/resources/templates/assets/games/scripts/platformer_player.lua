-- Robo, héros du jeu de plateforme (AGR ENGINE).
-- Clavier / manette : action move_x, saut (Espace, Haut, Z, W, bouton A) ; boutons tactiles < > Saut.
-- Saut tolérant (un peu après le bord, un peu avant d'atterrir), saut plus court si on relâche,
-- rebond sur les ennemis (sauf les hérissons), ressorts, plateformes qui portent, blocs friables.
local G = require("assets://games/scripts/common.lua")

local Player = {
    properties = { speed = 6, jumpSpeed = 13.5, invulnerableTime = 1.5, coyote = 0.1, buffer = 0.12 },
}

function Player:onStart()
    self.game = agr.find("Game"):script()
    self.facing, self.invulnerable, self.coyoteLeft, self.bufferLeft, self.knock = 1, 0, 0, 0, 0
    self.jumpHeld, self.jumpButtonHeld = false, false
end

function Player:spawn(x, y)
    self.entity:setPosition(x, y)
    self.entity:setVelocity(0, 0)
    self.invulnerable, self.knock = 0.5, 0
end

local function held(name)
    local b = agr.find(name)
    return b ~= nil and b:isActive() and agr.ui.isPressed(b)
end

function Player:squash(sx, sy)
    agr.tween(self.entity, { scale = { 1, 1, 1 } }, 0.22, { from = { sx, sy, 1 }, ease = "outBack" })
end

-- Sol sous les pieds : trois rayons (centre et bords de la capsule).
function Player:groundUnder(x, y)
    for _, dx in ipairs({ 0, -0.28, 0.28 }) do
        local hit = agr.physics2d.raycast(x + dx, y + 0.08, 0, -1, 0.2)
        if hit and hit.entity ~= self.entity then return hit end
    end
    return nil
end

function Player:onUpdate(dt)
    local e = self.entity
    local x, y = e:position()
    local vx, vy = e:velocity()
    if not self.game or self.game.state ~= "playing" then
        e:setVelocity(0, self.game and self.game.state == "dying" and vy or 0)
        e:setParameter("speed", 0)
        return
    end
    self.invulnerable = math.max(0, self.invulnerable - dt)
    self.knock = self.knock - dt
    e:set("SpriteRenderer", "color", { 1, 1, 1, (self.invulnerable > 0 and math.floor(self.invulnerable * 10) % 2 == 0) and 0.4 or 1 })

    local hit = self:groundUnder(x, y)
    local wasGrounded = self.grounded
    self.grounded = hit ~= nil and vy < 0.5
    local ground = hit and hit.entity and hit.entity:isValid() and hit.entity:script()
    if self.grounded then self.coyoteLeft = self.coyote else self.coyoteLeft = self.coyoteLeft - dt end
    if self.grounded and not wasGrounded and (self.fallSpeed or 0) < -8 then
        self:squash(1.25, 0.8)
        agr.fx.burst(x, y + 0.1, 0, { count = 8, color = { 0.85, 0.8, 0.7 }, speed = 2.5, life = 0.35, size = 0.12, gravity = -2, additive = false })
    end
    self.fallSpeed = vy

    local move = agr.input.value("move_x")
    if held("Left") then move = -1 elseif held("Right") then move = 1 end
    local button = held("Jump")
    local down = agr.input.down("jump") or agr.input.keyDown("Up") or agr.input.keyDown("W") or agr.input.keyDown("Z") or button
    local pressed = agr.input.pressed("jump") or agr.input.keyPressed("Up") or agr.input.keyPressed("W") or agr.input.keyPressed("Z")
                    or (button and not self.jumpButtonHeld)
    self.jumpButtonHeld = button
    if pressed then self.bufferLeft = self.buffer else self.bufferLeft = self.bufferLeft - dt end
    if self.bufferLeft > 0 and self.coyoteLeft > 0 then
        vy = self.jumpSpeed
        self.bufferLeft, self.coyoteLeft, self.rising = 0, 0, true
        self:squash(0.8, 1.25)
        agr.audio.play("assets://audio/jump.wav", { volume = 0.5 })
    end
    if self.rising and not down and vy > 3 then  -- relâché tôt : saut plus court
        vy = vy * 0.5
        self.rising = false
    end
    if vy <= 0 then self.rising = false end
    local carry = 0
    if self.grounded and ground then
        if ground.kind == "mover" then carry = ground.vx or 0 end
        if ground.kind == "crumble" and ground.step then ground:step() end
    end
    if self.knock > 0 then
        -- recul : la vitesse donnée par hurt() est gardée
    else
        e:setVelocity(move * self.speed + carry, vy)
    end
    if move ~= 0 then self.facing = move > 0 and 1 or -1 end
    e:set("SpriteRenderer", "flipX", self.facing < 0)
    e:setParameter("speed", math.abs(move))
    e:setParameter("grounded", self.grounded)
    if y < self.game:killY() then self.game:playerHurt(true) end
end

function Player:bounce(v)
    local vx = self.entity:velocity()
    self.entity:setVelocity(vx, v)
    self.rising = false
    self.coyoteLeft = 0
end

function Player:collect(kind, value, entity)
    self.game:collect(kind, value, entity)
end

function Player:hurt(from)
    if self.invulnerable > 0 or self.game.state ~= "playing" then return end
    self.invulnerable = self.invulnerableTime
    if from and from:isValid() then
        local x = self.entity:position()
        local fx = from:position()
        self.entity:setVelocity(x < fx and -7 or 7, 8)
        self.knock = 0.3
    end
    self.game:playerHurt(false)
end

-- Contact avec un ennemi : par-dessus = écrasé (sauf hérisson, canon), sinon on est touché.
function Player:onCollision(other, began)
    if not began or self.game.state ~= "playing" then return end
    local s = other:isValid() and other:script()
    if not s or not s.isEnemy or s.dead then return end
    local _, y = self.entity:position()
    local _, oy = other:position()
    local margin = s.stompHeight or 0.45
    if s.kind == "cannon" then return end
    -- Par-dessus : pieds au-dessus du haut de l'ennemi, ou en pleine chute un peu plus bas (le
    -- contact est signalé après le pas de physique, quand les formes se chevauchent déjà).
    if y >= oy + margin or ((self.fallSpeed or 0) < -1 and y >= oy + margin * 0.3) then
        if s.kind == "spiky" then
            self:bounce(self.jumpSpeed * 0.8)
            self:hurt(nil)
        else
            s:stomped()
            local down = agr.input.down("jump") or held("Jump") or agr.input.keyDown("Up")
            self:bounce(self.jumpSpeed * (down and 1.0 or 0.7))
        end
    else
        self:hurt(other)
    end
end

function Player:reach(kind, x, y)
    if kind == "checkpoint" then self.game:checkpoint(x, y) else self.game:levelComplete() end
end

return Player
