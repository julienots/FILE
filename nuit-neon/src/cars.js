// Voitures : liste du garage, modèle procédural par défaut, import de modèles GLB et animations.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { DRACOLoader } from 'three/addons/loaders/DRACOLoader.js';
import { MeshoptDecoder } from 'three/addons/libs/meshopt_decoder.module.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { CAR_LIST } from './carlist.js';
import { buildDesign } from './procar.js';

export const CARS = CAR_LIST;
export const STAT_KEYS = [
  ['vmax', 'Vitesse max'], ['accel', 'Accélération'], ['mania', 'Maniabilité'], ['adher', 'Adhérence'],
];
export const PAINTS = [0xb9bec9, 0xf2c200, 0xc8102e, 0x14161c, 0x1668ff, 0x19c37d, 0xff6a00, 0x8a2be2, 0xf2f2f2, 0xff3fa4];

// stats 1..10 -> grandeurs physiques
// tune : réglages extrêmes du garage (vitesse de pointe libre jusqu'à 10 000 km/h, multiplicateur d'accélération)
export const kmhDe = (def, s) => Math.round((180 + s.vmax * 18) * (def.sur ? 1 + (def.sur - 1) * 0.75 : 1));   // vitesse de pointe d'origine, en km/h
export function physFromStats(s, tune, sur = 1) {
  const p = {
    vmax: (180 + s.vmax * 18) / 3.6,        // m/s
    accel: 5 + s.accel * 1.3,               // m/s²
    steer: 1.15 + s.mania * 0.12,           // rad/s
    grip: 9 + s.adher * 2.0,                // accélération latérale tenable, m/s²
    brake: 16 + s.adher * 1.2,
  };
  if (sur > 1) { p.vmax *= 1 + (sur - 1) * 0.75; p.accel *= sur; p.brake *= 1 + (sur - 1) * 0.8; p.grip *= 1 + (sur - 1) * 0.7; }   // moteur suralimenté : freins et appui aérodynamique à la hauteur
  if (tune && tune.vmaxKmh > 0) {
    const V = Math.min(10000, tune.vmaxKmh) / 3.6, k = V / p.vmax;
    p.vmax = V;
    if (k > 1) { p.accel = Math.max(p.accel, V / 7); p.brake = Math.max(p.brake, V / 3.5); p.grip *= k; }   // le moteur, les freins et les pneus suivent
  }
  if (tune && tune.accelX > 1) p.accel *= Math.min(50, tune.accelX);
  return p;
}

