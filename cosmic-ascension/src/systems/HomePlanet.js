import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { BUILDINGS, BASE_SLOTS, planetLevel, MAX_OFFLINE_H } from '../data/buildings.js';
import { ITEMS } from '../data/items.js';
import { PLANETS } from '../data/planets.js';
import { addItem } from './Combat.js';
import { itemCount } from './Quests.js';
import { refreshStats } from './bonuses.js';
import { gainXp } from './Progression.js';

// Planète du joueur (B6): validation des constructions, arbre techno (req), production passive (même hors ligne), bonus, niveau.
// A (HomeBase) → `build:near {slot}`, `build:request {planetId,slot}`. B → `ui:buildMenu {planetId,slot}` → UI → `build:choose {planetId,slot,id}` → `build:placed`.
export const Home = {
  get hp() { return State.data.homePlanet; },
  built(id) { return this.hp.buildings.some(b => b.id === id); },
  check(id) {
    const B = BUILDINGS[id], d = State.data; if (!B) return { ok: false, reason: 'inconnu' };
    if (this.built(id)) return { ok: false, reason: 'déjà construit' };
    if (this.hp.buildings.length >= BASE_SLOTS) return { ok: false, reason: 'plus d’emplacement' };
    const r = B.req || {};
    if (r.level && this.hp.level < r.level) return { ok: false, reason: `planète niv. ${r.level}`, locked: true };
    if (r.stage != null && d.hero.stage < r.stage) return { ok: false, reason: `forme ${r.stage + 1}`, locked: true };
    if (r.flag && !d.world.flags[r.flag]) return { ok: false, reason: 'histoire', locked: true };
    for (const [it, n] of Object.entries(B.cost)) if (itemCount(it) < n) return { ok: false, reason: `${n - itemCount(it)} ${ITEMS[it]?.icon ?? it} manquant`, cost: true };
    return { ok: true };
  },
  build(id, slot, planetId = 'home') {
    const c = this.check(id); if (!c.ok) { bus.emit('ui:toast', { text: c.reason, kind: 'warn' }); return false; }
    if (this.hp.buildings.some(b => b.slot === slot)) return false;
    const B = BUILDINGS[id];
    for (const [it, n] of Object.entries(B.cost)) addItem(it, -n);
    this.collect(); // la production repart du moment de la construction
    this.hp.buildings.push({ slot, id }); const lv = planetLevel(this.hp.buildings.length), up = lv > this.hp.level; this.hp.level = lv;
    const d = State.data, shipBefore = d.ship.level; refreshStats(d);
    if (d.ship.level !== shipBefore) bus.emit('ship:upgraded', { level: d.ship.level, upgrades: d.ship.upgrades });
    if (B.bonus?.reveal) for (const p of PLANETS) if (!p.hidden && p.system === d.world.system && !d.world.discovered.includes(p.id)) { d.world.discovered.push(p.id); bus.emit('world:discovered', { id: p.id }); }
    bus.emit('build:placed', { planetId, slot, buildingId: id });
    bus.emit('ui:toast', { text: `${B.icon} ${B.name} construit !`, kind: 'questDone' });
    if (up) bus.emit('ui:toast', { text: `Base : niveau ${lv}`, kind: 'discover' });
    gainXp(40 * (B.tier ?? 1), 'quest'); bus.emit('hero:stats', d.hero); bus.emit('home:changed', this.hp); State.save();
    return true;
  },
  rates() { const r = {}; for (const { id } of this.hp.buildings) for (const [it, n] of Object.entries(BUILDINGS[id]?.prod ?? {})) r[it] = (r[it] ?? 0) + n; return r; },
  // production accumulée depuis la dernière récolte (heures réelles, plafonnée) → inventaire
  collect(now = Date.now()) {
    const hp = this.hp; if (!hp.lastCollect) { hp.lastCollect = now; return []; }
    const hrs = Math.min(MAX_OFFLINE_H, Math.max(0, (now - hp.lastCollect) / 3.6e6)); hp.lastCollect = now;
    hp.carry ??= {}; const got = [];
    for (const [it, rate] of Object.entries(this.rates())) { const v = rate * hrs + (hp.carry[it] ?? 0), n = Math.floor(v); hp.carry[it] = v - n; if (n > 0) { addItem(it, n); got.push([it, n]); } }
    if (got.length) bus.emit('ui:toast', { text: 'Récolte : ' + got.map(([it, n]) => `+${n} ${ITEMS[it]?.icon ?? it}`).join('  '), kind: 'loot' });
    return got;
  },
};
export function initHome() {
  bus.on('build:request', ({ planetId, slot }) => bus.emit('ui:buildMenu', { planetId, slot }));
  bus.on('build:choose', ({ planetId, slot, id }) => Home.build(id, slot, planetId));
  bus.on('build:near', ({ slot } = {}) => bus.emit('ui:context', { label: slot != null ? 'Construire' : null, source: 'build' }));
  bus.on('planet:enter', ({ id }) => { if (PLANETS.find(p => p.id === id)?.type === 'home') setTimeout(() => Home.collect(), 2500); });
  bus.on('state:loaded', () => refreshStats(State.data));
  refreshStats(State.data);
}
