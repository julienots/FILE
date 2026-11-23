# Tutorial: a 2D platformer in 30 minutes

*(English version of [TUTORIAL_2D.md](TUTORIAL_2D.md). The editor's interface is in French: menu names
are given in French with their meaning.)*

This tutorial builds a small platformer step by step: a character who runs and jumps, a floor, coins to
pick up and a counter on screen. It assumes no knowledge of the engine. Both scripts below are **run by
the engine's tests** (`tests/test_game_templates.cpp`, test "tutorial 2D (English)") in a scene built
as described here: the player lands on the floor, runs right and picks up the coin.

## 1. Create the project

1. Start the editor: the **project hub** opens.
2. Game type **2D**, template **Scène 2D vide** (empty 2D scene), a name and a folder, then *Créer*.
3. The editor opens on a scene that already has a 2D camera.

The window has the **Hierarchy** (scene objects, left), the **Scene view** (center), the **Inspector**
(settings of the selected object, right) and the **Assets** (project files, bottom).

## 2. The floor

1. Right-click in the Hierarchy › *Créer › 2D › Sprite* (Create › 2D › Sprite). Rename it `Floor` (F2).
2. In the Inspector, set its *Transform*: position `(0, -1)`, scale `(12, 1)`.
3. *Ajouter un composant › Collider2D* (Add component), shape `Box`. Without a `RigidBody2D`, a collider is
   **static**: the floor does not fall and blocks what touches it.

## 3. The player

1. *Créer › 2D › Sprite*, rename it `Player`, position `(0, 2)`.
2. *Ajouter un composant › CharacterController2D*: a character moved by its velocity (like Godot's
   `CharacterBody2D`): it slides along walls, stands on floors and climbs slopes. Set `size` to `(0.8, 1)`.
3. Press **Play** (Ctrl+P): the player falls and lands. **Stop** restores the scene.

## 4. Moving: a first script

In the Assets: *Nouveau script* (New script), name it `player.lua`, double-click it and replace its
content with:

```lua
-- assets/scripts/player.lua
local Player = {
    -- Editable in the Inspector, per object.
    properties = { speed = 6, jump = 9 },
}

function Player:onUpdate(dt)
    local body = self.entity
    local v = body:get("CharacterController2D", "velocity")    -- {x =, y =}
    v.x = agr.input.value("move_x") * self.speed               -- arrows, A/D, stick, touch
    if body:get("CharacterController2D", "onFloor") and agr.input.pressed("jump") then
        v.y = self.jump                                        -- Space, button A, touch
    end
    body:set("CharacterController2D", "velocity", v)           -- gravity is added by itself
end

return Player
```

Save (Ctrl+S) and **drag `player.lua` onto `Player`** in the Hierarchy (it adds a `Script` component).
Play: arrows run, Space jumps. The script editor completes names (`agr.input.` then Tab) and flags
mistakes as you type: `self.sped` or `body:sett(...)` is underlined in amber before you even play.

## 5. The on-screen counter

1. *Créer › Interface (UI) › Canvas*, then as its child *Interface (UI) › Texte* (Text).
2. Rename the text `Score`, type `Coins: 0` in its `text` field.

## 6. The coins

1. *Créer › 2D › Sprite*, rename it `Coin`, position `(3, -0.2)`, scale `(0.4, 0.4)`, yellow color.
2. *Ajouter un composant › Collider2D*, tick `isTrigger`: a **trigger** detects what goes through it
   without blocking it.
3. New script `coin.lua`, dragged onto the coin:

```lua
-- assets/scripts/coin.lua
local Coin = {
    properties = { value = 1 },
}

function Coin:onTrigger(other, began)
    if not began or other:name() ~= "Player" then return end
    -- agr.globals keeps values from one scene to the next (score, lives...).
    agr.globals.coins = (agr.globals.coins or 0) + self.value
    local label = agr.find("Score")
    if label then label:setText("Coins: " .. agr.globals.coins) end
    self.entity:destroy()
end

return Coin
```

4. Right-click the coin › *Créer un prefab* (Create a prefab), then drag the `.agrprefab` into the view
   several times to place more coins.

Play: each coin touched disappears and the counter goes up.

## 7. Going further

- **A camera that follows the player**: *Affichage › Plugins et paquets* (View › Plugins and packages),
  *En ligne* tab, *Bibliothèque officielle*, install `follow-camera`, drag
  `kits/camera/follow_camera.lua` onto the camera and set its `target` property to `Player`.
- **Tile levels**, **animated characters**, **sounds**: see [2D.md](2D.md), [ANIMATION.md](ANIMATION.md),
  [AUDIO.md](AUDIO.md).
- **Export**: *Fichier › Exporter le jeu...* (File › Export) for a PC folder, a Windows `.exe` or an
  Android APK; with a phone plugged in, *Installer et lancer sur l'appareil* ([EXPORT.md](EXPORT.md)).
