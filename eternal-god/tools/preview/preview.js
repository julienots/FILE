// Image d'aperçu rendue avec les vrais modules du jeu.
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
const base = '../../app/src/main/assets/game/js/';
const { G, settings, mulberry32 } = await import(base + 'core.js');
const { Terrain } = await import(base + 'terrain.js');
const { Environment } = await import(base + 'env.js');
const { Vegetation, loadVegetationAssets } = await import(base + 'vegetation.js');
const { loadTextureSet, TERRAIN_LAYERS, BUILDING_LAYERS } = await import(base + 'textures.js');
const { PhotoSky } = await import(base + 'sky.js');
const { buildGodModel } = await import(base + 'godmodel.js');
const { makeBuilding, buildingMaterial, windmillSails } = await import(base + 'buildings.js');

const P = new URLSearchParams(location.search);
settings.quality = 'high';
const W = innerWidth, H = innerHeight;
const renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
renderer.setSize(W, H); renderer.setPixelRatio(1);
renderer.toneMapping = THREE.AgXToneMapping;
renderer.shadowMap.enabled = true; renderer.shadowMap.type = THREE.PCFSoftShadowMap;
document.body.appendChild(renderer.domElement);
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(+(P.get('fov') || 50), W / H, 0.3, 6000);
G.scene = scene; G.camera = camera; G.renderer = renderer;
G.uniforms = { time: { value: 12 }, gust: { value: new THREE.Vector4(0, 0, 0, 0) }, press: { value: new THREE.Vector4(0, 0, 0, 0) } };

const TEXB = '../../app/src/main/assets/game/img/tex';
const terrTex = await loadTextureSet(TERRAIN_LAYERS, TEXB, 1024);
const bTex = await loadTextureSet(BUILDING_LAYERS, TEXB, 1024);
// les modules chargent leurs ressources relativement à la page du jeu
const _fetch = window.fetch; window.fetch = (u, ...a) => _fetch(typeof u === 'string' && !/^(https?:|blob:|data:|\.\.\/)/.test(u) ? '../../app/src/main/assets/game/' + u : u, ...a);
THREE.DefaultLoadingManager.setURLModifier((u) => (u.startsWith('img/') || u.startsWith('models/')) ? '../../app/src/main/assets/game/' + u : u);
G.photoSky = new PhotoSky(); await G.photoSky.load(renderer);
const vegAssets = await loadVegetationAssets();
const T = new Terrain(scene); G.terrain = T; T.textures = terrTex;
T.generate();
// village
const rnd = mulberry32(5);
const bm = buildingMaterial(bTex);
const placed = [];
function place(type, x, z, ry) {
  const b = makeBuilding(type, rnd);
  const y = T.heightAt(x, z);
  T.flatten(x, z, Math.max(b.w, b.d) * 0.6, y);
  const m = new THREE.Mesh(b.geo, bm); m.position.set(x, y, z); m.rotation.y = ry; m.castShadow = m.receiveShadow = true;
  scene.add(m); placed.push([x, z, Math.max(b.w, b.d)]);
  T.paintRect(x, z, b.w + 3, b.d + 3, ry, 1);
  return { m, b };
}
T.paintDisc(0, 0, 18, 3);
place('well', 0, 0, 0);
place('temple', 0, -52, 0);
const ring = [[-26, -14], [26, -16], [-30, 18], [30, 20], [-8, 34], [12, 36], [-46, -2], [46, 0], [-52, 34], [50, 40], [-24, 58], [24, 62], [-62, -34], [60, -36], [-36, -40], [38, -44]];
ring.forEach(([x, z], i) => place(i === 1 ? 'tavern' : i === 2 ? 'forge' : 'house', x, z, Math.atan2(-x, -z) + Math.PI));
place('stall', -9, -12, 0.3); place('stall', 9, -12, -0.3); place('stall', -12, 8, 1.8); place('stall', 12, 9, -1.8);
place('tower', -80, -70, 0); place('tower', 82, 70, 0); place('stable', -78, 30, 1.6);
const mill = place('windmill', 95, -20, -0.6);
const sails = new THREE.Mesh(windmillSails(), bm); sails.position.set(0, 8.6, 3.4); sails.rotation.z = 0.4; mill.m.add(sails);
for (const [x, z] of [[-110, 70], [-120, 100], [-90, 110]]) { T.paintRect(x, z, 34, 22, 0.2, 2); }
place('farmhouse', -140, 70, 0.2);
for (let i = 0; i < 10; i++) { const a = i / 10 * 6.28; T.paintPath(0, 0, Math.cos(a) * 140, Math.sin(a) * 140, 4); }

