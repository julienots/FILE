// Nuit Néon — boucle de jeu, conduite, adversaires, modes de jeu et menus.
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { ShaderPass } from 'three/addons/postprocessing/ShaderPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { TRACKS, ROUTE_LIBRE } from './tracks.js';
import { buildWorld, makeTrack, canvasTex } from './world.js';
import { buildArena, ARENA, TEAM_COL } from './arena.js';
import { CARS, STAT_KEYS, PAINTS, physFromStats, createCar, resolveModels, setPaint, animateCar, disposeCar } from './cars.js';
import { TRAFIC, TRAFIC_PAINTS } from './carlist.js';
import { FX } from './fx.js';
import * as audio from './audio.js';

const $ = (id) => document.getElementById(id);
const clamp = THREE.MathUtils.clamp;
const wrap = (a) => { while (a > Math.PI) a -= 2 * Math.PI; while (a < -Math.PI) a += 2 * Math.PI; return a; };
const store = {
  get(k, d) { try { const v = localStorage.getItem('nn_' + k); return v ? JSON.parse(v) : d; } catch (e) { return d; } },
  set(k, v) { try { localStorage.setItem('nn_' + k, JSON.stringify(v)); } catch (e) { /* stockage indisponible */ } },
};
const params = new URLSearchParams(location.search);

// ---------- réglages sauvegardés ----------
// essai rapide d'un modèle sans toucher à la liste : ?modele=models/fichier.glb (et &rotY=3.14 si besoin)
if (params.get('modele')) CARS.unshift({ id: 'essai', nom: 'Essai', modele: params.get('modele'), rotY: params.has('rotY') ? +params.get('rotY') : undefined, couleur: 0xb9bec9, stats: { vmax: 7, accel: 7, mania: 6, adher: 6 } });
const tuning = store.get('tuning', {});
const opts = Object.assign({ qual: 'auto', autoGas: false, son: true }, store.get('opts', {}));
if (opts.v !== 2) { opts.autoGas = false; opts.v = 2; }
const setup = Object.assign({ track: 0, diff: 1, laps: 3, bots: 7, team: 2, dur: 180 }, store.get('setup', {}));
const best = store.get('best3', {}); // records (route élargie)
let selCar = params.get('modele') ? 0 : Math.max(0, CARS.findIndex(c => c.id === store.get('car', CARS[0].id)));
if (params.get('q')) opts.qual = params.get('q');
setup.track = clamp(setup.track | 0, 0, TRACKS.length - 1);
const carStats = (def) => Object.assign({}, def.stats, (tuning[def.id] || {}).stats);
const carColor = (def) => (tuning[def.id] && tuning[def.id].couleur !== undefined) ? tuning[def.id].couleur : def.couleur;
const DIFFS = [
  { nom: 'Facile', speed: 0.80, corner: 0.60 }, { nom: 'Normal', speed: 0.88, corner: 0.74 },
  { nom: 'Difficile', speed: 0.95, corner: 0.87 }, { nom: 'Expert', speed: 1.0, corner: 0.97 },
];
const BOT_NAMES = ['Kaïto', 'Luna', 'Rex', 'Mila', 'Zéphyr', 'Nova', 'Axel', 'Iris', 'Oslo', 'Vega', 'Niko', 'Jade'];
const ITEM_NAMES = { turbo: 'Turbo', missile: 'Missile', huile: "Flaque d'huile", bouclier: 'Bouclier' };

// ---------- rendu ----------
const app = $('app'), canvas = $('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance', preserveDrawingBuffer: params.has('shot') });
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 0.95;
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(55, 1, 0.1, 2500);
camera.layers.enable(1);
const fx = new FX(scene);

const QUALITY = {
  haute: { pr: 2, refl: 0.55, bloom: true, msaa: 4, taps: 9, env: 256, rain: true, grass: 4200, terrain: 170 },
  moyenne: { pr: 1.5, refl: 0.45, bloom: true, msaa: 0, taps: 7, env: 128, rain: true, grass: 2600, terrain: 140 },
  basse: { pr: 1, refl: 0.33, bloom: false, msaa: 0, taps: 5, env: 64, rain: false, grass: 800, terrain: 90 },
};
const isTouch = matchMedia('(pointer: coarse)').matches;
let qName = 'moyenne', Q = QUALITY.moyenne, dyn = 1;
function pickQuality() { qName = opts.qual === 'auto' ? (isTouch ? 'moyenne' : 'haute') : (QUALITY[opts.qual] ? opts.qual : 'moyenne'); Q = QUALITY[qName]; dyn = 1; }
pickQuality();

let composer = null, finalPass = null;
function setupPost() {
  if (composer) { composer.dispose(); composer.renderTarget1.dispose(); composer.renderTarget2.dispose(); composer = null; }
  if (!Q.bloom) return;
  const rt = new THREE.WebGLRenderTarget(4, 4, { type: THREE.HalfFloatType, samples: Q.msaa });
  composer = new EffectComposer(renderer, rt);
  composer.addPass(new RenderPass(scene, camera));
  composer.addPass(new UnrealBloomPass(new THREE.Vector2(256, 256), 0.42, 0.6, 1.0));
  finalPass = new ShaderPass({
    uniforms: { tDiffuse: { value: null }, amt: { value: 0 }, time: { value: 0 } },
    vertexShader: 'varying vec2 vUv; void main(){ vUv=uv; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.); }',
    fragmentShader: `uniform sampler2D tDiffuse; uniform float amt; uniform float time; varying vec2 vUv;
      float hsh(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
      void main(){ vec2 c = vec2(0.5,0.56); vec2 d = vUv - c; float r = length(d*vec2(1.0,1.25));
        float k = smoothstep(0.28, 0.85, r) * 0.05 * amt;
        vec3 col = vec3(0.); for(int i=0;i<8;i++){ float t = float(i)/7.; col += texture2D(tDiffuse, vUv - d*k*t).rgb; } col /= 8.;
        col.r = mix(col.r, texture2D(tDiffuse, vUv - d*0.006*r).r, 0.7); col.b = mix(col.b, texture2D(tDiffuse, vUv + d*0.006*r).b, 0.7);
        col *= 1.0 - 0.45*smoothstep(0.4, 1.0, r);
        col *= 1.0 + (hsh(vUv*731.0 + fract(time)*17.0) - 0.5) * 0.07;
        float lum = dot(col, vec3(0.3,0.59,0.11)); col = mix(vec3(lum), col, 1.14); col *= 0.94 + 0.12*smoothstep(0.0, 1.2, lum);   // étalonnage : un peu plus de couleur et de contraste
        gl_FragColor = vec4(col,1.); }`,
  });
  composer.addPass(finalPass);
  composer.addPass(new OutputPass());
}
let W = 1, H = 1;
function applySize() {
  W = Math.max(1, app.clientWidth); H = Math.max(1, app.clientHeight);
  const pr = Math.max(0.5, Math.min(window.devicePixelRatio || 1, Q.pr) * dyn);
  renderer.setPixelRatio(pr); renderer.setSize(W, H, false);
  if (composer) { composer.setPixelRatio(pr); composer.setSize(W, H); }
  if (world && world.road) world.road.getRenderTarget().setSize(Math.max(64, Math.floor(W * pr * Q.refl)), Math.max(64, Math.floor(H * pr * Q.refl)));
  fx.setScale(H * pr);
  camera.aspect = W / H; camera.updateProjectionMatrix();
}
new ResizeObserver(() => applySize()).observe(app);

// ---------- monde ----------
let world = null, T = null, worldKey = '';
function ensureWorld(key) {
  if (worldKey === key) return;
  if (world) { scene.remove(world.group); world.dispose(); }
  fx.clear();
  world = key === 'arena' ? buildArena(renderer, Q) : buildWorld(key === 'libre' ? ROUTE_LIBRE : TRACKS[+key], renderer, Q);
  T = world.T || null; worldKey = key;
  scene.add(world.group); scene.fog = world.fog;
  scene.environment = null;
  scene.environment = world.captureEnv(scene);
  if (world.rain) world.rain.visible = Q.rain;
  if (world.day) fx.setTint(0.82, 0.82, 0.84); else fx.setTint(0.42, 0.42, 0.5);
  applySize();
}

// ---------- état de la partie ----------
// G.type : 'race' (course), 'items' (course à bonus), 'free' (balade libre), 'foot' (foot en voiture), 'demo' (fond du menu)
const G = { type: 'demo', boost: true, drain: 30, regen: 7, score: [0, 0], freeze: 0, timeLeft: 0, boxes: [], shots: [], oils: [], traffic: [], dist: 0, ended: false };
const objs = new THREE.Group(); scene.add(objs);
let cars = [], player = null, raceTime = 0, mode = 'loading', countT = 0, countN = 0, finishOrder = 0;
const tmp = { x: 0, z: 0 };
const ball = { x: 0, y: 2, z: 0, vx: 0, vy: 0, vz: 0, last: null };
function clearGame() {
  for (const c of cars) { scene.remove(c.vis.root); disposeCar(c.vis); }
  for (const t of G.traffic) { scene.remove(t.vis.root); disposeCar(t.vis); }
  cars = []; player = null; G.traffic = [];
  while (objs.children.length) { const o = objs.children.pop(); if (o.geometry && !o.userData.shared) o.geometry.dispose(); }
  G.boxes = []; G.shots = []; G.oils = []; fx.clear();
}
function makeCarEntity(def, paint, isPlayer, name, skill) {
  const vis = createCar(def, paint);
  if (vis.beam) vis.beam.visible = !world.day;
  vis.wet = world.arena ? 0 : (world.theme.wet ?? 1);
  vis.root.rotation.order = 'YXZ';
  const c = { def, vis, phys: physFromStats(carStats(def)), x: 0, z: 0, h: 0, y: 0, vy: 0, pitch: 0, v: 0, u: 0, steer: 0, idx: 0, lap: 0, total: 0,
    lapStart: 0, bestLap: 0, finished: false, finishTime: 0, rank: 0, isPlayer, name, yaw: 0, lon: 0, braking: false, onWall: false, slip: 0,
    boost: 100, boosting: false, turboT: 0, spinT: 0, spinA: 0, shieldT: 0, item: null, team: 0, goals: 0, jumps: 0, prevJump: false, prevUse: false, gear: 0,
    ai: { off: 0, base: 0, t: 1, skill, itemT: 0, role: 'att', jumpT: 0 } };
  scene.add(vis.root); cars.push(c);
  return c;
}
function placeOnGrid(c, slot) {
  const row = Math.floor(slot / 3), col = slot % 3, off = (col - 1) * 4.6;
  const idx = T.N - Math.round((8 + row * 9 + col * 2.2) / T.ds);
  T.point(idx, off, tmp);
  c.x = tmp.x; c.z = tmp.z; c.h = T.heading(idx); c.idx = ((idx % T.N) + T.N) % T.N; c.total = c.idx - T.N; c.lap = 0;
  c.ai.off = c.ai.base = off; c.ai.t = 1 + slot * 0.4;
}
function gridUp(withPlayer, nBots) {
  clearGame();
  const sk = DIFFS[setup.diff], order = [];
  for (let i = 0; i < 24; i++) order.push((selCar + 1 + i) % CARS.length);
  const used = new Set(withPlayer ? [carColor(CARS[selCar])] : []);
  const n = nBots + (withPlayer ? 1 : 0);
  for (let s = 0; s < n; s++) {
    const isP = withPlayer && s === n - 1, di = isP ? selCar : order[s];
    let paint = carColor(CARS[di]);
    if (!isP && used.has(paint)) paint = PAINTS.find(p => !used.has(p)) ?? PAINTS[s % PAINTS.length];
    used.add(paint);
    const c = makeCarEntity(CARS[di], paint, isP, isP ? 'Toi' : BOT_NAMES[s % BOT_NAMES.length], sk);
    placeOnGrid(c, s);
    if (isP) player = c;
  }
  raceTime = 0; finishOrder = 0;
}

