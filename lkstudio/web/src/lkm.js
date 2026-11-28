// LK Studio — 3D import for LOST KINGDOM: GLB/GLTF scene -> one textured mesh -> auto-rig on the game skeleton -> .lkm
// The rig follows the game's own tools (lostkingdom/tools/import_character.py, import_quadruped.py, import_prop.py):
// same bones and hierarchy, joints found from the mesh, two bones per vertex, weights spread ALONG the surface
// (Dijkstra on the mesh graph) so capes and skirts follow the body, never the legs.
import * as THREE from 'three';
import { MeshoptSimplifier } from 'meshoptimizer/meshopt_simplifier.module.js';
import GAME from './gamedata.json';

export const HUMAN_NAMES = ['root', 'spine', 'chest', 'neck', 'head', 'shL', 'uarmL', 'larmL', 'handL', 'shR', 'uarmR', 'larmR', 'handR',
  'thighL', 'shinL', 'footL', 'thighR', 'shinR', 'footR', 'propR', 'propL'];
export const HUMAN_PARENT = [-1, 0, 1, 2, 3, 2, 5, 6, 7, 2, 9, 10, 11, 0, 13, 14, 0, 16, 17, 12, 8];
export const QUAD_NAMES = ['root', 'spine', 'chest', 'neck', 'head', 'jaw', 'tail1', 'tail2', 'tail3',
  'fl1', 'fl2', 'fl3', 'fr1', 'fr2', 'fr3', 'bl1', 'bl2', 'bl3', 'br1', 'br2', 'br3'];
export const QUAD_PARENT = [-1, 0, 1, 2, 3, 4, 0, 6, 7, 2, 9, 10, 2, 12, 13, 0, 15, 16, 0, 18, 19];

// ------------------------------------------------------------------------------------------ mesh extraction
// Bakes every mesh of the scene (world transforms) into one geometry; up to 4 base-colour textures are packed into a
// 2 x 2 atlas (UVs remapped), meshes without texture get their material colour as a small swatch in the atlas.
export async function extractMesh(scene, texSize = 1024) {
  scene.updateMatrixWorld(true);
  const parts = [];
  scene.traverse(o => {
    if (!o.isMesh || !o.geometry || !o.geometry.attributes.position) return;
    const g = o.geometry.index ? o.geometry : o.geometry.clone().setIndex([...Array(o.geometry.attributes.position.count).keys()]);
    const mat = Array.isArray(o.material) ? o.material[0] : o.material;
    parts.push({ g, m: o.matrixWorld.clone(), mat, skinned: o.isSkinnedMesh });
  });
  if (!parts.length) throw new Error('Aucun maillage dans ce fichier.');
  // distinct "skins": a texture, or a flat colour
  const skins = [];
  const skinOf = mat => {
    const key = mat && mat.map ? mat.map.uuid : 'c' + (mat && mat.color ? mat.color.getHexString() : 'cccccc');
    let i = skins.findIndex(s => s.key === key);
    if (i < 0) { skins.push({ key, map: mat && mat.map, color: mat && mat.color ? mat.color.clone() : new THREE.Color(0.8, 0.8, 0.8) }); i = skins.length - 1; }
    return i;
  };
  const P = [], N = [], UV = [], I = [];
  let base = 0;
  for (const p of parts) {
    const pos = p.g.attributes.position, nor = p.g.attributes.normal, uv = p.g.attributes.uv;
    const nm = new THREE.Matrix3().getNormalMatrix(p.m);
    const s = skinOf(p.mat);
    const v = new THREE.Vector3(), n = new THREE.Vector3();
    for (let i = 0; i < pos.count; i++) {
      v.fromBufferAttribute(pos, i).applyMatrix4(p.m); P.push(v.x, v.y, v.z);
      if (nor) { n.fromBufferAttribute(nor, i).applyMatrix3(nm).normalize(); N.push(n.x, n.y, n.z); } else N.push(0, 1, 0);
      let u = 0.5, w = 0.5;
      if (uv && skins[s].map) { u = uv.getX(i); w = uv.getY(i); u -= Math.floor(u); w -= Math.floor(w); }
      UV.push(u, w, s);
    }
    const idx = p.g.index;
    for (let i = 0; i < idx.count; i++) I.push(idx.getX(i) + base);
    base += pos.count;
  }
  // triangles wound against their normals (some exporters) would be shaded from the back in the game: turn them over
  for (let t = 0; t < I.length; t += 3) {
    const a = I[t] * 3, b = I[t + 1] * 3, c = I[t + 2] * 3;
    const ux = P[b] - P[a], uy = P[b + 1] - P[a + 1], uz = P[b + 2] - P[a + 2], vx = P[c] - P[a], vy = P[c + 1] - P[a + 1], vz = P[c + 2] - P[a + 2];
    const cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
    const nx = N[a] + N[b] + N[c], ny = N[a + 1] + N[b + 1] + N[c + 1], nz = N[a + 2] + N[b + 2] + N[c + 2];
    if (cx * nx + cy * ny + cz * nz < 0) { const k = I[t + 1]; I[t + 1] = I[t + 2]; I[t + 2] = k; }
  }
  // atlas
  const n = Math.min(skins.length, 4), grid = n > 1 ? 2 : 1;
  const canvas = document.createElement('canvas'); canvas.width = canvas.height = texSize;
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#c8c8c8'; ctx.fillRect(0, 0, texSize, texSize);
  const cell = texSize / grid;
  for (let k = 0; k < n; k++) {
    const sk = skins[k], cx = (k % grid) * cell, cy = Math.floor(k / grid) * cell;
    if (sk.map && sk.map.image) {
      try { ctx.drawImage(sk.map.image, cx, cy, cell, cell); } catch (e) { ctx.fillStyle = '#' + sk.color.getHexString(); ctx.fillRect(cx, cy, cell, cell); }
    } else { ctx.fillStyle = '#' + sk.color.getHexString(); ctx.fillRect(cx, cy, cell, cell); }
  }
  const uv2 = new Float32Array(P.length / 3 * 2);
  for (let i = 0; i < P.length / 3; i++) {
    const s = Math.min(UV[i * 3 + 2], n - 1), cx = s % grid, cy = Math.floor(s / grid);
    // glTF: v goes down the image (flipY false), like the game's own tools write it: kept as is, moved into its atlas cell
    uv2[i * 2] = (cx + UV[i * 3]) / grid;
    uv2[i * 2 + 1] = (cy + UV[i * 3 + 1]) / grid;
  }
  return { P: new Float32Array(P), N: new Float32Array(N), UV: uv2, I: new Uint32Array(I), canvas, textures: skins.length };
}

