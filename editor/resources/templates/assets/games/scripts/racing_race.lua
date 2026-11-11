-- Course (AGR ENGINE) : compte à rebours, 3 tours, chronos, classement, meilleur tour sauvegardé.
-- Les voitures appellent advance() chaque image : elles doivent passer les points du circuit
-- (enfants de « Waypoints ») dans l'ordre ; revenir au premier point boucle un tour.
local G = require("assets://games/scripts/common.lua")

local Race = { properties = { laps = 3, countdown = 3, waypointRadius = 9 } }

function Race:onStart()
    self.wps = {}
    for _, w in ipairs(G.children("Waypoints")) do
        local x, _, z = w:position()
        table.insert(self.wps, { x = x, z = z })
    end
    self:restart()
end

function Race:restart()
    self.state, self.timer, self.time = "countdown", self.countdown, 0
    self.finishers = {}
    self.bestLap = agr.save.get("racing.bestLap")
    for _, name in ipairs({ "Player Car", "Rival Car" }) do
        local car = agr.find(name)
        local s = car and car:script()
        if s and s.reset then s:reset() end
    end
    G.show("Restart", false)
    self:hud()
end

-- Distance d'un point à l'axe du circuit (hors piste au-delà de la demi-largeur).
function Race:distanceToTrack(x, z)
    local best = math.huge
    local n = #self.wps
    for i = 1, n do
        local a, b = self.wps[i], self.wps[i % n + 1]
        local abx, abz = b.x - a.x, b.z - a.z
        local t = ((x - a.x) * abx + (z - a.z) * abz) / (abx * abx + abz * abz)
        t = math.max(0, math.min(1, t))
        best = math.min(best, G.distance(x, z, a.x + abx * t, a.z + abz * t))
    end
    return best
end

-- Suivi d'une voiture : c = { entity, next, lap, lapStart, finished }.
function Race:advance(c)
    if self.state ~= "racing" or c.finished then return end
    local x, _, z = c.entity:position()
    local w = self.wps[c.next]
    if G.distance(x, z, w.x, w.z) < self.waypointRadius then
        if c.next == 1 then
            c.lap = c.lap + 1
            self:lapDone(c)
        end
        c.next = c.next % #self.wps + 1
    end
end

function Race:progress(c)
    local x, _, z = c.entity:position()
    local w = self.wps[c.next]
    return c.lap * #self.wps + c.next - G.distance(x, z, w.x, w.z) / 100
end

function Race:lapDone(c)
    local lapTime = self.time - c.lapStart
    c.lapStart = self.time
    if c.player then
        if not self.bestLap or lapTime < self.bestLap then
            self.bestLap = lapTime
            agr.save.set("racing.bestLap", lapTime)
        end
        agr.audio.play("assets://audio/coin.wav")
    end
    if c.lap >= self.laps then
        c.finished = true
        table.insert(self.finishers, c)
        if c.player then
            self.state = "finished"
            local place = #self.finishers
            G.text("Message", agr.tr(place == 1 and "race.won" or "race.second", string.format("%.2f", self.time)))
            G.show("Restart", true)
        end
    end
end

function Race:hud()
    local player = agr.find("Player Car")
    local ps = player and player:script()
    local lap = ps and ps.car and math.min(self.laps, ps.car.lap + 1) or 1
    G.text("LapText", agr.tr("race.lap", lap, self.laps))
    G.text("TimeText", agr.tr("race.time", string.format("%.2f", self.time)))
    G.text("BestText", self.bestLap and agr.tr("race.best", string.format("%.2f", self.bestLap)) or "")
    if ps and ps.car then
        G.text("SpeedText", agr.tr("race.speed", math.floor(math.abs(ps.speed) * 3.6)))
        local rival = agr.find("Rival Car")
        local rs = rival and rival:script()
        local first = not (rs and rs.car) or self:progress(ps.car) >= self:progress(rs.car)
        if self.state == "racing" then G.text("PositionText", agr.tr(first and "race.first" or "race.secondpos")) end
    end
end

function Race:onUpdate(dt)
    if self.state == "countdown" then
        self.timer = self.timer - dt
        local n = math.ceil(self.timer)
        G.text("Message", n > 0 and tostring(n) or "")
        if self.timer <= 0 then
            self.state = "racing"
            G.text("Message", agr.tr("race.go"))
            self.goTime = 1
            agr.audio.play("assets://audio/launch.wav")
        end
    elseif self.state == "racing" then
        self.time = self.time + dt
        if self.goTime then
            self.goTime = self.goTime - dt
            if self.goTime <= 0 then
                self.goTime = nil
                G.text("Message", "")
            end
        end
    end
    self:hud()
end

return Race
