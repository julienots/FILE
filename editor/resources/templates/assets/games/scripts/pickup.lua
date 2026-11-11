-- Objet à ramasser (déclencheur) : prévient le script du ramasseur par collect(kind, value, entity),
-- joue un son et se cache (le jeu peut le réactiver en recommençant la partie).
local Pickup = {
    properties = { collector = "Hero", kind = "coin", value = 1, sound = "assets://audio/coin.wav", bob = 0.12 },
}

function Pickup:onStart()
    self.x, self.y, self.z = self.entity:position()
    self.phase = self.x
end

function Pickup:onUpdate(dt)
    self.phase = self.phase + dt * 3
    self.entity:setPosition(self.x, self.y + math.sin(self.phase) * self.bob, self.z)
end

function Pickup:onTrigger(other, began)
    if not began or other:name() ~= self.collector then return end
    local s = other:script()
    if s and s.collect then s:collect(self.kind, self.value, self.entity) end
    if self.sound ~= "" then agr.audio.play(self.sound, { volume = 0.8 }) end
    self.entity:setActive(false)
end

return Pickup
