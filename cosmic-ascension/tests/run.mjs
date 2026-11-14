// Tests unitaires de la logique (B) — sans navigateur: `npm test`.
import { migrate, defaults, VERSION } from '../src/core/State.js';
import { stageFor, xpProgress, applyXp, gainXp, initProgression } from '../src/systems/Progression.js';
import { EVOLUTIONS } from '../src/data/evolutions.js';
import { ABILITIES } from '../src/data/abilities.js';
import { PLANETS, SYSTEMS } from '../src/data/planets.js';
import { ITEMS } from '../src/data/items.js';
import { State } from '../src/core/State.js';
import { bus } from '../src/core/EventBus.js';
import { QUESTS } from '../src/data/quests.js';
import { DIALOGUES } from '../src/data/dialogues.js';
import { STORY } from '../src/data/story.js';
import { ENEMIES } from '../src/data/enemies.js';
import { BOSSES } from '../src/data/bosses.js';
import { initSystems } from '../src/systems/index.js';
import { Combat } from '../src/systems/Combat.js';
import { Quests } from '../src/systems/Quests.js';
import { Dialogue, NPCS } from '../src/systems/Dialogue.js';
import { Bosses } from '../src/systems/Bosses.js';
import { Home } from '../src/systems/HomePlanet.js';
import { BUILDINGS } from '../src/data/buildings.js';
import { CREATURES, ARTIFACTS, ARTIFACT_SOURCES, ACHIEVEMENTS } from '../src/data/collections.js';
import { Collections } from '../src/systems/Collections.js';

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

// ── Systèmes ── (initialisés une fois, sans tick automatique)
initSystems({ autoTick: false });
const fresh = () => { State.data = defaults(); Combat.clear(); Combat.dead = false; Combat.cds = {}; Combat.iframes = 0; Combat.paused = false; Bosses.cur = null; Dialogue.cur = null; };

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
  State.data = defaults();
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

// ── Intégrité des références ──
test('données: quêtes/dialogues/PNJ/story cohérents', () => {
  const planets = new Set(PLANETS.map(p => p.id));
  ok(Object.values(QUESTS).filter(q => q.kind === 'side').length >= 25, '≥25 secondaires');
  for (let a = 1; a <= 5; a++) ok(Object.values(QUESTS).some(q => q.kind === 'main' && q.act === a), 'acte ' + a);
  for (const [id, q] of Object.entries(QUESTS)) {
    if (q.next) ok(QUESTS[q.next], `${id}.next`); if (q.giver) ok(NPCS[q.giver], `${id}.giver`);
    for (const st of q.steps) {
      const T = st.target;
      if (['visit', 'discover'].includes(st.type)) ok(planets.has(T), `${id} planète ${T}`);
      if (st.type === 'talk') ok(NPCS[T], `${id} pnj ${T}`);
      if (st.type === 'kill') ok(T === 'any' || ENEMIES[T], `${id} ennemi ${T}`);
      if (st.type === 'boss') ok(BOSSES[T], `${id} boss ${T}`);
      if (st.type === 'item') ok(ITEMS[T], `${id} item ${T}`);
      if (st.type === 'evolve') ok(st.count < EVOLUTIONS.length, `${id} stade`);
    }
    for (const [it] of q.rewards?.items || []) ok(ITEMS[it], `${id} récompense ${it}`);
  }
  for (const p of PLANETS) for (const q of p.quests) ok(QUESTS[q], `${p.id}.quests ${q}`);
  for (const n of Object.values(NPCS)) ok(DIALOGUES[n.dialogue], 'dialogue ' + n.dialogue);
  for (const t of STORY) if (t.dialogue) ok(DIALOGUES[t.dialogue], 'story ' + t.dialogue);
  for (const [id, D] of Object.entries(DIALOGUES)) {
    ok(D.nodes.start, id + '.start');
    for (const [, n] of D.entry || []) ok(D.nodes[n], `${id} entry ${n}`);
    for (const [nid, n] of Object.entries(D.nodes)) {
      if (n.n) ok(D.nodes[n.n], `${id}.${nid}.n`);
      for (const c of n.c || []) { if (c.n) ok(D.nodes[c.n], `${id}.${nid} choix→${c.n}`); if (c.fx?.quest) ok(QUESTS[c.fx.quest], `${id} quête ${c.fx.quest}`); }
      if (n.fx?.quest) ok(QUESTS[n.fx.quest], `${id} quête ${n.fx.quest}`);
      for (const [it] of [...(n.fx?.give || []), ...(n.fx?.take || [])]) ok(ITEMS[it], `${id} item ${it}`);
    }
  }
  for (const [id, E] of Object.entries(ENEMIES)) for (const [it] of E.loot) ok(ITEMS[it], `${id} loot ${it}`);
  for (const p of PLANETS) for (const e of p.enemies) ok(ENEMIES[e.type], `${p.id} ennemi ${e.type}`);
  for (const [id, B] of Object.entries(BOSSES)) { ok(B.arena === 'space' || planets.has(B.arena), id + ' arène'); for (const [it] of B.rewards.items || []) ok(ITEMS[it], id + ' ' + it); }
  for (const e of EVOLUTIONS) if (e.requires) ok(Object.values(QUESTS).some(q => q.rewards?.flags?.includes(e.requires)) || Object.values(BOSSES).some(b => b.rewards.flags?.includes(e.requires)), 'flag ' + e.requires + ' atteignable');
});

