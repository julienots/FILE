import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { ShaderPass } from 'three/addons/postprocessing/ShaderPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { U } from './shared.js';
import { makeTextures } from './textures.js';
import { buildTerrain, DOME_R, WORLD_R, VILLAGE } from './terrain.js';
import { buildWater } from './water.js';
import { buildGrass, buildFlowers, buildTrees, buildRocks, buildBushes } from './vegetation.js';
import { makeDetailTexture, applyGroundShader } from './ground.js';
import { buildGrassField, createPushers } from './grassfield.js';
const pushers = createPushers();
import { createGame, ITEMS } from './game.js';
import { AtmospherePass } from './atmosphere.js';
import { applyToonShading } from './toon.js';
applyToonShading();
import { loadExternalModels } from './models.js';
import { makeVillageMaterials, buildVillage, buildHouse } from './village.js';
import { buildHumanoid, buildVillagers, buildSheep, buildBirds } from './characters.js';
import { buildDome, buildCosmos, buildPedestal, buildCreator, buildSunMoon, buildClouds } from './dome.js';
import { smoothstep, clamp, lerp } from './noise.js';

// ------------------------------------------------------------------ qualité
const params = new URLSearchParams(location.search);
const isMobile = /Android|iPhone|iPad|Mobile/i.test(navigator.userAgent) || Math.min(screen.width, screen.height) < 600;
const QUALITY = params.get('q') || (isMobile ? 'medium' : 'high');
const Q = {
  high: { pr: Math.min(devicePixelRatio, 2), shadow: 4096, grass: 26000, flowers: 6000, msaa: 4, bloom: true, atmo: { aoSamples: 10, raySamples: 32 }, grassLayers: [{ cell: 0.05, r: 9, seg: 5 }, { cell: 0.12, rIn: 8.3, r: 30, seg: 3 }] },
  medium: { pr: Math.min(devicePixelRatio, 1.3), shadow: 2048, grass: 12000, flowers: 3000, msaa: 2, bloom: true, atmo: { aoSamples: 6, raySamples: 18 }, grassLayers: [{ cell: 0.07, r: 8, seg: 4 }, { cell: 0.17, rIn: 7.3, r: 22, seg: 3 }] },
  low: { pr: 1, shadow: 1024, grass: 7000, flowers: 1500, msaa: 0, bloom: false, atmo: { aoSamples: 0, raySamples: 0 }, grassLayers: [{ cell: 0.1, r: 7, seg: 3 }, { cell: 0.24, rIn: 6.3, r: 15, seg: 2 }] },
}[QUALITY] || {};
const setMsg = m => { const e = document.getElementById('loadMsg'); if (e) e.textContent = m; };
const nextFrame = () => new Promise(r => requestAnimationFrame(() => setTimeout(r, 0)));

// ------------------------------------------------------------------ rendu
const canvas = document.getElementById('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance', preserveDrawingBuffer: params.has('capture') });
renderer.setPixelRatio(Q.pr);
renderer.setSize(innerWidth, innerHeight, false);
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.05;

const scene = new THREE.Scene();
scene.fog = new THREE.FogExp2(U.uFogColor.value.clone(), 0.004);
const camera = new THREE.PerspectiveCamera(40, innerWidth / innerHeight, 0.5, 9000);

// lumières : soleil (ombres dynamiques), ciel/sol hémisphérique, lumière d'intérieur
const sunLight = new THREE.DirectionalLight(0xffffff, 3);
sunLight.castShadow = true;
sunLight.shadow.mapSize.set(Q.shadow, Q.shadow);
sunLight.shadow.bias = -0.0004; sunLight.shadow.normalBias = 0.6;
scene.add(sunLight, sunLight.target);
const hemi = new THREE.HemisphereLight(0xbfdcff, 0x6b5a3a, 1.2);
scene.add(hemi);
const fill = new THREE.DirectionalLight(0xffe6c8, 0.25); fill.position.set(-1, 0.4, -0.6); scene.add(fill);
const interior = new THREE.PointLight(0xffb873, 0, 9, 1.6); scene.add(interior);

// post-production : bloom HDR + étalonnage (vignette, saturation, teinte chaude) + tone mapping ACES
const rt = new THREE.WebGLRenderTarget(1, 1, { type: THREE.HalfFloatType, samples: Q.msaa });
rt.depthTexture = new THREE.DepthTexture(1, 1); rt.depthTexture.type = THREE.UnsignedIntType;
const composer = new EffectComposer(renderer, rt);
composer.setPixelRatio(Q.pr);
composer.setSize(innerWidth, innerHeight);
composer.addPass(new RenderPass(scene, camera));
const atmo = Q.atmo ? new AtmospherePass(camera, Q.atmo) : null;
if (atmo) composer.addPass(atmo);
const bloom = new UnrealBloomPass(new THREE.Vector2(innerWidth / 2, innerHeight / 2), 0.32, 0.55, 0.92);
if (Q.bloom) composer.addPass(bloom);
const grade = new ShaderPass({
  uniforms: { tDiffuse: { value: null }, uVig: { value: 0.32 }, uSat: { value: 1.06 }, uWarm: { value: 0.025 }, uFlash: { value: 0 } },
  vertexShader: `varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix*modelViewMatrix*vec4(position,1.0); }`,
  fragmentShader: `uniform sampler2D tDiffuse; uniform float uVig, uSat, uWarm, uFlash; varying vec2 vUv;
    void main(){ vec4 c = texture2D(tDiffuse, vUv);
      float l = dot(c.rgb, vec3(0.2126,0.7152,0.0722));
      c.rgb = mix(vec3(l), c.rgb, uSat);
      // virage partiel : ombres légèrement bleutées, hautes lumières chaudes
      c.rgb += mix(vec3(-0.004, 0.0, 0.012), vec3(0.012, 0.004, -0.01), smoothstep(0.05, 0.8, l)) * 1.5;
      c.rgb *= vec3(1.0 + uWarm, 1.0, 1.0 - uWarm);
      vec2 d = vUv - 0.5; c.rgb *= 1.0 - uVig * smoothstep(0.25, 0.85, dot(d,d)*2.2);
      c.rgb += vec3(0.6,0.9,1.0) * uFlash;
      gl_FragColor = c; }`,
});
composer.addPass(grade);
composer.addPass(new OutputPass());

// ------------------------------------------------------------------ monde (UNE seule scène pour les deux modes)
const world = { village: VILLAGE };
let bushes, game, grassField, detailTex, terrain, village, trees, rocks, grass, dome, creator, sunMoon, clouds, birds, sheep, villagers, player, pedestal, beacon, motes, M, T;
const colliders = [], circles = [], floors = [], surfaces = [];

