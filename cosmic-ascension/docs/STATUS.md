# STATUS — journal vivant (chaque Claude édite SA section uniquement)

## État global
Squelette jouable livré par le coordinateur (session setup): scène espace (vaisseau, 4 planètes, nébuleuse shader, étoiles), scène planète à pied (gravité sphérique, héros procédural 6 stades), contrôles tactiles paysage, bloom, qualité adaptative, AssetLibrary GLB+fallback, smoke test. `npm run build && npm run smoke` OK.
Bugs/limites connus: terrain facetté (lissage à faire, A1), héros peu lisible sous brouillard, pas de HUD ni de contenu (B).

## Claude A (Fable 5.2) — Engine & Spectacle
- [ ] A1 Planète à pied  - [ ] A2 Héros  - [ ] A3 Espace  - [ ] A4 Transition/caméras  - [ ] A5 Boss  - [ ] A6 Shaders/post  - [ ] A7 Combat visuel  - [ ] A8 Rendu planète joueur  - [ ] A9 Perf
Notes / décisions:

## Claude B (Opus 5.5) — Systèmes, Contenu, UI, Android
- [ ] B1 UI/HUD  - [ ] B2 Données  - [ ] B3 Quêtes/dialogues  - [ ] B4 Progression  - [ ] B5 Combat logique  - [ ] B6 Planète joueur  - [ ] B7 Collections/save  - [ ] B8 Audio  - [ ] B9 Android  - [ ] B10 Tests
Notes / décisions:

## Demandes inter-Claude (format: `[A→B] / [B→A] date — demande — statut`)

## Modèles Tripo reçus / à intégrer
(l'utilisateur dépose les GLB dans `public/models/`; B active dans manifest.json; A vérifie l'échelle/orientation)
