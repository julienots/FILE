// Nuit Néon — boucle de jeu, conduite, adversaires, menus.
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { ShaderPass } from 'three/addons/postprocessing/ShaderPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { TRACKS } from './tracks.js';
import { buildWorld, makeTrack, LIMIT } from './world.js';
import { CARS, STAT_KEYS, PAINTS, physFromStats, createCar, resolveModels, setPaint, animateCar, disposeCar } from './cars.js';
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
const opts = Object.assign({ qual: 'auto', autoGas: true, son: true }, store.get('opts', {}));
const setup = Object.assign({ track: 0, diff: 1, laps: 3 }, store.get('setup', {}));
const best = store.get('best', {});
let selCar = params.get('modele') ? 0 : Math.max(0, CARS.findIndex(c => c.id === store.get('car', CARS[0].id)));
if (params.get('q')) opts.qual = params.get('q');
setup.track = clamp(setup.track | 0, 0, TRACKS.length - 1);
const carStats = (def) => Object.assign({}, def.stats, (tuning[def.id] || {}).stats);
const carColor = (def) => (tuning[def.id] && tuning[def.id].couleur !== undefined) ? tuning[def.id].couleur : def.couleur;
const DIFFS = [
  { nom: 'Facile', speed: 0.80, corner: 0.60 }, { nom: 'Normal', speed: 0.88, corner: 0.74 },
  { nom: 'Difficile', speed: 0.95, corner: 0.87 }, { nom: 'Expert', speed: 1.0, corner: 0.97 },
];
const BOT_NAMES = ['Kaïto', 'Luna', 'Rex', 'Mila', 'Zéphyr', 'Nova', 'Axel'];

// ---------- rendu ----------
const app = $('app'), canvas = $('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance', preserveDrawingBuffer: params.has('shot') });
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 0.95;
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(55, 1, 0.1, 2500);
camera.layers.enable(1);

const QUALITY = {
  haute: { pr: 2, refl: 0.55, bloom: true, msaa: 4, taps: 9, env: 256, rain: true },
  moyenne: { pr: 1.5, refl: 0.45, bloom: true, msaa: 0, taps: 7, env: 128, rain: true },
  basse: { pr: 1, refl: 0.33, bloom: false, msaa: 0, taps: 5, env: 64, rain: false },
};
const isTouch = matchMedia('(pointer: coarse)').matches;
let qName = 'moyenne', Q = QUALITY.moyenne, dyn = 1;
function pickQuality() { qName = opts.qual === 'auto' ? (isTouch ? 'moyenne' : 'haute') : (QUALITY[opts.qual] ? opts.qual : 'moyenne'); Q = QUALITY[qName]; dyn = 1; }
pickQuality();

