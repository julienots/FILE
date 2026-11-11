# Scripts Lua (phases 6 et 7)

Le gameplay s'écrit en **Lua 5.4** : un script est un fichier `.lua` attaché à une entité par le
composant `Script`. Ce guide décrit le modèle d'exécution, l'API complète et ses limites. Tout ce qui
est décrit est couvert par des tests automatisés (`tests/test_script.cpp`, test de jeu sans écran des
modèles, test de bout en bout de l'éditeur).

## Un script

```lua
local Player = {
    -- Valeurs modifiables dans l'inspecteur, par entité (nombres, booléens, textes).
    properties = { speed = 4, name = "Héros" },
}

function Player:onStart()            -- une fois, à la première image où le script tourne
    self.jumps = 0                   -- `self` : la table de cette entité ; ses champs persistent
end

function Player:onUpdate(dt)         -- chaque image (dt en secondes)
    local h = agr.input.axis("horizontal")
    self.entity:translate(h * self.speed * dt, 0, 0)
end

function Player:onFixedUpdate(dt) end            -- au pas fixe (60 Hz), avant la physique
function Player:onCollision(other, began) end    -- contact physique commencé / terminé
function Player:onTrigger(other, began) end      -- entrée / sortie d'un déclencheur
function Player:onDestroy() end                  -- composant retiré, entité détruite, arrêt du jeu
function Player:onClick() end                    -- UIButton de cette entité cliqué
function Player:onValueChanged(v) end            -- UISlider de cette entité modifié
function Player:onAnimationEvent(name) end       -- événement d'un clip de l'Animator
function Player:onStateEnter(name) end           -- la machine à états de l'Animator change d'état

return Player
```

- `self.entity` est l'entité ; les propriétés déclarées sont copiées dans `self` (valeur de l'inspecteur,
  sinon valeur par défaut). Toute fonction de la table est appelable par d'autres scripts :
  `other:script():collect("pièce")`.
- Ordre : par entité, de façon déterministe. `onStart` des nouveaux scripts, puis `onUpdate`. Au pas fixe :
  `onFixedUpdate` des scripts **puis** la physique ; les contacts de ce pas appellent ensuite
  `onCollision` / `onTrigger` des deux entités.
- Un script par entité (composer avec des entités enfants ou `require`).

## Erreurs et sécurité

- Une erreur de compilation ou d'exécution est affichée **une fois** dans la console avec
  `fichier:ligne`, le rappel en cause et l'entité ; ce script est mis en pause pour cette entité, les
  autres continuent. Enregistrer le fichier corrigé le relance.
- Un rappel qui tourne plus de 2 s (boucle infinie) est interrompu avec un message explicite.
- Bac à sable : pas de `io`, `os`, `debug`, `package`, `load`, `loadfile`, `dofile`, `string.dump` ni de
  bytecode précompilé. Bibliothèques disponibles : base, `string`, `table`, `math`, `utf8`, `coroutine`.
- `require("assets://scripts/outils.lua")` exécute un module une fois (via le système de fichiers du
  projet) et renvoie sa valeur ; les appels suivants renvoient la même table.
- Lua est compilé en C++ : une erreur Lua est une exception C++, ce qui garantit la libération des objets
  du moteur en cours d'utilisation dans une fonction de l'API (vérifié sous AddressSanitizer).
