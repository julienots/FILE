// Monde (B écrit, A lit). Schéma: docs/ARCHITECTURE.md §Data.
// pos = unités monde SpaceScene (absolues). Les 4 planètes d'origine gardent leur position.
// props: [{kind∈tree|pine|rock|crystal|mushroom|grass|flower|spire, density (instances/1000 m²), scale?, colors?[]}] — absent ⇒ défauts du biome (A, world/Props.js)
// phys: {g (gravité terrestre), temp (°C moyenne), atmo, day (h)} — affichés par le HUD (scan de surface)
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
    palette: { deep: '#0b2a4a', shore: '#8c8466', low: '#3d5a34', mid: '#2f4a2c', high: '#5a5650', peak: '#d8dce0' },
    atmosphere: { color: '#6a9ad8' }, clouds: '#c8ccd4', phys: { g: 0.82, temp: 14, atmo: 'N₂ 76 % · O₂ 21 % — respirable', day: 26 }, biome: 'meadow', species: 'Pipions',
    desc: 'Un petit monde bleu nuit, loin des projecteurs du Labo. Ton futur repaire.',
    npcs: [{ id: 'bolt', name: 'Boulon', species: 'robot', lat: 12, lon: 8, model: 'npc_robot', dialogue: 'bolt_intro' },
           { id: 'mayor_pip', name: 'Pipo', species: 'pipion', lat: -6, lon: 32, dialogue: 'mayor_intro' }],
    pickups: scatter('home', 11, [['dust', 14], ['crystal', 4], ['creature', 1]]),
    enemies: [{ type: 'slime', count: 4, lat: 30, lon: -40, radius: 15 }],
    structures: [{ kind: 'beacon', lat: 0, lon: 0 }, { kind: 'ship_pad', lat: 5, lon: -5 }],
    resources: ['cosmicDust'], secrets: ['home_hollow_tree'], quests: ['main_1_awakening', 'side_lost_pipion'] },

  { id: 'lab', name: 'Laboratoire Ω', type: 'artificial', system: 'pepin', danger: 1, radius: 70, seed: 3, pos: [-260, 140, -150], relief: .03, seaLevel: -.3,
    palette: { deep: '#1a1c22', shore: '#4a4a4e', low: '#6a6a6e', mid: '#58585c', high: '#7a7a7e', peak: '#9a9a9e' },
    atmosphere: { color: '#8ab4d0', intensity: .3 }, phys: { g: 0.31, temp: -120, atmo: 'aucune — vide', day: 61 }, biome: 'lab', species: 'Gardiens',
    desc: 'La simulation d’où tu t’es échappé. Les machines te cherchent encore.',
    npcs: [{ id: 'dr_quill', name: 'Dr Quill', species: 'scientist', lat: 40, lon: 60, model: 'npc_scientist', dialogue: 'quill_lab' }],
    pickups: scatter('lab', 3, [['dust', 8], ['relic', 2]]),
    enemies: [{ type: 'drone', count: 6, lat: 0, lon: 90, radius: 25 }, { type: 'drone', count: 4, lat: -30, lon: -90, radius: 20 }],
    structures: [{ kind: 'lab_dome', lat: 40, lon: 60 }, { kind: 'portal', lat: -50, lon: 0 }],
    resources: ['scrapAlloy', 'labKeycard'], secrets: ['lab_server_room'], quests: ['main_1_escape', 'side_drone_parts'] },

  // ───── Amas de Braise ─────
  { id: 'ember', name: 'Braise-9', type: 'volcanic', system: 'braise', danger: 1, radius: 130, seed: 23, pos: [760, 120, -1300], relief: .12, seaLevel: -.2,
    palette: { deep: '#c8380e', shore: '#4a2a1e', low: '#2a1c18', mid: '#3a2a24', high: '#4e3a32', peak: '#6a5a54' },
    atmosphere: { color: '#b0623a', intensity: 1.2 }, phys: { g: 1.12, temp: 280, atmo: 'SO₂ · CO₂ — toxique', day: 19 }, biome: 'volcanic', species: 'Braisiens',
    desc: 'Des rivières de lave et un peuple de forgerons au cœur chaud.',
    npcs: [{ id: 'forga', name: 'Forga la Fondeuse', species: 'ember', lat: 18, lon: 20, model: 'npc_ember', dialogue: 'forga_intro' },
           { id: 'tizon', name: 'Tison', species: 'ember', lat: 22, lon: 24, model: 'npc_ember', dialogue: 'tizon' }],
    pickups: scatter('ember', 23, [['dust', 12], ['crystal', 6], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'magmaSlime', count: 6, lat: -20, lon: 70, radius: 25 }, { type: 'scarab', count: 3, lat: 45, lon: -120, radius: 20 }, { type: 'dragon', count: 1, lat: -55, lon: 150, radius: 5 }],
    structures: [{ kind: 'forge_city', lat: 18, lon: 20 }, { kind: 'ruin', lat: -60, lon: 150 }],
    resources: ['emberCore'], secrets: ['ember_magma_heart'], quests: ['main_2_forge', 'side_tizon_lost', 'side_cool_the_core'] },

  { id: 'cinder', name: 'Cendrelune', type: 'ash', system: 'braise', danger: 2, radius: 80, seed: 29, pos: [1150, 420, -1700], relief: .09, seaLevel: -.4,
    palette: { deep: '#141214', shore: '#2e2a28', low: '#44403c', mid: '#55504a', high: '#6a645c', peak: '#8a847c' },
    atmosphere: { color: '#8a7060', intensity: .6 }, phys: { g: 0.24, temp: -40, atmo: 'cendres en suspension — ténue', day: 44 }, biome: 'ash', species: 'Spectres de cendre',
    desc: 'La lune grise de Braise-9, couverte des cendres d’une ancienne guerre.',
    npcs: [{ id: 'ash_hermit', name: 'L’Ermite Gris', species: 'ember', lat: -40, lon: 10, dialogue: 'hermit' }],
    pickups: scatter('cinder', 29, [['dust', 8], ['relic', 3], ['creature', 1]]),
    enemies: [{ type: 'ashWraith', count: 5, lat: 10, lon: 100, radius: 30 }],
    structures: [{ kind: 'ruin', lat: -40, lon: 10 }, { kind: 'wreck', lat: 30, lon: -60 }],
    resources: ['starIron'], secrets: ['cinder_war_memorial'], quests: ['side_hermit_memory'] },

  { id: 'dune', name: 'Ossable', type: 'desert', system: 'braise', danger: 1, radius: 120, seed: 41, pos: [1800, -60, -2100], relief: .07, seaLevel: -.5,
    palette: { deep: '#1f4a5a', shore: '#c8a878', low: '#b88a58', mid: '#a06e40', high: '#7a5232', peak: '#d8c0a0' },
    atmosphere: { color: '#d0a070', intensity: 1 }, phys: { g: 0.95, temp: 52, atmo: 'CO₂ · N₂ — irrespirable', day: 31 }, biome: 'desert', species: 'Nomades Sablons',
    desc: 'Mer de dunes dorées, squelettes géants et caravanes volantes.',
    npcs: [{ id: 'caravan_zaya', name: 'Zaya des Caravanes', species: 'sablon', lat: 5, lon: 45, dialogue: 'zaya' }],
    pickups: scatter('dune', 41, [['dust', 12], ['crystal', 4], ['creature', 2], ['relic', 2]]),
    enemies: [{ type: 'scarab', count: 7, lat: -15, lon: -60, radius: 35 }],
    structures: [{ kind: 'giant_skeleton', lat: -30, lon: 120 }, { kind: 'oasis', lat: 5, lon: 45 }],
    resources: ['sandPearl'], secrets: ['dune_buried_titan'], quests: ['side_caravan_escort', 'side_titan_bones'] },

  // ───── Couronne Glaciale ─────
  { id: 'glacio', name: 'Glacio', type: 'ice', system: 'glacio', danger: 1, radius: 110, seed: 37, pos: [-900, -80, -1700], relief: .09, seaLevel: .05, ring: '#cfeaff',
    palette: { deep: '#12324e', shore: '#9ab0c0', low: '#c8d4dc', mid: '#aebcc8', high: '#e0e8ee', peak: '#f4f8fa' },
    atmosphere: { color: '#8ab8e8' }, clouds: '#d8e0e8', phys: { g: 0.88, temp: -63, atmo: 'N₂ · CH₄ — glaciale', day: 22 }, biome: 'ice', species: 'Givrins',
    desc: 'Banquises chantantes et villages de cristal.',
    props: [{ kind: 'rock', density: .9, colors: ['#3f3a3b', '#5c5656'] }, { kind: 'tree', density: .25, colors: ['#302a2a', '#4c4646', '#5c5656'] }, { kind: 'spire', density: .2, colors: ['#312e31', '#c39273'] }],
    npcs: [{ id: 'elder_frost', name: 'Aïeule Névé', species: 'frost', lat: 50, lon: -20, model: 'npc_frost', dialogue: 'neve_intro' }],
    pickups: scatter('glacio', 37, [['dust', 12], ['crystal', 6], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'frostSlime', count: 5, lat: 0, lon: 80, radius: 25 }, { type: 'iceGolem', count: 1, lat: -40, lon: 160, radius: 5 }],
    structures: [{ kind: 'ice_village', lat: 50, lon: -20 }, { kind: 'frozen_ship', lat: -10, lon: 120 }],
    resources: ['frostCrystal'], secrets: ['glacio_singing_cave'], quests: ['main_3_glacio', 'side_frozen_ship'] },

  { id: 'aurora', name: 'Aurorée', type: 'crystal', system: 'glacio', danger: 2, radius: 95, seed: 43, pos: [-1650, 160, -2500], relief: .11, seaLevel: -.15,
    palette: { deep: '#141c3a', shore: '#5a6a7a', low: '#6a7a84', mid: '#525e6a', high: '#8a96a0', peak: '#c8d8e0' },
    atmosphere: { color: '#5ad8b0', intensity: 1 }, clouds: '#8a9aa8', phys: { g: 0.67, temp: -31, atmo: 'N₂ · Ne ionisé — aurores permanentes', day: 38 }, biome: 'crystal', species: 'Prismes',
    desc: 'Forêts de cristal sous des aurores qui ne s’éteignent jamais.',
    props: [{ kind: 'crystal', density: .6, colors: ['#ad7ee4', '#72e1c1'] }, { kind: 'spire', density: .3, colors: ['#92c4d0', '#e6e6e6'] }, { kind: 'pine', density: .2, colors: ['#5a546b', '#6da79f', '#e6e6e6'] }, { kind: 'grass', density: 4, colors: ['#6da79f', '#88a3cc'] }],
    npcs: [{ id: 'prism_oracle', name: 'L’Oracle Prisme', species: 'prism', lat: 70, lon: 0, dialogue: 'oracle' }],
    pickups: scatter('aurora', 43, [['dust', 10], ['crystal', 8], ['creature', 2]]),
    enemies: [{ type: 'shardBat', count: 6, lat: -20, lon: -100, radius: 30 }, { type: 'dragon', count: 1, lat: 40, lon: 120, radius: 5 }],
    structures: [{ kind: 'temple', lat: 70, lon: 0 }],
    resources: ['lumenSap', 'frostCrystal'], secrets: ['aurora_light_bridge'], quests: ['main_3_oracle', 'side_prism_tuning'] },

  // ───── Bosquet de Sylva ─────
  { id: 'sylva', name: 'Sylva-Mère', type: 'jungle', system: 'sylva', danger: 2, radius: 160, seed: 51, pos: [300, -200, -2600], relief: .1, seaLevel: -.1,
    palette: { deep: '#0e3a40', shore: '#6a6248', low: '#2a4a24', mid: '#1e3a1c', high: '#3a4a2e', peak: '#6a6a5a' },
    atmosphere: { color: '#7aa0c8' }, clouds: '#b8c0c8', phys: { g: 1.21, temp: 29, atmo: 'O₂ 34 % — hyperoxygénée', day: 29 }, biome: 'jungle', species: 'Sylvains',
    desc: 'Une jungle-mère consciente, aux arbres plus vieux que les étoiles.',
    props: [{ kind: 'tree', density: 1, variant: 'jungle' }, { kind: 'mushroom', density: .5 }, { kind: 'flower', density: .7 }, { kind: 'grass', density: 1 }],
    npcs: [{ id: 'sylvana', name: 'Sylvana', species: 'sylva', lat: -10, lon: 0, model: 'npc_sylva', dialogue: 'sylvana_intro' },
           { id: 'root_elder', name: 'Vieille-Racine', species: 'sylva', lat: -60, lon: 90, dialogue: 'root_elder' }],
    pickups: scatter('sylva', 51, [['dust', 14], ['crystal', 4], ['creature', 3], ['artifact', 1]]),
    enemies: [{ type: 'thornBeast', count: 5, lat: 30, lon: 140, radius: 30 }, { type: 'brute', count: 1, lat: 60, lon: -60, radius: 5 }],
    structures: [{ kind: 'tree_city', lat: -10, lon: 0 }, { kind: 'ruin', lat: 20, lon: -150 }],
    resources: ['sporeSilk'], secrets: ['sylva_heart_tree'], quests: ['main_3_sylva', 'side_seed_of_life', 'side_lost_saplings'] },

  { id: 'mycelia', name: 'Mycélia', type: 'mushroom', system: 'sylva', danger: 2, radius: 85, seed: 57, pos: [-120, -420, -3200], relief: .1, seaLevel: -.2,
    palette: { deep: '#2a1a2a', shore: '#5a4a48', low: '#4a3a40', mid: '#3a2e36', high: '#5a4a52', peak: '#8a7a7e' },
    atmosphere: { color: '#a080a8', intensity: .9 }, phys: { g: 0.74, temp: 18, atmo: 'spores · CO₂ — filtre requis', day: 35 }, biome: 'mushroom', species: 'Champignards',
    desc: 'Planète-rêve: champignons géants et spores qui murmurent.',
    props: [{ kind: 'mushroom', density: 1.1, colors: ['#e7c8d8', '#b88da8', '#e7e3c8'] }, { kind: 'mushroom', density: .4, scale: .6, colors: ['#d7cebf', '#9f77ab', '#88ccb8'] }, { kind: 'grass', density: 8, colors: ['#9f77ab', '#d49cc8'] }, { kind: 'flower', density: 3, colors: ['#9f77ab', '#88ccb8', '#e6e6e6'] }],
    npcs: [{ id: 'dreamer', name: 'Rêvasse', species: 'mushroom', lat: 0, lon: -70, dialogue: 'dreamer' }],
    pickups: scatter('mycelia', 57, [['dust', 10], ['crystal', 3], ['creature', 3]]),
    enemies: [{ type: 'sporeling', count: 8, lat: 40, lon: 60, radius: 30 }],
    structures: [{ kind: 'giant_mushroom', lat: 0, lon: -70 }],
    resources: ['dreamMoss'], secrets: ['mycelia_dream_door'], quests: ['side_dream_walk', 'side_spore_cure'] },

  { id: 'thalassa', name: 'Thalassa', type: 'ocean', system: 'sylva', danger: 1, radius: 140, seed: 61, pos: [900, -100, -3700], relief: .05, seaLevel: .25,
    palette: { deep: '#062a5a', shore: '#b8a888', low: '#4a6a3a', mid: '#3a5a30', high: '#5a6a4a', peak: '#a8a898' },
    atmosphere: { color: '#5a9ae0' }, clouds: '#d0d4dc', phys: { g: 1.05, temp: 22, atmo: 'N₂ · O₂ · vapeur — respirable', day: 24 }, biome: 'ocean', species: 'Marins-Étoiles',
    desc: 'Un océan sans fin parsemé d’archipels et de baleines célestes.',
    props: [{ kind: 'tree', density: .4, colors: ['#67554a', '#5a9c85', '#83b4a5'] }, { kind: 'grass', density: 6, colors: ['#5a9c85', '#83b4a5'] }, { kind: 'rock', density: .3, colors: ['#b8af9b', '#dfd4b4'] }, { kind: 'flower', density: 1.5, colors: ['#4a7a69', '#c78d7d', '#e6e6e6'] }],
    npcs: [{ id: 'captain_wave', name: 'Capitaine Houle', species: 'tide', lat: 8, lon: 100, dialogue: 'houle' }],
    pickups: scatter('thalassa', 61, [['dust', 10], ['crystal', 4], ['creature', 3], ['relic', 1]]),
    enemies: [{ type: 'crabber', count: 6, lat: -10, lon: -30, radius: 20 }],
    structures: [{ kind: 'lighthouse', lat: 8, lon: 100 }, { kind: 'wreck', lat: -35, lon: 160 }],
    resources: ['tideGlass'], secrets: ['thalassa_whale_song'], quests: ['side_lighthouse', 'side_sky_whale'] },

  // ───── Zone Interdite (Empire Écarlate) ─────
  { id: 'volta', name: 'Voltaria', type: 'storm', system: 'interdit', danger: 3, radius: 125, seed: 71, pos: [3300, 400, -4800], relief: .1, seaLevel: -.1,
    palette: { deep: '#0e1a2e', shore: '#3a3e48', low: '#2e323c', mid: '#3a3e4a', high: '#50545e', peak: '#7a7e88' },
    atmosphere: { color: '#7080b0', intensity: 1.1 }, clouds: '#4a4e5a', phys: { g: 1.34, temp: -8, atmo: 'Ar · N₂ chargé — orages perpétuels', day: 14 }, biome: 'storm', species: 'Fulgurants',
    desc: 'Tempêtes éternelles. L’Empire y puise son énergie.',
    props: [{ kind: 'rock', density: .9, colors: ['#414352', '#5e5e6f'] }, { kind: 'spire', density: .4, colors: ['#5e5e6f', '#ccc588'] }, { kind: 'crystal', density: .25, colors: ['#e1d672', '#7e8ce4'] }],
    npcs: [{ id: 'rebel_spark', name: 'Étincelle', species: 'volt', lat: -25, lon: 40, dialogue: 'spark_rebel' }],
    pickups: scatter('volta', 71, [['dust', 10], ['crystal', 6], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'empireDrone', count: 8, lat: 20, lon: -80, radius: 35 }, { type: 'knight', count: 3, lat: 0, lon: 160, radius: 15 }],
    structures: [{ kind: 'empire_refinery', lat: 0, lon: 160 }, { kind: 'rebel_camp', lat: -25, lon: 40 }],
    resources: ['voltShard'], secrets: ['volta_eye_of_storm'], quests: ['main_3_rebellion', 'side_sabotage'] },

  { id: 'bastion', name: 'Bastion Écarlate', type: 'fortress', system: 'interdit', danger: 3, radius: 150, seed: 79, pos: [4300, 800, -5600], relief: .08, seaLevel: -.35,
    palette: { deep: '#2a0e0e', shore: '#4a3030', low: '#5a2a24', mid: '#46322e', high: '#3a3236', peak: '#6a5a5a' },
    atmosphere: { color: '#b04a3a', intensity: 1 }, phys: { g: 1.48, temp: 41, atmo: 'CO₂ · poussière ferreuse — irrespirable', day: 20 }, biome: 'fortress', species: 'Empire Écarlate',
    desc: 'Capitale-forteresse de l’Empereur Rubis.',
    props: [{ kind: 'spire', density: .5, colors: ['#28282c', '#bb606f'] }, { kind: 'rock', density: .6, colors: ['#3e393c', '#624043'] }, { kind: 'crystal', density: .15, colors: ['#d7435c', '#ea9aaa'] }],
    npcs: [{ id: 'defector', name: 'Chevalier Garance', species: 'empire', lat: -50, lon: -40, model: 'npc_empire_knight', dialogue: 'garance' }],
    pickups: scatter('bastion', 79, [['dust', 8], ['relic', 3], ['artifact', 1]]),
    enemies: [{ type: 'knight', count: 8, lat: 30, lon: 0, radius: 30 }, { type: 'empireDrone', count: 6, lat: -10, lon: 120, radius: 30 }],
    structures: [{ kind: 'citadel', lat: 30, lon: 0 }],
    resources: ['starIron'], secrets: ['bastion_throne_vault'], quests: ['main_3_emperor', 'side_defector'] },

  // ───── Cimetière Stellaire ─────
  { id: 'necro', name: 'Nécropole Stellaire', type: 'graveyard', system: 'cimetiere', danger: 3, radius: 115, seed: 83, pos: [-3900, 700, -5600], relief: .1, seaLevel: -.3,
    palette: { deep: '#0c0c10', shore: '#2a2a2e', low: '#3a3a3e', mid: '#4a4a4e', high: '#5e5e62', peak: '#7a7a7e' },
    atmosphere: { color: '#6a8a6a', intensity: .5 }, phys: { g: 0.58, temp: -150, atmo: 'traces — quasi vide', day: 90 }, biome: 'graveyard', species: 'Échos',
    desc: 'Épaves de mille flottes et tombeaux d’étoiles éteintes.',
    props: [{ kind: 'rock', density: .8, colors: ['#524e57', '#716d79'] }, { kind: 'spire', density: .3, colors: ['#35323a', '#b6d49c'] }, { kind: 'tree', density: .2, colors: ['#35323a', '#524e57', '#716d79'] }, { kind: 'crystal', density: .1, colors: ['#b4e68a', '#e6e6e6'] }],
    npcs: [{ id: 'echo_keeper', name: 'Gardienne-Écho', species: 'echo', lat: 15, lon: -150, dialogue: 'echo_keeper' }],
    pickups: scatter('necro', 83, [['dust', 10], ['relic', 5], ['creature', 2], ['artifact', 1]]),
    enemies: [{ type: 'ashWraith', count: 8, lat: -30, lon: 30, radius: 35 }, { type: 'brute', count: 2, lat: 50, lon: 90, radius: 10 }],
    structures: [{ kind: 'tomb', lat: 15, lon: -150 }, { kind: 'wreck', lat: -20, lon: 50 }],
    resources: ['echoStone', 'scrapAlloy'], secrets: ['necro_first_ship'], quests: ['main_4_titans', 'side_echo_names'] },

  { id: 'umbra', name: 'Umbra', type: 'void', system: 'cimetiere', danger: 4, radius: 100, seed: 89, pos: [-4700, 1200, -6600], relief: .13, seaLevel: -.2,
    palette: { deep: '#020204', shore: '#0e0c12', low: '#16141c', mid: '#1e1a26', high: '#2a2634', peak: '#3e3a4a' },
    atmosphere: { color: '#5a3a9a', intensity: 1.2 }, phys: { g: 2.1, temp: -210, atmo: 'inconnue — absorbe la lumière', day: 0 }, biome: 'void', species: 'Ombres', hidden: true,
    desc: 'Une planète qui absorbe la lumière. Quelque chose y respire.',
    props: [{ kind: 'spire', density: .7, colors: ['#17121d', '#7d60bb'] }, { kind: 'crystal', density: .4, colors: ['#7344d7', '#ad7ee4'] }, { kind: 'rock', density: .4, colors: ['#261e2e', '#342940'] }],
    npcs: [], pickups: scatter('umbra', 89, [['relic', 4], ['artifact', 2], ['creature', 1]]),
    enemies: [{ type: 'voidling', count: 10, lat: 0, lon: 0, radius: 60 }],
    structures: [{ kind: 'portal', lat: -80, lon: 0 }],
    resources: ['voidEssence'], secrets: ['umbra_whisper'], quests: ['main_4_void'] },

  // ───── Cœur Galactique ─────
  { id: 'heart', name: 'Cœur Galactique', type: 'divine', system: 'coeur', danger: 4, radius: 200, seed: 97, pos: [0, 1200, -9000], relief: .08, seaLevel: -.1, ring: '#ffe9a0',
    palette: { deep: '#3a2a10', shore: '#8a7a5a', low: '#7a6a4a', mid: '#8a7a5e', high: '#a89878', peak: '#e0d4b8' },
    atmosphere: { color: '#e0b060', intensity: 1.4 }, clouds: '#e8dcc0', phys: { g: 1.0, temp: 24, atmo: 'inconnue — étrangement respirable', day: 24 }, biome: 'divine', species: 'Les Premiers',
    desc: 'Le lieu de naissance des dieux. Ton ascension s’achève ici.',
    props: [{ kind: 'crystal', density: .5, colors: ['#e7d38e', '#e6e6e6'] }, { kind: 'spire', density: .2, colors: ['#e7e0c8', '#bfa969'] }, { kind: 'flower', density: 3, colors: ['#d6c99f', '#e6e6e6', '#a4b8d8'] }, { kind: 'grass', density: 6, colors: ['#dfd6b4', '#d6c99f'] }],
    npcs: [{ id: 'first_one', name: 'Le Premier', species: 'divine', lat: 89, lon: 0, dialogue: 'first_one' }],
    pickups: scatter('heart', 97, [['relic', 4], ['artifact', 2]]),
    enemies: [{ type: 'seraph', count: 6, lat: 30, lon: 60, radius: 40 }],
    structures: [{ kind: 'temple', lat: 89, lon: 0, variant: 'golden' }],
    resources: ['primeSpark'], secrets: ['heart_creation_choice'], quests: ['main_5_ascension'] },

  // ───── Secrète (non listée sur la carte tant que non découverte) ─────
  { id: 'eden', name: 'Éden Perdu', type: 'secret', system: 'coeur', danger: 0, radius: 60, seed: 101, pos: [-600, 2200, -8400], relief: .07, seaLevel: 0,
    palette: { deep: '#0e3a4a', shore: '#9a9070', low: '#3a6a34', mid: '#2e5a2a', high: '#5a6a46', peak: '#c8c8b0' },
    atmosphere: { color: '#80b0e0', intensity: 1 }, clouds: '#e0e4ea', phys: { g: 0.96, temp: 21, atmo: 'N₂ · O₂ — air pur', day: 24 }, biome: 'eden', species: '???', hidden: true,
    desc: 'Le premier jardin. Les créatures légendaires y vivent en paix.',
    props: [{ kind: 'tree', density: .7, colors: ['#967e68', '#e3bed7', '#d6cf9f'] }, { kind: 'flower', density: 6 }, { kind: 'grass', density: 14, colors: ['#9ad092', '#e3bed7'] }],
    npcs: [], pickups: scatter('eden', 101, [['creature', 5], ['artifact', 2]]),
    enemies: [], structures: [{ kind: 'shrine', lat: 0, lon: 0 }],
    resources: ['lumenSap', 'primeSpark'], secrets: ['eden_legend'], quests: ['secret_eden'] },
];
export const getPlanet = (id) => PLANETS.find(p => p.id === id);
export const getSystem = (id) => SYSTEMS.find(s => s.id === id);
export const planetsOf = (systemId) => PLANETS.filter(p => p.system === systemId);
