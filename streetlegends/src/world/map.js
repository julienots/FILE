// VICE CITY DISTRICT — données du monde : graphe routier, relief, zones, bâtiments, murs de collision.
// Module purement logique (pas de rendu) : il sert à la physique, à l'IA, à la circulation, à la carte et au rendu.
import { clamp, lerp, smoothstep, noise2, rng, catmull, dist } from '../core/util.js';

export const BOUNDS = { x0: -2500, x1: 3000, z0: -2500, z1: 2000 };
export const SEA_LEVEL = -1.3;

export const ROAD = {
  highway: { w: 26, speed: 36, lanes: 3, surf: 'asphalt', traffic: 1.4 },
  avenue: { w: 18, speed: 19, lanes: 2, surf: 'asphalt', traffic: 1.2 },
  street: { w: 11, speed: 14, lanes: 1, surf: 'asphalt', traffic: 1 },
  alley: { w: 8, speed: 9, lanes: 1, surf: 'cobble', traffic: 0.35 },
  industrial: { w: 13, speed: 14, lanes: 1, surf: 'asphalt', traffic: 0.6 },
  coastal: { w: 12.5, speed: 22, lanes: 1, surf: 'asphalt', traffic: 0.8 },
  mountain: { w: 10.5, speed: 17, lanes: 1, surf: 'asphalt', traffic: 0.3 },
  track: { w: 15, speed: 50, lanes: 0, surf: 'asphalt', traffic: 0 },
  dirt: { w: 8.5, speed: 14, lanes: 0, surf: 'dirt', traffic: 0 },
  sand: { w: 10, speed: 14, lanes: 0, surf: 'sand', traffic: 0 },
};

export const ZONES = {
  downtown: { name: 'Downtown', color: '#5aa9ff', desc: 'Gratte-ciel, grandes avenues, tunnels et parkings.' },
  coastal: { name: 'Coastal Bay', color: '#ffd166', desc: 'Plages, palmiers, villas et route côtière.' },
  industrial: { name: 'Industrial Zone', color: '#b08968', desc: 'Entrepôts, usines et quais du port.' },
  mountain: { name: 'Mountain Roads', color: '#7bd389', desc: 'Lacets, tunnels, panoramas et descentes rapides.' },
  oldtown: { name: 'Old Town', color: '#ff9f6b', desc: 'Ruelles pavées, places et toits de tuiles.' },
  neon: { name: 'Neon District', color: '#ff4fd8', desc: 'Enseignes lumineuses et vie nocturne.' },
  highway: { name: 'Highway', color: '#c0c7d6', desc: 'Le périphérique : vitesse pure.' },
};

export const coastZ = (x) => 1720 + 90 * Math.sin(x / 600) + 40 * Math.sin(x / 230 + 1);

export function zoneAt(x, z) {
  if (z < -1040) return 'mountain';
  if (Math.abs(x) <= 690 && Math.abs(z) <= 690) return 'downtown';
  if (x >= 810 && x <= 1590 && z >= -390 && z <= 390) return 'neon';
  if (x >= -1600 && x <= -800 && z >= -200 && z <= 600) return 'oldtown';
  if (x >= 800 && x <= 1900 && z >= 760) return 'industrial';
  if (z >= 860 && x < 800) return 'coastal';
  return 'highway';
}

// Relief de base (sans les routes).
export function baseHeight(x, z) {
  let h = 0;
  const m = smoothstep(-1060, -1850, z);
  if (m > 0) h += m * (24 + 100 * noise2(x / 720 + 7.3, z / 720 + 2.1) + 14 * noise2(x / 170, z / 170));
  // collines de bordure (ouest / est / nord)
  const bx = Math.max(smoothstep(2300, 2500, -x), smoothstep(2850, 3000, x), smoothstep(2380, 2500, -z));
  if (bx > 0) h += bx * (50 + 40 * noise2(x / 200, z / 200));
  // campagne vallonnée hors du périphérique, à l'ouest et à l'est (pas sur l'autodrome)
  const c = z - coastZ(x);
  if (c > 0 && !(x > 860 && x < 1900 && c < 150)) h -= smoothstep(0, 170, c) * 8;
  else if (c > 0) h -= smoothstep(60, 170, c) * 8;
  return h;
}

// ---------------------------------------------------------------------------------------------
// Construction du graphe
// ---------------------------------------------------------------------------------------------
export const nodes = [];
export const edges = [];
const nodeKey = new Map();

function node(x, z) {
  const k = Math.round(x) + '|' + Math.round(z);
  let n = nodeKey.get(k);
  if (n === undefined) {
    n = nodes.length;
    nodes.push({ id: n, x, z, y: 0, edges: [], r: 0 });
    nodeKey.set(k, n);
  }
  return n;
}

function resample(poly, step) {
  const out = [poly[0]];
  let carry = 0;
  for (let i = 1; i < poly.length; i++) {
    const a = poly[i - 1], b = poly[i];
    const L = dist(a[0], a[1], b[0], b[1]);
    let d = step - carry;
    while (d < L) {
      out.push([lerp(a[0], b[0], d / L), lerp(a[1], b[1], d / L)]);
      d += step;
    }
    carry = L - (d - step);
  }
  const last = poly[poly.length - 1], pl = out[out.length - 1];
  if (dist(last[0], last[1], pl[0], pl[1]) < step * 0.4 && out.length > 1) out.pop();
  out.push(last);
  return out;
}

