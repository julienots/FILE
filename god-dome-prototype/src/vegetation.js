import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { mulberry32, noise, fbm, smoothstep, clamp } from './noise.js';
import { addWind } from './shared.js';
import { WORLD_R, VILLAGE } from './terrain.js';
import { applyRockDetail } from './ground.js';

const rnd = mulberry32(2024);
const dummy = new THREE.Object3D();
const C = h => new THREE.Color(h);

// ---------- Herbe : touffes de brins effilés, couleur racine→pointe, vent + rafales ----------
function grassClumpGeometry(blades = 6) {
  const pos = [], col = [], nor = [], idx = [];
  const r = mulberry32(5);
  for (let b = 0; b < blades; b++) {
    const ang = r() * Math.PI * 2, off = r() * 0.3, ox = Math.cos(ang) * off, oz = Math.sin(ang) * off;
    const h = 0.32 + r() * 0.42, w = 0.032 + r() * 0.022, lean = (r() - 0.3) * 0.35, rot = r() * Math.PI;
    const ca = Math.cos(rot), sa = Math.sin(rot);
    const segs = [0, 0.35, 0.7, 1];
    const base = pos.length / 3;
    segs.forEach((t, k) => {
      const ww = w * (1 - t * 0.9), y = t * h, fwd = lean * t * t;
      for (const side of [-1, 1]) {
        if (k === segs.length - 1 && side === 1) continue;
        const lx = side * ww * (k === segs.length - 1 ? 0 : 1), lz = fwd;
        pos.push(ox + lx * ca - lz * sa, y, oz + lx * sa + lz * ca);
        const c = 0.38 + 0.62 * t; col.push(c, c, c);
        nor.push(0, 1, 0);
      }
    });
    // indices : 3 quads (2 sommets/niveau) + pointe
    for (let k = 0; k < 2; k++) { const a = base + k * 2; idx.push(a, a + 1, a + 2, a + 1, a + 3, a + 2); }
    idx.push(base + 4, base + 5, base + 6);
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setAttribute('normal', new THREE.Float32BufferAttribute(nor, 3));
  g.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
  g.setIndex(idx);
  return g;
}

export function buildGrass(terrain, count) {
  const geo = grassClumpGeometry(6);
  const mat = addWind(new THREE.MeshLambertMaterial({ vertexColors: true, side: THREE.DoubleSide }), { strength: 0.3, heightRef: 0.7, sheen: 0.35 });
  const mesh = new THREE.InstancedMesh(geo, mat, count);
  const cA = C('#4f8f26'), cB = C('#a6c94a'), cDry = C('#c6c060'), cForest = C('#3d7a26'), tmp = new THREE.Color();
  let n = 0, tries = 0;
  const positions = [];
  while (n < count && tries < count * 12) {
    tries++;
    const a = rnd() * Math.PI * 2, r = Math.sqrt(rnd()) * (WORLD_R - 4);
    const x = Math.cos(a) * r, z = Math.sin(a) * r;
    if (!terrain.vegetationOK(x, z, 0.45)) continue;
    const y = terrain.heightAt(x, z);
    // densité : prairies et abords du village plus denses
    const dens = 0.35 + 0.65 * smoothstep(-0.2, 0.3, fbm(x * 0.025, z * 0.025, 2)) + smoothstep(45, 15, Math.hypot(x - VILLAGE.x, z - VILLAGE.y)) * 0.6;
    if (rnd() > dens || y > 40) continue;
    dummy.position.set(x, y - 0.05, z);
    dummy.rotation.set(0, rnd() * Math.PI * 2, 0);
    const s = 0.75 + rnd() * 0.45 + smoothstep(0.1, 0.5, fbm(x * 0.05, z * 0.05, 2)) * 0.4;
    dummy.scale.set(s, s * (0.8 + rnd() * 0.5), s);
    dummy.updateMatrix();
    mesh.setMatrixAt(n, dummy.matrix);
    const n1 = fbm(x * 0.03, z * 0.03, 3);
    tmp.copy(cA).lerp(cB, clamp(0.5 + n1 * 1.2, 0, 1)).lerp(cDry, smoothstep(0.25, 0.6, noise(x * 0.15, z * 0.15) * 0.5 + n1) * 0.45)
      .lerp(cForest, terrain.forestMask(x, z) * 0.5);
    tmp.multiplyScalar(0.92 + rnd() * 0.16);
    mesh.setColorAt(n, tmp);
    positions.push(x, z);
    n++;
  }
  mesh.count = n;
  mesh.receiveShadow = true;
  mesh.frustumCulled = false;
  mesh.userData.positions = positions;
  // supprimer l'herbe sous une nouvelle construction
  mesh.userData.clearArea = (x0, z0, hw, hd, rotY) => {
    const m = new THREE.Matrix4(), ca = Math.cos(-rotY), sa = Math.sin(-rotY);
    for (let i = 0; i < mesh.count; i++) {
      const dx = positions[i * 2] - x0, dz = positions[i * 2 + 1] - z0;
      const lx = dx * ca - dz * sa, lz = dx * sa + dz * ca;
      if (Math.abs(lx) < hw && Math.abs(lz) < hd) { mesh.getMatrixAt(i, m); m.scale(new THREE.Vector3(0, 0, 0)); mesh.setMatrixAt(i, m); }
    }
    mesh.instanceMatrix.needsUpdate = true;
  };
  return mesh;
}

// ---------- Fleurs ----------
export function buildFlowers(terrain, count) {
  const petals = new THREE.CircleGeometry(0.055, 7); petals.rotateX(-Math.PI / 2); petals.translate(0, 0.42, 0);
  const heart = new THREE.CircleGeometry(0.02, 6); heart.rotateX(-Math.PI / 2); heart.translate(0, 0.425, 0);
  const stem = new THREE.CylinderGeometry(0.005, 0.008, 0.42, 3); stem.translate(0, 0.21, 0);
  const paint = (g, c) => { const a = new Float32Array(g.attributes.position.count * 3); for (let i = 0; i < a.length; i += 3) { a[i] = c[0]; a[i + 1] = c[1]; a[i + 2] = c[2]; } g.setAttribute('color', new THREE.BufferAttribute(a, 3)); return g; };
  [petals, heart, stem].forEach(g => g.deleteAttribute('uv'));
  const geo = mergeGeometries([paint(petals, [1, 1, 1]), paint(heart, [1.6, 1.3, 0.2]), paint(stem, [0.25, 0.45, 0.12])]);
  const mat = addWind(new THREE.MeshLambertMaterial({ vertexColors: true, side: THREE.DoubleSide }), { strength: 0.25, heightRef: 0.45 });
  const mesh = new THREE.InstancedMesh(geo, mat, count);
  const palette = ['#f4f1ea', '#b89be6', '#8f6fd0', '#f2d14a', '#e98aa6', '#6f97e0', '#e8603c'].map(C);
  let n = 0, tries = 0;
  while (n < count && tries < count * 20) {
    tries++;
    const a = rnd() * Math.PI * 2, r = Math.sqrt(rnd()) * (WORLD_R - 6);
    const x = Math.cos(a) * r, z = Math.sin(a) * r;
    if (!terrain.vegetationOK(x, z, 0.35) || terrain.heightAt(x, z) > 30) continue;
    if (fbm(x * 0.04 + 3, z * 0.04, 2) < 0.08) continue; // massifs
    dummy.position.set(x, terrain.heightAt(x, z) - 0.02, z);
    dummy.rotation.set((rnd() - 0.5) * 0.3, rnd() * 6.28, (rnd() - 0.5) * 0.3);
    const s = 0.8 + rnd() * 0.9; dummy.scale.set(s, s, s); dummy.updateMatrix();
    mesh.setMatrixAt(n, dummy.matrix);
    const pi = Math.floor((noise(x * 0.05, z * 0.05) * 0.5 + 0.5) * palette.length * 0.999 + rnd() * 1.5) % palette.length;
    mesh.setColorAt(n, palette[pi]);
    n++;
  }
  mesh.count = n; mesh.receiveShadow = true; mesh.frustumCulled = false;
  return mesh;
}

// ---------- Arbres ----------
function displace(geo, amp, freq, seed) {
  const p = geo.attributes.position, v = new THREE.Vector3();
  for (let i = 0; i < p.count; i++) {
    v.fromBufferAttribute(p, i);
    const n = noise(v.x * freq + seed, v.y * freq + v.z * freq * 0.7 - seed);
    v.multiplyScalar(1 + n * amp);
    p.setXYZ(i, v.x, v.y, v.z);
  }
  return geo;
}
function colorize(geo, fn) {
  const p = geo.attributes.position, a = new Float32Array(p.count * 3), c = new THREE.Color();
  for (let i = 0; i < p.count; i++) { fn(p.getX(i), p.getY(i), p.getZ(i), c); a[i * 3] = c.r; a[i * 3 + 1] = c.g; a[i * 3 + 2] = c.b; }
  geo.setAttribute('color', new THREE.BufferAttribute(a, 3));
  return geo;
}
// normales "sphériques" : éclairage doux et peint du feuillage
function sphericalNormals(geo, center, k = 0.75) {
  const p = geo.attributes.position, n = geo.attributes.normal, v = new THREE.Vector3(), m = new THREE.Vector3();
  for (let i = 0; i < p.count; i++) {
    v.fromBufferAttribute(p, i).sub(center).normalize(); m.fromBufferAttribute(n, i);
    m.lerp(v, k).normalize(); n.setXYZ(i, m.x, m.y, m.z);
  }
  return geo;
}

function broadleafParts() {
  const r = mulberry32(3);
  // tronc + charpentières (branches porteuses)
  const parts = [];
  const trunk = new THREE.CylinderGeometry(0.2, 0.42, 5.4, 9, 4); trunk.translate(0, 2.7, 0);
  const tp = trunk.attributes.position; for (let i = 0; i < tp.count; i++) { const y = tp.getY(i); tp.setX(i, tp.getX(i) + Math.sin(y * 0.9) * 0.08); tp.setZ(i, tp.getZ(i) + Math.cos(y * 0.7) * 0.06); }
  parts.push(trunk);
  const flare = new THREE.CylinderGeometry(0.42, 0.7, 0.6, 9); flare.translate(0, 0.25, 0); parts.push(flare);
  const branchEnds = [];
  for (let i = 0; i < 6; i++) {
    const a = i / 6 * 6.28 + r() * 0.5, tilt = 0.55 + r() * 0.35, len = 2.2 + r() * 1.2, y0 = 3.6 + r() * 1.6;
    const b = new THREE.CylinderGeometry(0.05, 0.14, len, 5); b.translate(0, len / 2, 0);
    b.rotateZ(-tilt); b.rotateY(a); b.translate(0, y0, 0); parts.push(b);
    branchEnds.push(new THREE.Vector3(Math.sin(tilt) * len * Math.cos(a), y0 + Math.cos(tilt) * len, -Math.sin(tilt) * len * Math.sin(a)));
  }
  const t = mergeGeometries(parts.map(g => { g.deleteAttribute('uv'); return g.index ? g.toNonIndexed() : g; }));
  colorize(t, (x, y, z, c) => c.set('#7a6048').multiplyScalar(0.55 + 0.45 * Math.min(1, y / 4)));

  // couronne : grappes de feuilles autour des extrémités de branches + cœur sombre
  const center = new THREE.Vector3(0, 6.4, 0);
  const clusters = [...branchEnds, new THREE.Vector3(0, 8.0, 0), new THREE.Vector3(0.6, 7.2, 0.8), new THREE.Vector3(-0.7, 7.4, -0.6)];
  const coreGeos = clusters.map((cp, i) => { const g = displace(new THREE.IcosahedronGeometry(1.25, 1), 0.2, 0.8, i * 2.1); g.translate(cp.x * 0.85, cp.y, cp.z * 0.85); g.deleteAttribute('uv'); return g; });
  const canopy = mergeGeometries(coreGeos);
  sphericalNormals(canopy, center, 0.85);
  colorize(canopy, (x, y, z, c) => c.set('#1e3d12').lerp(C('#3d6b22'), smoothstep(4, 9, y)));
  const cards = [];
  clusters.forEach((cp) => {
    for (let k = 0; k < 22; k++) {
      const s = 1.1 + r() * 0.8;
      const q = new THREE.PlaneGeometry(s, s);
      const out = cp.clone().sub(center).normalize();
      const dir = new THREE.Vector3(r() - 0.5, r() - 0.4, r() - 0.5).normalize().lerp(out, 0.45).normalize();
      q.lookAt(new THREE.Vector3(r() - 0.5, r() - 0.5, r() - 0.5));
      q.rotateZ(r() * 6.28);
      const off = 0.4 + r() * 1.2;
      q.translate(cp.x + dir.x * off, cp.y + dir.y * off * 0.8, cp.z + dir.z * off);
      cards.push(q);
    }
  });
  const cardGeo = mergeGeometries(cards);
  sphericalNormals(cardGeo, center, 0.9);
  colorize(cardGeo, (x, y, z, c) => {
    const outer = smoothstep(1.0, 3.6, Math.hypot(x, y - center.y, z));
    c.setScalar(0.55 + 0.45 * outer).multiplyScalar(0.75 + 0.35 * smoothstep(4, 9.5, y));
  });
  return { trunk: t, canopy, cards: cardGeo };
}

function coniferParts() {
  const trunk = new THREE.CylinderGeometry(0.1, 0.36, 10.5, 8); trunk.translate(0, 5.25, 0); trunk.deleteAttribute('uv');
  colorize(trunk, (x, y, z, c) => c.set('#6a5038').multiplyScalar(0.6 + 0.4 * Math.min(1, y / 3)));
  const r = mulberry32(8);
  // cœur sombre conique pour remplir les vides
  const core = new THREE.ConeGeometry(1.7, 8.5, 10, 3, true); displace(core, 0.1, 1.2, 2); core.translate(0, 6.0, 0); core.deleteAttribute('uv');
  sphericalNormals(core, new THREE.Vector3(0, 6, 0), 0.6);
  colorize(core, (x, y, z, c) => c.set('#0f2414').lerp(C('#1f3a1e'), smoothstep(2, 10, y)));
  // verticilles de branches (cartes d'aiguilles inclinées vers le bas)
  const cards = [];
  const tiers = 11;
  for (let i = 0; i < tiers; i++) {
    const t = i / (tiers - 1), y = 1.9 + t * 8.6, len = 3.1 * (1 - t * 0.82) + 0.35, n = Math.round(8 - t * 3);
    for (let k = 0; k < n; k++) {
      const a = k / n * 6.28 + i * 0.7 + r() * 0.4;
      const q = new THREE.PlaneGeometry(len * 0.55, len);
      q.translate(0, len / 2, 0);
      q.rotateX(-Math.PI / 2 + 0.35 + r() * 0.25 + t * 0.2); // branche tombante
      q.rotateY(-a + Math.PI / 2);
      q.translate(0, y, 0);
      cards.push(q);
    }
  }
  const top = new THREE.PlaneGeometry(0.7, 1.4); top.translate(0, 10.9, 0); cards.push(top);
  const top2 = top.clone(); top2.rotateY(Math.PI / 2); top2.translate(0, 0, 0); cards.push(top2);
  const branches = mergeGeometries(cards);
  sphericalNormals(branches, new THREE.Vector3(0, 5, 0), 0.7);
  colorize(branches, (x, y, z, c) => c.setScalar(0.7 + 0.5 * smoothstep(0.5, 3.0, Math.hypot(x, z)) * 0.6 + smoothstep(2, 11, y) * 0.2));
  return { trunk, core, branches };
}

export function buildTrees(terrain, T, { broadleaf = 190, conifer = 200, extra = 120, detail = null } = {}) {
  const group = new THREE.Group(); group.name = 'trees';
  const B = broadleafParts(), K = coniferParts();
  const barkMat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 1 });
  if (detail) applyRockDetail(barkMat, detail, { scale: 1.1, channel: 'g', bump: 0, strength: 0.75, stretch: 0.15 });
  const coreMat = addWind(new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.9 }), { strength: 0.1, heightRef: 9 });
  const cardMat = addWind(new THREE.MeshStandardMaterial({ map: T.leaves, vertexColors: true, alphaTest: 0.5, side: THREE.DoubleSide, roughness: 0.75 }), { strength: 0.12, heightRef: 9, flutter: 0.06, sheen: 0.12, translucent: 0.45 });
  const firMat = addWind(new THREE.MeshStandardMaterial({ map: T.fir, vertexColors: true, alphaTest: 0.45, side: THREE.DoubleSide, roughness: 0.85 }), { strength: 0.09, heightRef: 11, flutter: 0.025, translucent: 0.25 });
  const depthFor = (map) => new THREE.MeshDepthMaterial({ depthPacking: THREE.RGBADepthPacking, map, alphaTest: 0.5 });

  const mk = (geo, mat, n, depthMap) => { const m = new THREE.InstancedMesh(geo, mat, n); m.castShadow = true; m.receiveShadow = true; m.count = 0; if (depthMap) m.customDepthMaterial = depthFor(depthMap); group.add(m); return m; };
  const bT = mk(B.trunk, barkMat, broadleaf + extra), bC = mk(B.canopy, coreMat, broadleaf + extra), bK = mk(B.cards, cardMat, broadleaf + extra, T.leaves);
  const cT = mk(K.trunk, barkMat, conifer + extra), cC = mk(K.core, coreMat, conifer + extra), cF = mk(K.branches, firMat, conifer + extra, T.fir);
  const colliders = [];
  const tint = new THREE.Color();

  function addTree(kind, x, z, s = 1, rotY = rnd() * 6.28) {
    const y = terrain.heightAt(x, z) - 0.2;
    dummy.position.set(x, y, z); dummy.rotation.set((rnd() - 0.5) * 0.05, rotY, (rnd() - 0.5) * 0.05); dummy.scale.set(s, s * (0.9 + rnd() * 0.25), s); dummy.updateMatrix();
    const set = (meshes, col) => meshes.forEach(m => { const i = m.count++; m.setMatrixAt(i, dummy.matrix); if (col) m.setColorAt(i, col); m.instanceMatrix.needsUpdate = true; if (m.instanceColor) m.instanceColor.needsUpdate = true; });
    if (kind === 'broad') {
      tint.setHSL(0.21 + (rnd() - 0.5) * 0.07, 0.35 + rnd() * 0.25, 0.42 + rnd() * 0.12).multiplyScalar(1.25);
      set([bT]); set([bC, bK], tint);
      colliders.push({ x, z, r: 0.45 * s });
    } else {
      tint.setHSL(0.31 + (rnd() - 0.5) * 0.05, 0.25 + rnd() * 0.2, 0.45 + rnd() * 0.1).multiplyScalar(1.3);
      set([cT]); set([cC, cF], tint);
      colliders.push({ x, z, r: 0.38 * s });
    }
  }

  // Répartition : forêt de feuillus à l'est, conifères sur les hauteurs, bosquets épars
  let tries = 0, nb = 0, nc = 0;
  while ((nb < broadleaf || nc < conifer) && tries < 40000) {
    tries++;
    const a = rnd() * Math.PI * 2, r = Math.sqrt(rnd()) * (WORLD_R - 6);
    const x = Math.cos(a) * r, z = Math.sin(a) * r;
    if (!terrain.vegetationOK(x, z, 0.6)) continue;
    if (Math.hypot(x - VILLAGE.x, z - VILLAGE.y) < 26) continue;
    if (Math.hypot(x + 58, z + 6) < 9) continue; // colline du moulin
    const y = terrain.heightAt(x, z);
    const fm = terrain.forestMask(x, z);
    const high = smoothstep(14, 28, y);
    if (rnd() > Math.max(fm * 0.9, 0.02 + high * 0.5)) continue;
    if (colliders.some(c => (c.x - x) ** 2 + (c.z - z) ** 2 < 9)) continue;
    const s = 0.75 + rnd() * 0.6;
    if (high > 0.5 || (rnd() < 0.25 && fm > 0.3)) { if (nc < conifer) { addTree('pine', x, z, s * 1.05); nc++; } }
    else if (nb < broadleaf) { addTree('broad', x, z, s); nb++; }
  }
  group.userData = { addTree, colliders };
  return group;
}

