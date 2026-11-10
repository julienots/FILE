# Benchmarks

`agr_benchmarks` (preset `release`, GCC 13, `-O3`). Meilleur temps sur plusieurs exécutions.
Machine : conteneur cloud, Intel Xeon 2.30 GHz, 4 vCPU (3 workers). Ce ne sont **pas** des mesures
sur téléphone : les mesures Android arriveront avec la phase 8 (objectif : appareil d'entrée de
gamme type Cortex-A55).

```
AGR ENGINE benchmarks
case                                                  best         per item
ecs: create 1M entities + 2 components           39.043 ms      39.04 ns/item
ecs: view<Position,Velocity> 1M                   3.013 ms       3.01 ns/item
ecs: view<Health,Position> (250k matches)         1.248 ms       4.99 ns/item
ecs: raw pool iteration Position 1M               0.677 ms       0.68 ns/item
ecs: random get<Velocity> 1M                     27.203 ms      27.20 ns/item
memory: LinearAllocator 1M x 32B                  1.717 ms       1.72 ns/item
memory: PoolAllocator 1M alloc+free 64B          13.855 ms      13.86 ns/item
memory: malloc/free 1M x 64B (reference)         28.548 ms      28.55 ns/item
jobs: 1M TRS matrices, single thread             14.004 ms      14.00 ns/item
jobs: 1M TRS matrices, parallelFor (3 w)          3.704 ms       3.70 ns/item
jobs: 10k empty jobs submit+wait                  1.213 ms     121.34 ns/item
jobs: 10k empty jobs submitBatch+wait             1.518 ms     151.80 ns/item
scene: updateTransforms 100k entities             3.030 ms      30.30 ns/item
scene: serialize 100k entities to JSON          470.376 ms    4703.76 ns/item
scene: load 100k entities from JSON             805.276 ms    8052.76 ns/item
    (scene JSON size: 25.0 MB)
```

## Lecture

