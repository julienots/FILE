# Tutorial: a 3D game, collect orbs without getting caught

*(English version of [TUTORIAL_3D.md](TUTORIAL_3D.md). The editor's interface is in French: menu names
are given in French with their meaning.)*

A small 3D game: a hero who runs and jumps on a floor, spinning orbs to collect, a counter, and an enemy
that chases the hero around obstacles. The three scripts are **run by the engine's tests** (test
"tutorial 3D (English)" in `tests/test_game_templates.cpp`) in the scene described here: the hero lands,
runs, picks up an orb, and the enemy goes around a wall to catch him.

## 1. The project and the floor

1. Project hub: type **3D**, template **Scène 3D vide** (empty 3D scene), a name, *Créer*. The scene has
   a camera (`Main Camera`), a sun, a floor `Ground` (a plane) and a cube. Delete the cube (Del).
2. Select `Ground`, rename it `Floor` (F2), scale `(30, 1, 30)`.
3. *Ajouter un composant › Collider* (Add component), shape `Box`, size `(1, 0.1, 1)` (it follows the
   object's scale). The plane alone is only an image: the collider makes the floor solid.
4. Put `Main Camera` above and behind: position `(0, 12, 14)`, rotation `(-40, 0, 0)`.

## 2. The hero

1. *Créer › Formes 3D › Cube* (Create › 3D shapes › Cube), rename it `Hero`, position `(0, 1, 0)`.
2. *Ajouter un composant › CharacterController3D*: a capsule (feet at the object's origin) that slides
   along walls, climbs steps and slopes, and pushes dynamic objects.
3. New script `hero.lua`, dragged onto the hero:

```lua
-- assets/scripts/hero.lua
local Hero = {
    properties = { speed = 5, jump = 6 },
}

function Hero:onUpdate(dt)
    local _, vy, _ = self.entity:velocity()
    if self.entity:isGrounded() and agr.input.pressed("jump") then vy = self.jump end
    -- move_y is 1 toward the top of the screen: forward is -Z, away from the camera.
    local x, z = agr.input.value("move_x"), agr.input.value("move_y")
    self.entity:setVelocity(x * self.speed, vy, -z * self.speed)   -- gravity is added in the air
end

return Hero
```

## 3. Orbs and the counter

1. *Créer › Interface (UI) › Canvas*, then as its child *Interface (UI) › Texte* renamed `Score`, text
   `Orbs: 0`.
2. *Créer › Formes 3D › Sphère*, rename it `Orb`, position `(4, 0.8, 0)`, scale `(0.6, 0.6, 0.6)`.
3. *Ajouter un composant › Collider*, shape `Sphere`, tick `isTrigger`.
4. Script `orb.lua`:

```lua
-- assets/scripts/orb.lua
local Orb = {
    properties = { spin = 90 },   -- degrees per second
}

function Orb:onUpdate(dt)
    self.entity:rotate(0, self.spin * dt, 0)
end

function Orb:onTrigger(other, began)
    if not began or other:name() ~= "Hero" then return end
    agr.globals.orbs = (agr.globals.orbs or 0) + 1
    local label = agr.find("Score")
    if label then label:setText("Orbs: " .. agr.globals.orbs) end
    self.entity:destroy()
end

return Orb
```

## 4. An enemy that chases the hero

**Navigation** computes paths around obstacles from the scene's static colliders ([NAVIGATION.md](NAVIGATION.md)).

1. An obstacle: *Formes 3D › Cube* `Wall`, position `(-3, 1, 0)`, scale `(1, 2, 8)`, with a `Collider`.
2. The enemy: *Formes 3D › Cube* `Enemy`, position `(-8, 0.5, 0)`, red, and *Ajouter un composant ›
   NavAgent* (speed `3`).
3. Script `enemy.lua`:

```lua
-- assets/scripts/enemy.lua
local Enemy = {
    properties = { target = "Hero", reach = 1.3 },
}

function Enemy:onStart()
    self.wait = 0
end

function Enemy:onUpdate(dt)
    local hero = agr.find(self.target)
    if not hero then return end
    local hx, hy, hz = hero:position()
    -- A new path four times a second: enough to follow a moving hero.
    self.wait = self.wait - dt
    if self.wait <= 0 then
        self.entity:navigateTo(hx, hy, hz)
        self.wait = 0.25
    end
    local ex, _, ez = self.entity:position()
    if (hx - ex) ^ 2 + (hz - ez) ^ 2 < self.reach ^ 2 then
        local label = agr.find("Score")
        if label then label:setText("Caught!") end
        agr.time.setScale(0)   -- everything stops; Stop restores the game
    end
end

return Enemy
```

Play: the enemy goes around the wall to reach the hero. Collect the orbs while staying out of reach.

## 5. Going further

- **Third / first person camera**: official packages `follow-camera` or `fps-controller`
  ([PACKAGES.md](PACKAGES.md)).
- **A real character**: drag an animated `.glb` model into the view, then an `Animator` and its state
  machine ([ANIMATION.md](ANIMATION.md)).
- **Scenery**: terrain, water, fog, particles ([3D.md](3D.md), [TERRAIN.md](TERRAIN.md)).
- **Health and pause**: the `health` and `pause-menu` packages.
- **Export**: *Fichier › Exporter le jeu...* ([EXPORT.md](EXPORT.md)).