async function buildWorld() {
  setMsg('Textures procédurales…'); await nextFrame();
  T = makeTextures();
  detailTex = makeDetailTexture(512);
  setMsg('Sculpture du relief et de la rivière…'); await nextFrame();
  terrain = buildTerrain();
  applyGroundShader(terrain.mesh.material, detailTex);
  scene.add(terrain.mesh);
  scene.add(buildWater(terrain));
  setMsg('Pousse de la végétation…'); await nextFrame();
  grass = buildGrass(terrain, Q.grass); scene.add(grass);
  scene.add(buildFlowers(terrain, Q.flowers));
  trees = buildTrees(terrain, T, { detail: detailTex }); scene.add(trees);
  rocks = buildRocks(terrain, 160, detailTex); scene.add(rocks);
  grassField = buildGrassField(terrain, { layers: Q.grassLayers }); scene.add(grassField);
  bushes = buildBushes(terrain, 300, T); scene.add(bushes);
  setMsg('Construction du village…'); await nextFrame();
  M = makeVillageMaterials(T);
  village = buildVillage(M, terrain, T);
  scene.add(village.group);
  colliders.push(...village.colliders); floors.push(...village.floors); surfaces.push(...village.surfaces);
  circles.push(...trees.userData.colliders, ...rocks.userData.colliders, ...bushes.userData.colliders);
  floors.forEach(f => grassField.userData.clearRect(f.x, f.z, f.hw + 0.6, f.hd + 0.6, f.rot));
  village.colliders.filter(c => c.hw > 1).forEach(c => grassField.userData.clearRect(c.x, c.z, c.hw + 0.3, c.hd + 0.3, c.rot));
  setMsg('Le dôme et le cosmos…'); await nextFrame();
  dome = buildDome(); scene.add(dome.group);
  scene.add(buildCosmos());
  pedestal = buildPedestal(T); scene.add(pedestal);
  creator = buildCreator(T); scene.add(creator);
  sunMoon = buildSunMoon(T); scene.add(sunMoon.sun, sunMoon.moon);
  clouds = buildClouds(T); scene.add(clouds.mesh);
  birds = buildBirds(); scene.add(birds.group);
  setMsg('Les habitants s\'éveillent…'); await nextFrame();
  sheep = buildSheep(terrain); sheep.list.forEach(s => scene.add(s.root));
  villagers = buildVillagers(terrain, world); villagers.list.forEach(v => scene.add(v.root));
  player = buildHumanoid({ tunic: '#2f8a7e', tunic2: '#efe4cc', scarf: '#e0783a', hair: '#3a2618', pack: true, staff: true, pants: '#4d3b2c' });
  player.root.userData.isPlayer = true;
  scene.add(player.root);
  beacon = buildBeacon(); scene.add(beacon);
  motes = buildMotes(); scene.add(motes.points);
}

world.groundAt = (x, z, curY = Infinity) => {
  let h = terrain.heightAt(x, z);
  for (const f of floors) {
    const dx = x - f.x, dz = z - f.z, c = Math.cos(f.rot), s = Math.sin(f.rot);
    const lx = dx * c - dz * s, lz = dx * s + dz * c;
    if (Math.abs(lx) < f.hw && Math.abs(lz) < f.hd && curY > f.y - 0.7) h = Math.max(h, f.y);
  }
  for (const f of surfaces) {
    const dx = x - f.x, dz = z - f.z, c = Math.cos(f.rot), s = Math.sin(f.rot);
    const lx = dx * c - dz * s, lz = dx * s + dz * c;
    if (Math.abs(lx) < f.hw && Math.abs(lz) < f.hd) { const y = f.heightAt(lz); if (curY > y - 1.0) h = Math.max(h, y); }
  }
  return h;
};
world.insideFloor = (x, z) => floors.find(f => {
  const dx = x - f.x, dz = z - f.z, c = Math.cos(f.rot), s = Math.sin(f.rot);
  return Math.abs(dx * c - dz * s) < f.hw && Math.abs(dx * s + dz * c) < f.hd;
});
// résolution des collisions : murs (OBB), troncs/rochers (cercles), paroi du dôme
function solve(x, z, rad) {
  for (let it = 0; it < 2; it++) {
    for (const k of colliders) {
      const dx = x - k.x, dz = z - k.z;
      if (Math.abs(dx) > k.hw + k.hd + rad + 1 || Math.abs(dz) > k.hw + k.hd + rad + 1) continue;
      const c = Math.cos(k.rot), s = Math.sin(k.rot);
      const lx = dx * c - dz * s, lz = dx * s + dz * c;
      const px = clamp(lx, -k.hw, k.hw), pz = clamp(lz, -k.hd, k.hd);
      let ox = lx - px, oz = lz - pz, d = Math.hypot(ox, oz);
      if (d >= rad) continue;
      let nlx, nlz;
      if (d > 1e-5) { nlx = px + ox / d * rad; nlz = pz + oz / d * rad; }
      else { // à l'intérieur : sortir par l'axe le moins pénétré
        const ex = k.hw - Math.abs(lx), ez = k.hd - Math.abs(lz);
        if (ex < ez) { nlx = Math.sign(lx) * (k.hw + rad); nlz = lz; } else { nlx = lx; nlz = Math.sign(lz) * (k.hd + rad); }
      }
      x = k.x + nlx * c + nlz * s; z = k.z - nlx * s + nlz * c;
    }
    for (const k of circles) {
      const dx = x - k.x, dz = z - k.z, d = Math.hypot(dx, dz), m = k.r + rad;
      if (d < m && d > 1e-5) { x = k.x + dx / d * m; z = k.z + dz / d * m; }
    }
  }
  const r = Math.hypot(x, z), lim = WORLD_R - 3.5;
  if (r > lim) { x *= lim / r; z *= lim / r; }
  return { x, z };
}

// balise du point d'entrée (pilier de lumière)
function buildBeacon() {
  const g = new THREE.Group();
  const pillar = new THREE.Mesh(new THREE.CylinderGeometry(1.2, 1.2, 140, 24, 1, true), new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide, uniforms: { uTime: U.uTime },
    vertexShader: `varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix*modelViewMatrix*vec4(position,1.0); }`,
    fragmentShader: `uniform float uTime; varying vec2 vUv; void main(){ float a = (1.0 - vUv.y) * (0.5 + 0.5*sin(vUv.y*60.0 - uTime*4.0)) * 0.55 + (1.0-vUv.y)*0.25; gl_FragColor = vec4(vec3(0.5,0.92,1.0)*a, a); }`,
  }));
  pillar.position.y = 70; g.add(pillar);
  const ring = new THREE.Mesh(new THREE.RingGeometry(2.2, 3, 48), new THREE.MeshBasicMaterial({ color: new THREE.Color(1.2, 2.4, 3.0), transparent: true, opacity: 0.9, side: THREE.DoubleSide, depthWrite: false }));
  ring.rotation.x = -Math.PI / 2; ring.position.y = 0.3; g.add(ring);
  g.userData.ring = ring;
  g.renderOrder = 4;
  return g;
}

// pollen le jour, lucioles la nuit (autour du joueur)
function buildMotes(n = 260) {
  const pos = new Float32Array(n * 3), seed = new Float32Array(n);
  for (let i = 0; i < n; i++) { pos[i * 3] = (Math.random() - 0.5) * 50; pos[i * 3 + 1] = Math.random() * 6; pos[i * 3 + 2] = (Math.random() - 0.5) * 50; seed[i] = Math.random(); }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3)); g.setAttribute('aSeed', new THREE.BufferAttribute(seed, 1));
  const m = new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
    uniforms: { uTime: U.uTime, uNight: U.uNight, uCenter: { value: new THREE.Vector3() }, uFade: { value: 0 } },
    vertexShader: `attribute float aSeed; uniform float uTime; uniform vec3 uCenter; varying float vS;
      void main(){ vS = aSeed; vec3 p = position;
        p.x = mod(p.x - uCenter.x + 25.0 + sin(uTime*0.3 + aSeed*20.0)*2.0, 50.0) - 25.0 + uCenter.x;
        p.z = mod(p.z - uCenter.z + 25.0 + cos(uTime*0.25 + aSeed*13.0)*2.0, 50.0) - 25.0 + uCenter.z;
        p.y += uCenter.y - 1.0 + sin(uTime*0.8 + aSeed*30.0)*0.6;
        vec4 mv = modelViewMatrix*vec4(p,1.0); gl_Position = projectionMatrix*mv; gl_PointSize = min(10.0, (1.0 + aSeed*1.5) * 16.0 / -mv.z); }`,
    fragmentShader: `uniform float uNight; uniform float uTime; uniform float uFade; varying float vS;
      void main(){ float d = length(gl_PointCoord - 0.5); float a = smoothstep(0.5, 0.0, d);
        vec3 c = mix(vec3(1.0, 0.97, 0.85)*0.3, vec3(1.0, 0.85, 0.35)*2.5*(0.5+0.5*sin(uTime*3.0+vS*40.0)), uNight);
        gl_FragColor = vec4(c*a, a) * uFade; }`,
  });
  const points = new THREE.Points(g, m); points.frustumCulled = false; points.renderOrder = 5;
  return { points, mat: m };
}

