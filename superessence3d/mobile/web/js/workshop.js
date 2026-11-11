// Atelier : modélisation libre au doigt (corps sur mesure, pièces, placement par toucher, déformations).
import * as THREE from "three";
import { buildOrganicModel, deformMesh, makePart } from "./gen/custom.js";
import { PRESETS } from "./materials.js";

// ------------------------------------------------------------------ gabarits de corps (paramétriques)
export const BODY_KINDS = { biped: "Bipède (prédateur)", quad: "Quadrupède", humanoid: "Humanoïde", serpent: "Serpent", winged: "Créature ailée", blob: "Blob" };
export const BODY_PARAMS = [
  ["head", "Tête", 0.5, 2], ["neck", "Cou", 0.3, 2.5], ["body", "Corps (épaisseur)", 0.5, 2], ["belly", "Ventre", 0, 2],
  ["legs", "Pattes (longueur)", 0.4, 2], ["limbs", "Membres (épaisseur)", 0.5, 2], ["tail", "Queue", 0, 2.5], ["snout", "Museau", 0.3, 2.5],
];

export function bodyTemplate(kind, P = {}) {
  const k = (n) => P[n] ?? 1;
  const bones = [], blobs = [];
  const add = (name, head, tail, r, parent, role) => bones.push({ name, head, tail, radius: r, parent, role });
  const B = k("body"), L = k("legs"), T = k("limbs"), N = k("neck"), Hd = k("head"), Tl = k("tail"), Sn = k("snout");
  if (kind === "serpent") {
    let prev = null;
    for (let i = 0; i < 8; i++) {
      const z0 = 1.6 - i * 0.45 * Tl, z1 = 1.6 - (i + 1) * 0.45 * Tl, x0 = Math.sin(i * 0.9) * 0.25, x1 = Math.sin((i + 1) * 0.9) * 0.25;
      const r0 = 0.13 * B * (1 - i / 9), r1 = 0.13 * B * (1 - (i + 1) / 9) + 0.01;
      add(`corps${i + 1}`, [x0, 0.13 * B, z0], [x1, 0.13 * B, z1], [r0, r1], prev, `tail_${i + 1}`);
      prev = `corps${i + 1}`;
    }
    add("tete", [0, 0.15 * B, 1.6], [0, 0.17 * B, 1.6 + 0.3 * Sn], [0.16 * Hd, 0.08 * Hd], null, "head");
    return { bones, blobs, eyes: [{ position: [0.08 * Hd, 0.24 * B, 1.68], radius: 0.03 * Hd, bone: "tete" }] };
  }
  if (kind === "blob") {
    add("corps", [0, 0.45 * B, 0], [0, 0.9 * B, 0], [0.7 * B, 0.55 * B], null, "chest");
    blobs.push({ center: [0, 0.3, 0], radii: [0.85 * B, 0.35 * k("belly") + 0.1, 0.85 * B], bone: "corps" });
    return { bones, blobs, eyes: [{ position: [0.2, 0.95 * B, 0.55 * B], radius: 0.09 * Hd, bone: "corps" }] };
  }
  if (kind === "humanoid") {
    const h = 0.53 * L + 0.4;
    add("bassin", [0, h, 0], [0, h + 0.2, 0], [0.15 * B, 0.14 * B], null, "spine");
    add("torse", [0, h + 0.2, 0], [0, h + 0.45, 0], [0.15 * B, 0.2 * B], "bassin", "chest");
    add("cou", [0, h + 0.45, 0], [0, h + 0.45 + 0.1 * N, 0.02], [0.07, 0.06], "torse", "neck");
    add("tete", [0, h + 0.45 + 0.1 * N, 0.02], [0, h + 0.45 + 0.1 * N + 0.25 * Hd, 0.03], [0.12 * Hd, 0.1 * Hd], "cou", "head");
    blobs.push({ center: [0, h + 0.55 + 0.1 * N + 0.06 * Hd, 0.03], radii: [0.11 * Hd, 0.13 * Hd, 0.12 * Hd], bone: "tete" });
    blobs.push({ center: [0, h + 0.15, 0.05], radii: [0.17 * B, 0.15, 0.12 + 0.06 * k("belly")], bone: "bassin" });
    for (const [s, sx] of [["L", 1], ["R", -1]]) {
      add(`bras_${s}`, [0.2 * B * sx, h + 0.42, 0], [(0.24 * B + 0.12 * L) * sx, h + 0.15, 0], [0.06 * T, 0.045 * T], "torse", `arm_${s}_upper`);
      add(`avantbras_${s}`, [(0.24 * B + 0.12 * L) * sx, h + 0.15, 0], [(0.26 * B + 0.15 * L) * sx, h - 0.1, 0.05], [0.045 * T, 0.035 * T], `bras_${s}`, `arm_${s}_lower`);
      add(`cuisse_${s}`, [0.1 * B * sx, h, 0], [0.11 * B * sx, h * 0.5, 0.02], [0.09 * T, 0.06 * T], "bassin", `leg_${s}_upper`);
      add(`tibia_${s}`, [0.11 * B * sx, h * 0.5, 0.02], [0.11 * B * sx, 0.07, -0.02], [0.06 * T, 0.045 * T], `cuisse_${s}`, `leg_${s}_lower`);
      add(`pied_${s}`, [0.11 * B * sx, 0.07, -0.02], [0.11 * B * sx, 0.03, 0.16], [0.05, 0.035], `tibia_${s}`, `leg_${s}_foot`);
    }
    const ty = h + 0.45 + 0.1 * N + 0.13 * Hd;
    return { bones, blobs, eyes: [{ position: [0.045 * Hd, ty, 0.03 + 0.11 * Hd], radius: 0.02 * Hd, bone: "tete" }] };
  }
  // bipède / quadrupède / ailé
  const quad = kind === "quad" || kind === "winged";
  const y = quad ? 0.75 * L + 0.2 * B : 0.95 * L + 0.1;
  const zc = quad ? 0.55 : 0.4;
  add("bassin", [0, y, -0.25], [0, y + (quad ? 0.02 : 0.1), 0.15], [0.26 * B, 0.3 * B], null, "spine");
  add("torse", [0, y + (quad ? 0.02 : 0.1), 0.15], [0, y + (quad ? 0.05 : 0.3), zc], [0.3 * B, 0.25 * B], "bassin", "chest");
  const ny = y + (quad ? 0.05 : 0.3), nz = zc;
  const ne = [0, ny + 0.28 * N, nz + 0.2 * N + 0.05];
  add("cou", [0, ny, nz], ne, [0.18 * B, 0.12 * Hd], "torse", "neck");
  const hc = [0, ne[1] + 0.03, ne[2] + 0.18 * Hd];
  add("tete", ne, [0, ne[1] + 0.02, ne[2] + 0.45 * Hd * Sn], [0.15 * Hd, 0.07 * Hd], "cou", "head");
  blobs.push({ center: hc, radii: [0.15 * Hd, 0.13 * Hd, 0.19 * Hd], bone: "tete" });
  blobs.push({ center: [0, y - 0.05, 0.15], radii: [0.32 * B, 0.27 * B + 0.06 * k("belly"), 0.42 * B], bone: "torse" });
  if (Tl > 0.05) {
    add("queue1", [0, y, -0.25], [0, y - 0.05, -0.25 - 0.6 * Tl], [0.2 * B, 0.13 * B], "bassin", "tail_1");
    add("queue2", [0, y - 0.05, -0.25 - 0.6 * Tl], [0, y - 0.1, -0.25 - 1.2 * Tl], [0.13 * B, 0.06], "queue1", "tail_2");
    add("queue3", [0, y - 0.1, -0.25 - 1.2 * Tl], [0, y - 0.05, -0.25 - 1.8 * Tl], [0.06, 0.015], "queue2", "tail_3");
  }
  for (const [s, sx] of [["L", 1], ["R", -1]]) {
    const pairs = quad ? [["front", zc - 0.05, "torse"], ["back", -0.15, "bassin"]] : [["back", -0.05, "bassin"]];
    for (const [fb, z, par] of pairs) {
      const hip = [0.2 * B * sx, y - 0.05, z];
      const knee = [0.25 * B * sx, y * 0.55, z + (fb === "front" ? -0.05 : 0.12)];
      const ankle = [0.25 * B * sx, y * 0.18, z + (fb === "front" ? 0 : -0.08)];
      const foot = [0.26 * B * sx, 0.04, z + 0.15];
      const sfx = quad ? `_${fb === "front" ? "av" : "ar"}` : "";
      add(`cuisse${sfx}_${s}`, hip, knee, [0.15 * T, 0.1 * T], par, `leg_${s}_${fb}_upper`);
      add(`tibia${sfx}_${s}`, knee, ankle, [0.09 * T, 0.06 * T], `cuisse${sfx}_${s}`, `leg_${s}_${fb}_lower`);
      add(`pied${sfx}_${s}`, ankle, foot, [0.06 * T, 0.05 * T], `tibia${sfx}_${s}`, `leg_${s}_${fb}_foot`);
      if (!quad) blobs.push({ center: [0.22 * B * sx, y * 0.8, z + 0.08], radii: [0.15 * T, 0.2 * T, 0.16 * T], bone: `cuisse_${s}` });
    }
    if (!quad) {
      add(`bras_${s}`, [0.2 * B * sx, y + 0.22, zc - 0.05], [0.27 * B * sx, y + 0.02, zc + 0.12], [0.06 * T, 0.045 * T], "torse", `arm_${s}_upper`);
      add(`avantbras_${s}`, [0.27 * B * sx, y + 0.02, zc + 0.12], [0.22 * B * sx, y - 0.05, zc + 0.35], [0.045 * T, 0.03 * T], `bras_${s}`, `arm_${s}_lower`);
    }
  }
  const def = { bones, blobs, eyes: [{ position: [0.1 * Hd, hc[1] + 0.05 * Hd, hc[2] + 0.1 * Hd], radius: 0.03 * Hd, bone: "tete" }], parts: [] };
  if (kind === "winged") {
    for (const sx of [1, -1]) {
      const S0 = [0.22 * B * sx, y + 0.25, zc - 0.1], E = [0.75 * sx, y + 0.65, zc - 0.15], W = [1.3 * sx, y + 0.75, zc - 0.05];
      def.parts.push({ name: "Aile os", shape: "tube", points: [S0, E, W, [1.9 * sx, y + 0.5, zc - 0.5]], radii: [0.05, 0.04, 0.03, 0.005], tip: true, follow_bones: true, material: "organic", color: "#3a2a2a" });
      def.parts.push({ name: "Aile", shape: "membrane", origin: W, rows: [[[1.65 * sx, y + 0.7, zc - 0.3], [1.9 * sx, y + 0.5, zc - 0.5]], [[1.4 * sx, y + 0.4, zc - 0.6], [1.5 * sx, y + 0.1, zc - 0.9]], [[0.9 * sx, y + 0.3, zc - 0.6], [0.5 * sx, y + 0.05, zc - 0.85]], [[0.75 * sx, y + 0.55, zc - 0.3], [S0[0], S0[1], S0[2]]]], sag: 0.06, follow_bones: true, material: "organic", color: "#5a2f3a", double_sided: true });
    }
  }
  return def;
}

