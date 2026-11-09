# Benchmarks

`agr_benchmarks` (preset `release`, GCC 13, `-O3`). Meilleur temps sur plusieurs exécutions.
Machine : conteneur cloud, Intel Xeon 2.30 GHz, 4 vCPU (3 workers). Ce ne sont **pas** des mesures
sur téléphone : les mesures Android arriveront avec la phase 8 (objectif : appareil d'entrée de
gamme type Cortex-A55).

```
AGR ENGINE benchmarks
case                                                  best         per item
ecs: create 1M entities + 2 components           41.667 ms      41.67 ns/item
ecs: view<Position,Velocity> 1M                   2.840 ms       2.84 ns/item
ecs: view<Health,Position> (250k matches)         1.672 ms       6.69 ns/item
ecs: raw pool iteration Position 1M               0.874 ms       0.87 ns/item
ecs: random get<Velocity> 1M                     31.352 ms      31.35 ns/item
memory: LinearAllocator 1M x 32B                  1.629 ms       1.63 ns/item
memory: PoolAllocator 1M alloc+free 64B          12.725 ms      12.72 ns/item
memory: malloc/free 1M x 64B (reference)         21.788 ms      21.79 ns/item
jobs: 1M TRS matrices, single thread             13.183 ms      13.18 ns/item
jobs: 1M TRS matrices, parallelFor (3 w)          2.834 ms       2.83 ns/item
jobs: 10k empty jobs submit+wait                 28.985 ms    2898.55 ns/item
jobs: 10k empty jobs submitBatch+wait             1.052 ms     105.21 ns/item
scene: updateTransforms 100k entities             2.209 ms      22.09 ns/item
scene: serialize 100k entities to JSON          464.236 ms    4642.36 ns/item
scene: load 100k entities from JSON             593.910 ms    5939.10 ns/item
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
| `updateTransforms` : ~22 ns/entité | Parcours complet de la hiérarchie à chaque image. Un marquage « dirty » est prévu pour les scènes majoritairement statiques. |
| JSON : ~5 µs/entité | Format d'édition, lisible et fusionnable. Les jeux livrés utiliseront un format binaire (phase 10). |
