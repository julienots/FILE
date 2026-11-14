// Tripo haute-def -> low-poly a couleurs de sommets: cuit la texture dans les sommets, puis regroupe les sommets (sloppy).
// Evite les craquelures dues aux ilots UV minuscules. Sortie sans texture (tres leger, ideal pour le rendu toon).
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { prune, dedup } from '@gltf-transform/functions';
import { MeshoptSimplifier, MeshoptDecoder, MeshoptEncoder } from 'meshoptimizer';
import sharp from 'sharp';
const [,, src, dst, targetTris = '16000'] = process.argv;
await MeshoptSimplifier.ready; await MeshoptDecoder.ready; await MeshoptEncoder.ready;
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS).registerDependencies({ 'meshopt.decoder': MeshoptDecoder, 'meshopt.encoder': MeshoptEncoder });
const doc = await io.read(src), buf = doc.getRoot().listBuffers()[0];
let total = 0; for (const m of doc.getRoot().listMeshes()) for (const p of m.listPrimitives()) total += (p.getIndices()?.getCount() ?? p.getAttribute('POSITION').getCount()) / 3;
const lin = new Float32Array(256); for (let i = 0; i < 256; i++) { const c = i / 255; lin[i] = c <= .04045 ? c / 12.92 : Math.pow((c + .055) / 1.055, 2.4); }
for (const m of doc.getRoot().listMeshes()) for (const p of m.listPrimitives()) {
  const posA = p.getAttribute('POSITION'), pos = posA.getArray() instanceof Float32Array ? posA.getArray() : new Float32Array(posA.getArray()), n = pos.length / 3;
  const idx = p.getIndices() ? p.getIndices().getArray() : Uint32Array.from({ length: n }, (_, i) => i), uvAcc = p.getAttribute('TEXCOORD_0'), nrm = p.getAttribute('NORMAL');
  const mat = p.getMaterial(), tex = mat?.getBaseColorTexture(), fac = mat?.getBaseColorFactor() ?? [1, 1, 1, 1];
  const T = 1024; let img = null; if (tex && uvAcc) img = await sharp(Buffer.from(tex.getImage())).resize(T, T).removeAlpha().raw().toBuffer();
  const col = new Float32Array(n * 3), uv = [0, 0];
  for (let i = 0; i < n; i++) { if (img) { uvAcc.getElement(i, uv); const x = Math.min(T - 1, Math.max(0, Math.floor((uv[0] - Math.floor(uv[0])) * T))), y = Math.min(T - 1, Math.max(0, Math.floor((uv[1] - Math.floor(uv[1])) * T))), o = (y * T + x) * 3;
      col[i * 3] = lin[img[o]] * fac[0]; col[i * 3 + 1] = lin[img[o + 1]] * fac[1]; col[i * 3 + 2] = lin[img[o + 2]] * fac[2]; } else { col[i * 3] = fac[0]; col[i * 3 + 1] = fac[1]; col[i * 3 + 2] = fac[2]; } }
  // soudure par position (moyenne des couleurs des doublons)
  let mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9]; for (let i = 0; i < n; i++) for (let k = 0; k < 3; k++) { const v = pos[i * 3 + k]; if (v < mn[k]) mn[k] = v; if (v > mx[k]) mx[k] = v; }
  const q = 4096 / Math.max(mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]), map = new Map(), rep = new Uint32Array(n);
  for (let i = 0; i < n; i++) { const key = Math.round((pos[i * 3] - mn[0]) * q) + ',' + Math.round((pos[i * 3 + 1] - mn[1]) * q) + ',' + Math.round((pos[i * 3 + 2] - mn[2]) * q); let r = map.get(key); if (r === undefined) { r = i; map.set(key, i); } rep[i] = r; }
  const welded = new Uint32Array(idx.length); for (let i = 0; i < idx.length; i++) welded[i] = rep[idx[i]];
  const target = Math.max(300, Math.floor(+targetTris * (idx.length / 3) / total)) * 3;
  let [out] = MeshoptSimplifier.simplify(welded, pos, 3, target, 0.25, []); if (out.length > target * 1.5) [out] = MeshoptSimplifier.simplifySloppy(welded, pos, 3, null, target, 0.5);
  // couleur de chaque sommet conserve = moyenne des sommets d'origine les plus proches (grille grossiere)
  const used = new Map(); for (const v of out) if (!used.has(v)) used.set(v, used.size);
  const U = used.size, np = new Float32Array(U * 3), nc = new Float32Array(U * 3), cnt = new Float32Array(U), nn = new Float32Array(U * 3), e = [0, 0, 0];
  const cell = Math.cbrt(((mx[0] - mn[0]) * (mx[1] - mn[1]) * (mx[2] - mn[2])) / U) * .7 || 1e-3, grid = new Map(), ck = (x, y, z) => Math.floor(x / cell) + ',' + Math.floor(y / cell) + ',' + Math.floor(z / cell);
  for (const [v, j] of used) { np[j * 3] = pos[v * 3]; np[j * 3 + 1] = pos[v * 3 + 1]; np[j * 3 + 2] = pos[v * 3 + 2]; if (nrm) { nrm.getElement(v, e); nn[j * 3] = e[0]; nn[j * 3 + 1] = e[1]; nn[j * 3 + 2] = e[2]; }
    const k = ck(pos[v * 3], pos[v * 3 + 1], pos[v * 3 + 2]); let a = grid.get(k); if (!a) grid.set(k, a = []); a.push(j); }
  for (let i = 0; i < n; i += 2) { const x = pos[i * 3], y = pos[i * 3 + 1], z = pos[i * 3 + 2], a = grid.get(ck(x, y, z)); if (!a) continue; let b = -1, bd = 1e9;
    for (const j of a) { const d = (np[j * 3] - x) ** 2 + (np[j * 3 + 1] - y) ** 2 + (np[j * 3 + 2] - z) ** 2; if (d < bd) { bd = d; b = j; } }
    nc[b * 3] += col[i * 3]; nc[b * 3 + 1] += col[i * 3 + 1]; nc[b * 3 + 2] += col[i * 3 + 2]; cnt[b]++; }
  let j = 0; for (const [v] of used) { if (cnt[j]) { nc[j * 3] /= cnt[j]; nc[j * 3 + 1] /= cnt[j]; nc[j * 3 + 2] /= cnt[j]; } else { nc[j * 3] = col[v * 3]; nc[j * 3 + 1] = col[v * 3 + 1]; nc[j * 3 + 2] = col[v * 3 + 2]; } j++; }
  const ni = []; for (let i = 0; i < out.length; i += 3) { const a = used.get(out[i]), b = used.get(out[i + 1]), c = used.get(out[i + 2]); if (a !== b && b !== c && a !== c) ni.push(a, b, c); }
  for (const s of p.listSemantics()) p.setAttribute(s, null);
  p.setAttribute('POSITION', doc.createAccessor().setType('VEC3').setArray(np).setBuffer(buf)).setAttribute('COLOR_0', doc.createAccessor().setType('VEC3').setArray(nc).setBuffer(buf));
  if (nrm) p.setAttribute('NORMAL', doc.createAccessor().setType('VEC3').setArray(nn).setBuffer(buf));
  p.setIndices(doc.createAccessor().setType('SCALAR').setArray(U > 65535 ? new Uint32Array(ni) : new Uint16Array(ni)).setBuffer(buf));
  if (mat) mat.setBaseColorTexture(null).setNormalTexture(null).setMetallicRoughnessTexture(null).setOcclusionTexture(null).setEmissiveTexture(null).setBaseColorFactor([1, 1, 1, 1]).setMetallicFactor(0).setRoughnessFactor(1);
  console.log(`  ${(idx.length / 3) | 0} -> ${ni.length / 3} tris, ${U} sommets`);
}
await doc.transform(prune(), dedup()); await io.write(dst, doc);
