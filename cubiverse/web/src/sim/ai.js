// Intelligence des personnages : besoins, décision par utilité, exécution des actions,
// déplacements (y compris entre cubes) et interactions sociales à deux.
import { CUBE, HALF, WALL_MARGIN, DOOR_HALF, MAX_PER_CUBE, CHAR_RADIUS, clamp, SIDES } from './constants.js';
import { OBJECT_TYPES, ITEMS } from './themes.js';
import { rand, rrange, pick, chance, weightedPick } from './rng.js';
import { buildPath, waypointLocal, cubeDistances } from './nav.js';
import { getRel, relate, remember, feel, mood, compat, bestFriendId } from './social.js';

// Comment un personnage utilise chaque type d'objet : 'on' = dessus/dedans, sinon devant.
const USE_MODE = {
  bed: { on: true, y: 0.2 }, sofa: { on: true, y: 0.13 }, chair: { on: true, y: 0.13 }, bench: { on: true, y: 0.13 },
  throne: { on: true, y: 0.2 }, sunbed: { on: true, y: 0.13 }, swing: { on: true, y: 0.18 }, rock: { on: true, y: 0.34 },
  car: { on: true, y: 0.06 }, pool: { on: true, y: -0.06, spread: 0.45 }, slide: { on: true, y: 0 }, tree: { ring: true }, campfire: { ring: true },
  fountain: { ring: true }, pond: { ring: true }, flowerbed: { ring: true }, table: { ring: true }, sandcastle: { ring: true }, jukebox: { ring: true, dist: 0.6 },
};

const NEED_DECAY = { energy: 1 / 420, hunger: 1 / 260, fun: 1 / 170, social: 1 / 210, comfort: 1 / 320, curiosity: 1 / 280 };
const LOCKED = new Set(['event', 'joint', 'climb']);
const tmp2 = [0, 0];

export const urgency = (v) => { const u = 1 - v; return u * u * 2.6 + u * 0.4; };

// ---------- animation ----------
export function setAnim(ch, name) {
  if (ch.anim !== name) { ch.prevAnim = ch.anim; ch.anim = name; ch.blend = 0; ch.animT = 0; }
}
function bubble(ch, icon, t = 2.2) { ch.bubble = { icon, t }; }

// ---------- besoins / émotions ----------
function decay(world, ch, dt) {
  const n = ch.needs; const t = ch.traits;
  const sleeping = ch.anim === 'sleep';
  n.energy = clamp(n.energy - (sleeping ? -1 / 60 : NEED_DECAY.energy * (0.7 + t.energy * 0.6) * (world.isNight ? 1.5 : 1)) * dt, 0, 1);
  n.hunger = clamp(n.hunger - NEED_DECAY.hunger * (sleeping ? 0.4 : 1) * dt, 0, 1);
  n.fun = clamp(n.fun - NEED_DECAY.fun * (0.6 + t.playfulness * 0.8) * (sleeping ? 0.2 : 1) * dt, 0, 1);
  n.social = clamp(n.social - NEED_DECAY.social * (0.4 + t.sociability * 1.2) * (sleeping ? 0.2 : 1) * dt, 0, 1);
  n.comfort = clamp(n.comfort - NEED_DECAY.comfort * dt, 0, 1);
  n.curiosity = clamp(n.curiosity - NEED_DECAY.curiosity * (0.4 + t.curiosity * 1.2) * (sleeping ? 0.2 : 1) * dt, 0, 1);
  const e = ch.emo;
  const k = Math.exp(-dt / 40);
  e.joy *= k; e.sadness *= k; e.anger *= Math.exp(-dt / (30 + t.temper * 30)); e.fear *= Math.exp(-dt / 12); e.surprise *= Math.exp(-dt / 5);
  // l'humeur générale nourrit les émotions de fond
  const m = mood(ch);
  if (m > 0.7) e.joy = Math.max(e.joy, (m - 0.55) * 0.9);
  if (n.hunger < 0.12 || n.energy < 0.08) e.sadness = Math.min(1, e.sadness + dt * 0.01);
  if (ch.bubble) { ch.bubble.t -= dt; if (ch.bubble.t <= 0) ch.bubble = null; }
}

function recordHabit(world, ch, id) {
  let h = ch.habits[id];
  if (!h) h = ch.habits[id] = { n: 0, h: [0, 0, 0, 0, 0, 0] };
  h.n++; h.h[Math.floor(world.hour / 4) % 6]++;
}
function habitBonus(world, ch, id) {
  const h = ch.habits[id]; if (!h) return 1;
  const slot = h.h[Math.floor(world.hour / 4) % 6] / Math.max(1, h.n);
  return 1 + Math.min(0.5, Math.sqrt(h.n) * 0.06) + slot * 0.35;
}
function gainSkill(ch, s, amt) { if (s) ch.skills[s] = Math.min(10, (ch.skills[s] || 0) + amt / (1 + (ch.skills[s] || 0) * 0.15)); }
function applyGains(ch, gains, f = 1) {
  for (const k in gains) ch.needs[k] = clamp(ch.needs[k] + gains[k] * f, 0, 1);
  if (gains.fun > 0) feel(ch, 'joy', gains.fun * 0.4 * f);
}
function addItem(world, ch, type) {
  if (ch.inventory.length >= 6) ch.inventory.shift();
  ch.inventory.push({ type, t: world.time });
}
function takeItem(ch, pred) {
  const i = ch.inventory.findIndex((it) => pred(it.type)); if (i < 0) return null;
  return ch.inventory.splice(i, 1)[0];
}
const hasItem = (ch, pred) => ch.inventory.some((it) => pred(it.type));
const isFood = (t) => !!ITEMS[t]?.food;
const isGift = (t) => !!ITEMS[t]?.gift;

// ---------- déplacement ----------
function canPass(world, cube, side, ch) {
  const n = world.neighbor(cube, side);
  return n && (n.chars.size < MAX_PER_CUBE || n.chars.has(ch.id)) ? n : null;
}

// Mouvement physique avec murs, portes et transferts entre cubes.
function physicalMove(world, ch, dx, dz) {
  const cube = world.cubes.get(ch.cubeId);
  let nx = ch.x + dx, nz = ch.z + dz;
  const lim = HALF - WALL_MARGIN;
  // axe X
  if (nx > lim || nx < -lim) {
    const side = nx > 0 ? 'px' : 'nx';
    if (Math.abs(nz) < DOOR_HALF && canPass(world, cube, side, ch)) {
      if (Math.abs(nx) > HALF) { ch.x = nx; ch.z = nz; return world.transfer(ch, world.neighbor(cube, side)) || clampBack(ch, lim); }
    } else nx = clamp(nx, -lim, lim);
  }
  if (nz > lim || nz < -lim) {
    const side = nz > 0 ? 'pz' : 'nz';
    if (Math.abs(nx) < DOOR_HALF && canPass(world, cube, side, ch)) {
      if (Math.abs(nz) > HALF) { ch.x = nx; ch.z = nz; return world.transfer(ch, world.neighbor(cube, side)) || clampBack(ch, lim); }
    } else nz = clamp(nz, -lim, lim);
  }
  // dans l'embrasure d'une porte, on reste dans le couloir
  if (Math.abs(nx) > lim) nz = clamp(nz, -DOOR_HALF + 0.08, DOOR_HALF - 0.08);
  if (Math.abs(nz) > lim) nx = clamp(nx, -DOOR_HALF + 0.08, DOOR_HALF - 0.08);
  ch.x = nx; ch.z = nz;
  return true;
}
function clampBack(ch, lim) { ch.x = clamp(ch.x, -lim, lim); ch.z = clamp(ch.z, -lim, lim); return false; }

