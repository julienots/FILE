# AGR ENGINE — État du projet et reste à faire (passation)

Document destiné à la personne ou à l'IA qui reprend le développement. Le dépôt contient tout le
travail (branche `ccr-36023c54-c5ozsq`). Langue du projet : interface, docs et textes du jeu en
français ; code, commentaires et messages du moteur en anglais.

## 1. Le cahier des charges (résumé)

Moteur propriétaire C++20 « AGR ENGINE » (éditeur Superessence Studio, « Build Beyond Limits. »),
entre Godot et Unity : modulaire, Vulkan + OpenGL / GLES, CMake, SDL3, ECS, physique, scripts,
éditeur professionnel, 2D / 3D, priorité mobile Android, modèles de jeux, outils, docs, tests.
Développement en 10 phases. Règles strictes : **pas de faux boutons, pas de fonction simulée
présentée comme terminée, ne jamais supprimer de ressources sans autorisation, gestion d'erreurs,
tests automatisés, docs, benchmarks ; ne jamais dire qu'un module est fini s'il ne marche pas.**

## 2. Ce qui est fait (vérifié par des tests)

| Phase | État | Docs |
|---|---|---|
| 1 Architecture (core, math, ECS, scène, sérialisation, VFS, jobs, plugins) | ✅ | ARCHITECTURE.md |
| 2 Rendu (RHI OpenGL / GLES / Vulkan, renderer forward) | ✅ | RENDERING.md |
| 3 Éditeur (ImGui, hiérarchie, inspecteur, vue, gizmos, undo, assets, console, stats) | ✅ | EDITOR.md |
| 4 2D (sprites, feuilles, tilemaps, lumières 2D, parallaxe, particules) | ✅ | 2D.md |
| 5 3D (PBR, glTF/OBJ/FBX, ombres en cascades, IBL, post-traitement, squelettes, instanciation) | ✅ | 3D.md |
| 6 Physique 2D (Box2D) / 3D (Jolt), scripts Lua | ✅ (sauf éditeur de logique visuelle) | PHYSICS.md, SCRIPTING.md |
| 7 Animation, audio, UI | ✅ | ANIMATION.md, AUDIO.md, UI.md |
| 8 Android (player, export APK / AAB signés, mobile : zones sûres, capteurs, vibreur…) | ✅ | EXPORT.md |
| 9.1–9.4 Prefabs, entrées / manettes, localisation, navigation (A* 2D, Recast/Detour), terrain, eau, particules 3D, shaders personnalisés + éditeur | ✅ | PREFABS.md, INPUT.md, LOCALIZATION.md, NAVIGATION.md, TERRAIN.md, SHADERS.md |
| 9.5 Modèles de jeux : plateforme 2D, RPG 2D, RPG 3D, tower defense, cartes, simulation (ferme), stratégie, survie, course, idle, tycoon, multijoueur (arène, réseau UDP `agr.net`) | ✅ jouables et testés | NETWORK.md, ce document |