const loader = new GLTFLoader();
const draco = new DRACOLoader(); draco.setDecoderPath('./draco/'); loader.setDRACOLoader(draco); loader.setMeshoptDecoder(MeshoptDecoder);
const cache = new Map();
export function loadModel(url) {
  if (!cache.has(url)) cache.set(url, new Promise((res, rej) => loader.load(url, (g) => res(g.scene), undefined, rej)));
  return cache.get(url);
}
let glowTex = null, shadowTex = null, headTex = null;
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
  v.root.add(beam); v.beam = beam;
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
  const o = Object.assign({ L: 1, W: 1, roof: 1.14, wing: 1, nose: 1, cab: 1 }, def.proc || {});
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
      const top = h0 + (o.roof - 0.76) * Math.max(0, cr(cRoof, u)) + 0.02, wb = Math.min(0.9, cr(cW, u) * o.cab) * HW, wt = wb * (o.cab > 1 ? 0.8 : 0.66), H = top - h0;
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
function rigImported(src, def, paint, loin) {
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
  const inWheel = (o) => { for (let p = o; p && p !== holder; p = p.parent) if (/^wheel_/i.test(p.name || '')) return true; return false; };
  holder.traverse(o => {
    if (!o.isMesh) return;
    const wh = inWheel(o);
    const conv = (mat) => { const key = mat.uuid + (wh ? 'w' : ''); if (!seen.has(key)) {
      let cl = mat.clone();
      // la carrosserie reçoit un vernis qui reflète le soleil et le ciel ; les roues gardent un aspect mat
      if (!wh && !loin && def.vernis !== false && cl.isMeshStandardMaterial && !cl.isMeshPhysicalMaterial) {
        const ph = new THREE.MeshPhysicalMaterial(); THREE.MeshStandardMaterial.prototype.copy.call(ph, cl); ph.defines = { STANDARD: '', PHYSICAL: '' };
        ph.clearcoat = def.moto ? 0.55 : 0.9; ph.clearcoatRoughness = 0.05; ph.envMapIntensity = 1.3; cl = ph;
      }
      seen.set(key, cl); if (PAINT_RE.test(mat.name || '') || PAINT_RE.test(o.name || '')) paintMats.push(cl); } return seen.get(key); };
    o.material = Array.isArray(o.material) ? o.material.map(conv) : conv(o.material);
    for (const m of (Array.isArray(o.material) ? o.material : [o.material])) if (m.name === 'interieur') m.side = THREE.DoubleSide;
  });
  if (def.teinte === false) paintMats.length = 0;
  // peinture réaliste : base peu métallique sous un vernis épais (avant : presque du chrome), paillettes fines
  for (const pm of paintMats) { pm.color.set(paint); if ('metalness' in pm) { pm.metalness = Math.min(Math.max(pm.metalness, 0.35), 0.55); pm.roughness = Math.min(Math.max(pm.roughness, 0.22), 0.35); }
    if (pm.isMeshPhysicalMaterial) { pm.clearcoat = 1; pm.clearcoatRoughness = 0.03; } }

  // roues préparées par tools/prep-model.mjs : nœuds wheel_fl, wheel_fr, wheel_rl, wheel_rr (voiture) ou wheel_f, wheel_r (moto),
  // dont l'origine est le centre de la roue et dont le rayon est noté dans les « extras » du fichier
  const wheels = [];
  const named = [];
  holder.traverse(o => { const mm = /^wheel_([fr])([lr])?$/i.exec(o.name || ''); if (mm) named.push([o, mm[1].toLowerCase() === 'f']); });
  if (named.length) {
    root.updateMatrixWorld(true);
    for (const [o, front] of named) {
      const steer = new THREE.Group(); o.getWorldPosition(steer.position);
      const spin = new THREE.Group(); steer.add(spin);
      (def.moto ? chassis : root).add(steer);           // la roue d'une moto se penche avec elle
      root.updateMatrixWorld(true); spin.attach(o);
      const bb2 = new THREE.Box3().setFromObject(o);
      wheels.push({ steer, spin, front, radius: (o.userData.rayon ? o.userData.rayon * sc : Math.max(0.2, (bb2.max.y - bb2.min.y) / 2)) });
    }
  }
  const found = [];
  const walk = (o) => { if (o !== holder && WHEEL_RE.test(o.name || '')) { found.push(o); return; } o.children.forEach(walk); };
  if (!named.length) walk(holder);
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

  // feux : posés sur la carrosserie, à l'endroit où le modèle a ses propres optiques (retrouvées dans sa texture), et orientés comme la tôle.
  // Ils font partie de la voiture : cachés par la caisse quand on la regarde de l'autre côté, ils roulent et tanguent avec elle.
  const glows = [], heads = []; let lampG = null;
  if (def.feux !== false && !loin) {
    glowTex = glowTex || radialTex([[0, 'rgba(255,255,255,1)'], [0.25, 'rgba(255,60,30,0.8)'], [1, 'rgba(255,0,0,0)']]);
    headTex = headTex || radialTex([[0, 'rgba(255,255,255,1)'], [0.2, 'rgba(230,240,255,0.85)'], [0.5, 'rgba(160,190,255,0.25)'], [1, 'rgba(120,160,255,0)']]);
    lampGeo = lampGeo || new THREE.PlaneGeometry(1, 1); lampGeo.userData.keep = true;
    root.updateMatrixWorld(true);
    const L = def._lampes = def._lampes || lampSpots(holder, half, len, top, def);
    lampG = new THREE.Group(); root.add(lampG);
    const put = (spot, s, tex, col, list) => {
      const m = new THREE.Mesh(lampGeo, new THREE.MeshBasicMaterial({ map: tex, color: col, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide }));
      const n = new THREE.Vector3(spot.nx * s, spot.ny, spot.nz).normalize();
      m.position.set(spot.x * s, spot.y, spot.z).addScaledVector(n, 0.025); m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 0, 1), n);
      m.renderOrder = 4; lampG.add(m); list.push(m);
    };
    for (const s of (def.moto ? [1] : [-1, 1])) {
      put(L.tail, s, glowTex, new THREE.Color(0xff2a10).multiplyScalar(2.2), glows);
      put(L.head, s, headTex, new THREE.Color(0xf0f4ff).multiplyScalar(2.0), heads);
    }
  }
  if (!loin) root.add(blobShadow(half * 2 + 0.9, len * 2 + 1.1));
  // pilote sur les motos
  let rider = null;
  if (def.moto && !loin && def.pilote !== false) { rider = buildRider(top, len * 2, paint); chassis.add(rider); }
  // vue à la première personne : pour une voiture, au-dessus du capot à la base du pare-brise (hauteur mesurée sur le modèle)
  let cockpit;
  if (def.moto) cockpit = new THREE.Vector3(0, top * 1.24, -len * 0.08);
  else {
    root.updateMatrixWorld(true);
    const zc = -len * 0.12, rc = new THREE.Raycaster(new THREE.Vector3(0, top + 2, zc), new THREE.Vector3(0, -1, 0));
    const hit = rc.intersectObject(holder, true)[0];
    cockpit = new THREE.Vector3(0, (hit ? hit.point.y : top * 0.8) + 0.42, zc);
  }
  return { root, chassis, wheels, paintMats, tailMats: [], glows, heads, lampG, half, len, moto: !!def.moto, rider, cockpit };
}