// ------------------------------------------------------------------------------------------ simplification
export async function simplify(mesh, targetTris) {
  const tris = mesh.I.length / 3;
  if (tris <= targetTris) return mesh;
  await MeshoptSimplifier.ready;
  // weld positions first (UV seams split vertices; the simplifier keeps the seams with the attribute-aware variant)
  const nv = mesh.P.length / 3;
  const attrs = new Float32Array(nv * 2);
  for (let i = 0; i < nv; i++) { attrs[i * 2] = mesh.UV[i * 2]; attrs[i * 2 + 1] = mesh.UV[i * 2 + 1]; }
  let out;
  if (MeshoptSimplifier.simplifyWithAttributes) {
    [out] = MeshoptSimplifier.simplifyWithAttributes(mesh.I, mesh.P, 3, attrs, 2, [0.5, 0.5], null, targetTris * 3, 0.05);
  } else {
    [out] = MeshoptSimplifier.simplify(mesh.I, mesh.P, 3, targetTris * 3, 0.05);
  }
  // compact
  const remap = new Int32Array(nv).fill(-1);
  const P = [], N = [], UV = [], I = new Uint32Array(out.length);
  let k = 0;
  for (let i = 0; i < out.length; i++) {
    const v = out[i];
    if (remap[v] < 0) { remap[v] = k++; P.push(mesh.P[v * 3], mesh.P[v * 3 + 1], mesh.P[v * 3 + 2]); N.push(mesh.N[v * 3], mesh.N[v * 3 + 1], mesh.N[v * 3 + 2]); UV.push(mesh.UV[v * 2], mesh.UV[v * 2 + 1]); }
    I[i] = remap[v];
  }
  return { ...mesh, P: new Float32Array(P), N: new Float32Array(N), UV: new Float32Array(UV), I };
}

