// Générateurs procéduraux mobiles (repère Y haut, avant +Z). Les modèles organiques sont des
// SkinnedMesh avec un vrai squelette (animables et exportables en glTF).
import * as THREE from "three";
import { buildOrganic, skinWeights } from "../organic.js";
import { V, box, curvePoints, cylinder, ellipseSection, ellipsoid, fanMembrane, lathe, loft, merge, place, sweep, taper } from "../geom.js";
import { applyPattern, boxUV, canvasTexture, fbm, makeMaterial } from "../materials.js";

export const QUALITY = { draft: 60, standard: 96, high: 128, ultra: 160 };

const DEFAULT_HEIGHTS = {
  "creature/dragon": 2.5, "creature/hydra": 3, "creature/kraken": 4, "creature/blob": 1.2, "creature/demon": 2.4,
  "creature/golem": 3, "creature/griffin": 2, "creature/alien": 1.7, "creature/beast": 1.8, "character/humanoid": 1.8,
  "animal/wolf": 0.85, "animal/cat": 0.35, "animal/horse": 1.7, "animal/bear": 1.4, "animal/deer": 1.6, "animal/cow": 1.5,
  "animal/rabbit": 0.3, "animal/bird": 0.4, "animal/fish": 0.4, "animal/snake": 0.25, "animal/spider": 0.15, "animal/lizard": 0.25,
  "vehicle/car": 1.45, "vehicle/truck": 3.2, "vehicle/spaceship": 4, "vehicle/boat": 8.5, "vehicle/plane": 4, "vehicle/tank": 2.6,
  "vehicle/bike": 1.1, "weapon/sword": 1.2, "weapon/dagger": 0.4, "weapon/axe": 0.95, "weapon/spear": 2.3, "weapon/hammer": 0.95,
  "weapon/bow": 1.4, "weapon/staff": 1.9, "weapon/shield": 1, "weapon/scythe": 1.9, "weapon/gun": 0.3, "prop/barrel": 0.9,
  "prop/crate": 0.8, "prop/chest": 0.65, "prop/table": 0.78, "prop/chair": 0.95, "prop/lamp": 0.74, "prop/bottle": 0.3,
  "prop/rock": 1, "prop/tree": 7, "prop/bush": 1.2, "prop/mushroom": 0.6, "prop/crystal": 0.9, "prop/well": 2.4,
  "prop/fence": 1.1, "prop/crown": 0.15,
};

const c3 = (v, d) => (v ? [v[0], v[1], v[2]] : d);
const S = (spec, k) => +(spec.scales?.[k] ?? 1);

// ------------------------------------------------------------------ constructeur organique
class Organic {
  constructor(spec, name) {
    this.spec = spec;
    this.name = name;
    this.bones = [];       // {name, head, tail, parent, role}
    this.caps = [];        // {a, b, ra, rb, bone, field}
    this.rigid = [];       // {geo, mat, bone, name}
    this.skinned = [];     // {geo, mat, bones:[...]|null, name}
    this.mats = {};
  }
  bone(name, head, tail, parent = null, role = "", r0 = null, r1 = null, field = true) {
    this.bones.push({ name, head: head.clone(), tail: tail.clone(), parent, role });
    if (r0 !== null) this.caps.push({ a: head.toArray(), b: tail.toArray(), ra: r0, rb: r1 ?? r0, bone: name, field });
    return name;
  }
  cap(a, b, ra, rb, bone, field = true) { this.caps.push({ a: a.toArray(), b: b.toArray(), ra, rb: rb ?? ra, bone, field }); }
  mat(key, preset, p) { return (this.mats[key] ||= makeMaterial(preset, p)); }
}

function bodyMaterialParams(spec, defPreset, defColor) {
  const preset = spec.material || (spec.features?.scales ? "scales" : spec.features?.fur ? "fur" : defPreset);
  const p = { color: c3(spec.colors?.primary, defColor) };
  if (spec.colors?.secondary) p.color2 = c3(spec.colors.secondary);
  if (spec.colors?.veins) p.veins = c3(spec.colors.veins);
  if (spec.wet) p.wet = 0.6;
  if (spec.glow) { p.veins = c3(spec.colors?.veins || spec.colors?.secondary, [1, 0.35, 0.05]); }
  return [preset, p];
}

function eyeMat(o) {
  const c = c3(o.spec.colors?.eyes, [0.95, 0.75, 0.1]);
  return o.mat("yeux", "eye", { color: c, emission: o.spec.glow_eyes ? c : null, emission_strength: o.spec.glow_eyes ? 3 : 0 });
}
function hornMat(o) {
  const c = c3(o.spec.colors?.horns, [0.85, 0.8, 0.68]);
  return o.spec.glow && o.spec.colors?.horns ? o.mat("cornes", "emission", { color: c }) : o.mat("cornes", "plain", { color: c, roughness: 0.4 });
}

function addEyes(o, hc, hr, fwd, count, bone, size = 0.2, spread = 0.55, height = 0.3) {
  if (count <= 0) return;
  const r = hr * size * S(o.spec, "eyes");
  const side = V(1, 0, 0), up = V(0, 1, 0);
  let k = 0;
  for (let row = 0; k < count; row++) {
    const n = Math.min(2, count - k);
    for (const s of n === 2 ? [1, -1] : [0]) {
      const p = hc.clone().addScaledVector(fwd, hr * 0.78).addScaledVector(side, s * hr * spread * (1 - row * 0.1)).addScaledVector(up, hr * (height - row * 0.35));
      p.add(p.clone().sub(hc).normalize().multiplyScalar(r * 0.15));
      o.rigid.push({ geo: place(ellipsoid(r, r, r, 14, 10), { pos: p }), mat: eyeMat(o), bone, name: "Œil" });
      if (!o.spec.glow_eyes) {
        const d = p.clone().sub(hc).normalize().multiplyScalar(0.6).add(fwd.clone().multiplyScalar(0.4)).normalize();
        o.rigid.push({ geo: place(ellipsoid(r * 0.45, r * 0.45, r * 0.45, 10, 8), { pos: p.clone().addScaledVector(d, r * 0.75) }), mat: o.mat("pupille", "plain", { color: [0.02, 0.02, 0.02], roughness: 0.2 }), bone, name: "Pupille" });
      }
      k++;
    }
  }
}

function addHorns(o, hc, hr, count, bone, style = "curved") {
  const s = S(o.spec, "horns");
  let k = 0;
  for (let row = 0; k < count; row++) {
    const n = Math.min(2, count - k);
    for (const sd of n === 2 ? [1, -1] : [0]) {
      const base = hc.clone().add(V(sd * hr * (0.45 + 0.1 * row), hr * (0.65 - 0.15 * row), -hr * (0.1 + 0.35 * row)));
      const len = hr * (2.2 - 0.4 * row) * s;
      const pts = style === "ram"
        ? curvePoints(base, V(sd * 0.6, 0.5, -0.6), len * 1.3, 12, V(sd * 0.2, 1, 0.3), (sd >= 0 ? 1 : -1) * 5.2)
        : curvePoints(base, V(sd * 0.35, 0.6, -0.75), len, 8, V(1, 0, 0), -0.95);
      o.rigid.push({ geo: sweep(pts, taper(pts.length, hr * 0.2 * Math.min(1.4, Math.sqrt(s)), 0, 0.9), { sides: 8, tip: true }), mat: hornMat(o), bone, name: "Corne" });
      k++;
    }
  }
}

function addTeeth(o, front, width, size, bone) {
  const geos = [];
  for (let k = -3; k <= 3; k++) {
    const b = front.clone().add(V(k / 3 * width, 0, -Math.abs(k) * size * 0.6));
    const L = size * (Math.abs(k) === 2 ? 1.8 : 1);
    geos.push(sweep([b, b.clone().add(V(0, -L * 0.6, 0)), b.clone().add(V(0, -L, size * 0.1))], [size * 0.3, size * 0.2, 0], { sides: 5, tip: true }));
  }
  o.rigid.push({ geo: merge(geos), mat: o.mat("dents", "plain", { color: [0.95, 0.92, 0.82], roughness: 0.2 }), bone, name: "Crocs" });
}

function addSpikes(o, pts, rad) {
  const c = c3(o.spec.colors?.secondary || o.spec.colors?.horns, [0.8, 0.75, 0.6]);
  const mat = o.spec.glow ? o.mat("pointes", "emission", { color: c }) : o.mat("pointes", "plain", { color: c, roughness: 0.35 });
  const s = S(o.spec, "spikes");
  const geos = pts.map((p, i) => {
    const r = rad[i];
    const base = p.clone().add(V(0, r * 0.75, 0));
    const q = curvePoints(base, V(0, 1, -0.35), r * 0.9 * s, 3, V(1, 0, 0), 0.45);
    return sweep(q, taper(q.length, r * 0.28 * Math.sqrt(s), 0), { sides: 6, tip: true, flatten: 0.45 });
  });
  o.skinned.push({ geo: merge(geos), mat, bones: null, name: "Pointes" });
}

function addClaws(o, feet, size) {
  const mat = o.mat("griffes", "plain", { color: c3(o.spec.colors?.horns, [0.12, 0.1, 0.08]), roughness: 0.25 });
  const geos = [];
  for (const f of feet) for (const k of [-1, 0, 1]) {
    const b = f.clone().add(V(k * size * 0.9, size * 0.3, size * 0.8));
    const q = curvePoints(b, V(k * 0.15, -0.1, 1), size * 1.4, 4, V(1, 0, 0), 1.2);
    geos.push(sweep(q, taper(q.length, size * 0.3, 0), { sides: 6, tip: true, flatten: 0.7 }));
  }
  o.skinned.push({ geo: merge(geos), mat, bones: null, name: "Griffes" });
}

function addBatWing(o, S0, side, span, idx, parentBone, armMat) {
  const E = S0.clone().add(V(side * span * 0.32, span * 0.28, -span * 0.05));
  const W = E.clone().add(V(side * span * 0.3, span * 0.06, span * 0.08));
  const dirs = [V(side * 0.75, 0.25, 0.55), V(side * 0.9, -0.05, 0), V(side * 0.75, -0.25, -0.45), V(side * 0.4, -0.45, -0.75)];
  const lens = [0.62, 0.58, 0.5, 0.42].map((x) => x * span);
  const fingers = dirs.map((d, i) => curvePoints(W, d, lens[i], 5, V(0, 1, 0), 0.2 * side));
  const hipT = S0.clone().add(V(side * span * 0.05, -span * 0.12, -span * 0.65));
  const hip = curvePoints(W, hipT.clone().sub(W), hipT.distanceTo(W), 5);
  const arm = curvePoints(W, S0.clone().sub(W), S0.distanceTo(W), 5);
  const mem = fanMembrane(W, [...fingers, hip, arm].map((r) => r.slice(1)), span * 0.08);
  const sn = side > 0 ? "G" : "D", sk = side > 0 ? "L" : "R";
  const b1 = o.bone(`aile_${sn}${idx}_1`, S0, E, parentBone, `wing_${sk}_1`, span * 0.045, span * 0.035, false);
  const b2 = o.bone(`aile_${sn}${idx}_2`, E, W, b1, `wing_${sk}_2`, span * 0.035, span * 0.03, false);
  const b3 = o.bone(`aile_${sn}${idx}_3`, W, fingers[1].at(-1), b2, `wing_${sk}_3`, span * 0.02, span * 0.005, false);
  const base = c3(o.spec.colors?.wings || o.spec.colors?.secondary, null) || c3(o.spec.colors?.primary, [0.15, 0.3, 0.12]).map((c) => Math.min(1, c * 1.4 + 0.05));
  const mm = o.mat("membrane", "organic", { color: base, color2: base.map((c) => c * 0.5), veins: base.map((c) => c * 0.4), doubleSide: true });
  o.skinned.push({ geo: mem, mat: mm, bones: [b1, b2, b3], name: `Membrane ${sn}${idx}` });
  const tubes = [sweep([S0, E, W], [span * 0.045, span * 0.035, span * 0.03], { sides: 8 })];
  for (const f of fingers) tubes.push(sweep(f, taper(f.length, span * 0.022, span * 0.004), { sides: 6 }));
  o.skinned.push({ geo: merge(tubes), mat: armMat, bones: [b1, b2, b3], name: `Aile ${sn}${idx}` });
}

function addFeatherWing(o, S0, side, span, idx, parentBone, mat) {
  const E = S0.clone().add(V(side * span * 0.35, span * 0.12, -span * 0.05));
  const W = E.clone().add(V(side * span * 0.3, 0, span * 0.02));
  const Tp = W.clone().add(V(side * span * 0.35, -span * 0.05, -span * 0.1));
  const geos = [sweep([S0, E, W, Tp], [span * 0.06, span * 0.05, span * 0.04, span * 0.02], { sides: 8 })];
  for (let k = 0; k < 14; k++) {
    const t = k / 13;
    const b = t < 0.33 ? S0.clone().lerp(E, t / 0.33) : t < 0.66 ? E.clone().lerp(W, (t - 0.33) / 0.33) : W.clone().lerp(Tp, (t - 0.66) / 0.34);
    const d = V(side * (0.25 + 0.6 * t), -0.08, -(1 - 0.5 * t)).normalize();
    const tip = b.clone().addScaledVector(d, span * (0.28 + 0.25 * t));
    const w = span * 0.06;
    geos.push(sweep([b, b.clone().lerp(tip, 0.5), tip], [w * 0.6, w, w * 0.2], { profile: [[-1, 0], [-0.5, 0.15], [0.5, 0.15], [1, 0], [0.5, -0.1], [-0.5, -0.1]] }));
  }
  const sn = side > 0 ? "G" : "D", sk = side > 0 ? "L" : "R";
  const b1 = o.bone(`aile_${sn}${idx}_1`, S0, E, parentBone, `wing_${sk}_1`, span * 0.05, span * 0.04, false);
  const b2 = o.bone(`aile_${sn}${idx}_2`, E, W, b1, `wing_${sk}_2`, span * 0.04, span * 0.03, false);
  const b3 = o.bone(`aile_${sn}${idx}_3`, W, Tp, b2, `wing_${sk}_3`, span * 0.03, span * 0.01, false);
  o.skinned.push({ geo: merge(geos), mat, bones: [b1, b2, b3], name: `Aile ${sn}${idx}` });
}