// Évitement : objets (sauf la cible) et autres personnages du même cube.
function avoid(world, ch, cube, ignoreObj) {
  for (const o of cube.objects) {
    if (o.id === ignoreObj) continue;
    const r = OBJECT_TYPES[o.type].r + CHAR_RADIUS * 0.6;
    const dx = ch.x - o.x, dz = ch.z - o.z; const d2 = dx * dx + dz * dz;
    if (d2 < r * r && d2 > 1e-6) { const d = Math.sqrt(d2); const p = (r - d) * 0.5; ch.x += (dx / d) * p; ch.z += (dz / d) * p; }
  }
  if (ch.y > 0.05) return;
  for (const id of cube.chars) {
    if (id === ch.id) continue;
    const o = world.chars.get(id); if (!o || o.y > 0.05) continue;
    const dx = ch.x - o.x, dz = ch.z - o.z; const d2 = dx * dx + dz * dz;
    const r = CHAR_RADIUS * 1.6;
    if (d2 < r * r && d2 > 1e-6) { const d = Math.sqrt(d2); const p = (r - d) * 0.25; ch.x += (dx / d) * p; ch.z += (dz / d) * p; }
  }
}

function turnTo(ch, tx, tz, dt, rate = 8) {
  const target = Math.atan2(tx - ch.x, tz - ch.z);
  let d = target - ch.ry; while (d > Math.PI) d -= Math.PI * 2; while (d < -Math.PI) d += Math.PI * 2;
  ch.ry += d * Math.min(1, dt * rate);
}

// Avance le long du chemin. Retourne 'moving' | 'done' | 'fail'.
function followPath(world, ch, act, dt) {
  const wp = act.path[act.wi];
  if (!wp) return 'done';
  if (!world.cubes.has(wp.cube)) return 'fail';
  if (wp.kind === 'climb' && ch.cubeId !== wp.cube) {
    const dest = world.cubes.get(wp.cube);
    if (dest.chars.size >= MAX_PER_CUBE) return 'fail';
    setAnim(ch, 'climb');
    ch.ry = Math.PI * 1.25 + (wp.dir > 0 ? 0 : Math.PI);
    ch.y += wp.dir * 1.25 * dt;
    if (Math.abs(ch.y) >= CUBE) {
      if (!world.transfer(ch, dest)) { ch.y = 0; return 'fail'; }
      ch.y = 0; act.wi++;
      bubble(ch, '👋', 1.2);
    }
    return 'moving';
  }
  if (!waypointLocal(world, ch, wp, tmp2)) return 'fail';
  const tx = tmp2[0], tz = tmp2[1];
  const dx = tx - ch.x, dz = tz - ch.z; const d = Math.hypot(dx, dz);
  const run = act.run;
  const spd = (run ? 1.35 : 0.55) * ch.speedMul;
  if (d < 0.06) { act.wi++; return act.wi >= act.path.length ? 'done' : 'moving'; }
  setAnim(ch, run ? 'run' : 'walk');
  const step = Math.min(d, spd * dt);
  turnTo(ch, tx, tz, dt, 10);
  const before = ch.cubeId;
  if (!physicalMove(world, ch, (dx / d) * step, (dz / d) * step)) {
    act.stuck = (act.stuck || 0) + dt; if (act.stuck > 1.5) return 'fail';
  }
  ch.stats.steps += step;
  if (ch.cubeId !== before) onEnterCube(world, ch, before);
  return 'moving';
}

function onEnterCube(world, ch, fromId) {
  const c = world.cubes.get(ch.cubeId);
  ch.seen = ch.seen || [];
  if (!ch.seen.includes(c.id)) {
    ch.seen.push(c.id); if (ch.seen.length > 30) ch.seen.shift();
    ch.needs.curiosity = clamp(ch.needs.curiosity + 0.25, 0, 1);
    feel(ch, 'surprise', 0.5);
    remember(world, ch, `A découvert le cube « ${c.name} »`, 1);
    bubble(ch, '✨', 1.8);
  }
}

// ---------- fabrique d'actions ----------
function pathTo(world, ch, cubeId, x, z, run = false) {
  if (cubeId === ch.cubeId) return [{ cube: cubeId, x, z, kind: 'walk' }];
  return buildPath(world, ch, cubeId, x, z);
}

function objUsePoint(o, slot, cap) {
  const t = OBJECT_TYPES[o.type]; const m = USE_MODE[o.type] || {};
  if (m.on) {
    const s = m.spread ? [rrange(-m.spread, m.spread), rrange(-m.spread, m.spread)] : [(slot - (cap - 1) / 2) * 0.32, 0];
    const c = Math.cos(o.r), si = Math.sin(o.r);
    return [o.x + s[0] * c + s[1] * si, o.z - s[0] * si + s[1] * c, m.y || 0];
  }
  const dist = t.r + (m.dist || 0.14);
  const ang = m.ring ? o.r + (slot / Math.max(1, cap)) * Math.PI * 2 + 0.4 : o.r + (slot - (cap - 1) / 2) * 0.7;
  return [clamp(o.x + Math.sin(ang) * dist, -HALF + 0.2, HALF - 0.2), clamp(o.z + Math.cos(ang) * dist, -HALF + 0.2, HALF - 0.2), 0];
}

function users(o) { return o._users || (o._users = new Set()); }

function makeObjAct(world, ch, cube, o, aff, opts = {}) {
  const u = users(o);
  const slot = u.size;
  const [px, pz, py] = objUsePoint(o, slot, aff.cap);
  const path = pathTo(world, ch, cube.id, px, pz);
  if (!path) return null;
  return {
    kind: 'obj', id: aff.id, label: aff.label, anim: aff.anim, path, wi: 0, phase: 'go',
    dur: rrange(aff.dur[0], aff.dur[1]) * (opts.durMul || 1), t: 0, obj: o.id, objCube: cube.id, seatY: py,
    face: USE_MODE[o.type]?.on ? null : [o.x, o.z], faceR: USE_MODE[o.type]?.on ? o.r : null, aff, lock: aff.anim === 'sleep',
    ...opts,
  };
}

