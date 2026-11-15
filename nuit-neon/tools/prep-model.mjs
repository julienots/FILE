// Prépare un modèle 3D d'un seul bloc pour le jeu :
//   1. réduit le nombre de triangles et la taille des textures (mobile),
//   2. le remet à l'échelle, posé au sol, l'avant vers -z,
//   3. repère les roues et les découpe en pièces séparées (wheel_fl, wheel_fr, wheel_rl, wheel_rr, ou wheel_f / wheel_r pour une moto)
//      pour que le jeu puisse les faire tourner et braquer.
// Usage : node prep-model.mjs entree.glb sortie.glb '{"front":"+x","len":4.5,"type":"car","tris":30000}'
//   front : direction de l'avant dans le fichier d'origine (+x, -x, +z, -z) ; type : car ou moto
//   réglages manuels facultatifs si la détection se trompe : "r", "rf", "rr" (rayon des roues, avant, arrière), "zf"/"zr" (position avant/arrière), "xin" (bord intérieur du pneu)
import { NodeIO, Document } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { weld, simplify, textureCompress, prune, dedup, flatten, meshopt } from '@gltf-transform/functions';
import { MeshoptDecoder, MeshoptEncoder, MeshoptSimplifier } from 'meshoptimizer';
import sharp from 'sharp';
await MeshoptDecoder.ready; await MeshoptEncoder.ready; await MeshoptSimplifier.ready;
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS).registerDependencies({ 'meshopt.decoder': MeshoptDecoder, 'meshopt.encoder': MeshoptEncoder });
const [,, src, dst, cfgStr] = process.argv;
const cfg = Object.assign({ front: '-z', len: 4.5, type: 'car', tris: 30000 }, JSON.parse(cfgStr || '{}'));

const doc = await io.read(src); const root = doc.getRoot();
const count = () => { let t = 0; for (const m of root.listMeshes()) for (const p of m.listPrimitives()) t += p.getIndices().getCount() / 3; return t | 0; };
await doc.transform(dedup(), flatten(), weld());
const t0 = count();
if (t0 > cfg.tris * 1.1) await doc.transform(simplify({ simplifier: MeshoptSimplifier, ratio: cfg.tris / t0, error: 0.08, lockBorder: false }));
await doc.transform(
  textureCompress({ encoder: sharp, targetFormat: 'webp', resize: [1024, 1024], quality: 80, slots: /baseColor/ }),
  textureCompress({ encoder: sharp, targetFormat: 'webp', resize: [512, 512], quality: 80, slots: /^(?!baseColor)/ }));

const node = root.listNodes().find(n => n.getMesh()), mesh = node.getMesh(), prim = mesh.listPrimitives()[0], mat = prim.getMaterial();
// lecture des attributs en valeurs réelles (certains fichiers les stockent en entiers compressés)
const read = (acc, n) => { if (!acc) return null; const cnt = acc.getCount(), o = new Float32Array(cnt * n), e = []; for (let i = 0; i < cnt; i++) { acc.getElement(i, e); for (let k = 0; k < n; k++) o[i * n + k] = e[k]; } return o; };
const P = read(prim.getAttribute('POSITION'), 3), Nn = read(prim.getAttribute('NORMAL'), 3), UV = read(prim.getAttribute('TEXCOORD_0'), 2), nv = P.length / 3;
// transformation de texture éventuelle (décalage et échelle), appliquée directement aux coordonnées
{ const ti = mat.getBaseColorTextureInfo(), tt = ti && ti.getExtension('KHR_texture_transform');
  if (tt && UV) { const o = tt.getOffset(), sc2 = tt.getScale(), ro = tt.getRotation(), c = Math.cos(ro), sn = Math.sin(ro);
    for (let i = 0; i < nv; i++) { const u = UV[i * 2] * sc2[0], v = UV[i * 2 + 1] * sc2[1]; UV[i * 2] = c * u + sn * v + o[0]; UV[i * 2 + 1] = -sn * u + c * v + o[1]; } } }