// ---------- adversaires en course ----------
function aiInput(c, dt) {
  const P = c.phys, sk = c.ai.skill, N = T.N, ai = c.ai, lim = T.limit;
  ai.t -= dt; if (ai.t < 0) { ai.t = 2 + Math.random() * 4; ai.base = (Math.random() * 2 - 1) * (lim - 2.6); }
  const kA = T.k[(c.idx + Math.round((14 + c.v * 0.5) / T.ds)) % N];
  let target = clamp(ai.base * 0.6 + clamp(kA * 70, -1, 1) * lim * 0.5, -(lim - 1.6), lim - 1.6);
  const myLat = T.lateral(c.x, c.z, c.idx);
  let slow = 0;
  for (const o of cars) {
    if (o === c) continue;
    let d = o.total - c.total; d = ((d % N) + N * 1.5) % N - N / 2;
    const dm = d * T.ds;
    if (dm > -1 && dm < 16) {
      const ol = T.lateral(o.x, o.z, o.idx);
      if (Math.abs(ol - myLat) < 2.6) {
        target = clamp(ol + (myLat >= ol ? 3.4 : -3.4), -(lim - 1.4), lim - 1.4);
        if (Math.abs(target - ol) < 2.4) target = clamp(ol + (ol > 0 ? -3.4 : 3.4), -(lim - 1.4), lim - 1.4);
        if (dm < 6.5 && c.v > o.v) slow = 0.5;
      }
    }
  }
  // évite les flaques d'huile
  for (const o of G.oils) { const d = ((o.idx - c.idx) % N + N) % N; if (d < 14 && Math.abs(o.lat - myLat) < 2.6) target = clamp(o.lat + (myLat >= o.lat ? 3.6 : -3.6), -(lim - 1.4), lim - 1.4); }
  ai.off += clamp(target - ai.off, -2.6 * dt, 2.6 * dt);
  const la = 7 + Math.abs(c.v) * 0.42;
  T.point(c.idx + Math.round(la / T.ds), ai.off, tmp);
  const want = Math.atan2(-(tmp.x - c.x), -(tmp.z - c.z));
  const steer = clamp(-wrap(want - c.h) * 2.4, -1, 1);
  let vT = P.vmax * sk.speed, straight = true;
  const scan = Math.min(110, Math.ceil((c.v * c.v / (2 * P.brake) + 14) / T.ds));
  for (let m = 0; m <= scan; m++) {
    const kk = Math.abs(T.k[(c.idx + m) % N]);
    if (kk < 1e-4) continue;
    const vc2 = P.grip * sk.corner / kk + 2 * P.brake * 0.75 * (m * T.ds);
    if (vc2 < vT * vT) { vT = Math.sqrt(vc2); straight = false; }
  }
  let gas = 1, brake = 0;
  if (c.v > vT + 0.4) { gas = 0; brake = clamp((c.v - vT) / 3, 0.15, 1); } else if (c.v > vT - 0.6) gas = 0.4;
  if (slow) { gas = Math.min(gas, 0.2); brake = Math.max(brake, slow * 0.4); }
  if (ai.stuck > 0) { ai.stuck -= dt; return { steer: -steer, gas: 0, brake: 1 }; }
  if (Math.abs(c.v) < 1.2 && raceTime > 2) { ai.still = (ai.still || 0) + dt; if (ai.still > 1.2) { ai.stuck = 0.9; ai.still = 0; } } else ai.still = 0;
  // boost en ligne droite
  if (straight && gas === 1 && c.boost > 70 && Math.random() < dt * 0.5 * (0.4 + sk.speed)) ai.boostOn = true;
  if (!straight || c.boost < 8) ai.boostOn = false;
  // objets
  let use = false;
  if (c.item) { ai.itemT -= dt; if (ai.itemT <= 0) use = true; }
  return { steer, gas, brake, boost: !!ai.boostOn, use };
}

// ---------- physique d'un véhicule ----------
function stepCar(c, inp, dt) {
  const P = c.phys;
  if (c.spinT > 0) { c.spinT -= dt; c.spinA += dt * 10; inp = { steer: 0, gas: 0, brake: 0.25 }; if (c.spinT <= 0) c.spinA = 0; }
  if (c.shieldT > 0) c.shieldT -= dt;
  const rate = inp.steer === 0 ? 7 : 4.5;
  c.steer += clamp(inp.steer - c.steer, -rate * dt, rate * dt);
  let boosting = false;
  if (c.turboT > 0) { c.turboT -= dt; boosting = true; }
  else if (inp.boost && c.boost > 1 && G.boost) { boosting = true; c.boost = Math.max(0, c.boost - G.drain * dt); }
  else if (G.regen) c.boost = Math.min(100, c.boost + G.regen * dt);
  c.boosting = boosting;
  const vmax = P.vmax * (boosting ? 1.25 : 1), acc = P.accel + (boosting ? 14 : 0), air = c.y > 0.01;
  let v = c.v; const v0 = v;
  const gas = boosting ? 1 : (inp.gas || 0), brake = inp.brake || 0;
  if (!air) {
    if (gas > 0 && v > -0.5) v += acc * gas * (1 - Math.pow(Math.max(0, v) / vmax, 2)) * dt;
    if (brake > 0) { if (v > 0.5) v = Math.max(0, v - P.brake * brake * dt); else v = Math.max(-9, v - 7 * brake * dt); }
    if (gas <= 0 && brake <= 0) { const d = (4.5 + 0.0016 * v * v) * dt; v = Math.abs(v) < d ? 0 : v - Math.sign(v) * d; }
    if (gas > 0 && v < -0.5) v = Math.min(0, v + 14 * dt);
  } else {
    // en l'air : le boost pousse dans l'axe du nez, ce qui permet de voler
    if (boosting) { v = Math.min(P.vmax * 1.3, v + 30 * Math.cos(c.pitch) * dt); c.vy += 30 * Math.sin(c.pitch) * dt; }
    v -= v * 0.12 * dt;
  }
  const sp = Math.abs(v);
  let yaw, excess = 0;
  if (!air) {
    const yawMax = Math.min(P.steer * Math.min(1, sp / 7), P.grip * 1.3 / Math.max(sp, 1));
    yaw = c.steer * yawMax * (v < 0 ? -1 : 1);
    excess = Math.max(0, Math.abs(yaw) * sp - P.grip);
    c.u -= Math.sign(yaw) * excess * 0.9 * dt;
    c.u *= Math.exp(-3.5 * dt);
    v -= Math.sign(v) * excess * 0.25 * dt;
  } else yaw = c.steer * 1.9;
  c.slip = excess;
  c.h = wrap(c.h - yaw * dt);
  const sh = Math.sin(c.h), ch = Math.cos(c.h);
  c.x += (-sh * v + ch * c.u) * dt; c.z += (-ch * v - sh * c.u) * dt;
  c.v = v;
  if (G.type === 'foot') {
    const jd = inp.jump && !c.prevJump; c.prevJump = !!inp.jump;
    if (jd && c.jumps < 2) { c.vy = c.jumps === 0 ? 10 : 8.5; c.jumps++; c.y = Math.max(c.y, 0.02); }
    if (c.y > 0 || c.vy > 0) {
      c.vy -= 24 * dt; c.y += c.vy * dt;
      const tp = inp.jump ? 1.0 : (c.vy < 0 ? -0.2 : 0.12);
      c.pitch += (tp - c.pitch) * Math.min(1, 5 * dt);
      if (c.y > ARENA.ceil - 1.6) { c.y = ARENA.ceil - 1.6; c.vy = Math.min(0, c.vy); }
      if (c.y <= 0) { c.y = 0; c.vy = 0; c.pitch = 0; c.jumps = 0; }
    }
    arenaConstrain(c, dt);
  } else trackConstrain(c, dt);
  c.lon = (c.v - v0) / dt; c.yaw = yaw; c.braking = brake > 0.05 && v0 > 0.5;
}
function hitWall(c, nx, nz, dt, sp) {
  // nx, nz : normale du mur, pointant vers l'extérieur du terrain
  const fx_ = -Math.sin(c.h) * Math.sign(c.v || 1), fz_ = -Math.cos(c.h) * Math.sign(c.v || 1);
  const into = fx_ * nx + fz_ * nz;
  if (!c.onWall && into > 0) { c.v *= (1 - 0.5 * into); c.bump = into * Math.min(1, sp / 30); }
  c.v *= Math.exp(-0.8 * dt);
  if (into > 0) {
    let tx = fx_ - nx * into, tz = fz_ - nz * into; const tl = Math.hypot(tx, tz);
    if (tl > 0.05) { tx /= tl; tz /= tl; const th = Math.atan2(-tx * Math.sign(c.v || 1), -tz * Math.sign(c.v || 1)); c.h = wrap(c.h + wrap(th - c.h) * Math.min(1, 3.5 * dt)); }
  }
  c.u = 0; c.onWall = true;
}
function trackConstrain(c, dt) {
  const prev = c.idx, N = T.N, sp = Math.abs(c.v);
  c.idx = T.nearest(c.x, c.z, c.idx);
  const d = T.lateral(c.x, c.z, c.idx), i = c.idx;
  c.offroad = false;
  if (Math.abs(d) > T.limit) {
    const s = Math.sign(d), push = Math.abs(d) - T.limit;
    c.x -= -T.tz[i] * s * push; c.z -= T.tx[i] * s * push;
    hitWall(c, -T.tz[i] * s, T.tx[i] * s, dt, sp);
  } else {
    c.onWall = false;
    if (T.libre && Math.abs(d) > T.half + 0.4) { c.offroad = true; if (sp > 14) c.v -= c.v * 1.3 * dt; } // l'herbe freine
  }
  if (T.libre) { if (prev !== c.idx) { let dd = c.idx - prev; if (dd > N / 2) dd -= N; if (dd < -N / 2) dd += N; G.dist += Math.abs(dd) * T.ds; } return; }
  if (prev > N * 0.75 && c.idx < N * 0.25) { c.lap++; onLap(c); }
  else if (prev < N * 0.25 && c.idx > N * 0.75) c.lap--;
  c.total = (c.lap - 1) * N + c.idx;
}
function arenaConstrain(c, dt) {
  const A = ARENA, m = 1.4, sp = Math.abs(c.v);
  const inGoal = Math.abs(c.x) < A.goalW - 1.3 && c.y < A.goalH - 2;
  const zmax = inGoal ? A.hz + A.goalD - 1.6 : A.hz - m;
  let hit = false;
  if (Math.abs(c.x) > A.hx - m) { const s = Math.sign(c.x); c.x = s * (A.hx - m); hitWall(c, s, 0, dt, sp); hit = true; }
  if (Math.abs(c.z) > zmax) { const s = Math.sign(c.z); c.z = s * zmax; hitWall(c, 0, s, dt, sp); hit = true; }
  if (Math.abs(c.z) > A.hz - m && Math.abs(c.x) > A.goalW - 1.3 && inGoal === false && Math.abs(c.z) > A.hz) { c.x = Math.sign(c.x) * (A.goalW - 1.3); }
  if (!hit) c.onWall = false;
}
function collide(list) {
  for (let i = 0; i < list.length; i++) for (let j = i + 1; j < list.length; j++) {
    const a = list[i], b = list[j];
    if (Math.abs(a.x - b.x) > 6 || Math.abs(a.z - b.z) > 6 || Math.abs(a.y - b.y) > 1.8) continue;
    for (const fa of [-1.15, 1.15]) for (const fb of [-1.15, 1.15]) {
      const ax = a.x - Math.sin(a.h) * fa, az = a.z - Math.cos(a.h) * fa, bx = b.x - Math.sin(b.h) * fb, bz = b.z - Math.cos(b.h) * fb;
      const dx = bx - ax, dz = bz - az, d = Math.hypot(dx, dz);
      if (d < 2.05 && d > 1e-4) {
        const p = (2.05 - d) / 2, nx = dx / d, nz = dz / d;
        a.x -= nx * p; a.z -= nz * p; b.x += nx * p; b.z += nz * p;
        const behind = (-Math.sin(a.h) * dx - Math.cos(a.h) * dz) > 0 ? a : b, front = behind === a ? b : a;
        if (Math.abs(a.v - b.v) > 14 && Math.random() < 0.5) fx.spark((ax + bx) / 2, 0.6, (az + bz) / 2, 0, 2, 0);
        behind.v *= 0.992; front.v = Math.min(front.v + 0.05, front.phys.vmax);
      }
    }
  }
}
function onLap(c) {
  if (G.type === 'demo') { if (c.lap > 1) c.lap = 1; return; }
  if (c.finished) return;
  if (c.lap >= 2) {
    const lt = raceTime - c.lapStart;
    if (!c.bestLap || lt < c.bestLap) c.bestLap = lt;
    if (c.isPlayer) {
      const id = TRACKS[setup.track].id;
      const rec = G.type === 'race' && (!best[id] || lt < best[id]);
      if (rec) { best[id] = lt; store.set('best3', best); }
      flash((rec ? 'Record du circuit ' : 'Tour ') + fmt(lt));
    }
  }
  c.lapStart = raceTime;
  if (c.lap > setup.laps) {
    c.finished = true; c.finishTime = raceTime; c.rank = ++finishOrder;
    if (c.isPlayer) finishRace();
  } else if (c.isPlayer && c.lap === setup.laps && setup.laps > 1) flash('Dernier tour');
}
const standings = () => cars.slice().sort((a, b) => (a.finished && b.finished) ? a.rank - b.rank : a.finished ? -1 : b.finished ? 1 : b.total - a.total);

