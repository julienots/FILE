-- Menu de lancement : chaque bouton « Game N » ouvre la scène correspondante (propriété scenes,
-- liste séparée par des « ; »). Retour / Échap dans un jeu ramène ici (player).
local Launcher = { properties = { scenes = "" } }

function Launcher:onStart()
    self.list = {}
    for uri in string.gmatch(self.scenes, "[^;]+") do table.insert(self.list, uri) end
end

-- Message des boutons.
function Launcher:open(button)
    local i = tonumber(button:name():match("(%d+)$"))
    if i and self.list[i] then
        agr.audio.play("assets://audio/click.wav")
        agr.loadScene(self.list[i])
    end
end

return Launcher
