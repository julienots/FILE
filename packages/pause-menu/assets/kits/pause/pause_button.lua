-- Bouton du menu pause (paquet officiel pause-menu) : "resume" reprend le jeu, "menu" reprend et
-- charge la scène `scene` (menu principal).
local PauseButton = {
    properties = { action = "resume", manager = "Canvas", scene = "game://scenes/menu.agrscene" },
}

function PauseButton:onClick()
    local manager = agr.find(self.manager)
    local pause = manager and manager:script()
    if pause and pause.toggle and pause.paused then pause:toggle() end
    if self.action == "menu" then agr.loadScene(self.scene) end
end

return PauseButton
