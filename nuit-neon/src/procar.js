// Voitures dessinées par le jeu : carrosserie galbée avec passages de roues, ailes bombées, jantes détaillées,
// étriers de frein, feux à LED épousant la caisse, bandes de course et néon sous le châssis.
import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

const lerp = (a, b, t) => a + (b - a) * t;
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
// table t -> valeur, interpolée en douceur
function table(ts, vs) {
  return (t) => {
    let i = 0; while (i < ts.length - 2 && t > ts[i + 1]) i++;
    const u = clamp((t - ts[i]) / (ts[i + 1] - ts[i]), 0, 1);
    const p0 = vs[Math.max(0, i - 1)], p1 = vs[i], p2 = vs[i + 1], p3 = vs[Math.min(vs.length - 1, i + 2)];
    return 0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (-p0 + 3 * p1 - 3 * p2 + p3) * u * u * u);
  };
}
const flat = (g) => { const n = g.index ? g.toNonIndexed() : g; n.deleteAttribute('uv'); if (!n.attributes.normal) n.computeVertexNormals(); return n; };
function box(w, h, d, x, y, z, rx = 0, ry = 0, rz = 0) { const g = new THREE.BoxGeometry(w, h, d); if (rx) g.rotateX(rx); if (ry) g.rotateY(ry); if (rz) g.rotateZ(rz); g.translate(x, y, z); return g; }
function tint(g, r, gg, b) { const n = g.attributes.position.count, c = new Float32Array(n * 3); for (let i = 0; i < n; i++) { c[i * 3] = r; c[i * 3 + 1] = gg; c[i * 3 + 2] = b; } g.setAttribute('color', new THREE.BufferAttribute(c, 3)); return g; }

let glowTex = null, shadowTex = null;
function radialTex(stops) {
  const c = document.createElement('canvas'); c.width = c.height = 128; const g = c.getContext('2d');
  const gr = g.createRadialGradient(64, 64, 4, 64, 64, 64); stops.forEach(([o, col]) => gr.addColorStop(o, col)); g.fillStyle = gr; g.fillRect(0, 0, 128, 128);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
}

// ---------- roue : pneu à flancs arrondis, jante à branches, disque ----------
const wheelCache = new Map();
function wheelGeos(r, tw, style, side) {
  const key = [r, tw, style, side].join('|'); if (wheelCache.has(key)) return wheelCache.get(key);
  const ri = r * 0.7, o = side * (tw / 2);                       // o : face extérieure
  const prof = [[ri, -tw / 2], [r - 0.04, -tw / 2], [r - 0.008, -tw / 2 + 0.03], [r, -tw / 2 + 0.07], [r, tw / 2 - 0.07], [r - 0.008, tw / 2 - 0.03], [r - 0.04, tw / 2], [ri, tw / 2]];
  const tire = new THREE.LatheGeometry(prof.map(p => new THREE.Vector2(p[0], p[1])), 30); tire.rotateZ(Math.PI / 2);
  const m = [];                                                    // parties métalliques, couleur par sommet (1 = teinte de la jante)
  const add = (g, k) => m.push(tint(flat(g), k, k, k));
  const barrel = new THREE.CylinderGeometry(ri, ri * 0.96, tw * 0.94, 26, 1, true); barrel.rotateZ(Math.PI / 2); add(barrel, 0.22);
  const lip = new THREE.TorusGeometry(ri - 0.004, 0.013, 6, 30); lip.rotateY(Math.PI / 2); lip.translate(o - side * 0.012, 0, 0); add(lip, 1.25);
  const face = o - side * 0.05;
  const spoke = (ang, w, tilt = 0) => { const g = new THREE.BoxGeometry(0.035, ri - 0.03, w); g.translate(0, (ri + 0.03) / 2, 0); g.rotateX(ang + tilt); g.translate(face, 0, 0); add(g, 1); };
  if (style === 'turbine') for (let i = 0; i < 14; i++) spoke(i / 14 * Math.PI * 2, 0.03, 0.12);
  else if (style === 'etoile') for (let i = 0; i < 5; i++) spoke(i / 5 * Math.PI * 2, 0.085);
  else if (style === 'six') for (let i = 0; i < 6; i++) spoke(i / 6 * Math.PI * 2, 0.07);
  else for (let i = 0; i < 5; i++) { const a = i / 5 * Math.PI * 2; spoke(a - 0.13, 0.032); spoke(a + 0.13, 0.032); }   // cinq branches doubles
  for (let i = 0; i < 5; i++) { const a = i / 5 * Math.PI * 2 + 0.3, nut = new THREE.CylinderGeometry(0.011, 0.011, 0.03, 6); nut.rotateZ(Math.PI / 2); nut.translate(face + side * 0.05, Math.cos(a) * 0.052, Math.sin(a) * 0.052); add(nut, 1.3); }   // écrous
  for (let i = 0; i < 12; i++) { const a = i / 12 * Math.PI * 2, sl = new THREE.BoxGeometry(0.006, ri * 0.3, 0.012); sl.translate(0, ri * 0.62, 0); sl.rotateX(a + 0.5); sl.translate(o - side * 0.116, 0, 0); add(sl, 0.12); }           // rainures du disque
  const hub = new THREE.CylinderGeometry(0.075, 0.085, 0.07, 14); hub.rotateZ(Math.PI / 2); hub.translate(face + side * 0.01, 0, 0); add(hub, 0.9);
  const cap = new THREE.CylinderGeometry(0.035, 0.035, 0.02, 10); cap.rotateZ(Math.PI / 2); cap.translate(face + side * 0.05, 0, 0); add(cap, 0.12);
  const disc = new THREE.CylinderGeometry(ri * 0.86, ri * 0.86, 0.025, 26); disc.rotateZ(Math.PI / 2); disc.translate(o - side * 0.13, 0, 0); add(disc, 0.42);
  // flanc du pneu : liseré clair et crampons de la bande de roulement
  const tp = [tint(flat(tire), 1, 1, 1)];
  const ring = new THREE.TorusGeometry((r + ri) / 2 + 0.01, 0.004, 4, 40); ring.rotateY(Math.PI / 2); ring.translate(o + side * 0.001, 0, 0); tp.push(tint(flat(ring), 40, 40, 40));
  for (let i = 0; i < 36; i++) { const a = i / 36 * Math.PI * 2, gr = new THREE.BoxGeometry(tw * 0.8, 0.012, 0.014); gr.translate(0, r + 0.001, 0); gr.rotateX(a); tp.push(tint(flat(gr), 0.35, 0.35, 0.35)); }
  const res = { tire: mergeGeometries(tp), metal: mergeGeometries(m), ri }; res.tire.userData.keep = res.metal.userData.keep = true;
  wheelCache.set(key, res); return res;
}