// Où sont les optiques d'un modèle importé ? On parcourt les triangles de la carrosserie tournés vers l'arrière (ou l'avant) et on lit
// la couleur de la texture à cet endroit : les feux arrière sont la zone rouge vif, les phares la zone la plus claire.
// Sans texture lisible, on prend le point de la carrosserie le plus proche de l'emplacement habituel. Le résultat est donné pour le côté droit.
let lampGeo = null;
function lampSpots(holder, half, len, top, def) {
  const moto = !!def.moto, tris = { tail: [], head: [] };
  const a = new THREE.Vector3(), b = new THREE.Vector3(), c = new THREE.Vector3(), e1 = new THREE.Vector3(), e2 = new THREE.Vector3(), nn = new THREE.Vector3();
  holder.traverse(o => {
    if (!o.isMesh || !o.geometry.attributes.position) return;
    const g = o.geometry, P = g.attributes.position, U = g.attributes.uv, I = g.index, mat = Array.isArray(o.material) ? o.material[0] : o.material;
    let px = null, W = 0, H = 0; const map = mat && mat.map;
    if (map && map.image && U) { try { W = H = 256; const cv = document.createElement('canvas'); cv.width = W; cv.height = H; const cx = cv.getContext('2d', { willReadFrequently: true }); cx.drawImage(map.image, 0, 0, W, H); px = cx.getImageData(0, 0, W, H).data; } catch (e) { px = null; } }
    const n = I ? I.count : P.count;
    for (let i = 0; i < n; i += 3) {
      const i0 = I ? I.getX(i) : i, i1 = I ? I.getX(i + 1) : i + 1, i2 = I ? I.getX(i + 2) : i + 2;
      a.fromBufferAttribute(P, i0).applyMatrix4(o.matrixWorld); b.fromBufferAttribute(P, i1).applyMatrix4(o.matrixWorld); c.fromBufferAttribute(P, i2).applyMatrix4(o.matrixWorld);
      const cz = (a.z + b.z + c.z) / 3, back = cz > len * 0.62, front = cz < -len * 0.55; if (!back && !front) continue;
      const cx = (a.x + b.x + c.x) / 3, cy = (a.y + b.y + c.y) / 3;
      if (cy < top * (moto ? 0.4 : 0.26) || cy > top * (moto ? 0.92 : back ? 0.8 : 0.7) || (moto ? Math.abs(cx) > half * 0.5 : Math.abs(cx) < half * 0.18)) continue;
      nn.crossVectors(e1.subVectors(b, a), e2.subVectors(c, a)); const area = nn.length() / 2; if (area < 1e-7) continue; nn.divideScalar(area * 2);
      if (mat.side === THREE.DoubleSide && nn.z * (back ? 1 : -1) < 0) nn.negate();
      if (nn.z * (back ? 1 : -1) < (back || moto ? 0.55 : 0.3)) continue;                     // seules les faces bien tournées vers l'arrière (ou l'avant) portent des optiques
      let red = 0, white = 0;
      if (px) { let u = (U.getX(i0) + U.getX(i1) + U.getX(i2)) / 3 * map.repeat.x + map.offset.x, v = (U.getY(i0) + U.getY(i1) + U.getY(i2)) / 3 * map.repeat.y + map.offset.y; u -= Math.floor(u); v -= Math.floor(v); if (map.flipY) v = 1 - v;
        const k = (Math.min(H - 1, Math.floor(v * H)) * W + Math.min(W - 1, Math.floor(u * W))) * 4, r = px[k] / 255, gg = px[k + 1] / 255, bb = px[k + 2] / 255;
        red = r > 0.45 ? Math.max(0, r - Math.max(gg, bb) * 1.25) : 0; white = Math.max(0, Math.min(r, gg, bb) - 0.55); }
      tris[back ? 'tail' : 'head'].push({ x: Math.abs(cx), y: cy, z: cz, nx: nn.x * Math.sign(cx || 1), ny: nn.y, nz: nn.z, area, s: back ? red : white });
    }
  });
  const spot = (list, gx, gy, sign, fixed) => {
    if (!list.length) return { x: gx, y: gy, z: sign * len, nx: 0, ny: 0, nz: sign };
    let seed = null;
    const tot = list.reduce((t, q) => t + q.s * q.area, 0), all = list.reduce((t, q) => t + q.area, 0);
    // la texture ne désigne les optiques que si la zone colorée est petite devant la face (sinon c'est la peinture)
    if (!fixed && tot > 1e-5) { let bs = 0; for (const q of list) { let sc = 0, ar = 0; for (const r of list) { const d2 = (r.x - q.x) ** 2 + (r.y - q.y) ** 2; if (d2 < 0.012) { sc += r.s * r.area; ar += r.area; } } sc *= Math.exp(-((q.x - gx) ** 2 + (q.y - gy) ** 2) / 0.16); if (ar < all * 0.35 && sc > bs) { bs = sc; seed = q; } } }   // à couleur égale, on préfère l'emplacement habituel
    if (!seed) { let bd = 1e9; for (const q of list) { const d = (q.x - gx) ** 2 + (q.y - gy) ** 2 - q.z * sign * 0.02; if (d < bd) { bd = d; seed = q; } } }
    // moyenne des triangles voisins de la même surface : position et orientation de l'optique
    const o = { x: 0, y: 0, z: 0, nx: 0, ny: 0, nz: 0 }; let w = 0;
    for (const q of list) { if ((q.x - seed.x) ** 2 + (q.y - seed.y) ** 2 > 0.008 || Math.abs(q.z - seed.z) > 0.12) continue; const k = q.area * (0.2 + q.s); for (const key in o) o[key] += q[key] * k; w += k; }
    for (const key in o) o[key] /= w;
    o.nx *= 0.5; o.ny *= 0.3; const nl = Math.hypot(o.nx, o.ny, o.nz) || 1; o.nx /= nl; o.ny /= nl; o.nz /= nl;   // l'optique reste surtout tournée vers l'arrière (ou l'avant)
    if (moto) { o.x = 0; o.nx = 0; }
    return o;
  };
  return { tail: spot(tris.tail, moto ? 0 : half * (def.feuxX ?? 0.62), top * (def.feuxY ?? (moto ? 0.6 : 0.55)), 1, def.feuxX !== undefined || def.feuxY !== undefined),
    head: spot(tris.head, moto ? 0 : half * (def.pharesX ?? 0.6), top * (def.pharesY ?? (moto ? 0.66 : 0.42)), -1, def.pharesX !== undefined || def.pharesY !== undefined) };
}