// ------------------------------------------------------------------ cycle jour / nuit
let timeOfDay = parseFloat(params.get('t') || '9.5'), timeSpeed = 0.04; // heures de jeu par seconde
const SKY = {
  day: { top: new THREE.Color('#3d85d8'), hor: new THREE.Color('#cfe6f3'), sun: new THREE.Color('#fff1dc') },
  dusk: { top: new THREE.Color('#41579e'), hor: new THREE.Color('#f6b07a'), sun: new THREE.Color('#ffa060') },
  night: { top: new THREE.Color('#081127'), hor: new THREE.Color('#1d2a4c'), sun: new THREE.Color('#8fb0ff') },
};
const sunPos = new THREE.Vector3(), tmpC = new THREE.Color();
function updateSky() {
  const a = (timeOfDay - 6) / 12 * Math.PI; // 0 au lever, π au coucher
  const el = Math.sin(a);
  sunPos.set(Math.cos(a) * 0.82, el * 0.78 + 0.08, 0.42).normalize();
  sunMoon.sun.position.copy(sunPos).multiplyScalar(DOME_R * 0.86);
  sunMoon.moon.position.copy(sunPos).multiplyScalar(-DOME_R * 0.86); sunMoon.moon.position.y = Math.abs(sunMoon.moon.position.y) * (el < 0 ? 1 : -1);
  sunMoon.sun.visible = sunPos.y > -0.12; sunMoon.moon.visible = el < 0.15;
  const day = smoothstep(-0.05, 0.35, el), dusk = smoothstep(-0.25, 0.05, el) * (1 - smoothstep(0.1, 0.45, el));
  const night = 1 - smoothstep(-0.2, 0.05, el);
  U.uNight.value = night;
  const blend = (k) => tmpC.copy(SKY.night[k]).lerp(SKY.day[k], day).lerp(SKY.dusk[k], dusk * 0.85).clone();
  U.uSkyTop.value.copy(blend('top')); U.uSkyHorizon.value.copy(blend('hor')); U.uSunColor.value.copy(blend('sun'));
  const rain = game ? game.rain : 0;
  if (rain > 0) { const grey = new THREE.Color('#7d8894').multiplyScalar(lerp(0.25, 1, day)); U.uSkyTop.value.lerp(grey, rain * 0.75); U.uSkyHorizon.value.lerp(grey.clone().multiplyScalar(1.15), rain * 0.7); U.uSunColor.value.lerp(new THREE.Color('#c8d0dc'), rain * 0.6); }
  // lumière principale : soleil le jour, lune la nuit
  const lightDir = el > -0.05 ? sunPos.clone() : sunMoon.moon.position.clone().normalize();
  U.uSunDir.value.copy(lightDir);
  sunLight.color.copy(U.uSunColor.value);
  sunLight.intensity = lerp(0.35, 3.1, day) * (1 - dusk * 0.25) * (1 - rain * 0.7);
  hemi.color.copy(U.uSkyTop.value).lerp(new THREE.Color(1, 1, 1), 0.45);
  hemi.intensity = lerp(0.35, 1.55, day);
  hemi.groundColor.set('#6b5a3a').multiplyScalar(lerp(0.4, 1, day));
  fill.intensity = 0.25 * day;
  M.glass.emissiveIntensity = lerp(0.25, 2.6, 1 - day);
  village.lanterns.userData.glowMat.opacity = lerp(0, 0.9, 1 - day);
  M.lamp.emissiveIntensity = lerp(0.8, 3.5, 1 - day);
  renderer.toneMappingExposure = lerp(1.25, 1.12, day);
}

// ------------------------------------------------------------------ caméras & modes
const god = { theta: 0.3, phi: 0.42, dist: 330, target: new THREE.Vector3(0, 12, 0), vTheta: 0, vPhi: 0, idle: 0 };
// le Créateur se tient du côté d'où regardait la caméra divine, haut au-dessus de la voûte
function creatorPos() {
  const el = 0.95, d = 380;
  return new THREE.Vector3(Math.sin(god.theta) * Math.cos(el) * d, Math.sin(el) * d, Math.cos(god.theta) * Math.cos(el) * d);
}
const tp = { yaw: 0, yawT: null, pitch: 0.18, dist: 5.6, distT: 5.6, pitchT: null, head: new THREE.Vector3() };
let mode = 'god'; // god | toPlay | play | toGod
let entry = new THREE.Vector3(-6, 0, 4);
const pl = { vy: 0, grounded: true, run: false, speed: 0, dash: 0 };
const input = { jx: 0, jy: 0, keys: new Set() };

function godPose(out = {}) {
  const p = new THREE.Vector3(Math.sin(god.theta) * Math.cos(god.phi), Math.sin(god.phi), Math.cos(god.theta) * Math.cos(god.phi)).multiplyScalar(god.dist).add(god.target);
  out.pos = p; out.look = god.target.clone(); return out;
}
function tpPose() {
  const p = player.root.position;
  const head = new THREE.Vector3(p.x, p.y + 1.42, p.z);
  const inHouse = world.insideFloor(p.x, p.z);
  const dist = inHouse ? Math.min(tp.dist, 2.8) : tp.dist;
  const off = new THREE.Vector3(Math.sin(tp.yaw) * Math.cos(tp.pitch), Math.sin(tp.pitch), Math.cos(tp.yaw) * Math.cos(tp.pitch));
  // la caméra ne traverse pas les murs : on avance du joueur vers la caméra et on s'arrête avant l'obstacle
  let free = dist;
  for (let t = 0.4; t <= dist; t += 0.2) {
    const q = head.clone().addScaledVector(off, t);
    if (colliders.some(k => { if (q.y < k.y0 - 0.2 || q.y > k.y1 + 0.2) return false; const dx = q.x - k.x, dz = q.z - k.z, c = Math.cos(k.rot), s2 = Math.sin(k.rot); return Math.abs(dx * c - dz * s2) < k.hw + 0.25 && Math.abs(dx * s2 + dz * c) < k.hd + 0.25; })) { free = Math.max(0.6, t - 0.3); break; }
  }
  const pos = head.clone().addScaledVector(off, free);
  const g = world.groundAt(pos.x, pos.z, pos.y + 5) + 0.35;
  if (pos.y < g) pos.y = g;
  const look = pos.clone().addScaledVector(off, -10);
  // vers le haut, on remonte le point visé pour garder l'habitant en bas de l'image
  if (tp.pitch < 0) look.y += -tp.pitch * 1.5;
  return { pos, look, head };
}