// ------------------------------------------------------------------ pièces prêtes à poser
export const PART_PRESETS = {
  sphere: { label: "Sphère", make: (s) => ({ shape: "sphere", radius: 0.15 * s }) },
  box: { label: "Cube", make: (s) => ({ shape: "box", size: [0.3 * s, 0.3 * s, 0.3 * s], bevel: 0.02 * s }) },
  ellipsoid: { label: "Ovale", make: (s) => ({ shape: "ellipsoid", radii: [0.12 * s, 0.18 * s, 0.12 * s] }) },
  cylinder: { label: "Cylindre", make: (s, len) => ({ shape: "cylinder", radius: 0.08 * s, height: 0.4 * len }) },
  cone: { label: "Cône", make: (s, len) => ({ shape: "cone", radius: 0.1 * s, height: 0.3 * len }) },
  torus: { label: "Anneau", make: (s) => ({ shape: "torus", radius: 0.15 * s, tube: 0.03 * s }) },
  horn: { label: "Corne", make: (s, len, curve) => ({ shape: "tube", points: [[0, 0, 0], [0, 0.18 * len, -0.06 * curve], [0, 0.3 * len, -0.2 * curve]].map((p) => p.map((v) => v * s)), radii: [0.05 * s, 0.03 * s, 0], tip: true }) },
  spike: { label: "Pointe", make: (s, len) => ({ shape: "tube", points: [[0, 0, 0], [0, 0.2 * len * s, 0]], radii: [0.05 * s, 0], tip: true, flatten: 0.5 }) },
  tentacle: { label: "Tentacule", make: (s, len, curve) => ({ shape: "tube", points: [[0, 0, 0], [0, -0.3 * len, 0.1 * curve], [0, -0.55 * len, 0.35 * curve], [0, -0.65 * len, 0.6 * curve]].map((p) => p.map((v) => v * s)), radii: [0.06 * s, 0.045 * s, 0.025 * s, 0], tip: true }) },
  tube: { label: "Tube courbé", make: (s, len, curve) => ({ shape: "tube", points: [[0, 0, 0], [0.1 * curve, 0.25 * len, 0], [0, 0.5 * len, 0]].map((p) => p.map((v) => v * s)), radii: [0.04 * s, 0.04 * s] }) },
  vase: { label: "Vase", make: (s, len) => ({ shape: "lathe", profile: [[0.0001, 0], [0.1, 0], [0.18, 0.15 * len], [0.12, 0.3 * len], [0.08, 0.38 * len], [0.1, 0.42 * len]].map(([r, y]) => [r * s, y * s]) }) },
  column: { label: "Colonne", make: (s, len) => ({ shape: "lathe", profile: [[0.0001, 0], [0.16, 0], [0.16, 0.05], [0.11, 0.08], [0.1, len * 1.2], [0.15, len * 1.24], [0.15, len * 1.3], [0.0001, len * 1.3]].map(([r, y]) => [r * s, y * s]) }) },
  dome: { label: "Dôme", make: (s) => ({ shape: "lathe", profile: Array.from({ length: 10 }, (_, i) => { const a = (i / 9) * Math.PI / 2; return [Math.cos(a) * 0.3 * s + 0.0001, Math.sin(a) * 0.3 * s]; }) }) },
  blade: { label: "Lame", make: (s, len) => ({ shape: "extrude", outline: [[-0.04, 0], [0.04, 0], [0.03, 0.8 * len], [0, 0.95 * len], [-0.03, 0.8 * len]].map(([x, y]) => [x * s, y * s]), depth: 0.012 * s, bevel: 0.004 * s }) },
  star: { label: "Étoile", make: (s) => ({ shape: "extrude", outline: Array.from({ length: 10 }, (_, i) => { const a = i * Math.PI / 5 + Math.PI / 2, r = (i % 2 ? 0.08 : 0.2) * s; return [Math.cos(a) * r, Math.sin(a) * r]; }), depth: 0.05 * s, bevel: 0.01 * s }) },
  shield: { label: "Bouclier", make: (s) => ({ shape: "extrude", outline: [[-0.25, 0.3], [0.25, 0.3], [0.25, 0], [0, -0.35], [-0.25, 0]].map(([x, y]) => [x * s, y * s]), depth: 0.04 * s, bevel: 0.015 * s }) },
  wing: { label: "Aile", make: (s, len) => ({ shape: "membrane", origin: [0, 0, 0], rows: [[[0.3, 0.3, 0], [0.7 * len, 0.35, -0.1]], [[0.35, 0.05, -0.2], [0.75 * len, -0.1, -0.35]], [[0.2, -0.05, -0.3], [0.3, -0.2, -0.55]]].map((r) => r.map((p) => p.map((v) => v * s))), sag: 0.04 * s, double_sided: true }) },
};