// ---------- Rochers et buissons ----------
export function buildRocks(terrain, count = 140, detail = null) {
  const geo = displace(new THREE.IcosahedronGeometry(1, 4), 0.3, 0.9, 1.7);
  displace(geo, 0.06, 3.5, 4.2);
  geo.deleteAttribute('uv');
  colorize(geo, (x, y, z, c) => { c.set('#8c877e').lerp(C('#5f8f3a'), smoothstep(0.35, 0.85, y) * 0.65); c.multiplyScalar(0.85 + noise(x * 3, z * 3 + y) * 0.15); });
  const mat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.92 });
  if (detail) applyRockDetail(mat, detail, { scale: 0.22, channel: 'b', bump: 0, strength: 0.9 });
  const mesh = new THREE.InstancedMesh(geo, mat, count);
  const colliders = [];
  let n = 0, tries = 0;
  while (n < count && tries < 20000) {
    tries++;
    const a = rnd() * Math.PI * 2, r = Math.sqrt(rnd()) * (WORLD_R - 6), x = Math.cos(a) * r, z = Math.sin(a) * r;
    if (Math.hypot(x - VILLAGE.x, z - VILLAGE.y) < 24 || terrain.pathAt(x, z) > 0.2) continue;
    const wet = terrain.wetAt(x, z), sl = terrain.slopeAt(x, z);
    if (rnd() > 0.04 + wet * 0.5 + sl * 0.5) continue;
    const s = (0.5 + rnd() ** 2 * 2.6) * (wet > 0.3 ? 0.7 : 1);
    dummy.position.set(x, terrain.heightAt(x, z) - s * 0.25, z);
    dummy.rotation.set(rnd() * 0.5, rnd() * 6.28, rnd() * 0.5);
    dummy.scale.set(s * (1 + rnd() * 0.6), s * (0.55 + rnd() * 0.4), s); dummy.updateMatrix();
    mesh.setMatrixAt(n++, dummy.matrix);
    if (s > 0.9) colliders.push({ x, z, r: s * 0.9 });
  }
  mesh.count = n; mesh.castShadow = true; mesh.receiveShadow = true;
  mesh.userData.colliders = colliders;
  return mesh;
}

