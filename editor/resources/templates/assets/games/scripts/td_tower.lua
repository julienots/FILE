-- Tour : vise la créature la plus avancée à portée et tire des projectiles (sprites copiés du
-- prototype « Bullet Prototype », déplacés par ce script). Trois niveaux.
local G = require("assets://games/scripts/common.lua")

local Tower = { properties = { range = 3.4, rate = 1.5, damage = 1, bulletSpeed = 12 } }

function Tower:onStart()
    self.game = agr.find("Game"):script()
    self.level, self.timer, self.bullets = 1, 0, {}
end

function Tower:upgrade()
    self.level = self.level + 1
    self.damage = self.damage + 1
    self.range = self.range + 0.5
    self.rate = self.rate + 0.4
    local s = 1 + (self.level - 1) * 0.15
    self.entity:setScale(s, s, 1)
    self.entity:set("SpriteRenderer", "color", self.level == 2 and { 0.8, 1, 0.8, 1 } or { 1, 0.85, 0.5, 1 })
end

function Tower:onDestroy()
    for _, b in ipairs(self.bullets) do if b.e:isValid() then b.e:destroy() end end
end

function Tower:onUpdate(dt)
    local x, y = self.entity:position()
    -- Projectiles en vol.
    for i = #self.bullets, 1, -1 do
        local b = self.bullets[i]
        local bx, by = b.e:position()
        local tx, ty = b.tx, b.ty
        if b.target:isValid() then tx, ty = b.target:position() end
        local d = G.distance(bx, by, tx, ty)
        local step = self.bulletSpeed * dt
        if d <= step + 0.15 then
            if b.target:isValid() then
                local s = b.target:script()
                if s then s:hit(b.damage) end
            end
            b.e:destroy()
            table.remove(self.bullets, i)
        else
            b.e:setPosition(bx + (tx - bx) / d * step, by + (ty - by) / d * step, 0)
            b.tx, b.ty = tx, ty
        end
    end
    if self.game.state ~= "playing" then return end
    self.timer = self.timer - dt
    if self.timer > 0 then return end
    local best, bestProgress = nil, -1
    for _, c in ipairs(self.game.creeps) do
        if c:isValid() then
            local cx, cy = c:position()
            local s = c:script()
            if s and G.distance(x, y, cx, cy) <= self.range and s:progress() > bestProgress then
                best, bestProgress = c, s:progress()
            end
        end
    end
    if not best then return end
    self.timer = 1 / self.rate
    local cx, cy = best:position()
    self.entity:setRotation(0, 0, math.deg(math.atan(cy - y, cx - x)) - 90)
    local bullet = agr.clone(agr.find("Bullet Prototype"), false)
    bullet:setName("Bullet")
    bullet:setPosition(x, y + 0.3, 0)
    table.insert(self.bullets, { e = bullet, target = best, damage = self.damage, tx = cx, ty = cy })
    agr.audio.play("assets://audio/click.wav", { volume = 0.25, pitch = 1.6 })
end

return Tower
