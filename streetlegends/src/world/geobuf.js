// Constructeur de géométrie fusionnée (positions, couleurs, uv, paramètres de façade) pour le décor statique.
// Tout le décor d'un secteur tient dans un seul maillage → un seul appel de rendu.
import * as THREE from 'three';

const ICO = new THREE.IcosahedronGeometry(1, 1).toNonIndexed().attributes.position.array;
const ICO0 = new THREE.IcosahedronGeometry(1, 0).toNonIndexed().attributes.position.array;
const _c = new THREE.Color();

export class GeoBuf {
  constructor() {
    this.pos = []; this.col = []; this.uv = []; this.bw = [];
    this.ox = 0; this.oy = 0; this.oz = 0; this.cs = 1; this.sn = 0;
  }
  // Repère local : origine + rotation autour de Y (yaw). Local +z = avant, +x = gauche.
  at(x, y, z, yaw = 0) {
    this.ox = x; this.oy = y; this.oz = z; this.cs = Math.cos(yaw); this.sn = Math.sin(yaw);
    return this;
  }
  v(x, y, z, c, u, w, b) {
    this.pos.push(this.ox + x * this.cs + z * this.sn, this.oy + y, this.oz - x * this.sn + z * this.cs);
    this.col.push(c.r, c.g, c.b);
    this.uv.push(u, w);
    this.bw.push(b[0], b[1], b[2]);
  }
  color(hex, shade = 1) {
    _c.set(hex);
    return { r: _c.r * shade, g: _c.g * shade, b: _c.b * shade };
  }
  // quadrilatère p0..p3 dans le sens antihoraire vu de l'extérieur
  quad(p0, p1, p2, p3, c, uv = [0, 0, 1, 1], b = NOW) {
    const [u0, v0, u1, v1] = uv;
    this.v(p0[0], p0[1], p0[2], c, u0, v0, b); this.v(p1[0], p1[1], p1[2], c, u1, v0, b); this.v(p2[0], p2[1], p2[2], c, u1, v1, b);
    this.v(p0[0], p0[1], p0[2], c, u0, v0, b); this.v(p2[0], p2[1], p2[2], c, u1, v1, b); this.v(p3[0], p3[1], p3[2], c, u0, v1, b);
  }
  tri(p0, p1, p2, c, b = NOW) {
    this.v(p0[0], p0[1], p0[2], c, 0, 0, b); this.v(p1[0], p1[1], p1[2], c, 1, 0, b); this.v(p2[0], p2[1], p2[2], c, 0.5, 1, b);
  }
  // boîte centrée en (x, z), base à y. w le long de x, d le long de z. Les murs portent des uv en mètres.
  box(x, y, z, w, h, d, hex, b = NOW, o = {}) {
    const c = typeof hex === 'object' ? hex : this.color(hex), cs = typeof hex === 'object' ? { r: c.r * 0.82, g: c.g * 0.82, b: c.b * 0.82 } : this.color(hex, 0.82);
    const x0 = x - w / 2, x1 = x + w / 2, z0 = z - d / 2, z1 = z + d / 2, y1 = y + h, s = o.seed || 0;
    this.quad([x0, y, z1], [x1, y, z1], [x1, y1, z1], [x0, y1, z1], c, [s, 0, s + w, h], b); // +z
    this.quad([x1, y, z0], [x0, y, z0], [x0, y1, z0], [x1, y1, z0], c, [s + 7, 0, s + 7 + w, h], b); // -z
    this.quad([x1, y, z1], [x1, y, z0], [x1, y1, z0], [x1, y1, z1], cs, [s + 13, 0, s + 13 + d, h], b); // +x
    this.quad([x0, y, z0], [x0, y, z1], [x0, y1, z1], [x0, y1, z0], cs, [s + 19, 0, s + 19 + d, h], b); // -x
    if (!o.noTop) { const ct = o.top !== undefined ? this.color(o.top) : c; this.quad([x0, y1, z1], [x1, y1, z1], [x1, y1, z0], [x0, y1, z0], ct, [0, 0, 1, 1], NOW); }
    if (o.bottom) this.quad([x0, y, z0], [x1, y, z0], [x1, y, z1], [x0, y, z1], cs, [0, 0, 1, 1], NOW);
  }
  // toit à deux pans, faîtage le long de x
  gable(x, y, z, w, h, d, hex, wallHex) {
    const c = this.color(hex), c2 = this.color(hex, 0.8), cw = this.color(wallHex ?? hex, 0.9);
    const x0 = x - w / 2, x1 = x + w / 2, z0 = z - d / 2 - 0.3, z1 = z + d / 2 + 0.3, yt = y + h;
    this.quad([x0, y, z1], [x1, y, z1], [x1, yt, z], [x0, yt, z], c);
    this.quad([x1, y, z0], [x0, y, z0], [x0, yt, z], [x1, yt, z], c2);
    this.tri([x1, y, z1], [x1, y, z0], [x1, yt, z], cw);
    this.tri([x0, y, z0], [x0, y, z1], [x0, yt, z], cw);
  }
  cyl(x, y, z, r1, r2, h, n, hex, b = NOW, cap = true) {
    const c = this.color(hex);
    for (let i = 0; i < n; i++) {
      const a0 = (i / n) * Math.PI * 2, a1 = ((i + 1) / n) * Math.PI * 2;
      const sh = 0.78 + 0.22 * Math.cos(a0 + 0.6), cc = { r: c.r * sh, g: c.g * sh, b: c.b * sh };
      const c0 = Math.cos(a0), s0 = Math.sin(a0), c1 = Math.cos(a1), s1 = Math.sin(a1);
      if (r2 > 0.001) this.quad([x + c1 * r1, y, z + s1 * r1], [x + c0 * r1, y, z + s0 * r1], [x + c0 * r2, y + h, z + s0 * r2], [x + c1 * r2, y + h, z + s1 * r2], cc, [0, 0, 1, 1], b);
      else this.tri([x + c1 * r1, y, z + s1 * r1], [x + c0 * r1, y, z + s0 * r1], [x, y + h, z], cc, b);
      if (cap && r2 > 0.001) this.tri([x + c1 * r2, y + h, z + s1 * r2], [x + c0 * r2, y + h, z + s0 * r2], [x, y + h, z], c, b);
    }
  }
  blob(x, y, z, rx, ry, rz, hex, detail = 1, b = NOW, jitter = 0) {
    const src = detail ? ICO : ICO0, c = this.color(hex);
    for (let i = 0; i < src.length; i += 9) {
      const ny = (src[i + 1] + src[i + 4] + src[i + 7]) / 3, sh = 0.72 + 0.34 * (ny * 0.5 + 0.5), cc = { r: c.r * sh, g: c.g * sh, b: c.b * sh };
      for (let k = 0; k < 9; k += 3) {
        const j = jitter ? 1 + jitter * Math.sin(src[i + k] * 7.1 + src[i + k + 1] * 13.7 + src[i + k + 2] * 5.3 + x) : 1;
        this.v(x + src[i + k] * rx * j, y + src[i + k + 1] * ry * j, z + src[i + k + 2] * rz * j, cc, 0, 0, b);
      }
    }
  }
  get count() { return this.pos.length / 3; }
  build() {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(this.pos, 3));
    g.setAttribute('color', new THREE.Float32BufferAttribute(this.col, 3));
    g.setAttribute('uv', new THREE.Float32BufferAttribute(this.uv, 2));
    g.setAttribute('bw', new THREE.Float32BufferAttribute(this.bw, 3));
    g.computeVertexNormals();
    g.computeBoundingSphere();
    return g;
  }
}
// Paramètres de façade : [largeur cellule, hauteur cellule, type]
// type 0 = uni ; 1 = fenêtres ; 2 = mur-rideau vitré ; 3 = façade sombre néon ; 4 = petites fenêtres à volets ;
// 5 = émissif (x = intensité de jour, y = intensité de nuit) ; 6 = feu tricolore (x = lampe, y = phase)
export const NOW = [0, 0, 0];
export const WIN = (w = 3.2, h = 3.4) => [w, h, 1];
export const GLASS = (w = 2.2, h = 3.6) => [w, h, 2];
export const NEONW = (w = 3, h = 3.2) => [w, h, 3];
export const OLDW = (w = 2.6, h = 3) => [w, h, 4];
export const EMI = (day = 1, night = 2) => [day, night, 5];
export const TL = (lamp, phase) => [lamp, phase, 6];