function addEdge(a, b, type, poly, opts = {}) {
  const pts = resample(poly, 11).map((p) => ({ x: p[0], z: p[1], y: 0, s: 0 }));
  let s = 0;
  for (let i = 1; i < pts.length; i++) {
    s += dist(pts[i].x, pts[i].z, pts[i - 1].x, pts[i - 1].z);
    pts[i].s = s;
  }
  const e = { id: edges.length, a, b, type, pts, len: s, hw: ROAD[type].w / 2, tunnel: !!opts.tunnel, name: opts.name || '' };
  edges.push(e);
  nodes[a].edges.push(e.id);
  nodes[b].edges.push(e.id);
  return e;
}
const straight = (ax, az, bx, bz, type, opts) => addEdge(node(ax, az), node(bx, bz), type, [[ax, az], [bx, bz]], opts);

// Chaîne de points de contrôle reliés par une spline continue ; les points marqués 'J' deviennent des carrefours.
function chain(ctrl, type, opts = {}) {
  const n = ctrl.length, closed = !!opts.closed;
  const P = (i) => (closed ? ctrl[((i % n) + n) % n] : ctrl[clamp(i, 0, n - 1)]);
  const isJ = (i) => (closed ? ctrl[((i % n) + n) % n][2] === 'J' : i === 0 || i === n - 1 || ctrl[i][2] === 'J');
  const segs = closed ? n : n - 1;
  let start = 0;
  if (closed) while (!isJ(start)) start++;
  let poly = [[P(start)[0], P(start)[1]]], from = start;
  for (let k = 0; k < segs; k++) {
    const i = start + k;
    for (let t = 1; t <= 10; t++) poly.push(catmull(P(i - 1), P(i), P(i + 1), P(i + 2), t / 10));
    if (isJ(i + 1) || k === segs - 1) {
      const A = P(from), B = P(i + 1);
      poly[poly.length - 1] = [B[0], B[1]];
      addEdge(node(A[0], A[1]), node(B[0], B[1]), type, poly, { ...opts, tunnel: opts.tunnelAt ? opts.tunnelAt(from, i + 1) : false });
      from = i + 1;
      poly = [[B[0], B[1]]];
    }
  }
}

const R = rng(20260);

// --- Downtown : grille 9x9 ---
for (let i = 0; i < 9; i++) for (let j = 0; j < 9; j++) {
  const x = -600 + i * 150, z = -600 + j * 150;
  const av = (v) => v % 4 === 0;
  if (i < 8) straight(x, z, x + 150, z, av(j) ? 'avenue' : 'street', { tunnel: j === 4 && (i === 1 || i === 6) });
  if (j < 8) straight(x, z, x, z + 150, av(i) ? 'avenue' : 'street');
}
// --- Neon District : grille 6x6 ---
for (let i = 0; i < 6; i++) for (let j = 0; j < 6; j++) {
  const x = 900 + i * 120, z = -300 + j * 120;
  if (i < 5) straight(x, z, x + 120, z, j === 2 || j === 3 ? 'avenue' : 'street');
  if (j < 5) straight(x, z, x, z + 120, 'street');
}
// --- Old Town : grille irrégulière de ruelles ---
const OT = [];
for (let i = 0; i < 7; i++) {
  OT.push([]);
  for (let j = 0; j < 7; j++) {
    const border = i === 0 || j === 0 || i === 6 || j === 6;
    const jx = border ? R.range(-8, 8) : R.range(-26, 26), jz = border ? R.range(-8, 8) : R.range(-26, 26);
    OT[i].push([Math.round(-1500 + i * 100 + jx), Math.round(-100 + j * 100 + jz)]);
  }
}
for (let i = 0; i < 7; i++) for (let j = 0; j < 7; j++) {
  const p = OT[i][j];
  const keep = (bi, bj) => bi === 0 || bj === 0 || bi === 6 || bj === 6 || i === 3 || j === 3 || R() > 0.2;
  if (i < 6 && (j === 0 || j === 6 || j === 3 || keep(i, j))) straight(p[0], p[1], OT[i + 1][j][0], OT[i + 1][j][1], 'alley');
  if (j < 6 && (i === 0 || i === 6 || i === 3 || keep(i, j))) straight(p[0], p[1], OT[i][j + 1][0], OT[i][j + 1][1], 'alley');
}
// --- Industrial Zone : grille 5x4 ---
for (let i = 0; i < 5; i++) for (let j = 0; j < 4; j++) {
  const x = 900 + i * 200, z = 800 + j * 200;
  if (i < 4) straight(x, z, x + 200, z, 'industrial');
  if (j < 3) straight(x, z, x, z + 200, 'industrial');
}
// --- Highway : périphérique fermé ---
chain([
  [-1200, -940, 'J'], [-600, -950, 'J'], [0, -960, 'J'], [600, -950, 'J'], [1200, -940, 'J'], [1750, -850], [2040, -430],
  [2060, -60, 'J'], [2050, 350, 'J'], [1860, 640], [1300, 720, 'J'], [300, 740, 'J'], [-300, 740, 'J'], [-1200, 730, 'J'],
  [-1800, 650], [-2050, 300], [-2060, -60, 'J'], [-2040, -450], [-1750, -850],
], 'highway', { closed: true, name: 'Périphérique', tunnelAt: (a, b) => a === 3 });
// --- Liaisons ---
for (const x of [-600, 0, 600]) straight(x, -600, x, x === 0 ? -960 : -950, 'avenue');
straight(-300, 600, -300, 740, 'avenue');
straight(300, 600, 300, 740, 'avenue');
const cz = (x) => Math.round(coastZ(x) - 115);
straight(-300, 740, -300, 1100, 'avenue');
straight(-300, 1100, -300, cz(-300), 'avenue');
straight(300, 740, 300, 1100, 'avenue');
straight(300, 1100, 300, cz(300), 'avenue');
addEdge(node(600, -150), node(900, -180), 'avenue', [[600, -150], [900, -180]]);
addEdge(node(600, 150), node(900, 180), 'avenue', [[600, 150], [900, 180]]);
straight(1500, -60, 2060, -60, 'avenue');
straight(1260, 300, 1300, 720, 'street');
straight(1300, 720, 1300, 800, 'industrial');
straight(1140, -300, 1200, -940, 'street');
straight(-600, 150, OT[6][3][0], OT[6][3][1], 'street');
straight(-600, -150, OT[6][0][0], OT[6][0][1], 'street');
straight(OT[0][1][0], OT[0][1][1], -2060, -60, 'street');
straight(OT[3][6][0], OT[3][6][1], -1200, 730, 'street');
straight(OT[3][0][0], OT[3][0][1], -1200, -940, 'street');
// --- Coastal Bay ---
chain([[-1200, 730], [-1350, 1100, 'J'], [-1300, cz(-1300)]], 'coastal');
{
  const c = [];
  for (const x of [-1300, -1050, -800, -550, -300, 0, 300, 500, 700]) c.push([x, cz(x), [-1300, -800, -300, 300, 700].includes(x) ? 'J' : '']);
  c.push([820, 1560], [900, 1400]);
  chain(c, 'coastal', { name: 'Route de la Baie' });
  const s = [[-1300, cz(-1300)]];
  for (const x of [-1150, -900, -600, -300, 0, 300, 520]) s.push([x, Math.round(coastZ(x) - 42)]);
  s.push([700, cz(700)]);
  chain(s, 'sand', { name: 'Piste de la plage' });
}
straight(-1350, 1100, -900, 1100, 'street');
straight(-900, 1100, -300, 1100, 'street');
straight(-300, 1100, 300, 1100, 'street');
straight(300, 1100, 900, 1000, 'street');
straight(-900, 1100, -800, cz(-800), 'street');
// --- Mountain Roads ---
chain([
  [-1200, -940], [-1300, -1150], [-1050, -1350, 'J'], [-1350, -1550], [-1000, -1750], [-1250, -1950], [-800, -2150, 'J'],
  [-300, -2050], [0, -2250], [350, -2050, 'J'], [700, -2200], [1050, -1950], [800, -1700, 'J'], [1200, -1500], [950, -1250], [1200, -940],
], 'mountain', { name: 'Route des Crêtes', tunnelAt: (a, b) => a === 6 && b === 9 });
chain([[0, -960], [120, -1200], [-150, -1450, 'J'], [150, -1700, 'J'], [-50, -1900], [350, -2050]], 'mountain', { name: 'Col du Centre' });
chain([[-1050, -1350], [-700, -1500], [-400, -1300], [-150, -1450]], 'dirt', { name: 'Piste des Pins' });
chain([[150, -1700], [500, -1600], [800, -1700]], 'dirt', { name: 'Piste du Ravin' });
// --- Autodrome (circuit fermé) ---
straight(2050, 350, 2300, 350, 'street');
chain([[2300, 350, 'J'], [2350, 0], [2600, -200], [2820, 0, 'J'], [2650, 300], [2820, 600], [2600, 850, 'J'], [2350, 700]], 'track', { closed: true, name: 'Autodrome Superessence' });

