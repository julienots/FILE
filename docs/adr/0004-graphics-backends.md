# ADR 0004 — Backends graphiques

**Statut :** proposé (sera implémenté et validé en phase 2)

## Décision

- Une couche **RHI** (Render Hardware Interface) mince : buffers, textures, samplers,
  pipelines, passes de rendu, listes de commandes. Le renderer ne voit jamais Vulkan ni OpenGL.
- **Vulkan 1.1** : backend principal sur Android (Google l'impose aux appareils 64 bits livrés
  avec Android 10 ou plus récent), Windows et Linux.
- **OpenGL ES 3.0** : backend de compatibilité pour les appareils sans Vulkan fiable
  (pilotes anciens, liste de blocage par GPU). Sur desktop, le même code tourne en OpenGL 3.3 core.
- Sélection au démarrage : Vulkan si disponible et non bloqué, sinon GLES ; forçable dans `engine.json`.
- Profils graphiques **Ultra / Élevé / Moyen / Faible / Ultra mobile** : chaque fonctionnalité
  coûteuse (ombres, SSAO, bloom, MSAA, échelle de rendu, distance des LOD) est un réglage du profil,
  jamais imposée. Le profil par défaut est choisi d'après le GPU détecté et la mémoire disponible.

## Pourquoi l'OpenGL est déjà présent en phase 1

`SdlPlatform` crée un contexte OpenGL pour valider de bout en bout la fenêtre, le contexte et la
présentation (la sandbox efface l'écran). Ce contexte deviendra le device du backend GLES.