function soloAct(id, label, anim, dur, extra = {}) {
  return { kind: 'solo', id, label, anim, dur, t: 0, phase: 'do', ...extra };
}

// ---------- décision ----------
export function decide(world, ch) {
  const cube = world.cubes.get(ch.cubeId);
  const n = ch.needs, t = ch.traits, e = ch.emo;
  const night = world.isNight;
  const cands = [];
  const add = (score, make, tag) => { if (score > 0.02) cands.push({ score, make, tag }); };

  // --- objets du cube (et des voisins directs avec pénalité) ---
  const waiting = [];
  for (const id of cube.chars) { const o = world.chars.get(id); if (o && o.waitingFood && o.id !== ch.id) waiting.push(o); }
  const scanCubes = [[cube, 0]];
  for (const [, nb] of world.neighbors(cube)) scanCubes.push([nb, 0.35]);
  for (const [c, pen] of scanCubes) {
    for (const o of c.objects) {
      const type = OBJECT_TYPES[o.type]; if (!type) continue;
      for (const aff of type.acts) {
        if (users(o).size >= aff.cap) continue;
        if (aff.req === 'dirty' && !o.state.dirty) continue;
        let s = 0;
        for (const k in aff.gains) if (aff.gains[k] > 0) s += urgency(n[k]) * aff.gains[k];
        if (aff.gains.energy < 0 && n.energy < 0.35) s *= 0.2;
        if (aff.anim === 'sleep' && aff.id === 'sleep') s *= night ? 2.2 : (n.energy > 0.35 ? 0.15 : 1);
        if (aff.night) s *= night ? 1 + aff.night : 1 - aff.night * 0.4;
        if (aff.skill) s *= 0.75 + t.diligence * 0.4 + Math.min(0.4, (ch.skills[aff.skill] || 0) * 0.05);
        if (aff.gains.curiosity) s *= 0.7 + t.curiosity * 0.6;
        if (aff.gains.fun) s *= 0.7 + t.playfulness * 0.5;
        if (aff.id === 'cook') s += waiting.length ? 0.9 * (0.5 + t.diligence + t.kindness * 0.5) : 0;
        if (aff.req === 'dirty') s += 0.35 + t.diligence * 0.6 + t.kindness * 0.2;
        if (aff.req === 'food') {
          if (!hasItem(ch, isFood)) {
            // commande au restaurant : il faut une cuisinière dans le cube
            if (c !== cube || !cube.objects.some((x) => x.type === 'stove')) continue;
            s = urgency(n.hunger) * 0.9 * (0.6 + t.sociability * 0.5);
            add(s * habitBonus(world, ch, 'order') - pen, () => orderAct(world, ch, cube, o), 'order');
            continue;
          }
          s *= 1.3;
        }
        s *= habitBonus(world, ch, aff.id);
        s *= 0.85 + rand() * 0.3;
        add(s - pen, () => makeObjAct(world, ch, c, o, aff), aff.id);
      }
    }
  }

  // --- service : apporter un plat à quelqu'un qui attend ---
  if (waiting.length && hasItem(ch, (x) => x === 'plat')) {
    const target = waiting.find((w) => !w.servedBy);
    if (target) add(2.2 + t.kindness, () => serveAct(world, ch, target), 'serve');
  }
  // --- manger un aliment de l'inventaire ---
  if (hasItem(ch, isFood)) add(urgency(n.hunger) * 1.1, () => {
    const it = takeItem(ch, isFood); const f = ITEMS[it.type].food;
    return soloAct('eatItem', `mange : ${ITEMS[it.type].label.toLowerCase()}`, 'eat', rrange(4, 6), { gains: { hunger: f } });
  }, 'eatItem');
  if (n.energy < 0.12) add(urgency(n.energy) * 0.7, () => soloAct('napFloor', 'fait une sieste par terre', 'sleep', rrange(12, 22), { gains: { energy: 0.45 }, lock: true }), 'nap');
  if (night && n.energy < 0.5) add(0.3, () => soloAct('napFloor', 's\'endort par terre', 'sleep', rrange(18, 30), { gains: { energy: 0.5 }, lock: true }), 'nap');

  // --- actions solo ---
  const fu = urgency(n.fun);
  add(0.18 + t.curiosity * 0.12, () => wanderAct(world, ch, cube), 'wander');
  add((0.1 + t.energy * 0.35) * (n.energy > 0.5 ? 1 : 0.2), () => jogAct(world, ch, cube), 'jog');
  add((e.joy * 0.6 + fu * 0.2) * t.playfulness, () => soloAct('dance', 'danse', 'dance', rrange(6, 10), { gains: { fun: 0.25 }, skill: 'musique' }), 'dance');
  add(fu * t.playfulness * 0.35, () => soloAct('jump', 'saute de joie', 'jump', 2.4, { gains: { fun: 0.12 } }), 'jump');
  add(fu * t.playfulness * 0.3, () => rollAct(ch), 'roll');
  add(urgency(n.comfort) * 0.25 + urgency(n.energy) * 0.2, () => soloAct('sitFloor', "s'assoit par terre", 'sitFloor', rrange(6, 12), { gains: { comfort: 0.15, energy: 0.08 } }), 'sitFloor');
  add(e.anger * 1.1, () => soloAct('sulk', 'boude', 'sulk', rrange(6, 10), { onDone: (w, c) => { c.emo.anger *= 0.5; } }), 'sulk');
  if (e.sadness > 0.45) add(e.sadness * 0.9, () => soloAct('cry', 'pleure', 'cry', rrange(4, 7), { onDone: (w, c) => { c.emo.sadness *= 0.6; } }), 'cry');
  add(e.joy > 0.6 ? 0.25 : 0, () => soloAct('laugh', 'rit tout seul', 'laugh', 2.5), 'laugh');
  if (hasItem(ch, (x) => x === 'ballon')) add(fu * 0.6, () => soloAct('juggle', 'jongle avec le ballon', 'throw', rrange(4, 7), { gains: { fun: 0.25 }, skill: 'sport', ball: true }), 'juggle');
  if (hasItem(ch, (x) => x === 'livre')) add(urgency(n.curiosity) * 0.6, () => soloAct('readOwn', 'lit son livre', 'read', rrange(6, 10), { gains: { curiosity: 0.3 }, skill: 'lecture' }), 'readOwn');

  // --- exploration des cubes voisins ---
  const nbs = world.neighbors(cube);
  if (nbs.length) {
    add((urgency(n.curiosity) * 0.9 + 0.08) * (0.4 + t.curiosity), () => exploreAct(world, ch, cube), 'explore');
    // rendre visite à un ami ailleurs
    if (urgency(n.social) > 0.25 || chance(0.15)) {
      const friend = findFriendElsewhere(world, ch);
      if (friend) add((urgency(n.social) * 0.9 + 0.12) * (0.5 + t.sociability), () => visitAct(world, ch, friend), 'visit');
    }
  }

  // --- interactions sociales dans le cube ---
  const others = [];
  for (const id of cube.chars) if (id !== ch.id) { const o = world.chars.get(id); if (o && available(o)) others.push(o); }
  // on échantillonne au plus 8 partenaires potentiels
  for (let i = others.length - 1; i > 0; i--) { const j = Math.floor(rand() * (i + 1)); [others[i], others[j]] = [others[j], others[i]]; }
  const su = urgency(n.social);
  const shy = 1 - t.sociability;
  for (const o of others.slice(0, 8)) {
    const r = ch.rel[o.id]; const a = r ? r.a : 0; const fam = r ? r.f : 0;
    const focus = ch.focus === o.id && world.time < ch.focusUntil ? 2.5 : 1;
    const base = (su * 0.9 + 0.1) * (0.25 + t.sociability) * focus;
    if (fam < 4) {
      add(base * (shy > 0.6 ? 0.5 : 0.8), () => jointAct(world, ch, o, 'wave'), 'wave');
      add(base * 0.6 * t.sociability, () => jointAct(world, ch, o, 'handshake'), 'handshake');
    }
    add(base * (a > -20 ? 1 : 0.3), () => jointAct(world, ch, o, 'talk'), 'talk');
    if (a > -10) add(base * t.playfulness * 0.7, () => jointAct(world, ch, o, 'joke'), 'joke');
    if (a > 30) add(base * (a / 100) * (0.6 + t.kindness * 0.5) * 1.1, () => jointAct(world, ch, o, 'hug'), 'hug');
    if (a > 10) add((fu * 0.7 + 0.05) * t.playfulness * (0.5 + t.sociability), () => jointAct(world, ch, o, 'chase'), 'chase');
    if (a > 15 && cube.objects.length >= 2) add((fu * 0.6) * t.playfulness * (0.4 + t.sociability) * 0.8, () => jointAct(world, ch, o, 'hide'), 'hide');
    if (a > 0 && (hasItem(ch, (x) => x === 'ballon') || cube.objects.some((x) => x.type === 'toybox'))) add(fu * 0.8 * t.playfulness, () => jointAct(world, ch, o, 'ball'), 'ball');
    if (a > 5) add((fu * 0.5 + e.joy * 0.3) * t.playfulness * 0.7, () => jointAct(world, ch, o, 'danceDuo'), 'danceDuo');
    if (a > 20 && hasItem(ch, isGift)) add(0.35 + t.kindness * 0.7 * (a / 100), () => jointAct(world, ch, o, 'gift'), 'gift');
    // conflits
    const irritation = e.anger * 0.8 + t.temper * 0.35 + (a < -20 ? 0.4 : 0) - t.kindness * 0.3;
    if (irritation > 0.2) add(irritation * 0.5, () => jointAct(world, ch, o, 'argue'), 'argue');
    if (irritation > 0.45 && a < -25) add(irritation * 0.45, () => jointAct(world, ch, o, 'fight'), 'fight');
    // réconciliation
    if (a < -20 && t.kindness > 0.6 && e.anger < 0.3) add(0.25 * t.kindness, () => jointAct(world, ch, o, 'handshake'), 'reconcile');
  }

  if (!cands.length) return wanderAct(world, ch, cube);
  cands.sort((a, b) => b.score - a.score);
  const top = cands.slice(0, 5);
  for (let tries = 0; tries < 4 && top.length; tries++) {
    const c = weightedPick(top, (x) => x.score * x.score);
    const act = c.make();
    if (act) return act;
    top.splice(top.indexOf(c), 1);
  }
  return wanderAct(world, ch, cube);
}