// ---------- moto avec son pilote ----------
function buildMoto(def, paint) {
  const root = new THREE.Group(), chassis = new THREE.Group(); root.add(chassis);
  const paintMat = new THREE.MeshPhysicalMaterial({ color: paint, metalness: 0.8, roughness: 0.3, clearcoat: 1, clearcoatRoughness: 0.05, envMapIntensity: 1.5 });
  const darkMat = new THREE.MeshStandardMaterial({ color: 0x0b0b0e, metalness: 0.7, roughness: 0.35, envMapIntensity: 1.2 });
  const suitMat = new THREE.MeshStandardMaterial({ color: 0x17191f, metalness: 0.1, roughness: 0.75 });
  const rot = (g, rx, x, y, z) => { g.rotateX(rx); g.translate(x, y, z); return g; };
  chassis.add(new THREE.Mesh(mergeGeometries([
    box(0.36, 0.26, 0.62, 0, 0.88, -0.08), rot(new THREE.BoxGeometry(0.42, 0.44, 0.46), 0.35, 0, 0.86, -0.56), rot(new THREE.BoxGeometry(0.28, 0.14, 0.6), 0.22, 0, 0.93, 0.52),
    box(0.17, 0.05, 0.5, 0, 0.69, -0.78), new THREE.SphereGeometry(0.175, 14, 10).translate(0, 1.62, -0.44),
  ].map(g => { const n = g.index ? g.toNonIndexed() : g; n.deleteAttribute('uv'); return n; })), paintMat));
  chassis.add(new THREE.Mesh(mergeGeometries([
    box(0.3, 0.38, 0.82, 0, 0.5, 0.02), box(0.06, 0.62, 0.06, -0.11, 0.62, -0.72), box(0.06, 0.62, 0.06, 0.11, 0.62, -0.72),
    box(0.07, 0.08, 0.72, -0.11, 0.36, 0.4), box(0.07, 0.08, 0.72, 0.11, 0.36, 0.4), rot(new THREE.BoxGeometry(0.3, 0.22, 0.03), 0.6, 0, 1.08, -0.72), box(0.62, 0.04, 0.04, 0, 1.04, -0.6),
  ]), darkMat));
  chassis.add(new THREE.Mesh(mergeGeometries([
    box(0.4, 0.22, 0.52, 0, 1.02, 0.12), rot(new THREE.BoxGeometry(0.4, 0.56, 0.3), -0.85, 0, 1.3, -0.14),
    rot(new THREE.BoxGeometry(0.1, 0.1, 0.46), 0.45, -0.24, 1.22, -0.42), rot(new THREE.BoxGeometry(0.1, 0.1, 0.46), 0.45, 0.24, 1.22, -0.42),
    box(0.13, 0.4, 0.16, -0.22, 0.78, 0.2), box(0.13, 0.4, 0.16, 0.22, 0.78, 0.2),
  ]), suitMat));
  const ex = new THREE.CylinderGeometry(0.06, 0.07, 0.62, 10); ex.rotateX(Math.PI / 2); ex.translate(0.19, 0.5, 0.62);
  chassis.add(new THREE.Mesh(ex, new THREE.MeshStandardMaterial({ color: 0x9a9aa4, metalness: 1, roughness: 0.25 })));
  const tailMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(0xff1408).multiplyScalar(2.2) });
  chassis.add(new THREE.Mesh(box(0.2, 0.06, 0.03, 0, 0.94, 0.84), tailMat));
  chassis.add(new THREE.Mesh(box(0.2, 0.1, 0.03, 0, 0.9, -0.81), new THREE.MeshBasicMaterial({ color: new THREE.Color(0xeaf4ff).multiplyScalar(3.5) })));
  const parts = [];
  const col = (g, r, gg, b) => { const n0 = g.index ? g.toNonIndexed() : g; const n = n0.attributes.position.count, c = new Float32Array(n * 3); for (let i = 0; i < n; i++) { c[i * 3] = r; c[i * 3 + 1] = gg; c[i * 3 + 2] = b; } n0.setAttribute('color', new THREE.BufferAttribute(c, 3)); n0.deleteAttribute('uv'); return n0; };
  parts.push(col(new THREE.CylinderGeometry(0.33, 0.33, 0.17, 22).rotateZ(Math.PI / 2), 0.015, 0.015, 0.018), col(new THREE.CylinderGeometry(0.22, 0.22, 0.18, 16).rotateZ(Math.PI / 2), 0.05, 0.05, 0.06));
  for (let i = 0; i < 3; i++) parts.push(col(new THREE.BoxGeometry(0.19, 0.42, 0.05).rotateX(i / 3 * Math.PI), 0.62, 0.63, 0.68));
  const wheelGeo = mergeGeometries(parts), wheelMat = new THREE.MeshStandardMaterial({ vertexColors: true, metalness: 0.6, roughness: 0.42 });
  const wheels = [];
  for (const z of [-0.78, 0.74]) { const steer = new THREE.Group(); steer.position.set(0, 0.33, z); const spin = new THREE.Mesh(wheelGeo, wheelMat); steer.add(spin); chassis.add(steer); wheels.push({ steer, spin, front: z < 0, radius: 0.33 }); }
  root.add(blobShadow(1.5, 3.0));
  return { root, chassis, wheels, paintMats: [paintMat], tailMats: [tailMat], glows: [], half: 0.5, len: 0.95, moto: true };
}

