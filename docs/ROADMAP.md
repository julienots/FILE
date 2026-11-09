# Feuille de route

Légende : ✅ fait, compilé et testé · 🟡 partiel · ⬜ à faire.
Rien n'est coché sans code compilé, tests qui passent et un exemple qui l'utilise.

## Phase 1 — Architecture et socle technique ✅

| Élément | État | Où |
|---|---|---|
| Build CMake modulaire, presets, dépendances figées | ✅ | `CMakeLists.txt`, `cmake/` |
| Détection plateforme/compilateur, types, assertions, `Result<T>` | ✅ | `engine/core` |
| Logs multi-sorties (console, fichier, tampon circulaire, logcat) thread-safe | ✅ | `Log.hpp` |
| Allocateurs heap / linéaire / pool / par image + statistiques mémoire par catégorie | ✅ | `Memory.hpp` |
| Handles générationnels | ✅ | `Handle.hpp` |
| Système de jobs (parallelFor, compteurs, soumission groupée) | ✅ | `JobSystem.hpp` |
| Bus d'événements typé (synchrone + file thread-safe) | ✅ | `EventBus.hpp` |
| Système de fichiers virtuel sécurisé (`assets://`, `user://`), écritures atomiques | ✅ | `FileSystem.hpp` |
| Bibliothèque mathématique + géométrie (frustum, AABB, rayons) | ✅ | `engine/math` |
| ECS sparse-set avec vues, hooks, accès effacé | ✅ | `engine/ecs` |
| Scène : hiérarchie ordonnée, transformations monde, duplication, UUID | ✅ | `engine/scene` |
| Réflexion des composants (base de l'inspecteur) | ✅ | `Reflection.hpp` |
| Sérialisation JSON des scènes et des fragments (copier/coller, prefabs) | ✅ | `SceneSerializer.hpp` |
| Moteur : boucle à pas fixe bornée, statistiques d'image, limiteur d'images | ✅ | `Engine.hpp` |
| Entrées : clavier, souris, multi-touch avec phases | ✅ | `Input.hpp`, `SdlPlatform.cpp` |
| Cycle de vie mobile (arrière-plan, mémoire faible) | 🟡 | géré dans le code ; non testé sur un appareil Android |
| Plateforme SDL3 : fenêtre, contexte OpenGL 3.3 / GLES 3.0 | 🟡 | testé sous Linux (X11, Mesa) ; Windows compilé uniquement en CI ; Android en phase 8 |
| Système de plugins statiques et dynamiques avec dépendances | ✅ | `PluginManager.hpp`, `plugins/spin` |
| Tests unitaires (58 cas), sanitizers, benchmarks, CI | ✅ | `tests/`, `benchmarks/`, `.github/workflows` |
| Identité visuelle (logo, palette, typographie) | ✅ | `docs/brand` |

## Phase 2 — Renderer et ressources ⬜

- RHI + backend OpenGL ES 3.0 / GL 3.3, puis backend Vulkan 1.1 ([ADR 0004](adr/0004-graphics-backends.md)).
- Chaîne de shaders GLSL ➜ SPIR-V ➜ GLSL ES.
- Gestionnaire de ressources : handles, comptage de références, chargement asynchrone via le
  `JobSystem`, rechargement à chaud dans l'éditeur.
- Textures (PNG, JPG, WEBP, SVG), sprite batcher, maillages, caméra.
- Profils graphiques (Ultra ➜ Ultra mobile) et détection des capacités.
- Compteurs du profiler : draw calls, triangles, temps GPU (requêtes de timestamp).

## Phase 3 — Éditeur visuel ⬜

Application `agr_editor` (Dear ImGui docking + thème AGR) : assistant de création de projet,
viewport 2D/3D avec gizmos, Hierarchy (glisser-déposer, verrouillage, masquage), Inspector
généré par la réflexion, Asset Browser, Console (déjà alimentée par `RingBufferSink`), undo/redo,
mode Play/Pause/Stop sur une copie de la scène.

## Phase 4 — 2D ⬜
Sprites, sprite sheets, tilemaps, calques, caméras 2D, parallaxe, animation par images et
squelettique, particules, éclairage et ombres 2D, canvas UI, gestion des résolutions mobiles.

## Phase 5 — 3D ⬜
Import glTF/GLB/OBJ/FBX, matériaux PBR avec aperçu, squelettes et animations, lumières, ombres,
skybox, terrain, eau, végétation, post-traitement, culling (frustum, occlusion), LOD, instancing.

## Phase 6 — Physique et scripting ⬜
Jolt (3D) et Box2D v3 (2D) reliés à l'ECS ; Lua/Luau avec liaisons générées depuis la réflexion ;
éditeur de logique visuelle (événements, conditions, variables, actions, boucles, minuteurs).

## Phase 7 — Animation, audio, UI ⬜
Timeline et courbes, machines à états, mixage audio avec bus et audio spatial (miniaudio),
UI Builder avec ancres et mise en page responsive.

## Phase 8 — Android ⬜
Projet Gradle généré, NDK, export APK/AAB, signature, permissions, capteurs, vibrations,
sauvegarde locale, aperçu mobile dans l'éditeur, mesures sur appareils réels.

## Phase 9 — Templates, outils, plugins ⬜
Templates jouables (platformer 2D, RPG 2D/3D, tower defense, cartes, idle, racing…),
éditeurs de terrain / matériaux / particules / shaders, navmesh, localisation, gestionnaire de paquets.

## Phase 10 — Tests, optimisation, livraison ⬜
Format de scène binaire, empaquetage des assets, profilage sur appareils, installeur de l'éditeur,
jeu mobile de démonstration et APK.
