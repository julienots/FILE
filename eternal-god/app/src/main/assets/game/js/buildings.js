// Fabrique des bâtiments médiévaux fantastiques. Chaque bâtiment est fusionné en une seule
// géométrie (un seul appel de dessin) et garde la liste de ses pièces pour la destruction.
import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

const col = (hex) => new THREE.Color(hex);
export const PALETTE = {
  plaster: [0xe9dfc8, 0xf1e8d2, 0xdcd0b4, 0xe8d9bd, 0xd8cdb8],
  timber: [0x5a3b22, 0x4b2f1b, 0x664429],
  roof: [0x9c3b26, 0x7d3220, 0x5a4a44, 0xb8955a, 0x46525e, 0xa5482c, 0xb8955a],
  stone: [0x8d8a82, 0x9a948a, 0x7b776f],
  wood: 0x8a6440,
  gold: 0xe3b347,
  canvas: [0xc23b30, 0x2f6fb0, 0xe0b23a, 0x4f8f3a, 0x8b3fa0],
};

// couche de texture photo selon la couleur de conception (-1 = couleur unie)
const LAYER = new Map();
for (const c of PALETTE.plaster) LAYER.set(c, 0);
for (const c of [...PALETTE.timber, PALETTE.wood, 0x3d2817, 0x4b2f1b, 0x664429]) LAYER.set(c, 1);
for (const c of [0x9c3b26, 0x7d3220, 0xa5482c, 0x8a5a2b, 0x7a2e1e, 0x8b6f4e]) LAYER.set(c, 2);
for (const c of [0x5a4a44, 0x46525e, 0x4a4440]) LAYER.set(c, 3);
LAYER.set(0xb8955a, 4); LAYER.set(0xc9a35a, 4);
for (const c of [...PALETTE.stone, 0x6d6a64, 0x5a5650, 0x7b776f, 0xbdb6a8, 0x9a948a]) LAYER.set(c, 5);
for (const c of [0xefe9dc, 0xf4efe4, 0xe9e2d2, 0xe6dcc6]) LAYER.set(c, 6);
const LAYER_BASE = [0xe2dccf, 0x6b5440, 0xa0503a, 0x55555a, 0xb09060, 0x8a857c, 0xe8e4dc];

export class Builder {
  constructor() { this.geos = []; this.pieces = []; }
  _push(g, color, piece) {
    g = g.index ? g.toNonIndexed() : g;
    g.deleteAttribute('uv');
    const layer = LAYER.has(color) ? LAYER.get(color) : -1;
    let c = col(color);
    if (layer >= 0) { // teinte relative à la texture photo, adoucie
      const b = col(LAYER_BASE[layer]);
      c = new THREE.Color(Math.min(1.25, c.r / b.r), Math.min(1.25, c.g / b.g), Math.min(1.25, c.b / b.b)).lerp(new THREE.Color(1, 1, 1), 0.45);
    }
    const n = g.attributes.position.count, arr = new Float32Array(n * 3), lay = new Float32Array(n);
    const v0 = 0.92 + Math.random() * 0.08;
    for (let i = 0; i < n; i++) { arr[i * 3] = c.r * v0; arr[i * 3 + 1] = c.g * v0; arr[i * 3 + 2] = c.b * v0; lay[i] = layer; }
    g.setAttribute('color', new THREE.BufferAttribute(arr, 3));
    g.setAttribute('layer', new THREE.BufferAttribute(lay, 1));
    if (piece) piece.layer = layer;
    this.geos.push(g);
    if (piece) this.pieces.push(piece);
  }
  box(w, h, d, x, y, z, color, ry = 0, rx = 0, rz = 0, breakable = true) {
    const g = new THREE.BoxGeometry(w, h, d);
    const m = new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(rx, ry, rz));
    m.setPosition(x, y, z); g.applyMatrix4(m);
    this._push(g, color, breakable ? { w, h, d, x, y, z, ry, rx, rz, color } : null);
  }
  cyl(rt, rb, h, x, y, z, color, seg = 10, breakable = true) {
    const g = new THREE.CylinderGeometry(rt, rb, h, seg); g.translate(x, y, z);
    this._push(g, color, breakable ? { w: rb * 1.6, h, d: rb * 1.6, x, y, z, ry: 0, rx: 0, rz: 0, color } : null);
  }
  cone(r, h, x, y, z, color, seg = 8, ry = 0) {
    const g = new THREE.ConeGeometry(r, h, seg); g.rotateY(ry); g.translate(x, y, z);
    this._push(g, color, { w: r * 1.4, h: h * 0.6, d: r * 1.4, x, y, z, ry: 0, rx: 0, rz: 0, color });
  }
  // toit à deux pans (prisme) avec débord
  roof(w, h, d, x, y, z, color, ry = 0) {
    const s = new THREE.Shape();
    s.moveTo(-w / 2, 0); s.lineTo(w / 2, 0); s.lineTo(0, h); s.lineTo(-w / 2, 0);
    const g = new THREE.ExtrudeGeometry(s, { depth: d, bevelEnabled: false });
    g.translate(0, 0, -d / 2);
    const m = new THREE.Matrix4().makeRotationY(ry); m.setPosition(x, y, z); g.applyMatrix4(m);
    this._push(g, color, null);
    // pièces de destruction : deux pans
    for (const sd of [-1, 1]) this.pieces.push({ w: w * 0.55, h: 0.35, d, x: x + Math.cos(ry) * sd * w * 0.25, y: y + h * 0.5, z: z - Math.sin(ry) * sd * w * 0.25, ry, rx: 0, rz: sd * -Math.atan2(h, w / 2), color });
  }
  geometry() { const g = mergeGeometries(this.geos, false); g.computeBoundingSphere(); g.computeBoundingBox(); return g; }
}

