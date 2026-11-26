// Analyse d'un modèle Tripo (maillage unique, sans squelette) : composantes connexes
// (corps / objets séparés comme des planètes) et profil de la silhouette pour placer les articulations.
import { NodeIO } from '@gltf-transform/core';
import fs from 'fs';
const [,, file, out] = process.argv;
const io = new NodeIO();
const doc = await io.read(file);
const prim = doc.getRoot().listMeshes()[0].listPrimitives()[0];
const pos = prim.getAttribute('POSITION').getArray();
const idx = prim.getIndices().getArray();
const n = pos.length / 3;
// soudure par position (les coutures UV séparent les sommets)
const key = (i) => `${Math.round(pos[i * 3] * 2e4)},${Math.round(pos[i * 3 + 1] * 2e4)},${Math.round(pos[i * 3 + 2] * 2e4)}`;
const weld = new Int32Array(n); const map = new Map();
for (let i = 0; i < n; i++) { const k = key(i); if (!map.has(k)) map.set(k, i); weld[i] = map.get(k); }
const parent = new Int32Array(n).map((_, i) => i);
const find = (a) => { while (parent[a] !== a) { parent[a] = parent[parent[a]]; a = parent[a]; } return a; };
const uni = (a, b) => { a = find(a); b = find(b); if (a !== b) parent[a] = b; };
for (let t = 0; t < idx.length; t += 3) { uni(weld[idx[t]], weld[idx[t + 1]]); uni(weld[idx[t]], weld[idx[t + 2]]); }
const comp = new Map();
for (let i = 0; i < n; i++) { const r = find(weld[i]); if (!comp.has(r)) comp.set(r, []); comp.get(r).push(i); }
const comps = [...comp.values()].map((vs) => {
  const mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9];
  for (const i of vs) for (let a = 0; a < 3; a++) { mn[a] = Math.min(mn[a], pos[i * 3 + a]); mx[a] = Math.max(mx[a], pos[i * 3 + a]); }
  return { n: vs.length, min: mn.map((v) => +v.toFixed(3)), max: mx.map((v) => +v.toFixed(3)), root: find(weld[vs[0]]) };
}).sort((a, b) => b.n - a.n);
console.log('composantes', comps.length);
for (const c of comps.slice(0, 25)) console.log(c.n, 'min', c.min.join(','), 'max', c.max.join(','));
// étiquette par sommet : index de composante
const label = new Int32Array(n);
const order = new Map(comps.map((c, i) => [c.root, i]));
for (let i = 0; i < n; i++) label[i] = order.get(find(weld[i]));
fs.writeFileSync(out, JSON.stringify({ comps, label: Array.from(label) }));