// ---------------------------------------------------------------------------------------------
// Altitudes des routes : relief lissé le long de chaque arête
// ---------------------------------------------------------------------------------------------
for (const n of nodes) n.y = baseHeight(n.x, n.z);
for (const e of edges) for (const q of e.pts) q.y = baseHeight(q.x, q.z);
for (let pass = 0; pass < 700; pass++) {
  for (const e of edges) {
    const p = e.pts, n = p.length;
    p[0].y = nodes[e.a].y; p[n - 1].y = nodes[e.b].y;
    let prev = p[0].y;
    for (let i = 1; i < n - 1; i++) {
      const cur = p[i].y;
      p[i].y = (prev + cur * 2 + p[i + 1].y) / 4;
      prev = cur;
    }
  }
  // les carrefours suivent la moyenne de leurs voisins : pas de marche aux intersections
  for (const n of nodes) {
    let sum = 0;
    for (const id of n.edges) { const e = edges[id]; sum += e.a === n.id ? e.pts[1].y : e.pts[e.pts.length - 2].y; }
    n.y = (n.y * 2 + sum) / (2 + n.edges.length);
  }
}
for (const e of edges) { e.pts[0].y = nodes[e.a].y; e.pts[e.pts.length - 1].y = nodes[e.b].y; }
for (const n of nodes) n.r = Math.max(...n.edges.map((id) => edges[id].hw)) + 1;

// ---------------------------------------------------------------------------------------------
// Index spatial des segments de route
// ---------------------------------------------------------------------------------------------
const CELL = 64, GW = Math.ceil((BOUNDS.x1 - BOUNDS.x0) / CELL) + 2, GH = Math.ceil((BOUNDS.z1 - BOUNDS.z0) / CELL) + 2;
const grid = new Array(GW * GH);
const cellOf = (x, z) => {
  const cx = clamp(Math.floor((x - BOUNDS.x0) / CELL) + 1, 0, GW - 1), czz = clamp(Math.floor((z - BOUNDS.z0) / CELL) + 1, 0, GH - 1);
  return cx + czz * GW;
};
for (const e of edges) for (let i = 0; i < e.pts.length - 1; i++) {
  const a = e.pts[i], b = e.pts[i + 1];
  const set = new Set([cellOf(a.x, a.z), cellOf(b.x, b.z), cellOf((a.x + b.x) / 2, (a.z + b.z) / 2)]);
  for (const c of set) (grid[c] || (grid[c] = [])).push(e.id, i);
}

