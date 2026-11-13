# ARCHITECTURE & CONTRATS

```
cosmic-ascension/
  index.html  vite.config.js  package.json  CLAUDE.md
  public/models/        GLB Tripo + manifest.json (clé → fichier, hauteur en m)
  src/
    main.js             bootstrap (State.load → Assets.init → Game → scènes → UI)
    core/               Game(boucle+scènes) Quality(FPS auto) Pool EventBus Input Config State
    render/             createRenderer(bloom) shaders/ {Nebula Stars Toon Atmosphere noise.glsl}
    world/              PlanetFactory(noise, planètes procédurales)
    scenes/             SpaceScene  PlanetScene  (+ futur HomeScene…)
    entities/           Hero Ship (+ Enemy, Npc, boss/…)
    vfx/                particules, effets (A)
    assets/             AssetLibrary (GLB + fallback)
    ui/ systems/ data/ audio/   (B)
  tests/                smoke.mjs (A)  run.mjs (B)
```

## Interface de scène
`{ scene: THREE.Scene, camera, enter(params), update(dt, t), exit() }` — enregistrée via `game.register(name, Class)`, navigation `ctx.goto(name, params)`.
Scènes: `space` · `planet {id}` · (prévu) `home`, `boss {id}`.

## Entrées
Lire **uniquement** `Input.move{x,y}`, `Input.look{x,y}`, `Input.buttons`, `Input.pressed` (front montant). Boutons: `action attack boost jump special`. B peut ajouter des boutons contextuels dans `ui/Controls.js` mais écrit toujours dans `Input`.

