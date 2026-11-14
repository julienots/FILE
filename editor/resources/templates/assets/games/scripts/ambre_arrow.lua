-- Flèche des archers squelettes : file droit devant elle (axe +X tourné par l'archer), blesse le
-- héros, se brise sur les murs et les piliers ; l'épée peut la renvoyer (parade).
local Arrow = { properties = { speed = 10, life = 3 } }

function Arrow:onStart()
    local _, yaw = self.entity:rotation()
    local a = math.rad(yaw)
    self.dx, self.dz = math.cos(a), -math.sin(a)
    self.t = 0
    self.game = agr.find("Game"):script()
    self.id = self.game:registerArrow(self.entity)
end

function Arrow:remove(burst)
    if not self.entity:isValid() then return end
    local x, y, z = self.entity:position()
    if burst then agr.fx.burst(x, y, z, { count = 8, color = { 0.8, 0.7, 0.5 }, speed = 2.5, life = 0.3, size = 0.12, additive = false }) end
    self.game.arrows[self.id] = nil
    self.entity:destroy()
end

function Arrow:onUpdate(dt)
    if self.game.state ~= "playing" then
        if self.game.state ~= "paused" then self:remove(false) end
        return
    end
    self.t = self.t + dt
    if self.t > self.life then return self:remove(false) end
    local x, y, z = self.entity:position()
    local step = self.speed * dt
    local hit = agr.physics3d.raycast(x, y, z, self.dx, 0, self.dz, step + 0.3)
    local hero = agr.find("Hero")
    local hx, hy, hz = hero:position()
    if math.sqrt((hx - x) ^ 2 + (hz - z) ^ 2) < 0.6 and y - hy > 0 and y - hy < 1.7 or (hit and hit.entity == hero) then
        hero:script():damage(1, self.entity)
        return self:remove(true)
    end
    if hit then return self:remove(true) end
    self.entity:setPosition(x + self.dx * step, y, z + self.dz * step)
end

-- Parade : l'épée du héros détruit la flèche.
function Arrow:parry()
    local x, y, z = self.entity:position()
    agr.fx.popup(x, y + 0.8, z, agr.tr("ambre.parry"), { color = { 0.7, 0.9, 1 }, size = 44 })
    agr.audio.play("assets://audio/hit.wav", { volume = 0.6, pitch = 1.8 })
    self:remove(true)
end

return Arrow
