-- Héros de plateforme 2D (AGR ENGINE, Lua 5.4).
-- Clavier : flèches / WASD / ZQSD pour marcher, Espace ou Haut pour sauter.
-- Tactile : toucher le tiers gauche ou droit de l'écran pour marcher, le tiers central pour sauter.
-- L'entité a un RigidBody2D (rotation bloquée), un Collider2D capsule dont le bas est aux pieds,
-- un SpriteRenderer avec la feuille du héros et un SpriteAnimator (clips "idle" et "run").
local Hero = {
    properties = {
        speed = 4.5,       -- m/s
        jumpSpeed = 13.5,  -- m/s vers le haut au départ du saut
        fallLimit = -12,   -- en dessous : retour au point de départ
    },
}

function Hero:onStart()
    self.startX, self.startY = self.entity:position()
    self.facing = 1
    self.grounded = false
    self.collected = 0
end

-- Commandes tactiles : renvoie le déplacement (-1, 0, 1) et le saut (début de toucher au centre).
local function touchControls()
    local width = agr.screen.size()
    if width <= 0 then return 0, false end
    local move, jump = 0, false
    for i = 1, agr.input.touchCount() do
        local x, _, phase = agr.input.touch(i)
        if x < width / 3 then
            move = -1
        elseif x > width * 2 / 3 then
            move = 1
        elseif phase == "began" then
            jump = true
        end
    end
    return move, jump
end

function Hero:onUpdate(dt)
    local e = self.entity
    local x, y = e:position()

    -- Au sol si un rayon partant juste au-dessus des pieds touche quelque chose sous eux
    -- (le rayon part de l'intérieur de notre capsule, qu'il ignore donc).
    local hit = agr.physics2d.raycast(x, y + 0.05, 0, -1, 0.15)
    self.grounded = hit ~= nil and hit.entity ~= e

    local move = agr.input.axis("horizontal")
    local jump = agr.input.keyPressed("Space") or agr.input.keyPressed("Up")
              or agr.input.keyPressed("W") or agr.input.keyPressed("Z")
    local touchMove, touchJump = touchControls()
    if touchMove ~= 0 then move = touchMove end
    jump = jump or touchJump

    local _, vy = e:velocity()
    if jump and self.grounded then vy = self.jumpSpeed end
    e:setVelocity(move * self.speed, vy)

    if move ~= 0 then self.facing = move > 0 and 1 or -1 end
    e:set("SpriteRenderer", "flipX", self.facing < 0)
    local running = move ~= 0 and self.grounded
    e:set("SpriteAnimator", "clip", running and "run" or "idle")

    if y < self.fallLimit then  -- tombé dans l'eau ou hors du monde
        e:setPosition(self.startX, self.startY)
        e:setVelocity(0, 0)
    end
end

-- Appelé par les objets à ramasser (collectible.lua).
function Hero:collect(what)
    self.collected = self.collected + 1
    agr.log("Ramassé : " .. what .. " (" .. self.collected .. ")")
end

return Hero
