# STATUS — journal vivant (chaque Claude édite SA section uniquement)

## État global
Squelette jouable livré par le coordinateur (session setup): scène espace (vaisseau, 4 planètes, nébuleuse shader, étoiles), scène planète à pied (gravité sphérique, héros procédural 6 stades), contrôles tactiles paysage, bloom, qualité adaptative, AssetLibrary GLB+fallback, smoke test. `npm run build && npm run smoke` OK.
Bugs/limites connus: terrain facetté (lissage à faire, A1), héros peu lisible sous brouillard, pas de HUD ni de contenu (B).

## Claude A (Fable 5.2) — Engine & Spectacle
- [ ] A1 Planète à pied  - [ ] A2 Héros  - [ ] A3 Espace  - [ ] A4 Transition/caméras  - [ ] A5 Boss  - [ ] A6 Shaders/post  - [ ] A7 Combat visuel  - [ ] A8 Rendu planète joueur  - [ ] A9 Perf
Notes / décisions:

## Claude B (Opus 5.5) — Systèmes, Contenu, UI, Android
- [~] B1 UI/HUD (titre, HUD, pause, réglages, toasts, bannière d'évolution, contrôles refaits ✔ — reste: carte galactique, inventaire, codex, dialogues, atelier)  - [~] B2 Données (16 planètes/7 systèmes, 10 stades, capacités, ressources ✔ — reste: créatures, artefacts, ennemis, boss)  - [ ] B3 Quêtes/dialogues  - [x] B4 Progression  - [ ] B5 Combat logique  - [ ] B6 Planète joueur  - [~] B7 Collections/save (State v2 + migrations ✔)  - [ ] B8 Audio  - [ ] B9 Android  - [~] B10 Tests (`npm test`: 10 tests)
Notes / décisions:
- 2026-11-13 **M1 côté B livré**. `main.js`: State → `initSystems()` → Assets → Game → `mountUI()`. UI dans `src/ui/` (index, Hud, Screens, Controls, styles, i18n, dom, haptics). `window.__ui` pour debug.
- XP: découverte planète (25), 1er atterrissage (75), pickups, kills, quêtes; × danger planète (`data/balance.js`). Évolution → `hero:evolved {stage,from,id}` + soin complet + capacités. Stades 7-10 verrouillés par flags d'histoire (`requires`).
- Contrôles: joystick flottant (moitié gauche), gros bouton Attaque, Saut, Boost, Capacité (icône du stade), bouton Action **contextuel** (Atterrir/Décoller/…), mode gaucher (miroir), vibrations.
- `npm run ui` (tests/ui.mjs): captures téléphone+tablette de tous les écrans dans tests/out/ui_*.png.

## Demandes inter-Claude (format: `[A→B] / [B→A] date — demande — statut`)
- [B→A] 2026-11-13 — `PlanetScene`: écouter `hero:evolved {stage}` → `hero.setStage(stage)` (+ ta cinématique), et **retirer l'évolution debug sur ✦** (elle écrit `State.hero.stage` hors progression; ✦ = `hero:ability` avec la capacité courante). — ouvert
- [B→A] 2026-11-13 — `Game.js`: gérer `game:pause` / `game:resume` (geler update de scène, garder le rendu) — la pause UI les émet déjà. — ouvert
- [B→A] 2026-11-13 — `Quality`: écouter `settings:quality {level}` (`'auto'` = adaptatif, 0/1/2 = forcé) et appliquer `State.data.settings.quality` au démarrage. — ouvert
- [B→A] 2026-11-13 — `SpaceScene`: 16 planètes maintenant, sur ~10 000 u (systèmes lointains: Interdit, Cimetière, Cœur). Il faudra warp/floating-origin (A3) et ne pas afficher les planètes `hidden` absentes de `State.data.world.discovered`. Inclure `distance` réelle dans `space:near` (déjà fait ✔). — ouvert
- [B→A] 2026-11-13 — `PlanetScene`: rendre `planet.props/npcs/pickups/enemies/structures` (A1); ignorer les pickups dont l'id ∈ `State.data.world.collected`; émettre `pickup:collected {id,kind}` et `npc:interact {npcId}`. Kinds de props utilisés: tree grass rock flower pylon crate pipe lava_vent crystal(color) dead_tree bones cactus ice_spire snow_rock crystal_tree mushroom glow_plant palm coral wall wreck ruin void_spike (fallback: rock). — ouvert
- [B→A] 2026-11-13 — `tests/smoke.mjs`: l'écran titre recouvre maintenant la capture `space.png`; si tu veux une capture en jeu: `await p.click('.cbtn')` avant. — info

## Modèles Tripo reçus / à intégrer
(l'utilisateur dépose les GLB dans `public/models/`; B active dans manifest.json; A vérifie l'échelle/orientation)
