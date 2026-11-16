// Nuit Néon — boucle de jeu, conduite, adversaires, modes de jeu et menus.
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { ShaderPass } from 'three/addons/postprocessing/ShaderPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { TRACKS, ROUTE_LIBRE, ROUTE_LIGNE, ambiance } from './tracks.js';
import { buildWorld, makeTrack, canvasTex } from './world.js';
import { buildArena, ARENA, TEAM_COL } from './arena.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { CARS, STAT_KEYS, PAINTS, physFromStats, createCar, resolveModels, setPaint, animateCar, disposeCar, setLod, setGhost } from './cars.js';
import { TRAFIC, TRAFIC_PAINTS } from './carlist.js';
import { FX, Skids } from './fx.js';
import * as music from './music.js';
import { CHAMPS, REGLES_BASE, defautCustom, borner, circuitDe } from './custom.js';
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
const opts = Object.assign({ qual: 'auto', autoGas: false, son: true, vue: 'ext', musique: 'alea', volMus: 0.5 }, store.get('opts', {}));
if (opts.v !== 2) { opts.autoGas = false; opts.v = 2; }
const setup = Object.assign({ track: 0, diff: 1, laps: 3, bots: 7, team: 2, dur: 180, route: 'libre', obst: 1, amb: 'def' }, store.get('setup', {}));
// économie : crédits gagnés en jouant, véhicules achetés, niveaux de réglage débloqués, paliers du moteur extrême
const ecoSaved = store.get('eco', null);
const eco = Object.assign({ cash: 3000, owned: {}, caps: {}, extreme: 0 }, ecoSaved || {});
if (!ecoSaved) { const cid = store.get('car', null); if (cid) eco.owned[cid] = true; }   // la voiture déjà choisie reste acquise
const owned = (def) => !def.prix || !!eco.owned[def.id];
const saveEco = () => store.set('eco', eco);
const cr = (n) => Math.round(n).toLocaleString('fr-FR') + ' CR';
const EXTREMES = [[500, 2000], [1000, 6000], [2500, 15000], [5000, 30000], [10000, 60000]];   // [km/h, prix]
// circuits de l'atelier : trois emplacements, ajoutés à la suite des circuits du jeu
const NB_BASE = TRACKS.length, THEMES = TRACKS.map(t => t.theme), THEME_NOMS = TRACKS.map(t => t.nom);
const customs = store.get('customs', null) || [0, 1, 2].map(defautCustom);
function syncCustoms() { TRACKS.length = NB_BASE; customs.forEach((c, i) => TRACKS.push(circuitDe(c, i, THEMES))); }
syncCustoms();
let RU = REGLES_BASE;   // règles physiques du circuit en cours
const best = store.get('best3', {}); // records (route élargie)
let selCar = params.get('modele') ? 0 : Math.max(0, CARS.findIndex(c => c.id === store.get('car', CARS[0].id)));
if (params.get('q')) opts.qual = params.get('q');
setup.track = clamp(setup.track | 0, 0, TRACKS.length - 1);
const carTune = (def) => { const t = tuning[def.id] || {}; return { vmaxKmh: Math.min(t.vmaxKmh || 0, eco.extreme), accelX: eco.extreme ? t.accelX || 1 : 1 }; };
const carStats = (def) => Object.assign({}, def.stats, (tuning[def.id] || {}).stats);
const carColor = (def) => (tuning[def.id] && tuning[def.id].couleur !== undefined) ? tuning[def.id].couleur : def.couleur;
const DIFFS = [
  { nom: 'Facile', speed: 0.80, corner: 0.60 }, { nom: 'Normal', speed: 0.88, corner: 0.74 },
  { nom: 'Difficile', speed: 0.95, corner: 0.87 }, { nom: 'Expert', speed: 1.0, corner: 0.97 },
  { nom: 'Légende', speed: 1.0, corner: 1.03, bonus: 1.07 },   // adversaires un peu plus puissants que leur voiture
];
const BOT_NAMES = ['Kaïto', 'Luna', 'Rex', 'Mila', 'Zéphyr', 'Nova', 'Axel', 'Iris', 'Oslo', 'Vega', 'Niko', 'Jade'];
const ITEM_NAMES = { turbo: 'Turbo', missile: 'Missile', huile: "Flaque d'huile", bouclier: 'Bouclier', eclair: 'Éclair', aimant: 'Aimant', saut: 'Super saut', mine: 'Mine', onde: 'Onde de choc',
  fantome: 'Fantôme', geant: 'Géant', gel: 'Gel', teleport: 'Téléporteur', triple: 'Triple missile', nitro: 'Nitro' };
const DIFF_GAIN = [0.7, 1, 1.3, 1.6, 2];

// ---------- rendu ----------
const app = $('app'), canvas = $('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance', preserveDrawingBuffer: params.has('shot') });
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 0.95;
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(55, 1, 0.1, 2500);
camera.layers.enable(1);
const fx = new FX(scene), skids = new Skids(scene), scratches = new Skids(scene, 500);
scratches.mesh.material.opacity = 0.6; scratches.mesh.material.polygonOffset = true; scratches.mesh.material.polygonOffsetFactor = -2;

