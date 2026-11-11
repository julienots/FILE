// Rendu du monde ouvert : secteurs chargés en continu (streaming), routes, relief, bâtiments, mobilier, végétation.
// Stratégie mobile : 3 à 5 appels de rendu par secteur (relief, routes, décor fusionné, enseignes, halos).
import * as THREE from 'three';
import * as MAP from './map.js';
import { GeoBuf, NOW, WIN, GLASS, NEONW, OLDW, EMI, TL } from './geobuf.js';
import { clamp, lerp, smoothstep, noise2, rng, hash2, dist } from '../core/util.js';
import { glowTex } from '../vehicle/carBuilder.js';

export const CH = 200;
const { ROAD, edges, nodes, BOUNDS } = MAP;
const ckey = (cx, cz) => cx + ',' + cz;
const cOf = (v) => Math.floor(v / CH);

// ---------------------------------------------------------------------------------------------
// Matériaux partagés
// ---------------------------------------------------------------------------------------------
export const uniforms = { uNight: { value: 0 }, uTime: { value: 0 }, uWet: { value: 0 } };

function makeStaticMat() {
  const m = new THREE.MeshLambertMaterial({ vertexColors: true });
  m.onBeforeCompile = (sh) => {
    sh.uniforms.uNight = uniforms.uNight; sh.uniforms.uTime = uniforms.uTime;
    sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nattribute vec3 bw;\nvarying vec3 vBw;\nvarying vec2 vUvM;')
      .replace('#include <begin_vertex>', '#include <begin_vertex>\nvBw = bw; vUvM = uv;');
    sh.fragmentShader = sh.fragmentShader.replace('#include <common>', '#include <common>\nvarying vec3 vBw;\nvarying vec2 vUvM;\nuniform float uNight;\nuniform float uTime;')
      .replace('#include <color_fragment>', `#include <color_fragment>
        vec3 seEmis = vec3(0.0);
        float ty = vBw.z;
        if (ty > 0.5 && ty < 4.5) {
          vec2 cell = vUvM / vBw.xy; vec2 f = fract(cell); vec2 id = floor(cell);
          float rnd = fract(sin(dot(id, vec2(12.9898, 78.233))) * 43758.5453);
          float win;
          if (ty < 1.5) win = step(0.16, f.x) * step(f.x, 0.84) * step(0.22, f.y) * step(f.y, 0.78);
          else if (ty < 2.5) win = step(0.05, f.x) * step(0.1, f.y);
          else if (ty < 3.5) win = step(0.2, f.x) * step(f.x, 0.8) * step(0.3, f.y) * step(f.y, 0.75);
          else win = step(0.3, f.x) * step(f.x, 0.7) * step(0.25, f.y) * step(f.y, 0.8);
          vec3 gcol = mix(vec3(0.08, 0.13, 0.2), vec3(0.34, 0.5, 0.66), rnd * 0.55 + (ty > 1.5 && ty < 2.5 ? 0.35 : 0.1));
          gcol = mix(gcol, gcol * 0.25, uNight);
          diffuseColor.rgb = mix(diffuseColor.rgb, gcol, win);
          float litp = ty < 2.5 ? 0.42 : (ty < 3.5 ? 0.3 : 0.55);
          float lit = step(1.0 - litp * uNight, rnd) * win;
          vec3 lc = mix(vec3(1.0, 0.8, 0.45), vec3(0.6, 0.82, 1.0), step(0.78, fract(rnd * 7.0)));
          if (ty > 2.5 && ty < 3.5) lc = mix(vec3(1.0, 0.25, 0.8), vec3(0.2, 0.9, 1.0), fract(rnd * 5.0));
          seEmis = lc * lit * 1.25;
        } else if (ty > 4.5 && ty < 5.5) {
          seEmis = vColor.rgb * mix(vBw.x, vBw.y, uNight);
        } else if (ty > 5.5) {
          float ph = fract(uTime / 16.0 + vBw.y);
          float on = vBw.x < 0.5 ? step(0.5, ph) : (vBw.x < 1.5 ? step(0.42, ph) * step(ph, 0.5) : step(ph, 0.42));
          seEmis = vColor.rgb * on * 2.2; diffuseColor.rgb *= 0.2 + on;
        }`)
      .replace('#include <emissivemap_fragment>', '#include <emissivemap_fragment>\ntotalEmissiveRadiance += seEmis;');
  };
  return m;
}

function makeRoadMat() {
  const m = new THREE.MeshStandardMaterial({ roughness: 0.92, metalness: 0 });
  m.onBeforeCompile = (sh) => {
    sh.uniforms.uWet = uniforms.uWet;
    sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nattribute vec4 rp;\nvarying vec4 vRp;\nvarying vec2 vUvM;')
      .replace('#include <begin_vertex>', '#include <begin_vertex>\nvRp = rp; vUvM = uv;');
    sh.fragmentShader = sh.fragmentShader.replace('#include <common>', `#include <common>
      varying vec4 vRp; varying vec2 vUvM; uniform float uWet;
      float seHash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
      vec3 roadColor(vec2 uv, vec4 rp, out float wetMask) {
        float w = rp.z; float x = (uv.x - 0.5) * w; float ax = abs(x); float v = uv.y;
        float grain = seHash(floor(vec2(x, v) * 3.0)); float big = seHash(floor(vec2(x, v) * 0.22));
        vec3 asphalt = vec3(0.135, 0.135, 0.142) * (0.86 + 0.2 * grain + 0.12 * big);
        vec3 white = vec3(0.86); vec3 yellow = vec3(0.96, 0.72, 0.1);
        vec3 col = asphalt; wetMask = 1.0; int s = int(rp.y + 0.5); float m = rp.w; float lanes = rp.x;
        if (s == 0 || s == 5) {
          float line = 0.0; vec3 lc = white;
          if (lanes > 2.5) {
            float lw = (w * 0.5 - 1.7) / 3.0;
            col = mix(col, vec3(0.22, 0.23, 0.25), step(ax, 0.8) * m);
            line = step(abs(ax - 0.8), 0.09);
            float d = step(fract(v / 12.0), 0.35);
            line = max(line, d * (step(abs(ax - 0.8 - lw), 0.08) + step(abs(ax - 0.8 - 2.0 * lw), 0.08)));
          } else if (lanes > 1.5) {
            line = step(abs(ax - 0.17), 0.065); lc = yellow;
            col = mix(col, white, step(fract(v / 9.0), 0.4) * step(abs(ax - w * 0.25 + 0.1), 0.07) * m);
          } else if (lanes > 0.5) {
            line = step(ax, 0.07) * step(fract(v / 8.0), 0.5);
          }
          col = mix(col, lc, min(line, 1.0) * m);
          if (lanes > 0.5) col = mix(col, white, step(abs(ax - (w * 0.5 - 0.42)), 0.08) * m);
          if (s == 5) {
            float k = step(w * 0.5 - 1.0, ax);
            col = mix(col, mix(vec3(0.86, 0.08, 0.1), vec3(0.92), step(0.5, fract(v / 4.0))), k);
          }
        } else if (s == 1) {
          vec2 q = vec2(x * 2.4 + step(1.0, mod(v * 2.4, 2.0)) * 0.5, v * 2.4); vec2 f = fract(q);
          col = mix(vec3(0.4, 0.36, 0.33), vec3(0.58, 0.52, 0.45), seHash(floor(q)));
          col *= 1.0 - 0.35 * min(step(f.x, 0.09) + step(f.y, 0.09), 1.0);
        } else if (s == 2) {
          col = vec3(0.44, 0.31, 0.19) * (0.8 + 0.3 * grain) * (1.0 - 0.22 * smoothstep(0.4, 0.0, abs(ax - 0.95))); wetMask = 0.35;
        } else if (s == 3) {
          col = vec3(0.86, 0.75, 0.52) * (0.88 + 0.15 * grain) * (1.0 - 0.14 * smoothstep(0.4, 0.0, abs(ax - 0.95))); wetMask = 0.15;
        } else if (s == 6) {
          col = mix(asphalt, white, step(0.5, fract(x / 1.2 + 0.25)));
        } else if (s == 7) {
          float bay = step(fract(x / 2.7), 0.06) * step(0.22, abs(fract(v / 12.0) - 0.5));
          col = mix(asphalt * 1.25, white, bay);
        } else if (s == 8) {
          vec2 f = fract(vec2(x, v) * 0.7);
          col = vec3(0.6, 0.59, 0.57) * (0.92 + 0.1 * grain) * (1.0 - 0.14 * min(step(f.x, 0.05) + step(f.y, 0.05), 1.0)) * rp.x; wetMask = 0.6;
        }
        return col;
      }`)
      .replace('#include <color_fragment>', '#include <color_fragment>\nfloat seWetMask; diffuseColor.rgb = roadColor(vUvM, vRp, seWetMask);\ndiffuseColor.rgb *= 1.0 - 0.3 * uWet * seWetMask;')
      .replace('#include <roughnessmap_fragment>', '#include <roughnessmap_fragment>\nroughnessFactor = mix(roughnessFactor, 0.12, uWet * seWetMask);')
      .replace('#include <metalnessmap_fragment>', '#include <metalnessmap_fragment>\nmetalnessFactor = mix(metalnessFactor, 0.45, uWet * seWetMask);');
  };
  return m;
}