// ---------- course à bonus : caisses, missiles, flaques, boucliers ----------
let boxGeo = null, boxMat = null, oilGeo = null, oilMat = null, shotGeo = null, shotMat = null, shieldGeo = null, shieldMat = null;
function itemAssets() {
  if (boxGeo) return;
  const t = canvasTex(128, 128, (g) => { const gr = g.createLinearGradient(0, 0, 128, 128); gr.addColorStop(0, '#ff2bd0'); gr.addColorStop(0.5, '#ffd23f'); gr.addColorStop(1, '#19e6ff'); g.fillStyle = gr; g.fillRect(0, 0, 128, 128);
    g.fillStyle = '#05040c'; g.fillRect(10, 10, 108, 108); g.fillStyle = '#fff'; g.font = '900 96px Arial, sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText('?', 64, 70); });
  boxGeo = new THREE.BoxGeometry(1.5, 1.5, 1.5); boxMat = new THREE.MeshBasicMaterial({ map: t, color: new THREE.Color(1.6, 1.6, 1.6) });
  oilGeo = new THREE.CircleGeometry(2.1, 20).rotateX(-Math.PI / 2); oilMat = new THREE.MeshStandardMaterial({ color: 0x050308, roughness: 0.05, metalness: 1, transparent: true, opacity: 0.92, envMapIntensity: 3 });
  shotGeo = new THREE.ConeGeometry(0.28, 1.5, 8).rotateX(-Math.PI / 2); shotMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(3, 0.5, 0.2) });
  shieldGeo = new THREE.SphereGeometry(2.7, 20, 12); shieldMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(0.2, 0.9, 1.6), transparent: true, opacity: 0.22, blending: THREE.AdditiveBlending, depthWrite: false });
}
function spawnBoxes() {
  itemAssets();
  for (const f of [0.1, 0.28, 0.46, 0.64, 0.82]) for (const off of [-8, -4, 0, 4, 8]) {
    const idx = Math.floor(T.N * f); T.point(idx, off, tmp);
    const m = new THREE.Mesh(boxGeo, boxMat); m.userData.shared = true; m.position.set(tmp.x, 1.3, tmp.z); objs.add(m);
    G.boxes.push({ idx, x: tmp.x, z: tmp.z, t: 0, mesh: m });
  }
}
function rollItem(c) {
  const rank = standings().indexOf(c) / Math.max(1, cars.length - 1); // 0 = en tête
  const r = Math.random();
  if (rank < 0.2) return r < 0.5 ? 'huile' : r < 0.85 ? 'bouclier' : 'turbo';
  if (rank > 0.6) return r < 0.4 ? 'missile' : r < 0.8 ? 'turbo' : 'bouclier';
  return r < 0.3 ? 'missile' : r < 0.55 ? 'turbo' : r < 0.8 ? 'huile' : 'bouclier';
}
function useItem(c) {
  const it = c.item; if (!it) return; c.item = null;
  if (it === 'turbo') c.turboT = 2.6;
  else if (it === 'bouclier') c.shieldT = 8;
  else if (it === 'huile') {
    const m = new THREE.Mesh(oilGeo, oilMat); m.userData.shared = true; const x = c.x + Math.sin(c.h) * 4.5, z = c.z + Math.cos(c.h) * 4.5;
    m.position.set(x, 0.04, z); m.layers.set(1); m.renderOrder = 3; objs.add(m);
    G.oils.push({ x, z, idx: c.idx, lat: T.lateral(x, z, c.idx), life: 30, mesh: m, owner: c, arm: 0.8 });
  } else if (it === 'missile') {
    let target = null, bd = 1e9;
    for (const o of cars) { if (o === c) continue; const d = ((o.total - c.total) % T.N + T.N) % T.N; if (d > 0 && d < T.N * 0.45 && d < bd && o.total > c.total) { bd = d; target = o; } }
    const m = new THREE.Mesh(shotGeo, shotMat); m.userData.shared = true; objs.add(m);
    G.shots.push({ s: c.idx + 3, lat: T.lateral(c.x, c.z, c.idx), target, owner: c, life: 7, mesh: m });
  }
  if (c.isPlayer) audio.beep(660, 0.08);
}
function strike(c, strong) {
  if (c.shieldT > 0) { c.shieldT = 0; fx.burst(c.x, 1, c.z, 0.4, 1.6, 3, 24, 12); return; }
  c.spinT = strong ? 1.3 : 1.0; c.v *= 0.55; c.turboT = 0;
  if (strong) { fx.burst(c.x, 0.8, c.z, 3.5, 1.4, 0.3, 46, 16); for (let i = 0; i < 8; i++) fx.smoke(c.x, 0.8, c.z, 0, 3, 0, 1.6, 0.4, 1.4); }
  if (c.isPlayer) { flash(strong ? 'Touché !' : 'Dérapage !'); c.bump = 0.8; }
}
function stepItems(dt) {
  const N = T.N;
  for (const b of G.boxes) {
    if (b.t > 0) { b.t -= dt; b.mesh.visible = b.t <= 0; continue; }
    b.mesh.rotation.y += dt * 1.6; b.mesh.rotation.x += dt * 0.9; b.mesh.position.y = 1.3 + Math.sin(raceTime * 3 + b.idx) * 0.15;
    for (const c of cars) {
      if (Math.abs(c.x - b.x) > 2.3 || Math.abs(c.z - b.z) > 2.3) continue;
      b.t = 4; b.mesh.visible = false; fx.burst(b.x, 1.3, b.z, 2.5, 1.6, 3, 16, 9);
      if (!c.item) { c.item = rollItem(c); c.ai.itemT = 1 + Math.random() * 4; if (c.isPlayer) audio.beep(990, 0.1); }
      break;
    }
  }
  for (let i = G.shots.length - 1; i >= 0; i--) {
    const s = G.shots[i]; s.life -= dt; s.s += 120 * dt / T.ds;
    if (s.target) s.lat += clamp(T.lateral(s.target.x, s.target.z, s.target.idx) - s.lat, -14 * dt, 14 * dt);
    const i0 = Math.floor(s.s), f = s.s - i0, a = { x: 0, z: 0 }, b = { x: 0, z: 0 };
    T.point(i0, s.lat, a); T.point(i0 + 1, s.lat, b);
    const x = a.x + (b.x - a.x) * f, z = a.z + (b.z - a.z) * f;
    s.mesh.position.set(x, 0.9, z); s.mesh.rotation.y = T.heading(i0);
    fx.flame(x, 0.9, z, 0, 0, 0, 1.2); if (Math.random() < 0.5) fx.smoke(x, 0.9, z, 0, 0.5, 0, 0.7, 0.25, 0.7);
    let done = s.life <= 0;
    for (const c of cars) { if (c === s.owner) continue; if (Math.abs(c.x - x) < 3 && Math.abs(c.z - z) < 3) { strike(c, true); done = true; break; } }
    if (done) { objs.remove(s.mesh); G.shots.splice(i, 1); }
  }
  for (let i = G.oils.length - 1; i >= 0; i--) {
    const o = G.oils[i]; o.life -= dt; o.arm -= dt;
    let done = o.life <= 0;
    for (const c of cars) { if (c === o.owner && o.arm > 0) continue; if (Math.abs(c.x - o.x) < 2.3 && Math.abs(c.z - o.z) < 2.3 && c.spinT <= 0) { strike(c, false); done = true; break; } }
    if (done) { objs.remove(o.mesh); G.oils.splice(i, 1); }
  }
  for (const c of cars) {
    if (c.shieldT > 0 && !c.vis.shield) { c.vis.shield = new THREE.Mesh(shieldGeo, shieldMat); c.vis.shield.userData.shared = true; c.vis.shield.position.y = 0.8; c.vis.shield.layers.set(1); c.vis.root.add(c.vis.shield); }
    if (c.vis.shield) c.vis.shield.visible = c.shieldT > 0;
  }
  void N;
}

