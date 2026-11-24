-- Tweens (paquet officiel tween) : local Tween = require("assets://kits/tween/tween.lua")
--   local t = Tween.to(obj, { x = 10, alpha = 0 }, 0.5, "outBack")   -- anime obj.x et obj.alpha
--   t:delay(0.2):yoyo(2):onDone(function() ... end)
--   Tween.update(dt)  -- une fois par image (ou t:step(dt) pour un tween seul)
local Tween = {}
Tween.__index = Tween
local active = {}

local pi, sin, cos, sqrt = math.pi, math.sin, math.cos, math.sqrt
local function outBounce(t)
    if t < 1 / 2.75 then return 7.5625 * t * t
    elseif t < 2 / 2.75 then t = t - 1.5 / 2.75 return 7.5625 * t * t + 0.75
    elseif t < 2.5 / 2.75 then t = t - 2.25 / 2.75 return 7.5625 * t * t + 0.9375 end
    t = t - 2.625 / 2.75
    return 7.5625 * t * t + 0.984375
end
Tween.easings = {
    linear = function(t) return t end,
    inQuad = function(t) return t * t end,
    outQuad = function(t) return t * (2 - t) end,
    inOutQuad = function(t) return t < 0.5 and 2 * t * t or -1 + (4 - 2 * t) * t end,
    inCubic = function(t) return t ^ 3 end,
    outCubic = function(t) return (t - 1) ^ 3 + 1 end,
    inOutCubic = function(t) return t < 0.5 and 4 * t ^ 3 or (t - 1) * (2 * t - 2) ^ 2 + 1 end,
    inQuart = function(t) return t ^ 4 end,
    outQuart = function(t) return 1 - (t - 1) ^ 4 end,
    inSine = function(t) return 1 - cos(t * pi / 2) end,
    outSine = function(t) return sin(t * pi / 2) end,
    inOutSine = function(t) return -(cos(pi * t) - 1) / 2 end,
    inExpo = function(t) return t == 0 and 0 or 2 ^ (10 * t - 10) end,
    outExpo = function(t) return t == 1 and 1 or 1 - 2 ^ (-10 * t) end,
    inCirc = function(t) return 1 - sqrt(1 - t * t) end,
    outCirc = function(t) return sqrt(1 - (t - 1) ^ 2) end,
    inBack = function(t) return 2.70158 * t ^ 3 - 1.70158 * t * t end,
    outBack = function(t) return 1 + 2.70158 * (t - 1) ^ 3 + 1.70158 * (t - 1) ^ 2 end,
    outElastic = function(t)
        if t == 0 or t == 1 then return t end
        return 2 ^ (-10 * t) * sin((t * 10 - 0.75) * (2 * pi / 3)) + 1
    end,
    outBounce = outBounce,
    inBounce = function(t) return 1 - outBounce(1 - t) end,
}

--- Animates the numeric fields of `target` toward `goal` in `duration` seconds.
function Tween.to(target, goal, duration, easing)
    local t = setmetatable({ target = target, goal = goal, duration = math.max(duration or 0.3, 1e-4), time = 0,
                             wait = 0, ease = Tween.easings[easing or "outQuad"] or Tween.easings.linear,
                             from = nil, cycles = 1, done = false, callbacks = {} }, Tween)
    active[#active + 1] = t
    return t
end
--- A tween of a single value: fn(value) is called every update.
function Tween.value(from, to, duration, easing, fn)
    local box = { v = from }
    local t = Tween.to(box, { v = to }, duration, easing)
    t.onValue = fn
    return t
end
function Tween:delay(seconds) self.wait = seconds or 0 return self end
--- Goes back and forth `count` times (each way counts).
function Tween:yoyo(count) self.cycles = math.max(1, count or 2) self.yoyoing = true return self end
function Tween:onDone(fn) self.callbacks[#self.callbacks + 1] = fn return self end
function Tween:cancel() self.done = true end

function Tween:step(dt)
    if self.done then return false end
    if self.wait > 0 then
        self.wait = self.wait - dt
        if self.wait > 0 then return true end
        dt = -self.wait
        self.wait = 0
    end
    if not self.from then
        self.from = {}
        for k in pairs(self.goal) do self.from[k] = self.target[k] or 0 end
    end
    self.time = self.time + dt
    local p = math.min(self.time / self.duration, 1)
    local e = self.ease(p)
    for k, to in pairs(self.goal) do self.target[k] = self.from[k] + (to - self.from[k]) * e end
    if self.onValue then self.onValue(self.target.v) end
    if p >= 1 then
        self.cycles = self.cycles - 1
        if self.cycles > 0 and self.yoyoing then
            self.from, self.goal = self.goal, self.from
            self.time = 0
            local g = {}
            for k, v in pairs(self.goal) do g[k] = v end
            self.goal = g
            return true
        end
        self.done = true
        for _, fn in ipairs(self.callbacks) do fn(self.target) end
        return false
    end
    return true
end

--- Updates every running tween (call once per frame).
function Tween.update(dt)
    local keep = {}
    for _, t in ipairs(active) do
        if t:step(dt) then keep[#keep + 1] = t end
    end
    active = keep
end
function Tween.count() return #active end
function Tween.clear() active = {} end

return Tween