function placePlayer(x, z, faceCenter = true) {
  const s = solve(x, z, 0.35);
  player.root.position.set(s.x, world.groundAt(s.x, s.z), s.z);
  if (faceCenter) player.root.rotation.y = Math.atan2(VILLAGE.x - s.x, VILLAGE.y - s.z);
  tp.yaw = player.root.rotation.y + Math.PI; tp.pitch = 0.16;
}

// --- transitions cinématiques ---
let tr = null;
const easeInOut = t => t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
const flashEl = document.getElementById('flash'), captionEl = document.getElementById('caption'), barsEl = document.getElementById('bars');
function startEnter() {
  if (mode !== 'god') return;
  const E = entry.clone(); E.y = world.groundAt(E.x, E.z);
  placePlayer(E.x, E.z);
  const P0 = camera.position.clone(), L0 = god.target.clone();
  const away = new THREE.Vector3(P0.x - E.x, 0, P0.z - E.z).normalize();
  const P1 = new THREE.Vector3(E.x + away.x * 70, DOME_R + 38, E.z + away.z * 70);
  const P2 = new THREE.Vector3(E.x + away.x * 14, E.y + 26, E.z + away.z * 14);
  tp.yaw = Math.atan2(away.x, away.z); tp.pitch = 0.16;
  player.root.rotation.y = tp.yaw + Math.PI;
  const end = tpPose();
  const curve = new THREE.CatmullRomCurve3([P0, P1, P2, end.pos], false, 'centripetal', 0.5);
  tr = { kind: 'in', t: 0, dur: 6.2, curve, looks: [L0, E.clone(), E.clone().add(new THREE.Vector3(0, 1.2, 0)), end.look], fov: [40, 60], crossed: false };
  mode = 'toPlay';
  setUI();
}
function startExit() {
  if (mode !== 'play') return;
  const P0 = camera.position.clone();
  const p = player.root.position.clone();
  const gp = godPose();
  const away = new THREE.Vector3(gp.pos.x - p.x, 0, gp.pos.z - p.z).normalize();
  const P1 = new THREE.Vector3(p.x + away.x * 10, p.y + 24, p.z + away.z * 10);
  const P2 = new THREE.Vector3(p.x + away.x * 80, DOME_R + 45, p.z + away.z * 80);
  const curve = new THREE.CatmullRomCurve3([P0, P1, P2, gp.pos], false, 'centripetal', 0.5);
  const lk = tpPose().look;
  tr = { kind: 'out', t: 0, dur: 5.2, curve, looks: [lk, p.clone().add(new THREE.Vector3(0, 1.5, 0)), p.clone(), gp.look], fov: [60, 40], crossed: false };
  mode = 'toGod';
  closeDialog();
  setUI();
}
function lookAlong(looks, s) {
  const n = looks.length - 1, f = s * n, i = Math.min(n - 1, Math.floor(f)), t = easeInOut(f - i);
  return looks[i].clone().lerp(looks[i + 1], t);
}
function updateTransition(dt) {
  tr.t += dt / tr.dur;
  const t = Math.min(1, tr.t), s = easeInOut(t);
  // la caméra accélère au début, freine à l'arrivée
  camera.position.copy(tr.curve.getPoint(s));
  camera.lookAt(lookAlong(tr.looks, s));
  camera.fov = tr.kind === 'in' ? lerp(40, 60, smoothstep(0.35, 0.95, s)) : lerp(60, 40, smoothstep(0.05, 0.65, s));
  camera.updateProjectionMatrix();
  // franchissement de la paroi du dôme : onde lumineuse + éclair
  const r = camera.position.length();
  const inside = r < DOME_R;
  if (!tr.crossed && ((tr.kind === 'in' && inside) || (tr.kind === 'out' && !inside))) {
    tr.crossed = true;
    dome.uniforms.uRipplePos.value.copy(camera.position).setLength(DOME_R);
    rippleT = 0.0001;
    flashT = 1;
    captionEl.textContent = tr.kind === 'in' ? 'VOUS ENTREZ DANS VOTRE MONDE' : 'VOUS REDEVENEZ LE CRÉATEUR';
    captionEl.classList.add('on'); setTimeout(() => captionEl.classList.remove('on'), 2200);
  }
  if (t >= 1) {
    if (tr.kind === 'in') { mode = 'play'; } else { mode = 'god'; }
    tr = null; setUI();
  }
}
let rippleT = 0, flashT = 0, uiTick = 0, actionPickup = null;

// ------------------------------------------------------------------ boucle
const clock = new THREE.Clock();
let fpsAcc = 0, fpsN = 0, fps = 60, perfT = 0, dynPR = Q.pr;
const perfEl = document.getElementById('perf');