Livrables : `dist/AGR-ENGINE-Android.apk` (menu + 12 jeux, arm64, signé avec une clé de
démonstration) et `dist/AGR-Demo-Platformer.apk` (ces fichiers ne sont pas versionnés : `dist/`
est ignoré ; les recréer avec la section 5). **Les APK n'ont jamais été lancés sur un vrai
téléphone** (pas d'appareil ici) : à valider en priorité.

## 3. Ce qui reste à faire (par priorité)

1. **Validation sur appareil Android** réel (performances des modèles 3D, tactile, audio, réseau
   local de l'arène, retour arrière). Corriger ce qui casse.
2. **`docs/TEMPLATES.md`** : guide des 12 modèles (règles, scripts dans `assets/games/scripts/`,
   comment les modifier) ; mettre à jour `docs/ROADMAP.md` (lignes « Templates » de la phase 9 encore
   ⬜) et EDITOR.md (option `--template-gallery`).
3. **Phase 9.6 — gestionnaire de plugins / paquets et documentation intégrée** :
   - panneau éditeur listant les plugins chargés (`engine.plugins()`), activation par projet,
     chargement des plugins dynamiques (`AGR_DEFINE_PLUGIN`, voir PLUGINS.md) ;
   - format de paquet (zip d'assets + manifeste : nom, version, dépendances) importable dans un
     projet, avec vérification et désinstallation (jamais de suppression sans confirmation) ;
   - « marketplace » extensible : au minimum un index local / URL configurable, pas de faux service ;
   - documentation intégrée : afficher les fichiers `docs/*.md` dans un panneau (rendu Markdown
     simple) et lier l'aide contextuelle (Aide › …).
4. **Éditeur de logique visuelle (phase 6)** : graphe de nœuds (événements, conditions, actions,
   variables) compilé en Lua ou interprété ; panneau d'édition, sérialisation, tests.
5. **Outils avancés encore absents** : éditeur d'animation de particules par courbes, profiler
   détaillé par système (le panneau Statistiques existe), gestionnaire de build multi-cibles
   (l'export PC / Android existe), éditeur de shaders par nœuds (les shaders GLSL existent).
6. **Phase 10 — tests, optimisation, livraison** : format de scène binaire, empaquetage /
   compression des assets, profilage sur appareils, installeur de l'éditeur (Windows / Linux /
   macOS), CI (GitHub Actions) exécutant les suites, jeu mobile de démonstration finalisé (le menu
   des 12 modèles en tient lieu pour l'instant).
7. **Éditeur sur Android** (demande prioritaire de l'utilisateur : « le moteur sur mobile ») —
   **commencé, pas fonctionnel** : preset CMake `android-editor-arm64` et cible `agr_editor` compilée
   en `libmain.so` sur Android (`editor/CMakeLists.txt`). La compilation n'a pas encore été menée à
   terme. Reste : (a) faire compiler l'éditeur avec le NDK ; (b) dans `editor/app/main.cpp`, sous
   Android, extraire `editor_resources` (polices, modèles) des assets de l'APK vers
   `SDL_GetPrefPath` au premier lancement (liste de fichiers générée à l'empaquetage), dossier des
   projets dans le stockage interne ; (c) échelle d'interface adaptée au téléphone / tablette,
   orientation paysage, clavier virtuel (ImGui + SDL3) ; (d) masquer sur Android ce qui ne peut pas
   marcher (export Gradle, « Ouvrir dans l'explorateur », import par sélecteur natif) ou le
   remplacer ; (e) empaqueter l'APK de l'éditeur (réutiliser le générateur Gradle d'AndroidExport
   avec l'éditeur comme `libmain.so`) ; (f) tester sur appareil.

Limites connues documentées dans chaque fichier `docs/*.md` (sections « Limites actuelles »).
Points d'attention : `agr.loadScene` dans l'éditeur en mode Jeu remplace la scène jouée (prévu pour
le player) ; le template x86_64 Android n'est pas reconstruit dans `build/export_templates`.

## 4. Organisation du code

- `engine/<module>/` : core, math, ecs, scene, rhi (gl, vk), assets, runtime, render, twod, audio,
  ui, animation, physics2d, physics3d, navigation, net, script, platform. Chaque module :
  `include/agr/<module>/`, `src/`, `CMakeLists.txt` (`agr_add_module`).
- `editor/core` (logique testable : projet, modèles de scènes `SceneTemplates.cpp` et
  `games/*.cpp`, export), `editor/ui` (panneaux ImGui), `editor/app`.
- `editor/resources/templates/assets/` : assets copiés dans chaque nouveau projet (art original
  généré par `tools/generate_*.py`, scripts Lua des jeux dans `games/scripts/`, textes
  `localization.csv` fr / en / es).
- `apps/player` : le lecteur de jeux (PC et Android). `tests/` : doctest (unitaires, éditeur,
  modèles de jeux joués sans écran), `tests/gpu/` (rendu sur les 3 API).

## 5. Compiler et tester

```bash
cmake --preset debug && cmake --build --preset debug          # aussi : release, clang (-Werror), asan
Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
./build/debug/bin/agr_tests                                    # unitaires + modèles de jeux
AGR_TEST_API=gl ./build/debug/bin/agr_gpu_tests                 # gl, gles, vulkan
./build/debug/bin/agr_editor --vulkan --smoke-test /tmp/smoke   # test de bout en bout de l'éditeur
./build/debug/bin/agr_editor --opengl --template-gallery /tmp/g # capture des 12 jeux
# Android : NDK + SDK requis
ANDROID_NDK_HOME=… tools/build_android_templates.sh build/export_templates arm64-v8a
ANDROID_HOME=… AGR_EXPORT_TEMPLATES=$PWD/build/export_templates AGR_ENGINE_APK=$PWD/dist/AGR-ENGINE-Android.apk \
  ./build/debug/bin/agr_tests -tc="engine showcase APK*"
```

## 6. Pièges rencontrés (à connaître)

- ECS : créer des entités / composants peut invalider les références `auto&` obtenues avant ;
  relire le composant après chaque création.
- Désactiver une entité détruit l'instance de son script ; la réactiver la recrée (`onStart`).
- Les composants d'un module doivent être enregistrés avant de sauvegarder une scène qui les
  utilise (sinon ils sont perdus).
- Traducteur Vulkan des shaders : il lit les déclarations sans préprocesseur ; mettre les
  déclarations conditionnelles dans des blocs séparés.
- Physique 3D : un hash de forme bruité reconstruisait les corps chaque image (corrigé) ; les corps
  cinématiques doivent être arrêtés quand ils ne bougent plus (corrigé).
- Ne jamais modifier le code de SDL (son CLAUDE.md interdit les contributions IA). Dépendances
  tierces : versions figées, publiées depuis au moins deux semaines.
