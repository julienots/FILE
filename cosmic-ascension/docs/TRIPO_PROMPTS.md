# PROMPTS TRIPO 3D — pour l'utilisateur

**Comment**: sur Tripo, colle le *Préfixe de style* + le prompt de l'objet (en anglais, Tripo comprend mieux). Génère en **Text-to-3D**, mode *stylized/cartoon* si dispo, **low-poly ≤ 8-10k faces**, avec texture. Exporte en **GLB**. Nomme le fichier comme la clé (`hero_mote.glb`). Pose en **A-pose/T-pose** pour les personnages (rigging possible ensuite via Tripo « Auto-rig » + animations Idle/Run si dispo — optionnel, le jeu anime en procédural sinon). Dépose les GLB dans `cosmic-ascension/public/models/` (ou donne-les moi) puis les Claude activent l'entrée dans `manifest.json`.

**Préfixe de style (à coller avant chaque prompt):**
> **(Nouveau, réaliste — à utiliser désormais)** realistic sci-fi 3D asset, cinematic film quality, PBR materials, physically plausible proportions, detailed but clean surfaces, muted natural colors with subtle emissive accents, weathered and believable, game-ready low poly (≤ 10k faces), single object, neutral pose, no background, centered
>
> (ancien préfixe cartoon, abandonné: stylized cartoon 3D asset, Pixar / Disney animated-film quality, …)

**Suffixe pour objets sans personnage:** `, isolated prop, symmetrical, clean silhouette`

Priorité: **P0** (indispensable tôt) · P1 · P2. Hauteur = taille du jeu en mètres (le jeu normalise).

## Héros (évolution) — P0 : hero_mote, hero_sprout, hero_comet ; puis les autres
| Clé | Prompt |
|---|---|
| `hero_mote` P0 h=1.0 | tiny cute round glowing blob creature, cyan-white translucent jelly body, huge shiny eyes, two tiny stubby feet, small antenna with star tip, innocent expression |
| `hero_sprout` P0 h=1.5 | small cute alien creature evolving, green-cyan skin, short arms and legs appearing, leaf-like crest on head, glowing chest crystal, big curious eyes |
| `hero_comet` P0 h=2.0 | young heroic alien child, golden orange skin, flowing comet-tail of fire from the back, small horns of light, glowing runes on arms, confident grin |
| `hero_nova` P1 h=2.8 | graceful cosmic guardian, pink and magenta body, wings made of pure light shards, floating halo ring, star-pattern markings, glowing eyes, regal pose |
| `hero_titan` P1 h=4.0 | massive stellar titan hero, purple armored-skin with constellation patterns, shoulder pauldrons of floating crystals, glowing cracks of energy, calm powerful face |
| `hero_god` P1 h=7.0 | cosmic deity, white and gold, body made of living starlight and nebula, multiple floating rings and orbiting small planets, crown of light, serene godlike face |
| `hero_chimera`, `hero_voidlord`, `hero_architect`, `hero_cosmos` P2 | (stades 7-10, voir B: variantes de plus en plus abstraites: chimera of nebula smoke with many eyes / lord of the void with black-violet armor and cracked-galaxy cape / architect with geometric floating limbs / cosmos incarnate humanoid made of a galaxy) |

## Vaisseaux — P0 : ship_basic
| `ship_basic` P0 h=3 | tiny cute spaceship, rounded egg-shaped white hull, pink wings, round cyan glass cockpit, small thruster at back, chunky retro-futuristic |
| `ship_explorer` P1 h=4 | upgraded explorer spaceship, teal and white, bigger wings with glowing blue stripes, antenna dish, twin thrusters |
| `ship_cruiser` P2 h=6 | sleek cosmic cruiser, gold and navy hull, crystal engine core, long wings, ornate fins |
| `ship_celestial` P2 h=8 | legendary living spaceship made of starlight, white-gold organic hull with floating wing feathers of light |

