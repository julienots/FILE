import { Config } from './Config.js';
import { bus } from './EventBus.js';
// État de jeu sérialisable unique (propriétaire: B). Toute persistance passe par ici (ARCHITECTURE §State).
// Évolution de schéma: incrémenter VERSION + ajouter MIGRATIONS[ancienneVersion] (data muté en place).
export const VERSION = 2;
export const defaults = () => ({
  v: VERSION,
  hero: { stage: 0, xp: 0, energy: 100, maxEnergy: 100, hp: 100, maxHp: 100, abilities: ['hop'] },
  ship: { level: 1, fuel: 100, upgrades: [] },
  inventory: { cosmicDust: 0, items: {} },          // items: {id: count}
  collection: { creatures: [], artifacts: [] },
  quests: { active: {}, done: [], tracked: null },   // active: {id: {step, n}}
  world: { visited: [], discovered: ['home'], location: 'home', flags: {}, system: 'pepin', collected: [] }, // collected: ids de pickups ramassés (A ne les fait pas réapparaître)
  homePlanet: { buildings: [], biome: 'meadow', level: 1, lastCollect: 0, carry: {} },   // buildings: [{slot,id}]
  stats: { playTime: 0, kills: 0, pickups: 0, deaths: 0, landings: 0 },
  settings: { music: 0.7, sfx: 0.8, quality: 'auto', lang: 'fr', leftHanded: false, haptics: true },
  meta: { created: 0, saved: 0, seenIntro: false },
});
export const MIGRATIONS = {
  1: d => { // v1 → v2: stats, nouveaux réglages, discovered initial, abilities de base
    d.stats ??= defaults().stats; d.meta ??= defaults().meta;
    d.world.discovered = [...new Set(['home', ...(d.world.discovered || [])])];
    if (!d.hero.abilities?.length) d.hero.abilities = ['hop'];
  },
};
// fusion profonde: les clés manquantes reçoivent les valeurs par défaut (tableaux = valeur sauvegardée telle quelle)
function fill(target, def) {
  for (const k in def) {
    const dv = def[k];
    if (!(k in target) || target[k] === null) target[k] = dv;
    else if (dv && typeof dv === 'object' && !Array.isArray(dv) && typeof target[k] === 'object') fill(target[k], dv);
  }
  return target;
}
export function migrate(raw) {
  const d = raw && typeof raw === 'object' ? raw : {};
  let v = d.v ?? 1;
  while (v < VERSION) { MIGRATIONS[v]?.(fill(d, defaults())); v++; }
  d.v = VERSION;
  return fill(d, defaults());
}
const ls = () => { try { return globalThis.localStorage ?? null; } catch { return null; } };
export const State = {
  data: defaults(),
  load() {
    try { const raw = ls()?.getItem(Config.saveKey); this.data = raw ? migrate(JSON.parse(raw)) : defaults(); }
    catch (e) { console.warn('[State] sauvegarde illisible, nouvelle partie', e); this.data = defaults(); }
    if (!this.data.meta.created) this.data.meta.created = Date.now();
    bus.emit('state:loaded', this.data);
  },
  save() { this.data.meta.saved = Date.now(); try { ls()?.setItem(Config.saveKey, JSON.stringify(this.data)); } catch (e) { /* quota */ } },
  reset() { this.data = defaults(); this.data.meta.created = Date.now(); this.save(); bus.emit('state:loaded', this.data); },
  get hasSave() { try { return !!ls()?.getItem(Config.saveKey); } catch { return false; } },
  // Autosave 5 s + sauvegarde à la mise en arrière-plan (Android). Appelé par main.js (pas en test node).
  startAutosave() {
    let t = 0; setInterval(() => { this.data.stats.playTime++; if (++t % 5 === 0) this.save(); }, 1000);
    globalThis.document?.addEventListener('visibilitychange', () => { if (document.hidden) this.save(); });
    globalThis.addEventListener?.('pagehide', () => this.save());
  },
};