const pickc = (rnd, arr) => arr[Math.floor(rnd() * arr.length)];

// Maison à colombages, 1 ou 2 étages
function house(b, rnd, big = false) {
  const w = (big ? 9 : 6) + rnd() * 2.5, d = (big ? 8 : 5.5) + rnd() * 2, floors = big || rnd() < 0.45 ? 2 : 1;
  const fh = 3.1, H = floors * fh;
  const plaster = pickc(rnd, PALETTE.plaster), timber = pickc(rnd, PALETTE.timber), roof = pickc(rnd, PALETTE.roof), stone = pickc(rnd, PALETTE.stone);
  b.box(w + 0.4, 0.8, d + 0.4, 0, 0.4, 0, stone);                 // soubassement
  // murs découpés en panneaux (pour une destruction morceau par morceau)
  const panel = (len, along, fixed, isX, f) => {
    const n = Math.max(2, Math.round(len / 2.4)), pw = len / n;
    for (let i = 0; i < n; i++) {
      const o = -len / 2 + pw * (i + 0.5);
      if (isX) b.box(pw, fh, 0.35, o, 0.8 + fh * (f + 0.5), fixed, f === 0 && floors === 2 ? stone : plaster);
      else b.box(0.35, fh, pw, fixed, 0.8 + fh * (f + 0.5), o, f === 0 && floors === 2 ? stone : plaster);
    }
  };
  for (let f = 0; f < floors; f++) {
    panel(w, 0, d / 2, true, f); panel(w, 0, -d / 2, true, f);
    panel(d, 0, w / 2, false, f); panel(d, 0, -w / 2, false, f);
    // colombages en saillie sur les quatre façades (étages enduits)
    const y0 = 0.8 + fh * f;
    const timbered = !(f === 0 && floors === 2);
    const faces = [[0, d / 2, w, 0, 1], [0, -d / 2, w, 0, -1], [w / 2, 0, d, Math.PI / 2, 1], [-w / 2, 0, d, Math.PI / 2, -1]];
    for (const [fx, fz, L, ry, sgn] of faces) {
      const alongX = ry === 0;
      const out = 0.24 * sgn;
      const P = (o, y) => alongX ? [fx + o, y, fz + out] : [fx + out, y, fz + o];
      // sablière basse et haute
      b.box(L + 0.5, 0.24, 0.16, ...P(0, y0 + 0.12), timber, ry, 0, 0, false);
      b.box(L + 0.5, 0.24, 0.16, ...P(0, y0 + fh - 0.12), timber, ry, 0, 0, false);
      if (!timbered) continue;
      const nPosts = Math.max(2, Math.round(L / 1.7));
      const step = L / nPosts;
      for (let i = 0; i <= nPosts; i++) b.box(0.2, fh - 0.24, 0.14, ...P(-L / 2 + i * step, y0 + fh / 2), timber, ry, 0, 0, false);
      b.box(L, 0.16, 0.12, ...P(0, y0 + fh * 0.45), timber, ry, 0, 0, false);
      for (let i = 0; i < nPosts; i++) {
        if ((i + f) % 2) continue;
        const cx = -L / 2 + (i + 0.5) * step, ang = Math.atan2(fh * 0.42, step) * (i % 4 < 2 ? 1 : -1);
        b.box(Math.hypot(step, fh * 0.42), 0.14, 0.12, ...P(cx, y0 + fh * 0.24), timber, ry, 0, ang * (alongX ? 1 : 1), false);
      }
      // fenêtres
      for (let i = 0; i < nPosts; i++) {
        if ((i + (alongX ? 1 : 0)) % 2 === 0 || (f === 0 && alongX && sgn > 0 && i === Math.floor(nPosts / 2))) continue;
        const cx = -L / 2 + (i + 0.5) * step;
        b.box(Math.min(1.0, step * 0.6), 0.95, 0.1, ...P(cx, y0 + fh * 0.68), 0x1d1a18, ry, 0, 0, false);
        b.box(Math.min(1.2, step * 0.7), 0.1, 0.22, ...P(cx, y0 + fh * 0.68 - 0.52), timber, ry, 0, 0, false);
      }
    }
  }
  // porte
  b.box(1.2, 2.1, 0.2, w * 0.15, 1.85, d / 2 + 0.22, 0x3d2817, 0, 0, 0, false);
  b.box(1.5, 0.22, 0.3, w * 0.15, 2.95, d / 2 + 0.25, timber, 0, 0, 0, false);
  // toit et cheminée
  const rh = 2.4 + rnd() * 1.4;
  b.roof(w + 1.2, rh, d + 1.0, 0, 0.8 + H, 0, roof, rnd() < 0.5 ? 0 : 0);
  b.box(0.9, rh + 1.2, 0.9, w * 0.28, 0.8 + H + rh * 0.55, -d * 0.2, stone);
  return { w: w + 1.2, d: d + 1.0, h: 0.8 + H + rh, door: [w * 0.15, d / 2 + 1.5], chimney: [w * 0.28, 0.8 + H + rh * 1.15, -d * 0.2] };
}

