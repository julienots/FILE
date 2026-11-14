import { BUILDINGS } from '../data/buildings.js';
import { EVOLUTIONS } from '../data/evolutions.js';
import { ARTIFACTS } from '../data/collections.js';
// Bonus cumulés des bâtiments de la planète du joueur (pur, partagé par Progression et HomePlanet).
export function bonuses(d) {
  const b = { hp: 0, energy: 0, xp: 0, ship: 0 };
  for (const { id } of d.homePlanet.buildings) { const B = BUILDINGS[id]?.bonus; if (B) for (const k in b) b[k] += B[k] ?? 0; }
  for (const id of d.collection?.artifacts ?? []) { const B = ARTIFACTS[id]?.bonus; if (B) for (const k in b) b[k] += B[k] ?? 0; }
  return b;
}
// recalcule PV/énergie max et niveau du vaisseau (après évolution ou construction)
export function refreshStats(d) {
  const e = EVOLUTIONS[d.hero.stage], b = bonuses(d), h = d.hero;
  const mh = Math.round(e.hp * (1 + b.hp)), me = Math.round(e.energy * (1 + b.energy));
  h.hp = Math.min(mh, h.hp + Math.max(0, mh - h.maxHp)); h.energy = Math.min(me, h.energy + Math.max(0, me - h.maxEnergy));
  h.maxHp = mh; h.maxEnergy = me; d.ship.level = 1 + b.ship;
  return b;
}