function addTentacles(o, center, radius, count, length, parentBone, down = true) {
  const s = S(o.spec, "tentacles");
  for (let k = 0; k < count; k++) {
    const a = 2 * Math.PI * (k + 0.5) / count;
    const d = V(Math.cos(a), down ? -0.9 : 0.2, Math.sin(a)).normalize();
    const base = center.clone().add(V(Math.cos(a) * radius, 0, Math.sin(a) * radius));
    const pts = curvePoints(base, d, length * s, 12, V(-Math.sin(a), 0, Math.cos(a)), down ? 2.6 : -2);
    const rr = taper(pts.length, radius * 0.35 * Math.min(1.5, Math.sqrt(s)), radius * 0.04, 0.8);
    let prev = parentBone;
    for (let i = 0; i < 4; i++) {
      const i0 = i * 3, i1 = Math.min((i + 1) * 3, pts.length - 1);
      prev = o.bone(`tentacule_${k + 1}_${i + 1}`, pts[i0], pts[i1], prev, `tentacle_${k + 1}_${i + 1}`, rr[i0], rr[i1]);
    }
  }
}

// ------------------------------------------------------------------ plans de corps
const QUAD = {
  dragon: { body: 1.6, leg: 0.7, r: 0.46, neck: 0.85, up: 0.6, head: 0.26, snout: 0.3, tail: 2.4, tail_r: 0.26, leg_r: 0.15, ears: 0, color: [0.15, 0.3, 0.12], mat: "scales", sprawl: 0.12 },
  beast: { body: 1.4, leg: 0.8, r: 0.38, neck: 0.45, up: 0.35, head: 0.24, snout: 0.2, tail: 1, tail_r: 0.12, leg_r: 0.13, ears: 0, color: [0.3, 0.12, 0.1], mat: "organic", sprawl: 0.05 },
  hydra: { body: 1.5, leg: 0.6, r: 0.42, neck: 1.1, up: 0.7, head: 0.17, snout: 0.25, tail: 1.8, tail_r: 0.22, leg_r: 0.13, ears: 0, color: [0.1, 0.32, 0.25], mat: "scales", sprawl: 0.15 },
  griffin: { body: 1.3, leg: 0.75, r: 0.33, neck: 0.4, up: 0.8, head: 0.18, snout: 0.2, tail: 1, tail_r: 0.07, leg_r: 0.09, ears: 0, color: [0.6, 0.45, 0.25], mat: "fur", sprawl: 0.03 },
  wolf: { body: 1, leg: 0.6, r: 0.2, neck: 0.3, up: 0.55, head: 0.14, snout: 0.17, tail: 0.6, tail_r: 0.07, leg_r: 0.055, ears: 2, color: [0.42, 0.42, 0.44], mat: "fur", sprawl: 0 },
  cat: { body: 0.95, leg: 0.5, r: 0.18, neck: 0.22, up: 0.6, head: 0.13, snout: 0.06, tail: 0.85, tail_r: 0.05, leg_r: 0.05, ears: 2, color: [0.85, 0.45, 0.12], mat: "fur", sprawl: 0 },
  horse: { body: 1.4, leg: 1.1, r: 0.3, neck: 0.65, up: 1, head: 0.15, snout: 0.38, tail: 0.7, tail_r: 0.07, leg_r: 0.065, ears: 2, color: [0.4, 0.22, 0.1], mat: "fur", sprawl: 0 },
  bear: { body: 1.2, leg: 0.55, r: 0.38, neck: 0.2, up: 0.3, head: 0.2, snout: 0.14, tail: 0.12, tail_r: 0.06, leg_r: 0.13, ears: 2, color: [0.3, 0.18, 0.09], mat: "fur", sprawl: 0.02 },
  deer: { body: 1.1, leg: 0.95, r: 0.22, neck: 0.55, up: 1.1, head: 0.12, snout: 0.2, tail: 0.15, tail_r: 0.04, leg_r: 0.045, ears: 2, color: [0.55, 0.36, 0.2], mat: "fur", sprawl: 0 },
  cow: { body: 1.5, leg: 0.75, r: 0.4, neck: 0.35, up: 0.4, head: 0.17, snout: 0.22, tail: 0.7, tail_r: 0.04, leg_r: 0.09, ears: 2, color: [0.9, 0.88, 0.85], mat: "fur", sprawl: 0 },
  rabbit: { body: 0.55, leg: 0.22, r: 0.17, neck: 0.08, up: 0.7, head: 0.12, snout: 0.05, tail: 0.06, tail_r: 0.05, leg_r: 0.05, ears: 2, color: [0.65, 0.55, 0.45], mat: "fur", sprawl: 0, longEars: true },
  lizard: { body: 0.9, leg: 0.25, r: 0.15, neck: 0.15, up: 0.1, head: 0.11, snout: 0.15, tail: 1.4, tail_r: 0.09, leg_r: 0.045, ears: 0, color: [0.25, 0.45, 0.15], mat: "scales", sprawl: 0.3 },
};

function quadruped(o, preset) {
  const spec = o.spec, f = spec.features || {};
  const P = { ...QUAD[preset] };
  if (spec.build === "heavy") { P.r *= 1.3; P.leg_r *= 1.3; } else if (spec.build === "thin") { P.r *= 0.8; P.leg_r *= 0.8; }
  if (spec.style === "cartoon") { P.head *= 1.6; P.leg *= 0.8; }
  const L = P.body * S(spec, "body"), H = P.leg * S(spec, "legs"), R = P.r * S(spec, "body");
  const y = H + R * 0.55;
  const sp = [V(0, y, -L / 2), V(0, y + R * 0.08, -L / 6), V(0, y + R * 0.12, L / 6), V(0, y + R * 0.15, L / 2)];
  o.bone("racine", V(0, 0, 0), V(0, R * 0.5, 0), null, "root");
  o.bone("bassin", sp[0], sp[1], "racine", "spine", R * 0.82, R * 0.97);
  o.bone("colonne", sp[1], sp[2], "bassin", "spine", R * 0.97, R * 1.02);
  o.bone("torse", sp[2], sp[3], "colonne", "chest", R * 1.02, R * 0.93);
  const heads = Math.max(1, +f.heads || 1);
  const neckLen = P.neck * S(spec, "neck");
  const hr = P.head * S(spec, "heads") * (heads === 1 ? 1 : 0.85);
  const headInfo = [];
  for (let h = 0; h < heads; h++) {
    const spread = heads === 1 ? 0 : (h / (heads - 1) - 0.5) * 1.6;
    const dir = V(spread * 0.6, P.up, 1).normalize();
    const pts = curvePoints(sp[3], dir, neckLen, neckLen > 0.5 ? 4 : 2, V(1, 0, 0), neckLen > 0.6 ? 0.26 : 0);
    const end = pts.at(-1);
    const suf = heads === 1 ? "" : `_${h + 1}`;
    const nb = o.bone(`cou${suf}`, sp[3], end, "torse", "neck", Math.max(R * 0.68, hr * 0.75), Math.max(R * 0.4, hr * 0.8));
    const hc = end.clone().add(V(0, hr * 0.15, hr * 0.6));
    const sd = V(spread * 0.2, -0.15, 1).normalize();
    const snout = hc.clone().addScaledVector(sd, hr * 0.5 + P.snout);
    const hb = o.bone(`tete${suf}`, end, snout, nb, "head");
    o.cap(end, hc, hr * 0.8, hr, hb);
    o.cap(hc, snout, hr * 0.95, hr * 0.5, hb);
    headInfo.push({ hc, hr, sd, hb, snout });
  }
  const tails = f.tails ?? (P.tail > 0.05 ? 1 : 0);
  const tailPts = [];
  for (let t = 0; t < tails; t++) {
    const spread = tails === 1 ? 0 : (t / (tails - 1) - 0.5);
    const tl = P.tail * S(spec, "tails");
    const nseg = P.tail > 1 ? 8 : 5;
    const pts = curvePoints(sp[0], V(spread, -0.25, -1), tl, nseg, V(1, 0, 0), -0.45);
    const rr = pts.map((_, i) => i === 0 ? Math.max(R * 0.6, P.tail_r) : P.tail_r * Math.pow(1 - i / nseg, 0.8) + 0.012);
    const nb = 4, step = Math.max(1, Math.floor(nseg / nb));
    let prev = "bassin";
    for (let i = 0; i < nb; i++) {
      const i0 = i * step, i1 = i === nb - 1 ? nseg : (i + 1) * step;
      prev = o.bone(`queue${tails === 1 ? "" : t + 1}_${i + 1}`, pts[i0], pts[i1], prev, `tail_${i + 1}`, rr[i0], rr[i1]);
    }
    tailPts.push([pts, rr]);
  }
  const nLegs = Math.max(0, Math.min(12, (+f.legs || (f.legs === 0 ? 0 : 4)) + ((+f.legs || 0) % 2)));
  const pairs = nLegs / 2;
  const feet = [];
  const lr = P.leg_r;
  for (let pi = 0; pi < pairs; pi++) {
    const t = pairs === 1 ? 0 : pi / (pairs - 1);
    const z = pairs > 1 ? L / 2 * 0.85 - t * L * 0.85 : 0;
    const front = pairs > 1 ? t < 0.5 : true;
    for (const side of [1, -1]) {
      const hip = V(side * R * 0.55, y - R * 0.35, z);
      const knee = V(side * (R * 0.62 + P.sprawl), H * 0.52, z + (front ? -0.05 : 0.12) * H);
      const ankle = V(side * (R * 0.6 + P.sprawl * 1.2), H * 0.12, z + (front ? 0 : -0.1) * H);
      const foot = V(side * (R * 0.6 + P.sprawl * 1.2), lr * 0.6, z + 0.14 * H + lr);
      const sn = side > 0 ? "G" : "D", sk = side > 0 ? "L" : "R";
      const fb = pairs === 2 ? (front ? "front" : "back") : (pi % 2 === 0 ? "front" : "back");
      const lab = `${fb === "front" ? "av" : "ar"}${pairs <= 2 ? "" : pi + 1}`;
      const par = front ? "torse" : "bassin";
      const b1 = o.bone(`patte_${lab}_${sn}`, hip, knee, par, `leg_${sk}_${fb}_upper`, lr * 2.3, lr * 1.25);
      const b2 = o.bone(`jarret_${lab}_${sn}`, knee, ankle, b1, `leg_${sk}_${fb}_lower`, lr * 1.2, lr * 0.8);
      o.bone(`pied_${lab}_${sn}`, ankle, foot, b2, `leg_${sk}_${fb}_foot`, lr * 0.8, lr * 0.7);
      feet.push(foot);
    }
  }
  const [preset2, mp] = bodyMaterialParams(spec, P.mat, c3(null, P.color));
  o.bodyPreset = preset2; o.bodyParams = mp;
  for (const { hc, hr: r, sd, hb, snout } of headInfo) {
    addEyes(o, hc, r, sd, f.eyes ?? 2, hb, ["cat", "rabbit"].includes(preset) || spec.style === "cartoon" ? 0.24 : 0.2, 0.6, 0.35);
    if (f.horns) addHorns(o, hc, r, +f.horns, hb, spec.subtype === "demon" ? "ram" : "curved");
    else if (preset === "deer" && f.horns === undefined) addHorns(o, hc, r, 2, hb, "curved");
    if (P.ears) for (const side of [1, -1]) {
      const tall = P.longEars ? 2.6 : 1;
      const b = hc.clone().add(V(side * r * 0.55, r * 0.7, -r * 0.15));
      o.cap(b, b.clone().add(V(side * r * 0.2, r * 0.85 * tall, -r * 0.15)), r * 0.28, r * 0.06, hb);
    }
    if (f.fangs || ["dragon", "beast", "hydra"].includes(preset) || spec.category === "creature") addTeeth(o, snout.clone().add(V(0, -r * 0.35, -r * 0.15)), r * 0.35, r * 0.12, hb);
  }
  if (f.spikes) {
    const pts = [], rad = [];
    for (let i = 0; i < sp.length - 1; i++) for (const t of [0.25, 0.75]) { pts.push(sp[i].clone().lerp(sp[i + 1], t)); rad.push(R * 0.55); }
    for (const [tp, tr] of tailPts) for (let i = 1; i < tp.length - 1; i++) { pts.push(tp[i]); rad.push(Math.max(tr[i], R * 0.15) * 0.9); }
    addSpikes(o, pts, rad);
  }
  if (f.claws || ["dragon", "beast", "bear", "hydra", "griffin"].includes(preset)) addClaws(o, feet, lr * 0.5);
  const nw = +f.wings || 0;
  if (nw) {
    const span = L * 1.4 * S(spec, "wings");
    for (let k = 0; k < Math.ceil(nw / 2); k++) for (const side of [1, -1]) {
      const sh = V(side * R * 0.5, y + R * 0.75, L * 0.3 - k * L * 0.45);
      if (preset === "griffin" || f.feathers) addFeatherWing(o, sh, side, span * (1 - 0.3 * k), k + 1, "torse", o.mat("plumes", "fur", { color: c3(spec.colors?.wings, [0.55, 0.42, 0.28]) }));
      else addBatWing(o, sh, side, span * (1 - 0.3 * k), k + 1, "torse", o.mat("os_ailes", o.bodyPreset, o.bodyParams));
    }
  }
  if (+f.tentacles) addTentacles(o, V(0, y - R * 0.6, L * 0.1), R * 0.45, +f.tentacles, H * 1.1, "torse");
  if (f.shell) o.rigid.push({ geo: place(ellipsoid(R * 1.4, R * 0.9, L * 0.65, 24, 12), { pos: V(0, y + R * 0.3, 0) }), mat: o.mat("carapace", "stone", { color: [0.3, 0.35, 0.15] }), bone: "colonne", name: "Carapace" });
}

