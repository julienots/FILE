-- La pousse danse quelques secondes quand une balle la touche (AGR ENGINE).
-- Son Animator joue la machine à états sprout.agrctrl (Idle <-> Dance, paramètre « dance ») ;
-- les clips viennent du modèle glTF animé (assets://3d/sprout.glb#Idle, #Dance).
local Sprout = { properties = { danceTime = 3 } }

function Sprout:onStart() self.timer = 0 end

function Sprout:onCollision(other, began)
    if began and other:name():sub(1, 4) == "Ball" then
        self.timer = self.danceTime
        self.entity:setParameter("dance", true)
    end
end

function Sprout:onUpdate(dt)
    if self.timer > 0 then
        self.timer = self.timer - dt
        if self.timer <= 0 then self.entity:setParameter("dance", false) end
    end
end

return Sprout