// ---------- balade libre : circulation ----------
function spawnTraffic(n) {
  for (let i = 0; i < n; i++) {
    const def = TRAFIC[i % TRAFIC.length], vis = createCar(def, TRAFIC_PAINTS[(i * 3 + 1) % TRAFIC_PAINTS.length]);
    if (vis.beam) vis.beam.visible = false; vis.wet = world.theme.wet ?? 1; vis.root.rotation.order = 'YXZ';
    scene.add(vis.root);
    const dir = i % 2 ? -1 : 1;
    G.traffic.push({ vis, s: 60 + (i + 0.5) * (T.N - 80) / n, dir, lat: dir * 2.3, speed: 19 + (i * 37 % 9), cur: 20, stun: 0, x: 0, z: 0, h: 0, y: 0, v: 20 });
  }
}
function stepTraffic(dt) {
  const N = T.N, a = { x: 0, z: 0 }, b = { x: 0, z: 0 };
  for (const t of G.traffic) {
    let want = t.speed;
    if (t.stun > 0) { t.stun -= dt; want = 0; }
    // ralentit derrière le joueur ou une autre voiture dans sa voie
    const ahead = (ox, oz, ov) => { const dx = ox - t.x, dz = oz - t.z, f = -Math.sin(t.h) * dx - Math.cos(t.h) * dz, l = Math.cos(t.h) * dx - Math.sin(t.h) * dz; if (f > 0 && f < 22 && Math.abs(l) < 2.2) want = Math.min(want, Math.max(0, ov * (t.dir > 0 ? 1 : 1)) * (f < 9 ? 0.4 : 0.9)); };
    if (player) ahead(player.x, player.z, Math.abs(player.v) * (Math.cos(wrap(player.h - t.h)) > 0 ? 1 : 0));
    for (const o of G.traffic) if (o !== t && o.dir === t.dir) ahead(o.x, o.z, o.cur);
    t.cur += clamp(want - t.cur, -14 * dt, 5 * dt);
    t.s = (t.s + t.dir * t.cur * dt / T.ds + N) % N;
    const i0 = Math.floor(t.s), f = t.s - i0;
    T.point(i0, t.lat, a); T.point(i0 + 1, t.lat, b);
    t.x = a.x + (b.x - a.x) * f; t.z = a.z + (b.z - a.z) * f; t.h = wrap(T.heading(i0) + (t.dir < 0 ? Math.PI : 0)); t.v = t.cur;
    t.vis.root.position.set(t.x, 0, t.z); t.vis.root.rotation.y = t.h;
    animateCar(t.vis, dt, t.cur, 0, 0, 0, want < t.cur - 1, time);
    if (!player) continue;
    const dx = player.x - t.x, dz = player.z - t.z, d = Math.hypot(dx, dz);
    if (d < 3.4 && d > 1e-3) {
      const nx = dx / d, nz = dz / d, p = 3.4 - d;
      player.x += nx * p; player.z += nz * p;
      if (Math.abs(player.v) > 6 && t.stun <= 0) { for (let k = 0; k < 14; k++) fx.spark((player.x + t.x) / 2, 0.7, (player.z + t.z) / 2, 0, 2, 0); player.bump = 0.7; flash('Attention à la circulation'); }
      player.v *= 0.6; t.stun = 1.6; t.cur *= 0.4;
    }
  }
}

// ---------- foot en voiture ----------
function kickoff() {
  const A = ARENA;
  Object.assign(ball, { x: 0, y: A.ballR + 0.5, z: 0, vx: 0, vy: 0, vz: 0 });
  const slots = [[0, 30], [-15, 44], [15, 50]];
  const count = [0, 0];
  for (const c of cars) {
    const s = slots[count[c.team]++ % 3], sg = c.team === 0 ? 1 : -1;
    Object.assign(c, { x: s[0] * sg, z: s[1] * sg, h: c.team === 0 ? 0 : Math.PI, v: 0, u: 0, y: 0, vy: 0, pitch: 0, jumps: 0, steer: 0, boost: 34, spinT: 0 });
  }
  for (const p of world.pads) { p.t = 0; p.mesh.visible = true; }
  chaseSnap = true;
}
function stepBall(dt) {
  const A = ARENA, r = A.ballR, b = ball;
  b.vy -= 20 * dt;
  b.x += b.vx * dt; b.y += b.vy * dt; b.z += b.vz * dt;
  const k = Math.exp(-0.28 * dt); b.vx *= k; b.vz *= k;
  if (b.y < r) { b.y = r; if (b.vy < 0) b.vy = -b.vy * 0.62; if (Math.abs(b.vy) < 1.3) b.vy = 0; const f = Math.exp(-0.45 * dt); b.vx *= f; b.vz *= f; }
  if (b.y > A.ceil - r) { b.y = A.ceil - r; b.vy = -Math.abs(b.vy) * 0.7; }
  if (Math.abs(b.x) > A.hx - r) { b.x = Math.sign(b.x) * (A.hx - r); b.vx = -b.vx * 0.75; }
  if (Math.abs(b.z) > A.hz - r) {
    const open = Math.abs(b.x) < A.goalW - r * 0.5 && b.y < A.goalH - r * 0.4;
    if (!open) { b.z = Math.sign(b.z) * (A.hz - r); b.vz = -b.vz * 0.75; }
    else if (Math.abs(b.z) > A.hz + r * 0.5) return b.z < 0 ? 0 : 1; // les bleus (0) marquent du côté -z
  }
  for (const c of cars) {
    const cy = c.y + 0.75, dx = b.x - c.x, dy = b.y - cy, dz = b.z - c.z, d = Math.hypot(dx, dy, dz), R = r + 1.75;
    if (d >= R || d < 1e-3) continue;
    let nx = dx / d, ny = dy / d + 0.22, nz = dz / d; const nl = Math.hypot(nx, ny, nz); nx /= nl; ny /= nl; nz /= nl;
    const sh = Math.sin(c.h), ch = Math.cos(c.h), cvx = -sh * c.v + ch * c.u, cvz = -ch * c.v - sh * c.u;
    const rel = (b.vx - cvx) * nx + (b.vy - c.vy) * ny + (b.vz - cvz) * nz;
    b.x = c.x + (dx / d) * R; b.y = Math.max(r, cy + (dy / d) * R); b.z = c.z + (dz / d) * R;
    if (rel < 0) {
      const j = -1.2 * rel; b.vx += nx * j; b.vy += ny * j; b.vz += nz * j;
      b.last = c; c.v *= 0.9;
      if (j > 12) { for (let i = 0; i < 6; i++) fx.spark(b.x - nx * r, b.y - ny * r, b.z - nz * r, 0, 2, 0); }
    }
  }
  const sp = Math.hypot(b.vx, b.vy, b.vz); if (sp > 62) { const s = 62 / sp; b.vx *= s; b.vy *= s; b.vz *= s; }
  return -1;
}
function footAI(c, dt) {
  const A = ARENA, b = ball, sk = c.ai.skill, ai = c.ai;
  const goalZ = c.team === 0 ? -A.hz : A.hz, own = -goalZ;
  const dist = Math.hypot(b.x - c.x, b.z - c.z), tl = clamp(dist / Math.max(Math.abs(c.v), 16), 0, 1.1);
  const px = clamp(b.x + b.vx * tl, -A.hx + 2, A.hx - 2), pz = clamp(b.z + b.vz * tl, -A.hz + 2, A.hz - 2);
  let tx, tz, stop = false;
  const danger = Math.abs(pz - own) < 30;
  if (ai.role === 'att' || danger) {
    let gx = -px, gz = goalZ - pz; const gl = Math.hypot(gx, gz) || 1; gx /= gl; gz /= gl;
    const behind = ((px - c.x) * gx + (pz - c.z) * gz) / (dist || 1);
    if (behind > 0.25) { tx = px - gx * 2.6; tz = pz - gz * 2.6; }
    else { const side = ((c.x - px) * -gz + (c.z - pz) * gx) > 0 ? 1 : -1; tx = px - gx * 9 + -gz * side * 7; tz = pz - gz * 9 + gx * side * 7; }
  } else { tx = px * 0.5; tz = own * 0.6 + pz * 0.3; stop = Math.hypot(tx - c.x, tz - c.z) < 6; }
  tx = clamp(tx, -A.hx + 2.5, A.hx - 2.5); tz = clamp(tz, -A.hz + 2.5, A.hz - 2.5);
  const want = stop ? Math.atan2(-(b.x - c.x), -(b.z - c.z)) : Math.atan2(-(tx - c.x), -(tz - c.z));
  const ang = wrap(want - c.h), steer = clamp(-ang * 2.6, -1, 1);
  let gas = 1, brake = 0;
  if (stop) { gas = 0; brake = c.v > 1.5 ? 0.8 : 0; }
  else if (Math.abs(ang) > 1.3 && c.v > 13) { gas = 0; brake = 0.7; }
  if (c.v > c.phys.vmax * sk.speed * 0.85) gas = 0;
  const boost = !stop && Math.abs(ang) < 0.22 && dist > 16 && c.boost > 12 && setup.diff > 0 && c.y < 0.1;
  if (ai.jumpT > 0) ai.jumpT -= dt;
  else if (b.y > 3.4 && b.y < 9.5 && dist < 7.5 && c.y < 0.1 && Math.random() < sk.corner) ai.jumpT = 0.4;
  if (ai.stuck > 0) { ai.stuck -= dt; return { steer: -steer, gas: 0, brake: 1 }; }
  if (Math.abs(c.v) < 1 && !stop) { ai.still = (ai.still || 0) + dt; if (ai.still > 1) { ai.stuck = 0.7; ai.still = 0; } } else ai.still = 0;
  return { steer, gas, brake, boost, jump: ai.jumpT > 0 };
}
let roleT = 0;
function stepFoot(dt) {
  const A = ARENA;
  if (G.freeze > 0) {
    G.freeze -= dt;
    if (G.freeze <= 0) { $('banner').hidden = true; if (G.ended) return endFoot(); kickoff(); }
    else if (G.freeze < 1.2 && !G.reset && !G.ended) { G.reset = true; kickoff(); $('banner').textContent = 'PRÊT'; $('banner').style.color = '#fff'; }
    if (G.reset || G.ended) { syncBall(dt); return; }
  }
  roleT -= dt;
  if (roleT <= 0) { roleT = 0.5; for (const t of [0, 1]) { let bc = null, bd = 1e9; for (const c of cars) if (c.team === t) { const d = Math.hypot(ball.x - c.x, ball.z - c.z); c.ai.role = 'def'; if (d < bd) { bd = d; bc = c; } } if (bc) bc.ai.role = 'att'; } }
  raceTime += dt;
  for (const c of cars) { const inp = c.isPlayer ? playerInput() : footAI(c, dt); c.inp = inp; stepCar(c, inp, dt); }
  collide(cars);
  for (const p of world.pads) {
    if (p.t > 0) { p.t -= dt; if (p.t <= 0) p.mesh.visible = true; continue; }
    for (const c of cars) if (c.y < 1 && Math.abs(c.x - p.x) < 2.4 && Math.abs(c.z - p.z) < 2.4 && c.boost < 99) { c.boost = Math.min(100, c.boost + 45); p.t = 6; p.mesh.visible = false; if (c.isPlayer) audio.beep(880, 0.07); break; }
  }
  if (G.freeze <= 0) {
    const scored = stepBall(dt);
    if (scored >= 0) {
      G.score[scored]++; if (ball.last && ball.last.team === scored) ball.last.goals++;
      const col = new THREE.Color(TEAM_COL[scored]);
      fx.burst(ball.x, ball.y, ball.z, col.r * 4, col.g * 4, col.b * 4, 140, 34);
      $('banner').hidden = false; $('banner').textContent = 'BUT !'; $('banner').style.color = '#' + col.getHexString();
      $('sc-b').textContent = G.score[0]; $('sc-o').textContent = G.score[1];
      audio.beep(scored === 0 ? 880 : 330, 0.5);
      G.freeze = 2.6; G.reset = false;
      ball.vx *= 0.2; ball.vz *= 0.2;
    }
    G.timeLeft -= dt;
    if (G.timeLeft <= 0 && G.freeze <= 0) { G.timeLeft = 0; G.ended = true; G.freeze = 1.6; $('banner').hidden = false; $('banner').textContent = 'FIN DU MATCH'; $('banner').style.color = '#fff'; }
  } else stepBall(dt);
  syncBall(dt);
  void A;
}
function syncBall(dt) {
  const m = world.ball, r = ARENA.ballR;
  m.position.set(ball.x, ball.y, ball.z);
  m.rotation.x += ball.vz * dt / r; m.rotation.z -= ball.vx * dt / r;
  const s = clamp(3.8 - ball.y * 0.12, 1.2, 3.8);
  world.ballShadow.position.set(ball.x, 0.03, ball.z); world.ballShadow.scale.set(s, s, 1); world.ballShadow.material.opacity = clamp(1 - ball.y / 22, 0.2, 1);
}

