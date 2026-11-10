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

## Lancer

```bash
cd build/debug
./bin/agr_tests                        # tests unitaires (sans GPU)
./bin/agr_gpu_tests                    # tests de rendu (OpenGL) — nécessite un affichage
AGR_TEST_GLES=1 ./bin/agr_gpu_tests    # mêmes tests en OpenGL ES 3.0
./bin/agr_sandbox                      # Échap : quitter · 1-5 : profils · F12 : capture
./bin/agr_sandbox --gles --profile low --screenshot user://shot.png --frames 120
./bin/agr_sandbox --sprites 10000      # test de charge des sprites
./bin/agr_sandbox --headless --frames 120
./bin/agr_benchmarks                   # benchmarks CPU (preset release)
./bin/agr_gpu_benchmarks               # benchmarks du renderer (preset release, affichage requis)
```

La sandbox lit ses textures dans `bin/assets/` (copiées au build, montage `assets://`) et écrit
`user/sandbox.agrscene` et les captures dans le dossier courant (montage `user://`).

### Sans écran (serveur, CI)

Les tests GPU ont besoin d'un affichage. Sous Linux sans écran : `xvfb-run -a ./bin/agr_gpu_tests`
(rendu logiciel Mesa llvmpipe, OpenGL 4.5 et OpenGL ES 3.2). Sans affichage, `agr_gpu_tests`
s'arrête avec le message « GPU tests skipped » et le code 0, sauf si `AGR_REQUIRE_GPU_TESTS=1`
est défini (c'est le cas en CI) : l'absence d'affichage devient alors un échec.

## Android

La chaîne Android (NDK r27+, Gradle, APK/AAB) est l'objet de la **phase 8**. Le code est déjà
écrit pour être portable : aucune dépendance desktop dans `core/ecs/scene/runtime`, logs vers logcat,
contexte OpenGL ES 3.0 sur Android dans `SdlPlatform`, gestion du cycle de vie mobile.
Rien de tout cela n'a encore été compilé pour Android : ne le considérez pas comme validé.
