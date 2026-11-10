# ADR 0004 — Backends graphiques

**Statut :** accepté. Backend OpenGL / OpenGL ES implémenté en phase 2 ; backend Vulkan à venir.

## Décision

- Une couche **RHI** mince (`agr/rhi/Rhi.hpp`) : buffers, textures, pipelines, cibles de rendu,
  passes, draws, blits. Le renderer ne voit jamais Vulkan ni OpenGL.
- **Vulkan 1.1** : backend principal sur Android (Google l'impose aux appareils 64 bits livrés avec
  Android 10 ou plus récent), Windows et Linux.
- **OpenGL ES 3.0** : backend de compatibilité pour les appareils sans Vulkan fiable (pilotes anciens,
  liste de blocage par GPU). Sur desktop, le même code tourne en OpenGL 3.3 core.
- Sélection au démarrage : Vulkan si disponible et non bloqué, sinon GLES ; forçable dans `engine.json`.
- Profils graphiques Ultra / High / Medium / Low / Ultra mobile : chaque fonctionnalité coûteuse est
  un réglage du profil, jamais imposée à tous les appareils ([RENDERING.md](../RENDERING.md)).

## Pourquoi GL avant Vulkan

1. C'est le chemin qui fonctionne sur **tous** les appareils Android ciblés : il doit exister de toute façon.
2. Il permet de construire et tester le renderer, les ressources et l'éditeur sans attendre le
   backend le plus long à écrire.
3. Il est **testable en CI** sans GPU : Mesa llvmpipe fournit OpenGL 4.5 et OpenGL ES 3.2 logiciels,
   et les tests GPU lisent les pixels réels dans les deux dialectes.

## Choix de conception qui préparent Vulkan

- Uniformes en blocs std140 sur des slots numérotés, textures sur des unités numérotées ➜ descriptor sets.
- Données par objet dans un buffer unique par image avec offsets alignés ➜ *dynamic uniform buffers*.
- État de rendu figé dans le pipeline (blend, profondeur, culling, format de vertex) ➜ `VkPipeline`.
- Passes explicites avec valeurs d'effacement ➜ *render passes* / *dynamic rendering*.
- UV (0,0) en haut à gauche et première ligne de texture en haut : identique sur les deux API.