const _near = { edge: null, seg: 0, t: 0, d: Infinity, x: 0, z: 0, y: 0, hw: 0, tx: 0, tz: 1 };
// Route la plus proche dans un rayon maxD. Réutilise un objet résultat (ne pas conserver la référence).
export function nearestRoad(x, z, maxD = 60, out = _near, filter = null) {
  out.edge = null; out.d = maxD;
  const r = Math.ceil(maxD / CELL);
  const cx = Math.floor((x - BOUNDS.x0) / CELL) + 1, czz = Math.floor((z - BOUNDS.z0) / CELL) + 1;
  for (let gz = czz - r; gz <= czz + r; gz++) {
    if (gz < 0 || gz >= GH) continue;
    for (let gx = cx - r; gx <= cx + r; gx++) {
      if (gx < 0 || gx >= GW) continue;
      const list = grid[gx + gz * GW];
      if (!list) continue;
      for (let k = 0; k < list.length; k += 2) {
        const e = edges[list[k]];
        if (filter && !filter(e)) continue;
        const i = list[k + 1], a = e.pts[i], b = e.pts[i + 1];
        const dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz;
        const t = clamp(((x - a.x) * dx + (z - a.z) * dz) / l2, 0, 1);
        const px = a.x + dx * t, pz = a.z + dz * t, d = Math.hypot(x - px, z - pz);
        if (d < out.d) {
          out.d = d; out.edge = e; out.seg = i; out.t = t; out.x = px; out.z = pz;
          out.y = a.y + (b.y - a.y) * t; out.hw = e.hw;
          const l = Math.sqrt(l2); out.tx = dx / l; out.tz = dz / l;
        }
      }
    }
  }
  return out.edge ? out : null;
}

// ---------------------------------------------------------------------------------------------
// Tremplins (cascades) : ajoutent du relief
// ---------------------------------------------------------------------------------------------
export const ramps = [];
function addRamp(x, z, yaw, len = 14, width = 8, h = 3.2) {
  ramps.push({ x, z, yaw, len, width, h, s: Math.sin(yaw), c: Math.cos(yaw), y: 0 });
}
const _h = { edge: null, seg: 0, t: 0, d: 0, x: 0, z: 0, y: 0, hw: 0, tx: 0, tz: 1 };
function rampHeight(x, z) {
  let add = 0;
  for (let i = 0; i < ramps.length; i++) {
    const r = ramps[i], dx = x - r.x, dz = z - r.z;
    if (dx * dx + dz * dz > 400) continue;
    const f = dx * r.s + dz * r.c, l = dx * r.c - dz * r.s;
    if (f >= 0 && f <= r.len && Math.abs(l) <= r.width / 2) add = Math.max(add, (f / r.len) * r.h);
  }
  return add;
}

// Hauteur du sol (relief aplani autour des routes + tremplins).
export function groundHeight(x, z) {
  const b = baseHeight(x, z);
  let h = b;
  const n = nearestRoad(x, z, 44, _h);
  if (n) {
    const t = smoothstep(n.hw + 2, n.hw + 24, n.d);
    h = lerp(n.y, b, t);
  }
  return ramps.length ? h + rampHeight(x, z) : h;
}
// Type de surface : 'asphalt' | 'cobble' | 'dirt' | 'sand' | 'grass' | 'pavement' | 'water'
const _s = { edge: null, seg: 0, t: 0, d: 0, x: 0, z: 0, y: 0, hw: 0, tx: 0, tz: 1 };
export function surfaceAt(x, z) {
  const n = nearestRoad(x, z, 16, _s);
  if (n && n.d <= n.hw + 0.4) return ROAD[n.edge.type].surf;
  return offroadSurface(x, z);
}
export function offroadSurface(x, z) {
  const c = z - coastZ(x);
  if (c > 95) return 'water';
  const zn = zoneAt(x, z);
  if (c > -75 && zn !== 'industrial') return 'sand';
  if (zn === 'downtown' || zn === 'neon' || zn === 'oldtown' || zn === 'industrial') return 'pavement';
  return 'grass';
}

// ---------------------------------------------------------------------------------------------
// Murs de collision (segments) et obstacles ronds, indexés spatialement
// ---------------------------------------------------------------------------------------------
const WC = 32, WGW = Math.ceil((BOUNDS.x1 - BOUNDS.x0) / WC) + 2, WGH = Math.ceil((BOUNDS.z1 - BOUNDS.z0) / WC) + 2;
const wgrid = new Array(WGW * WGH);
export const walls = []; // [x1,z1,x2,z2] à plat
export const pillars = []; // [x,z,r] à plat
const wcell = (x, z) => clamp(Math.floor((x - BOUNDS.x0) / WC) + 1, 0, WGW - 1) + clamp(Math.floor((z - BOUNDS.z0) / WC) + 1, 0, WGH - 1) * WGW;
export function addWall(x1, z1, x2, z2) {
  const id = walls.length / 4;
  walls.push(x1, z1, x2, z2);
  const n = Math.max(1, Math.ceil(Math.hypot(x2 - x1, z2 - z1) / (WC * 0.5)));
  let last = -1;
  for (let i = 0; i <= n; i++) {
    const c = wcell(lerp(x1, x2, i / n), lerp(z1, z2, i / n));
    if (c !== last) { (wgrid[c] || (wgrid[c] = [])).push(id); last = c; }
  }
}
export function addPillar(x, z, r) {
  const id = pillars.length / 3;
  pillars.push(x, z, r);
  const c = wcell(x, z);
  (wgrid[c] || (wgrid[c] = [])).push(-1 - id);
}
// Appelle cb(id) pour chaque mur (id>=0) ou pilier (id<0 → index -1-id) proche.
export function queryStatic(x, z, r, cb) {
  const x0 = Math.floor((x - r - BOUNDS.x0) / WC) + 1, x1 = Math.floor((x + r - BOUNDS.x0) / WC) + 1;
  const z0 = Math.floor((z - r - BOUNDS.z0) / WC) + 1, z1 = Math.floor((z + r - BOUNDS.z0) / WC) + 1;
  for (let gz = z0; gz <= z1; gz++) {
    if (gz < 0 || gz >= WGH) continue;
    for (let gx = x0; gx <= x1; gx++) {
      if (gx < 0 || gx >= WGW) continue;
      const l = wgrid[gx + gz * WGW];
      if (l) for (let i = 0; i < l.length; i++) cb(l[i]);
    }
  }
}