// pilote de moto : combinaison, casque à visière, bras tendus vers le guidon, genoux serrés contre le réservoir.
// Dimensions calées sur une moto de 2,1 m et 1,2 m de haut, puis mises à l'échelle du modèle.
function buildRider(top, length, accent) {
  const g = new THREE.Group(), sy = top / 1.2, sz = length / 2.1;
  const suit = new THREE.MeshStandardMaterial({ color: 0x15171c, roughness: 0.55, metalness: 0.15, envMapIntensity: 0.8 });
  const stripe = new THREE.MeshStandardMaterial({ color: accent, roughness: 0.4, metalness: 0.2 });
  const helm = new THREE.MeshPhysicalMaterial({ color: accent, metalness: 0.3, roughness: 0.25, clearcoat: 1, clearcoatRoughness: 0.04, envMapIntensity: 1.4 });
  const visor = new THREE.MeshPhysicalMaterial({ color: 0x050608, metalness: 0.9, roughness: 0.03, envMapIntensity: 2.0 });
  const P = (x, y, z) => new THREE.Vector3(x, y * sy, z * sz);
  const limb = (a, b, r, mat) => { const d = b.clone().sub(a), l = d.length(); const m = new THREE.Mesh(new THREE.CapsuleGeometry(r, Math.max(0.01, l - r), 4, 10), mat);
    m.position.copy(a).add(b).multiplyScalar(0.5); m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), d.normalize()); g.add(m); };
  // torse penché vers l'avant, bassin sur la selle
  limb(P(0, 0.98, 0.26), P(0, 1.30, -0.12), 0.19, suit);
  limb(P(0, 1.04, 0.16), P(0, 1.30, -0.10), 0.12, stripe);
  for (const s of [-1, 1]) {
    limb(P(s * 0.2, 1.30, -0.10), P(s * 0.26, 1.13, -0.36), 0.075, suit);   // bras
    limb(P(s * 0.26, 1.13, -0.36), P(s * 0.32, 1.06, -0.56), 0.065, suit);  // avant-bras jusqu'au guidon
    limb(P(s * 0.14, 0.98, 0.24), P(s * 0.21, 0.88, -0.16), 0.095, suit);   // cuisse
    limb(P(s * 0.21, 0.88, -0.16), P(s * 0.17, 0.50, 0.18), 0.075, suit);   // tibia jusqu'au repose-pied
    const boot = new THREE.Mesh(new THREE.BoxGeometry(0.11, 0.1, 0.26), suit); boot.position.copy(P(s * 0.17, 0.46, 0.12)); g.add(boot);
  }
  const head = new THREE.Mesh(new THREE.SphereGeometry(0.16, 20, 14), helm); head.position.copy(P(0, 1.44, -0.30)); head.scale.set(1, 1, 1.12); g.add(head);
  const vz = new THREE.Mesh(new THREE.SphereGeometry(0.162, 18, 10, -1.0, 2.0, 1.1, 0.75), visor); vz.position.copy(head.position); vz.scale.copy(head.scale);
  vz.rotation.y = Math.PI; g.add(vz);
  g.traverse(o => { if (o.isMesh) o.userData.shared = false; });
  return g;
}