function tavern(b, rnd) {
  const r = house(b, rnd, true);
  b.box(0.2, 1.6, 0.2, -r.w * 0.45, 4.2, r.d / 2 + 0.2, PALETTE.timber[0]);
  b.box(1.4, 0.9, 0.12, -r.w * 0.45, 3.2, r.d / 2 + 0.9, 0x7a2e1e);
  b.box(1.0, 0.25, 0.14, -r.w * 0.45, 3.2, r.d / 2 + 0.98, PALETTE.gold, 0, 0, 0, false);
  // tables extérieures
  for (let i = 0; i < 2; i++) { b.box(2, 0.12, 1, -2 + i * 4, 1.0, r.d / 2 + 3, PALETTE.wood); b.box(0.2, 0.9, 0.2, -2 + i * 4, 0.5, r.d / 2 + 3, PALETTE.wood); }
  return r;
}

function forge(b, rnd) {
  const w = 8, d = 7, stone = 0x6d6a64;
  b.box(w, 0.6, d, 0, 0.3, 0, stone);
  b.box(w, 3.6, 0.5, 0, 2.4, -d / 2, stone); b.box(0.5, 3.6, d, -w / 2, 2.4, 0, stone);
  b.box(0.4, 3.6, 0.4, w / 2, 2.4, d / 2, PALETTE.timber[0]); b.box(0.4, 3.6, 0.4, -w / 2, 2.4, d / 2, PALETTE.timber[0]); b.box(0.4, 3.6, 0.4, w / 2, 2.4, -d / 2, PALETTE.timber[0]);
  b.roof(w + 1, 2.2, d + 1, 0, 4.2, 0, 0x4a4440, Math.PI / 2);
  b.box(2.2, 1.4, 2.2, -2, 1.3, -1.6, 0x5a5650);                 // foyer
  b.box(1.3, 6.5, 1.3, -2, 4.5, -2.4, 0x5a5650);                 // cheminée
  b.box(1.0, 0.5, 0.5, 1.5, 1.0, 0.5, 0x2c2c30);                 // enclume
  b.box(0.5, 0.6, 0.4, 1.5, 0.6, 0.5, 0x3a3a3e);
  return { w: w + 1, d: d + 1, h: 7.7, door: [1, d / 2 + 1.6], chimney: [-2, 8, -2.4], forge: [-2, 2.2, -1.6] };
}

