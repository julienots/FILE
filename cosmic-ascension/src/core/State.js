import { Config } from './Config.js';
import { bus } from './EventBus.js';
// État de jeu sérialisable unique. Toute persistance passe par ici (docs/CONTRACTS.md §State).
const defaults = () => ({
  v: 1,
  hero: { stage: 0, xp: 0, energy: 100, maxEnergy: 100, hp: 100, maxHp: 100, abilities: [] },
  ship: { level: 1, fuel: 100, upgrades: [] },
  inventory: { cosmicDust: 0, items: {} },          // items: {id: count}
  collection: { creatures: [], artifacts: [] },
  quests: { active: {}, done: [] },                  // active: {id: {step, progress}}
  world: { visited: ['home'], discovered: [], location: 'home', flags: {} },
  homePlanet: { buildings: [], biome: 'meadow', level: 1 },
  settings: { music: 0.7, sfx: 0.8, quality: 'auto', lang: 'fr' },
});
export const State = {
  data: defaults(),
  load() {
    try { const raw = localStorage.getItem(Config.saveKey); if (raw) this.data = { ...defaults(), ...JSON.parse(raw) }; } catch (e) { /* ignore */ }
    bus.emit('state:loaded', this.data);
  },
  save() { try { localStorage.setItem(Config.saveKey, JSON.stringify(this.data)); } catch (e) { /* ignore */ } },
  reset() { this.data = defaults(); this.save(); bus.emit('state:loaded', this.data); },
};
let t = 0; setInterval(() => { if (++t % 5 === 0) State.save(); }, 1000);
