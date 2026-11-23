-- Boîte de dialogue (paquet officiel dialogue). Sur une entité UIText ; `choicesRoot` nomme une entité
-- dont les enfants sont des boutons (UIButton + UIText) utilisés pour les choix.
local Dialogue = require("assets://kits/dialogue/dialogue.lua")

local Box = {
    properties = { file = "assets://kits/dialogue/example.json", start = "hello", speed = 40, choicesRoot = "Choices" },
}

function Box:onStart()
    self.d = Dialogue.load(self.file)
    self.root = agr.find(self.choicesRoot)
    if self.d then self:show(self.d:start(self.start)) end
end

function Box:show(line)
    self.line = line
    self.shown = 0
    if self.root then
        for i, b in ipairs(self.root:children()) do
            local c = line and line.choices[i]
            b:setActive(c ~= nil)
            if c then
                b:setText(c.text)
                -- The button calls Box:pick(button) on this entity's script.
                b:set("UIButton", "target", self.entity)
                b:set("UIButton", "message", "pick")
            end
        end
    end
    if not line then self.entity:setText("") end
end

function Box:pick(button)
    if not self.root then return end
    for i, b in ipairs(self.root:children()) do
        if b:key() == button:key() then return self:show(self.d:choose(i)) end
    end
end

function Box:onUpdate(dt)
    if not self.line then return end
    local full = (self.line.speaker ~= "" and (self.line.speaker .. " : ") or "") .. self.line.text
    if self.shown < #full then
        self.shown = math.min(#full, self.shown + self.speed * dt)
        self.entity:setText(full:sub(1, math.floor(self.shown)))
    elseif #self.line.choices == 0 and (agr.input.keyPressed("Space") or agr.input.mousePressed(0) or agr.input.tapped()) then
        self:show(self.d:choose(nil))
    end
end

return Box