function update(dt) {
  U.uTime.value += dt;
  const time = U.uTime.value;
  timeOfDay = (timeOfDay + dt * timeSpeed + 24) % 24;
  updateSky();
  syncTimeUI();

  // simulation autonome (tourne dans tous les modes)
  villagers.update(dt, solve);
  sheep.update(dt, world);
  birds.update(dt, time);
  clouds.update(dt, 1 + timeSpeed * 20);
  village.mill.blades.rotation.z += dt * 0.6;
  pedestal.userData.frags.children.forEach((f, i) => { f.rotation.y += dt * (f.userData.spin || 0.05); f.position.y += Math.sin(time * 0.5 + i) * dt * 0.6; });

  game.update(dt, mode, time);
  if ((uiTick += dt) > 0.4) {
    uiTick = 0; $('objective').textContent = game.objective();
    // ombres des personnages : seulement ceux proches de l'habitant (économie d'appels de dessin)
    const pp = player.root.position;
    for (const c of [...villagers.list, ...sheep.list]) {
      const cast = mode === 'play' && c.root.position.distanceTo(pp) < 28;
      if (c.root.userData.setLOD) c.root.userData.setLOD(c.root.position.distanceTo(camera.position) > 45);
      if (c.root.userData.cast !== cast) { c.root.userData.cast = cast; c.root.traverse(o => { if (o.isMesh) o.castShadow = cast && !o.userData.tiny; }); }
    }
  }
  for (const v of villagers.list) v.root.userData.extAnimate?.(v.state.speed, time);
  player.root.userData.extAnimate?.(pl.speed, time);
  if (mode === 'god') updateGodCamera(dt);
  else if (mode === 'play') updatePlayer(dt);
  else { updateTransition(dt); if (mode === 'toPlay' || mode === 'toGod') player.animate(dt, 0); }
  if (mode === 'god') player.animate(dt, 0);

  // paramètres dépendant de la position caméra : "à l'intérieur" du dôme ou non
  const camR = camera.position.length();
  const inside = smoothstep(DOME_R + 6, DOME_R - 30, camR);
  U.uInside.value = inside;
  const reveal = mode === 'play' ? smoothstep(-0.2, -0.9, tp.pitch) : 0;
  dome.uniforms.uReveal.value += (reveal - dome.uniforms.uReveal.value) * Math.min(1, dt * 3);
  scene.fog.color.copy(U.uSkyHorizon.value);
  U.uFogColor.value.copy(U.uSkyHorizon.value);
  scene.fog.density = lerp(0.0006, atmo ? 0.0026 : 0.0058, inside);
  if (atmo) atmo.update(sunMoon.sun.position, inside * (sunMoon.sun.visible ? 1 : 0.0) + (sunMoon.sun.visible ? 0 : inside));
  U.uFogDensity.value = scene.fog.density;
  camera.near = lerp(2.0, 0.12, smoothstep(DOME_R + 40, DOME_R - 40, camR));
  camera.far = 9000; camera.updateProjectionMatrix();
  // le Créateur apparaît au-delà de la voûte, là où se tenait la caméra divine
  creator.position.copy(creatorPos());
  dome.uniforms.uCreatorPos.value.copy(creator.position);
  creator.lookAt(0, 0, 0);
  creator.userData.setFade(inside * (mode === 'play' || mode === 'toPlay' ? 1 : inside));
  creator.userData.blink(time);
  // onde du dôme / éclair
  if (rippleT > 0) { rippleT = Math.min(1, rippleT + dt / 2.4); dome.uniforms.uRippleT.value = rippleT >= 1 ? 0 : rippleT; if (rippleT >= 1) rippleT = 0; }
  flashT = Math.max(0, flashT - dt * 1.6);
  flashEl.style.opacity = (flashT * flashT * 0.85).toFixed(3);
  grade.uniforms.uFlash.value = flashT * 0.25;
  // ombres : cadrage large en vue divine, serré et net autour de l'habitant
  const focus = mode === 'god' || mode === 'toGod' && !tr?.crossed ? new THREE.Vector3(0, 0, 0) : player.root.position;
  const half = lerp(128, 42, inside);
  const sc = sunLight.shadow.camera;
  sc.left = -half; sc.right = half; sc.top = half; sc.bottom = -half; sc.near = 1; sc.far = 600; sc.updateProjectionMatrix();
  const f = new THREE.Vector3().copy(focus).multiplyScalar(inside);
  sunLight.target.position.copy(f);
  sunLight.position.copy(f).addScaledVector(U.uSunDir.value, 260);
  // lumière d'intérieur de maison
  const inHouse = mode === 'play' && world.insideFloor(player.root.position.x, player.root.position.z);
  interior.intensity += ((inHouse ? 6 : 0) - interior.intensity) * Math.min(1, dt * 4);
  if (inHouse) interior.position.set(inHouse.x, inHouse.y + 2.4, inHouse.z);
  // particules autour de l'habitant
  motes.mat.uniforms.uCenter.value.copy(player.root.position);
  // l'herbe haute définition suit le regard (devant la caméra) en mode habitant
  { const gc = grassField.userData.uniforms; const fwd = new THREE.Vector3(); camera.getWorldDirection(fwd); fwd.y = 0; if (fwd.lengthSq() > 0) fwd.normalize();
    gc.uCenter.value.copy(player.root.position).addScaledVector(fwd, mode === 'god' ? 0 : 4); gc.uFade.value = smoothstep(0.3, 0.9, inside); grassField.visible = inside > 0.3;
    const farR = Q.grassLayers[1].r + 2;
    U.uGrassCut.value = inside > 0.5 ? farR : 0; U.uGrassCutCenter.value.copy(gc.uCenter.value);
    pushers(dt, player, [...villagers.list, ...sheep.list.map(s => ({ root: s.root, pushR: 0.8 })), ...game._enemies.filter(e => !e.dead).map(e => ({ root: e.g, pushR: 0.95 }))], gc); }
  motes.mat.uniforms.uFade.value = inside;
  // balise
  beacon.visible = mode === 'god' || (mode === 'toPlay' && inside < 0.6);
  beacon.position.set(entry.x, world.groundAt(entry.x, entry.z), entry.z);
  beacon.userData.ring.scale.setScalar(1 + Math.sin(time * 3) * 0.12);
  // visibilité du verre extérieur quand on est dedans
  dome.group.children[1].visible = inside < 0.999;
  grade.uniforms.uVig.value = lerp(0.42, 0.28, inside);
}

function updateGodCamera(dt) {
  god.idle += dt;
  if (god.idle > 5) god.vTheta += (0.03 - god.vTheta) * dt * 0.5; // rotation lente du globe quand on n'y touche pas
  god.theta += god.vTheta * dt;
  if (god.idle <= 5) god.vTheta *= Math.pow(0.04, dt);
  god.phi = clamp(god.phi + god.vPhi * dt, 0.06, 1.45); god.vPhi *= Math.pow(0.04, dt);
  const p = godPose();
  camera.position.copy(p.pos); camera.lookAt(p.look);
  camera.fov = 40; camera.updateProjectionMatrix();
}

function updatePlayer(dt) {
  // entrées
  let jx = input.jx, jy = input.jy;
  const k = input.keys;
  if (k.has('KeyW') || k.has('ArrowUp') || k.has('KeyZ')) jy = 1;
  if (k.has('KeyS') || k.has('ArrowDown')) jy = -1;
  if (k.has('KeyA') || k.has('ArrowLeft') || k.has('KeyQ')) jx = -1;
  if (k.has('KeyD') || k.has('ArrowRight')) jx = 1;
  const mag = Math.min(1, Math.hypot(jx, jy));
  const run = pl.run || k.has('ShiftLeft') || mag > 0.97 && input.jx !== 0 && false;
  const p = player.root.position;
  const fx = -Math.sin(tp.yaw), fz = -Math.cos(tp.yaw), rx = Math.cos(tp.yaw), rz = -Math.sin(tp.yaw);
  let mx = fx * jy + rx * jx, mz = fz * jy + rz * jx;
  const ml = Math.hypot(mx, mz);
  const inWater = terrain.wetAt(p.x, p.z) > 0.6 && p.y < terrain.heightAt(p.x, p.z) + 0.2;
  let target = ml > 0.01 ? mag * (run ? 6.8 : 3.4) * (inWater ? 0.6 : 1) : 0;
  // Élan : ruée de 0,25 s dans la direction du regard (ou du déplacement)
  if (pl.dash > 0) { pl.dash -= dt; target = 17; if (ml < 0.01) { mx = Math.sin(player.root.rotation.y); mz = Math.cos(player.root.rotation.y); } }
  pl.speed += (target - pl.speed) * Math.min(1, dt * (pl.dash > 0 ? 30 : 8));
  const mlen = Math.hypot(mx, mz);
  if (mlen > 0.01) {
    mx /= mlen; mz /= mlen;
    const yaw = Math.atan2(mx, mz);
    let d = yaw - player.root.rotation.y; d = Math.atan2(Math.sin(d), Math.cos(d));
    player.root.rotation.y += d * Math.min(1, dt * 12);
    const nx = p.x + mx * pl.speed * dt, nz = p.z + mz * pl.speed * dt;
    // falaises infranchissables
    const g0 = world.groundAt(p.x, p.z, p.y), g1 = world.groundAt(nx, nz, p.y);
    if (g1 - g0 < Math.max(0.45, pl.speed * dt * 1.6) || !pl.grounded) {
      const s = solve(nx, nz, 0.35); p.x = s.x; p.z = s.z;
    }
  }
  // saut & gravité
  const ground = world.groundAt(p.x, p.z, p.y);
  if ((k.has('Space') || input.jump) && pl.grounded) { pl.vy = 6.2; pl.grounded = false; input.jump = false; }
  pl.vy -= 18 * dt;
  p.y += pl.vy * dt;
  if (p.y <= ground) { p.y = pl.grounded ? lerp(p.y, ground, 1) : ground; pl.vy = 0; pl.grounded = true; }
  else if (p.y - ground > 0.3) pl.grounded = false;
  else { p.y = ground; pl.vy = Math.max(pl.vy, 0); pl.grounded = pl.vy <= 0 ? true : pl.grounded; }
  player.animate(dt, pl.speed, pl.grounded, pl.vy);

  // caméra 3e personne
  if (tp.pitchT !== null) { tp.pitch += (tp.pitchT - tp.pitch) * Math.min(1, dt * 2.5); if (Math.abs(tp.pitch - tp.pitchT) < 0.01) tp.pitchT = null; }
  if (tp.yawT !== null) { let d = tp.yawT - tp.yaw; d = Math.atan2(Math.sin(d), Math.cos(d)); tp.yaw += d * Math.min(1, dt * 2.5); if (Math.abs(d) < 0.01) tp.yawT = null; }
  // si l'on bouge, la caméra se replace doucement derrière l'habitant
  if (ml > 0.01 && input.dragging !== true && jy > 0.3) {
    let d = (player.root.rotation.y + Math.PI) - tp.yaw; d = Math.atan2(Math.sin(d), Math.cos(d));
    tp.yaw += d * Math.min(1, dt * 0.8);
  }
  const pose = tpPose();
  camera.position.lerp(pose.pos, Math.min(1, dt * 14));
  camera.lookAt(pose.look);
  camera.fov += (60 + (run && pl.speed > 4 ? 6 : 0) - camera.fov) * Math.min(1, dt * 4);
  camera.updateProjectionMatrix();

  // villageois proches : bouton Parler
  let near = null, nd = 2.8;
  for (const v of villagers.list) { const d = v.root.position.distanceTo(p); if (d < nd) { nd = d; near = v; } }
  talkTarget = near;
  if (near) { btnTalk.hidden = false; btnTalk.textContent = 'Parler'; } else if (actionPickup) { btnTalk.hidden = false; btnTalk.textContent = 'Ramasser'; } else btnTalk.hidden = true;
  if (!near && dialogNPC) closeDialog();
}