let I = new Uint32Array(prim.getIndices().getArray());
// maillages très morcelés que la réduction normale n'arrive pas à alléger : on autorise à franchir les coutures de texture
for (const err of [0.05, 0.15, 0.4]) { if (I.length / 3 <= cfg.tris * 1.25) break;
  const r = MeshoptSimplifier.simplify(I, P, 3, cfg.tris * 3, err, ['Permissive', 'Prune']); if (r[0].length < I.length) I = r[0]; }
const finalTris = I.length / 3;
// matrice du nœud, puis orientation : l'avant vers -z
const M = node.getWorldMatrix();
const rot = { '-z': [1, 0, 0, 1], '+z': [-1, 0, 0, -1], '+x': [0, 1, -1, 0], '-x': [0, -1, 1, 0] }[cfg.front]; // x' = a x + b z ; z' = c x + d z
const xf = (x, y, z, w) => { const X = M[0] * x + M[4] * y + M[8] * z + M[12] * w, Y = M[1] * x + M[5] * y + M[9] * z + M[13] * w, Z = M[2] * x + M[6] * y + M[10] * z + M[14] * w; return [rot[0] * X + rot[1] * Z, Y, rot[2] * X + rot[3] * Z]; };
let mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9];
for (let i = 0; i < nv; i++) { const v = xf(P[i * 3], P[i * 3 + 1], P[i * 3 + 2], 1); for (let d = 0; d < 3; d++) { P[i * 3 + d] = v[d]; if (v[d] < mn[d]) mn[d] = v[d]; if (v[d] > mx[d]) mx[d] = v[d]; } }
if (Nn) for (let i = 0; i < nv; i++) { const v = xf(Nn[i * 3], Nn[i * 3 + 1], Nn[i * 3 + 2], 0), l = Math.hypot(...v) || 1; Nn[i * 3] = v[0] / l; Nn[i * 3 + 1] = v[1] / l; Nn[i * 3 + 2] = v[2] / l; }
const sc = cfg.len / (mx[2] - mn[2]), cx = (mn[0] + mx[0]) / 2, cz = (mn[2] + mx[2]) / 2;
for (let i = 0; i < nv; i++) { P[i * 3] = (P[i * 3] - cx) * sc; P[i * 3 + 1] = (P[i * 3 + 1] - mn[1]) * sc; P[i * 3 + 2] = (P[i * 3 + 2] - cz) * sc; }
const hw = (mx[0] - mn[0]) * sc / 2, hgt = (mx[1] - mn[1]) * sc;

