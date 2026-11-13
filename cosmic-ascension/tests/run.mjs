// Tests unitaires de la logique (B) — sans navigateur: `npm test`.
import { migrate, defaults, VERSION } from '../src/core/State.js';
import { stageFor, xpProgress, applyXp, gainXp, initProgression } from '../src/systems/Progression.js';
import { EVOLUTIONS } from '../src/data/evolutions.js';
import { ABILITIES } from '../src/data/abilities.js';
import { PLANETS, SYSTEMS } from '../src/data/planets.js';
import { ITEMS } from '../src/data/items.js';
import { State } from '../src/core/State.js';
import { bus } from '../src/core/EventBus.js';

let pass = 0, fail = 0;
const test = (name, fn) => { try { fn(); pass++; } catch (e) { fail++; console.error('✗', name, '\n   ', e.message); } };
const eq = (a, b, m = '') => { if (JSON.stringify(a) !== JSON.stringify(b)) throw new Error(`${m} attendu ${JSON.stringify(b)}, obtenu ${JSON.stringify(a)}`); };
const ok = (c, m) => { if (!c) throw new Error(m); };

// ── State ──
test('migrate: v1 → courant, garde les données, ajoute les champs', () => {
  const v1 = { v: 1, hero: { stage: 2, xp: 400, energy: 50, maxEnergy: 100, hp: 80, maxHp: 100, abilities: [] }, world: { visited: ['home', 'ember'], discovered: [], location: 'ember', flags: {} }, settings: { music: .2 } };
  const d = migrate(v1);
  eq(d.v, VERSION); eq(d.hero.xp, 400); eq(d.settings.music, .2); eq(d.settings.haptics, true);
  ok(d.world.discovered.includes('home'), 'home découvert'); eq(d.hero.abilities, ['hop']); ok(Array.isArray(d.world.collected), 'collected');
  ok(d.stats && d.stats.kills === 0, 'stats');
});
test('migrate: entrée corrompue → défauts', () => { eq(migrate(null).hero, defaults().hero); eq(migrate('x').v, VERSION); });
test('migrate: idempotent sur version courante', () => { const d = defaults(); d.hero.xp = 77; eq(migrate(JSON.parse(JSON.stringify(d))), d); });

// ── Données ──
test('évolutions: 10 stades, XP et échelle croissantes, capacités définies', () => {
  eq(EVOLUTIONS.length, 10);
  for (let i = 1; i < EVOLUTIONS.length; i++) { ok(EVOLUTIONS[i].xp > EVOLUTIONS[i - 1].xp, 'xp ' + i); ok(EVOLUTIONS[i].scale > EVOLUTIONS[i - 1].scale, 'scale ' + i); }
  for (const e of EVOLUTIONS) for (const a of e.abilities) ok(ABILITIES[a], 'capacité ' + a);
  ok(EVOLUTIONS.filter(e => e.divine).length >= 3, '≥3 divines');
});
test('planètes: ≥14, ids uniques, systèmes et ressources valides, palettes complètes', () => {
  ok(PLANETS.length >= 14, 'nb'); const ids = new Set(PLANETS.map(p => p.id)); eq(ids.size, PLANETS.length, 'ids uniques');
  const pk = new Set();
  for (const p of PLANETS) {
    ok(SYSTEMS.find(s => s.id === p.system), p.id + ' système');
    for (const k of ['deep', 'shore', 'low', 'mid', 'high', 'peak']) ok(/^#[0-9a-f]{6}$/i.test(p.palette[k]), `${p.id} palette.${k}`);
    for (const r of p.resources) ok(ITEMS[r], `${p.id} ressource ${r}`);
    ok(p.pos.length === 3 && p.radius > 0, p.id + ' pos/radius');
    for (const x of p.pickups) { ok(!pk.has(x.id), 'pickup dupliqué ' + x.id); pk.add(x.id); ok(Math.abs(x.lat) <= 90 && Math.abs(x.lon) <= 180, 'latlon'); }
  }
  // pas de chevauchement de planètes
  for (const a of PLANETS) for (const b of PLANETS) if (a !== b) { const d = Math.hypot(...a.pos.map((v, i) => v - b.pos[i])); ok(d > (a.radius + b.radius) * 2.5, `${a.id}/${b.id} trop proches`); }
});

// ── Progression ──
test('stageFor: seuils et verrous narratifs', () => {
  eq(stageFor(0), 0); eq(stageFor(99), 0); eq(stageFor(100), 1); eq(stageFor(5000), 5);
  eq(stageFor(999999), 5, 'bloqué sans flag'); eq(stageFor(999999, { act4_started: 1, titan_defeated: 1 }), 7);
});
test('xpProgress', () => { const p = xpProgress(225, 1); eq(p.ratio, .5); eq(p.need, 125); eq(xpProgress(99999, 9).next, null); });
test('applyXp: évolution multi-stades, stats et capacités', () => {
  const d = defaults(); const r = applyXp(d, 400);
  eq([r.from, r.to], [0, 2]); eq(d.hero.maxHp, EVOLUTIONS[2].hp); eq(d.hero.abilities, ['hop', 'dash', 'starbolt']);
  const r2 = applyXp(d, 10); eq(r2.to, 2);
});
test('applyXp: ne régresse jamais (stade debug supérieur conservé)', () => { const d = defaults(); d.hero.stage = 4; applyXp(d, 5); eq(d.hero.stage, 4); });
test('bus: atterrissage, découverte, pickup unique, hero:evolved', () => {
  State.data = defaults(); initProgression();
  const evo = []; bus.on('hero:evolved', e => evo.push(e.stage));
  const ember = PLANETS.find(p => p.id === 'ember');
  bus.emit('space:near', { planet: ember, canLand: false, distance: 500 });
  bus.emit('space:near', { planet: ember, canLand: true, distance: 50 });
  ok(State.data.world.discovered.includes('ember'), 'découverte');
  bus.emit('scene:entered', { id: 'planet', planet: ember });
  bus.emit('scene:entered', { id: 'planet', planet: ember });
  eq(State.data.world.location, 'ember'); eq(State.data.stats.landings, 2);
  const xp1 = State.data.hero.xp; ok(xp1 > 0, 'xp gagnée');
  bus.emit('pickup:collected', { id: 'ember_dust_0', kind: 'dust' }); bus.emit('pickup:collected', { id: 'ember_dust_0', kind: 'dust' });
  eq(State.data.world.collected, ['ember_dust_0']); eq(State.data.inventory.cosmicDust, 5);
  gainXp(1000, 'quest'); ok(evo.at(-1) >= 3 && evo.every((s, i) => !i || s > evo[i - 1]), 'évolué: ' + evo);
});

console.log(`${fail ? '✗' : '✓'} ${pass} OK, ${fail} échec(s)`);
process.exit(fail ? 1 : 0);