const QUALITY = {
  haute: { pr: 2, refl: 0.55, bloom: true, msaa: 4, taps: 9, env: 256, rain: true, grass: 6000, terrain: 170 },
  moyenne: { pr: 1.5, refl: 0.45, bloom: true, msaa: 0, taps: 7, env: 128, rain: true, grass: 3800, terrain: 140 },
  basse: { pr: 1, refl: 0.33, bloom: false, msaa: 0, taps: 5, env: 64, rain: false, grass: 1100, terrain: 90 },
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
    uniforms: { tDiffuse: { value: null }, amt: { value: 0 }, time: { value: 0 }, sunPos: { value: new THREE.Vector2() }, sunVis: { value: 0 }, sunCol: { value: new THREE.Color() }, aspect: { value: 1 } },
    vertexShader: 'varying vec2 vUv; void main(){ vUv=uv; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.); }',
    fragmentShader: `uniform sampler2D tDiffuse; uniform float amt; uniform float time; uniform vec2 sunPos; uniform float sunVis; uniform vec3 sunCol; uniform float aspect; varying vec2 vUv;
      float hsh(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
      void main(){ vec2 c = vec2(0.5,0.56); vec2 d = vUv - c; float r = length(d*vec2(1.0,1.25));
        float k = smoothstep(0.28, 0.85, r) * 0.05 * amt;
        vec3 col = vec3(0.); for(int i=0;i<8;i++){ float t = float(i)/7.; col += texture2D(tDiffuse, vUv - d*k*t).rgb; } col /= 8.;
        col.r = mix(col.r, texture2D(tDiffuse, vUv - d*0.006*r).r, 0.7); col.b = mix(col.b, texture2D(tDiffuse, vUv + d*0.006*r).b, 0.7);
        if (sunVis > 0.0) {
          // le soleil n'éblouit que s'il est visible : on regarde la luminosité de l'image à son emplacement (masqué par un arbre, il disparaît)
          float occ = 0.0; for (int i = 0; i < 5; i++) { vec2 o = vec2(float(i - 2)*0.006, float(i - 2)*0.004); occ += smoothstep(1.2, 3.5, dot(texture2D(tDiffuse, sunPos + o).rgb, vec3(0.33))); } occ /= 5.0;
          vec2 sd = (vUv - sunPos) * vec2(aspect, 1.0); float sl = length(sd);
          vec3 fl = sunCol * (exp(-sl*5.0)*0.45 + exp(-sl*28.0)*0.8);                          // halo
          fl += sunCol * exp(-abs(sd.y)*90.0) * exp(-abs(sd.x)*2.2) * 0.35;                    // traînée horizontale
          vec2 axis = vec2(0.5) - sunPos;
          for (int i = 0; i < 4; i++) { float k = 0.55 + float(i)*0.45; vec2 gp = sunPos + axis * k * 2.0; float gs = 0.025 + 0.03*float(i);
            float gd = length((vUv - gp) * vec2(aspect, 1.0)); vec3 gc = i == 0 ? vec3(0.9,0.5,0.2) : i == 1 ? vec3(0.3,0.6,0.9) : i == 2 ? vec3(0.6,0.9,0.4) : vec3(0.9,0.3,0.6);
            fl += gc * sunCol * smoothstep(gs, gs*0.6, gd) * 0.07 + gc * sunCol * smoothstep(gs*0.15, 0.0, abs(gd - gs)) * 0.05; }   // reflets dans les lentilles
          col += fl * occ * sunVis;
        }
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
  const base = key; key = key === 'arena' ? key : key + '|' + setup.amb;
  if (worldKey === key) return;
  if (world) { scene.remove(world.group); world.dispose(); }
  fx.clear();
  const bd = base === 'libre' ? ROUTE_LIBRE : base === 'ligne' ? ROUTE_LIGNE : TRACKS[+base];
  world = key === 'arena' ? buildArena(renderer, Q) : buildWorld(Object.assign({}, bd, { theme: ambiance(bd.theme, setup.amb) }), renderer, Q);
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
const G = { type: 'demo', boost: true, drain: 30, regen: 7, score: [0, 0], freeze: 0, timeLeft: 0, boxes: [], shots: [], oils: [], traffic: [], obst: [], dist: 0, ended: false, dr: { cur: 0, total: 0, hold: 0 }, paid: false, sonic: false };
const objs = new THREE.Group(); scene.add(objs);
let cars = [], player = null, raceTime = 0, mode = 'loading', countT = 0, countN = 0, finishOrder = 0;
const tmp = { x: 0, z: 0 };
const ball = { x: 0, y: 2, z: 0, vx: 0, vy: 0, vz: 0, last: null };
function clearGame() {
  for (const c of cars) { scene.remove(c.vis.root); disposeCar(c.vis); }
  for (const t of G.traffic) { scene.remove(t.vis.root); disposeCar(t.vis); }
  cars = []; player = null; G.traffic = [];
  while (objs.children.length) { const o = objs.children.pop(); if (o.geometry && !o.userData.shared) o.geometry.dispose(); }
  G.boxes = []; G.shots = []; G.oils = []; G.obst = []; fx.clear(); skids.clear(); scratches.clear();
}
function makeCarEntity(def, paint, isPlayer, name, skill) {
  const vis = createCar(def, paint);
  if (vis.beam) vis.beam.visible = !!world.lights;
  vis.lights = !!world.lights;
  vis.wet = world.arena ? 0 : (world.theme.wet ?? 1);
  vis.root.rotation.order = 'YXZ';
  if (vis.neon) vis.neon.visible = !!world.lights;
  const phys = physFromStats(carStats(def), isPlayer ? carTune(def) : null), kv = RU.vitesse;
  phys.vmax *= kv; phys.accel *= kv; phys.brake *= Math.max(1, kv); phys.grip *= RU.adherence * Math.max(1, kv) * (world.theme && world.theme.orage ? 0.82 : 1);   // sous l'orage, la piste détrempée accroche moins
  const c = { def, vis, phys, yawK: 0, ghostT: 0, giantT: 0, iceT: 0, magnetT: 0, nitroT: 0, scale: 1, driftA: 0, mass: def.moto ? 0.35 : Math.max(0.6, vis.half * vis.len / 2.1), sk: [null, null], scr: null, rubT: 0, drifting: false, x: 0, z: 0, h: 0, y: 0, vy: 0, pitch: 0, v: 0, u: 0, steer: 0, idx: 0, lap: 0, total: 0,
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
    if (!isP && sk.bonus) { c.phys.vmax *= sk.bonus; c.phys.accel *= sk.bonus; c.phys.grip *= sk.bonus; }
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
  for (const o of G.obst) { if (o.def.zone || o.state) continue; const d = ((o.idx - c.idx) % N + N) % N; if (d < 20 && Math.abs(o.lat - myLat) < o.def.r + 1.7) target = clamp(o.lat + (myLat >= o.lat ? 1 : -1) * (o.def.r + 2.5), -(lim - 1.4), lim - 1.4); }
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
const velOf = (c) => { const sh = Math.sin(c.h), ch = Math.cos(c.h); return [-sh * c.v + ch * c.u, -ch * c.v - sh * c.u]; };
const setVel = (c, vx, vz) => { const sh = Math.sin(c.h), ch = Math.cos(c.h); c.v = -sh * vx - ch * vz; c.u = ch * vx - sh * vz; };
function stepCar(c, inp, dt) {
  const P = c.phys;
  if (c.spinT > 0) { c.spinT -= dt; c.spinA += dt * 10; inp = { steer: 0, gas: 0, brake: 0.25 }; if (c.spinT <= 0) c.spinA = 0; }
  if (c.shieldT > 0) c.shieldT -= dt;
  if (c.ghostT > 0) c.ghostT -= dt; if (c.giantT > 0) c.giantT -= dt; if (c.iceT > 0) c.iceT -= dt; if (c.magnetT > 0) c.magnetT -= dt; if (c.nitroT > 0) c.nitroT -= dt;
  const sc = c.giantT > 0 ? 1.75 : 1; c.scale += (sc - c.scale) * Math.min(1, 6 * dt);
  const rate = inp.steer === 0 ? 7 : 4.5;
  c.steer += clamp(inp.steer - c.steer, -rate * dt, rate * dt);
  let boosting = false;
  if (c.turboT > 0 || c.nitroT > 0) { c.turboT -= dt; boosting = true; }
  else if (inp.boost && c.boost > 1 && G.boost) { boosting = true; c.boost = Math.max(0, c.boost - G.drain * dt); }
  else if (G.regen) c.boost = Math.min(100, c.boost + G.regen * dt);
  c.boosting = boosting;
  const bk = RU.boost * (c.nitroT > 0 ? 2.4 : 1);
  const vmax = P.vmax * (boosting ? 1 + 0.25 * bk : 1), acc = P.accel + (boosting ? Math.max(14, P.accel * 0.6) * bk : 0), air = c.y > 0.01;
  let v = c.v; const v0 = v;
  const gas = boosting ? 1 : (inp.gas || 0), brake = inp.brake || 0;
  if (!air) {
    if (gas > 0 && v > -0.5) v += acc * gas * (1 - Math.pow(Math.max(0, v) / vmax, 2)) * dt;
    if (brake > 0) { if (v > 0.5) v = Math.max(0, v - P.brake * brake * dt); else v = Math.max(-9, v - 7 * brake * dt); }
    // frein moteur et résistance de l'air (ramenée à la vitesse de pointe du véhicule, pour rester la même à 300 comme à 10 000 km/h)
    if (gas <= 0 && brake <= 0) { const kd = 72 / Math.max(72, P.vmax), d = (4.5 + 0.0016 * kd * kd * v * v) * dt; v = Math.abs(v) < d ? 0 : v - Math.sign(v) * d; }
    if (gas > 0 && v < -0.5) v = Math.min(0, v + 14 * dt);
  } else {
    // en l'air : le boost pousse dans l'axe du nez, ce qui permet de voler
    if (boosting) { v = Math.min(P.vmax * 1.3, v + 30 * Math.cos(c.pitch) * dt); c.vy += 30 * Math.sin(c.pitch) * dt; }
    v -= v * 0.12 * dt;
  }
  const sp = Math.abs(v);
  let yaw, excess = 0, drift = false;
  if (!air) {
    // dérapage (frein à main) : l'arrière décroche, la voiture pivote plus vite qu'elle ne tourne et glisse vers l'extérieur
    drift = !!inp.drift && sp > 7 && v > 0;
    const grip = P.grip * (c.iceT > 0 ? 0.16 : 1);
    const yawMax = Math.min(P.steer * Math.min(1, sp / 7) * (drift ? 1.15 : 1), grip * (drift ? 1.85 : 1.3) / Math.max(sp, 1));
    yaw = c.steer * yawMax * (v < 0 ? -1 : 1);
    excess = Math.max(0, Math.abs(yaw) * sp - grip * (drift ? 1.85 : 1));
    c.u -= Math.sign(yaw) * excess * 0.9 * dt;
    if (drift) c.u += (-c.steer * Math.min(7, sp * 0.14) - c.u) * Math.min(1, 3 * dt);   // l'arrière chasse : légère glissade vers l'extérieur
    else c.u *= Math.exp(-(c.iceT > 0 ? 0.8 : 3.5) * dt);
    v -= Math.sign(v) * (excess * 0.25 + (drift ? 6 * Math.abs(c.steer) : 0)) * dt;
  } else yaw = c.steer * 1.9;
  // angle de dérive affiché : le nez pointe vers l'intérieur du virage pendant que la voiture glisse
  c.driftA += ((drift ? -c.steer * 0.55 * Math.min(1, sp / 22) : 0) - c.driftA) * Math.min(1, (drift ? 4 : 6) * dt);
  c.slip = excess; c.drifting = drift && Math.abs(c.driftA) > 0.12;
  c.h = wrap(c.h - yaw * dt + c.yawK * dt); c.yawK *= Math.exp(-5 * dt);
  const sh = Math.sin(c.h), ch = Math.cos(c.h);
  c.x += (-sh * v + ch * c.u) * dt; c.z += (-ch * v - sh * c.u) * dt;
  c.v = v;
  const foot = G.type === 'foot';
  if (foot) {
    const jd = inp.jump && !c.prevJump; c.prevJump = !!inp.jump;
    if (jd && c.jumps < 2) { c.vy = (c.jumps === 0 ? 10 : 8.5) * RU.saut; c.jumps++; c.y = Math.max(c.y, 0.02); }
  }
  if (c.y > 0 || c.vy > 0) {
    c.vy -= 24 * RU.gravite * dt; c.y += c.vy * dt;
    const tp = foot ? (inp.jump ? 1.0 : (c.vy < 0 ? -0.2 : 0.12)) : clamp(Math.atan2(c.vy, Math.max(14, sp)) * 0.8, -0.45, 0.45);
    c.pitch += (tp - c.pitch) * Math.min(1, 5 * dt);
    if (foot && c.y > ARENA.ceil - 1.6) { c.y = ARENA.ceil - 1.6; c.vy = Math.min(0, c.vy); }
    if (c.y <= 0) { if (c.vy < -7) c.landed = -c.vy; c.y = 0; c.vy = 0; c.pitch = 0; c.jumps = 0; }
  }
  if (foot) arenaConstrain(c, dt); else trackConstrain(c, dt);
  c.lon = (c.v - v0) / dt; c.yaw = yaw; c.braking = brake > 0.05 && v0 > 0.5;
}
// choc contre un mur : la vitesse perpendiculaire rebondit (amortie), le frottement ralentit le long du mur, le nez se remet dans l'axe
function hitWall(c, nx, nz, dt, sp) {
  // nx, nz : normale du mur, pointant vers l'extérieur du terrain
  let [vx, vz] = velOf(c);
  const vn = vx * nx + vz * nz;
  if (vn > 0) {
    vx -= 1.22 * vn * nx; vz -= 1.22 * vn * nz;
    const tx = -nz, tz = nx, vt = vx * tx + vz * tz, fr = Math.min(Math.abs(vt), 0.3 * vn);
    vx -= Math.sign(vt) * fr * tx; vz -= Math.sign(vt) * fr * tz;
    if (vn > 2.5) {
      c.bump = Math.max(c.bump || 0, Math.min(1.2, vn / 16));
      const n = Math.min(14, vn * 0.6); for (let k = 0; k < n; k++) fx.spark(c.x + nx * c.vis.half, 0.5, c.z + nz * c.vis.half, vx * 0.4, 1.5, vz * 0.4);
      if (c.isPlayer) audio.thud(vn / 24);
      // l'impact fait pivoter la voiture selon le côté touché
      const side = Math.cos(c.h) * nx - Math.sin(c.h) * nz;
      c.yawK = clamp(c.yawK + side * Math.min(2, vn * 0.08) * Math.sign(c.v || 1), -3, 3);
    }
    setVel(c, vx, vz);
  }
  c.v *= Math.exp(-(0.45 + 0.9 * Math.max(0, vn) / Math.max(4, Math.abs(c.v))) * dt);   // la tôle frotte : plus on appuie contre le mur, plus on ralentit
  const fx_ = -Math.sin(c.h) * Math.sign(c.v || 1), fz_ = -Math.cos(c.h) * Math.sign(c.v || 1), into = fx_ * nx + fz_ * nz;
  if (into > 0) {
    let tx = fx_ - nx * into, tz = fz_ - nz * into; const tl = Math.hypot(tx, tz);
    if (tl > 0.05) { tx /= tl; tz /= tl; const th = Math.atan2(-tx * Math.sign(c.v || 1), -tz * Math.sign(c.v || 1)); c.h = wrap(c.h + wrap(th - c.h) * Math.min(1, 3.5 * dt)); }
  }
  c.onWall = true; void sp;
}
function trackConstrain(c, dt) {
  const prev = c.idx, N = T.N, sp = Math.abs(c.v);
  c.idx = T.nearest(c.x, c.z, c.idx);
  const d = T.lateral(c.x, c.z, c.idx), i = c.idx;
  c.offroad = false;
  // le mur s'arrête au ras de la carrosserie : une moto passe plus près qu'une voiture
  const lim = T.libre ? T.limit : Math.max(1, T.half - 0.06 - c.vis.half * c.scale);
  if (Math.abs(d) > lim) {
    const s = Math.sign(d), push = Math.abs(d) - lim;
    T.point(i, s * (lim + c.vis.half * c.scale + 0.02), tmp); c.wx = tmp.x; c.wz = tmp.z;
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
// Collisions : chaque véhicule est une suite de disques le long de sa caisse. Au contact, on sépare les deux véhicules selon leur masse,
// on échange de la quantité de mouvement le long de la normale (avec rebond et frottement) et le point d'impact fait pivoter chacun.
function discs(c) { const k = c.scale || 1, r = Math.max(0.45, c.vis.half * 0.95) * k, L = Math.max(0, c.vis.len * k - r); return { r, offs: L < 0.3 ? [0] : c.vis.moto ? [-L, L] : [-L, 0, L] }; }
function collidePair(a, b) {
  if (Math.abs(a.x - b.x) > 9 || Math.abs(a.z - b.z) > 9 || Math.abs(a.y - b.y) > 1.6) return;
  const A = discs(a), B = discs(b), sa = Math.sin(a.h), ca = Math.cos(a.h), sb = Math.sin(b.h), cb = Math.cos(b.h);
  let pen = 0, nx = 0, nz = 0, px = 0, pz = 0;
  for (const fa of A.offs) for (const fb of B.offs) {
    const ax = a.x - sa * fa, az = a.z - ca * fa, dx = b.x - sb * fb - ax, dz = b.z - cb * fb - az, d = Math.hypot(dx, dz), q = A.r + B.r - d;
    if (q > pen && d > 1e-4) { pen = q; nx = dx / d; nz = dz / d; px = ax + nx * A.r; pz = az + nz * A.r; }
  }
  if (!pen) return;
  const ma = a.mass * a.scale ** 3, mb = b.rail ? 1e6 : b.mass * (b.scale || 1) ** 3, im = 1 / ma + 1 / mb;
  a.x -= nx * pen / ma / im; a.z -= nz * pen / ma / im;
  if (!b.rail) { b.x += nx * pen / mb / im; b.z += nz * pen / mb / im; }
  const [avx, avz] = velOf(a), [bvx, bvz] = b.rail ? [-sb * b.v, -cb * b.v] : velOf(b);
  // carrosseries qui se frottent : étincelles au point de contact tant que les deux glissent l'une contre l'autre
  { const slide = Math.abs((bvx - avx) * -nz + (bvz - avz) * nx); if (slide > 3) { a.rubT = 0.12; if (!b.rail) b.rubT = 0.12; if (Math.random() < 0.6) fx.ember(px, 0.55 + a.y, pz, (avx + bvx) * 0.35, 1, (avz + bvz) * 0.35); } }
  const rel = (bvx - avx) * nx + (bvz - avz) * nz;
  if (rel >= 0) return;
  const j = -1.28 * rel / im, tx = -nz, tz = nx, jt = clamp(-((bvx - avx) * tx + (bvz - avz) * tz) * 0.3 / im, -0.45 * j, 0.45 * j);
  const ix = nx * j + tx * jt, iz = nz * j + tz * jt;                                // impulsion reçue par b ; a reçoit l'opposé
  setVel(a, avx - ix / ma, avz - iz / ma);
  a.yawK = clamp(a.yawK + ((pz - a.z) * -ix - (px - a.x) * -iz) / (ma * 3.4), -3, 3);
  if (b.rail) { b.stun = 1.6; b.cur *= 0.5; }
  else { setVel(b, bvx + ix / mb, bvz + iz / mb); b.yawK = clamp(b.yawK + ((pz - b.z) * ix - (px - b.x) * iz) / (mb * 3.4), -3, 3); }
  const hit = j / Math.min(ma, mb);                                                  // variation de vitesse du plus léger, en m/s
  if (hit > 1.5) { const n = Math.min(16, hit * 0.9); for (let k = 0; k < n; k++) fx.spark(px, 0.55 + a.y, pz, (avx + bvx) * 0.2, 2, (avz + bvz) * 0.2); }
  for (const c of [a, b]) if (c.isPlayer) { c.bump = Math.max(c.bump || 0, Math.min(1, hit / 14)); audio.thud(hit / 20); if (b.rail && hit > 3) flash('Attention à la circulation'); }
  // un géant envoie valser ce qu'il touche
  if (a.giantT > 0 && !(b.giantT > 0) && !b.rail && b.spinT <= 0) strike(b, false); else if (b.giantT > 0 && !(a.giantT > 0) && a.spinT <= 0) strike(a, false);
}
function collide(list) {
  for (let i = 0; i < list.length; i++) { const a = list[i]; if (a.ghostT > 0) continue;
    for (let j = i + 1; j < list.length; j++) if (!(list[j].ghostT > 0)) collidePair(a, list[j]);
    for (const t of G.traffic) collidePair(a, t); }
}
function onLap(c) {
  if (G.type === 'demo') { if (c.lap > 1) c.lap = 1; return; }
  if (c.finished) return;
  if (c.lap >= 2) {
    const lt = raceTime - c.lapStart;
    if (!c.bestLap || lt < c.bestLap) c.bestLap = lt;
    if (c.isPlayer) {
      const id = TRACKS[setup.track].id;
      const rec = (G.type === 'race' || G.type === 'chrono') && (!best[id] || lt < best[id]);
      if (rec) { best[id] = lt; store.set('best3', best); }
      flash((rec ? 'Record du circuit ' : 'Tour ') + fmt(lt));
    }
  }
  c.lapStart = raceTime;
  if (c.lap > G.laps) {
    c.finished = true; c.finishTime = raceTime; c.rank = ++finishOrder;
    if (c.isPlayer) finishRace();
  } else if (c.isPlayer && c.lap === G.laps && G.laps > 1) flash('Dernier tour');
}
const standings = () => cars.slice().sort((a, b) => {
  if (a.out || b.out) return a.out && b.out ? a.rank - b.rank : a.out ? 1 : -1;      // éliminés en bas du classement
  return (a.finished && b.finished) ? a.rank - b.rank : a.finished ? -1 : b.finished ? 1 : b.total - a.total; });

// ---------- élimination : toutes les 20 secondes, le dernier est éliminé ----------
const ELIM_DELAI = 20;
function stepElim(dt) {
  G.elimT -= dt;
  if (player && !player.out && G.elimT < 5.5 && mode === 'race') flash('Élimination dans ' + Math.ceil(G.elimT) + ' s');
  if (G.elimT > 0) return;
  G.elimT = ELIM_DELAI;
  const alive = standings().filter(c => !c.out);
  const last = alive[alive.length - 1];
  last.out = true; last.rank = alive.length; last.vis.root.visible = false;
  audio.boom(0.7); fx.burst(last.x, 1, last.z, 3.5, 1.2, 0.3, 80, 18); for (let i = 0; i < 10; i++) fx.smoke(last.x, 1, last.z, 0, 3, 0, 2, 0.4, 1.6);
  if (last.isPlayer) return finishElim(false);
  flash(last.name + ' est éliminé');
  if (alive.length - 1 === 1) { player.rank = 1; finishElim(true); }
}
function finishElim(win) {
  mode = 'finished';
  $('rtitle').textContent = win ? 'Victoire !' : 'Éliminé';
  $('rsub').textContent = win ? 'Dernier survivant après ' + fmtShort(raceTime) : 'Tu termines ' + ordinal(player.rank) + ' sur ' + cars.length;
  fillResults(); show('results'); showGains(); audio.engine(null); audio.skid(0); audio.scrape(0);
}

// ---------- course à bonus : caisses, missiles, flaques, boucliers ----------
let mineGeo = null, mineMat = null, boxGeo = null, boxMat = null, oilGeo = null, oilMat = null, shotGeo = null, shotMat = null, shieldGeo = null, shieldMat = null;
function itemAssets() {
  if (boxGeo) return;
  const t = canvasTex(128, 128, (g) => { const gr = g.createLinearGradient(0, 0, 128, 128); gr.addColorStop(0, '#ff2bd0'); gr.addColorStop(0.5, '#ffd23f'); gr.addColorStop(1, '#19e6ff'); g.fillStyle = gr; g.fillRect(0, 0, 128, 128);
    g.fillStyle = '#05040c'; g.fillRect(10, 10, 108, 108); g.fillStyle = '#fff'; g.font = '900 96px Arial, sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText('?', 64, 70); });
  boxGeo = new THREE.BoxGeometry(1.5, 1.5, 1.5); boxMat = new THREE.MeshBasicMaterial({ map: t, color: new THREE.Color(1.6, 1.6, 1.6) });
  oilGeo = new THREE.CircleGeometry(2.1, 20).rotateX(-Math.PI / 2); oilMat = new THREE.MeshStandardMaterial({ color: 0x050308, roughness: 0.05, metalness: 1, transparent: true, opacity: 0.92, envMapIntensity: 3 });
  shotGeo = new THREE.ConeGeometry(0.28, 1.5, 8).rotateX(-Math.PI / 2); shotMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(3, 0.5, 0.2) });
  mineGeo = new THREE.SphereGeometry(0.42, 12, 8); mineMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(3.2, 0.25, 0.15) });
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
const pick = (w) => { let t = 0; for (const k in w) t += w[k]; let r = Math.random() * t; for (const k in w) { r -= w[k]; if (r <= 0) return k; } return 'turbo'; };
function rollItem(c) {
  const rank = standings().indexOf(c) / Math.max(1, cars.length - 1); // 0 = en tête
  if (rank < 0.2) return pick({ huile: 18, mine: 18, bouclier: 18, fantome: 10, onde: 10, saut: 6, turbo: 14, geant: 6 });
  if (rank > 0.6) return pick({ missile: 10, triple: 12, eclair: 10, nitro: 12, teleport: 10, aimant: 10, geant: 10, gel: 8, turbo: 10, bouclier: 6 });
  return pick({ missile: 14, turbo: 12, huile: 8, bouclier: 8, mine: 8, onde: 8, saut: 6, fantome: 7, geant: 7, gel: 6, aimant: 6, triple: 5, nitro: 5 });
}
const aheadOf = (c, o) => { const d = ((o.total - c.total) % T.N + T.N) % T.N; return d > 0 && d < T.N * 0.5 ? d * T.ds : -1; };   // distance en mètres si o est devant c
function bolt(o) { for (let k = 0; k < 14; k++) fx.flame(o.x + (Math.random() - 0.5) * 0.8, 0.4 + k * 1.1, o.z + (Math.random() - 0.5) * 0.8, 0, 0, 0, 3.2, 1); fx.burst(o.x, 1, o.z, 1.5, 2.2, 4, 30, 14); }
function useItem(c) {
  const it = c.item; if (!it) return; c.item = null;
  const others = cars.filter(o => o !== c && !o.out);
  if (it === 'turbo') c.turboT = 2.6;
  else if (it === 'nitro') c.nitroT = 4.5;
  else if (it === 'bouclier') c.shieldT = 8;
  else if (it === 'fantome') c.ghostT = 6;
  else if (it === 'geant') c.giantT = 7;
  else if (it === 'aimant') c.magnetT = 4;
  else if (it === 'saut') { c.vy = 12 * RU.saut; c.y = Math.max(c.y, 0.05); }
  else if (it === 'huile' || it === 'mine') {
    const mine = it === 'mine', m = new THREE.Mesh(mine ? mineGeo : oilGeo, mine ? mineMat : oilMat); m.userData.shared = true; const x = c.x + Math.sin(c.h) * 4.5, z = c.z + Math.cos(c.h) * 4.5;
    m.position.set(x, mine ? 0.3 : 0.04, z); m.layers.set(1); m.renderOrder = 3; objs.add(m);
    G.oils.push({ x, z, idx: c.idx, lat: T.lateral(x, z, c.idx), life: mine ? 45 : 30, mesh: m, owner: c, arm: mine ? 1.2 : 0.8, mine });
  } else if (it === 'missile' || it === 'triple') {
    let target = null, bd = 1e9;
    for (const o of others) { const d = aheadOf(c, o); if (d > 0 && d < bd) { bd = d; target = o; } }
    for (const off of (it === 'triple' ? [-3, 0, 3] : [0])) { const m = new THREE.Mesh(shotGeo, shotMat); m.userData.shared = true; objs.add(m);
      G.shots.push({ s: c.idx + 3, lat: T.lateral(c.x, c.z, c.idx) + off, target: off === 0 ? target : null, owner: c, life: 7, mesh: m }); }
  } else if (it === 'eclair') {
    // la foudre frappe tous ceux qui sont devant (ou tout le monde si on est en tête)
    let v = others.filter(o => aheadOf(c, o) > 0); if (!v.length) v = others;
    for (const o of v) { bolt(o); strike(o, false); }
    if (v.some(o => o.isPlayer) || c.isPlayer) audio.thud(1);
  } else if (it === 'gel') {
    let v = others.filter(o => { const d = aheadOf(c, o); return d > 0 && d < 220; }); if (!v.length) v = others.filter(o => Math.hypot(o.x - c.x, o.z - c.z) < 70);
    for (const o of v) { if (o.shieldT > 0) { o.shieldT = 0; continue; } o.iceT = 4.5; fx.burst(o.x, 0.8, o.z, 0.6, 1.6, 3.2, 26, 9); if (o.isPlayer) flash('Verglas !'); }
  } else if (it === 'onde') {
    fx.burst(c.x, 0.7, c.z, 0.5, 1.6, 3.2, 110, 34);
    for (const o of others) { const dx = o.x - c.x, dz = o.z - c.z, d = Math.hypot(dx, dz); if (d > 26 || d < 0.01 || o.ghostT > 0) continue;
      if (o.shieldT > 0) { o.shieldT = 0; continue; }
      const k = 14 * (1 - d / 40), [vx, vz] = velOf(o); setVel(o, vx + dx / d * k, vz + dz / d * k); o.v *= 0.8; o.yawK += (Math.random() < 0.5 ? -1 : 1) * 2; if (o.isPlayer) { o.bump = 1; flash('Onde de choc !'); audio.thud(0.8); } }
    if (c.isPlayer) audio.thud(0.7);
  } else if (it === 'teleport') {
    const lat = clamp(T.lateral(c.x, c.z, c.idx), -(T.limit - 2), T.limit - 2), ni = c.idx + Math.round(75 / T.ds);
    fx.burst(c.x, 1, c.z, 2.4, 0.6, 3.4, 40, 12);
    if (ni >= T.N) { c.lap++; c.idx = ni - T.N; onLap(c); } else c.idx = ni;
    T.point(c.idx, lat, tmp); c.x = tmp.x; c.z = tmp.z; c.h = T.heading(c.idx); c.u = 0; c.total = (c.lap - 1) * T.N + c.idx;
    fx.burst(c.x, 1, c.z, 2.4, 0.6, 3.4, 40, 12); if (c.isPlayer) chaseSnap = true;
  }
  if (c.isPlayer) audio.beep(660, 0.08);
}
function strike(c, strong) {
  if (c.shieldT > 0) { c.shieldT = 0; fx.burst(c.x, 1, c.z, 0.4, 1.6, 3, 24, 12); return; }
  c.spinT = strong ? 1.3 : 1.0; c.v *= 0.55; c.turboT = 0;
  if (strong) { if (player && Math.hypot(c.x - player.x, c.z - player.z) < 90) audio.boom(c.isPlayer ? 1 : 0.5); fx.burst(c.x, 0.8, c.z, 3.5, 1.4, 0.3, 46, 16); for (let i = 0; i < 8; i++) fx.smoke(c.x, 0.8, c.z, 0, 3, 0, 1.6, 0.4, 1.4); }
  if (c.isPlayer) { flash(strong ? 'Touché !' : 'Dérapage !'); c.bump = 0.8; }
}
function stepItems(dt) {
  const N = T.N;
  for (const b of G.boxes) {
    if (b.t > 0) { b.t -= dt; b.mesh.visible = b.t <= 0; continue; }
    b.mesh.rotation.y += dt * 1.6; b.mesh.rotation.x += dt * 0.9; b.mesh.position.y = 1.3 + Math.sin(raceTime * 3 + b.idx) * 0.15;
    for (const c of cars) {
      if (c.out || Math.abs(c.x - b.x) > 2.3 * c.scale || Math.abs(c.z - b.z) > 2.3 * c.scale) continue;
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
    for (const c of cars) { if (c === s.owner || c.out || c.ghostT > 0) continue; if (Math.abs(c.x - x) < 3 && Math.abs(c.z - z) < 3 && c.y < 2.5) { strike(c, true); done = true; break; } }
    if (done) { objs.remove(s.mesh); G.shots.splice(i, 1); }
  }
  for (let i = G.oils.length - 1; i >= 0; i--) {
    const o = G.oils[i]; o.life -= dt; o.arm -= dt;
    let done = o.life <= 0;
    if (o.mine) o.mesh.scale.setScalar(1 + 0.25 * Math.sin(raceTime * 9));
    for (const c of cars) { if ((c === o.owner && o.arm > 0) || c.out || c.ghostT > 0 || c.y > 1) continue; if (Math.abs(c.x - o.x) < 2.3 && Math.abs(c.z - o.z) < 2.3 && c.spinT <= 0) { strike(c, !!o.mine); if (o.mine) { fx.burst(o.x, 0.6, o.z, 3.6, 1.2, 0.2, 70, 20); if (c.isPlayer) audio.thud(1); } done = true; break; } }
    if (done) { objs.remove(o.mesh); G.oils.splice(i, 1); }
  }
  for (const c of cars) {
    if (c.shieldT > 0 && !c.vis.shield) { c.vis.shield = new THREE.Mesh(shieldGeo, shieldMat); c.vis.shield.userData.shared = true; c.vis.shield.position.y = 0.8; c.vis.shield.layers.set(1); c.vis.root.add(c.vis.shield); }
    if (c.vis.shield) c.vis.shield.visible = c.shieldT > 0;
  }
  // aimant : accroché à la voiture de devant, on est tiré vers elle pendant qu'elle est retenue
  for (const c of cars) { if (!(c.magnetT > 0)) continue; let tg = null, bd = 110;
    for (const o of cars) { if (o === c || o.out) continue; const d = aheadOf(c, o); if (d > 0 && d < bd) { bd = d; tg = o; } }
    if (tg) { c.turboT = Math.max(c.turboT, 0.15); if (tg.v > 12) tg.v -= 7 * dt; const k = Math.random(); fx.flame(c.x + (tg.x - c.x) * k, 0.8, c.z + (tg.z - c.z) * k, 0, 0, 0, 1.4, 1); } }
  void N;
}

// ---------- obstacles : cônes et barrières qui s'envolent, caisses qui éclatent, piles de pneus, blocs de béton, tremplins, zones turbo ----------
const OB = { cone: { r: 0.42, leger: true }, barriere: { r: 0.9, leger: true }, caisse: { r: 0.8, casse: true }, pneus: { r: 0.85, e: 0.6 }, bloc: { r: 1.05, e: 0.12 }, tremplin: { zone: true }, turbo: { zone: true } };
let OA = null;
function obstacleAssets() {
  if (OA) return OA;
  const std = (map, o = {}) => new THREE.MeshStandardMaterial(Object.assign({ map, roughness: 0.75, metalness: 0.05 }, o));
  const geo = (g) => { g.userData.keep = true; return g; };
  OA = {};
  OA.cone = [geo(mergeGeometries([new THREE.ConeGeometry(0.3, 0.78, 14, 1, true).translate(0, 0.42, 0), new THREE.BoxGeometry(0.66, 0.05, 0.66).translate(0, 0.025, 0)])),
    std(canvasTex(8, 64, (g) => { g.fillStyle = '#ff5a10'; g.fillRect(0, 0, 8, 64); g.fillStyle = '#fff'; g.fillRect(0, 18, 8, 12); g.fillRect(0, 38, 8, 6); }), { emissive: 0x401000 })];
  OA.barriere = [geo(new THREE.BoxGeometry(2.0, 0.85, 0.42).translate(0, 0.43, 0)), std(canvasTex(128, 32, (g) => { for (let i = 0; i < 8; i++) { g.fillStyle = i % 2 ? '#f4f4f4' : '#d8121e'; g.fillRect(i * 16, 0, 16, 32); } }))];
  OA.caisse = [geo(new THREE.BoxGeometry(1.3, 1.3, 1.3).translate(0, 0.65, 0)), std(canvasTex(64, 64, (g) => { g.fillStyle = '#9a6a38'; g.fillRect(0, 0, 64, 64); g.strokeStyle = '#5a3a1c'; g.lineWidth = 3; for (let i = 0; i <= 4; i++) { g.beginPath(); g.moveTo(0, i * 16); g.lineTo(64, i * 16); g.stroke(); } g.lineWidth = 6; g.strokeRect(3, 3, 58, 58); g.beginPath(); g.moveTo(4, 4); g.lineTo(60, 60); g.stroke(); }))];
  OA.pneus = [geo(mergeGeometries([0.22, 0.64, 1.06].map(y => new THREE.TorusGeometry(0.52, 0.22, 8, 18).rotateX(Math.PI / 2).translate(0, y, 0)))), new THREE.MeshStandardMaterial({ color: 0x141418, roughness: 0.9 })];
  OA.bloc = [geo(new THREE.BoxGeometry(2.2, 0.9, 0.7).translate(0, 0.45, 0)), std(canvasTex(128, 64, (g) => { g.fillStyle = '#9a9aa0'; g.fillRect(0, 0, 128, 64); g.fillStyle = '#ffd23f'; g.fillRect(0, 8, 128, 22); g.fillStyle = '#15151a'; for (let i = -1; i < 9; i++) { g.beginPath(); g.moveTo(i * 20, 30); g.lineTo(i * 20 + 10, 30); g.lineTo(i * 20 + 22, 8); g.lineTo(i * 20 + 12, 8); g.fill(); } }))];
  const arrows = (bg, fg) => canvasTex(64, 128, (g) => { g.fillStyle = bg; g.fillRect(0, 0, 64, 128); g.strokeStyle = fg; g.lineWidth = 9; for (let i = 0; i < 4; i++) { const y = 28 + i * 32; g.beginPath(); g.moveTo(10, y); g.lineTo(32, y - 20); g.lineTo(54, y); g.stroke(); } });
  // tremplin : un plan incliné (l'avant du véhicule arrive par +z et monte vers -z)
  { const w = 2.6, l = 3.2, h = 1.5, P = [-w, 0, l, w, 0, l, -w, h, -l, w, h, -l,  -w, 0, -l, w, 0, -l], g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(P, 3)); g.setAttribute('uv', new THREE.Float32BufferAttribute([0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0], 2));
    g.setIndex([0, 1, 2, 1, 3, 2, 0, 2, 4, 1, 5, 3, 2, 3, 4, 3, 5, 4]); g.computeVertexNormals();
    OA.tremplin = [geo(g), new THREE.MeshStandardMaterial({ map: arrows('#141624', '#ffd23f'), roughness: 0.5, metalness: 0.3, emissive: 0x332a08, side: THREE.DoubleSide })]; }
  OA.turboTex = arrows('rgba(0,0,0,0)', '#19e6ff'); OA.turboTex.wrapT = THREE.RepeatWrapping;
  OA.turbo = [geo(new THREE.PlaneGeometry(5, 9).rotateX(-Math.PI / 2)), new THREE.MeshBasicMaterial({ map: OA.turboTex, color: new THREE.Color(1.8, 1.8, 1.8), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false })];
  return OA;
}
// nombre d'obstacles selon le réglage « Aucun / Parfois / Souvent / Chaos » et la longueur du circuit
function obstaclePreset(level, L) { const k = [0, 1 / 260, 1 / 100, 1 / 35][level] || 0, z = [0, 1 / 1500, 1 / 700, 1 / 300][level] || 0; return { obstacles: Math.round(L * k), tremplins: Math.round(L * z), turbos: Math.round(L * z), trafic: 0 }; }
function spawnObstacles(n, nRamps, nTurbos) {
  const A = obstacleAssets(), N = T.N, latMax = (T.libre ? T.half : T.limit) - 1.4, i0 = Math.ceil(50 / T.ds), i1 = N - Math.ceil(140 / T.ds);
  if (i1 <= i0) return;
  const place = (type, idx, lat) => {
    idx = ((Math.round(idx) % N) + N) % N; if (idx < i0 || idx > i1) return;
    lat = clamp(lat, -latMax, latMax); T.point(idx, lat, tmp);
    const m = new THREE.Mesh(A[type][0], A[type][1]); m.userData.shared = true; const h = T.heading(idx);
    m.position.set(tmp.x, type === 'turbo' ? 0.035 : 0, tmp.z); m.rotation.y = h; if (type === 'turbo') { m.layers.set(1); m.renderOrder = 3; }
    objs.add(m);
    G.obst.push({ type, def: OB[type], idx, lat, x: tmp.x, z: tmp.z, y: 0, h, mesh: m, state: 0, t: 0, vx: 0, vy: 0, vz: 0, hx: tmp.x, hz: tmp.z });
  };
  const rIdx = () => i0 + Math.random() * (i1 - i0), rLat = (m = 0) => (Math.random() * 2 - 1) * Math.max(0, latMax - m);
  const kinds = ['cone', 'cone', 'barriere', 'caisse', 'caisse', 'pneus', 'bloc'];
  for (let i = 0; i < n; i++) {
    const type = kinds[Math.floor(Math.random() * kinds.length)], idx = rIdx(), lat = rLat();
    if (type === 'cone' && Math.random() < 0.6) { const dir = Math.random() < 0.5 ? -1 : 1, m = Math.min(5, n - i); for (let k = 0; k < m; k++) place('cone', idx + k * 3.5 / T.ds, lat + dir * k * 1.5); i += m - 1; }   // rangée de cônes en biais
    else place(type, idx, lat);
  }
  for (let i = 0; i < nRamps; i++) place('tremplin', rIdx(), rLat(2.2));
  for (let i = 0; i < nTurbos; i++) place('turbo', rIdx(), rLat(2.2));
}
function stepObstacles(dt) {
  const N = T.N;
  if (OA) OA.turboTex.offset.y = (raceTime * 1.6) % 1;
  for (const o of G.obst) {
    if (o.state === 1) {                                           // en vol après un choc
      o.vy -= 22 * RU.gravite * dt; o.x += o.vx * dt; o.y += o.vy * dt; o.z += o.vz * dt;
      o.mesh.rotation.x += o.vx * 0.25 * dt + 4 * dt; o.mesh.rotation.z += o.vz * 0.25 * dt;
      if (o.y <= 0) { o.y = 0; if (Math.abs(o.vy) < 3) { o.state = 2; o.t = 10; o.mesh.rotation.set(Math.PI / 2, o.mesh.rotation.y, 0); } else { o.vy = -o.vy * 0.4; o.vx *= 0.6; o.vz *= 0.6; } }
      o.mesh.position.set(o.x, o.y + (o.state === 2 ? 0.3 : 0), o.z);
    } else if (o.state === 2) { o.t -= dt; if (o.t <= 0) { o.state = 0; o.x = o.hx; o.z = o.hz; o.y = 0; o.mesh.visible = true; o.mesh.position.set(o.x, 0, o.z); o.mesh.rotation.set(0, o.h, 0); } }
  }
  for (const c of cars) {
    if (c.out) continue;
    const D = discs(c), sh = Math.sin(c.h), ch = Math.cos(c.h), sp = Math.abs(c.v);
    for (const o of G.obst) {
      if (o.state) continue;
      let di = o.idx - c.idx; if (di > N / 2) di -= N; else if (di < -N / 2) di += N;
      if (di > 8 || di < -8) continue;
      const dx = c.x - o.x, dz = c.z - o.z;
      if (o.def.zone) {
        const f = -Math.sin(o.h) * dx - Math.cos(o.h) * dz, l = Math.cos(o.h) * dx - Math.sin(o.h) * dz;   // position dans le repère de la zone
        if (o.type === 'tremplin') { if (Math.abs(l) < 2.6 + D.r * 0.5 && Math.abs(f) < 3 && c.y < 0.4 && c.vy <= 0 && c.v > 5 && Math.cos(c.h - o.h) > 0.3) { c.vy = clamp(sp * 0.13, 4, 8.5) * RU.saut; c.y = 0.4; if (c.isPlayer) audio.beep(520, 0.08); } }
        else if (Math.abs(l) < 2.5 + D.r * 0.5 && Math.abs(f) < 4.5 && c.y < 0.4) { if (c.turboT < 0.5 && c.isPlayer) audio.beep(1040, 0.06); c.turboT = Math.max(c.turboT, 1.6); }
        continue;
      }
      if (c.ghostT > 0 || c.y > 1.3) continue;
      let pen = 0, nx = 0, nz = 0;
      for (const fo of D.offs) { const ex = o.x - (c.x - sh * fo), ez = o.z - (c.z - ch * fo), d = Math.hypot(ex, ez), q = D.r + o.def.r - d; if (q > pen && d > 1e-4) { pen = q; nx = ex / d; nz = ez / d; } }
      if (!pen) continue;
      let [vx, vz] = velOf(c); const vn = vx * nx + vz * nz;
      if (o.def.casse) {                                           // caisse en bois : elle éclate
        o.state = 2; o.t = 14; o.mesh.visible = false; fx.burst(o.x, 0.7, o.z, 1.6, 0.9, 0.35, 34, 12); for (let k = 0; k < 4; k++) fx.smoke(o.x, 0.6, o.z, vx * 0.3, 2, vz * 0.3, 1.4, 0.3, 1);
        c.v *= 0.84; if (c.isPlayer) { c.bump = Math.max(c.bump || 0, 0.6); audio.thud(0.5); }
      } else if (o.def.leger || c.giantT > 0) {                    // cône, barrière : projeté par le choc
        o.state = 1; o.vx = vx * 1.1 + nx * 4; o.vz = vz * 1.1 + nz * 4; o.vy = 4 + Math.min(14, sp * 0.14); o.y = 0.2;
        c.v *= o.type === 'cone' ? 0.985 : 0.93; if (c.isPlayer) { c.bump = Math.max(c.bump || 0, 0.25); audio.thud(o.type === 'cone' ? 0.2 : 0.4); }
      } else {                                                     // pneus, béton : obstacle fixe
        c.x -= nx * pen; c.z -= nz * pen;
        if (vn > 0) {
          const e = o.def.e; vx -= (1 + e) * vn * nx; vz -= (1 + e) * vn * nz; setVel(c, vx, vz); if (e < 0.3) c.v *= 0.8;
          c.yawK = clamp(c.yawK + (nx * ch - nz * sh) * Math.min(2.4, vn * 0.12), -3, 3);
          const n = Math.min(16, vn); for (let k = 0; k < n; k++) fx.spark(o.x - nx * o.def.r, 0.5, o.z - nz * o.def.r, vx * 0.3, 2, vz * 0.3);
          if (c.isPlayer) { c.bump = Math.max(c.bump || 0, Math.min(1.2, vn / 12)); audio.thud(vn / 18); }
        }
      }
    }
  }
}


// ---------- balade libre : circulation ----------
function spawnTraffic(n, sensUnique) {
  for (let i = 0; i < n; i++) {
    const def = TRAFIC[i % TRAFIC.length], vis = createCar(def, TRAFIC_PAINTS[(i * 3 + 1) % TRAFIC_PAINTS.length]);
    if (vis.beam) vis.beam.visible = false; vis.wet = world.theme.wet ?? 1; vis.root.rotation.order = 'YXZ';
    scene.add(vis.root);
    const dir = sensUnique ? 1 : i % 2 ? -1 : 1, lat = sensUnique ? (Math.random() * 2 - 1) * Math.max(0, T.limit - 2.5) : dir * 2.3;
    G.traffic.push({ vis, s: 60 + (i + 0.5) * (T.N - 80) / n, dir, lat, speed: 19 + (i * 37 % 9), cur: 20, stun: 0, x: 0, z: 0, h: 0, y: 0, v: 20, u: 0, yawK: 0, mass: 5, rail: true });
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
const keys = { left: false, right: false, gas: false, brake: false, boost: false, act: false, drift: false };
const KEYMAP = { ArrowLeft: 'left', KeyA: 'left', ArrowRight: 'right', KeyD: 'right', ArrowUp: 'gas', KeyW: 'gas', ArrowDown: 'brake', KeyS: 'brake', Space: 'act', ShiftLeft: 'boost', ShiftRight: 'boost', KeyX: 'boost', ControlLeft: 'drift', ControlRight: 'drift', KeyV: 'drift' };
addEventListener('keydown', (e) => { const k = KEYMAP[e.code]; if (k) { keys[k] = true; if (mode === 'race' || mode === 'count') e.preventDefault(); } if ((e.code === 'Escape' || e.code === 'KeyP') && (mode === 'race' || mode === 'pause')) togglePause(); if (e.code === 'KeyC' && (mode === 'race' || mode === 'count')) toggleView(); });
addEventListener('keyup', (e) => { const k = KEYMAP[e.code]; if (k) keys[k] = false; });
for (const [id, k] of [['k-left', 'left'], ['k-right', 'right'], ['k-gas', 'gas'], ['k-brake', 'brake'], ['k-boost', 'boost'], ['k-act', 'act'], ['k-drift', 'drift']]) {
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
  const act = keys.act || pad.act, actMode = G.type === 'foot' || G.type === 'items';
  return { steer, gas, brake, boost: keys.boost || pad.boost, jump: act, use: act, drift: keys.drift || pad.drift || (!actMode && keys.act) };
}

// ---------- manette (Xbox ou compatible) ----------
// En course : gâchette droite = accélérer, gâchette gauche = freiner, X = déraper, B ou RB = boost, A = saut / objet (sinon accélérer),
// stick gauche ou croix = tourner, Start = pause. Dans les menus : croix ou stick = se déplacer, A = valider, B = retour.
const pad = { steer: 0, gas: 0, brake: 0, boost: false, act: false, drift: false, prev: [], navT: 0, used: false };
const SCREEN_PRIORITY = ['pause', 'results', 'setup', 'garage', 'options', 'editor', 'menu'];
function padNav(dx, dy) {
  const scr = SCREEN_PRIORITY.map($).find(e => !e.hidden); if (!scr) return;
  const items = [...scr.querySelectorAll('button, input')].filter(e => e.offsetParent !== null);
  if (!items.length) return;
  const cur = document.activeElement;
  if (!items.includes(cur)) { (scr.querySelector('.btn.primary') || items[0]).focus(); return; }
  if (cur.tagName === 'INPUT' && dx) { cur.value = clamp(+cur.value + dx * (+cur.step || 1), +cur.min, +cur.max); cur.dispatchEvent(new Event('input')); return; }
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
  if (!g) { pad.steer = pad.gas = pad.brake = 0; pad.boost = pad.act = pad.drift = false; return; }
  const val = (i) => { const b = g.buttons[i]; return b ? Math.max(b.value || 0, b.pressed ? 1 : 0) : 0; };
  const hit = (i) => val(i) > 0.5 && !pad.prev[i];
  let sx = g.axes[0] || 0; const sy = g.axes[1] || 0;
  sx = Math.abs(sx) < 0.15 ? 0 : (sx - Math.sign(sx) * 0.15) / 0.85;
  const steer = val(14) ? -1 : val(15) ? 1 : sx;
  const actMode = G.type === 'foot' || G.type === 'items';
  pad.steer = steer; pad.gas = Math.max(val(7), actMode ? 0 : val(0)); pad.brake = val(6); pad.drift = val(2) > 0.5;
  pad.boost = val(1) > 0.5 || val(5) > 0.5; pad.act = actMode && (val(0) > 0.5 || val(4) > 0.5);
  if (!pad.used && (Math.abs(steer) > 0.3 || g.buttons.some(b => b.pressed))) { pad.used = true; app.classList.add('pad'); audio.initAudio(); }
  if (mode === 'race' || mode === 'count') {
    if (hit(9) && mode === 'race') togglePause();
    if (hit(3)) toggleView();
  } else {
    pad.steer = pad.gas = pad.brake = 0; pad.boost = pad.act = pad.drift = false;
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
// vue à la première personne : au-dessus du capot (voiture) ou dans le casque du pilote (moto)
function cockpitCam(c, dt, snap) {
  if (c.vis.rider) c.vis.rider.visible = false;                       // on est le pilote : on ne voit pas sa propre tête
  const k = c.vis.cockpit, ch = Math.cos(c.h), sh = Math.sin(c.h);
  const shake = (c.bump || 0) * 0.08 + clamp(Math.abs(c.v) / 95, 0, 1) * 0.006;
  camera.position.set(c.x + k.x * ch + k.z * sh + (Math.random() - 0.5) * shake, c.y + k.y + (Math.random() - 0.5) * shake, c.z - k.x * sh + k.z * ch);
  camLook.set(camera.position.x - sh * 30, camera.position.y - 1.6 + c.vy * 0.05, camera.position.z - ch * 30);
  camera.lookAt(camLook);
  camera.rotateZ(c.vis.moto ? c.vis.roll : c.vis.roll * 0.5);
  camH = c.h;
  setFov(70 + clamp(Math.abs(c.v) / 95, 0, 1) * 12 + (c.boosting ? 5 : 0), snap ? 0 : dt);
  c.bump = (c.bump || 0) * Math.exp(-6 * dt);
}
function chaseCam(c, dt, snap) {
  if (opts.vue === 'int' && c.vis.cockpit) return cockpitCam(c, dt, snap);
  if (c.vis.rider) c.vis.rider.visible = true;
  const portrait = H > W;
  const ratio = clamp(Math.abs(c.v) / 95, 0, 1), moto = c.vis.moto;
  // la moto se suit de plus près : la caméra reste dans son axe et se penche un peu avec elle
  camH = snap ? c.h : camH + wrap(c.h - camH) * (1 - Math.exp(-(moto ? 10 : 5.5) * dt));
  // le champ de vision s'élargit avec la vitesse ; la caméra se rapproche d'autant pour que le véhicule garde la même taille
  const fov0 = portrait ? 72 : 54, fov = fov0 + ratio * 12 + (c.boosting ? 5 : 0) + clamp((Math.abs(c.v) - 95) / 600, 0, 1) * 24;
  const dist = (portrait ? 8.2 : (moto ? 4.9 : 5.6)) * (0.6 + 0.4 * c.scale) * Math.tan(fov0 * Math.PI / 360) / Math.tan(camera.fov * Math.PI / 360), hgt = (portrait ? 2.5 : (moto ? 2.0 : 1.6)) + c.y * 0.8;
  let x = c.x + Math.sin(camH) * dist, z = c.z + Math.cos(camH) * dist;
  const i = T.nearest(x, z, c.idx, 10), d = T.lateral(x, z, i);
  if (Math.abs(d) > T.limit + 0.5) { const s = Math.sign(d), p = Math.abs(d) - T.limit - 0.5; x -= -T.tz[i] * s * p; z -= T.tx[i] * s * p; }
  const shake = (c.bump || 0) * 0.25 + ratio * ratio * 0.012 + (c.boosting ? 0.02 : 0);
  camPos.set(x + (Math.random() - 0.5) * shake, hgt + (Math.random() - 0.5) * shake, z);
  camera.position.copy(camPos); // position rigide : pas de retard qui éloignerait le véhicule à haute vitesse
  camLook.set(c.x - Math.sin(camH) * 7, (moto ? 0.45 : 0.85) + c.y * 0.8, c.z - Math.cos(camH) * 7);
  camera.lookAt(camLook);
  camRoll += ((moto ? c.vis.roll * 0.3 : clamp(-c.yaw * 0.07, -0.05, 0.05)) - camRoll) * (1 - Math.exp(-5 * dt)); camera.rotateZ(camRoll);
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
const screens = ['loading', 'menu', 'setup', 'garage', 'options', 'editor', 'hud', 'pause', 'results'];
function show(...ids) { for (const s of screens) $(s).hidden = !ids.includes(s); }
const fmt = (t) => { const m = Math.floor(t / 60), s = t - m * 60; return m + ':' + (s < 10 ? '0' : '') + s.toFixed(2); };
const fmtShort = (t) => { t = Math.max(0, Math.ceil(t)); return Math.floor(t / 60) + ':' + String(t % 60).padStart(2, '0'); };
const ordinal = (n) => n === 1 ? '1er' : n + 'e';
let flashT = 0;
function flash(msg) { $('h-msg').textContent = msg; flashT = 3; }
let toastT = 0;
function toast(msg) { $('toast').textContent = msg; $('toast').hidden = false; toastT = 4; }
function refreshCash() { for (const el of document.querySelectorAll('.cash')) el.textContent = Math.round(eco.cash).toLocaleString('fr-FR'); }
// musique : démarre au premier appui (les navigateurs l'exigent)
const wake = () => { audio.initAudio(); music.setVolume(opts.volMus); if (opts.musique === 'off') music.stop(); else if (!music.current()) music.play(opts.musique); };
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
  $('s-title').textContent = { foot: 'Foot Turbo', items: 'Course à bonus', chrono: 'Contre-la-montre', elim: 'Élimination', free: 'Balade libre' }[type] || 'Choix de la course';
  for (const el of document.querySelectorAll('#setup [data-for]')) el.hidden = !el.dataset.for.split(' ').includes(type);
  const el = $('tracks'); el.textContent = '';
  TRACKS.forEach((t, i) => {
    const b = document.createElement('button'); b.className = 'track'; b.type = 'button'; b.setAttribute('aria-pressed', String(setup.track === i));
    const cv = document.createElement('canvas'); cv.width = 232; cv.height = 124; b.appendChild(cv);
    const g = cv.getContext('2d'); const col = '#' + new THREE.Color(t.theme.env === 'nature' ? t.theme.c : t.theme.a).getHexString();
    g.shadowColor = col; g.shadowBlur = 12; drawPath(g, trackCache[i], 232, 124, 14, col, 5);
    const nm = document.createElement('b'); nm.textContent = t.nom; b.appendChild(nm);
    const ds = document.createElement('small'); ds.textContent = (t.perso !== undefined ? 'Atelier' : t.desc) + ' · ' + (trackCache[i].L / 1000).toFixed(1) + ' km'; b.appendChild(ds);
    const rc = document.createElement('em'); rc.textContent = best[t.id] ? 'Record ' + fmt(best[t.id]) : 'Pas encore de record'; b.appendChild(rc);
    b.addEventListener('click', () => { setup.track = i; for (const o of el.children) o.setAttribute('aria-pressed', 'false'); b.setAttribute('aria-pressed', 'true'); });
    el.appendChild(b);
  });
  chips($('diffs'), DIFFS.map((d, i) => [i, d.nom]), () => setup.diff, (v) => { setup.diff = v; });
  chips($('laps'), [[1, '1'], [3, '3'], [5, '5'], [10, '10'], [25, '25'], [99, '99']], () => setup.laps, (v) => { setup.laps = v; });
  chips($('ambs'), [['def', "D'origine"], ['jour', 'Jour'], ['nuit', 'Nuit'], ['orage', 'Orage violent']], () => setup.amb, (v) => { setup.amb = v; });
  chips($('obst'), [[0, 'Aucun'], [1, 'Parfois'], [2, 'Souvent'], [3, 'Chaos']], () => setup.obst, (v) => { setup.obst = v; });
  chips($('bots'), [[1, '1'], [3, '3'], [5, '5'], [7, '7'], [11, '11'], [15, '15'], [23, '23'], [31, '31']], () => setup.bots, (v) => { setup.bots = v; });
  chips($('routes'), [['libre', 'Route des Prés · circulation'], ['ligne', 'Ligne Droite · sans circulation']], () => setup.route, (v) => { setup.route = v; });
  chips($('teams'), [[1, '1 contre 1'], [2, '2 contre 2'], [3, '3 contre 3']], () => setup.team, (v) => { setup.team = v; });
  chips($('durs'), [[120, '2 min'], [180, '3 min'], [300, '5 min']], () => setup.dur, (v) => { setup.dur = v; });
}
function buildOptions() {
  chips($('o-qual'), [['auto', 'Auto'], ['haute', 'Élevés'], ['moyenne', 'Moyens'], ['basse', 'Bas']], () => opts.qual, (v) => { opts.qual = v; store.set('opts', opts); changeQuality(); });
  chips($('o-gas'), [[false, 'Manuel'], [true, 'Automatique']], () => opts.autoGas, (v) => { opts.autoGas = v; store.set('opts', opts); });
  chips($('o-snd'), [[true, 'Activé'], [false, 'Coupé']], () => opts.son, (v) => { opts.son = v; store.set('opts', opts); audio.setSound(v); });
  chips($('o-mus'), [['off', 'Coupée'], ['alea', 'Au hasard'], ...music.MORCEAUX.map(m => [m.id, m.nom])], () => opts.musique, (v) => { opts.musique = v; store.set('opts', opts); audio.initAudio(); music.setVolume(opts.volMus); if (v === 'off') music.stop(); else music.play(v); });
  chips($('o-vol'), [[0.25, '25 %'], [0.5, '50 %'], [0.75, '75 %'], [1, '100 %']], () => opts.volMus, (v) => { opts.volMus = v; store.set('opts', opts); music.setVolume(v); });
  $('o-info').textContent = 'Manette : gâchette droite pour accélérer, gâchette gauche pour freiner, X pour déraper, B pour le boost, A pour sauter ou lancer un objet, Y pour changer de vue, Start pour la pause. Clavier : flèches ou ZQSD, Maj pour le boost, Ctrl ou V pour déraper (Espace aussi hors Foot et Bonus), Espace pour sauter ou lancer un objet, C pour la vue.';
}
function changeQuality() {
  pickQuality(); setupPost();
  show('loading'); $('loadmsg').textContent = 'Mise à jour des graphismes…';
  setTimeout(() => { worldKey = ''; startDemo(); show('options'); }, 30);
}
function hudFor(type) {
  const race = type === 'race' || type === 'items' || type === 'elim';
  $('h-posbox').hidden = !race; $('h-lapbox').hidden = type === 'foot' || type === 'elim';
  $('h-laplabel').textContent = type === 'free' ? 'Distance' : 'Tour'; $('h-lapsfx').hidden = type === 'free';
  $('score').hidden = type !== 'foot'; $('mini').hidden = type === 'foot';
  $('k-act').hidden = !(type === 'foot' || type === 'items'); $('k-actl').textContent = type === 'foot' ? 'SAUT' : 'OBJET';
  $('k-boost').hidden = type === 'items'; $('boostbar').hidden = type === 'items';
  $('k-gas').hidden = opts.autoGas; $('h-item').textContent = ''; $('h-drift').textContent = ''; $('h-msg').textContent = ''; $('wrong').hidden = true; $('banner').hidden = true;
  $('h-tot').textContent = cars.length; $('h-laps').textContent = G.laps; $('h-lap').textContent = type === 'free' ? '0.0' : 1;
  $('sc-b').textContent = 0; $('sc-o').textContent = 0;
}

// garage
let gCar = null;
// niveau maximal débloqué pour chaque réglage (au départ : la valeur d'origine du véhicule)
function statCaps(def) { const sv = eco.caps[def.id] || {}, cur = carStats(def), o = {}; for (const [k] of STAT_KEYS) o[k] = Math.max(def.stats[k], sv[k] || 0, cur[k]); return o; }
const gnote = (msg) => { $('gnote').textContent = msg || 'Les réglages sont gardés sur cet appareil.'; };
function pay(cost) { if (eco.cash < cost) { gnote('Pas assez de crédits : il te manque ' + cr(cost - eco.cash) + '.'); audio.beep(180, 0.15); return false; } eco.cash -= cost; saveEco(); refreshCash(); audio.chime(); return true; }
function garageShow(i) {
  selCar = (i + CARS.length) % CARS.length;
  const def = CARS[selCar], mine = owned(def);
  clearGame();
  gCar = makeCarEntity(def, carColor(def), true, 'Toi', DIFFS[1]);
  T.point(T.N - 5, 0, tmp); gCar.x = tmp.x; gCar.z = tmp.z; gCar.h = T.heading(T.N - 5); gCar.idx = T.N - 5;
  $('g-name').textContent = def.nom; gnote(); refreshCash();
  $('g-price').textContent = mine ? 'Dans ton garage' : 'À vendre : ' + cr(def.prix);
  $('g-pick').textContent = mine ? 'Choisir' : 'Acheter';
  const sw = $('swatches'); sw.textContent = '';
  $('gpaint').hidden = gCar.vis.paintMats.length === 0;
  PAINTS.forEach((p) => {
    const b = document.createElement('button'); b.className = 'sw'; b.type = 'button'; b.style.background = '#' + new THREE.Color(p).getHexString();
    b.setAttribute('aria-label', 'Peinture'); b.setAttribute('aria-pressed', String(carColor(def) === p));
    b.addEventListener('click', () => { (tuning[def.id] = tuning[def.id] || {}).couleur = p; store.set('tuning', tuning); setPaint(gCar.vis, p); for (const o of sw.children) o.setAttribute('aria-pressed', 'false'); b.setAttribute('aria-pressed', 'true'); });
    sw.appendChild(b);
  });
  const st = $('stats'); st.textContent = '';
  const cur = carStats(def), caps = statCaps(def);
  STAT_KEYS.forEach(([k, label]) => {
    const row = document.createElement('div'); row.className = 'stat';
    const sp = document.createElement('span'); sp.textContent = label + ' ';
    const out = document.createElement('output'); out.textContent = cur[k] + ' / ' + caps[k];
    const inp = document.createElement('input'); inp.type = 'range'; inp.min = 1; inp.max = caps[k]; inp.step = 1; inp.value = cur[k]; inp.id = 'stat-' + k; inp.disabled = !mine; inp.setAttribute('aria-label', label);
    inp.addEventListener('input', () => {
      const t = tuning[def.id] = tuning[def.id] || {}; t.stats = Object.assign({}, carStats(def), { [k]: +inp.value });
      store.set('tuning', tuning); out.textContent = inp.value + ' / ' + caps[k]; power();
    });
    row.append(sp, out);
    if (mine && caps[k] < 10) {
      const cost = 300 * (caps[k] + 1), up = document.createElement('button'); up.type = 'button'; up.className = 'chip up'; up.textContent = '+1 · ' + cr(cost);
      up.addEventListener('click', () => { if (!pay(cost)) return; (eco.caps[def.id] = eco.caps[def.id] || {})[k] = caps[k] + 1; saveEco();
        const t = tuning[def.id] = tuning[def.id] || {}; t.stats = Object.assign({}, carStats(def), { [k]: caps[k] + 1 }); store.set('tuning', tuning); garageShow(selCar); });
      row.append(up);
    }
    row.append(inp); st.appendChild(row);
  });
  // moteur extrême : vitesse de pointe libre, par paliers à débloquer (valable pour tous les véhicules)
  const ex = $('gext'); ex.textContent = ''; ex.hidden = !mine;
  if (mine) {
    const t = tuning[def.id] = tuning[def.id] || {}, setV = (v) => { t.vmaxKmh = clamp(Math.round(v) || 0, 0, eco.extreme); store.set('tuning', tuning); num.value = t.vmaxKmh; power(); };
    const head = document.createElement('div'); head.className = 'label'; head.textContent = 'Moteur extrême'; ex.appendChild(head);
    const num = document.createElement('input'); num.type = 'number'; num.min = 0; num.max = eco.extreme; num.step = 10; num.value = Math.min(t.vmaxKmh || 0, eco.extreme); num.id = 'g-vmax'; num.setAttribute('aria-label', 'Vitesse de pointe en km/h');
    if (eco.extreme) {
      const ch = document.createElement('div'); ch.className = 'chips';
      for (const [v, lab] of [[0, 'Série'], ...EXTREMES.filter(e => e[0] <= eco.extreme).map(e => [e[0], e[0] + ''])]) { const b = document.createElement('button'); b.type = 'button'; b.className = 'chip'; b.textContent = lab; b.addEventListener('click', () => setV(v)); ch.appendChild(b); }
      const row = document.createElement('div'); row.className = 'row'; const sp = document.createElement('span'); sp.textContent = 'km/h (0 = série)'; num.addEventListener('change', () => setV(+num.value)); row.append(num, sp);
      const ar = document.createElement('div'); ar.className = 'stat'; const al = document.createElement('span'); al.textContent = 'Accélération '; const ao = document.createElement('output'); ao.textContent = '× ' + (t.accelX || 1);
      const ai = document.createElement('input'); ai.type = 'range'; ai.min = 1; ai.max = 50; ai.step = 1; ai.value = t.accelX || 1; ai.setAttribute('aria-label', "Multiplicateur d'accélération");
      ai.addEventListener('input', () => { t.accelX = +ai.value; store.set('tuning', tuning); ao.textContent = '× ' + ai.value; }); ar.append(al, ao, ai);
      ex.append(ch, row, ar);
    }
    const nx = EXTREMES.find(e => e[0] > eco.extreme);
    if (nx) { const b = document.createElement('button'); b.type = 'button'; b.className = 'chip up'; b.textContent = 'Débloquer ' + nx[0].toLocaleString('fr-FR') + ' km/h · ' + cr(nx[1]);
      b.addEventListener('click', () => { if (!pay(nx[1])) return; eco.extreme = nx[0]; saveEco(); garageShow(selCar); gnote('Palier débloqué : règle la vitesse de pointe jusqu\'à ' + nx[0].toLocaleString('fr-FR') + ' km/h.'); }); ex.appendChild(b); }
  }
  power();
}
function power() {
  const def = CARS[selCar], s = carStats(def), tn = carTune(def);
  $('g-kmh').textContent = tn.vmaxKmh > 0 ? tn.vmaxKmh.toLocaleString('fr-FR') : Math.round(180 + s.vmax * 18);
  $('g-pow').textContent = Math.round((s.vmax * 1.2 + s.accel * 1.1 + s.mania * 0.8 + s.adher * 0.9) * 25 * (tn.vmaxKmh > 0 ? tn.vmaxKmh / (180 + s.vmax * 18) : 1));
}

// ---------- atelier de circuits ----------
let edSlot = 0, edDirty = false; const edShape = new Set();
function edPreview() {
  const c = customs[edSlot], Tk = makeTrack(circuitDe(c, edSlot, THEMES)), cv = $('e-prev'), g = cv.getContext('2d');
  g.clearRect(0, 0, cv.width, cv.height); const col = '#' + new THREE.Color(THEMES[c.theme].env === 'nature' ? THEMES[c.theme].c : THEMES[c.theme].a).getHexString();
  const mp = drawPath(g, Tk, cv.width, cv.height, 22, 'rgba(255,255,255,0.18)', 2);
  g.shadowColor = col; g.shadowBlur = 10; drawPath(g, Tk, cv.width, cv.height, 22, col, clamp(c.largeur * mp.sc, 2, 60)); g.shadowBlur = 0;
  $('e-info').textContent = (Tk.L / 1000).toFixed(2).replace('.', ',') + ' km · route de ' + c.largeur + ' m · ' + c.obstacles + ' obstacles';
}
function edSave(key) { borner(customs[edSlot]); store.set('customs', customs); edDirty = true; if (!key || ['graine', 'virages', 'taille', 'chaos', 'largeur'].includes(key)) edShape.add(edSlot); edPreview(); }
function buildEditor() {
  const c = customs[edSlot];
  chips($('e-slots'), customs.map((x, i) => [i, x.nom]), () => edSlot, (v) => { edSlot = v; buildEditor(); });
  chips($('e-themes'), THEME_NOMS.map((n, i) => [i, n]), () => c.theme, (v) => { c.theme = v; edSave('theme'); });
  const box = $('e-fields'); box.textContent = '';
  for (const f of CHAMPS) {
    if (f.length === 1) { const h = document.createElement('div'); h.className = 'label sec'; h.textContent = f[0]; box.appendChild(h); continue; }
    const [k, label, min, max, step, , unit] = f, row = document.createElement('div'); row.className = 'field';
    const sp = document.createElement('span'); sp.textContent = label + (unit ? ' (' + unit + ')' : '');
    const num = document.createElement('input'); num.type = 'number'; const rng = document.createElement('input'); rng.type = 'range';
    for (const e of [num, rng]) { e.min = min; e.max = max; e.step = step; e.value = c[k]; e.setAttribute('aria-label', label); }
    num.id = 'e-' + k;
    const set = (v, from) => { c[k] = clamp(+v || 0, min, max); if (from !== num) num.value = c[k]; if (from !== rng) rng.value = c[k]; edSave(k); };
    rng.addEventListener('input', () => set(rng.value, rng)); num.addEventListener('change', () => set(num.value, null));
    row.append(sp, num, rng); box.appendChild(row);
  }
  edPreview();
}
// en quittant l'atelier : les circuits modifiés sont reconstruits et leurs records remis à zéro
function edApply() {
  if (!edDirty) return; edDirty = false; syncCustoms();
  customs.forEach((c, i) => { trackCache[NB_BASE + i] = makeTrack(TRACKS[NB_BASE + i]); if (edShape.has(i)) delete best['perso' + (i + 1)]; });
  edShape.clear(); store.set('best3', best); worldKey = '';
}
function refreshMenu() {
  $('m-car').textContent = CARS[selCar].nom;
  const id = TRACKS[setup.track].id;
  $('m-best').textContent = TRACKS[setup.track].nom + (best[id] ? ' · record ' + fmt(best[id]) : '');
}

// ---------- déroulement ----------
function startDemo() {
  RU = TRACKS[setup.track].regles || REGLES_BASE;
  ensureWorld(String(setup.track));
  G.type = 'demo'; G.boost = true; G.drain = 30; G.regen = 7;
  gridUp(false, 7);
  for (const c of cars) c.lap = 1;
  mode = 'menu'; chaseSnap = true;
}
function toMenu() {
  edApply();
  if (!owned(CARS[selCar])) { selCar = CARS.findIndex(c => c.id === store.get('car', '') && owned(c)); if (selCar < 0) selCar = CARS.findIndex(owned); }
  startDemo(); refreshMenu(); refreshCash(); show('menu'); audio.engine(null); audio.skid(0); audio.scrape(0); music.setDuck(1);
}
// gains de la partie : selon le classement, la difficulté, les buts, la distance et les dérapages
function payout() {
  if (G.paid || !player) return null; G.paid = true;
  const lines = [], mul = DIFF_GAIN[setup.diff] || 1, n = cars.length;
  if (G.dr.cur >= 50) G.dr.total += G.dr.cur; G.dr.cur = 0;
  if (G.type === 'race' || G.type === 'items') { const r = player.rank || n; lines.push([ordinal(r) + ' sur ' + n, 700 * (n - r + 1) / n * mul * clamp(G.laps / 3, 0.4, 4) * (n > 1 ? 1 : 0.3)]); if (r === 1 && n > 1) lines.push(['Victoire', 300 * mul]); }
  else if (G.type === 'chrono') lines.push(['Tours bouclés', 120 * Math.min(G.laps, 30)]);
  else if (G.type === 'elim') lines.push([player.rank === 1 ? 'Dernier survivant' : ordinal(player.rank) + ' sur ' + n, (player.rank === 1 ? 1200 : 500 * (n - player.rank) / n) * mul]);
  else if (G.type === 'foot') { const [b, o] = G.score; lines.push(['Buts de ton équipe', 120 * b]); if (b > o) lines.push(['Victoire', 500 * mul]); }
  else if (G.type === 'free') lines.push(['Distance ' + (G.dist / 1000).toFixed(1) + ' km', G.dist / 1000 * 60]);
  if (G.dr.total > 0) lines.push(['Dérapages', G.dr.total / 6]);
  let tot = 0; for (const l of lines) { l[1] = Math.round(l[1]); tot += l[1]; }
  eco.cash += tot; saveEco(); refreshCash();
  return { lines, tot };
}
function showGains() {
  const g = payout(), el = $('rgain'); el.textContent = '';
  if (!g || !g.tot) return;
  el.textContent = g.lines.filter(l => l[1] > 0).map(l => l[0] + ' +' + l[1]).join(' · ') + ' — total +' + cr(g.tot); audio.chime();
}
// quitter ou recommencer une balade libre : la distance et les dérapages sont payés
function settleFree() { if (G.type === 'free' && (mode === 'race' || mode === 'pause')) { const g = payout(); if (g && g.tot) toast('Balade : +' + cr(g.tot)); } }
function startGame(type) {
  store.set('setup', setup); store.set('car', CARS[selCar].id);
  show('loading'); $('loadmsg').textContent = type === 'foot' ? 'Foot Turbo' : type === 'free' ? (setup.route === 'ligne' ? ROUTE_LIGNE : ROUTE_LIBRE).nom : TRACKS[setup.track].nom;
  setTimeout(() => {
    RU = (type !== 'foot' && type !== 'free' && TRACKS[setup.track].regles) || REGLES_BASE;
    G.dr = { cur: 0, total: 0, hold: 0 }; G.paid = false; G.sonic = false; music.setDuck(1); if (opts.musique === 'alea') music.shuffle();
    G.type = type; G.ended = false; G.freeze = 0; G.score = [0, 0]; G.dist = 0; G.laps = type === 'elim' ? 999 : setup.laps; G.elimT = ELIM_DELAI;
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
      const route = setup.route === 'ligne' ? 'ligne' : 'libre';
      ensureWorld(route); clearGame();
      G.boost = true; G.drain = 30; G.regen = 7;
      player = makeCarEntity(CARS[selCar], carColor(CARS[selCar]), true, 'Toi', DIFFS[1]);
      T.point(5, 2.3, tmp); Object.assign(player, { x: tmp.x, z: tmp.z, h: T.heading(5), idx: 5 });
      if (route === 'libre') spawnTraffic(22);
      { const pc = obstaclePreset(setup.obst, T.L); spawnObstacles(pc.obstacles, pc.tremplins, pc.turbos); }
      raceTime = 0;
    } else {
      ensureWorld(String(setup.track));
      G.boost = type !== 'items'; G.drain = 30; G.regen = 7;
      gridUp(true, type === 'chrono' ? 0 : setup.bots);
      if (type === 'items') spawnBoxes();
      { const pc = TRACKS[setup.track].contenu || obstaclePreset(setup.obst, T.L); spawnObstacles(pc.obstacles, pc.tremplins, pc.turbos); if (pc.trafic) spawnTraffic(pc.trafic, true); }
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
  if (G.type === 'chrono') $('rtitle').textContent = 'Temps : ' + fmt(player.finishTime);
  $('rsub').textContent = 'Meilleur tour ' + (player.bestLap ? fmt(player.bestLap) : '—') + ((G.type === 'race' || G.type === 'chrono') && best[id] ? ' · record du circuit ' + fmt(best[id]) : '');
  fillResults(); show('results'); showGains(); audio.skid(0); audio.scrape(0);
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
  show('results'); showGains(); audio.engine(null); audio.skid(0); audio.scrape(0);
}
function fillResults() {
  const tb = $('rtable'); tb.textContent = '';
  standings().forEach((c, i) => {
    const tr = document.createElement('tr'); if (c.isPlayer) tr.className = 'me';
    for (const v of [i + 1, c.name + ' · ' + c.def.nom, c.out ? 'éliminé' : c.finished ? fmt(c.finishTime) : G.type === 'elim' && mode === 'finished' && c.rank === 1 ? 'vainqueur' : 'en course']) { const td = document.createElement('td'); td.textContent = v; tr.appendChild(td); }
    tb.appendChild(tr);
  });
}
function togglePause() {
  if (mode === 'race') { mode = 'pause'; show('hud', 'pause'); audio.engine(null); audio.skid(0); audio.scrape(0); music.setDuck(0.4); }
  else if (mode === 'pause') { mode = 'race'; show('hud'); music.setDuck(1); }
}
document.addEventListener('visibilitychange', () => { if (document.hidden && mode === 'race') togglePause(); audio.suspend(document.hidden); });

$('m-race').onclick = () => { wake(); buildSetup('race'); show('setup'); };
$('m-items').onclick = () => { wake(); buildSetup('items'); show('setup'); };
$('m-foot').onclick = () => { wake(); buildSetup('foot'); show('setup'); };
$('m-free').onclick = () => { wake(); buildSetup('free'); show('setup'); };
$('m-chrono').onclick = () => { wake(); buildSetup('chrono'); show('setup'); };
$('m-elim').onclick = () => { wake(); buildSetup('elim'); show('setup'); };
$('m-garage').onclick = () => { wake(); mode = 'garage'; G.type = 'demo'; garageShow(selCar); show('garage'); };
$('m-options').onclick = () => { wake(); buildOptions(); show('options'); };
$('m-atelier').onclick = () => { wake(); buildEditor(); show('editor'); };
$('e-seed').onclick = () => { customs[edSlot].graine = Math.floor(Math.random() * 100000); edSave('graine'); buildEditor(); };
$('e-wild').onclick = () => { const c = customs[edSlot]; for (const f of CHAMPS) if (f.length > 1) { const hi = { taille: 1200, obstacles: 200, trafic: 30, largeur: 90 }[f[0]] ?? f[3]; c[f[0]] = Math.round((f[2] + Math.random() * (hi - f[2])) / f[4]) * f[4]; c[f[0]] = +c[f[0]].toFixed(2); } c.theme = Math.floor(Math.random() * THEMES.length); edSave(); buildEditor(); };
$('e-reset').onclick = () => { customs[edSlot] = defautCustom(edSlot); edSave(); buildEditor(); };
$('e-try').onclick = () => { edApply(); setup.track = NB_BASE + edSlot; buildSetup('race'); show('setup'); };
$('p-music').onclick = () => { if (opts.musique === 'off') { opts.musique = 'alea'; store.set('opts', opts); } audio.initAudio(); music.setVolume(opts.volMus); const m = music.next(); toast('Musique : ' + m.nom); };
for (const b of document.querySelectorAll('[data-back]')) b.onclick = () => { store.set('setup', setup); toMenu(); };
$('s-go').onclick = () => startGame(setupType);
$('g-prev').onclick = () => garageShow(selCar - 1);
$('g-next').onclick = () => garageShow(selCar + 1);
$('g-reset').onclick = () => { delete tuning[CARS[selCar].id]; store.set('tuning', tuning); garageShow(selCar); };
$('g-pick').onclick = () => {
  const def = CARS[selCar];
  if (!owned(def)) { if (!pay(def.prix)) return; eco.owned[def.id] = true; saveEco(); garageShow(selCar); gnote(def.nom + ' rejoint ton garage !'); return; }
  store.set('car', def.id); toMenu();
};
$('bpause').onclick = togglePause;
function toggleView() { opts.vue = opts.vue === 'int' ? 'ext' : 'int'; store.set('opts', opts); chaseSnap = true; }
$('bvue').onclick = toggleView;
$('p-resume').onclick = togglePause;
$('p-restart').onclick = () => { settleFree(); startGame(G.type); };
$('p-quit').onclick = () => { settleFree(); toMenu(); };
$('r-menu').onclick = toMenu;
$('r-again').onclick = () => startGame(G.type);

// touche Retour et mise en veille (application Android)
let backAt = 0;
window.__nnBack = () => {
  const now = performance.now(); if (now - backAt < 350) return true; backAt = now;
  if (mode === 'race' || mode === 'pause') { togglePause(); return true; }
  if (mode === 'count') return true;
  if (mode === 'finished') { toMenu(); return true; }
  if (!$('garage').hidden && !owned(CARS[selCar])) { toMenu(); return true; }
  if (!$('menu').hidden) return false; // au menu principal : l'application se ferme
  store.set('setup', setup); toMenu(); return true;
};
window.__nnPause = () => { if (mode === 'race') togglePause(); audio.engine(null); audio.skid(0); audio.scrape(0); audio.suspend(true); };
window.__nnResume = () => audio.suspend(false);

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

// ---------- orage : éclairs, foudre, tonnerre, pluie battante ----------
const storm = { t: 2, flash: 0, bolt: null };
function strikeBolt() {
  if (!storm.bolt) { const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(90 * 6), 3));
    storm.bolt = new THREE.LineSegments(g, new THREE.LineBasicMaterial({ color: new THREE.Color(5, 5.5, 7), fog: false, depthWrite: false })); storm.bolt.frustumCulled = false; storm.bolt.layers.set(1); scene.add(storm.bolt); }
  // la foudre tombe plutôt devant le joueur, entre 200 m et 1,1 km
  const fwd = camera.getWorldDirection(sunW), a = Math.atan2(fwd.x, fwd.z) + (Math.random() - 0.5) * 2.2, dist = 200 + Math.random() * 900;
  const p = storm.bolt.geometry.attributes.position.array; let n = 0, x = camera.position.x + Math.sin(a) * dist, z = camera.position.z + Math.cos(a) * dist, y = 420;
  const seg = (x0, y0, z0, x1, y1, z1) => { if (n >= 90) return; p.set([x0, y0, z0, x1, y1, z1], n * 6); n++; };
  while (y > 0 && n < 60) { const nx = x + (Math.random() - 0.5) * 34, nz = z + (Math.random() - 0.5) * 34, ny = y - 12 - Math.random() * 26; seg(x, y, z, nx, Math.max(0, ny), nz);
    if (Math.random() < 0.22) { let bx = nx, by = ny, bz = nz, dx = (Math.random() - 0.5) * 50; for (let k = 0; k < 4 && by > 20; k++) { const ex = bx + dx + (Math.random() - 0.5) * 20, ey = by - 14 - Math.random() * 18, ez = bz + (Math.random() - 0.5) * 30; seg(bx, by, bz, ex, ey, ez); bx = ex; by = ey; bz = ez; } }
    x = nx; y = ny; z = nz; }
  for (let i = n * 6; i < p.length; i++) p[i] = 0;
  storm.bolt.geometry.attributes.position.needsUpdate = true; storm.flash = 0.4 + Math.random() * 0.35; audio.thunder(dist);
}
function stepStorm(dt) {
  const on = world && world.theme && world.theme.orage && mode !== 'loading';
  audio.rain(on && mode !== 'pause' ? 1 : 0);
  if (!on) { if (renderer.toneMappingExposure !== 0.95) renderer.toneMappingExposure = 0.95; if (storm.bolt) storm.bolt.visible = false; storm.flash = 0; return; }
  if (mode === 'pause') return;
  storm.t -= dt; if (storm.t <= 0) { storm.t = 2 + Math.random() * 7; strikeBolt(); }
  if (storm.flash > 0) { storm.flash -= dt; const k = storm.flash > 0 ? (0.45 + 0.55 * Math.abs(Math.sin(storm.flash * 55))) * Math.min(1, storm.flash * 5) : 0;
    renderer.toneMappingExposure = 0.95 + k * 2.6; storm.bolt.visible = storm.flash > 0.1 && Math.sin(storm.flash * 90) > -0.4; }
}

// ---------- boucle ----------
let time = 0;
const sunV = new THREE.Vector3(), sunW = new THREE.Vector3();
function simulate(dt) {
  raceTime += dt;
  for (const c of cars) {
    if (c.out) continue;
    const inp = (c.isPlayer && mode === 'race') ? playerInput() : aiInput(c, dt);
    if (G.type === 'items') { if (inp.use && !c.prevUse) useItem(c); c.prevUse = !!inp.use; }
    stepCar(c, inp, dt);
    c.inp = inp;
  }
  collide(G.type === 'elim' ? cars.filter(c => !c.out) : cars);
  if (G.type === 'items') stepItems(dt);
  if (G.type === 'elim' && mode === 'race') stepElim(dt);
  if (G.traffic.length) stepTraffic(dt);
  if (G.obst.length) stepObstacles(dt);
  if (player && mode === 'race') driftScore(player, dt);
}
// dérapages du joueur : les points montent tant que la voiture glisse, sont encaissés à la fin de la glissade et perdus contre un mur
function driftScore(p, dt) {
  const D = G.dr;
  if ((p.drifting || Math.abs(p.u) > 3.5) && Math.abs(p.v) > 9 && p.y < 0.05 && p.spinT <= 0) { D.cur += (p.drifting ? Math.abs(p.driftA) * Math.abs(p.v) * 3.2 : Math.abs(p.u) * 5) * dt; D.hold = 1.1; }
  else if (D.cur > 0) { D.hold -= dt; if (D.hold <= 0) { if (D.cur >= 50) { D.total += D.cur; flash('Dérapage +' + Math.round(D.cur)); } D.cur = 0; } }
  if (p.onWall && D.cur > 0) { if (D.cur >= 50) flash('Dérapage perdu'); D.cur = 0; }
  // mur du son
  const sp = Math.abs(p.v);
  if (sp > 343 && !G.sonic) { G.sonic = true; flash('Mur du son franchi !'); audio.boom(1.2); fx.burst(p.x, 1, p.z, 3, 3, 3.4, 90, 40); p.bump = 1; } else if (sp < 300) G.sonic = false;
}
// moteur du joueur et de l'adversaire le plus proche
const sonDe = (def) => def.son || (def.moto ? 'moto' : 'v12');
function engineSound(thr) {
  const p = player; let near = null, bd = 50 * 50;
  for (const c of cars) { if (c === p || c.out) continue; const d = (c.x - p.x) ** 2 + (c.z - p.z) ** 2; if (d < bd) { bd = d; near = c; } }
  let r = null;
  if (near) { const d = Math.sqrt(bd) || 1, [avx, avz] = velOf(p), [bvx, bvz] = velOf(near); r = { type: sonDe(near.def), ratio: clamp(Math.abs(near.v) / near.phys.vmax, 0, 1.1), dist: d, rel: -((bvx - avx) * (near.x - p.x) + (bvz - avz) * (near.z - p.z)) / d }; }
  audio.engine({ v: p.v, vmax: p.phys.vmax, thr, boosting: p.boosting, nitro: p.nitroT > 0, air: p.y > 0.05, type: sonDe(p.def) }, r);
}
// à très haute vitesse, la simulation est découpée en petits pas pour ne traverser ni les murs ni les obstacles
function simSteps(dt, fn) {
  let vm = 0; for (const c of cars) vm = Math.max(vm, Math.abs(c.v));
  const n = clamp(Math.ceil(vm * dt / 2.4), 1, 48);
  for (let i = 0; i < n; i++) fn(dt / n);
}
// fumée, flammes et étincelles produites par un véhicule
function emit(c, dt) {
  if (camera.position.distanceToSquared(c.vis.root.position) > 160 * 160) return;
  const sh = Math.sin(c.h + c.driftA), ch = Math.cos(c.h + c.driftA), len = c.vis.len, hw = c.vis.half;
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
    // dérapage : épais panache blanc aux roues arrière et traces de gomme au sol
    if (c.drifting) for (const s of [-1, 1]) { const [wx, wz] = side(s * hw * 0.82, len * 0.6); for (let k = 0; k < 2; k++) fx.smoke(wx, 0.25, wz, vx * 0.2, 0.9, vz * 0.2, 1.9, 0.4, 1.5); }
    else if ((c.slip > 0.6 || Math.abs(c.u) > 1.6 || c.spinT > 0) && sp > 8) for (const s of [-1, 1]) { const [wx, wz] = side(s * hw * 0.82, len * 0.6); fx.smoke(wx, 0.2, wz, vx * 0.25, 0.6, vz * 0.25, 1.2, 0.3, 1.0); }
    else if (c.braking && sp > 24 && Math.random() < 0.35) { const [wx, wz] = side((Math.random() < 0.5 ? -1 : 1) * hw * 0.82, len * 0.6); fx.smoke(wx, 0.2, wz, vx * 0.3, 0.4, vz * 0.3, 0.9, 0.2, 0.7); }
    if (c.offroad && sp > 6 && Math.random() < 0.5) fx.smoke(rx, 0.25, rz, vx * 0.3, 1.2, vz * 0.3, 1.3, 0.22, 1.0);
    if (c.inp && c.inp.gas > 0.5 && sp < 14 && Math.random() < 0.12) fx.smoke(rx, 0.45, rz, sh * 2, 0.6, ch * 2, 0.6, 0.14, 0.8);
  }
  // gerbes d'eau derrière les roues sur route mouillée
  const wet = c.vis.wet ?? 0;
  if (wet > 0.3 && y < 0.05 && sp > 12) { const n = sp > 40 ? 2 : 1, amt = Math.min(1.4, sp / 45) * wet;
    for (let k = 0; k < n; k++) for (const s of (c.vis.moto ? [0] : [-1, 1])) { const [wx, wz] = side(s * hw * 0.85, len * 0.62); fx.splash(wx, 0.15, wz, vx, vz, amt); } }
  if (y < 0.05 && sp > 6 && (c.drifting || c.slip > 2.5 || Math.abs(c.u) > 4 || (c.braking && sp > 22))) { const m = c.vis.moto;
    for (let k = 0; k < (m ? 1 : 2); k++) { const [wx, wz] = side(m ? 0 : (k ? 1 : -1) * hw * 0.8, len * 0.6); c.sk[k] = skids.mark(c.sk[k], wx, wz, m ? 0.15 : 0.26); } } else c.sk[0] = c.sk[1] = null;
  if (c.landed) { for (let k = 0; k < 8; k++) fx.smoke(c.x + (Math.random() - 0.5) * 2, 0.3, c.z + (Math.random() - 0.5) * 2, vx * 0.2, 1.5, vz * 0.2, 1.6, 0.3, 1.1); for (let k = 0; k < Math.min(12, c.landed); k++) fx.spark(c.x, 0.3, c.z, vx * 0.4, 2, vz * 0.4);
    if (c.isPlayer) { c.bump = Math.max(c.bump || 0, Math.min(1, c.landed / 22)); audio.thud(c.landed / 30); } c.landed = 0; }
  if (c.onWall && sp > 4 && c.wx !== undefined) {
    const k = Math.min(1, sp / 45), n = 1 + Math.floor(k * 5);   // le contact se fait sur toute la longueur du flanc
    for (let q = 0; q < n; q++) { const f = Math.random() * 2 - 1, px = c.wx + sh * len * f, pz = c.wz + ch * len * f, h = 0.3 + Math.random() * 0.4;
      if (Math.random() < 0.35) fx.ember(px, h, pz, vx * 0.75, 1.5, vz * 0.75); else fx.spark(px, h, pz, vx * 0.6, 1, vz * 0.6); }
    if (Math.random() < 0.25 + k * 0.5) fx.dust(c.wx, 0.4, c.wz, vx * 0.3, 0.8, vz * 0.3, 0.8 + k);
    if (sp > 30 && Math.random() < 0.3) fx.flame(c.wx, 0.45, c.wz, vx * 0.5, 0.5, vz * 0.5, 0.9);
    c.scr = scratches.markWall(c.scr, c.wx, c.wz);
    if (c.isPlayer) c.bump = Math.max(c.bump || 0, 0.12 + k * 0.2);
  } else c.scr = null;
  if (c.rubT > 0) c.rubT -= dt;
  void dt;
}
function syncVisuals(dt) {
  for (const c of cars) {
    if (c.out) continue;
    c.vis.root.position.set(c.x, c.y, c.z); c.vis.root.rotation.set(c.pitch, c.h + c.spinA + c.driftA, 0);
    if (c.vis.root.scale.x !== c.scale) c.vis.root.scale.setScalar(c.scale);
    setGhost(c.vis, c.ghostT > 0);
    setLod(c.vis, c !== player && c !== gCar && camera.position.distanceToSquared(c.vis.root.position) > 42 * 42);
    animateCar(c.vis, dt, c.v, c.steer, c.yaw * Math.abs(c.v), c.lon, c.braking, time);
    // de jour, l'ombre s'étire à l'opposé du soleil
    // phares visibles seulement de face, feux arrière seulement de dos
    if (c.vis.heads) { const dx = camera.position.x - c.x, dz = camera.position.z - c.z, dl = Math.hypot(dx, dz) || 1, f = (-Math.sin(c.h) * dx - Math.cos(c.h) * dz) / dl;
      for (const hd of c.vis.heads) hd.material.opacity *= clamp(f * 2 + 0.2, 0, 1); for (const g of c.vis.glows) g.material.opacity *= clamp(-f * 2 + 0.4, 0, 1); }
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
    simSteps(dt, simulate); syncVisuals(dt);
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
    engineSound(playerInput().gas);
  } else if (mode === 'race' || mode === 'finished') {
    if (G.type === 'foot') { if (mode === 'race') simSteps(dt, stepFoot); } else simSteps(dt, simulate);
    syncVisuals(dt);
    if (mode !== 'race') { demoAng += dt * 0.3; orbitCam(player, player.h + Math.PI + Math.sin(demoAng) * 0.8, 7, 2.0, 0); }
    else if (G.type === 'foot') { ballCam(player, dt, chaseSnap); chaseSnap = false; }
    else chaseCam(player, dt, false);
    blur = G.type === 'foot' ? clamp(player.v / 120, 0, 0.5) : clamp(player.v / 80, 0, 1) + (player.boosting ? 0.3 : 0);
    if (mode === 'race') engineSound(player.boosting ? 1 : player.inp ? (player.inp.gas || 0) : 0); else audio.engine(null);
    audio.scrape(mode === 'race' ? Math.max(player.onWall && Math.abs(player.v) > 4 ? 0.25 + Math.abs(player.v) / 60 : 0, player.rubT > 0 ? 0.55 : 0) : 0);
    audio.skid(mode === 'race' && player.y < 0.05 && Math.abs(player.v) > 6 ? (player.drifting ? 0.5 + Math.abs(player.driftA) : clamp((player.slip - 1.5) / 8, 0, 0.7)) : 0);
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
          $('h-lap').textContent = clamp(player.lap, 1, G.laps);
          $('h-time').textContent = fmt(raceTime);
          $('wrong').hidden = !(Math.cos(wrap(player.h - T.heading(player.idx))) < -0.3 && Math.abs(player.v) > 3);
          if (G.type === 'items') $('h-item').textContent = player.item ? 'Objet : ' + ITEM_NAMES[player.item] : player.shieldT > 0 ? 'Bouclier actif' : player.ghostT > 0 ? 'Fantôme' : player.giantT > 0 ? 'Géant' : player.nitroT > 0 ? 'Nitro' : player.iceT > 0 ? 'Verglas' : player.magnetT > 0 ? 'Aimant' : '';
          drawMini();
        }
      } else if (G.type !== 'foot') fillResults();
    }
    if (mode === 'race') $('h-drift').textContent = G.dr.cur >= 20 ? 'DÉRAPAGE ' + Math.round(G.dr.cur) : '';
    if (flashT > 0) { flashT -= dt; if (flashT <= 0) $('h-msg').textContent = ''; }
  }
  stepStorm(dt);
  if (toastT > 0) { toastT -= dt; if (toastT <= 0) $('toast').hidden = true; }
  if (mode !== 'pause') fx.update(dt);
  if (focus) world.update(camera, dt, time, focus.idx || 0);
  if (world.rain && world.rain.visible) { const wd = world.theme && world.theme.orage ? 16 + Math.sin(time * 0.7) * 8 : 0; world.updateRain(camera, dt, (focus ? -Math.sin(focus.h) * focus.v : 0) + wd, (focus ? -Math.cos(focus.h) * focus.v : 0) + wd * 0.4); }
  if (composer) {
    finalPass.uniforms.amt.value = blur; finalPass.uniforms.time.value = time;
    const fu = finalPass.uniforms; fu.sunVis.value = 0;
    if (world.sunDir) {
      sunV.copy(camera.position).addScaledVector(world.sunDir, 1000).project(camera);
      const facing = camera.getWorldDirection(sunW).dot(world.sunDir);
      if (facing > 0 && Math.abs(sunV.x) < 1.25 && Math.abs(sunV.y) < 1.25) {
        fu.sunPos.value.set(sunV.x * 0.5 + 0.5, sunV.y * 0.5 + 0.5); fu.sunCol.value.copy(world.sunCol); fu.aspect.value = W / H;
        fu.sunVis.value = clamp(1.3 - Math.max(Math.abs(sunV.x), Math.abs(sunV.y)), 0, 1) * (world.theme.flare ?? 1);
      }
    }
    composer.render(dt);
  } else renderer.render(scene, camera);
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
    for (let t = 0; t < seconds; t += step) simSteps(step, G.type === 'foot' ? stepFoot : simulate);
    return cars.map(c => ({ n: c.name, lap: c.lap, idx: c.idx, v: +c.v.toFixed(1), best: c.bestLap }));
  },
  go(m) { mode = m; }, audio, set garageAng(v) { garageAng = v; garageDrag = { x: 0, a: v }; }, garageShow(i) { garageShow(i); }, eco, customs, applyCustoms() { edDirty = true; edShape.add(0); edApply(); }, get player() { return player; }, get RU() { return RU; }, give(it) { player.item = it; useItem(player); }, start(t) { startGame(t); },
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