| Mesure | Analyse |
|---|---|
| Itération ECS 2 composants : ~3 ns/entité | 1 M d'entités mises à jour en ~3 ms. Un budget d'image à 60 FPS est de 16,6 ms. |
| `get<T>` aléatoire : ~30 ns | Dominé par 3 défauts de cache par accès ; les systèmes doivent itérer par vues. |
| `PoolAllocator` | ~1,7× plus rapide que `malloc`/`free`, sans fragmentation. |
| `parallelFor` | ~4,6× sur 3 workers + thread principal pour du calcul de matrices. |
| Jobs soumis un par un : ~3 µs/job | Coût du verrou partagé et des réveils de threads. **Point faible connu** : pour du travail fin, utiliser `submitBatch`/`parallelFor` (~0,1 µs/job). Une file *work-stealing* sans verrou est prévue si le renderer en a besoin. |
| `updateTransforms` : ~30 ns/entité | Parcours complet de la hiérarchie à chaque image ; calcule aussi `activeInHierarchy` (utilisé par le renderer à la place d'une remontée des parents). Un marquage « dirty » est prévu pour les scènes majoritairement statiques. |
| JSON : ~5 µs/entité | Format d'édition, lisible et fusionnable. Les jeux livrés utiliseront un format binaire (phase 10). |

## Renderer (phase 2)

`agr_gpu_benchmarks [gl|gles|vulkan]` (preset `release`, fenêtre cachée 1280×720, médiane sur 60 images).
Les tableaux de cette section datent de la phase 2 (avant l'instanciation de la phase 5).
GPU : **Mesa llvmpipe / lavapipe**, rastériseurs *logiciels* qui exécutent le travail du GPU sur le CPU.

```
AGR ENGINE renderer benchmarks — OpenGL on llvmpipe (LLVM 20.1.2, 256 bits), 1280x720
3D: 120 lit cubes                        collect  0.021 ms  submit   2.200 ms  wait   0.000 ms  draws   120  batches    0  visible    120  culled      0
3D: 2000 lit cubes (half culled)         collect  0.244 ms  submit  12.004 ms  wait   0.000 ms  draws  1665  batches    0  visible   1665  culled    335
2D: 10k sprites, 1 texture               collect  1.029 ms  submit   2.556 ms  wait   0.000 ms  draws     1  batches    1  visible  10000  culled      0
2D: 10k sprites, 2 textures interleaved  collect  1.025 ms  submit   2.475 ms  wait   0.000 ms  draws     2  batches    2  visible  10000  culled      0
2D: 50k sprites, 2 textures interleaved  collect  5.681 ms  submit  13.477 ms  wait   0.000 ms  draws     2  batches    2  visible  50000  culled      0
2D: 50k sprites, 75% off-screen          collect  3.777 ms  submit   9.794 ms  wait   0.000 ms  draws     2  batches    2  visible  26015  culled  23985

AGR ENGINE renderer benchmarks — Vulkan on llvmpipe (LLVM 20.1.2, 256 bits), 1280x720
3D: 120 lit cubes                        collect  0.025 ms  submit   0.189 ms  wait   5.226 ms  draws   120  batches    0  visible    120  culled      0
3D: 2000 lit cubes (half culled)         collect  0.260 ms  submit   1.773 ms  wait  29.995 ms  draws  1665  batches    0  visible   1665  culled    335
2D: 10k sprites, 1 texture               collect  1.128 ms  submit   0.623 ms  wait  12.495 ms  draws     1  batches    1  visible  10000  culled      0
2D: 10k sprites, 2 textures interleaved  collect  1.088 ms  submit   0.658 ms  wait  12.897 ms  draws     2  batches    2  visible  10000  culled      0
2D: 50k sprites, 2 textures interleaved  collect  6.160 ms  submit   3.162 ms  wait  23.453 ms  draws     2  batches    2  visible  50000  culled      0
2D: 50k sprites, 75% off-screen          collect  3.975 ms  submit   1.768 ms  wait  30.235 ms  draws     2  batches    2  visible  26015  culled  23985
```

| Colonne | Signification |
|---|---|
| `collect` | Code du moteur : parcours de la scène, culling, tri, préparation des uniformes et des sommets |
| `submit` | Envoi des données et des commandes au pilote, hors attente |
| `wait` | Temps bloqué sur le GPU ou la présentation (Vulkan uniquement ; en OpenGL cette attente se produit dans `SwapBuffers`, hors mesure) |

| Mesure | Analyse |
|---|---|
| ~0,17 µs par maillage, ~0,1 µs par sprite (`collect`) | 50 000 sprites préparés en ~6 ms sur un cœur de ce Xeon. Pistes si nécessaire : collecte parallèle sur le `JobSystem`, cache des coins de sprites statiques. |
| Soumission Vulkan ~1 µs par draw contre ~7 µs en OpenGL | 1 665 draw calls soumis en 1,8 ms (Vulkan) contre 12 ms (OpenGL) sur ce pilote. Le temps total reste borné par le rastériseur logiciel (colonne `wait`). |
| 10 000 sprites, 2 textures alternées ➜ 2 draw calls | En orthographique, le tri regroupe par texture à profondeur égale. |
| Perspective | Le tri de l'arrière vers l'avant des sprites transparents limite les lots (mesuré dans la sandbox : `--sprites 10000` ➜ ~2 000 lots). Correct visuellement ; les atlas (phase 4) réduiront ce coût. |

**Méthode :** la première version de ce tableau attribuait le surcoût Vulkan à la soumission. Une mesure
section par section a montré qu'il s'agissait de `vkQueuePresentKHR`, synchrone avec lavapipe sur X11.
D'où la colonne `wait` (`FrameStats::cpuWaitMs`), qui sert aussi au profiler.

## Systèmes 2D (phase 4)

CPU (`agr_benchmarks`, même machine) :

```
2d: simulate 10k particles (one step)             0.015 ms       1.46 ns/item
2d: serialize 1000x1000 tilemap (RLE)             0.320 ms       0.32 ns/item
2d: load 1000x1000 tilemap (RLE)                  0.345 ms       0.34 ns/item
    (tilemap scene JSON size: 0.7 KB)
2d: pack atlas of 256 images                      0.300 ms    1173.36 ns/item
```

Renderer 2D (`agr_gpu_benchmarks`, mêmes conditions que ci-dessus) :

```
AGR ENGINE renderer benchmarks — OpenGL on llvmpipe (LLVM 20.1.2, 256 bits), 1280x720
2D: tilemap 1000x1000, ~16k cells shown  collect  0.731 ms  submit   5.665 ms  wait   0.000 ms  draws     1  batches    1  visible  16684  culled      0
2D: 10k particles, additive              collect  0.707 ms  submit   2.761 ms  wait   0.000 ms  draws     1  batches    1  visible  10000  culled      0
2D: 10k lit sprites, 16 lights           collect  1.036 ms  submit   2.328 ms  wait   0.000 ms  draws     1  batches    1  visible  10000  culled      0
AGR ENGINE renderer benchmarks — Vulkan on llvmpipe (LLVM 20.1.2, 256 bits), 1280x720
2D: tilemap 1000x1000, ~16k cells shown  collect  0.824 ms  submit   1.069 ms  wait  22.392 ms  draws     1  batches    1  visible  16684  culled      0
2D: 10k particles, additive              collect  0.773 ms  submit   0.618 ms  wait  21.733 ms  draws     1  batches    1  visible  10000  culled      0
2D: 10k lit sprites, 16 lights           collect  1.121 ms  submit   0.570 ms  wait  21.680 ms  draws     1  batches    1  visible  10000  culled      0
```

| Mesure | Analyse |
|---|---|
| Tilemap 1000 × 1000 : seules les ~16 700 cellules visibles sont générées | Coût proportionnel à l'écran, pas à la carte : 0,8 ms de collecte, 1 draw call. |
| Tilemap enregistrée en RLE : 0,7 Ko pour 1 M de cellules (données de type « sol + ciel ») | Une carte très variée (bruit) compresse mal : jusqu'à ~2 entiers par cellule dans le JSON. |
| 10 000 particules : ~0,015 ms de simulation, ~0,7 ms de collecte | La collecte (coins, couleurs, tri) domine, comme pour les sprites. |
| 10 000 sprites éclairés par 16 lumières : même coût CPU que des sprites normaux | L'éclairage est calculé dans le shader de fragments ; son coût GPU réel sera mesuré sur téléphones (phase 8). |
| Atlas de 256 images : 0,3 ms | Empaquetage par étagères ; suffisant pour l'éditeur (pas fait à l'exécution). |

**Correction de mesure (phase 4) :** depuis la phase 3, `render()` remplaçait `gpuWaitMs` (attente pendant
la soumission) par l'attente de toute l'image, présentation comprise ; la colonne `submit` de ce
benchmark devenait négative en Vulkan. Les deux valeurs sont maintenant séparées : `gpuWaitMs`
(dans la soumission) et `frameWaitMs` (toute l'image), affichée dans la colonne `wait`.

## Rendu 3D (phase 5)

Import (`agr_benchmarks`) :

```
3d: import OBJ 20k triangles (+normals, tangents)      7.703 ms     385.17 ns/item
```

Renderer 3D (`agr_gpu_benchmarks`, profil « low » sans ombres sauf mention, mêmes conditions) :

```
AGR ENGINE renderer benchmarks — OpenGL on llvmpipe (LLVM 20.1.2, 256 bits), 1280x720
3D: 120 lit cubes                        collect  0.026 ms  submit   0.240 ms  wait   0.000 ms  draws     1  batches    1  visible    120  culled      0
3D: 2000 lit cubes (half culled)         collect  0.250 ms  submit   2.477 ms  wait   0.000 ms  draws     1  batches    1  visible   1665  culled    335
3D: 10k cubes (instanced)                collect  1.493 ms  submit  16.465 ms  wait   0.000 ms  draws     1  batches    1  visible   9299  culled    701
3D: 2000 cubes + ground, no shadows      collect  0.245 ms  submit   1.935 ms  wait   0.000 ms  draws     2  batches    2  visible   1662  culled    339
3D: same, shadows 2048 + PCF             collect  0.350 ms  submit  12.209 ms  wait   0.000 ms  draws     4  batches    2  visible   1662  culled    339
3D: 120 cubes, 8 point lights            collect  0.042 ms  submit   0.346 ms  wait   0.000 ms  draws     1  batches    1  visible    120  culled      0
AGR ENGINE renderer benchmarks — Vulkan on llvmpipe (LLVM 20.1.2, 256 bits), 1280x720
3D: 120 lit cubes                        collect  0.030 ms  submit   0.039 ms  wait   4.415 ms  draws     1  batches    1  visible    120  culled      0
3D: 2000 lit cubes (half culled)         collect  0.268 ms  submit   0.106 ms  wait  15.839 ms  draws     1  batches    1  visible   1665  culled    335
3D: 10k cubes (instanced)                collect  1.699 ms  submit   0.801 ms  wait  41.575 ms  draws     1  batches    1  visible   9299  culled    701
3D: 2000 cubes + ground, no shadows      collect  0.286 ms  submit   0.112 ms  wait  17.047 ms  draws     2  batches    2  visible   1662  culled    339
3D: same, shadows 2048 + PCF             collect  0.404 ms  submit   0.284 ms  wait  38.867 ms  draws     4  batches    2  visible   1662  culled    339
3D: 120 cubes, 8 point lights            collect  0.043 ms  submit   0.046 ms  wait   5.253 ms  draws     1  batches    1  visible    120  culled      0
```

| Mesure | Analyse |
|---|---|
| 2 000 cubes : 1 665 draw calls en phase 2, **1** maintenant | Instanciation automatique (même maillage + même matériau ; la teinte est par instance). Soumission OpenGL : 12 ms ➜ ~2,4 ms sur ce pilote. |
| 10 000 cubes instanciés : 1,5 ms de collecte | ~0,15 µs par objet (culling, matrice normale, tri). La collecte parallèle reste une piste si nécessaire. |
| Ombres 2048 + PCF : 2 passes, ~2× le temps GPU de cette scène | Coût dominé par le rastériseur logiciel ; à mesurer sur GPU mobiles (phase 8). Le profil ultra-mobile les désactive. |
| 8 lumières ponctuelles : coût CPU négligeable | Les lumières sont évaluées par pixel dans le shader (forward) ; le profil limite leur nombre. |
| Import OBJ 20 000 triangles : 6 à 8 ms (jusqu'à 12 ms selon la charge de la machine) | Analyse du texte, déduplication des sommets, normales et tangentes ; fait hors du thread principal. |

Ces chiffres valident l'architecture (lots, culling, coût par objet) mais **pas** les performances sur
téléphone : elles seront mesurées en phase 8 sur des appareils Android réels.
