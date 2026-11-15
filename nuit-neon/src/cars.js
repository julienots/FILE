// Voitures : liste du garage, modèle procédural par défaut, import de modèles GLB et animations.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { DRACOLoader } from 'three/addons/loaders/DRACOLoader.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { CAR_LIST } from './carlist.js';

export const CARS = CAR_LIST;
export const STAT_KEYS = [
  ['vmax', 'Vitesse max'], ['accel', 'Accélération'], ['mania', 'Maniabilité'], ['adher', 'Adhérence'],
];
export const PAINTS = [0xb9bec9, 0xf2c200, 0xc8102e, 0x14161c, 0x1668ff, 0x19c37d, 0xff6a00, 0x8a2be2, 0xf2f2f2, 0xff3fa4];

// stats 1..10 -> grandeurs physiques
export function physFromStats(s) {
  return {
    vmax: (180 + s.vmax * 18) / 3.6,        // m/s
    accel: 5 + s.accel * 1.3,               // m/s²
    steer: 1.15 + s.mania * 0.12,           // rad/s
    grip: 9 + s.adher * 2.0,                // accélération latérale tenable, m/s²
    brake: 16 + s.adher * 1.2,
  };
}

const loader = new GLTFLoader();
const draco = new DRACOLoader(); draco.setDecoderPath('./draco/'); loader.setDRACOLoader(draco);
const cache = new Map();
export function loadModel(url) {
  if (!cache.has(url)) cache.set(url, new Promise((res, rej) => loader.load(url, (g) => res(g.scene), undefined, rej)));
  return cache.get(url);
}
let glowTex = null, shadowTex = null;
function radialTex(stops) {
  const c = document.createElement('canvas'); c.width = c.height = 128; const g = c.getContext('2d');
  const gr = g.createRadialGradient(64, 64, 4, 64, 64, 64); stops.forEach(([o, col]) => gr.addColorStop(o, col)); g.fillStyle = gr; g.fillRect(0, 0, 128, 128);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
}
function blobShadow(w, l) {
  shadowTex = shadowTex || radialTex([[0, 'rgba(0,0,0,0.95)'], [0.6, 'rgba(0,0,0,0.7)'], [1, 'rgba(0,0,0,0)']]);
  const m = new THREE.Mesh(new THREE.PlaneGeometry(w, l), new THREE.MeshBasicMaterial({ map: shadowTex, transparent: true, depthWrite: false, fog: false }));
  m.rotation.x = -Math.PI / 2; m.position.y = 0.015; m.layers.set(1); m.renderOrder = 1;
  return m;
}
function box(w, h, d, x, y, z) { const g = new THREE.BoxGeometry(w, h, d); g.translate(x, y, z); return g; }
let beamTex = null, mistTex = null;
// faisceau des phares projeté sur la route + nuage d'eau soulevé derrière la voiture
function addEffects(v) {
  if (!beamTex) {
    const c = document.createElement('canvas'); c.width = 128; c.height = 256; const g = c.getContext('2d');
    for (const cx of [44, 84]) { const gr = g.createRadialGradient(cx, 250, 4, cx, 250, 240); gr.addColorStop(0, 'rgba(255,255,255,0.9)'); gr.addColorStop(0.35, 'rgba(255,255,255,0.3)'); gr.addColorStop(1, 'rgba(255,255,255,0)');
      g.save(); g.translate(cx, 250); g.scale(0.22, 1); g.translate(-cx, -250); g.fillStyle = gr; g.fillRect(-400, 0, 900, 256); g.restore(); }
    beamTex = new THREE.CanvasTexture(c); beamTex.colorSpace = THREE.SRGBColorSpace;
    mistTex = radialTex([[0, 'rgba(255,255,255,0.9)'], [0.5, 'rgba(255,255,255,0.35)'], [1, 'rgba(255,255,255,0)']]);
  }
  const beam = new THREE.Mesh(new THREE.PlaneGeometry(6, 15), new THREE.MeshBasicMaterial({ map: beamTex, color: new THREE.Color(0.11, 0.115, 0.14), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, fog: false }));
  beam.rotation.x = -Math.PI / 2; beam.position.set(0, 0.03, -v.len - 7.2); beam.layers.set(1); beam.renderOrder = 2;
  v.root.add(beam);
  v.mist = new THREE.Sprite(new THREE.SpriteMaterial({ map: mistTex, color: 0xaab4d0, transparent: true, opacity: 0, depthWrite: false }));
  v.mist.position.set(0, 0.5, v.len + 1.5); v.mist.layers.set(1);
  v.root.add(v.mist);
}

