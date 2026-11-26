// Auto-rigging d'un personnage statique (Tripo) : squelette humanoïde aux noms du jeu,
// poids de peau par distance aux segments osseux avec contraintes de région,
// séparation des objets indépendants (planètes…) qui seront animés à part.
// Sortie : <nom>.rig.json (os, positions de liaison) et <nom>.rig.bin (os/poids par sommet + étiquette d'objet).
import { NodeIO } from '@gltf-transform/core';
import fs from 'fs';
const [,, glb, landmarks, outBase] = process.argv;
const L = JSON.parse(fs.readFileSync(landmarks, 'utf8'));
const doc = await new NodeIO().read(glb);
const prim = doc.getRoot().listMeshes()[0].listPrimitives()[0];
const pos = prim.getAttribute('POSITION').getArray(), idx = prim.getIndices().getArray();
const n = pos.length / 3;

// ---- objets séparés (composantes connexes après soudure par position)
const key = (i) => `${Math.round(pos[i * 3] * 2e4)},${Math.round(pos[i * 3 + 1] * 2e4)},${Math.round(pos[i * 3 + 2] * 2e4)}`;
const weld = new Int32Array(n), wm = new Map();
for (let i = 0; i < n; i++) { const k = key(i); if (!wm.has(k)) wm.set(k, i); weld[i] = wm.get(k); }
const par = new Int32Array(n).map((_, i) => i);
const find = (a) => { while (par[a] !== a) { par[a] = par[par[a]]; a = par[a]; } return a; };
for (let t = 0; t < idx.length; t += 3) for (const o of [1, 2]) { const a = find(weld[idx[t]]), b = find(weld[idx[t + o]]); if (a !== b) par[a] = b; }
const counts = new Map(); for (let i = 0; i < n; i++) { const r = find(weld[i]); counts.set(r, (counts.get(r) || 0) + 1); }
const ranked = [...counts.entries()].sort((a, b) => b[1] - a[1]).map((e) => e[0]);
const label = new Uint8Array(n); const rank = new Map(ranked.map((r, i) => [r, i]));
for (let i = 0; i < n; i++) label[i] = Math.min(255, rank.get(find(weld[i])));
const body = (i) => label[i] === 0;

// ---- articulations : centrage dans le volume du membre
const J = {};
for (const [name, [x, y]] of Object.entries(L.joints)) {
  let z0 = L.zHint?.[name];
  const fixed = L.fixed.includes(name);
  let sx = 0, sy = 0, sz = 0, c = 0;
  const R = 0.03;
  for (let i = 0; i < n; i++) {
    if (!body(i)) continue;
    const dx = pos[i * 3] - x, dy = pos[i * 3 + 1] - y;
    if (dx * dx + dy * dy > R * R) continue;
    if (z0 !== undefined && Math.abs(pos[i * 3 + 2] - z0) > 0.06) continue;
    sx += pos[i * 3]; sy += pos[i * 3 + 1]; sz += pos[i * 3 + 2]; c++;
  }
  J[name] = c ? (fixed ? [x, y, sz / c] : [sx / c, sy / c, sz / c]) : [x, y, z0 ?? 0];
  if (!c) console.warn('articulation sans sommets proches :', name);
}
// colonne et tête alignées au-dessus du bassin
for (const k of ['spine', 'chest', 'neck', 'head', 'headTop']) J[k][0] = J.hips[0] * 0.5 + J[k][0] * 0.5;

// ---- squelette : [nom, parent, articulation de fin (direction de l'os)]
const BONES = [
  ['hips', null, 'spine'], ['spine', 'hips', 'chest'], ['chest', 'spine', 'neck'], ['neck', 'chest', 'head'], ['head', 'neck', 'headTop'],
  ['upperL', 'chest', 'foreL'], ['foreL', 'upperL', 'handL'], ['handL', 'foreL', 'handTipL'],
  ['upperR', 'chest', 'foreR'], ['foreR', 'upperR', 'handR'], ['handR', 'foreR', 'handTipR'],
  ['thighL', 'hips', 'shinL'], ['shinL', 'thighL', 'footL'], ['footL', 'shinL', 'toeL'],
  ['thighR', 'hips', 'shinR'], ['shinR', 'thighR', 'footR'], ['footR', 'shinR', 'toeR'],
];
const bi = new Map(BONES.map((b, i) => [b[0], i]));
const seg = BONES.map(([name, , end]) => [J[name], J[end]]);