const TRAITS = {
  orc: { color: [0.3, 0.45, 0.2], build: "heavy", tusks: true, ears: "pointed" }, orque: { color: [0.3, 0.45, 0.2], build: "heavy", tusks: true, ears: "pointed" },
  troll: { color: [0.35, 0.42, 0.35], build: "heavy", tusks: true }, ogre: { color: [0.55, 0.5, 0.35], build: "heavy", tusks: true },
  gobelin: { color: [0.35, 0.55, 0.2], build: "thin", ears: "pointed", head: 1.3 }, goblin: { color: [0.35, 0.55, 0.2], build: "thin", ears: "pointed", head: 1.3 },
  elfe: { build: "thin", ears: "pointed" }, elf: { build: "thin", ears: "pointed" }, nain: { build: "heavy", beard: true, head: 1.15 }, dwarf: { build: "heavy", beard: true, head: 1.15 },
  zombie: { color: [0.45, 0.55, 0.4], build: "thin" }, squelette: { color: [0.88, 0.85, 0.75], build: "thin" }, skeleton: { color: [0.88, 0.85, 0.75], build: "thin" },
  sorcier: { robe: true, hat: true, beard: true }, wizard: { robe: true, hat: true, beard: true }, mage: { robe: true, hat: true }, sorciere: { robe: true, hat: true }, witch: { robe: true, hat: true },
  chevalier: { armor: true }, knight: { armor: true }, paladin: { armor: true }, soldat: { armor: true }, soldier: { armor: true },
  guerrier: { armor: true, build: "heavy" }, warrior: { armor: true, build: "heavy" }, viking: { armor: true, beard: true, build: "heavy" },
  robot: { robot: true }, androide: { robot: true }, android: { robot: true }, cyborg: { robot: true }, astronaute: { robot: true },
  demon: { color: [0.55, 0.06, 0.04], build: "heavy", tail: true }, golem: { color: [0.45, 0.43, 0.4], build: "heavy", material: "stone", robot: true, blocky: true },
  alien: { color: [0.4, 0.6, 0.5], build: "thin", head: 1.5, material: "organic" }, enfant: { head: 1.25 }, child: { head: 1.25 },
  vampire: { color: [0.85, 0.82, 0.8], build: "thin" }, momie: { color: [0.75, 0.68, 0.5], material: "fabric" },
};

function humanoid(o) {
  const spec = o.spec, f = spec.features || {};
  const tr = { ...(TRAITS[spec.keyword] || TRAITS[spec.subtype] || {}) };
  if (tr.robot) return robot(o, tr);
  const build = spec.build || tr.build || "normal";
  const cartoon = spec.style === "cartoon";
  const H = 1.8;
  const hr = H * (cartoon ? 1 / 4 : 1 / 7.5) * (tr.head || 1) * S(spec, "heads") * 0.5;
  const w = { thin: 0.85, normal: 1, heavy: 1.3 }[build] * S(spec, "body");
  const arm = S(spec, "arms");
  const hip = H * 0.53;
  const pel = V(0, hip, 0), spine = V(0, H * 0.64, -0.01), chest = V(0, H * 0.74, 0), neck = V(0, H * 0.84, -0.01);
  const hc = V(0, H * 0.84 + hr * 1.15, 0.01), top = hc.clone().add(V(0, hr * 0.55, 0));
  o.bone("racine", V(0, 0, 0), V(0, 0.12, 0), null, "root");
  o.bone("bassin", pel, spine, "racine", "spine", 0.15 * w, 0.14 * w);
  o.bone("torse", spine, chest, "bassin", "chest", 0.14 * w, 0.19 * w);
  o.bone("cou", chest, neck, "torse", "neck", 0.15 * w, 0.06 * w);
  o.bone("tete", neck, top, "cou", "head");
  o.cap(neck, hc, 0.06 * w, hr * 0.9, "tete");
  o.cap(hc, top, hr * 0.9, hr * 0.6, "tete");
  o.cap(hc.clone().add(V(0, -hr * 0.1, hr * 0.85)), hc.clone().add(V(0, -hr * 0.2, hr * 0.95)), hr * 0.14, hr * 0.1, "tete");
  const hands = [];
  const pairs = Math.max(1, Math.floor((+f.arms || 2) / 2));
  for (let k = 0; k < pairs; k++) {
    const dy = -k * 0.09 * H;
    for (const side of [1, -1]) {
      const sh = V(side * 0.21 * w, H * 0.81 + dy, -0.01);
      const el = V(side * (0.22 * w + 0.16 * arm), H * 0.81 + dy - 0.27 * arm, -0.03);
      const wr = V(side * (0.24 * w + 0.19 * arm), H * 0.81 + dy - 0.52 * arm, 0.04);
      const hd = wr.clone().add(V(side * 0.02, -0.09, 0.03));
      const sn = side > 0 ? "G" : "D", sk = side > 0 ? "L" : "R", suf = k ? `_${k + 1}` : "";
      const b0 = o.bone(`epaule_${sn}${suf}`, chest.clone().add(V(side * 0.05, 0.05 + dy, 0)), sh, "torse", `shoulder_${sk}`, 0.07 * w, 0.065 * w);
      const b1 = o.bone(`bras_${sn}${suf}`, sh, el, b0, `arm_${sk}_upper`, 0.062 * w, 0.045 * w);
      const b2 = o.bone(`avantbras_${sn}${suf}`, el, wr, b1, `arm_${sk}_lower`, 0.045 * w, 0.032 * w);
      o.bone(`main_${sn}${suf}`, wr, hd, b2, `arm_${sk}_hand`, 0.04, 0.03);
      hands.push(hd);
    }
  }
  for (const side of [1, -1]) {
    const hp = V(side * 0.1 * w, hip - 0.04, 0), kn = V(side * 0.11 * w, hip * 0.52, 0.02), an = V(side * 0.11 * w, 0.08, -0.02), toe = V(side * 0.11 * w, 0.03, 0.17);
    const sn = side > 0 ? "G" : "D", sk = side > 0 ? "L" : "R";
    const b1 = o.bone(`cuisse_${sn}`, hp, kn, "bassin", `leg_${sk}_upper`, 0.095 * w, 0.06 * w);
    const b2 = o.bone(`tibia_${sn}`, kn, an, b1, `leg_${sk}_lower`, 0.06 * w, 0.045 * w);
    o.bone(`pied_${sn}`, an, toe, b2, `leg_${sk}_foot`, 0.05, 0.035);
  }
  const bodyMat = ["metal", "gold", "rust"].includes(spec.material) ? null : spec.material;
  const preset = bodyMat || tr.material || (spec.category === "creature" ? "organic" : "skin");
  const [p2, mp] = bodyMaterialParams({ ...spec, material: f.scales ? "scales" : preset }, preset, tr.color || [0.82, 0.6, 0.48]);
  o.bodyPreset = p2; o.bodyParams = mp;
  addEyes(o, hc, hr, V(0, 0, 1), f.eyes ?? 2, "tete", cartoon ? 0.24 : 0.17, 0.42, 0.12);
  for (const side of [1, -1]) {
    const b = hc.clone().add(V(side * hr * 0.86, -hr * 0.05, -0.02));
    if (tr.ears === "pointed") o.cap(b, b.clone().add(V(side * hr * 0.55, hr * 0.7, -hr * 0.3)), hr * 0.18, hr * 0.03, "tete");
    else o.cap(b, b.clone().add(V(side * hr * 0.1, hr * 0.08, -hr * 0.05)), hr * 0.16, hr * 0.12, "tete");
  }
  const hairC = c3(spec.colors?.secondary, [0.12, 0.07, 0.04]);
  if (spec.category === "character" && !["squelette", "skeleton", "zombie", "alien"].includes(spec.keyword) && !tr.hat && !tr.armor) {
    const g = ellipsoid(hr * 1.06, hr * 0.9, hr * 1.1, 20, 12, 0);
    cutBelow(g, 0);
    o.rigid.push({ geo: place(g, { pos: hc.clone().add(V(0, hr * 0.18, -hr * 0.1)) }), mat: o.mat("cheveux", "fur", { color: hairC }), bone: "tete", name: "Cheveux" });
  }
  if (tr.beard) o.rigid.push({ geo: sweep([hc.clone().add(V(0, -hr * 0.45, hr * 0.6)), hc.clone().add(V(0, -hr, hr * 0.75)), hc.clone().add(V(0, -hr * 1.7, hr * 0.6))], [hr * 0.55, hr * 0.45, 0], { sides: 10, tip: true, flatten: 0.6 }), mat: o.mat("barbe", "fur", { color: c3(spec.colors?.secondary, [0.55, 0.52, 0.5]) }), bone: "tete", name: "Barbe" });
  if (tr.tusks) for (const side of [1, -1]) {
    const b = hc.clone().add(V(side * hr * 0.3, -hr * 0.5, hr * 0.75));
    o.rigid.push({ geo: sweep([b, b.clone().add(V(side * hr * 0.05, hr * 0.3, hr * 0.1))], [hr * 0.08, 0], { sides: 6, tip: true }), mat: o.mat("dents", "plain", { color: [0.95, 0.92, 0.82] }), bone: "tete", name: "Défense" });
  }
  if (tr.robe) o.skinned.push({ geo: lathe([[0.001, 0.02], [0.36 * w, 0.02], [0.3 * w, H * 0.3], [0.22 * w, H * 0.55], [0.19 * w, H * 0.72], [0.16 * w, H * 0.8], [0.07, H * 0.84]], 24), mat: o.mat("robe", "fabric", { color: c3(spec.colors?.secondary || spec.colors?.primary, [0.15, 0.15, 0.45]) }), bones: ["bassin", "torse"], name: "Robe" });
  if (tr.hat) o.rigid.push({ geo: place(lathe([[0.001, 0], [hr * 2, 0], [hr * 1.9, hr * 0.08], [hr * 0.95, hr * 0.15], [hr * 0.6, hr * 1.2], [0.001, hr * 2.4]], 24), { pos: hc.clone().add(V(0, hr * 0.45, 0)) }), mat: o.mat("chapeau", "fabric", { color: c3(spec.colors?.secondary, [0.15, 0.15, 0.45]) }), bone: "tete", name: "Chapeau" });
  if (tr.armor || spec.material === "metal") {
    const metal = o.mat("armure", "metal", { color: c3(spec.colors?.secondary, [0.7, 0.72, 0.75]) });
    const plate = ellipsoid(0.24 * w, 0.22, 0.22 * w, 20, 12); cutBelow(plate, -0.12);
    o.rigid.push({ geo: place(plate, { pos: V(0, H * 0.72, 0.01) }), mat: metal, bone: "torse", name: "Plastron" });
    for (const side of [1, -1]) {
      const pad = ellipsoid(0.11 * w, 0.07, 0.11 * w, 14, 8); cutBelow(pad, -0.01);
      o.rigid.push({ geo: place(pad, { pos: V(side * 0.24 * w, H * 0.81, -0.01) }), mat: metal, bone: "torse", name: "Épaulière" });
    }
    const helm = ellipsoid(hr * 1.12, hr * 1.08, hr * 1.15, 20, 12); cutBelow(helm, -hr * 0.3);
    o.rigid.push({ geo: place(helm, { pos: hc.clone().add(V(0, hr * 0.12, -0.01)) }), mat: metal, bone: "tete", name: "Casque" });
  }
  if (f.horns) addHorns(o, hc, hr, +f.horns, "tete", spec.subtype === "demon" || spec.keyword === "demon" ? "ram" : "curved");
  if (+f.wings) for (let k = 0; k < Math.ceil(+f.wings / 2); k++) for (const side of [1, -1]) addBatWing(o, V(side * 0.12 * w, H * (0.76 - 0.06 * k), -0.12 - 0.05 * k), side, H * 0.9 * S(spec, "wings") * (1 - 0.15 * k), k + 1, "torse", o.mat("os_ailes", o.bodyPreset, o.bodyParams));
  const tails = +(f.tails ?? (tr.tail ? 1 : 0)) || 0;
  for (let t = 0; t < tails; t++) {
    const pts = curvePoints(pel.clone().add(V(0, -0.02, -0.08)), V(tails === 1 ? 0 : t / (tails - 1) - 0.5, -0.6, -1), H * 0.6 * S(spec, "tails"), 8, V(1, 0, 0), -1.2);
    const rr = taper(pts.length, 0.05 * w, 0.01);
    let prev = "bassin";
    for (let i = 0; i < 3; i++) prev = o.bone(`queue${tails === 1 ? "" : t + 1}_${i + 1}`, pts[i * 3 ? i * 3 : 0], pts[Math.min((i + 1) * 3, 8)], prev, `tail_${i + 1}`, rr[i * 3], rr[Math.min((i + 1) * 3, 8)]);
  }
  if (+f.tentacles) addTentacles(o, V(0, hip - 0.05, 0), 0.12 * w, +f.tentacles, H * 0.5, "bassin");
  if (f.claws || spec.category === "creature") addClaws(o, hands, 0.02);
}

function cutBelow(g, y) {
  const p = g.attributes.position;
  for (let i = 0; i < p.count; i++) if (p.getY(i) < y) p.setY(i, y);
  g.computeVertexNormals();
}

