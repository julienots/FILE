-- Calcul du joystick virtuel (paquet officiel mobile-joystick), sans dépendance : testable.
local Stick = {}

-- Centre (cx, cy), doigt (tx, ty), rayon et zone morte (0..1) -> x, y dans -1..1 (y vers le haut).
function Stick.compute(cx, cy, tx, ty, radius, deadZone)
    local dx, dy = tx - cx, cy - ty  -- écran : y vers le bas
    local d = math.sqrt(dx * dx + dy * dy)
    if d < 1e-6 then return 0, 0 end
    local m = math.min(d / radius, 1)
    if m <= (deadZone or 0) then return 0, 0 end
    local k = (m - (deadZone or 0)) / (1 - (deadZone or 0)) / d * 1
    return dx * k, dy * k
end

return Stick