// paillettes métalliques sous le vernis : de minuscules facettes qui accrochent la lumière
function flakes(mat) {
  if (mat.userData.flakes || !mat.isMeshStandardMaterial) return; mat.userData.flakes = true;
  mat.onBeforeCompile = (sh) => {
    sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nvarying vec3 vOP;').replace('#include <begin_vertex>', '#include <begin_vertex>\nvOP = position;');
    sh.fragmentShader = sh.fragmentShader.replace('#include <common>', '#include <common>\nvarying vec3 vOP;\nfloat fkh(vec3 p){ return fract(sin(dot(p, vec3(12.9898,78.233,37.719)))*43758.5453); }')
      .replace('#include <normal_fragment_maps>', `#include <normal_fragment_maps>
        { vec3 q = floor(vOP * 380.0); float fade = clamp(1.0 - length(vViewPosition) / 22.0, 0.0, 1.0);
          normal = normalize(normal + (vec3(fkh(q), fkh(q + 1.7), fkh(q + 3.1)) - 0.5) * 0.06 * fade); }`);
  };
}

export function createCar(def, paint) {
  const v = def.design ? buildDesign(def, paint) : (def.modele && def._scene) ? rigImported(def._scene, def, paint) : (def.proc && def.proc.type === 'moto') ? buildMoto(def, paint) : buildProcedural(def, paint);
  v.def = def; v.wheelAngle = 0; v.roll = 0; v.pitch = 0;
  if (def.modele && def._scene && def._sceneLoin) {
    const f = rigImported(def._sceneLoin, def, paint, true);
    f.root.visible = false; v.root.add(f.root);
    v.far = f; v.isFar = false;
    v.nearObjs = [v.chassis, ...v.wheels.filter(w => w.steer.parent === v.root).map(w => w.steer)];
  }
  addEffects(v);
  v.shadow = v.root.children.find(o => o.isMesh && o.renderOrder === 1);
  for (const m of v.paintMats) flakes(m);
  return v;
}
// bascule entre le modèle détaillé et sa version allégée (models/nom_loin.glb) selon la distance
export function setLod(v, far) { if (!v.far || v.isFar === far) return; v.isFar = far; v.far.root.visible = far; for (const o of v.nearObjs) o.visible = !far; }
export async function resolveModels() {
  await Promise.all(CARS.filter(c => c.modele).map(async (c) => {
    try { c._scene = await loadModel(c.modele); } catch (e) { console.warn('Modèle introuvable :', c.modele); c._scene = null; return; }
    if (c.lod !== false) c._sceneLoin = await loadModel(c.modele.replace(/\.glb$/, '_loin.glb')).catch(() => null);
    for (const sc of [c._scene, c._sceneLoin]) if (sc && !sc.userData.rouesOk) { sc.userData.rouesOk = true; try { fixWheels(sc, !!c.moto); } catch (e) { console.warn('Roues non corrigées :', c.modele, e); } }
  }));
}
// Les modèles ont été découpés triangle par triangle : des morceaux de pneu, de jante ou de disque sont restés collés à la carrosserie
// et ne tournaient pas. On rattache à chaque roue tous les triangles de la carrosserie qui se trouvent dans son cylindre.
function fixWheels(scene, moto) {
  scene.updateMatrixWorld(true);
  const body = scene.getObjectByName('body'); if (!body || !body.isMesh || !body.geometry.index) return;
  const g = body.geometry, P = g.attributes.position, Nn = g.attributes.normal, UV = g.attributes.uv, I = g.index, e = body.matrixWorld.elements, nv = P.count;
  const W = new Float32Array(nv * 3);
  for (let i = 0; i < nv; i++) { const x = P.getX(i), y = P.getY(i), z = P.getZ(i); W[i * 3] = e[0] * x + e[4] * y + e[8] * z + e[12]; W[i * 3 + 1] = e[1] * x + e[5] * y + e[9] * z + e[13]; W[i * 3 + 2] = e[2] * x + e[6] * y + e[10] * z + e[14]; }
  const wheels = [];
  scene.traverse(o => { if (!/^wheel_[fr][lr]?$/i.test(o.name || '')) return; const bb = new THREE.Box3().setFromObject(o); if (bb.isEmpty()) return;
    const m = moto ? 0.012 : 0.03; wheels.push({ o, cy: (bb.min.y + bb.max.y) / 2, cz: (bb.min.z + bb.max.z) / 2, r: o.userData.rayon || (bb.max.y - bb.min.y) / 2, x0: bb.min.x - m, x1: bb.max.x + m, tris: [] }); });
  if (!wheels.length) return;
  const keep = [];
  for (let t = 0; t < I.count; t += 3) {
    const a = I.getX(t), b = I.getX(t + 1), c = I.getX(t + 2); let own = null;
    for (const w of wheels) {
      let ok = true, dmax = 0;
      for (const v of [a, b, c]) { const x = W[v * 3]; if (x < w.x0 || x > w.x1) { ok = false; break; } dmax = Math.max(dmax, Math.hypot(W[v * 3 + 1] - w.cy, W[v * 3 + 2] - w.cz)); }
      if (!ok) continue;
      const dc = Math.hypot((W[a * 3 + 1] + W[b * 3 + 1] + W[c * 3 + 1]) / 3 - w.cy, (W[a * 3 + 2] + W[b * 3 + 2] + W[c * 3 + 2]) / 3 - w.cz);
      if (dmax <= w.r * 1.04 || (dc <= w.r * 0.99 && dmax <= w.r * 1.12)) { own = w; break; }
    }
    if (own) own.tris.push(a, b, c); else keep.push(a, b, c);
  }
  for (const w of wheels) {
    if (!w.tris.length) continue;
    const inv = w.o.matrixWorld.clone().invert().elements, map = new Map(), pos = [], nor = [], uv = [], idx = [];
    for (const v of w.tris) { let n = map.get(v);
      if (n === undefined) { n = pos.length / 3; map.set(v, n); const x = W[v * 3], y = W[v * 3 + 1], z = W[v * 3 + 2];
        pos.push(inv[0] * x + inv[4] * y + inv[8] * z + inv[12], inv[1] * x + inv[5] * y + inv[9] * z + inv[13], inv[2] * x + inv[6] * y + inv[10] * z + inv[14]);
        if (Nn) nor.push(Nn.getX(v), Nn.getY(v), Nn.getZ(v)); if (UV) uv.push(UV.getX(v), UV.getY(v)); }
      idx.push(n); }
    const ng = new THREE.BufferGeometry(); ng.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    if (Nn) ng.setAttribute('normal', new THREE.Float32BufferAttribute(nor, 3)); if (UV) ng.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
    ng.setIndex(idx);
    const mesh = new THREE.Mesh(ng, body.material); mesh.name = 'wheel_extra'; w.o.add(mesh);
  }
  g.setIndex(new THREE.BufferAttribute(new Uint32Array(keep), 1));
  scene.updateMatrixWorld(true);
}
// pouvoir Fantôme : le véhicule devient translucide
export function setGhost(v, on) {
  if (!!v.ghost === on) return; v.ghost = on;
  v.root.traverse(o => { if (!o.isMesh || o.layers.mask !== 1) return;
    for (const m of (Array.isArray(o.material) ? o.material : [o.material])) { if (m.userData.op0 === undefined) { m.userData.op0 = m.opacity; m.userData.tr0 = m.transparent; }
      m.transparent = on ? true : m.userData.tr0; m.opacity = on ? m.userData.op0 * 0.32 : m.userData.op0; m.needsUpdate = true; } });
}
export function setPaint(v, color) { for (const m of v.paintMats) m.color.set(color); }

