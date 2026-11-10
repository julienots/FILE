-- Lance des balles physiques depuis la caméra (Espace, clic gauche ou toucher) — AGR ENGINE.
-- Chaque balle est une entité créée par le script : MeshRenderer + RigidBody + Collider.
local Launcher = {
    properties = {
        speed = 14,          -- m/s
        radius = 0.25,
        material = "assets://3d/gold.agrmat",
        maxBalls = 25,       -- les plus anciennes disparaissent au-delà
    },
}

function Launcher:onStart()
    self.balls = {}
    self.count = 0
end

function Launcher:fire()
    local x, y, z = self.entity:worldPosition()
    local fx, fy, fz = self.entity:forward()
    self.count = self.count + 1
    local ball = agr.spawn("Ball " .. self.count)
    ball:setPosition(x + fx, y + fy, z + fz)
    ball:setScale(self.radius * 2)
    ball:add("MeshRenderer")
    ball:set("MeshRenderer", "mesh", "Sphere")
    ball:set("MeshRenderer", "material", self.material)
    ball:add("RigidBody")
    ball:set("RigidBody", "initialVelocity", { fx * self.speed, fy * self.speed + 2, fz * self.speed })
    ball:set("RigidBody", "bullet", true)
    ball:add("Collider")
    ball:set("Collider", "shape", "Sphere")
    ball:set("Collider", "radius", 0.5)       -- en unités locales : l'échelle donne le rayon réel
    ball:set("Collider", "density", 3000)
    ball:set("Collider", "restitution", 0.4)
    table.insert(self.balls, ball)
    if #self.balls > self.maxBalls then
        local old = table.remove(self.balls, 1)
        if old:isValid() then old:destroy() end
    end
end

function Launcher:onUpdate(dt)
    local fire = agr.input.keyPressed("Space") or agr.input.mousePressed(0)
    for i = 1, agr.input.touchCount() do
        local _, _, phase = agr.input.touch(i)
        if phase == "began" then fire = true end
    end
    if fire then self:fire() end
end

return Launcher