// ------------------------------------------------------------------ interface (appelée par app.js)
export function workshopUI(ctx) {
  const { h, field, chips, S, vp, toast } = ctx;
  const W = (S.ws ||= { kind: "biped", params: {}, preset: "horn", size: 1, len: 1, curve: 1, color: "#d9d2c0", material: "plain", mirror: true, emissive: false, bodyColor: "#2f5d3a", bodyMaterial: "scales", bodyColor2: "#16281b" });
  const wrap = h("div", {});
  wrap.append(h("h4", {}, "Atelier — créer un corps sur mesure"));
  wrap.append(chips(Object.entries(BODY_KINDS), (k) => W.kind === k, (k) => { W.kind = k; ctx.render(); }));
  const bc = h("input", { type: "color", value: W.bodyColor }); bc.onchange = () => { W.bodyColor = bc.value; };
  const bc2 = h("input", { type: "color", value: W.bodyColor2 }); bc2.onchange = () => { W.bodyColor2 = bc2.value; };
  const bm = h("select", {}, Object.entries(PRESETS).map(([k, l]) => h("option", { value: k, selected: W.bodyMaterial === k }, l))); bm.onchange = () => { W.bodyMaterial = bm.value; };
  wrap.append(h("div", { class: "row3" }, field("Matière", bm), field("Couleur", bc), field("Couleur 2", bc2)));
  wrap.append(h("button", { class: "btn pri full", onclick: () => createBody(ctx) }, "Créer ce corps (puis sculpte-le)"));
  wrap.append(h("p", { class: "muted" }, "Ensuite : onglet Objet › Atelier pour régler les proportions, ajouter volumes, yeux, cornes en touchant le modèle."));
  wrap.append(h("h4", {}, "Atelier — poser une pièce"));
  wrap.append(partPicker(ctx));
  return wrap;
}

