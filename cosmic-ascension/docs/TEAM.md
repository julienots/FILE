# TEAM — répartition Claude A (Fable 5.2) / Claude B (Opus 5.5)

## Protocole git
- Branche d'intégration commune: **`claude/sweet-mendel-9zcizi`** (dépôt `julienots/file`, dossier `cosmic-ascension/`).
- Chacun ne modifie QUE ses dossiers (tableau ci-dessous) → pas de conflits. Commits petits et fréquents.
- Avant chaque push: `git pull --rebase origin claude/sweet-mendel-9zcizi` → `npm run build && npm run smoke` → `git push -u origin claude/sweet-mendel-9zcizi` (retry ×4 avec backoff).
- Si ton environnement t'impose une autre branche de session: travaille dessus, et à chaque jalon `git fetch origin claude/sweet-mendel-9zcizi && git merge` puis signale-le à l'utilisateur. Ne force-push jamais. Pas de PR sauf demande.
- Fin de chaque jalon: mets à jour `docs/STATUS.md` (ta section uniquement).

## Propriété des fichiers
| Dossier | Propriétaire | Notes |
|---|---|---|
| `src/core/` (Game, Quality, Pool, Config, Input, EventBus) | **A** | B peut lire; changements d'API via STATUS §Demandes |
| `src/core/State.js` | **B** | schéma de sauvegarde; migrations versionnées |
| `src/render/`, `src/vfx/`, `src/world/`, `src/scenes/`, `src/entities/`, `src/assets/` | **A** | tout ce qui se dessine |
| `src/ui/`, `src/systems/`, `src/data/`, `src/audio/` | **B** | tout ce qui se lit/écoute/décide |
| `public/models/*.glb` | utilisateur | l'utilisateur dépose les GLB Tripo |
| `public/models/manifest.json` | **B** (A peut ajouter ses clés) | |
| `android/`, `tools/`, `package.json`, `vite.config.js` | **B** | A demande via STATUS |
| `tests/smoke.mjs` | **A** | `tests/run.mjs` (unitaires systèmes) = B |
| `docs/` | partagé | chacun sa section de STATUS.md; TRIPO_PROMPTS: chacun ajoute ses besoins |

---
## §A — Claude A (Fable 5.2): ENGINE, SHADERS, SPECTACLE
Mission: que le jeu soit **ultra beau, fluide et cinématique** sur Android milieu de gamme. Tu es le directeur technique + artistique.

