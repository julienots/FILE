-- Exemple (paquet officiel state-machine) : un garde avec NavAgent qui patrouille entre deux points,
-- poursuit le joueur proche et retourne à son poste.
local FSM = require("assets://kits/fsm/fsm.lua")

local Guard = { properties = { target = "Player", sight = 6, ax = -4, az = 0, bx = 4, bz = 0 } }

local function dist(a, b)
    local ax, ay, az = a:worldPosition()
    local bx, by, bz = b:worldPosition()
    return math.sqrt((ax - bx) ^ 2 + (ay - by) ^ 2 + (az - bz) ^ 2)
end

function Guard:onStart()
    self.player = agr.find(self.target)
    self.toB = true
    self.fsm = FSM.new(self, {
        patrol = {
            enter = function(g) g.entity:navigateTo(g.toB and g.bx or g.ax, 0, g.toB and g.bz or g.az) end,
            update = function(g)
                if g.player and dist(g.entity, g.player) < g.sight then return "chase" end
                if g.entity:navigationStatus() == "arrived" then
                    g.toB = not g.toB
                    g.entity:navigateTo(g.toB and g.bx or g.ax, 0, g.toB and g.bz or g.az)
                end
            end,
        },
        chase = {
            update = function(g, _, time)
                if not (g.player and g.player:isValid()) then return "patrol" end
                if time % 0.5 < 0.02 then
                    local x, y, z = g.player:worldPosition()
                    g.entity:navigateTo(x, y, z)
                end
                if dist(g.entity, g.player) > g.sight * 1.5 then return "patrol" end
            end,
            exit = function(g) g.entity:stopNavigation() end,
        },
    }, "patrol")
end

function Guard:onUpdate(dt) self.fsm:update(dt) end

return Guard
