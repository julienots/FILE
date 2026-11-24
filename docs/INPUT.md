# Entrées : actions, manettes, tactile

Le moteur lit le clavier, la souris, l'écran tactile (multi-touch), les capteurs de mouvement et les
**manettes** (jusqu'à 4, via SDL3 : Xbox, PlayStation, Switch Pro et la plupart des manettes USB /
Bluetooth). Au-dessus, les **actions** donnent aux scripts des noms indépendants du matériel.

Module `agr::runtime` (`Input.hpp`, `InputActions.hpp`). Tests : `tests/test_input_actions.cpp`, tests
de script, test de jeu des modèles.

## Actions

Une action est un **bouton** (appuyé, juste appuyé, juste relâché) ou un **axe** (−1 à 1, droite / haut
positifs), liée à plusieurs entrées à la fois :

| Liaison | Paramètres |
|---|---|
| Touche | touche physique (indépendante de la disposition du clavier) |
| Bouton de souris | gauche, milieu, droit, X1, X2 |
| Bouton de manette | `South` (A / Croix), `East`, `West`, `North`, `Start`, croix directionnelle… ; manette précise ou toutes |
| Axe de manette | sticks et gâchettes, avec zone morte |
| Zone tactile | rectangle de l'écran (fractions) ; seuls les doigts **posés dans la zone** comptent |
| Joystick tactile | zone de départ, axe x ou y, rayon (déplacement pour la valeur maximale) ; suit le doigt qui l'a pris |

Chaque liaison a un **facteur** (−1 pour « gauche » d'un axe, −1 pour inverser un stick). Les valeurs
analogiques sous la **zone morte** de l'action comptent pour 0 ; au-delà, elles sont remises à l'échelle
de 0 à 1. Plusieurs liaisons d'un axe s'additionnent (bornées à −1..1).

Actions par défaut (nouveaux projets) :

| Action | Type | Liaisons |
|---|---|---|
| `move_x`, `move_y` | axes | flèches, WASD / ZQSD, stick gauche, joystick tactile en bas à gauche |
| `jump` | bouton | Espace, bouton South, zone tactile en bas à droite |
| `fire` | bouton | clic gauche, Ctrl gauche, bouton West |
| `pause` | bouton | Échap, retour Android, Start |

```lua
function Hero:onUpdate(dt)
    local move = agr.input.value("move_x")
    if agr.input.pressed("jump") and self.grounded then self:jump() end
end
```

Les actions sont enregistrées dans `assets/input.agrinput` (JSON) et exportées avec le jeu ; elles sont
évaluées une fois par image par le moteur. Les modèles Platformer 2D et Vitrine 3D les utilisent.

## Gestes tactiles

Le moteur reconnaît les gestes à chaque image (`agr::GestureRecognizer`, mis à jour par `Engine`) à partir
des doigts ; sur PC, le bouton gauche de la souris agit comme un doigt et la molette comme un pincement,
donc tout se teste sans téléphone. Coordonnées en pixels de fenêtre, comme `agr.camera.screenToWorld`.

| Geste | Quand |
|---|---|
| `tap` | doigt levé en moins de 0,35 s sans avoir bougé de plus de 14 px |
| `doubleTap` | deuxième toucher à moins de 0,35 s et 40 px du premier |
| `longPress` | doigt posé 0,5 s sans bouger |
| `dragBegin`, `drag`, `dragEnd` | un doigt qui glisse (au-delà de 14 px) ; `dragBegin` porte le mouvement depuis l'appui |
| `swipe` | au lâcher d'un glisser rapide (≥ 900 px/s), avec la vitesse `vx, vy` |
| `pinch` | deux doigts : `scale` (> 1 = écartés) et `dx, dy` (déplacement du centre) ; molette sur PC |

```lua
for _, g in ipairs(agr.input.gestures()) do
    if g.type == "tap" and not agr.ui.pointerOverUI() then
        local x, y, z = agr.camera.screenToWorld(g.x, g.y)   -- point du sol touché
        self:select(x, z)
    elseif g.type == "dragEnd" then
        self:drop(g.x, g.y)
    end
end
local dx, dy = agr.input.drag()          -- glissé de cette image (0, 0 sinon)
local zoom, panX, panY, twist = agr.input.pinch()   -- twist : rotation à deux doigts (radians, > 0 = sens horaire)
```

**Caméra de stratégie prête à l'emploi** : `assets://scripts/strategy_camera.lua` (glisser pour déplacer en
gardant le sol sous le doigt, pincer ou molette pour zoomer vers les doigts, inertie au lâcher, limites
`minX..maxX`, `minY..maxY`, zoom `minZoom..maxZoom` ; caméra 3D ou orthographique 2D ; les gestes commencés
sur l'interface sont ignorés). À poser sur la caméra d'un jeu de village, de carte ou de tactique.

**Glisser une carte sur le terrain** : le modèle *Jeu : Duel des Tours* le montre (`duel_game.lua`) :
au `dragBegin`, `agr.ui.isPressed(carte)` dit quelle carte est tenue ; pendant `drag`, un repère suit
`screenToWorld(g.x, g.y)` ; au `dragEnd`, l'unité est posée si l'endroit est permis.

## Remappage par le joueur et vibration des manettes

Un menu d'options peut laisser le joueur changer ses touches (comme `InputMap.action_erase_events` /
`action_add_event` de Godot), sans toucher au fichier `input.agrinput` du jeu :

```lua
-- « Appuyez sur une touche... » : capture() rend ce qui vient d'être appuyé (touche, souris,
-- bouton de manette, stick poussé : {type =, input =, scale =, text = "Key W"}), sinon nil.
function Options:onUpdate(dt)
    if not self.waiting then return end
    local b = agr.input.capture()
    if b then
        agr.input.setBindings(self.waiting, { b })   -- remplace les liaisons de l'action
        agr.input.saveBindings()                      -- gardé dans la sauvegarde, réappliqué à chaque lancement
        self.waiting = nil
    end
end
-- agr.input.bindings("jump") -> liste des liaisons ({type, input, text...}) ;
-- agr.input.resetBindings() -> celles du jeu (puis saveBindings pour l'oublier).
```

Seules les actions modifiées sont sauvegardées (clé `agr.input` de `agr.save`) : une mise à jour du jeu
qui change d'autres actions les garde.

**Vibration des manettes** : `agr.input.vibrate(manette, force, secondes[, force_légère])` (manette 1 à
4 ; moteur lourd et moteur léger, 0..1) rend `false` si la manette n'en a pas.

## Dans l'éditeur

*Affichage › Entrées* : liste des actions, type, zone morte, liaisons. Pour une touche ou un bouton de
manette, cliquer sur le bouton puis appuyer sur la touche voulue. Pendant le jeu, la valeur de chaque
action s'affiche en direct. Chaque modification est enregistrée.

## Limites actuelles

- Les manettes n'ont pas été testées sur Android avec un vrai appareil.
- Les gestes sont testés avec des touchers simulés et la souris, pas encore sur un vrai téléphone.
- Vibration des manettes : passe par SDL (`SDL_RumbleGamepad`), pas essayée avec une vraie manette
  (aucune dans la machine de développement) ; sans manette, `vibrate` rend `false` (testé).
- Remappage : pas d'écran tout fait dans les modèles de jeu (l'API ci-dessus suffit à en écrire un).
