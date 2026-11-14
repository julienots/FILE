import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { EVOLUTIONS } from '../data/evolutions.js';
import { BALANCE, xpFor } from '../data/balance.js';
import { getPlanet } from '../data/planets.js';
import { bonuses, refreshStats } from './bonuses.js';

// ── Logique pure (testée dans tests/run.mjs) ──────────────────────────────
// Stade atteignable avec `xp` et les flags de quête (verrous narratifs `requires`).
export function stageFor(xp, flags = {}) {
  let s = 0;
  for (let i = 1; i < EVOLUTIONS.length; i++) { const e = EVOLUTIONS[i]; if (xp >= e.xp && (!e.requires || flags[e.requires])) s = i; else break; }
  return s;
}
// Progression vers le prochain stade: {cur, next, ratio 0..1, need}
export function xpProgress(xp, stage) {
  const cur = EVOLUTIONS[stage].xp, nx = EVOLUTIONS[stage + 1];
  if (!nx) return { cur, next: null, ratio: 1, need: 0 };
  return { cur, next: nx.xp, ratio: Math.max(0, Math.min(1, (xp - cur) / (nx.xp - cur))), need: Math.max(0, nx.xp - xp) };
}
// Applique un gain d'XP à `d` (State.data). Renvoie {gained, from, to} (to>from ⇒ évolution).
export function applyXp(d, amount) {
  const h = d.hero, from = h.stage; amount = Math.max(0, Math.round(amount * (1 + bonuses(d).xp))); h.xp += amount;
  const to = Math.max(from, stageFor(h.xp, d.world.flags));
  if (to > from) applyStage(d, to);
  return { gained: amount, from, to };
}
export function applyStage(d, to) {
  const h = d.hero; h.stage = to;
  refreshStats(d); h.hp = h.maxHp; h.energy = h.maxEnergy; // évoluer soigne complètement (bonus de bâtiments inclus)
  for (let i = 0; i <= to; i++) for (const a of EVOLUTIONS[i].abilities) if (!h.abilities.includes(a)) h.abilities.push(a);
}

// ── Câblage bus ────────────────────────────────────────────────────────────
const dangerOf = () => getPlanet(State.data.world.location)?.danger ?? 0;
export function gainXp(amount, reason) {
  const d = State.data, r = applyXp(d, amount);
  bus.emit('progress:xp', { gained: r.gained, reason, xp: d.hero.xp, stage: d.hero.stage, ...xpProgress(d.hero.xp, d.hero.stage) });
  if (r.to > r.from) {
    bus.emit('hero:evolved', { stage: r.to, from: r.from, id: EVOLUTIONS[r.to].id });
    State.save();
  }
  return r;
}
export function initProgression() {
  const d = () => State.data;
  bus.on('hero:xp', ({ amount, reason }) => gainXp(amount, reason));
  bus.on('space:near', ({ planet }) => {
    const w = d().world;
    if (!w.discovered.includes(planet.id)) {
      w.discovered.push(planet.id); w.system = planet.system ?? w.system;
      bus.emit('world:discovered', { id: planet.id });
      bus.emit('ui:toast', { text: `Nouvelle planète: ${planet.name}`, kind: 'discover' });
      gainXp(xpFor(BALANCE.xp.discoverPlanet, planet.danger), 'discover');
    }
  });
  bus.on('scene:entered', ({ id, planet }) => {
    const w = d().world;
    if (id === 'planet' && planet) {
      w.location = planet.id; w.system = planet.system ?? w.system; d().stats.landings++;
      if (!w.discovered.includes(planet.id)) w.discovered.push(planet.id);
      if (!w.visited.includes(planet.id)) { w.visited.push(planet.id); gainXp(xpFor(BALANCE.xp.firstLanding, planet.danger), 'landing'); }
    } else if (id === 'space') w.location = 'space';
  });
  // A émet {id, kind:'cosmicDust', amount} pour les orbes; les autres kinds (crystal, relic, creature, artifact) viennent de planet.pickups.
  bus.on('pickup:collected', ({ id, kind, amount = 1 }) => {
    const s = d(); if (id && s.world.collected.includes(id)) return;
    if (id) s.world.collected.push(id); s.stats.pickups++;
    if (kind === 'cosmicDust' || kind === 'dust') { s.inventory.cosmicDust += BALANCE.dustPerPickup * amount; kind = 'dust'; }
    const base = (BALANCE.xp.pickup[kind] ?? 2) * amount; gainXp(xpFor(base, dangerOf()), 'pickup');
  });
  // enemy:killed: Combat (enregistré avant) complète `level` en mode compat
  bus.on('enemy:killed', ({ level = 1 }) => { d().stats.kills++; gainXp(xpFor(BALANCE.xp.killBase * level, dangerOf()), 'kill'); });
  bus.on('secret:found', ({ id, planet }) => { const f = d().world.flags, k = 'secret_' + id; if (f[k]) return; f[k] = true; bus.emit('world:flag', { flag: k, value: true });
    bus.emit('ui:toast', { text: '✧ Secret découvert ✧', kind: 'discover' }); gainXp(xpFor(100, getPlanet(planet)?.danger ?? dangerOf()), 'discover'); });
  bus.on('quest:done', ({ xp }) => gainXp(xp ?? BALANCE.xp.questBase, 'quest'));
  bus.on('world:flag', () => gainXp(0, 'quest')); // un flag peut débloquer un stade verrouillé
  // la régénération PV/énergie est gérée par systems/Combat.js (tick)
}