function temple(b, rnd) {
  const w = 11, d = 17, marble = 0xefe9dc, stone = 0xbdb6a8;
  b.box(w + 3, 1.2, d + 3, 0, 0.6, 0, stone);
  b.box(w + 1.5, 0.6, d + 1.5, 0, 1.5, 0, marble);
  b.box(w - 2, 6.5, d - 5, 0, 5.05, -1.5, 0xf4efe4);
  for (let i = 0; i < 6; i++) for (const sx of [-1, 1]) b.cyl(0.45, 0.5, 7, sx * (w / 2 - 0.4), 5.3, -d / 2 + 1 + i * (d - 2) / 5, marble, 12);
  b.box(w + 1.4, 0.8, d + 1.4, 0, 9.2, 0, marble);
  b.roof(w + 2, 3.4, d + 2, 0, 9.6, 0, 0x8b6f4e, Math.PI / 2);
  b.box(2.4, 3.4, 0.4, 0, 3.5, d / 2 - 3.3, 0x3a2a1e, 0, 0, 0, false);
  // clocher et emblème doré
  b.box(3.2, 7, 3.2, 0, 13.5, -d / 2 + 2.5, 0xf4efe4);
  b.cone(2.8, 5, 0, 19.5, -d / 2 + 2.5, 0x8b6f4e, 4, Math.PI / 4);
  b.cyl(1.0, 1.0, 0.25, 0, 14, -d / 2 + 0.85, PALETTE.gold, 16);
  return { w: w + 3, d: d + 3, h: 22, door: [0, d / 2 + 2.5], altar: [0, 2, 0] };
}

function tower(b, rnd) {
  const s = 4.4, stone = pickc(rnd, PALETTE.stone);
  for (let i = 0; i < 4; i++) b.box(s, 3.4, s, 0, 1.7 + i * 3.4, 0, stone);
  b.box(s + 1.2, 0.6, s + 1.2, 0, 13.9, 0, PALETTE.wood);
  for (const [x, z] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) b.box(0.4, 2.6, 0.4, x * (s / 2 + 0.3), 15.4, z * (s / 2 + 0.3), PALETTE.wood);
  b.cone(s * 0.95, 3.8, 0, 18.5, 0, pickc(rnd, PALETTE.roof), 4, Math.PI / 4);
  return { w: s + 1.2, d: s + 1.2, h: 20.4, door: [0, s / 2 + 1.5], lookout: [0, 15, 0] };
}

function windmill(b, rnd) {
  b.cyl(2.6, 3.4, 9, 0, 4.5, 0, 0xe6dcc6, 12);
  b.cone(3.1, 3.6, 0, 10.8, 0, 0x7d3220, 12);
  b.box(1.0, 2.1, 0.3, 0, 1.05, 3.3, 0x3d2817, 0, 0, 0, false);
  return { w: 7, d: 7, h: 12.6, door: [0, 5], hub: [0, 8.6, 3.2] };
}