// spline de Catmull-Rom uniforme sur un tableau de nombres
function cr(arr, t) {
  const n = arr.length - 1, f = Math.min(n - 1e-6, Math.max(0, t * n)), i = Math.floor(f), u = f - i;
  const p0 = arr[Math.max(0, i - 1)], p1 = arr[i], p2 = arr[i + 1], p3 = arr[Math.min(n, i + 2)];
  return 0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (-p0 + 3 * p1 - 3 * p2 + p3) * u * u * u);
}
function profile(pts, n) {
  const c = new THREE.CatmullRomCurve3(pts.map(p => new THREE.Vector3(p[0], p[1], 0)), false, 'centripetal');
  return c.getPoints(n - 1);
}

// ---------- voiture procédurale (utilisée tant qu'aucun modèle n'est importé) ----------
function buildProcedural(def, paint) {
  const o = Object.assign({ L: 1, W: 1, roof: 1.14, wing: 1, nose: 1 }, def.proc || {});
  const root = new THREE.Group(), chassis = new THREE.Group(); root.add(chassis);
  const HW = 0.98 * o.W, Lh = 2.25 * o.L;
  const st = {
    t: [0, 0.035, 0.11, 0.24, 0.38, 0.5, 0.64, 0.79, 0.91, 0.975, 1],
    w: [0.56, 0.84, 0.94, 0.985, 0.955, 0.95, 0.985, 1.0, 0.985, 0.93, 0.82],
    h: [0.36 * o.nose, 0.45 * o.nose, 0.53, 0.63, 0.70, 0.76, 0.82, 0.86, 0.88, 0.86, 0.80],
    b: [0.25, 0.18, 0.15, 0.15, 0.15, 0.15, 0.15, 0.16, 0.20, 0.27, 0.32],
  };
  const at = (arr, t) => { let lo = 0, hi = 1; for (let i = 0; i < 18; i++) { const mid = (lo + hi) / 2; if (cr(st.t, mid) < t) lo = mid; else hi = mid; } return cr(arr, (lo + hi) / 2); };
  const M = 34, R = 13;
  const pos = [], idx = [];
  const ring = R * 2 - 2;
  for (let m = 0; m <= M; m++) {
    const t = m / M, z = -Lh + cr(st.t, t) * 2 * Lh, w = cr(st.w, t) * HW, h = cr(st.h, t), b = cr(st.b, t), H = h - b;
    const pr = profile([[0, b], [0.74 * w, b], [0.96 * w, b + 0.05], [w, b + 0.24 * H], [w, b + 0.58 * H], [0.94 * w, b + 0.86 * H], [0.74 * w, h], [0, h + 0.035]], R);
    for (let i = 0; i < R; i++) pos.push(pr[i].x, pr[i].y, z);
    for (let i = R - 2; i >= 1; i--) pos.push(-pr[i].x, pr[i].y, z);
  }
  for (let m = 0; m < M; m++) for (let i = 0; i < ring; i++) {
    const a = m * ring + i, b2 = m * ring + (i + 1) % ring, c = a + ring, d = b2 + ring;
    idx.push(a, b2, c, b2, d, c);
  }
  const bodyGeo = new THREE.BufferGeometry(); bodyGeo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); bodyGeo.setIndex(idx); bodyGeo.computeVertexNormals();
  // extrémités fermées (sommets dupliqués pour une arête franche)
  const caps = [];
  for (const m of [0, M]) {
    const cp = [], ci = []; let cy = 0;
    for (let i = 0; i < ring; i++) { const k = (m * ring + i) * 3; cp.push(pos[k], pos[k + 1], pos[k + 2]); cy += pos[k + 1]; }
    cp.push(0, cy / ring, pos[m * ring * 3 + 2]);
    for (let i = 0; i < ring; i++) m === 0 ? ci.push(ring, i, (i + 1) % ring) : ci.push(ring, (i + 1) % ring, i);
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(cp, 3)); g.setIndex(ci); g.computeVertexNormals(); caps.push(g);
  }
  const strip = (g) => { const n = g.toNonIndexed(); n.deleteAttribute('uv'); return n; };
  const paintMat = new THREE.MeshPhysicalMaterial({ color: paint, metalness: 0.85, roughness: 0.3, clearcoat: 1, clearcoatRoughness: 0.04, envMapIntensity: 1.5 });
  chassis.add(new THREE.Mesh(mergeGeometries([bodyGeo, ...caps].map(strip)), paintMat));

  // habitacle vitré
  {
    const ct = [0.27, 0.34, 0.43, 0.52, 0.62, 0.72, 0.80], cRoof = [0, 0.62, 0.96, 1, 0.88, 0.45, 0], cW = [0.80, 0.78, 0.74, 0.72, 0.72, 0.74, 0.76];
    const CM = 20, CR = 8, cp = [], ci = [], rowN = CR * 2 - 1;
    for (let m = 0; m <= CM; m++) {
      const u = m / CM, t = cr(ct, u), z = -Lh + t * 2 * Lh, h0 = at(st.h, t) - 0.03;
      const top = h0 + (o.roof - 0.76) * Math.max(0, cr(cRoof, u)) + 0.02, wb = cr(cW, u) * HW, wt = wb * 0.66, H = top - h0;
      const pr = profile([[wb, h0], [wb * 0.985, h0 + 0.3 * H], [wt * 1.12, h0 + 0.78 * H], [wt * 0.8, top], [0, top + 0.02]], CR);
      for (let i = 0; i < CR; i++) cp.push(pr[i].x, pr[i].y, z);
      for (let i = CR - 2; i >= 0; i--) cp.push(-pr[i].x, pr[i].y, z);
    }
    for (let m = 0; m < CM; m++) for (let i = 0; i < rowN - 1; i++) { const a = m * rowN + i, b2 = a + 1, c = a + rowN, d = c + 1; ci.push(a, c, b2, b2, c, d); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(cp, 3)); g.setIndex(ci); g.computeVertexNormals();
    chassis.add(new THREE.Mesh(g, new THREE.MeshPhysicalMaterial({ color: 0x030408, metalness: 0.9, roughness: 0.05, envMapIntensity: 2.2, side: THREE.DoubleSide })));
  }

  // éléments noirs : diffuseur, lame avant, aileron, prises d'air, rétroviseurs
  const trim = [];
  trim.push(box(HW * 1.5, 0.2, 0.03, 0, 0.57, Lh + 0.004));               // bandeau arrière
  trim.push(box(HW * 1.45, 0.17, 0.34, 0, 0.3, Lh - 0.14));               // diffuseur
  for (let i = -3; i <= 3; i++) trim.push(box(0.025, 0.13, 0.3, i * HW * 0.2, 0.3, Lh - 0.16));
  trim.push(box(HW * 1.75, 0.03, 0.3, 0, 0.13, -Lh + 0.12));              // lame avant
  for (const s of [-1, 1]) {
    trim.push(box(0.05, 0.24, 0.62, s * (HW * 0.985), 0.5, Lh * 0.26));   // prise d'air latérale
    trim.push(box(0.2, 0.07, 0.11, s * (HW * 0.84 + 0.12), 0.84, -Lh * 0.17)); // rétroviseur
    trim.push(box(HW * 0.5, 0.03, 0.42, s * HW * 0.42, 0.885, Lh * 0.62)); // ouïes capot moteur
  }
  if (o.wing) {
    const wy = 1.0 + 0.12 * o.wing, ww = HW * (1.75 + 0.15 * o.wing), z = Lh - 0.2;
    trim.push(box(ww, 0.035, 0.42, 0, wy, z), box(ww, 0.03, 0.14, 0, wy + 0.05, z + 0.24));
    for (const s of [-1, 1]) { trim.push(box(0.03, 0.26, 0.56, s * ww / 2, wy - 0.03, z + 0.06)); trim.push(box(0.04, wy - 0.84, 0.22, s * HW * 0.42, (wy + 0.84) / 2, z - 0.1)); }
  }
  chassis.add(new THREE.Mesh(mergeGeometries(trim), new THREE.MeshStandardMaterial({ color: 0x08080b, metalness: 0.7, roughness: 0.3, envMapIntensity: 1.4 })));
  // échappements
  const ex = [];
  for (const s of [-1, 1]) { const g = new THREE.CylinderGeometry(0.065, 0.065, 0.12, 12); g.rotateX(Math.PI / 2); g.translate(s * 0.2, 0.47, Lh + 0.03); ex.push(g); }
  chassis.add(new THREE.Mesh(mergeGeometries(ex), new THREE.MeshStandardMaterial({ color: 0x9a9aa4, metalness: 1, roughness: 0.25 })));
  // feux
  const tail = [box(HW * 1.46, 0.045, 0.03, 0, 0.71, Lh + 0.012)];
  for (const s of [-1, 1]) tail.push(box(0.05, 0.17, 0.03, s * HW * 0.7, 0.62, Lh + 0.014), box(0.05, 0.17, 0.03, s * HW * 0.62, 0.62, Lh + 0.014));
  const tailMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(0xff1408).multiplyScalar(3.2) });
  chassis.add(new THREE.Mesh(mergeGeometries(tail), tailMat));
  const head = [];
  for (const s of [-1, 1]) head.push(box(0.34, 0.035, 0.1, s * HW * 0.56, 0.46 * o.nose + 0.03, -Lh + 0.2));
  chassis.add(new THREE.Mesh(mergeGeometries(head), new THREE.MeshBasicMaterial({ color: new THREE.Color(0xeaf4ff).multiplyScalar(3.5) })));

  // roues (pneu + jante dans un seul maillage, couleurs par sommet)
  const wheelGeo = (() => {
    const parts = [];
    const col = (g, r, gg, b) => { const n = g.attributes.position.count, c = new Float32Array(n * 3); for (let i = 0; i < n; i++) { c[i * 3] = r; c[i * 3 + 1] = gg; c[i * 3 + 2] = b; } g.setAttribute('color', new THREE.BufferAttribute(c, 3)); g.deleteAttribute('uv'); return g.index ? g.toNonIndexed() : g; };
    const tire = new THREE.CylinderGeometry(0.34, 0.34, 0.3, 22); tire.rotateZ(Math.PI / 2); parts.push(col(tire, 0.015, 0.015, 0.018));
    const hub = new THREE.CylinderGeometry(0.235, 0.235, 0.31, 18); hub.rotateZ(Math.PI / 2); parts.push(col(hub, 0.05, 0.05, 0.06));
    for (let i = 0; i < 5; i++) { const sp = new THREE.BoxGeometry(0.325, 0.44, 0.06); sp.rotateX(i / 5 * Math.PI); parts.push(col(sp, 0.62, 0.63, 0.68)); }
    return mergeGeometries(parts);
  })();
  const wheelMat = new THREE.MeshStandardMaterial({ vertexColors: true, metalness: 0.6, roughness: 0.42, envMapIntensity: 1.1 });
  const wheels = [];
  for (const [sx, sz] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) {
    const steer = new THREE.Group(); steer.position.set(sx * (HW - 0.135), 0.34, sz * Lh * 0.6);
    const spin = new THREE.Mesh(wheelGeo, wheelMat); steer.add(spin); root.add(steer);
    wheels.push({ steer, spin, front: sz < 0, radius: 0.34 });
  }
  root.add(blobShadow(HW * 2 + 0.9, Lh * 2 + 1.1));
  return { root, chassis, wheels, paintMats: [paintMat], tailMats: [tailMat], glows: [], half: HW, len: Lh };
}