// ---------------------------------------------------------------------------------------------
// Bâtiments (lots orientés le long des routes)
// ---------------------------------------------------------------------------------------------
export const buildings = [];
const BSTYLE = {
  downtown: { w: [30, 52], d: [26, 44], h: [38, 190], gap: [3, 8], set: 5, p: 0.94, kinds: ['tower', 'tower', 'office', 'glass'] },
  neon: { w: [20, 34], d: [18, 30], h: [14, 58], gap: [1, 4], set: 4, p: 0.95, kinds: ['neon'] },
  oldtown: { w: [9, 16], d: [10, 15], h: [7, 15], gap: [0, 1.5], set: 3.4, p: 0.95, kinds: ['old'] },
  industrial: { w: [44, 80], d: [34, 60], h: [9, 18], gap: [10, 26], set: 8, p: 0.8, kinds: ['warehouse', 'warehouse', 'factory'] },
  coastal: { w: [15, 24], d: [13, 20], h: [6, 11], gap: [9, 22], set: 9, p: 0.72, kinds: ['villa', 'villa', 'villa', 'hotel'] },
  highway: { w: [16, 30], d: [14, 24], h: [6, 22], gap: [30, 110], set: 16, p: 0.4, kinds: ['house', 'office', 'house'] },
  mountain: { w: [10, 14], d: [9, 12], h: [5, 7], gap: [160, 420], set: 12, p: 0.35, kinds: ['cabin'] },
};
const bcell = new Map();
const bkey = (x, z) => Math.floor(x / 80) + '|' + Math.floor(z / 80);
function obbCorners(b) {
  const c = Math.cos(b.yaw), s = Math.sin(b.yaw), hx = b.w / 2, hz = b.d / 2;
  // axe local x = le long de la route (tangente), z = profondeur
  return [[-hx, -hz], [hx, -hz], [hx, hz], [-hx, hz]].map(([lx, lz]) => [b.x + lx * c + lz * s, b.z - lx * s + lz * c]);
}
function obbOverlap(A, B) {
  for (const P of [A, B]) for (let i = 0; i < 2; i++) {
    const ax = P[i + 1][0] - P[i][0], az = P[i + 1][1] - P[i][1];
    let a0 = Infinity, a1 = -Infinity, b0 = Infinity, b1 = -Infinity;
    for (const p of A) { const v = p[0] * ax + p[1] * az; a0 = Math.min(a0, v); a1 = Math.max(a1, v); }
    for (const p of B) { const v = p[0] * ax + p[1] * az; b0 = Math.min(b0, v); b1 = Math.max(b1, v); }
    if (a1 < b0 + 0.5 * Math.hypot(ax, az) * 0 || b1 < a0) return false;
  }
  return true;
}
const _b = { edge: null, seg: 0, t: 0, d: 0, x: 0, z: 0, y: 0, hw: 0, tx: 0, tz: 1 };
export const reserved = []; // zones interdites aux bâtiments (POI) : [x,z,r]
function tryBuilding(b) {
  if (b.x < BOUNDS.x0 + 60 || b.x > BOUNDS.x1 - 60 || b.z < BOUNDS.z0 + 60) return false;
  if (b.z - coastZ(b.x) > -85 && !(b.kind === 'warehouse' || b.kind === 'factory')) return false;
  if (b.z - coastZ(b.x) > -20) return false;
  for (const r of reserved) if (dist(b.x, b.z, r[0], r[1]) < r[2] + Math.max(b.w, b.d) * 0.6) return false;
  const cs = obbCorners(b);
  const test = [...cs, [b.x, b.z]];
  for (let i = 0; i < 4; i++) test.push([(cs[i][0] + cs[(i + 1) % 4][0]) / 2, (cs[i][1] + cs[(i + 1) % 4][1]) / 2]);
  if (b.w > 30 || b.d > 30) for (let i = 0; i < 4; i++) for (const f of [0.25, 0.75]) test.push([lerp(cs[i][0], cs[(i + 1) % 4][0], f), lerp(cs[i][1], cs[(i + 1) % 4][1], f)]);
  for (const p of test) {
    const n = nearestRoad(p[0], p[1], 30, _b);
    if (n && n.d < n.hw + 2.6) return false;
  }
  const kx = Math.floor(b.x / 80), kz = Math.floor(b.z / 80);
  for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) {
    const l = bcell.get(kx + i + '|' + (kz + j));
    if (l) for (const o of l) if (obbOverlap(cs, o.cs)) return false;
  }
  b.cs = cs;
  b.y = Math.min(...test.map((p) => groundHeight(p[0], p[1]))) - 0.3;
  const k = bkey(b.x, b.z);
  (bcell.get(k) || bcell.set(k, []).get(k)).push(b);
  b.id = buildings.length;
  buildings.push(b);
  for (let i = 0; i < 4; i++) addWall(cs[i][0], cs[i][1], cs[(i + 1) % 4][0], cs[(i + 1) % 4][1]);
  return true;
}