function partPicker(ctx) {
  const { h, field, chips, S } = ctx;
  const W = S.ws;
  const box = h("div", {});
  box.append(chips(Object.entries(PART_PRESETS).map(([k, v]) => [k, v.label]), (k) => W.preset === k, (k) => { W.preset = k; ctx.render(); }));
  const sl = (key, label, min, max) => { const i = h("input", { type: "range", min, max, step: "0.05", value: W[key] }); i.oninput = () => { W[key] = parseFloat(i.value); }; return field(label, i); };
  box.append(h("div", { class: "row3" }, sl("size", "Taille", 0.2, 4), sl("len", "Longueur", 0.3, 3), sl("curve", "Courbure", -2, 3)));
  const col = h("input", { type: "color", value: W.color }); col.onchange = () => { W.color = col.value; };
  const mat = h("select", {}, Object.entries(PRESETS).map(([k, l]) => h("option", { value: k, selected: W.material === k }, l))); mat.onchange = () => { W.material = mat.value; };
  box.append(h("div", { class: "row2" }, field("Matière", mat), field("Couleur", col)));
  box.append(chips([["mirror", "Symétrie gauche/droite"], ["emissive", "Lumineux"]], (k) => W[k], (k) => { W[k] = !W[k]; ctx.render(); }));
  box.append(h("div", { class: "row2" },
    h("button", { class: "btn acc", onclick: () => startPlacing(ctx) }, "Poser en touchant"),
    h("button", { class: "btn", onclick: () => placeAt(ctx, null) }, "Ajouter seule")));
  return box;
}

