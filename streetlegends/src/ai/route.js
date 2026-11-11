// Itinéraire de course : polyligne lissée (trajectoire), courbure, profil de vitesse, suivi de progression.
import { clamp, lerp, dist, wrapAngle } from '../core/util.js';
import * as MAP from '../world/map.js';

export class Route {
  // pts : [{x,z,y,s,hw,type}] (axe de la route). closed : circuit fermé.
  constructor(pts, closed = false) {
    this.closed = closed;
    // en circuit, le dernier point double le premier : on le retire
    if (closed && pts.length > 2 && dist(pts[0].x, pts[0].z, pts[pts.length - 1].x, pts[pts.length - 1].z) < 1) pts = pts.slice(0, -1);
    this.c = pts; // axe
    const n = pts.length;
    this.n = n;
    // trajectoire : lissage contraint à la largeur de route (coupe les angles comme un pilote)
    const L = pts.map((p) => ({ x: p.x, z: p.z }));
    const I = (i) => (closed ? ((i % n) + n) % n : clamp(i, 0, n - 1));
    for (let pass = 0; pass < 22; pass++) {
      for (let i = closed ? 0 : 1; i < (closed ? n : n - 1); i++) {
        const a = L[I(i - 1)], b = L[I(i + 1)], p = L[i];
        let x = lerp(p.x, (a.x + b.x) / 2, 0.5), z = lerp(p.z, (a.z + b.z) / 2, 0.5);
        const o = pts[i], m = Math.max(0.3, o.hw - 3.3), dx = x - o.x, dz = z - o.z, d = Math.hypot(dx, dz);
        if (d > m) { x = o.x + (dx / d) * m; z = o.z + (dz / d) * m; }
        p.x = x; p.z = z;
      }
    }
    this.s = new Float32Array(n); this.k = new Float32Array(n); this.tx = new Float32Array(n); this.tz = new Float32Array(n);
    let s = 0;
    for (let i = 0; i < n; i++) {
      if (i > 0) s += dist(L[i].x, L[i].z, L[i - 1].x, L[i - 1].z);
      this.s[i] = s;
      L[i].y = pts[i].y; L[i].hw = pts[i].hw; L[i].type = pts[i].type;
    }
    this.len = s + (closed ? dist(L[0].x, L[0].z, L[n - 1].x, L[n - 1].z) : 0);
    this.p = L;
    for (let i = 0; i < n; i++) {
      const a = L[I(i - 1)], b = L[I(i + 1)], l = Math.hypot(b.x - a.x, b.z - a.z) || 1;
      this.tx[i] = (b.x - a.x) / l; this.tz[i] = (b.z - a.z) / l;
    }
    const raw = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const a = I(i - 2), b = I(i + 2);
      const dh = wrapAngle(Math.atan2(this.tx[b], this.tz[b]) - Math.atan2(this.tx[a], this.tz[a]));
      const ds = Math.max(4, dist(L[a].x, L[a].z, L[b].x, L[b].z));
      raw[i] = dh / ds;
    }
    for (let i = 0; i < n; i++) this.k[i] = (raw[I(i - 1)] + raw[i] * 2 + raw[I(i + 1)]) / 4;
  }
  idx(i) { return this.closed ? ((i % this.n) + this.n) % this.n : clamp(i, 0, this.n - 1); }
  // Profil de vitesse pour une adhérence et un freinage donnés.
  profile(mu, brake, vmax, surfMu = null) {
    const n = this.n, v = new Float32Array(n), g = 9.81;
    for (let i = 0; i < n; i++) {
      const m = mu * (surfMu ? surfMu(this.p[i].type) : 1);
      v[i] = Math.min(vmax, Math.sqrt((m * g) / Math.max(1e-4, Math.abs(this.k[i]))));
    }
    const pass = this.closed ? 2 * n : n;
    for (let q = pass - 2; q >= 0; q--) {
      const i = this.idx(q), j = this.idx(q + 1);
      if (!this.closed && q + 1 >= n) continue;
      const ds = Math.max(1, dist(this.p[i].x, this.p[i].z, this.p[j].x, this.p[j].z));
      v[i] = Math.min(v[i], Math.sqrt(v[j] * v[j] + 2 * brake * g * ds));
    }
    return v;
  }
  // Indice du point le plus proche, recherché autour d'un indice connu.
  nearest(x, z, hint = 0, back = 6, fwd = 30) {
    let best = hint, bd = Infinity;
    for (let q = hint - back; q <= hint + fwd; q++) {
      if (!this.closed && (q < 0 || q >= this.n)) continue;
      const i = this.idx(q), p = this.p[i], d = (p.x - x) * (p.x - x) + (p.z - z) * (p.z - z);
      if (d < bd) { bd = d; best = i; }
    }
    this._d = Math.sqrt(bd);
    return best;
  }
  nearestGlobal(x, z) {
    let best = 0, bd = Infinity;
    for (let i = 0; i < this.n; i++) { const p = this.p[i], d = (p.x - x) * (p.x - x) + (p.z - z) * (p.z - z); if (d < bd) { bd = d; best = i; } }
    this._d = Math.sqrt(bd);
    return best;
  }
  // Position à l'abscisse s (+ décalage latéral vers la gauche).
  at(s, off = 0, out = {}) {
    const n = this.n;
    if (this.closed) s = ((s % this.len) + this.len) % this.len; else s = clamp(s, 0, this.len);
    let lo = 0, hi = n - 1;
    while (hi - lo > 1) { const m = (lo + hi) >> 1; if (this.s[m] <= s) lo = m; else hi = m; }
    if (s >= this.s[n - 1]) { lo = n - 1; hi = this.closed ? 0 : n - 1; }
    const a = this.p[lo], b = this.p[hi], seg = (hi === 0 ? this.len : this.s[hi]) - this.s[lo] || 1, t = clamp((s - this.s[lo]) / seg, 0, 1);
    const tx = lerp(this.tx[lo], this.tx[hi], t), tz = lerp(this.tz[lo], this.tz[hi], t);
    out.x = lerp(a.x, b.x, t) + tz * off; out.z = lerp(a.z, b.z, t) - tx * off; out.y = lerp(a.y, b.y, t);
    out.tx = tx; out.tz = tz; out.hw = lerp(a.hw, b.hw, t); out.i = lo; out.yaw = Math.atan2(tx, tz);
    return out;
  }
  // Abscisse curviligne projetée d'un point proche de l'indice i.
  project(x, z, i) {
    const p = this.p[i];
    return this.s[i] + (x - p.x) * this.tx[i] + (z - p.z) * this.tz[i];
  }
  lateral(x, z, i) { const p = this.p[i]; return (x - p.x) * this.tz[i] - (z - p.z) * this.tx[i]; }
}

// Construit un itinéraire à partir de points de passage (coordonnées approximatives de carrefours).
export function buildRoute(waypoints, opts = {}) {
  const allow = opts.allow || ((e) => e.type !== 'sand' && e.type !== 'dirt' && e.type !== 'track');
  const nf = (n) => n.edges.some((id) => allow(MAP.edges[id]));
  const ids = waypoints.map((w) => MAP.nearestNode(w[0], w[1], nf));
  if (opts.closed) ids.push(ids[0]);
  const clean = ids.filter((v, i) => i === 0 || v !== ids[i - 1]);
  const pts = MAP.routeThrough(clean, allow);
  if (!pts || pts.length < 4) return null;
  return new Route(pts, !!opts.closed);
}