// ---------- entrées ----------
const keys = { left: false, right: false, gas: false, brake: false, boost: false, act: false };
const KEYMAP = { ArrowLeft: 'left', KeyA: 'left', ArrowRight: 'right', KeyD: 'right', ArrowUp: 'gas', KeyW: 'gas', ArrowDown: 'brake', KeyS: 'brake', Space: 'act', ShiftLeft: 'boost', ShiftRight: 'boost', KeyX: 'boost' };
addEventListener('keydown', (e) => { const k = KEYMAP[e.code]; if (k) { keys[k] = true; if (mode === 'race' || mode === 'count') e.preventDefault(); } if ((e.code === 'Escape' || e.code === 'KeyP') && (mode === 'race' || mode === 'pause')) togglePause(); });
addEventListener('keyup', (e) => { const k = KEYMAP[e.code]; if (k) keys[k] = false; });
for (const [id, k] of [['k-left', 'left'], ['k-right', 'right'], ['k-gas', 'gas'], ['k-brake', 'brake'], ['k-boost', 'boost'], ['k-act', 'act']]) {
  const el = $(id);
  const set = (v) => { keys[k] = v; el.classList.toggle('on', v); };
  el.addEventListener('pointerdown', (e) => { e.preventDefault(); try { el.setPointerCapture(e.pointerId); } catch (err) { /* sans capture */ } set(true); });
  for (const ev of ['pointerup', 'pointercancel', 'lostpointercapture']) el.addEventListener(ev, () => set(false));
  el.addEventListener('contextmenu', (e) => e.preventDefault());
}
function playerInput() {
  const steer = clamp((keys.right ? 1 : 0) - (keys.left ? 1 : 0) + pad.steer, -1, 1);
  const brake = Math.max(keys.brake ? 1 : 0, pad.brake);
  const gas = brake > 0.1 ? 0 : Math.max(opts.autoGas || keys.gas ? 1 : 0, pad.gas);
  const act = keys.act || pad.act;
  return { steer, gas, brake, boost: keys.boost || pad.boost, jump: act, use: act };
}

// ---------- manette (Xbox ou compatible) ----------
// En course : gâchette droite = accélérer, gâchette gauche ou X = freiner, B ou RB = boost, A = saut / objet (sinon accélérer),
// stick gauche ou croix = tourner, Start = pause. Dans les menus : croix ou stick = se déplacer, A = valider, B = retour.
const pad = { steer: 0, gas: 0, brake: 0, boost: false, act: false, prev: [], navT: 0, used: false };
const SCREEN_PRIORITY = ['pause', 'results', 'setup', 'garage', 'options', 'menu'];
function padNav(dx, dy) {
  const scr = SCREEN_PRIORITY.map($).find(e => !e.hidden); if (!scr) return;
  const items = [...scr.querySelectorAll('button, input')].filter(e => e.offsetParent !== null);
  if (!items.length) return;
  const cur = document.activeElement;
  if (!items.includes(cur)) { (scr.querySelector('.btn.primary') || items[0]).focus(); return; }
  if (cur.tagName === 'INPUT' && dx) { cur.value = clamp(+cur.value + dx, +cur.min, +cur.max); cur.dispatchEvent(new Event('input')); return; }
  const r = cur.getBoundingClientRect(), cx = r.left + r.width / 2, cy = r.top + r.height / 2;
  let bestEl = null, bestD = 1e9;
  for (const e of items) {
    if (e === cur) continue;
    const q = e.getBoundingClientRect(), ex = q.left + q.width / 2 - cx, ey = q.top + q.height / 2 - cy;
    const along = ex * dx + ey * dy, across = Math.abs(ex * dy) + Math.abs(ey * dx);
    if (along < 4) continue;
    const d = along + across * 2.5;
    if (d < bestD) { bestD = d; bestEl = e; }
  }
  if (bestEl) { bestEl.focus(); bestEl.scrollIntoView({ block: 'nearest', inline: 'nearest' }); }
}
function pollPad(dt) {
  let g = null;
  try { const gps = navigator.getGamepads ? navigator.getGamepads() : []; for (const x of gps) if (x && x.connected && x.buttons.length >= 10) { g = x; break; } } catch (e) { g = null; }
  if (!g) { pad.steer = pad.gas = pad.brake = 0; pad.boost = pad.act = false; return; }
  const val = (i) => { const b = g.buttons[i]; return b ? Math.max(b.value || 0, b.pressed ? 1 : 0) : 0; };
  const hit = (i) => val(i) > 0.5 && !pad.prev[i];
  let sx = g.axes[0] || 0; const sy = g.axes[1] || 0;
  sx = Math.abs(sx) < 0.15 ? 0 : (sx - Math.sign(sx) * 0.15) / 0.85;
  const steer = val(14) ? -1 : val(15) ? 1 : sx;
  const actMode = G.type === 'foot' || G.type === 'items';
  pad.steer = steer; pad.gas = Math.max(val(7), actMode ? 0 : val(0)); pad.brake = Math.max(val(6), val(2));
  pad.boost = val(1) > 0.5 || val(5) > 0.5; pad.act = actMode && (val(0) > 0.5 || val(4) > 0.5);
  if (!pad.used && (Math.abs(steer) > 0.3 || g.buttons.some(b => b.pressed))) { pad.used = true; app.classList.add('pad'); audio.initAudio(); }
  if (mode === 'race' || mode === 'count') {
    if (hit(9) && mode === 'race') togglePause();
  } else {
    pad.steer = pad.gas = pad.brake = 0; pad.boost = pad.act = false;
    if (hit(9) && mode === 'pause') togglePause();
    else if (hit(0)) { const el = document.activeElement; if (el && el !== document.body && el.offsetParent !== null) el.click(); else padNav(0, 1); }
    else if (hit(1)) window.__nnBack();
    else {
      const dx = val(14) ? -1 : val(15) ? 1 : Math.abs(sx) > 0.6 ? Math.sign(sx) : 0;
      const dy = val(12) ? -1 : val(13) ? 1 : Math.abs(sy) > 0.6 ? Math.sign(sy) : 0;
      pad.navT -= dt;
      if (!dx && !dy) pad.navT = 0;
      else if (pad.navT <= 0) { pad.navT = 0.22; Math.abs(dx) >= Math.abs(dy) && dx ? padNav(dx, 0) : padNav(0, dy); }
    }
  }
  for (let i = 0; i < g.buttons.length; i++) pad.prev[i] = val(i) > 0.5;
}
addEventListener('touchstart', () => { if (pad.used) { pad.used = false; app.classList.remove('pad'); } }, { passive: true });