function currentPartDef(W) {
  const d = PART_PRESETS[W.preset].make(W.size, W.len, W.curve);
  return { ...d, name: PART_PRESETS[W.preset].label, material: W.material, color: W.color, emissive: W.emissive ? W.color : undefined };
}

function startPlacing(ctx) {
  const { vp, toast } = ctx;
  if (!vp.content.children.length) return placeAt(ctx, null);
  ctx.setSheet("closed");
  ctx.hint("Touchez le modèle à l'endroit où poser la pièce");
  vp.placeMode = (hit) => { vp.placeMode = null; ctx.hint(""); placeAt(ctx, hit); ctx.setSheet("half"); };
  toast("Touchez le modèle pour poser la pièce");
}

function placeAt(ctx, hit) {
  const { vp, S } = ctx;
  const W = S.ws;
  const def = currentPartDef(W);
  const part = makePart(def, ctx.parseColor);
  if (!hit) {
    part.name = ctx.uniqueName(part.name);
    ctx.placeBeside(part);
    vp.add(part);
    ctx.commit(`Pièce ${part.name}`);
    ctx.render();
    return;
  }
  const model = vp.topLevel(hit.object);
  const normal = hit.face.normal.clone().transformDirection(hit.object.matrixWorld).normalize();
  // la pièce « pousse » le long de la normale de la surface touchée
  const q = new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), normal);
  const place = (point, quat, name) => {
    const p = part.clone(); p.name = name;
    model.updateMatrixWorld(true);
    // si le modèle a un os, on attache la pièce à l'os le plus proche pour qu'elle suive l'animation
    let parent = model, best = Infinity;
    model.traverse((b) => { if (b.isBone) { const d = b.getWorldPosition(new THREE.Vector3()).distanceTo(point); if (d < best) { best = d; parent = b; } } });
    const inv = parent.matrixWorld.clone().invert();
    const m = new THREE.Matrix4().compose(point, quat, new THREE.Vector3(1, 1, 1)).premultiply(inv);
    m.decompose(p.position, p.quaternion, p.scale);
    parent.add(p);
    p.userData.part = name;
  };
  place(hit.point.clone().addScaledVector(normal, -0.01 * W.size), q, def.name);
  if (W.mirror) {
    const local = model.worldToLocal(hit.point.clone());
    if (Math.abs(local.x) > 0.02) {
      local.x = -local.x;
      const wp = model.localToWorld(local);
      const ln = normal.clone().transformDirection(model.matrixWorld.clone().invert()); ln.x = -ln.x; ln.transformDirection(model.matrixWorld);
      place(wp.addScaledVector(ln, -0.01 * W.size), new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), ln.normalize()), `${def.name} (miroir)`);
    }
  }
  vp.select(model);
  ctx.commit(`Pièce ${def.name}`);
  ctx.render();
}

