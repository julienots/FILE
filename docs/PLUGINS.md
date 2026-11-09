# Écrire un plugin

Un plugin ajoute des composants, des systèmes et (plus tard) des panneaux d'éditeur sans
modifier le moteur. Exemple complet : [`plugins/spin`](../plugins/spin/SpinPlugin.cpp).

## Interface

```cpp
class MyPlugin final : public agr::Plugin {
public:
    agr::PluginInfo info() const override {
        return {"MyPlugin", "1.0.0", "Description", "Auteur", /*dependencies*/ {"Spin"}};
    }
    agr::Status onLoad(agr::Engine& engine) override;        // enregistrer composants / abonnements
    void onUnload(agr::Engine& engine) override;             // tout désenregistrer
    void onFixedUpdate(agr::Engine&, const agr::FrameTime&) override;  // gameplay, physique
    void onUpdate(agr::Engine&, const agr::FrameTime&) override;       // une fois par image
};
AGR_DEFINE_PLUGIN(MyPlugin)   // uniquement pour un plugin dynamique
```

## Deux modes de chargement

| Mode | Utilisation | Comment |
|---|---|---|
| Statique | Android, builds de jeu | `engine.plugins().addStatic(std::make_unique<MyPlugin>())` avant `initialize()` |
| Dynamique | Éditeur, outils, mods sur desktop | Bibliothèque partagée dont le nom contient `agr_plugin`, placée dans `pluginsDirectory` (par défaut : à côté de l'exécutable) |

Le gestionnaire trie les plugins selon leurs dépendances, refuse les cycles, et continue de charger
les autres plugins si l'un échoue (dépendance manquante, erreur dans `onLoad`).
Le déchargement se fait dans l'ordre inverse.

## Règles des plugins dynamiques

1. **Même compilateur et même version du moteur que l'hôte.** La fonction exportée
   `agrPluginApiVersion` est vérifiée avant toute autre chose (`kPluginApiVersion`).
2. **Tout ce qui est enregistré dans `onLoad` doit être retiré dans `onUnload`** : le code des
   composants, des callbacks et des pools vit dans la bibliothèque, qui est fermée ensuite.
   ```cpp
   void onUnload(agr::Engine& engine) override {
       const auto id = agr::ecs::componentTypeId<MyComponent>();
       engine.scene().registry().erasePool(id);
       agr::ComponentRegistry::instance().remove(id);
       engine.events().unsubscribe(m_subscription);
   }
   ```
3. Les logs (`AGR_LOG_*`) et le registre de composants du plugin sont automatiquement redirigés
   vers ceux de l'hôte par `AGR_DEFINE_PLUGIN` : les messages apparaissent dans la console du moteur.
4. Compiler avec une visibilité cachée (`CXX_VISIBILITY_PRESET hidden`) : seuls les trois points
   d'entrée sont exportés.

## CMake

```cmake
add_library(agr_plugin_my MODULE MyPlugin.cpp)
target_link_libraries(agr_plugin_my PRIVATE agr::runtime)
set_target_properties(agr_plugin_my PROPERTIES PREFIX "lib" CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
```