const SIGNS = ['SUPERESSENCE', 'VOLTA', 'KAIDO', 'NEON BAR', 'HÔTEL BAIE', 'GARAGE', 'MOTORS', 'CAFÉ SOL', 'ARCADE', 'RAMEN 24', 'CLUB VICE', 'DINER', 'TUNING', 'RADIO VC', 'DOCKS', 'STREET LEGENDS'];
function makeSignTexture() {
  const cv = document.createElement('canvas');
  cv.width = 1024; cv.height = 512;
  const g = cv.getContext('2d');
  const pal = [['#ff7a00', '#0b0f1a'], ['#ffe14d', '#1b2a6b'], ['#ff2d55', '#ffffff'], ['#00e5ff', '#1a0033'], ['#ffffff', '#0a7f8c'], ['#ffd400', '#141414'], ['#ffffff', '#c1121f'], ['#3b1d0a', '#ffd9a0'],
    ['#ff4fd8', '#12002b'], ['#ffec3d', '#b3001b'], ['#b388ff', '#0d0221'], ['#ffffff', '#1f6feb'], ['#9dff00', '#101010'], ['#ffffff', '#6a00f4'], ['#f2f2f2', '#33415c'], ['#00e5ff', '#0b0f1a']];
  SIGNS.forEach((t, i) => {
    const x = (i % 4) * 256, y = Math.floor(i / 4) * 128;
    g.fillStyle = pal[i][1]; g.fillRect(x, y, 256, 128);
    g.strokeStyle = pal[i][0]; g.lineWidth = 6; g.strokeRect(x + 7, y + 7, 242, 114);
    g.fillStyle = pal[i][0]; g.textAlign = 'center'; g.textBaseline = 'middle';
    let fs = 44; g.font = `italic 900 ${fs}px Arial`;
    while (g.measureText(t).width > 216 && fs > 14) { fs -= 2; g.font = `italic 900 ${fs}px Arial`; }
    g.fillText(t, x + 128, y + 66);
  });
  const tex = new THREE.CanvasTexture(cv);
  tex.colorSpace = THREE.SRGBColorSpace; tex.anisotropy = 4;
  return tex;
}

// ---------------------------------------------------------------------------------------------
// Mobilier global (placé une fois, collisions comprises), rangé par secteur
// ---------------------------------------------------------------------------------------------
const propsByChunk = new Map();
const roadRuns = new Map();
const nodesByChunk = new Map();
const buildingsByChunk = new Map();
const poisByChunk = new Map();
const railsByChunk = new Map();
const pushMap = (m, k, v) => (m.get(k) || m.set(k, []).get(k)).push(v);
let prepared = false;
const _n = { edge: null, seg: 0, t: 0, d: 0, x: 0, z: 0, y: 0, hw: 0, tx: 0, tz: 1 };

function addProp(type, x, z, yaw = 0, seed = 0, extra = null) {
  const p = { type, x, z, y: MAP.groundHeight(x, z), yaw, seed, extra };
  pushMap(propsByChunk, ckey(cOf(x), cOf(z)), p);
  return p;
}
function prepare() {
  if (prepared) return;
  prepared = true;
  for (const e of edges) {
    let run = null;
    for (let i = 0; i < e.pts.length - 1; i++) {
      const a = e.pts[i], b = e.pts[i + 1], k = ckey(cOf((a.x + b.x) / 2), cOf((a.z + b.z) / 2));
      if (!run || run.k !== k) { run = { k, e, i0: i, i1: i }; pushMap(roadRuns, k, run); } else run.i1 = i;
    }
  }
  for (const n of nodes) pushMap(nodesByChunk, ckey(cOf(n.x), cOf(n.z)), n);
  for (const b of MAP.buildings) pushMap(buildingsByChunk, ckey(cOf(b.x), cOf(b.z)), b);
  for (const p of MAP.pois) pushMap(poisByChunk, ckey(cOf(p.x), cOf(p.z)), p);
  for (const r of MAP.rails) for (let i = 0; i < r.pts.length - 1; i++) pushMap(railsByChunk, ckey(cOf(r.pts[i].x), cOf(r.pts[i].z)), [r.pts[i], r.pts[i + 1], r.kind]);

  // lampadaires, arbres d'alignement, feux
  const P = {};
  for (const e of edges) {
    const R = ROAD[e.type];
    if (R.surf === 'dirt' || R.surf === 'sand' || e.type === 'track' || e.type === 'mountain') continue;
    const r = rng(e.id * 31 + 5), a = nodes[e.a].r + 7, b = e.len - nodes[e.b].r - 7;
    const lampStep = e.type === 'alley' ? 34 : e.type === 'highway' ? 60 : 46;
    let side = r.chance(0.5) ? 1 : -1;
    for (let s = a + r.range(0, 12); s < b; s += lampStep) {
      MAP.pointAt(e, s, P);
      const zn = MAP.zoneAt(P.x, P.z);
      const off = e.hw + (e.type === 'highway' ? 3.2 : 1.4);
      const x = P.x - P.tz * side * off, z = P.z + P.tx * side * off;
      if (!MAP.buildingNear(x, z, -3)) {
        addProp(e.type === 'alley' ? 'lantern' : zn === 'neon' ? 'neonlamp' : 'lamp', x, z, Math.atan2(P.tx, P.tz) + (side > 0 ? 0 : Math.PI), s | 0);
        if (e.type !== 'highway') MAP.addPillar(x, z, 0.22);
      }
      side = -side;
    }
    const treeStep = e.type === 'avenue' ? 23 : e.type === 'coastal' ? 19 : e.type === 'street' ? 38 : 0;
    if (treeStep) for (const sd of [1, -1]) for (let s = a + 10 + (sd > 0 ? 0 : treeStep / 2); s < b; s += treeStep) {
      MAP.pointAt(e, s, P);
      const zn = MAP.zoneAt(P.x, P.z);
      if (zn === 'industrial' || (zn === 'neon' && r.chance(0.5))) continue;
      const off = e.hw + 2.4, x = P.x - P.tz * sd * off, z = P.z + P.tx * sd * off;
      if (MAP.buildingNear(x, z, 0.5)) continue;
      addProp(zn === 'coastal' || e.type === 'coastal' ? 'palm' : 'tree', x, z, r() * 6.28, Math.floor(r() * 1000));
      MAP.addPillar(x, z, 0.32);
    }
    if (e.type === 'highway') for (const sd of [1, -1]) for (let s = a + r.range(60, 300); s < b; s += r.range(320, 620)) {
      MAP.pointAt(e, s, P);
      const off = e.hw + 9, x = P.x - P.tz * sd * off, z = P.z + P.tx * sd * off;
      if (!MAP.buildingNear(x, z, 4)) addProp('billboard', x, z, Math.atan2(P.tx, P.tz) + (sd > 0 ? Math.PI : 0) + 0.5 * sd, r.int(0, 15));
    }
  }
  // feux tricolores
  for (const n of nodes) {
    if (n.edges.length < 3) continue;
    const zn = MAP.zoneAt(n.x, n.z);
    const types = n.edges.map((id) => edges[id].type);
    if (!types.some((t) => t === 'avenue') || types.some((t) => t === 'highway' || t === 'alley')) continue;
    if (zn !== 'downtown' && zn !== 'neon' && zn !== 'coastal') continue;
    n.light = true;
    for (const id of n.edges) {
      const e = edges[id], out = e.a === n.id, p = MAP.pointAt(e, out ? n.r + 2.5 : e.len - n.r - 2.5, P);
      const dx = out ? -p.tx : p.tx, dz = out ? -p.tz : p.tz; // sens d'arrivée vers le carrefour
      const x = p.x + -dz * (e.hw + 1.0), z = p.z + dx * (e.hw + 1.0);
      const axis = Math.abs(dx) > Math.abs(dz) ? 0 : 0.5;
      addProp('tlight', x, z, Math.atan2(-dx, -dz), 0, axis);
      MAP.addPillar(x, z, 0.2);
    }
  }
  // végétation et rochers dispersés
  const x0 = cOf(BOUNDS.x0), x1 = cOf(BOUNDS.x1), z0 = cOf(BOUNDS.z0), z1 = cOf(BOUNDS.z1);
  for (let cx = x0; cx <= x1; cx++) for (let cz = z0; cz <= z1; cz++) {
    const r = rng(cx * 7919 + cz * 104729 + 3);
    const mx = cx * CH + CH / 2, mz = cz * CH + CH / 2, zn = MAP.zoneAt(mx, mz);
    const count = zn === 'mountain' ? 26 : zn === 'highway' ? 16 : zn === 'coastal' ? 12 : 0;
    for (let i = 0; i < count; i++) {
      const x = cx * CH + r() * CH, z = cz * CH + r() * CH;
      if (x < BOUNDS.x0 + 30 || x > BOUNDS.x1 - 30 || z < BOUNDS.z0 + 30) continue;
      const surf = MAP.offroadSurface(x, z);
      if (surf === 'water' || surf === 'pavement') continue;
      const n = MAP.nearestRoad(x, z, 30, _n);
      if (n && n.d < n.hw + (n.edge.type === 'track' ? 16 : 7)) continue;
      if (MAP.buildingNear(x, z, 2)) continue;
      let near = false;
      for (const q of MAP.pois) if (q.type !== 'token' && dist(q.x, q.z, x, z) < 30) { near = true; break; }
      if (near) continue;
      const zz = MAP.zoneAt(x, z);
      const dense = noise2(x / 140 + 3, z / 140) > 0.42;
      let type;
      if (surf === 'sand') type = r.chance(0.35) ? 'palm' : r.chance(0.3) ? 'parasol' : null;
      else if (zz === 'mountain') type = !dense ? (r.chance(0.3) ? 'rock' : null) : r.chance(0.85) ? 'pine' : 'rock';
      else if (zz === 'coastal') type = r.chance(0.6) ? 'palm' : 'bush';
      else type = dense ? (r.chance(0.8) ? 'tree' : 'bush') : r.chance(0.25) ? 'bush' : null;
      if (!type) continue;
      addProp(type, x, z, r() * 6.28, Math.floor(r() * 1000));
      if (type !== 'bush' && type !== 'parasol') MAP.addPillar(x, z, type === 'rock' ? 1.3 : 0.38);
    }
  }
  // repères : conteneurs et grues du port, phare, antenne, grande roue
  const r = rng(77);
  for (let i = 0; i < 46; i++) {
    const x = r.range(930, 1680), z = r.range(1440, 1575);
    const n = MAP.nearestRoad(x, z, 30, _n);
    if ((n && n.d < n.hw + 6) || MAP.buildingNear(x, z, 5) || z - MAP.coastZ(x) > -40) continue;
    const yaw = r.chance(0.5) ? 0 : Math.PI / 2;
    addProp('containers', x, z, yaw, r.int(0, 999));
    const hx = yaw ? 1.3 : 6.2, hz = yaw ? 6.2 : 1.3;
    MAP.addWall(x - hx, z - hz, x + hx, z - hz); MAP.addWall(x + hx, z - hz, x + hx, z + hz); MAP.addWall(x + hx, z + hz, x - hx, z + hz); MAP.addWall(x - hx, z + hz, x - hx, z - hz);
  }
  for (const x of [1000, 1250, 1500]) { const z = MAP.coastZ(x) - 30; addProp('crane', x, z, 0, x); MAP.addPillar(x - 7, z, 1.2); MAP.addPillar(x + 7, z, 1.2); }
  { const p = MAP.pois.find((q) => q.name === 'Jetée du Phare'); addProp('lighthouse', p.x + 26, p.z + 20, 0, 0); MAP.addPillar(p.x + 26, p.z + 20, 4.2); }
  { const p = MAP.pois.find((q) => q.name === 'Belvédère des Crêtes'); addProp('antenna', p.x - 30, p.z - 26, 0, 0); MAP.addPillar(p.x - 30, p.z - 26, 1.5); }
  { addProp('ferris', 1560, 380, 0.4, 0); MAP.addPillar(1560, 380, 5); }
  { const p = MAP.pois.find((q) => q.name === 'Tribune de l\'Autodrome'); addProp('grandstand', p.x, p.z, p.yaw, 0); }
  for (const [x, z] of [[-950, -1900], [-1150, -1700], [600, -1850], [900, -2100], [-100, -2150]]) { const n = MAP.nearestRoad(x, z, 40, _n); if (!n || n.d > n.hw + 10) { addProp('turbine', x, z, 0.8, x); MAP.addPillar(x, z, 1.2); } }
}

