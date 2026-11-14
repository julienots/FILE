-- Éléments du décor du jeu de plateforme (propriété kind) :
--   spring  : ressort (déclencheur) qui propulse le héros très haut ;
--   mover   : plateforme mobile (corps cinématique) qui va et vient de (dx, dy) ; le héros est porté ;
--   crumble : bloc qui tremble puis tombe quand on reste dessus, et revient 3 s plus tard ;
--   sign    : panneau (déclencheur) qui affiche un conseil.
local Platform = { properties = { kind = "mover", dx = 0, dy = 0, speed = 2, power = 21, text = "" } }

function Platform:onStart()
    self.game = agr.find("Game"):script()
    self.x0, self.y0 = self.entity:position()
    self:reset()
end

function Platform:reset()
    self.t, self.vx, self.vy, self.shaking = 0, 0, 0, nil
    self.entity:setPosition(self.x0, self.y0)
    self.entity:setScale(1, 1, 1)
    if self.kind == "crumble" then self.entity:set("SpriteRenderer", "color", { 1, 1, 1, 1 }) end
    if self.kind == "mover" then self.entity:setVelocity(0, 0) end
end

function Platform:onUpdate(dt)
    if self.kind == "mover" then
        if not self.game or self.game.state ~= "playing" then
            self.entity:setVelocity(0, 0)
            self.vx, self.vy = 0, 0
            return
        end
        -- Va-et-vient doux : position = départ + d * (1 - cos(wt)) / 2.
        local dist = math.max(0.01, math.sqrt(self.dx * self.dx + self.dy * self.dy))
        local w = math.pi * self.speed / dist
        self.t = self.t + dt
        local k = 0.5 * w * math.sin(w * self.t)
        self.vx, self.vy = self.dx * k, self.dy * k
        -- Corrige la dérive : vise la position exacte de la courbe.
        local x, y = self.entity:position()
        local tx = self.x0 + self.dx * (1 - math.cos(w * (self.t + dt))) / 2
        local ty = self.y0 + self.dy * (1 - math.cos(w * (self.t + dt))) / 2
        self.entity:setVelocity((tx - x) / math.max(dt, 1e-3), (ty - y) / math.max(dt, 1e-3))
    elseif self.kind == "crumble" and self.shaking then
        self.shaking = self.shaking - dt
        self.entity:setPosition(self.x0 + math.sin(self.shaking * 70) * 0.05, self.y0)
        if self.shaking <= 0 then
            local x, y = self.x0, self.y0
            agr.fx.burst(x, y, 0, { count = 16, color = { 0.6, 0.55, 0.5 }, speed = 3, life = 0.6, size = 0.18, gravity = -10, additive = false })
            agr.audio.play("assets://audio/hit.wav", { volume = 0.4, pitch = 0.7 })
            self.game:respawnLater(self.entity, 3)
            self.entity:setActive(false)
        end
    end
end

-- Le héros se tient sur le bloc friable.
function Platform:step()
    if self.kind == "crumble" and not self.shaking then
        self.shaking = 0.5
        self.entity:set("SpriteRenderer", "color", { 1, 0.8, 0.7, 1 })
    end
end

function Platform:onTrigger(other, began)
    if other:name() ~= "Hero" or not self.game or self.game.state ~= "playing" then return end
    if self.kind == "spring" and began then
        local _, vy = other:velocity()
        if vy <= 1 then
            other:script():bounce(self.power)
            agr.tween(self.entity, { scale = { 1, 1, 1 } }, 0.35, { from = { 1.3, 0.5, 1 }, ease = "outElastic" })
            agr.audio.play("assets://audio/jump.wav", { volume = 0.6, pitch = 1.5 })
        end
    elseif self.kind == "sign" then
        if began then self.game:showHint(self.text) else self.game:hideHint(self.text) end
    end
end

return Platform
