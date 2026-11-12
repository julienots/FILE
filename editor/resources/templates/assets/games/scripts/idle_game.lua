-- Jeu idle (AGR ENGINE) : toucher le cristal rapporte des cristaux ; les mineurs, foreuses et usines
-- en produisent seuls (prix x1,15 à chaque achat) ; la pioche double la récolte au toucher.
-- La partie est sauvegardée régulièrement ; au retour, la production pendant l'absence est
-- ajoutée (8 h au plus). La renaissance remet tout à zéro contre un bonus permanent de +25 %.
local G = require("assets://games/scripts/common.lua")

local GENERATORS = {
    BuyMiner = { id = "miner", base = 15, rate = 0.5 },
    BuyDrill = { id = "drill", base = 120, rate = 4 },
    BuyFactory = { id = "factory", base = 1000, rate = 30 },
}

local Idle = { properties = { growth = 1.15, pickaxeBase = 50, prestigeAt = 100000, offlineCap = 28800, crystalRadius = 1.6 } }

function Idle:onStart()
    self:load()
    self.saveTimer, self.messageTime = 5, 0
    if self.offline and self.offline > 0 then
        self:say(agr.tr("idle.offline", G.short(self.offline)), 5)
    end
    self:refresh()
end

function Idle:load()
    local s = agr.save.get("idle", {})
    self.crystals = s.crystals or 0
    self.owned = s.owned or { miner = 0, drill = 0, factory = 0 }
    self.pickaxe = s.pickaxe or 0
    self.prestige = s.prestige or 0
    self.offline = 0
    if s.time then  -- production pendant l'absence
        local away = math.min(self.offlineCap, math.max(0, agr.time.now() - s.time))
        self.offline = self:rate() * away
        self.crystals = self.crystals + self.offline
    end
end

function Idle:save()
    agr.save.set("idle", { crystals = self.crystals, owned = self.owned, pickaxe = self.pickaxe, prestige = self.prestige,
                           time = agr.time.now() })
end

function Idle:multiplier() return 1 + 0.25 * self.prestige end
function Idle:rate()
    local r = 0
    for _, g in pairs(GENERATORS) do r = r + g.rate * (self.owned[g.id] or 0) end
    return r * self:multiplier()
end
function Idle:tapValue() return (2 ^ self.pickaxe) * self:multiplier() end
function Idle:cost(g) return math.floor(g.base * self.growth ^ (self.owned[g.id] or 0)) end
function Idle:pickaxeCost() return math.floor(self.pickaxeBase * 3 ^ self.pickaxe) end

function Idle:say(text, seconds)
    G.text("Message", text)
    self.messageTime = seconds or 2.5
end

function Idle:refresh()
    G.text("CrystalsText", agr.tr("idle.crystals", G.short(self.crystals)))
    G.text("RateText", agr.tr("idle.rate", G.short(self:rate()), G.short(self:tapValue())))
    for name, g in pairs(GENERATORS) do
        local b = agr.find(name)
        if b then
            b:find("Title"):setText(agr.tr("idle.gen." .. g.id, self.owned[g.id] or 0))
            b:find("Info"):setText(agr.tr("idle.gen.info", G.short(self:cost(g)), g.rate * self:multiplier()))
            b:set("UIImage", "color", self.crystals >= self:cost(g) and { 0.2, 0.32, 0.22, 0.9 } or { 0.16, 0.18, 0.24, 0.85 })
        end
    end
    local p = agr.find("BuyPickaxe")
    if p then
        p:find("Title"):setText(agr.tr("idle.pickaxe", self.pickaxe + 1))
        p:find("Info"):setText(agr.tr("idle.pickaxe.info", G.short(self:pickaxeCost())))
        p:set("UIImage", "color", self.crystals >= self:pickaxeCost() and { 0.2, 0.32, 0.22, 0.9 } or { 0.16, 0.18, 0.24, 0.85 })
    end
    local r = agr.find("Prestige")
    if r then
        r:find("Title"):setText(agr.tr("idle.prestige", self.prestige))
        r:find("Info"):setText(agr.tr("idle.prestige.info", G.short(self.prestigeAt)))
        r:set("UIImage", "color", self.crystals >= self.prestigeAt and { 0.4, 0.25, 0.5, 0.9 } or { 0.16, 0.18, 0.24, 0.85 })
    end
end

-- Message des boutons de la boutique.
function Idle:buy(button)
    local name = button:name()
    local g = GENERATORS[name]
    if g then
        local c = self:cost(g)
        if self.crystals < c then return self:say(agr.tr("idle.notenough")) end
        self.crystals = self.crystals - c
        self.owned[g.id] = (self.owned[g.id] or 0) + 1
    elseif name == "BuyPickaxe" then
        local c = self:pickaxeCost()
        if self.crystals < c then return self:say(agr.tr("idle.notenough")) end
        self.crystals = self.crystals - c
        self.pickaxe = self.pickaxe + 1
    elseif name == "Prestige" then
        if self.crystals < self.prestigeAt then return self:say(agr.tr("idle.prestige.locked", G.short(self.prestigeAt))) end
        self.prestige = self.prestige + 1
        self.crystals, self.pickaxe = 0, 0
        self.owned = { miner = 0, drill = 0, factory = 0 }
        self:say(agr.tr("idle.reborn", math.floor(self:multiplier() * 100)), 4)
    end
    agr.audio.play("assets://audio/coin.wav", { volume = 0.6 })
    self:save()
    self:refresh()
end

function Idle:tap()
    self.crystals = self.crystals + self:tapValue()
    local sparks = agr.find("Sparks")
    if sparks then
        sparks:set("ParticleEmitter2D", "emitting", true)
    end
    self.pulse = 0.15
    agr.audio.play("assets://audio/click.wav", { volume = 0.5, pitch = 1 + math.random() * 0.3 })
    local c = agr.find("Crystal")
    if c then
        local x, y = c:position()
        agr.fx.popup(x + (math.random() - 0.5) * 1.5, y + 1, 0, "+" .. G.short(self:tapValue()), { color = { 0.8, 0.7, 1 }, size = 46, duration = 0.7 })
    end
end

function Idle:onUpdate(dt)
    self.crystals = self.crystals + self:rate() * dt
    local x, y = G.tappedWorld()
    if x then
        local c = agr.find("Crystal")
        local cx, cy = c:position()
        if G.distance(x, y, cx, cy) < self.crystalRadius then self:tap() end
    end
    if agr.input.pressed("jump") then self:tap() end
    self.pulse = math.max(0, (self.pulse or 0) - dt)
    local c = agr.find("Crystal")
    if c then
        local s = 1 + self.pulse * 0.8 + math.sin(agr.time.total() * 2) * 0.02
        c:setScale(s, s, 1)
    end
    self.messageTime = self.messageTime - dt
    if self.messageTime <= 0 then G.text("Message", agr.tr("idle.hint")) end
    self.saveTimer = self.saveTimer - dt
    if self.saveTimer <= 0 then
        self.saveTimer = 5
        self:save()
    end
    self:refresh()
end

function Idle:onDestroy() self:save() end

return Idle
