-- Le sage : donne la quête des trois gemmes et la récompense (expérience).
-- `texts` : préfixe des textes traduits (« rpg » pour le RPG 2D, « rpg3d » pour le RPG 3D).
local Npc = { properties = { reward = 10, needed = 3, texts = "rpg" } }

function Npc:talk(hero)
    if hero.quest == "none" then
        hero:say(agr.tr(self.texts .. ".npc.intro", self.needed), 6)
        hero:setQuest("active")
    elseif hero.quest == "active" then
        if hero.gems >= self.needed then
            hero:say(agr.tr(self.texts .. ".npc.thanks", self.reward), 5)
            hero:setQuest("done")
            hero:gainXp(self.reward)
        else
            hero:say(agr.tr(self.texts .. ".npc.remind", hero.gems, self.needed), 4)
        end
    else
        hero:say(agr.tr(self.texts .. ".npc.done"), 4)
    end
end

return Npc
