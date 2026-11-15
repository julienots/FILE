# Physique 2D et 3D (phase 6)

| | 2D | 3D |
|---|---|---|
| Moteur | [Box2D](https://github.com/erincatto/box2d) 3.1.1 | [Jolt Physics](https://github.com/jrouwe/JoltPhysics) 5.6.0 |
| Module / plugin | `agr::physics2d`, `Physics2DPlugin` | `agr::physics3d`, `Physics3DPlugin` |
| Corps | `RigidBody2D` | `RigidBody` |
| Formes | `Collider2D` (boîte, cercle, capsule) ; `TilemapCollider2D` | `Collider` (boîte, sphère, capsule) |
| Plan / unités | XY, rotation autour de Z ; mètres, kg, secondes | mètres, kg, secondes |

Les deux mondes suivent le même modèle, couvert par des tests automatisés (`tests/test_physics2d.cpp`,
`tests/test_physics3d.cpp`, test de jeu des modèles, test de bout en bout de l'éditeur).

## Principe

- La physique tourne **au pas fixe** du moteur (60 Hz par défaut, `EngineConfig::fixedTimestep`), après
  `onFixedUpdate` des scripts. Dans l'éditeur, uniquement en mode Jeu.
- Les corps reflètent les composants : créés à la première image, **reconstruits** quand un paramètre
  change (y compris pendant le jeu, depuis l'inspecteur ou un script), détruits avec leur entité ou au
  rechargement de la scène.
- **Synchronisation dans les deux sens** : la simulation écrit la position / rotation des corps
  dynamiques et cinématiques dans leur `Transform` ; un `Transform` modifié par le gameplay **téléporte**
  le corps (statique, dynamique) ou le **conduit** (cinématique : il atteint sa nouvelle position au pas
  suivant en poussant les corps dynamiques — plateformes mobiles).
- **Corps** : `type` statique, cinématique ou dynamique ; amortissements ; échelle de gravité ;
  rotation bloquée (personnages) ; détection continue (`bullet`) ; vitesse initiale ; masse imposée (3D).
- **Formes** : celles de l'entité et de ses descendants sans corps propre forment **un seul corps composé**.
  L'échelle monde de chaque entité s'applique à sa forme. Un collider sans corps parent est **statique**.
- **Déclencheurs** (`isTrigger`) : détectent les chevauchements sans bloquer.
- **Événements** : `Collision2DEvent` / `Collision3DEvent` (début / fin, déclencheur ou contact), sur le
  bus d'événements du moteur et en rappels de scripts `onCollision` / `onTrigger`. En 3D, un seul
  début / fin par paire de corps (même composés).
- **Requêtes** : lancer de rayon le plus proche (entité, point, normale, distance), forces, impulsions,
  vitesses, masse.

## Tilemaps

`TilemapCollider2D` sur une entité `Tilemap` rend **toutes les cellules peintes** solides, fusionnées en
bandes horizontales (une forme par bande) ; la collision est reconstruite quand les tuiles changent. Pour
un décor traversable, utiliser une seconde tilemap sans collider (comme « Decor » dans la Vitrine 2D).

## Couches de collision

Chaque `Collider`, `Collider2D`, `TilemapCollider2D` et `CharacterController3D` a une **couche** (`layer`,
0 à 31) et la liste des couches qu'il **touche** (`collidesWith`). Deux objets se touchent seulement si
chacun accepte la couche de l'autre ; les déclencheurs suivent la même règle. Les noms des couches se
donnent dans *Affichage › Couches de collision* (fichier `assets/physics_layers.json`, exporté avec le
jeu) ; l'inspecteur propose les couches par leur nom.

```lua
local mask = agr.physics3d.mask("Decor", "Ennemis")          -- ou agr.physics2d.mask
local hit = agr.physics3d.raycast(x, y, z, dx, dy, dz, 30, mask) -- le rayon ignore les autres couches
local hit2 = agr.physics2d.raycast(x, y, 1, 0, 10, "Sol")         -- un nom, une table de noms ou un masque
```

## Articulations (joints)

`Joint2D` (Box2D) et `Joint3D` (Jolt) relient le corps de l'entité à `connectedBody` (un autre corps) ou,
sans corps connecté, à un point fixe du monde.

| Type | Usage | Réglages |
|---|---|---|
| `Fixed` | souder deux objets | `springFrequency` > 0 : soudure souple (2D) |
| `Hinge` | portes, roues, pendules, leviers | axe (3D), butées `lowerAngle` / `upperAngle`, moteur (rad/s) |
| `Distance` | corde (butées 0..max), barre, **ressort** (`springFrequency`, `springDamping`) | `length` (0 = distance initiale) |
| `Slider` | ascenseurs, pistons, tiroirs | axe, butées `lowerLimit` / `upperLimit`, moteur (m/s) |
| `Ball` (3D) | chaînes, épaules | — |

- `anchor` est dans le repère de l'entité. Avec `autoConnectedAnchor`, l'autre ancrage est le même point
  du monde à la création ; sinon `connectedAnchor` est dans le repère du corps connecté (ou un point du
  monde sans corps connecté).
- Les deux corps reliés ne se touchent plus, sauf avec `collideConnected`.
- Le **moteur** (`enableMotor`, `motorSpeed`, `maxMotorForce`) se pilote en direct depuis un script
  (`e:set("Joint3D", "motorSpeed", 2)`), sans reconstruire l'articulation.
- `breakForce` (N) : au-delà, l'articulation casse, `broken` passe à vrai et le script de l'entité reçoit
  `onJointBreak(force)`. Décocher `broken` la reconstruit.
- `e:jointValue()` : angle de la charnière (radians), position de la glissière ou longueur de la corde.

## Contrôleur de personnage 3D

`CharacterController3D` (Jolt *CharacterVirtual*) remplace un `RigidBody` pour un héros ou un PNJ : une
capsule (pieds à l'origine de l'entité) qui **glisse le long des murs**, **monte les marches** jusqu'à
`stepHeight`, les pentes jusqu'à `maxSlope`, reste **collée au sol** en descente, suit les plateformes
mobiles et **pousse** les corps dynamiques (`mass`, `pushForce`). Les déclencheurs la voient et les rayons
la touchent.

```lua
function Hero:onUpdate(dt)
    local _, vy, _ = self.entity:velocity()
    if self.entity:isGrounded() and agr.input.pressed("jump") then vy = 6 end
    local x, z = agr.input.value("move_x"), agr.input.value("move_y")
    self.entity:setVelocity(x * 5, vy, -z * 5)     -- la gravité s'ajoute toute seule en l'air
end
```

`isGrounded()` vaut vrai sur un sol praticable ; `groundNormal()` donne sa normale. Les contacts avec les
murs et le sol arrivent dans `onCollision(autre, debut)`.

## Dans l'éditeur

- Composants dans *Ajouter un composant › Physics 2D / Physics 3D*.
- Bouton **Collisions** de la vue : contours des formes (vert ; déclencheurs en ambre), bandes des
  tilemaps comprises.
- *Arrêter* le jeu restaure la scène : les corps sont recréés au prochain lancement.

## Depuis le code

```cpp
engine.plugins().addStatic(std::make_unique<agr::ScriptPlugin>());     // avant la physique
engine.plugins().addStatic(std::make_unique<agr::Physics2DPlugin>());
engine.plugins().addStatic(std::make_unique<agr::Physics3DPlugin>());
...
auto& world = engine.service<agr::Physics2DPlugin>()->world();
if (auto hit = world.raycast({x, y}, {0, -1}, 0.2f)) { /* au sol */ }
engine.events().subscribe<agr::Collision2DEvent>([](const agr::Collision2DEvent& e) { ... });
```

Les scripts Lua utilisent les mêmes fonctions : [SCRIPTING.md](SCRIPTING.md).

## Performances (machine de test, un cœur)

| Scène | Pas de simulation |
|---|---|
| 2D : 1 000 boîtes qui s'empilent | ~2,8 ms |
| 3D : 1 000 boîtes qui s'empilent | ~10 ms |

La simulation 3D est **mono-thread** (déterministe sur une plateforme donnée) ; une version
multi-thread branchée sur le `JobSystem` du moteur est une optimisation possible. Mesures sur téléphones :
phase 8.

## Limites actuelles

- 3D : pas de formes **maillage** ni **enveloppe convexe** (niveaux importés à approximer avec des boîtes),
  pas de *heightfield* ; frottement, rebond et couche **par corps** (ceux du premier collider) ; un corps
  qui a au moins un déclencheur est entièrement un capteur.
- 2D : pas de polygones quelconques ni de chaînes (contours lisses) ; pas de tuiles en pente ni de tuiles
  traversables par le dessous (*one-way*) ; pas de contrôleur de personnage 2D dédié (un corps dynamique à
  rotation bloquée piloté par script, comme dans les modèles).
- Pas de requêtes de chevauchement de formes (seulement les rayons).
- Articulations : pas de roue (*wheel*), de poulie ni d'engrenage ; les butées 2D sont légèrement souples
  sous un choc (Box2D) ; la rupture ne regarde que la force, pas le couple.
- Personnage 3D : capsule verticale uniquement ; entre deux personnages, simple collision des capsules.
- Pas de *debug draw* des contacts ; pas de physique en mode édition (aperçu).
