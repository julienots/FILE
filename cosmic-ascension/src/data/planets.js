// Définitions des planètes. OPUS: étends ce fichier (objectif ≥ 14 planètes, voir docs/TEAM.md).
// Positions en unités monde de la SpaceScene (1 unité ≈ 1 m de vaisseau; planètes ~ 80-400 de rayon).
export const PLANETS = [
  { id: 'home', name: 'Pépin', type: 'home', radius: 90, seed: 11, pos: [0, 0, -420], relief: .06, seaLevel: -.05,
    palette: { deep: '#2a6fe0', shore: '#f3e3a1', low: '#6fd36a', mid: '#3fb55a', high: '#8a9a5b', peak: '#ffffff' },
    atmosphere: { color: '#7fd0ff' }, clouds: '#ffffff', biome: 'meadow',
    npcs: [], quests: [], resources: ['cosmicDust'], secrets: [] },
  { id: 'ember', name: 'Braise-9', type: 'volcanic', radius: 130, seed: 23, pos: [760, 120, -1300], relief: .12, seaLevel: -.2,
    palette: { deep: '#ff4a1c', shore: '#ffb347', low: '#5a2d3a', mid: '#7b3f4a', high: '#c76a4a', peak: '#ffe08a' },
    atmosphere: { color: '#ff8a4a', intensity: 1.4 }, biome: 'volcanic', npcs: [], quests: [], resources: ['emberCore'], secrets: [] },
  { id: 'glacio', name: 'Glacio', type: 'ice', radius: 110, seed: 37, pos: [-900, -80, -1700], relief: .09, seaLevel: .05, ring: '#cfeaff',
    palette: { deep: '#3aa0e8', shore: '#d9f4ff', low: '#e8fbff', mid: '#cfefff', high: '#ffffff', peak: '#ffffff' },
    atmosphere: { color: '#b8e8ff' }, clouds: '#f4fbff', biome: 'ice', npcs: [], quests: [], resources: ['frostCrystal'], secrets: [] },
  { id: 'sylva', name: 'Sylva-Mère', type: 'jungle', radius: 160, seed: 51, pos: [300, -200, -2600], relief: .1, seaLevel: -.1,
    palette: { deep: '#1ea0a0', shore: '#e7d7a0', low: '#2fbf4a', mid: '#1f9a4a', high: '#c47bff', peak: '#ffd1ff' },
    atmosphere: { color: '#a07bff' }, clouds: '#ffe4ff', biome: 'jungle', npcs: [], quests: [], resources: ['sporeSilk'], secrets: [] },
];
export const getPlanet = (id) => PLANETS.find(p => p.id === id);
