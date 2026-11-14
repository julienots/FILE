-- Accessoires des Ruines d'Ambre (propriété kind) :
--   pot    : jarre que l'épée brise (pièce ou cœur à l'intérieur, parfois) ;
--   chest  : coffre qui s'ouvre au contact (5 pièces) ;
--   sign   : panneau qui affiche un conseil quand on s'en approche ;
--   spikes : piège à pics (rentrés, ils tremblent, puis sortent : 1 dégât) ;
--   plate  : dalle de l'énigme de la Forge (le jeu allume / éteint les dalles).
-- Rangés dans « Area N/Props » : positions du monde par worldPosition().
local G = require("assets://games/scripts/common.lua")

local Prop = { properties = { kind = "pot", text = "", phase = 0, index = 0, spacing = 70 } }

local CYCLE = { down = 1.6, warn = 0.5, up = 1.0 }

function Prop:onStart()
    self.game = agr.find("Game"):script()
    self.sprite = self.entity:find("Sprite")
    local wx = self.entity:worldPosition()
    self.area = math.floor((wx + self.spacing / 2) / self.spacing) + 1
    self:reset()
end

function Prop:reset()
    self.broken, self.opened, self.near, self.on = false, false, false, false
    self.t = self.phase
    self.state = "down"
    if self.kind == "chest" and self.sprite then self.sprite:set("SpriteRenderer", "texture", "assets://games/chest.png") end
    if self.kind == "spikes" and self.sprite then self.sprite:setScale(1, 0.12, 1) end
end

function Prop:active()
    return self.game and self.game.state == "playing" and self.game.level == self.area
end

function Prop:onUpdate(dt)
    if not self:active() then
        if self.kind == "sign" and self.near then
            self.near = false
            self.game:hideHint(self.text)
        end
        return
    end
    local hero = agr.find("Hero")
    local hx, hy, hz = hero:position()
    local x, _, z = self.entity:worldPosition()
    local d = G.distance(x, z, hx, hz)
    if self.kind == "chest" then
        if not self.opened and d < 1.5 then self:open() end
    elseif self.kind == "sign" then
        local near = d < 2.6
        if near ~= self.near then
            self.near = near
            if near then self.game:showHint(self.text) else self.game:hideHint(self.text) end
        end
    elseif self.kind == "spikes" then
        self:spikes(dt, hero, hx, hy, hz, x, z)
    elseif self.kind == "plate" then
        local on = d < 0.95 and hy < 0.5
        if on and not self.on then self.game:pressPlate(self.index) end
        self.on = on
    end
end

function Prop:spikes(dt, hero, hx, hy, hz, x, z)
    self.t = self.t + dt
    local total = CYCLE.down + CYCLE.warn + CYCLE.up
    local t = self.t % total
    local state = t < CYCLE.down and "down" or (t < CYCLE.down + CYCLE.warn and "warn" or "up")
    if state ~= self.state then
        self.state = state
        if self.sprite then
            if state == "warn" then
                agr.tween(self.sprite, { scale = { 1, 0.35, 1 } }, 0.15, { ease = "outQuad" })
            elseif state == "up" then
                agr.tween(self.sprite, { scale = { 1, 1, 1 } }, 0.08, { ease = "outBack" })
                if G.distance(x, z, hx, hz) < 8 then agr.audio.play("assets://audio/swing.wav", { volume = 0.35, pitch = 1.8 }) end
            else
                agr.tween(self.sprite, { scale = { 1, 0.12, 1 } }, 0.3, { ease = "inQuad" })
            end
        end
    end
    if state == "warn" and self.sprite then  -- tremble avant de sortir
        self.sprite:setPosition(math.sin(self.t * 60) * 0.04, 0, 0)
    elseif self.sprite then
        self.sprite:setPosition(0, 0, 0)
    end
    if state == "up" and math.abs(hx - x) < 1.0 and math.abs(hz - z) < 1.0 and hy < 0.4 then
        hero:script():damage(1, self.entity)
    end
end

-- Jarre : brisée par l'épée (ambre_hero.lua appelle hit).
function Prop:hit(n, from)
    if self.kind ~= "pot" or self.broken then return end
    self.broken = true
    local x, y, z = self.entity:worldPosition()
    agr.audio.play("assets://audio/hit.wav", { volume = 0.6, pitch = 1.4 })
    agr.fx.burst(x, y + 0.5, z, { count = 18, color = { 0.75, 0.45, 0.3 }, speed = 4, life = 0.5, size = 0.2, additive = false, gravity = -9 })
    local roll = math.random()
    if roll < 0.55 then
        self.game:addCoins(1, x, y, z)
    elseif roll < 0.75 then
        agr.find("Hero"):script():heal(1)
        agr.fx.popup(x, y + 1.2, z, "+1", { color = { 1, 0.5, 0.55 }, size = 44 })
    end
    self.entity:setActive(false)
end

function Prop:open()
    self.opened = true
    local x, y, z = self.entity:worldPosition()
    if self.sprite then
        self.sprite:set("SpriteRenderer", "texture", "assets://games/chest_open.png")
        agr.tween(self.sprite, { scale = { 1, 1, 1 } }, 0.4, { from = { 1.3, 0.7, 1 }, ease = "outElastic" })
    end
    agr.audio.play("assets://audio/gem.wav", { volume = 0.7, pitch = 1.1 })
    agr.fx.burst(x, y + 0.8, z, { count = 40, color = { 1, 0.85, 0.3 }, speed = 5, life = 0.9, size = 0.3, spread = "up",
                                  texture = "assets://games/sparkle.png" })
    self.game:addCoins(5, x, y + 0.6, z)
end

-- Dalle : allumée ou éteinte (énigme).
function Prop:setLit(lit)
    if self.sprite then self.sprite:set("SpriteRenderer", "color", lit and { 1, 0.75, 0.3, 1 } or { 0.5, 0.45, 0.4, 1 }) end
    self.entity:set("PointLight", "intensity", lit and 4 or 0)
end

return Prop
