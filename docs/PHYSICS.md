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

- Pas de **joints** (charnières, ressorts, cordes), ni de **contrôleur de personnage** dédié (le héros de la
  Vitrine 2D est un corps dynamique piloté par script).
- 3D : pas de formes **maillage** ni **enveloppe convexe** (niveaux importés à approximer avec des boîtes),
  pas de *heightfield* ; frottement et rebond **par corps** (ceux du premier collider) ; un corps qui a au
  moins un déclencheur est entièrement un capteur.
- 2D : pas de polygones quelconques ni de chaînes (contours lisses) ; pas de tuiles en pente ni de tuiles
  traversables par le dessous (*one-way*).
- Pas de couches / masques de collision configurables (tout entre en collision avec tout) ni de requêtes
  de chevauchement de formes.
- Pas de *debug draw* des contacts ; pas de physique en mode édition (aperçu).