export function buildingNear(x, z, margin = 0) {
  const kx = Math.floor(x / 80), kz = Math.floor(z / 80);
  for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) {
    const l = bcell.get(kx + i + '|' + (kz + j));
    if (l) for (const o of l) if (dist(x, z, o.x, o.z) < Math.hypot(o.w, o.d) / 2 + margin) return o;
  }
  return null;
}
// ---------------------------------------------------------------------------------------------
// Points d'intérêt
// ---------------------------------------------------------------------------------------------
export const pois = [];
function poi(type, name, x, z, extra = {}) {
  const p = { id: pois.length, type, name, x, z, y: 0, zone: zoneAt(x, z), ...extra };
  pois.push(p);
  return p;
}
// Emplacement sur le bord d'une route proche d'un point donné (dégagement réservé).
function roadside(x, z, off = 22, radius = 20) {
  const n = nearestRoad(x, z, 400, _b);
  const side = ((x - n.x) * -n.tz + (z - n.z) * n.tx) >= 0 ? 1 : -1;
  const px = n.x + -n.tz * side * (n.hw + off), pz = n.z + n.tx * side * (n.hw + off);
  reserved.push([px, pz, radius]);
  return { x: px, z: pz, yaw: Math.atan2(-n.tz * -side, n.tx * -side), rx: n.x, rz: n.z, ryaw: Math.atan2(n.tx, n.tz) };
}
function sitePoi(type, name, x, z, extra, off, radius) {
  const s = roadside(x, z, off, radius);
  return poi(type, name, s.x, s.z, { yaw: s.yaw, rx: s.rx, rz: s.rz, ryaw: s.ryaw, ...extra });
}
// Garages
sitePoi('garage', 'Garage Central', -75, 75);
sitePoi('garage', 'Atelier du Port', 1000, 1100);
sitePoi('garage', 'Garage de la Baie', -600, 1190);
sitePoi('garage', 'Néon Tuning', 1200, 60);
sitePoi('garage', 'Atelier des Cimes', -1000, -1100);
// Concessions
sitePoi('dealer', 'Superessence Motors', 225, -75, {}, 26, 26);
sitePoi('dealer', 'Bay Luxury Cars', 60, 1190, {}, 26, 26);
sitePoi('dealer', 'Rallye & 4x4 Dépôt', 1210, -1040, {}, 26, 26);
// Stations-service (décoratives)
sitePoi('fuel', 'SuperEssence Station Nord', -300, -1030);
sitePoi('fuel', 'SuperEssence Station Sud', 800, 800);
sitePoi('fuel', 'SuperEssence Station Baie', -1100, 1190);
sitePoi('fuel', 'SuperEssence Station Est', 1800, -150);
sitePoi('fuel', 'SuperEssence Station Ouest', -1900, -150);
// Parkings et rassemblements
sitePoi('meet', 'Rassemblement du Dôme', 375, 380, { r: 30 }, 34, 34);
sitePoi('meet', 'Nuit Néon', 1320, -240, { r: 26 }, 30, 30);
sitePoi('meet', 'Sunset Meet', 150, 1500, { r: 28 }, 34, 34);
sitePoi('meet', 'Docks After Hours', 1600, 1300, { r: 30 }, 36, 36);
sitePoi('parking', 'Parking Skyline', -375, -380, { r: 26 }, 32, 32);
sitePoi('parking', 'Parking du Marché', -1150, 250, { r: 20 }, 24, 24);
sitePoi('parking', 'Parking des Quais', 1200, 1300, { r: 26 }, 32, 32);
sitePoi('parking', 'Parking de la Plage', -700, 1490, { r: 24 }, 30, 30);
// Points de vue
sitePoi('view', 'Belvédère des Crêtes', -800, -2150, {}, 16, 14);
sitePoi('view', 'Panorama du Col', 350, -2050, {}, 16, 14);
sitePoi('view', 'Balcon du Ravin', 1050, -1950, {}, 16, 14);
sitePoi('view', 'Jetée du Phare', 500, 1640, {}, 18, 14);
sitePoi('view', 'Terrasse de la Vieille Ville', -1500, 500, {}, 14, 12);
sitePoi('view', 'Rooftop Skyline', 600, -600, {}, 16, 12);
sitePoi('view', 'Colline des Néons', 1500, 300, {}, 14, 12);
sitePoi('view', 'Tribune de l\'Autodrome', 2820, 300, {}, 18, 14);
sitePoi('view', 'Digue du Port', 1700, 1400, {}, 16, 12);
sitePoi('view', 'Lacets de l\'Ouest', -1350, -1550, {}, 16, 12);
// Radars de vitesse (sur la route)
function roadPoi(type, name, x, z, extra) {
  const n = nearestRoad(x, z, 300, _b);
  return poi(type, name, n.x, n.z, { yaw: Math.atan2(n.tx, n.tz), hw: n.hw, ...extra });
}
roadPoi('radar', 'Radar Périph Nord', 300, -955, { target: 230 });
roadPoi('radar', 'Radar Périph Sud', -700, 735, { target: 240 });
roadPoi('radar', 'Radar Périph Est', 2055, 150, { target: 220 });
roadPoi('radar', 'Radar Périph Ouest', -2055, 100, { target: 220 });
roadPoi('radar', 'Radar Grande Avenue', 0, -300, { target: 170 });
roadPoi('radar', 'Radar Avenue Sud', -300, 950, { target: 180 });
roadPoi('radar', 'Radar de la Baie', 0, cz(0), { target: 190 });
roadPoi('radar', 'Radar des Néons', 1200, 60, { target: 150 });
roadPoi('radar', 'Radar du Port', 1300, 1000, { target: 150 });
roadPoi('radar', 'Radar de la Descente', 1200, -1500, { target: 140 });
roadPoi('radar', 'Radar Ligne Droite Autodrome', 2330, 180, { target: 210 });
roadPoi('radar', 'Radar Vieille Ville', -1200, 200, { target: 110 });
// Zones de drift (portion de route)
roadPoi('drift', 'Drift des Lacets', -1350, -1550, { r: 170, target: 6000 });
roadPoi('drift', 'Drift du Ravin', 1050, -1950, { r: 170, target: 7000 });
roadPoi('drift', 'Drift des Docks', 1300, 1200, { r: 150, target: 5000 });
roadPoi('drift', 'Drift du Dôme', 0, 0, { r: 150, target: 5000 });
roadPoi('drift', 'Drift Néon', 1140, 0, { r: 130, target: 4500 });
roadPoi('drift', 'Drift de l\'Épingle', 2820, 0, { r: 170, target: 8000 });
roadPoi('drift', 'Drift de la Corniche', 820, 1560, { r: 150, target: 5500 });
roadPoi('drift', 'Drift du Col', -50, -1900, { r: 160, target: 6500 });
// Sauts (tremplins)
function jump(name, x, z, flip, target, h = 3.2, len = 14) {
  const n = nearestRoad(x, z, 300, _b);
  const yaw = Math.atan2(n.tx, n.tz) + (flip ? Math.PI : 0);
  const s = Math.sin(yaw), c = Math.cos(yaw);
  // tremplin décalé sur la voie de droite (dans le sens du saut)
  const off = n.hw > 6 ? n.hw * 0.45 : 0;
  const rx = n.x + -c * off, rz = n.z + s * off;
  addRamp(rx, rz, yaw, len, Math.min(8, n.hw * 0.9), h);
  return poi('jump', name, rx - s * 6, rz - c * 6, { yaw, target, rampId: ramps.length - 1 });
}
jump('Saut du Périph', 900, -946, false, 55, 3.6, 16);
jump('Saut des Docks', 1500, 1100, false, 35);
jump('Saut de la Baie', -550, cz(-550), false, 45);
jump('Saut du Sommet', -300, -2050, false, 50, 3.4);
jump('Saut de la Plage', 0, coastZ(0) - 42, false, 40, 3.4);
jump('Saut Skyline', 300, 300, true, 30);
jump('Saut de la Piste', -700, -1500, false, 40, 3.4);
jump('Saut de l\'Autodrome', 2740, 760, false, 50, 3.6, 16);
jump('Saut de l\'Ouest', -2050, 300, true, 55, 3.6, 16);
jump('Saut des Néons', 1380, 180, false, 30);
jump('Saut du Ravin', 500, -1600, true, 40, 3.4);
jump('Saut de la Corniche', 1200, -1500, true, 45, 3.4);
// Jetons cachés (Essence Tokens à découvrir)
{
  const spots = [[75, 75], [-450, 450], [520, -520], [1085, -240], [1440, 240], [-1250, 150], [-1000, 420], [-1420, -40], [1000, 900], [1600, 1100], [1400, 1560],
    [-100, 1640], [-950, 1600], [520, 1700], [-1200, 1000], [0, 1300], [-700, -1500], [500, -1600], [-800, -2190], [370, -2010], [1050, -1990], [-1250, -1990],
    [150, -1740], [2600, -200], [2700, 450], [2400, 800], [1900, -700], [-1900, -700], [-1900, 520], [1700, 560], [-150, -800], [800, 450], [-760, -300], [-2200, 900], [2250, -500], [900, -1150]];
  spots.forEach((s, i) => poi('token', 'Jeton caché n°' + (i + 1), s[0], s[1], {}));
}
for (const p of pois) p.y = groundHeight(p.x, p.z);
for (const r of ramps) r.y = groundHeight(r.x - r.s * 0.5, r.z - r.c * 0.5);

