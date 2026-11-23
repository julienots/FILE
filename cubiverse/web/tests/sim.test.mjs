// Tests de la simulation (Node, sans rendu) : node tests/sim.test.mjs
import assert from 'node:assert/strict';
import { setSeed } from '../src/sim/rng.js';
import { World } from '../src/sim/world.js';
import { Simulation } from '../src/sim/simulation.js';
import { starterWorld, buildStructure } from '../src/sim/presets.js';
import { serialize, deserialize } from '../src/sim/persistence.js';
import { buildPath } from '../src/sim/nav.js';
import { MAX_PER_CUBE } from '../src/sim/constants.js';

globalThis.performance ??= { now: () => Date.now() };
let passed = 0;
function test(name, fn) {
  const t0 = Date.now();
  try { fn(); passed++; console.log(`✔ ${name} (${Date.now() - t0} ms)`); }
  catch (e) { console.error(`✘ ${name}\n`, e); process.exitCode = 1; }
}
function run(sim, seconds, dt = 1 / 30) { for (let t = 0; t < seconds; t += dt) sim.step(dt); }

setSeed(42);

test('connexions : adjacence et ouverture des deux côtés', () => {
  const w = new World();
  const a = w.addCube(0, 0, 0, 'maison'); const b = w.addCube(1, 0, 0, 'parc'); const c = w.addCube(0, 1, 0, 'arcade');
  assert.equal(w.neighbor(a, 'px'), b); assert.equal(w.neighbor(b, 'nx'), a); assert.equal(w.neighbor(a, 'py'), c);
  assert.equal(w.neighbor(a, 'nx'), null);
  w.setLink(a, 'px', false);
  assert.equal(w.neighbor(a, 'px'), null); assert.equal(w.neighbor(b, 'nx'), null);
  w.setLink(a, 'px', true);
  assert.ok(w.moveCube(b.id, 5, 0, 0));
  assert.equal(w.neighbor(a, 'px'), null, 'séparé après déplacement');
  assert.ok(!w.moveCube(b.id, 0, 1, 0), 'emplacement occupé refusé');
  assert.ok(w.moveCube(b.id, 1, 0, 0));
  assert.equal(w.neighbor(a, 'px'), b, 'reconnecté');
});

test('déplacer un cube conserve ses personnages', () => {
  const w = new World(); const a = w.addCube(0, 0, 0); w.addCube(1, 0, 0);
  for (let i = 0; i < 5; i++) w.createChar(a.id);
  w.moveCube(a.id, 10, 0, 3);
  assert.equal(a.chars.size, 5);
  for (const id of a.chars) assert.equal(w.chars.get(id).cubeId, a.id);
});

test('chemin multi-cubes (horizontal + vertical)', () => {
  const w = new World();
  const a = w.addCube(0, 0, 0); w.addCube(1, 0, 0); const c = w.addCube(1, 1, 0);
  const ch = w.createChar(a.id);
  const p = buildPath(w, ch, c.id, 0, 0);
  assert.ok(p, 'chemin trouvé');
  assert.ok(p.some((x) => x.kind === 'door')); assert.ok(p.some((x) => x.kind === 'climb'));
});

test('32 personnages maximum par cube', () => {
  const w = new World(); const a = w.addCube(0, 0, 0);
  for (let i = 0; i < 40; i++) w.createChar(a.id);
  assert.equal(a.chars.size, MAX_PER_CUBE);
});

