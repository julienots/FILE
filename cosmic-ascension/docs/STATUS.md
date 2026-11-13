# STATUS — journal vivant (chaque Claude édite SA section uniquement)

## État global
Squelette jouable livré par le coordinateur (session setup): scène espace (vaisseau, 4 planètes, nébuleuse shader, étoiles), scène planète à pied (gravité sphérique, héros procédural 6 stades), contrôles tactiles paysage, bloom, qualité adaptative, AssetLibrary GLB+fallback, smoke test. `npm run build && npm run smoke` OK.
Bugs/limites connus: terrain facetté (lissage à faire, A1), héros peu lisible sous brouillard, pas de HUD ni de contenu (B).

## Claude A (Fable 5.2) — Engine & Spectacle
- [~] A1 Planète à pied (base faite)  - [~] A2 Héros (anim + cinématique faites)  - [ ] A3 Espace  - [ ] A4 Transition/caméras  - [ ] A5 Boss  - [ ] A6 Shaders/post  - [ ] A7 Combat visuel  - [ ] A8 Rendu planète joueur  - [ ] A9 Perf
Notes / décisions:
- **Fait (jalon 1)**: terrain lissé haute densité (`buildPlanet(def,{surface:true})`), eau/lave toon avec écume de rivage (`Water.js`), sol « peint » (détail shader), ciel atmosphérique + soleil (`SkyDome.js`), jour/nuit naturel selon la position, décors instanciés par biome avec vent (`world/Props.js`: tree pine rock crystal mushroom grass flower spire; biomes meadow volcanic ice jungle desert), collisions props, ombre blob, particules poolées (`vfx/Particles.js`), orbes de poussière à collecter (`pickup:collected {id,kind:'cosmicDust',amount}`), anim héros squash&stretch, **cinématique d'évolution** (`vfx/EvolutionFX.js`, écoute `hero:evolved`, émet `cinematic:start/end` et `hero:transformed`), caméra orbite (glisser X/Y).
- Nouveaux champs data optionnels lus par A: `planet.freq` (fréquence du relief), `planet.water:false`, `planet.waterColors{shallow,deep,foam}`, `planet.props[{kind,density,scale?,colors?[]}]` (sinon défauts du biome), biome `desert` dispo.
- Le bouton ✦ fait évoluer le héros (TEMP, à retirer quand B4 émet `hero:evolved`).
- À faire ensuite: PNJ/structures/ennemis depuis la data, nuages vus du sol, A3 espace, A4 transition.
- Note perf: le smoke tourne en rendu logiciel (~8 fps), non représentatif; à mesurer sur vrai appareil.

## Claude B (Opus 5.5) — Systèmes, Contenu, UI, Android
- [ ] B1 UI/HUD  - [ ] B2 Données  - [ ] B3 Quêtes/dialogues  - [ ] B4 Progression  - [ ] B5 Combat logique  - [ ] B6 Planète joueur  - [ ] B7 Collections/save  - [ ] B8 Audio  - [ ] B9 Android  - [ ] B10 Tests
Notes / décisions:

## Demandes inter-Claude (format: `[A→B] / [B→A] date — demande — statut`)
- [A→B] — Écoute `pickup:collected` (poussière cosmique → inventaire/XP) et émets `hero:evolved {stage}` aux seuils d'XP: la cinématique se joue toute seule. — ouvert
- [A→B] — Cache le HUD entre `cinematic:start` et `cinematic:end`. — ouvert
- [A→B] — Dans `planets.js`, renseigne `npcs/structures/enemies/pickups` (lat/lon): je les affiche dès qu'ils existent. — ouvert

## Modèles Tripo reçus / à intégrer
(l'utilisateur dépose les GLB dans `public/models/`; B active dans manifest.json; A vérifie l'échelle/orientation)
