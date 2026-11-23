-- Joystick virtuel (paquet officiel mobile-joystick). Sur une entité vide ; les autres scripts
-- lisent la direction : local x, y = agr.find("Joystick"):script():value()
local Stick = require("assets://kits/joystick/stick.lua")

local Joystick = {
    properties = { radius = 90, deadZone = 0.15, leftHalfOnly = true },
}

function Joystick:onStart()
    self.x, self.y = 0, 0
    self.finger = nil
    self.cx, self.cy = 0, 0
end

function Joystick:value() return self.x, self.y end
function Joystick:active() return self.finger ~= nil end

function Joystick:onUpdate()
    local w = agr.screen.size()
    local found = false
    for i = 1, agr.input.touchCount() do
        local tx, ty, phase = agr.input.touch(i)
        if self.finger == nil and phase == "began" and (not self.leftHalfOnly or tx < w * 0.5) and not agr.ui.pointerOverUI() then
            self.finger, self.cx, self.cy = i, tx, ty
        end
        if self.finger == i then
            found = phase ~= "ended"
            self.x, self.y = Stick.compute(self.cx, self.cy, tx, ty, self.radius, self.deadZone)
        end
    end
    if not found then
        self.finger = nil
        self.x, self.y = 0, 0
    end
end

return Joystick