// ------------------------------------------------------------------------------------------ orientation
// turn the mesh (around Y, by quarter turns) and normalise: feet on the ground at y = 0, centred on x / z
export function orient(mesh, quarterTurns) {
  const P = mesh.P.slice(), N = mesh.N.slice();
  const a = quarterTurns * Math.PI / 2, c = Math.cos(a), s = Math.sin(a);
  for (let i = 0; i < P.length; i += 3) {
    const x = P[i], z = P[i + 2]; P[i] = c * x + s * z; P[i + 2] = -s * x + c * z;
    const nx = N[i], nz = N[i + 2]; N[i] = c * nx + s * nz; N[i + 2] = -s * nx + c * nz;
  }
  const lo = [1e9, 1e9, 1e9], hi = [-1e9, -1e9, -1e9];
  for (let i = 0; i < P.length; i += 3) for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], P[i + k]); hi[k] = Math.max(hi[k], P[i + k]); }
  for (let i = 0; i < P.length; i += 3) { P[i] -= (lo[0] + hi[0]) / 2; P[i + 1] -= lo[1]; P[i + 2] -= (lo[2] + hi[2]) / 2; }
  return { ...mesh, P, N, size: [hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]] };
}

export function scaleMesh(mesh, k) { const P = mesh.P.map(v => v * k); return { ...mesh, P }; }

// ------------------------------------------------------------------------------------------ helpers
function segDist(px, py, pz, a, b) {
  const abx = b[0] - a[0], aby = b[1] - a[1], abz = b[2] - a[2];
  const L = abx * abx + aby * aby + abz * abz;
  let t = L > 1e-9 ? ((px - a[0]) * abx + (py - a[1]) * aby + (pz - a[2]) * abz) / L : 0;
  t = Math.max(0, Math.min(1, t));
  const dx = px - (a[0] + abx * t), dy = py - (a[1] + aby * t), dz = pz - (a[2] + abz * t);
  return Math.sqrt(dx * dx + dy * dy + dz * dz);
}

// binary heap for Dijkstra
class Heap {
  constructor() { this.k = []; this.v = []; }
  push(key, val) { const k = this.k, v = this.v; let i = k.length; k.push(key); v.push(val); while (i > 0) { const p = (i - 1) >> 1; if (k[p] <= key) break; k[i] = k[p]; v[i] = v[p]; i = p; } k[i] = key; v[i] = val; }
  pop() { const k = this.k, v = this.v; const rk = k[0], rv = v[0]; const lk = k.pop(), lv = v.pop(); if (k.length) { let i = 0; const n = k.length; while (true) { let c = 2 * i + 1; if (c >= n) break; if (c + 1 < n && k[c + 1] < k[c]) c++; if (k[c] >= lk) break; k[i] = k[c]; v[i] = v[c]; i = c; } k[i] = lk; v[i] = lv; } return [rk, rv]; }
  get size() { return this.k.length; }
}

// welded vertex graph of the surface
function buildGraph(P, I) {
  const nv = P.length / 3, weld = new Int32Array(nv), map = new Map();
  let nW = 0;
  for (let i = 0; i < nv; i++) {
    const key = Math.round(P[i * 3] * 1e4) + ',' + Math.round(P[i * 3 + 1] * 1e4) + ',' + Math.round(P[i * 3 + 2] * 1e4);
    let w = map.get(key); if (w === undefined) { w = nW++; map.set(key, w); } weld[i] = w;
  }
  const Pw = new Float32Array(nW * 3);
  for (let i = 0; i < nv; i++) { Pw[weld[i] * 3] = P[i * 3]; Pw[weld[i] * 3 + 1] = P[i * 3 + 1]; Pw[weld[i] * 3 + 2] = P[i * 3 + 2]; }
  const adj = Array.from({ length: nW }, () => []);
  const add = (a, b) => { if (a === b) return; const d = Math.hypot(Pw[a * 3] - Pw[b * 3], Pw[a * 3 + 1] - Pw[b * 3 + 1], Pw[a * 3 + 2] - Pw[b * 3 + 2]); adj[a].push(b, d); adj[b].push(a, d); };
  for (let t = 0; t < I.length; t += 3) { const a = weld[I[t]], b = weld[I[t + 1]], c = weld[I[t + 2]]; add(a, b); add(b, c); add(c, a); }
  return { weld, nW, adj };
}

