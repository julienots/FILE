// Monde (B écrit, A lit). Schéma: docs/ARCHITECTURE.md §Data.
// pos = unités monde SpaceScene (absolues). Les 4 planètes d'origine gardent leur position.
// danger 0..4 (couleur radar/carte + multiplicateur d'XP) · hidden: invisible tant que world.discovered ne la contient pas.
// npcs[].dialogue → src/data/dialogues/<id>.js (M2) · quests[] → src/data/quests (M2).

// Systèmes stellaires (carte galactique / saut warp). center = repère pour la carte, color = teinte UI.
export const SYSTEMS = [
  { id: 'pepin',    name: 'Système Pépin',       color: '#7fd0ff', center: [0, 0, -500],      danger: 0, desc: 'Berceau du héros, à l’ombre du Laboratoire.' },
  { id: 'braise',   name: 'Amas de Braise',      color: '#ff8a4a', center: [1400, 200, -1900], danger: 1, desc: 'Forges, volcans et déserts brûlants.' },
  { id: 'glacio',   name: 'Couronne Glaciale',   color: '#b8e8ff', center: [-1500, -100, -2300], danger: 1, desc: 'Mondes gelés sous des aurores éternelles.' },
  { id: 'sylva',    name: 'Bosquet de Sylva',    color: '#a07bff', center: [400, -300, -3400], danger: 2, desc: 'Une nébuleuse où la vie pousse partout.' },
  { id: 'interdit', name: 'Zone Interdite',      color: '#ff3b5c', center: [3800, 600, -5200], danger: 3, desc: 'Territoire de l’Empire Écarlate.' },
  { id: 'cimetiere',name: 'Cimetière Stellaire', color: '#8a7aa8', center: [-4200, 900, -6000], danger: 3, desc: 'Là où les étoiles viennent mourir.' },
  { id: 'coeur',    name: 'Cœur Galactique',     color: '#ffe9a0', center: [0, 1200, -9000],   danger: 4, desc: 'Le centre de tout. Les Titans y dorment.' },
];

// PRNG déterministe pour disperser pickups/ennemis (même résultat à chaque lancement).
function rng(seed) { let s = seed >>> 0 || 1; return () => ((s = (s * 1664525 + 1013904223) >>> 0) / 4294967296); }
function scatter(pid, seed, list) {
  const r = rng(seed * 7919), out = [];
  for (const [kind, n] of list) for (let i = 0; i < n; i++) out.push({ id: `${pid}_${kind}_${i}`, kind, lat: Math.round((r() * 150 - 75) * 10) / 10, lon: Math.round((r() * 360 - 180) * 10) / 10 });
  return out;
}