// ---------------------------------------------------------------------------------------------
// Dessin des éléments de décor dans un GeoBuf
// ---------------------------------------------------------------------------------------------
const TREE_G = [0x3fa34d, 0x4cb85a, 0x2f8f46, 0x6abf4b];
const PASTEL = [0xfff1d6, 0xffd9c7, 0xd7f0ff, 0xf6f6f0, 0xffe3ef, 0xe2f6df, 0xfff6b8];
const OLDC = [0xe9c58f, 0xf0d9a8, 0xdba77a, 0xf3e3c3, 0xe8b996, 0xd9c2a0, 0xf7ecd2];
const TOWERC = [0x8fa3b8, 0xd8cdb8, 0x6f8196, 0xe6e2da, 0xb38f7a, 0xa9b9c9, 0xc9b99a, 0xf0ede6, 0x9c6b5a, 0x7f8f8a, 0xd9c08f, 0x5f6f86];
const NEONC = [0xff2bd6, 0x00e5ff, 0x9dff00, 0xff7a00, 0x8a4dff, 0xffe600, 0xff3355];
const WAREC = [0x8d99a6, 0xb5651d, 0x4f6d7a, 0x9a8c7a, 0x6b7f5c, 0xa33b3b];

function drawProp(g, glow, signs, p) {
  const s = p.seed, r = rng(s * 13 + 7);
  g.at(p.x, p.y, p.z, p.yaw);
  switch (p.type) {
    case 'lamp': case 'neonlamp': {
      const c = p.type === 'neonlamp' ? NEONC[s % NEONC.length] : 0xffe2a8;
      g.cyl(0, 0, 0, 0.13, 0.08, 7.6, 5, 0x3a4048);
      g.box(1.1, 7.5, 0, 2.4, 0.12, 0.14, 0x3a4048);
      g.box(2.0, 7.36, 0, 0.7, 0.12, 0.3, c, EMI(0.25, 3));
      glow.push(p.x + 2.0 * Math.cos(p.yaw), p.y + 7.3, p.z - 2.0 * Math.sin(p.yaw), c, 7);
      break;
    }
    case 'lantern': g.cyl(0, 0, 0, 0.09, 0.07, 3.4, 5, 0x22262c); g.box(0, 3.4, 0, 0.36, 0.46, 0.36, 0xffc978, EMI(0.3, 3)); g.cyl(0, 3.86, 0, 0.3, 0, 0.26, 4, 0x22262c); glow.push(p.x, p.y + 3.6, p.z, 0xffc978, 5); break;
    case 'tlight': {
      g.cyl(0, 0, 0, 0.11, 0.09, 5.2, 5, 0x2a2e34);
      g.box(0, 5.0, 1.6, 0.14, 0.14, 3.3, 0x2a2e34);
      g.box(0, 4.05, 3.0, 0.4, 1.15, 0.36, 0x15171a);
      const ph = p.extra;
      [[0xff2a2a, 0, 4.85], [0xffb400, 1, 4.5], [0x2aff6a, 2, 4.15]].forEach(([c, l, y]) => g.box(0, y, 3.2, 0.26, 0.26, 0.08, c, TL(l, ph)));
      break;
    }
    case 'tree': {
      const h = 3 + r() * 2.4, c = TREE_G[s % 4];
      g.cyl(0, 0, 0, 0.26, 0.18, h, 5, 0x6b4a2f);
      g.blob(0, h + 1.3, 0, 2.3 + r(), 2 + r() * 0.8, 2.3 + r(), c, 1, NOW, 0.1);
      g.blob(0.9, h + 0.5, 0.5, 1.4, 1.2, 1.4, TREE_G[(s + 1) % 4], 0);
      break;
    }
    case 'pine': {
      const h = 7 + r() * 6, c = [0x1f6b3a, 0x2a7d46, 0x1a5c34][s % 3];
      g.cyl(0, 0, 0, 0.28, 0.2, h * 0.3, 5, 0x5a3d28);
      for (let i = 0; i < 3; i++) g.cyl(0, h * (0.2 + i * 0.25), 0, 2.6 - i * 0.7, 0, h * 0.36, 7, c);
      break;
    }
    case 'palm': {
      const h = 6 + r() * 3.5, lean = 0.5 + r() * 0.9;
      for (let i = 0; i < 5; i++) g.cyl((lean * i * i) / 25, (h * i) / 5, 0, 0.24 - i * 0.02, 0.22 - i * 0.02, h / 5 + 0.05, 5, 0x8a6a4a, NOW, false);
      const tx = lean, cg = g.color(0x2fa35a), cg2 = g.color(0x4cc46a);
      for (let i = 0; i < 7; i++) {
        const a = (i / 7) * 6.283 + s, ca = Math.cos(a), sa = Math.sin(a), L = 3 + r() * 0.8;
        const m = [tx + ca * L * 0.55, h + 0.75, sa * L * 0.55], e = [tx + ca * L, h - 0.5, sa * L], w = 0.55;
        g.quad([tx - sa * 0.15, h, ca * 0.15], [tx + sa * 0.15, h, -ca * 0.15], [m[0] + sa * w, m[1], m[2] - ca * w], [m[0] - sa * w, m[1], m[2] + ca * w], i % 2 ? cg : cg2);
        g.quad([m[0] - sa * w, m[1], m[2] + ca * w], [m[0] + sa * w, m[1], m[2] - ca * w], [e[0], e[1], e[2]], [e[0], e[1], e[2]], i % 2 ? cg2 : cg);
        // dessous des palmes
        g.quad([tx + sa * 0.15, h - 0.02, -ca * 0.15], [tx - sa * 0.15, h - 0.02, ca * 0.15], [m[0] - sa * w, m[1] - 0.02, m[2] + ca * w], [m[0] + sa * w, m[1] - 0.02, m[2] - ca * w], g.color(0x1f7a42));
        g.tri([m[0] + sa * w, m[1] - 0.02, m[2] - ca * w], [m[0] - sa * w, m[1] - 0.02, m[2] + ca * w], [e[0], e[1] - 0.02, e[2]], g.color(0x1f7a42));
      }
      g.blob(tx, h - 0.1, 0, 0.4, 0.3, 0.4, 0x6b4a2f, 0);
      break;
    }
    case 'bush': g.blob(0, 0.5, 0, 1.2 + r(), 0.8, 1.2 + r(), TREE_G[s % 4], 0, NOW, 0.12); break;
    case 'rock': g.blob(0, 0.4, 0, 1.4 + r() * 1.2, 1 + r(), 1.3 + r(), [0x8a8580, 0x9b958c, 0x77726d][s % 3], 0, NOW, 0.18); break;
    case 'parasol': { const c = [0xff5a5f, 0xffd166, 0x06d6a0, 0x4cc9f0, 0xf72585][s % 5]; g.cyl(0, 0, 0, 0.04, 0.04, 2.2, 4, 0xf0f0f0); g.cyl(0, 2.0, 0, 1.5, 0, 0.5, 8, c); g.box(1.0, 0.25, 0.4, 0.7, 0.08, 1.9, 0xfdf6e3); break; }
    case 'billboard': {
      g.cyl(-3.5, 0, 0, 0.22, 0.22, 9, 5, 0x4a4f57); g.cyl(3.5, 0, 0, 0.22, 0.22, 9, 5, 0x4a4f57);
      g.box(0, 8.6, -0.25, 12.6, 6.6, 0.3, 0x23272e);
      signQuad(signs, g, 0, 11.9, 0.0, 12, 6, s % 16);
      break;
    }
    case 'containers': {
      const cols = [0xc0392b, 0x2e86c1, 0x27ae60, 0xf39c12, 0x8e44ad, 0x7f8c8d];
      const n = 1 + (s % 3);
      for (let i = 0; i < n; i++) for (let k = 0; k < 2; k++) g.box(k ? 0 : 0, i * 2.6, k * 0 , 2.5, 2.55, 12.2, cols[(s + i * 3 + k) % 6], NOW);
      break;
    }
    case 'crane': {
      const c = 0xf2a900;
      for (const x of [-7, 7]) for (const z of [-5, 5]) g.box(x, 0, z, 1, 30, 1, c);
      g.box(0, 30, 0, 16, 2, 12, c); g.box(0, 32, 18, 2.4, 2, 60, c); g.box(0, 34, -6, 5, 5, 6, 0xe9edf2, WIN(2, 2));
      g.box(0, 22, 28, 0.3, 10, 0.3, 0x222222); g.box(0, 19.5, 28, 2.5, 2.6, 12, 0x2e86c1);
      break;
    }
    case 'lighthouse': {
      g.cyl(0, 0, 0, 4, 2.6, 24, 10, 0xf7f3ea); g.cyl(0, 8, 0, 3.6, 3.25, 4, 10, 0xd62828); g.cyl(0, 16, 0, 3.05, 2.8, 4, 10, 0xd62828);
      g.cyl(0, 24, 0, 3.4, 3.4, 0.5, 10, 0x33373d); g.cyl(0, 24.5, 0, 1.8, 1.8, 3, 8, 0xfff2a8, EMI(0.6, 4)); g.cyl(0, 27.5, 0, 2.4, 0, 2.4, 8, 0xd62828);
      glow.push(p.x, p.y + 26, p.z, 0xfff2a8, 40);
      break;
    }
    case 'antenna': {
      for (let i = 0; i < 6; i++) g.cyl(0, i * 9, 0, 1.4 - i * 0.2, 1.2 - i * 0.2, 9, 4, i % 2 ? 0xe63946 : 0xf1faee, NOW, false);
      g.box(0, 54, 0, 0.5, 0.5, 0.5, 0xff2222, EMI(2, 4)); glow.push(p.x, p.y + 54, p.z, 0xff2222, 16);
      break;
    }
    case 'turbine': {
      g.cyl(0, 0, 0, 1.1, 0.6, 46, 7, 0xf2f4f7); g.blob(0, 46, 0.5, 1.2, 1.2, 2.4, 0xf2f4f7, 0);
      for (let i = 0; i < 3; i++) { const a = (i / 3) * 6.283 + s; g.quad([0, 46, 2.2], [Math.cos(a) * 20 - Math.sin(a) * 1.2, 46 + Math.sin(a) * 20 + Math.cos(a) * 1.2, 2.2], [Math.cos(a) * 20, 46 + Math.sin(a) * 20, 2.2], [Math.sin(a) * 1.2, 46 - Math.cos(a) * 1.2, 2.2], g.color(0xf7f9fb)); g.quad([0, 46, 2.15], [Math.sin(a) * 1.2, 46 - Math.cos(a) * 1.2, 2.15], [Math.cos(a) * 20, 46 + Math.sin(a) * 20, 2.15], [Math.cos(a) * 20 - Math.sin(a) * 1.2, 46 + Math.sin(a) * 20 + Math.cos(a) * 1.2, 2.15], g.color(0xdfe5ea)); }
      break;
    }
    case 'ferris': {
      const R = 26, cy = 30;
      for (const x of [-3, 3]) { g.box(x, 0, -8, 0.8, 0.8, 0.8, 0x2b2f36); for (const zz of [-9, 9]) { const n = 8; for (let i = 0; i < n; i++) g.box(x, (cy * i) / n, zz * (1 - i / n), 0.6, cy / n + 0.2, 0.6, 0xe9edf2); } }
      for (let i = 0; i < 24; i++) {
        const a = (i / 24) * 6.283, a2 = ((i + 1) / 24) * 6.283, c = NEONC[i % NEONC.length];
        const y1 = cy + Math.sin(a) * R, z1 = Math.cos(a) * R, y2 = cy + Math.sin(a2) * R, z2 = Math.cos(a2) * R;
        g.box(0, (y1 + y2) / 2 - 0.3, (z1 + z2) / 2, 0.5, 0.6, 7, c, EMI(0.5, 3));
        if (i % 2 === 0) { g.box(0, cy + Math.sin(a) * R * 0.5 - 0.15, Math.cos(a) * R * 0.5, 0.25, 0.3, 0.3, 0xcfd6de); g.box(0, y1 - 3, z1, 2.4, 2.4, 2.4, NEONC[(i / 2) % NEONC.length], WIN(1.2, 2.4)); }
      }
      g.cyl(0, cy - 1.5, 0, 2, 2, 3, 8, 0xff2bd6, EMI(0.6, 3));
      glow.push(p.x, p.y + cy, p.z, 0xff2bd6, 60);
      break;
    }
    case 'grandstand': {
      for (let i = 0; i < 6; i++) g.box(0, i * 1.4, 26 + i * 2.2, 70, 1.4, 2.3, i % 2 ? 0x2a6cff : 0xf2f4f8);
      g.box(0, 0, 40.5, 70, 14, 1, 0x3a4048); g.box(0, 14, 34, 72, 0.6, 16, 0xff7a00);
      signQuad(signs, g, 0, 13.5, 26, 20, 5, 0); signQuad(signs, g, -24, 13.5, 26, 16, 4, 15); signQuad(signs, g, 24, 13.5, 26, 16, 4, 6);
      break;
    }
  }
}
function signQuad(signs, g, x, yTop, z, w, h, cell) {
  // panneau vertical orienté vers +z local (double face)
  const u0 = (cell % 4) / 4, v1 = 1 - Math.floor(cell / 4) / 4, u1 = u0 + 0.25, v0 = v1 - 0.25;
  const P = (lx, ly, lz) => [g.ox + lx * g.cs + lz * g.sn, g.oy + ly, g.oz - lx * g.sn + lz * g.cs];
  const a = P(x - w / 2, yTop - h, z), b = P(x + w / 2, yTop - h, z), c = P(x + w / 2, yTop, z), d = P(x - w / 2, yTop, z);
  signs.pos.push(...a, ...b, ...c, ...a, ...c, ...d, ...b, ...a, ...d, ...b, ...d, ...c);
  signs.uv.push(u0, v0, u1, v0, u1, v1, u0, v0, u1, v1, u0, v1, u0, v0, u1, v0, u1, v1, u0, v0, u1, v1, u0, v1);
}