export function buildBushes(terrain, count = 260, T = null) {
  const r = mulberry32(21);
  const core = mergeGeometries([[0, 0.45, 0, 0.55], [0.4, 0.35, 0.2, 0.4], [-0.35, 0.4, -0.2, 0.45]].map(([x, y, z, rr], i) => { const g = displace(new THREE.IcosahedronGeometry(rr, 1), 0.2, 1.4, i); g.translate(x, y, z); g.deleteAttribute('uv'); return g; }));
  colorize(core, (x, y, z, c) => c.set('#16300d'));
  const cards = [];
  for (let k = 0; k < 26; k++) {
    const q = new THREE.PlaneGeometry(0.9, 0.9);
    q.lookAt(new THREE.Vector3(r() - 0.5, r() - 0.5, r() - 0.5)); q.rotateZ(r() * 6.28);
    const d = new THREE.Vector3(r() - 0.5, r() * 0.7, r() - 0.5).normalize();
    q.translate(d.x * 0.7, 0.45 + d.y * 0.6, d.z * 0.7); cards.push(q);
  }
  const cardGeo = mergeGeometries(cards);
  const center = new THREE.Vector3(0, 0.3, 0);
  [core, cardGeo].forEach(g => sphericalNormals(g, center, 0.85));
  colorize(cardGeo, (x, y, z, c) => c.setScalar(0.6 + 0.5 * smoothstep(0, 1.2, y)));
  const coreMat = addWind(new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.9 }), { strength: 0.04, heightRef: 1.2 });
  const leafMat = addWind(new THREE.MeshStandardMaterial({ map: T.leaves, vertexColors: true, alphaTest: 0.5, side: THREE.DoubleSide, roughness: 0.8 }), { strength: 0.05, heightRef: 1.2, flutter: 0.03, translucent: 0.4 });
  const mesh = new THREE.InstancedMesh(core, coreMat, count);
  const cardsMesh = new THREE.InstancedMesh(cardGeo, leafMat, count);
  cardsMesh.customDepthMaterial = new THREE.MeshDepthMaterial({ depthPacking: THREE.RGBADepthPacking, map: T.leaves, alphaTest: 0.5 });
  const tint = new THREE.Color(), colliders = [];
  let n = 0, tries = 0;
  while (n < count && tries < 30000) {
    tries++;
    const a = rnd() * Math.PI * 2, rr = Math.sqrt(rnd()) * (WORLD_R - 6), x = Math.cos(a) * rr, z = Math.sin(a) * rr;
    if (!terrain.vegetationOK(x, z, 0.5)) continue;
    const dv = Math.hypot(x - VILLAGE.x, z - VILLAGE.y);
    if (dv < 9 || Math.hypot(x + 6, z - 4) < 6) continue;
    if (rnd() > 0.06 + terrain.forestMask(x, z) * 0.5 + (dv < 26 ? 0.25 : 0)) continue;
    dummy.position.set(x, terrain.heightAt(x, z) - 0.1, z); dummy.rotation.set(0, rnd() * 6.28, 0);
    const s = 0.7 + rnd() * 1.0; colliders.push({ x, z, r: 0.45 * s }); dummy.scale.set(s, s * (0.8 + rnd() * 0.4), s); dummy.updateMatrix();
    mesh.setMatrixAt(n, dummy.matrix); cardsMesh.setMatrixAt(n, dummy.matrix);
    tint.setHSL(0.22 + (rnd() - 0.5) * 0.08, 0.4, 0.5).multiplyScalar(1.3); mesh.setColorAt(n, tint); cardsMesh.setColorAt(n, tint);
    n++;
  }
  mesh.count = cardsMesh.count = n;
  [mesh, cardsMesh].forEach(m => { m.castShadow = true; m.receiveShadow = true; });
  const g = new THREE.Group(); g.add(mesh, cardsMesh); g.userData.colliders = colliders;
  return g;
}
