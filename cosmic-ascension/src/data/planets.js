// Monde (B écrit, A lit). Schéma: docs/ARCHITECTURE.md §Data.
// pos = unités monde SpaceScene (absolues). Les 4 planètes d'origine gardent leur position.
// props: [{kind∈tree|pine|rock|crystal|mushroom|grass|flower|spire, density (instances/1000 m²), scale?, colors?[]}] — absent ⇒ défauts du biome (A, world/Props.js)
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
    palette: { deep: '#0e2a6a', shore: '#8a7a9a', low: '#2a7a6a', mid: '#1f5a5a', high: '#4a4a6a', peak: '#cfd8ff' },
    atmosphere: { color: '#5a9aff' }, clouds: '#aab4e0', biome: 'meadow', species: 'Pipions',
    desc: 'Un petit monde bleu nuit, loin des projecteurs du Labo. Ton futur repaire.',
    npcs: [{ id: 'bolt', name: 'Boulon', species: 'robot', lat: 12, lon: 8, model: 'npc_robot', dialogue: 'bolt_intro' },
           { id: 'mayor_pip', name: 'Maire Pipo', species: 'pipion', lat: -6, lon: 32, dialogue: 'mayor_intro' }],
    pickups: scatter('home', 11, [['dust', 14], ['crystal', 4], ['creature', 1]]),
    enemies: [{ type: 'slime', count: 4, lat: 30, lon: -40, radius: 15 }],
    structures: [{ kind: 'beacon', lat: 0, lon: 0 }, { kind: 'ship_pad', lat: 5, lon: -5 }],
    resources: ['cosmicDust'], secrets: ['home_hollow_tree'], quests: ['main_1_awakening', 'side_lost_pipion'] },

  { id: 'lab', name: 'Laboratoire Ω', type: 'artificial', system: 'pepin', danger: 1, radius: 70, seed: 3, pos: [-260, 140, -150], relief: .03, seaLevel: -.3,
    palette: { deep: '#06101e', shore: '#2a3448', low: '#4a5670', mid: '#323c54', high: '#6a7690', peak: '#3ff0ff' },
    atmosphere: { color: '#3ff0ff', intensity: .7 }, biome: 'lab', species: 'Gardiens',
    desc: 'La simulation d’où tu t’es échappé. Les machines te cherchent encore.',
    npcs: [{ id: 'dr_quill', name: 'Dr Quill', species: 'scientist', lat: 40, lon: 60, model: 'npc_scientist', dialogue: 'quill_lab' }],
    pickups: scatter('lab', 3, [['dust', 8], ['relic', 2]]),
    enemies: [{ type: 'drone', count: 6, lat: 0, lon: 90, radius: 25 }, { type: 'drone', count: 4, lat: -30, lon: -90, radius: 20 }],
    structures: [{ kind: 'lab_dome', lat: 40, lon: 60 }, { kind: 'portal', lat: -50, lon: 0 }],
    resources: ['scrapAlloy', 'labKeycard'], secrets: ['lab_server_room'], quests: ['main_1_escape', 'side_drone_parts'] },

  // ───── Amas de Braise ─────
  { id: 'ember', name: 'Braise-9', type: 'volcanic', system: 'braise', danger: 1, radius: 130, seed: 23, pos: [760, 120, -1300], relief: .12, seaLevel: -.2,
    palette: { deep: '#ff3a10', shore: '#c0501c', low: '#2a1418', mid: '#3a1c22', high: '#5a2a2a', peak: '#ff9a4a' },
    atmosphere: { color: '#ff8a4a', intensity: 1.4 }, biome: 'volcanic', species: 'Braisiens',
    desc: 'Des rivières de lave et un peuple de forgerons au cœur chaud.',
    npcs: [{ id: 'forga', name: 'Forga la Fondeuse', species: 'ember', lat: 18, lon: 20, model: 'npc_ember', dialogue: 'forga_intro' },
           { id: 'tizon', name: 'P’tit Tison', species: 'ember', lat: 22, lon: 24, model: 'npc_ember', dialogue: 'tizon' }],
    pickups: scatter('ember', 23, [['dust', 12], ['crystal', 6], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'magmaSlime', count: 6, lat: -20, lon: 70, radius: 25 }, { type: 'scarab', count: 3, lat: 45, lon: -120, radius: 20 }, { type: 'dragon', count: 1, lat: -55, lon: 150, radius: 5 }],
    structures: [{ kind: 'forge_city', lat: 18, lon: 20 }, { kind: 'ruin', lat: -60, lon: 150 }],
    resources: ['emberCore'], secrets: ['ember_magma_heart'], quests: ['main_2_forge', 'side_tizon_lost', 'side_cool_the_core'] },

  { id: 'cinder', name: 'Cendrelune', type: 'ash', system: 'braise', danger: 2, radius: 80, seed: 29, pos: [1150, 420, -1700], relief: .09, seaLevel: -.4,
    palette: { deep: '#120c10', shore: '#2a2024', low: '#3a2e30', mid: '#4a3c3c', high: '#6a5a56', peak: '#ff7a3a' },
    atmosphere: { color: '#a05a40', intensity: .9 }, biome: 'ash', species: 'Spectres de cendre',
    desc: 'La lune grise de Braise-9, couverte des cendres d’une ancienne guerre.',
    npcs: [{ id: 'ash_hermit', name: 'L’Ermite Gris', species: 'ember', lat: -40, lon: 10, dialogue: 'hermit' }],
    pickups: scatter('cinder', 29, [['dust', 8], ['relic', 3], ['creature', 1]]),
    enemies: [{ type: 'ashWraith', count: 5, lat: 10, lon: 100, radius: 30 }],
    structures: [{ kind: 'ruin', lat: -40, lon: 10 }, { kind: 'wreck', lat: 30, lon: -60 }],
    resources: ['starIron'], secrets: ['cinder_war_memorial'], quests: ['side_hermit_memory'] },

  { id: 'dune', name: 'Ossable', type: 'desert', system: 'braise', danger: 1, radius: 120, seed: 41, pos: [1800, -60, -2100], relief: .07, seaLevel: -.5,
    palette: { deep: '#0a3a4a', shore: '#8a6a4a', low: '#7a5232', mid: '#6a3e24', high: '#4a2618', peak: '#ffb05a' },
    atmosphere: { color: '#d07a3a', intensity: 1.1 }, biome: 'desert', species: 'Nomades Sablons',
    desc: 'Mer de dunes dorées, squelettes géants et caravanes volantes.',
    npcs: [{ id: 'caravan_zaya', name: 'Zaya des Caravanes', species: 'sablon', lat: 5, lon: 45, dialogue: 'zaya' }],
    pickups: scatter('dune', 41, [['dust', 12], ['crystal', 4], ['creature', 2], ['relic', 2]]),
    enemies: [{ type: 'scarab', count: 7, lat: -15, lon: -60, radius: 35 }],
    structures: [{ kind: 'giant_skeleton', lat: -30, lon: 120 }, { kind: 'oasis', lat: 5, lon: 45 }],
    resources: ['sandPearl'], secrets: ['dune_buried_titan'], quests: ['side_caravan_escort', 'side_titan_bones'] },

  // ───── Couronne Glaciale ─────
  { id: 'glacio', name: 'Glacio', type: 'ice', system: 'glacio', danger: 1, radius: 110, seed: 37, pos: [-900, -80, -1700], relief: .09, seaLevel: .05, ring: '#cfeaff',
    palette: { deep: '#0e3a6a', shore: '#7a9ac0', low: '#9ab4d8', mid: '#6a8ab4', high: '#c8dcf4', peak: '#ffffff' },
    atmosphere: { color: '#7ab8ff' }, clouds: '#b0c4e0', biome: 'ice', species: 'Givrins',
    desc: 'Banquises chantantes et villages de cristal.',
    props: [{ kind: 'rock', density: .9, colors: ['#4a3c40', '#6b5a5c'] }, { kind: 'tree', density: .25, colors: ['#3a2a2a', '#5a4a48', '#6b5a5c'] }, { kind: 'spire', density: .2, colors: ['#3a3038', '#ff9a5a'] }],
    npcs: [{ id: 'elder_frost', name: 'Aïeule Névé', species: 'frost', lat: 50, lon: -20, model: 'npc_frost', dialogue: 'neve_intro' }],
    pickups: scatter('glacio', 37, [['dust', 12], ['crystal', 6], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'frostSlime', count: 5, lat: 0, lon: 80, radius: 25 }, { type: 'iceGolem', count: 1, lat: -40, lon: 160, radius: 5 }],
    structures: [{ kind: 'ice_village', lat: 50, lon: -20 }, { kind: 'frozen_ship', lat: -10, lon: 120 }],
    resources: ['frostCrystal'], secrets: ['glacio_singing_cave'], quests: ['main_3_glacio', 'side_frozen_ship'] },

  { id: 'aurora', name: 'Aurorée', type: 'crystal', system: 'glacio', danger: 2, radius: 95, seed: 43, pos: [-1650, 160, -2500], relief: .11, seaLevel: -.15,
    palette: { deep: '#120a3a', shore: '#2a5a7a', low: '#1a4a5a', mid: '#2a3a6a', high: '#5a2a8a', peak: '#7affd8' },
    atmosphere: { color: '#3affc0', intensity: 1.2 }, clouds: '#5a8aa0', biome: 'crystal', species: 'Prismes',
    desc: 'Forêts de cristal sous des aurores qui ne s’éteignent jamais.',
    props: [{ kind: 'crystal', density: .6, colors: ['#c08aff', '#7affd8'] }, { kind: 'spire', density: .3, colors: ['#8ae8ff', '#ffffff'] }, { kind: 'pine', density: .2, colors: ['#5a4a8a', '#5ad8c8', '#ffffff'] }, { kind: 'grass', density: 4, colors: ['#5ad8c8', '#7ab0ff'] }],
    npcs: [{ id: 'prism_oracle', name: 'L’Oracle Prisme', species: 'prism', lat: 70, lon: 0, dialogue: 'oracle' }],
    pickups: scatter('aurora', 43, [['dust', 10], ['crystal', 8], ['creature', 2]]),
    enemies: [{ type: 'shardBat', count: 6, lat: -20, lon: -100, radius: 30 }, { type: 'dragon', count: 1, lat: 40, lon: 120, radius: 5 }],
    structures: [{ kind: 'temple', lat: 70, lon: 0 }],
    resources: ['lumenSap', 'frostCrystal'], secrets: ['aurora_light_bridge'], quests: ['main_3_oracle', 'side_prism_tuning'] },

  // ───── Bosquet de Sylva ─────
  { id: 'sylva', name: 'Sylva-Mère', type: 'jungle', system: 'sylva', danger: 2, radius: 160, seed: 51, pos: [300, -200, -2600], relief: .1, seaLevel: -.1,
    palette: { deep: '#0a3a4a', shore: '#5a4a6a', low: '#1a5a4a', mid: '#143a3a', high: '#5a2a8a', peak: '#d07bff' },
    atmosphere: { color: '#8a5aff' }, clouds: '#b094e0', biome: 'jungle', species: 'Sylvains',
    desc: 'Une jungle-mère consciente, aux arbres plus vieux que les étoiles.',
    props: [{ kind: 'tree', density: 1, variant: 'jungle' }, { kind: 'mushroom', density: .5 }, { kind: 'flower', density: .7 }, { kind: 'grass', density: 1 }],
    npcs: [{ id: 'sylvana', name: 'Sylvana', species: 'sylva', lat: -10, lon: 0, model: 'npc_sylva', dialogue: 'sylvana_intro' },
           { id: 'root_elder', name: 'Vieille-Racine', species: 'sylva', lat: -60, lon: 90, dialogue: 'root_elder' }],
    pickups: scatter('sylva', 51, [['dust', 14], ['crystal', 4], ['creature', 3], ['artifact', 1]]),
    enemies: [{ type: 'thornBeast', count: 5, lat: 30, lon: 140, radius: 30 }, { type: 'brute', count: 1, lat: 60, lon: -60, radius: 5 }],
    structures: [{ kind: 'tree_city', lat: -10, lon: 0 }, { kind: 'ruin', lat: 20, lon: -150 }],
    resources: ['sporeSilk'], secrets: ['sylva_heart_tree'], quests: ['main_3_sylva', 'side_seed_of_life', 'side_lost_saplings'] },

  { id: 'mycelia', name: 'Mycélia', type: 'mushroom', system: 'sylva', danger: 2, radius: 85, seed: 57, pos: [-120, -420, -3200], relief: .1, seaLevel: -.2,
    palette: { deep: '#1a0a2a', shore: '#4a2a4a', low: '#3a1a4a', mid: '#5a2a5a', high: '#7a3a6a', peak: '#ff6ae0' },
    atmosphere: { color: '#ff4ad8', intensity: 1.1 }, biome: 'mushroom', species: 'Champignards',
    desc: 'Planète-rêve: champignons géants et spores qui murmurent.',
    props: [{ kind: 'mushroom', density: 1.1, colors: ['#ffe0f0', '#e08ac0', '#fffbe0'] }, { kind: 'mushroom', density: .4, scale: .6, colors: ['#f3e6d0', '#c06ad8', '#7affd8'] }, { kind: 'grass', density: 8, colors: ['#c06ad8', '#ff9ae8'] }, { kind: 'flower', density: 3, colors: ['#c06ad8', '#7affd8', '#ffffff'] }],
    npcs: [{ id: 'dreamer', name: 'Rêvasse', species: 'mushroom', lat: 0, lon: -70, dialogue: 'dreamer' }],
    pickups: scatter('mycelia', 57, [['dust', 10], ['crystal', 3], ['creature', 3]]),
    enemies: [{ type: 'sporeling', count: 8, lat: 40, lon: 60, radius: 30 }],
    structures: [{ kind: 'giant_mushroom', lat: 0, lon: -70 }],
    resources: ['dreamMoss'], secrets: ['mycelia_dream_door'], quests: ['side_dream_walk', 'side_spore_cure'] },

  { id: 'thalassa', name: 'Thalassa', type: 'ocean', system: 'sylva', danger: 1, radius: 140, seed: 61, pos: [900, -100, -3700], relief: .05, seaLevel: .25,
    palette: { deep: '#041a4a', shore: '#4a5a6a', low: '#0a5a5a', mid: '#0a4a4a', high: '#2a6a6a', peak: '#9afff0' },
    atmosphere: { color: '#2a9aff' }, clouds: '#8aa8c8', biome: 'ocean', species: 'Marins-Étoiles',
    desc: 'Un océan sans fin parsemé d’archipels et de baleines célestes.',
    props: [{ kind: 'tree', density: .4, colors: ['#8a5a3a', '#3ad8a0', '#7ae0c0'] }, { kind: 'grass', density: 6, colors: ['#3ad8a0', '#7ae0c0'] }, { kind: 'rock', density: .3, colors: ['#d8c8a0', '#fff0c0'] }, { kind: 'flower', density: 1.5, colors: ['#2ab080', '#ff8a6a', '#ffffff'] }],
    npcs: [{ id: 'captain_wave', name: 'Capitaine Houle', species: 'tide', lat: 8, lon: 100, dialogue: 'houle' }],
    pickups: scatter('thalassa', 61, [['dust', 10], ['crystal', 4], ['creature', 3], ['relic', 1]]),
    enemies: [{ type: 'crabber', count: 6, lat: -10, lon: -30, radius: 20 }],
    structures: [{ kind: 'lighthouse', lat: 8, lon: 100 }, { kind: 'wreck', lat: -35, lon: 160 }],
    resources: ['tideGlass'], secrets: ['thalassa_whale_song'], quests: ['side_lighthouse', 'side_sky_whale'] },

  // ───── Zone Interdite (Empire Écarlate) ─────
  { id: 'volta', name: 'Voltaria', type: 'storm', system: 'interdit', danger: 3, radius: 125, seed: 71, pos: [3300, 400, -4800], relief: .1, seaLevel: -.1,
    palette: { deep: '#080e2a', shore: '#22284a', low: '#1a1e3a', mid: '#2a2e4a', high: '#4a4a6a', peak: '#fff23a' },
    atmosphere: { color: '#6a7aff', intensity: 1.3 }, clouds: '#3a3a5a', biome: 'storm', species: 'Fulgurants',
    desc: 'Tempêtes éternelles. L’Empire y puise son énergie.',
    props: [{ kind: 'rock', density: .9, colors: ['#3a3f6a', '#5a5a8a'] }, { kind: 'spire', density: .4, colors: ['#5a5a8a', '#fff27a'] }, { kind: 'crystal', density: .25, colors: ['#fff27a', '#8a9aff'] }],
    npcs: [{ id: 'rebel_spark', name: 'Étincelle', species: 'volt', lat: -25, lon: 40, dialogue: 'spark_rebel' }],
    pickups: scatter('volta', 71, [['dust', 10], ['crystal', 6], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'empireDrone', count: 8, lat: 20, lon: -80, radius: 35 }, { type: 'knight', count: 3, lat: 0, lon: 160, radius: 15 }],
    structures: [{ kind: 'empire_refinery', lat: 0, lon: 160 }, { kind: 'rebel_camp', lat: -25, lon: 40 }],
    resources: ['voltShard'], secrets: ['volta_eye_of_storm'], quests: ['main_3_rebellion', 'side_sabotage'] },

  { id: 'bastion', name: 'Bastion Écarlate', type: 'fortress', system: 'interdit', danger: 3, radius: 150, seed: 79, pos: [4300, 800, -5600], relief: .08, seaLevel: -.35,
    palette: { deep: '#1a0208', shore: '#2a0a10', low: '#4a0a18', mid: '#2a1a24', high: '#1a1a22', peak: '#ff2a4a' },
    atmosphere: { color: '#ff2a4a', intensity: 1.2 }, biome: 'fortress', species: 'Empire Écarlate',
    desc: 'Capitale-forteresse de l’Empereur Rubis.',
    props: [{ kind: 'spire', density: .5, colors: ['#2a2a34', '#ff3b5c'] }, { kind: 'rock', density: .6, colors: ['#4a3a44', '#8a2a34'] }, { kind: 'crystal', density: .15, colors: ['#ff3b5c', '#ffb0c0'] }],
    npcs: [{ id: 'defector', name: 'Chevalier Garance', species: 'empire', lat: -50, lon: -40, model: 'npc_empire_knight', dialogue: 'garance' }],
    pickups: scatter('bastion', 79, [['dust', 8], ['relic', 3], ['artifact', 1]]),
    enemies: [{ type: 'knight', count: 8, lat: 30, lon: 0, radius: 30 }, { type: 'empireDrone', count: 6, lat: -10, lon: 120, radius: 30 }],
    structures: [{ kind: 'citadel', lat: 30, lon: 0 }],
    resources: ['starIron'], secrets: ['bastion_throne_vault'], quests: ['main_3_emperor', 'side_defector'] },

  // ───── Cimetière Stellaire ─────
  { id: 'necro', name: 'Nécropole Stellaire', type: 'graveyard', system: 'cimetiere', danger: 3, radius: 115, seed: 83, pos: [-3900, 700, -5600], relief: .1, seaLevel: -.3,
    palette: { deep: '#06040c', shore: '#1a1624', low: '#2a2436', mid: '#3a3248', high: '#5a5070', peak: '#9aff6a' },
    atmosphere: { color: '#6aff5a', intensity: .8 }, biome: 'graveyard', species: 'Échos',
    desc: 'Épaves de mille flottes et tombeaux d’étoiles éteintes.',
    props: [{ kind: 'rock', density: .8, colors: ['#5a5068', '#7a7090'] }, { kind: 'spire', density: .3, colors: ['#3a3048', '#c8ff9a'] }, { kind: 'tree', density: .2, colors: ['#3a3048', '#5a5068', '#7a7090'] }, { kind: 'crystal', density: .1, colors: ['#c8ff9a', '#ffffff'] }],
    npcs: [{ id: 'echo_keeper', name: 'Gardienne-Écho', species: 'echo', lat: 15, lon: -150, dialogue: 'echo_keeper' }],
    pickups: scatter('necro', 83, [['dust', 10], ['relic', 5], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'ashWraith', count: 8, lat: -30, lon: 30, radius: 35 }, { type: 'brute', count: 2, lat: 50, lon: 90, radius: 10 }],
    structures: [{ kind: 'tomb', lat: 15, lon: -150 }, { kind: 'wreck', lat: -20, lon: 50 }],
    resources: ['echoStone', 'scrapAlloy'], secrets: ['necro_first_ship'], quests: ['main_4_titans', 'side_echo_names'] },

  { id: 'umbra', name: 'Umbra', type: 'void', system: 'cimetiere', danger: 4, radius: 100, seed: 89, pos: [-4700, 1200, -6600], relief: .13, seaLevel: -.2,
    palette: { deep: '#000000', shore: '#0a0414', low: '#140a24', mid: '#1e0e34', high: '#2e1450', peak: '#b06aff' },
    atmosphere: { color: '#7a2aff', intensity: 1.5 }, biome: 'void', species: 'Ombres', hidden: true,
    desc: 'Une planète qui absorbe la lumière. Quelque chose y respire.',
    props: [{ kind: 'spire', density: .7, colors: ['#1a0a2a', '#7a3cff'] }, { kind: 'crystal', density: .4, colors: ['#7a3cff', '#c08aff'] }, { kind: 'rock', density: .4, colors: ['#2a1440', '#3a1a5a'] }],
    npcs: [], pickups: scatter('umbra', 89, [['relic', 4], ['artifact', 2], ['creature', 1]]),
    enemies: [{ type: 'voidling', count: 10, lat: 0, lon: 0, radius: 60 }],
    structures: [{ kind: 'portal', lat: -80, lon: 0 }],
    resources: ['voidEssence'], secrets: ['umbra_whisper'], quests: ['main_4_void'] },

  // ───── Cœur Galactique ─────
  { id: 'heart', name: 'Cœur Galactique', type: 'divine', system: 'coeur', danger: 4, radius: 200, seed: 97, pos: [0, 1200, -9000], relief: .08, seaLevel: -.1, ring: '#ffe9a0',
    palette: { deep: '#3a1a04', shore: '#5a4a3a', low: '#4a3a2a', mid: '#6a5a4a', high: '#8a7a6a', peak: '#ffd04a' },
    atmosphere: { color: '#ffc04a', intensity: 1.6 }, clouds: '#8a7a5a', biome: 'divine', species: 'Les Premiers',
    desc: 'Le lieu de naissance des dieux. Ton ascension s’achève ici.',
    props: [{ kind: 'crystal', density: .5, colors: ['#ffe9a0', '#ffffff'] }, { kind: 'spire', density: .2, colors: ['#fff8e0', '#ffd04a'] }, { kind: 'flower', density: 3, colors: ['#ffe9a0', '#ffffff', '#a8c8ff'] }, { kind: 'grass', density: 6, colors: ['#fff3c0', '#ffe9a0'] }],
    npcs: [{ id: 'first_one', name: 'Le Premier', species: 'divine', lat: 89, lon: 0, dialogue: 'first_one' }],
    pickups: scatter('heart', 97, [['relic', 4], ['artifact', 2]]),
    enemies: [{ type: 'seraph', count: 6, lat: 30, lon: 60, radius: 40 }],
    structures: [{ kind: 'temple', lat: 89, lon: 0, variant: 'golden' }],
    resources: ['primeSpark'], secrets: ['heart_creation_choice'], quests: ['main_5_ascension'] },

  // ───── Secrète (non listée sur la carte tant que non découverte) ─────
  { id: 'eden', name: 'Éden Perdu', type: 'secret', system: 'coeur', danger: 0, radius: 60, seed: 101, pos: [-600, 2200, -8400], relief: .07, seaLevel: 0,
    palette: { deep: '#062a3a', shore: '#4a5a4a', low: '#1a5a3a', mid: '#2a4a4a', high: '#4a3a5a', peak: '#ffe07a' },
    atmosphere: { color: '#ff9ae0', intensity: 1.2 }, clouds: '#a090b0', biome: 'eden', species: '???', hidden: true,
    desc: 'Le premier jardin. Les créatures légendaires y vivent en paix.',
    props: [{ kind: 'tree', density: .7, colors: ['#c08a5a', '#ffd0f0', '#fff3a0'] }, { kind: 'flower', density: 6 }, { kind: 'grass', density: 14, colors: ['#9aff8a', '#ffd0f0'] }],
    npcs: [], pickups: scatter('eden', 101, [['creature', 5], ['artifact', 2]]),
    enemies: [], structures: [{ kind: 'shrine', lat: 0, lon: 0 }],
    resources: ['lumenSap', 'primeSpark'], secrets: ['eden_legend'], quests: ['secret_eden'] },
];
export const getPlanet = (id) => PLANETS.find(p => p.id === id);
export const getSystem = (id) => SYSTEMS.find(s => s.id === id);
export const planetsOf = (systemId) => PLANETS.filter(p => p.system === systemId);
