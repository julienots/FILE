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

`agr_gpu_benchmarks` (preset `release`, fenêtre cachée 1280×720, médiane sur 60 images).
GPU : **Mesa llvmpipe**, un rasteriseur *logiciel* qui exécute le travail du GPU sur le CPU.

```
AGR ENGINE renderer benchmarks — llvmpipe (LLVM 20.1.2, 256 bits), 1280x720
3D: 120 lit cubes                      collect   0.020 ms   submit   1.893 ms   draws   120   batches     0   visible    120   culled      0
3D: 2000 lit cubes (half culled)       collect   0.203 ms   submit   7.077 ms   draws  1665   batches     0   visible   1665   culled    335
2D: 10k sprites, 1 texture             collect   0.945 ms   submit   2.264 ms   draws     1   batches     1   visible  10000   culled      0
2D: 10k sprites, 2 textures interleaved collect   1.006 ms   submit   2.328 ms   draws     2   batches     2   visible  10000   culled      0
2D: 50k sprites, 2 textures interleaved collect   5.371 ms   submit  11.957 ms   draws     2   batches     2   visible  50000   culled      0
2D: 50k sprites, 75% off-screen        collect   3.530 ms   submit   8.778 ms   draws     2   batches     2   visible  26015   culled  23985
```

| Colonne | Signification |
|---|---|
| `collect` | Code du moteur : parcours de la scène, culling, tri, préparation des uniformes et des sommets |
| `submit` | Upload et soumission des commandes. Avec llvmpipe, cela inclut une partie du travail « GPU » ; sur un vrai pilote, compter environ 1 à 5 µs par draw call |

| Mesure | Analyse |
|---|---|
| ~0,17 µs par maillage, ~0,1 µs par sprite (`collect`) | 50 000 sprites préparés en ~5 ms sur un cœur de ce Xeon. Prochaines optimisations si nécessaire : collecte parallèle sur le `JobSystem`, cache des coins de sprites statiques. |
| 10 000 sprites, 2 textures alternées ➜ 2 draw calls | En orthographique, le tri regroupe par texture à profondeur égale. |
| Perspective | Le tri de l'arrière vers l'avant des sprites transparents limite les lots (mesuré dans la sandbox : `--sprites 10000` ➜ ~2 000 lots). Correct visuellement ; les atlas (phase 4) réduiront ce coût. |
| Profils sur la sandbox (llvmpipe) | ultra-mobile 640×360 limité à 30 FPS ; low 960×540 ; high 1280×720 MSAA x4. |

Ces chiffres valident l'architecture (lots, culling, coût par objet) mais **pas** les performances
sur téléphone : elles seront mesurées en phase 8 sur des appareils Android réels.
