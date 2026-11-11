// Photo → 3D par méthode GÉOMÉTRIQUE (pas d'IA neuronale) : la silhouette est extraite puis gonflée
// en un volume fermé, texturé par la photo en face avant.
import * as THREE from "three";
import { taubin } from "./organic.js";

export async function imageToModel(file, { height = 1, depth = 0.5, resolution = 80 } = {}) {
  const img = await new Promise((res, rej) => { const i = new Image(); i.onload = () => res(i); i.onerror = () => rej(new Error("image illisible")); i.src = URL.createObjectURL(file); });
  const W = 512, H = Math.round(512 * img.height / img.width);
  const cv = document.createElement("canvas"); cv.width = W; cv.height = H;
  const g = cv.getContext("2d"); g.drawImage(img, 0, 0, W, H);
  const px = g.getImageData(0, 0, W, H).data;
  let transparent = false;
  for (let i = 3; i < px.length; i += 4 * 97) if (px[i] < 230) { transparent = true; break; }
  const corners = [[2, 2], [W - 3, 2], [2, H - 3], [W - 3, H - 3]].map(([x, y]) => { const k = (y * W + x) * 4; return [px[k], px[k + 1], px[k + 2]]; });
  const bg = [0, 1, 2].map((c) => corners.map((q) => q[c]).sort((a, b) => a - b)[1]);
  const inside = (x, y) => { const k = (y * W + x) * 4; return transparent ? px[k + 3] > 128 : Math.hypot(px[k] - bg[0], px[k + 1] - bg[1], px[k + 2] - bg[2]) > 40; };
  let x0 = W, x1 = 0, y0 = H, y1 = 0, cnt = 0;
  for (let y = 0; y < H; y += 2) for (let x = 0; x < W; x += 2) if (inside(x, y)) { cnt++; x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
  if (cnt < 30) throw new Error("Silhouette introuvable : utilisez une photo sur fond uni ou une image détourée.");
  const side = Math.max(x1 - x0, y1 - y0) * 1.08, cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
  const n = resolution;
  const mask = new Uint8Array(n * n);
  for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) {
    const x = Math.round(cx - side / 2 + (i + 0.5) / n * side), y = Math.round(cy + side / 2 - (j + 0.5) / n * side);
    mask[j * n + i] = x >= 0 && y >= 0 && x < W && y < H && inside(x, y) ? 1 : 0;
  }
  // distance au bord (chanfrein 3-4)
  const D = new Float32Array(n * n).map((_, k) => (mask[k] ? 1e6 : 0));
  const at = (i, j) => (i < 0 || j < 0 || i >= n || j >= n ? 0 : D[j * n + i]);
  for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) if (D[j * n + i]) D[j * n + i] = Math.min(D[j * n + i], at(i - 1, j) + 3, at(i, j - 1) + 3, at(i - 1, j - 1) + 4, at(i + 1, j - 1) + 4);
  for (let j = n - 1; j >= 0; j--) for (let i = n - 1; i >= 0; i--) if (D[j * n + i]) D[j * n + i] = Math.min(D[j * n + i], at(i + 1, j) + 3, at(i, j + 1) + 3, at(i + 1, j + 1) + 4, at(i - 1, j + 1) + 4);
  let dmax = 1;
  for (const d of D) dmax = Math.max(dmax, d);
  const vid = new Int32Array((n + 1) * (n + 1) * 2).fill(-1);
  const pos = [], uv = [], idx = [];
  const cornerIn = (i, j) => [[i - 1, j - 1], [i, j - 1], [i - 1, j], [i, j]].some(([a, b]) => a >= 0 && b >= 0 && a < n && b < n && mask[b * n + a]);
  const cornerBorder = (i, j) => [[i - 1, j - 1], [i, j - 1], [i - 1, j], [i, j]].some(([a, b]) => a < 0 || b < 0 || a >= n || b >= n || !mask[b * n + a]);
  const hAt = (i, j) => { let s = 0, c = 0; for (const [a, b] of [[i - 1, j - 1], [i, j - 1], [i - 1, j], [i, j]]) if (a >= 0 && b >= 0 && a < n && b < n) { s += Math.sqrt(D[b * n + a] / dmax); c++; } return c ? s / c : 0; };
  const vert = (i, j, back) => {
    const key = ((j * (n + 1)) + i) * 2 + (back ? 1 : 0);
    const border = cornerBorder(i, j);
    if (border && back) return vert(i, j, false);
    if (vid[key] >= 0) return vid[key];
    const t = border ? 0 : hAt(i, j) * depth * 0.5;
    pos.push(i / n - 0.5, j / n, back ? -t : t);
    const ux = (cx - side / 2 + i / n * side) / W, uy = (cy + side / 2 - j / n * side) / H;
    uv.push(ux, 1 - uy);
    vid[key] = pos.length / 3 - 1;
    return vid[key];
  };
  for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) {
    if (!mask[j * n + i]) continue;
    const a = vert(i, j, false), b = vert(i + 1, j, false), c = vert(i + 1, j + 1, false), d = vert(i, j + 1, false);
    idx.push(a, b, c, a, c, d);
    const A = vert(i, j, true), B = vert(i + 1, j, true), C = vert(i + 1, j + 1, true), Dd = vert(i, j + 1, true);
    idx.push(A, C, B, A, Dd, C);
  }
  void cornerIn;
  const P = new Float32Array(pos);
  taubin(P, idx, 3);
  const geo = new THREE.BufferGeometry();
  geo.setAttribute("position", new THREE.BufferAttribute(P, 3));
  geo.setAttribute("uv", new THREE.Float32BufferAttribute(uv, 2));
  geo.setIndex(idx);
  geo.scale(height, height, height);
  geo.computeVertexNormals();
  const tex = new THREE.CanvasTexture(cv);
  tex.colorSpace = THREE.SRGBColorSpace;
  const mat = new THREE.MeshStandardMaterial({ map: tex, roughness: 0.6 });
  mat.userData = { preset: "image", params: {} };
  const mesh = new THREE.Mesh(geo, mat);
  mesh.name = (file.name || "Photo").replace(/\.[^.]+$/, "").slice(0, 30) || "Photo";
  mesh.userData.method = "silhouette gonflée (géométrique, non neuronale)";
  return mesh;
}