// ---- poids de peau
const side = (x) => (x < J.hips[0] ? 'L' : 'R');
const joints = new Uint8Array(n * 4), weights = new Uint8Array(n * 4);
const dist = (p, a, b) => {
  const ab = [b[0] - a[0], b[1] - a[1], b[2] - a[2]], ap = [p[0] - a[0], p[1] - a[1], p[2] - a[2]];
  const L2 = ab[0] ** 2 + ab[1] ** 2 + ab[2] ** 2 || 1e-9;
  const t = Math.max(0, Math.min(1, (ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / L2));
  return Math.hypot(ap[0] - ab[0] * t, ap[1] - ab[1] * t, ap[2] - ab[2] * t);
};
const shoulderX = Math.abs(J.upperR[0] - J.hips[0]) * 0.85;
for (let i = 0; i < n; i++) {
  const p = [pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]];
  if (!body(i)) { joints[i * 4] = bi.get('hips'); weights[i * 4] = 255; continue; }
  const s = side(p[0]);
  const dxAbs = Math.abs(p[0] - J.hips[0]);
  let allowed;
  if (p[1] > J.neck[1] + 0.01 && dxAbs < 0.12) allowed = ['neck', 'head'];
  else if (dxAbs > shoulderX && p[1] > J.chest[1] - 0.06) allowed = ['chest', 'upper' + s, 'fore' + s, 'hand' + s];
  else if (p[1] < J.hips[1] - 0.02) allowed = ['hips', 'thigh' + s, 'shin' + s, 'foot' + s];
  else allowed = ['hips', 'spine', 'chest', 'neck', 'upper' + s, 'thigh' + s];
  const cand = allowed.map((b) => { const k = bi.get(b); const d = dist(p, seg[k][0], seg[k][1]); return [k, Math.exp(-((d / 0.028) ** 2)) + 1e-6 / (d + 1e-3)]; });
  cand.sort((a, b) => b[1] - a[1]);
  const top = cand.slice(0, 4), sum = top.reduce((a, c) => a + c[1], 0);
  let acc = 0;
  top.forEach(([k, w], j) => { const q = j === top.length - 1 ? 255 - acc : Math.round(w / sum * 255); joints[i * 4 + j] = k; weights[i * 4 + j] = Math.max(0, q); acc += Math.max(0, q); });
}
// ---- objets séparés : centre et taille (animés en orbite par le jeu)
const objects = [];
for (let r = 1; r < Math.min(ranked.length, 255); r++) {
  let mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9];
  for (let i = 0; i < n; i++) if (label[i] === r) for (let a = 0; a < 3; a++) { mn[a] = Math.min(mn[a], pos[i * 3 + a]); mx[a] = Math.max(mx[a], pos[i * 3 + a]); }
  objects.push({ label: r, center: mn.map((v, a) => (v + mx[a]) / 2), size: mx.map((v, a) => v - mn[a]) });
}
const rig = { vertices: n, bones: BONES.map(([name, parent]) => ({ name, parent, pos: J[name] })), joints: J, objects, height: L.height };
fs.writeFileSync(outBase + '.rig.json', JSON.stringify(rig));
const bin = new Uint8Array(n * 9); bin.set(joints, 0); bin.set(weights, n * 4); bin.set(label, n * 8);
fs.writeFileSync(outBase + '.rig.bin', bin);
console.log('os', BONES.length, 'sommets', n, 'objets', objects.length);
for (const [k, v] of Object.entries(J)) console.log(k.padEnd(9), v.map((x) => x.toFixed(3)).join(', '));