function robot(o, tr) {
  const spec = o.spec;
  const H = 1.9, w = tr.blocky || spec.build === "heavy" ? 1.3 : 1;
  const c = c3(spec.colors?.primary, tr.color || [0.7, 0.72, 0.75]);
  const mat = o.mat("coque", tr.blocky ? "stone" : spec.material || "metal", { color: c });
  const joint = o.mat("articulations", "metal", { color: [0.15, 0.15, 0.17], roughness: 0.4 });
  const r = tr.blocky ? 0.06 : 0.02;
  const part = (name, s, p, bone, m = mat) => o.rigid.push({ geo: place(box(s[0], s[1], s[2], r), { pos: p }), mat: m, bone, name });
  o.bone("racine", V(0, 0, 0), V(0, 0.1, 0), null, "root");
  o.bone("bassin", V(0, H * 0.5, 0), V(0, H * 0.6, 0), "racine", "spine");
  o.bone("torse", V(0, H * 0.6, 0), V(0, H * 0.8, 0), "bassin", "chest");
  o.bone("tete", V(0, H * 0.84, 0), V(0, H * 0.98, 0), "torse", "head");
  part("Bassin", [0.36 * w, 0.2, 0.22 * w], V(0, H * 0.53, 0), "bassin");
  part("Torse", [0.5 * w, 0.36, 0.3 * w], V(0, H * 0.7, 0), "torse");
  part("Tête", [0.24, 0.22, 0.24], V(0, H * 0.91, 0), "tete");
  part("Cou", [0.08, 0.08, 0.08], V(0, H * 0.84, 0), "tete", joint);
  const ec = c3(spec.colors?.eyes, [0.2, 0.8, 1]);
  o.rigid.push({ geo: place(box(0.18, 0.04, 0.02, 0.008), { pos: V(0, H * 0.92, 0.125) }), mat: o.mat("visiere", "emission", { color: ec }), bone: "tete", name: "Visière" });
  for (const side of [1, -1]) {
    const sn = side > 0 ? "G" : "D", sk = side > 0 ? "L" : "R", x = side * 0.33 * w, lx = side * 0.12 * w;
    o.bone(`bras_${sn}`, V(x, H * 0.82, 0), V(x + side * 0.05, H * 0.6, 0), "torse", `arm_${sk}_upper`);
    o.bone(`avantbras_${sn}`, V(x + side * 0.05, H * 0.6, 0), V(x + side * 0.07, H * 0.4, 0.02), `bras_${sn}`, `arm_${sk}_lower`);
    part(`Épaule ${sn}`, [0.16 * w, 0.14 * w, 0.18 * w], V(x, H * 0.82, 0), `bras_${sn}`);
    part(`Bras ${sn}`, [0.1 * w, 0.25, 0.1 * w], V(x + side * 0.025, H * 0.71, 0), `bras_${sn}`);
    part(`Avant-bras ${sn}`, [0.12 * w, 0.26, 0.12 * w], V(x + side * 0.06, H * 0.5, 0.01), `avantbras_${sn}`);
    part(`Main ${sn}`, [0.09 * w, 0.1, 0.1 * w], V(x + side * 0.07, H * 0.37, 0.02), `avantbras_${sn}`, joint);
    o.bone(`cuisse_${sn}`, V(lx, H * 0.48, 0), V(lx, H * 0.26, 0), "bassin", `leg_${sk}_upper`);
    o.bone(`tibia_${sn}`, V(lx, H * 0.26, 0), V(lx, H * 0.05, 0), `cuisse_${sn}`, `leg_${sk}_lower`);
    part(`Cuisse ${sn}`, [0.15 * w, 0.27, 0.16 * w], V(lx, H * 0.37, 0), `cuisse_${sn}`);
    part(`Genou ${sn}`, [0.1, 0.06, 0.1], V(lx, H * 0.26, 0.01), `tibia_${sn}`, joint);
    part(`Tibia ${sn}`, [0.14 * w, 0.3, 0.15 * w], V(lx, H * 0.15, 0), `tibia_${sn}`);
    part(`Pied ${sn}`, [0.15 * w, 0.07, 0.28], V(lx, 0.035, 0.06), `tibia_${sn}`);
  }
  o.noBody = true;
}

function serpent(o) {
  const spec = o.spec, f = spec.features || {};
  const L = 4 * S(spec, "body"), R = 0.12 * S(spec, "body") * (spec.build === "heavy" ? 1.4 : 1);
  const n = 24;
  const pts = Array.from({ length: n + 1 }, (_, i) => V(Math.sin(i / n * Math.PI * 3) * 0.45 * (i / n), R * 0.9, L / 2 - L * i / n));
  const rad = pts.map((_, i) => i > 2 ? R * (1 - 0.92 * Math.max(0, (i - n * 0.55) / (n * 0.45))) : R * (0.8 + 0.1 * i));
  const hc = pts[0].clone().add(V(0, R * 0.3, R * 1.2)), snout = hc.clone().add(V(0, -R * 0.1, R * 1.4));
  o.bone("racine", V(0, 0, 0), V(0, R, 0), null, "root");
  o.bone("tete", pts[0], snout, "racine", "head");
  o.cap(pts[0], hc, rad[0], R * 1.3, "tete");
  o.cap(hc, snout, R * 1.25, R * 0.55, "tete");
  let prev = "racine";
  for (let i = 0; i < 8; i++) prev = o.bone(`corps_${i + 1}`, pts[i * 3], pts[(i + 1) * 3], prev, `tail_${i + 1}`, rad[i * 3], rad[(i + 1) * 3]);
  const [p, mp] = bodyMaterialParams(spec, "scales", [0.2, 0.35, 0.1]);
  o.bodyPreset = p; o.bodyParams = mp;
  addEyes(o, hc, R * 1.2, V(0, 0, 1), f.eyes ?? 2, "tete", 0.22, 0.7, 0.4);
  if (f.horns) addHorns(o, hc, R * 1.2, +f.horns, "tete");
  if (f.spikes) addSpikes(o, pts.filter((_, i) => i % 2 && i < n - 3), rad.filter((_, i) => i % 2 && i < n - 3));
}

function octopus(o) {
  const spec = o.spec, f = spec.features || {}, s = S(spec, "body");
  const c = V(0, 1.4 * s, 0), top = V(0, 2.3 * s, -0.35 * s);
  o.bone("racine", V(0, 0, 0), V(0, 0.3, 0), null, "root");
  o.bone("tete", c, top, "racine", "head", 0.6 * s, 0.42 * s);
  const [p, mp] = bodyMaterialParams(spec, "organic", [0.5, 0.12, 0.18]);
  o.bodyPreset = p; o.bodyParams = mp;
  addTentacles(o, c.clone().add(V(0, -0.35 * s, 0)), 0.4 * s, +f.tentacles || 8, 2 * s, "tete");
  addEyes(o, c.clone().add(V(0, 0.15 * s, 0)), 0.58 * s, V(0, 0, 1), f.eyes ?? 2, "tete", 0.25, 0.7, 0.2);
}

function blob(o) {
  const spec = o.spec, f = spec.features || {}, s = S(spec, "body");
  o.bone("racine", V(0, 0, 0), V(0, 0.3, 0), null, "root");
  o.bone("corps", V(0, 0.45 * s, 0), V(0, 0.9 * s, 0), "racine", "chest", 0.75 * s, 0.6 * s);
  o.bone("tete", V(0, 0.9 * s, 0), V(0, 1.1 * s, 0.2 * s), "corps", "head");
  for (let i = 0; i < 6; i++) { const a = i * 1.05; o.cap(V(Math.cos(a) * 0.4 * s, 0.25 * s, Math.sin(a) * 0.4 * s), V(Math.cos(a) * 0.55 * s, 0.15 * s, Math.sin(a) * 0.55 * s), 0.32 * s, 0.22 * s, "corps"); }
  o.bodyPreset = spec.material || "organic";
  o.bodyParams = { color: c3(spec.colors?.primary, [0.3, 0.75, 0.2]), wet: 0.9 };
  addEyes(o, V(0, 0.75 * s, 0), 0.75 * s, V(0, 0.1, 1).normalize(), f.eyes ?? 2, "tete", 0.18, 0.5, 0.35);
  if (+f.tentacles) addTentacles(o, V(0, 0.6 * s, 0), 0.6 * s, +f.tentacles, 1.2 * s, "corps", false);
}

function bird(o) {
  const spec = o.spec, f = spec.features || {}, s = S(spec, "body");
  const tb = V(0, 0.75 * s, -0.35 * s), mid = V(0, 0.8 * s, -0.1 * s), ch = V(0, 0.85 * s, 0.15 * s), nk = V(0, 1.1 * s, 0.3 * s), hd = V(0, 1.25 * s, 0.38 * s);
  o.bone("racine", V(0, 0, 0), V(0, 0.3 * s, 0), null, "root");
  o.bone("corps", tb, ch, "racine", "chest");
  o.cap(tb, mid, 0.12 * s, 0.23 * s, "corps"); o.cap(mid, ch, 0.23 * s, 0.25 * s, "corps");
  o.bone("cou", ch, nk, "corps", "neck", 0.2 * s, 0.11 * s);
  o.bone("tete", nk, hd.clone().add(V(0, 0, 0.15 * s)), "cou", "head");
  o.cap(nk, hd, 0.11 * s, 0.13 * s, "tete");
  const feet = [];
  for (const side of [1, -1]) {
    const hp = V(side * 0.1 * s, 0.7 * s, -0.05 * s), kn = V(side * 0.11 * s, 0.4 * s, -0.1 * s), an = V(side * 0.1 * s, 0.08 * s, -0.02 * s);
    const sn = side > 0 ? "G" : "D", sk = side > 0 ? "L" : "R";
    const b1 = o.bone(`cuisse_${sn}`, hp, kn, "corps", `leg_${sk}_upper`, 0.08 * s, 0.035 * s);
    o.bone(`tibia_${sn}`, kn, an, b1, `leg_${sk}_lower`, 0.035 * s, 0.02 * s);
    feet.push([an, sn]);
  }
  const col = c3(spec.colors?.primary, [0.35, 0.25, 0.18]);
  o.bodyPreset = spec.material || "fur"; o.bodyParams = { color: col };
  const beakM = o.mat("bec", "plain", { color: [0.95, 0.7, 0.15], roughness: 0.3 });
  o.rigid.push({ geo: sweep([hd.clone().add(V(0, -0.01 * s, 0.09 * s)), hd.clone().add(V(0, -0.03 * s, 0.2 * s)), hd.clone().add(V(0, -0.07 * s, 0.28 * s))], [0.05 * s, 0.03 * s, 0], { sides: 8, tip: true }), mat: beakM, bone: "tete", name: "Bec" });
  addEyes(o, hd, 0.13 * s, V(0, 0, 1), 2, "tete", 0.25, 0.75, 0.25);
  const toes = [];
  for (const [an] of feet) for (const k of [-1, 0, 1]) toes.push(sweep([an, an.clone().add(V(k * 0.05 * s, -0.06 * s, 0.08 * s)), an.clone().add(V(k * 0.08 * s, -0.075 * s, 0.13 * s))], [0.015 * s, 0.012 * s, 0.004 * s], { sides: 6 }));
  o.skinned.push({ geo: merge(toes), mat: beakM, bones: null, name: "Pattes" });
  const wm = o.mat("plumes", "fur", { color: c3(spec.colors?.wings || spec.colors?.secondary, col) });
  for (const side of [1, -1]) addFeatherWing(o, ch.clone().add(V(side * 0.15 * s, 0.1 * s, -0.05 * s)), side, 1 * s * S(spec, "wings"), 1, "corps", wm);
  const tf = [];
  for (let k = 0; k < 7; k++) { const a = (k / 6 - 0.5) * 0.9; const tip = tb.clone().add(V(Math.sin(a), -0.1, -Math.cos(a)).multiplyScalar(0.4 * s)); tf.push(sweep([tb, tb.clone().lerp(tip, 0.6), tip], [0.03 * s, 0.05 * s, 0.01 * s], { profile: [[-1, 0], [0, 0.15], [1, 0], [0, -0.1]] })); }
  const qb = o.bone("queue_1", tb, tb.clone().add(V(0, -0.04 * s, -0.35 * s)), "corps", "tail_1");
  o.skinned.push({ geo: merge(tf), mat: wm, bones: [qb], name: "Queue" });
}

function fish(o) {
  const spec = o.spec, s = S(spec, "body");
  const shark = ["requin", "shark"].includes(spec.keyword);
  const L = 1.6 * s;
  const prof = [[0, 0.02], [0.08, 0.12], [0.25, 0.2], [0.45, 0.21], [0.65, 0.16], [0.82, 0.08], [0.95, 0.035]];
  o.bone("racine", V(0, 0, 0), V(0, 0.2 * s, 0), null, "root");
  o.bone("tete", V(0, 0.5 * s, L * 0.1), V(0, 0.5 * s, L / 2), "racine", "head");
  let prev = "racine";
  const zs = [L * 0.1, -L * 0.2, -L * 0.45, -L * 0.7];
  for (let i = 0; i < 3; i++) prev = o.bone(`corps_${i + 1}`, V(0, 0.5 * s, zs[i]), V(0, 0.5 * s, zs[i + 1]), prev, `tail_${i + 1}`);
  for (let i = 0; i < prof.length - 1; i++) {
    const za = L / 2 - prof[i][0] * L, zb = L / 2 - prof[i + 1][0] * L;
    const bone = za > L * 0.1 ? "tete" : za > -L * 0.2 ? "corps_1" : za > -L * 0.45 ? "corps_2" : "corps_3";
    o.cap(V(0, 0.5 * s, za), V(0, 0.5 * s, zb), Math.max(prof[i][1] * s, 0.01), Math.max(prof[i + 1][1] * s, 0.01), bone);
  }
  const [p, mp] = bodyMaterialParams(spec, shark ? "skin" : "scales", shark ? [0.5, 0.55, 0.6] : [0.85, 0.5, 0.15]);
  o.bodyPreset = p; o.bodyParams = mp;
  const fm = o.mat("nageoires", "organic", { color: c3(spec.colors?.secondary, mp.color.map((c) => Math.min(1, c * 1.2))), doubleSide: true });
  const tr = V(0, 0.5 * s, -L * 0.45);
  const fins = [fanMembrane(tr, [[tr.clone().add(V(0, 0.28 * s, -0.22 * s)), tr.clone().add(V(0, 0.4 * s, -0.32 * s))], [tr.clone().add(V(0, 0, -0.18 * s)), tr.clone().add(V(0, 0, -0.24 * s))], [tr.clone().add(V(0, -0.28 * s, -0.22 * s)), tr.clone().add(V(0, -0.4 * s, -0.32 * s))]], 0)];
  const top = V(0, 0.7 * s, L * 0.05);
  fins.push(fanMembrane(top.clone().add(V(0, 0, 0.15 * s)), [[top.clone().add(V(0, shark ? 0.3 * s : 0.15 * s, -0.02 * s))], [top.clone().add(V(0, 0, -0.15 * s))]], 0));
  o.skinned.push({ geo: merge(fins), mat: fm, bones: null, name: "Nageoires" });
  addEyes(o, V(0, 0.55 * s, L * 0.38), 0.12 * s, V(0, 0, 1), 2, "tete", 0.35, 0.9, 0.2);
  if (shark) addTeeth(o, V(0, 0.44 * s, L * 0.47), 0.05 * s, 0.015 * s, "tete");
}

