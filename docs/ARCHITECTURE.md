# Architecture d'AGR ENGINE

## Principes

1. **Modules indépendants, dépendances à sens unique.** Un module ne connaît que les modules
   situés en dessous de lui. Aucun cycle n'est possible : CMake refuserait de lier.
2. **L'éditeur est un client du moteur, pas l'inverse.** Le runtime embarqué dans un jeu Android
   ne contient aucune ligne de code de l'éditeur. La plateforme qui *héberge l'éditeur*
   (Windows/Linux) et celle qui *exécute le jeu* (Android, Windows, Linux) sont distinctes.
3. **Données décrites une seule fois.** La réflexion des composants alimente l'inspecteur, la
   sérialisation, et plus tard le scripting et l'éditeur de logique visuelle.
4. **Mobile d'abord.** Pas d'allocation dans les boucles chaudes, données contiguës, nombre de
   threads adapté aux CPU big.LITTLE, pause du rendu en arrière-plan, limiteur d'images pour la batterie.
5. **Erreurs explicites.** `Result<T>` / `Status` plutôt que des exceptions dans le moteur ;
   un fichier corrompu produit un message, pas un crash.

## Graphe des modules (phase 1)

```
                 apps/sandbox            plugins/spin (bibliothèque dynamique)
                      │                         │
               agr::platform (SDL3)             │
                      │                         │
                      └──────► agr::runtime ◄───┘
                                    │
                               agr::scene ──► nlohmann/json
                              ┌─────┴─────┐
                          agr::ecs    agr::math
                              └─────┬─────┘
                                agr::core
```

| Module | Type | Rôle |
|---|---|---|
| `agr::core` | statique | Types, assertions, `Result`, logs (console, fichier, tampon circulaire pour la console de l'éditeur, logcat), allocateurs (heap, linéaire, pool, double tampon par frame) avec statistiques par catégorie, handles générationnels, `JobSystem`, `EventBus`, système de fichiers virtuel, UUID, chargement de bibliothèques dynamiques |
| `agr::math` | en-têtes | Vec2/3/4, Quat, Mat4 (colonne majeure, main droite, +Y en haut), perspective/ortho avec plage de profondeur GL ou Vulkan, AABB, rayon, plan, frustum |
| `agr::ecs` | en-têtes | ECS à *sparse sets* : `Registry`, `Pool<T>`, `View<Ts...>`, hooks de construction/destruction, accès typé ou effacé |
| `agr::scene` | statique | `Scene` (hiérarchie, transformations monde, UUID persistants, duplication), réflexion des composants, sérialisation JSON (`.agrscene`) et fragments (copier/coller, futurs prefabs) |
| `agr::runtime` | statique | `Engine` (services + boucle), `Application`, `EngineConfig`, `Input`, événements moteur, `PluginManager`, interface `PlatformLayer` |
| `agr::platform` | statique | Implémentation SDL3 de `PlatformLayer` : fenêtre, contexte OpenGL 3.3 core / OpenGL ES 3.0, clavier, souris, multi-touch, cycle de vie mobile |

Modules prévus (voir la [feuille de route](ROADMAP.md)) : `agr::rhi` + backends `vulkan` / `gles`,
`agr::render`, `agr::assets`, `agr::physics2d`, `agr::physics3d`, `agr::audio`, `agr::script`,
`agr::ui`, `agr::animation`, puis l'application `agr_editor`.

## Boucle principale

```
Engine::run
 ├─ Input::beginFrame()                    états « pressé ce frame », deltas souris/touch
 ├─ PlatformLayer::pollEvents()            événements OS ➜ Input + EventBus (synchrone)
 ├─ EventBus::dispatchQueued()             événements postés par d'autres threads
 ├─ pas fixes (0..maxFixedStepsPerFrame)   plugins puis application ; physique en phase 6
 ├─ update variable                        plugins puis application
 ├─ Scene::flushDestroyed()                destructions différées
 ├─ Scene::updateTransforms()              matrices monde
 ├─ onRender + present                     seulement si la plateforme peut rendre (pas en arrière-plan)
 └─ limiteur d'images (targetFps)          économise la batterie
```

Le delta est borné (`maxFrameDelta`) et le nombre de pas fixes par image est plafonné : un appareil
lent ralentit la simulation au lieu d'entrer dans la « spirale de la mort ».

Ordre de démarrage : logs ➜ jobs ➜ VFS ➜ plateforme ➜ scène ➜ plugins ➜ application.
L'arrêt se fait dans l'ordre inverse.

## ECS

Voir [ADR 0003](adr/0003-ecs-design.md). Points clés :

- `Entity` = index + version (8 octets). Une entité détruite rend toutes ses copies invalides.
- Chaque type de composant est stocké dans un tableau dense (`Pool<T>`), indexé via un tableau
  *sparse*. Ajout, retrait, test : O(1). Retrait par *swap-and-pop* : la mémoire reste contiguë.
- Une vue parcourt le plus petit des pools concernés.
- L'identifiant d'un type de composant est le hash de son **nom** ; il est donc identique dans
  l'exécutable et dans les plugins.

## Scène et hiérarchie

La hiérarchie est stockée dans `HierarchyComponent` sous forme de listes chaînées intrusives
(parent, premier/dernier enfant, frères). L'ordre des frères est préservé, ce qui permet le
glisser-déposer et le réordonnancement dans le panneau Hierarchy de l'éditeur.
`setParent` refuse les cycles et peut conserver la transformation monde.

## Réflexion et sérialisation

```cpp
ComponentRegistry::instance().add<RigidBody>("RigidBody")
    .category("Physics")
    .field("mass", &RigidBody::mass, {.rangeMin = 0.0f, .rangeMax = 10000.0f})
    .field("type", &RigidBody::type, {.enumEntries = {{"Static", 0}, {"Dynamic", 1}}});
```

Cette déclaration suffit pour que le composant soit sauvegardé, chargé, copié, et (phase 3)
affiché dans l'inspecteur et dans le menu « Add Component ». Les références entre entités
sont écrites sous forme d'UUID et résolues au chargement. Un composant inconnu (plugin absent)
est ignoré avec un avertissement : la scène reste chargeable.

## Système de fichiers virtuel

| Montage | Contenu | Accès |
|---|---|---|
| `assets://` | Données du jeu (dans l'APK sur Android) | lecture seule |
| `user://` | Sauvegardes, réglages | lecture / écriture |

Les chemins sont normalisés et tout `..` qui sortirait du point de montage est refusé.
Les écritures passent par un fichier temporaire puis un renommage : une coupure pendant une
sauvegarde ne corrompt pas le fichier existant.

## Threads

`JobSystem` : *workers* = cœurs − 1 sur desktop, plafonné à 4 sur mobile. `wait()` exécute des
jobs en attendant au lieu de bloquer, donc le parallélisme imbriqué ne provoque pas d'interblocage.
Pour un travail fin, utilisez `parallelFor` / `submitBatch` (un seul verrou et un seul réveil).