function drawBuilding(g, glow, signs, b) {
  const r = rng(b.seed), f = -b.face, { w, d, h } = b;
  g.at(b.x, b.y, b.z, b.yaw);
  const sd = (b.seed % 97) * 3.1;
  const front = (f * d) / 2;
  switch (b.kind) {
    case 'tower': case 'office': case 'glass': {
      const c = TOWERC[b.seed % TOWERC.length], glass = b.kind === 'glass' || (b.kind === 'tower' && r.chance(0.45));
      const wt = glass ? GLASS(2 + r() * 1.2, 3.4 + r()) : WIN(2.6 + r() * 1.6, 3.3 + r() * 0.6);
      const base = glass ? [0x24303c, 0x14494f, 0x2b2f3a, 0x4a3b22, 0x1c2f5a, 0x0f3d3a, 0x3a2a4a][b.seed % 7] : c;
      const pod = Math.min(h * 0.22, 9 + r() * 6);
      if (h > 60 && r.chance(0.7)) {
        g.box(0, 0, 0, w, pod, d, c, WIN(3.4, 4.2), { seed: sd });
        const tw = w * (0.62 + r() * 0.2), td = d * (0.62 + r() * 0.2), h2 = h * (0.7 + r() * 0.15);
        g.box(0, pod, 0, tw, h2 - pod, td, base, wt, { seed: sd + 3 });
        g.box(0, h2, 0, tw * 0.7, h - h2, td * 0.7, base, wt, { seed: sd + 5, top: 0x3a3f47 });
        if (r.chance(0.5)) { g.cyl(0, h, 0, 0.5, 0.1, 16 + r() * 14, 4, 0xbfc7d1); g.box(0, h + 14, 0, 0.5, 0.5, 0.5, 0xff2222, EMI(2, 4)); }
      } else {
        g.box(0, 0, 0, w, h, d, base, wt, { seed: sd, top: 0x3a3f47 });
        g.box(w * 0.2, h, d * 0.15, w * 0.3, 2.2, d * 0.3, 0x8b929b);
        if (r.chance(0.5)) g.box(-w * 0.25, h, -d * 0.2, 3, 1.4, 3, 0x9aa1aa);
      }
      g.box(0, 0, front + f * 0.25, w * 0.5, 4.4, 0.5, 0x1b2026, GLASS(2.4, 4.4));
      if (r.chance(0.3)) { g.at(b.x, b.y, b.z, b.yaw + (f > 0 ? 0 : Math.PI)); signQuad(signs, g, 0, Math.min(h - 2, 12), d / 2 + 0.3, Math.min(w * 0.6, 14), 3.4, r.int(0, 15)); g.at(b.x, b.y, b.z, b.yaw); }
      break;
    }
    case 'neon': {
      const c = [0x1c1b2e, 0x22203a, 0x151a2b, 0x2a1d3a, 0x101522][b.seed % 5], nc = NEONC[b.seed % NEONC.length], nc2 = NEONC[(b.seed + 3) % NEONC.length];
      g.box(0, 0, 0, w, h, d, c, NEONW(2.8 + r(), 3.2), { seed: sd, top: 0x16161f });
      g.box(0, h, 0, w + 0.3, 0.35, d + 0.3, nc, EMI(0.35, 2.6));
      g.box(0, 4.2, front + f * 0.2, w * 0.96, 0.3, 0.3, nc2, EMI(0.4, 3));
      if (r.chance(0.75)) { g.box(w * 0.5 + 0.35, h * 0.3, front - f * 1.2, 0.5, h * 0.55, 1.8, 0x0d0d14); g.box(w * 0.5 + 0.35, h * 0.3 + 0.3, front - f * 1.2, 0.62, h * 0.55 - 0.6, 1.3, nc, EMI(0.5, 3)); }
      g.at(b.x, b.y, b.z, b.yaw + (f > 0 ? 0 : Math.PI));
      signQuad(signs, g, 0, Math.min(h - 1, 9.5), d / 2 + 0.35, Math.min(w * 0.7, 12), 3.2, [3, 8, 9, 10, 11, 12, 13, 15, 0, 1, 2][b.seed % 11]);
      glow.push(b.x - Math.sin(b.yaw) * 0 + Math.sin(b.yaw) * front * 1.1, b.y + 6, b.z + Math.cos(b.yaw) * front * 1.1, nc, 22);
      g.at(b.x, b.y, b.z, b.yaw);
      break;
    }
    case 'old': {
      const c = OLDC[b.seed % OLDC.length], roof = [0xb5543a, 0xc4633f, 0xa84a32, 0x9c5a3c][b.seed % 4];
      g.box(0, 0, 0, w, h, d, c, OLDW(2.4 + r() * 0.8, 2.9), { seed: sd, noTop: true });
      g.gable(0, h, 0, w + 0.6, 2.4 + r() * 1.6, d, roof, c);
      if (r.chance(0.45)) { const ac = [0xd62828, 0x2a9d8f, 0xe9c46a, 0x457b9d, 0xf4f1de][b.seed % 5]; g.quad([-w * 0.4, 2.2, front + f * 1.5], [w * 0.4, 2.2, front + f * 1.5], [w * 0.4, 3.1, front], [-w * 0.4, 3.1, front], g.color(ac)); g.quad([w * 0.4, 2.2, front + f * 1.5], [-w * 0.4, 2.2, front + f * 1.5], [-w * 0.4, 3.1, front], [w * 0.4, 3.1, front], g.color(ac, 0.7)); }
      if (r.chance(0.3)) g.box(w * 0.25, h + 1, 0, 0.8, 2.6, 0.8, 0x8a5a44);
      if (b.seed % 41 === 0) { g.box(0, h, 0, 5, 14, 5, c, OLDW(2.5, 3.5)); g.cyl(0, h + 14, 0, 4, 0, 7, 4, roof); }
      break;
    }
    case 'warehouse': case 'factory': {
      const c = WAREC[b.seed % WAREC.length];
      g.box(0, 0, 0, w, h, d, c, NOW, { noTop: true });
      g.gable(0, h, 0, w, 2.6, d - 0.6, 0x6a737d, c);
      for (let i = -1; i <= 1; i++) g.box(i * w * 0.28, 0, front + f * 0.12, 7, 5.5, 0.3, 0x2b3038);
      g.box(0, h * 0.62, front + f * 0.1, w * 0.9, 1.6, 0.2, 0x1e2630, GLASS(3, 1.6));
      if (b.kind === 'factory') { for (const x of [-w * 0.3, w * 0.1]) { g.cyl(x, h, d * 0.2, 1.6, 1.3, 16 + r() * 10, 7, 0xb9a89a); g.cyl(x, h + 16, d * 0.2, 1.45, 1.45, 1.2, 7, 0xc0392b); } g.cyl(w * 0.34, 0, -d * 0.1, 4.5, 4.5, h + 3, 9, 0xd9dde2); }
      break;
    }
    case 'villa': {
      const c = PASTEL[b.seed % PASTEL.length], h1 = h * 0.55;
      g.box(0, 0, 0, w, h1, d, c, WIN(3.4, h1), { seed: sd, top: 0xe8e2d4 });
      g.box(-w * 0.18, h1, -f * d * 0.12, w * 0.6, h - h1, d * 0.7, c, GLASS(2.6, h - h1), { seed: sd, top: 0xf4efe6 });
      g.box(0, h1, front * 0.8, w * 0.98, 0.9, 0.12, 0x9fd3e6);
      g.box(0, h, -f * d * 0.12, w * 0.7, 0.25, d * 0.82, [0xd96c4a, 0xf4efe6, 0x2a9d8f][b.seed % 3]);
      if (r.chance(0.5)) g.box(w * 0.3, 0.02, front + f * 4.5, 5, 0.06, 3.4, 0x35c8f0, EMI(0.25, 0.9));
      break;
    }
    case 'hotel': {
      const c = PASTEL[b.seed % PASTEL.length];
      g.box(0, 0, 0, w, h, d, c, WIN(3, 3.1), { seed: sd, top: 0xe8e2d4 });
      for (let y = 3.1; y < h - 1; y += 3.1) g.box(0, y, front + f * 0.5, w * 0.96, 0.18, 1, 0xf7f7f2);
      g.box(0, h, 0, w * 0.5, 2.6, d * 0.5, 0xf7f7f2);
      g.at(b.x, b.y, b.z, b.yaw + (f > 0 ? 0 : Math.PI)); signQuad(signs, g, 0, h + 2.4, d * 0.25 + 0.1, Math.min(w * 0.45, 10), 2.4, 4); g.at(b.x, b.y, b.z, b.yaw);
      break;
    }
    case 'cabin': g.box(0, 0, 0, w, h * 0.6, d, 0x8a5a36, OLDW(3, h * 0.6), { noTop: true }); g.gable(0, h * 0.6, 0, w + 1, h * 0.7, d, 0x4a3426, 0x8a5a36); g.box(w * 0.3, h * 0.6, 0, 0.8, 3, 0.8, 0x6d6d6d); break;
    default: { // maison
      const c = PASTEL[(b.seed + 2) % PASTEL.length];
      if (h > 12) { g.box(0, 0, 0, w, h, d, TOWERC[b.seed % TOWERC.length], WIN(3, 3.2), { seed: sd, top: 0x4a4f57 }); }
      else { g.box(0, 0, 0, w, h * 0.62, d, c, WIN(3.6, h * 0.62), { seed: sd, noTop: true }); g.gable(0, h * 0.62, 0, w + 0.8, h * 0.45, d, [0x8d5a44, 0x556270, 0xa8553a][b.seed % 3], c); }
    }
  }
}

