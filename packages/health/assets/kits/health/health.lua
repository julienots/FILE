-- Points de vie (paquet officiel health). Autres scripts : target:script():damage(10, source)
local Health = {
    properties = { max = 100, invulnerableTime = 0.4, destroyOnDeath = true },
    signals = { damaged = {"number", "entity?"}, healed = {"number"}, died = {"entity?"} },
}

function Health:onStart()
    self.hp = self.max
    self.invulnerable = 0
    self.dead = false
end

function Health:onUpdate(dt)
    if self.invulnerable > 0 then self.invulnerable = self.invulnerable - dt end
end

function Health:damage(amount, source)
    if self.dead or self.invulnerable > 0 or amount <= 0 then return false end
    self.hp = math.max(0, self.hp - amount)
    self.invulnerable = self.invulnerableTime
    self.entity:emit("damaged", amount, source)
    if self.hp == 0 then
        self.dead = true
        self.entity:emit("died", source)
        if self.destroyOnDeath then self.entity:destroy() else self.entity:setActive(false) end
    end
    return true
end

function Health:heal(amount)
    if self.dead or amount <= 0 then return end
    local before = self.hp
    self.hp = math.min(self.max, self.hp + amount)
    if self.hp > before then self.entity:emit("healed", self.hp - before) end
end

function Health:fraction() return self.hp / self.max end

return Health
