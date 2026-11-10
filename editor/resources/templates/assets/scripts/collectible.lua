-- Objet à ramasser : un déclencheur (Collider2D « isTrigger ») qui disparaît au contact du héros
-- et prévient son script (fonction Hero:collect). Il flotte doucement en attendant ; son Animator
-- (mushroom_pulse.agranim) le fait aussi « respirer ».
local Collectible = {
    properties = { collector = "Hero", bob = 0.15, sound = "assets://audio/coin.wav" },
}

function Collectible:onStart()
    self.baseX, self.baseY, self.baseZ = self.entity:position()
    self.phase = self.baseX  -- décale l'oscillation entre les objets
end

function Collectible:onUpdate(dt)
    self.phase = self.phase + dt * 3
    self.entity:setPosition(self.baseX, self.baseY + math.sin(self.phase) * self.bob, self.baseZ)
end

function Collectible:onTrigger(other, began)
    if not began or other:name() ~= self.collector then return end
    local script = other:script()
    if script and script.collect then script:collect(self.entity:name()) end
    if self.sound ~= "" then agr.audio.play(self.sound) end
    self.entity:destroy()
end

return Collectible