// rayon divin : tranchée incandescente
const A = new THREE.Vector3(+(P.get('ax')||60), 0, +(P.get('az')||-150)), Bv = new THREE.Vector3(+(P.get('bx')||120), 0, +(P.get('bz')||-90));
for (let i = 0; i <= 30; i++) { const p = A.clone().lerp(Bv, i / 30); T.scorch(p.x, p.z, 4.5, 3.5, 0.6, 1.0); }
T.crater(Bv.x, Bv.z, 16, 9, { rim: 0.3, burn: 0.8, heat: 1 });
T.build(scene);

const veg = new Vegetation(scene, vegAssets); G.veg = veg;
const env = new Environment(scene, renderer); G.env = env;
const god = buildGodModel();
const num = (k, d) => (P.has(k) ? +P.get(k) : d);
camera.position.set(num('camx', 60), 0, num('camz', 10));
camera.position.y = T.heightAt(camera.position.x, camera.position.z) + num('camy', 30);
const fwdT = new THREE.Vector3(num('fx', -60), num('fy', 25), num('fz', -330));
camera.lookAt(fwdT);
const fwd = fwdT.clone().sub(camera.position).normalize();
const right = new THREE.Vector3().crossVectors(fwd, new THREE.Vector3(0, 1, 0)).normalize();
const gp = camera.position.clone().addScaledVector(fwd, num('gf', 13)).addScaledVector(right, num('gr', 4)).add(new THREE.Vector3(0, num('gu', -2.5), 0));
veg.build((x, z) => placed.some(([px, pz, r]) => Math.hypot(x - px, z - pz) < r));
env.dayTime = +(P.get('t') || 0.725);
env.build();
god.root.position.copy(gp); god.root.rotation.y = Math.atan2(camera.position.x - gp.x, camera.position.z - gp.z) + num('ry', 0);
god.root.scale.setScalar(1.0);
scene.add(god.root);
// pose : vol stationnaire, bras droit tendu vers la cible
const Bn = god.bones;
Bn.spine.rotation.x = 0.08; Bn.chest.rotation.y = -0.15;
Bn.upperR.rotation.set(-1.25, 0, -0.25); Bn.foreR.rotation.set(-0.2, 0, 0);
Bn.upperL.rotation.set(0.25, 0, 0.45); Bn.foreL.rotation.set(-0.5, 0, 0);
Bn.thighL.rotation.x = -0.15; Bn.shinL.rotation.x = 0.35; Bn.thighR.rotation.x = 0.15; Bn.shinR.rotation.x = 0.45;
Bn.footL.rotation.x = 0.5; Bn.footR.rotation.x = 0.55; Bn.head.rotation.x = 0.12; Bn.head.rotation.y = -0.1;
god.updateWings(+(P.get('spread') || 1), +(P.get('flap') || 0.35), 3, 0.3);
scene.updateMatrixWorld(true);

// le faisceau
const hand = new THREE.Vector3(); Bn.handR.getWorldPosition(hand);
const target = A.clone().lerp(Bv, 0.62); target.y = T.heightAt(target.x, target.z);
{ // bras droit pointé vers la cible
  const sh = new THREE.Vector3(); Bn.upperR.getWorldPosition(sh);
  const dir = target.clone().sub(sh).normalize();
  const pq = new THREE.Quaternion(); Bn.shoulderR.getWorldQuaternion(pq);
  dir.applyQuaternion(pq.invert());
  Bn.upperR.quaternion.setFromUnitVectors(new THREE.Vector3(0, -1, 0), dir);
  Bn.foreR.rotation.set(0, 0, 0); Bn.handR.rotation.set(0.3, 0, 0);
  scene.updateMatrixWorld(true); Bn.handR.getWorldPosition(hand);
  const hd = target.clone().sub(hand).normalize(); hand.addScaledVector(hd, 0.25);
}
const beamLen = hand.distanceTo(target);
const beamMat = (c, o) => new THREE.ShaderMaterial({ transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide,
  uniforms: { uC: { value: new THREE.Color(c) }, uO: { value: o } },
  vertexShader: 'varying float vF; void main(){ vec3 n = normalize(normalMatrix*normal); vec4 mv = modelViewMatrix*vec4(position,1.0); vF = abs(dot(n, normalize(-mv.xyz))); gl_Position = projectionMatrix*mv; }',
  fragmentShader: 'uniform vec3 uC; uniform float uO; varying float vF; void main(){ gl_FragColor = vec4(uC * pow(vF, 2.5) * uO * 1.6, 1.0); }' });
