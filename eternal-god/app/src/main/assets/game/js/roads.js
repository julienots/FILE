// Réseau de chemins entre les villages : tracé selon le relief (A* sur une grille, évitant pentes
// et eau), chaussée aplanie et peinte, ponts aux traversées de rivière, panneaux indicateurs
// avec le nom des lieux, bornes et lanternes. Les tracés servent aussi aux voyageurs.
import * as THREE from 'three';
import { G, WORLD, clamp, lerp, mulberry32 } from './core.js';
import { distToRiver, riverBed, REGIONS } from './terrain.js';
import { Builder, makeBuilding } from './buildings.js';

const CELL = 6;

// tas binaire minimal pour l'A*
class Heap {
  constructor() { this.a = []; }
  get size() { return this.a.length; }
  push(n) { const a = this.a; a.push(n); let i = a.length - 1; while (i > 0) { const p = (i - 1) >> 1; if (a[p][0] <= a[i][0]) break; [a[p], a[i]] = [a[i], a[p]]; i = p; } }
  pop() {
    const a = this.a, top = a[0], last = a.pop();
    if (a.length) { a[0] = last; let i = 0; for (;;) { const l = i * 2 + 1, r = l + 1; let m = i; if (l < a.length && a[l][0] < a[m][0]) m = l; if (r < a.length && a[r][0] < a[m][0]) m = r; if (m === i) break; [a[m], a[i]] = [a[i], a[m]]; i = m; } }
    return top;
  }
}

export class Roads {
  constructor(scene, villages) {
    this.scene = scene;
    this.villages = villages;
    this.routes = [];          // { from, to, pts: Vector3[] }
    this.builder = new Builder();
    this.signs = [];
  }

