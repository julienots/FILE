-- Arène multijoueur (AGR ENGINE) : ramasser la pièce, le premier à 10 gagne.
--  * « 2 joueurs (local) » : joueur 1 ZQSD / WASD ou manette 1, joueur 2 flèches ou manette 2.
--  * « Héberger » : cette machine est l'hôte (port 7777) ; elle simule la partie et l'envoie
--    aux autres 20 fois par seconde. « Rejoindre » : se connecte à l'adresse enregistrée dans la
--    sauvegarde (clé « arena.address », 127.0.0.1 par défaut) ; envoie seulement ses commandes.
-- Jusqu'à 4 joueurs en réseau. Voir docs/NETWORK.md.
local G = require("assets://games/scripts/common.lua")

local Arena = { properties = { speed = 6, port = 7777, target = 10, halfW = 9.6, halfH = 4.6, radius = 0.55, sendRate = 20 } }

function Arena:onStart()
    self.mode = "local"
    self:reset()
    self:status(agr.tr("arena.status.local"))
end

function Arena:reset()
    self.players = {}
    for i = 1, 4 do
        local e = agr.find("Player " .. i)
        local x, y = e:position()
        self.players[i] = { e = e, x = x, y = y, sx = x, sy = y, score = 0, inX = 0, inY = 0,
                            active = i <= 2 and self.mode == "local" or i == 1 }
        e:setActive(self.players[i].active)
    end
    if self.mode == "client" then for i = 1, 4 do self.players[i].active = false; self.players[i].e:setActive(false) end end
    self.winner = nil
    self:moveCoin()
    self.sendTimer = 0
    G.text("Message", "")
    self:hud()
end

function Arena:status(text) G.text("Status", text) end

function Arena:hud()
    for i = 1, 4 do
        local p = self.players[i]
        G.text("Score" .. i, p.active and agr.tr("arena.score", i, p.score) or "")
    end
end

function Arena:moveCoin()
    -- Ailleurs que sous un joueur (sinon il la reprendrait aussitôt).
    for _ = 1, 30 do
        self.coinX = (math.random() * 2 - 1) * (self.halfW - 1)
        self.coinY = (math.random() * 2 - 1) * (self.halfH - 1)
        local free = true
        for _, p in ipairs(self.players or {}) do
            if p.active and G.distance(p.x, p.y, self.coinX, self.coinY) < 2.5 then free = false end
        end
        if free then break end
    end
    local c = agr.find("Coin")
    if c then c:setPosition(self.coinX, self.coinY, 0) end
end

-- Boutons Local / Host / Join.
function Arena:setMode(button)
    local name = button:name()
    agr.net.leave()
    if name == "Local" then
        self.mode = "local"
        self:reset()
        self:status(agr.tr("arena.status.local"))
    elseif name == "Host" then
        local ok, err = agr.net.host(self.port, 4)
        if not ok then return self:status(agr.tr("arena.status.error", err)) end
        self.mode = "host"
        self:reset()
        self:status(agr.tr("arena.status.hosting", agr.net.port()))
    elseif name == "Join" then
        local address = agr.save.get("arena.address", "127.0.0.1")
        local ok, err = agr.net.join(address, self.port)
        if not ok then return self:status(agr.tr("arena.status.error", err)) end
        self.mode = "client"
        self:reset()
        self:status(agr.tr("arena.status.joining", address))
    end
end

local function axis(neg1, neg2, pos1, pos2)
    local v = 0
    if agr.input.keyDown(neg1) or (neg2 and agr.input.keyDown(neg2)) then v = v - 1 end
    if agr.input.keyDown(pos1) or (pos2 and agr.input.keyDown(pos2)) then v = v + 1 end
    return v
end

-- Commandes locales : (x, y) du joueur n sur cette machine.
function Arena:localInput(n)
    local x, y
    if n == 1 then
        x, y = axis("A", "Q", "D"), axis("S", nil, "W", "Z")
        if self.mode ~= "local" then  -- en réseau, les flèches pilotent aussi le joueur de la machine
            x, y = x + axis("Left", nil, "Right"), y + axis("Down", nil, "Up")
        end
    else
        x, y = axis("Left", nil, "Right"), axis("Down", nil, "Up")
    end
    if agr.input.gamepadCount() >= n then
        x = x + agr.input.gamepadAxis("LeftX", n)
        y = y - agr.input.gamepadAxis("LeftY", n)
    end
    return math.max(-1, math.min(1, x)), math.max(-1, math.min(1, y))
end