function available(o) { return !o.act || (!LOCKED.has(o.act.kind) && !o.act.lock && !o.waitingFood); }

function wanderAct(world, ch, cube) {
  const x = rrange(-HALF + 0.35, HALF - 0.35), z = rrange(-HALF + 0.35, HALF - 0.35);
  return { kind: 'solo', id: 'wander', label: 'se promène', anim: 'idle', path: [{ cube: cube.id, x, z, kind: 'walk' }], wi: 0, phase: 'go', dur: rrange(1, 3), t: 0, gains: { curiosity: 0.03 } };
}
function jogAct(world, ch, cube) {
  const pts = []; for (let i = 0; i < 3; i++) pts.push({ cube: cube.id, x: rrange(-1.6, 1.6), z: rrange(-1.6, 1.6), kind: 'walk' });
  return { kind: 'solo', id: 'jog', label: 'court partout', anim: 'idle', path: pts, wi: 0, phase: 'go', run: true, dur: 0.6, t: 0, gains: { fun: 0.12, energy: -0.05 }, skill: 'sport' };
}
function rollAct(ch) {
  return soloAct('roll', 'fait des roulades', 'roll', 1.6, { gains: { fun: 0.15 }, move: 0.9 });
}
function exploreAct(world, ch, cube) {
  const dist = cubeDistances(world, cube.id, 1 + Math.floor(ch.traits.curiosity * 3));
  const options = [...dist.keys()].filter((id) => id !== cube.id && world.cubes.get(id).chars.size < 28);
  if (!options.length) return null;
  // préfère les cubes jamais visités
  const seen = ch.seen || [];
  const dest = weightedPick(options, (id) => (seen.includes(id) ? 1 : 3) / dist.get(id));
  const path = buildPath(world, ch, dest, rrange(-1.2, 1.2), rrange(-1.2, 1.2));
  if (!path) return null;
  const c = world.cubes.get(dest);
  return { kind: 'solo', id: 'explore', label: `explore « ${c.name} »`, anim: 'idle', path, wi: 0, phase: 'go', dur: rrange(1, 2), t: 0, gains: { curiosity: 0.2 }, travel: true };
}
function findFriendElsewhere(world, ch) {
  const dist = cubeDistances(world, ch.cubeId, 5);
  let best = null, bs = 0;
  for (const id in ch.rel) {
    const r = ch.rel[id]; if (r.a < 30) continue;
    const o = world.chars.get(id); if (!o || o.cubeId === ch.cubeId || !dist.has(o.cubeId)) continue;
    const s = r.a / (1 + dist.get(o.cubeId));
    if (s > bs) { bs = s; best = o; }
  }
  return best;
}
function visitAct(world, ch, friend) {
  const path = buildPath(world, ch, friend.cubeId, clamp(friend.x + rrange(-0.4, 0.4), -1.6, 1.6), clamp(friend.z + rrange(-0.4, 0.4), -1.6, 1.6));
  if (!path) return null;
  return {
    kind: 'solo', id: 'visit', label: `rend visite à ${friend.name}`, anim: 'wave', path, wi: 0, phase: 'go', dur: 1.5, t: 0, travel: true,
    onDone: (w, c) => {
      c.focus = friend.id; c.focusUntil = w.time + 25;
      if (friend.cubeId === c.cubeId) { bubble(c, '👋'); bubble(friend, '😊'); remember(w, c, `A rendu visite à ${friend.name}`, 1, friend.id); w.addLog('visite', '🚪', `${c.name} rend visite à ${friend.name}`, c.cubeId, [c.id, friend.id], false); }
    },
  };
}
function orderAct(world, ch, cube, table) {
  const aff = OBJECT_TYPES.table.acts[0];
  const act = makeObjAct(world, ch, cube, table, aff, { id: 'order', label: 'attend son plat', anim: 'sit', dur: 25 });
  if (!act) return null;
  act.onArrive = (w, c) => { c.waitingFood = true; bubble(c, '🍽️', 3); };
  act.onDone = (w, c) => { c.waitingFood = false; feel(c, 'sadness', 0.1); bubble(c, '😕'); };
  act.noGains = true;
  return act;
}
function serveAct(world, ch, target) {
  target.servedBy = ch.id;
  return {
    servedTarget: target.id, kind: 'solo', id: 'serve', label: `sert un plat à ${target.name}`, anim: 'give', phase: 'go', wi: 0, dur: 1.4, t: 0, lock: true,
    path: [{ cube: target.cubeId, x: clamp(target.x + 0.25, -1.7, 1.7), z: clamp(target.z + 0.25, -1.7, 1.7), kind: 'walk' }],
    faceChar: target.id,
    onDone: (w, c) => {
      target.servedBy = null;
      if (!target.waitingFood || target.cubeId !== c.cubeId) return;
      const it = takeItem(c, (x) => x === 'plat'); if (!it) return;
      target.waitingFood = false;
      // le client mange tout de suite à sa table
      const a = target.act;
      if (a) { a.id = 'eatTable'; a.label = 'mange à table'; a.anim = 'eat'; a.dur = rrange(8, 12); a.t = 0; a.noGains = false; a.food = 0.8; a.onDone = null; }
      relate(w, target, c, 7, 4); feel(target, 'joy', 0.3); feel(c, 'joy', 0.15); gainSkill(c, 'cuisine', 0.2); gainSkill(c, 'social', 0.1);
      bubble(target, '😋'); bubble(c, '🍲');
      remember(w, target, `${c.name} lui a servi un bon plat`, 1, c.id);
      w.addLog('restaurant', '🍲', `${c.name} sert un plat à ${target.name}`, c.cubeId, [c.id, target.id], false);
    },
  };
}