  // A* sur une grille de 6 m ; coût : distance, pente, eau, rivière (franchissable par un pont)
  route(ax, az, bx, bz) {
    const T = G.terrain, N = Math.floor(WORLD.size / CELL), H = WORLD.half;
    const toI = (x) => clamp(Math.round((x + H) / CELL), 0, N - 1);
    const sx = toI(ax), sz = toI(az), gx = toI(bx), gz = toI(bz);
    const key = (i, j) => j * N + i;
    const cost = new Map(), from = new Map(), open = new Heap(); open.push([0, sx, sz]);
    cost.set(key(sx, sz), 0);
    const hAt = (i, j) => T.heightAt(i * CELL - H, j * CELL - H);
    let found = false, iter = 0;
    while (open.size && iter++ < 120000) {
      // file de priorité minimale (tas binaire simple)
      const [, i, j] = open.pop();
      if (i === gx && j === gz) { found = true; break; }
      const c0 = cost.get(key(i, j)), h0 = hAt(i, j);
      for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [1, -1], [-1, 1], [-1, -1]]) {
        const ni = i + di, nj = j + dj;
        if (ni < 2 || nj < 2 || ni > N - 3 || nj > N - 3) continue;
        const x = ni * CELL - H, z = nj * CELL - H, h1 = hAt(ni, nj);
        const step = Math.hypot(di, dj) * CELL;
        const slope = Math.abs(h1 - h0) / step;
        if (slope > 0.32) continue;
        let c = step * (1 + slope * slope * 60);
        const [dr] = distToRiver(x, z);
        if (dr < 14) c += 45;                               // traversée de rivière : pont (rare)
        else if (h1 < WORLD.water + 1.2) c += 400;           // lac et marais
        if (!this.villages.isFree(x, z, 1) && Math.hypot(x - bx, z - bz) > 30 && Math.hypot(x - ax, z - az) > 30) c += 60;
        const nc = c0 + c, k = key(ni, nj);
        if (nc < (cost.get(k) ?? Infinity)) { cost.set(k, nc); from.set(k, key(i, j)); open.push([nc + Math.hypot(gx - ni, gz - nj) * CELL, ni, nj]); }
      }
    }
    if (!found) return null;
    const pts = [];
    let k = key(gx, gz);
    while (k !== undefined) { const i = k % N, j = Math.floor(k / N); pts.push(new THREE.Vector3(i * CELL - H, 0, j * CELL - H)); k = from.get(k); }
    pts.reverse();
    pts[0].set(ax, 0, az); pts[pts.length - 1].set(bx, 0, bz);
    // lissage (Chaikin) pour des courbes naturelles
    let p = pts;
    for (let it = 0; it < 3; it++) {
      const q = [p[0]];
      for (let s = 0; s < p.length - 1; s++) { q.push(p[s].clone().lerp(p[s + 1], 0.25), p[s].clone().lerp(p[s + 1], 0.75)); }
      q.push(p[p.length - 1]); p = q;
    }
    for (const v of p) v.y = T.heightAt(v.x, v.z);
    return p;
  }

  // entrée d'un village : bout de la rue la plus orientée vers la destination
  entrance(v, tx, tz) {
    let best = null, bd = -2;
    for (const st of v.streetsPts) {
      const e = st[st.length - 1];
      const dx = e.x - v.x, dz = e.z - v.z, L = Math.hypot(dx, dz) || 1;
      const ux = tx - v.x, uz = tz - v.z, Lu = Math.hypot(ux, uz) || 1;
      const d = (dx * ux + dz * uz) / (L * Lu);
      if (d > bd) { bd = d; best = e; }
    }
    return best || new THREE.Vector3(v.x, 0, v.z);
  }

  build() {
    const V = this.villages.villages, main = V[0];
    const links = [[1, 0], [2, 0], [3, 0], [2, 3]];
    // route du Levant : du bourg vers l'est, au-delà de la rivière, jusqu'aux contreforts du volcan
    const east = { x: 290, z: -120, name: 'Mont Braise', streetsPts: [], id: -1 };
    for (const [a, b] of links) this.link(V[a], V[b]);
    this.link(main, east, true);
    this.finish();
  }

  link(A, B, toPoint = false) {
    const ea = this.entrance(A, B.x, B.z);
    const eb = toPoint ? new THREE.Vector3(B.x, 0, B.z) : this.entrance(B, A.x, A.z);
    const pts = this.route(ea.x, ea.z, eb.x, eb.z);
    if (!pts) return;
    const T = G.terrain;
    // chaussée : assise aplanie puis peinte
    for (let i = 0; i < pts.length; i += 2) {
      const p = pts[i];
      const [dr] = distToRiver(p.x, p.z);
      if (dr < 16 || T.heightAt(p.x, p.z) < WORLD.water + 0.8) continue;
      T.flatten(p.x, p.z, 2.6, T.heightAt(p.x, p.z));
    }
    for (let i = 1; i < pts.length; i++) {
      const [dr] = distToRiver(pts[i].x, pts[i].z);
      if (dr < 12) continue;
      T.paintPath(pts[i - 1].x, pts[i - 1].z, pts[i].x, pts[i].z, 4.2);
    }
    for (const p of pts) p.y = T.heightAt(p.x, p.z);
    this.routes.push({ from: A, to: B, pts });
    this.bridges(pts);
    this.dress(A, B, pts);
  }

  // ponts là où la route franchit la rivière
  bridges(pts) {
    const T = G.terrain;
    let start = -1;
    for (let i = 0; i < pts.length; i++) {
      const [dr] = distToRiver(pts[i].x, pts[i].z);
      const inside = dr < 13;
      if (inside && start < 0) start = i;
      if ((!inside || i === pts.length - 1) && start >= 0) {
        const a = pts[Math.max(0, start - 2)], b = pts[Math.min(pts.length - 1, i + 1)];
        const mid = a.clone().lerp(b, 0.5);
        const len = a.distanceTo(b) + 6;
        const ry = Math.atan2(b.x - a.x, b.z - a.z);
        const deck = Math.max(T.heightAt(a.x, a.z), T.heightAt(b.x, b.z)) + 0.4;
        const bb = new Builder();
        bb.box(5, 0.6, len, 0, 0, 0, 0x8d8a82);
        for (const sx of [-1, 1]) bb.box(0.45, 1.1, len, sx * 2.3, 0.8, 0, 0x9a948a);
        for (let k = -1; k <= 1; k++) bb.box(4, 9, 2.2, 0, -4.8, k * len / 3.2, 0x7b776f);
        const m = new THREE.Mesh(bb.geometry(), this.villages.mat);
        m.position.set(mid.x, deck, mid.z); m.rotation.y = ry; m.castShadow = true; m.receiveShadow = true;
        this.scene.add(m);
        this.villages.buildings.push({ id: this.villages.buildings.length, village: 0, type: 'bridge', mesh: m, x: mid.x, z: mid.z, y: deck, ry, w: 5, d: len, h: 2, rad: len / 2, pieces: bb.pieces, hp: 2500, maxHp: 2500, state: 'intact', door: mid.clone(), residents: [], workers: [], rebuild: 0, info: {} });
        for (let k = start; k <= i; k++) pts[k].y = deck + 0.3;
        start = -1;
      }
    }
  }

  // panneaux, bornes, lanternes, clôtures le long du chemin
  dress(A, B, pts) {
    const T = G.terrain, rnd = mulberry32(pts.length * 7 + 3), b = this.builder;
    let acc = 0, side = 1;
    for (let i = 1; i < pts.length - 1; i++) {
      acc += pts[i].distanceTo(pts[i - 1]);
      const dir = pts[i + 1].clone().sub(pts[i - 1]).setY(0).normalize();
      const nx = -dir.z * side, nz = dir.x * side;
      const x = pts[i].x + nx * 3.4, z = pts[i].z + nz * 3.4;
      const [dr] = distToRiver(x, z);
      if (dr < 16 || T.heightAt(x, z) < WORLD.water + 1) continue;
      if (acc > 110) { // borne de pierre
        acc = 0; side = -side;
        const y = T.heightAt(x, z);
        b.box(0.5, 0.9, 0.35, x, y + 0.4, z, 0x9a948a, Math.atan2(dir.x, dir.z), 0, 0, false);
        b.box(0.56, 0.12, 0.4, x, y + 0.9, z, 0x8d8a82, Math.atan2(dir.x, dir.z), 0, 0, false);
      }
      // lanternes près des villages
      const nearV = Math.min(Math.hypot(x - A.x, z - A.z), Math.hypot(x - B.x, z - B.z));
      if (nearV < 140 && i % 9 === 0) this.villages.addProp('wooden_lantern_01', x, z, rnd() * 6, 1.6);
      // petites clôtures de bord de route par endroits
      if (nearV < 200 && rnd() < 0.08) {
        const y = T.heightAt(x, z), ang = Math.atan2(dir.x, dir.z);
        for (let k = -2; k <= 2; k++) { const px = x + dir.x * k * 1.6, pz = z + dir.z * k * 1.6; b.box(0.14, 1.1, 0.14, px, T.heightAt(px, pz) + 0.5, pz, 0x664429, 0, 0, 0, false); }
        b.box(0.06, 0.09, 6.6, x, y + 0.75, z, 0x5a3b22, ang, 0, 0, false);
      }
    }
    // panneaux indicateurs aux deux extrémités
    this.sign(pts[2] || pts[0], pts[3] || pts[1], B.name, 1);
    if (B.id >= 0) this.sign(pts[pts.length - 3] || pts[pts.length - 1], pts[pts.length - 4] || pts[pts.length - 2], A.name, -1);
  }

  sign(p, q, label, s) {
    const T = G.terrain;
    const dir = q.clone().sub(p).setY(0).normalize();
    const x = p.x - dir.z * 3.2, z = p.z + dir.x * 3.2, y = T.heightAt(x, z);
    const b = this.builder;
    b.box(0.18, 2.6, 0.18, x, y + 1.3, z, 0x5a3b22, 0, 0, 0, false);
    // planche gravée du nom du lieu
    const c = document.createElement('canvas'); c.width = 512; c.height = 96;
    const ctx = c.getContext('2d');
    ctx.fillStyle = '#7a5634'; ctx.fillRect(0, 0, 512, 96);
    for (let i = 0; i < 18; i++) { ctx.strokeStyle = `rgba(40,25,10,${0.12 + Math.random() * 0.12})`; ctx.beginPath(); ctx.moveTo(0, Math.random() * 96); ctx.bezierCurveTo(170, Math.random() * 96, 340, Math.random() * 96, 512, Math.random() * 96); ctx.stroke(); }
    ctx.fillStyle = '#2b1a0c'; ctx.font = 'bold 46px Georgia, serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillText(label, 256, 50);
    const tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace; tex.anisotropy = 4;
    const board = new THREE.Mesh(new THREE.BoxGeometry(1.9, 0.38, 0.06), [
      new THREE.MeshStandardMaterial({ color: 0x6a4a2c, roughness: 0.9 }), new THREE.MeshStandardMaterial({ color: 0x6a4a2c, roughness: 0.9 }),
      new THREE.MeshStandardMaterial({ color: 0x6a4a2c, roughness: 0.9 }), new THREE.MeshStandardMaterial({ color: 0x6a4a2c, roughness: 0.9 }),
      new THREE.MeshStandardMaterial({ map: tex, roughness: 0.85 }), new THREE.MeshStandardMaterial({ map: tex, roughness: 0.85 })]);
    board.position.set(x + dir.x * 0.85, y + 2.2, z + dir.z * 0.85);
    board.rotation.y = Math.atan2(dir.x, dir.z) - Math.PI / 2;
    board.castShadow = true;
    this.scene.add(board);
    this.signs.push(board);
  }

  finish() {
    if (this.builder.geos.length) {
      const m = new THREE.Mesh(this.builder.geometry(), this.villages.mat);
      m.castShadow = true; m.receiveShadow = true;
      this.scene.add(m);
    }
  }

  // point aléatoire d'un trajet (pour les voyageurs)
  randomRoute() { return this.routes.length ? this.routes[Math.floor(Math.random() * this.routes.length)] : null; }
}