// ---------- repérage des roues ----------
const median = (a) => { a.sort((p, q) => p - q); return a[a.length >> 1]; };
const moto = cfg.type === 'moto';
function findWheel(side, zsign) {
  // side : -1 gauche, +1 droite, 0 moto (roue au centre) ; zsign : -1 avant, +1 arrière
  const inSide = (x) => side === 0 ? Math.abs(x) < hw * 0.75 : side * x > hw * 0.5;
  const zs = [], xs = [];
  let yb = 1e9;                                   // point le plus bas de ce coin du véhicule : c'est le bas du pneu
  for (let i = 0; i < nv; i++) { const x = P[i * 3], y = P[i * 3 + 1], z = P[i * 3 + 2]; if (zsign * z > cfg.len * 0.12 && inSide(x) && y < yb) yb = y; }
  for (let i = 0; i < nv; i++) { const x = P[i * 3], y = P[i * 3 + 1], z = P[i * 3 + 2]; if (y < yb + 0.05 && zsign * z > cfg.len * 0.12 && inSide(x)) { zs.push(z); xs.push(side === 0 ? Math.abs(x) : side * x); } }
  if (zs.length < 3) return null;
  let z0 = cfg[zsign < 0 ? 'zf' : 'zr'] ?? median(zs);
  xs.sort((p, q) => p - q);
  let xin = side === 0 ? 0 : (cfg.xin ?? xs[Math.floor(xs.length * 0.04)]), xout = xs[Math.floor(xs.length * 0.97)];
  // rayon : on cherche le cylindre de la bande de roulement, là où s'alignent le plus de sommets
  let bestR = 0.33, bestS = -1, z0b = z0;
  // plage de rayons plausible, ramenée à la longueur du véhicule (une moto de 2,1 m a des roues d'environ 0,30 m)
  const lo = moto ? cfg.len * 0.128 : 0.24, hi = moto ? cfg.len * 0.165 : Math.min(0.5, hgt * 0.62);
  for (let dz = -0.06; dz <= 0.0601; dz += 0.02) for (let r = lo; r <= hi; r += 0.005) {
    let s = 0;
    for (let i = 0; i < nv; i++) { const x = P[i * 3], y = P[i * 3 + 1] - yb, z = P[i * 3 + 2];
      if (y > r * 1.35 || Math.abs(z - z0 - dz) > r + 0.03) continue;
      if (side === 0 ? Math.abs(x) > xout + 0.03 : (side * x < xin - 0.02)) continue;
      if (Math.abs(Math.hypot(z - z0 - dz, y - r) - r) < 0.011) s++; }
    s /= Math.pow(r, cfg.pw ?? 0.5);
    if (s > bestS) { bestS = s; bestR = r; z0b = z0 + dz; }
  }
  const r = cfg[zsign < 0 ? 'rf' : 'rr'] ?? cfg.r ?? bestR; z0 = cfg[zsign < 0 ? 'zf' : 'zr'] ?? z0b;
  return { side, zsign, z0, r, xin, xout, yb };
}
const wheels = [];
if (moto) { for (const zs of [-1, 1]) { const w = findWheel(0, zs); if (w) wheels.push(Object.assign(w, { name: zs < 0 ? 'wheel_f' : 'wheel_r' })); } }
else for (const zs of [-1, 1]) for (const s of [-1, 1]) { const w = findWheel(s, zs); if (w) wheels.push(Object.assign(w, { name: 'wheel_' + (zs < 0 ? 'f' : 'r') + (s < 0 ? 'l' : 'r') })); }
// roues d'un même essieu : même rayon et même position, pour qu'elles tournent rond
if (!moto) for (const zs of [-1, 1]) { const ax = wheels.filter(w => w.zsign === zs); if (ax.length === 2) { const r = (ax[0].r + ax[1].r) / 2, z = (ax[0].z0 + ax[1].z0) / 2, xi = Math.min(ax[0].xin, ax[1].xin); const yb = Math.min(ax[0].yb, ax[1].yb); for (const w of ax) { w.r = cfg[zs < 0 ? 'rf' : 'rr'] ?? cfg.r ?? r; w.z0 = z; w.xin = cfg.xin ?? xi; w.yb = yb; } } }

if (moto && wheels.length === 2 && cfg.r === undefined && cfg.rf === undefined && cfg.rr === undefined) { const r = (wheels[0].r + wheels[1].r) / 2; for (const w of wheels) w.r = r; }
// ---------- découpe ----------
const inWheel = (w, i) => { const x = P[i * 3], y = P[i * 3 + 1], z = P[i * 3 + 2];
  if (Math.hypot(z - w.z0, y - w.yb - w.r) > w.r * 1.025) return false;
  return w.side === 0 ? Math.abs(x) < Math.max(0.085, w.xout * 1.15) : w.side * x > w.xin - 0.025; };
const owner = new Int8Array(I.length / 3).fill(-1);
for (let t = 0; t < I.length; t += 3) for (let k = 0; k < wheels.length; k++) if (inWheel(wheels[k], I[t]) && inWheel(wheels[k], I[t + 1]) && inWheel(wheels[k], I[t + 2])) { owner[t / 3] = k; break; }

const out = new Document(); const buf = out.createBuffer(); const scene = out.createScene('vehicule');
// copie du matériau et de ses textures
const cpTex = (t) => t ? out.createTexture(t.getName()).setImage(t.getImage()).setMimeType(t.getMimeType()) : null;
const texMap = new Map(); const tex = (t) => { if (!t) return null; if (!texMap.has(t)) texMap.set(t, cpTex(t)); return texMap.get(t); };
const m2 = out.createMaterial('carrosserie').setBaseColorFactor(mat.getBaseColorFactor()).setBaseColorTexture(tex(mat.getBaseColorTexture()))
  .setMetallicFactor(mat.getMetallicFactor()).setRoughnessFactor(mat.getRoughnessFactor()).setMetallicRoughnessTexture(tex(mat.getMetallicRoughnessTexture()))
  .setNormalTexture(tex(mat.getNormalTexture())).setDoubleSided(true);