// ---------- interactions à deux ----------
const JOINT = {
  wave: { label: 'fait coucou à', dist: 1.0, dur: [2, 3], anim: ['coucou', 'wave'], gains: { social: 0.08 }, aff: 2, fam: 4 },
  handshake: { label: 'serre la main de', dist: 0.3, dur: [2, 3], anim: ['handshake', 'handshake'], gains: { social: 0.15 }, aff: 6, fam: 6, skill: 'social' },
  talk: { label: 'discute avec', dist: 0.38, dur: [5, 9], anim: ['talk', 'talk'], gains: { social: 0.35, curiosity: 0.05 }, aff: 5, fam: 5, skill: 'social' },
  joke: { label: 'raconte une blague à', dist: 0.38, dur: [4, 6], anim: ['talk', 'laugh'], gains: { social: 0.25, fun: 0.2 }, aff: 6, fam: 4 },
  hug: { label: 'fait un câlin à', dist: 0.22, dur: [3, 4], anim: ['hug', 'hug'], gains: { social: 0.35, comfort: 0.1 }, aff: 7, fam: 4 },
  danceDuo: { label: 'danse avec', dist: 0.45, dur: [6, 10], anim: ['dance', 'dance'], gains: { fun: 0.35, social: 0.2 }, aff: 6, fam: 4, skill: 'musique' },
  gift: { label: 'offre un cadeau à', dist: 0.3, dur: [2.5, 3], anim: ['give', 'cheer'], gains: { social: 0.25 }, aff: 12, fam: 4 },
  argue: { label: 'se dispute avec', dist: 0.42, dur: [4, 6], anim: ['argue', 'argue'], gains: { social: 0.05 }, aff: -9, fam: 3 },
  fight: { label: 'se bagarre avec', dist: 0.3, dur: [6, 6], anim: ['fight', 'fight'], gains: {}, aff: -10, fam: 3, custom: true },
  chase: { label: 'joue à chat avec', dist: 0.5, dur: [10, 10], anim: ['run', 'run'], gains: { fun: 0.4, social: 0.15 }, aff: 5, fam: 3, custom: true, skill: 'sport' },
  hide: { label: 'joue à cache-cache avec', dist: 0.6, dur: [16, 16], anim: ['seek', 'hide'], gains: { fun: 0.45, social: 0.15 }, aff: 5, fam: 3, custom: true },
  ball: { label: 'joue au ballon avec', dist: 1.2, dur: [6, 9], anim: ['throw', 'catch'], gains: { fun: 0.4, social: 0.15 }, aff: 5, fam: 3, custom: true, skill: 'sport' },
};

function jointAct(world, ch, o, type) {
  if (!available(o) || o.cubeId !== ch.cubeId) return null;
  const J = JOINT[type];
  // acceptation par le partenaire selon personnalité, relation et humeur
  const r = o.rel[ch.id]; const a = r ? r.a : 0;
  let accept = 0.3 + o.traits.sociability * 0.45 + a / 220 + urgency(o.needs.social) * 0.25;
  if (type === 'argue' || type === 'fight') accept = 0.45 + o.traits.temper * 0.5 - o.traits.kindness * 0.2;
  if (o.anim === 'sleep') accept = 0;
  if (rand() > accept) {
    bubble(o, o.traits.sociability < 0.35 ? '😳' : '🙅', 1.8);
    if (type !== 'argue' && type !== 'fight') { feel(ch, 'sadness', 0.06); bubble(ch, '😕', 1.5); }
    return soloAct('rejected', `${o.name} ne veut pas`, 'idle', 1.2, { faceChar: o.id });
  }
  const dur = rrange(J.dur[0], J.dur[1]);
  const lead = {
    kind: 'joint', type, id: type, label: `${J.label} ${o.name}`, role: 'lead', partner: o.id, phase: 'go', t: 0, dur, J, lock: true,
    run: type === 'chase', stage: 0, timer: 0,
  };
  o.act = { kind: 'joint', type, id: type, label: `${J.label.replace(/ à$| avec$| de$/, '')} avec ${ch.name}`, role: 'follow', partner: ch.id, phase: 'wait', t: 0, dur: 20, lock: true, faceChar: ch.id };
  if (type === 'gift') o.act.label = `reçoit un cadeau de ${ch.name}`;
  if (type === 'wave') o.act.label = `répond au coucou de ${ch.name}`;
  setAnim(o, 'idle');
  return lead;
}