// animation visuelle : roues qui tournent et braquent, roulis, tangage, feux stop
export function animateCar(v, dt, speed, steer, latAcc, lonAcc, braking, time) {
  v.wheelAngle += speed * dt;
  for (const w of v.wheels) {
    w.spin.rotation.x = -v.wheelAngle / w.radius;
    if (w.front) w.steer.rotation.y = -steer * (v.moto ? 0.22 : 0.42);
  }
  const k = 1 - Math.exp(-8 * dt);
  // la moto se couche vers l'intérieur du virage, la voiture prend un peu de roulis vers l'extérieur
  v.roll += ((v.moto ? -THREE.MathUtils.clamp(Math.atan(latAcc / 9.81) * 0.6, -0.58, 0.58) : THREE.MathUtils.clamp(latAcc * 0.0035, -0.07, 0.07)) - v.roll) * k;
  v.pitch += (THREE.MathUtils.clamp(-lonAcc * 0.0022, -0.035, 0.03) - v.pitch) * k;
  v.chassis.rotation.z = v.roll;
  v.chassis.rotation.x = v.pitch;
  if (v.lampG) { v.lampG.rotation.z = v.roll; v.lampG.rotation.x = v.pitch; }   // les feux suivent les mouvements de la caisse
  v.chassis.position.y = v.hover ? v.hover + Math.sin(time * 2.3 + v.len) * 0.05 : Math.sin(time * 31 + v.wheelAngle) * 0.004 * Math.min(1, Math.abs(speed) / 30);   // une voiture volante flotte et ondule
  if (v.mist) { const a = THREE.MathUtils.clamp((Math.abs(speed) - 10) / 45, 0, 1) * (v.wet ?? 1); v.mist.material.opacity = a * (0.16 + Math.random() * 0.1); const s = 2.6 + a * 2.2 + Math.random() * 0.8; v.mist.scale.set(s, s * 0.42, 1); v.mist.position.z = v.len + 1.2 + a * 1.4; }
  const b = braking ? 2.6 : 1.1;
  for (const m of v.tailMats) m.color.setRGB(b, 0.02 * b, 0.01 * b);
  for (const g of v.glows) { g.material.opacity = braking ? 1 : 0.6; g.scale.set(braking ? 0.44 : 0.34, braking ? 0.2 : 0.15, 1); }
  if (v.heads) for (const h of v.heads) { const k = v.lights ? 1 : 0.35; h.material.opacity = k; h.scale.set(0.26 * k + 0.12, 0.13 * k + 0.07, 1); }
}

export function disposeCar(v) {
  // les modèles importés partagent leurs géométries entre voitures : on ne libère que le procédural
  if (!v.def._scene) v.root.traverse(o => { if (o.isMesh && !o.geometry.userData.keep) o.geometry.dispose(); });
}
