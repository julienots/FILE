-- Lance des balles physiques depuis la caméra (Espace, clic gauche ou toucher) — AGR ENGINE.
-- Chaque balle est une entité créée par le script : MeshRenderer + RigidBody + Collider.
-- L'interface affiche le nombre de balles (texte « Counter ») et règle la vitesse avec un
-- curseur (UISlider « Power », qui appelle setPower) ; un clic sur l'interface ne tire pas.
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
    self.counter = agr.find("Counter")
    self:showCount()
end

function Launcher:showCount()
    if self.counter then
        -- Texte traduit (Affichage > Localisation, assets/localization.csv) avec ses valeurs {1}, {2}, {3}.
        self.counter:setText(agr.tr("hud.balls", #self.balls, self.maxBalls, math.floor(self.speed + 0.5)))
    end
end

-- Appelé par le curseur de l'interface (UISlider, message « setPower »).
function Launcher:setPower(slider, value)
    self.speed = value
    self:showCount()
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
    agr.audio.play("assets://audio/launch.wav", { volume = 0.6, pitch = 0.9 + math.random() * 0.2 })
    self:showCount()
end

function Launcher:onUpdate(dt)
    if agr.ui.pointerOverUI() then return end  -- le curseur de l'interface a la priorité
    -- Actions « jump » (Espace, bouton A) et « fire » (clic, Ctrl, bouton X) ; tout toucher lance aussi.
    local fire = agr.input.pressed("jump") or agr.input.pressed("fire")
    for i = 1, agr.input.touchCount() do
        local _, _, phase = agr.input.touch(i)
        if phase == "began" then fire = true end
    end
    if fire then self:fire() end
end

return Launcher
