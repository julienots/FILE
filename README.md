<p align="center"><img src="docs/brand/agr-logo.svg" alt="AGR ENGINE — Build Beyond Limits." width="420"></p>

# AGR ENGINE

**Build Beyond Limits.** — un moteur de jeu propriétaire de **Superessence Studio**.

AGR ENGINE vise la place entre Godot et Unity : plus simple à prendre en main qu'Unity, assez puissant
pour des jeux commerciaux, et conçu **d'abord pour les smartphones Android**, avec un éditeur sur PC.

> **État actuel : phase 1 terminée, phase 2 en grande partie terminée.**
> Le rendu OpenGL 3.3 / OpenGL ES 3.0 fonctionne. Le backend Vulkan est la prochaine étape, et il n'y a
> pas encore d'éditeur visuel (phase 3). L'avancement détaillé et honnête est dans [docs/ROADMAP.md](docs/ROADMAP.md).

![Sandbox AGR ENGINE](docs/images/sandbox.png)

*Image rendue par AGR ENGINE (sandbox, profil « high », MSAA x4).*

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
- Un **renderer** sur une couche GPU neutre (RHI) : backend OpenGL 3.3 core / OpenGL ES 3.0,
  caméras, lumière directionnelle, maillages intégrés éclairés, sprites triés et regroupés en lots,
  frustum culling, MSAA, échelle de rendu, **profils graphiques Ultra ➜ Ultra mobile** choisis
  automatiquement, temps GPU, captures d'écran ([détails](docs/RENDERING.md)).
- Un **gestionnaire de ressources** : chargement asynchrone, cache, budget d'upload par image,
  rechargement à chaud.
- **68 tests unitaires + 15 tests GPU au pixel près** (exécutés en OpenGL et en OpenGL ES), sanitizers
  (ASan/UBSan), benchmarks CPU et GPU, CI GCC/Clang/MSVC.

## Démarrage rapide

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/bin/agr_sandbox          # Échap : quitter · 1-5 : profils graphiques · F12 : capture
```

Prérequis et options : [docs/BUILDING.md](docs/BUILDING.md).

## Exemple

```cpp
#include "agr/platform/SdlPlatform.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/RenderPlugin.hpp"
#include "agr/runtime/Engine.hpp"

struct MyGame : agr::Application {
    agr::Status onStart(agr::Engine& engine) override {
        auto& scene = engine.scene();
        auto& reg = scene.registry();

        auto camera = scene.createEntity("Camera");
        reg.emplace<agr::CameraComponent>(camera);
        reg.get<agr::TransformComponent>(camera).position = {0.0f, 1.0f, 6.0f};

        auto sun = scene.createEntity("Sun");
        reg.emplace<agr::DirectionalLightComponent>(sun);
        reg.get<agr::TransformComponent>(sun).rotation = agr::math::Quat::fromEuler({-0.8f, 0.4f, 0.0f});

        auto player = scene.createEntity("Player");
        reg.emplace<agr::MeshRendererComponent>(player).color = {0.96f, 0.65f, 0.14f, 1.0f};
        reg.get<agr::TransformComponent>(player).rotation = agr::math::Quat::fromEuler({0.4f, 0.7f, 0.0f});
        auto badge = scene.createEntity("Badge", player);     // enfant de Player
        reg.get<agr::TransformComponent>(badge).position = {0.0f, 1.2f, 0.0f};
        reg.emplace<agr::SpriteRendererComponent>(badge).texture = "assets://textures/agr_mark.png";

        return engine.saveScene("user://level1.agrscene");
    }
    void onUpdate(agr::Engine& engine, const agr::FrameTime&) override {
        if (engine.input().keyPressed(agr::Key::Escape)) engine.requestQuit();
    }
};

int main() {
    agr::Engine engine({});
    engine.plugins().addStatic(std::make_unique<agr::RenderPlugin>());
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
  rhi/        interface GPU neutre + backend OpenGL / OpenGL ES
  assets/     images, gestionnaire de ressources
  render/     composants de rendu, renderer, profils graphiques
  platform/   implémentation SDL3 (fenêtre, contexte GL, événements)
plugins/spin/ plugin dynamique d'exemple
apps/sandbox/ application de démonstration (scène 3D + 2D, assets originaux)
tests/        tests unitaires ; tests/gpu : tests de rendu au pixel près
benchmarks/   micro-benchmarks CPU ; benchmarks/gpu : benchmarks du renderer
docs/         architecture, ADR, feuille de route, identité visuelle
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Rendu et ressources](docs/RENDERING.md)
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