function stall(b, rnd) {
  const c = pickc(rnd, PALETTE.canvas);
  b.box(3, 0.9, 1.6, 0, 0.45, 0, PALETTE.wood);
  for (const [x, z] of [[-1.4, -0.7], [1.4, -0.7], [-1.4, 0.7], [1.4, 0.7]]) b.box(0.15, 2.6, 0.15, x, 1.3, z, PALETTE.wood);
  b.roof(3.4, 0.8, 2.2, 0, 2.6, 0, c, Math.PI / 2);
  for (let i = 0; i < 4; i++) b.box(0.5, 0.35, 0.5, -1 + i * 0.66, 1.1, 0, [0xd9a23a, 0x6aa84f, 0xc0392b, 0xe67e22][i]);
  return { w: 3.4, d: 2.2, h: 3.4, door: [0, 2] };
}

function well(b) {
  b.cyl(1.4, 1.5, 1.1, 0, 0.55, 0, 0x8d8a82, 14);
  b.box(0.2, 2.6, 0.2, -1.2, 1.9, 0, PALETTE.wood); b.box(0.2, 2.6, 0.2, 1.2, 1.9, 0, PALETTE.wood);
  b.roof(3.0, 1.0, 2.0, 0, 3.2, 0, 0x7d3220, Math.PI / 2);
  return { w: 3, d: 3, h: 4.2, door: [0, 2.2] };
}

function stable(b, rnd) {
  const w = 11, d = 6;
  b.box(w, 0.5, d, 0, 0.25, 0, 0x7b776f);
  b.box(w, 3, 0.3, 0, 2, -d / 2, PALETTE.wood); b.box(0.3, 3, d, -w / 2, 2, 0, PALETTE.wood); b.box(0.3, 3, d, w / 2, 2, 0, PALETTE.wood);
  for (let i = 0; i < 4; i++) b.box(0.3, 1.3, d * 0.9, -w / 2 + 2.2 * (i + 1), 1.15, 0, PALETTE.wood);
  b.roof(w + 1, 2, d + 1, 0, 3.5, 0, 0x8a5a2b, Math.PI / 2);
  return { w: w + 1, d: d + 1, h: 5.5, door: [0, d / 2 + 1.5] };
}

function farmhouse(b, rnd) {
  const r = house(b, rnd, false);
  b.box(3, 2.5, 3, r.w / 2 + 1.5, 1.25, -1, 0xc9a35a); // meule de foin
  return r;
}

function dock(b) {
  b.box(4, 0.4, 22, 0, 0, 0, PALETTE.wood);
  for (let i = 0; i < 6; i++) for (const sx of [-1, 1]) b.box(0.3, 4, 0.3, sx * 1.8, -1.8, -10 + i * 4, 0x4b2f1b);
  return { w: 4, d: 22, h: 1, door: [0, -12] };
}

function bridge(b) {
  const L = 32;
  b.box(5, 0.6, L, 0, 0, 0, 0x8d8a82);
  for (const sx of [-1, 1]) { b.box(0.4, 1.1, L, sx * 2.3, 0.8, 0, 0x9a948a); }
  for (const z of [-8, 0, 8]) b.box(4, 6, 2, 0, -3.2, z, 0x7b776f);
  return { w: 5, d: L, h: 1.4, door: [0, 0] };
}

function shrine(b) {
  // statue du dieu érigée par les fidèles : socle et silhouette stylisée
  const m = 0xe9e2d2;
  b.box(3.4, 1.2, 3.4, 0, 0.6, 0, 0xbdb6a8);
  b.box(1.0, 2.6, 0.7, 0, 2.5, 0, m);
  b.box(0.7, 0.7, 0.7, 0, 4.2, 0, m);
  b.cyl(0.8, 0.8, 0.12, 0, 4.4, -0.45, PALETTE.gold, 18);
  for (const s of [-1, 1]) { b.box(2.2, 1.5, 0.2, s * 1.4, 3.4, -0.3, m, 0, 0, s * -0.5); b.box(1.6, 1.0, 0.2, s * 2.2, 2.5, -0.4, m, 0, 0, s * -0.9); }
  return { w: 3.4, d: 3.4, h: 5.2, door: [0, 2.6] };
}

