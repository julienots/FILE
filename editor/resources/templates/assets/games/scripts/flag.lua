-- Drapeau (déclencheur) : point de passage ou arrivée ; appelle reach(kind, x, y) sur le joueur.
local Flag = { properties = { kind = "goal", collector = "Hero" } }

function Flag:onTrigger(other, began)
    if not began or other:name() ~= self.collector then return end
    local s = other:script()
    local x, y = self.entity:position()
    if s and s.reach then s:reach(self.kind, x, y) end
end

return Flag