function arthropod(o) {
  const spec = o.spec, f = spec.features || {}, s = S(spec, "body");
  const spider = ["araignee", "spider", "arachnide"].includes(spec.keyword) || spec.subtype === "spider_monster";
  const crab = ["crabe", "crab", "scorpion"].includes(spec.keyword);
  const nLegs = +f.legs || (spider || crab ? 8 : 6);
  const th = V(0, 0.45 * s, 0.1 * s), hd = V(0, 0.45 * s, 0.42 * s), ab = V(0, 0.55 * s, -0.45 * s);
  o.bone("racine", V(0, 0, 0), V(0, 0.2 * s, 0), null, "root");
  o.bone("corps", ab, th, "racine", "chest");
  o.bone("tete", th, hd.clone().add(V(0, 0, 0.1 * s)), "corps", "head");
  if (spider) { o.cap(th, th, 0.3 * s, 0.3 * s, "corps"); o.cap(ab.clone().add(V(0, 0, 0.1 * s)), ab.clone().add(V(0, 0, -0.1 * s)), 0.42 * s, 0.38 * s, "corps"); }
  else if (crab) o.cap(th.clone().add(V(-0.25 * s, 0, 0)), th.clone().add(V(0.25 * s, 0, 0)), 0.3 * s, 0.3 * s, "corps");
  else { o.cap(hd, hd, 0.14 * s, 0.14 * s, "tete"); o.cap(th, th, 0.18 * s, 0.18 * s, "corps"); o.cap(ab.clone().add(V(0, 0, 0.2 * s)), ab.clone().add(V(0, 0, -0.2 * s)), 0.22 * s, 0.16 * s, "corps"); }
  const col = c3(spec.colors?.primary, spider ? [0.08, 0.06, 0.05] : [0.4, 0.15, 0.08]);
  o.bodyPreset = spec.material || "plastic"; o.bodyParams = { color: col, roughness: 0.3 };
  const legGeos = [];
  const pairs = Math.max(1, Math.floor(nLegs / 2));
  const legBones = [];
  for (let i = 0; i < pairs; i++) {
    const t = i / Math.max(1, pairs - 1);
    for (const side of [1, -1]) {
      const b = th.clone().add(V(side * 0.2 * s, 0, (0.18 - 0.36 * t) * s));
      const a = (-50 + 100 * t) * Math.PI / 180;
      const out = V(side * Math.cos(a), 0, -Math.sin(a));
      const kn = b.clone().addScaledVector(out, 0.45 * s).add(V(0, 0.35 * s, 0));
      const ft = b.clone().addScaledVector(out, 0.95 * s).add(V(0, -0.45 * s, 0));
      legGeos.push(sweep([b, b.clone().lerp(kn, 0.5), kn, kn.clone().lerp(ft, 0.5), ft], [0.05 * s, 0.045 * s, 0.04 * s, 0.03 * s, 0.01 * s], { sides: 6 }));
      const sn = side > 0 ? "G" : "D", sk = side > 0 ? "L" : "R", fb = i % 2 === 0 ? "front" : "back";
      const b1 = o.bone(`patte${i + 1}_${sn}`, b, kn, "corps", `leg_${sk}_${fb}_upper`, 0.05 * s, 0.04 * s, false);
      legBones.push(b1, o.bone(`tarse${i + 1}_${sn}`, kn, ft, b1, `leg_${sk}_${fb}_lower`, 0.04 * s, 0.01 * s, false));
    }
  }
  o.skinned.push({ geo: merge(legGeos), mat: o.mat("carapace", o.bodyPreset, o.bodyParams), bones: legBones, name: "Pattes" });
  const ne = f.eyes ?? (spider ? 8 : 2);
  addEyes(o, hd, 0.16 * s, V(0, 0.2, 1).normalize(), ne, "tete", ne <= 2 ? 0.22 : 0.14, 0.5, 0.4);
}

// ------------------------------------------------------------------ assemblage organique
function assembleOrganic(o, quality) {
  const root = new THREE.Group();
  root.name = o.name;
  // échelle & pose au sol (avant skinning)
  const box3 = new THREE.Box3();
  for (const c of o.caps) {
    if (c.sub) continue;
    if (c.type === "ellipsoid") { box3.expandByPoint(V(...c.c).sub(V(...c.r))); box3.expandByPoint(V(...c.c).add(V(...c.r))); }
    else { box3.expandByPoint(V(...c.a).subScalar(c.ra)); box3.expandByPoint(V(...c.b).addScalar(c.rb)); box3.expandByPoint(V(...c.a).addScalar(c.ra)); box3.expandByPoint(V(...c.b).subScalar(c.rb)); }
  }
  for (const p of [...o.rigid, ...o.skinned]) { p.geo.computeBoundingBox(); box3.union(p.geo.boundingBox); }
  let target = o.spec.height || DEFAULT_HEIGHTS[`${o.spec.category}/${o.spec.subtype}`] || 1.8;
  if (!o.spec.height && o.spec.size_hint === "big") target *= 2.5;
  if (!o.spec.height && o.spec.size_hint === "small") target *= 0.5;
  const cur = Math.max(box3.max.y - box3.min.y, 1e-3);
  const k = o.noRescale ? 1 : target / cur;
  const shift = o.noRescale ? 0 : -box3.min.y * k;
  if (o.noRescale) target = cur;
  const tf = (v) => [v[0] * k, v[1] * k + shift, v[2] * k];
  for (const c of o.caps) {
    if (c.type === "ellipsoid") { c.c = tf(c.c); c.r = c.r.map((v) => v * k); }
    else { c.a = tf(c.a); c.b = tf(c.b); c.ra *= k; c.rb *= k; }
  }
  for (const b of o.bones) { b.head.multiplyScalar(k).y += shift; b.tail.multiplyScalar(k).y += shift; }
  const M = new THREE.Matrix4().makeScale(k, k, k).premultiply(new THREE.Matrix4().makeTranslation(0, shift, 0));
  for (const p of [...o.rigid, ...o.skinned]) { p.geo.applyMatrix4(M); p.geo.computeVertexNormals(); }
  // squelette
  const bmap = {}, bones = [];
  for (const b of o.bones) {
    const bone = new THREE.Bone();
    bone.name = b.name;
    bone.userData.role = b.role;
    bmap[b.name] = { bone, head: b.head, def: b };
    bones.push(bone);
  }
  let rootBone = null;
  for (const b of o.bones) {
    const e = bmap[b.name];
    const par = b.parent && bmap[b.parent];
    if (par) { par.bone.add(e.bone); e.bone.position.copy(b.head).sub(par.head); }
    else { e.bone.position.copy(b.head); if (!rootBone) rootBone = e.bone; else rootBone.add(e.bone); }
  }
  root.add(rootBone);
  root.updateMatrixWorld(true);
  const skeleton = new THREE.Skeleton(bones);
  const boneIndex = Object.fromEntries(bones.map((b, i) => [b.name, i]));
  const capsIdx = (filter) => o.caps.filter((c) => !filter || filter.includes(c.bone)).map((c) => ({ ...c, bone: boneIndex[c.bone] }));
  const bindSkinned = (geo, mat, filter, name) => {
    let caps = capsIdx(filter);
    if (!caps.length) caps = capsIdx(null);
    skinWeights(geo, caps);
    const m = new THREE.SkinnedMesh(geo, mat);
    m.name = name;
    root.add(m);
    m.bind(skeleton);
    m.userData.part = name;
    return m;
  };
  const res = QUALITY[quality] || QUALITY.standard;
  if (!o.noBody) {
    const fieldCaps = o.caps.filter((c) => c.field);
    const sizeY = target;
    const minR = sizeY / res * 0.9;
    for (const c of fieldCaps) if (c.type !== "ellipsoid" && !c.sub) { c.ra = Math.max(c.ra, minR); c.rb = Math.max(c.rb, minR); }
    const geo = buildOrganic(fieldCaps, { resolution: o.resolution || res, smooth: o.smooth ?? 2, blend: o.blend ?? null });
    const relief = o.spec.detail === "high" || ["scales", "stone", "bark"].includes(o.bodyPreset) ? target * 0.004 : 0;
    applyPattern(geo, o.bodyPreset, o.bodyParams, { scale: (o.patternScale || 1) * 1.6 / Math.max(target, 0.2), relief: o.relief ?? relief });
    const mat = makeMaterial(o.bodyPreset, o.bodyParams);
    mat.vertexColors = true;
    mat.color.set(0xffffff);
    mat.userData.vertexColors = true;
    bindSkinned(geo, mat, null, "Corps");
  }
  for (const p of o.skinned) bindSkinned(p.geo, p.mat, p.bones, p.name);
  for (const p of o.rigid) {
    const e = bmap[p.bone] || bmap[o.bones[0].name];
    const wp = new THREE.Vector3();
    e.bone.getWorldPosition(wp);
    p.geo.translate(-wp.x, -wp.y, -wp.z);
    const m = new THREE.Mesh(p.geo, p.mat);
    m.name = p.name;
    m.userData.part = p.name;
    e.bone.add(m);
  }
  root.userData.skeleton = skeleton;
  root.userData.rigged = true;
  return root;
}

// ------------------------------------------------------------------ surfaces dures
function mesh(geo, mat, name) { const m = new THREE.Mesh(geo, mat); m.name = name; m.userData.part = name; return m; }
const col = (spec, k, d) => c3(spec.colors?.[k], d);

function texMat(kind, c1, c2, rep, extra = {}) {
  const m = makeMaterial(extra.preset || "plain", { color: [1, 1, 1], roughness: extra.roughness ?? 0.85, metallic: extra.metallic ?? 0 });
  m.map = canvasTexture(kind, c1, c2, rep);
  m.userData.params.color = c1;
  return m;
}

function building(spec) {
  const g = new THREE.Group();
  const sub = spec.subtype;
  const wallKind = spec.material === "wood" ? "planks" : spec.material === "brick" || sub === "house" ? "brick" : "stone";
  const wc = col(spec, "primary", wallKind === "brick" ? [0.62, 0.3, 0.2] : wallKind === "planks" ? [0.5, 0.32, 0.18] : [0.58, 0.55, 0.5]);
  const walls = texMat(wallKind, wc, wc.map((c) => c * 0.75), [1, 1]);
  const roof = texMat("tiles", col(spec, "secondary", sub === "house" || sub === "shop" ? [0.5, 0.17, 0.1] : [0.25, 0.27, 0.32]), [0.15, 0.12, 0.1], [2, 2]);
  const glass = makeMaterial("glass", { color: [0.6, 0.75, 0.85] });
  const wood = texMat("planks", [0.4, 0.24, 0.12], [0.25, 0.15, 0.08], [1, 1]);
  const W_ = (geo) => { boxUV(geo, 0.6); return geo; };
  if (sub === "castle") {
    const S0 = 18;
    for (let k = 0; k < 4; k++) {
      const a = k * Math.PI / 2;
      const parts = [box(S0, 7, 1.6)];
      for (let i = 0; i < 11; i++) parts.push(place(box(0.8, 0.9, 1.6), { pos: V(-S0 / 2 + 0.8 + i * 1.6, 3.95, 0) }));
      const wl = merge(parts.map((p) => p));
      wl.translate(0, 3.5, S0 / 2); wl.rotateY(a); W_(wl);
      g.add(mesh(wl, walls, "Rempart"));
    }
    for (const [x, z] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) {
      const t = cylinder(2.4, 9, 20); t.translate(x * S0 / 2, 4.5, z * S0 / 2); W_(t);
      g.add(mesh(t, walls, "Tour"));
      const r = cylinder(2.8, 4.5, 20, 0.01); r.translate(x * S0 / 2, 11.25, z * S0 / 2); W_(r);
      g.add(mesh(r, roof, "Toit"));
    }
    const keep = box(8, 14, 8); keep.translate(0, 7, -2); W_(keep); g.add(mesh(keep, walls, "Donjon"));
    const kr = cylinder(6, 5, 4, 0.01); kr.rotateY(Math.PI / 4); kr.translate(0, 16.5, -2); W_(kr); g.add(mesh(kr, roof, "Toit du donjon"));
    const gate = box(3, 4, 0.3, 0.05); gate.translate(0, 2, S0 / 2 + 0.85); W_(gate); g.add(mesh(gate, wood, "Porte"));
    return g;
  }
  if (sub === "tower") {
    const floors = spec.floors || 5, H = floors * 3.2, r = 3;
    const t = lathe([[r * 1.15, 0], [r * 1.1, 1], [r, 1.4], [r * 0.92, H], [r * 1.1, H + 0.2], [r * 1.1, H + 1], [0.001, H + 1]], 28); W_(t);
    g.add(mesh(t, walls, "Tour"));
    const roofG = lathe([[r * 1.3, H + 1], [0.001, H + 1 + r * 2.2]], 28); W_(roofG); g.add(mesh(roofG, roof, "Toit"));
    const wins = [];
    for (let f = 1; f < floors; f++) for (let k = 0; k < 4; k++) { const a = k * Math.PI / 2 + f * 0.4; wins.push(place(box(0.9, 1.3, 0.5), { pos: V(Math.cos(a) * r * 0.93, f * 3.2 + 1.2, Math.sin(a) * r * 0.93), rot: [0, -a + Math.PI / 2, 0] })); }
    if (wins.length) g.add(mesh(merge(wins), glass, "Fenêtres"));
    return g;
  }
  if (sub === "temple") {
    const W = 12, D = 18;
    for (let i = 0; i < 3; i++) { const s = box(W + 2 - i, 0.4, D + 2 - i); s.translate(0, 0.2 + 0.4 * i, 0); W_(s); g.add(mesh(s, walls, "Marche")); }
    const cols = [];
    const colG = () => lathe([[0.6, 0], [0.6, 0.3], [0.45, 0.4], [0.4, 6], [0.55, 6.2], [0.55, 6.5], [0.001, 6.5]], 16);
    for (let i = 0; i < 6; i++) for (const z of [-D / 2 + 1, D / 2 - 1]) cols.push(colG().translate(-W / 2 + 1 + i * (W - 2) / 5, 1.2, z));
    for (let j = 1; j < 5; j++) for (const x of [-W / 2 + 1, W / 2 - 1]) cols.push(colG().translate(x, 1.2, -D / 2 + 1 + j * (D - 2) / 5));
    const cg = merge(cols); W_(cg); g.add(mesh(cg, walls, "Colonnes"));
    const ent = box(W + 1, 1, D + 1); ent.translate(0, 8.2, 0); W_(ent); g.add(mesh(ent, walls, "Entablement"));
    const pr = new THREE.BufferGeometry();
    const P = [[-W / 2 - 0.6, 8.7, -D / 2 - 0.6], [W / 2 + 0.6, 8.7, -D / 2 - 0.6], [W / 2 + 0.6, 8.7, D / 2 + 0.6], [-W / 2 - 0.6, 8.7, D / 2 + 0.6], [0, 11, -D / 2 - 0.6], [0, 11, D / 2 + 0.6]];
    pr.setAttribute("position", new THREE.Float32BufferAttribute(P.flat(), 3));
    pr.setIndex([0, 4, 1, 3, 2, 5, 1, 4, 5, 1, 5, 2, 0, 3, 5, 0, 5, 4, 0, 1, 2, 0, 2, 3]);
    pr.computeVertexNormals(); W_(pr);
    g.add(mesh(pr.toNonIndexed(), walls, "Fronton"));
    return g;
  }
  const floors = spec.floors || (sub === "modern" ? 8 : 2);
  const fh = 3, W = sub === "modern" ? 14 : 8, D = sub === "modern" ? 12 : 6.5, H = floors * fh;
  const body = box(W, H, D); body.translate(0, H / 2, 0); W_(body);
  g.add(mesh(body, sub === "modern" ? makeMaterial("plain", { color: col(spec, "primary", [0.85, 0.85, 0.82]) }) : walls, "Murs"));
  const wins = [], frames = [];
  const nW = Math.max(1, Math.floor(W / 2.4)), nD = Math.max(1, Math.floor(D / 2.6));
  const ww = sub === "modern" ? 1.4 : 1, wh = sub === "modern" ? 2 : 1.3;
  for (let f = 0; f < floors; f++) {
    const y = f * fh + fh * 0.55;
    for (let i = 0; i < nW; i++) for (const z of [D / 2, -D / 2]) {
      const x = -W / 2 + (i + 0.5) * W / nW;
      if (f === 0 && z > 0 && i === Math.floor(nW / 2)) continue;
      wins.push(place(box(ww, wh, 0.06), { pos: V(x, y, z + Math.sign(z) * 0.01) }));
      frames.push(place(box(ww + 0.16, wh + 0.16, 0.04), { pos: V(x, y, z) }));
    }
    for (let i = 0; i < nD; i++) for (const x of [W / 2, -W / 2]) {
      const z = -D / 2 + (i + 0.5) * D / nD;
      wins.push(place(box(0.06, wh, ww), { pos: V(x + Math.sign(x) * 0.01, y, z) }));
      frames.push(place(box(0.04, wh + 0.16, ww + 0.16), { pos: V(x, y, z) }));
    }
  }
  g.add(mesh(merge(frames), makeMaterial("plain", { color: [0.92, 0.9, 0.85] }), "Encadrements"));
  g.add(mesh(merge(wins), glass, "Fenêtres"));
  const door = box(1.1, 2.2, 0.1, 0.02); door.translate(0, 1.1, D / 2 + 0.05); W_(door); g.add(mesh(door, wood, "Porte"));
  if (sub === "modern") { const r = box(W + 0.4, 0.4, D + 0.4); r.translate(0, H + 0.2, 0); W_(r); g.add(mesh(r, makeMaterial("plain", { color: [0.3, 0.3, 0.32] }), "Toit")); }
  else {
    const rh = D * 0.45, o2 = 0.5;
    const shape = new THREE.Shape([new THREE.Vector2(-D / 2 - o2, 0), new THREE.Vector2(D / 2 + o2, 0), new THREE.Vector2(0, rh)]);
    const rg = new THREE.ExtrudeGeometry(shape, { depth: W + 2 * o2, bevelEnabled: false });
    rg.rotateY(Math.PI / 2); rg.translate(-W / 2 - o2, H, 0); W_(rg);
    g.add(mesh(rg, roof, "Toit"));
    if (sub === "house") { const ch = box(0.7, rh + 1.2, 0.7); ch.translate(W * 0.25, H + rh / 2 + 0.4, -D * 0.15); W_(ch); g.add(mesh(ch, walls, "Cheminée")); }
  }
  return g;
}

