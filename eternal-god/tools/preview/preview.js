// Image d'aperçu rendue avec les vrais modules du jeu (terrain, ciel, végétation, villages, habitants, dieu).
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
const GAME = '../../app/src/main/assets/game/';
const base = GAME + 'js/';
// les modules chargent leurs ressources relativement à la page du jeu
const _fetch = window.fetch;
window.fetch = (u, ...a) => _fetch(typeof u === 'string' && !/^(https?:|blob:|data:|\.\.\/)/.test(u) ? GAME + u : u, ...a);
THREE.DefaultLoadingManager.setURLModifier((u) => (/^(img|models)\//.test(u) ? GAME + u : u));
const { G, settings, mulberry32 } = await import(base + 'core.js');
const { Terrain } = await import(base + 'terrain.js');
const { Environment } = await import(base + 'env.js');
const { Vegetation, loadVegetationAssets } = await import(base + 'vegetation.js');
const { buildGodModel } = await import(base + 'godmodel.js');
const { loadTextureSet, TERRAIN_LAYERS, BUILDING_LAYERS } = await import(base + 'textures.js');
const { PhotoSky } = await import(base + 'sky.js');
const { Villages, loadProps } = await import(base + 'village.js');
const { People, loadPeople } = await import(base + 'people.js');
const { Roads } = await import(base + 'roads.js');
const { Fauna, loadFauna } = await import(base + 'fauna.js');

const P = new URLSearchParams(location.search);
const num = (k, d) => (P.has(k) ? +P.get(k) : d);
settings.quality = 'high';
const W = innerWidth, H = innerHeight;
const renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
renderer.setSize(W, H); renderer.setPixelRatio(1);
renderer.toneMapping = THREE.AgXToneMapping;
renderer.shadowMap.enabled = true; renderer.shadowMap.type = THREE.PCFSoftShadowMap;
document.body.appendChild(renderer.domElement);
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(num('fov', 50), W / H, 0.3, 6000);
G.scene = scene; G.camera = camera; G.renderer = renderer;
G.uniforms = { time: { value: 12 }, gust: { value: new THREE.Vector4(0, 0, 0, 0) }, press: { value: new THREE.Vector4(0, 0, 0, 0) } };

const terrTex = await loadTextureSet(TERRAIN_LAYERS, GAME + 'img/tex', 1024);
const bTex = await loadTextureSet(BUILDING_LAYERS, GAME + 'img/tex', 1024);
G.photoSky = new PhotoSky(); await G.photoSky.load(renderer);
const vegAssets = await loadVegetationAssets();
const props = await loadProps();
const peopleAssets = await loadPeople();
const faunaAssets = await loadFauna();

const T = new Terrain(scene); G.terrain = T; T.textures = terrTex;
T.generate();
const villages = new Villages(scene, props, bTex); G.villages = villages;
villages.prepare();
villages.build();
const roads = new Roads(scene, villages); G.roads = roads;
console.time('routes'); roads.build(); console.timeEnd('routes');
console.log('FIELDS', JSON.stringify(villages.villages.map(v => [v.name, Math.round(v.x), Math.round(v.z), (v.fields||[]).map(f => [Math.round(f.x), Math.round(f.z)])])));
console.log('ROUTES', JSON.stringify(roads.routes.map(r => [r.from.name, r.to.name, r.pts.length, r.pts.filter((_, i) => i % 15 === 0).map(p => [Math.round(p.x), Math.round(p.z)])])));
const crops = villages.dressFarms(vegAssets.grass);
villages.finalize();
T.build(scene);
const veg = new Vegetation(scene, vegAssets); G.veg = veg;
veg.build((x, z) => !villages.isFree(x, z, 2));
veg.extraFields = crops;
const env = new Environment(scene, renderer); G.env = env;
env.dayTime = num('t', 0.63);
env.build();

// ----- caméra -----
const vill = villages.villages[num('vil', 0)];
camera.position.set(vill.x + num('camx', 40), 0, vill.z + num('camz', 40));
camera.position.y = T.heightAt(camera.position.x, camera.position.z) + num('camy', 26);
const fwdT = new THREE.Vector3(vill.x + num('fx', -60), num('fy', 30), vill.z + num('fz', -330));
camera.lookAt(fwdT);
const fwd = fwdT.clone().sub(camera.position).normalize();
const right = new THREE.Vector3().crossVectors(fwd, new THREE.Vector3(0, 1, 0)).normalize();

// ----- le dieu -----
const god = buildGodModel();
const gp = camera.position.clone().addScaledVector(fwd, num('gf', 13)).addScaledVector(right, num('gr', 4)).add(new THREE.Vector3(0, num('gu', -2.5), 0));
if (P.has('gx')) { gp.set(vill.x + num('gx', 0), 0, vill.z + num('gz', 0)); gp.y = T.heightAt(gp.x, gp.z) + num('gy', 0); }
god.root.position.copy(gp);
god.root.rotation.y = Math.atan2(camera.position.x - gp.x, camera.position.z - gp.z) + num('ry', 0);
scene.add(god.root);
const Bn = god.bones;
const landed = num('gy', 99) < 1;
Bn.spine.rotation.x = 0.06;
Bn.upperL.rotation.set(0.25, 0, 0.45); Bn.foreL.rotation.set(-0.5, 0, 0);
if (!landed) { Bn.thighL.rotation.x = -0.15; Bn.shinL.rotation.x = 0.35; Bn.thighR.rotation.x = 0.15; Bn.shinR.rotation.x = 0.45; Bn.footL.rotation.x = 0.5; Bn.footR.rotation.x = 0.55; }
Bn.upperR.rotation.set(landed ? 0.3 : 0.25, 0, landed ? -0.5 : -0.45); Bn.foreR.rotation.set(-0.5, 0, 0);
god.updateWings(num('spread', 1), num('flap', 0.35), 3, 0.3);
scene.updateMatrixWorld(true);
G.god = { pos: gp.clone(), vel: new THREE.Vector3() };
let lookAt = fwdT;
if (P.has('ca')) { // caméra en orbite autour du dieu, hors des bâtiments
  const a = num('ca', 0) * Math.PI / 180, d = num('cd', 12);
  camera.position.set(gp.x + Math.sin(a) * d, 0, gp.z + Math.cos(a) * d);
  for (let i = 0; i < 4; i++) villages.pushOut(camera.position, 1.2);
  camera.position.y = T.heightAt(camera.position.x, camera.position.z) + num('ch', 2.2);
  lookAt = gp.clone().add(new THREE.Vector3(0, num('ly', 3), 0));
  god.root.rotation.y = Math.atan2(camera.position.x - gp.x, camera.position.z - gp.z) + num('ry', 0);
  camera.lookAt(lookAt);
}

// ----- habitants : une minute de vie simulée -----
const people = new People(scene, peopleAssets, villages); G.people = people;
people.populate();
people.addTravelers(roads, num('travelers', 8));
const fauna = new Fauna(scene, faunaAssets); G.fauna = fauna; fauna.populate();
if (P.has('worship')) for (const v of villages.villages) v.worship = num('worship', 0.15);
const sim = num('sim', 60);
G.god.pos.y += 500; // invisible pendant la simulation de routine
for (let t = 0; t < sim; t += 0.1) { env.dayTime = num('t', 0.63); people.update(0.1); fauna.update(0.1); }
G.god.pos.copy(gp);
for (let t = 0; t < num('react', 4); t += 0.1) people.update(0.1);
{ const h = {}; for (const p of people.list) if (p.pos.distanceTo(gp) < 42) { const k = p.state + ':' + p.role + ':' + p.child; h[k] = (h[k] || 0) + 1; } console.log('STATES', JSON.stringify(h), 'pop', people.list.length, 'gp', gp.toArray().map(v => v.toFixed(1)).join(','), 'ground', T.groundAt(gp.x, gp.z).toFixed(1)); }

// ----- rayon divin (facultatif) -----
const glowTex = new THREE.CanvasTexture((() => { const c = document.createElement('canvas'); c.width = c.height = 128; const x = c.getContext('2d'); const g = x.createRadialGradient(64, 64, 0, 64, 64, 64); g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.25, 'rgba(255,220,150,0.6)'); g.addColorStop(1, 'rgba(255,140,40,0)'); x.fillStyle = g; x.fillRect(0, 0, 128, 128); return c; })());
const spr = (pos, s, c = 0xffffff) => { const m = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: c, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true })); m.position.copy(pos); m.scale.setScalar(s); scene.add(m); return m; };
if (P.get('beam') === '1') {
  const target = new THREE.Vector3(vill.x + num('tx', -80), 0, vill.z + num('tz', -190)); target.y = T.heightAt(target.x, target.z);
  for (let i = 0; i <= 20; i++) T.scorch(target.x - 25 + i * 2.5, target.z + i * 1.2, 4.5, 3.5, 0.6, 1.0);
  T.crater(target.x, target.z, 15, 8, { rim: 0.3, burn: 0.8, heat: 1 });
  const hand = new THREE.Vector3(); Bn.handR.getWorldPosition(hand);
  const sh = new THREE.Vector3(); Bn.upperR.getWorldPosition(sh);
  const dir = target.clone().sub(sh).normalize(); const pq = new THREE.Quaternion(); Bn.shoulderR.getWorldQuaternion(pq);
  Bn.upperR.quaternion.setFromUnitVectors(new THREE.Vector3(0, -1, 0), dir.applyQuaternion(pq.invert())); Bn.foreR.rotation.set(0, 0, 0);
  scene.updateMatrixWorld(true); Bn.handR.getWorldPosition(hand);
  const L = hand.distanceTo(target);
  const beamMat = (c, o) => new THREE.ShaderMaterial({ transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, uniforms: { uC: { value: new THREE.Color(c) }, uO: { value: o } },
    vertexShader: 'varying float vF; void main(){ vec3 n = normalize(normalMatrix*normal); vec4 mv = modelViewMatrix*vec4(position,1.0); vF = abs(dot(n, normalize(-mv.xyz))); gl_Position = projectionMatrix*mv; }',
    fragmentShader: 'uniform vec3 uC; uniform float uO; varying float vF; void main(){ gl_FragColor = vec4(uC * pow(vF, 2.5) * uO * 1.6, 1.0); }' });
  const beam = new THREE.Group();
  for (const [r0, r1, c, o] of [[0.5, 1.4, 0xfff4d0, 1], [1.1, 4.0, 0xffb040, 0.35], [0.9, 7.0, 0xff7a20, 0.12]]) beam.add(new THREE.Mesh(new THREE.CylinderGeometry(r1, r0, L, 16, 1, true).rotateX(Math.PI / 2).translate(0, 0, L / 2), beamMat(c, o)));
  beam.position.copy(hand); beam.lookAt(target); scene.add(beam);
  spr(target.clone().setY(target.y + 4), 70, 0xff9030); spr(target.clone().setY(target.y + 3), 26);
  const pl = new THREE.PointLight(0xff9a40, 4000, 160, 1.6); pl.position.copy(target).setY(target.y + 8); scene.add(pl);
}
// particules divines autour du dieu
const motes = new THREE.BufferGeometry(); const mp = [];
for (let i = 0; i < 140; i++) { const a = Math.random() * 6.28, r = 2 + Math.random() * 6; mp.push(gp.x + Math.cos(a) * r, gp.y + Math.random() * 7, gp.z + Math.sin(a) * r); }
motes.setAttribute('position', new THREE.Float32BufferAttribute(mp, 3));
scene.add(new THREE.Points(motes, new THREE.PointsMaterial({ color: 0xffdf90, size: 0.12, map: glowTex, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false })));

env.update(0, true);
const composer = new EffectComposer(renderer);
composer.addPass(new RenderPass(scene, camera));
composer.addPass(new UnrealBloomPass(new THREE.Vector2(W / 2, H / 2), num('bs', 0.35), 0.5, num('bt', 1.4)));
composer.addPass(new OutputPass());
let frames = 0;
function loop() {
  G.time += 0.016; G.uniforms.time.value += 0.016;
  T.update(0.016); veg.update(0.016); villages.update(0.016);
  env.dayTime = num('t', 0.63); env.update(0.0001);
  camera.lookAt(lookAt);
  composer.render();
  if (++frames === 6) { window.__ready = true; return; }
  requestAnimationFrame(loop);
}
loop();