// ---------- modèle importé (GLB) : mise à l'échelle, roues détectées, feux ajoutés ----------
const WHEEL_RE = /wheel|roue|tyre|tire|rim|jante/i;
const PAINT_RE = /body|paint|carross|carpaint|coque/i;
function rigImported(src, def, paint) {
  const root = new THREE.Group(), chassis = new THREE.Group(); root.add(chassis);
  const m = src.clone(true);
  const holder = new THREE.Group(); holder.add(m); chassis.add(holder);
  let bb = new THREE.Box3().setFromObject(holder), size = bb.getSize(new THREE.Vector3());
  // l'axe le plus long doit être l'axe z ; rotY permet de retourner la voiture (avant vers -z)
  let rot = def.rotY || 0;
  if (def.rotY === undefined && size.x > size.z) rot = Math.PI / 2;
  holder.rotation.y = rot;
  bb = new THREE.Box3().setFromObject(holder); size = bb.getSize(new THREE.Vector3());
  const sc = (def.longueur || 4.5) / size.z;
  holder.scale.setScalar(sc);
  bb = new THREE.Box3().setFromObject(holder);
  const c = bb.getCenter(new THREE.Vector3());
  holder.position.set(-c.x, -bb.min.y + (def.hauteur || 0), -c.z);
  root.updateMatrixWorld(true);
  bb = new THREE.Box3().setFromObject(holder);
  const half = (bb.max.x - bb.min.x) / 2, len = (bb.max.z - bb.min.z) / 2, top = bb.max.y;

  // matériaux : copie par voiture, peinture repérée par son nom
  const paintMats = [], seen = new Map();
  holder.traverse(o => {
    if (!o.isMesh) return;
    const conv = (mat) => { if (!seen.has(mat)) { const cl = mat.clone(); seen.set(mat, cl); if (PAINT_RE.test(mat.name || '') || PAINT_RE.test(o.name || '')) paintMats.push(cl); } return seen.get(mat); };
    o.material = Array.isArray(o.material) ? o.material.map(conv) : conv(o.material);
  });
  if (def.teinte === false) paintMats.length = 0;
  for (const pm of paintMats) { pm.color.set(paint); if ('metalness' in pm) { pm.metalness = Math.max(pm.metalness, 0.6); pm.roughness = Math.min(pm.roughness, 0.35); } }

  // roues : nœuds dont le nom évoque une roue, regroupés par coin de la voiture
  const wheels = [];
  const found = [];
  const walk = (o) => { if (o !== holder && WHEEL_RE.test(o.name || '')) { found.push(o); return; } o.children.forEach(walk); };
  walk(holder);
  const quad = new Map();
  for (const o of found) {
    const b = new THREE.Box3().setFromObject(o); if (b.isEmpty()) continue;
    const ctr = b.getCenter(new THREE.Vector3());
    if (Math.abs(ctr.x) < half * 0.3) continue; // roue de secours, volant...
    const key = (ctr.x < 0 ? 'L' : 'R') + (ctr.z < 0 ? 'F' : 'B');
    if (!quad.has(key)) quad.set(key, { box: new THREE.Box3(), nodes: [] });
    quad.get(key).box.union(b); quad.get(key).nodes.push(o);
  }
  if (quad.size === 4) for (const [key, qd] of quad) {
    const ctr = qd.box.getCenter(new THREE.Vector3()), s = qd.box.getSize(new THREE.Vector3());
    const steer = new THREE.Group(); steer.position.copy(ctr); const spin = new THREE.Group(); steer.add(spin); root.add(steer);
    root.updateMatrixWorld(true);
    for (const n of qd.nodes) spin.attach(n);
    wheels.push({ steer, spin, front: key[1] === 'F', radius: Math.max(0.2, s.y / 2) });
  }

  // feux arrière lumineux ajoutés (s'intensifient au freinage)
  const glows = [];
  if (def.feux !== false) {
    glowTex = glowTex || radialTex([[0, 'rgba(255,255,255,1)'], [0.25, 'rgba(255,60,30,0.8)'], [1, 'rgba(255,0,0,0)']]);
    for (const s of [-1, 1]) {
      const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: new THREE.Color(0xff2a10).multiplyScalar(2.2), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false }));
      sp.scale.set(0.36, 0.2, 1); sp.position.set(s * half * (def.feuxX ?? 0.62), top * (def.feuxY ?? 0.55), len + 0.03);
      chassis.add(sp); glows.push(sp);
    }
  }
  root.add(blobShadow(half * 2 + 0.9, len * 2 + 1.1));
  return { root, chassis, wheels, paintMats, tailMats: [], glows, half, len };
}