const dark = out.createMaterial('interieur').setBaseColorFactor([0.015, 0.015, 0.018, 1]).setRoughnessFactor(0.9).setMetallicFactor(0);
function part(name, k, origin) {
  const map = new Map(), pos = [], nor = [], uv = [], idx = [];
  for (let t = 0; t < I.length; t += 3) { if (owner[t / 3] !== k) continue;
    for (let j = 0; j < 3; j++) { const v = I[t + j]; let n = map.get(v);
      if (n === undefined) { n = pos.length / 3; map.set(v, n); pos.push(P[v * 3] - origin[0], P[v * 3 + 1] - origin[1], P[v * 3 + 2] - origin[2]); if (Nn) nor.push(Nn[v * 3], Nn[v * 3 + 1], Nn[v * 3 + 2]); if (UV) uv.push(UV[v * 2], UV[v * 2 + 1]); }
      idx.push(n); } }
  const pr = out.createPrimitive().setMaterial(m2).setAttribute('POSITION', out.createAccessor().setType('VEC3').setArray(new Float32Array(pos)).setBuffer(buf))
    .setIndices(out.createAccessor().setType('SCALAR').setArray(new Uint32Array(idx)).setBuffer(buf));
  if (Nn) pr.setAttribute('NORMAL', out.createAccessor().setType('VEC3').setArray(new Float32Array(nor)).setBuffer(buf));
  if (UV) pr.setAttribute('TEXCOORD_0', out.createAccessor().setType('VEC2').setArray(new Float32Array(uv)).setBuffer(buf));
  const me = out.createMesh(name).addPrimitive(pr);
  const nd = out.createNode(name).setMesh(me).setTranslation(origin); scene.addChild(nd);
  return { me, nd, tris: idx.length / 3 };
}
const log = [`corps ${part('body', -1, [0, 0, 0]).tris}`];
wheels.forEach((w, k) => {
  const xc = w.side === 0 ? 0 : w.side * (w.xin + w.xout) / 2;
  const p = part(w.name, k, [xc, w.yb + w.r, w.z0]);
  p.nd.setExtras({ rayon: +w.r.toFixed(3) });   // lu par le jeu pour la vitesse de rotation
  if (w.side !== 0) {
    // disque sombre qui ferme le côté intérieur de la roue
    const pos = [w.side * w.xin - xc, 0, 0], nor = [-w.side, 0, 0], idx = [], seg = 18, rr = w.r * 0.9;
    for (let i = 0; i < seg; i++) { const a = i / seg * Math.PI * 2; pos.push(w.side * w.xin - xc, Math.cos(a) * rr, Math.sin(a) * rr); nor.push(-w.side, 0, 0); idx.push(0, 1 + i, 1 + (i + 1) % seg); }
    p.me.addPrimitive(out.createPrimitive().setMaterial(dark).setAttribute('POSITION', out.createAccessor().setType('VEC3').setArray(new Float32Array(pos)).setBuffer(buf))
      .setAttribute('NORMAL', out.createAccessor().setType('VEC3').setArray(new Float32Array(nor)).setBuffer(buf)).setIndices(out.createAccessor().setType('SCALAR').setArray(new Uint16Array(idx)).setBuffer(buf)));
  }
  log.push(`${w.name} r=${w.r.toFixed(3)} z=${w.z0.toFixed(2)} x=${w.side === 0 ? '0' : w.xin.toFixed(2) + '..' + w.xout.toFixed(2)} tris=${p.tris}`);
});
await out.transform(prune(), meshopt({ encoder: MeshoptEncoder, level: 'medium' }));
await io.write(dst, out);
console.log(`${dst.split('/').pop()} : ${t0} -> ${finalTris} triangles, ${(cfg.len).toFixed(1)} x ${(hw * 2).toFixed(2)} x ${hgt.toFixed(2)} m | ` + log.join(' | '));
