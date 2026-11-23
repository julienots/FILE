// Petit constructeur de géométries « cartoon » : primitives fusionnées avec couleurs par sommet.
import * as THREE from 'three';
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js';

const col = new THREE.Color();
const m4 = new THREE.Matrix4(); const q = new THREE.Quaternion(); const e = new THREE.Euler(); const v = new THREE.Vector3(); const s = new THREE.Vector3();

export class Builder {
  constructor() { this.parts = []; }
  push(g, color, x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1) {
    g = g.index ? g.toNonIndexed() : g;
    g.deleteAttribute('uv');
    e.set(rx, ry, rz); q.setFromEuler(e); v.set(x, y, z); s.set(sx, sy, sz);
    m4.compose(v, q, s); g.applyMatrix4(m4);
    col.set(color);
    const n = g.attributes.position.count; const c = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { c[i * 3] = col.r; c[i * 3 + 1] = col.g; c[i * 3 + 2] = col.b; }
    g.setAttribute('color', new THREE.BufferAttribute(c, 3));
    this.parts.push(g); return this;
  }
  // boîte posée : y = bas de la boîte
  box(w, h, d, x, y, z, color, ry = 0, rx = 0, rz = 0) { return this.push(roundedBox(w, h, d), color, x, y + h / 2, z, rx, ry, rz); }
  rbox(w, h, d, x, y, z, color, ry = 0) { return this.push(roundedBox(w, h, d, Math.min(w, h, d) * 0.18), color, x, y + h / 2, z, 0, ry, 0); }
  cyl(rt, rb, h, x, y, z, color, seg = 12, rx = 0, rz = 0) { return this.push(new THREE.CylinderGeometry(rt, rb, h, seg), color, x, y + h / 2, z, rx, 0, rz); }
  sphere(r, x, y, z, color, sx = 1, sy = 1, sz = 1, seg = 12) { return this.push(new THREE.SphereGeometry(r, seg, Math.max(6, seg * 0.66 | 0)), color, x, y, z, 0, 0, 0, sx, sy, sz); }
  cone(r, h, x, y, z, color, seg = 10) { return this.push(new THREE.ConeGeometry(r, h, seg), color, x, y + h / 2, z); }
  torus(r, t, x, y, z, color, rx = 0, ry = 0, arc = Math.PI * 2) { return this.push(new THREE.TorusGeometry(r, t, 6, 16, arc), color, x, y, z, rx, ry, 0); }
  build() { const g = mergeGeometries(this.parts, false); g.computeVertexNormals(); g.computeBoundingSphere(); return g; }
}

const boxCache = new Map();
export function roundedBox(w, h, d, r = 0) {
  if (!r) return new THREE.BoxGeometry(w, h, d);
  const k = `${w}|${h}|${d}|${r}`;
  if (boxCache.has(k)) return boxCache.get(k).clone();
  // boîte arrondie simple par déformation d'une boîte subdivisée
  const g = new THREE.BoxGeometry(w, h, d, 3, 3, 3);
  const p = g.attributes.position; const hw = w / 2 - r, hh = h / 2 - r, hd = d / 2 - r;
  for (let i = 0; i < p.count; i++) {
    const x = p.getX(i), y = p.getY(i), z = p.getZ(i);
    const cx = Math.max(-hw, Math.min(hw, x)), cy = Math.max(-hh, Math.min(hh, y)), cz = Math.max(-hd, Math.min(hd, z));
    v.set(x - cx, y - cy, z - cz); if (v.lengthSq() > 0) v.normalize().multiplyScalar(r);
    p.setXYZ(i, cx + v.x, cy + v.y, cz + v.z);
  }
  boxCache.set(k, g);
  return g.clone();
}
