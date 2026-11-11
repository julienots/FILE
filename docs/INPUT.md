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

## Dans l'éditeur

*Affichage › Entrées* : liste des actions, type, zone morte, liaisons. Pour une touche ou un bouton de
manette, cliquer sur le bouton puis appuyer sur la touche voulue. Pendant le jeu, la valeur de chaque
action s'affiche en direct. Chaque modification est enregistrée.

## Limites actuelles

- Pas de remappage par le joueur depuis le jeu (l'API pour réécrire les liaisons à l'exécution n'est pas
  exposée aux scripts).
- Pas de vibration des manettes (la vibration du téléphone existe : `agr.device.vibrate`).
- Les manettes n'ont pas été testées sur Android avec un vrai appareil.
