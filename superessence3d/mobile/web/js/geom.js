// Géométries procédurales (repère Y vers le haut, l'avant des créatures vers +Z).
import * as THREE from "three";
import { RoundedBoxGeometry } from "../vendor/three/addons/utils/RoundedBoxGeometry.js";
import { mergeGeometries } from "../vendor/three/addons/utils/BufferGeometryUtils.js";

export const V = (x, y, z) => new THREE.Vector3(x, y, z);

export function taper(n, r0, r1 = 0, power = 1) {
  return Array.from({ length: n }, (_, i) => r0 + (r1 - r0) * Math.pow(i / Math.max(1, n - 1), power));
}

/** Points d'une courbe : départ, direction, longueur, courbure totale (rad) autour d'un axe. */
export function curvePoints(start, dir, length, segments = 8, bendAxis = V(1, 0, 0), bend = 0) {
  const pts = [start.clone()];
  const d = dir.clone().normalize();
  const axis = bendAxis.clone().normalize();
  const q = new THREE.Quaternion().setFromAxisAngle(axis, bend / segments);
  const p = start.clone();
  for (let i = 0; i < segments; i++) {
    d.applyQuaternion(q).normalize();
    p.addScaledVector(d, length / segments);
    pts.push(p.clone());
  }
  return pts;
}

/** Tube effilé le long d'une polyligne (repères à transport parallèle). */
export function sweep(path, radii, { sides = 10, tip = false, flatten = 1, profile = null, cap = true } = {}) {
  const n = path.length;
  const tangents = path.map((p, i) => {
    const t = i === 0 ? path[1].clone().sub(path[0]) : i === n - 1 ? path[n - 1].clone().sub(path[n - 2]) : path[i + 1].clone().sub(path[i - 1]);
    return t.lengthSq() > 1e-12 ? t.normalize() : V(0, 1, 0);
  });
  let normal = Math.abs(tangents[0].y) < 0.9 ? V(0, 1, 0).cross(tangents[0]).normalize() : V(1, 0, 0).cross(tangents[0]).normalize();
  const prof = profile || Array.from({ length: sides }, (_, k) => [Math.cos(2 * Math.PI * k / sides), Math.sin(2 * Math.PI * k / sides) * flatten]);
  const K = prof.length;
  const pos = [], idx = [];
  const rings = [];
  for (let i = 0; i < n; i++) {
    if (i > 0) {
      const axis = tangents[i - 1].clone().cross(tangents[i]);
      if (axis.lengthSq() > 1e-12) normal.applyAxisAngle(axis.normalize(), tangents[i - 1].angleTo(tangents[i]));
    }
    const bin = tangents[i].clone().cross(normal).normalize();
    const r = radii[i];
    const start = pos.length / 3;
    if (tip && i === n - 1) { pos.push(path[i].x, path[i].y, path[i].z); rings.push([start]); continue; }
    for (const [x, y] of prof) {
      const p = path[i].clone().addScaledVector(normal, x * r).addScaledVector(bin, y * r);
      pos.push(p.x, p.y, p.z);
    }
    rings.push(Array.from({ length: K }, (_, k) => start + k));
  }
  for (let i = 0; i < n - 1; i++) {
    const a = rings[i], b = rings[i + 1];
    for (let k = 0; k < K; k++) {
      const k2 = (k + 1) % K;
      if (b.length === 1) idx.push(a[k], a[k2], b[0]);
      else idx.push(a[k], a[k2], b[k2], a[k], b[k2], b[k]);
    }
  }
  if (cap && rings[0].length > 2) {
    const c = pos.length / 3; pos.push(path[0].x, path[0].y, path[0].z);
    for (let k = 0; k < K; k++) idx.push(c, rings[0][(k + 1) % K], rings[0][k]);
  }
  if (cap && !tip && rings[n - 1].length > 2) {
    const c = pos.length / 3; pos.push(path[n - 1].x, path[n - 1].y, path[n - 1].z);
    for (let k = 0; k < K; k++) idx.push(c, rings[n - 1][k], rings[n - 1][(k + 1) % K]);
  }
  return finish(pos, idx);
}

function finish(pos, idx) {
  const g = new THREE.BufferGeometry();
  g.setAttribute("position", new THREE.Float32BufferAttribute(pos, 3));
  g.setIndex(idx);
  g.computeVertexNormals();
  return g;
}

/** Révolution d'un profil [[rayon, hauteur], ...] autour de Y. */
export function lathe(profile, segments = 32) {
  return new THREE.LatheGeometry(profile.map(([r, y]) => new THREE.Vector2(Math.max(r, 0.0001), y)), segments);
}