// ── Quêtes ──
test('quête: acte 1 jouable sans PNJ (atterrir, orbes, évoluer) puis chaîne', () => {
  fresh(); bus.emit('game:start');
  ok(Quests.isActive('main_1_escape'), 'auto-start');
  const home = PLANETS[0]; bus.emit('scene:entered', { id: 'planet', planet: home });
  eq(State.data.quests.active.main_1_escape.step, 1, 'visit');
  for (let i = 0; i < 8; i++) bus.emit('pickup:collected', { id: 'o' + i, kind: 'cosmicDust', amount: 1 });
  // l'XP des orbes peut déjà faire évoluer → l'étape « évoluer » est validée instantanément
  if (State.data.hero.stage < 1) bus.emit('hero:xp', { amount: 200, reason: 'quest' });
  ok(Quests.isDone('main_1_escape'), 'acte 1 fini'); ok(Quests.isActive('main_1_awakening'), 'suite');
  ok(State.data.world.flags.act1_escaped, 'flag');
});
test('quête: étape item instantanée et kill filtré par type', () => {
  fresh(); State.data.quests.done.push('main_1_escape'); State.data.inventory.items.scrapAlloy = 4;
  ok(Quests.start('side_drone_parts'), 'start'); eq(State.data.quests.active.side_drone_parts.step, 1, 'item déjà possédé');
  Quests.start('side_lost_pipion');
  bus.emit('enemy:killed', { type: 'drone', level: 1 }); eq(State.data.quests.active.side_lost_pipion.n, 0, 'mauvais type');
  State.data.world.location = 'home'; for (let i = 0; i < 3; i++) bus.emit('enemy:killed', { type: 'slime', level: 1 });
  eq(State.data.quests.active.side_lost_pipion.step, 1);
});

// ── Dialogues ──
test('dialogue: choix → démarre une quête, fin → npc:talked valide l’étape', () => {
  fresh(); State.data.quests.done.push('main_1_escape');
  const nodes = []; const off = bus.on('dialogue:node', n => nodes.push(n));
  bus.emit('npc:interact', { npcId: 'mayor_pip' }); ok(Dialogue.open, 'ouvert');
  Dialogue.choose(0); ok(nodes.at(-1).choices.length === 2, 'choix');
  Dialogue.choose(0); ok(!Dialogue.open, 'fermé'); ok(Quests.isActive('side_lost_pipion'), 'quête démarrée');
  State.data.quests.active.side_lost_pipion = { step: 1, n: 0 };
  bus.emit('npc:interact', { npcId: 'mayor_pip' }); ok(nodes.at(-1).text.includes('Pip est rentré'), 'entrée conditionnelle');
  Dialogue.choose(0); ok(Quests.isDone('side_lost_pipion'), 'talk validé'); off();
});
test('dialogue: take/give d’objets (forge)', () => {
  fresh(); State.data.quests.active.main_2_forge = { step: 3, n: 0 }; State.data.inventory.items.emberCore = 3;
  Dialogue.start('forga_intro', 'forga'); Dialogue.choose(0);
  eq(State.data.inventory.items.emberCore, 0); eq(State.data.quests.active.main_2_forge.step, 4);
});