// ---------- caméra ----------
let camH = 0, camRoll = 0, garageAng = 0.6, garageDrag = null, demoAng = 0, chaseSnap = true;
const camPos = new THREE.Vector3(), camLook = new THREE.Vector3();
function setFov(f, dt) { if (Math.abs(camera.fov - f) > 0.05) { camera.fov += (f - camera.fov) * (dt ? 1 - Math.exp(-4 * dt) : 1); camera.updateProjectionMatrix(); } }
function chaseCam(c, dt, snap) {
  const portrait = H > W;
  camH = snap ? c.h : camH + wrap(c.h - camH) * (1 - Math.exp(-5.5 * dt));
  const ratio = clamp(Math.abs(c.v) / 95, 0, 1), moto = c.vis.moto;
  // le champ de vision s'élargit avec la vitesse ; la caméra se rapproche d'autant pour que le véhicule garde la même taille
  const fov0 = portrait ? 72 : 54, fov = fov0 + ratio * 12 + (c.boosting ? 5 : 0);
  const dist = (portrait ? 8.2 : (moto ? 4.3 : 5.6)) * Math.tan(fov0 * Math.PI / 360) / Math.tan(camera.fov * Math.PI / 360), hgt = portrait ? 2.5 : (moto ? 1.75 : 1.6);
  let x = c.x + Math.sin(camH) * dist, z = c.z + Math.cos(camH) * dist;
  const i = T.nearest(x, z, c.idx, 10), d = T.lateral(x, z, i);
  if (Math.abs(d) > T.limit + 0.5) { const s = Math.sign(d), p = Math.abs(d) - T.limit - 0.5; x -= -T.tz[i] * s * p; z -= T.tx[i] * s * p; }
  const shake = (c.bump || 0) * 0.25 + ratio * ratio * 0.012 + (c.boosting ? 0.02 : 0);
  camPos.set(x + (Math.random() - 0.5) * shake, hgt + (Math.random() - 0.5) * shake, z);
  camera.position.copy(camPos); // position rigide : pas de retard qui éloignerait le véhicule à haute vitesse
  camLook.set(c.x - Math.sin(camH) * 7, 0.85, c.z - Math.cos(camH) * 7);
  camera.lookAt(camLook);
  camRoll += (clamp(-c.yaw * 0.07, -0.05, 0.05) - camRoll) * (1 - Math.exp(-5 * dt)); camera.rotateZ(camRoll);
  setFov(fov, snap ? 0 : dt);
  c.bump = (c.bump || 0) * Math.exp(-6 * dt);
}
// caméra du foot : placée derrière la voiture, dans l'axe du ballon
function ballCam(c, dt, snap) {
  const A = ARENA, portrait = H > W;
  const toBall = Math.atan2(-(ball.x - c.x), -(ball.z - c.z)), dist2 = Math.hypot(ball.x - c.x, ball.z - c.z);
  const want = dist2 < 5 ? c.h : toBall;
  camH = snap ? want : camH + wrap(want - camH) * (1 - Math.exp(-4.5 * dt));
  const dist = portrait ? 12 : 8.2;
  const x = clamp(c.x + Math.sin(camH) * dist, -A.hx + 0.6, A.hx - 0.6), z = clamp(c.z + Math.cos(camH) * dist, -A.hz - A.goalD + 0.8, A.hz + A.goalD - 0.8);
  const shake = (c.bump || 0) * 0.25;
  camera.position.set(x + (Math.random() - 0.5) * shake, (portrait ? 4 : 3) + c.y * 0.75, z);
  const k = clamp(8 / Math.max(dist2, 8), 0.15, 0.5);
  camLook.set(c.x + (ball.x - c.x) * k, 1.1 + c.y * 0.6 + (ball.y - 1.7) * 0.3 * k * 2, c.z + (ball.z - c.z) * k);
  camera.lookAt(camLook);
  setFov(portrait ? 78 : 64, snap ? 0 : dt);
  c.bump = (c.bump || 0) * Math.exp(-6 * dt);
}
function orbitCam(c, ang, radius, hgt, shift) {
  const portrait = H > W;
  const r = radius * (portrait ? 1.35 : 1);
  camera.position.set(c.x + Math.sin(ang) * r, hgt, c.z + Math.cos(ang) * r);
  camLook.set(c.x, 0.6, c.z);
  if (shift && !portrait) { const rx = Math.cos(ang), rz = -Math.sin(ang); camera.position.x += rx * shift; camera.position.z += rz * shift; camLook.x += rx * shift; camLook.z += rz * shift; }
  if (shift && portrait) { camLook.y -= 1.2; camera.position.y -= 0.2; }
  camera.lookAt(camLook);
  setFov(46, 0);
}
canvas.addEventListener('pointerdown', (e) => { if (mode === 'garage') garageDrag = { x: e.clientX, a: garageAng }; });
addEventListener('pointermove', (e) => { if (garageDrag && mode === 'garage') garageAng = garageDrag.a - (e.clientX - garageDrag.x) * 0.01; });
addEventListener('pointerup', () => { garageDrag = null; });

// ---------- interface ----------
const screens = ['loading', 'menu', 'setup', 'garage', 'options', 'hud', 'pause', 'results'];
function show(...ids) { for (const s of screens) $(s).hidden = !ids.includes(s); }
const fmt = (t) => { const m = Math.floor(t / 60), s = t - m * 60; return m + ':' + (s < 10 ? '0' : '') + s.toFixed(2); };
const fmtShort = (t) => { t = Math.max(0, Math.ceil(t)); return Math.floor(t / 60) + ':' + String(t % 60).padStart(2, '0'); };
const ordinal = (n) => n === 1 ? '1er' : n + 'e';
let flashT = 0;
function flash(msg) { $('h-msg').textContent = msg; flashT = 3; }
function chips(el, items, get, set) {
  el.textContent = '';
  items.forEach(([val, label]) => {
    const b = document.createElement('button'); b.className = 'chip'; b.type = 'button'; b.textContent = label;
    b.setAttribute('aria-pressed', String(get() === val));
    b.addEventListener('click', () => { set(val); for (const o of el.children) o.setAttribute('aria-pressed', 'false'); b.setAttribute('aria-pressed', 'true'); });
    el.appendChild(b);
  });
}
const trackCache = TRACKS.map(makeTrack);
function drawPath(g, Tk, w, h, padPx, color, lw) {
  let x0 = 1e9, x1 = -1e9, z0 = 1e9, z1 = -1e9;
  for (let i = 0; i < Tk.N; i++) { x0 = Math.min(x0, Tk.px[i]); x1 = Math.max(x1, Tk.px[i]); z0 = Math.min(z0, Tk.pz[i]); z1 = Math.max(z1, Tk.pz[i]); }
  const sc = Math.min((w - padPx * 2) / (x1 - x0), (h - padPx * 2) / (z1 - z0)), ox = (w - (x1 - x0) * sc) / 2 - x0 * sc, oz = (h - (z1 - z0) * sc) / 2 - z0 * sc;
  g.beginPath(); for (let i = 0; i <= Tk.N; i += 2) { const j = i % Tk.N; i ? g.lineTo(Tk.px[j] * sc + ox, Tk.pz[j] * sc + oz) : g.moveTo(Tk.px[j] * sc + ox, Tk.pz[j] * sc + oz); }
  g.closePath(); g.strokeStyle = color; g.lineWidth = lw; g.lineJoin = 'round'; g.stroke();
  return { sc, ox, oz };
}
let setupType = 'race';
function buildSetup(type) {
  setupType = type;
  $('s-title').textContent = type === 'foot' ? 'Foot Turbo' : type === 'items' ? 'Course à bonus' : 'Choix de la course';
  for (const el of document.querySelectorAll('#setup [data-for]')) el.hidden = !el.dataset.for.split(' ').includes(type);
  const el = $('tracks'); el.textContent = '';
  TRACKS.forEach((t, i) => {
    const b = document.createElement('button'); b.className = 'track'; b.type = 'button'; b.setAttribute('aria-pressed', String(setup.track === i));
    const cv = document.createElement('canvas'); cv.width = 232; cv.height = 124; b.appendChild(cv);
    const g = cv.getContext('2d'); const col = '#' + new THREE.Color(t.theme.env === 'nature' ? t.theme.c : t.theme.a).getHexString();
    g.shadowColor = col; g.shadowBlur = 12; drawPath(g, trackCache[i], 232, 124, 14, col, 5);
    const nm = document.createElement('b'); nm.textContent = t.nom; b.appendChild(nm);
    const ds = document.createElement('small'); ds.textContent = t.desc + ' · ' + (trackCache[i].L / 1000).toFixed(1) + ' km'; b.appendChild(ds);
    const rc = document.createElement('em'); rc.textContent = best[t.id] ? 'Record ' + fmt(best[t.id]) : 'Pas encore de record'; b.appendChild(rc);
    b.addEventListener('click', () => { setup.track = i; for (const o of el.children) o.setAttribute('aria-pressed', 'false'); b.setAttribute('aria-pressed', 'true'); });
    el.appendChild(b);
  });
  chips($('diffs'), DIFFS.map((d, i) => [i, d.nom]), () => setup.diff, (v) => { setup.diff = v; });
  chips($('laps'), [[1, '1'], [3, '3'], [5, '5']], () => setup.laps, (v) => { setup.laps = v; });
  chips($('bots'), [[3, '3'], [5, '5'], [7, '7'], [9, '9'], [11, '11']], () => setup.bots, (v) => { setup.bots = v; });
  chips($('teams'), [[1, '1 contre 1'], [2, '2 contre 2'], [3, '3 contre 3']], () => setup.team, (v) => { setup.team = v; });
  chips($('durs'), [[120, '2 min'], [180, '3 min'], [300, '5 min']], () => setup.dur, (v) => { setup.dur = v; });
}
function buildOptions() {
  chips($('o-qual'), [['auto', 'Auto'], ['haute', 'Élevés'], ['moyenne', 'Moyens'], ['basse', 'Bas']], () => opts.qual, (v) => { opts.qual = v; store.set('opts', opts); changeQuality(); });
  chips($('o-gas'), [[false, 'Manuel'], [true, 'Automatique']], () => opts.autoGas, (v) => { opts.autoGas = v; store.set('opts', opts); });
  chips($('o-snd'), [[true, 'Activé'], [false, 'Coupé']], () => opts.son, (v) => { opts.son = v; store.set('opts', opts); audio.setSound(v); });
  $('o-info').textContent = 'Manette : gâchette droite pour accélérer, gâchette gauche pour freiner, B pour le boost, A pour sauter ou lancer un objet, Start pour la pause. Clavier : flèches ou ZQSD, Maj pour le boost, Espace pour sauter ou lancer un objet.';
}
function changeQuality() {
  pickQuality(); setupPost();
  show('loading'); $('loadmsg').textContent = 'Mise à jour des graphismes…';
  setTimeout(() => { worldKey = ''; startDemo(); show('options'); }, 30);
}
function hudFor(type) {
  const race = type === 'race' || type === 'items';
  $('h-posbox').hidden = !race; $('h-lapbox').hidden = type === 'foot';
  $('h-laplabel').textContent = type === 'free' ? 'Distance' : 'Tour'; $('h-lapsfx').hidden = type === 'free';
  $('score').hidden = type !== 'foot'; $('mini').hidden = type === 'foot';
  $('k-act').hidden = !(type === 'foot' || type === 'items'); $('k-actl').textContent = type === 'foot' ? 'SAUT' : 'OBJET';
  $('k-boost').hidden = type === 'items'; $('boostbar').hidden = type === 'items';
  $('k-gas').hidden = opts.autoGas; $('h-item').textContent = ''; $('h-msg').textContent = ''; $('wrong').hidden = true; $('banner').hidden = true;
  $('h-tot').textContent = cars.length; $('h-laps').textContent = setup.laps; $('h-lap').textContent = type === 'free' ? '0.0' : 1;
  $('sc-b').textContent = 0; $('sc-o').textContent = 0;
}