export const BUILDERS = { house, tavern, forge, temple, tower, windmill, stall, well, stable, farmhouse, dock, bridge, shrine };

export function makeBuilding(type, rnd) {
  const b = new Builder();
  const info = BUILDERS[type](b, rnd);
  return { geo: b.geometry(), pieces: b.pieces, ...info };
}

// Ailes du moulin (pièce animée séparée)
export function windmillSails() {
  const b = new Builder();
  for (let i = 0; i < 4; i++) {
    const a = i * Math.PI / 2;
    b.box(0.35, 7.5, 0.25, Math.sin(a) * 3.75, Math.cos(a) * 3.75, 0, PALETTE.wood, 0, 0, -a, false);
    b.box(1.6, 6, 0.08, Math.sin(a) * 4.1 + Math.cos(a) * 0.9, Math.cos(a) * 4.1 - Math.sin(a) * 0.9, 0.12, 0xeee4cc, 0, 0, -a, false);
  }
  b.cyl(0.5, 0.5, 0.8, 0, 0, 0, PALETTE.wood, 8, false);
  return b.geometry();
}

// Matériau des bâtiments : textures photo en projection triplanaire (espace objet, en mètres)
export function buildingMaterial(tex) {
  const m = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.85, metalness: 0.0 });
  if (!tex) return m;
  m.onBeforeCompile = (sh) => {
    sh.uniforms.tBDiff = { value: tex.diff }; sh.uniforms.tBNor = { value: tex.nor };
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', '#include <common>\nattribute float layer; varying float vLayer; varying vec3 vOP; varying vec3 vON; varying vec3 vNM0; varying vec3 vNM1; varying vec3 vNM2;')
      .replace('#include <begin_vertex>', '#include <begin_vertex>\nvLayer = layer; vOP = position; vON = normal; vNM0 = normalMatrix[0]; vNM1 = normalMatrix[1]; vNM2 = normalMatrix[2];');
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', `#include <common>
        precision highp sampler2DArray;
        uniform sampler2DArray tBDiff, tBNor; varying float vLayer; varying vec3 vOP; varying vec3 vON; varying vec3 vNM0; varying vec3 vNM1; varying vec3 vNM2;`)
      .replace('#include <map_fragment>', `
        vec3 bN = normalize(vON); vec3 bPert = vec3(0.0); float bRough = 0.85;
        if (vLayer > -0.5) {
          float L = floor(vLayer + 0.5);
          float sc = L == 2.0 || L == 3.0 || L == 4.0 ? 0.42 : (L == 1.0 ? 0.55 : 0.33);
          vec3 bw = pow(abs(bN), vec3(5.0)); bw /= (bw.x + bw.y + bw.z);
          vec3 p = vOP * sc;
          vec3 cx = texture(tBDiff, vec3(p.zy, L)).rgb, cy = texture(tBDiff, vec3(p.xz, L)).rgb, cz = texture(tBDiff, vec3(p.xy, L)).rgb;
          diffuseColor.rgb *= cx * bw.x + cy * bw.y + cz * bw.z;
          vec3 nx = texture(tBNor, vec3(p.zy, L)).xyz * 2.0 - 1.0, ny = texture(tBNor, vec3(p.xz, L)).xyz * 2.0 - 1.0, nz = texture(tBNor, vec3(p.xy, L)).xyz * 2.0 - 1.0;
          bPert = vec3(0.0, nx.y, nx.x) * sign(bN.x) * bw.x + vec3(ny.x, 0.0, -ny.y) * bw.y + vec3(nz.x, nz.y, 0.0) * sign(bN.z) * bw.z;
          bRough = L == 6.0 ? 0.45 : (L == 3.0 ? 0.7 : 0.88);
        }`)
      .replace('#include <roughnessmap_fragment>', 'float roughnessFactor = bRough;')
      .replace('#include <normal_fragment_maps>', 'normal = normalize(mat3(vNM0, vNM1, vNM2) * normalize(bN + bPert * 0.9)); if (!gl_FrontFacing) normal = -normal;');
  };
  return m;
}