// ------------------------------------------------------------------ UI
const $ = id => document.getElementById(id);
const btnTalk = $('btnTalk');
let talkTarget = null, dialogNPC = null, dialogIdx = 0, dialogLines = [];
function setUI() {
  const playing = mode === 'play';
  $('godUI').hidden = mode !== 'god';
  $('playUI').hidden = !playing;
  barsEl.classList.toggle('on', mode === 'toPlay' || mode === 'toGod');
  const chip = $('modeChip');
  chip.textContent = mode === 'god' || mode === 'toGod' ? 'Mode Créateur' : 'Mode Habitant';
  chip.classList.toggle('inside', mode === 'play' || mode === 'toPlay');
}
function closeDialog() { if (dialogNPC) dialogNPC.talking = false; dialogNPC = null; $('dialog').hidden = true; }
function talk() {
  if (!talkTarget) { if (actionPickup) { game.collect(actionPickup); actionPickup = null; } return; }
  if (dialogNPC !== talkTarget) { closeDialog(); dialogNPC = talkTarget; dialogIdx = 0; dialogLines = game.linesFor(dialogNPC) || dialogNPC.lines; }
  else dialogIdx++;
  if (dialogIdx >= dialogLines.length) { const n = dialogNPC; closeDialog(); game.dialogEnd(n); return; }
  dialogNPC.talking = true; dialogNPC.target = null;
  const p = player.root.position, q = dialogNPC.root.position;
  dialogNPC.root.rotation.y = Math.atan2(p.x - q.x, p.z - q.z);
  $('dlgName').textContent = dialogNPC.name; $('dlgText').textContent = dialogLines[dialogIdx];
  $('dialog').hidden = false;
}
function syncTimeUI() {
  const h = Math.floor(timeOfDay), m = Math.floor((timeOfDay - h) * 60);
  const lbl = `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
  const el = $('timeLabel'); if (el.textContent !== lbl) el.textContent = (timeOfDay > 6 && timeOfDay < 18 ? '☀ ' : '☾ ') + lbl;
  if (document.activeElement !== $('timeRange')) $('timeRange').value = timeOfDay;
}
function bindUI() {
  $('btnEnter').onclick = startEnter;
  $('btnExit').onclick = startExit;
  $('btnSky').onclick = () => {
    if (tp.pitch > -0.4) { // se tourner vers le Créateur et lever les yeux
      const c = creatorPos(), p = player.root.position;
      tp.yawT = Math.atan2(-(c.x - p.x), -(c.z - p.z)); tp.pitchT = -0.92;
    } else { tp.pitchT = 0.16; tp.yawT = null; }
  };
  $('btnRun').onclick = () => { pl.run = !pl.run; $('btnRun').classList.toggle('on', pl.run); };
  $('btnJump').addEventListener('pointerdown', e => { e.preventDefault(); input.jump = true; });
  btnTalk.onclick = talk;
  $('timeRange').addEventListener('input', e => { timeOfDay = parseFloat(e.target.value); });
  const speeds = [0, 0.04, 1.2];
  ['spd0', 'spd1', 'spd2'].forEach((id, i) => $(id).onclick = () => { timeSpeed = speeds[i]; ['spd0', 'spd1', 'spd2'].forEach(j => $(j).classList.toggle('on', j === id)); });
  $('btnHouse').onclick = () => { if (game.spendFoi(15)) addHouseAt(entry.x, entry.z); };
  $('btnGodFoudre').onclick = () => setGodPower('foudre');
  $('btnGodBenir').onclick = () => setGodPower('benir');
  $('btnOnde').addEventListener('pointerdown', e => { e.preventDefault(); game.onde(); });
  $('btnElan').addEventListener('pointerdown', e => { e.preventDefault(); if (game.elan()) pl.dash = 0.25; });
  $('btnFoudre').addEventListener('pointerdown', e => { e.preventDefault(); game.foudreDivine(); });
  ui.refreshFoi(); ui.refreshPowers();
  $('btnTree').onclick = () => { if (!game.spendFoi(5)) return; for (let i = 0; i < 5; i++) { const a = Math.random() * 6.28, r = 3 + Math.random() * 7; const x = entry.x + Math.cos(a) * r, z = entry.z + Math.sin(a) * r; plantTree(Math.random() > 0.4 ? 'broad' : 'pine', x, z, 0.8 + Math.random() * 0.5); } };
  $('btnAttack').addEventListener('pointerdown', e => { e.preventDefault(); game.attack(); });
  $('btnRain').onclick = () => { game.setWeather(game.state.weather === 'rain' ? 'clear' : 'rain'); $('btnRain').classList.toggle('on', game.state.weather === 'rain'); };
  $('btnRain').classList.toggle('on', game.state.weather === 'rain');
  $('btnJournal').onclick = () => { ui.refreshQuests(); $('journal').hidden = !$('journal').hidden; $('bag').hidden = true; };
  $('btnBag').onclick = () => { ui.refreshInventory(); $('bag').hidden = !$('bag').hidden; $('journal').hidden = true; };
  document.querySelectorAll('[data-close]').forEach(b => b.onclick = () => { b.closest('.sheet').hidden = true; });
  ui.refreshLife();
  $('perf').onclick = () => { showPerf = !showPerf; };

  // --- joystick ---
  const stick = $('stick'), knob = $('knob');
  let sid = null, sc = null;
  stick.addEventListener('pointerdown', e => { sid = e.pointerId; stick.setPointerCapture(sid); const r = stick.getBoundingClientRect(); sc = { x: r.left + r.width / 2, y: r.top + r.height / 2, r: r.width / 2 }; moveStick(e); });
  const moveStick = e => {
    if (e.pointerId !== sid) return;
    let dx = e.clientX - sc.x, dy = e.clientY - sc.y; const d = Math.hypot(dx, dy), m = sc.r * 0.8;
    if (d > m) { dx *= m / d; dy *= m / d; }
    knob.style.transform = `translate(${dx}px, ${dy}px)`;
    input.jx = dx / m; input.jy = -dy / m;
  };
  stick.addEventListener('pointermove', moveStick);
  const endStick = e => { if (e.pointerId !== sid) return; sid = null; input.jx = input.jy = 0; knob.style.transform = ''; };
  stick.addEventListener('pointerup', endStick); stick.addEventListener('pointercancel', endStick);

  // --- caméra tactile (glisser / pincer) sur le canvas ---
  const ptrs = new Map(); let pinch = 0, downAt = null;
  canvas.addEventListener('pointerdown', e => { canvas.setPointerCapture(e.pointerId); ptrs.set(e.pointerId, { x: e.clientX, y: e.clientY }); downAt = { x: e.clientX, y: e.clientY, t: performance.now() }; god.idle = 0; input.dragging = true; });
  canvas.addEventListener('pointermove', e => {
    const prev = ptrs.get(e.pointerId); if (!prev) return;
    const dx = e.clientX - prev.x, dy = e.clientY - prev.y;
    ptrs.set(e.pointerId, { x: e.clientX, y: e.clientY });
    if (ptrs.size === 2) {
      const [a, b] = [...ptrs.values()]; const d = Math.hypot(a.x - b.x, a.y - b.y);
      if (pinch) zoom((pinch - d) * 0.004); pinch = d; return;
    }
    if (mode === 'god') { god.theta -= dx * 0.005; god.phi = clamp(god.phi + dy * 0.004, 0.06, 1.45); god.vTheta = -dx * 0.3; god.vPhi = 0; god.idle = 0; }
    else if (mode === 'play') { tp.yaw -= dx * 0.006; tp.pitch = clamp(tp.pitch + dy * 0.005, -1.25, 1.15); tp.pitchT = null; tp.yawT = null; }
  });
  const up = e => {
    ptrs.delete(e.pointerId); if (ptrs.size < 2) pinch = 0; input.dragging = ptrs.size > 0;
    if (downAt && mode === 'god' && Math.hypot(e.clientX - downAt.x, e.clientY - downAt.y) < 8 && performance.now() - downAt.t < 350) pick(e.clientX, e.clientY);
    downAt = null;
  };
  canvas.addEventListener('pointerup', up); canvas.addEventListener('pointercancel', up);
  canvas.addEventListener('wheel', e => { e.preventDefault(); zoom(e.deltaY * 0.0012); }, { passive: false });
  addEventListener('keydown', e => {
    input.keys.add(e.code);
    if (e.code === 'Enter') mode === 'god' ? startEnter() : startExit();
    if (e.code === 'Escape') startExit();
    if (e.code === 'KeyE') talk();
    if (e.code === 'KeyF') game.attack();
    if (e.code === 'KeyR') game.onde();
    if (e.code === 'KeyX' && game.elan()) pl.dash = 0.25;
    if (e.code === 'KeyG') game.foudreDivine();
    if (e.code === 'KeyJ') $('btnJournal').click();
    if (e.code === 'KeyI') $('btnBag').click();
    if (e.code === 'KeyC') $('btnSky').click();
  });
  addEventListener('keyup', e => input.keys.delete(e.code));
}
function zoom(d) {
  if (mode === 'god') god.dist = clamp(god.dist * (1 + d), 150, 700);
  else if (mode === 'play') tp.dist = clamp(tp.dist * (1 + d), 2.5, 12);
}
const ray = new THREE.Raycaster();
function pick(x, y) {
  ray.setFromCamera(new THREE.Vector2(x / innerWidth * 2 - 1, -(y / innerHeight) * 2 + 1), camera);
  const hit = ray.intersectObject(terrain.mesh)[0];
  if (hit && Math.hypot(hit.point.x, hit.point.z) < WORLD_R - 6) {
    if (godPower) { if (game.godCast(godPower, hit.point.clone())) { setGodPower(null); } return; }
    entry.set(hit.point.x, 0, hit.point.z);
    $('hint').textContent = 'Point d\'entrée choisi · touchez « Entrer dans le monde »';
  }
}
let houseSeed = 100;
let godPower = null;
function setGodPower(p) {
  godPower = godPower === p ? null : p;
  ['Foudre', 'Benir'].forEach(n => $('btnGod' + n).classList.toggle('on', godPower === n.toLowerCase()));
  $('hint').textContent = godPower === 'foudre' ? 'Touchez le sol : la foudre frappe (anéantit les Ombrelins, blesse le Cœur d\'Ombre)'
    : godPower === 'benir' ? 'Touchez le sol : bénédiction (soigne l\'habitant, paralyse les Ombres)'
    : 'Glissez pour tourner · pincez pour zoomer · touchez le sol pour choisir où entrer';
}
function plantTree(kind, x, z, s, record = true) {
  trees.userData.addTree(kind, x, z, s);
  circles.length = 0; circles.push(...trees.userData.colliders, ...rocks.userData.colliders, ...bushes.userData.colliders);
  if (record) { game.state.builds.push({ type: 'tree', kind, x, z, s }); game.save(); }
}
const ui = {
  toast(msg, kind = 'item') { const t = document.createElement('div'); t.className = 'toast ' + kind; t.textContent = msg; $('toasts').appendChild(t); setTimeout(() => t.remove(), 3300); },
  refreshLife() { const l = $('life'); if (!l || !game) return; l.innerHTML = ''; for (let i = 0; i < game.state.maxHp; i++) { const s = document.createElement('i'); if (i >= game.state.hp) s.className = 'off'; l.appendChild(s); } },
  hurtFlash() { const h = $('hurt'); h.classList.add('on'); setTimeout(() => h.classList.remove('on'), 60); },
  setAction(label, pk) { actionPickup = pk; },
  refreshFoi() { if (!game) return; const f = Math.floor(game.state.foi); document.querySelectorAll('.foi i').forEach(i => i.style.width = f + '%'); document.querySelectorAll('.foi b').forEach(b => b.textContent = f); },
  refreshPowers() { if (!game) return; $('btnOnde').classList.toggle('locked', !game.state.onde); },
  cooldowns(c) { for (const [k, v] of Object.entries(c)) { const el = $('btn' + k[0].toUpperCase() + k.slice(1)); if (el) el.style.setProperty('--cd', (v * 100).toFixed(1) + '%'); } },
  actCard(kicker, title, text) { const c = $('actCard'); c.querySelector('small').textContent = kicker; c.querySelector('h2').textContent = title; c.querySelector('p').textContent = text; c.classList.remove('on'); void c.offsetWidth; c.classList.add('on'); },
  bossBar(v) { const b = $('bossBar'); b.hidden = v === null; if (v !== null) b.querySelector('i').style.width = Math.max(0, v * 100) + '%'; },
  story(lines, { title = '', final = null, onDone = () => {} } = {}) {
    const box = $('story'); let i = 0;
    const show = () => { box.querySelector('h2').textContent = title; box.querySelector('p').textContent = lines[i]; box.querySelector('.dots').textContent = lines.map((_, k) => k === i ? '●' : '○').join(' '); $('storyNext').textContent = i < lines.length - 1 ? 'Suivant' : (final || 'Commencer'); };
    $('storyNext').onclick = () => { i++; if (i >= lines.length) { box.hidden = true; onDone(); } else show(); };
    $('storySkip').onclick = () => { box.hidden = true; onDone(); };
    box.hidden = false; show();
  },
  ending(lines, stats) {
    ui.story([...lines, `Ombrelins chassés : ${stats.kills} · Foi : ${stats.foi}. Merci d'avoir joué. Le monde reste à vous : construisez, explorez, protégez.`], { title: 'ÉPILOGUE', final: 'Continuer à jouer' });
  },
  refreshQuests() {
    if (!game) return;
    $('questList').innerHTML = game.quests().map(q => {
      const st = q.state; const done = st && st.stage >= 2;
      const status = !st ? `Parlez à ${q.giver}` : done ? 'Accomplie' : q.steps[0];
      return `<div class="quest ${done ? 'done' : ''}"><b><span class="tag">${q.kind === 'main' ? 'Principale' : done ? 'Terminée' : 'Secondaire'}</span>${q.title}</b><span>${status}</span></div>`;
    }).join('') + (game.state.act >= 2 ? `<div class="quest ${game.state.won ? 'done' : ''}"><b><span class="tag">Acte II</span>Le Cœur d'Ombre</b><span>${game.state.won ? 'Vaincu — Aube est sauvée' : 'Détruire le Cœur d\'Ombre dans la Faille, au nord-ouest'}</span></div>` : '')
      + `<h4>Chroniques d'Aube</h4>` + game.lore.chronicle.map(([t, d]) => `<div class="quest lore"><b>${t}</b><span>${d}</span></div>`).join('');
    $('objective').textContent = game.objective();
  },
  refreshInventory() {
    if (!game) return;
    const inv = game.state.inv, keys = Object.keys(inv);
    $('invList').innerHTML = keys.length ? keys.map(k => `<button class="slot" data-item="${k}"><i>${ITEMS[k].icon}</i>${ITEMS[k].name}<br><em>×${inv[k]}</em></button>`).join('') : '<p class="note">Votre sac est vide. Explorez le monde !</p>';
    $('invList').querySelectorAll('[data-item]').forEach(b => b.onclick = () => { if (game.useItem(b.dataset.item)) ui.toast('Vous vous sentez mieux', 'done'); });
  },
};
function addHouseAt(x, z, record = true) {
  if (record) { game.state.builds.push({ type: 'house', x, z }); game.save(); }
  const rotY = Math.atan2(-x, -z);
  const ox = x + Math.sin(rotY) * 7, oz = z + Math.cos(rotY) * 7; // la maison se pose à côté de la balise
  const h = buildHouse(M, terrain, { x: ox, z: oz, rotY: rotY + Math.PI, w: 7, d: 6, H: 3.6, roofH: 2.7, roof: Math.random() > 0.5 ? 'roof' : 'slate', shutter: '#c0604a', seed: houseSeed++ });
  h.group.scale.setScalar(0.01);
  scene.add(h.group); colliders.push(...h.colliders); floors.push(h.floor);
  grass.userData.clearArea(ox, oz, 4.2, 3.7, rotY + Math.PI);
  grassField.userData.clearRect(ox, oz, 4.2, 3.7, rotY + Math.PI);
  // petite animation d'apparition "divine"
  const t0 = performance.now();
  const grow = () => { const k = Math.min(1, (performance.now() - t0) / 900); h.group.scale.setScalar(0.01 + 0.99 * (1 - Math.pow(1 - k, 3))); if (k < 1) requestAnimationFrame(grow); };
  grow();
  return h;
}

