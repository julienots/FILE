-- Garde en patrouille (paquet officiel patrol-ai). Points : entités "<nom>_Point1", "<nom>_Point2"...
-- États : patrol -> chase (cible vue) -> patrol (perdue). Signaux : "spotted" (cible), "lost".
local Patrol = {
    signals = { spotted = {"entity"}, lost = {} },
    properties = { target = "Player", speed = 2, chaseSpeed = 3.5, sight = 6, fov = 120, loseAfter = 2, reach = 0.6 },
}

function Patrol:onStart()
    self.points = {}
    for i = 1, 32 do
        local p = agr.find(self.entity:name() .. "_Point" .. i)
        if not p then break end
        local x, y, z = p:worldPosition()
        self.points[#self.points + 1] = { x, y, z }
    end
    self.index, self.state, self.lost = 1, "patrol", 0
    self.fx, self.fy = 1, 0
end

function Patrol:sees(t)
    local x, y = self.entity:worldPosition()
    local tx, ty = t:worldPosition()
    local dx, dy = tx - x, ty - y
    local d = math.sqrt(dx * dx + dy * dy)
    if d > self.sight then return false end
    if d > 0.001 then
        local cosAngle = (dx * self.fx + dy * self.fy) / d
        if cosAngle < math.cos(math.rad(self.fov * 0.5)) then return false end
    end
    local hit = agr.physics2d.raycast(x, y, dx, dy, d)
    return hit == nil or hit.entity == t or hit.entity == self.entity
end

function Patrol:moveTo(tx, ty, speed, dt)
    local x, y, z = self.entity:position()
    local dx, dy = tx - x, ty - y
    local d = math.sqrt(dx * dx + dy * dy)
    if d < 1e-4 then return true end
    self.fx, self.fy = dx / d, dy / d
    local step = math.min(d, speed * dt)
    self.entity:setPosition(x + self.fx * step, y + self.fy * step, z)
    return d <= self.reach
end

function Patrol:onUpdate(dt)
    local t = agr.find(self.target)
    if self.state ~= "chase" and t and self:sees(t) then
        self.state, self.lost = "chase", 0
        self.entity:emit("spotted", t)
    end
    if self.state == "chase" then
        if not t then self.state = "patrol" return end
        if self:sees(t) then self.lost = 0 else self.lost = self.lost + dt end
        if self.lost > self.loseAfter then
            self.state = "patrol"
            self.entity:emit("lost")
            return
        end
        local tx, ty = t:worldPosition()
        self:moveTo(tx, ty, self.chaseSpeed, dt)
    elseif #self.points > 0 then
        local p = self.points[self.index]
        if self:moveTo(p[1], p[2], self.speed, dt) then self.index = self.index % #self.points + 1 end
    end
end

return Patrol