function endJoint(world, lead, follower, cancelled) {
  lead.act = null; if (follower && follower.act && follower.act.partner === lead.id) follower.act = null;
  if (cancelled) return;
}

function jointOutcome(world, a, b, type, J) {
  applyGains(a, J.gains); applyGains(b, J.gains);
  if (J.skill) { gainSkill(a, J.skill, 0.12); gainSkill(b, J.skill, 0.12); }
  recordHabit(world, a, type);
  let aff = J.aff * (0.7 + rand() * 0.6);
  if (type === 'talk') {
    // une discussion peut bien ou mal tourner selon la compatibilité
    const c = compat(a, b);
    if (rand() > 0.35 + c * 0.6) { aff = -4; bubble(a, '😤'); feel(a, 'anger', 0.15); } else { bubble(b, pick(['😄', '💬', '🙂'])); }
  }
  relate(world, a, b, aff, J.fam);
  if (aff > 0) { feel(a, 'joy', 0.12); feel(b, 'joy', 0.12); }
  if (type === 'argue') { feel(a, 'anger', 0.35 * (0.5 + a.traits.temper)); feel(b, 'anger', 0.35 * (0.5 + b.traits.temper)); bubble(a, '💢'); bubble(b, '💢'); remember(world, a, `S'est disputé avec ${b.name}`, -1, b.id); remember(world, b, `S'est disputé avec ${a.name}`, -1, a.id); if (chance(0.25)) world.addLog('social', '💢', `${a.name} et ${b.name} se disputent`, a.cubeId, [a.id, b.id], false); }
  if (type === 'hug') { bubble(a, '🤗'); bubble(b, '❤️'); remember(world, b, `Câlin de ${a.name}`, 1, a.id); }
  if (type === 'handshake' && getRel(a, b.id).a < 0) { bubble(a, '🕊️'); remember(world, a, `S'est réconcilié avec ${b.name}`, 1, b.id); world.addLog('social', '🕊️', `${a.name} et ${b.name} se réconcilient`, a.cubeId, [a.id, b.id]); relate(world, a, b, 14, 2, true); }
  if (type === 'gift') {
    const it = takeItem(a, isGift);
    if (it) {
      addItem(world, b, it.type); a.stats.gifts++;
      bubble(b, ITEMS[it.type].icon, 2.5); feel(b, 'joy', 0.4); feel(a, 'joy', 0.25);
      remember(world, b, `A reçu ${ITEMS[it.type].label.toLowerCase()} de ${a.name}`, 1, a.id);
      remember(world, a, `A offert ${ITEMS[it.type].label.toLowerCase()} à ${b.name}`, 1, b.id);
      world.addLog('social', '🎁', `${a.name} offre ${ITEMS[it.type].icon} à ${b.name}`, a.cubeId, [a.id, b.id], false);
    }
  }
  if (type === 'joke') { bubble(b, '😂'); feel(b, 'joy', 0.25); }
}

// Pilotage des interactions à deux (exécuté par le meneur).
function runJoint(world, ch, act, dt) {
  const o = world.chars.get(act.partner);
  if (!o || !o.act || o.act.partner !== ch.id || o.cubeId !== ch.cubeId) { ch.act = null; if (o && o.act && o.act.partner === ch.id) o.act = null; return; }
  const J = act.J; const fa = o.act;
  if (act.phase === 'go') {
    const dx = o.x - ch.x, dz = o.z - ch.z; const d = Math.hypot(dx, dz);
    act.t += dt;
    if (d <= J.dist + 0.04 || act.t > 12) {
      if (act.t > 12 && d > J.dist + 0.5) { endJoint(world, ch, o, true); return; }
      act.phase = 'do'; act.t = 0; fa.phase = 'do'; fa.t = 0;
      if (J.custom) startCustom(world, ch, o, act);
      return;
    }
    // le meneur s'approche du partenaire
    const tx = o.x - (dx / d) * J.dist, tz = o.z - (dz / d) * J.dist;
    const spd = (act.run ? 1.35 : 0.6) * ch.speedMul;
    setAnim(ch, act.run ? 'run' : 'walk'); turnTo(ch, tx, tz, dt, 10);
    const m = Math.min(Math.hypot(tx - ch.x, tz - ch.z), spd * dt);
    const l = Math.hypot(tx - ch.x, tz - ch.z) || 1;
    physicalMove(world, ch, ((tx - ch.x) / l) * m, ((tz - ch.z) / l) * m);
    turnTo(o, ch.x, ch.z, dt, 6); setAnim(o, act.type === 'chase' ? 'idle' : 'idle');
    return;
  }
  act.t += dt;
  if (J.custom) { if (tickCustom(world, ch, o, act, dt)) { jointOutcome(world, ch, o, act.type, J); endJoint(world, ch, o); } return; }
  setAnim(ch, J.anim[0]); setAnim(o, J.anim[1]);
  turnTo(ch, o.x, o.z, dt); turnTo(o, ch.x, ch.z, dt);
  if (act.type === 'joke' && act.t > act.dur * 0.55) { setAnim(ch, 'laugh'); }
  if (act.t >= act.dur) { jointOutcome(world, ch, o, act.type, J); endJoint(world, ch, o); }
}

function startCustom(world, a, b, act) {
  if (act.type === 'hide') {
    // le partenaire choisit une cachette derrière un objet
    const cube = world.cubes.get(a.cubeId);
    const o = pick(cube.objects);
    const ang = Math.atan2(o.x - a.x, o.z - a.z);
    const r = OBJECT_TYPES[o.type].r + 0.15;
    act.spot = [clamp(o.x + Math.sin(ang) * r, -1.75, 1.75), clamp(o.z + Math.cos(ang) * r, -1.75, 1.75)];
    act.search = [[rrange(-1.5, 1.5), rrange(-1.5, 1.5)], [rrange(-1.5, 1.5), rrange(-1.5, 1.5)], act.spot];
    bubble(a, '🙈', 3);
  }
  if (act.type === 'chase') { bubble(a, '😆'); bubble(b, '😱'); act.flee = [rrange(-1.6, 1.6), rrange(-1.6, 1.6)]; }
  if (act.type === 'ball') {
    if (!hasItem(a, (x) => x === 'ballon') && !hasItem(b, (x) => x === 'ballon')) addItem(world, a, 'ballon');
    act.throws = 4 + Math.floor(rand() * 5); act.holder = a.id; act.timer = 0.6;
  }
  if (act.type === 'fight') {
    const loserIsA = (a.skills.sport || 0) + rand() * 3 < (b.skills.sport || 0) + rand() * 3;
    act.loser = loserIsA ? a.id : b.id;
    world.fx.push({ type: 'dust', cube: a.cubeId, x: (a.x + b.x) / 2, z: (a.z + b.z) / 2, t: 0, dur: 3.2 });
    a.stats.fights++; b.stats.fights++;
  }
}

