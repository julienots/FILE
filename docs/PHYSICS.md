# Physique 2D et 3D (phase 6)

| | 2D | 3D |
|---|---|---|
| Moteur | [Box2D](https://github.com/erincatto/box2d) 3.1.1 | [Jolt Physics](https://github.com/jrouwe/JoltPhysics) 5.6.0 |
| Module / plugin | `agr::physics2d`, `Physics2DPlugin` | `agr::physics3d`, `Physics3DPlugin` |
| Corps | `RigidBody2D` | `RigidBody` |
| Formes | `Collider2D` (boîte, cercle, capsule, **polygone**, **chaîne**) ; `TilemapCollider2D` | `Collider` (boîte, sphère, capsule, **maillage**, **enveloppe convexe**) |
| Personnage | `CharacterController2D` (glisser sur les murs et les pentes) | `CharacterController3D` |
| Plan / unités | XY, rotation autour de Z ; mètres, kg, secondes | mètres, kg, secondes |

Les deux mondes suivent le même modèle, couvert par des tests automatisés (`tests/test_physics2d.cpp`,
`tests/test_physics3d.cpp`, test de jeu des modèles, test de bout en bout de l'éditeur).

## Formes maillage et convexes (3D)

`Collider.shape` = **Mesh** : les triangles exacts d'un modèle importé (glTF, OBJ, FBX), pour les
niveaux, bâtiments et décors statiques ou cinématiques. **Convex** : l'enveloppe convexe de ses
sommets, pour les objets qui bougent (caisses, rochers qui roulent, véhicules).

- Le modèle est `Collider.model` / `meshIndex` ; vides (-1), ce sont ceux du `MeshRenderer` de la même
  entité : il suffit d'ajouter un `Collider` en *Mesh* à un objet importé.
- Un corps **dynamique** ne peut pas utiliser de maillage exact (comme dans Godot et Jolt) : il prend
  automatiquement l'enveloppe convexe du même maillage. Sa masse vient du volume de l'enveloppe et de
  `density`.
- La géométrie est importée une fois par `Physics3DPlugin` (fichier lu par le système de fichiers du
  moteur, mis en cache) ; hors moteur, `Physics3DWorld::setCollisionMeshSource` fournit les triangles.
- Échelle de l'entité appliquée aux sommets ; raycasts, déclencheurs, couches et événements comme les
  autres formes. Un modèle ou un maillage introuvable est ignoré avec un avertissement (une fois).

## Polygones, chaînes et plateformes traversables (2D)

- `Collider2D.shape` = **Polygon** : `points` (unités locales), n'importe quel polygone simple. Convexe
  et jusqu'à 8 points : une seule forme ; concave (un L, un escalier, un rocher découpé) : découpé
  automatiquement en triangles (*ear clipping*). Le sens des points est corrigé s'il est horaire.
- **Chain** : un contour lisse (sol vallonné, parois de grotte) ; un objet qui roule ou glisse ne
  « accroche » pas aux jonctions entre segments (sommets fantômes de Box2D). Solide des deux côtés quel
  que soit l'ordre des points ; `loop` ferme le contour. Pour les décors statiques.
- `oneWay` : **plateforme traversable par le dessous** (solide seulement depuis son +Y local) : on saute
  à travers depuis le dessous ou les côtés, on retombe dessus. Pour les corps dynamiques et les
  personnages.
- Les points s'éditent dans l'inspecteur (déplacer, insérer après un point, supprimer, ajouter à la
  fin) et depuis Lua : `e:set("Collider2D", "points", { {0, 0}, {2, 0}, {x = 1, y = 1} })`. Ils sont
  sauvegardés dans les deux formats de scène.

## Contrôleur de personnage 2D

`CharacterController2D` est l'équivalent du `CharacterBody2D` + `move_and_slide` de Godot : une
capsule verticale déplacée par sa `velocity`, sans corps dynamique à régler.

```lua
function Hero:onFixedUpdate(dt)
  local c = self.entity
  local v = c:get("CharacterController2D", "velocity")  -- {x =, y =}
  v.x = agr.input.axis("horizontal") * 6
  if c:get("CharacterController2D", "onFloor") and agr.input.pressed("jump") then v.y = 9 end
  c:set("CharacterController2D", "velocity", v)
end
```

- À chaque pas : gravité (`gravityScale`), plans de contact autour de la capsule, résolution
  (`b2SolvePlanes`), balayage sans traverser, 5 itérations ; la vitesse perd ce qui entre dans les murs
  et les sols. Sur un sol (pente ≤ `maxSlope`), le personnage ne glisse pas à l'arrêt et
  `snapDistance` le garde collé en descendant une pente ou une marche.
- État après le pas : `onFloor`, `onWall`, `onCeiling`, `floorNormal`.
- **Plateformes mobiles** : debout sur un corps cinématique ou dynamique, il suit son mouvement.
  **Traversables** (`oneWay`) : il passe au travers en montant et se pose dessus en retombant.
- **Caisses** : `pushForce` > 0, il les pousse (son corps cinématique les écarte) ; 0, elles bloquent
  comme des murs. Les déclencheurs (pièces, zones), les rayons et les requêtes le voient ; deux
  personnages se traversent.

## Requêtes de formes

| | 2D (`Physics2DWorld`) | 3D (`Physics3DWorld`) | Lua |
|---|---|---|---|
| Chevauchement | `overlap(QueryShape2D, position, angle, masque, déclencheurs)` | `overlap(QueryShape3D, position, rotation, masque, déclencheurs)` | `agr.physics2d.overlapCircle / overlapBox`, `agr.physics3d.overlapSphere / overlapBox` |
| Lancer de forme | `castShape(forme, position, angle, direction, distance, masque)` | `castShape(...)` | `agr.physics2d.castCircle / castBox`, `agr.physics3d.castSphere` |

Le chevauchement rend chaque entité touchée une fois (zones d'explosion, attaques au corps à corps,
« y a-t-il de la place ici ? ») ; les déclencheurs n'y sont qu'à la demande. Le lancer rend le premier
obstacle (entité, point, normale, distance parcourue) d'une forme qui se déplace : projectiles épais,
anticipation d'un saut ; une forme qui chevauche déjà un collider au départ l'ignore.

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


**Collisions par tuile** (comme les couches physiques d'un TileSet de Godot) : `emptyTiles` (`"4,5"`)
n'ont aucune collision (herbe, fleurs, décor), `triggerTiles` (`"30,31"`) sont des **déclencheurs**
(pointes, eau, échelles, drapeaux : on les traverse, `onTrigger(other, began)` est appelé sur la tilemap
et sur l'entité), `shapeTiles` (`"7:0,0,1,0.5; 8:0.4,0,0.2,1"`) donnent à une tuile **sa propre boîte**
en fractions de la case depuis son coin bas-gauche (demi-bloc, poteau, rebord). Test : une caisse se pose
sur une tuile pleine et sur un demi-bloc à la bonne hauteur, traverse le décor, traverse les pointes en
les signalant.

`TilemapCollider2D.oneWayTiles` (`"12,13"`) : ces tuiles sont des **plateformes traversables** (on saute
au travers par dessous, on se pose dessus) ; `slopeTiles` (`"20:/,21:\"`) : **pentes** à 45°, `/`
montant vers la droite, `\` vers la gauche (un triangle par cellule) ; `/1` et `/2` (`\1`, `\2`) sont la
moitié basse et la moitié haute d'une **pente douce** sur deux cellules (22,5°). Les tuiles pleines voisines sont
toujours fusionnées en rangées, chaque sorte à part.

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
| `Pulley` (3D) | ascenseur à contrepoids, palan : `longueurA + ratio × longueurB` constante, la corde passe par `pulleyPoint` (au-dessus de ce corps) et `connectedPulleyPoint` (au-dessus du corps relié, obligatoire) ; `jointValue` = longueur totale | `enableLimits` : longueur min / max (`lowerLimit` / `upperLimit`) |
| `SwingTwist` (3D) | articulations de ragdoll (épaule, hanche, cou) : balancement dans un cône de demi-angle `swingLimit` autour de `axis`, torsion entre `lowerAngle` et `upperAngle` | sous un choc très violent la limite cède un peu (~6° pour 300 N·s sur un membre léger) puis revient |

- `anchor` est dans le repère de l'entité. Avec `autoConnectedAnchor`, l'autre ancrage est le même point
  du monde à la création ; sinon `connectedAnchor` est dans le repère du corps connecté (ou un point du
  monde sans corps connecté).
- Les deux corps reliés ne se touchent plus, sauf avec `collideConnected`.
- Le **moteur** (`enableMotor`, `motorSpeed`, `maxMotorForce`) se pilote en direct depuis un script
  (`e:set("Joint3D", "motorSpeed", 2)`), sans reconstruire l'articulation.
- `breakForce` (N) : au-delà, l'articulation casse, `broken` passe à vrai et le script de l'entité reçoit
  `onJointBreak(force)`. Décocher `broken` la reconstruit.
- `e:jointValue()` : angle de la charnière (radians), position de la glissière ou longueur de la corde.

## Véhicules (`Vehicle3D`)

Sur une entité avec un `RigidBody` dynamique et un `Collider` (la carrosserie) : 4 roues lancées en
rayons vers le sol avec **suspensions** (`suspensionMin` / `suspensionMax`, `suspensionFrequency`,
`suspensionDamping`), un **moteur** à boîte automatique (`engineTorque`, `maxRpm`), la **direction** sur
les roues avant (`maxSteerAngle`), des **freins** (`brakeTorque`) et un **frein à main** sur les roues
arrière (`handBrakeTorque`) ; traction avant, arrière ou intégrale (`frontDrive`, `rearDrive`) ;
`tireFriction` règle l'adhérence. Disposition : `wheelBase` (entre essieux), `trackWidth` (entre roues
gauche et droite), `wheelHeight` (hauteur des fixations), `wheelRadius`, `wheelWidth`. Le véhicule
avance vers son −Z local.

Les scripts le conduisent en écrivant `throttle` (−1 marche arrière … 1), `steering` (−1 gauche … 1),
`brake` (0 … 1) et `handBrake` ; la simulation écrit `speed` (m/s, négatif en marche arrière), `rpm` et
`gear`. Des enfants nommés **WheelFL, WheelFR, WheelRL, WheelRR** (avant / arrière, gauche / droite)
suivent leur roue : rotation, braquage, débattement. Basé sur le contrôleur de véhicule à roues de Jolt.
Test : la voiture se pose sur ses suspensions, avance, tourne et s'arrête au freinage.
*Créer › Véhicule* crée une voiture prête à rouler (carrosserie, `Vehicle3D`, quatre roues nommées) :
il reste à lui donner un script qui écrit `throttle` et `steering`.

**Types** (`kind`) : `Car` (voiture ; `axles` = 2 à 4 essieux pour les camions, l'essieu avant braque,
la propulsion entraîne tous les essieux arrière ; roues du milieu : `WheelML1`, `WheelMR1`...),
`Motorcycle` (deux roues en ligne, `WheelF` / `WheelR`, qui **penche dans les virages** et tient debout
à l'arrêt ; baisser son centre de gravité avec `RigidBody.centerOfMassOffset`, par exemple `(0, -0.3, 0)`)
et `Tracked` (chenilles : `axles` galets par côté, `WheelL0`... / `WheelR0`... ; `steering` ralentit la
chenille intérieure et, à l'arrêt, fait **tourner sur place**). Testés : un camion à 3 essieux avance, une
moto tient debout puis vire en penchant, un char pivote sur place puis avance.

**Centre de gravité** : `RigidBody.centerOfMassOffset` déplace le centre de masse de tout corps dynamique
(plus bas = plus stable : voitures, bateaux, culbuto).

## Eau et flottaison (`WaterVolume3D`)

Une boîte d'eau (`size`, centrée sur l'entité ; sa face du haut est la surface) : les corps dynamiques
dedans **flottent** selon la part immergée de leur forme (poussée d'Archimède de Jolt : bateaux, caisses,
tonneaux qui tanguent), sont freinés (`linearDrag`, `angularDrag`) et emportés par le courant (`flow`, m/s).
`buoyancy` : 1 = neutre, moins = coule lentement, plus = flotte plus haut. Les personnages et les corps
statiques ne sont pas concernés. Test : une caisse lâchée dans l'eau plonge, remonte et flotte, dérive avec
le courant et coule quand l'eau est désactivée. (Godot n'a pas d'équivalent intégré.)

## Corps mous (`SoftBody3D`)

Tissus, drapeaux, rideaux, capes, gelées et ballons simulés par les *soft bodies* de Jolt : ils
tombent, se plient, s'appuient sur les autres corps et les poussent. *Créer › Tissu (corps mou)* ou
*Balle molle*.

| Champ | Rôle |
|---|---|
| `shape` | `Cloth` (grille dans le plan XZ de l'entité, bord à −z), `Box` (gelée), `Sphere` (balle, ballon) |
| `size`, `resolution` | dimensions (échelle de l'entité comprise) et nombre de sommets par côté (anneaux pour une sphère) |
| `mass`, `stiffness`, `bendStiffness` | masse répartie sur les sommets ; résistance à l'étirement et au pliage (0..1) |
| `pressure` | formes fermées : > 0 gonfle (ballon), 0 = gelée qui s'affaisse |
| `pin` | tissu : `Edge` (le bord −z tient), `Corners`, `None` ; les sommets tenus **suivent l'entité** (un mât qui bouge emporte son drapeau) |
| `iterations`, `damping`, `gravityScale` | précision, frottement de l'air (0,5 par défaut : un tissu se pose au lieu de balancer), gravité |

Le maillage déformé est rendu avec le **matériau du `MeshRenderer`** de l'entité (couleur, texture,
`.agrmat`), les deux faces pour un tissu, avec ombres (`DynamicMeshComponent` : aussi utilisable pour
de la géométrie procédurale). Tests : un tissu de 2 m tenu par un bord pend à ~2 m sous son attache et
suit l'entité déplacée ; un ballon lâché de 3 m se pose sur le sol sans le traverser et reste rond ;
le rendu du maillage dynamique (deux faces, mises à jour) en OpenGL, OpenGL ES et Vulkan. Limites : pas
de déchirure, pas de collision du tissu avec lui-même, pas de fixation à un autre corps que l'entité.

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

La simulation 3D s'exécute sur le **`JobSystem` du moteur** (`Physics3DPlugin` branche le monde sur
`engine.jobs()` ; `Physics3DWorld::setJobSystem(nullptr)` revient au mono-thread). Jolt reste
**déterministe** quel que soit le nombre de threads ; seuls ses rappels de contact arrivent dans un ordre
variable, et le moteur les trie (débuts puis fins, par paire de corps) : positions et événements sont
identiques au bit près à une simulation mono-thread (test `physics3d`). Mesure : 4 000 boîtes qui tombent,
~7,1 ms par pas sur un thread, ~3,8 ms sur 3 *workers* + le thread principal. Mesures sur téléphones : phase 8.

## Limites actuelles

- 3D : pas de *heightfield* dédié pour les terrains importés (le `Terrain` du moteur a sa propre collision) ;
  maillage et enveloppe convexe : voir plus haut ; le maillage n'est pas rechargé si le fichier change en jeu,
  frottement, rebond et couche **par corps** (ceux du premier collider) ; un corps
  qui a au moins un déclencheur est entièrement un capteur.
- 2D : pentes de tilemap à 45° ou 22,5° (sur deux cellules) seulement ; deux
  `CharacterController2D` se bloquent, mais l'un ne pousse pas l'autre et on ne tient pas debout sur la
  tête arrondie d'un autre (on glisse à côté, comme une capsule dans Godot).
- Articulations : pas d'engrenage (les roues de voiture : `Vehicle3D`) ; véhicules : pas de remorque attelée ; flottaison : une surface plane (pas de vagues physiques) ; les butées 2D sont légèrement souples
  sous un choc (Box2D) ; la rupture ne regarde que la force, pas le couple.
- Personnage 3D : capsule verticale uniquement ; entre deux personnages, simple collision des capsules.
- Pas de *debug draw* des contacts ; pas de physique en mode édition (aperçu).
