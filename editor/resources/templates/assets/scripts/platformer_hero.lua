-- Héros de plateforme 2D (AGR ENGINE, Lua 5.4).
-- Clavier : flèches / WASD / ZQSD pour marcher, Espace ou Haut pour sauter.
-- Tactile : boutons de l'interface « Left », « Right » et « Jump » (maintenus).
-- L'entité a un RigidBody2D (rotation bloquée), un Collider2D capsule dont le bas est aux pieds,
-- un SpriteRenderer, un SpriteAnimator et un Animator dont la machine à états (hero.agrctrl)
-- choisit l'animation selon les paramètres « speed » et « grounded » réglés ici.
local Hero = {
    properties = {
        speed = 4.5,       -- m/s
        jumpSpeed = 13.5,  -- m/s vers le haut au départ du saut
        fallLimit = -12,   -- en dessous : retour au point de départ
        total = 3,         -- objets à ramasser (affichage)
    },
}

function Hero:onStart()
    self.startX, self.startY = self.entity:position()
    self.facing = 1
    self.grounded = false
    self.collected = 0
    self.jumpHeld = false
    self.score = agr.find("Score")  -- texte de l'interface (peut ne pas exister)
    self:showScore()
end

function Hero:showScore()
    if self.score then self.score:setText(agr.tr("hud.mushrooms", self.collected, self.total)) end
end

-- Boutons tactiles de l'interface : déplacement (-1, 0, 1) et saut (au moment de l'appui).
local function held(name)
    local b = agr.find(name)
    return b ~= nil and agr.ui.isPressed(b)
end

function Hero:onUpdate(dt)
    local e = self.entity
    local x, y = e:position()

    -- Au sol si un rayon partant juste au-dessus des pieds touche quelque chose sous eux
    -- (le rayon part de l'intérieur de notre capsule, qu'il ignore donc).
    local hit = agr.physics2d.raycast(x, y + 0.05, 0, -1, 0.15)
    self.grounded = hit ~= nil and hit.entity ~= e

    -- Actions (Affichage > Entrées) : clavier, manette et joystick tactile ; plus les boutons du HUD.
    local move = agr.input.value("move_x")
    if held("Left") then move = -1 elseif held("Right") then move = 1 end
    local jumpButton = held("Jump")
    local jump = agr.input.pressed("jump") or agr.input.keyPressed("Up")
              or agr.input.keyPressed("W") or agr.input.keyPressed("Z")
              or (jumpButton and not self.jumpHeld)
    self.jumpHeld = jumpButton

    local _, vy = e:velocity()
    if jump and self.grounded then
        vy = self.jumpSpeed
        agr.audio.play("assets://audio/jump.wav", { volume = 0.7 })
    end
    e:setVelocity(move * self.speed, vy)

    if move ~= 0 then self.facing = move > 0 and 1 or -1 end
    e:set("SpriteRenderer", "flipX", self.facing < 0)
    -- La machine à états (Idle / Run / Air) choisit le clip de sprite.
    e:setParameter("speed", math.abs(move))
    e:setParameter("grounded", self.grounded)

    if y < self.fallLimit then  -- tombé dans l'eau ou hors du monde
        e:setPosition(self.startX, self.startY)
        e:setVelocity(0, 0)
    end
end

-- Appelé par les objets à ramasser (collectible.lua).
function Hero:collect(what)
    self.collected = self.collected + 1
    self:showScore()
    agr.log("Ramassé : " .. what .. " (" .. self.collected .. ")")
    if self.collected >= self.total and self.score then self.score:setText(agr.tr("hud.all_collected")) end
end

return Hero
