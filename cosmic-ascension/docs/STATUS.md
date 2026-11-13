# STATUS — journal vivant (chaque Claude édite SA section uniquement)

## État global
Squelette jouable livré par le coordinateur (session setup): scène espace (vaisseau, 4 planètes, nébuleuse shader, étoiles), scène planète à pied (gravité sphérique, héros procédural 6 stades), contrôles tactiles paysage, bloom, qualité adaptative, AssetLibrary GLB+fallback, smoke test. `npm run build && npm run smoke` OK.
Bugs/limites connus: terrain facetté (lissage à faire, A1), héros peu lisible sous brouillard, pas de HUD ni de contenu (B).

## Claude A (Fable 5.2) — Engine & Spectacle
- [~] A1 Planète à pied (base faite)  - [~] A2 Héros (anim + cinématique faites)  - [~] A3 Espace (base faite)  - [~] A4 Transition (fondu + chute/décollage faits)  - [~] A5 Boss (serpent visible, pas encore combattable)  - [ ] A6 Shaders/post  - [~] A7 Combat visuel (base faite)  - [ ] A8 Rendu planète joueur  - [ ] A9 Perf
Notes / décisions:
- **Fait (jalon 1)**: terrain lissé haute densité (`buildPlanet(def,{surface:true})`), eau/lave toon avec écume de rivage (`Water.js`), sol « peint » (détail shader), ciel atmosphérique + soleil (`SkyDome.js`), jour/nuit naturel selon la position, décors instanciés par biome avec vent (`world/Props.js`: tree pine rock crystal mushroom grass flower spire; biomes meadow volcanic ice jungle desert), collisions props, ombre blob, particules poolées (`vfx/Particles.js`), orbes de poussière à collecter (`pickup:collected {id,kind:'cosmicDust',amount}`), anim héros squash&stretch, **cinématique d'évolution** (`vfx/EvolutionFX.js`, écoute `hero:evolved`, émet `cinematic:start/end` et `hero:transformed`), caméra orbite (glisser X/Y).
- Nouveaux champs data optionnels lus par A: `planet.freq` (fréquence du relief), `planet.water:false`, `planet.waterColors{shallow,deep,foam}`, `planet.props[{kind,density,scale?,colors?[]}]` (sinon défauts du biome), biome `desert` dispo.
- Le bouton ✦ fait évoluer le héros (TEMP, à retirer quand B4 émet `hero:evolved`).
- **Fait (jalon 2)**: espace vivant (`world/SpaceDecor.js`: 2 champs d'astéroïdes avec rebond, station tournante, soleil shader, galaxies lointaines), boost warp (traits de vitesse `vfx/SpeedLines.js`, FOV, traînée moteur), atterrissage cinématique (pilote auto + fondu couleur d'atmosphère + chute du héros depuis le ciel + impact), décollage. `ctx.transition(name, params, {color,out,in})` dispo pour tout fondu de scène. Nouveaux événements: `ship:bump {speed}`, `cinematic:start/end {id:'landing'|'takeoff'|'evolution'}`.
- **Fait (jalon 3)**: `entities/Enemies.js` — ennemis poolés (slime, scarab, brute; GLB `enemy_*` si présents), IA de secours (errance/poursuite/contact, fuient les formes géantes), projectiles du héros avec visée auto (bouton ✸ / touche J), recul, réapparition. Lit `planet.enemies[{type,count,lat?,lon?,radius?}]` sinon population par défaut. Événements: `enemy:hit {uid,type,dmg,hp,pos}`, `enemy:touch {uid,type,dmg,pos}`, `enemy:killed {uid,type,pos,xp}`, `hero:hit {pos}`, `hero:ability {id:'starbolt',pos,dir}`; accepte `enemy:order {uid,order:'die'}`.
- `entities/boss/Serpent.js` — Serpent Céleste (≈3000 u de long, tête plus grosse que Pépin) qui nage entre les planètes; rebond sur son corps; première rencontre: `boss:spawn {id:'serpent'}` + `cinematic:start/end {id:'boss-sighting'}`.
- [A a touché `ui/Controls.js`: ajout du bouton ✸ `attack` — B, garde-le quand tu refais les contrôles.]
- **Fait (jalon 4)**: boucle de jeu complète jouable. `core/Fallback.js` = **systèmes de secours** (PV, XP, seuils d'évolution, mort/renaissance, mini-HUD, toasts) actifs tant que `Config.systemsOwner !== 'B'` → B: mets ce drapeau quand tes systèmes prennent le relais. ✦ = onde de choc (rayon/dégâts selon le stade). Combat du Serpent: lasers du vaisseau (✸, visée assistée), 6 cœurs stellaires à détruire, crachats téléguidés (`boss:hit-hero {dmg}`), rage par phase (`boss:phase {phase,coresLeft}`), mort en cascade (`boss:defeated {id,xp}`), drapeau `State.data.world.flags.serpentDefeated`. Événements: `ship:fire`, `hero:respawn`.
- **Fait (jalon 5)**: `world/Structures.js` — villages (huttes), habitants animés qui se tournent vers le héros (⏎ = parler → `npc:interact {npcId,name,line}`; proximité → `npc:near {npcId|null,name}`), ruines, monolithe secret (`secret:found {id,planet,xp}`), nuages vus du sol, particules d'ambiance par biome, vaisseau garé au point d'arrivée. Lit `planet.structures[{kind:'hut'|'ruin'|'monolith',lat,lon,scale?}]` et `planet.npcs[{id,name,species?,lat,lon,model?,line?}]`; sans data → génération déterministe par défaut. GLB `npc_<species|biome>` utilisés s'ils existent.
- À faire ensuite: formes du héros plus riches, A6 post-process, A8 planète du joueur, autres boss.
- Note perf: le smoke tourne en rendu logiciel (~8 fps), non représentatif; à mesurer sur vrai appareil.

## Claude B (Opus 5.5) — Systèmes, Contenu, UI, Android
- [ ] B1 UI/HUD  - [ ] B2 Données  - [ ] B3 Quêtes/dialogues  - [ ] B4 Progression  - [ ] B5 Combat logique  - [ ] B6 Planète joueur  - [ ] B7 Collections/save  - [ ] B8 Audio  - [ ] B9 Android  - [ ] B10 Tests
Notes / décisions:

## Demandes inter-Claude (format: `[A→B] / [B→A] date — demande — statut`)
- [A→B] — Écoute `pickup:collected` (poussière cosmique → inventaire/XP) et émets `hero:evolved {stage}` aux seuils d'XP: la cinématique se joue toute seule. — ouvert
- [A→B] — PV du héros: écoute `enemy:touch {dmg}` → `hero:damaged`/`hero:died`; XP: `enemy:killed {xp}`. — ouvert
- [A→B] — Cache le HUD entre `cinematic:start` et `cinematic:end`. — ouvert
- [A→B] — Dans `planets.js`, renseigne `npcs/structures/enemies/pickups` (lat/lon): je les affiche dès qu'ils existent. — ouvert

## Modèles Tripo reçus / à intégrer
(l'utilisateur dépose les GLB dans `public/models/`; B active dans manifest.json; A vérifie l'échelle/orientation)