function vehicle(spec) {
  const g = new THREE.Group(), sub = spec.subtype;
  const paint = makeMaterial("metal", { color: col(spec, "primary", { car: [0.7, 0.05, 0.04], truck: [0.1, 0.3, 0.6], spaceship: [0.85, 0.86, 0.88], boat: [0.92, 0.92, 0.9], plane: [0.85, 0.86, 0.88], tank: [0.3, 0.35, 0.2], bike: [0.1, 0.1, 0.12] }[sub] || [0.6, 0.6, 0.6]), roughness: 0.3, metallic: 0.5 });
  const glass = makeMaterial("glass", { color: [0.15, 0.2, 0.25] });
  const tire = makeMaterial("plastic", { color: [0.03, 0.03, 0.03], roughness: 0.85 });
  const rim = makeMaterial("metal", { color: [0.75, 0.76, 0.78] });
  const wheel = (r, w, x, y, z) => { const t = new THREE.TorusGeometry(r * 0.78, r * 0.22, 12, 32); t.rotateY(Math.PI / 2); t.translate(x, y, z); g.add(mesh(t, tire, "Pneu")); const c = cylinder(r * 0.62, w, 24); c.rotateZ(Math.PI / 2); c.translate(x, y, z); g.add(mesh(c, rim, "Jante")); };
  if (sub === "car" || sub === "truck") {
    const L = sub === "car" ? 4.4 : 6.5, Wd = sub === "car" ? 1.85 : 2.4, Hb = sub === "car" ? 0.75 : 1.2;
    const prof = [[-0.5, 0.45, 0.25], [-0.47, 0.75, 0.5], [-0.3, 0.85, 0.75], [0, 0.88, 0.8], [0.3, 0.88, 0.8], [0.45, 0.82, 0.7], [0.5, 0.6, 0.45]];
    g.add(mesh(loft(prof.map(([t, wf, hf]) => ellipseSection(0, 0.35 + Hb * hf / 2, -t * L, Wd / 2 * wf, Hb * hf / 2, 18, 0.7))), paint, "Carrosserie"));
    const cl = sub === "car" ? 0.45 : 0.25, c0 = sub === "car" ? 0.1 * L : 0.42 * L;
    g.add(mesh(loft([[0, 0.82, 0.15], [0.2, 0.75, 0.9], [0.75, 0.72, 1], [1, 0.8, 0.15]].map(([t, wf, hf]) => ellipseSection(0, 0.35 + Hb * 0.8 + 0.28 * hf * 0.5, c0 - t * cl * L, Wd / 2 * wf, 0.28 * Math.max(hf, 0.15), 14, 0.6))), glass, "Habitacle"));
    if (sub === "truck") { const c = box(Wd, 1.6, L * 0.55, 0.04); c.translate(0, 0.35 + Hb * 0.8 + 0.8, -L * 0.18); g.add(mesh(c, paint, "Benne")); }
    const r = sub === "car" ? 0.36 : 0.5;
    for (const z of [-0.32 * L, 0.32 * L]) for (const s of [1, -1]) wheel(r, 0.26, s * (Wd / 2 - 0.05), r, z);
    const lm = makeMaterial("emission", { color: [1, 0.95, 0.8] });
    for (const s of [1, -1]) g.add(mesh(place(ellipsoid(0.15, 0.07, 0.04, 12, 6), { pos: V(s * Wd * 0.33, 0.75, L / 2 - 0.03) }), lm, "Phare"));
  } else if (sub === "spaceship") {
    const L = 12;
    g.add(mesh(loft([[0, 0.05], [0.08, 0.45], [0.25, 0.9], [0.55, 1.1], [0.85, 1], [1, 0.75]].map(([t, r]) => ellipseSection(0, 1.6, L / 2 - t * L, r * 1.3, r, 18))), paint, "Coque"));
    const wm = makeMaterial("metal", { color: col(spec, "secondary", [0.35, 0.37, 0.42]) });
    for (const s of [1, -1]) {
      const sh = new THREE.Shape([new THREE.Vector2(s * 1, 1), new THREE.Vector2(s * 6.5, -3), new THREE.Vector2(s * 6.8, -4.2), new THREE.Vector2(s * 1, -4.5)]);
      const wg = new THREE.ExtrudeGeometry(sh, { depth: 0.25, bevelEnabled: false }); wg.rotateX(Math.PI / 2); wg.translate(0, 1.5, 0);
      g.add(mesh(wg, wm, "Aile"));
      const e = cylinder(0.55, 2.4, 20); e.rotateX(Math.PI / 2); e.translate(s * 1.6, 1.5, -L / 2 + 0.6); g.add(mesh(e, wm, "Réacteur"));
      const fl = cylinder(0.42, 0.1, 20); fl.rotateX(Math.PI / 2); fl.translate(s * 1.6, 1.5, -L / 2 - 0.62); g.add(mesh(fl, makeMaterial("emission", { color: col(spec, "eyes", [0.2, 0.6, 1]) }), "Flamme"));
    }
    g.add(mesh(place(ellipsoid(0.7, 0.55, 1.6, 20, 10), { pos: V(0, 2.4, 2.5) }), glass, "Cockpit"));
  } else if (sub === "boat") {
    const L = 8;
    const secs = [[0, 0.02, 0.8], [0.15, 0.6, 0.9], [0.45, 1, 1], [0.8, 0.95, 0.95], [1, 0.85, 0.9]].map(([t, w, d]) => Array.from({ length: 13 }, (_, k) => { const a = Math.PI * k / 12; return V(Math.cos(a) * 1.4 * w, 1.2 - Math.sin(a) * 1.1 * d, L / 2 - t * L); }));
    const hull = loft(secs.map((s) => [...s, ...s.slice(1, -1).reverse().map((p) => V(p.x * 0.97, Math.max(p.y, 1.15), p.z))]));
    g.add(mesh(hull, paint, "Coque"));
    const deck = box(2.6, 0.1, L * 0.85); deck.translate(0, 1.15, -L * 0.05); boxUV(deck, 1); g.add(mesh(deck, texMat("planks", [0.55, 0.38, 0.2], [0.35, 0.22, 0.1], [1, 3]), "Pont"));
    const mast = cylinder(0.08, 7, 12); mast.translate(0, 4.6, 0.5); g.add(mesh(mast, makeMaterial("wood", { color: [0.45, 0.3, 0.15] }), "Mât"));
    const sail = fanMembrane(V(0, 1.6, 0.45), [[V(0, 4.5, 0.45), V(0, 7.8, 0.45)], [V(0.35, 4.2, -1.6), V(0.1, 7, 0)]], 0);
    g.add(mesh(sail, makeMaterial("fabric", { color: col(spec, "secondary", [0.95, 0.93, 0.88]), doubleSide: true }), "Voile"));
  } else if (sub === "plane") {
    const fus = lathe([[0.001, 0], [0.4, 0.4], [0.7, 1.5], [0.75, 4], [0.6, 7.5], [0.2, 9.6], [0.001, 10]], 20);
    fus.rotateX(-Math.PI / 2); fus.translate(0, 2, 5);
    g.add(mesh(fus, paint, "Fuselage"));
    const wing = (pts, y, d, name) => { const sh = new THREE.Shape(pts.map(([x, z]) => new THREE.Vector2(x, z))); const w = new THREE.ExtrudeGeometry(sh, { depth: d, bevelEnabled: false }); w.rotateX(Math.PI / 2); w.translate(0, y, 0); g.add(mesh(w, paint, name)); };
    wing([[-5.5, -0.2], [5.5, -0.2], [5.5, -1], [0, -1.5], [-5.5, -1]], 1.9, 0.12, "Ailes");
    wing([[-2, -4.6], [2, -4.6], [2, -5], [-2, -5]], 2.15, 0.1, "Stabilisateur");
    const fin = new THREE.Shape([new THREE.Vector2(-3.6, 0), new THREE.Vector2(-5, 0), new THREE.Vector2(-5, 1.5), new THREE.Vector2(-4.5, 1.5)]);
    const fg = new THREE.ExtrudeGeometry(fin, { depth: 0.1, bevelEnabled: false }); fg.rotateY(Math.PI / 2); fg.translate(-0.05, 2.5, 0); g.add(mesh(fg, paint, "Dérive"));
    g.add(mesh(place(ellipsoid(0.5, 0.45, 1, 16, 8), { pos: V(0, 2.6, 3) }), glass, "Cockpit"));
  } else if (sub === "tank") {
    const h = box(3.2, 1.2, 6.5, 0.15); h.translate(0, 1.2, 0); g.add(mesh(h, paint, "Caisse"));
    const t = cylinder(1.2, 0.8, 20); t.translate(0, 2.2, -0.3); g.add(mesh(t, paint, "Tourelle"));
    const b = cylinder(0.14, 4, 16); b.rotateX(Math.PI / 2); b.translate(0, 2.25, 2); g.add(mesh(b, paint, "Canon"));
    for (const s of [1, -1]) { const tr = box(0.7, 1, 6.8, 0.3); tr.translate(s * 1.85, 0.5, 0); g.add(mesh(tr, tire, "Chenille")); }
  } else {
    g.add(mesh(sweep([V(0, 0.35, 0.6), V(0, 0.8, 0.3), V(0, 0.75, -0.4), V(0, 0.35, -0.6)], [0.05, 0.05, 0.05, 0.05], { sides: 10 }), paint, "Cadre"));
    for (const z of [-0.7, 0.7]) wheel(0.33, 0.12, 0, 0.33, z);
    const seat = box(0.25, 0.08, 0.45, 0.03); seat.translate(0, 0.85, -0.2); g.add(mesh(seat, tire, "Selle"));
  }
  return g;
}

