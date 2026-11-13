// Définitions des planètes. OPUS: étends ce fichier (objectif ≥ 14 planètes, voir docs/TEAM.md).
// Positions en unités monde de la SpaceScene (1 unité ≈ 1 m de vaisseau; planètes ~ 80-400 de rayon).
export const PLANETS = [
  { id: 'home', name: 'Pépin', type: 'home', radius: 90, seed: 11, pos: [0, 0, -420], relief: .06, seaLevel: -.05,
    palette: { deep: '#0e2a6a', shore: '#8a7a9a', low: '#2a7a6a', mid: '#1f5a5a', high: '#4a4a6a', peak: '#cfd8ff' },
    atmosphere: { color: '#5a9aff' }, clouds: '#aab4e0', biome: 'meadow',
    npcs: [], quests: [], resources: ['cosmicDust'], secrets: [] },
  { id: 'ember', name: 'Braise-9', type: 'volcanic', radius: 130, seed: 23, pos: [760, 120, -1300], relief: .12, seaLevel: -.2,
    palette: { deep: '#ff3a10', shore: '#c0501c', low: '#2a1418', mid: '#3a1c22', high: '#5a2a2a', peak: '#ff9a4a' },
    atmosphere: { color: '#ff8a4a', intensity: 1.4 }, biome: 'volcanic', npcs: [], quests: [], resources: ['emberCore'], secrets: [] },
  { id: 'glacio', name: 'Glacio', type: 'ice', radius: 110, seed: 37, pos: [-900, -80, -1700], relief: .09, seaLevel: .05, ring: '#cfeaff',
    palette: { deep: '#0e3a6a', shore: '#7a9ac0', low: '#9ab4d8', mid: '#6a8ab4', high: '#c8dcf4', peak: '#ffffff' },
    atmosphere: { color: '#7ab8ff' }, clouds: '#b0c4e0', biome: 'ice', npcs: [], quests: [], resources: ['frostCrystal'], secrets: [] },
  { id: 'sylva', name: 'Sylva-Mère', type: 'jungle', radius: 160, seed: 51, pos: [300, -200, -2600], relief: .1, seaLevel: -.1,
    palette: { deep: '#0a3a4a', shore: '#5a4a6a', low: '#1a5a4a', mid: '#143a3a', high: '#5a2a8a', peak: '#d07bff' },
    atmosphere: { color: '#8a5aff' }, clouds: '#b094e0', biome: 'jungle', npcs: [], quests: [], resources: ['sporeSilk'], secrets: [] },
];
export const getPlanet = (id) => PLANETS.find(p => p.id === id);
