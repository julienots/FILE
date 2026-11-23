-- Cycle jour / nuit (paquet officiel day-night). Sur l'entité de la lumière directionnelle (le soleil) ;
-- `environment` nomme l'entité qui porte le composant Environment (ciel, ambiance).
local DN = {
    properties = { dayMinutes = 4, startHour = 8, environment = "Environment", nightSky = 0.08 },
    signals = { dawn = {}, dusk = {}, hour = {"number"} },
}

local function lerp(a, b, t) return a + (b - a) * t end
local function mix(c1, c2, t) return { r = lerp(c1[1], c2[1], t), g = lerp(c1[2], c2[2], t), b = lerp(c1[3], c2[3], t) } end

function DN:onStart()
    self.hourValue = self.startHour
    self.env = agr.find(self.environment)
    self.lastHour = math.floor(self.hourValue)
end

function DN:hour() return self.hourValue end
function DN:setHour(h) self.hourValue = h % 24 end

function DN:onUpdate(dt)
    local before = self.hourValue
    self.hourValue = (self.hourValue + dt * 24 / (self.dayMinutes * 60)) % 24
    local h = self.hourValue
    if before < 6 and h >= 6 then self.entity:emit("dawn") end
    if before < 19 and h >= 19 then self.entity:emit("dusk") end
    if math.floor(h) ~= self.lastHour then
        self.lastHour = math.floor(h)
        self.entity:emit("hour", self.lastHour)
    end
    -- Soleil : lever à 6 h à l'est, zénith à 12 h, coucher à 18 h.
    local elevation = math.sin((h - 6) / 12 * math.pi)  -- 1 à midi, < 0 la nuit
    self.entity:setRotation(-math.deg(math.asin(math.max(-1, math.min(1, elevation)))), (h / 24) * 360 - 90, 0)
    local day = math.max(0, elevation)
    local warm = 1 - math.min(1, day * 3)  -- aube et crépuscule
    local color = mix({1.0, 1.0, 0.96}, {1.0, 0.55, 0.3}, warm)
    if elevation < 0 then color = { r = 0.45, g = 0.55, b = 0.9 } end
    self.entity:set("DirectionalLight", "color", color)
    self.entity:set("DirectionalLight", "intensity", elevation < 0 and 0.15 or lerp(0.4, 3.0, day))
    if self.env and self.env:isValid() then
        self.env:set("Environment", "skyIntensity", math.max(self.nightSky, lerp(0.25, 1.0, day)))
    end
end

return DN