test('simulation de 32 personnages dans un cube pendant 10 minutes simulées', () => {
  const w = new World(); const a = w.addCube(0, 0, 0, 'restaurant');
  for (let i = 0; i < 32; i++) w.createChar(a.id);
  const sim = new Simulation(w);
  const t0 = Date.now(); run(sim, 600, 1 / 20); const ms = Date.now() - t0;
  console.log(`   ${ms} ms pour 12 000 pas ; liens créés : ${[...w.chars.values()].reduce((s, c) => s + Object.keys(c.rel).length, 0)}`);
  for (const ch of w.chars.values()) {
    assert.ok(Math.abs(ch.x) <= 2.01 && Math.abs(ch.z) <= 2.01, 'reste dans le cube (aucune connexion)');
    for (const k in ch.needs) assert.ok(ch.needs[k] >= 0 && ch.needs[k] <= 1);
  }
  assert.ok([...w.chars.values()].some((c) => Object.keys(c.rel).length > 0), 'interactions sociales');
  assert.ok([...w.chars.values()].some((c) => Object.keys(c.habits).length > 3), 'habitudes');
});

test('passage entre les cubes connectés (3 cubes + étage)', () => {
  setSeed(7);
  const w = starterWorld('test');
  const sim = new Simulation(w);
  run(sim, 900, 1 / 15);
  const crossings = [...w.chars.values()].reduce((s, c) => s + c.stats.crossings, 0);
  console.log(`   passages : ${crossings}, événements : ${w.log.length}`);
  assert.ok(crossings > 5, 'des personnages changent de cube');
  for (const c of w.cubes.values()) assert.ok(c.chars.size <= MAX_PER_CUBE);
  // cohérence index cube <-> personnage
  for (const ch of w.chars.values()) assert.ok(w.cubes.get(ch.cubeId).chars.has(ch.id));
  let total = 0; for (const c of w.cubes.values()) total += c.chars.size;
  assert.equal(total, w.chars.size);
  const types = new Set(w.log.map((e) => e.type));
  console.log('   types de journal :', [...types].join(', '));
});

test('pas de passage par un côté déconnecté', () => {
  setSeed(3);
  const w = new World(); const a = w.addCube(0, 0, 0, 'parc'); const b = w.addCube(1, 0, 0, 'parc');
  w.setLink(a, 'px', false);
  for (let i = 0; i < 12; i++) w.createChar(a.id);
  const sim = new Simulation(w); run(sim, 300, 1 / 15);
  assert.equal(b.chars.size, 0);
});

test('sauvegarde / restauration fidèles', () => {
  const w = starterWorld('save');
  const sim = new Simulation(w); run(sim, 120, 1 / 10);
  const data = JSON.parse(JSON.stringify(serialize(w)));
  const w2 = deserialize(data);
  assert.equal(w2.cubes.size, w.cubes.size); assert.equal(w2.chars.size, w.chars.size);
  const c1 = [...w.chars.values()][3]; const c2 = w2.chars.get(c1.id);
  assert.equal(c2.name, c1.name); assert.equal(c2.cubeId, c1.cubeId);
  assert.deepEqual(Object.keys(c2.rel).sort(), Object.keys(c1.rel).sort());
  assert.equal(w2.log.length, w.log.length);
  const sim2 = new Simulation(w2); run(sim2, 30, 1 / 10);
});

test('grande structure : 2 000 cubes / 8 000 personnages', () => {
  setSeed(11);
  const w = new World(); w.addCube(0, 0, 0);
  let t0 = Date.now();
  buildStructure(w, 'grid', 2000, { charsPer: 4 });
  console.log(`   création : ${Date.now() - t0} ms, ${w.cubes.size} cubes, ${w.chars.size} personnages`);
  const sim = new Simulation(w);
  t0 = Date.now(); let worst = 0;
  for (let i = 0; i < 600; i++) { const s = Date.now(); sim.step(1 / 30); worst = Math.max(worst, Date.now() - s); }
  console.log(`   600 images : ${Date.now() - t0} ms (pire image ${worst} ms), cubes actifs : ${sim.stats.active}`);
  assert.ok(sim.stats.active <= sim.maxActive);
  assert.ok((Date.now() - t0) / 600 < 30, 'moins de 30 ms par image en moyenne');
});

console.log(`\n${passed} test(s) réussi(s)`);
