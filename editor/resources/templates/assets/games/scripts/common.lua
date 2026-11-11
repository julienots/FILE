-- Outils partagés par les modèles de jeux (AGR ENGINE). Utilisation :
--     local G = require("assets://games/scripts/common.lua")
local G = {}

-- Texte d'un élément d'interface (ignoré s'il n'existe pas).
function G.text(name, value)
    local e = agr.find(name)
    if e then e:setText(value) end
end

-- Affiche / cache une entité par son nom.
function G.show(name, visible)
    local e = agr.find(name)
    if e then e:setActive(visible) end
end

function G.clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end

function G.distance(ax, ay, bx, by) return math.sqrt((ax - bx) ^ 2 + (ay - by) ^ 2) end

function G.distance3(ax, ay, az, bx, by, bz) return math.sqrt((ax - bx) ^ 2 + (ay - by) ^ 2 + (az - bz) ^ 2) end

-- Point de l'écran tapé ou cliqué à cette image (hors interface), ou nil.
function G.tapped()
    if agr.ui.pointerOverUI() then return nil end
    if agr.input.mousePressed(0) then return agr.input.mousePosition() end
    for i = 1, agr.input.touchCount() do
        local x, y, phase = agr.input.touch(i)
        if phase == "began" then return x, y end
    end
    return nil
end

-- Point du monde du jeu sous le clic / toucher de cette image (sol ou plan 2D), ou nil.
function G.tappedWorld(height)
    local sx, sy = G.tapped()
    if not sx then return nil end
    return agr.camera.screenToWorld(sx, sy, height)
end

-- Enfants actifs / tous les enfants d'une entité nommée.
function G.children(name)
    local e = agr.find(name)
    return e and e:children() or {}
end

-- Positions de départ des enfants d'une entité nommée (ennemis, objets), par nom.
function G.homes(parentName)
    local t = {}
    for _, e in ipairs(G.children(parentName)) do
        local x, y, z = e:position()
        t[e:name()] = { x, y, z }
    end
    return t
end

-- Remet une entité à sa place et l'active. Une entité désactivée a perdu l'instance de son script :
-- la réactiver en crée une nouvelle (onStart) ; une entité encore active est remise à zéro par reset().
function G.respawn(e, home)
    local s = e:isActive() and e:script()
    if home then e:setPosition(home[1], home[2], home[3]) end
    e:setActive(true)
    if s and s.reset then s:reset() end
end

-- Meilleur score mémorisé dans la sauvegarde locale ; renvoie le record (et s'il est nouveau).
function G.record(key, value, higherIsBetter)
    local best = agr.save.get(key)
    local better = best == nil or (higherIsBetter ~= false and value > best) or (higherIsBetter == false and value < best)
    if better then
        agr.save.set(key, value)
        return value, true
    end
    return best, false
end

-- Nombre lisible : 1234567 -> "1,23 M".
function G.short(n)
    local units = { "", " k", " M", " G", " T", " P" }
    local i = 1
    while math.abs(n) >= 1000 and i < #units do
        n = n / 1000
        i = i + 1
    end
    if i == 1 then
        if n == math.floor(n) or math.abs(n) >= 100 then return tostring(math.floor(n)) end
        return string.format("%.1f", n)
    end
    return string.format("%.2f%s", n, units[i])
end

return G