function createBody(ctx) {
  const { S, vp } = ctx;
  const W = S.ws;
  const tpl = bodyTemplate(W.kind, W.params);
  const def = { name: ctx.uniqueName(BODY_KINDS[W.kind].split(" ")[0]), ...tpl, material: W.bodyMaterial, color: W.bodyColor, color2: W.bodyColor2, smoothness: 0.12, resolution: 110, template: W.kind, params: { ...W.params } };
  rebuildBody(ctx, def, null);
}

export function rebuildBody(ctx, def, old) {
  const { vp } = ctx;
  ctx.busy("Construction du corps…");
  setTimeout(() => {
    try {
      const root = buildOrganicModel(def, ctx.parseColor, "standard");
      root.name = def.name;
      if (old) { root.position.copy(old.position); root.rotation.copy(old.rotation); root.scale.copy(old.scale); vp.content.remove(old); }
      else ctx.placeBeside(root);
      ctx.storeRest(root);
      vp.add(root);
      if (!old) vp.frame(root);
      ctx.commit(old ? "Atelier : corps modifié" : `Atelier : ${root.name}`);
      ctx.render();
    } catch (e) { ctx.toast(e.message, "err", 6000); }
    ctx.busy(null);
  }, 30);
}

/** Panneau « Atelier » d'un objet : proportions, volumes/yeux/cornes par toucher, déformations, pièces. */
export function objectWorkshop(ctx, o) {
  const { h, field, chips, S, vp } = ctx;
  const wrap = h("div", {});
  const def = o.userData.def && o.userData.custom ? o.userData.def : null;
  if (def) {
    wrap.append(h("h4", {}, "Atelier — proportions"));
    if (def.template) {
      def.params ||= {};
      const grid = h("div", { class: "row2" });
      for (const [key, label, min, max] of ctx.BODY_PARAMS) {
        const i = h("input", { type: "range", min, max, step: "0.05", value: def.params[key] ?? 1 });
        i.onchange = () => {
          def.params[key] = parseFloat(i.value);
          const t = bodyTemplate(def.template, def.params);
          const extraBlobs = (def.blobs || []).filter((b) => b.user);
          const extraEyes = (def.eyes || []).filter((b) => b.user);
          rebuildBody(ctx, { ...def, bones: t.bones, blobs: [...t.blobs, ...extraBlobs], eyes: extraEyes.length ? [...t.eyes, ...extraEyes] : t.eyes, parts: [...(t.parts || []), ...(def.parts || []).filter((p) => p.user)] }, o);
        };
        grid.append(field(label, i));
      }
      wrap.append(grid);
    }
    const r = h("input", { type: "range", min: "0.02", max: "0.6", step: "0.01", value: S.ws?.touchRadius ?? 0.12 });
    r.oninput = () => { (S.ws ||= {}).touchRadius = parseFloat(r.value); };
    wrap.append(h("h4", {}, "Atelier — en touchant le modèle"), field("Taille de l'ajout", r));
    const touch = (kind) => {
      ctx.setSheet("closed");
      ctx.hint({ add: "Touchez pour ajouter un volume", sub: "Touchez pour creuser", eye: "Touchez pour poser un œil", horn: "Touchez pour poser une corne", spike: "Touchez pour poser une pointe" }[kind]);
      vp.placeMode = (hit) => {
        vp.placeMode = null; ctx.hint(""); ctx.setSheet("half");
        if (vp.topLevel(hit.object) !== o) return ctx.toast("Touchez ce modèle-ci.");
        const inv = o.matrixWorld.clone().invert();
        const lp = hit.point.clone().applyMatrix4(inv);
        const ln = hit.face.normal.clone().transformDirection(hit.object.matrixWorld).transformDirection(inv).normalize();
        const rad = (S.ws?.touchRadius ?? 0.12) / Math.max(o.scale.x, 1e-3);
        const bone = nearestBone(def, lp);
        const mirror = S.ws?.mirror !== false && Math.abs(lp.x) > rad * 0.3;
        const pts = mirror ? [lp, new THREE.Vector3(-lp.x, lp.y, lp.z)] : [lp];
        const nd = mirror ? [ln, new THREE.Vector3(-ln.x, ln.y, ln.z)] : [ln];
        const nb = { ...def, blobs: [...(def.blobs || [])], eyes: [...(def.eyes || [])], parts: [...(def.parts || [])] };
        pts.forEach((p, i) => {
          const n = nd[i];
          if (kind === "add") nb.blobs.push({ center: p.clone().addScaledVector(n, rad * 0.2).toArray(), radii: [rad, rad, rad], bone, user: true });
          else if (kind === "sub") nb.blobs.push({ center: p.clone().addScaledVector(n, rad * 0.3).toArray(), radii: [rad, rad * 0.7, rad], bone, subtract: true, smoothness: rad * 0.3, user: true });
          else if (kind === "eye") nb.eyes.push({ position: p.clone().addScaledVector(n, -rad * 0.25).toArray(), radius: rad * 0.45, color: S.ws?.color || "#ffd21a", glow: !!S.ws?.emissive, look: n.toArray(), bone, mirror_x: false, user: true });
          else {
            const len = kind === "horn" ? rad * 3 : rad * 1.6;
            const tip = p.clone().addScaledVector(n, len).add(kind === "horn" ? new THREE.Vector3(0, len * 0.3, -len * 0.4) : new THREE.Vector3());
            const mid = p.clone().lerp(tip, 0.5).addScaledVector(n, len * 0.1);
            nb.parts.push({ name: kind === "horn" ? "Corne" : "Pointe", shape: "tube", points: [p.clone().addScaledVector(n, -rad * 0.2).toArray(), mid.toArray(), tip.toArray()], radii: [rad * 0.35, rad * 0.22, 0], tip: true, bone, follow_bones: kind === "spike", material: S.ws?.material || "plain", color: S.ws?.color || "#e8dcc0", user: true });
          }
        });
        rebuildBody(ctx, nb, o);
      };
    };
    wrap.append(h("div", { class: "row3" },
      h("button", { class: "btn", onclick: () => touch("add") }, "+ Volume"),
      h("button", { class: "btn", onclick: () => touch("sub") }, "− Creuser"),
      h("button", { class: "btn", onclick: () => touch("eye") }, "Œil")));
    wrap.append(h("div", { class: "row3", style: "margin-top:8px" },
      h("button", { class: "btn", onclick: () => touch("horn") }, "Corne"),
      h("button", { class: "btn", onclick: () => touch("spike") }, "Pointe"),
      h("button", { class: "btn", onclick: () => { const nb = { ...def, blobs: (def.blobs || []).filter((b) => !b.user), eyes: (def.eyes || []).filter((b) => !b.user), parts: (def.parts || []).filter((b) => !b.user) }; rebuildBody(ctx, nb, o); } }, "Retirer ajouts")));
    wrap.append(h("p", { class: "muted" }, "Couleur, matière, « Lumineux » et symétrie : réglages de la section « Poser une pièce » (onglet Créer)."));
  }
  // déformations
  S.def ||= { op: "smooth", amount: 1, axis: "y" };
  wrap.append(h("h4", {}, "Atelier — déformer"));
  wrap.append(chips([["smooth", "Lisser"], ["inflate", "Gonfler"], ["noise", "Relief"], ["taper", "Effiler"], ["twist", "Tordre"], ["bend", "Courber"], ["stretch", "Étirer"]], (k) => S.def.op === k, (k) => { S.def.op = k; S.def.amount = { taper: 0.5, twist: 45, bend: 30, stretch: 1.3 }[k] ?? 1; ctx.render(); }));
  const am = h("input", { type: "number", step: "0.1", value: S.def.amount }); am.onchange = () => { S.def.amount = parseFloat(am.value); };
  const ax = h("select", {}, ["x", "y", "z"].map((a) => h("option", { value: a, selected: S.def.axis === a }, `Axe ${a.toUpperCase()}`))); ax.onchange = () => { S.def.axis = ax.value; };
  wrap.append(h("div", { class: "row3" }, field("Intensité", am), field("Axe", ax), field(" ", h("button", { class: "btn pri", onclick: () => {
    try { vp.stop(); o.traverse((m) => { if (m.isMesh) deformMesh(m, S.def.op, S.def.amount, S.def.axis); }); ctx.commit(`Déformer (${S.def.op})`); ctx.render(); } catch (e) { ctx.toast(e.message, "err"); }
  } }, "Appliquer"))));
  // pièces
  const parts = [];
  o.traverse((x) => { if (x !== o && x.userData.part && !x.isBone) parts.push(x); });
  if (parts.length) {
    wrap.append(h("h4", {}, `Pièces (${parts.length})`));
    const list = h("div", { class: "list" });
    for (const p of parts.slice(0, 40)) list.append(h("div", { class: "it" }, h("span", { class: "nm" }, p.name),
      h("button", { onclick: () => { vp.selectPart(p); ctx.setTool("move"); ctx.toast(`Pièce « ${p.name} » sélectionnée : déplacez-la avec le gizmo`); } }, "Bouger"),
      h("button", { onclick: () => { p.parent.remove(p); ctx.commit("Retirer une pièce"); ctx.render(); } }, "✕")));
    wrap.append(list);
  }
  return wrap;
}

function nearestBone(def, lp) {
  let best = Infinity, name = "racine";
  for (const b of def.bones || []) {
    const a = new THREE.Vector3(...b.head), c = new THREE.Vector3(...b.tail);
    const ab = c.clone().sub(a), t = Math.max(0, Math.min(1, lp.clone().sub(a).dot(ab) / Math.max(ab.lengthSq(), 1e-9)));
    const d = lp.distanceTo(a.clone().addScaledVector(ab, t));
    if (d < best) { best = d; name = b.name; }
  }
  return name;
}