function drawPoi(g, glow, signs, road, p) {
  if (p.type === 'token' || p.type === 'radar' || p.type === 'drift') {
    if (p.type === 'radar') { g.at(p.x, p.y, p.z, p.yaw); const x = p.hw + 1.6; g.cyl(x, 0, 0, 0.12, 0.1, 4.2, 5, 0x3a4048); g.box(x, 4.2, 0, 0.9, 0.6, 0.7, 0xffd400); g.box(x, 4.35, 0.36, 0.3, 0.3, 0.06, 0x111111); g.box(x, 2.6, 0, 1.2, 1.0, 0.08, 0x1e5bff); }
    if (p.type === 'drift') { g.at(p.x, p.y, p.z, p.yaw); for (const s of [1, -1]) { g.cyl(s * (p.hw + 1.2), 0, 0, 0.16, 0.16, 6.5, 5, 0xff2bd6, EMI(0.4, 2)); } g.box(0, 6.2, 0, p.hw * 2 + 2.8, 1.1, 0.3, 0x12002b); g.box(0, 6.4, 0.2, p.hw * 2 + 2.2, 0.16, 0.1, 0xff2bd6, EMI(0.8, 3)); g.box(0, 6.4, -0.2, p.hw * 2 + 2.2, 0.16, 0.1, 0x00e5ff, EMI(0.8, 3)); }
    return;
  }
  if (p.type === 'jump') {
    const R = MAP.ramps[p.rampId];
    g.at(R.x, R.y, R.z, R.yaw);
    const w = R.width / 2, L = R.len, h = R.h, c1 = g.color(0xff7a00), c2 = g.color(0xf4f4f4), cs = g.color(0x2b2f36);
    const n = 6;
    for (let i = 0; i < n; i++) { const a = (i / n) * L, b2 = ((i + 1) / n) * L, ya = (a / L) * h, yb = (b2 / L) * h; g.quad([w, ya + 0.02, a], [-w, ya + 0.02, a], [-w, yb + 0.02, b2], [w, yb + 0.02, b2], i % 2 ? c1 : c2); }
    g.quad([-w, 0, L], [w, 0, L], [w, h, L], [-w, h, L], cs);
    g.tri([w, 0, 0], [w, 0, L], [w, h, L], cs); g.tri([-w, 0, L], [-w, 0, 0], [-w, h, L], cs);
    for (const s of [1, -1]) { g.cyl(s * (w + 0.5), 0, 0, 0.08, 0.08, 4.5, 4, 0xf4f4f4); g.box(s * (w + 0.5), 3.5, 0.5, 0.05, 0.9, 1, 0xff7a00); }
    return;
  }
  g.at(p.x, p.y, p.z, p.yaw);
  const lot = (w, d, zc, surf = 7) => { // dalle de parking alignée sur le POI
    const P = (lx, lz) => [g.ox + lx * g.cs + lz * g.sn, g.oy + 0.06, g.oz - lx * g.sn + lz * g.cs];
    const a = P(-w / 2, zc - d / 2), b2 = P(w / 2, zc - d / 2), c = P(w / 2, zc + d / 2), e = P(-w / 2, zc + d / 2);
    road.pos.push(...a, ...e, ...c, ...a, ...c, ...b2);
    road.uv.push(0, 0, 0, d, 1, d, 0, 0, 1, d, 1, 0);
    for (let i = 0; i < 6; i++) road.rp.push(0, surf, w, 1);
  };
  switch (p.type) {
    case 'garage': {
      lot(34, 26, 4, 4);
      g.box(0, 0, -6, 22, 7, 12, 0x2b3038, NOW, { top: 0x3a4048 });
      g.box(0, 7, -6, 23, 0.6, 13, 0xff7a00, EMI(0.3, 1.6));
      for (const x of [-6.5, 0, 6.5]) g.box(x, 0, 0.1, 5.2, 4.4, 0.3, 0x12161c, GLASS(1.3, 1.1));
      signQuad(signs, g, 0, 10.6, 0.2, 10, 3, p.name.includes('Tuning') ? 12 : 5);
      glow.push(p.x, p.y + 6, p.z, 0xff7a00, 26);
      break;
    }
    case 'dealer': {
      lot(46, 34, 6, 7);
      g.box(0, 0, -7, 30, 8, 14, 0x1b2733, GLASS(2.6, 4), { top: 0xe9edf2 });
      g.box(0, 8, -7, 32, 0.7, 16, 0xf2f4f8);
      g.box(0, 0.02, 4, 26, 0.3, 6, 0xe9edf2);
      signQuad(signs, g, 0, 13, 0.2, 12, 3.4, p.name.includes('Superessence') ? 0 : 6);
      g.cyl(-17, 0, 6, 0.3, 0.3, 12, 5, 0xbfc7d1); g.at(p.x, p.y, p.z, p.yaw); signQuad(signs, g, -17, 15, 6, 6, 3, 6);
      glow.push(p.x, p.y + 7, p.z, 0xcfe8ff, 30);
      break;
    }
    case 'fuel': {
      lot(34, 22, 4, 4);
      g.box(0, 5.2, 4, 22, 0.7, 12, 0xf2f4f8); g.box(0, 5.05, 4, 22.3, 0.24, 12.3, 0xff7a00, EMI(0.4, 2.2));
      for (const x of [-8, 8]) for (const z of [0, 8]) g.cyl(x, 0, z, 0.3, 0.3, 5.2, 6, 0xdfe4ea);
      for (const x of [-5, 0, 5]) { g.box(x, 0, 4, 0.9, 1.9, 0.7, 0xff7a00); g.box(x, 1.2, 4.36, 0.6, 0.45, 0.04, 0x0b0f1a, EMI(0.6, 1.5)); g.box(x, 0, 4, 2.4, 0.2, 3.2, 0xc9ced6); }
      g.box(0, 0, -7, 14, 4, 6, 0xf7f3ea, GLASS(2.6, 4), { top: 0xff7a00 });
      signQuad(signs, g, 0, 7.4, 10.1, 9, 1.8, 0);
      g.cyl(13, 0, 10, 0.3, 0.3, 10, 5, 0xdfe4ea); signQuad(signs, g, 13, 13, 10, 6, 3, 0);
      glow.push(p.x, p.y + 5, p.z, 0xfff0cc, 30);
      break;
    }
    case 'meet': case 'parking': {
      const r2 = p.r;
      lot(r2 * 2, r2 * 1.5, 0, 7);
      if (p.type === 'meet') { for (const s of [1, -1]) { g.cyl(s * r2, 0, -r2 * 0.75, 0.2, 0.2, 9, 5, 0x3a4048); g.box(s * r2, 9, -r2 * 0.75 + 0.5, 1.6, 0.5, 1.6, 0xffffff, EMI(0.5, 4)); glow.push(p.x, p.y + 9, p.z, 0xffffff, 30); } signQuad(signs, g, 0, 8, -r2 * 0.75, 12, 3, 15); g.box(0, 0, -r2 * 0.75, 12.4, 8.2, 0.3, 0x23272e); }
      else { g.cyl(-r2 + 1, 0, -r2 * 0.7, 0.14, 0.14, 4, 5, 0x3a4048); g.box(-r2 + 1, 3, -r2 * 0.7, 1.6, 1.6, 0.14, 0x1e5bff); g.box(-r2 + 1, 3.3, -r2 * 0.7 + 0.08, 0.3, 1, 0.04, 0xffffff); }
      break;
    }
    case 'view': {
      lot(18, 14, 0, 8);
      for (let i = -4; i <= 4; i++) g.cyl(i * 2, 0, -6.6, 0.07, 0.07, 1.1, 4, 0xe9edf2);
      g.box(0, 1.05, -6.6, 16.4, 0.1, 0.1, 0xe9edf2);
      g.cyl(7, 0, 5, 0.1, 0.1, 5, 5, 0xe9edf2); g.box(7, 4, 5.9, 0.05, 1, 1.7, 0x00e5ff, EMI(0.4, 2));
      g.cyl(-5, 0, -5, 0.14, 0.2, 1.3, 5, 0x3a4048); g.box(-5, 1.3, -5, 0.3, 0.3, 0.6, 0x2a6cff);
      g.box(3, 0, 3, 3, 0.45, 0.8, 0x8a5a36);
      break;
    }
  }
}