// ---------------------------------------------------------------------------------------------
// Génération des bâtiments
// ---------------------------------------------------------------------------------------------
{
  const order = edges.slice().sort((a, b) => ROAD[b.type].w - ROAD[a.type].w);
  for (const e of order) {
    if (e.type === 'dirt' || e.type === 'sand' || e.type === 'track') continue;
    const r = rng(e.id * 7919 + 13);
    for (const side of [1, -1]) {
      let s = r.range(4, 14);
      while (s < e.len - 6) {
        const pt = pointAt(e, s);
        const zn = zoneAt(pt.x, pt.z);
        let st = BSTYLE[zn];
        if (e.type === 'highway' && zn !== 'industrial') st = BSTYLE.highway;
        const w = r.range(st.w[0], st.w[1]), d = r.range(st.d[0], st.d[1]);
        if (s + w > e.len - 4) break;
        const mid = pointAt(e, s + w / 2);
        const nx = -mid.tz * side, nz = mid.tx * side;
        const off = e.hw + st.set + d / 2 + (e.type === 'highway' ? 14 : 0);
        const kind = r.pick(st.kinds);
        let h = r.range(st.h[0], st.h[1]);
        if (zn === 'downtown') { const cd = Math.hypot(mid.x, mid.z) / 800; h = lerp(st.h[1], st.h[0], clamp(cd + r.range(-0.3, 0.3), 0, 1)); }
        if (kind === 'hotel') h = r.range(22, 40);
        const b = { x: mid.x + nx * off, z: mid.z + nz * off, w, d, h, yaw: Math.atan2(mid.tx, mid.tz) - Math.PI / 2, kind, zone: zn, seed: Math.floor(r() * 1e6), face: side };
        if (r() < st.p) tryBuilding(b);
        s += w + r.range(st.gap[0], st.gap[1]);
      }
    }
  }
}

// Point à l'abscisse curviligne s d'une arête.
export function pointAt(e, s, out = {}) {
  const p = e.pts;
  s = clamp(s, 0, e.len);
  let lo = 0, hi = p.length - 1;
  while (hi - lo > 1) { const m = (lo + hi) >> 1; if (p[m].s <= s) lo = m; else hi = m; }
  const a = p[lo], b = p[hi], l = b.s - a.s || 1, t = (s - a.s) / l;
  out.x = a.x + (b.x - a.x) * t; out.z = a.z + (b.z - a.z) * t; out.y = a.y + (b.y - a.y) * t;
  out.tx = (b.x - a.x) / l; out.tz = (b.z - a.z) / l;
  return out;
}