export const PLANETS = [
  // ───── Système Pépin ─────
  { id: 'home', name: 'Pépin', type: 'home', system: 'pepin', danger: 0, radius: 90, seed: 11, pos: [0, 0, -420], relief: .06, seaLevel: -.05,
    palette: { deep: '#2a6fe0', shore: '#f3e3a1', low: '#6fd36a', mid: '#3fb55a', high: '#8a9a5b', peak: '#ffffff' },
    atmosphere: { color: '#7fd0ff' }, clouds: '#ffffff', biome: 'meadow', species: 'Pipions',
    desc: 'Une petite planète douce et verte. Ton futur foyer.',
    props: [{ kind: 'tree', density: .7 }, { kind: 'grass', density: 1 }, { kind: 'rock', density: .3 }, { kind: 'flower', density: .5 }],
    npcs: [{ id: 'bolt', name: 'Boulon', species: 'robot', lat: 12, lon: 8, model: 'npc_robot', dialogue: 'bolt_intro' },
           { id: 'mayor_pip', name: 'Maire Pipo', species: 'pipion', lat: -6, lon: 32, dialogue: 'mayor_intro' }],
    pickups: scatter('home', 11, [['dust', 14], ['crystal', 4], ['creature', 1]]),
    enemies: [{ type: 'slime', count: 4, lat: 30, lon: -40, radius: 15 }],
    structures: [{ kind: 'beacon', lat: 0, lon: 0 }, { kind: 'ship_pad', lat: 5, lon: -5 }],
    resources: ['cosmicDust'], secrets: ['home_hollow_tree'], quests: ['main_1_awakening', 'side_lost_pipion'] },

  { id: 'lab', name: 'Laboratoire Ω', type: 'artificial', system: 'pepin', danger: 1, radius: 70, seed: 3, pos: [-260, 140, -150], relief: .03, seaLevel: -.3,
    palette: { deep: '#1a1f3a', shore: '#5d6a8a', low: '#c9d3e8', mid: '#9aa7c4', high: '#e9eef8', peak: '#7ff0ff' },
    atmosphere: { color: '#7ff0ff', intensity: .7 }, biome: 'lab', species: 'Gardiens',
    desc: 'La simulation d’où tu t’es échappé. Les machines te cherchent encore.',
    props: [{ kind: 'pylon', density: .6 }, { kind: 'crate', density: .5 }, { kind: 'pipe', density: .4 }],
    npcs: [{ id: 'dr_quill', name: 'Dr Quill', species: 'scientist', lat: 40, lon: 60, model: 'npc_scientist', dialogue: 'quill_lab' }],
    pickups: scatter('lab', 3, [['dust', 8], ['relic', 2]]),
    enemies: [{ type: 'drone', count: 6, lat: 0, lon: 90, radius: 25 }, { type: 'drone', count: 4, lat: -30, lon: -90, radius: 20 }],
    structures: [{ kind: 'lab_dome', lat: 40, lon: 60 }, { kind: 'portal', lat: -50, lon: 0 }],
    resources: ['scrapAlloy', 'labKeycard'], secrets: ['lab_server_room'], quests: ['main_1_escape', 'side_drone_parts'] },

  // ───── Amas de Braise ─────
  { id: 'ember', name: 'Braise-9', type: 'volcanic', system: 'braise', danger: 1, radius: 130, seed: 23, pos: [760, 120, -1300], relief: .12, seaLevel: -.2,
    palette: { deep: '#ff4a1c', shore: '#ffb347', low: '#5a2d3a', mid: '#7b3f4a', high: '#c76a4a', peak: '#ffe08a' },
    atmosphere: { color: '#ff8a4a', intensity: 1.4 }, biome: 'volcanic', species: 'Braisiens',
    desc: 'Des rivières de lave et un peuple de forgerons au cœur chaud.',
    props: [{ kind: 'lava_vent', density: .5 }, { kind: 'rock', density: .8 }, { kind: 'crystal', density: .3, color: '#ff7a3c' }],
    npcs: [{ id: 'forga', name: 'Forga la Fondeuse', species: 'ember', lat: 18, lon: 20, model: 'npc_ember', dialogue: 'forga_intro' },
           { id: 'tizon', name: 'P’tit Tison', species: 'ember', lat: 22, lon: 24, model: 'npc_ember', dialogue: 'tizon' }],
    pickups: scatter('ember', 23, [['dust', 12], ['crystal', 6], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'magmaSlime', count: 6, lat: -20, lon: 70, radius: 25 }, { type: 'scarab', count: 3, lat: 45, lon: -120, radius: 20 }],
    structures: [{ kind: 'forge_city', lat: 18, lon: 20 }, { kind: 'ruin', lat: -60, lon: 150 }],
    resources: ['emberCore'], secrets: ['ember_magma_heart'], quests: ['main_2_forge', 'side_tizon_lost', 'side_cool_the_core'] },

  { id: 'cinder', name: 'Cendrelune', type: 'ash', system: 'braise', danger: 2, radius: 80, seed: 29, pos: [1150, 420, -1700], relief: .09, seaLevel: -.4,
    palette: { deep: '#2a2228', shore: '#4a3c40', low: '#6b5a5c', mid: '#8c7a78', high: '#b8a8a0', peak: '#ff9a5a' },
    atmosphere: { color: '#c08a70', intensity: .9 }, biome: 'ash', species: 'Spectres de cendre',
    desc: 'La lune grise de Braise-9, couverte des cendres d’une ancienne guerre.',
    props: [{ kind: 'dead_tree', density: .5 }, { kind: 'rock', density: .6 }, { kind: 'bones', density: .3 }],
    npcs: [{ id: 'ash_hermit', name: 'L’Ermite Gris', species: 'ember', lat: -40, lon: 10, dialogue: 'hermit' }],
    pickups: scatter('cinder', 29, [['dust', 8], ['relic', 3], ['creature', 1]]),
    enemies: [{ type: 'ashWraith', count: 5, lat: 10, lon: 100, radius: 30 }],
    structures: [{ kind: 'ruin', lat: -40, lon: 10 }, { kind: 'wreck', lat: 30, lon: -60 }],
    resources: ['starIron'], secrets: ['cinder_war_memorial'], quests: ['side_hermit_memory'] },

  { id: 'dune', name: 'Ossable', type: 'desert', system: 'braise', danger: 1, radius: 120, seed: 41, pos: [1800, -60, -2100], relief: .07, seaLevel: -.5,
    palette: { deep: '#3fb0c0', shore: '#ffe9b0', low: '#f5c97a', mid: '#e8a95a', high: '#c4733e', peak: '#fff3d0' },
    atmosphere: { color: '#ffcf8a', intensity: 1.1 }, biome: 'desert', species: 'Nomades Sablons',
    desc: 'Mer de dunes dorées, squelettes géants et caravanes volantes.',
    props: [{ kind: 'cactus', density: .5 }, { kind: 'bones', density: .4 }, { kind: 'rock', density: .5 }],
    npcs: [{ id: 'caravan_zaya', name: 'Zaya des Caravanes', species: 'sablon', lat: 5, lon: 45, dialogue: 'zaya' }],
    pickups: scatter('dune', 41, [['dust', 12], ['crystal', 4], ['creature', 2], ['relic', 2]]),
    enemies: [{ type: 'scarab', count: 7, lat: -15, lon: -60, radius: 35 }],
    structures: [{ kind: 'giant_skeleton', lat: -30, lon: 120 }, { kind: 'oasis', lat: 5, lon: 45 }],
    resources: ['sandPearl'], secrets: ['dune_buried_titan'], quests: ['side_caravan_escort', 'side_titan_bones'] },

  // ───── Couronne Glaciale ─────
  { id: 'glacio', name: 'Glacio', type: 'ice', system: 'glacio', danger: 1, radius: 110, seed: 37, pos: [-900, -80, -1700], relief: .09, seaLevel: .05, ring: '#cfeaff',
    palette: { deep: '#3aa0e8', shore: '#d9f4ff', low: '#e8fbff', mid: '#cfefff', high: '#ffffff', peak: '#ffffff' },
    atmosphere: { color: '#b8e8ff' }, clouds: '#f4fbff', biome: 'ice', species: 'Givrins',
    desc: 'Banquises chantantes et villages de cristal.',
    props: [{ kind: 'ice_spire', density: .6 }, { kind: 'crystal', density: .5, color: '#9fe0ff' }, { kind: 'snow_rock', density: .4 }],
    npcs: [{ id: 'elder_frost', name: 'Aïeule Névé', species: 'frost', lat: 50, lon: -20, model: 'npc_frost', dialogue: 'neve_intro' }],
    pickups: scatter('glacio', 37, [['dust', 12], ['crystal', 6], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'frostSlime', count: 5, lat: 0, lon: 80, radius: 25 }, { type: 'iceGolem', count: 1, lat: -40, lon: 160, radius: 5 }],
    structures: [{ kind: 'ice_village', lat: 50, lon: -20 }, { kind: 'frozen_ship', lat: -10, lon: 120 }],
    resources: ['frostCrystal'], secrets: ['glacio_singing_cave'], quests: ['main_3_glacio', 'side_frozen_ship'] },

  { id: 'aurora', name: 'Aurorée', type: 'crystal', system: 'glacio', danger: 2, radius: 95, seed: 43, pos: [-1650, 160, -2500], relief: .11, seaLevel: -.15,
    palette: { deep: '#3a2a8a', shore: '#8ae8ff', low: '#5ad8c8', mid: '#7ab0ff', high: '#d08aff', peak: '#ffffff' },
    atmosphere: { color: '#7affd8', intensity: 1.2 }, clouds: '#d8fff4', biome: 'crystal', species: 'Prismes',
    desc: 'Forêts de cristal sous des aurores qui ne s’éteignent jamais.',
    props: [{ kind: 'crystal', density: .9, color: '#c08aff' }, { kind: 'crystal_tree', density: .5 }],
    npcs: [{ id: 'prism_oracle', name: 'L’Oracle Prisme', species: 'prism', lat: 70, lon: 0, dialogue: 'oracle' }],
    pickups: scatter('aurora', 43, [['dust', 10], ['crystal', 8], ['creature', 2]]),
    enemies: [{ type: 'shardBat', count: 6, lat: -20, lon: -100, radius: 30 }],
    structures: [{ kind: 'temple', lat: 70, lon: 0 }],
    resources: ['lumenSap', 'frostCrystal'], secrets: ['aurora_light_bridge'], quests: ['main_3_oracle', 'side_prism_tuning'] },

  // ───── Bosquet de Sylva ─────
  { id: 'sylva', name: 'Sylva-Mère', type: 'jungle', system: 'sylva', danger: 2, radius: 160, seed: 51, pos: [300, -200, -2600], relief: .1, seaLevel: -.1,
    palette: { deep: '#1ea0a0', shore: '#e7d7a0', low: '#2fbf4a', mid: '#1f9a4a', high: '#c47bff', peak: '#ffd1ff' },
    atmosphere: { color: '#a07bff' }, clouds: '#ffe4ff', biome: 'jungle', species: 'Sylvains',
    desc: 'Une jungle-mère consciente, aux arbres plus vieux que les étoiles.',
    props: [{ kind: 'tree', density: 1, variant: 'jungle' }, { kind: 'mushroom', density: .5 }, { kind: 'flower', density: .7 }, { kind: 'grass', density: 1 }],
    npcs: [{ id: 'sylvana', name: 'Sylvana', species: 'sylva', lat: -10, lon: 0, model: 'npc_sylva', dialogue: 'sylvana_intro' },
           { id: 'root_elder', name: 'Vieille-Racine', species: 'sylva', lat: -60, lon: 90, dialogue: 'root_elder' }],
    pickups: scatter('sylva', 51, [['dust', 14], ['crystal', 4], ['creature', 3], ['artifact', 1]]),
    enemies: [{ type: 'thornBeast', count: 5, lat: 30, lon: 140, radius: 30 }, { type: 'brute', count: 1, lat: 60, lon: -60, radius: 5 }],
    structures: [{ kind: 'tree_city', lat: -10, lon: 0 }, { kind: 'ruin', lat: 20, lon: -150 }],
    resources: ['sporeSilk'], secrets: ['sylva_heart_tree'], quests: ['main_3_sylva', 'side_seed_of_life', 'side_lost_saplings'] },

  { id: 'mycelia', name: 'Mycélia', type: 'mushroom', system: 'sylva', danger: 2, radius: 85, seed: 57, pos: [-120, -420, -3200], relief: .1, seaLevel: -.2,
    palette: { deep: '#5a2a7a', shore: '#ffb0e0', low: '#c06ad8', mid: '#e08ac0', high: '#ffd0f0', peak: '#fffbe0' },
    atmosphere: { color: '#ff9ae8', intensity: 1.1 }, biome: 'mushroom', species: 'Champignards',
    desc: 'Planète-rêve: champignons géants et spores qui murmurent.',
    props: [{ kind: 'mushroom', density: 1 }, { kind: 'glow_plant', density: .6 }],
    npcs: [{ id: 'dreamer', name: 'Rêvasse', species: 'mushroom', lat: 0, lon: -70, dialogue: 'dreamer' }],
    pickups: scatter('mycelia', 57, [['dust', 10], ['crystal', 3], ['creature', 3]]),
    enemies: [{ type: 'sporeling', count: 8, lat: 40, lon: 60, radius: 30 }],
    structures: [{ kind: 'giant_mushroom', lat: 0, lon: -70 }],
    resources: ['dreamMoss'], secrets: ['mycelia_dream_door'], quests: ['side_dream_walk', 'side_spore_cure'] },

  { id: 'thalassa', name: 'Thalassa', type: 'ocean', system: 'sylva', danger: 1, radius: 140, seed: 61, pos: [900, -100, -3700], relief: .05, seaLevel: .25,
    palette: { deep: '#0a4ab0', shore: '#fff0c0', low: '#3ad8a0', mid: '#2ab080', high: '#7ae0c0', peak: '#ffffff' },
    atmosphere: { color: '#5ac8ff' }, clouds: '#ffffff', biome: 'ocean', species: 'Marins-Étoiles',
    desc: 'Un océan sans fin parsemé d’archipels et de baleines célestes.',
    props: [{ kind: 'palm', density: .8 }, { kind: 'coral', density: .5 }, { kind: 'rock', density: .3 }],
    npcs: [{ id: 'captain_wave', name: 'Capitaine Houle', species: 'tide', lat: 8, lon: 100, dialogue: 'houle' }],
    pickups: scatter('thalassa', 61, [['dust', 10], ['crystal', 4], ['creature', 3], ['relic', 1]]),
    enemies: [{ type: 'crabber', count: 6, lat: -10, lon: -30, radius: 20 }],
    structures: [{ kind: 'lighthouse', lat: 8, lon: 100 }, { kind: 'wreck', lat: -35, lon: 160 }],
    resources: ['tideGlass'], secrets: ['thalassa_whale_song'], quests: ['side_lighthouse', 'side_sky_whale'] },

  // ───── Zone Interdite (Empire Écarlate) ─────
  { id: 'volta', name: 'Voltaria', type: 'storm', system: 'interdit', danger: 3, radius: 125, seed: 71, pos: [3300, 400, -4800], relief: .1, seaLevel: -.1,
    palette: { deep: '#1a2a5a', shore: '#5a6aa0', low: '#3a3f6a', mid: '#5a5a8a', high: '#9a9ad0', peak: '#fff27a' },
    atmosphere: { color: '#8a9aff', intensity: 1.3 }, clouds: '#5a5a80', biome: 'storm', species: 'Fulgurants',
    desc: 'Tempêtes éternelles. L’Empire y puise son énergie.',
    props: [{ kind: 'pylon', density: .7 }, { kind: 'rock', density: .6 }, { kind: 'crystal', density: .4, color: '#fff27a' }],
    npcs: [{ id: 'rebel_spark', name: 'Étincelle', species: 'volt', lat: -25, lon: 40, dialogue: 'spark_rebel' }],
    pickups: scatter('volta', 71, [['dust', 10], ['crystal', 6], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'empireDrone', count: 8, lat: 20, lon: -80, radius: 35 }, { type: 'knight', count: 3, lat: 0, lon: 160, radius: 15 }],
    structures: [{ kind: 'empire_refinery', lat: 0, lon: 160 }, { kind: 'rebel_camp', lat: -25, lon: 40 }],
    resources: ['voltShard'], secrets: ['volta_eye_of_storm'], quests: ['main_3_rebellion', 'side_sabotage'] },

  { id: 'bastion', name: 'Bastion Écarlate', type: 'fortress', system: 'interdit', danger: 3, radius: 150, seed: 79, pos: [4300, 800, -5600], relief: .08, seaLevel: -.35,
    palette: { deep: '#3a0a14', shore: '#5a1a24', low: '#8a2a34', mid: '#4a3a44', high: '#2a2a34', peak: '#ff3b5c' },
    atmosphere: { color: '#ff3b5c', intensity: 1.2 }, biome: 'fortress', species: 'Empire Écarlate',
    desc: 'Capitale-forteresse de l’Empereur Rubis.',
    props: [{ kind: 'pylon', density: .8, color: '#ff3b5c' }, { kind: 'wall', density: .5 }, { kind: 'rock', density: .3 }],
    npcs: [{ id: 'defector', name: 'Chevalier Garance', species: 'empire', lat: -50, lon: -40, model: 'npc_empire_knight', dialogue: 'garance' }],
    pickups: scatter('bastion', 79, [['dust', 8], ['relic', 3], ['artifact', 1]]),
    enemies: [{ type: 'knight', count: 8, lat: 30, lon: 0, radius: 30 }, { type: 'empireDrone', count: 6, lat: -10, lon: 120, radius: 30 }],
    structures: [{ kind: 'citadel', lat: 30, lon: 0 }],
    resources: ['starIron'], secrets: ['bastion_throne_vault'], quests: ['main_3_emperor', 'side_defector'] },

  // ───── Cimetière Stellaire ─────
  { id: 'necro', name: 'Nécropole Stellaire', type: 'graveyard', system: 'cimetiere', danger: 3, radius: 115, seed: 83, pos: [-3900, 700, -5600], relief: .1, seaLevel: -.3,
    palette: { deep: '#14101e', shore: '#3a3048', low: '#5a5068', mid: '#7a7090', high: '#a8a0c0', peak: '#c8ff9a' },
    atmosphere: { color: '#9aff8a', intensity: .8 }, biome: 'graveyard', species: 'Échos',
    desc: 'Épaves de mille flottes et tombeaux d’étoiles éteintes.',
    props: [{ kind: 'wreck', density: .6 }, { kind: 'bones', density: .5 }, { kind: 'ruin', density: .5 }],
    npcs: [{ id: 'echo_keeper', name: 'Gardienne-Écho', species: 'echo', lat: 15, lon: -150, dialogue: 'echo_keeper' }],
    pickups: scatter('necro', 83, [['dust', 10], ['relic', 5], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'ashWraith', count: 8, lat: -30, lon: 30, radius: 35 }, { type: 'brute', count: 2, lat: 50, lon: 90, radius: 10 }],
    structures: [{ kind: 'tomb', lat: 15, lon: -150 }, { kind: 'wreck', lat: -20, lon: 50 }],
    resources: ['echoStone', 'scrapAlloy'], secrets: ['necro_first_ship'], quests: ['main_4_titans', 'side_echo_names'] },

  { id: 'umbra', name: 'Umbra', type: 'void', system: 'cimetiere', danger: 4, radius: 100, seed: 89, pos: [-4700, 1200, -6600], relief: .13, seaLevel: -.2,
    palette: { deep: '#000000', shore: '#1a0a2a', low: '#2a1440', mid: '#3a1a5a', high: '#5a2a8a', peak: '#c08aff' },
    atmosphere: { color: '#7a3cff', intensity: 1.5 }, biome: 'void', species: 'Ombres', hidden: true,
    desc: 'Une planète qui absorbe la lumière. Quelque chose y respire.',
    props: [{ kind: 'void_spike', density: .7 }, { kind: 'crystal', density: .3, color: '#7a3cff' }],
    npcs: [], pickups: scatter('umbra', 89, [['relic', 4], ['artifact', 2], ['creature', 1]]),
    enemies: [{ type: 'voidling', count: 10, lat: 0, lon: 0, radius: 60 }],
    structures: [{ kind: 'portal', lat: -80, lon: 0 }],
    resources: ['voidEssence'], secrets: ['umbra_whisper'], quests: ['main_4_void'] },

  // ───── Cœur Galactique ─────
  { id: 'heart', name: 'Cœur Galactique', type: 'divine', system: 'coeur', danger: 4, radius: 200, seed: 97, pos: [0, 1200, -9000], relief: .08, seaLevel: -.1, ring: '#ffe9a0',
    palette: { deep: '#ffb02e', shore: '#fff3c0', low: '#ffe9a0', mid: '#fff8e0', high: '#ffffff', peak: '#a8c8ff' },
    atmosphere: { color: '#ffe9a0', intensity: 1.6 }, clouds: '#fff8e0', biome: 'divine', species: 'Les Premiers',
    desc: 'Le lieu de naissance des dieux. Ton ascension s’achève ici.',
    props: [{ kind: 'crystal', density: .6, color: '#ffe9a0' }, { kind: 'ruin', density: .6, variant: 'golden' }, { kind: 'flower', density: .4 }],
    npcs: [{ id: 'first_one', name: 'Le Premier', species: 'divine', lat: 89, lon: 0, dialogue: 'first_one' }],
    pickups: scatter('heart', 97, [['relic', 4], ['artifact', 2]]),
    enemies: [{ type: 'seraph', count: 6, lat: 30, lon: 60, radius: 40 }],
    structures: [{ kind: 'temple', lat: 89, lon: 0, variant: 'golden' }],
    resources: ['primeSpark'], secrets: ['heart_creation_choice'], quests: ['main_5_ascension'] },

  // ───── Secrète (non listée sur la carte tant que non découverte) ─────
  { id: 'eden', name: 'Éden Perdu', type: 'secret', system: 'coeur', danger: 0, radius: 60, seed: 101, pos: [-600, 2200, -8400], relief: .07, seaLevel: 0,
    palette: { deep: '#3ae0ff', shore: '#fffbe0', low: '#9aff8a', mid: '#ffd0f0', high: '#fff3a0', peak: '#ffffff' },
    atmosphere: { color: '#ffd0ff', intensity: 1.2 }, clouds: '#ffffff', biome: 'eden', species: '???', hidden: true,
    desc: 'Le premier jardin. Les créatures légendaires y vivent en paix.',
    props: [{ kind: 'tree', density: .8, variant: 'golden' }, { kind: 'flower', density: 1 }],
    npcs: [], pickups: scatter('eden', 101, [['creature', 5], ['artifact', 2]]),
    enemies: [], structures: [{ kind: 'shrine', lat: 0, lon: 0 }],
    resources: ['lumenSap', 'primeSpark'], secrets: ['eden_legend'], quests: ['secret_eden'] },
];
export const getPlanet = (id) => PLANETS.find(p => p.id === id);
export const getSystem = (id) => SYSTEMS.find(s => s.id === id);
export const planetsOf = (systemId) => PLANETS.filter(p => p.system === systemId);