function Arena:simulate(dt)
    if self.winner then return end
    for i, p in ipairs(self.players) do
        if p.active then
            local l = math.sqrt(p.inX * p.inX + p.inY * p.inY)
            local ix, iy = p.inX, p.inY
            if l > 1 then ix, iy = ix / l, iy / l end
            p.x = G.clamp(p.x + ix * self.speed * dt, -self.halfW, self.halfW)
            p.y = G.clamp(p.y + iy * self.speed * dt, -self.halfH, self.halfH)
        end
    end
    -- Les joueurs se repoussent.
    for i = 1, 4 do
        for j = i + 1, 4 do
            local a, b = self.players[i], self.players[j]
            if a.active and b.active then
                local d = G.distance(a.x, a.y, b.x, b.y)
                if d < self.radius * 2 and d > 0.001 then
                    local push = (self.radius * 2 - d) / 2
                    local nx, ny = (b.x - a.x) / d, (b.y - a.y) / d
                    a.x, a.y, b.x, b.y = a.x - nx * push, a.y - ny * push, b.x + nx * push, b.y + ny * push
                end
            end
        end
    end
    for i, p in ipairs(self.players) do
        if p.active and G.distance(p.x, p.y, self.coinX, self.coinY) < self.radius + 0.35 then
            p.score = p.score + 1
            agr.audio.play("assets://audio/coin.wav")
            agr.fx.burst(self.coinX, 0.6, self.coinY, { count = 20, color = { 1, 0.85, 0.3 }, speed = 4, life = 0.5, spread = "up" })
            agr.fx.popup(self.coinX, 1.2, self.coinY, "+1", { color = { 1, 0.9, 0.4 }, size = 48 })
            self:moveCoin()
            if p.score >= self.target then
                self.winner = i
                self.restartTimer = 3
            end
            self:hud()
        end
    end
end

function Arena:onUpdate(dt)
    -- Réseau : événements.
    if self.mode ~= "local" then
        for _, ev in ipairs(agr.net.poll()) do
            if self.mode == "host" then
                local p = self.players[ev.peer]
                if ev.type == "connected" and p then
                    p.active, p.score, p.inX, p.inY = true, 0, 0, 0
                    p.e:setActive(true)
                    self:status(agr.tr("arena.status.players", #agr.net.peers() + 1))
                    self:hud()
                elseif ev.type == "disconnected" and p then
                    p.active = false
                    p.e:setActive(false)
                    self:status(agr.tr("arena.status.players", #agr.net.peers() + 1))
                    self:hud()
                elseif ev.type == "message" and p and ev.data.t == "in" then
                    p.inX, p.inY = tonumber(ev.data.x) or 0, tonumber(ev.data.y) or 0
                end
            else  -- client
                if ev.type == "connected" then
                    self:status(agr.tr("arena.status.joined", agr.net.id()))
                elseif ev.type == "disconnected" then
                    self.mode = "local"
                    self:reset()
                    self:status(agr.tr("arena.status.lost"))
                    return
                elseif ev.type == "message" and ev.data.t == "st" then
                    for i, s in ipairs(ev.data.p) do
                        local p = self.players[i]
                        if p then
                            p.active = s[4] == 1
                            p.x, p.y, p.score = s[1], s[2], s[3]
                            p.e:setActive(p.active)
                        end
                    end
                    self.coinX, self.coinY = ev.data.c[1], ev.data.c[2]
                    agr.find("Coin"):setPosition(self.coinX, self.coinY, 0)
                    self.winner = ev.data.w ~= 0 and ev.data.w or nil
                    self:hud()
                end
            end
        end
    end

    if self.mode == "local" then
        self.players[1].inX, self.players[1].inY = self:localInput(1)
        self.players[2].inX, self.players[2].inY = self:localInput(2)
        self:simulate(dt)
    elseif self.mode == "host" then
        self.players[1].inX, self.players[1].inY = self:localInput(1)
        self:simulate(dt)
        self.sendTimer = self.sendTimer - dt
        if self.sendTimer <= 0 then
            self.sendTimer = 1 / self.sendRate
            local st = { t = "st", p = {}, c = { self.coinX, self.coinY }, w = self.winner or 0 }
            for i, p in ipairs(self.players) do st.p[i] = { p.x, p.y, p.score, p.active and 1 or 0 } end
            agr.net.broadcast(st, false)
        end
    elseif self.mode == "client" and agr.net.id() ~= 0 then
        local x, y = self:localInput(1)
        agr.net.send(1, { t = "in", x = x, y = y }, false)
    end

    -- Affichage (lissé côté client).
    local k = self.mode == "client" and math.min(1, 15 * dt) or 1
    for _, p in ipairs(self.players) do
        p.sx = p.sx + (p.x - p.sx) * k
        p.sy = p.sy + (p.y - p.sy) * k
        if p.active then p.e:setPosition(p.sx, p.sy, 0) end
    end
    if self.winner then
        G.text("Message", agr.tr("arena.won", self.winner))
        if self.mode ~= "client" then
            self.restartTimer = self.restartTimer - dt
            if self.restartTimer <= 0 then
                for _, p in ipairs(self.players) do p.score = 0 end
                self.winner = nil
                G.text("Message", "")
                self:hud()
            end
        end
    end
end

function Arena:onDestroy() agr.net.leave() end

return Arena