// ---------- la voiture ----------
// def.design : { L, W, t, w, hc, hs, b, roues, toit, ... } — voir carlist.js pour les valeurs de chaque modèle
export function buildDesign(def, paint) {
  const d = def.design, root = new THREE.Group(), chassis = new THREE.Group(); root.add(chassis);
  const Lh = d.L / 2, HW = d.W / 2;
  const Wf = table(d.t, d.w), HC = table(d.t, d.hc), HS = table(d.t, d.hs), Bf = table(d.t, d.b);
  const wheelsDef = [];
  for (const [t, r, tw] of d.roues) wheelsDef.push({ t, z: -Lh + t * 2 * Lh, r, tw });
  const NB = 3, NS = 14, NP = NB + NS;
  const bump = (x) => Math.abs(x) < 1 ? 0.5 + 0.5 * Math.cos(Math.PI * x) : 0;
  // profil d'une section (demi-voiture, x >= 0) : du centre du plancher au centre du capot
  function section(t) {
    const z = -Lh + t * 2 * Lh, w = Wf(t) * HW, hc = HC(t); let hs = HS(t); const b = Bf(t); let ba = b, xi = w * 0.72;
    for (const wh of wheelsDef) {
      const dz = z - wh.z, Ra = wh.r + 0.07;
      if (Math.abs(dz) < Ra) { ba = Math.max(ba, wh.r + Math.sqrt(Ra * Ra - dz * dz)); xi = Math.max(0.2, w - wh.tw - 0.09); }
      const top = wh.r + Ra + (d.aile ?? 0.085);
      hs += Math.max(0, top - hs) * bump(dz / (Ra * 1.7));        // l'aile se bombe au-dessus de la roue
    }
    hs = Math.max(hs, ba + 0.075);
    const H = hs - ba, crown = d.bombe ?? 0.02;
    const ctrl = [[w * 0.975, ba], [w, ba + 0.3 * H], [w * (d.flanc ?? 1), ba + 0.62 * H], [w * 0.955, ba + 0.88 * H], [w * 0.8, hs], [w * 0.45, lerp(hs, hc, 0.72) + crown * 0.5], [0, hc + crown]];
    const sp = new THREE.CatmullRomCurve3(ctrl.map(p => new THREE.Vector3(p[0], p[1], 0)), false, 'centripetal').getPoints(NS - 1);
    const pts = [[0, b], [xi, b], [xi, ba]];
    for (const p of sp) pts.push([p.x, p.y]);
    return { z, pts };
  }
  const M = 96, secs = []; for (let m = 0; m <= M; m++) secs.push(section(m / M));
  const ring = NP * 2 - 2, pos = [], col = [], idx = [];
  for (const s of secs) {
    for (let i = 0; i < NP; i++) { pos.push(s.pts[i][0], s.pts[i][1], s.z); const k = i < NB ? 0.02 : 1; col.push(k, k, k); }
    for (let i = NP - 2; i >= 1; i--) { pos.push(-s.pts[i][0], s.pts[i][1], s.z); const k = i < NB ? 0.02 : 1; col.push(k, k, k); }
  }
  for (let m = 0; m < M; m++) for (let i = 0; i < ring; i++) { const a = m * ring + i, b2 = m * ring + (i + 1) % ring, c = a + ring, e = b2 + ring; idx.push(a, b2, c, b2, e, c); }
  const bodyGeo = new THREE.BufferGeometry(); bodyGeo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); bodyGeo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3)); bodyGeo.setIndex(idx); bodyGeo.computeVertexNormals();
  const caps = [];
  for (const m of [0, M]) {
    const cp = [], cc = [], ci = []; let cy = 0;
    for (let i = 0; i < ring; i++) { const k = (m * ring + i) * 3; cp.push(pos[k], pos[k + 1], pos[k + 2]); cc.push(1, 1, 1); cy += pos[k + 1]; }
    cp.push(0, cy / ring, pos[m * ring * 3 + 2]); cc.push(1, 1, 1);
    for (let i = 0; i < ring; i++) m === 0 ? ci.push(ring, (i + 1) % ring, i) : ci.push(ring, i, (i + 1) % ring);   // faces tournées vers l'extérieur
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(cp, 3)); g.setAttribute('color', new THREE.Float32BufferAttribute(cc, 3)); g.setIndex(ci); g.computeVertexNormals(); caps.push(g);
  }
  const paintMat = new THREE.MeshPhysicalMaterial({ color: paint, vertexColors: true, metalness: 0.82, roughness: 0.28, clearcoat: 1, clearcoatRoughness: 0.035, envMapIntensity: 1.6 });
  chassis.add(new THREE.Mesh(mergeGeometries([bodyGeo, ...caps].map(g => g.toNonIndexed())), paintMat));

  // pièce rapportée qui épouse la carrosserie (feux, bandes, ouïes) : t le long de la voiture, f le long du profil
  const secAt = (t) => section(clamp(t, 0, 1));
  function skin(t0, t1, f0, f1, side, nt = 8, nf = 6, off = 0.007) {
    const p = [], ix = [];
    for (let a = 0; a <= nt; a++) { const s = secAt(lerp(t0, t1, a / nt));
      for (let c = 0; c <= nf; c++) { const f = clamp(lerp(f0, f1, c / nf), 0, NP - 1), i0 = Math.floor(f), i1 = Math.min(NP - 1, i0 + 1), u = f - i0;
        p.push(side * lerp(s.pts[i0][0], s.pts[i1][0], u), lerp(s.pts[i0][1], s.pts[i1][1], u), s.z); } }
    for (let a = 0; a < nt; a++) for (let c = 0; c < nf; c++) { const i = a * (nf + 1) + c; ix.push(i, i + 1, i + nf + 1, i + 1, i + nf + 2, i + nf + 1); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(p, 3)); g.setIndex(ix); g.computeVertexNormals();
    const P = g.attributes.position, N = g.attributes.normal;
    for (let i = 0; i < P.count; i++) { const sgn = (N.getX(i) * P.getX(i) + N.getY(i) * (P.getY(i) - 0.4)) < 0 ? -1 : 1; P.setXYZ(i, P.getX(i) + N.getX(i) * off * sgn, P.getY(i) + N.getY(i) * off * sgn, P.getZ(i) + N.getZ(i) * off * sgn); }
    g.computeVertexNormals(); return flat(g);
  }
  const TOP = NP - 1;
  const dark = [], accent = [], headG = [], tailG = [], chrome = [];
  const accentCol = new THREE.Color(d.accent ?? 0xffffff);

  // ---------- habitacle vitré, toit peint ----------
  const cab = d.toit;
  {
    const CM = 26, CR = 9, cp = [], ci = [], rowN = CR * 2 - 1, roofP = [], roofI = [];
    const shape = table(cab.u || [0, 0.18, 0.4, 0.55, 0.72, 0.88, 1], cab.forme || [0, 0.66, 0.97, 1, 0.9, 0.5, 0]);
    const wide = table([0, 0.5, 1], cab.larg || [0.8, 0.74, 0.76]);
    const rows = [];
    for (let m = 0; m <= CM; m++) {
      const u = m / CM, t = lerp(cab.t0, cab.t1, u), z = -Lh + t * 2 * Lh, h0 = Math.min(HC(t), HS(t)) - 0.035;
      const top = h0 + Math.max(0, (cab.h - h0) * shape(u)) + 0.02, wb = Math.min(0.93, wide(u)) * HW * Wf(t), wt = wb * (cab.pince ?? 0.68), H = top - h0;
      const pr = new THREE.CatmullRomCurve3([[wb, h0], [wb * 0.985, h0 + 0.3 * H], [wt * 1.1, h0 + 0.8 * H], [wt * 0.78, top], [0, top + 0.015]].map(q => new THREE.Vector3(q[0], q[1], 0)), false, 'centripetal').getPoints(CR - 1);
      const row = []; for (let i = 0; i < CR; i++) row.push([pr[i].x, pr[i].y, z]); for (let i = CR - 2; i >= 0; i--) row.push([-pr[i].x, pr[i].y, z]);
      rows.push(row); for (const q of row) cp.push(q[0], q[1], q[2]);
    }
    for (let m = 0; m < CM; m++) for (let i = 0; i < rowN - 1; i++) { const a = m * rowN + i, b2 = a + 1, c = a + rowN, e = c + 1; ci.push(a, c, b2, b2, c, e); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(cp, 3)); g.setIndex(ci); g.computeVertexNormals();
    // vitres teintées mais transparentes : on voit l'habitacle
    const glass = new THREE.Mesh(g, new THREE.MeshPhysicalMaterial({ color: 0x0a1016, metalness: 0.9, roughness: 0.03, envMapIntensity: 2.6, side: THREE.DoubleSide, transparent: true, opacity: 0.56, depthWrite: false }));
    glass.renderOrder = 4; chassis.add(glass);
    {
      const tm = lerp(cab.t0, cab.t1, 0.47), zs = -Lh + tm * 2 * Lh, fl = Math.min(HC(tm), HS(tm)) - 0.02, hh = Math.max(0.25, cab.h - fl - 0.1), sx = Math.min(0.4, Wf(tm) * HW * 0.36), int = [], cuir = [];
      for (const s of [-1, 1]) {
        cuir.push(flat(box(0.4, 0.1, 0.44, s * sx, fl + 0.07, zs)), flat(box(0.4, hh * 0.72, 0.1, s * sx, fl + hh * 0.4, zs + 0.24, -0.22)), flat(box(0.2, 0.14, 0.08, s * sx, fl + hh * 0.84, zs + 0.3, -0.22)));   // assise, dossier, appuie-tête
      }
      const zd = -Lh + lerp(cab.t0, cab.t1, 0.22) * 2 * Lh;
      int.push(flat(box(Wf(tm) * HW * 1.3, 0.14, 0.26, 0, fl + hh * 0.42, zd)));                                                                // planche de bord
      int.push(flat(box(0.2, 0.2, 0.9, 0, fl + 0.08, zs - 0.1)));                                                                                // console centrale
      const vol = new THREE.TorusGeometry(0.15, 0.018, 6, 18); vol.rotateX(-0.45); vol.translate(-sx, fl + hh * 0.5, zd + 0.2); int.push(flat(vol));  // volant
      int.push(flat(box(0.02, 0.2, 0.02, -sx, fl + hh * 0.4, zd + 0.12, -0.45)));
      chassis.add(new THREE.Mesh(mergeGeometries(int), new THREE.MeshStandardMaterial({ color: 0x15161a, roughness: 0.8 })));
      chassis.add(new THREE.Mesh(mergeGeometries(cuir), new THREE.MeshStandardMaterial({ color: d.sellerie ?? 0x2a2c33, roughness: 0.6 })));
    }
    if (cab.peint) {
      // toit et montants : une peau posée sur le haut de la bulle, entre le pare-brise et la lunette
      const [u0, u1] = cab.peint, m0 = Math.round(u0 * CM), m1 = Math.round(u1 * CM), i0 = CR - 4, i1 = CR + 2, n = i1 - i0 + 1;
      for (let m = m0; m <= m1; m++) for (let i = i0; i <= i1; i++) { const q = rows[m][i]; roofP.push(q[0] * 1.012, q[1] + 0.012, q[2]); }
      for (let m = 0; m < m1 - m0; m++) for (let i = 0; i < n - 1; i++) { const a = m * n + i; roofI.push(a, a + n, a + 1, a + 1, a + n, a + n + 1); }
      const rg = new THREE.BufferGeometry(); rg.setAttribute('position', new THREE.Float32BufferAttribute(roofP, 3)); rg.setIndex(roofI); rg.computeVertexNormals();
      const rgf = tint(rg.toNonIndexed(), 1, 1, 1);
      const roofMat = cab.toitNoir ? new THREE.MeshPhysicalMaterial({ color: 0x08090c, metalness: 0.7, roughness: 0.25, clearcoat: 1, side: THREE.DoubleSide }) : paintMat.clone();
      roofMat.side = THREE.DoubleSide; chassis.add(new THREE.Mesh(rgf, roofMat)); if (!cab.toitNoir) chassis.userData.roofMat = roofMat;
    }
  }

  // ---------- bandes de course, ouïes, prises d'air ----------
  if (d.bandes === 'double') for (const s of [-1, 1]) accent.push(skin(0.012, 0.988, TOP - 2.1, TOP - 0.75, s, 60, 3));
  else if (d.bandes === 'centre') for (const s of [-1, 1]) accent.push(skin(0.012, 0.988, TOP - 1.25, TOP, s, 60, 3));
  else if (d.bandes === 'flanc') for (const s of [-1, 1]) accent.push(skin(0.24, 0.76, NB + 1.1, NB + 2.2, s, 30, 3));
  for (const s of [-1, 1]) {
    if (d.ouies) for (const tv of d.ouies) dark.push(skin(tv, tv + 0.05, TOP - 4.2, TOP - 1.6, s, 4, 4));          // ouïes de capot
    if (d.priseFlanc) dark.push(skin(d.priseFlanc[0], d.priseFlanc[1], NB + 2.2, NB + 5.0, s, 8, 6));               // prise d'air latérale
    if (d.persiennes) for (let i = 0; i < 5; i++) dark.push(skin(d.persiennes + i * 0.035, d.persiennes + i * 0.035 + 0.02, TOP - 3.4, TOP, s, 2, 5)); // persiennes de lunette
    // optiques
    const hd = d.phares || [0.03, 0.085, NB + 6.4, NB + 9.6];
    headG.push(skin(hd[0], hd[1], hd[2], hd[3], s, 5, 5, 0.009));
    dark.push(skin(hd[0] - 0.008, hd[1] + 0.012, hd[2] - 0.5, hd[3] + 0.5, s, 5, 5, 0.004));
    if (d.feuxFlanc) tailG.push(skin(0.965, 0.998, NB + 7.6, NB + 8.8, s, 4, 3, 0.009));                  // feux qui débordent sur l'aile
  }
  const amber = [], plate = [];
  {
    const t0 = cab.t0, t1 = cab.t1, dA = t0 + (t1 - t0) * 0.12, dB = t0 + (t1 - t0) * (d.porte ?? 0.62);
    for (const s of [-1, 1]) {
      for (const tv of [dA, dB]) dark.push(skin(tv, tv + 0.0035, NB + 0.4, NB + 9.2, s, 1, 12, 0.004));                 // joints de porte
      dark.push(skin(dA, dB, NB + 0.9, NB + 1.0, s, 12, 1, 0.004));                                                    // bas de porte
      chrome.push(skin(dB - 0.05, dB - 0.018, NB + 6.3, NB + 6.6, s, 2, 1, 0.012));                                      // poignée
      dark.push(skin(0.03, t0 - 0.01, TOP - 3.25, TOP - 3.15, s, 14, 1, 0.004));                                         // arête de capot
      amber.push(skin(0.105, 0.125, NB + 4.4, NB + 5.0, s, 1, 1, 0.01));                                                 // répétiteur d'aile
      dark.push(skin(t1 + 0.03, t1 + 0.055, NB + 7.2, NB + 8.2, s, 2, 2, 0.006));                                        // trappe à essence / aération
      // essuie-glace posé au pied du pare-brise
      const wz = -Lh + (t0 + 0.035) * 2 * Lh, wy = Math.min(HC(t0 + 0.035), HS(t0 + 0.035)) + 0.035;
      dark.push(flat(box(HW * 0.42, 0.012, 0.018, s * HW * 0.2, wy, wz, 0, s * 0.12)));
    }
  }
  // face arrière
  const hR = HC(1), bR = Bf(1), wR = Wf(1) * HW, yL = lerp(bR, hR, 0.72);
  if (d.feux === 'ronds') for (const s of [-1, 1]) for (const k of [0.42, 0.72]) { const g = new THREE.CylinderGeometry(0.075, 0.075, 0.03, 16); g.rotateX(Math.PI / 2); g.translate(s * wR * k, yL, Lh + 0.012); tailG.push(flat(g)); }
  else { tailG.push(flat(box(wR * 1.7, 0.035, 0.03, 0, yL, Lh + 0.012))); for (const s of [-1, 1]) tailG.push(flat(box(wR * 0.34, 0.07, 0.03, s * wR * 0.68, yL - 0.02, Lh + 0.014))); }
  dark.push(flat(box(wR * 1.7, (hR - bR) * 0.4, 0.025, 0, lerp(bR, hR, 0.27), Lh + 0.006)));                           // bouclier noir
  plate.push(flat(box(0.5, 0.11, 0.012, 0, lerp(bR, hR, 0.4), Lh + 0.022)));                                           // plaque arrière
  tailG.push(flat(box(0.34, 0.016, 0.02, 0, hR + 0.012, Lh - 0.03)));                                                 // troisième feu stop
  for (const s of [-1, 1]) { amber.push(flat(box(wR * 0.2, 0.03, 0.02, s * wR * 0.78, yL - 0.085, Lh + 0.014))); chrome.push(flat(box(0.05, 0.05, 0.02, s * wR * 0.86, lerp(bR, hR, 0.18), Lh + 0.016))); }   // clignotants, catadioptres
  dark.push(flat(box(wR * 1.5, 0.2, 0.36, 0, bR + 0.03, Lh - 0.16)));                                                // diffuseur
  for (let i = -3; i <= 3; i++) dark.push(flat(box(0.022, 0.15, 0.34, i * wR * 0.2, bR + 0.02, Lh - 0.14)));
  // face avant : calandre, lame
  const hF = HC(0), bF = Bf(0), wF = Wf(0) * HW;
  dark.push(flat(box(wF * 1.6, (hF - bF) * (d.calandre ?? 0.55), 0.03, 0, lerp(bF, hF, 0.42), -Lh - 0.006)));
  dark.push(flat(box(Wf(0.06) * HW * 1.9, 0.028, 0.34, 0, Bf(0.05) - 0.01, -Lh + 0.13)));
  { const gh = (hF - bF) * (d.calandre ?? 0.55), gy = lerp(bF, hF, 0.42);
    for (let i = -1; i <= 1; i++) chrome.push(flat(box(wF * 1.5, 0.008, 0.012, 0, gy + i * gh * 0.28, -Lh - 0.024)));  // barrettes de calandre
    plate.push(flat(box(0.42, 0.1, 0.012, 0, gy - gh * 0.05, -Lh - 0.03)));                                            // plaque avant
    for (const s of [-1, 1]) { headG.push(flat(box(0.16, 0.025, 0.02, s * Wf(0.03) * HW * 0.78, Bf(0.03) + 0.07, -Lh + 0.035))); dark.push(flat(box(0.03, 0.09, 0.3, s * Wf(0.05) * HW * 1.0, Bf(0.05) + 0.05, -Lh + 0.2, 0, 0, s * 0.5))); } }   // feux de jour, dérives de pare-chocs
  for (const s of [-1, 1]) {
    dark.push(flat(box(0.05, 0.1, d.L * 0.36, s * (HW * 0.99), Bf(0.5) + 0.02, 0)));                                 // bas de caisse
    const mt = d.retro ?? (cab.t0 + 0.07), mh = Math.min(HC(mt), HS(mt));
    dark.push(flat(box(0.2, 0.075, 0.11, s * (Wf(mt) * HW * 0.86 + 0.1), mh + 0.14, -Lh + mt * 2 * Lh, 0, s * -0.25)));
    dark.push(flat(box(0.035, 0.1, 0.035, s * (Wf(mt) * HW * 0.82), mh + 0.07, -Lh + mt * 2 * Lh)));
  }
  // aileron
  const al = d.aileron;
  if (al === 'gt' || al === 'proto') {
    const wy = (d.aileronH ?? 1.18), ww = HW * (al === 'proto' ? 1.96 : 1.78), z = Lh - 0.16;
    dark.push(flat(box(ww, 0.03, 0.4, 0, wy, z, -0.1)), flat(box(ww, 0.025, 0.13, 0, wy + 0.055, z + 0.23, -0.35)));
    for (const s of [-1, 1]) { dark.push(flat(box(0.028, 0.3, 0.56, s * ww / 2, wy - 0.03, z + 0.05))); dark.push(flat(box(0.035, wy - HC(0.9) + 0.06, 0.2, s * HW * 0.4, (wy + HC(0.9)) / 2 - 0.02, z - 0.12, 0.25))); }
  } else if (al === 'canard') dark.push(flat(box(wR * 1.85, 0.03, 0.26, 0, hR + 0.075, Lh - 0.1, -0.32)));
  else if (al === 'plat') { const wy = HC(0.95) + 0.2; dark.push(flat(box(HW * 1.86, 0.035, 0.34, 0, wy, Lh - 0.22))); for (const s of [-1, 1]) dark.push(flat(box(0.05, 0.22, 0.4, s * HW * 0.9, wy - 0.1, Lh - 0.22))); }
  if (d.derive) dark.push(flat(box(0.03, 0.42, d.L * 0.3, 0, cab.h - 0.06, Lh * 0.52, 0.16)));                         // dérive de prototype
  if (d.rampe) { headG.push(flat(box(HW * 1.1, 0.07, 0.07, 0, cab.h + 0.1, -Lh + (cab.t0 + 0.2) * 2 * Lh))); dark.push(flat(box(HW * 1.2, 0.05, 0.1, 0, cab.h + 0.06, -Lh + (cab.t0 + 0.2) * 2 * Lh + 0.05))); }
  if (d.galerie) for (const s of [-1, 1]) chrome.push(flat(box(0.035, 0.035, d.L * 0.34, s * HW * 0.5, cab.h + 0.055, Lh * 0.18)));
  // échappements
  const exh = d.echap || [-0.2, 0.2];
  for (const x of exh) { const g = new THREE.CylinderGeometry(0.058, 0.066, 0.14, 14, 1, true); g.rotateX(Math.PI / 2); g.translate(x * wR, bR + (d.echapY ?? 0.2), Lh + 0.035); chrome.push(flat(g)); const gi = new THREE.CircleGeometry(0.05, 12); gi.translate(x * wR, bR + (d.echapY ?? 0.2), Lh + 0.02); dark.push(flat(gi)); }

  const trimMat = new THREE.MeshStandardMaterial({ color: 0x07080b, metalness: 0.6, roughness: 0.38, envMapIntensity: 1.3, side: THREE.DoubleSide });
  chassis.add(new THREE.Mesh(mergeGeometries(dark), trimMat));
  if (accent.length) chassis.add(new THREE.Mesh(mergeGeometries(accent), new THREE.MeshPhysicalMaterial({ color: accentCol, metalness: 0.5, roughness: 0.3, clearcoat: 1, clearcoatRoughness: 0.05, side: THREE.DoubleSide })));
  chassis.add(new THREE.Mesh(mergeGeometries(chrome), new THREE.MeshStandardMaterial({ color: 0xb8bcc8, metalness: 1, roughness: 0.18, side: THREE.DoubleSide })));
  chassis.add(new THREE.Mesh(mergeGeometries(amber), new THREE.MeshBasicMaterial({ color: new THREE.Color(0xff8a10).multiplyScalar(2), side: THREE.DoubleSide })));
  chassis.add(new THREE.Mesh(mergeGeometries(plate), new THREE.MeshStandardMaterial({ color: 0xe8e8e0, roughness: 0.5, emissive: 0x30302c })));
  const tailMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(0xff1408).multiplyScalar(3), side: THREE.DoubleSide });
  chassis.add(new THREE.Mesh(mergeGeometries(tailG), tailMat));
  chassis.add(new THREE.Mesh(mergeGeometries(headG), new THREE.MeshBasicMaterial({ color: new THREE.Color(d.pharesCol ?? 0xeaf4ff).multiplyScalar(3.4), side: THREE.DoubleSide })));

  // ---------- roues ----------
  const tireMat = new THREE.MeshStandardMaterial({ color: 0x0c0c0e, vertexColors: true, metalness: 0, roughness: 0.92 });
  const rimMat = new THREE.MeshStandardMaterial({ color: d.jante ?? 0xc9ccd6, vertexColors: true, metalness: 0.95, roughness: 0.24, envMapIntensity: 1.5 });
  const calMat = new THREE.MeshStandardMaterial({ color: d.etrier ?? 0xd8121e, metalness: 0.4, roughness: 0.4 });
  const wheels = [];
  for (const wh of wheelsDef) for (const s of [-1, 1]) {
    const wx = Wf(wh.t) * HW - wh.tw / 2 - 0.015, G = wheelGeos(wh.r, wh.tw, d.jantes || 'double', s);
    const steer = new THREE.Group(); steer.position.set(s * wx, wh.r, wh.z);
    const spin = new THREE.Group(); spin.add(new THREE.Mesh(G.tire, tireMat), new THREE.Mesh(G.metal, rimMat)); steer.add(spin);
    const cal = new THREE.Mesh(box(0.075, G.ri * 0.3, G.ri * 0.95, s * (wh.tw / 2 - 0.13), G.ri * 0.68, 0, 0), calMat); cal.rotation.x = (wh.t < 0.5 ? -1 : 1) * 0.9; steer.add(cal);   // étrier fixe
    root.add(steer); wheels.push({ steer, spin, front: wh.t < 0.5, radius: wh.r });
  }

  // ---------- voiture volante : propulseurs lumineux à la place des roues ----------
  const jets = [];
  if (d.propulseurs) {
    const ring = [], glowG = [];
    for (const [t, k, r] of d.propulseurs) for (const s of [-1, 1]) { const x = s * Wf(t) * HW * k, z = -Lh + t * 2 * Lh, y = Bf(t) - 0.02;
      const g1 = new THREE.CylinderGeometry(r * 1.25, r * 1.05, 0.12, 20, 1, true); g1.translate(x, y, z); ring.push(flat(g1));
      const g2 = new THREE.CircleGeometry(r, 20); g2.rotateX(Math.PI / 2); g2.translate(x, y - 0.03, z); glowG.push(flat(g2)); jets.push([x, y, z]); }
    chassis.add(new THREE.Mesh(mergeGeometries(ring), new THREE.MeshStandardMaterial({ color: 0x30333c, metalness: 1, roughness: 0.3, side: THREE.DoubleSide })));
    chassis.add(new THREE.Mesh(mergeGeometries(glowG), new THREE.MeshBasicMaterial({ color: new THREE.Color(d.neon ?? 0x19e6ff).multiplyScalar(3.5), side: THREE.DoubleSide })));
  }
  // ---------- néon sous la caisse, ombre ----------
  shadowTex = shadowTex || radialTex([[0, 'rgba(0,0,0,0.95)'], [0.6, 'rgba(0,0,0,0.7)'], [1, 'rgba(0,0,0,0)']]);
  const sh = new THREE.Mesh(new THREE.PlaneGeometry(d.W + 0.9, d.L + 1.1), new THREE.MeshBasicMaterial({ map: shadowTex, transparent: true, depthWrite: false, fog: false }));
  sh.rotation.x = -Math.PI / 2; sh.position.y = 0.015; sh.layers.set(1); sh.renderOrder = 1; root.add(sh);
  let neon = null;
  if (d.neon) {
    glowTex = glowTex || radialTex([[0, 'rgba(255,255,255,0.9)'], [0.45, 'rgba(255,255,255,0.45)'], [1, 'rgba(255,255,255,0)']]);
    neon = new THREE.Mesh(new THREE.PlaneGeometry(d.W + 1.5, d.L + 1.3), new THREE.MeshBasicMaterial({ map: glowTex, color: new THREE.Color(d.neon).multiplyScalar(1.5), transparent: true, opacity: 0.75, blending: THREE.AdditiveBlending, depthWrite: false, fog: false }));
    neon.rotation.x = -Math.PI / 2; neon.position.y = 0.03; neon.layers.set(1); neon.renderOrder = 2; root.add(neon);
  }
  const tc = cab.t0 + (cab.t1 - cab.t0) * 0.12;
  const paintMats = [paintMat]; if (chassis.userData.roofMat) paintMats.push(chassis.userData.roofMat);
  return { root, chassis, wheels, paintMats, tailMats: [tailMat], glows: [], half: HW, len: Lh, neon, jets, hover: d.propulseurs ? 0.42 : 0, cockpit: new THREE.Vector3(0, HC(tc) + 0.4, -Lh + tc * 2 * Lh) };
}