// ---------------------------------------------------------------------------------------------
// Monde
// ---------------------------------------------------------------------------------------------
export class World {
  constructor(scene, q) {
    prepare();
    this.scene = scene;
    this.q = q;
    this.chunks = new Map();
    this.queue = [];
    this.terrainMat = new THREE.MeshLambertMaterial({ vertexColors: true });
    this.staticMat = makeStaticMat();
    this.roadMat = makeRoadMat();
    this.signMat = new THREE.MeshBasicMaterial({ map: makeSignTexture() });
    this.glowMat = new THREE.PointsMaterial({ map: glowTex(), size: 1, vertexColors: true, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, sizeAttenuation: true, opacity: 0 });
    this.glowMat.onBeforeCompile = (sh) => { sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nattribute float gsize;').replace('gl_PointSize = size;', 'gl_PointSize = size * gsize;'); };
    this.poolMat = new THREE.MeshBasicMaterial({ map: glowTex(), color: 0xffd9a0, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, opacity: 0, vertexColors: true });
    // mer
    const wg = new THREE.PlaneGeometry(9000, 3000, 1, 1);
    wg.rotateX(-Math.PI / 2);
    this.waterMat = new THREE.MeshStandardMaterial({ color: 0x1fa8d8, roughness: 0.12, metalness: 0.55, transparent: true, opacity: 0.86 });
    this.water = new THREE.Mesh(wg, this.waterMat);
    this.water.position.set(250, MAP.SEA_LEVEL, 3000);
    this.water.renderOrder = 1;
    scene.add(this.water);
    this.cx = 1e9; this.cz = 1e9;
  }
  setQuality(q) { this.q = q; this.cx = 1e9; }
  // Charge / décharge les secteurs autour d'un point. Renvoie le nombre de secteurs encore en attente.
  update(x, z, budget = 1) {
    const cx = cOf(x), cz = cOf(z), R = this.q.chunkRadius;
    if (cx !== this.cx || cz !== this.cz) {
      this.cx = cx; this.cz = cz;
      this.queue.length = 0;
      for (let i = -R; i <= R; i++) for (let j = -R; j <= R; j++) {
        if (i * i + j * j > (R + 0.5) * (R + 0.5)) continue;
        const k = ckey(cx + i, cz + j);
        if (!this.chunks.has(k)) this.queue.push([cx + i, cz + j, i * i + j * j]);
      }
      this.queue.sort((a, b) => b[2] - a[2]);
      for (const [k, c] of this.chunks) {
        if (Math.abs(c.cx - cx) > R + 1 || Math.abs(c.cz - cz) > R + 1) { this.dispose(c); this.chunks.delete(k); }
      }
    }
    while (budget-- > 0 && this.queue.length) {
      const [qx, qz] = this.queue.pop();
      const k = ckey(qx, qz);
      if (!this.chunks.has(k)) this.chunks.set(k, this.build(qx, qz));
    }
    this.water.position.x = clamp(x, -1500, 2000);
    return this.queue.length;
  }
  dispose(c) {
    for (const m of c.meshes) { this.scene.remove(m); m.geometry.dispose(); }
  }
  setNight(n, lampOn) {
    uniforms.uNight.value = n;
    this.glowMat.opacity = lampOn * 0.95;
    this.poolMat.opacity = lampOn * 0.34;
    for (const c of this.chunks.values()) { if (c.glow) c.glow.visible = lampOn > 0.02; if (c.pool) c.pool.visible = lampOn > 0.02; }
    this.lampOn = lampOn;
  }
  build(cx, cz) {
    const k = ckey(cx, cz), x0 = cx * CH, z0 = cz * CH, meshes = [];
    const chunk = { cx, cz, meshes, glow: null, pool: null };
    const inWorld = x0 + CH > BOUNDS.x0 - 600 && x0 < BOUNDS.x1 + 600 && z0 + CH > BOUNDS.z0 - 600 && z0 < BOUNDS.z1 + 900;
    if (!inWorld) return chunk;
    // ---------------- relief ----------------
    const N = this.q.terrainRes, st = CH / N, tp = [], tc = [];
    const H = new Float32Array((N + 1) * (N + 1)), C = new Float32Array((N + 1) * (N + 1) * 3);
    const runs = roadRuns.get(k) || [];
    let flat = true;
    for (let j = 0; j <= N; j++) for (let i = 0; i <= N; i++) {
      const x = x0 + i * st, z = z0 + j * st, o = j * (N + 1) + i;
      const xc = clamp(x, BOUNDS.x0 - 300, BOUNDS.x1 + 300), zc = clamp(z, BOUNDS.z0 - 300, BOUNDS.z1 + 800);
      let h = MAP.groundHeight(xc, zc);
      const n = MAP.nearestRoad(x, z, 30, _n);
      const base = MAP.baseHeight(xc, zc);
      if (n) { const steep = Math.abs(base - n.y) > 0.3; h -= (steep ? 1.6 : 0.16) * (1 - smoothstep(n.hw + (steep ? 5 : 0.5), n.hw + (steep ? 17 : 2.5), n.d)); }
      if (Math.abs(h) > 0.2) flat = false;
      H[o] = h;
      const surf = MAP.offroadSurface(x, z), zn = MAP.zoneAt(x, z), nz = noise2(x / 37, z / 37), nz2 = hash2(i + cx * 31, j + cz * 17);
      let r, gg, b;
      if (surf === 'water' || surf === 'sand') { r = 0.9; gg = 0.8; b = 0.56; if (h < MAP.SEA_LEVEL) { r = 0.55; gg = 0.6; b = 0.5; } }
      else if (surf === 'pavement') { const cc = zn === 'downtown' ? [0.44, 0.44, 0.45] : zn === 'neon' ? [0.3, 0.3, 0.37] : zn === 'oldtown' ? [0.68, 0.6, 0.5] : [0.43, 0.42, 0.41]; r = cc[0]; gg = cc[1]; b = cc[2]; }
      else if (zn === 'mountain') {
        const sl = Math.abs(MAP.baseHeight(x + 6, z) - base) + Math.abs(MAP.baseHeight(x, z + 6) - base);
        const rock = smoothstep(2.2, 4.2, sl + nz * 1.5);
        r = lerp(0.3, 0.56, rock); gg = lerp(0.55, 0.53, rock); b = lerp(0.26, 0.5, rock);
        if (n && n.edge.type !== 'dirt' && Math.abs(base - n.y) > 1.5 && n.d < n.hw + 20) { r = 0.55; gg = 0.5; b = 0.44; }
      } else { r = 0.36; gg = 0.64; b = 0.3; if (zn === 'coastal') { r = 0.44; gg = 0.68; b = 0.32; } }
      const sh = 0.88 + 0.16 * nz + 0.06 * nz2;
      C[o * 3] = r * sh; C[o * 3 + 1] = gg * sh; C[o * 3 + 2] = b * sh;
    }
    const pushV = (i, j) => { const o = j * (N + 1) + i; tp.push(x0 + i * st, H[o], z0 + j * st); tc.push(C[o * 3], C[o * 3 + 1], C[o * 3 + 2]); };
    for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
      if ((i + j) % 2) { pushV(i, j); pushV(i, j + 1); pushV(i + 1, j + 1); pushV(i, j); pushV(i + 1, j + 1); pushV(i + 1, j); }
      else { pushV(i, j); pushV(i, j + 1); pushV(i + 1, j); pushV(i + 1, j); pushV(i, j + 1); pushV(i + 1, j + 1); }
    }
    // ---------------- routes ----------------
    const road = { pos: [], uv: [], rp: [] };
    const rquad = (a, b, c, d, u0, u1, va, vb, rp) => { // a,b = gauche/droite au début ; c,d = gauche/droite à la fin
      road.pos.push(...a, ...b, ...d, ...a, ...d, ...c);
      road.uv.push(u0, va, u1, va, u1, vb, u0, va, u1, vb, u0, vb);
      for (let i = 0; i < 6; i++) road.rp.push(rp[0], rp[1], rp[2], i === 0 || i === 1 || i === 3 ? rp[3] : rp[4]);
    };
    const tan = (e, i) => { const p = e.pts, a = p[Math.max(0, i - 1)], b = p[Math.min(p.length - 1, i + 1)], l = Math.hypot(b.x - a.x, b.z - a.z); return [(b.x - a.x) / l, (b.z - a.z) / l]; };
    const skirtC = (x, z, h) => { const zn = MAP.zoneAt(x, z); return zn === 'mountain' ? [0.52, 0.48, 0.42] : MAP.offroadSurface(x, z) === 'sand' ? [0.88, 0.78, 0.55] : [0.35, 0.6, 0.29]; };
    for (const run of runs) {
      const e = run.e, R = ROAD[e.type], hw = e.hw;
      const surf = { asphalt: 0, cobble: 1, dirt: 2, sand: 3 }[R.surf] + (e.type === 'track' ? 5 : 0);
      const ra = nodes[e.a].r + 1.5, rb = e.len - nodes[e.b].r - 1.5;
      const cityWalk = (e.type === 'avenue' || e.type === 'street' || e.type === 'alley' || e.type === 'industrial' || e.type === 'coastal');
      const sw = e.type === 'alley' ? 1.6 : e.type === 'coastal' ? 3.2 : 2.6;
      for (let i = run.i0; i <= run.i1; i++) {
        const p = e.pts[i], q = e.pts[i + 1], [tx, tz] = tan(e, i), [ux, uz] = tan(e, i + 1);
        const y0 = p.y + 0.03, y1 = q.y + 0.03;
        const L0 = [p.x + tz * hw, y0, p.z - tx * hw], R0 = [p.x - tz * hw, y0, p.z + tx * hw], L1 = [q.x + uz * hw, y1, q.z - ux * hw], R1 = [q.x - uz * hw, y1, q.z + ux * hw];
        const m0 = p.s > ra && p.s < rb ? 1 : 0, m1 = q.s > ra && q.s < rb ? 1 : 0;
        rquad(L0, R0, L1, R1, 0, 1, p.s, q.s, [R.lanes, surf, R.w, m0, m1]);
        if (cityWalk) {
          const zn = MAP.zoneAt(p.x, p.z), tint = zn === 'oldtown' ? 1.12 : zn === 'neon' ? 0.62 : 1;
          for (const sd of [1, -1]) {
            const a0 = [p.x + tz * sd * hw, y0 + 0.02, p.z - tx * sd * hw], b0 = [p.x + tz * sd * (hw + sw), y0 + 0.02, p.z - tx * sd * (hw + sw)];
            const a1 = [q.x + uz * sd * hw, y1 + 0.02, q.z - ux * sd * hw], b1 = [q.x + uz * sd * (hw + sw), y1 + 0.02, q.z - ux * sd * (hw + sw)];
            if (sd > 0) rquad(b0, a0, b1, a1, 0, 1, p.s, q.s, [tint, 8, sw, 1, 1]); else rquad(a0, b0, a1, b1, 0, 1, p.s, q.s, [tint, 8, sw, 1, 1]);
          }
        }
        // talus (déblais / remblais) en relief
        const bh0 = MAP.baseHeight(p.x, p.z);
        if (Math.abs(bh0 - p.y) > 0.3 || Math.abs(MAP.baseHeight(p.x + tz * (hw + 16), p.z - tx * (hw + 16)) - p.y) > 0.3 || Math.abs(MAP.baseHeight(p.x - tz * (hw + 16), p.z + tx * (hw + 16)) - p.y) > 0.3) {
          for (const sd of [1, -1]) {
            let pa = [p.x + tz * sd * hw, p.y - 0.02, p.z - tx * sd * hw], pb = [q.x + uz * sd * hw, q.y - 0.02, q.z - ux * sd * hw];
            for (const off of [3, 8, 14, 21]) {
              const ax = p.x + tz * sd * (hw + off), az = p.z - tx * sd * (hw + off), bx = q.x + uz * sd * (hw + off), bz = q.z - ux * sd * (hw + off);
              const na = [ax, MAP.groundHeight(ax, az) - (off === 21 ? 0.1 : 0), az], nb = [bx, MAP.groundHeight(bx, bz) - (off === 21 ? 0.1 : 0), bz];
              const c = off === 3 ? (R.surf === 'dirt' ? [0.44, 0.33, 0.22] : [0.42, 0.44, 0.4]) : skirtC(ax, az);
              const quadv = sd > 0 ? [na, pa, pb, na, pb, nb] : [pa, na, nb, pa, nb, pb];
              for (const v of quadv) { tp.push(v[0], v[1], v[2]); tc.push(c[0], c[1], c[2]); }
              pa = na; pb = nb;
            }
          }
        }
      }
    }
    // carrefours + passages piétons
    for (const n of nodesByChunk.get(k) || []) {
      const r = n.r + 0.8, y = n.y + 0.055, seg = 14, hasWalk = n.edges.some((id) => ['avenue', 'street', 'alley', 'coastal', 'industrial'].includes(edges[id].type));
      const surfN = n.edges.every((id) => ROAD[edges[id].type].surf === 'cobble') ? 1 : n.edges.every((id) => ROAD[edges[id].type].surf === 'dirt') ? 2 : n.edges.every((id) => ROAD[edges[id].type].surf === 'sand') ? 3 : 4;
      for (let i = 0; i < seg; i++) {
        const a0 = (i / seg) * 6.2832, a1 = ((i + 1) / seg) * 6.2832;
        road.pos.push(n.x, y, n.z, n.x + Math.sin(a0) * r, y, n.z + Math.cos(a0) * r, n.x + Math.sin(a1) * r, y, n.z + Math.cos(a1) * r);
        road.uv.push(0.5, n.z, 0.5 + Math.sin(a0) * 0.5, n.z + Math.cos(a0) * r, 0.5 + Math.sin(a1) * 0.5, n.z + Math.cos(a1) * r);
        for (let q = 0; q < 3; q++) road.rp.push(0, surfN, r * 2, 1);
      }
      if (n.light || (hasWalk && n.edges.length >= 3 && MAP.zoneAt(n.x, n.z) !== 'highway' && MAP.zoneAt(n.x, n.z) !== 'industrial')) for (const id of n.edges) {
        const e = edges[id];
        if (ROAD[e.type].surf !== 'asphalt' || e.type === 'highway') continue;
        const out = e.a === n.id, P1 = MAP.pointAt(e, out ? n.r + 1.4 : e.len - n.r - 1.4), P2 = MAP.pointAt(e, out ? n.r + 4.4 : e.len - n.r - 4.4), hw = e.hw - 0.5;
        const A = [P1.x + P1.tz * hw, P1.y + 0.075, P1.z - P1.tx * hw], B = [P1.x - P1.tz * hw, P1.y + 0.075, P1.z + P1.tx * hw], Cc = [P2.x + P2.tz * hw, P2.y + 0.075, P2.z - P2.tx * hw], D = [P2.x - P2.tz * hw, P2.y + 0.075, P2.z + P2.tx * hw];
        if (out) rquad(A, B, Cc, D, 0, 1, 0, 3, [0, 6, hw * 2, 1, 1]); else rquad(Cc, D, A, B, 0, 1, 0, 3, [0, 6, hw * 2, 1, 1]);
      }
    }
    // ---------------- décor ----------------
    const g = new GeoBuf(), glow = [], signs = { pos: [], uv: [] };
    for (const b of buildingsByChunk.get(k) || []) drawBuilding(g, glow, signs, b);
    for (const p of propsByChunk.get(k) || []) drawProp(g, glow, signs, p);
    for (const p of poisByChunk.get(k) || []) drawPoi(g, glow, signs, road, p);
    g.at(0, 0, 0, 0);
    for (const [a, b, kind] of railsByChunk.get(k) || []) {
      const c1 = kind === 'track' ? g.color(0xd62828) : g.color(0xb9c0c9), c2 = kind === 'track' ? g.color(0xf4f4f4) : g.color(0x8d959f), hgt = kind === 'track' ? 1.0 : 0.78;
      const alt = ((Math.round(a.x + a.z) / 9) | 0) % 2 ? c1 : c2;
      g.quad([a.x, a.y + 0.25, a.z], [b.x, b.y + 0.25, b.z], [b.x, b.y + hgt, b.z], [a.x, a.y + hgt, a.z], kind === 'track' ? alt : c1);
      g.quad([b.x, b.y + 0.25, b.z], [a.x, a.y + 0.25, a.z], [a.x, a.y + hgt, a.z], [b.x, b.y + hgt, b.z], kind === 'track' ? alt : c2);
      g.box(a.x, a.y, a.z, 0.14, hgt, 0.14, 0x6d747d);
    }
    // tunnels
    for (const t of MAP.tunnels) {
      const e = t.e, P = {}, Q = {};
      for (let s = t.s0; s < t.s1 - 0.5; s += 11) {
        MAP.pointAt(e, s, P);
        if (cOf(P.x) !== cx || cOf(P.z) !== cz) continue;
        MAP.pointAt(e, Math.min(s + 11, t.s1), Q);
        const w = e.hw + 1.3, hh = 6.4, cw = g.color(0x8f969e), cd = g.color(0x5d646c), ct = g.color(0x70777f);
        const L0 = [P.x + P.tz * w, P.y, P.z - P.tx * w], R0 = [P.x - P.tz * w, P.y, P.z + P.tx * w], L1 = [Q.x + Q.tz * w, Q.y, Q.z - Q.tx * w], R1 = [Q.x - Q.tz * w, Q.y, Q.z + Q.tx * w];
        const up = (v, dy) => [v[0], v[1] + dy, v[2]];
        g.quad(L1, L0, up(L0, hh), up(L1, hh), cw); g.quad(L0, L1, up(L1, hh + 0.8), up(L0, hh + 0.8), cd); // mur gauche (intérieur / extérieur)
        g.quad(R0, R1, up(R1, hh), up(R0, hh), cw); g.quad(R1, R0, up(R0, hh + 0.8), up(R1, hh + 0.8), cd);
        g.quad(up(L0, hh), up(R0, hh), up(R1, hh), up(L1, hh), cd); g.quad(up(R0, hh + 0.8), up(L0, hh + 0.8), up(L1, hh + 0.8), up(R1, hh + 0.8), ct);
        const lc = g.color(0xffe0a0);
        for (const sd of [1, -1]) { const lx = sd * (w - 0.3); g.quad([P.x + P.tz * lx, P.y + hh - 0.5, P.z - P.tx * lx], [Q.x + Q.tz * lx, Q.y + hh - 0.5, Q.z - Q.tx * lx], [Q.x + Q.tz * lx * 0.9, Q.y + hh - 0.1, Q.z - Q.tx * lx * 0.9], [P.x + P.tz * lx * 0.9, P.y + hh - 0.1, P.z - P.tx * lx * 0.9], lc, [0, 0, 1, 1], EMI(1.5, 2.2)); }
        if (s === t.s0 || s + 11 >= t.s1 - 0.5) { const F = s === t.s0 ? P : Q; g.quad([F.x + F.tz * (w + 1), F.y + hh, F.z - F.tx * (w + 1)], [F.x - F.tz * (w + 1), F.y + hh, F.z + F.tx * (w + 1)], [F.x - F.tz * (w + 1), F.y + hh + 2, F.z + F.tx * (w + 1)], [F.x + F.tz * (w + 1), F.y + hh + 2, F.z - F.tx * (w + 1)], cd); g.quad([F.x - F.tz * (w + 1), F.y + hh, F.z + F.tx * (w + 1)], [F.x + F.tz * (w + 1), F.y + hh, F.z - F.tx * (w + 1)], [F.x + F.tz * (w + 1), F.y + hh + 2, F.z - F.tx * (w + 1)], [F.x - F.tz * (w + 1), F.y + hh + 2, F.z + F.tx * (w + 1)], cd); }
      }
    }
    // ---------------- maillages ----------------
    const add = (m, shadow = false, cast = false) => { m.receiveShadow = shadow; m.castShadow = cast; m.matrixAutoUpdate = false; this.scene.add(m); meshes.push(m); return m; };
    {
      const tg = new THREE.BufferGeometry();
      tg.setAttribute('position', new THREE.Float32BufferAttribute(tp, 3));
      tg.setAttribute('color', new THREE.Float32BufferAttribute(tc, 3));
      tg.computeVertexNormals();
      add(new THREE.Mesh(tg, this.terrainMat), true);
    }
    if (road.pos.length) {
      const rg = new THREE.BufferGeometry();
      rg.setAttribute('position', new THREE.Float32BufferAttribute(road.pos, 3));
      rg.setAttribute('uv', new THREE.Float32BufferAttribute(road.uv, 2));
      rg.setAttribute('rp', new THREE.Float32BufferAttribute(road.rp, 4));
      rg.computeVertexNormals();
      add(new THREE.Mesh(rg, this.roadMat), true);
    }
    if (g.count) add(new THREE.Mesh(g.build(), this.staticMat), true, true);
    if (signs.pos.length) {
      const sg = new THREE.BufferGeometry();
      sg.setAttribute('position', new THREE.Float32BufferAttribute(signs.pos, 3));
      sg.setAttribute('uv', new THREE.Float32BufferAttribute(signs.uv, 2));
      add(new THREE.Mesh(sg, this.signMat));
    }
    if (glow.length) {
      const n = glow.length / 5, gp = new Float32Array(n * 3), gc = new Float32Array(n * 3), gs = new Float32Array(n), pp = [], pc = [], pu = [], col = new THREE.Color();
      for (let i = 0; i < n; i++) {
        gp[i * 3] = glow[i * 5]; gp[i * 3 + 1] = glow[i * 5 + 1]; gp[i * 3 + 2] = glow[i * 5 + 2];
        col.set(glow[i * 5 + 3]); gc[i * 3] = col.r; gc[i * 3 + 1] = col.g; gc[i * 3 + 2] = col.b; gs[i] = glow[i * 5 + 4];
        if (glow[i * 5 + 4] <= 8) { // flaque de lumière au sol
          const x = glow[i * 5], z = glow[i * 5 + 2], y = MAP.groundHeight(x, z) + 0.12, s = 9;
          pp.push(x - s, y, z - s, x - s, y, z + s, x + s, y, z + s, x - s, y, z - s, x + s, y, z + s, x + s, y, z - s);
          pu.push(0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0);
          for (let q = 0; q < 6; q++) pc.push(col.r, col.g, col.b);
        }
      }
      const gg = new THREE.BufferGeometry();
      gg.setAttribute('position', new THREE.BufferAttribute(gp, 3)); gg.setAttribute('color', new THREE.BufferAttribute(gc, 3)); gg.setAttribute('gsize', new THREE.BufferAttribute(gs, 1));
      chunk.glow = add(new THREE.Points(gg, this.glowMat));
      chunk.glow.visible = (this.lampOn || 0) > 0.02;
      if (pp.length && this.q.lightPools) {
        const pg = new THREE.BufferGeometry();
        pg.setAttribute('position', new THREE.Float32BufferAttribute(pp, 3)); pg.setAttribute('uv', new THREE.Float32BufferAttribute(pu, 2)); pg.setAttribute('color', new THREE.Float32BufferAttribute(pc, 3));
        chunk.pool = add(new THREE.Mesh(pg, this.poolMat));
        chunk.pool.renderOrder = 2;
        chunk.pool.visible = (this.lampOn || 0) > 0.02;
      }
    }
    void flat;
    return chunk;
  }
  stats() {
    let tris = 0, meshes = 0;
    for (const c of this.chunks.values()) for (const m of c.meshes) { meshes++; tris += (m.geometry.attributes.position.count / 3) | 0; }
    return { chunks: this.chunks.size, meshes, tris };
  }
}