const beam = new THREE.Group();
beam.add(new THREE.Mesh(new THREE.CylinderGeometry(1.5, 0.5, beamLen, 12, 1, true).rotateX(Math.PI / 2).translate(0, 0, beamLen / 2), beamMat(0xfff4d0, 1)));
beam.add(new THREE.Mesh(new THREE.CylinderGeometry(4.5, 1.1, beamLen, 16, 1, true).rotateX(Math.PI / 2).translate(0, 0, beamLen / 2), beamMat(0xffb040, 0.35)));
beam.add(new THREE.Mesh(new THREE.CylinderGeometry(7.0, 0.9, beamLen, 16, 1, true).rotateX(Math.PI / 2).translate(0, 0, beamLen / 2), beamMat(0xff7a20, 0.12)));
beam.position.copy(hand); beam.lookAt(target); if (P.get('beam') !== '0') scene.add(beam);
const glowTex = new THREE.CanvasTexture((() => { const c = document.createElement('canvas'); c.width = c.height = 128; const x = c.getContext('2d'); const g = x.createRadialGradient(64, 64, 0, 64, 64, 64); g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.25, 'rgba(255,220,150,0.6)'); g.addColorStop(1, 'rgba(255,140,40,0)'); x.fillStyle = g; x.fillRect(0, 0, 128, 128); return c; })());
const spr = (pos, s, c = 0xffffff) => { const m = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: c, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true })); m.position.copy(pos); m.scale.setScalar(s); scene.add(m); return m; };
spr(hand, 1.6); spr(target.clone().setY(target.y + 4), 75, 0xff9030); spr(target.clone().setY(target.y + 3), 30); spr(target.clone().setY(target.y + 10), 120, 0xff6a20).material.opacity = 0.5;
// onde de choc au sol
const ringMat = new THREE.ShaderMaterial({ transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide,
  vertexShader: 'varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix*modelViewMatrix*vec4(position,1.0); }',
  fragmentShader: 'varying vec2 vUv; void main(){ float r = length(vUv-0.5)*2.0; float a = smoothstep(0.7,0.93,r)*smoothstep(1.0,0.95,r) + smoothstep(0.5,0.0,r)*0.25; gl_FragColor = vec4(vec3(1.0,0.75,0.4)*a*1.4, 1.0); }' });