let composer = null, finalPass = null, bloomPass = null;
function setupPost() {
  if (composer) { composer.dispose(); composer.renderTarget1.dispose(); composer.renderTarget2.dispose(); composer = null; }
  if (!Q.bloom) return;
  const rt = new THREE.WebGLRenderTarget(4, 4, { type: THREE.HalfFloatType, samples: Q.msaa });
  composer = new EffectComposer(renderer, rt);
  composer.addPass(new RenderPass(scene, camera));
  bloomPass = new UnrealBloomPass(new THREE.Vector2(256, 256), 0.42, 0.6, 1.0);
  composer.addPass(bloomPass);
  finalPass = new ShaderPass({
    uniforms: { tDiffuse: { value: null }, amt: { value: 0 } },
    vertexShader: 'varying vec2 vUv; void main(){ vUv=uv; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.); }',
    fragmentShader: `uniform sampler2D tDiffuse; uniform float amt; varying vec2 vUv;
      void main(){ vec2 c = vec2(0.5,0.56); vec2 d = vUv - c; float r = length(d*vec2(1.0,1.25));
        float k = smoothstep(0.28, 0.85, r) * 0.05 * amt;
        vec3 col = vec3(0.); for(int i=0;i<8;i++){ float t = float(i)/7.; col += texture2D(tDiffuse, vUv - d*k*t).rgb; } col /= 8.;
        col *= 1.0 - 0.45*smoothstep(0.4, 1.0, r);
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
  if (world) world.road.getRenderTarget().setSize(Math.max(64, Math.floor(W * pr * Q.refl)), Math.max(64, Math.floor(H * pr * Q.refl)));
  camera.aspect = W / H; camera.updateProjectionMatrix();
}
new ResizeObserver(() => applySize()).observe(app);

// ---------- monde ----------
let world = null, T = null;
function loadTrack(i) {
  if (world) { scene.remove(world.group); world.dispose(); }
  world = buildWorld(TRACKS[i], renderer, Q); T = world.T;
  scene.add(world.group); scene.fog = world.fog;
  scene.environment = null;
  scene.environment = world.captureEnv(scene);
  if (world.rain) world.rain.visible = Q.rain;
  applySize();
  miniPath = null;
}

// ---------- voitures et simulation ----------
let cars = [], player = null, raceTime = 0, mode = 'loading', countT = 0, countN = 0, finishOrder = 0;
const tmp = { x: 0, z: 0 };
function clearCars() { for (const c of cars) { scene.remove(c.vis.root); disposeCar(c.vis); } cars = []; player = null; }
function spawn(defIdx, slot, isPlayer, paint, name, skill) {
  const def = CARS[defIdx], vis = createCar(def, paint);
  const row = Math.floor(slot / 2), side = slot % 2 ? 1 : -1;
  const idx = T.N - Math.round((8 + row * 9 + (side > 0 ? 3 : 0)) / T.ds);
  T.point(idx, side * 2.7, tmp);
  const c = { def, vis, phys: physFromStats(carStats(def)), x: tmp.x, z: tmp.z, h: T.heading(idx), v: 0, u: 0, steer: 0, idx: ((idx % T.N) + T.N) % T.N, lap: 0, total: 0,
    lapStart: 0, bestLap: 0, finished: false, finishTime: 0, rank: 0, isPlayer, name, yaw: 0, lon: 0, braking: false, onWall: false,
    ai: { off: side * 2.7, base: side * 2.7, t: 1 + slot * 0.4, skill } };
  c.total = c.idx - T.N;
  vis.root.position.set(c.x, 0, c.z); vis.root.rotation.y = c.h;
  scene.add(vis.root); cars.push(c);
  return c;
}
function gridUp(withPlayer) {
  clearCars();
  const sk = DIFFS[setup.diff];
  const order = [];
  for (let i = 0; i < CARS.length * 2; i++) order.push((selCar + 1 + i) % CARS.length);
  const usedPaint = new Set(withPlayer ? [carColor(CARS[selCar])] : []);
  const n = 6;
  for (let s = 0; s < n; s++) {
    const isP = withPlayer && s === n - 1;
    const di = isP ? selCar : order[s];
    let paint = isP ? carColor(CARS[di]) : carColor(CARS[di]);
    if (!isP && usedPaint.has(paint)) paint = PAINTS.find(p => !usedPaint.has(p)) ?? paint;
    usedPaint.add(paint);
    const c = spawn(di, s, isP, paint, isP ? 'Toi' : BOT_NAMES[s % BOT_NAMES.length], sk);
    if (isP) player = c;
  }
  raceTime = 0; finishOrder = 0;
}

function aiInput(c, dt) {
  const P = c.phys, sk = c.ai.skill, N = T.N, ai = c.ai;
  ai.t -= dt; if (ai.t < 0) { ai.t = 2 + Math.random() * 4; ai.base = (Math.random() * 2 - 1) * 4.4; }
  const kA = T.k[(c.idx + Math.round((14 + c.v * 0.5) / T.ds)) % N];
  let target = clamp(ai.base * 0.6 + clamp(kA * 70, -1, 1) * 3.2, -5.4, 5.4);
  const myLat = T.lateral(c.x, c.z, c.idx);
  let slow = 0;
  for (const o of cars) {
    if (o === c) continue;
    let d = o.total - c.total; if (d < -N / 2) d += N; if (d > N / 2) d -= N;
    const dm = d * T.ds;
    if (dm > -1 && dm < 16) {
      const ol = T.lateral(o.x, o.z, o.idx);
      if (Math.abs(ol - myLat) < 2.6) { target = clamp(ol + (myLat >= ol ? 3.2 : -3.2), -5.6, 5.6); if (Math.abs(target - ol) < 2.4) target = clamp(ol + (ol > 0 ? -3.2 : 3.2), -5.6, 5.6); if (dm < 6.5 && c.v > o.v) slow = 0.5; }
    }
  }
  ai.off += clamp(target - ai.off, -2.4 * dt, 2.4 * dt);
  const la = 7 + Math.abs(c.v) * 0.42;
  T.point(c.idx + Math.round(la / T.ds), ai.off, tmp);
  const want = Math.atan2(-(tmp.x - c.x), -(tmp.z - c.z));
  const steer = clamp(-wrap(want - c.h) * 2.4, -1, 1);
  // vitesse permise par les virages à venir
  let vT = P.vmax * sk.speed;
  const scan = Math.min(90, Math.ceil((c.v * c.v / (2 * P.brake) + 14) / T.ds));
  for (let m = 0; m <= scan; m++) {
    const kk = Math.abs(T.k[(c.idx + m) % N]);
    if (kk < 1e-4) continue;
    const vc2 = P.grip * sk.corner / kk + 2 * P.brake * 0.75 * (m * T.ds);
    if (vc2 < vT * vT) vT = Math.sqrt(vc2);
  }
  let gas = 1, brake = 0;
  if (c.v > vT + 0.4) { gas = 0; brake = clamp((c.v - vT) / 3, 0.15, 1); } else if (c.v > vT - 0.6) gas = 0.4;
  if (slow) { gas = Math.min(gas, 0.2); brake = Math.max(brake, slow * 0.4); }
  // bloqué contre un mur : petite marche arrière
  if (ai.stuck > 0) { ai.stuck -= dt; return { steer: -steer, gas: 0, brake: 1 }; }
  if (Math.abs(c.v) < 1.2 && raceTime > 2) { ai.still = (ai.still || 0) + dt; if (ai.still > 1.2) { ai.stuck = 0.9; ai.still = 0; } } else ai.still = 0;
  return { steer, gas, brake };
}

function stepCar(c, inp, dt) {
  const P = c.phys;
  const rate = inp.steer === 0 ? 7 : 4.5;
  c.steer += clamp(inp.steer - c.steer, -rate * dt, rate * dt);
  let v = c.v; const v0 = v;
  if (inp.gas > 0 && v > -0.5) v += P.accel * inp.gas * (1 - Math.pow(Math.max(0, v) / P.vmax, 2)) * dt;
  if (inp.brake > 0) { if (v > 0.5) v = Math.max(0, v - P.brake * inp.brake * dt); else v = Math.max(-9, v - 7 * inp.brake * dt); }
  if (inp.gas <= 0 && inp.brake <= 0) { const d = (1.2 + 0.0025 * v * v) * dt; v = Math.abs(v) < d ? 0 : v - Math.sign(v) * d; }
  if (inp.gas > 0 && v < -0.5) v = Math.min(0, v + 14 * dt);
  const sp = Math.abs(v);
  const yawMax = Math.min(P.steer * Math.min(1, sp / 7), P.grip * 1.3 / Math.max(sp, 1));
  const yaw = c.steer * yawMax * (v < 0 ? -1 : 1);
  const excess = Math.max(0, Math.abs(yaw) * sp - P.grip);
  c.u -= Math.sign(yaw) * excess * 0.9 * dt;
  c.u *= Math.exp(-3.5 * dt);
  v -= Math.sign(v) * excess * 0.25 * dt;
  c.h = wrap(c.h - yaw * dt);
  const sh = Math.sin(c.h), ch = Math.cos(c.h);
  c.x += (-sh * v + ch * c.u) * dt; c.z += (-ch * v - sh * c.u) * dt;
  // limites de la piste
  const prev = c.idx, N = T.N;
  c.idx = T.nearest(c.x, c.z, c.idx);
  const d = T.lateral(c.x, c.z, c.idx);
  if (Math.abs(d) > LIMIT) {
    const s = Math.sign(d), push = Math.abs(d) - LIMIT, i = c.idx;
    c.x -= -T.tz[i] * s * push; c.z -= T.tx[i] * s * push;
    let th = T.heading(i), diff = wrap(c.h - th);
    if (Math.cos(diff) < 0) { th = wrap(th + Math.PI); diff = wrap(c.h - th); }
    const hit = Math.abs(Math.sin(diff));
    if (!c.onWall) { v *= (1 - 0.5 * hit); c.bump = hit * Math.min(1, sp / 30); }
    v *= Math.exp(-0.8 * dt);
    c.h = wrap(th + diff * (1 - Math.min(1, 3.5 * dt)));
    c.u = 0; c.onWall = true;
  } else c.onWall = false;
  if (prev > N * 0.75 && c.idx < N * 0.25) { c.lap++; onLap(c); }
  else if (prev < N * 0.25 && c.idx > N * 0.75) c.lap--;
  c.total = (c.lap - 1) * N + c.idx;
  c.lon = (v - v0) / dt; c.yaw = yaw; c.v = v; c.braking = inp.brake > 0.05 && v0 > 0.5;
}
function collide() {
  for (let i = 0; i < cars.length; i++) for (let j = i + 1; j < cars.length; j++) {
    const a = cars[i], b = cars[j];
    if (Math.abs(a.x - b.x) > 6 || Math.abs(a.z - b.z) > 6) continue;
    for (const fa of [-1.15, 1.15]) for (const fb of [-1.15, 1.15]) {
      const ax = a.x - Math.sin(a.h) * fa, az = a.z - Math.cos(a.h) * fa, bx = b.x - Math.sin(b.h) * fb, bz = b.z - Math.cos(b.h) * fb;
      const dx = bx - ax, dz = bz - az, d = Math.hypot(dx, dz);
      if (d < 2.05 && d > 1e-4) {
        const p = (2.05 - d) / 2, nx = dx / d, nz = dz / d;
        a.x -= nx * p; a.z -= nz * p; b.x += nx * p; b.z += nz * p;
        // celui qui est derrière perd un peu de vitesse
        const behind = (-Math.sin(a.h) * dx - Math.cos(a.h) * dz) > 0 ? a : b;
        behind.v *= 0.992; (behind === a ? b : a).v = Math.min((behind === a ? b : a).v + 0.05, (behind === a ? b : a).phys.vmax);
      }
    }
  }
}
function onLap(c) {
  if (mode === 'menu') { if (c.lap > 1) c.lap = 1; return; }
  if (c.finished) return;
  if (c.lap >= 2) {
    const lt = raceTime - c.lapStart;
    if (!c.bestLap || lt < c.bestLap) c.bestLap = lt;
    if (c.isPlayer) {
      const id = TRACKS[setup.track].id;
      const rec = !best[id] || lt < best[id];
      if (rec) { best[id] = lt; store.set('best', best); }
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

// ---------- entrées ----------
const keys = { left: false, right: false, gas: false, brake: false };
const KEYMAP = { ArrowLeft: 'left', KeyA: 'left', ArrowRight: 'right', KeyD: 'right', ArrowUp: 'gas', KeyW: 'gas', ArrowDown: 'brake', KeyS: 'brake', Space: 'brake' };
addEventListener('keydown', (e) => { const k = KEYMAP[e.code]; if (k) { keys[k] = true; if (mode === 'race' || mode === 'count') e.preventDefault(); } if ((e.code === 'Escape' || e.code === 'KeyP') && (mode === 'race' || mode === 'pause')) togglePause(); });
addEventListener('keyup', (e) => { const k = KEYMAP[e.code]; if (k) keys[k] = false; });
for (const [id, k] of [['k-left', 'left'], ['k-right', 'right'], ['k-gas', 'gas'], ['k-brake', 'brake']]) {
  const el = $(id);
  const set = (v) => { keys[k] = v; el.classList.toggle('on', v); };
  el.addEventListener('pointerdown', (e) => { e.preventDefault(); try { el.setPointerCapture(e.pointerId); } catch (err) { /* sans capture */ } set(true); });
  for (const ev of ['pointerup', 'pointercancel', 'lostpointercapture']) el.addEventListener(ev, () => set(false));
  el.addEventListener('contextmenu', (e) => e.preventDefault());
}
function playerInput() {
  const steer = (keys.right ? 1 : 0) - (keys.left ? 1 : 0);
  const brake = keys.brake ? 1 : 0;
  const gas = brake ? 0 : (opts.autoGas || keys.gas ? 1 : 0);
  return { steer, gas, brake };
}

// ---------- caméra ----------
let camH = 0, garageAng = 0.6, garageDrag = null, demoAng = 0;
const camPos = new THREE.Vector3(), camLook = new THREE.Vector3();
function chaseCam(c, dt, snap) {
  const portrait = H > W;
  camH = snap ? c.h : camH + wrap(c.h - camH) * (1 - Math.exp(-5.5 * dt));
  const ratio = clamp(Math.abs(c.v) / 95, 0, 1);
  const dist = (portrait ? 8.2 : 5.7) + ratio * 1.2, hgt = portrait ? 2.5 : 1.6;
  let x = c.x + Math.sin(camH) * dist, z = c.z + Math.cos(camH) * dist;
  // la caméra reste entre les glissières
  const i = T.nearest(x, z, c.idx, 10), d = T.lateral(x, z, i);
  if (Math.abs(d) > 7.4) { const s = Math.sign(d), p = Math.abs(d) - 7.4; x -= -T.tz[i] * s * p; z -= T.tx[i] * s * p; }
  const shake = (c.bump || 0) * 0.25 + ratio * ratio * 0.012;
  camPos.set(x + (Math.random() - 0.5) * shake, hgt + (Math.random() - 0.5) * shake, z);
  if (snap) camera.position.copy(camPos); else camera.position.lerp(camPos, 1 - Math.exp(-14 * dt));
  camLook.set(c.x - Math.sin(camH) * 7, 0.85, c.z - Math.cos(camH) * 7);
  camera.lookAt(camLook);
  const fov = (portrait ? 72 : 54) + ratio * 13;
  if (Math.abs(camera.fov - fov) > 0.05) { camera.fov += (fov - camera.fov) * (1 - Math.exp(-4 * dt)); camera.updateProjectionMatrix(); }
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
  if (Math.abs(camera.fov - 46) > 0.05) { camera.fov = 46; camera.updateProjectionMatrix(); }
}
canvas.addEventListener('pointerdown', (e) => { if (mode === 'garage') garageDrag = { x: e.clientX, a: garageAng }; });
addEventListener('pointermove', (e) => { if (garageDrag && mode === 'garage') garageAng = garageDrag.a - (e.clientX - garageDrag.x) * 0.01; });
addEventListener('pointerup', () => { garageDrag = null; });

// ---------- interface ----------
const screens = ['loading', 'menu', 'setup', 'garage', 'options', 'hud', 'pause', 'results'];
function show(...ids) { for (const s of screens) $(s).hidden = !ids.includes(s); }
const fmt = (t) => { const m = Math.floor(t / 60), s = t - m * 60; return m + ':' + (s < 10 ? '0' : '') + s.toFixed(2); };
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
function drawPath(g, Tk, w, h, pad, color, lw) {
  let x0 = 1e9, x1 = -1e9, z0 = 1e9, z1 = -1e9;
  for (let i = 0; i < Tk.N; i++) { x0 = Math.min(x0, Tk.px[i]); x1 = Math.max(x1, Tk.px[i]); z0 = Math.min(z0, Tk.pz[i]); z1 = Math.max(z1, Tk.pz[i]); }
  const sc = Math.min((w - pad * 2) / (x1 - x0), (h - pad * 2) / (z1 - z0)), ox = (w - (x1 - x0) * sc) / 2 - x0 * sc, oz = (h - (z1 - z0) * sc) / 2 - z0 * sc;
  g.beginPath(); for (let i = 0; i <= Tk.N; i += 2) { const j = i % Tk.N; i ? g.lineTo(Tk.px[j] * sc + ox, Tk.pz[j] * sc + oz) : g.moveTo(Tk.px[j] * sc + ox, Tk.pz[j] * sc + oz); }
  g.closePath(); g.strokeStyle = color; g.lineWidth = lw; g.lineJoin = 'round'; g.stroke();
  return { sc, ox, oz };
}
function buildSetup() {
  const el = $('tracks'); el.textContent = '';
  TRACKS.forEach((t, i) => {
    const b = document.createElement('button'); b.className = 'track'; b.type = 'button'; b.setAttribute('aria-pressed', String(setup.track === i));
    const cv = document.createElement('canvas'); cv.width = 232; cv.height = 124; b.appendChild(cv);
    const g = cv.getContext('2d'); const col = '#' + new THREE.Color(t.theme.a).getHexString();
    g.shadowColor = col; g.shadowBlur = 12; drawPath(g, trackCache[i], 232, 124, 14, col, 5);
    const nm = document.createElement('b'); nm.textContent = t.nom; b.appendChild(nm);
    const ds = document.createElement('small'); ds.textContent = t.desc + ' · ' + (trackCache[i].L / 1000).toFixed(1) + ' km'; b.appendChild(ds);
    const rc = document.createElement('em'); rc.textContent = best[t.id] ? 'Record ' + fmt(best[t.id]) : 'Pas encore de record'; b.appendChild(rc);
    b.addEventListener('click', () => { setup.track = i; for (const o of el.children) o.setAttribute('aria-pressed', 'false'); b.setAttribute('aria-pressed', 'true'); });
    el.appendChild(b);
  });
  chips($('diffs'), DIFFS.map((d, i) => [i, d.nom]), () => setup.diff, (v) => { setup.diff = v; });
  chips($('laps'), [[1, '1'], [3, '3'], [5, '5']], () => setup.laps, (v) => { setup.laps = v; });
}
function buildOptions() {
  chips($('o-qual'), [['auto', 'Auto'], ['haute', 'Élevés'], ['moyenne', 'Moyens'], ['basse', 'Bas']], () => opts.qual, (v) => { opts.qual = v; store.set('opts', opts); changeQuality(); });
  chips($('o-gas'), [[true, 'Automatique'], [false, 'Bouton GAZ']], () => opts.autoGas, (v) => { opts.autoGas = v; store.set('opts', opts); syncPads(); });
  chips($('o-snd'), [[true, 'Activé'], [false, 'Coupé']], () => opts.son, (v) => { opts.son = v; store.set('opts', opts); audio.setSound(v); });
  $('o-info').textContent = 'En mode Auto, le jeu baisse la résolution tout seul si le téléphone ralentit. Clavier : flèches ou ZQSD, Échap pour la pause.';
}
function syncPads() { $('k-gas').hidden = opts.autoGas; }
function changeQuality() {
  pickQuality(); setupPost();
  show('loading'); $('loadmsg').textContent = 'Mise à jour des graphismes…';
  setTimeout(() => { loadTrack(currentTrack); startDemo(); show('options'); mode = 'menu'; }, 30);
}

// garage
let gCar = null;
function garageShow(i) {
  selCar = (i + CARS.length) % CARS.length;
  const def = CARS[selCar];
  clearCars();
  gCar = spawn(selCar, 0, true, carColor(def), 'Toi', DIFFS[1]);
  T.point(T.N - 5, 0, tmp); gCar.x = tmp.x; gCar.z = tmp.z; gCar.vis.root.position.set(gCar.x, 0, gCar.z);
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
let currentTrack = -1;
function ensureTrack(i) { if (currentTrack !== i) { loadTrack(i); currentTrack = i; } }
function startDemo() {
  gridUp(false);
  for (const c of cars) c.lap = 1;
  mode = 'menu'; chaseSnap = true;
}
function toMenu() { ensureTrack(setup.track); startDemo(); refreshMenu(); show('menu'); audio.engine(0, 0, false); }
let chaseSnap = true;
function startRace() {
  store.set('setup', setup); store.set('car', CARS[selCar].id);
  show('loading'); $('loadmsg').textContent = TRACKS[setup.track].nom;
  setTimeout(() => {
    ensureTrack(setup.track);
    gridUp(true);
    $('h-tot').textContent = cars.length; $('h-laps').textContent = setup.laps; $('h-lap').textContent = 1; $('h-msg').textContent = ''; $('wrong').hidden = true;
    syncPads(); show('hud');
    mode = 'count'; countT = 3.2; countN = 4; chaseSnap = true; $('count').hidden = false; $('count').textContent = '';
  }, 30);
}
function finishRace() {
  mode = 'finished'; player.ai.skill = DIFFS[1];
  const r = player.rank;
  $('rtitle').textContent = r === 1 ? 'Victoire !' : ordinal(r) + ' place';
  const id = TRACKS[setup.track].id;
  $('rsub').textContent = 'Meilleur tour ' + (player.bestLap ? fmt(player.bestLap) : '—') + (best[id] ? ' · record du circuit ' + fmt(best[id]) : '');
  fillResults(); show('results');
}
function fillResults() {
  const tb = $('rtable'); tb.textContent = '';
  standings().forEach((c, i) => {
    const tr = document.createElement('tr'); if (c.isPlayer) tr.className = 'me';
    const cells = [i + 1, c.name + ' · ' + c.def.nom, c.finished ? fmt(c.finishTime) : 'en course'];
    for (const v of cells) { const td = document.createElement('td'); td.textContent = v; tr.appendChild(td); }
    tb.appendChild(tr);
  });
}
function togglePause() {
  if (mode === 'race') { mode = 'pause'; show('hud', 'pause'); audio.engine(0, 0, false); }
  else if (mode === 'pause') { mode = 'race'; show('hud'); }
}
document.addEventListener('visibilitychange', () => { if (document.hidden && mode === 'race') togglePause(); });

$('m-race').onclick = () => { audio.initAudio(); buildSetup(); show('setup'); };
$('m-garage').onclick = () => { audio.initAudio(); mode = 'garage'; garageShow(selCar); show('garage'); };
$('m-options').onclick = () => { audio.initAudio(); buildOptions(); show('options'); };
for (const b of document.querySelectorAll('[data-back]')) b.onclick = () => { store.set('setup', setup); toMenu(); };
$('s-go').onclick = startRace;
$('g-prev').onclick = () => garageShow(selCar - 1);
$('g-next').onclick = () => garageShow(selCar + 1);
$('g-reset').onclick = () => { delete tuning[CARS[selCar].id]; store.set('tuning', tuning); garageShow(selCar); };
$('g-pick').onclick = () => { store.set('car', CARS[selCar].id); toMenu(); };
$('bpause').onclick = togglePause;
$('p-resume').onclick = togglePause;
$('p-restart').onclick = startRace;
$('p-quit').onclick = toMenu;
$('r-menu').onclick = toMenu;
$('r-again').onclick = startRace;

// touche Retour et mise en veille (application Android)
window.__nnBack = () => {
  if (mode === 'race' || mode === 'pause') { togglePause(); return true; }
  if (mode === 'count') return true;
  if (mode === 'finished') { toMenu(); return true; }
  if (!$('menu').hidden) return false; // au menu principal : l'application se ferme
  store.set('setup', setup); toMenu(); return true;
};
window.__nnPause = () => { if (mode === 'race') togglePause(); audio.engine(0, 0, false); };

// ---------- mini-carte ----------
let miniPath = null;
const mini = $('mini'), mg = mini.getContext('2d');
function drawMini() {
  mg.clearRect(0, 0, 192, 192);
  mg.shadowBlur = 0;
  drawPath(mg, T, 192, 192, 16, 'rgba(0,0,0,0.55)', 14);
  miniPath = drawPath(mg, T, 192, 192, 16, 'rgba(255,255,255,0.75)', 5);
  for (const c of cars) {
    mg.beginPath(); mg.arc(c.x * miniPath.sc + miniPath.ox, c.z * miniPath.sc + miniPath.oz, c.isPlayer ? 9 : 6, 0, 7);
    mg.fillStyle = c.isPlayer ? '#19e6ff' : '#ff2bd0'; mg.fill();
  }
}

// ---------- boucle ----------
function simulate(dt) {
  raceTime += dt;
  for (const c of cars) {
    const inp = (c.isPlayer && mode === 'race') ? playerInput() : aiInput(c, dt);
    stepCar(c, inp, dt);
    c.inp = inp;
  }
  collide();
}
function syncVisuals(dt, time) {
  for (const c of cars) {
    c.vis.root.position.set(c.x, 0, c.z); c.vis.root.rotation.y = c.h;
    animateCar(c.vis, dt, c.v, c.steer, c.yaw * Math.abs(c.v), c.lon, c.braking, time);
  }
}
let last = performance.now(), hudT = 0, fpsAcc = 0, fpsN = 0, good = 0, time = 0;
function frame(now) {
  requestAnimationFrame(frame);
  let dt = Math.min(0.05, (now - last) / 1000); last = now;
  if (dt <= 0) return;
  time += dt;
  if (mode === 'loading' || !world) return;
  let blur = 0;
  if (mode === 'menu') {
    simulate(dt); syncVisuals(dt, time);
    const lead = cars[0];
    demoAng += dt * 0.12;
    if (chaseSnap) { camH = lead.h; chaseSnap = false; }
    camH += wrap(lead.h - camH) * (1 - Math.exp(-3 * dt));
    orbitCam(lead, camH + Math.sin(demoAng) * 0.9 + 0.35, 7.5, 1.9 + Math.sin(demoAng * 0.7) * 0.5, 0);
    blur = 0.3;
  } else if (mode === 'garage') {
    if (!garageDrag) garageAng += dt * 0.25;
    syncVisuals(dt, time);
    orbitCam(gCar, garageAng + gCar.h, 5.8, 1.6, 1.3);
  } else if (mode === 'count') {
    countT -= dt;
    const n = Math.ceil(countT);
    if (n !== countN && n >= 0) { countN = n; $('count').textContent = n > 0 ? n : 'GO !'; audio.beep(n > 0 ? 440 : 880, n > 0 ? 0.12 : 0.4); }
    if (n > 3) $('count').textContent = '';
    syncVisuals(dt, time); chaseCam(player, dt, chaseSnap); chaseSnap = false;
    if (countT <= 0) { mode = 'race'; raceTime = 0; setTimeout(() => { $('count').hidden = true; }, 700); }
    audio.engine(0.02, keys.gas ? 1 : 0.2, true);
  } else if (mode === 'race' || mode === 'finished') {
    simulate(dt); syncVisuals(dt, time);
    if (mode === 'race') chaseCam(player, dt, false);
    else { demoAng += dt * 0.3; orbitCam(player, player.h + Math.PI + Math.sin(demoAng) * 0.8, 7, 2.0, 0); }
    blur = clamp(player.v / 80, 0, 1);
    audio.engine(clamp(Math.abs(player.v) / player.phys.vmax, 0, 1), player.inp ? player.inp.gas : 0, mode === 'race');
    hudT -= dt;
    $('h-kmh').textContent = Math.round(Math.abs(player.v) * 3.6);
    if (hudT <= 0) {
      hudT = 0.15;
      if (mode === 'race') {
        $('h-pos').textContent = standings().indexOf(player) + 1;
        $('h-lap').textContent = clamp(player.lap, 1, setup.laps);
        $('h-time').textContent = fmt(raceTime);
        const wrong = Math.cos(wrap(player.h - T.heading(player.idx))) < -0.3 && Math.abs(player.v) > 3;
        $('wrong').hidden = !wrong;
        drawMini();
      } else fillResults();
    }
    if (flashT > 0) { flashT -= dt; if (flashT <= 0) $('h-msg').textContent = ''; }
  }
  if (world.rain && world.rain.visible) {
    const f = cars[0] && (mode === 'race' || mode === 'count') ? player : cars[0];
    world.updateRain(camera, dt, f ? -Math.sin(f.h) * f.v : 0, f ? -Math.cos(f.h) * f.v : 0);
  }
  if (composer) { finalPass.uniforms.amt.value = blur; composer.render(dt); } else renderer.render(scene, camera);
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
  get mode() { return mode; }, get cars() { return cars; }, get T() { return T; }, setup, opts,
  // avance la simulation sans rendu (tests)
  run(seconds, step = 1 / 60) { for (let t = 0; t < seconds; t += step) simulate(step); return cars.map(c => ({ n: c.name, lap: c.lap, idx: c.idx, v: +c.v.toFixed(1), best: c.bestLap })); },
  go(m) { mode = m; },
  get world() { return world; }, scene, camera, renderer,
  freeze() { mode = 'count'; countT = 1e9; countN = 99; chaseSnap = true; $('count').hidden = true; },
};
(async () => {
  try {
    setupPost();
    await resolveModels();
    await new Promise(r => setTimeout(r, 30));
    ensureTrack(setup.track);
    buildSetup(); buildOptions(); syncPads(); audio.setSound(opts.son);
    toMenu();
    requestAnimationFrame(frame);
    window.__booted = true;
  } catch (e) {
    console.error(e);
    $('loadmsg').textContent = 'Le jeu ne peut pas démarrer sur cet appareil (WebGL indisponible).';
    window.__err = String(e && e.stack || e);
  }
})();