// garage
let gCar = null;
function garageShow(i) {
  selCar = (i + CARS.length) % CARS.length;
  const def = CARS[selCar];
  clearGame();
  gCar = makeCarEntity(def, carColor(def), true, 'Toi', DIFFS[1]);
  T.point(T.N - 5, 0, tmp); gCar.x = tmp.x; gCar.z = tmp.z; gCar.h = T.heading(T.N - 5); gCar.idx = T.N - 5;
  $('g-name').textContent = def.nom;
  const sw = $('swatches'); sw.textContent = '';
  $('gpaint').hidden = gCar.vis.paintMats.length === 0;
  PAINTS.forEach((p) => {
    const b = document.createElement('button'); b.className = 'sw'; b.type = 'button'; b.style.background = '#' + new THREE.Color(p).getHexString();
    b.setAttribute('aria-label', 'Peinture'); b.setAttribute('aria-pressed', String(carColor(def) === p));
    b.addEventListener('click', () => { (tuning[def.id] = tuning[def.id] || {}).couleur = p; store.set('tuning', tuning); setPaint(gCar.vis, p); for (const o of sw.children) o.setAttribute('aria-pressed', 'false'); b.setAttribute('aria-pressed', 'true'); });
    sw.appendChild(b);
  });
  const st = $('stats'); st.textContent = '';
  const cur = carStats(def);
  STAT_KEYS.forEach(([k, label]) => {
    const row = document.createElement('label'); row.className = 'stat';
    const sp = document.createElement('span'); sp.textContent = label;
    const out = document.createElement('output'); out.textContent = cur[k];
    const inp = document.createElement('input'); inp.type = 'range'; inp.min = 1; inp.max = 10; inp.step = 1; inp.value = cur[k]; inp.id = 'stat-' + k;
    inp.addEventListener('input', () => {
      const t = tuning[def.id] = tuning[def.id] || {}; t.stats = Object.assign({}, carStats(def), { [k]: +inp.value });
      store.set('tuning', tuning); out.textContent = inp.value; power();
    });
    row.append(sp, out, inp); st.appendChild(row);
  });
  power();
}
function power() {
  const s = carStats(CARS[selCar]);
  $('g-kmh').textContent = Math.round(180 + s.vmax * 18);
  $('g-pow').textContent = Math.round((s.vmax * 1.2 + s.accel * 1.1 + s.mania * 0.8 + s.adher * 0.9) * 25);
}
function refreshMenu() {
  $('m-car').textContent = CARS[selCar].nom;
  const id = TRACKS[setup.track].id;
  $('m-best').textContent = TRACKS[setup.track].nom + (best[id] ? ' · record ' + fmt(best[id]) : '');
}

// ---------- déroulement ----------
function startDemo() {
  ensureWorld(String(setup.track));
  G.type = 'demo'; G.boost = true; G.drain = 30; G.regen = 7;
  gridUp(false, 7);
  for (const c of cars) c.lap = 1;
  mode = 'menu'; chaseSnap = true;
}
function toMenu() { startDemo(); refreshMenu(); show('menu'); audio.engine(0, 0, false); }
function startGame(type) {
  store.set('setup', setup); store.set('car', CARS[selCar].id);
  show('loading'); $('loadmsg').textContent = type === 'foot' ? 'Foot Turbo' : type === 'free' ? ROUTE_LIBRE.nom : TRACKS[setup.track].nom;
  setTimeout(() => {
    G.type = type; G.ended = false; G.freeze = 0; G.score = [0, 0]; G.dist = 0;
    if (type === 'foot') {
      ensureWorld('arena'); clearGame();
      G.boost = true; G.drain = 33; G.regen = 0; G.timeLeft = setup.dur;
      const sk = DIFFS[setup.diff], n = setup.team;
      for (let t = 0; t < 2; t++) for (let i = 0; i < n; i++) {
        const isP = t === 0 && i === 0, di = isP ? selCar : (selCar + 1 + t * n + i) % CARS.length;
        const c = makeCarEntity(CARS[di], isP ? carColor(CARS[di]) : TEAM_COL[t], isP, isP ? 'Toi' : BOT_NAMES[(t * 3 + i) % BOT_NAMES.length], sk);
        c.team = t; if (isP) player = c;
        // repère de couleur d'équipe au-dessus de chaque voiture
        const mk = new THREE.Mesh(new THREE.ConeGeometry(0.28, 0.6, 4).rotateX(Math.PI), new THREE.MeshBasicMaterial({ color: new THREE.Color(TEAM_COL[t]).multiplyScalar(2.5) }));
        mk.position.y = 2.3; mk.layers.set(1); c.vis.root.add(mk);
      }
      kickoff(); raceTime = 0;
    } else if (type === 'free') {
      ensureWorld('libre'); clearGame();
      G.boost = true; G.drain = 30; G.regen = 7;
      player = makeCarEntity(CARS[selCar], carColor(CARS[selCar]), true, 'Toi', DIFFS[1]);
      T.point(5, 2.3, tmp); Object.assign(player, { x: tmp.x, z: tmp.z, h: T.heading(5), idx: 5 });
      spawnTraffic(22); raceTime = 0;
    } else {
      ensureWorld(String(setup.track));
      G.boost = type === 'race'; G.drain = 30; G.regen = 7;
      gridUp(true, setup.bots);
      if (type === 'items') spawnBoxes();
    }
    hudFor(type); show('hud');
    mode = 'count'; countT = 3.2; countN = 4; chaseSnap = true; $('count').hidden = false; $('count').textContent = '';
  }, 30);
}
function finishRace() {
  mode = 'finished'; player.ai.skill = DIFFS[1];
  const r = player.rank;
  $('rtitle').textContent = r === 1 ? 'Victoire !' : ordinal(r) + ' place';
  const id = TRACKS[setup.track].id;
  $('rsub').textContent = 'Meilleur tour ' + (player.bestLap ? fmt(player.bestLap) : '—') + (G.type === 'race' && best[id] ? ' · record du circuit ' + fmt(best[id]) : '');
  fillResults(); show('results');
}
function endFoot() {
  mode = 'finished';
  const [b, o] = G.score;
  $('rtitle').textContent = b > o ? 'Victoire !' : b < o ? 'Défaite' : 'Match nul';
  $('rsub').textContent = 'Bleus ' + b + ' – ' + o + ' Orange';
  const tb = $('rtable'); tb.textContent = '';
  cars.slice().sort((x, y) => y.goals - x.goals).forEach((c) => {
    const tr = document.createElement('tr'); if (c.isPlayer) tr.className = 'me';
    for (const v of [c.team === 0 ? 'Bleu' : 'Orange', c.name + ' · ' + c.def.nom, c.goals + (c.goals > 1 ? ' buts' : ' but')]) { const td = document.createElement('td'); td.textContent = v; tr.appendChild(td); }
    tb.appendChild(tr);
  });
  show('results'); audio.engine(0, 0, false);
}
function fillResults() {
  const tb = $('rtable'); tb.textContent = '';
  standings().forEach((c, i) => {
    const tr = document.createElement('tr'); if (c.isPlayer) tr.className = 'me';
    for (const v of [i + 1, c.name + ' · ' + c.def.nom, c.finished ? fmt(c.finishTime) : 'en course']) { const td = document.createElement('td'); td.textContent = v; tr.appendChild(td); }
    tb.appendChild(tr);
  });
}
function togglePause() {
  if (mode === 'race') { mode = 'pause'; show('hud', 'pause'); audio.engine(0, 0, false); }
  else if (mode === 'pause') { mode = 'race'; show('hud'); }
}
document.addEventListener('visibilitychange', () => { if (document.hidden && mode === 'race') togglePause(); });

$('m-race').onclick = () => { audio.initAudio(); buildSetup('race'); show('setup'); };
$('m-items').onclick = () => { audio.initAudio(); buildSetup('items'); show('setup'); };
$('m-foot').onclick = () => { audio.initAudio(); buildSetup('foot'); show('setup'); };
$('m-free').onclick = () => { audio.initAudio(); startGame('free'); };
$('m-garage').onclick = () => { audio.initAudio(); mode = 'garage'; G.type = 'demo'; garageShow(selCar); show('garage'); };
$('m-options').onclick = () => { audio.initAudio(); buildOptions(); show('options'); };
for (const b of document.querySelectorAll('[data-back]')) b.onclick = () => { store.set('setup', setup); toMenu(); };
$('s-go').onclick = () => startGame(setupType);
$('g-prev').onclick = () => garageShow(selCar - 1);
$('g-next').onclick = () => garageShow(selCar + 1);
$('g-reset').onclick = () => { delete tuning[CARS[selCar].id]; store.set('tuning', tuning); garageShow(selCar); };
$('g-pick').onclick = () => { store.set('car', CARS[selCar].id); toMenu(); };
$('bpause').onclick = togglePause;
$('p-resume').onclick = togglePause;
$('p-restart').onclick = () => startGame(G.type);
$('p-quit').onclick = toMenu;
$('r-menu').onclick = toMenu;
$('r-again').onclick = () => startGame(G.type);

// touche Retour et mise en veille (application Android)
let backAt = 0;
window.__nnBack = () => {
  const now = performance.now(); if (now - backAt < 350) return true; backAt = now;
  if (mode === 'race' || mode === 'pause') { togglePause(); return true; }
  if (mode === 'count') return true;
  if (mode === 'finished') { toMenu(); return true; }
  if (!$('menu').hidden) return false; // au menu principal : l'application se ferme
  store.set('setup', setup); toMenu(); return true;
};
window.__nnPause = () => { if (mode === 'race') togglePause(); audio.engine(0, 0, false); };

// ---------- mini-carte ----------
const mini = $('mini'), mg = mini.getContext('2d');
function drawMini() {
  mg.clearRect(0, 0, 192, 192);
  mg.shadowBlur = 0;
  drawPath(mg, T, 192, 192, 16, 'rgba(0,0,0,0.55)', 14);
  const mp = drawPath(mg, T, 192, 192, 16, 'rgba(255,255,255,0.75)', 5);
  const dot = (c, col, r) => { mg.beginPath(); mg.arc(c.x * mp.sc + mp.ox, c.z * mp.sc + mp.oz, r, 0, 7); mg.fillStyle = col; mg.fill(); };
  for (const t of G.traffic) dot(t, '#ffd23f', 4);
  for (const c of cars) if (!c.isPlayer) dot(c, '#ff2bd0', 6);
  if (player) dot(player, '#19e6ff', 9);
}

