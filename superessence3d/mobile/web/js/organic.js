// Corps organiques : champ de distance (capsules effilées fusionnées en douceur) → marching cubes
// → maillage lisse indexé, avec poids d'os (skinning) pour l'animation.
import * as THREE from "three";
import { edgeTable, triTable } from "../vendor/three/addons/objects/MarchingCubes.js";

// primitive : capsule effilée { a, b, ra, rb, bone } ou ellipsoïde { type:"ellipsoid", c, r:[rx,ry,rz], bone }
// sub:true = volume creusé (soustraction douce : bouche, orbites, creux)
export function primDist(px, py, pz, c) {
  if (c.type === "ellipsoid") {
    const x = (px - c.c[0]) / c.r[0], y = (py - c.c[1]) / c.r[1], z = (pz - c.c[2]) / c.r[2];
    return (Math.sqrt(x * x + y * y + z * z) - 1) * Math.min(c.r[0], c.r[1], c.r[2]);
  }
  return capsuleDist(px, py, pz, c);
}

function capsuleDist(px, py, pz, c) {
  const abx = c.b[0] - c.a[0], aby = c.b[1] - c.a[1], abz = c.b[2] - c.a[2];
  const apx = px - c.a[0], apy = py - c.a[1], apz = pz - c.a[2];
  const l2 = abx * abx + aby * aby + abz * abz;
  let t = l2 > 1e-12 ? (apx * abx + apy * aby + apz * abz) / l2 : 0;
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  const dx = apx - abx * t, dy = apy - aby * t, dz = apz - abz * t;
  return Math.sqrt(dx * dx + dy * dy + dz * dz) - (c.ra + (c.rb - c.ra) * t);
}

function smin(a, b, k) {
  const h = Math.max(k - Math.abs(a - b), 0) / k;
  return Math.min(a, b) - h * h * k * 0.25;
}

/** Construit la géométrie d'un corps organique. resolution = nombre de cellules sur le plus grand axe. */
export function buildOrganic(capsules, { resolution = 72, blend = null, smooth = 2, detail = null } = {}) {
  let lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  let rmax = 0;
  const bounds = (c) => c.type === "ellipsoid"
    ? [c.c.map((v, i) => v - c.r[i]), c.c.map((v, i) => v + c.r[i]), Math.max(...c.r)]
    : [[0, 1, 2].map((i) => Math.min(c.a[i], c.b[i]) - Math.max(c.ra, c.rb)), [0, 1, 2].map((i) => Math.max(c.a[i], c.b[i]) + Math.max(c.ra, c.rb)), Math.max(c.ra, c.rb)];
  for (const c of capsules) {
    if (c.sub) continue;
    const [l, hh, r] = bounds(c);
    rmax = Math.max(rmax, r);
    for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], l[k]); hi[k] = Math.max(hi[k], hh[k]); }
  }
  const size = Math.max(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]);
  const cell = size / resolution;
  const k = blend ?? Math.max(cell * 2.5, rmax * 0.35);
  const pad = k + cell * 2;
  for (let i = 0; i < 3; i++) { lo[i] -= pad; hi[i] += pad; }
  const nx = Math.ceil((hi[0] - lo[0]) / cell) + 1, ny = Math.ceil((hi[1] - lo[1]) / cell) + 1, nz = Math.ceil((hi[2] - lo[2]) / cell) + 1;
  const field = new Float32Array(nx * ny * nz).fill(cell * 6);
  const idx = (x, y, z) => x + nx * (y + ny * z);
  const ordered = [...capsules.filter((c) => !c.sub), ...capsules.filter((c) => c.sub)];
  for (const c of ordered) {
    const [l, hh] = bounds(c);
    const kk = c.blend ?? k;
    const m = kk + cell;
    const x0 = Math.max(0, Math.floor((l[0] - m - lo[0]) / cell)), x1 = Math.min(nx - 1, Math.ceil((hh[0] + m - lo[0]) / cell));
    const y0 = Math.max(0, Math.floor((l[1] - m - lo[1]) / cell)), y1 = Math.min(ny - 1, Math.ceil((hh[1] + m - lo[1]) / cell));
    const z0 = Math.max(0, Math.floor((l[2] - m - lo[2]) / cell)), z1 = Math.min(nz - 1, Math.ceil((hh[2] + m - lo[2]) / cell));
    for (let z = z0; z <= z1; z++) {
      const pz = lo[2] + z * cell;
      for (let y = y0; y <= y1; y++) {
        const py = lo[1] + y * cell;
        let i = idx(x0, y, z);
        for (let x = x0; x <= x1; x++, i++) {
          const d = primDist(lo[0] + x * cell, py, pz, c);
          field[i] = c.sub ? -smin(-field[i], d, kk) : smin(field[i], d, kk);
        }
      }
    }
  }
  if (detail) detail(field, nx, ny, nz, lo, cell);
  // marching cubes avec partage des sommets sur les arêtes
  const positions = [];
  const indices = [];
  const edgeCache = new Map();
  const corner = [[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0], [0, 0, 1], [1, 0, 1], [1, 1, 1], [0, 1, 1]];
  const edgeCorners = [[0, 1], [1, 2], [2, 3], [3, 0], [4, 5], [5, 6], [6, 7], [7, 4], [0, 4], [1, 5], [2, 6], [3, 7]];
  const vals = new Float32Array(8);
  const vertOnEdge = new Int32Array(12);
  for (let z = 0; z < nz - 1; z++) for (let y = 0; y < ny - 1; y++) for (let x = 0; x < nx - 1; x++) {
    let cubeIndex = 0;
    for (let c = 0; c < 8; c++) {
      const v = field[idx(x + corner[c][0], y + corner[c][1], z + corner[c][2])];
      vals[c] = v;
      if (v < 0) cubeIndex |= 1 << c;
    }
    const edges = edgeTable[cubeIndex];
    if (!edges) continue;
    for (let e = 0; e < 12; e++) {
      if (!(edges & (1 << e))) continue;
      const [c0, c1] = edgeCorners[e];
      const ax = x + corner[c0][0], ay = y + corner[c0][1], az = z + corner[c0][2];
      const bx = x + corner[c1][0], by = y + corner[c1][1], bz = z + corner[c1][2];
      const ia = idx(ax, ay, az), ib = idx(bx, by, bz);
      const axis = ax !== bx ? 0 : ay !== by ? 1 : 2;
      const key = Math.min(ia, ib) * 3 + axis;
      let vi = edgeCache.get(key);
      if (vi === undefined) {
        const va = vals[c0], vb = vals[c1];
        const t = Math.abs(va - vb) < 1e-9 ? 0.5 : va / (va - vb);
        positions.push(lo[0] + (ax + (bx - ax) * t) * cell, lo[1] + (ay + (by - ay) * t) * cell, lo[2] + (az + (bz - az) * t) * cell);
        vi = positions.length / 3 - 1;
        edgeCache.set(key, vi);
      }
      vertOnEdge[e] = vi;
    }
    const base = cubeIndex * 16;
    for (let t = 0; triTable[base + t] !== -1; t += 3) {
      // ordre inversé : normales vers l'extérieur (champ négatif = intérieur)
      indices.push(vertOnEdge[triTable[base + t]], vertOnEdge[triTable[base + t + 2]], vertOnEdge[triTable[base + t + 1]]);
    }
  }
  const pos = new Float32Array(positions);
  if (smooth) taubin(pos, indices, smooth);
  const g = new THREE.BufferGeometry();
  g.setAttribute("position", new THREE.BufferAttribute(pos, 3));
  g.setIndex(indices);
  g.computeVertexNormals();
  g.computeBoundingBox();
  g.computeBoundingSphere();
  return g;
}