function weapon(spec) {
  const g = new THREE.Group(), sub = spec.subtype;
  const metal = makeMaterial(["gold", "rust", "crystal", "ice", "lava"].includes(spec.material) ? spec.material : "metal", { color: col(spec, "primary", [0.75, 0.77, 0.8]) });
  const grip = makeMaterial("leather", { color: col(spec, "secondary", [0.25, 0.12, 0.06]) });
  const gold = makeMaterial("gold", { color: [1, 0.75, 0.3] });
  const wood = makeMaterial("wood", { color: col(spec, "secondary", [0.4, 0.25, 0.12]) });
  if (sub === "sword" || sub === "dagger") {
    const L = sub === "sword" ? 1 : 0.35, w = sub === "sword" ? 0.05 : 0.035;
    g.add(mesh(sweep([V(0, 0.2, 0), V(0, 0.2 + L * 0.6, 0), V(0, 0.2 + L * 0.92, 0), V(0, 0.2 + L, 0)], [w, w * 0.95, w * 0.7, 0], { profile: [[-1, 0], [0, 0.18], [1, 0], [0, -0.18]], tip: true }), metal, "Lame"));
    g.add(mesh(place(box(sub === "sword" ? 0.28 : 0.12, 0.04, 0.05, 0.012), { pos: V(0, 0.19, 0) }), gold, "Garde"));
    const h = cylinder(0.02, 0.17, 12); h.translate(0, 0.085, 0); g.add(mesh(h, grip, "Poignée"));
    g.add(mesh(place(ellipsoid(0.032, 0.032, 0.032, 12, 8), { pos: V(0, -0.01, 0) }), gold, "Pommeau"));
    if (spec.glow) g.add(mesh(place(box(0.012, L * 0.5, 0.008), { pos: V(0, 0.2 + L * 0.35, 0) }), makeMaterial("emission", { color: col(spec, "eyes", [0.3, 0.7, 1]) }), "Runes"));
  } else if (["axe", "hammer", "spear", "scythe", "staff"].includes(sub)) {
    const hl = { axe: 0.9, hammer: 0.9, spear: 2, scythe: 1.8, staff: 1.7 }[sub];
    const h = cylinder(sub === "staff" ? 0.03 : 0.022, hl, 12); h.translate(0, hl / 2, 0); g.add(mesh(h, wood, "Manche"));
    if (sub === "axe") { const sh = new THREE.Shape([new THREE.Vector2(0, hl - 0.25), new THREE.Vector2(0.22, hl - 0.32), new THREE.Vector2(0.26, hl - 0.1), new THREE.Vector2(0.24, hl + 0.02), new THREE.Vector2(0, hl - 0.06)]); const e = new THREE.ExtrudeGeometry(sh, { depth: 0.03, bevelEnabled: true, bevelSize: 0.004, bevelThickness: 0.006 }); e.translate(0, 0, -0.015); g.add(mesh(e, metal, "Fer")); }
    else if (sub === "hammer") g.add(mesh(place(box(0.28, 0.13, 0.13, 0.012), { pos: V(0, hl - 0.05, 0) }), metal, "Tête"));
    else if (sub === "spear") g.add(mesh(sweep([V(0, hl, 0), V(0, hl + 0.12, 0), V(0, hl + 0.3, 0)], [0.04, 0.035, 0], { profile: [[-1, 0], [0, 0.2], [1, 0], [0, -0.2]], tip: true }), metal, "Pointe"));
    else if (sub === "scythe") { const pts = curvePoints(V(0, hl - 0.05, 0), V(1, 0.1, 0), 0.85, 10, V(0, 0, 1), -1.2); g.add(mesh(sweep(pts, taper(pts.length, 0.07, 0), { profile: [[-1, 0], [0, 0.08], [1, 0], [0, -0.08]], tip: true }), metal, "Lame")); }
    else {
      g.add(mesh(place(ellipsoid(0.07, 0.07, 0.07, 16, 10), { pos: V(0, hl + 0.1, 0) }), makeMaterial("crystal", { color: col(spec, "eyes", [0.4, 0.2, 0.9]), emission: col(spec, "eyes", [0.4, 0.2, 0.9]), emission_strength: 1.5 }), "Orbe"));
      g.add(mesh(merge([0, 2.1, 4.2].map((a) => sweep(curvePoints(V(Math.cos(a) * 0.04, hl - 0.02, Math.sin(a) * 0.04), V(Math.cos(a) * 0.4, 1, Math.sin(a) * 0.4), 0.2, 4, V(-Math.sin(a), 0, Math.cos(a)), -0.9), [0.012, 0.01, 0.008, 0.005, 0], { sides: 6, tip: true }))), gold, "Couronne"));
    }
  } else if (sub === "bow") {
    const arc = Array.from({ length: 17 }, (_, k) => V(0, -0.7 + 1.4 * k / 16, 0.18 * Math.pow(Math.sin(Math.PI * k / 16), 0.8)));
    g.add(mesh(sweep(arc, arc.map((_, k) => 0.012 + 0.012 * Math.sin(Math.PI * k / 16)), { sides: 8 }), makeMaterial("wood", { color: col(spec, "primary", [0.45, 0.28, 0.12]) }), "Arc"));
    g.add(mesh(sweep([arc[0], arc[16]], [0.002, 0.002], { sides: 4 }), makeMaterial("fabric", { color: [0.9, 0.88, 0.8] }), "Corde"));
    g.position.y = 0.7;
  } else if (sub === "shield") {
    const s = lathe([[0.001, 0.06], [0.25, 0.05], [0.45, 0.02], [0.5, 0], [0.5, -0.02], [0.001, -0.02]], 32); s.rotateX(Math.PI / 2); s.translate(0, 0.5, 0);
    g.add(mesh(s, makeMaterial("wood", { color: col(spec, "primary", [0.45, 0.28, 0.12]) }), "Bouclier"));
    g.add(mesh(place(ellipsoid(0.1, 0.1, 0.06, 16, 8), { pos: V(0, 0.5, 0.07) }), metal, "Umbo"));
    const r = new THREE.TorusGeometry(0.5, 0.02, 8, 48); r.translate(0, 0.5, 0); g.add(mesh(r, metal, "Cerclage"));
  } else {
    g.add(mesh(place(box(0.06, 0.1, 0.32, 0.01), { pos: V(0, 0.18, 0) }), metal, "Corps"));
    const b = cylinder(0.018, 0.25, 16); b.rotateX(Math.PI / 2); b.translate(0, 0.2, 0.27); g.add(mesh(b, metal, "Canon"));
    g.add(mesh(place(box(0.05, 0.15, 0.07, 0.01), { pos: V(0, 0.07, -0.08), rot: [0.25, 0, 0] }), grip, "Crosse"));
    const c = cylinder(0.022, 0.12, 12); c.rotateX(Math.PI / 2); c.translate(0, 0.25, -0.02); g.add(mesh(c, makeMaterial("emission", { color: col(spec, "eyes", [0.2, 0.9, 1]) }), "Cellule"));
  }
  return g;
}

function tree(spec, rnd, small = false, kw = spec.keyword) {
  const g = new THREE.Group();
  const bark = makeMaterial("bark", { color: [0.32, 0.22, 0.14] });
  const leaves = makeMaterial("organic", { color: col(spec, "primary", [0.15, 0.42, 0.1]) });
  const H = small ? 1.2 : 6;
  if (kw === "sapin" || kw === "pine") {
    const t = cylinder(H * 0.04, H, 10, H * 0.01); t.translate(0, H / 2, 0); g.add(mesh(t, bark, "Tronc"));
    const cones = [0, 1, 2, 3].map((k) => { const c = cylinder(H * (0.32 - 0.06 * k), H * 0.35, 14, 0.01); c.translate(0, H * (0.3 + 0.18 * k), 0); return c; });
    const cg = merge(cones); applyPattern(cg, "organic", { color: [0.1, 0.32, 0.12] }); const lm = makeMaterial("organic", { color: [1, 1, 1] }); lm.vertexColors = true;
    g.add(mesh(cg, lm, "Feuillage"));
    return g;
  }
  const br = [], blobs = [];
  const grow = (p, d, len, r, depth) => {
    const pts = curvePoints(p, d, len, 4, V(rnd() - 0.5, 0, rnd() - 0.5), (rnd() - 0.5) * 0.8);
    br.push(sweep(pts, taper(pts.length, r, r * 0.6), { sides: 8 }));
    if (!depth) { blobs.push([pts.at(-1), len * 0.9]); return; }
    for (let k = 0; k < (depth < 3 ? 2 : 3); k++) grow(pts.at(-1), d.clone().add(V((rnd() - 0.5) * 1.8, 0.1 + rnd() * 0.5, (rnd() - 0.5) * 1.8)).normalize(), len * 0.68, r * 0.6, depth - 1);
  };
  grow(V(0, 0, 0), V(0, 1, 0), H * (small ? 0.25 : 0.38), H * (small ? 0.03 : 0.06), small ? 2 : 3);
  g.add(mesh(merge(br), bark, "Tronc"));
  const fol = merge(blobs.map(([c, s]) => { const ic = new THREE.IcosahedronGeometry(s * 0.8, 2); const p = ic.attributes.position; for (let i = 0; i < p.count; i++) { const f = 1 + (fbm(p.getX(i) * 3 + c.x, p.getY(i) * 3, p.getZ(i) * 3) - 0.5) * 0.5; p.setXYZ(i, p.getX(i) * f + c.x, p.getY(i) * f + c.y, p.getZ(i) * f + c.z); } ic.computeVertexNormals(); return ic; }));
  applyPattern(fol, "organic", { color: col(spec, "primary", [0.15, 0.42, 0.1]) });
  leaves.vertexColors = true; leaves.color.set(0xffffff);
  g.add(mesh(fol, leaves, "Feuillage"));
  return g;
}

function prop(spec, rnd) {
  const g = new THREE.Group(), sub = spec.subtype;
  const woodT = (rep = [1, 1]) => texMat("planks", col(spec, "primary", [0.5, 0.32, 0.16]), [0.3, 0.18, 0.08], rep);
  const metal = makeMaterial("metal", { color: [0.35, 0.35, 0.37], roughness: 0.45 });
  const add = (geo, mat, name) => { g.add(mesh(geo, mat, name)); return geo; };
  if (sub === "barrel") {
    const b = lathe([[0.001, 0], [0.28, 0], [0.33, 0.25], [0.35, 0.45], [0.33, 0.65], [0.28, 0.9], [0.001, 0.9]], 24); add(b, woodT([3, 1]), "Douelles");
    add(merge([[0.3, 0.1], [0.345, 0.32], [0.345, 0.58], [0.3, 0.8]].map(([r, y]) => { const t = new THREE.TorusGeometry(r + 0.004, 0.012, 6, 32); t.rotateX(Math.PI / 2); t.translate(0, y, 0); return t; })), metal, "Cercles");
  } else if (sub === "crate" || sub === "chest") {
    const chest = sub === "chest";
    const c = box(0.8, chest ? 0.5 : 0.8, chest ? 0.6 : 0.8, 0.015); c.translate(0, chest ? 0.25 : 0.4, 0); boxUV(c, 1.2); add(c, woodT(), "Caisse");
    if (chest) {
      const lid = new THREE.CylinderGeometry(0.3, 0.3, 0.8, 24, 1, false, 0, Math.PI); lid.rotateZ(Math.PI / 2); lid.translate(0, 0.5, 0); boxUV(lid, 1.2); add(lid, woodT(), "Couvercle");
      add(merge([-0.28, 0.28].map((x) => place(box(0.06, 0.82, 0.62), { pos: V(x, 0.42, 0) }))), makeMaterial("gold", { color: [1, 0.75, 0.3] }), "Ferrures");
    }
  } else if (sub === "table") {
    const t = box(1.6, 0.06, 0.9, 0.01); t.translate(0, 0.75, 0); boxUV(t, 1); add(t, woodT(), "Plateau");
    add(merge([[-0.72, -0.37], [-0.72, 0.37], [0.72, -0.37], [0.72, 0.37]].map(([x, z]) => place(box(0.07, 0.72, 0.07), { pos: V(x, 0.36, z) }))), makeMaterial("wood", { color: [0.4, 0.25, 0.12] }), "Pieds");
  } else if (sub === "chair") {
    const wm = makeMaterial("wood", { color: col(spec, "primary", [0.45, 0.28, 0.14]) });
    add(place(box(0.45, 0.05, 0.45, 0.01), { pos: V(0, 0.45, 0) }), wm, "Assise");
    add(place(box(0.45, 0.5, 0.05, 0.01), { pos: V(0, 0.72, -0.2) }), wm, "Dossier");
    add(merge([[-0.2, -0.2], [-0.2, 0.2], [0.2, -0.2], [0.2, 0.2]].map(([x, z]) => place(box(0.04, 0.45, 0.04), { pos: V(x, 0.225, z) }))), wm, "Pieds");
  } else if (sub === "lamp") {
    add(lathe([[0.001, 0], [0.12, 0], [0.12, 0.03], [0.03, 0.06], [0.025, 0.45], [0.001, 0.45]], 20), metal, "Socle");
    add(lathe([[0.001, 0.45], [0.09, 0.47], [0.11, 0.6], [0.08, 0.72], [0.001, 0.74]], 20), makeMaterial("glass", { color: [1, 0.9, 0.7] }), "Verre");
    add(place(ellipsoid(0.025, 0.05, 0.025, 10, 6), { pos: V(0, 0.58, 0) }), makeMaterial("emission", { color: col(spec, "eyes", [1, 0.6, 0.2]) }), "Flamme");
  } else if (sub === "bottle") {
    const vase = ["vase", "amphore", "jarre", "jar", "urne", "urn", "pot"].includes(spec.keyword);
    const c = col(spec, "primary", vase ? [0.75, 0.4, 0.25] : [0.2, 0.8, 0.3]);
    if (vase) add(lathe([[0.001, 0], [0.12, 0], [0.22, 0.2], [0.24, 0.35], [0.14, 0.55], [0.1, 0.62], [0.14, 0.68], [0.12, 0.68]], 28), makeMaterial("plain", { color: c, roughness: 0.3 }), "Vase");
    else {
      add(lathe([[0.001, 0], [0.12, 0], [0.15, 0.08], [0.15, 0.18], [0.05, 0.3], [0.04, 0.38], [0.05, 0.4], [0.001, 0.4]], 24), makeMaterial("glass", { color: c }), "Flacon");
      add(lathe([[0.001, 0.01], [0.13, 0.01], [0.14, 0.08], [0.14, 0.17], [0.001, 0.17]], 24), makeMaterial(spec.glow ? "emission" : "plain", { color: c, roughness: 0.1 }), "Liquide");
      const k = cylinder(0.04, 0.06, 12); k.translate(0, 0.42, 0); add(k, makeMaterial("wood", { color: [0.5, 0.35, 0.2] }), "Bouchon");
    }
  } else if (sub === "rock") {
    const r = new THREE.IcosahedronGeometry(0.6, 4);
    const p = r.attributes.position;
    for (let i = 0; i < p.count; i++) { const x = p.getX(i), y = p.getY(i), z = p.getZ(i); const f = 0.8 + fbm(x * 2 + 3, y * 2, z * 2, 5) * 0.5; p.setXYZ(i, x * f, y * f * 0.7, z * f); }
    r.computeVertexNormals();
    applyPattern(r, "stone", { color: col(spec, "primary", [0.45, 0.43, 0.4]) });
    const m = makeMaterial("stone", { color: [1, 1, 1] }); m.vertexColors = true;
    add(r, m, "Rocher");
  } else if (sub === "tree" || sub === "bush") return tree(spec, rnd, sub === "bush");
  else if (sub === "mushroom") {
    add(lathe([[0.001, 0], [0.08, 0], [0.06, 0.25], [0.07, 0.4], [0.001, 0.4]], 16), makeMaterial("plain", { color: [0.92, 0.88, 0.8] }), "Pied");
    add(lathe([[0.001, 0.33], [0.3, 0.35], [0.28, 0.45], [0.15, 0.55], [0.001, 0.58]], 24), makeMaterial("plain", { color: col(spec, "primary", [0.8, 0.1, 0.05]) }), "Chapeau");
    add(merge([0, 1, 2, 3, 4, 5].map((a) => place(ellipsoid(0.03, 0.015, 0.03, 8, 4), { pos: V(0.18 * Math.cos(a), 0.5, 0.18 * Math.sin(a)) }))), makeMaterial("plain", { color: [0.95, 0.95, 0.9] }), "Points");
  } else if (sub === "crystal") {
    add(merge([0, 1, 2, 3, 4].map((k) => { const a = k * 1.3, h = 0.4 + rnd() * 0.5; const d = k ? V(Math.cos(a) * 0.35, 1, Math.sin(a) * 0.35).normalize() : V(0, 1, 0); return sweep([V(0, 0, 0), d.clone().multiplyScalar(h * 0.75), d.clone().multiplyScalar(h)], [0.1, 0.1, 0], { sides: 6, tip: true }); })), makeMaterial("crystal", { color: col(spec, "primary", [0.5, 0.3, 0.95]), emission: col(spec, "primary", [0.5, 0.3, 0.95]), emission_strength: 0.6 }), "Cristaux");
  } else if (sub === "well") {
    add(lathe([[0.55, 0], [0.75, 0], [0.75, 0.8], [0.55, 0.8], [0.55, 0]], 24), texMat("stone", [0.55, 0.52, 0.48], [0.35, 0.33, 0.3], [3, 1]), "Margelle");
    add(merge([-0.7, 0.7].map((x) => place(box(0.1, 1.6, 0.1), { pos: V(x, 1.6, 0) }))), makeMaterial("wood", { color: [0.4, 0.25, 0.12] }), "Poteaux");
    const rf = cylinder(0.9, 0.5, 4, 0.01); rf.rotateY(Math.PI / 4); rf.translate(0, 2.6, 0); add(rf, makeMaterial("wood", { color: [0.45, 0.2, 0.1] }), "Toit");
  } else if (sub === "fence") {
    add(merge([...[-2, -1, 0, 1, 2].map((x) => place(box(0.1, 1.1, 0.1), { pos: V(x, 0.55, 0) })), ...[0.4, 0.85].map((y) => place(box(4.2, 0.1, 0.05), { pos: V(0, y, 0) }))]), makeMaterial("wood", { color: [0.45, 0.3, 0.15] }), "Clôture");
  } else {
    const ring = new THREE.CylinderGeometry(0.12, 0.12, 0.06, 32, 1, true); ring.translate(0, 0.03, 0);
    add(merge([ring, ...Array.from({ length: 8 }, (_, k) => { const a = k * Math.PI / 4; return sweep([V(0.12 * Math.cos(a), 0.06, 0.12 * Math.sin(a)), V(0.12 * Math.cos(a), 0.14, 0.12 * Math.sin(a))], [0.02, 0], { sides: 6, tip: true }); })]), makeMaterial("gold", { color: [1, 0.75, 0.3] }), "Couronne");
  }
  return g;
}