- Rechargement à chaud : à l'enregistrement d'un script, les instances gardent leurs champs (`self.*`) et
  utilisent les nouvelles fonctions. Une version qui ne compile pas est refusée (l'ancienne continue).

## API

### Entité (`self.entity`, résultats de `agr.find`, `agr.spawn`, `other`...)

| Méthode | Rôle |
|---|---|
| `isValid()` | `false` si l'entité a été détruite (les autres méthodes lèvent alors une erreur claire) |
| `name()`, `setName(texte)` | nom |
| `position()` → x, y, z ; `setPosition(x, y [, z])` ; `translate(dx, dy [, dz])` | position locale |
| `worldPosition()` → x, y, z ; `forward()` → x, y, z | position monde, direction de l'axe −Z (caméras, lumières) |
| `rotation()` → x, y, z ; `setRotation(x, y, z)` ; `rotate(dx, dy, dz)` | rotation locale en **degrés** (Euler) |
| `scale()` → x, y, z ; `setScale(x [, y, z])` | échelle locale |
| `get(composant, champ)` ; `set(composant, champ, valeur)` | n'importe quel champ réfléchi (voir ci-dessous) |
| `has(composant)`, `add(composant)`, `remove(composant)` | composants par nom (`"SpriteRenderer"`, `"RigidBody2D"`...) |
| `parent()`, `children()` → table, `find(nom)` | hiérarchie (`find` cherche parmi les descendants) |
| `destroy()` | détruit l'entité et ses enfants à la fin de l'image |
| `script()` | la table `self` du script de cette entité, ou `nil` |
| `velocity()`, `setVelocity(...)`, `applyForce(...)`, `applyImpulse(...)`, `setAngularVelocity(...)` | corps physique : 2D (`x, y` ; angulaire en rad/s) ou 3D (`x, y, z`) selon que l'entité a un `RigidBody2D` ou un `RigidBody` |
| `setActive(bool)`, `isActive()` | active / désactive l'entité (et ses enfants) : menus, objets cachés. Désactiver une entité détruit l'instance de son script (`onDestroy`) ; la réactiver en crée une nouvelle (`onStart`, propriétés d'origine) |
| `playSound()`, `stopSound([fondu])`, `isSoundPlaying()` | `AudioSource` de l'entité ([AUDIO.md](AUDIO.md)) |
| `setText(valeur)`, `text()` | `UIText` de l'entité (les nombres sont convertis) ([UI.md](UI.md)) |
| `setParameter(nom, nombre \| booléen)`, `setTrigger(nom)`, `getParameter(nom)`, `playState(nom [, fondu])`, `currentState()` | `Animator` et sa machine à états ([ANIMATION.md](ANIMATION.md)) |

`get` / `set` convertissent : booléens, nombres, textes ; vecteurs et couleurs en tables
`{x=, y=, z=, w=}` (en écriture aussi `{r=, g=, b=, a=}` ou `{1, 2, 3}`) ; rotations en degrés ;
énumérations par nom (`"Orthographic"`) ; références d'entités ; tableaux d'entiers (tuiles).
Exemples : `e:set("SpriteAnimator", "clip", "run")`, `e:get("Camera", "fieldOfView")` (radians, comme le
composant).

### `agr`

| Fonction | Rôle |
|---|---|
| `agr.log(...)`, `agr.warn(...)`, `agr.error(...)` (et `print`) | console de l'éditeur, logcat sur Android |
| `agr.find(nom)` | première entité de ce nom, ou `nil` |
| `agr.spawn([nom [, parent]])` | nouvelle entité (ajouter ensuite ses composants avec `add` / `set`) |
| `agr.instantiate(uri [, parent])` | nouvelle instance d'un prefab (`assets://enemy.agrprefab`) avec ses enfants et ses scripts ; `nil, message` si le prefab manque ([PREFABS.md](PREFABS.md)) |
| `entity:navigateTo(x, y [, z])`, `stopNavigation()`, `navigationStatus()`, `navigationPath()`, `agr.nav.path(...)`, `agr.nav.rebuild()` | déplacement par recherche de chemin (composant `NavAgent`), voir [NAVIGATION.md](NAVIGATION.md) |
| `agr.input.keyDown(t)`, `keyPressed(t)`, `keyReleased(t)` | touches : `"A"`–`"Z"`, `"0"`–`"9"`, `"Space"`, `"Return"`, `"Escape"`, `"Left"`, `"Right"`, `"Up"`, `"Down"`, `"LeftShift"`, `"LeftCtrl"`, `"F1"`–`"F12"`, `"Back"` (Android)... |
| `agr.input.axis("horizontal" \| "vertical")` | −1..1 depuis les flèches, WASD et ZQSD |
| `agr.input.down(action)`, `pressed(action)`, `released(action)`, `value(action)` | **actions** du jeu (`"jump"`, `"move_x"`…) liées au clavier, à la souris, aux manettes et à l'écran tactile ; voir [INPUT.md](INPUT.md) |
| `agr.input.gamepadCount()`, `gamepadDown(bouton [, n])`, `gamepadPressed(bouton [, n])`, `gamepadAxis(axe [, n])` | manettes 1 à 4 ; boutons `"South"`, `"East"`, `"West"`, `"North"`, `"Start"`, `"DpadUp"`… ; axes `"LeftX"`, `"LeftY"`, `"RightX"`, `"RightY"`, `"LeftTrigger"`, `"RightTrigger"` |
| `agr.tr(clé, ...)` | texte traduit dans la langue courante, `{1}`, `{2}`… remplacés par les arguments ; voir [LOCALIZATION.md](LOCALIZATION.md) |
| `agr.locale.get()`, `agr.locale.set(code)`, `agr.locale.list()` | langue courante, changement (mémorisé dans la sauvegarde locale), langues disponibles |
| `agr.input.mouseDown(b)`, `mousePressed(b)`, `mousePosition()` | `b` : 0 gauche, 1 milieu, 2 droit ; position en pixels de fenêtre |
| `agr.input.touchCount()`, `agr.input.touch(i)` → x, y, phase | phase : `"began"`, `"moved"`, `"stationary"`, `"ended"` |
| `agr.screen.size()` → largeur, hauteur | taille de la fenêtre dans les unités du toucher / de la souris |
| `agr.time.delta()`, `fixedDelta()`, `total()`, `frame()` | temps (secondes) |
| `agr.time.now()` | heure réelle en secondes depuis 1970 (gains hors ligne, récompenses quotidiennes) |
| `agr.physics2d.raycast(ox, oy, dx, dy [, distance])` | table `{entity, x, y, nx, ny, distance}` ou `nil` |
| `agr.physics2d.gravity()`, `setGravity(x, y)` | gravité 2D (−9,81 m/s² par défaut) |
| `agr.physics3d.raycast(ox, oy, oz, dx, dy, dz [, distance])` | `{entity, x, y, z, nx, ny, nz, distance}` ou `nil` |
| `agr.physics3d.setGravity(x, y, z)` | gravité 3D |
| `agr.audio.play(uri [, options])` → voix ou `nil, message` | options : `volume`, `pitch`, `pan`, `loop`, `bus`, `stream`, `fadeIn`, `x`/`y`/`z` (voix spatiale), `minDistance`, `maxDistance` |
| `agr.audio.stop(voix [, fondu])`, `isPlaying(voix)`, `setVolume(voix, v)`, `setPitch(voix, v)` | contrôle d'une voix |
| `agr.audio.setBusVolume(bus, v)`, `busVolume(bus)`, `setBusMuted(bus, b)`, `busMuted(bus)` | bus `"Master"`, `"Music"`, `"Effects"`, `"Ambient"`, `"Interface"`, `"Voice"` |
| `agr.ui.pointerOverUI()` | vrai si la souris / un doigt est sur l'interface (le gameplay l'ignore alors) |
| `agr.ui.isPressed(entité)` | bouton maintenu (commandes tactiles) |
| `agr.input.accelerometer()` → x, y, z ou `nil` ; `agr.input.gyroscope()` | capteurs de mouvement (m/s², rad/s), `nil` sans capteur |
| `agr.save.set(clé, valeur)`, `get(clé [, défaut])`, `has`, `remove`, `clear`, `flush()` | sauvegarde locale : nombres, textes, booléens, tableaux et tables imbriquées ; écrite automatiquement quand l'application passe en arrière-plan ou se ferme |
| `agr.device.platform()`, `isMobile()` | `"Android"`, `"Linux"`, `"Windows"`... |
| `agr.device.vibrate(secondes [, force])`, `canVibrate()` | vibreur du téléphone (sans effet ailleurs) |
| `agr.device.safeArea()` → gauche, haut, droite, bas | marges de l'encoche et des barres système |
| `agr.clone(entité [, parent])` | copie active d'une entité et de ses enfants (composants, scripts) : ennemis, projectiles et cartes à partir d'un **prototype** caché dans la scène ; `parent` : une entité, `false` pour la racine de la scène, rien pour le parent de l'original (son script démarre à l'image suivante) |
| `agr.camera.screenToWorld(x, y [, hauteur])` → x, y, z | point du jeu sous un clic ou un doigt : plan z = 0 pour une caméra orthographique (2D), sinon sol horizontal y = hauteur (0 par défaut) ; `nil` si le rayon le manque |
| `agr.camera.worldToScreen(x, y, z)` → x, y, visible ; `agr.camera.ray(x, y)` → origine, direction | sens inverse (barres de vie au-dessus des unités) et rayon de la caméra (avec `agr.physics3d.raycast`) |
| `agr.net.*` | multijoueur en réseau : héberger, rejoindre, envoyer des tables, événements ; voir [NETWORK.md](NETWORK.md) |