## Bus d'événements (`bus.on/emit`) — nom → payload
| Événement | Émis par | Payload | Écouté par |
|---|---|---|---|
| `scene:changed` / `scene:entered` | A | `{name,params}` / `{id,planet?}` | B (HUD, audio, quêtes) |
| `space:near` | A | `{planet,canLand,distance}` | B HUD |
| `planet:enter` / `planet:leave` | A | `{id}` | B quêtes |
| `hero:xp` | B | `{amount}` | B |
| `hero:evolved` | B (progression) | `{stage}` | A (cinématique + forme) |
| `hero:ability` | A (input) | `{id, pos, dir}` | A VFX, B combat |
| `hero:damaged` / `hero:died` | B | `{amount,source}` | A (flash), UI |
| `enemy:spawn` | B | `{uid,type,pos,level}` (pos = {lat,lon} ou xyz) | A (crée l'Enemy) |
| `enemy:order` | B | `{uid,order:'move'|'attack'|'die',…}` | A |
| `enemy:hit` | A | `{uid,dmg,pos}` | B |
| `enemy:killed` | B | `{uid,type,pos}` | quêtes, loot |
| `npc:interact` | A | `{npcId}` | B dialogues |
| `dialogue:start/end` | B | `{npcId}` | A (gèle le joueur, caméra) |
| `quest:started/progress/done` | B | `{id,…}` | UI, A (marqueurs) |
| `hero:transformed` | A | `{stage}` (instant du flash) | B audio/UI |
| `pickup:collected` | A | `{id,kind,amount}` | B inventaire/quêtes |
| `boss:spawn/phase/defeated` | A/B | `{id,phase?}` | tous |
| `cinematic:start/end` | A | `{id}` | B (cache HUD) |
| `build:placed/removed` | B | `{planetId,slot,buildingId}` | A rendu |
| `ship:bump` | A | `{speed}` (choc astéroïde) | B (dégâts/son) |
| `ship:upgraded` | B | `{level,upgrades}` | A visuels |
| `quality:changed` | A | preset | tous |
| `ui:toast` | tous | `{text,kind}` | B UI |
| `game:pause` / `game:resume` | B | — | A (stoppe la sim) |
| `game:start` | B | — | quitte l'écran titre (A peut lancer une intro) |
| `progress:xp` | B | `{gained,reason,xp,stage,ratio,need,next}` | UI, audio |
| `hero:stats` | B | `State.data.hero` | UI (PV/énergie changés) |
| `world:discovered` | B | `{id}` | UI, carte |
| `settings:changed` | B | `State.data.settings` | tous (gaucher, volumes…) |
| `settings:quality` | B | `{level:'auto'|0|1|2}` | A (Quality: forcer/verrouiller ou repasser en auto) |
| `ui:context` | B | `{label|null}` | UI (libellé du bouton Action) |
| `quest:tracked` | B | `{title,progress}` | UI (suivi d'objectif) |
| `ui:click` | B | — | audio |
| `enemy:sense` | A | `{uid,dist}` ≤5 Hz, ennemis à < 2× aggro | B IA (aggro/portée/leash) |
| `enemy:hit` | A | `{uid,source:'attack'|abilityId,part?}` (B calcule les dégâts) | B combat |
| `enemy:order` | B | `{uid,order:'idle'|'move'|'attack'|'return'|'dead', target?:'hero'|'home', speed?, windup?, kind?, dmg?}` | A anime (attaque = télégraphe `windup` s; B résout le coup) |
| `enemy:spawn` (détail) | B | `+{hp,maxHp,name,shape,color,size,model,flying,elite,speed}` | A |
| `enemy:damaged` / `enemy:despawn` | B | `{uid,dmg,hp,maxHp,crit,resist}` / `{uid}` | A (chiffres flottants, flash), audio |
| `hero:ability` → `ability:cast` / `ability:fail` | A → B | `{id?}` → `{id,dmg,radius?,range?,cd}` / `{id,reason}` | A joue le VFX **seulement sur `ability:cast`** |
| `hero:respawn` | B | `{planetId}` | A (replace le héros au point d'apparition) |
| `hero:fell` | A | — (chute/eau mortelle) | B (−20 % PV) |
| `combat:combo` | B | `{n}` | UI, audio |
| `boss:spawn` (détail) | B | `{id,name,title,hp,maxHp,scale,arena,attacks,weakPoints,music}` | A crée le boss |
| `boss:hit` / `boss:attackHit` | A | `{id,source,part?}` / `{id,attack}` | B (PV boss / dégâts héros) |
| `boss:phase` / `boss:damaged` / `boss:defeated` / `boss:despawn` | B | `{id,phase,attacks}` / `{id,hp,maxHp,weak}` / `{id,name}` | A, UI, quêtes |
| `npc:near` | A | `{npcId|null}` | B (bouton Action → « Parler ») |
| `npc:talked` | B | `{npcId}` (fin de dialogue) | quêtes |
| `dialogue:node` / `dialogue:choose` | B / UI | `{text,speaker,portrait,radio,choices[]}` / `{i}` | UI / moteur |
| `quest:start` / `quest:track` | tous | `{id}` | B quêtes |
| `world:flag` | B | `{flag,value}` | quêtes, story, progression |
| `inventory:changed` | B | `{id,n}` | UI, quêtes |

Ajout d'événement: documente-le ici (une ligne) dans ton commit.

## Data (B écrit, A lit) — `src/data/`
- `planets.js`: `SYSTEMS[{id,name,color,center,danger}]` + `PLANETS`: `{id,name,type,system,danger(0-4),hidden?,desc,species,radius,seed,pos[3],relief,seaLevel,ring?,palette{deep,shore,low,mid,high,peak},atmosphere{color,intensity},clouds?,biome,`
  `props:[{kind,density,scale?}]` (kinds: tree,palm,crystal,rock,mushroom,grass,ruin,… A mappe kind→mesh/instancing),
  `npcs:[{id,name,species,lat,lon,model?,dialogue}]`, `pickups:[{id,kind,lat,lon}]`, `enemies:[{type,count,lat,lon,radius}]`, `structures:[{kind,lat,lon}]`, `resources[]`, `secrets[]`, `quests[]`}`
- `evolutions.js` (stades), `abilities.js`, `creatures.js`, `artifacts.js`, `enemies.js`, `bosses.js`, `quests/*.js`, `dialogues/*.js`, `buildings.js`, `balance.js`.
- lat/lon en degrés sur la sphère de la planète (A convertit en position + normale).
- Textes en français via `ui/i18n.js` (B), clés pas chaînes en dur dans les systèmes.

## State (sauvegarde) — `core/State.js` (B)
Un seul objet sérialisable, `State.data`, version `v` (actuel: 2); toute évolution de schéma = `MIGRATIONS[v]` + `VERSION++` (testé par `npm test`). Autosave 5 s + à la mise en arrière-plan. Ne jamais y stocker d'objets THREE.
À lire par A: `world.collected` (ids de pickups déjà ramassés → ne pas les instancier), `world.discovered` (planètes `hidden` à n'afficher qu'une fois découvertes), `hero.stage`, `settings`.

## Modèles 3D (Tripo → GLB)
- Déposer dans `public/models/<clé>.glb`, déclarer dans `manifest.json` (`file`, `height` en mètres, `disabled:false`).
- `Assets.get(clé)` → Group normalisé (pieds y=0, centré). Si absent → fallback procédural: **le jeu ne dépend jamais d'un GLB**.
- Budget: ≤ 8k tris héros/PNJ, ≤ 15k boss (on instancie/étend), texture ≤ 1024², 1 matériau idéalement. Optionnel: compresser en meshopt (`gltfpack -cc`), le loader le supporte.
- Convention: orientation +Z = avant, Y haut, échelle 1 unité = 1 m, origine aux pieds.

## Perf (budgets mobiles milieu de gamme)
≤150 draw calls · ≤300k tris visibles · textures ≤1024² · pas d'alloc dans `update` · tout objet répété = `InstancedMesh`/`Pool` · ombres réelles OFF (blob shadows) · bloom demi-résolution · `Quality` baisse seul sous 40 FPS.