/** Lissage de Taubin (ne rétrécit pas le volume). */
export function taubin(pos, indices, iterations = 2, lambda = 0.5, mu = -0.53) {
  const n = pos.length / 3;
  const nb = Array.from({ length: n }, () => new Set());
  for (let i = 0; i < indices.length; i += 3) {
    const a = indices[i], b = indices[i + 1], c = indices[i + 2];
    nb[a].add(b); nb[a].add(c); nb[b].add(a); nb[b].add(c); nb[c].add(a); nb[c].add(b);
  }
  const lists = nb.map((s) => [...s]);
  const tmp = new Float32Array(pos.length);
  for (let it = 0; it < iterations * 2; it++) {
    const f = it % 2 === 0 ? lambda : mu;
    for (let i = 0; i < n; i++) {
      const L = lists[i];
      if (!L.length) { tmp[i * 3] = pos[i * 3]; tmp[i * 3 + 1] = pos[i * 3 + 1]; tmp[i * 3 + 2] = pos[i * 3 + 2]; continue; }
      let sx = 0, sy = 0, sz = 0;
      for (const j of L) { sx += pos[j * 3]; sy += pos[j * 3 + 1]; sz += pos[j * 3 + 2]; }
      sx /= L.length; sy /= L.length; sz /= L.length;
      tmp[i * 3] = pos[i * 3] + f * (sx - pos[i * 3]);
      tmp[i * 3 + 1] = pos[i * 3 + 1] + f * (sy - pos[i * 3 + 1]);
      tmp[i * 3 + 2] = pos[i * 3 + 2] + f * (sz - pos[i * 3 + 2]);
    }
    pos.set(tmp);
  }
}

/** Poids de skinning : chaque sommet suit les deux os (capsules) les plus proches. */
export function skinWeights(geometry, capsules) {
  const p = geometry.attributes.position;
  const n = p.count;
  const si = new Uint16Array(n * 4), sw = new Float32Array(n * 4);
  for (let i = 0; i < n; i++) {
    const x = p.getX(i), y = p.getY(i), z = p.getZ(i);
    let b1 = 0, d1 = Infinity, b2 = 0, d2 = Infinity;
    const perBone = new Map();
    for (const c of capsules) {
      if (c.bone == null) continue;
      if (c.sub) continue;
      const d = primDist(x, y, z, c);
      const cur = perBone.get(c.bone);
      if (cur === undefined || d < cur) perBone.set(c.bone, d);
    }
    for (const [b, d] of perBone) {
      if (d < d1) { b2 = b1; d2 = d1; b1 = b; d1 = d; } else if (d < d2) { b2 = b; d2 = d; }
    }
    const e1 = Math.max(d1, 0) + 1e-3, e2 = Math.max(d2, 0) + 1e-3;
    let w1 = 1 / (e1 * e1), w2 = d2 === Infinity ? 0 : 1 / (e2 * e2);
    // atténue l'influence du second os s'il est loin
    if (d2 - d1 > 0.25) w2 *= 0.2;
    const s = w1 + w2;
    si[i * 4] = b1; si[i * 4 + 1] = b2;
    sw[i * 4] = w1 / s; sw[i * 4 + 1] = w2 / s;
  }
  geometry.setAttribute("skinIndex", new THREE.Uint16BufferAttribute(si, 4));
  geometry.setAttribute("skinWeight", new THREE.Float32BufferAttribute(sw, 4));
}
