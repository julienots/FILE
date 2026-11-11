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
| Tests unitaires, sanitizers, benchmarks, CI | ✅ | `tests/`, `benchmarks/`, `.github/workflows` |
| Identité visuelle (logo, palette, typographie) | ✅ | `docs/brand` |

## Phase 2 — Renderer et ressources ✅

| Élément | État | Où |
|---|---|---|
| RHI neutre (buffers, textures, pipelines, cibles, passes, blits, handles générationnels) | ✅ | `engine/rhi/include/agr/rhi/Rhi.hpp` |
| Backend OpenGL 3.3 core / OpenGL ES 3.0, chargeur intégré | ✅ | `engine/rhi/src/gl` — testé sur Mesa en GL et GLES |
| Dialecte de shaders unique GL / GLES, erreurs de compilation détaillées | ✅ | `RENDERING.md` |
| Backend Vulkan 1.1 (volk, VMA, glslang), sélection automatique et repli vers OpenGL | ✅ | `engine/rhi/src/vk` — tests GPU avec *validation layers*, parité d'image avec GL |
| Gestionnaire de ressources : cache, références, chargement asynchrone, budget d'upload, rechargement à chaud | ✅ | `engine/assets` |
| Images PNG / JPG / TGA / BMP / PSD / GIF / HDR | ✅ | stb_image |
| Images WEBP, SVG | ⬜ | libwebp, nanosvg |
| Caméras perspective / orthographique, lumière directionnelle, ambiance | ✅ | `RenderComponents.hpp` |
| Maillages intégrés (cube, sphère, plan, quad) éclairés (Lambert + Blinn-Phong) | ✅ | PBR en phase 5 |
| Sprites : tri par calque/ordre/profondeur, lots par texture, sprite sheets (rectangle UV), miroir, pixel art | ✅ | `Renderer.cpp` |
| Frustum culling | ✅ | |
| Profils graphiques (échelle 3D, MSAA, taille de texture, limite FPS, budget d'upload) + sélection automatique | ✅ | `GraphicsProfile.hpp` |
| Statistiques : draw calls, triangles, culling, lots, temps CPU, temps GPU, mémoire GPU | ✅ | `RenderStats` |
| Captures d'écran PNG | ✅ | `RenderPlugin::requestScreenshot` |
| Tests GPU au pixel près (16 cas × OpenGL, OpenGL ES et Vulkan), benchmarks du renderer | ✅ | `tests/gpu`, `benchmarks/gpu` |
| Instancing, LOD, occlusion culling | ⬜ | phase 5 |
| Recréation des ressources après perte du contexte Android | ⬜ | phase 8 |

## Phase 3 — Éditeur visuel 🟡 (fondations complètes ; outils spécialisés dans les phases suivantes)

| Élément | État | Où |
|---|---|---|
| Hub de projets : projets récents, assistant (2D / 2.5D / 3D, modèle, nom, emplacement, cibles) | ✅ | `editor/ui/src/EditorApp.cpp` |
| Format de projet (dossier, `project.agrproject`, assets, scènes) | ✅ | `editor/core` |
| Interface Dear ImGui (docking) rendue par la RHI : Vulkan, OpenGL, OpenGL ES | ✅ | `ImGuiRenderer` |
| Thème AGR, polices Inter / JetBrains Mono, icônes vectorielles | ✅ | `Theme.cpp` |
| Vue de scène 3D et 2D, caméra d'édition (orbite, survol, panoramique, zoom, cadrage) | ✅ | `ViewportPanel` |
| Vue 2.5D dédiée | ⬜ | un projet 2.5D utilise la vue 3D |
| Gizmos déplacer / pivoter / échelle, local / monde, magnétisme, multi-sélection | ✅ | ImGuizmo |
| Grille adaptative, sélection par clic, icônes caméras / lumières, vue jeu | ✅ | |
| Hiérarchie : création, parentage, réordonnancement, renommage, masquage, verrouillage, suppression, glisser-déposer, recherche, groupes / dossiers | ✅ | `HierarchyPanel` |
| Inspecteur généré par la réflexion, ajout / retrait de composants (plugins compris) | ✅ | `InspectorPanel` |
| Explorateur d'assets : dossiers, vignettes, recherche, import, renommer, supprimer avec confirmation | ✅ | `AssetBrowserPanel` |
| Console, statistiques | ✅ | |
| Annuler / rétablir, indicateur de modifications, confirmation avant perte | ✅ | `UndoStack` (instantanés) |
| Jouer / pause / arrêter dans l'éditeur | ✅ | `EditorContext::play` |
| Tests : logique d'édition (11 cas) + test de bout en bout par entrées réelles sur 3 API, sous ASan | ✅ | `tests/test_editor.cpp`, `--smoke-test` |
| Prefabs en fichiers, éditeurs de matériaux / shaders / terrain / particules / animation / UI | ⬜ | phases 4, 5, 7, 9 |
| Panneaux détachables en fenêtres système | ⬜ | rendu multi-fenêtres à ajouter à la RHI |

## Phase 4 — Systèmes 2D 🟡 (cœur 2D terminé ; physique, squelettes, UI et ombres dans d'autres phases)

Guide : [2D.md](2D.md).

| Élément | État | Où |
|---|---|---|
| Feuilles de sprites `.agrsheet` (frames, pivots, clips), cache, rechargement à chaud, validation | ✅ | `render/SpriteSheet` |
| Découpe en grille et atlas (empaquetage, bordures répétées) depuis l'explorateur d'assets | ✅ | `twod/Atlas`, `AssetBrowserPanel` |
| Sprites sur frames de feuille, pivot par frame, sélection de frame et taille native dans l'inspecteur | ✅ | `Renderer`, `InspectorPanel` |
| Animation image par image (`SpriteAnimator`) | ✅ | `twod/SpriteAnimator` |
| Tilemaps : rendu limité aux cellules visibles, sérialisation RLE, redimensionnement | ✅ | `TilemapComponent`, `SceneSerializer` |
| Peinture de tilemaps dans la vue : pinceau, gomme, rectangle, remplissage, pipette, palette, annulation par geste | ✅ | `ViewportPanel`, `twod/TilemapEditing` |
| Parallaxe | ✅ | `ParallaxComponent` |
| Lumières 2D ponctuelles (16 visibles), sprites / tilemaps éclairés | ✅ | `Light2DComponent`, shader `sprite.lit` |
| Particules 2D (CPU), mélange additif, aperçu en mode édition | ✅ | `twod/Particles2D` |
| Modes d'échelle pour écrans mobiles (hauteur / largeur fixe, tout voir, rogner) | ✅ | `CameraComponent::scaleMode` |
| Menu Créer › 2D (Tilemap, Particules 2D, Lumière 2D, Couche de parallaxe), dépôt de feuilles dans la scène | ✅ | éditeur |
| Modèle « Vitrine 2D » avec art original reproductible | ✅ | `SceneTemplates`, `tools/generate_template_2d_art.py` |
| Tests : 9 unitaires 2D + 5 éditeur, 6 GPU au pixel près × 3 API, test de bout en bout (peinture à la souris, animation en jeu) | ✅ | `tests/test_twod.cpp`, `tests/gpu/test_renderer2d.cpp` |
| Collisions / physique 2D, formes de collision des tilemaps | ⬜ | phase 6 |
| Animation squelettique 2D, timeline | ⬜ | phase 7 |
| Canvas UI | ⬜ | phase 7 |
| Ombres 2D, normal maps 2D | ⬜ | non planifié en détail |
| Tuiles animées, autotiling, éditeur visuel de feuilles et de particules | ⬜ | phase 9 |

## Phase 5 — 3D 🟡 (rendu terminé : PBR, squelettes, ombres en cascades, post-traitement HDR, ciel en image + IBL ; terrain, eau, végétation, LOD et occlusion à venir)

Guide : [3D.md](3D.md).

| Élément | État | Où |
|---|---|---|
| Import glTF 2.0 / GLB (cgltf), OBJ + MTL, FBX (ufbx) : maillages, hiérarchie, matériaux, textures intégrées | ✅ | `assets/Model` |
| Normales et tangentes générées, avertissements d'import explicites | ✅ | `assets/Model` |
| Chargement asynchrone des modèles, upload GPU, rechargement à chaud, `instantiateModel` | ✅ | `render/ModelLibrary` |
| Matériaux PBR métal / rugosité `.agrmat` (couleur, métal-rugosité, normal, occlusion, émission, alpha mask / blend, double face, sans éclairage) | ✅ | `assets/Material`, shaders PBR |
| Lumière directionnelle avec ombres (carte stable, PCF selon le profil), points et projecteurs (8 max) | ✅ | `Renderer3D.cpp` |
| Ciel procédural, ambiance hémisphérique, brume, exposition, tone mapping ACES | ✅ | |
| Instanciation automatique (flux par instance sur les 3 API) | ✅ | RHI `instanceLayout`, renderer |
| Éditeur : glisser un modèle dans la scène, éditeur de matériau avec aperçu, lumières dans les menus | ✅ | `editor/ui` |
| Modèle « Vitrine 3D » avec ressources originales reproductibles | ✅ | `SceneTemplates`, `tools/generate_template_3d_art.py` |
| Tests : importeurs (5 cas), 6 cas GPU 3D × 3 API, éditeur, test de bout en bout | ✅ | `tests/test_models.cpp`, `tests/gpu/test_renderer3d.cpp` |
| Squelettes, skinning, animations des modèles | ✅ | glTF skins + clips, skinning GPU, testés sur les 3 API |
| Post-traitement HDR : bloom, FXAA, étalonnage, vignettage (selon le profil) | ✅ | `Renderer.cpp`, composant `PostProcess` |
| Ciel en image (.hdr / .png équirectangulaire) et IBL (harmoniques sphériques + mips préfiltrés) | ✅ | `assets/Environment`, `render/EnvironmentLibrary` |
| SSAO, profondeur de champ, TAA, sondes de réflexion locales | ⬜ | à planifier |
| Ombres en cascades (1 à 4 selon le profil, atlas, fondu entre cascades) | ✅ | `Renderer3D.cpp` |
| Ombres des lumières locales | ⬜ | à planifier |
| Terrain, eau, végétation, LOD, occlusion culling | ⬜ | phase 9 (éditeurs) / à planifier |

## Phase 6 — Physique et scripting 🟡 (physique 2D / 3D et Lua terminés ; éditeur de logique visuelle non réalisé)

Guides : [PHYSICS.md](PHYSICS.md), [SCRIPTING.md](SCRIPTING.md).

| Élément | État | Où |
|---|---|---|
| Physique 2D Box2D v3 : corps statiques / cinématiques / dynamiques, boîtes, cercles, capsules, composés, déclencheurs, tilemaps solides, rayons, forces | ✅ | `agr::physics2d` |
| Physique 3D Jolt : corps, boîtes, sphères, capsules, composés, déclencheurs, masse imposée, rotation bloquée, rayons, forces | ✅ | `agr::physics3d` |
| Synchronisation scène ↔ physique (téléportations, plateformes cinématiques, reconstruction à l'édition), événements de contact | ✅ | |
| Scripts Lua 5.4 : cycle de vie, propriétés dans l'inspecteur, API entité / composants (réflexion) / entrées / temps / physique, bac à sable, chien de garde, erreurs localisées, rechargement à chaud | ✅ | `agr::script` |
| Éditeur : scripts et physique en mode Jeu, inspecteur de script, nouveau script, glisser un script sur un objet, affichage des collisions | ✅ | `editor/ui` |
| Modèles jouables : Vitrine 2D (plateforme, clavier et tactile), Vitrine 3D (lancer de balles) | ✅ | `SceneTemplates`, `assets/scripts` |
| Tests : 7 cas 2D, 4 cas 3D, 10 cas scripts, jeu des modèles sans écran, mode Jeu piloté au clavier dans le test de bout en bout | ✅ | `tests/` |
| **Éditeur de logique visuelle** (événements, conditions, variables, actions, boucles, minuteurs) | ⬜ | **non réalisé** : à reprendre |
| Joints, contrôleur de personnage, formes maillage / convexes 3D, couches de collision | ⬜ | à planifier |
| Liaisons Luau / typage, débogueur de scripts | ⬜ | à planifier |

## Phase 7 — Animation, audio, UI 🟡 (audio, interface et animation de propriétés terminés ; squelettes, effets audio et widgets avancés à venir)

Guides : [AUDIO.md](AUDIO.md), [UI.md](UI.md), [ANIMATION.md](ANIMATION.md).

| Élément | État | Où |
|---|---|---|
| Audio miniaudio : WAV / Ogg / MP3 / FLAC, clips décodés ou en flux, voix (volume, hauteur, panoramique, boucle, fondus, limite) | ✅ | `agr::audio` |
| Bus (Master, Music, Effects, Ambient, Interface, Voice), coupure, pause du jeu, vumètre | ✅ | `AudioSystem` |
| Audio spatial (atténuation, panoramique) relatif à `AudioListener` ou à la caméra ; `AudioSource` piloté par la scène | ✅ | |
| Sortie absente (serveur, CI) : mixage silencieux en temps réel ; arrière-plan mobile : sortie suspendue | ✅ | testé sous Linux ; Android en phase 8 |
| Interface : canvas adaptatif (résolution de référence, largeur / hauteur), ancres, pivot, images 9-slice, textes TrueType UTF-8, boutons, curseurs, colonnes / lignes / grilles | ✅ | `agr::ui` |
| Pointeurs souris et multi-touch (plusieurs boutons maintenus), événements vers les scripts, `pointerOverUI` | ✅ | `UISystem`, `UIPlugin` |
| Rendu de l'interface en surimpression (pleine résolution, même avec échelle de rendu / MSAA) sur Vulkan, OpenGL, OpenGL ES | ✅ | `Renderer` (overlay) |
| Clips de propriétés : courbes (constante, linéaire, lissée, tangentes), tout champ réfléchi, enfants, événements | ✅ | `agr::animation` |
| Machines à états : paramètres, déclencheurs, conditions, tout état, instant de sortie, fondus, clips de sprite | ✅ | `AnimationSystem` |
| API Lua : `agr.audio`, `agr.ui`, textes, `setActive`, paramètres d'animation, rappels `onClick`, `onValueChanged`, `onAnimationEvent`, `onStateEnter` | ✅ | `agr::script` |
| Éditeur : UI Builder dans la vue Jeu, préréglages d'ancres, timeline, machine à états, mixeur, aperçu audio | ✅ | `editor/ui` |
| Modèles : sons générés, musique, HUD, boutons tactiles, héros à machine à états, objets animés | ✅ | `SceneTemplates`, `tools/generate_template_audio.py` |
| Tests : 7 audio, 6 interface, 6 animation, 3 scripts, 2 GPU (surimpression), modèles et bout en bout | ✅ | `tests/` |
| Animation squelettique (skinning glTF) | ✅ | voir ANIMATION.md |
| Arbres de mélange, couches, IK, morph targets | ⬜ | à planifier |
| Effets audio (réverbération, filtres), Doppler, occlusion | ⬜ | à planifier |
| UI : masques et défilement, champs de saisie, cases à cocher, listes, texte enrichi, zones sûres (encoches), navigation manette | ⬜ | zones sûres en phase 8 ; le reste à planifier |
| Annuler / rétablir dans la timeline et la machine à états, sélection multiple de clés | ⬜ | à planifier |
| **Éditeur de logique visuelle** (reporté de la phase 6) | ⬜ | **non réalisé** |

## Phase 8 — Android 🟡 (export APK / AAB et fonctions mobiles terminés ; aucun essai sur appareil réel)

Guide : [EXPORT.md](EXPORT.md).

| Élément | État | Où |
|---|---|---|
| Lecteur de jeu (`agr_player`) : exécute un jeu exporté avec tous les plugins ; exécutable PC, `libmain.so` Android | ✅ | `apps/player` |
| Format de jeu exporté (`game.json`, assets, scènes), lecture dans l'APK (`SdlFileProvider`) | ✅ | `GameManifest`, `agr::platform` |
| Export PC (dossier) depuis l'éditeur et en ligne de commande, vérifié par le test automatique du lecteur | ✅ | `GameExport`, test `agr_player_*` |
| Compilation croisée NDK (arm64-v8a, x86_64, API 26+, pages de 16 Ko) et modèles d'export | ✅ | presets `android-*`, `tools/build_android_templates.sh` |
| Projet Gradle généré (manifeste, activité, icônes, permissions, orientation), APK debug, APK signé, AAB | ✅ | `AndroidExport` ; vérifiés par `aapt2`, `apksigner`, `zipalign` |
| Signature : création de keystore, mots de passe jamais enregistrés | ✅ | |
| Fenêtre d'export de l'éditeur, export en arrière-plan avec journal ; job Android en CI | ✅ | `ExportPanel`, `.github/workflows/ci.yml` |
| Fonctions mobiles : sauvegarde locale, vibration, accéléromètre, gyroscope, zones sûres, orientation, écran allumé, bouton Retour | ✅ | `SaveStore`, `SdlPlatform`, API Lua |
| Arrière-plan : son suspendu, sauvegarde écrite, surface Vulkan libérée puis recréée | ✅ | testé sur bureau (libération / recréation de surface) ; pas sur appareil |
| Aperçu mobile dans l'éditeur (formats, zones sûres simulées, souris = doigt) | ✅ | vue Jeu |
| **Exécution sur appareil réel / émulateur, mesures** (démarrage, FPS, mémoire, batterie, thermique) | ⬜ | **non réalisé** : pas d'appareil ni de virtualisation dans l'environnement de développement |
| Icônes adaptatives, écran de démarrage, compression de textures ETC2 / ASTC, Play Asset Delivery | ⬜ | à planifier |
| Lancement direct sur un appareil connecté (adb) depuis l'éditeur | ⬜ | à planifier |

## Phase 9 — Templates, outils, plugins ⬜
Templates jouables (platformer 2D, RPG 2D/3D, tower defense, cartes, idle, racing…),
éditeurs de terrain / matériaux / particules / shaders, navmesh, localisation, gestionnaire de paquets.

## Phase 10 — Tests, optimisation, livraison ⬜
Format de scène binaire, empaquetage des assets, profilage sur appareils, installeur de l'éditeur,
jeu mobile de démonstration et APK.