## Dans l'éditeur

- *Nouveau script* (explorateur d'assets) crée un modèle commenté ; glisser un `.lua` sur un objet de la
  vue l'y attache (ou *Ajouter un composant › Script*).
- L'inspecteur du composant affiche les erreurs de compilation, les **propriétés déclarées** (modifiables
  par entité, enregistrées dans la scène) et les fonctions définies ; *Ouvrir le fichier* l'ouvre dans
  l'éditeur de texte du système.
- Les scripts ne tournent qu'en **mode Jeu** ; *Pause* les suspend ; *Arrêter* appelle `onDestroy` puis
  restaure la scène telle qu'avant le jeu.

## Exemples livrés

Dans les projets créés par l'éditeur (`assets/scripts/`) : `platformer_hero.lua` (déplacement, saut avec
détection du sol par rayon, paramètres de la machine à états, boutons tactiles de l'interface, son de
saut, score affiché), `camera_follow.lua`, `collectible.lua` (déclencheur, son), `ball_launcher.lua`
(crée des entités physiques 3D, son de lancer, compteur et curseur de vitesse de l'interface).

## Performances

`onUpdate` de 1 000 entités qui se déplacent : 0,25 à 0,4 ms par image sur la machine de test (~0,3 µs par
appel, y compris l'accès au Transform) ; ~260 Ko de mémoire Lua. Détails : [BENCHMARKS.md](BENCHMARKS.md).

## Limites actuelles

- **Éditeur de logique visuelle** (événements, conditions, actions sans code) : **non réalisé** ; il était
  prévu en phase 6 et reste à faire.
- Pas de débogueur pas à pas ni de points d'arrêt ; pas d'autocomplétion intégrée (éditeur de texte externe).
- Un script par entité ; pas de types statiques (Lua 5.4 standard, pas Luau).
- Pas encore d'API pour le chargement de scènes ni les préfabriqués ; la création d'entités se fait
  composant par composant.