// Glissières de sécurité : périphérique et routes de montagne (segments de collision + rendu).
export const rails = []; // {pts:[{x,z,y}], kind}
for (const e of edges) {
  if (e.type !== 'highway' && e.type !== 'mountain' && e.type !== 'track') continue;
  for (const side of [1, -1]) {
    const off = e.hw + (e.type === 'track' ? 5 : 1.6);
    let cur = null;
    const end = e.len - nodes[e.b].r - 8;
    for (let s = nodes[e.a].r + 8; s <= end; s += 9) {
      const p = pointAt(e, s);
      const x = p.x - p.tz * side * off, z = p.z + p.tx * side * off;
      // interrompre la glissière aux tremplins, POI et pistes de terre qui partent de la route
      let block = false;
      for (const q of pois) if ((q.type === 'view' || q.type === 'fuel' || q.type === 'dealer' || q.type === 'garage') && dist(q.x, q.z, x, z) < 34) block = true;
      const o = nearestRoad(x, z, 14, _b, (f) => f !== e);
      if (o && o.d < o.hw + 3) block = true;
      if (block) { cur = null; continue; }
      if (!cur) { cur = { pts: [], kind: e.type }; rails.push(cur); }
      cur.pts.push({ x, z, y: p.y });
    }
  }
}
for (const r of rails) for (let i = 0; i < r.pts.length - 1; i++) addWall(r.pts[i].x, r.pts[i].z, r.pts[i + 1].x, r.pts[i + 1].z);
// Tunnels : portion couverte d'une arête, avec murs latéraux
export const tunnels = [];
for (const e of edges) {
  if (!e.tunnel) continue;
  const long = e.len > 400, s0 = long ? e.len * 0.32 : e.len * 0.14, s1 = long ? Math.min(e.len * 0.32 + 260, e.len * 0.7) : e.len * 0.86;
  tunnels.push({ e, s0, s1 });
  if (e.type === 'avenue') for (const side of [1, -1]) {
    let prev = null;
    for (let s = s0; s <= s1 + 0.1; s += (s1 - s0) / 6) {
      const p = pointAt(e, s), x = p.x - p.tz * side * (e.hw + 0.9), z = p.z + p.tx * side * (e.hw + 0.9);
      if (prev) addWall(prev[0], prev[1], x, z);
      prev = [x, z];
    }
  }
}
// Murs du monde
addWall(BOUNDS.x0 + 20, BOUNDS.z0 + 20, BOUNDS.x1 - 20, BOUNDS.z0 + 20);
addWall(BOUNDS.x0 + 20, BOUNDS.z0 + 20, BOUNDS.x0 + 20, BOUNDS.z1 - 20);
addWall(BOUNDS.x1 - 20, BOUNDS.z0 + 20, BOUNDS.x1 - 20, BOUNDS.z1 - 20);

// ---------------------------------------------------------------------------------------------
// Itinéraires : plus court chemin entre carrefours
// ---------------------------------------------------------------------------------------------
export function nearestNode(x, z, filter = null) {
  let best = -1, bd = Infinity;
  for (const n of nodes) {
    if (filter && !filter(n)) continue;
    const d = dist2n(n, x, z);
    if (d < bd) { bd = d; best = n.id; }
  }
  return best;
}
const dist2n = (n, x, z) => (n.x - x) * (n.x - x) + (n.z - z) * (n.z - z);
// Renvoie la liste d'arêtes orientées [{e, rev}] de a vers b. allow(edge) filtre les types.
export function findPath(a, b, allow = null, costFn = null) {
  const distA = new Float64Array(nodes.length).fill(Infinity), prev = new Int32Array(nodes.length).fill(-1), prevE = new Int32Array(nodes.length).fill(-1);
  const open = [a];
  distA[a] = 0;
  const done = new Uint8Array(nodes.length);
  while (open.length) {
    let bi = 0;
    for (let i = 1; i < open.length; i++) if (distA[open[i]] < distA[open[bi]]) bi = i;
    const u = open.splice(bi, 1)[0];
    if (u === b) break;
    if (done[u]) continue;
    done[u] = 1;
    for (const id of nodes[u].edges) {
      const e = edges[id];
      if (allow && !allow(e)) continue;
      const v = e.a === u ? e.b : e.a;
      const c = distA[u] + (costFn ? costFn(e) : e.len / ROAD[e.type].speed);
      if (c < distA[v]) { distA[v] = c; prev[v] = u; prevE[v] = id; open.push(v); }
    }
  }
  if (prev[b] < 0 && a !== b) return null;
  const out = [];
  for (let v = b; v !== a; v = prev[v]) out.push({ e: edges[prevE[v]], rev: edges[prevE[v]].b !== v });
  return out.reverse();
}
// Convertit une suite de carrefours en polyligne continue {x,z,y,s,hw,type}.
export function routeThrough(nodeIds, allow = null) {
  const pts = [];
  let s = 0;
  for (let i = 0; i < nodeIds.length - 1; i++) {
    const path = findPath(nodeIds[i], nodeIds[i + 1], allow);
    if (!path) return null;
    for (const { e, rev } of path) {
      const n = e.pts.length;
      for (let k = 0; k < n; k++) {
        const p = e.pts[rev ? n - 1 - k : k];
        const last = pts[pts.length - 1];
        if (last && Math.abs(last.x - p.x) < 0.01 && Math.abs(last.z - p.z) < 0.01) continue;
        if (last) s += dist(last.x, last.z, p.x, p.z);
        pts.push({ x: p.x, z: p.z, y: p.y, s, hw: e.hw, type: e.type });
      }
    }
  }
  return pts;
}
export const worldStats = () => ({
  nodes: nodes.length, edges: edges.length, km: edges.reduce((a, e) => a + e.len, 0) / 1000, buildings: buildings.length, pois: pois.length, walls: walls.length / 4,
});