function moveToward(world, ch, tx, tz, spd, dt) {
  const dx = tx - ch.x, dz = tz - ch.z; const d = Math.hypot(dx, dz);
  if (d < 0.05) return true;
  turnTo(ch, tx, tz, dt, 10);
  const m = Math.min(d, spd * dt);
  physicalMove(world, ch, (dx / d) * m, (dz / d) * m);
  return d < 0.08;
}

// Retourne true quand l'interaction est terminée.
function tickCustom(world, a, b, act, dt) {
  const t = act.t;
  switch (act.type) {
    case 'chase': {
      if (act.caught) { setAnim(a, 'laugh'); setAnim(b, 'laugh'); turnTo(a, b.x, b.z, dt); turnTo(b, a.x, a.z, dt); return t > act.caught + 2.5; }
      if (moveToward(world, b, act.flee[0], act.flee[1], 1.25 * b.speedMul, dt)) act.flee = [rrange(-1.6, 1.6), rrange(-1.6, 1.6)];
      setAnim(b, 'run');
      moveToward(world, a, b.x, b.z, 1.3 * a.speedMul, dt); setAnim(a, 'run');
      if (Math.hypot(a.x - b.x, a.z - b.z) < 0.24 || t > 12) { act.caught = t; bubble(a, '✋'); bubble(b, '😂'); }
      return false;
    }
    case 'hide': {
      if (act.found) { setAnim(a, 'laugh'); setAnim(b, 'laugh'); turnTo(a, b.x, b.z, dt); turnTo(b, a.x, a.z, dt); return t > act.found + 2.5; }
      if (t < 4) { setAnim(a, 'seek'); } // compte en se cachant les yeux
      // le cacheur court à sa cachette puis s'accroupit
      if (moveToward(world, b, act.spot[0], act.spot[1], 1.2 * b.speedMul, dt)) { setAnim(b, 'hide'); } else setAnim(b, 'run');
      if (t >= 4) {
        const p = act.search[0];
        setAnim(a, 'walk');
        if (moveToward(world, a, p[0], p[1], 0.6 * a.speedMul, dt)) {
          act.search.shift(); if (act.search.length) bubble(a, '🤔', 1);
        }
        if (!act.search.length || Math.hypot(a.x - b.x, a.z - b.z) < 0.35 || t > 22) { act.found = t; bubble(a, '👀'); bubble(b, '😆'); }
      }
      return false;
    }
    case 'ball': {
      turnTo(a, b.x, b.z, dt); turnTo(b, a.x, a.z, dt);
      act.timer -= dt;
      const thrower = act.holder === a.id ? a : b; const catcher = thrower === a ? b : a;
      if (act.timer <= 0) {
        if (act.throws <= 0) return true;
        act.throws--;
        setAnim(thrower, 'throw'); setAnim(catcher, 'catch');
        world.fx.push({ type: 'ball', cube: a.cubeId, from: [thrower.x, thrower.z], to: [catcher.x, catcher.z], t: 0, dur: 0.9 });
        act.holder = catcher.id; act.timer = 1.15;
        if (chance(0.12)) { act.fumble = catcher.id; act.timer = 2.2; }
      } else if (act.fumble && act.timer < 1.3) {
        const c = world.chars.get(act.fumble); setAnim(c, 'fall'); bubble(c, '😵', 1.2); bubble(c === a ? b : a, '😂', 1.2); act.fumble = null;
      } else if (act.timer < 0.25 && thrower.anim === 'fall') setAnim(thrower, 'getup');
      return false;
    }
    case 'fight': {
      const loser = act.loser === a.id ? a : b; const winner = loser === a ? b : a;
      if (t < 3.2) { setAnim(a, 'fight'); setAnim(b, 'fight'); turnTo(a, b.x, b.z, dt); turnTo(b, a.x, a.z, dt); return false; }
      if (!act.resolved) {
        act.resolved = true;
        bubble(winner, '😤'); bubble(loser, '💫');
        feel(loser, 'sadness', 0.35); feel(loser, 'anger', 0.2); feel(winner, 'anger', -0.2);
        remember(world, loser, `A perdu une bagarre contre ${winner.name}`, -1, winner.id);
        remember(world, winner, `A gagné une bagarre contre ${loser.name}`, 0, loser.id);
        world.addLog('social', '💥', `Bagarre cartoon entre ${a.name} et ${b.name} (${winner.name} l'emporte)`, a.cubeId, [a.id, b.id]);
      }
      if (t < 4.4) { setAnim(loser, 'fall'); setAnim(winner, 'cheer'); return false; }
      if (t < 5.4) { setAnim(loser, 'getup'); return false; }
      setAnim(loser, 'sulk'); setAnim(winner, 'idle');
      return t > 6.5;
    }
  }
  return true;
}

// ---------- exécution ----------
function finishAct(world, ch, act) {
  if (act.obj) {
    const c = world.cubes.get(act.objCube); const o = c && c.objects.find((x) => x.id === act.obj);
    if (o) users(o).delete(ch.id);
  }
  ch.y = 0;
  ch.act = null;
}

function completeAct(world, ch, act) {
  const aff = act.aff;
  if (!act.noGains) {
    if (act.gains) applyGains(ch, act.gains);
    if (aff) applyGains(ch, aff.gains);
    if (act.food) ch.needs.hunger = clamp(ch.needs.hunger + act.food, 0, 1);
  }
  if (act.skill) gainSkill(ch, act.skill, act.dur * 0.012);
  if (aff && aff.skill) gainSkill(ch, aff.skill, act.dur * 0.015);
  recordHabit(world, ch, act.id);
  const cube = world.cubes.get(act.objCube || ch.cubeId);
  const o = act.obj && cube ? cube.objects.find((x) => x.id === act.obj) : null;
  if (aff && aff.produce && !act.noGains && rand() < aff.produceChance) {
    addItem(world, ch, aff.produce);
    bubble(ch, ITEMS[aff.produce].icon, 2.2); feel(ch, 'joy', 0.2);
    if (['poisson', 'cristal', 'potion', 'maquette'].includes(aff.produce)) {
      remember(world, ch, `A obtenu : ${ITEMS[aff.produce].label.toLowerCase()}`, 1);
      world.addLog('objet', ITEMS[aff.produce].icon, `${ch.name} ${aff.label} et obtient ${ITEMS[aff.produce].label.toLowerCase()}`, ch.cubeId, [ch.id], false);
    }
  }
  if (act.id === 'eatTable' && o) o.state.dirty = true;
  if (act.id === 'clean' && o) { o.state.dirty = false; bubble(ch, '✨'); }
  if (act.id === 'experiment' && rand() < 0.12) {
    world.fx.push({ type: 'smoke', cube: ch.cubeId, x: ch.x, z: ch.z, t: 0, dur: 2.5 });
    feel(ch, 'surprise', 0.8); bubble(ch, '💥', 2);
    world.addLog('accident', '💥', `Petite explosion au labo ! ${ch.name} est tout ébouriffé`, ch.cubeId, [ch.id]);
    remember(world, ch, 'A fait exploser une expérience', 0);
    ch.act = { kind: 'solo', id: 'oops', label: 'est tombé à la renverse', anim: 'fall', dur: 1.3, t: 0, phase: 'do', next: soloAct('getup', 'se relève', 'getup', 1) };
    if (o) users(o).delete(ch.id);
    ch.y = 0;
    return;
  }
  if (act.onDone) act.onDone(world, ch, act);
  if (ch.act === act) {
    finishAct(world, ch, act);
    if (act.next) ch.act = act.next;
  } else if (act.obj) {
    const c = world.cubes.get(act.objCube); const oo = c && c.objects.find((x) => x.id === act.obj); if (oo) users(oo).delete(ch.id);
  }
}

