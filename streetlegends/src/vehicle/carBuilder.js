// Générateur de véhicules 3D : carrosseries lissées par sections (loft), passages de roue creusés, ailes galbées,
// vitrage, habitacle visible, optiques, jantes, kits carrosserie. Tout est paramétrique : chaque modèle a sa silhouette.
import * as THREE from 'three';
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js';
import { clamp, lerp, smoothstep } from '../core/util.js';

export const PAINT_FINISH = {
  gloss: { name: 'Brillant', metalness: 0.25, roughness: 0.32, clearcoat: 1, ccr: 0.06 },
  metal: { name: 'Métallisé', metalness: 0.75, roughness: 0.34, clearcoat: 1, ccr: 0.08 },
  pearl: { name: 'Nacré', metalness: 0.55, roughness: 0.28, clearcoat: 1, ccr: 0.04, irid: 1 },
  matte: { name: 'Mat', metalness: 0.3, roughness: 0.78, clearcoat: 0, ccr: 0.5 },
  satin: { name: 'Satiné', metalness: 0.55, roughness: 0.5, clearcoat: 0.35, ccr: 0.4 },
  chrome: { name: 'Chrome', metalness: 1, roughness: 0.08, clearcoat: 1, ccr: 0.02 },
  candy: { name: 'Candy', metalness: 0.9, roughness: 0.22, clearcoat: 1, ccr: 0.02 },
};

const M = {};
let envMap = null;
export function setCarEnvMap(tex) {
  envMap = tex;
}
function mat(key, make) {
  return M[key] || (M[key] = make());
}
const std = (o) => new THREE.MeshStandardMaterial(o);
export const sharedMats = () => ({
  dark: mat('dark', () => std({ color: 0x15171b, roughness: 0.75, metalness: 0.2 })),
  carbon: mat('carbon', () => std({ color: 0x1b1d22, roughness: 0.4, metalness: 0.6 })),
  chrome: mat('chrome', () => std({ color: 0xe8ecf2, roughness: 0.14, metalness: 1 })),
  rubber: mat('rubber', () => std({ color: 0x121214, roughness: 0.92, metalness: 0 })),
  under: mat('under', () => std({ color: 0x0b0c0e, roughness: 1, metalness: 0 })),
  plate: mat('plate', () => std({ color: 0xf2f2ea, roughness: 0.6, metalness: 0 })),
  disc: mat('disc', () => std({ color: 0x4a4e55, roughness: 0.45, metalness: 0.8 })),
});

// --------------------------------------------------------------------------------------------
// Profil de carrosserie
// --------------------------------------------------------------------------------------------
export function makeProfile(S) {
  const { L, W, H, wb, wr, gc, belt } = S;
  const cab = S.cab;
  const zf = L / 2 - S.fo * L, zr = zf - wb;
  const tf = S.fo, tr = (L / 2 - zr) / L;
  const hoodFront = Math.max(belt * S.hood * lerp(0.82, 0.98, S.nose), gc + 0.2);
  const noseTop = gc + (hoodFront - gc) * lerp(0.6, 0.97, S.nose);
  const deckY = belt * S.deck;
  const tailTop = deckY * 0.9;
  const open = !!S.cage;
  const bw = open ? 0.66 : 1; // buggy : caisse étroite, roues apparentes
  const top = (t) => {
    if (t < 0.05) return lerp(noseTop, hoodFront, Math.sin((t / 0.05) * Math.PI / 2));
    if (t < cab[0]) return lerp(hoodFront, belt, Math.pow(smoothstep(0.05, cab[0], t), 0.85));
    if (t <= cab[3]) return belt;
    if (t < 0.965) return lerp(belt, deckY, smoothstep(cab[3], Math.min(cab[3] + 0.08, 0.96), t));
    return lerp(deckY, tailTop, Math.pow((t - 0.965) / 0.035, 2));
  };
  const R = wr * 1.17 + 0.025;
  const archY = (t) => {
    let a = 0;
    for (const ta of [tf, tr]) {
      const dz = (t - ta) * L;
      if (Math.abs(dz) < R) a = Math.max(a, wr + Math.sqrt(R * R - dz * dz));
    }
    return a;
  };
  const R2 = R + 0.3;
  const fenderY = (t) => {
    let a = 0;
    for (const ta of [tf, tr]) {
      const dz = (t - ta) * L;
      if (Math.abs(dz) < R2) a = Math.max(a, wr + R + 0.07 - 0.9 * (R2 - Math.sqrt(R2 * R2 - dz * dz)));
    }
    return a;
  };
  const bottom = (t) => {
    let b = gc;
    if (t < 0.07) b += 0.13 * Math.pow(1 - t / 0.07, 2) * (1.25 - S.nose);
    if (t > 0.93) b += 0.11 * Math.pow((t - 0.93) / 0.07, 2);
    return b;
  };
  const halfw = (t) => {
    let w = W / 2;
    w *= lerp(S.noseW, 1, Math.pow(smoothstep(0, 0.2, t), 0.7));
    w *= lerp(1, S.tailW, smoothstep(0.8, 1, t));
    w *= 1 - 0.14 * Math.pow(1 - smoothstep(0, 0.035, t), 2) - 0.1 * Math.pow(smoothstep(0.97, 1, t), 2);
    const sg = (wr * 1.5) / L;
    w += S.flare * (Math.exp(-Math.pow((t - tf) / sg, 2)) + Math.exp(-Math.pow((t - tr) / sg, 2)));
    w -= 0.012 * Math.sin(Math.PI * clamp((t - tf) / (tr - tf), 0, 1));
    return w * bw;
  };
  const roofH = H - belt;
  const roof = (t) => {
    if (t <= cab[0] || t >= cab[3]) return 0;
    if (t < cab[1]) return roofH * Math.pow(Math.sin(((t - cab[0]) / (cab[1] - cab[0])) * Math.PI / 2), 0.9);
    if (t <= cab[2]) return roofH * (1 - 0.03 * Math.pow((t - cab[1]) / Math.max(0.001, cab[2] - cab[1]), 2));
    return roofH * 0.97 * Math.pow(Math.cos(((t - cab[2]) / (cab[3] - cab[2])) * Math.PI / 2), 0.75);
  };
  return { top, bottom, halfw, archY, fenderY, roof, zf, zr, tf, tr, zOf: (t) => L / 2 - t * L, roofH, open, bw, track: (W / 2 + S.flare) * (S.track || 1) };
}

function stations(S, P, n) {
  const set = [];
  for (let i = 0; i <= n; i++) set.push(i / n);
  set.push(0.012, 0.03, 0.05, 0.97, 0.985, ...S.cab);
  const R = (S.wr * 1.17 + 0.025) / S.L;
  for (const ta of [P.tf, P.tr]) for (const f of [-1, -0.93, -0.75, -0.5, -0.25, 0, 0.25, 0.5, 0.75, 0.93, 1]) set.push(ta + f * R);
  set.sort((a, b) => a - b);
  const out = [];
  for (const t of set) if (t >= 0 && t <= 1 && (!out.length || t - out[out.length - 1] > 0.006)) out.push(t);
  return out;
}

// Surface de la caisse au paramètre (t, angle de section) → [x,y,z]
function ringPoint(S, P, t, a, out) {
  const hw = P.halfw(t), tp = P.top(t), bt = P.bottom(t), cy = (tp + bt) / 2, hh = (tp - bt) / 2;
  const c = Math.cos(a), s = Math.sin(a), e = S.box;
  let x = hw * Math.sign(c) * Math.pow(Math.abs(c), e), y = cy + hh * Math.sign(s) * Math.pow(Math.abs(s), e);
  const ax = Math.abs(x) / hw;
  if (!P.open) {
    if (s > 0) {
      const f = P.fenderY(t);
      if (f > tp) y += (f - tp) * smoothstep(0.42, 0.86, ax) * Math.pow(Math.abs(s), 0.5);
    } else if (ax > 0.6) {
      const ar = P.archY(t);
      if (ar > y) y = Math.max(y, Math.min(ar, Math.max(tp, P.fenderY(t)) - 0.035));
    }
  }
  out[0] = x; out[1] = y; out[2] = P.zOf(t);
  return out;
}