/** Surface passant par des sections fermées de même nombre de points. */
export function loft(sections, cap = true) {
  const K = sections[0].length;
  const pos = [], idx = [];
  sections.forEach((s) => s.forEach((p) => pos.push(p.x, p.y, p.z)));
  for (let i = 0; i < sections.length - 1; i++) for (let k = 0; k < K; k++) {
    const a = i * K + k, b = i * K + (k + 1) % K, c = (i + 1) * K + (k + 1) % K, d = (i + 1) * K + k;
    idx.push(a, b, c, a, c, d);
  }
  if (cap) {
    const last = (sections.length - 1) * K;
    const c0 = pos.length / 3; const s0 = sections[0].reduce((a, p) => a.add(p), V(0, 0, 0)).divideScalar(K); pos.push(s0.x, s0.y, s0.z);
    const c1 = c0 + 1; const s1 = sections.at(-1).reduce((a, p) => a.add(p), V(0, 0, 0)).divideScalar(K); pos.push(s1.x, s1.y, s1.z);
    for (let k = 0; k < K; k++) { idx.push(c0, (k + 1) % K, k); idx.push(c1, last + k, last + (k + 1) % K); }
  }
  return finish(pos, idx);
}

/** Section elliptique dans le plan XY à la profondeur z. */
export function ellipseSection(cx, cy, z, rx, ry, n = 16, squareness = 0) {
  return Array.from({ length: n }, (_, k) => {
    const a = 2 * Math.PI * k / n;
    let c = Math.cos(a), s = Math.sin(a);
    if (squareness) { const e = 2 / (2 + 6 * squareness); c = Math.sign(c) * Math.abs(c) ** e; s = Math.sign(s) * Math.abs(s) ** e; }
    return V(cx + rx * c, cy + ry * s, z);
  });
}

/** Membrane en éventail depuis un point commun W, entre des rangées de points (ailes, nageoires, voiles). */
export function fanMembrane(W, rows, sag = 0.06) {
  const all = [];
  for (let r = 0; r < rows.length - 1; r++) {
    const a = rows[r], b = rows[r + 1];
    for (const t of [0, 0.34, 0.67]) {
      const row = [];
      for (let j = 0; j < a.length; j++) {
        const p = a[j].clone().lerp(b[j], t);
        const s = Math.sin(Math.PI * t) * (a.length > 1 ? j / (a.length - 1) : 1) * sag;
        p.y -= s;
        p.add(W.clone().sub(p).normalize().multiplyScalar(s * 0.6));
        row.push(p);
      }
      all.push(row);
    }
  }
  all.push(rows.at(-1).map((p) => p.clone()));
  const pos = [W.x, W.y, W.z], idx = [];
  const M = all[0].length;
  all.forEach((row) => row.forEach((p) => pos.push(p.x, p.y, p.z)));
  for (let r = 0; r < all.length - 1; r++) {
    const a = 1 + r * M, b = 1 + (r + 1) * M;
    idx.push(0, a, b);
    for (let j = 0; j < M - 1; j++) idx.push(a + j, a + j + 1, b + j + 1, a + j, b + j + 1, b + j);
  }
  const g = finish(pos, idx);
  return doubleSided(g);
}

/** Ajoute la face arrière (membranes fines visibles des deux côtés, y compris à l'export). */
export function doubleSided(g, thickness = 0) {
  const p = g.attributes.position.array, ix = g.index.array;
  const n = p.length / 3;
  const nrm = g.attributes.normal.array;
  const pos = new Float32Array(p.length * 2);
  pos.set(p);
  for (let i = 0; i < n; i++) for (let k = 0; k < 3; k++) pos[(n + i) * 3 + k] = p[i * 3 + k] - nrm[i * 3 + k] * thickness;
  const idx = [...ix];
  for (let i = 0; i < ix.length; i += 3) idx.push(n + ix[i], n + ix[i + 2], n + ix[i + 1]);
  return finish(pos, idx);
}

export function box(w, h, d, r = 0) {
  return r > 0 ? new RoundedBoxGeometry(w, h, d, 2, Math.min(r, Math.min(w, h, d) / 2.01)) : new THREE.BoxGeometry(w, h, d);
}

export function ellipsoid(rx, ry, rz, ws = 20, hs = 12) {
  const g = new THREE.SphereGeometry(1, ws, hs);
  g.scale(rx, ry, rz);
  return g;
}

export function cylinder(r, h, segs = 24, rTop = null) {
  return new THREE.CylinderGeometry(rTop ?? r, r, h, segs);
}

/** Positionne / oriente une géométrie (copie). */
export function place(g, { pos = null, rot = null, scale = null } = {}) {
  const m = new THREE.Matrix4();
  m.compose(pos || V(0, 0, 0), new THREE.Quaternion().setFromEuler(new THREE.Euler(...(rot || [0, 0, 0]))), scale || V(1, 1, 1));
  return g.applyMatrix4(m);
}

export function merge(geos) {
  const clean = geos.filter(Boolean).map((g) => {
    if (!g.attributes.normal) g.computeVertexNormals();
    const x = g.index ? g.toNonIndexed() : g;
    for (const k of Object.keys(x.attributes)) if (!["position", "normal"].includes(k)) x.deleteAttribute(k);
    return x;
  });
  return mergeGeometries(clean, false);
}

export function mirrorX(points) { return points.map((p) => V(-p.x, p.y, p.z)); }