export function abortAct(world, ch) {
  const act = ch.act; if (!act) return;
  if (act.kind === 'joint') { const o = world.chars.get(act.partner); if (o && o.act && o.act.partner === ch.id) o.act = null; }
  if (act.servedTarget) { const t = world.chars.get(act.servedTarget); if (t) t.servedBy = null; }
  ch.waitingFood = false;
  finishAct(world, ch, act);
}

export function updateChar(world, ch, dt) {
  decay(world, ch, dt);
  ch.animT += dt; ch.blend = Math.min(1, ch.blend + dt * 5);
  if (!ch.act) {
    ch.act = decide(world, ch);
    if (ch.act && ch.act.obj) { const c = world.cubes.get(ch.act.objCube); const o = c && c.objects.find((x) => x.id === ch.act.obj); if (o) users(o).add(ch.id); }
  }
  const act = ch.act; if (!act) return;

  if (act.kind === 'joint') {
    if (act.role === 'lead') return runJoint(world, ch, act, dt);
    // suiveur : attend le meneur
    act.t += dt;
    const lead = world.chars.get(act.partner);
    if (!lead || !lead.act || lead.act.partner !== ch.id || act.t > 40) { ch.act = null; return; }
    if (act.phase === 'wait') { setAnim(ch, 'idle'); turnTo(ch, lead.x, lead.z, dt, 5); }
    return;
  }
  if (act.kind === 'event') return; // piloté par le gestionnaire d'événements

  if (act.phase === 'go') {
    const r = followPath(world, ch, act, dt);
    if (r === 'fail') { bubble(ch, '❓', 1.5); abortAct(world, ch); return; }
    if (r === 'done') {
      act.phase = 'do'; act.t = 0;
      if (act.seatY !== undefined) ch.y = act.seatY;
      if (act.faceR !== null && act.faceR !== undefined) ch.ry = act.faceR;
      if (act.onArrive) act.onArrive(world, ch, act);
    }
    return;
  }
  // phase 'do'
  act.t += dt;
  setAnim(ch, act.anim);
  if (act.face) turnTo(ch, act.face[0], act.face[1], dt, 6);
  if (act.faceChar) { const o = world.chars.get(act.faceChar); if (o && o.cubeId === ch.cubeId) turnTo(ch, o.x, o.z, dt, 6); }
  if (act.move) physicalMove(world, ch, Math.sin(ch.ry) * act.move * dt / act.dur, Math.cos(ch.ry) * act.move * dt / act.dur);
  if (act.anim === 'sleep' && ch.needs.energy > 0.98 && act.t > 8) act.t = act.dur;
  if (act.ball && Math.floor(act.t / 0.8) !== Math.floor((act.t - dt) / 0.8)) world.fx.push({ type: 'juggle', cube: ch.cubeId, ch: ch.id, t: 0, dur: 0.8 });
  if (act.t >= act.dur) completeAct(world, ch, act);
}

// ---------- simulation simplifiée (cubes en veille) ----------
// Pas de déplacement visuel : besoins, activités "virtuelles", relations et migrations statistiques.
export function coarseUpdate(world, cube, dt) {
  const ids = [...cube.chars];
  for (const id of ids) {
    const ch = world.chars.get(id); if (!ch) continue;
    if (ch.act) abortAct(world, ch);
    decay(world, ch, dt);
    // posture de repos visible de loin
    ch.anim = world.isNight && ch.needs.energy < 0.6 ? 'sleep' : pick(['idle', 'idle', 'sitFloor', 'talk', 'read', 'idle']); ch.blend = 1;
    // satisfait le besoin le plus urgent avec un objet du cube, en accéléré
    let worst = 'fun', wv = 2;
    for (const k in ch.needs) if (ch.needs[k] < wv) { wv = ch.needs[k]; worst = k; }
    const affs = [];
    for (const o of cube.objects) for (const a of OBJECT_TYPES[o.type].acts) if ((a.gains[worst] || 0) > 0 && !a.req) affs.push(a);
    const p = Math.min(1, dt / 14);
    if (affs.length && rand() < p * 1.6) {
      const a = pick(affs); applyGains(ch, a.gains, 0.9); recordHabit(world, ch, a.id);
      if (a.skill) gainSkill(ch, a.skill, 0.12);
      if (a.produce && rand() < a.produceChance * 0.5) addItem(world, ch, a.produce);
    } else if (worst === 'hunger' && rand() < p) ch.needs.hunger = clamp(ch.needs.hunger + 0.4, 0, 1);
    else if (worst === 'energy' && rand() < p) ch.needs.energy = clamp(ch.needs.energy + 0.5, 0, 1);
    else if (worst === 'social' || worst === 'fun') { if (rand() < p) ch.needs[worst] = clamp(ch.needs[worst] + 0.25, 0, 1); }
    else if (rand() < p) ch.needs[worst] = clamp(ch.needs[worst] + 0.3, 0, 1);
    // rencontres
    if (ids.length > 1 && rand() < p * (0.5 + ch.traits.sociability)) {
      const o = world.chars.get(pick(ids));
      if (o && o !== ch) {
        const r = ch.rel[o.id]; const a = r ? r.a : 0;
        const good = rand() < 0.55 + compat(ch, o) * 0.4 - ch.traits.temper * 0.15 + a / 300;
        relate(world, ch, o, good ? rrange(2, 7) : -rrange(2, 6), 3);
        ch.needs.social = clamp(ch.needs.social + 0.2, 0, 1);
      }
    }
    // migration vers un voisin (curiosité / amis)
    if (rand() < p * 0.15 * (0.3 + ch.traits.curiosity)) {
      const nbs = world.neighbors(cube);
      if (nbs.length) {
        const [, dest] = pick(nbs);
        if (world.transfer(ch, dest)) { ch.x = clamp(ch.x, -1.7, 1.7); ch.z = clamp(ch.z, -1.7, 1.7); ch.y = 0; }
      }
    }
  }
}

export { bubble, addItem, recordHabit, applyGains, gainSkill, finishAct, physicalMove, moveToward, turnTo, followPath, hasItem, isGift, takeItem, available };