let showPerf = true;
function frame() {
  requestAnimationFrame(frame);
  const dt = Math.min(0.05, clock.getDelta());
  update(dt);
  composer.render();
  // performances + résolution dynamique (vise ≥ 40 i/s sur téléphone)
  fpsAcc += dt; fpsN++; perfT += dt;
  if (perfT > 1) {
    fps = fpsN / fpsAcc; fpsAcc = 0; fpsN = 0; perfT = 0;
    if (!params.has('fixed') && isMobile) {
      if (fps < 36 && dynPR > 0.7) dynPR = Math.max(0.7, dynPR - 0.15);
      else if (fps > 55 && dynPR < Q.pr) dynPR = Math.min(Q.pr, dynPR + 0.1);
      if (renderer.getPixelRatio() !== dynPR) { renderer.setPixelRatio(dynPR); composer.setPixelRatio(dynPR); composer.setSize(innerWidth, innerHeight); }
    }
    const info = renderer.info.render;
    perfEl.innerHTML = showPerf ? `<b>${fps.toFixed(0)}</b> i/s · ${QUALITY}<br>${info.calls} appels · ${(info.triangles / 1e6).toFixed(2)} M tri<br>rés. ×${renderer.getPixelRatio().toFixed(2)}` : '<b>' + fps.toFixed(0) + '</b>';
  }
}
addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight, false); composer.setSize(innerWidth, innerHeight);
  bloom.resolution.set(innerWidth / 2, innerHeight / 2);
});