## Espèces / PNJ — P1
| `npc_ember` h=2 | friendly lava-folk alien, orange-red rocky skin with glowing cracks, big warm eyes, small flame hair, wearing simple tunic |
| `npc_frost` h=2 | gentle ice-folk alien, pale blue crystalline body, snowflake patterns, fluffy white fur collar, serene eyes |
| `npc_sylva` h=2 | tall elegant plant-folk alien, purple-green bark skin, flower crown, glowing spores, vine clothing |
| `npc_robot` h=1.8 | cute round old maintenance robot, brass and teal, big lens eye, antenna, friendly |
| `npc_scientist` h=1.8 | lab-keeper alien with oversized glasses, white coat, tentacle beard, nervous expression (antagoniste du Labo) |
| `npc_empire_knight` h=2.4 | rival empire soldier, red-black round armor, big helmet with one glowing visor, shield, menacing but cartoonish |

## Ennemis — P1
| `enemy_slime` h=1 | angry star-slime blob, translucent purple, tiny stars inside, big grumpy eyes, little fangs |
| `enemy_drone` h=1 | round floating security drone, white shell with red eye, small spikes, two stubby arms |
| `enemy_scarab` h=1.2 | big cosmic beetle, iridescent blue-green shell, glowing orange eyes, chunky legs |
| `enemy_brute` h=3 | huge rock-and-crystal brute monster, mossy stone body, crystal spikes on back, small angry eyes |

## Créatures à collectionner — P2 (10 pour commencer)
Prompt type: `tiny cute collectible alien pet, <X>, round chibi proportions, big shiny eyes` avec X ∈ {fluffy cloud-fox with comet tail · jelly jellyfish that floats · tiny dragon made of crystal · mushroom bunny glowing spores · rock turtle with a tiny garden on its shell · firefly moth with galaxy wings · ice penguin with scarf · star-whale baby · lava axolotl · cosmic owl with constellation feathers}`. Clés `creature_01…10`, h≈0.8.

## Boss géants — P0 : boss_serpent (le plus spectaculaire) ; ils seront étendus/mis à l'échelle par le jeu
| `boss_serpent` P0 | colossal celestial space serpent, long segmented body, iridescent blue-purple scales with glowing cyan stars along its back, majestic dragon-like head with luminous horns and cosmic eyes, ethereal fins, mouth glowing, coiled S-shape pose (si possible: tête seule + un segment de corps séparés) |
| `boss_titan` P1 | ancient cosmic titan, giant stone-and-star humanoid, body of cracked asteroid rock with glowing golden runes, crown of floating rocks, one glowing eye, hands raised |
| `boss_devourer` P1 | planet-devouring leviathan, enormous round mouth with rings of crystal teeth, dark purple flesh with glowing orange veins, tentacles, menacing |
| `boss_celestial_god` P1 | mysterious celestial god, tall slender figure of white marble and gold, six wings of light, face hidden behind a radiant mask, floating stars around |
| `boss_secret_01` P2 | hidden legendary entity, giant glowing jellyfish-whale of the void with galaxy inside its body, trailing tentacles of stars |

## Planète du joueur — bâtiments (P1) — h≈3, style maquette cartoon
`bld_lab` (futuristic dome lab, glass dome, antenna) · `bld_greenhouse` (glass greenhouse with giant plants) · `bld_forge` (star forge with glowing furnace and chimney) · `bld_hangar` (round hangar with landing pad) · `bld_observatory` (telescope tower) · `bld_temple` (small cosmic temple with floating crystal) · `bld_market` (colorful alien market stall tent) · `bld_house` (cute round alien house, mushroom-shaped) · `bld_tower` (energy tower with orbiting ring) · `bld_farm` (floating-garden pod)

## Décors / props (P1) — par biome, petits et instanciables (h 1-4)
`prop_tree_meadow` (round fluffy tree) · `prop_tree_jungle` (giant purple mushroom-tree) · `prop_palm_alien` · `prop_crystal_blue` / `prop_crystal_pink` (crystal cluster) · `prop_rock_a/b` (smooth rounded rocks) · `prop_ice_spire` · `prop_lava_vent` · `prop_flower_giant` · `prop_ruin_arch` (ancient arch with glowing glyphs) · `prop_temple_big` (ancient temple on steps) · `prop_portal` (stone ring portal with swirling energy) · `prop_station` (space station, ring + modules) · `prop_wreck` (abandoned crashed spaceship, broken, vines) · `prop_asteroid_a/b/c` (chunky cartoon asteroids with crystals) · `prop_beacon` (glowing quest beacon pillar)
