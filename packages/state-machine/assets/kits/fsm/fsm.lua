-- Machine à états (paquet officiel state-machine). Module :
--   local FSM = require("assets://kits/fsm/fsm.lua")
--   self.fsm = FSM.new(self, { idle = { enter = f, update = f(self, dt), exit = f }, ... }, "idle")
--   self.fsm:update(dt) ; self.fsm:go("chase") ; self.fsm.state ; self.fsm.time
local FSM = {}
FSM.__index = FSM

function FSM.new(owner, states, initial)
    local m = setmetatable({ owner = owner, states = states, state = nil, time = 0, history = {} }, FSM)
    m:go(initial)
    return m
end

function FSM:go(name)
    if name == self.state then return end
    local old = self.states[self.state]
    if old and old.exit then old.exit(self.owner, name) end
    if self.state then self.history[#self.history + 1] = self.state end
    if #self.history > 16 then table.remove(self.history, 1) end
    self.state, self.time = name, 0
    local new = self.states[name]
    assert(new, "unknown state: " .. tostring(name))
    if new.enter then new.enter(self.owner) end
end

function FSM:back() if #self.history > 0 then self:go(table.remove(self.history)) end end

function FSM:update(dt)
    self.time = self.time + dt
    local s = self.states[self.state]
    if s and s.update then
        local nextState = s.update(self.owner, dt, self.time)
        if nextState then self:go(nextState) end
    end
end

return FSM