// API de pilotage (captures automatisées / tests)
window.GD = {
  get mode() { return mode; }, god, tp, player: () => player, entry,
  setTime: h => { timeOfDay = h; timeSpeed = 0; },
  enter: startEnter, exit: startExit, addHouseAt, placePlayer,
  sky: () => document.getElementById('btnSky').click(),
  step: (n = 1, dt = 1 / 30) => { for (let i = 0; i < n; i++) { update(dt); } composer.render(); },
  render: () => composer.render(),
  hideUI: (h = true) => { $('hud').style.display = h ? 'none' : ''; },
  setEntry: (x, z) => entry.set(x, 0, z),
  look: (yaw, pitch) => { tp.yaw = yaw; tp.pitch = pitch; tp.pitchT = null; },
  info: () => ({ ...renderer.info.render, fps }),
  renderer: () => renderer, scene: () => scene, dome: () => dome, creator: () => creator, story: (l) => ui.story(l), actCard: (...a) => ui.actCard(...a), godPower: (p) => setGodPower(p), cast: (p, x, z) => game.godCast(p, new THREE.Vector3(x, world.groundAt(x, z), z)), onde: () => game.onde(), elan: () => { if (game.elan()) pl.dash = 0.25; }, foudre: () => game.foudreDivine(), game: () => game, villagers: () => villagers.list, enemies: () => game._enemies, attack: () => game.attack(), talk: () => talk(), rain: (on = true) => game.setWeather(on ? 'rain' : 'clear'),
  move: (jx, jy) => { input.jx = jx; input.jy = jy; },
};

(async () => {
  await buildWorld();
  game = createGame({ scene, terrain, world, player, villagers, T, solve, ui, camera, respawn: () => placePlayer(VILLAGE.x + 3, VILLAGE.y + 4),
    flash: (k) => { flashT = Math.max(flashT, k); },
    onVictory: () => { dome.uniforms.uRipplePos.value.set(0, DOME_R, 0); rippleT = 0.0001; } });
  const ext = await loadExternalModels({ scene, terrain, world, player, villagers: villagers, onProgress: setMsg });
  if (ext.length) console.info('Modèles 3D chargés :', ext.join(', '));
  for (const b of game.state.builds) { if (b.type === 'house') addHouseAt(b.x, b.z, false); if (b.type === 'tree') plantTree(b.kind, b.x, b.z, b.s, false); }
  bindUI();
  placePlayer(entry.x, entry.z);
  updateSky();
  setUI();
  const ld = $('loading'); ld.style.opacity = 0; setTimeout(() => ld.remove(), 900);
  clock.start();
  window.GD.ready = true;
  // prologue au premier lancement
  if (!game.state.seenIntro && !params.has('manual')) {
    setTimeout(() => ui.story([...game.lore.prologue, game.lore.goal], { title: 'LA VOÛTE D\'AUBE', onDone: () => { game.state.seenIntro = true; game.save(); ui.actCard('ACTE I', 'Les Éclats de la Voûte', 'Entrez dans votre monde et parlez au Vieil Oswin, près du puits.'); } }), 900);
  }
  if (!params.has('manual')) frame();
})();
