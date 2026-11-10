# Compiler AGR ENGINE

## Prérequis

| Outil | Version minimale |
|---|---|
| CMake | 3.24 |
| Ninja | 1.10 |
| Compilateur C++20 | GCC 13, Clang 17 (NDK r27), MSVC 19.38 (VS 2022 17.8) |
| Accès réseau au premier `configure` | les dépendances sont téléchargées par `FetchContent` |

Linux (Ubuntu 24.04) — en-têtes nécessaires à SDL3 :

```bash
sudo apt-get install -y ninja-build libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxi-dev libxss-dev libxfixes-dev libxtst-dev libgl-dev libegl-dev libgles-dev
```

Windows : Visual Studio 2022 avec la charge de travail « Développement Desktop en C++ »,
puis une « Developer PowerShell » (Ninja et CMake sont inclus).

## Presets

```bash
cmake --preset debug      # ou release, asan, headless
cmake --build --preset debug
ctest --preset debug
```

| Preset | Contenu |
|---|---|
| `debug` | Debug complet, assertions actives (`AGR_ASSERT`) |
| `release` | Optimisé ; à utiliser pour les benchmarks |
| `asan` | Debug + AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang) |
| `headless` | Sans SDL3 : core, ECS, scène, runtime, tests. Pour CI et serveurs |

Tous les binaires (exécutables, plugins) sont produits dans `build/<preset>/bin/`.

## Options CMake

| Option | Défaut | Effet |
|---|---|---|
| `AGR_BUILD_PLATFORM` | ON | Couche plateforme SDL3 (désactive aussi la sandbox si OFF) |
| `AGR_BUILD_TESTS` | ON | Tests unitaires (`agr_tests`) |
| `AGR_BUILD_BENCHMARKS` | ON | Micro-benchmarks (`agr_benchmarks`) |
| `AGR_BUILD_SANDBOX` | ON | Application de démonstration runtime |
| `AGR_BUILD_PLUGINS` | ON | Plugins d'exemple (`libagr_plugin_spin`) |
| `AGR_WARNINGS_AS_ERRORS` | OFF | `-Werror` / `/WX` (activé en CI) |
| `AGR_ENABLE_ASAN` | OFF | Sanitizers |
| `AGR_ENABLE_VULKAN` | ON | Backend Vulkan (Vulkan-Headers, volk, VMA et glslang sont téléchargés) |
| `AGR_BUILD_EDITOR` | ON | Éditeur (Dear ImGui et ImGuizmo sont téléchargés ; nécessite la plateforme) |

## Lancer

```bash
cd build/debug
./bin/agr_editor                       # éditeur (voir docs/EDITOR.md)
./bin/agr_editor --vulkan --smoke-test /tmp/agr_smoke   # test de bout en bout + capture
./bin/agr_tests                        # tests unitaires (sans GPU)
./bin/agr_gpu_tests                    # tests de rendu (OpenGL) — nécessite un affichage
AGR_TEST_API=gles ./bin/agr_gpu_tests  # mêmes tests en OpenGL ES 3.0
AGR_TEST_API=vulkan ./bin/agr_gpu_tests  # mêmes tests en Vulkan, validation layers actives
./bin/agr_sandbox                      # Échap : quitter · 1-5 : profils · F12 : capture
./bin/agr_sandbox --vulkan --validation # Vulkan avec validation layers
./bin/agr_sandbox --gles --profile low --screenshot user://shot.png --frames 120
./bin/agr_sandbox --freeze --opengl --screenshot user://gl.png --frames 20   # image déterministe
./bin/agr_sandbox --sprites 10000      # test de charge des sprites
./bin/agr_sandbox --headless --frames 120
./bin/agr_benchmarks                   # benchmarks CPU (preset release)
./bin/agr_gpu_benchmarks vulkan        # benchmarks du renderer : gl | gles | vulkan (preset release)
```

La sandbox lit ses textures dans `bin/assets/` (copiées au build, montage `assets://`) et écrit
`user/sandbox.agrscene` et les captures dans le dossier courant (montage `user://`).

### Vulkan

Le chargeur Vulkan (`libvulkan.so.1`, `vulkan-1.dll`) et un pilote sont fournis par le système. Pour les
*validation layers* sous Ubuntu : `sudo apt-get install vulkan-validationlayers`. Sans GPU, Mesa
**lavapipe** (`mesa-vulkan-drivers`) fournit Vulkan en logiciel ; c'est ce qu'utilise la CI.

### Sanitizers

`ctest --preset asan` applique `tools/lsan.supp`, qui ne masque que des allocations de SDL et de Xlib
faites une fois au démarrage. Pour la suite GPU Vulkan, la détection de fuites est désactivée : les
threads internes de lavapipe laissent deux allocations (112 octets) sans aucune frame du moteur dans leur
pile. AddressSanitizer et UBSan restent actifs.

### Sans écran (serveur, CI)

Les tests GPU ont besoin d'un affichage. Sous Linux sans écran : `xvfb-run -a ./bin/agr_gpu_tests`
(rendu logiciel Mesa llvmpipe, OpenGL 4.5 et OpenGL ES 3.2). Sans affichage, `agr_gpu_tests`
s'arrête avec le message « GPU tests skipped » et le code 0, sauf si `AGR_REQUIRE_GPU_TESTS=1`
est défini (c'est le cas en CI) : l'absence d'affichage devient alors un échec.

## Android

Le moteur se compile pour Android (NDK r27 ou plus récent, testé avec r28c) sous la forme du **lecteur
de jeu** `libmain.so` et de `libSDL3.so` :

```bash
export ANDROID_NDK_HOME=/chemin/vers/ndk/28.2.13676358
cmake --preset android-arm64 && cmake --build --preset android-arm64     # ou android-x86_64
tools/build_android_templates.sh build/export_templates                  # les deux ABI, allégées, + Java de SDL
```

Ce sont les **modèles d'export** : l'éditeur les assemble avec les données d'un jeu dans un projet Gradle,
sans recompiler de C++ à chaque export. Construction d'un APK / AAB : [EXPORT.md](EXPORT.md).
API 26 minimum (Android 8.0), libc++ statique, pages de 16 Ko (exigence de Google Play pour Android 15).