const ringM = new THREE.Mesh(new THREE.PlaneGeometry(110, 110).rotateX(-Math.PI / 2), ringMat); ringM.position.copy(target).setY(target.y + 1.5); scene.add(ringM);
// colonne de lumière et de poussière qui s'élève de l'impact
const pillar = new THREE.Mesh(new THREE.CylinderGeometry(6, 14, 160, 20, 1, true).translate(0, 80, 0), beamMat(0xffa860, 0.5)); pillar.position.copy(target); scene.add(pillar);
const impactLight = new THREE.PointLight(0xff9a40, 4000, 160, 1.6); impactLight.position.copy(target).setY(target.y + 8); scene.add(impactLight);
const handLight = new THREE.PointLight(0xffd890, 25, 20, 2); handLight.position.copy(hand); scene.add(handLight);
// gerbes d'étincelles et rochers projetés
const sparks = new THREE.BufferGeometry(); const sp = [];
for (let i = 0; i < 900; i++) { const a = Math.random() * 6.28, r = Math.pow(Math.random(), 0.5) * 30, h = Math.random() * Math.random() * 45; sp.push(target.x + Math.cos(a) * r, target.y + h, target.z + Math.sin(a) * r); }
sparks.setAttribute('position', new THREE.Float32BufferAttribute(sp, 3));
scene.add(new THREE.Points(sparks, new THREE.PointsMaterial({ color: 0xffb050, size: 1.1, map: glowTex, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false })));
const rockM = new THREE.MeshStandardMaterial({ color: 0x4a3f36, roughness: 1, flatShading: true });
for (let i = 0; i < 70; i++) { const m = new THREE.Mesh(new THREE.DodecahedronGeometry(0.4 + Math.random() * 1.6, 0), rockM); const a = Math.random() * 6.28, r = 5 + Math.random() * 35; m.position.set(target.x + Math.cos(a) * r, target.y + 4 + Math.random() * 40, target.z + Math.sin(a) * r); m.rotation.set(Math.random() * 3, Math.random() * 3, 0); m.castShadow = true; scene.add(m); }
// fumée
const smokeTex = new THREE.CanvasTexture((() => { const c = document.createElement('canvas'); c.width = c.height = 128; const x = c.getContext('2d'); for (let i = 0; i < 20; i++) { const px = 30 + Math.random() * 68, py = 30 + Math.random() * 68, r = 20 + Math.random() * 30; const g = x.createRadialGradient(px, py, 0, px, py, r); g.addColorStop(0, 'rgba(255,255,255,0.25)'); g.addColorStop(1, 'rgba(255,255,255,0)'); x.fillStyle = g; x.fillRect(0, 0, 128, 128); } return c; })());
for (let i = 0; i < 14; i++) { const m = new THREE.Sprite(new THREE.SpriteMaterial({ map: smokeTex, color: 0x7a6656, transparent: true, opacity: 0.5, depthWrite: false })); const h = 10 + i * 5; m.position.set(target.x + Math.sin(i) * 6 + i * 1.5, target.y + h, target.z - i * 1.2); m.scale.setScalar(18 + i * 2.2); scene.add(m); }
// météore dans le ciel
const met = camera.position.clone().addScaledVector(fwd, 700).addScaledVector(right, -120).add(new THREE.Vector3(0, 95, 0));
spr(met, 45, 0xffc070); spr(met, 14);
const metRock = new THREE.Mesh(new THREE.DodecahedronGeometry(5, 1), new THREE.MeshStandardMaterial({ color: 0x221a14, emissive: 0xff5010, emissiveIntensity: 2 })); metRock.position.copy(met); scene.add(metRock);
const trailDir = right.clone().multiplyScalar(-0.7).add(new THREE.Vector3(0, 0.6, 0)).normalize();
for (let i = 1; i < 70; i++) spr(met.clone().addScaledVector(trailDir, i * 6), 34 - i * 0.4, i < 20 ? 0xffa040 : 0xff6a28).material.opacity = Math.pow(1 - i / 70, 1.5);
// particules divines autour du dieu
const motes = new THREE.BufferGeometry(); const mp = [];
for (let i = 0; i < 160; i++) { const a = Math.random() * 6.28, r = 2 + Math.random() * 7; mp.push(gp.x + Math.cos(a) * r, gp.y + Math.random() * 8 - 1, gp.z + Math.sin(a) * r); }
motes.setAttribute('position', new THREE.Float32BufferAttribute(mp, 3));
scene.add(new THREE.Points(motes, new THREE.PointsMaterial({ color: 0xffdf90, size: 0.12, map: glowTex, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false })));

const look = fwdT;
camera.lookAt(look);
G.god = null;
const fill = new THREE.DirectionalLight(0xffe2b8, num('fill', 0.9)); fill.position.copy(camera.position).add(new THREE.Vector3(0, 6, 0)); fill.target = god.root; scene.add(fill);
env.update(0, true);
const composer = new EffectComposer(renderer);
composer.addPass(new RenderPass(scene, camera));
composer.addPass(new UnrealBloomPass(new THREE.Vector2(W / 2, H / 2), +(P.get('bs')||0.4), 0.5, +(P.get('bt')||1.4)));
composer.addPass(new OutputPass());
let frames = 0;
function loop() {
  G.time += 0.016; G.uniforms.time.value += 0.016;
  T.update(0.016); veg.update(0.016);
  env.dayTime = +(P.get('t') || 0.725); env.update(0.0001);
  camera.lookAt(look);
  composer.render();
  if (++frames === 6) window.__ready = true;
  requestAnimationFrame(loop);
}
loop();