function scenery(spec, rnd, quality) {
  const g = new THREE.Group(), sub = spec.subtype;
  const size = spec.terrain_size || 40;
  if (sub === "dungeon") {
    const st = texMat("stone", col(spec, "primary", [0.4, 0.38, 0.36]), [0.25, 0.24, 0.22], [1, 1]);
    const W = 12, D = 16, H = 5;
    const parts = [place(box(W, 0.3, D), { pos: V(0, -0.15, 0) }), place(box(W, H, 0.6), { pos: V(0, H / 2, -D / 2) }), place(box(0.6, H, D), { pos: V(W / 2, H / 2, 0) }), place(box(0.6, H, D), { pos: V(-W / 2, H / 2, 0) }), place(box(W * 0.35, H, 0.6), { pos: V(-W * 0.33, H / 2, D / 2) }), place(box(W * 0.35, H, 0.6), { pos: V(W * 0.33, H / 2, D / 2) })];
    const room = merge(parts); boxUV(room, 0.4); g.add(mesh(room, st, "Salle"));
    const pil = merge([-3, 3].flatMap((x) => [-4, 0, 4].map((z) => { const c = cylinder(0.45, H, 12); c.translate(x, H / 2, z); return c; }))); boxUV(pil, 0.4); g.add(mesh(pil, st, "Piliers"));
    for (const x of [-W / 2 + 0.4, W / 2 - 0.4]) for (const z of [-4, 4]) {
      const t = cylinder(0.05, 0.5, 8); t.translate(x, 2.4, z); g.add(mesh(t, makeMaterial("wood", { color: [0.3, 0.2, 0.1] }), "Torche"));
      g.add(mesh(place(ellipsoid(0.08, 0.15, 0.08, 8, 6), { pos: V(x, 2.75, z) }), makeMaterial("emission", { color: [1, 0.5, 0.15] }), "Flamme"));
      const l = new THREE.PointLight(0xff9a4a, 6, 8); l.position.set(x * 0.9, 2.9, z); l.name = "Lueur"; g.add(l);
    }
    return g;
  }
  const res = { draft: 48, standard: 96, high: 128, ultra: 160 }[quality] || 96;
  const tg = new THREE.PlaneGeometry(size, size, res, res);
  tg.rotateX(-Math.PI / 2);
  const mountain = ["montagne", "mountain", "canyon"].includes(spec.keyword);
  const p = tg.attributes.position;
  for (let i = 0; i < p.count; i++) {
    const x = p.getX(i), z = p.getZ(i);
    const h = fbm(x / size * 4 + 10, 0.5, z / size * 4, 6);
    p.setY(i, (h - 0.35) * size * (mountain ? 0.35 : 0.08));
  }
  tg.computeVertexNormals();
  const desert = spec.keyword === "desert";
  applyPattern(tg, "ground", { color: col(spec, "primary", desert ? [0.8, 0.7, 0.45] : [0.25, 0.4, 0.12]), color2: desert ? [0.65, 0.55, 0.35] : [0.35, 0.28, 0.18] }, { scale: 0.3 });
  const tm = makeMaterial("ground", { color: [1, 1, 1] }); tm.vertexColors = true;
  g.add(mesh(tg, tm, "Terrain"));
  const heightAt = (x, z) => (fbm(x / size * 4 + 10, 0.5, z / size * 4, 6) - 0.35) * size * (mountain ? 0.35 : 0.08);
  if (sub === "forest") for (let i = 0; i < (spec.count || 16); i++) {
    const x = (rnd() - 0.5) * size * 0.8, z = (rnd() - 0.5) * size * 0.8;
    const t = tree({ colors: {} }, rnd, false, rnd() < 0.5 ? "sapin" : "chene");
    t.position.set(x, heightAt(x, z) - 0.1, z); t.scale.setScalar(0.7 + rnd() * 0.6); t.name = `Arbre ${i + 1}`;
    g.add(t);
  }
  if (sub === "village") for (let i = 0; i < (spec.count || 6); i++) {
    const a = i / 6 * Math.PI * 2, x = Math.cos(a) * size * 0.25, z = Math.sin(a) * size * 0.25;
    const h = building({ subtype: "house", colors: {}, floors: 1 + (i % 2) });
    h.position.set(x, heightAt(x, z) - 0.2, z); h.rotation.y = -a - Math.PI / 2; h.scale.setScalar(0.6); h.name = `Maison ${i + 1}`;
    g.add(h);
  }
  if (sub === "ruins") {
    const st = texMat("stone", [0.6, 0.57, 0.5], [0.4, 0.38, 0.33], [1, 1]);
    const pieces = [];
    for (let i = 0; i < 10; i++) { const x = (rnd() - 0.5) * 16, z = (rnd() - 0.5) * 16, h = 1 + rnd() * 4; pieces.push(place(lathe([[0.5, 0], [0.4, 0.2], [0.38, h], [0.001, h]], 14), { pos: V(x, heightAt(x, z) - 0.2, z), rot: [(rnd() - 0.5) * 0.3, 0, (rnd() - 0.5) * 0.3] })); }
    for (let i = 0; i < 4; i++) { const x = (rnd() - 0.5) * 16, z = (rnd() - 0.5) * 16; pieces.push(place(box(2 + rnd() * 3, 1 + rnd() * 2, 0.6), { pos: V(x, heightAt(x, z) + 0.5, z) })); }
    const rg = merge(pieces); boxUV(rg, 0.5); g.add(mesh(rg, st, "Ruines"));
  }
  return g;
}

function fitHard(root, spec) {
  root.updateMatrixWorld(true);
  const b = new THREE.Box3().setFromObject(root);
  const target = spec.height || (DEFAULT_HEIGHTS[`${spec.category}/${spec.subtype}`] ? DEFAULT_HEIGHTS[`${spec.category}/${spec.subtype}`] * (spec.size_hint === "big" ? 2.5 : spec.size_hint === "small" ? 0.5 : 1) : null);
  if (target && spec.category !== "building" && spec.category !== "scenery") {
    const k = target / Math.max(b.max.y - b.min.y, 1e-4);
    root.scale.setScalar(k);
    root.updateMatrixWorld(true);
  }
  const b2 = new THREE.Box3().setFromObject(root);
  for (const c of root.children) c.position.y -= b2.min.y / root.scale.y;
}

// ------------------------------------------------------------------ point d'entrée
const CREATURE = { dragon: ["quad", "dragon"], hydra: ["quad", "hydra"], griffin: ["quad", "griffin"], beast: ["quad", "beast"], kraken: ["octopus"], blob: ["blob"], demon: ["humanoid"], golem: ["humanoid"], alien: ["humanoid"], spider_monster: ["arthropod"] };
const ANIMAL = { wolf: ["quad", "wolf"], cat: ["quad", "cat"], horse: ["quad", "horse"], bear: ["quad", "bear"], deer: ["quad", "deer"], cow: ["quad", "cow"], rabbit: ["quad", "rabbit"], lizard: ["quad", "lizard"], bird: ["bird"], fish: ["fish"], snake: ["serpent"], spider: ["arthropod"] };

function seeded(seed) { let s = seed >>> 0 || 1; return () => ((s = Math.imul(s ^ (s >>> 15), 2246822519) + 0x9e3779b9 >>> 0) / 4294967296); }

export function generate(spec, params = {}) {
  const t0 = performance.now();
  const quality = params.quality || "standard";
  spec = JSON.parse(JSON.stringify(spec));
  spec.features ||= {}; spec.colors ||= {}; spec.scales ||= {};
  if (quality === "high" || quality === "ultra") spec.detail ||= "high";
  spec.seed ??= Math.floor(Math.random() * 100000);
  const rnd = seeded(spec.seed);
  const name = (spec.keyword || spec.subtype || spec.category || "Modèle").replace(/_/g, " ");
  const label = name.charAt(0).toUpperCase() + name.slice(1);
  let root;
  if (["creature", "character", "animal"].includes(spec.category)) {
    const o = new Organic(spec, label);
    let [plan, preset] = spec.category === "character" ? ["humanoid"] : spec.category === "animal" ? ANIMAL[spec.subtype] || ["quad", "wolf"] : CREATURE[spec.subtype] || ["quad", "beast"];
    if (["minotaure", "minotaur", "loup-garou", "werewolf", "troll", "ogre"].includes(spec.keyword)) plan = "humanoid";
    if (["serpent", "basilic", "basilisk"].includes(spec.keyword) || (spec.features.legs === 0 && spec.subtype !== "blob")) plan = "serpent";
    if (spec.category === "creature" && preset === "beast" && spec.features.horns === undefined) spec.features.horns = 2;
    if (["tortue", "turtle"].includes(spec.keyword)) spec.features.shell = true;
    ({ quad: () => quadruped(o, preset), humanoid: () => humanoid(o), serpent: () => serpent(o), octopus: () => octopus(o), blob: () => blob(o), bird: () => bird(o), fish: () => fish(o), arthropod: () => arthropod(o) })[plan]();
    root = assembleOrganic(o, quality);
  } else {
    root = ({ building, vehicle, weapon, prop: (s) => prop(s, rnd), scenery: (s) => scenery(s, rnd, quality) })[spec.category]?.(spec) || prop({ ...spec, subtype: "barrel" }, rnd);
    root.name = label;
    fitHard(root, spec);
  }
  root.userData.spec = spec;
  root.userData.params = params;
  root.userData.generated = true;
  root.traverse((c) => { if (c.isMesh) { c.castShadow = true; c.receiveShadow = true; } });
  let tris = 0;
  root.traverse((c) => { if (c.isMesh) tris += (c.geometry.index ? c.geometry.index.count : c.geometry.attributes.position.count) / 3; });
  return { root, stats: { triangles: Math.round(tris), ms: Math.round(performance.now() - t0) } };
}

export { Organic, assembleOrganic, addEyes };
