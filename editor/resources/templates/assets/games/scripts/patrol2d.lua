-- Ennemi qui fait des allers-retours autour de sa position de départ (corps 2D dynamique).
local Patrol = {
    properties = { speed = 1.6, range = 2.5 },
}

function Patrol:onStart()
    self.x0, self.y0 = self.entity:position()
    self.dir = -1
end

-- Appelé quand la partie recommence.
function Patrol:reset()
    if not self.x0 then self:onStart() end  -- reset before our first frame
    self.entity:setActive(true)
    self.entity:setPosition(self.x0, self.y0)
    self.entity:setVelocity(0, 0)
    self.dir = -1
    self.moving = false
end

function Patrol:onUpdate(dt)
    local x = self.entity:position()
    local vx, vy = self.entity:velocity()
    if x < self.x0 - self.range then self.dir = 1 elseif x > self.x0 + self.range then self.dir = -1
    elseif self.moving and math.abs(vx) < 0.05 then self.dir = -self.dir end  -- blocked by a wall
    self.moving = true
    self.entity:setVelocity(self.dir * self.speed, vy)
    self.entity:set("SpriteRenderer", "flipX", self.dir > 0)
end

return Patrol
