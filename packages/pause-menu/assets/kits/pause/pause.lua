-- Menu pause (paquet officiel pause-menu). Sur une entité toujours active (le Canvas, par exemple),
-- avec un enfant `panel` (UIPanel et ses boutons) caché au démarrage : Échap, Start (manette) ou
-- retour (Android) montrent le panneau et arrêtent le temps ; encore une fois pour reprendre.
-- Les boutons du panneau reçoivent kits/pause/pause_button.lua (action "resume" ou "quit").
local Pause = {
    properties = { panel = "PausePanel", key = "Escape" },
    signals = { paused = {}, resumed = {} },
}

function Pause:onStart()
    self.paused = false
    self.panelEntity = self.entity:find(self.panel)
    if self.panelEntity then self.panelEntity:setActive(false) end
end

function Pause:toggle()
    self.paused = not self.paused
    agr.time.setScale(self.paused and 0 or 1)
    if self.panelEntity then self.panelEntity:setActive(self.paused) end
    self.entity:emit(self.paused and "paused" or "resumed")
end

function Pause:onUpdate()
    if agr.input.keyPressed(self.key) or agr.input.keyPressed("Back") or agr.input.gamepadPressed("Start") then self:toggle() end
end

function Pause:onDestroy()
    if self.paused then agr.time.setScale(1) end
end

return Pause
