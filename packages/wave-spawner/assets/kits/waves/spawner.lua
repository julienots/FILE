-- Générateur de vagues (paquet officiel wave-spawner) : à mettre sur une entité vide.
-- `waves` (JSON) : [{"prefab": "assets://enemy.agrprefab", "count": 5, "interval": 0.5}, ...]
local Waves = require("assets://kits/waves/waves.lua")

local Spawner = {
    properties = { waves = "[]", radius = 4, pause = 3, waitForClear = true, autoStart = true },
    signals = { waveStarted = {"number"}, waveCleared = {"number"}, finished = {} },
}

function Spawner:onStart()
    self.plan = Waves.schedule(agr.json.decode(self.waves) or {}, self.pause)
    self.clock = 0
    self.next = 1
    self.wave = 0
    self.alive = {}
    self.running = self.autoStart
end

function Spawner:begin() self.running = true end

function Spawner:aliveCount()
    local n = 0
    for i = #self.alive, 1, -1 do
        if self.alive[i]:valid() then n = n + 1 else table.remove(self.alive, i) end
    end
    return n
end

function Spawner:onUpdate(dt)
    if not self.running then return end
    local step = self.plan[self.next]
    if not step then
        if self.wave > 0 and self:aliveCount() == 0 then
            self.entity:emit("waveCleared", self.wave)
            self.entity:emit("finished")
            self.running = false
        end
        return
    end
    -- Next wave: wait until the previous one is defeated (optional).
    if step.wave ~= self.wave and self.wave > 0 and self.waitForClear and self:aliveCount() > 0 then return end
    self.clock = self.clock + dt
    while step and self.clock >= step.time do
        if step.wave ~= self.wave then
            if self.wave > 0 then self.entity:emit("waveCleared", self.wave) end
            self.wave = step.wave
            self.entity:emit("waveStarted", self.wave)
        end
        local x, y, z = self.entity:position()
        local count = 0
        for _, s in ipairs(self.plan) do if s.wave == step.wave then count = count + 1 end end
        local px, pz = Waves.ring(step.index, count, self.radius, x, z)
        local e = agr.instantiate(step.prefab)
        if e then
            e:setPosition(px, y, pz)
            self.alive[#self.alive + 1] = e
        end
        self.next = self.next + 1
        step = self.plan[self.next]
    end
end

return Spawner
