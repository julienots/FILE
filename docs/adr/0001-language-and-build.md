# ADR 0001 — Langage, standard et système de build

**Statut :** accepté (phase 1)

## Décision

- **C++20** pour tout le moteur. Concepts, `std::span`, `std::format`, initialiseurs désignés,
  `<=>` réduisent le code passe-partout sans dépendre du C++23.
- **CMake ≥ 3.24 + Ninja**, presets (`CMakePresets.json`), dépendances par `FetchContent` à des
  versions figées (tag ou hash SHA-256).
- Bibliothèques **statiques** pour les modules du moteur. Un jeu Android est un unique `.so`,
  ce qui réduit la taille de l'APK et le temps de démarrage (pas de résolution de symboles entre modules).

## Raisons

- Le NDK Android r27 (Clang 18, libc++) supporte tout ce sous-ensemble de C++20, `std::format` compris.
- C++23 (`std::expected`, `std::print`) n'est pas encore homogène entre MSVC, libstdc++ et libc++ :
  `Result<T>` comble le manque en ~60 lignes.

## Conséquences

- Les plugins dynamiques embarquent leur propre copie des bibliothèques statiques. Les singletons
  (logs, registre de composants) sont donc redirigés vers ceux de l'hôte au chargement
  (`bindPluginToHost`). Une option de build « moteur en bibliothèque partagée » pour l'éditeur
  sera évaluée en phase 3.