function buildBodyGeometry(S, P, lod) {
  const st = stations(S, P, lod === 0 ? 30 : lod === 1 ? 18 : 12), K = lod === 0 ? 30 : lod === 1 ? 20 : 14;
  const pos = [], uv = [], idx = [], p = [0, 0, 0];
  for (let i = 0; i < st.length; i++) for (let k = 0; k <= K; k++) {
    ringPoint(S, P, st[i], -Math.PI / 2 + (k / K) * Math.PI * 2, p);
    pos.push(p[0], p[1], p[2]);
    uv.push(st[i], k / K);
  }
  const row = K + 1;
  for (let i = 0; i < st.length - 1; i++) for (let k = 0; k < K; k++) {
    const a = i * row + k, b = a + 1, c = a + row, d = c + 1;
    idx.push(a, c, b, b, c, d);
  }
  // fermetures avant / arrière
  for (const [i, flip] of [[0, false], [st.length - 1, true]]) {
    const t = st[i], ci = pos.length / 3;
    pos.push(0, (P.top(t) + P.bottom(t)) / 2, P.zOf(t) + (flip ? -0.02 : 0.02));
    uv.push(t, 0.5);
    for (let k = 0; k < K; k++) {
      const a = i * row + k, b = a + 1;
      if (flip) idx.push(ci, b, a); else idx.push(ci, a, b);
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  g.setIndex(idx);
  g.computeVertexNormals();
  return g;
}

// Pavillon : renvoie { paint, glass } (géométries non indexées)
function buildGreenhouse(S, P, lod) {
  const cab = S.cab, openTop = !!S.open;
  const t0 = cab[0], t1 = openTop ? cab[1] : cab[3];
  const n = lod === 0 ? 16 : 9, ts = [];
  for (let i = 0; i <= n; i++) ts.push(lerp(t0, t1, i / n));
  if (!openTop) ts.push(cab[1], cab[2]);
  ts.sort((a, b) => a - b);
  const prof = [[1, -0.06], [0.955, 0.5], [0.9, 0.86], [0.8, 0.975], [0.5, 1.02], [0, 1.045]]; // [fraction de largeur, fraction de hauteur]
  const ringOf = (t) => {
    const r = P.roof(t), wB = P.halfw(t) * (P.open ? 0.9 : 0.9) - 0.02, wR = wB * (1 - S.tumble), by = P.top(t), z = P.zOf(t);
    const pts = [];
    for (let k = 0; k < 11; k++) {
      const m = k <= 5 ? prof[k] : prof[10 - k], sgn = k < 5 ? 1 : k === 5 ? 0 : -1;
      const ww = m[0] > 0.85 ? lerp(wB, wR, clamp(m[1], 0, 1)) : wR * m[0];
      pts.push([sgn * ww, by + (m[1] < 0 ? m[1] : m[1] * r), z]);
    }
    return pts;
  };
  const paint = [], glass = [], uvP = [], uvG = [];
  const quad = (arr, uvs, a, b, c, d, ta, tb, v) => {
    arr.push(...a, ...b, ...c, ...b, ...d, ...c);
    uvs.push(ta, v, ta, v, tb, v, ta, v, tb, v, tb, v);
  };
  for (let i = 0; i < ts.length - 1; i++) {
    const ta = ts[i], tb = ts[i + 1], A = ringOf(ta), B = ringOf(tb), tm = (ta + tb) / 2;
    if (tb - ta < 1e-4) continue;
    const isRoof = !openTop && tm > cab[1] && tm < cab[2];
    for (let k = 0; k < 10; k++) {
      const corner = k === 2 || k === 7, topS = k >= 3 && k <= 6;
      const toPaint = corner || (topS && isRoof);
      // ordre des sommets pour des normales sortantes (x>0 d'abord, on tourne vers x<0 en passant par le toit)
      quad(toPaint ? paint : glass, toPaint ? uvP : uvG, A[k], B[k], A[k + 1], B[k + 1], ta, tb, 0.5);
    }
  }
  const mk = (arr, uvs) => {
    if (!arr.length) return null;
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(arr, 3));
    g.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
    g.computeVertexNormals();
    return g;
  };
  return { paint: mk(paint, uvP), glass: mk(glass, uvG) };
}

// --------------------------------------------------------------------------------------------
// Roues
// --------------------------------------------------------------------------------------------
export const RIMS = {
  five: { name: 'Cinq branches', spokes: 5, w: 0.2, twist: 0 },
  multi: { name: 'Multibranches', spokes: 10, w: 0.09, twist: 0 },
  split: { name: 'Bâtons doubles', spokes: 6, w: 0.08, twist: 0, double: true },
  turbine: { name: 'Turbine', spokes: 9, w: 0.13, twist: 0.5 },
  dish: { name: 'Déport profond', spokes: 4, w: 0.26, twist: 0, dish: true },
  mesh: { name: 'Nid d\'abeille', spokes: 14, w: 0.05, twist: 0.25 },
  steel: { name: 'Tôle renforcée', spokes: 8, w: 0.3, twist: 0, steel: true },
};
export const TIRES = {
  street: { name: 'Route', side: 0.3, grip: 0, off: 0 },
  sport: { name: 'Sport', side: 0.24, grip: 0.05, off: -0.05 },
  slick: { name: 'Semi-slick', side: 0.2, grip: 0.1, off: -0.15 },
  drift: { name: 'Drift', side: 0.24, grip: -0.04, off: -0.05 },
  offroad: { name: 'Tout-terrain', side: 0.4, grip: -0.04, off: 0.35 },
};
const wheelCache = new Map();
function wheelGeo(wr, ww, rim, tire, lod) {
  const key = [wr.toFixed(3), ww.toFixed(3), rim, tire, lod].join('|');
  if (wheelCache.has(key)) return wheelCache.get(key);
  const R = RIMS[rim] || RIMS.five, T = TIRES[tire] || TIRES.street;
  const rr = wr * (1 - T.side), hw = ww / 2, seg = lod === 0 ? 22 : 12;
  // pneu : profil tourné autour de l'axe x
  const pts = [];
  const add = (r, x) => pts.push(new THREE.Vector2(r, x));
  add(rr, -hw * 0.92); add(wr * 0.94, -hw); add(wr * 0.995, -hw * 0.78); add(wr, -hw * 0.5); add(wr, hw * 0.5); add(wr * 0.995, hw * 0.78); add(wr * 0.94, hw); add(rr, hw * 0.92);
  const tireG = new THREE.LatheGeometry(pts, seg).toNonIndexed();
  tireG.rotateZ(-Math.PI / 2);
  const tires = [tireG];
  if (tire === 'offroad' && lod === 0) for (let i = 0; i < 14; i++) {
    const b = new THREE.BoxGeometry(ww * 0.98, 0.035, 0.05).toNonIndexed();
    b.translate(0, wr + 0.008, 0); b.rotateX((i / 14) * Math.PI * 2);
    tires.push(b);
  }
  const rims = [];
  const face = hw * (R.dish ? 0.25 : 0.72);
  const barrel = new THREE.CylinderGeometry(rr, rr, ww * 0.86, seg, 1, true).toNonIndexed();
  barrel.rotateZ(Math.PI / 2); rims.push(barrel);
  const lip = new THREE.TorusGeometry(rr * 0.97, 0.014, 5, seg).toNonIndexed();
  lip.rotateY(Math.PI / 2); lip.translate(hw * 0.9, 0, 0); rims.push(lip);
  const hub = new THREE.CylinderGeometry(rr * 0.2, rr * 0.24, 0.05, 10).toNonIndexed();
  hub.rotateZ(-Math.PI / 2); hub.translate(face + 0.01, 0, 0); rims.push(hub);
  if (R.steel) {
    const d = new THREE.CylinderGeometry(rr * 0.96, rr * 0.96, 0.03, seg).toNonIndexed();
    d.rotateZ(-Math.PI / 2); d.translate(face - 0.01, 0, 0); rims.push(d);
  } else {
    const ns = lod === 0 ? R.spokes : Math.min(R.spokes, 5);
    for (let i = 0; i < ns; i++) for (const off of R.double ? [-0.09, 0.09] : [0]) {
      const sp = new THREE.BoxGeometry(0.045, rr * 0.86, rr * R.w * (lod === 0 ? 1.15 : 1.5)).toNonIndexed();
      sp.translate(0, rr * 0.52, 0);
      sp.rotateY(R.twist * 0.5);
      sp.rotateX((i / ns) * Math.PI * 2 + off + R.twist * 0.4);
      sp.translate(face - 0.01, 0, 0);
      rims.push(sp);
    }
  }
  const back = new THREE.CylinderGeometry(rr * 0.98, rr * 0.98, 0.02, seg).toNonIndexed();
  back.rotateZ(-Math.PI / 2); back.translate(-hw * 0.4, 0, 0);
  const disc = new THREE.CylinderGeometry(rr * 0.66, rr * 0.66, 0.025, seg).toNonIndexed();
  disc.rotateZ(-Math.PI / 2); disc.translate(hw * 0.25, 0, 0);
  const out = { tire: mergeGeometries(tires), rim: mergeGeometries(rims), back: mergeGeometries([back]), disc, rr };
  wheelCache.set(key, out);
  return out;
}

// --------------------------------------------------------------------------------------------
// Livrées (texture peinte sur la carrosserie : u = longueur, v = tour de section, 0.5 = axe du capot/toit)
// --------------------------------------------------------------------------------------------
export const LIVERIES = {
  none: { name: 'Aucune' },
  stripes: { name: 'Bandes course' },
  single: { name: 'Bande décalée' },
  side: { name: 'Filets latéraux' },
  twotone: { name: 'Bicolore' },
  halves: { name: 'Avant / arrière' },
  checker: { name: 'Damier' },
  bolt: { name: 'Éclairs' },
  waves: { name: 'Vagues' },
  camo: { name: 'Camouflage' },
  sunset: { name: 'Dégradé Sunset' },
  superessence: { name: 'Superessence Racing' },
  police: { name: 'VCPD', hidden: true },
};
export const STICKERS = { none: 'Aucun', star: 'Étoile', bolt: 'Éclair', number: 'Numéro', wings: 'Ailes', crown: 'Couronne', se: 'Logo SE' };
const hex = (c) => '#' + new THREE.Color(c).getHexString();
function drawLivery(cfg, baseColor) {
  const cv = document.createElement('canvas');
  cv.width = 512; cv.height = 256;
  const g = cv.getContext('2d'), Wd = 512, Hd = 256;
  const base = hex(baseColor), c2 = hex(cfg.liveryColor ?? 0xffffff), c3 = hex(cfg.liveryColor2 ?? 0x111111);
  g.fillStyle = base; g.fillRect(0, 0, Wd, Hd);
  const top = Hd * 0.5; // axe supérieur ; flancs à 0.25 (droite) et 0.75 (gauche) environ ; dessous à 0 et 1
  const mirror = (fn) => { fn(1); g.save(); g.translate(0, Hd); g.scale(1, -1); fn(-1); g.restore(); };
  switch (cfg.livery) {
    case 'stripes': g.fillStyle = c2; g.fillRect(0, top - 15, Wd, 11); g.fillRect(0, top + 4, Wd, 11); break;
    case 'single': g.fillStyle = c2; g.fillRect(0, top + 12, Wd, 16); g.fillStyle = c3; g.fillRect(0, top + 30, Wd, 4); break;
    case 'side': mirror(() => { g.fillStyle = c2; g.fillRect(40, 44, 430, 9); g.fillStyle = c3; g.fillRect(70, 56, 400, 4); }); break;
    case 'twotone': g.fillStyle = c2; g.fillRect(0, top - 62, Wd, 124); break;
    case 'halves': { const gr = g.createLinearGradient(200, 0, 300, 0); gr.addColorStop(0, base); gr.addColorStop(0.5, c2); gr.addColorStop(1, c2); g.fillStyle = gr; g.fillRect(200, 0, 312, Hd); break; }
    case 'checker': mirror(() => { for (let i = 0; i < 9; i++) for (let j = 0; j < 3; j++) { g.fillStyle = (i + j) % 2 ? c2 : c3; g.fillRect(250 + i * 22, 30 + j * 14, 22, 14); } }); break;
    case 'bolt': mirror(() => { g.fillStyle = c2; g.beginPath(); g.moveTo(90, 60); g.lineTo(260, 36); g.lineTo(240, 52); g.lineTo(430, 34); g.lineTo(250, 66); g.lineTo(268, 50); g.closePath(); g.fill(); }); break;
    case 'waves': mirror(() => { for (let k = 0; k < 3; k++) { g.strokeStyle = k % 2 ? c3 : c2; g.lineWidth = 7 - k * 1.5; g.beginPath(); for (let x = 30; x < 490; x += 6) g.lineTo(x, 44 + k * 10 + Math.sin(x / 34 + k) * 9); g.stroke(); } }); break;
    case 'camo': { let s = 7; const r = () => ((s = (s * 16807) % 2147483647) / 2147483647); for (let i = 0; i < 70; i++) { g.fillStyle = [c2, c3, base][i % 3]; g.beginPath(); g.ellipse(r() * Wd, r() * Hd, 18 + r() * 30, 10 + r() * 16, r() * 3, 0, 7); g.fill(); } break; }
    case 'sunset': { const gr = g.createLinearGradient(0, 0, Wd, 0); gr.addColorStop(0, c2); gr.addColorStop(0.5, base); gr.addColorStop(1, c3); g.fillStyle = gr; g.fillRect(0, 0, Wd, Hd); break; }
    case 'superessence': g.fillStyle = '#0b0f1a'; g.fillRect(0, top - 40, Wd, 80); g.fillStyle = '#ff7a00'; g.fillRect(0, top - 46, Wd, 6); g.fillRect(0, top + 40, Wd, 6); g.fillStyle = '#00e0ff'; g.fillRect(0, top - 6, Wd, 12);
      mirror(() => { g.fillStyle = '#ff7a00'; g.fillRect(60, 40, 400, 6); g.fillStyle = '#00e0ff'; g.fillRect(60, 50, 300, 4); }); break;
    case 'police': g.fillStyle = '#10131a'; g.fillRect(0, 0, 150, Hd); g.fillRect(360, 0, 152, Hd); mirror(() => { g.fillStyle = '#1e5bff'; g.fillRect(150, 46, 210, 8); }); break;
  }
  // autocollant (capot + portières)
  const st = cfg.sticker;
  if (st && st !== 'none') {
    const col = hex(cfg.stickerColor ?? 0xffffff);
    const draw = (x, y, sc) => {
      g.save(); g.translate(x, y); g.scale(sc, sc); g.fillStyle = col; g.strokeStyle = col; g.lineWidth = 3;
      if (st === 'star') { g.beginPath(); for (let i = 0; i < 10; i++) { const r = i % 2 ? 6 : 14, a = (i / 10) * Math.PI * 2; g.lineTo(Math.cos(a) * r * 1.6, Math.sin(a) * r); } g.closePath(); g.fill(); }
      else if (st === 'bolt') { g.beginPath(); g.moveTo(-20, -4); g.lineTo(4, -12); g.lineTo(-2, -2); g.lineTo(20, 4); g.lineTo(-4, 12); g.lineTo(2, 2); g.closePath(); g.fill(); }
      else if (st === 'number') { g.beginPath(); g.ellipse(0, 0, 22, 14, 0, 0, 7); g.fill(); g.fillStyle = '#111'; g.font = 'bold 20px Arial'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText(String(cfg.number ?? 7), 0, 1); }
      else if (st === 'wings') { for (const s of [1, -1]) { g.beginPath(); g.moveTo(0, 0); g.quadraticCurveTo(14 * s, -14, 30 * s, -10); g.quadraticCurveTo(18 * s, -2, 26 * s, 2); g.quadraticCurveTo(12 * s, 4, 0, 6); g.fill(); } }
      else if (st === 'crown') { g.beginPath(); g.moveTo(-18, 10); g.lineTo(-20, -8); g.lineTo(-9, 2); g.lineTo(0, -12); g.lineTo(9, 2); g.lineTo(20, -8); g.lineTo(18, 10); g.closePath(); g.fill(); }
      else if (st === 'se') { g.font = 'italic 900 22px Arial'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText('SE', 0, 0); g.fillRect(-20, 11, 40, 3); }
      g.restore();
    };
    draw(70, top, 1.1);
    draw(270, 50, 1); draw(270, Hd - 50, 1);
  }
  const tex = new THREE.CanvasTexture(cv);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 4;
  return tex;
}

export const DEFAULT_CONFIG = {
  finish: 'gloss', color: null, livery: 'none', liveryColor: 0xffffff, liveryColor2: 0x111111, sticker: 'none', stickerColor: 0xffffff, number: 7,
  rim: 'five', rimColor: 0xcfd3da, tire: 'street', caliper: 0xd11a1a, spoiler: null, bumper: 'stock', hood: null, skirts: 'none', diffuser: 'none', exhaust: 'dual',
  neon: 'none', tint: 0.5, headColor: 0xfff4d6, wheel: 'sport', seats: 'stock', seatColor: 0x202226, dash: 'stock', ambient: 0x00c8ff,
};
export const NEONS = { none: null, cyan: 0x00e5ff, pink: 0xff2bd6, lime: 0x9dff00, orange: 0xff7a00, purple: 0x8a4dff, red: 0xff2030, white: 0xdfeaff, gold: 0xffc400 };

// --------------------------------------------------------------------------------------------
// Assemblage
// --------------------------------------------------------------------------------------------
export function buildCar(spec, userCfg = {}, opts = {}) {
  const lod = opts.lod ?? 0;
  const S = spec.shape, st = spec.style, P = makeProfile(S);
  const cfg = { ...DEFAULT_CONFIG, ...userCfg };
  const color = cfg.color ?? st.color;
  const spoiler = cfg.spoiler ?? st.spoiler ?? 'none', hood = cfg.hood ?? st.hood ?? 'stock';
  const bumper = cfg.bumper === 'stock' && st.bumper ? st.bumper : cfg.bumper;
  const sm = sharedMats();
  const root = new THREE.Group(), chassis = new THREE.Group();
  root.add(chassis);
  const B = {}; // lots de géométries par matériau
  const put = (k, g, x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0) => {
    if (g.index) g = g.toNonIndexed();
    if (rx || ry || rz) g.applyMatrix4(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(rx, ry, rz)));
    g.translate(x, y, z);
    (B[k] || (B[k] = [])).push(g);
    return g;
  };
  const box = (w, h, d) => new THREE.BoxGeometry(w, h, d);
  const cyl = (r1, r2, h, s = 10) => new THREE.CylinderGeometry(r1, r2, h, s);
  const sym = (fn) => { fn(1); fn(-1); };
  const zN = (t) => P.zOf(t);
  const p3 = [0, 0, 0];
  const surf = (t, a) => ringPoint(S, P, t, a, p3);

  // ---- caisse + pavillon ----
  const bodyGeo = buildBodyGeometry(S, P, lod);
  const gh = buildGreenhouse(S, P, lod);
  const fin = PAINT_FINISH[cfg.finish] || PAINT_FINISH.gloss;
  let paintMat;
  const livCfg = st.police ? { ...cfg, livery: 'police' } : cfg;
  const needTex = lod < 2 && (livCfg.livery !== 'none' || (livCfg.sticker && livCfg.sticker !== 'none'));
  const tex = needTex && typeof document !== 'undefined' ? drawLivery(livCfg, color) : null;
  if (lod === 0 && opts.physical !== false) {
    paintMat = new THREE.MeshPhysicalMaterial({ color: tex ? 0xffffff : color, map: tex, metalness: fin.metalness, roughness: fin.roughness, clearcoat: fin.clearcoat, clearcoatRoughness: fin.ccr, envMap, envMapIntensity: 1.25 });
    if (fin.irid) { paintMat.iridescence = 0.9; paintMat.iridescenceIOR = 1.6; }
  } else {
    paintMat = new THREE.MeshStandardMaterial({ color: tex ? 0xffffff : color, map: tex, metalness: Math.min(0.6, fin.metalness), roughness: Math.max(0.3, fin.roughness), envMap, envMapIntensity: 1 });
  }
  const glassMat = new THREE.MeshStandardMaterial({ color: 0x0b1420, metalness: 0.95, roughness: 0.06, transparent: true, opacity: lod === 0 ? 0.35 + 0.5 * cfg.tint : 0.9, envMap, envMapIntensity: 1.6 });
  const bodyMesh = new THREE.Mesh(bodyGeo, paintMat);
  bodyMesh.castShadow = lod < 2; bodyMesh.receiveShadow = false;
  chassis.add(bodyMesh);
  if (gh.paint) put('paint', gh.paint);
  if (gh.glass) put('glass', gh.glass);

  const belt = S.belt, hwMid = P.halfw(0.5);
  const cabA = S.cab[0], cabD = S.cab[3], cabLen = (cabD - cabA) * S.L;

  // ---- habitacle ----
  const interiorMat = new THREE.MeshStandardMaterial({ color: cfg.seatColor, roughness: 0.8 });
  const steer = new THREE.Group();
  let cockpit = new THREE.Vector3(0.36, belt + 0.3, zN(lerp(S.cab[1], S.cab[2], 0.45)));
  if (lod === 0) {
    const zFront = zN(cabA) - 0.25, zBack = zN(S.open ? Math.min(cabD, S.cab[1] + 0.2) : cabD) + 0.2;
    const tubL = zFront - zBack, tubZ = (zFront + zBack) / 2, tubW = hwMid * 1.62;
    put('under', box(tubW, 0.02, tubL), 0, belt + 0.012, tubZ);
    // planche de bord
    const dashZ = zN(lerp(cabA, S.cab[1], 0.55));
    put(cfg.dash === 'carbon' ? 'carbon' : cfg.dash === 'wood' ? 'wood' : 'dark', box(tubW * 0.98, 0.14, 0.3), 0, belt + 0.06, dashZ);
    put('ambient', box(tubW * 0.9, 0.012, 0.012), 0, belt + 0.135, dashZ - 0.15);
    put('screen', box(0.3, 0.1, 0.02), 0, belt + 0.15, dashZ - 0.14, -0.3);
    // sièges
    const rows = S.wb > 2.6 && !S.open && cabLen > 1.9 ? 2 : 1;
    const seatZ0 = dashZ - 0.78;
    for (let r = 0; r < rows; r++) sym((s) => {
      const sx = s * hwMid * 0.44, sz = seatZ0 - r * 0.85, bucket = cfg.seats === 'bucket', lux = cfg.seats === 'luxury';
      const sw = bucket ? 0.42 : lux ? 0.52 : 0.47, sh = Math.min(0.5, Math.max(0.24, P.roofH - 0.2));
      put('seat', box(sw, sh, 0.13), sx, belt + sh / 2 - 0.1, sz - 0.2, -0.16);
      put('seat', box(sw * (bucket ? 0.7 : 0.55), 0.15, 0.1), sx, belt + sh - 0.06, sz - 0.25, -0.16);
      if (bucket) sym((q) => put('seat', box(0.06, sh * 0.8, 0.2), sx + q * sw * 0.5, belt + sh / 2 - 0.1, sz - 0.14, -0.16));
      if (lux) put('ambient', box(sw * 0.9, 0.012, 0.012), sx, belt + sh * 0.6, sz - 0.14);
    });
    cockpit.set(hwMid * 0.44, Math.min(belt + 0.36, S.H - 0.13), seatZ0 - 0.12);
    // volant
    const wr = cfg.wheel === 'race' ? 0.15 : 0.17;
    const swg = [new THREE.TorusGeometry(wr, cfg.wheel === 'classic' ? 0.014 : 0.02, 6, 18).toNonIndexed(), box(wr * 1.9, 0.035, 0.02).toNonIndexed()];
    if (cfg.wheel !== 'race') swg.push(box(0.035, wr, 0.02).translate(0, -wr / 2, 0).toNonIndexed());
    const sw = new THREE.Mesh(mergeGeometries(swg), cfg.wheel === 'classic' ? mat('wood', () => std({ color: 0x6b4226, roughness: 0.45 })) : sm.dark);
    steer.add(sw);
    steer.position.set(hwMid * 0.44, belt + 0.17, dashZ - 0.24);
    steer.rotation.x = -0.35;
    chassis.add(steer);
    if (S.open && !S.cage) sym((s) => put('chrome', new THREE.TorusGeometry(0.16, 0.022, 6, 12, Math.PI), s * hwMid * 0.44, belt + 0.06, seatZ0 - 0.42));
  }
  if (S.cage) {
    const tube = (x1, y1, z1, x2, y2, z2) => {
      const a = new THREE.Vector3(x1, y1, z1), b = new THREE.Vector3(x2, y2, z2), d = b.clone().sub(a);
      const g = cyl(0.03, 0.03, d.length(), 6).toNonIndexed();
      g.applyQuaternion(new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), d.clone().normalize()));
      g.translate((x1 + x2) / 2, (y1 + y2) / 2, (z1 + z2) / 2);
      (B.cage || (B.cage = [])).push(g);
    };
    const z1 = zN(S.cab[0] + 0.02), z2 = zN(S.cab[1]), z3 = zN(S.cab[2]), z4 = zN(S.cab[3] + 0.08), w = hwMid * 0.86, top = S.H;
    sym((s) => { tube(s * w, belt, z1, s * w * 0.9, top, z2); tube(s * w * 0.9, top, z2, s * w * 0.9, top, z3); tube(s * w * 0.9, top, z3, s * w, belt, z4); tube(s * w * 0.9, top, z3, s * w, belt, z3 + 0.1); });
    tube(-w * 0.9, top, z2, w * 0.9, top, z2); tube(-w * 0.9, top, z3, w * 0.9, top, z3); tube(-w * 0.9, top, z2, w * 0.9, top, z3);
  }

  // ---- optiques ----
  const headT = 0.03, hwN = P.halfw(headT), topN = P.top(headT);
  const headY = Math.max(topN - 0.075, P.bottom(headT) + 0.16), zH = zN(headT) - 0.015;
  const fl = P.fenderY(0.06) > P.top(0.06) ? (P.fenderY(0.06) - P.top(0.06)) * 0.5 : 0;
  const HL = (g, x, y, z, rx, ry, rz) => put('head', g, x, y, z, rx, ry, rz);
  const sph = (r) => new THREE.SphereGeometry(r, 10, 8);
  switch (st.head) {
    case 'round': sym((s) => { HL(sph(0.105).scale(1, 1, 0.5), s * hwN * 0.66, headY + fl, zH); put('chrome', new THREE.TorusGeometry(0.108, 0.014, 5, 14), s * hwN * 0.66, headY + fl, zH + 0.01); }); break;
    case 'round2': sym((s) => { for (const k of [0.74, 0.44]) { HL(sph(0.078).scale(1, 1, 0.5), s * hwN * k, headY + fl * (k > 0.5 ? 1 : 0.3), zH + (k > 0.5 ? -0.02 : 0.01)); put('chrome', new THREE.TorusGeometry(0.08, 0.011, 5, 12), s * hwN * k, headY + fl * (k > 0.5 ? 1 : 0.3), zH + (k > 0.5 ? -0.01 : 0.02)); } }); break;
    case 'quad': sym((s) => { for (const k of [0.76, 0.52]) HL(box(0.13, 0.055, 0.05), s * hwN * k, headY + fl, zH, 0, s * 0.25, 0); }); break;
    case 'rect': sym((s) => HL(box(0.27, 0.12, 0.06), s * hwN * 0.66, headY, zH, 0, s * 0.2, 0)); break;
    case 'slim': sym((s) => HL(box(0.36, 0.042, 0.06), s * hwN * 0.62, headY + fl + 0.02, zH - 0.03, 0, s * 0.32, s * 0.12)); break;
    case 'angular': sym((s) => { HL(box(0.3, 0.075, 0.07), s * hwN * 0.64, headY + fl, zH - 0.03, 0, s * 0.36, s * 0.22); HL(box(0.12, 0.03, 0.06), s * hwN * 0.5, headY - 0.07, zH + 0.0, 0, s * 0.2, 0); }); break;
    case 'bar': HL(box(hwN * 1.7, 0.03, 0.05), 0, headY + 0.03, zH + 0.01); sym((s) => HL(box(0.16, 0.05, 0.06), s * hwN * 0.72, headY + fl, zH - 0.03, 0, s * 0.3, 0)); break;
    case 'popup': sym((s) => { put('paint', box(0.3, 0.035, 0.22), s * hwN * 0.62, P.top(0.09) + fl * 0.6 + 0.005, zN(0.09)); HL(box(0.2, 0.035, 0.04), s * hwN * 0.6, P.bottom(headT) + 0.2, zH + 0.02); }); break;
    case 'hidden': sym((s) => HL(sph(0.07).scale(1, 1, 0.4), s * hwN * 0.7, headY - 0.03, zH - 0.0)); break;
  }
  const tailT = 0.975, hwT = P.halfw(tailT), topT = P.top(tailT), tailY = topT - 0.1, zT = zN(tailT) + 0.01;
  const TL = (g, x, y, z, rx, ry, rz) => put('tail', g, x, y, z, rx, ry, rz);
  switch (st.tail) {
    case 'round': sym((s) => TL(sph(0.085).scale(1, 1, 0.45), s * hwT * 0.68, tailY, zT)); break;
    case 'round2': sym((s) => { for (const k of [0.74, 0.48]) TL(sph(0.07).scale(1, 1, 0.45), s * hwT * k, tailY, zT); }); break;
    case 'round4': sym((s) => { for (const k of [0.78, 0.5]) { TL(sph(0.075).scale(1, 1, 0.45), s * hwT * k, tailY, zT); put('dark', new THREE.TorusGeometry(0.078, 0.012, 5, 12), s * hwT * k, tailY, zT - 0.01); } }); break;
    case 'rect': sym((s) => TL(box(0.2, 0.15, 0.05), s * hwT * 0.72, tailY, zT)); break;
    case 'rect3': sym((s) => { for (let k = 0; k < 3; k++) TL(box(0.075, 0.15, 0.05), s * hwT * (0.5 + k * 0.15), tailY, zT); }); break;
    case 'slim': sym((s) => TL(box(0.38, 0.04, 0.05), s * hwT * 0.58, tailY + 0.03, zT, 0, -s * 0.15, 0)); break;
    case 'bar': TL(box(hwT * 1.75, 0.04, 0.05), 0, tailY + 0.02, zT); break;
  }
  sym((s) => put('rev', box(0.07, 0.035, 0.03), s * hwT * 0.3, Math.max(P.bottom(tailT) + 0.14, tailY - 0.2), zT));
  put('plate', box(0.36, 0.1, 0.02), 0, Math.max(P.bottom(tailT) + 0.2, tailY - 0.13), zT + 0.0);

  // ---- calandre et boucliers ----
  const gy = P.bottom(headT) + 0.1, gz = zN(0.012) - 0.0;
  const lowH = Math.max(0.08, Math.min(0.2, headY - gy - 0.1));
  switch (st.grille) {
    case 'slot': put('dark', box(hwN * 1.2, 0.07, 0.06), 0, gy + 0.05, gz); break;
    case 'wide': put('dark', box(hwN * 1.5, lowH, 0.06), 0, gy + lowH / 2 - 0.02, gz); sym((s) => put('dark', box(0.05, lowH * 0.8, 0.07), s * hwN * 0.4, gy + lowH / 2 - 0.02, gz + 0.004)); break;
    case 'smile': put('dark', new THREE.TorusGeometry(hwN * 0.5, 0.035, 5, 14, Math.PI).scale(1, 0.35, 1), 0, gy + 0.11, gz - 0.01, 0, 0, Math.PI); break;
    case 'bars': put('dark', box(hwN * 0.9, 0.22, 0.05), 0, headY - 0.02, gz - 0.01); for (let i = -3; i <= 3; i++) put('paint', box(0.035, 0.22, 0.06), i * hwN * 0.12, headY - 0.02, gz); break;
    case 'full': put('dark', box(hwN * 1.75, 0.17, 0.05), 0, headY - 0.02, gz - 0.02); put('chrome', box(hwN * 1.78, 0.02, 0.06), 0, headY - 0.115, gz - 0.015); break;
    case 'chrome': put('chrome', box(hwN * 0.82, 0.3, 0.05), 0, headY - 0.05, gz - 0.005); put('dark', box(hwN * 0.74, 0.25, 0.06), 0, headY - 0.05, gz); for (let i = -3; i <= 3; i++) put('chrome', box(0.018, 0.25, 0.07), i * hwN * 0.1, headY - 0.05, gz); break;
  }
  const fz = zN(0) , fy = P.bottom(0.04);
  if (bumper === 'sport' || bumper === 'race') {
    put('carbon', box(hwN * 2.0, 0.03, 0.34), 0, fy - 0.01, fz - 0.1);
    if (bumper === 'race') sym((s) => { put('carbon', box(0.22, 0.02, 0.2), s * hwN * 1.02, fy + 0.2, fz - 0.2, 0.25, 0, s * 0.3); put('carbon', box(0.03, 0.12, 0.3), s * hwN * 0.98, fy + 0.05, fz - 0.12); });
  } else if (bumper === 'offroad') {
    put('dark', box(hwN * 1.9, 0.16, 0.14), 0, fy + 0.12, fz + 0.03);
    sym((s) => put('dark', cyl(0.025, 0.025, 0.42, 6), s * hwN * 0.4, fy + 0.36, fz + 0.09));
    put('dark', cyl(0.025, 0.025, hwN * 0.84, 6), 0, fy + 0.56, fz + 0.09, 0, 0, Math.PI / 2);
  }
  // rétroviseurs
  if (lod < 2 && !S.cage) sym((s) => { const t = S.cab[0] + 0.035; put('paint', new THREE.SphereGeometry(0.075, 8, 6).scale(1.15, 0.72, 0.8), s * (P.halfw(t) + 0.03), P.top(t) + 0.035, zN(t)); put('dark', box(0.12, 0.03, 0.05), s * (P.halfw(t) - 0.05), P.top(t) + 0.0, zN(t)); });
  // bas de caisse
  if (cfg.skirts !== 'none') sym((s) => {
    const zA = P.zf - S.wr * 1.25, zB = P.zr + S.wr * 1.25, wide = cfg.skirts === 'wide';
    put(wide ? 'carbon' : 'paint', box(wide ? 0.11 : 0.07, wide ? 0.07 : 0.1, zA - zB), s * (hwMid + (wide ? 0.035 : 0.01)), S.gc + 0.03, (zA + zB) / 2);
  });
  // diffuseur
  const rz0 = zN(1), ry0 = P.bottom(0.97);
  if (cfg.diffuser !== 'none') {
    put('carbon', box(hwT * 1.6, 0.03, 0.4), 0, ry0 - 0.02, rz0 + 0.14, 0.14);
    const nf = cfg.diffuser === 'race' ? 6 : 3;
    for (let i = 0; i < nf; i++) put('carbon', box(0.02, 0.1, 0.36), lerp(-hwT * 0.6, hwT * 0.6, i / (nf - 1)), ry0 + 0.02, rz0 + 0.14, 0.14);
  }
  // échappements
  const exh = [];
  const tip = (x, y, z, r = 0.045) => { put('chrome', new THREE.CylinderGeometry(r, r * 0.9, 0.16, 10, 1, true), x, y, z, Math.PI / 2); put('under', cyl(r * 0.85, r * 0.85, 0.02, 8), x, y, z + 0.05, Math.PI / 2); exh.push(new THREE.Vector3(x, y, z - 0.1)); };
  const ey = ry0 + 0.07, ez = rz0 + 0.02;
  if (spec.stats.eng === 'ev') { /* pas d'échappement */ }
  else if (cfg.exhaust === 'single') tip(-hwT * 0.55, ey, ez);
  else if (cfg.exhaust === 'quad') sym((s) => { tip(s * hwT * 0.6, ey, ez, 0.04); tip(s * hwT * 0.42, ey, ez, 0.04); });
  else if (cfg.exhaust === 'center') { tip(-0.07, ey + 0.05, ez, 0.05); tip(0.07, ey + 0.05, ez, 0.05); }
  else if (cfg.exhaust === 'side') sym((s) => { const z = P.zr + S.wr * 1.5; put('chrome', new THREE.CylinderGeometry(0.045, 0.045, 0.5, 8, 1, true), s * (hwMid + 0.02), S.gc + 0.05, z + 0.25, Math.PI / 2); exh.push(new THREE.Vector3(s * (hwMid + 0.05), S.gc + 0.05, z)); });
  else sym((s) => tip(s * hwT * 0.55, ey, ez));
  // aileron
  const deckT = S.body === 'pickup' ? 0.98 : Math.min(0.955, Math.max(S.cab[3] + 0.04, 0.9)), dz = zN(deckT), dy = P.top(deckT);
  if (spoiler === 'lip') put('paint', box(hwT * 1.7, 0.035, 0.12), 0, dy + 0.025, dz - 0.02, 0.2);
  else if (spoiler === 'duck') put('paint', box(hwT * 1.8, 0.04, 0.26), 0, dy + 0.06, dz - 0.0, 0.42);
  else if (spoiler === 'wing' || spoiler === 'gt' || spoiler === 'mega') {
    const hgt = spoiler === 'wing' ? 0.17 : spoiler === 'gt' ? 0.3 : 0.42, span = spoiler === 'wing' ? hwT * 1.75 : S.W * (spoiler === 'gt' ? 0.98 : 1.06), chord = spoiler === 'mega' ? 0.36 : 0.25;
    put(spoiler === 'wing' ? 'paint' : 'carbon', box(span, 0.028, chord), 0, dy + hgt, dz - 0.04, 0.12);
    if (spoiler === 'mega') put('carbon', box(span * 0.92, 0.02, 0.14), 0, dy + hgt + 0.1, dz - 0.12, 0.3);
    sym((s) => { put('carbon', box(0.025, hgt, 0.12), s * hwT * 0.55, dy + hgt / 2, dz - 0.02); put(spoiler === 'wing' ? 'paint' : 'carbon', box(0.02, 0.13, chord + 0.06), s * span / 2, dy + hgt + 0.02, dz - 0.04); });
  }
  // capot
  const hT = lerp(0.06, S.cab[0], 0.55), hz = zN(hT), hy = P.top(hT);
  if (hood === 'scoop') { put('paint', box(0.4, 0.07, 0.5), 0, hy + 0.02, hz, -0.05); put('dark', box(0.34, 0.045, 0.02), 0, hy + 0.035, hz + 0.25); }
  else if (hood === 'vents') sym((s) => { for (let k = 0; k < 3; k++) put('dark', box(0.22, 0.012, 0.045), s * hwN * 0.42, hy + 0.012 + fl * 0.2, hz + 0.16 - k * 0.1, 0, s * 0.3, 0); })
  else if (hood === 'blower') { put('chrome', box(0.36, 0.2, 0.44), 0, hy + 0.1, hz); put('dark', box(0.3, 0.09, 0.3), 0, hy + 0.24, hz + 0.04); put('dark', box(0.32, 0.06, 0.02), 0, hy + 0.24, hz + 0.2); }
  else if (hood === 'carbon') put('carbon', box(hwN * 1.1, 0.012, (hT - 0.07) * S.L * 1.5), 0, hy + 0.012, hz, -Math.atan2(P.top(S.cab[0]) - P.top(0.07), (S.cab[0] - 0.07) * S.L) * 0.8);
  // accessoires de série
  const roofY = S.H, roofZ = zN(lerp(S.cab[1], S.cab[2], 0.5));
  if (st.roofrack) { sym((s) => put('dark', cyl(0.018, 0.018, (S.cab[2] - S.cab[1]) * S.L * 0.9, 6), s * hwMid * 0.6, roofY + 0.07, roofZ, Math.PI / 2)); for (const k of [-0.3, 0.3]) put('dark', box(hwMid * 1.3, 0.025, 0.04), 0, roofY + 0.08, roofZ + k * (S.cab[2] - S.cab[1]) * S.L); }
  const lbar = [];
  if (st.lightbar && !st.police) { const ly = S.cage ? S.H + 0.06 : S.body === 'pickup' || S.body === 'suv' ? S.H + 0.05 : P.top(0.04) + 0.09, lz = S.cage || S.body === 'pickup' || S.body === 'suv' ? zN(S.cab[1]) : zN(0.03); for (let i = 0; i < 4; i++) { const x = (i - 1.5) * 0.19; put('dark', cyl(0.075, 0.075, 0.06, 10), x, ly, lz, Math.PI / 2); HL(cyl(0.062, 0.062, 0.02, 10), x, ly, lz + 0.035, Math.PI / 2); } void lbar; }
  if (st.mudflaps) sym((s) => { put('mud', box(0.24, 0.2, 0.015), s * (P.track - 0.12), S.wr * 0.75, P.zr - S.wr - 0.06); put('mud', box(0.24, 0.2, 0.015), s * (P.track - 0.12), S.wr * 0.75, P.zf - S.wr - 0.06); });
  if (st.spare) { const g = wheelGeo(S.wr, 0.22, 'steel', 'offroad', 1); put('rubber', g.tire.clone(), 0, belt - 0.05, zN(1) - 0.12, 0, Math.PI / 2); put('dark', g.rim.clone(), 0, belt - 0.05, zN(1) - 0.12, 0, -Math.PI / 2); }
  if (S.body === 'pickup') { const z1 = zN(S.cab[3] + 0.03), z2 = zN(0.975); put('under', box(hwMid * 1.7, 0.02, z1 - z2), 0, P.top(0.8) + 0.012, (z1 + z2) / 2); sym((s) => put('paint', box(0.07, 0.24, z1 - z2), s * hwMid * 0.9, P.top(0.8) + 0.12, (z1 + z2) / 2)); put('paint', box(hwMid * 1.8, 0.24, 0.06), 0, P.top(0.8) + 0.12, z2); put('dark', cyl(0.03, 0.03, hwMid * 1.5, 6), 0, belt + 0.5, z1 - 0.1, 0, 0, Math.PI / 2); sym((s) => put('dark', cyl(0.03, 0.03, 0.5, 6), s * hwMid * 0.75, belt + 0.25, z1 - 0.1)); }
  // gyrophares
  let beacon = null;
  if (st.police) {
    put('dark', box(0.9, 0.05, 0.22), 0, roofY + 0.03, roofZ);
    const bm = [new THREE.MeshBasicMaterial({ color: 0xff2030 }), new THREE.MeshBasicMaterial({ color: 0x2060ff })];
    beacon = bm;
    [1, -1].forEach((s, i) => { const m = new THREE.Mesh(box(0.4, 0.09, 0.2), bm[i]); m.position.set(s * 0.23, roofY + 0.09, roofZ); chassis.add(m); });
  }

  // ---- matériaux des lots ----
  const headMat = new THREE.MeshStandardMaterial({ color: 0xf5f5f0, emissive: cfg.headColor, emissiveIntensity: 0.25, roughness: 0.2 });
  const tailMat = new THREE.MeshStandardMaterial({ color: 0x7a0c10, emissive: 0xff1a22, emissiveIntensity: 0.35, roughness: 0.3 });
  const revMat = new THREE.MeshStandardMaterial({ color: 0xdddddd, emissive: 0xffffff, emissiveIntensity: 0, roughness: 0.3 });
  const ambMat = new THREE.MeshBasicMaterial({ color: cfg.ambient });
  const mats = { paint: paintMat, glass: glassMat, dark: sm.dark, carbon: sm.carbon, chrome: sm.chrome, rubber: sm.rubber, under: sm.under, plate: sm.plate, head: headMat, tail: tailMat, rev: revMat,
    seat: interiorMat, ambient: ambMat, cage: sm.chrome, mud: sm.rubber, wood: mat('wood', () => std({ color: 0x6b4226, roughness: 0.45 })), screen: mat('screen', () => new THREE.MeshBasicMaterial({ color: 0x1fb6ff })) };
  for (const k in B) {
    const m = new THREE.Mesh(mergeGeometries(B[k]), mats[k]);
    m.castShadow = lod === 0 && (k === 'paint' || k === 'carbon');
    if (k === 'glass') m.renderOrder = 2;
    chassis.add(m);
  }

  // ---- roues ----
  const offroadish = spec.cat === 'offroad' || spec.cat === 'rally';
  const tireType = cfg.tire === 'street' && spec.cat === 'offroad' ? 'offroad' : cfg.tire;
  const ww = S.ww || (offroadish ? 0.27 : S.wr * 0.74);
  const wg = wheelGeo(S.wr, ww, st.police ? 'steel' : cfg.rim, tireType, lod === 0 ? 0 : 1);
  const rimMat = new THREE.MeshStandardMaterial({ color: st.police ? 0x22252b : cfg.rimColor, metalness: 0.9, roughness: 0.25, envMap });
  const calMat = lod === 0 ? new THREE.MeshStandardMaterial({ color: cfg.caliper, roughness: 0.4 }) : null;
  const wheels = [];
  const tx = P.track - ww / 2 - 0.005;
  for (const [sx, z, front] of [[1, P.zf, true], [-1, P.zf, true], [1, P.zr, false], [-1, P.zr, false]]) {
    const g = new THREE.Group(), spin = new THREE.Group();
    g.position.set(sx * tx, S.wr, z);
    const flip = sx < 0 ? Math.PI : 0;
    const t = new THREE.Mesh(wg.tire, sm.rubber), r = new THREE.Mesh(wg.rim, rimMat);
    t.rotation.y = flip; r.rotation.y = flip; t.castShadow = lod === 0;
    spin.add(t, r);
    if (lod === 0) {
      const bk = new THREE.Mesh(wg.back, sm.under); bk.rotation.y = flip; spin.add(bk);
      const d = new THREE.Mesh(wg.disc, sm.disc); d.rotation.y = flip; spin.add(d);
      const cal = new THREE.Mesh(box(0.05, wg.rr * 0.55, wg.rr * 0.42), calMat);
      cal.position.set(sx * ww * 0.14, wg.rr * 0.5, -wg.rr * 0.45); g.add(cal);
    }
    g.add(spin);
    root.add(g);
    wheels.push({ g, spin, front, sx, baseY: S.wr, x: sx * tx, z });
  }
  // ---- néon de bas de caisse ----
  let neon = null;
  const neonColor = NEONS[cfg.neon];
  if (neonColor && lod === 0) {
    neon = new THREE.Mesh(new THREE.PlaneGeometry(S.W * 1.9, S.L * 1.25), new THREE.MeshBasicMaterial({ color: neonColor, map: glowTex(), transparent: true, opacity: 0.85, blending: THREE.AdditiveBlending, depthWrite: false }));
    neon.rotation.x = -Math.PI / 2; neon.position.y = 0.04; neon.renderOrder = 1;
    root.add(neon);
    sym((s) => { const tube = new THREE.Mesh(box(0.03, 0.03, S.wb * 0.8), new THREE.MeshBasicMaterial({ color: neonColor })); tube.position.set(s * hwMid * 0.9, S.gc + 0.01, (P.zf + P.zr) / 2); chassis.add(tube); });
  }
  // ombre de contact (blob) pour les niveaux de qualité sans ombres portées
  const blob = new THREE.Mesh(new THREE.PlaneGeometry(S.W * 1.45, S.L * 1.2), mat('blob', () => new THREE.MeshBasicMaterial({ color: 0x000000, map: glowTex(), transparent: true, opacity: 0.55, depthWrite: false })));
  blob.rotation.x = -Math.PI / 2; blob.position.y = 0.03; blob.renderOrder = 1;
  root.add(blob);

  // ---- déformations (carrosserie du joueur) ----
  const rest = lod === 0 ? bodyGeo.attributes.position.array.slice() : null;
  let dents = 0;
  const api = {
    spec, cfg, chassis, wheels, steer, paintMat, glassMat, headMat, tailMat, revMat, neon, blob, beacon, exhausts: exh, cockpit, dims: { L: S.L, W: S.W, H: S.H, wr: S.wr, zf: P.zf, zr: P.zr, belt, track: tx },
    hoodCam: new THREE.Vector3(0, P.top(S.cab[0] * 0.7) + 0.32, zN(S.cab[0] * 0.75)),
    setLights(on, brake, reverse) {
      headMat.emissiveIntensity = on ? 2.4 : 0.25;
      tailMat.emissiveIntensity = brake ? 3.2 : on ? 1.1 : 0.35;
      revMat.emissiveIntensity = reverse ? 2 : 0;
    },
    dent(lx, ly, lz, strength) {
      if (!rest) return;
      const a = bodyGeo.attributes.position.array, R = 0.75, depth = Math.min(0.2, strength);
      for (let i = 0; i < a.length; i += 3) {
        const dx = a[i] - lx, dy = a[i + 1] - ly, dz2 = a[i + 2] - lz, d = Math.sqrt(dx * dx + dy * dy + dz2 * dz2);
        if (d > R) continue;
        const f = depth * Math.pow(1 - d / R, 2), n = 0.6 + 0.4 * Math.sin(i * 12.9898);
        // enfoncement vers l'axe de la voiture, limité par rapport à la forme d'origine
        const ox = rest[i], oz = rest[i + 2];
        a[i] = clamp(a[i] - Math.sign(lx || 0.001) * f * n * (Math.abs(lx) > Math.abs(lz) * 0.3 ? 1 : 0.2), ox - 0.22, ox + 0.22);
        a[i + 2] = clamp(a[i + 2] - Math.sign(lz || 0.001) * f * n * (Math.abs(lz) > S.L * 0.3 ? 1 : 0.15), oz - 0.25, oz + 0.25);
      }
      bodyGeo.attributes.position.needsUpdate = true;
      bodyGeo.computeVertexNormals();
      dents++;
    },
    repair() {
      if (!rest || !dents) return;
      bodyGeo.attributes.position.array.set(rest);
      bodyGeo.attributes.position.needsUpdate = true;
      bodyGeo.computeVertexNormals();
      dents = 0;
    },
    get dents() { return dents; },
    dispose() {
      bodyGeo.dispose();
      if (tex) tex.dispose();
      root.traverse((o) => { if (o.isMesh && o.geometry && !o.geometry.userData.shared && o.geometry !== wg.tire && o.geometry !== wg.rim && o.geometry !== wg.back && o.geometry !== wg.disc) o.geometry.dispose(); });
      for (const m of [paintMat, glassMat, headMat, tailMat, revMat, rimMat, interiorMat, ambMat]) m.dispose();
    },
  };
  root.userData.car = api;
  return root;
}

