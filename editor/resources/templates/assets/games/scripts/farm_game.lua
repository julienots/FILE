-- Simulation de ferme (AGR ENGINE).
-- Outils (boutons ou touches 1, 2, 3) : semer (coûte des pièces), arroser, récolter ; puis toucher /
-- cliquer une parcelle. Une plante pousse tant que la terre est humide (4 stades) ; trop longtemps
-- au sec, elle fane. La pluie arrose tout. Le prix de vente change chaque jour.
local G = require("assets://games/scripts/common.lua")

local Farm = {
    properties = { coins = 20, seedCost = 2, goal = 200, dayLength = 60, growTime = 8, moisture = 25, witherTime = 40, plotSize = 0.6 },
}

local TOOLS = { ToolSeed = "seed", ToolWater = "water", ToolHarvest = "harvest" }

function Farm:onStart()
    self.plots = {}
    for _, p in ipairs(G.children("Field")) do
        self.plots[#self.plots + 1] = { e = p, crop = p:find("Crop") }
    end
    self:restart()
end

function Farm:restart()
    self.coinsNow = self.coins
    self.day, self.clock = 1, 0
    self.state = "playing"
    self.tool = "seed"
    for _, p in ipairs(self.plots) do
        p.stage, p.growth, p.water, p.dry = nil, 0, 0, 0
        p.crop:setActive(false)
        p.e:set("SpriteRenderer", "frame", 9)
    end
    self:newDay()
    G.show("Restart", false)
    G.text("Message", agr.tr("farm.hint"))
    self.messageTime = 6
    self:highlightTool()
    self:hud()
end

function Farm:newDay()
    self.rainy = self.day > 1 and math.random() < 0.3
    self.price = 4 + math.random(0, 4)
    local rain = agr.find("Rain")
    if rain then rain:set("ParticleEmitter2D", "emitting", self.rainy) end
end

function Farm:hud()
    G.text("CoinsText", agr.tr("farm.coins", self.coinsNow))
    G.text("DayText", agr.tr(self.rainy and "farm.day.rain" or "farm.day.sun", self.day))
    G.text("MarketText", agr.tr("farm.market", self.price))
    G.text("Goal", agr.tr("farm.goal", self.goal))
end

function Farm:selectTool(button)
    self.tool = TOOLS[button:name()] or self.tool
    self:highlightTool()
end

function Farm:highlightTool()
    for name, tool in pairs(TOOLS) do
        local b = agr.find(name)
        if b then b:set("UIImage", "color", tool == self.tool and { 0.85, 0.55, 0.15, 0.95 } or { 0.16, 0.18, 0.24, 0.85 }) end
    end
end

function Farm:plotAt(x, y)
    for _, p in ipairs(self.plots) do
        local px, py = p.e:position()
        if math.abs(px - x) < self.plotSize and math.abs(py - y) < self.plotSize then return p end
    end
end

function Farm:setStage(p, stage)
    p.stage = stage
    if stage == nil then
        p.crop:setActive(false)
        return
    end
    p.crop:setActive(true)
    p.crop:set("SpriteRenderer", "texture", "assets://games/crop_" .. stage .. ".png")
    p.crop:set("SpriteRenderer", "color", { 1, 1, 1, 1 })
end

function Farm:use(p)
    if self.tool == "seed" then
        if p.stage ~= nil then return end
        if self.coinsNow < self.seedCost then
            self:say(agr.tr("farm.nocoins"))
            return
        end
        self.coinsNow = self.coinsNow - self.seedCost
        p.growth, p.dry = 0, 0
        self:setStage(p, 0)
        agr.audio.play("assets://audio/click.wav")
    elseif self.tool == "water" then
        p.water = self.moisture
        p.e:set("SpriteRenderer", "frame", 10)
        agr.audio.play("assets://audio/click.wav", { pitch = 0.7 })
    elseif self.tool == "harvest" then
        if p.stage == 3 then
            self.coinsNow = self.coinsNow + self.price
            self:setStage(p, nil)
            agr.audio.play("assets://audio/coin.wav")
            if self.coinsNow >= self.goal then
                self.state = "won"
                local best = G.record("farm.bestDays", self.day, false)
                G.text("Message", agr.tr("farm.won", self.day, best))
                G.show("Restart", true)
            end
        elseif p.stage == "withered" then
            self:setStage(p, nil)
        end
    end
    self:hud()
end

function Farm:say(text)
    G.text("Message", text)
    self.messageTime = 2.5
end

function Farm:onUpdate(dt)
    self.messageTime = (self.messageTime or 0) - dt
    if self.messageTime <= 0 and self.state == "playing" then G.text("Message", "") end
    if self.state ~= "playing" then return end
    -- Jour / nuit : le voile « Night » s'assombrit en fin de journée.
    self.clock = self.clock + dt
    if self.clock >= self.dayLength then
        self.clock = 0
        self.day = self.day + 1
        self:newDay()
        self:hud()
    end
    local t = self.clock / self.dayLength
    local dark = math.max(0, math.sin((t - 0.6) / 0.4 * math.pi)) * 0.45
    local night = agr.find("Night")
    if night then night:set("SpriteRenderer", "color", { 0.02, 0.03, 0.12, dark }) end

    for _, p in ipairs(self.plots) do
        if self.rainy then p.water = self.moisture end
        if p.water > 0 then
            p.water = p.water - dt
            if p.water <= 0 then p.e:set("SpriteRenderer", "frame", 9) end
        end
        if self.rainy then p.e:set("SpriteRenderer", "frame", 10) end
        if type(p.stage) == "number" and p.stage < 3 then
            if p.water > 0 then
                p.dry = 0
                p.growth = p.growth + dt
                if p.growth >= self.growTime then
                    p.growth = 0
                    self:setStage(p, p.stage + 1)
                end
            else
                p.dry = p.dry + dt
                if p.dry >= self.witherTime and p.stage > 0 then  -- fanée : à arracher (outil récolte)
                    p.stage = "withered"
                    p.crop:set("SpriteRenderer", "color", { 0.55, 0.45, 0.3, 1 })
                end
            end
        end
    end

    for i, name in ipairs({ "ToolSeed", "ToolWater", "ToolHarvest" }) do
        if agr.input.keyPressed(tostring(i)) then
            self.tool = TOOLS[name]
            self:highlightTool()
        end
    end
    local x, y = G.tappedWorld()
    if x then
        local p = self:plotAt(x, y)
        if p then self:use(p) end
    end
end

return Farm