export function createCar(def, paint) {
  const v = (def.modele && def._scene) ? rigImported(def._scene, def, paint) : buildProcedural(def, paint);
  v.def = def; v.wheelAngle = 0; v.roll = 0; v.pitch = 0;
  addEffects(v);
  return v;
}
export async function resolveModels() {
  for (const c of CARS) if (c.modele) { try { c._scene = await loadModel(c.modele); } catch (e) { console.warn('Modèle introuvable :', c.modele); c._scene = null; } }
}
export function setPaint(v, color) { for (const m of v.paintMats) m.color.set(color); }

// animation visuelle : roues qui tournent et braquent, roulis, tangage, feux stop
export function animateCar(v, dt, speed, steer, latAcc, lonAcc, braking, time) {
  v.wheelAngle += speed * dt;
  for (const w of v.wheels) {
    w.spin.rotation.x = -v.wheelAngle / w.radius;
    if (w.front) w.steer.rotation.y = -steer * 0.42;
  }
  const k = 1 - Math.exp(-8 * dt);
  v.roll += (THREE.MathUtils.clamp(latAcc * 0.0035, -0.07, 0.07) - v.roll) * k;
  v.pitch += (THREE.MathUtils.clamp(-lonAcc * 0.0022, -0.035, 0.03) - v.pitch) * k;
  v.chassis.rotation.z = v.roll;
  v.chassis.rotation.x = v.pitch;
  v.chassis.position.y = Math.sin(time * 31 + v.wheelAngle) * 0.004 * Math.min(1, Math.abs(speed) / 30);
  if (v.mist) { const a = THREE.MathUtils.clamp((Math.abs(speed) - 10) / 45, 0, 1); v.mist.material.opacity = a * (0.16 + Math.random() * 0.1); const s = 2.6 + a * 2.2 + Math.random() * 0.8; v.mist.scale.set(s, s * 0.42, 1); v.mist.position.z = v.len + 1.2 + a * 1.4; }
  const b = braking ? 2.6 : 1.1;
  for (const m of v.tailMats) m.color.setRGB(b, 0.02 * b, 0.01 * b);
  for (const g of v.glows) { g.material.opacity = braking ? 1 : 0.55; g.scale.set(braking ? 0.5 : 0.36, braking ? 0.28 : 0.2, 1); }
}

export function disposeCar(v) {
  // les modèles importés partagent leurs géométries entre voitures : on ne libère que le procédural
  if (!v.def._scene) v.root.traverse(o => { if (o.isMesh) o.geometry.dispose(); });
}
