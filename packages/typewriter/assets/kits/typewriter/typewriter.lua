-- Texte machine à écrire (paquet officiel typewriter). Sur une entité UIText. Signal "typed" à la fin.
--   agr.find("Dialogue"):script():say("Bonjour, voyageur !")
local Typewriter = {
    signals = { typed = {"string"} },
    properties = { speed = 40, punctuationPause = 0.25, skipAction = "jump" },
}

function Typewriter:onStart()
    self.full, self.shown, self.wait = "", 0, 0
end

--- Starts typing `text` (UTF-8 aware).
function Typewriter:say(text)
    self.full = text or ""
    self.chars = {}
    for _, c in utf8.codes(self.full) do self.chars[#self.chars + 1] = utf8.char(c) end
    self.shown, self.wait = 0, 0
    self.entity:setText("")
end

function Typewriter:isTyping() return self.chars ~= nil and self.shown < #self.chars end

function Typewriter:finish()
    if not self.chars then return end
    self.shown = #self.chars
    self.entity:setText(self.full)
    self:done()
end

function Typewriter:done()
    self.entity:emit("typed", self.full)  -- self.entity:connect("typed", function(text) ... end)
end

function Typewriter:onUpdate(dt)
    if not self:isTyping() then return end
    if agr.input.pressed(self.skipAction) then return self:finish() end
    self.wait = self.wait - dt
    local changed = false
    while self.wait <= 0 and self.shown < #self.chars do
        self.shown = self.shown + 1
        local c = self.chars[self.shown]
        self.wait = self.wait + 1 / math.max(self.speed, 1)
        if c == "." or c == "," or c == "!" or c == "?" or c == ":" then self.wait = self.wait + self.punctuationPause end
        changed = true
    end
    if changed then
        self.entity:setText(table.concat(self.chars, "", 1, self.shown))
        if self.shown == #self.chars then self:done() end
    end
end

return Typewriter