let _glow = null;
export function glowTex() {
  if (_glow) return _glow;
  const cv = document.createElement('canvas');
  cv.width = cv.height = 64;
  const g = cv.getContext('2d'), gr = g.createRadialGradient(32, 32, 2, 32, 32, 32);
  gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(0.45, 'rgba(255,255,255,0.55)'); gr.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = gr; g.fillRect(0, 0, 64, 64);
  _glow = new THREE.CanvasTexture(cv);
  return _glow;
}

// --------------------------------------------------------------------------------------------
// Véhicules utilitaires de la circulation : camion, bus, utilitaire, moto
// --------------------------------------------------------------------------------------------
const rbox = (w, h, d, r = 0.12) => {
  // boîte aux arêtes adoucies (extrusion d'un rectangle arrondi)
  const s = new THREE.Shape(), hw = w / 2, hh = h / 2;
  r = Math.min(r, hw, hh);
  s.moveTo(-hw + r, -hh); s.lineTo(hw - r, -hh); s.quadraticCurveTo(hw, -hh, hw, -hh + r); s.lineTo(hw, hh - r); s.quadraticCurveTo(hw, hh, hw - r, hh);
  s.lineTo(-hw + r, hh); s.quadraticCurveTo(-hw, hh, -hw, hh - r); s.lineTo(-hw, -hh + r); s.quadraticCurveTo(-hw, -hh, -hw + r, -hh);
  const g = new THREE.ExtrudeGeometry(s, { depth: d, bevelEnabled: false, curveSegments: 3 });
  g.translate(0, 0, -d / 2);
  return g.toNonIndexed();
};
export const UTILITY = {
  truck: { L: 8.6, W: 2.5, H: 3.5, wr: 0.5, mass: 9000 },
  bus: { L: 11, W: 2.55, H: 3.1, wr: 0.5, mass: 11000 },
  van: { L: 5.3, W: 2.0, H: 2.3, wr: 0.36, mass: 2300 },
  moto: { L: 2.1, W: 0.7, H: 1.5, wr: 0.31, mass: 260 },
};
export function buildUtility(kind, color, seed = 0) {
  const U = UTILITY[kind], sm = sharedMats(), root = new THREE.Group(), B = {};
  const put = (k, g, x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0) => { if (g.index) g = g.toNonIndexed(); if (rx || ry || rz) g.applyMatrix4(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(rx, ry, rz))); g.translate(x, y, z); (B[k] || (B[k] = [])).push(g); };
  const box = (w, h, d) => new THREE.BoxGeometry(w, h, d);
  const wheel = (x, z, wr, ww = 0.3) => { const g = wheelGeo(wr, ww, 'steel', 'street', 1); put('rubber', g.tire.clone(), x, wr, z); put('rim', g.rim.clone(), x, wr, z, 0, x < 0 ? Math.PI : 0); };
  const { L, W, H, wr } = U;
  const body = new THREE.MeshStandardMaterial({ color, roughness: 0.45, metalness: 0.3, envMap });
  if (kind === 'truck') {
    put('body', rbox(W, 2.3, 2.2, 0.3), 0, 1.75, L / 2 - 1.1);
    put('glass', box(W * 0.9, 0.8, 0.05), 0, 2.25, L / 2 + 0.01);
    put('trailer', rbox(W, 2.7, L - 2.6, 0.08), 0, 2.15, -1.2);
    put('dark', box(W * 0.8, 0.4, L - 1), 0, 0.7, -0.3);
    put('head', box(0.3, 0.14, 0.05), 0.85, 0.95, L / 2); put('head', box(0.3, 0.14, 0.05), -0.85, 0.95, L / 2);
    put('tail', box(0.3, 0.14, 0.05), 0.9, 1.0, -L / 2 + 0.1); put('tail', box(0.3, 0.14, 0.05), -0.9, 1.0, -L / 2 + 0.1);
    for (const z of [L / 2 - 1.3, -L / 2 + 1.2, -L / 2 + 2.4]) for (const s of [1, -1]) wheel(s * (W / 2 - 0.17), z, wr, 0.32);
  } else if (kind === 'bus') {
    put('body', rbox(W, H - 0.45, L, 0.28), 0, H / 2 + 0.22, 0);
    put('glass', box(W + 0.02, 0.85, L * 0.86), 0, H * 0.66, 0);
    put('glass', box(W * 0.9, 1.1, 0.05), 0, H * 0.6, L / 2 + 0.01);
    put('stripe', box(W + 0.03, 0.22, L * 0.98), 0, 1.0, 0);
    put('head', box(0.3, 0.14, 0.05), 0.9, 0.85, L / 2); put('head', box(0.3, 0.14, 0.05), -0.9, 0.85, L / 2);
    put('tail', box(0.2, 0.3, 0.05), 1.0, 1.2, -L / 2); put('tail', box(0.2, 0.3, 0.05), -1.0, 1.2, -L / 2);
    for (const z of [L / 2 - 2.2, -L / 2 + 2.6]) for (const s of [1, -1]) wheel(s * (W / 2 - 0.17), z, wr, 0.32);
  } else if (kind === 'van') {
    put('body', rbox(W, H - 0.5, L - 1.1, 0.22), 0, H / 2 + 0.22, -0.55);
    put('body', rbox(W, 1.0, 1.3, 0.3), 0, 0.92, L / 2 - 0.65);
    put('glass', box(W * 0.88, 0.7, 0.05), 0, 1.75, L / 2 - 1.08, -0.35);
    put('glass', box(W + 0.02, 0.5, 0.9), 0, 1.75, L / 2 - 1.75);
    put('head', box(0.26, 0.14, 0.05), 0.7, 0.95, L / 2); put('head', box(0.26, 0.14, 0.05), -0.7, 0.95, L / 2);
    put('tail', box(0.12, 0.4, 0.05), 0.85, 1.3, -L / 2); put('tail', box(0.12, 0.4, 0.05), -0.85, 1.3, -L / 2);
    for (const z of [L / 2 - 1.0, -L / 2 + 1.1]) for (const s of [1, -1]) wheel(s * (W / 2 - 0.13), z, wr, 0.24);
  } else {
    // moto + pilote
    const g1 = wheelGeo(wr, 0.13, 'five', 'street', 1);
    for (const z of [0.72, -0.68]) { put('rubber', g1.tire.clone(), 0, wr, z); put('rim', g1.rim.clone(), 0, wr, z); }
    put('body', rbox(0.3, 0.3, 0.75, 0.1), 0, 0.72, 0.1);
    put('dark', box(0.26, 0.12, 0.6), 0, 0.82, -0.42);
    put('dark', new THREE.CylinderGeometry(0.03, 0.03, 0.7, 6), 0, 0.75, 0.62, -0.45);
    put('dark', new THREE.CylinderGeometry(0.02, 0.02, 0.62, 6), 0, 1.06, 0.48, 0, 0, Math.PI / 2);
    put('head', new THREE.SphereGeometry(0.09, 8, 6), 0, 0.95, 0.74);
    put('tail', box(0.12, 0.05, 0.04), 0, 0.84, -0.74);
    put('rider', rbox(0.4, 0.55, 0.26, 0.1), 0, 1.22, -0.1, 0.5);
    put('helmet', new THREE.SphereGeometry(0.15, 10, 8), 0, 1.6, 0.14);
    put('rider', box(0.12, 0.5, 0.12), 0.16, 0.86, -0.12, 0.6); put('rider', box(0.12, 0.5, 0.12), -0.16, 0.86, -0.12, 0.6);
    put('rider', box(0.09, 0.09, 0.5), 0.2, 1.22, 0.24, 0.3); put('rider', box(0.09, 0.09, 0.5), -0.2, 1.22, 0.24, 0.3);
  }
  const headMat = new THREE.MeshStandardMaterial({ color: 0xf5f5f0, emissive: 0xfff4d6, emissiveIntensity: 0.25 });
  const tailMat = new THREE.MeshStandardMaterial({ color: 0x7a0c10, emissive: 0xff1a22, emissiveIntensity: 0.35 });
  const r = (seed * 9301 + 49297) % 233280 / 233280;
  const mats = { body, trailer: mat('trailer' + Math.floor(r * 4), () => std({ color: [0xe9edf2, 0xf2c14e, 0x4f9dde, 0xd8dde3][Math.floor(r * 4)], roughness: 0.6 })), glass: mat('uglass', () => std({ color: 0x0b1420, metalness: 0.9, roughness: 0.1 })), dark: sm.dark, rubber: sm.rubber, rim: mat('urim', () => std({ color: 0x9aa0a8, metalness: 0.8, roughness: 0.4 })),
    head: headMat, tail: tailMat, stripe: mat('ustripe', () => std({ color: 0xff7a00, roughness: 0.5 })), rider: mat('rider', () => std({ color: 0x20242c, roughness: 0.8 })), helmet: mat('helmet' + Math.floor(r * 3), () => std({ color: [0xff3b30, 0xf5f5f5, 0x00c2ff][Math.floor(r * 3)], roughness: 0.25, metalness: 0.3 })) };
  for (const k in B) { const m = new THREE.Mesh(mergeGeometries(B[k]), mats[k]); m.castShadow = k === 'body' || k === 'trailer'; root.add(m); }
  const blob = new THREE.Mesh(new THREE.PlaneGeometry(W * 1.45, L * 1.15), mat('blob', () => new THREE.MeshBasicMaterial({ color: 0x000000, map: glowTex(), transparent: true, opacity: 0.55, depthWrite: false })));
  blob.rotation.x = -Math.PI / 2; blob.position.y = 0.03; root.add(blob);
  root.userData.car = { spec: { shape: U, stats: { mass: U.mass } }, dims: { L, W, H, wr }, wheels: [], headMat, tailMat, blob,
    setLights(on, brake) { headMat.emissiveIntensity = on ? 2.2 : 0.25; tailMat.emissiveIntensity = brake ? 3 : on ? 1.1 : 0.35; }, dispose() { body.dispose(); headMat.dispose(); tailMat.dispose(); } };
  return root;
}
