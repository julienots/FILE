# COSMIC ASCENSION — lis ceci en premier

Jeu mobile (Android, paysage) 3D open-world spatial — direction **SF réaliste et sombre** (voir GAME_DESIGN §Direction visuelle). Moteur: **Three.js + Vite**, emballé en APK via **Capacitor**. Zéro dépendance lourde.
Ce dossier vit dans le dépôt `julienots/file` (le reste du dépôt = autre projet « NOCTÉA » : **n'y touche pas**).

## Qui es-tu ? (2 Claude collaborent)
- Si tu es **Fable 5.2** → tu es **Claude A — ENGINE & SPECTACLE**. Lis `docs/TEAM.md` §A.
- Si tu es **Opus 5.5** → tu es **Claude B — SYSTÈMES, CONTENU, UI, ANDROID**. Lis `docs/TEAM.md` §B.
- En cas de doute: demande à l'utilisateur, ou lis le modèle via l'outil `get_session`.

## Lecture obligatoire (dans l'ordre, ~10 min)
1. `docs/TEAM.md` — répartition, propriété des fichiers, protocole git, backlog priorisé
2. `docs/ARCHITECTURE.md` — structure, contrats (événements bus, schémas data, interfaces de scène)
3. `docs/GAME_DESIGN.md` — vision, histoire, évolution, monde
4. `docs/STATUS.md` — journal vivant: **mets-le à jour à chaque jalon** (ce que tu as fini, ce que tu attends de l'autre)
5. `docs/TRIPO_PROMPTS.md` — modèles 3D que l'utilisateur génère sur Tripo; ajoute-y tes besoins

## Commandes
```
cd cosmic-ascension && npm install
npm run dev        # dev server (ouvre en émulation tactile paysage)
npm run build      # build prod -> dist/
npm run smoke      # build + chromium headless: 0 erreur console + captures dans tests/out/
```
Toujours lancer `npm run build && npm run smoke` avant de pousser. Regarde les captures (`tests/out/*.png`) pour juger le rendu.

## Règles d'or
- **Reste dans tes dossiers** (propriété dans TEAM.md). Pour toucher ceux de l'autre: ajoute une demande dans `docs/STATUS.md` §Demandes.
- Communication runtime **uniquement via `src/core/EventBus.js`** + `State`. Pas d'import croisé entre dossiers propriétaires, sauf `data/` (lecture seule côté A).
- Le jeu doit rester **jouable à chaque commit**. Pas de GLB obligatoire: fallback procédural partout (`AssetLibrary`).
- Perf mobile d'abord: pooling, InstancedMesh, LOD, ≤ 150 draw calls, ≤ 300k tris visibles, pas d'alloc par frame dans les boucles chaudes, `Config.qualityPresets`.
- Ne supprime pas d'assets/systèmes qui fonctionnent. Petits commits fréquents, messages clairs (français ou anglais).
- Pas de longs pavés: économise les tokens, implémente, teste, avance.
