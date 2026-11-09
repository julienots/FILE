<p align="center"><img src="docs/brand/agr-logo.svg" alt="AGR ENGINE — Build Beyond Limits." width="420"></p>

# AGR ENGINE

**Build Beyond Limits.** — un moteur de jeu propriétaire de **Superessence Studio**.

AGR ENGINE vise la place entre Godot et Unity : plus simple à prendre en main qu'Unity, assez puissant
pour des jeux commerciaux, et conçu **d'abord pour les smartphones Android**, avec un éditeur sur PC.

> **État actuel : phase 1 sur 10 terminée** — architecture et socle technique.
> Il n'y a pas encore de rendu de géométrie ni d'éditeur visuel : ce sont les phases 2 et 3.
> L'avancement détaillé et honnête est dans [docs/ROADMAP.md](docs/ROADMAP.md).

## Ce qui fonctionne aujourd'hui

- Un **cœur C++20 modulaire** : logs, allocateurs mémoire avec statistiques, système de jobs
  multithread, bus d'événements, système de fichiers virtuel, UUID, handles générationnels.
- Un **ECS** à sparse sets (1 M d'entités mises à jour en ~3 ms sur un cœur).
- Des **scènes** avec hiérarchie ordonnée, transformations monde, duplication, sauvegarde et
  chargement JSON pilotés par la **réflexion** (la base de l'inspecteur de l'éditeur).
- Une **boucle moteur** à pas fixe bornée, adaptée aux appareils lents, avec statistiques d'image.
- Une **couche plateforme SDL3** : fenêtre, contexte OpenGL 3.3 (desktop) / ES 3.0 (Android),
  clavier, souris, multi-touch, cycle de vie mobile.
- Un **système de plugins** statiques et dynamiques avec dépendances ; le plugin d'exemple `Spin`
  ajoute un composant et son système sans toucher au moteur.
- **58 tests** unitaires, builds sanitizers (ASan/UBSan), benchmarks, CI GCC/Clang/MSVC.

## Démarrage rapide

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/bin/agr_sandbox          # fenêtre de démonstration, Échap pour quitter
```

Prérequis et options : [docs/BUILDING.md](docs/BUILDING.md).

## Exemple

```cpp
#include "agr/runtime/Engine.hpp"
#include "agr/platform/SdlPlatform.hpp"

struct MyGame : agr::Application {
    agr::Status onStart(agr::Engine& engine) override {
        auto& scene = engine.scene();
        auto player = scene.createEntity("Player");
        auto weapon = scene.createEntity("Sword", player);   // enfant de Player
        scene.registry().get<agr::TransformComponent>(weapon).position = {0.5f, 1.0f, 0.0f};
        return engine.saveScene("user://level1.agrscene");
    }
    void onUpdate(agr::Engine& engine, const agr::FrameTime&) override {
        if (engine.input().keyPressed(agr::Key::Escape)) engine.requestQuit();
    }
};

int main() {
    agr::Engine engine({});
    if (!engine.initialize(std::make_unique<agr::SdlPlatform>())) return 1;
    MyGame game;
    return engine.run(game);
}
```

## Organisation du dépôt

```
engine/
  core/       types, logs, mémoire, jobs, événements, fichiers, plugins (bas niveau)
  math/       vecteurs, quaternions, matrices, géométrie
  ecs/        entités, composants, vues
  scene/      scène, hiérarchie, réflexion, sérialisation
  runtime/    moteur, boucle, entrées, configuration, gestionnaire de plugins
  platform/   implémentation SDL3 (fenêtre, contexte GL, événements)
plugins/spin/ plugin dynamique d'exemple
apps/sandbox/ application de démonstration runtime
tests/        tests unitaires (doctest)
benchmarks/   micro-benchmarks
docs/         architecture, ADR, feuille de route, identité visuelle
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Compilation](docs/BUILDING.md)
- [Feuille de route](docs/ROADMAP.md)
- [Écrire un plugin](docs/PLUGINS.md)
- [Benchmarks](docs/BENCHMARKS.md)
- [Règles de code](docs/CODING_STANDARDS.md)
- Décisions d'architecture : [langage et build](docs/adr/0001-language-and-build.md),
  [dépendances](docs/adr/0002-third-party-dependencies.md), [ECS](docs/adr/0003-ecs-design.md),
  [backends graphiques](docs/adr/0004-graphics-backends.md)
- [Identité visuelle](docs/brand/BRAND.md) · [Licences tierces](THIRD_PARTY_NOTICES.md)

---

© Superessence Studio. Tous droits réservés.