function dijkstra(graph, seeds, allowed) {
  const dist = new Float32Array(graph.nW).fill(Infinity);
  const h = new Heap();
  for (const s of seeds) { dist[s] = 0; h.push(0, s); }
  while (h.size) {
    const [d, u] = h.pop();
    if (d > dist[u]) continue;
    const a = graph.adj[u];
    for (let i = 0; i < a.length; i += 2) { const v = a[i]; if (!allowed[v]) continue; const nd = d + a[i + 1]; if (nd < dist[v]) { dist[v] = nd; h.push(nd, v); } }
  }
  return dist;
}

// weights from region-masked distances D[v * nb + b] (BIG = forbidden), spread along the surface
function skinFromDistances(P, I, D, nb, bones, H, seedRadius, blendW) {
  const BIG = 1e8, nv = P.length / 3;
  const g = buildGraph(P, I);
  const Dw = new Float32Array(g.nW * nb).fill(BIG);
  for (let v = 0; v < nv; v++) { const w = g.weld[v]; for (let b = 0; b < nb; b++) Dw[w * nb + b] = Math.min(Dw[w * nb + b], D[v * nb + b]); }
  const nearest = new Int32Array(g.nW), dnear = new Float32Array(g.nW);
  for (let w = 0; w < g.nW; w++) { let bi = 0, bd = BIG; for (let b = 0; b < nb; b++) if (Dw[w * nb + b] < bd) { bd = Dw[w * nb + b]; bi = b; } nearest[w] = bi; dnear[w] = bd; }
  const G = new Float32Array(g.nW * nb).fill(Infinity);
  for (const b of bones) {
    let seeds = [];
    const r = seedRadius(b);
    for (let w = 0; w < g.nW; w++) if (nearest[w] === b && dnear[w] < r) seeds.push(w);
    if (!seeds.length) for (let w = 0; w < g.nW; w++) if (nearest[w] === b) { seeds.push(w); break; }
    if (!seeds.length) continue;
    const allowed = new Uint8Array(g.nW); for (let w = 0; w < g.nW; w++) allowed[w] = Dw[w * nb + b] < BIG / 2 ? 1 : 0;
    const d = dijkstra(g, seeds, allowed);
    for (let w = 0; w < g.nW; w++) G[w * nb + b] = Dw[w * nb + b] >= BIG / 2 ? Infinity : d[w];
  }
  const b0 = new Uint8Array(nv), b1 = new Uint8Array(nv), w0 = new Float32Array(nv);
  for (let v = 0; v < nv; v++) {
    const w = g.weld[v];
    let f = -1, fd = Infinity, s = -1, sd = Infinity;
    for (let b = 0; b < nb; b++) { const d = G[w * nb + b]; if (d < fd) { s = f; sd = fd; f = b; fd = d; } else if (d < sd) { s = b; sd = d; } }
    if (f < 0) {   // no surface path: Euclidean
      for (let b = 0; b < nb; b++) { const d = Dw[w * nb + b]; if (d < fd) { s = f; sd = fd; f = b; fd = d; } else if (d < sd) { s = b; sd = d; } }
    }
    const w1 = isFinite(sd) && s >= 0 ? 0.5 * Math.max(0, Math.min(1, 1 - (sd - fd) / blendW)) : 0;
    b0[v] = Math.max(0, f); b1[v] = s >= 0 && isFinite(sd) ? s : Math.max(0, f); w0[v] = 1 - w1;
  }
  return { b0, b1, w0 };
}

