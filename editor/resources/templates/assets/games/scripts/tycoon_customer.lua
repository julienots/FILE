-- Client : marche dans la rue jusqu'à une boutique, fait la queue, paie puis s'en va ; repart
-- mécontent si l'attente dépasse sa patience.
local Customer = { properties = { speed = 2.6 } }

function Customer:onStart()
    self.game = agr.find("Game"):script()
    self.game:addCustomer(self.entity)
    self.shop = self.game:pickShop()
    self.state = "walking"
    self.wait = 0
    self.t = math.random() * 6
    if not self.shop then return self:leave() end
    local px, py = self.shop.plot:position()
    local slot = #self.shop.queue  -- place dans la file (devant la boutique, en descendant vers la rue)
    self.route = { { px, self.game.streetY }, { px + 0.3 * (slot % 2 == 0 and 1 or -1), py - 1.3 - slot * 0.35 } }
end

function Customer:leave()
    self.state = "leaving"
    local x = self.entity:position()
    self.route = { { x, self.game.streetY }, { 16, self.game.streetY } }
end

function Customer:served()
    self.entity:set("SpriteRenderer", "color", { 0.8, 1, 0.8, 1 })
    self:leave()
end

function Customer:onUpdate(dt)
    if self.game.state ~= "playing" then return end
    self.t = self.t + dt * 8
    if self.state == "waiting" then
        self.wait = self.wait + dt
        if self.wait > self.game.patience then
            self.entity:set("SpriteRenderer", "color", { 1, 0.6, 0.6, 1 })
            self.game:giveUp(self.entity, self.shop)
            self:leave()
        end
        return
    end
    local target = self.route[1]
    if not target then
        if self.state == "walking" then
            self.state = "waiting"
            self.game:arrive(self.entity, self.shop)
        else
            self.game:removeCustomer(self.entity)
            self.entity:destroy()
        end
        return
    end
    local x, y = self.entity:position()
    local dx, dy = target[1] - x, target[2] - y
    local d = math.sqrt(dx * dx + dy * dy)
    local step = self.speed * dt
    if d <= step then
        self.entity:setPosition(target[1], target[2], 0)
        table.remove(self.route, 1)
    else
        self.entity:setPosition(x + dx / d * step, y + dy / d * step + math.sin(self.t) * 0.01, 0)
        if math.abs(dx) > 0.01 then self.entity:set("SpriteRenderer", "flipX", dx < 0) end
    end
end

return Customer