// ---------- boucle ----------
let time = 0;
function simulate(dt) {
  raceTime += dt;
  for (const c of cars) {
    const inp = (c.isPlayer && mode === 'race') ? playerInput() : aiInput(c, dt);
    if (G.type === 'items') { if (inp.use && !c.prevUse) useItem(c); c.prevUse = !!inp.use; }
    stepCar(c, inp, dt);
    c.inp = inp;
  }
  collide(cars);
  if (G.type === 'items') stepItems(dt);
  if (G.type === 'free') stepTraffic(dt);
}
// fumée, flammes et étincelles produites par un véhicule
function emit(c, dt) {
  if (camera.position.distanceToSquared(c.vis.root.position) > 160 * 160) return;
  const sh = Math.sin(c.h), ch = Math.cos(c.h), len = c.vis.len, hw = c.vis.half;
  const rx = c.x + sh * (len + 0.1), rz = c.z + ch * (len + 0.1), vx = -sh * c.v, vz = -ch * c.v, y = c.y;
  const side = (s, back) => [c.x + sh * back + ch * s, c.z + ch * back - sh * s];
  const sp = Math.abs(c.v);
  if (c.boosting) {
    for (let k = 0; k < 2; k++) { const [ex, ez] = side((k ? 1 : -1) * Math.min(0.24, hw * 0.3), len + 0.15); fx.flame(ex, y + 0.47, ez, vx * 0.55 + sh * 9, c.vy, vz * 0.55 + ch * 9, 1.5, c.turboT > 0 ? 1 : 0); }
    if (Math.random() < 0.3) fx.smoke(rx, y + 0.5, rz, vx * 0.4 + sh * 3, 1, vz * 0.4 + ch * 3, 0.9, 0.16, 0.7);
  }
  const gear = Math.min(5, Math.floor(clamp(sp / c.phys.vmax, 0, 1) * 6));
  if (gear > c.gear && c.inp && c.inp.gas > 0.5) for (let k = 0; k < 5; k++) fx.flame(rx, y + 0.47, rz, vx * 0.6 + sh * 6, 0, vz * 0.6 + ch * 6, 1.1); // détonation au passage de rapport
  c.gear = gear;
  if (y < 0.05) {
    if ((c.slip > 0.6 || Math.abs(c.u) > 1.6 || c.spinT > 0) && sp > 8) for (const s of [-1, 1]) { const [wx, wz] = side(s * hw * 0.82, len * 0.6); fx.smoke(wx, 0.2, wz, vx * 0.25, 0.6, vz * 0.25, 1.2, 0.3, 1.0); }
    else if (c.braking && sp > 24 && Math.random() < 0.35) { const [wx, wz] = side((Math.random() < 0.5 ? -1 : 1) * hw * 0.82, len * 0.6); fx.smoke(wx, 0.2, wz, vx * 0.3, 0.4, vz * 0.3, 0.9, 0.2, 0.7); }
    if (c.offroad && sp > 6 && Math.random() < 0.5) fx.smoke(rx, 0.25, rz, vx * 0.3, 1.2, vz * 0.3, 1.3, 0.22, 1.0);
    if (c.inp && c.inp.gas > 0.5 && sp < 14 && Math.random() < 0.12) fx.smoke(rx, 0.45, rz, sh * 2, 0.6, ch * 2, 0.6, 0.14, 0.8);
  }
  if (c.onWall && sp > 9) for (let k = 0; k < 2; k++) { const [wx, wz] = side((Math.random() < 0.5 ? -1 : 1) * hw, (Math.random() - 0.5) * len * 2); fx.spark(wx, 0.5, wz, vx * 0.5, 1, vz * 0.5); }
  void dt;
}
function syncVisuals(dt) {
  for (const c of cars) {
    c.vis.root.position.set(c.x, c.y, c.z); c.vis.root.rotation.set(c.pitch, c.h + c.spinA, 0);
    animateCar(c.vis, dt, c.v, c.steer, c.yaw * Math.abs(c.v), c.lon, c.braking, time);
    // de jour, l'ombre s'étire à l'opposé du soleil
    if (world.sun && c.vis.shadow) { const sx = -world.sun[0] * 1.1, sz = -world.sun[1] * 1.1, ch = Math.cos(c.h), sh = Math.sin(c.h); c.vis.shadow.position.set(sx * ch - sz * sh, 0.015 - c.y, sx * sh + sz * ch); }
    emit(c, dt);
  }
}
let last = performance.now(), hudT = 0, fpsAcc = 0, fpsN = 0, good = 0;
function frame(now) {
  requestAnimationFrame(frame);
  const dt = Math.min(0.05, (now - last) / 1000); last = now;
  if (dt <= 0) return;
  time += dt;
  if (mode === 'loading' || !world) return;
  pollPad(dt);
  let blur = 0, focus = player || cars[0];
  if (mode === 'menu') {
    simulate(dt); syncVisuals(dt);
    const lead = cars[0];
    demoAng += dt * 0.12;
    if (chaseSnap) { camH = lead.h; chaseSnap = false; }
    camH += wrap(lead.h - camH) * (1 - Math.exp(-3 * dt));
    orbitCam(lead, camH + Math.sin(demoAng) * 0.9 + 0.35, 7.5, 1.9 + Math.sin(demoAng * 0.7) * 0.5, 0);
    blur = 0.3;
  } else if (mode === 'garage') {
    if (!garageDrag) garageAng += dt * 0.25;
    syncVisuals(dt);
    orbitCam(gCar, garageAng + gCar.h, gCar.vis.moto ? 4.2 : 5.8, 1.6, gCar.vis.moto ? 0.9 : 1.3);
    focus = gCar;
  } else if (mode === 'count') {
    countT -= dt;
    const n = Math.ceil(countT);
    if (n !== countN && n >= 0) { countN = n; $('count').textContent = n > 0 ? n : 'GO !'; audio.beep(n > 0 ? 440 : 880, n > 0 ? 0.12 : 0.4); }
    if (n > 3) $('count').textContent = '';
    syncVisuals(dt);
    if (G.type === 'foot') { syncBall(dt); ballCam(player, dt, chaseSnap); } else chaseCam(player, dt, chaseSnap);
    chaseSnap = false;
    if (countT <= 0) { mode = 'race'; raceTime = 0; setTimeout(() => { $('count').hidden = true; }, 700); }
    audio.engine(0.02, keys.gas ? 1 : 0.2, true);
  } else if (mode === 'race' || mode === 'finished') {
    if (G.type === 'foot') { if (mode === 'race') stepFoot(dt); } else simulate(dt);
    syncVisuals(dt);
    if (mode !== 'race') { demoAng += dt * 0.3; orbitCam(player, player.h + Math.PI + Math.sin(demoAng) * 0.8, 7, 2.0, 0); }
    else if (G.type === 'foot') { ballCam(player, dt, chaseSnap); chaseSnap = false; }
    else chaseCam(player, dt, false);
    blur = G.type === 'foot' ? clamp(player.v / 120, 0, 0.5) : clamp(player.v / 80, 0, 1) + (player.boosting ? 0.3 : 0);
    audio.engine(clamp(Math.abs(player.v) / player.phys.vmax, 0, 1), player.inp ? (player.inp.gas || 0) : 0, mode === 'race');
    hudT -= dt;
    $('h-kmh').textContent = Math.round(Math.abs(player.v) * 3.6);
    $('h-boost').style.transform = 'scaleX(' + (player.turboT > 0 ? 1 : player.boost / 100).toFixed(3) + ')';
    if (hudT <= 0) {
      hudT = 0.15;
      if (mode === 'race') {
        if (G.type === 'foot') $('h-time').textContent = fmtShort(G.timeLeft);
        else if (G.type === 'free') { $('h-time').textContent = fmtShort(raceTime); $('h-lap').textContent = (G.dist / 1000).toFixed(1) + ' km'; drawMini(); }
        else {
          $('h-pos').textContent = standings().indexOf(player) + 1;
          $('h-lap').textContent = clamp(player.lap, 1, setup.laps);
          $('h-time').textContent = fmt(raceTime);
          $('wrong').hidden = !(Math.cos(wrap(player.h - T.heading(player.idx))) < -0.3 && Math.abs(player.v) > 3);
          if (G.type === 'items') $('h-item').textContent = player.item ? 'Objet : ' + ITEM_NAMES[player.item] : player.shieldT > 0 ? 'Bouclier actif' : '';
          drawMini();
        }
      } else if (G.type !== 'foot') fillResults();
    }
    if (flashT > 0) { flashT -= dt; if (flashT <= 0) $('h-msg').textContent = ''; }
  }
  if (mode !== 'pause') fx.update(dt);
  if (focus) world.update(camera, dt, time, focus.idx || 0);
  if (world.rain && world.rain.visible) world.updateRain(camera, dt, focus ? -Math.sin(focus.h) * focus.v : 0, focus ? -Math.cos(focus.h) * focus.v : 0);
  if (composer) { finalPass.uniforms.amt.value = blur; finalPass.uniforms.time.value = time; composer.render(dt); } else renderer.render(scene, camera);
  // résolution adaptative en mode Auto
  if (opts.qual === 'auto' && mode !== 'pause') {
    fpsAcc += dt; fpsN++;
    if (fpsN >= 80) {
      const fps = fpsN / fpsAcc; fpsAcc = 0; fpsN = 0;
      if (fps < 40 && dyn > 0.5) { dyn = Math.max(0.5, dyn - 0.1); good = 0; applySize(); }
      else if (fps > 56 && dyn < 1 && ++good >= 4) { dyn = Math.min(1, dyn + 0.05); good = 0; applySize(); }
    }
  }
  if (window.__shot) { window.__shot = false; window.__ready = true; }
}

// ---------- démarrage ----------
window.__nn = {
  get mode() { return mode; }, get cars() { return cars; }, get T() { return T; }, get world() { return world; }, setup, opts, G, ball, scene, camera, renderer,
  // avance la simulation sans rendu (tests)
  run(seconds, step = 1 / 60) {
    for (let t = 0; t < seconds; t += step) { if (G.type === 'foot') stepFoot(step); else simulate(step); }
    return cars.map(c => ({ n: c.name, lap: c.lap, idx: c.idx, v: +c.v.toFixed(1), best: c.bestLap }));
  },
  go(m) { mode = m; },
  freeze() { mode = 'count'; countT = 1e9; countN = 99; chaseSnap = true; $('count').hidden = true; },
};
(async () => {
  try {
    setupPost();
    $('loadmsg').textContent = 'Chargement des véhicules…';
    await resolveModels();
    await new Promise(r => setTimeout(r, 30));
    buildOptions(); audio.setSound(opts.son);
    toMenu();
    requestAnimationFrame(frame);
    window.__booted = true;
  } catch (e) {
    console.error(e);
    $('loadmsg').textContent = 'Le jeu ne peut pas démarrer sur cet appareil (WebGL indisponible).';
    window.__err = String(e && e.stack || e);
  }
})();