// ------------------------------------------------------------------------------------------ humanoid (T-pose)
export function rigHumanoid(mesh, height = 1.8) {
  let { P, N, I } = mesh;
  P = P.slice(); N = N.slice();
  let minY = 1e9, maxY = -1e9;
  for (let i = 1; i < P.length; i += 3) { minY = Math.min(minY, P[i]); maxY = Math.max(maxY, P[i]); }
  const k = height / Math.max(1e-6, maxY - minY);
  for (let i = 0; i < P.length; i += 3) { P[i] *= k; P[i + 1] = (P[i + 1] - minY) * k; P[i + 2] *= k; }
  const H = height, nv = P.length / 3;
  const x = i => P[i * 3], y = i => P[i * 3 + 1], z = i => P[i * 3 + 2];
  // centre: x on the bounding box (a T-pose is symmetric), z on the legs
  let lo = 1e9, hi = -1e9; const zs = [];
  for (let i = 0; i < nv; i++) { lo = Math.min(lo, x(i)); hi = Math.max(hi, x(i)); if (y(i) > 0.05 * H && y(i) < 0.22 * H) zs.push(z(i)); }
  zs.sort((a, b) => a - b); const zMed = zs.length ? zs[zs.length >> 1] : 0;
  for (let i = 0; i < nv; i++) { P[i * 3] -= (lo + hi) / 2; P[i * 3 + 2] -= zMed; }
  const median = a => { if (!a.length) return 0; a.sort((p, q) => p - q); return a[a.length >> 1]; };
  let maxX = 0; for (let i = 0; i < nv; i++) maxX = Math.max(maxX, Math.abs(x(i)));
  const tpose = maxX > 0.3 * H;
  const armY = [], armZ = []; for (let i = 0; i < nv; i++) if (Math.abs(x(i)) > 0.55 * maxX) { armY.push(y(i)); armZ.push(z(i)); }
  const yArm = tpose ? median(armY) : 0.8 * H, zArm = median(armZ);
  let xEdge = maxX;
  for (let xs = 0.05 * H; xs < maxX; xs += 0.01) { let any = false, low = 1e9; for (let i = 0; i < nv; i++) { const ax = Math.abs(x(i)); if (ax >= xs && ax < xs + 0.02) { any = true; low = Math.min(low, y(i)); } } if (any && low > yArm - 0.12 * H) { xEdge = xs; break; } }
  const chest = []; for (let i = 0; i < nv; i++) if (y(i) > yArm - 0.17 * H && y(i) < yArm - 0.07 * H) chest.push(Math.abs(x(i)));
  chest.sort((a, b) => a - b);
  const p95 = chest.length ? chest[Math.floor(chest.length * 0.95)] : 0.15 * H;
  const shoulderX = Math.min(xEdge * 0.85, p95 + 0.02 * H);
  const wristX = tpose ? maxX - 0.085 * H : shoulderX + 0.05 * H, elbowX = shoulderX + 0.5 * (wristX - shoulderX);
  const lx = [], lz = []; for (let i = 0; i < nv; i++) if (y(i) > 0.08 * H && y(i) < 0.3 * H) { lx.push(Math.abs(x(i))); lz.push(z(i)); }
  const legX = Math.max(0.04 * H, median(lx)), legZ = median(lz);
  const hipY = 0.51 * H, kneeY = 0.27 * H, ankleY = 0.06 * H, rootY = hipY + 0.02 * H, bodyZ = 0.5 * zArm + 0.5 * legZ;
  const B = Object.fromEntries(HUMAN_NAMES.map((n, i) => [n, i]));
  const J = HUMAN_NAMES.map(() => [0, 0, 0]);
  J[B.root] = [0, rootY, bodyZ]; J[B.spine] = [0, rootY + 0.065 * H, bodyZ]; J[B.chest] = [0, rootY + 0.17 * H, bodyZ];
  J[B.neck] = [0, Math.max(yArm + 0.05 * H, rootY + 0.28 * H), bodyZ - 0.01 * H]; J[B.head] = [0, J[B.neck][1] + 0.055 * H, bodyZ];
  for (const [s, sd] of [[1, 'L'], [-1, 'R']]) {
    J[B['sh' + sd]] = [s * 0.045 * H, yArm + 0.02 * H, zArm * 0.5 + bodyZ * 0.5];
    J[B['uarm' + sd]] = [s * shoulderX, yArm, zArm]; J[B['larm' + sd]] = [s * elbowX, yArm, zArm]; J[B['hand' + sd]] = [s * wristX, yArm, zArm];
    J[B['thigh' + sd]] = [s * legX * 0.9, hipY, legZ]; J[B['shin' + sd]] = [s * legX, kneeY, legZ]; J[B['foot' + sd]] = [s * legX, ankleY, legZ - 0.01 * H];
  }
  J[B.propL] = [J[B.handL][0] + 0.04 * H, J[B.handL][1], J[B.handL][2] + 0.01 * H];
  J[B.propR] = [J[B.handR][0] - 0.04 * H, J[B.handR][1], J[B.handR][2] + 0.01 * H];
  const child = { root: 'spine', spine: 'chest', chest: 'neck', neck: 'head', shL: 'uarmL', uarmL: 'larmL', larmL: 'handL', shR: 'uarmR', uarmR: 'larmR', larmR: 'handR', thighL: 'shinL', shinL: 'footL', thighR: 'shinR', shinR: 'footR' };
  const tips = { head: [J[B.head][0], J[B.head][1] + 0.12 * H, J[B.head][2]], handL: [maxX, yArm, zArm], handR: [-maxX, yArm, zArm],
    footL: [J[B.footL][0], J[B.footL][1] - 0.04 * H, J[B.footL][2] + 0.09 * H], footR: [J[B.footR][0], J[B.footR][1] - 0.04 * H, J[B.footR][2] + 0.09 * H] };
  const nb = 21, BIG = 1e9, D = new Float32Array(nv * nb).fill(BIG);
  const skin = HUMAN_NAMES.filter(n => n !== 'propL' && n !== 'propR');
  for (const n of skin) { const a = J[B[n]], e = child[n] ? J[B[child[n]]] : tips[n]; for (let i = 0; i < nv; i++) D[i * nb + B[n]] = segDist(x(i), y(i), z(i), a, e); }
  const armL = ['shL', 'uarmL', 'larmL', 'handL'], armR = ['shR', 'uarmR', 'larmR', 'handR'], legL = ['thighL', 'shinL', 'footL'], legR = ['thighR', 'shinR', 'footR'];
  const mask = (cond, names) => { for (let i = 0; i < nv; i++) if (cond(i)) for (const n of names) D[i * nb + B[n]] = BIG; };
  mask(i => x(i) <= 0, [...armL, ...legL]); mask(i => x(i) > 0, [...armR, ...legR]);
  const armHeight = i => y(i) > yArm - 0.2 * H;
  const inArm = i => Math.abs(x(i)) > shoulderX + 0.02 * H && armHeight(i);
  if (tpose) {
    mask(inArm, ['root', 'spine', 'neck', 'head', ...legL, ...legR]);
    mask(i => Math.abs(x(i)) < shoulderX - 0.03 * H || !armHeight(i), ['larmL', 'handL', 'larmR', 'handR']);
    mask(i => Math.abs(x(i)) < shoulderX - 0.07 * H, ['uarmL', 'uarmR']);
  }
  mask(i => y(i) < hipY - 0.04 * H, ['chest', 'neck', 'head', ...armL, ...armR]);
  mask(i => y(i) > rootY + 0.06 * H, [...legL, ...legR]);
  const cape = i => z(i) < bodyZ - 0.075 * H && y(i) > kneeY - 0.05 * H && !inArm(i);
  mask(cape, [...legL, ...legR, ...armL.slice(1), ...armR.slice(1)]);
  for (let i = 0; i < nv; i++) { let all = true; for (let b = 0; b < nb; b++) if (D[i * nb + b] < BIG / 2) { all = false; break; }
    if (all) for (const n of skin) { const a = J[B[n]], e = child[n] ? J[B[child[n]]] : tips[n]; D[i * nb + B[n]] = segDist(x(i), y(i), z(i), a, e); } }
  const limb = new Set([...armL, ...armR, ...legL, ...legR].filter(n => n !== 'shL' && n !== 'shR').map(n => B[n]));
  const { b0, b1, w0 } = skinFromDistances(P, I, D, nb, skin.map(n => B[n]), H,
    b => limb.has(b) ? 0.045 * H : (b === B.shL || b === B.shR ? 0.07 * H : 0.13 * H), 0.05 * H);
  // T-pose -> rest pose: arms down
  const Jr = J.map(j => j.slice());
  if (tpose) {
    const ang = Math.PI / 2 - 0.08;
    const rot = (p, piv, a) => { const c = Math.cos(a), s = Math.sin(a), dx = p[0] - piv[0], dy = p[1] - piv[1]; return [piv[0] + c * dx - s * dy, piv[1] + s * dx + c * dy, p[2]]; };
    const side = b => { const n = HUMAN_NAMES[b]; if (['uarmL', 'larmL', 'handL', 'propL'].includes(n)) return -ang; if (['uarmR', 'larmR', 'handR', 'propR'].includes(n)) return ang; return 0; };
    const pivot = b => HUMAN_NAMES[b].endsWith('L') ? J[B.uarmL] : J[B.uarmR];
    const P2 = P.slice(), N2 = N.slice();
    for (let i = 0; i < nv; i++) {
      const p = [x(i), y(i), z(i)], nn = [N[i * 3], N[i * 3 + 1], N[i * 3 + 2]];
      let acc = [0, 0, 0], an = [0, 0, 0];
      for (const [b, w] of [[b0[i], w0[i]], [b1[i], 1 - w0[i]]]) {
        const a = side(b); let q = p, m = nn;
        if (a) { q = rot(p, pivot(b), a); const c = Math.cos(a), s = Math.sin(a); m = [c * nn[0] - s * nn[1], s * nn[0] + c * nn[1], nn[2]]; }
        for (let k2 = 0; k2 < 3; k2++) { acc[k2] += q[k2] * w; an[k2] += m[k2] * w; }
      }
      const L = Math.hypot(an[0], an[1], an[2]) || 1;
      P2[i * 3] = acc[0]; P2[i * 3 + 1] = acc[1]; P2[i * 3 + 2] = acc[2]; N2[i * 3] = an[0] / L; N2[i * 3 + 1] = an[1] / L; N2[i * 3 + 2] = an[2] / L;
    }
    for (let b = 0; b < 21; b++) { const a = side(b); if (a) Jr[b] = rot(J[b], pivot(b), a); }
    P = P2; N = N2;
  }
  return { ...mesh, P, N, joints: Jr, b0, b1, w0, height: H, kind: 'humanoid', tpose };
}