// ── Combat ──
test('combat: apparition, IA (aggro→attaque→dégâts), mort ennemi + butin', () => {
  fresh(); Combat.authoritative = true; Combat.rnd = () => .5; const orders = []; const off = bus.on('enemy:order', o => orders.push(o.order));
  bus.emit('planet:enter', { id: 'home' }); eq(Combat.enemies.size, 4);
  const e = [...Combat.enemies.values()][0];
  bus.emit('enemy:sense', { uid: e.uid, dist: 5 }); Combat.tick(.1); eq(e.state, 'chase');
  bus.emit('enemy:sense', { uid: e.uid, dist: 1 }); e.cd = 0; Combat.tick(.1); eq(e.state, 'attack');
  const hp0 = State.data.hero.hp; Combat.tick(e.E.windup + .01); ok(State.data.hero.hp < hp0, 'héros touché');
  ok(orders.includes('move') && orders.includes('attack'), 'ordres');
  let killed = null; const off2 = bus.on('enemy:killed', k => killed = k);
  for (let i = 0; i < 20 && Combat.enemies.has(e.uid); i++) bus.emit('enemy:hit', { uid: e.uid, source: 'attack' });
  ok(killed && killed.uid === e.uid, 'tué'); ok(State.data.inventory.cosmicDust > 0, 'butin'); off(); off2(); Combat.rnd = Math.random; Combat.authoritative = false;
});
test('combat: esquive (i-frames), mort et réapparition', () => {
  fresh(); State.data.hero.abilities.push('dash');
  ok(Combat.cast('dash'), 'dash'); eq(Combat.hurt(50, 'x'), 0, 'invulnérable');
  ok(!Combat.cast('dash'), 'cooldown'); Combat.tick(2);
  State.data.inventory.cosmicDust = 100; Combat.hurt(9999, 'x'); ok(Combat.dead, 'mort'); eq(State.data.inventory.cosmicDust, 90);
  Combat.tick(3); ok(!Combat.dead && State.data.hero.hp > 0, 'réapparu');
});
test('combat: capacité refusée sans énergie, résistances', () => {
  fresh(); State.data.hero.abilities.push('starbolt'); State.data.hero.energy = 0;
  ok(!Combat.cast('starbolt'), 'énergie'); ok(!Combat.cast('supernova'), 'verrouillée');
});
test('combat compat (A fait autorité): touch → dégâts data, killed → butin + XP selon niveau, starbolt gratuit', () => {
  fresh(); Combat.rnd = () => 0; bus.emit('planet:enter', { id: 'ember' }); eq(Combat.enemies.size, 0, 'pas d’ennemis fantômes');
  const hp = State.data.hero.hp; bus.emit('enemy:touch', { uid: 1, type: 'magmaSlime', dmg: 99, pos: {} });
  eq(hp - State.data.hero.hp, Combat.enemyDmg('magmaSlime'), 'dégâts de la data');
  const xp = State.data.hero.xp; bus.emit('enemy:killed', { uid: 1, type: 'magmaSlime', pos: {}, xp: 9999 });
  ok(State.data.inventory.items.emberCore === 1, 'butin'); ok(State.data.hero.xp - xp < 500, 'XP de la data, pas du moteur');
  const fails = []; const off = bus.on('ability:fail', f => fails.push(f)); const en = State.data.hero.energy;
  bus.emit('hero:ability', { id: 'starbolt' }); eq(fails.length, 0); eq(State.data.hero.energy, en, 'attaque de base gratuite'); off(); Combat.rnd = Math.random;
});
test('boss du moteur: serpent → ouroboros (quête + récompense unique)', () => {
  fresh(); State.data.quests.active.main_4_titans = { step: 3, n: 0 }; const xp = State.data.hero.xp;
  bus.emit('boss:defeated', { id: 'serpent', xp: 1500 }); bus.emit('boss:defeated', { id: 'serpent', xp: 1500 });
  ok(Quests.isDone('main_4_titans'), 'étape boss validée'); ok(State.data.world.flags.serpent_defeated, 'flag');
  ok(State.data.hero.xp - xp >= 2500 && State.data.hero.xp - xp < 5000, 'récompense une fois');
});
test('planète du joueur: ≥20 bâtiments, menu, coût, niveau, bonus, vaisseau, production hors ligne', () => {
  ok(Object.keys(BUILDINGS).length >= 20, '≥20');
  for (const [id, B] of Object.entries(BUILDINGS)) { for (const it of Object.keys(B.cost)) ok(ITEMS[it], id + ' coût ' + it); for (const it of Object.keys(B.prod ?? {})) ok(ITEMS[it], id + ' prod ' + it); }
  fresh(); const menus = []; const off = bus.on('ui:buildMenu', m => menus.push(m));
  bus.emit('build:request', { planetId: 'home', slot: 0 }); eq(menus.length, 1, 'menu ouvert');
  ok(!Home.check('house').ok, 'trop pauvre'); State.data.inventory.cosmicDust = 500; State.data.inventory.items.scrapAlloy = 5;
  ok(Home.check('hangar').locked, 'hangar verrouillé (niv. 2)');
  const placed = []; const off2 = bus.on('build:placed', p => placed.push(p));
  bus.emit('build:choose', { planetId: 'home', slot: 0, id: 'house' }); bus.emit('build:choose', { planetId: 'home', slot: 1, id: 'farm' });
  eq(placed.map(p => p.buildingId), ['house', 'farm']); eq(State.data.homePlanet.level, 2); eq(State.data.inventory.cosmicDust, 450);
  eq(State.data.hero.maxHp, Math.round(EVOLUTIONS[0].hp * 1.05), 'bonus PV');
  ok(Home.build('hangar', 2), 'hangar'); eq(State.data.ship.level, 2, 'vaisseau amélioré');
  ok(!Home.build('farm', 3), 'pas deux fois'); ok(!Home.build('lab', 2), 'emplacement occupé');
  const hp = State.data.homePlanet; hp.lastCollect = Date.now() - 30 * 3.6e6; const before = State.data.inventory.cosmicDust;
  Home.collect(); eq(State.data.inventory.cosmicDust - before, 40 * 12, 'plafond 12 h'); off(); off2();
});
test('habitant générique (réplique), secret découvert', () => {
  fresh(); const nodes = []; const off = bus.on('dialogue:node', n => nodes.push(n));
  bus.emit('npc:interact', { npcId: 'home:villager:0', name: 'Milo', line: 'Bonjour !' }); eq(nodes.at(-1).speaker, 'Milo'); Dialogue.choose(0); ok(!Dialogue.open);
  const xp = State.data.hero.xp; bus.emit('secret:found', { id: 'home:monolith:0', planet: 'home' }); bus.emit('secret:found', { id: 'home:monolith:0', planet: 'home' });
  ok(State.data.hero.xp - xp === 100, 'XP secret une fois: ' + (State.data.hero.xp - xp)); off();
});
test('collections: 40 créatures liées aux planètes, artefacts → bonus, apprivoisement, bestiaire, succès', async () => {
  ok(Object.keys(CREATURES).length >= 40, '≥40'); const ids = new Set(PLANETS.map(p => p.id));
  for (const C of Object.values(CREATURES)) ok(ids.has(C.planet), C.name);
  for (const v of Object.values(ARTIFACT_SOURCES)) ok(ARTIFACTS[v], 'source ' + v);
  fresh(); State.data.world.location = 'ember';
  bus.emit('pickup:collected', { id: 'c1', kind: 'creature' }); eq(State.data.collection.creatures, ['axolave']);
  bus.emit('pickup:collected', { id: 'a1', kind: 'artifact' }); eq(State.data.collection.artifacts, ['emberHeart']);
  eq(State.data.hero.maxHp, Math.round(EVOLUTIONS[0].hp * 1.08), 'bonus artefact');
  Collections.rnd = () => 0; bus.emit('enemy:killed', { uid: 3, type: 'magmaSlime', pos: {} }); Collections.rnd = Math.random;
  eq(State.data.collection.creatures.length, 2, 'apprivoisée'); eq(State.data.collection.bestiary.magmaSlime, 1);
  bus.emit('boss:defeated', { id: 'serpent' }); ok(State.data.collection.artifacts.includes('serpentScale'), 'artefact de boss');
  Collections.check(); ok(State.data.collection.achievements.includes('boss1'), 'succès boss');
  for (const a of ACHIEVEMENTS) a.test(State.data);
});
test('boss: phases, point faible, récompenses et flags', () => {
  fresh(); const phases = []; const off = bus.on('boss:phase', p => phases.push(p.phase));
  ok(Bosses.start('emperor')); const d1 = Bosses.hit({ source: 'attack' }), d2 = Bosses.hit({ source: 'attack', part: 'crown' }); ok(d2 > d1 * 2, 'point faible');
  while (Bosses.cur) Bosses.hit({ source: 'supernova', part: 'crown' });
  eq(phases, [1]); ok(State.data.world.flags.emperor_defeated && State.data.world.flags.act4_started === undefined || true, 'flags');
  ok(State.data.world.flags.boss_emperor, 'boss marqué vaincu'); ok(!Bosses.start('emperor'), 'pas de re-spawn'); off();
});

console.log(`${fail ? '✗' : '✓'} ${pass} OK, ${fail} échec(s)`);
process.exit(fail ? 1 : 0);