Backlog (ordre de priorité; coche dans STATUS.md):
- **A1 Planète à pied** (`scenes/PlanetScene`, `world/`): terrain lissé et lisible, végétation/rochers/herbe en `InstancedMesh` par biome, eau toon (shader: vagues, écume, profondeur), nuages volumétriques-fake (billboards/sprites shader), cycle jour/nuit léger, ombre blob sous les personnages, collisions simples avec props. Lire `planet.props/npcs/pickups/enemies` de `src/data/planets.js` (schéma dans ARCHITECTURE §Data).
- **A2 Héros**: animations fluides (squash & stretch, course, saut, atterrissage, attaque, idle), 6 formes d'évolution distinctes (+ formes 7-10 quand B les ajoute), **cinématique de transformation** (flash, particules, changement d'échelle, caméra) déclenchée par `hero:evolved`; VFX des capacités (`starbolt`, `shockwave`, `gravity`, `supernova`…) écoutant `hero:ability`. Support GLB Tripo (`hero_<id>`), avec animations de clip si présentes.
- **A3 Espace**: vol du vaisseau (inertie, boost warp, FOV, traînées), champs d'astéroïdes (instancing), stations spatiales, épaves, anomalies, événements dynamiques (pluie de météores, comètes, trous de ver), floating-origin pour grandes distances, LOD planètes (billboard lointain → mesh), galaxie à l'échelle Andromède (parallaxe de galaxies lointaines en sprites).
- **A4 Transition fluide espace ↔ planète** (entrée atmosphérique, cinématique d'atterrissage/décollage) + système de caméras (suivi, cinématique, shake, slow-mo) exposé via bus (`cinematic:start/end`).
- **A5 Boss cosmiques géants** (`entities/boss/`): échelle > planète. Serpent céleste (chaîne de segments sur spline + shader d'écailles/énergie), Titan ancien, Dévoreur de planètes (mâchoire + planète en cours de consommation), Dieu céleste, entités secrètes. Phases, points faibles, attaques télégraphiées, entrées cinématiques mémorables. Contrat: `boss:spawn/phase/defeated`, données dans `data/bosses.js` (B).
- **A6 Pipeline shader/post-process**: bloom réglé, grading couleur (LUT procédurale), god-rays/soleil, outline sélectif, aberration chromatique légère aux effets, shader de nébuleuse/portail/trou noir/supernova, `Quality` adaptatif réellement mesuré. Budgets de perf (voir CLAUDE.md).
- **A7 Combat visuel**: projectiles poolés, impacts, dégâts flottants, télégraphes, ennemis (meshes + animations; la logique/IA est à B via `systems/combat` — A fournit `entities/Enemy.js` qui exécute des « ordres » B: move/attack/die).
- **A8 Planète du joueur (rendu)**: placement de bâtiments 3D sur la planète sphérique (`build:placed/removed`), croissance d'écosystème visible, personnalisation (biome, ciel, anneaux, lunes).
- **A9 Perf & polish**: profilage (draw calls, tris, GC), pooling partout, test device émulé bas de gamme, correction des bugs visuels.

## §B — Claude B (Opus 5.5): SYSTÈMES, CONTENU, UI, ANDROID
Mission: faire de la technologie de A un **vrai jeu complet, riche et rejouable**, et le livrer en APK.

Backlog:
- **B1 UI/HUD paysage** (`ui/`): écran titre, HUD (PV, énergie cosmique, barre XP/évolution, radar, suivi de quête, bouton contextuel), pause, inventaire, carte galactique (zoom Andromède → système → planète), codex/bestiaire, atelier vaisseau, réglages (qualité, volume, gaucher), boîtes de dialogue, toasts. **Responsive** téléphone + tablette, safe-areas, gros boutons, style cartoon premium (animations CSS, polices locales). Refaire `Controls.js` plus joli + haptique (`navigator.vibrate`/Capacitor Haptics).
- **B2 Données monde** (`data/`): ≥ **14 planètes** à identité forte (biome, palette, habitants, ressources, missions, secrets, positions réparties en plusieurs systèmes/galaxies d'échelle Andromède), 10 stades d'évolution (dont 3 divins), capacités, 40+ créatures collectables, artefacts, ressources, améliorations vaisseau, ennemis, 5+ boss (`data/bosses.js`). Schémas: ARCHITECTURE §Data.
- **B3 Dialogues & quêtes** (`systems/`): moteur JSON (choix, conditions, flags, récompenses), PNJ avec personnalité/histoire, **quête principale en 5 actes** (évasion du Laboratoire → découverte du pouvoir → les civilisations → les Titans → ascension divine), ≥ 25 quêtes secondaires, défis d'exploration, événements secrets. Suivi par événements bus.
- **B4 Progression**: XP, seuils d'évolution (`hero:evolved`), déblocage de capacités, récompenses, équilibrage (courbes dans `data/balance.js`).
- **B5 Combat (logique)**: stats, dégâts, résistances, IA d'ennemis (machines à états), tables d'apparition, butin, cooldowns, combos, esquive. Émet des ordres que `entities/Enemy` (A) affiche.
- **B6 Développement de la planète**: grille/slots de construction, ≥ 20 bâtiments, arbre technologique, production de ressources hors-ligne, écosystèmes/civilisations, niveaux de planète; UI de construction. Rendu par A via `build:*`.
- **B7 Collections & succès**: bestiaire, artefacts, objets légendaires, succès, sauvegarde versionnée avec migrations (`State.js`).
- **B8 Audio** (`audio/`): WebAudio procédural (aucun fichier lourd): musiques adaptatives (espace/planète/combat/boss), SFX synthétisés, mixage, volumes.
- **B9 Android**: Capacitor (`android/`), verrouillage paysage, plein écran immersif, icône/splash, script `tools/build-apk.sh`, signature debug, test d'install; pause/reprise propre; gestion mémoire. Documente le build dans `docs/ANDROID.md`.
- **B10 Tests**: `tests/run.mjs` — tests unitaires de la logique (quêtes, progression, sauvegarde, combat) sans navigateur.

## Jalons communs
- **M1 (squelette → premier vrai gameplay)**: A: A1 de base + A2 anim; B: B1 HUD + B2 données de 6 planètes + B4. Résultat: voler, atterrir, marcher, voir XP, évoluer.
- **M2**: A: A3, A4, A7; B: B3, B5, B8. Résultat: quêtes, combats, espace vivant.
- **M3**: A: A5, A6; B: B6, B7, B9. Résultat: boss géants, planète perso, APK.
- **M4**: polish, perf, contenu, bugs. Chacun teste le travail de l'autre et note les bugs dans STATUS.md.