// ------------------------------------------------------------------------------------------ quadruped (standing, head towards +Z)
// The game skeleton (unit wolf) is stretched onto the animal's bounding box; legs are told apart by quadrant.
export function rigQuadruped(mesh) {
  let { P, N, I } = mesh;
  const sk = GAME.quadSkel;
  const nv = P.length / 3;
  let lo = [1e9, 1e9, 1e9], hi = [-1e9, -1e9, -1e9];
  for (let i = 0; i < nv; i++) for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], P[i * 3 + k]); hi[k] = Math.max(hi[k], P[i * 3 + k]); }
  // the game skeleton's own extent (tail tip to nose, feet to back)
  const skLen = 0.96 + 0.86, skH = 0.95;   // z from -0.96 (tail3) to ~0.86 (snout); y from 0 to ~0.95 (head)
  const L = hi[2] - lo[2], Hm = hi[1] - lo[1];
  // scale so the model's height matches the skeleton's (the game scales the creature as a whole)
  const k = skH / Math.max(1e-6, Hm);
  P = P.slice(); for (let i = 0; i < nv; i++) { P[i * 3] *= k; P[i * 3 + 1] = (P[i * 3 + 1] - lo[1]) * k; P[i * 3 + 2] = (P[i * 3 + 2] - (lo[2] + hi[2]) / 2) * k; }
  const zl = L * k, zs = zl / skLen;
  const J = sk.map(b => [b[0] * Math.max(0.6, (hi[0] - lo[0]) * k / 0.4), b[1], b[2] * zs + (zs - 1) * 0.05]);
  const nb = 21, BIG = 1e9, D = new Float32Array(nv * nb).fill(BIG);
  const child = { 0: 1, 1: 2, 2: 3, 3: 4, 4: 5, 6: 7, 7: 8, 9: 10, 10: 11, 12: 13, 13: 14, 15: 16, 16: 17, 18: 19, 19: 20 };
  const tip = b => b === 5 ? [J[5][0], J[5][1] - 0.03, J[5][2] + 0.15 * zs] : b === 8 ? [J[8][0], J[8][1] - 0.1, J[8][2] - 0.15 * zs] : [J[b][0], 0, J[b][2] + 0.02];
  for (let b = 0; b < nb; b++) { const a = J[b], e = child[b] !== undefined ? J[child[b]] : tip(b); for (let i = 0; i < nv; i++) D[i * nb + b] = segDist(P[i * 3], P[i * 3 + 1], P[i * 3 + 2], a, e); }
  const bellyY = Math.min(J[9][1], J[15][1]) - 0.08, zMid = (J[9][2] + J[15][2]) / 2;
  const legs = { fl: [9, 10, 11], fr: [12, 13, 14], bl: [15, 16, 17], br: [18, 19, 20] };
  for (let i = 0; i < nv; i++) {
    const X = P[i * 3], Y = P[i * 3 + 1], Z = P[i * 3 + 2], low = Y < bellyY;
    for (const [key, bones] of Object.entries(legs)) {
      const mine = low && ((Z > zMid) === (key[0] === 'f')) && ((X > 0) === (key[1] === 'l'));
      if (low && !mine) for (const b of bones) D[i * nb + b] = BIG;
      if (!low) for (const b of bones.slice(1)) D[i * nb + b] = BIG;
    }
    if (low) for (const b of [0, 1, 2, 3, 4, 5]) D[i * nb + b] = BIG;
    if (Z > J[6][2] - 0.04) for (const b of [6, 7, 8]) D[i * nb + b] = BIG;
  }
  for (let i = 0; i < nv; i++) { let all = true; for (let b = 0; b < nb; b++) if (D[i * nb + b] < BIG / 2) { all = false; break; }
    if (all) for (let b = 0; b < nb; b++) D[i * nb + b] = segDist(P[i * 3], P[i * 3 + 1], P[i * 3 + 2], J[b], child[b] !== undefined ? J[child[b]] : tip(b)); }
  const body = new Set([0, 1, 2, 3, 4, 5]);
  const { b0, b1, w0 } = skinFromDistances(P, I, D, nb, [...Array(nb).keys()], skH, b => body.has(b) ? 0.14 * skH : 0.05 * skH, 0.06 * skH);
  return { ...mesh, P, N, joints: J, b0, b1, w0, height: skH, kind: 'quadruped' };
}

