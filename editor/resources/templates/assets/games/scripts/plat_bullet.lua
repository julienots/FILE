-- Boulet de canon : file à l'horizontale (sens = signe de l'échelle X), blesse le héros, se brise
-- contre le décor.
local Bullet = { properties = { speed = 6, life = 4 } }

function Bullet:onStart()
    self.dir = select(1, self.entity:scale()) < 0 and -1 or 1
    self.t = 0
    self.game = agr.find("Game"):script()
end

function Bullet:onUpdate(dt)
    if not self.game or self.game.state ~= "playing" then
        if self.game and self.game.state ~= "paused" then self.entity:destroy() end
        return
    end
    self.t = self.t + dt
    if self.t > self.life then return self.entity:destroy() end
    local x, y = self.entity:position()
    local hero = agr.find("Hero")
    local hx, hy = hero:position()
    if math.abs(hx - x) < 0.5 and y > hy and y < hy + 1.3 then
        hero:script():hurt(self.entity)
        return self.entity:destroy()
    end
    local step = self.speed * dt
    local hit = agr.physics2d.raycast(x, y, self.dir, 0, step + 0.2)
    if hit and hit.entity ~= hero then
        local s = hit.entity:isValid() and hit.entity:script()
        if not (s and s.kind == "cannon") then
            agr.fx.burst(x, y, 0, { count = 8, color = { 0.5, 0.5, 0.5 }, speed = 2, life = 0.3, size = 0.12, additive = false })
            return self.entity:destroy()
        end
    end
    self.entity:setPosition(x + self.dir * step, y)
end

return Bullet
