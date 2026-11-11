-- Duel de cartes (AGR ENGINE). Chaque tour : 3 énergie, main de 5 cartes ; cliquer / toucher une
-- carte la joue, « Fin du tour » laisse jouer l'adversaire, dont l'intention est affichée à l'avance.
-- L'armure absorbe les dégâts et disparaît au début du tour suivant de son propriétaire.
local G = require("assets://games/scripts/common.lua")

local CARDS = {
    strike = { cost = 1, icon = "assets://games/icon_sword.png", damage = 5 },
    heavy = { cost = 2, icon = "assets://games/icon_sword.png", damage = 11, tint = { 1, 0.75, 0.6, 1 } },
    shield = { cost = 1, icon = "assets://games/icon_shield.png", armor = 6 },
    potion = { cost = 1, icon = "assets://games/icon_potion.png", heal = 6 },
    bolt = { cost = 1, icon = "assets://games/icon_bolt.png", pierce = 4, draw = 1 },
}
local DECK = { "strike", "strike", "strike", "strike", "strike", "heavy", "heavy", "shield", "shield", "shield", "shield",
               "potion", "potion", "bolt", "bolt" }

local Game = { properties = { playerHp = 30, foeHp = 40, energyPerTurn = 3, handSize = 5 } }

local function shuffle(t)
    for i = #t, 2, -1 do
        local j = math.random(i)
        t[i], t[j] = t[j], t[i]
    end
    return t
end

function Game:onStart() self:restart() end

function Game:restart()
    self.player = { hp = self.playerHp, max = self.playerHp, armor = 0 }
    self.foe = { hp = self.foeHp, max = self.foeHp, armor = 0, turn = 0 }
    self.draw, self.discard, self.hand = shuffle({ table.unpack(DECK) }), {}, {}
    self.state = "playing"
    G.show("Restart", false)
    G.show("EndTurn", true)
    G.text("Message", "")
    G.text("Log", agr.tr("cards.start"))
    self:startTurn()
end

function Game:drawCard()
    if #self.draw == 0 then
        self.draw, self.discard = shuffle(self.discard), {}
    end
    local id = table.remove(self.draw)
    if id then table.insert(self.hand, id) end
end

function Game:startTurn()
    self.energy = self.energyPerTurn
    self.player.armor = 0
    while #self.hand < self.handSize do self:drawCard() end
    -- Intention de l'adversaire pour son prochain tour.
    self.foe.turn = self.foe.turn + 1
    if self.foe.turn % 3 == 0 then self.intent = { kind = "heavy", value = 14 }
    elseif math.random() < 0.3 then self.intent = { kind = "defend", value = 8 }
    else self.intent = { kind = "attack", value = 6 + math.random(0, 3) } end
    self:refresh()
end

function Game:refresh()
    G.text("FoeStats", agr.tr("cards.stats", self.foe.hp, self.foe.max, self.foe.armor))
    G.text("FoeIntent", agr.tr("cards.intent." .. self.intent.kind, self.intent.value))
    G.text("PlayerStats", agr.tr("cards.stats", self.player.hp, self.player.max, self.player.armor))
    G.text("EnergyText", agr.tr("cards.energy", self.energy, self.energyPerTurn))
    for i = 1, self.handSize do
        local button = agr.find("Card " .. i)
        local id = self.hand[i]
        if button then
            button:setActive(id ~= nil and self.state == "playing")
            if id then
                local c = CARDS[id]
                button:find("Icon"):set("UIImage", "texture", c.icon)
                button:find("Icon"):set("UIImage", "color", c.tint or { 1, 1, 1, 1 })
                button:find("Cost"):setText(c.cost)
                button:find("Name"):setText(agr.tr("cards.name." .. id))
                button:find("Text"):setText(agr.tr("cards.text." .. id, c.damage or c.armor or c.heal or c.pierce))
                local playable = c.cost <= self.energy
                button:set("UIImage", "color", playable and { 1, 1, 1, 1 } or { 0.55, 0.55, 0.6, 1 })
            end
        end
    end
end

-- Dégâts sur une cible { hp, armor } ; l'armure absorbe d'abord (sauf perçants).
local function hurt(target, amount, pierce)
    if not pierce then
        local absorbed = math.min(target.armor, amount)
        target.armor = target.armor - absorbed
        amount = amount - absorbed
    end
    target.hp = math.max(0, target.hp - amount)
    return amount
end

-- Message d'un bouton de carte (UIButton -> playCard(bouton)).
function Game:playCard(button)
    if self.state ~= "playing" then return end
    local index = tonumber(button:name():match("(%d+)$"))
    self:play(index)
end

function Game:play(index)
    local id = self.hand[index]
    if not id or self.state ~= "playing" then return false end
    local c = CARDS[id]
    if c.cost > self.energy then
        G.text("Log", agr.tr("cards.noenergy"))
        return false
    end
    self.energy = self.energy - c.cost
    table.remove(self.hand, index)
    table.insert(self.discard, id)
    if c.damage then
        G.text("Log", agr.tr("cards.log.hit", hurt(self.foe, c.damage)))
        agr.audio.play("assets://audio/launch.wav", { volume = 0.6 })
    elseif c.armor then
        self.player.armor = self.player.armor + c.armor
        G.text("Log", agr.tr("cards.log.armor", c.armor))
        agr.audio.play("assets://audio/click.wav")
    elseif c.heal then
        local before = self.player.hp
        self.player.hp = math.min(self.player.max, self.player.hp + c.heal)
        G.text("Log", agr.tr("cards.log.heal", self.player.hp - before))
        agr.audio.play("assets://audio/coin.wav")
    elseif c.pierce then
        G.text("Log", agr.tr("cards.log.bolt", hurt(self.foe, c.pierce, true)))
        for _ = 1, c.draw do self:drawCard() end
        agr.audio.play("assets://audio/launch.wav", { pitch = 1.5 })
    end
    if self.foe.hp <= 0 then self:finish(true) end
    self:refresh()
    return true
end

function Game:endTurn()
    if self.state ~= "playing" then return end
    for _, id in ipairs(self.hand) do table.insert(self.discard, id) end
    self.hand = {}
    -- Tour de l'adversaire : son armure tombe, puis il applique son intention.
    self.foe.armor = 0
    local i = self.intent
    if i.kind == "defend" then
        self.foe.armor = i.value
        G.text("Log", agr.tr("cards.log.foedefend", i.value))
    else
        G.text("Log", agr.tr("cards.log.foehit", hurt(self.player, i.value)))
        agr.device.vibrate(0.1)
    end
    if self.player.hp <= 0 then
        self:finish(false)
        self:refresh()
        return
    end
    self:startTurn()
end

function Game:finish(won)
    self.state = won and "won" or "lost"
    if won then
        local wins = (agr.save.get("cards.wins", 0)) + 1
        agr.save.set("cards.wins", wins)
        G.text("Message", agr.tr("cards.won", wins))
    else
        G.text("Message", agr.tr("cards.lost"))
    end
    G.show("Restart", true)
    G.show("EndTurn", false)
end

function Game:onUpdate(dt)
    if self.state ~= "playing" then return end
    for i = 1, self.handSize do  -- clavier : touches 1 à 5, Entrée pour finir le tour
        if agr.input.keyPressed(tostring(i)) then self:play(i) end
    end
    if agr.input.keyPressed("Return") then self:endTurn() end
end

return Game