// ------------------------------------------------------------------------------------------ static prop (buildings)
export function rigProp(mesh) {
  const nv = mesh.P.length / 3;
  let hy = 0; for (let i = 1; i < mesh.P.length; i += 3) hy = Math.max(hy, mesh.P[i]);
  return { ...mesh, joints: [[0, 0, 0]], b0: new Uint8Array(nv), b1: new Uint8Array(nv), w0: new Float32Array(nv).fill(1), height: hy, kind: 'prop' };
}

// ------------------------------------------------------------------------------------------ .lkm writer (LKM2)
export function writeLKM(r, clothAll = true) {
  const nv = r.P.length / 3, ni = r.I.length, nj = r.joints.length;
  if (nv > 200000 || ni > 1200000) throw new Error('Modèle trop lourd (limite : 200 000 sommets). Réduisez le nombre de triangles.');
  const size = 4 + 16 + nj * 12 + nv * (12 + 3 + 4 + 4) + ni * 4;
  const buf = new ArrayBuffer(size), dv = new DataView(buf);
  let o = 0;
  for (const ch of 'LKM2') dv.setUint8(o++, ch.charCodeAt(0));
  dv.setUint32(o, nv, true); o += 4; dv.setUint32(o, ni, true); o += 4; dv.setUint32(o, nj, true); o += 4; dv.setFloat32(o, r.height, true); o += 4;
  for (const j of r.joints) { dv.setFloat32(o, j[0], true); dv.setFloat32(o + 4, j[1], true); dv.setFloat32(o + 8, j[2], true); o += 12; }
  for (let i = 0; i < nv * 3; i++) { dv.setFloat32(o, r.P[i], true); o += 4; }
  for (let i = 0; i < nv * 3; i++) { dv.setInt8(o, Math.max(-127, Math.min(127, Math.round(r.N[i] * 127)))); o++; }
  for (let i = 0; i < nv * 2; i++) { const u = Math.max(0, Math.min(1, r.UV[i])); dv.setUint16(o, Math.round(u * 65535), true); o += 2; }
  for (let i = 0; i < nv; i++) { dv.setUint8(o++, r.b0[i]); dv.setUint8(o++, r.b1[i]); dv.setUint8(o++, Math.round(r.w0[i] * 255)); dv.setUint8(o++, clothAll ? 255 : 0); }
  for (let i = 0; i < ni; i++) { dv.setUint32(o, r.I[i], true); o += 4; }
  return new Uint8Array(buf);
}

export function canvasToJpeg(canvas, size = 1024, quality = 0.88) {
  let c = canvas;
  if (canvas.width !== size) { c = document.createElement('canvas'); c.width = c.height = size; c.getContext('2d').drawImage(canvas, 0, 0, size, size); }
  const url = c.toDataURL('image/jpeg', quality);
  const bin = atob(url.split(',')[1]); const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}
