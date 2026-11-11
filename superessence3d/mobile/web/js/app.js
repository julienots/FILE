// SUPERESSENCE 3D Mobile — application autonome (moteur 3D embarqué, hors ligne).
import * as THREE from "three";
import { Viewport } from "./viewport.js";
import { generate } from "./gen/generators.js";
import { parseMaterialText, parseModification, parsePrompt } from "./gen/prompt.js";
import { T } from "./gen/tables.js";
import { makeMaterial, PRESETS } from "./materials.js";
import { makeClip, makeObjectClip, MOTIONS, OBJECT_MOTIONS } from "./anim.js";
import { cloneDeep, deserialize, exportGLB, exportOBJ, exportSTL, History, saveFile, serialize, store, verify } from "./io.js";
import { imageToModel } from "./image3d.js";
import { buildOrganicModel, deformMesh, makePart } from "./gen/custom.js";
import { paintMesh, sculptMesh } from "./viewport.js";
import { BODY_PARAMS, objectWorkshop, workshopUI } from "./workshop.js";

const $ = (s, r = document) => r.querySelector(s);
const h = (tag, attrs = {}, ...kids) => {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v == null || v === false) continue;
    if (k === "class") e.className = v;
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else if (k === "html") e.innerHTML = v;
    else e.setAttribute(k, v === true ? "" : v);
  }
  for (const c of kids.flat()) if (c != null && c !== false) e.append(c instanceof Node ? c : document.createTextNode(String(c)));
  return e;
};
const toast = (msg, kind = "", ms = 3200) => { const t = h("div", { class: `toast ${kind}` }, msg); $("#toasts").append(t); setTimeout(() => t.remove(), ms); };
const busy = (msg) => { $("#busy").classList.toggle("hidden", !msg); if (msg) $("#busy span").textContent = msg; };
const nextFrame = () => new Promise((r) => requestAnimationFrame(() => setTimeout(r, 30)));
const hex = (c) => "#" + (c || [0.8, 0.8, 0.8]).slice(0, 3).map((x) => Math.round(Math.max(0, Math.min(1, x)) * 255).toString(16).padStart(2, "0")).join("");
const rgb = (s) => { const n = parseInt(s.slice(1), 16); return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255]; };

const S = { tab: "create", spec: null, quality: "standard", project: { id: "courant", name: "Sans titre" }, motions: new Set(["idle"]), amp: 1 };
const history = new History(20);

// ------------------------------------------------------------------ viewport
const vp = new Viewport($("#vp"), {
  selected: () => { if (S.tab === "object" || S.tab === "scene") render(); updatePlay(); },
  beforeChange: () => {},
  changed: () => commit("Modification"),
});

function uniqueName(base) {
  const names = new Set(vp.content.children.map((o) => o.name));
  if (!names.has(base)) return base;
  let i = 2;
  while (names.has(`${base} ${i}`)) i++;
  return `${base} ${i}`;
}

function storeRest(root) { root.traverse((b) => { if (b.isBone) b.userData.rest = { p: b.position.clone(), q: b.quaternion.clone(), s: b.scale.clone() }; }); }

let saveTimer = null;
function commit(label) {
  history.push(label, vp.content);
  updateUndo();
  clearTimeout(saveTimer);
  saveTimer = setTimeout(autosave, 1200);
}
function updateUndo() { $("#undo").disabled = !history.canUndo(); $("#redo").disabled = !history.canRedo(); }
function afterRestore(label) {
  vp.stop();
  vp.mixers.clear();
  vp.content.children.forEach(storeRest);
  vp.select(null);
  updateUndo();
  render();
  toast(label);
  clearTimeout(saveTimer); saveTimer = setTimeout(autosave, 800);
}
$("#undo").onclick = () => { if (history.canUndo()) { const l = history.stack[history.index].label; history.restore(history.index - 1, vp.content); afterRestore(`Annulé : ${l}`); } };
$("#redo").onclick = () => { if (history.canRedo()) afterRestore(`Rétabli : ${history.restore(history.index + 1, vp.content)}`); };

async function autosave() {
  try {
    const buf = await serialize(vp.content);
    await store.put("projects", { id: S.project.id, name: S.project.name, glb: buf, updated: Date.now(), thumb: vp.snapshot(160) });
  } catch (e) { console.warn("sauvegarde", e); }
}

// ------------------------------------------------------------------ outils
const TOOLS = [["select", "☝", "Sélection"], ["part", "◫", "Pièce"], ["move", "✥", "Déplacer"], ["rotate", "⟳", "Tourner"], ["scale", "⤡", "Échelle"], ["sculpt", "✎", "Sculpter"], ["paint", "🖌", "Peindre"]];
function buildTools() {
  const t = $("#tools");
  t.innerHTML = "";
  for (const [id, ic, label] of TOOLS) t.append(h("button", { class: vp.tool === id ? "on" : "", title: label, onclick: () => setTool(id) }, ic));
}
function setTool(id) {
  if ((id === "sculpt" || id === "paint") && !vp.selection) { toast("Touchez d'abord un modèle pour le sélectionner."); return; }
  if (id === "sculpt" || id === "paint") vp.stop();
  vp.setTool(id);
  buildTools();
  $("#hint").textContent = { part: "Touchez une pièce d'un modèle pour la sélectionner seule", sculpt: "Glissez un doigt pour sculpter · deux doigts pour tourner/zoomer", paint: "Glissez un doigt pour peindre · deux doigts pour tourner/zoomer", move: "Faites glisser les flèches du gizmo", rotate: "Faites glisser les anneaux du gizmo", scale: "Faites glisser les poignées du gizmo" }[id] || "";
  if (id === "sculpt" || id === "paint") { S.tab = "object"; setSheet("half"); render(); }
}
document.querySelectorAll("#views button").forEach((b) => (b.onclick = async () => {
  if (b.dataset.v === "frame") return vp.frame(vp.selection);
  if (b.dataset.v === "four") {
    if (!vp.content.children.length) return toast("Scène vide.");
    busy("Vues…");
    const img = await window.SEAPI.view_scene({ name: vp.selection && vp.selection.parent === vp.content ? vp.selection.name : undefined });
    busy(null);
    return showModal("Vérification — 4 vues", h("img", { src: `data:image/jpeg;base64,${img.image}`, style: "width:100%;border-radius:10px" }), [["Fermer", closeModal]]);
  }
  vp.view(b.dataset.v);
}));

function updatePlay() {
  const sel = vp.selection;
  $("#playbtn").classList.toggle("hidden", !(sel && sel.userData.clip));
  $("#playbtn").textContent = vp.playing ? "■" : "▶";
}
$("#playbtn").onclick = () => {
  if (vp.playing) vp.stop();
  else if (vp.selection?.userData.clip) vp.play(vp.selection, vp.selection.userData.clip);
  updatePlay();
};

// ------------------------------------------------------------------ panneau / onglets
function setSheet(state) { $("#sheet").className = state; setTimeout(() => vp.resize(), 250); }
$("#grip").onclick = () => { const c = $("#sheet").className; setSheet(c === "closed" ? "half" : c === "half" ? "full" : "closed"); };
document.querySelectorAll("#tabs button").forEach((b) => (b.onclick = () => {
  if (S.tab === b.dataset.t && $("#sheet").className !== "closed") { setSheet("closed"); return; }
  S.tab = b.dataset.t;
  document.querySelectorAll("#tabs button").forEach((x) => x.classList.toggle("on", x === b));
  if ($("#sheet").className === "closed") setSheet("half");
  render();
}));

function render() {
  const body = $("#sheetbody");
  const st = body.scrollTop;
  body.innerHTML = "";
  ({ create: renderCreate, scene: renderScene, object: renderObject, library: renderLibrary, export: renderExport })[S.tab](body);
  body.scrollTop = st;
}

function wctx() {
  return { h, field, chips, S, vp, toast, busy, render, commit, placeBeside, uniqueName, storeRest, setTool, BODY_PARAMS,
    parseColor: (c) => parsePrompt(`objet ${c}`).colors.primary || null,
    setSheet, hint: (t) => { $("#hint").textContent = t; } };
}
const field = (label, input) => h("div", { class: "field" }, h("label", {}, label), input);
const chips = (items, isOn, onTap) => h("div", { class: "chips" }, items.map(([id, label]) => h("button", { class: `chip ${isOn(id) ? "on" : ""}`, onclick: () => onTap(id) }, label)));

// ------------------------------------------------------------------ Créer
const QUICK = [["Dragon", "Un dragon noir de 2 mètres, avec quatre ailes, des cornes, des écailles détaillées, des yeux rouges et une queue avec des pointes."], ["Personnage", "un chevalier en armure"], ["Monstre", "un monstre violet avec six tentacules et trois yeux jaunes lumineux"], ["Animal", "un loup gris"], ["Bâtiment", "un château médiéval"], ["Véhicule", "un vaisseau spatial blanc"], ["Arme", "une épée en acier avec des runes lumineuses bleues"], ["Objet", "un coffre au trésor"], ["Décor", "une forêt"], ["Sorcier", "un sorcier avec une robe bleue"], ["Robot", "un robot"], ["Araignée", "une araignée géante noire"]];

function renderCreate(body) {
  body.append(h("h3", {}, "Créer un modèle 3D"));
  const ta = h("textarea", { placeholder: "Décrivez votre modèle… ex. « un dragon noir avec quatre ailes et des yeux rouges »" });
  ta.value = S.prompt || "";
  ta.oninput = () => { S.prompt = ta.value; S.spec = null; };
  body.append(ta);
  body.append(h("div", { class: "chips" }, QUICK.map(([l, p]) => h("button", { class: "chip", onclick: () => { S.prompt = p; S.spec = parsePrompt(p); render(); } }, l))));
  body.append(h("h4", {}, "Qualité"), chips([["draft", "Brouillon"], ["standard", "Standard"], ["high", "Haute"], ["ultra", "Ultra"]], (q) => S.quality === q, (q) => { S.quality = q; render(); }));
  body.append(h("div", { class: "row2" },
    h("button", { class: "btn", onclick: () => { if (!ta.value.trim()) return toast("Écrivez une description."); S.spec = parsePrompt(ta.value); render(); } }, "Analyser"),
    h("button", { class: "btn pri", onclick: () => doGenerate() }, "✦ Générer")));
  if (S.spec) body.append(specEditor());
  body.append(workshopUI(wctx()));
  body.append(h("h4", {}, "Photo → 3D"),
    h("div", { class: "note" }, "Méthode géométrique (pas une IA) : la silhouette de la photo est gonflée en volume et texturée. Utilisez une photo sur fond uni."),
    h("button", { class: "btn full", onclick: () => $("#photo").click() }, "📷 Choisir une photo"));
  body.append(h("h4", {}, "Formes de base"), h("div", { class: "row3" }, [["Cube", "box"], ["Sphère", "sphere"], ["Cylindre", "cylinder"], ["Cône", "cone"], ["Tore", "torus"], ["Plan", "plane"]].map(([l, k]) => h("button", { class: "btn", onclick: () => addPrimitive(k, l) }, l))));
  body.append(h("p", { class: "muted" }, "Génération procédurale sur le téléphone, hors ligne : la géométrie est construite par des algorithmes (pas un réseau neuronal)."));
}

function specEditor() {
  const sp = S.spec;
  const cats = T.CATEGORIES;
  const wrap = h("div", {});
  wrap.append(h("h4", {}, "Interprétation (modifiable)"));
  const cat = h("select", { onchange: () => { sp.category = cat.value; sp.subtype = Object.keys(cats[cat.value])[0]; render(); } }, Object.keys(cats).map((k) => h("option", { value: k, selected: k === sp.category }, T.CATEGORY_LABELS[k])));
  const sub = h("select", { onchange: () => { sp.subtype = sub.value; } }, Object.keys(cats[sp.category]).map((k) => h("option", { value: k, selected: k === sp.subtype }, k)));
  wrap.append(h("div", { class: "row2" }, field("Catégorie", cat), field("Type", sub)));
  const height = h("input", { type: "number", step: "0.1", min: "0", value: sp.height ?? "" , placeholder: "auto" });
  height.onchange = () => { sp.height = parseFloat(height.value) || undefined; };
  const c1 = h("input", { type: "color", value: hex(sp.colors?.primary) });
  c1.onchange = () => { (sp.colors ||= {}).primary = rgb(c1.value); };
  const ce = h("input", { type: "color", value: hex(sp.colors?.eyes || [0.95, 0.75, 0.1]) });
  ce.onchange = () => { (sp.colors ||= {}).eyes = rgb(ce.value); };
  wrap.append(h("div", { class: "row3" }, field("Hauteur (m)", height), field("Couleur", c1), field("Yeux", ce)));
  if (["creature", "character", "animal"].includes(sp.category)) {
    const f = (sp.features ||= {});
    for (const [k, l] of [["wings", "Ailes"], ["horns", "Cornes"], ["tails", "Queues"], ["tentacles", "Tentacules"], ["heads", "Têtes"], ["eyes", "Yeux"], ["legs", "Pattes"]]) {
      const val = h("span", {}, f[k] ?? "auto");
      const step = (d) => { f[k] = Math.max(0, Math.min(16, (+f[k] || (k === "eyes" || k === "heads" ? (k === "eyes" ? 2 : 1) : 0)) + d)); val.textContent = f[k]; };
      wrap.append(h("div", { class: "counter" }, h("span", { class: "muted", style: "text-align:left" }, l), h("button", { onclick: () => step(-1) }, "−"), val, h("button", { onclick: () => step(1) }, "+")));
    }
    wrap.append(chips([["spikes", "Pointes"], ["scales", "Écailles"], ["claws", "Griffes"], ["fangs", "Crocs"], ["fur", "Fourrure"], ["feathers", "Plumes"]], (k) => f[k], (k) => { f[k] = !f[k]; render(); }));
    wrap.append(chips([["glow_eyes", "Yeux lumineux"], ["glow", "Parties lumineuses"], ["wet", "Humide"]], (k) => sp[k], (k) => { sp[k] = !sp[k]; render(); }));
  }
  const mat = h("select", { onchange: () => { sp.material = mat.value || undefined; } }, h("option", { value: "" }, "Matériau automatique"), Object.entries(PRESETS).map(([k, l]) => h("option", { value: k, selected: sp.material === k }, l)));
  wrap.append(field("Matériau", mat));
  if (sp.notes?.length) wrap.append(h("p", { class: "muted" }, sp.notes.join(" · ")));
  return wrap;
}

async function doGenerate() {
  if (!S.spec) {
    if (!(S.prompt || "").trim()) return toast("Écrivez une description ou choisissez un exemple.");
    S.spec = parsePrompt(S.prompt);
  }
  busy("Génération du modèle…");
  await nextFrame();
  try {
    const { root, stats } = generate(S.spec, { quality: S.quality });
    root.name = uniqueName(root.name);
    storeRest(root);
    placeBeside(root);
    vp.add(root);
    vp.frame(root);
    commit(`Générer ${root.name}`);
    toast(`« ${root.name} » créé — ${stats.triangles.toLocaleString("fr")} triangles, ${stats.ms} ms`, "ok");
    S.tab = "object";
    document.querySelectorAll("#tabs button").forEach((x) => x.classList.toggle("on", x.dataset.t === "object"));
    render();
  } catch (e) { console.error(e); toast(`Échec : ${e.message}`, "err", 6000); }
  busy(null);
}

function placeBeside(obj) {
  if (!vp.content.children.length) return;
  const b = new THREE.Box3().setFromObject(vp.content);
  const ob = new THREE.Box3().setFromObject(obj);
  obj.position.x = b.max.x + (ob.max.x - ob.min.x) / 2 + 0.3 - (ob.min.x + ob.max.x) / 2;
}

function addPrimitive(kind, label) {
  const g = { box: () => new THREE.BoxGeometry(1, 1, 1, 8, 8, 8), sphere: () => new THREE.SphereGeometry(0.5, 48, 32), cylinder: () => new THREE.CylinderGeometry(0.5, 0.5, 1, 40, 8), cone: () => new THREE.ConeGeometry(0.5, 1, 40, 8), torus: () => new THREE.TorusGeometry(0.4, 0.15, 24, 64), plane: () => new THREE.PlaneGeometry(2, 2, 32, 32).rotateX(-Math.PI / 2) }[kind]();
  if (kind !== "plane") g.translate(0, kind === "torus" ? 0.55 : 0.5, 0);
  const m = new THREE.Mesh(g, makeMaterial("plain", { color: [0.75, 0.75, 0.78] }));
  m.name = uniqueName(label);
  m.castShadow = m.receiveShadow = true;
  placeBeside(m);
  vp.add(m);
  commit(`Créer ${m.name}`);
}

$("#photo").onchange = async (ev) => {
  const f = ev.target.files[0];
  ev.target.value = "";
  if (!f) return;
  busy("Reconstruction depuis la photo…");
  await nextFrame();
  try {
    const m = await imageToModel(f, { height: 1.2, depth: 0.5 });
    m.name = uniqueName(m.name);
    m.castShadow = true;
    placeBeside(m);
    vp.add(m);
    vp.frame(m);
    commit(`Photo → 3D ${m.name}`);
    toast("Modèle créé depuis la photo (silhouette gonflée)", "ok");
  } catch (e) { toast(e.message, "err", 6000); }
  busy(null);
};

// ------------------------------------------------------------------ Scène
function renderScene(body) {
  body.append(h("h3", {}, "Scène"));
  const list = h("div", { class: "list" });
  if (!vp.content.children.length) list.append(h("p", { class: "muted" }, "Scène vide. Créez un modèle dans l'onglet Créer."));
  for (const o of vp.content.children) {
    list.append(h("div", { class: `it ${vp.selection === o ? "sel" : ""}`, onclick: () => { vp.select(o); render(); } },
      h("span", {}, o.userData.generated ? "✦" : "◇"), h("span", { class: "nm" }, o.name),
      h("button", { onclick: (e) => { e.stopPropagation(); o.visible = !o.visible; render(); } }, o.visible ? "👁" : "—"),
      h("button", { onclick: (e) => { e.stopPropagation(); vp.select(o); vp.frame(o); } }, "Cadrer")));
  }
  body.append(list);
  body.append(h("h4", {}, "Éclairage"), chips([["studio", "Studio"], ["outdoor", "Extérieur"], ["dramatic", "Dramatique"], ["night", "Nuit"]], (k) => vp.lightPreset === k, (k) => { vp.setLighting(k); render(); }));
  const bg = h("input", { type: "color", value: "#" + vp.scene.background.getHexString() });
  bg.onchange = () => { vp.customBg = bg.value; vp.scene.background = new THREE.Color(bg.value); };
  body.append(h("div", { class: "row2" }, field("Fond", bg), field("Grille", h("button", { class: "btn", onclick: () => { vp.grid.visible = !vp.grid.visible; render(); } }, vp.grid.visible ? "Masquer" : "Afficher"))));
}

// ------------------------------------------------------------------ Objet
function renderObject(body) {
  const o = vp.selection;
  if (!o) { body.append(h("h3", {}, "Objet"), h("p", { class: "muted" }, "Touchez un modèle dans la vue 3D pour le sélectionner.")); return; }
  const name = h("input", { type: "text", value: o.name });
  name.onchange = () => { o.name = o.parent === vp.content ? uniqueName(name.value.trim() || o.name) : (name.value.trim() || o.name); commit("Renommer"); render(); };
  body.append(h("h3", {}, o.name), field("Nom", name));
  if (o.parent !== vp.content) {
    body.append(h("div", { class: "note" }, "Pièce sélectionnée : déplacez-la, tournez-la ou redimensionnez-la avec les outils de gauche."),
      h("button", { class: "btn full", onclick: () => { setTool("select"); vp.select(vp.topLevel(o)); render(); } }, "← Sélectionner le modèle entier"));
  }
  if (vp.tool === "sculpt" || vp.tool === "paint") body.append(brushPanel());
  // transformations
  const num = (v, fn, step = 0.1) => { const i = h("input", { type: "number", step, value: +v.toFixed(3) }); i.onchange = () => { fn(parseFloat(i.value) || 0); commit("Transformer"); }; return i; };
  body.append(h("h4", {}, "Position (m)"), h("div", { class: "row3" }, ["x", "y", "z"].map((k) => num(o.position[k], (v) => (o.position[k] = v)))));
  body.append(h("h4", {}, "Rotation (°) et taille"), h("div", { class: "row3" },
    num(THREE.MathUtils.radToDeg(o.rotation.y), (v) => (o.rotation.y = THREE.MathUtils.degToRad(v)), 15),
    num(o.scale.x, (v) => o.scale.setScalar(Math.max(0.001, v)), 0.1),
    h("button", { class: "btn", onclick: () => { o.position.y -= new THREE.Box3().setFromObject(o).min.y; commit("Poser au sol"); render(); } }, "Au sol")));
  if (o.parent === vp.content) body.append(objectWorkshop(wctx(), o));
  // modification par texte
  if (o.userData.generated && o.userData.spec) {
    const ta = h("textarea", { placeholder: "Ex. : agrandis ses ailes, ajoute six tentacules et remplace les yeux par des yeux blancs lumineux" });
    body.append(h("h4", {}, "Modifier par texte"), ta, h("div", { class: "row2" },
      h("button", { class: "btn pri", onclick: () => modifyModel(o, ta.value) }, "Appliquer"),
      h("button", { class: "btn", onclick: () => regenerate(o, { ...o.userData.spec, seed: Math.floor(Math.random() * 1e5) }, "Variante") }, "Variante")));
  }
  // matériaux
  const meshes = [];
  o.traverse((m) => { if (m.isMesh && !meshes.some((x) => x.material === m.material)) meshes.push(m); });
  const matBox = h("div", {});
  meshes.slice(0, 12).forEach((m) => {
    const mat = m.material;
    const col = h("input", { type: "color", value: hex(mat.userData?.params?.color || [mat.color.r, mat.color.g, mat.color.b]) });
    const pre = h("select", {}, Object.entries(PRESETS).map(([k, l]) => h("option", { value: k, selected: mat.userData?.preset === k }, l)));
    const apply = () => {
      const nm = makeMaterial(pre.value, { color: rgb(col.value), doubleSide: mat.side === THREE.DoubleSide });
      o.traverse((x) => { if (x.isMesh && x.material === mat) { if (x.geometry.attributes.color && !mat.vertexColors) x.geometry.deleteAttribute("color"); x.material = nm; } });
      commit("Matériau"); render();
    };
    col.onchange = apply; pre.onchange = apply;
    matBox.append(h("div", { class: "row2" }, field(m.name || "Partie", pre), field("Couleur", col)));
  });
  const desc = h("input", { type: "text", placeholder: "ou décrivez : « métal rouillé sombre », « peau noire humide »" });
  body.append(h("h4", {}, "Matériaux"), matBox, h("div", { class: "row2" }, desc, h("button", { class: "btn", onclick: () => {
    if (!desc.value.trim()) return;
    const r = parseMaterialText(desc.value);
    const nm = makeMaterial(r.preset, r.params);
    o.traverse((x) => { if (x.isMesh) { if (x.geometry.attributes.color) x.geometry.deleteAttribute("color"); x.material = nm; } });
    commit("Matériau décrit"); toast(`Matériau : ${PRESETS[r.preset] || r.preset}`, "ok"); render();
  } }, "Appliquer")));
  // animation
  const hasBones = !!o.getObjectByProperty("isBone", true);
  body.append(h("h4", {}, "Animation"));
  if (hasBones) {
    body.append(chips(Object.entries(MOTIONS), (k) => S.motions.has(k), (k) => { S.motions.has(k) ? S.motions.delete(k) : S.motions.add(k); render(); }));
    const amp = h("input", { type: "range", min: "0.2", max: "2.5", step: "0.1", value: S.amp });
    amp.onchange = () => { S.amp = parseFloat(amp.value); };
    body.append(field("Amplitude", amp), h("div", { class: "row2" },
      h("button", { class: "btn acc", onclick: () => {
        if (!S.motions.size) return toast("Choisissez au moins un mouvement.");
        vp.stop();
        const clip = makeClip(o, [...S.motions], { amplitude: S.amp, seconds: S.motions.has("walk") ? 1.2 : S.motions.has("run") ? 0.8 : 1.6 });
        if (!clip) return toast("Aucun os ne correspond à ces mouvements.", "err");
        o.userData.clip = clip;
        vp.play(o, clip); updatePlay(); commit("Animation");
        toast(`Animation « ${clip.name} » (${clip.tracks.length} pistes)`, "ok");
      } }, "Créer et lire"),
      h("button", { class: "btn", onclick: () => { vp.stop(); updatePlay(); } }, "Arrêter")));
  } else {
    body.append(chips(Object.entries(OBJECT_MOTIONS), () => false, (k) => { vp.stop(); const clip = makeObjectClip(o, k); o.userData.clip = clip; vp.play(o, clip); updatePlay(); commit("Animation"); }));
  }
  body.append(h("h4", {}, "Actions"), h("div", { class: "row3" },
    h("button", { class: "btn", onclick: () => { const c = cloneDeep(o); c.name = uniqueName(o.name); c.position.x += new THREE.Box3().setFromObject(o).getSize(new THREE.Vector3()).x + 0.3; storeRest(c); vp.add(c); commit("Dupliquer"); render(); } }, "Dupliquer"),
    h("button", { class: "btn", onclick: () => saveToLibrary(o) }, "→ Biblio"),
    h("button", { class: "btn dan", onclick: async () => { if (await confirmBox(`Supprimer « ${o.name} » ?`)) { vp.content.remove(o); vp.select(null); commit("Supprimer"); render(); } } }, "Supprimer")));
  const tris = (() => { let t = 0; o.traverse((m) => { if (m.isMesh) t += (m.geometry.index ? m.geometry.index.count : m.geometry.attributes.position.count) / 3; }); return t; })();
  const size = new THREE.Box3().setFromObject(o).getSize(new THREE.Vector3());
  body.append(h("p", { class: "muted" }, `${Math.round(tris).toLocaleString("fr")} triangles · ${size.x.toFixed(2)} × ${size.y.toFixed(2)} × ${size.z.toFixed(2)} m${hasBones ? " · squelette animable" : ""}`));
}

function brushPanel() {
  const b = vp.brush;
  const w = h("div", { class: "note" });
  w.append(h("b", {}, vp.tool === "sculpt" ? "Pinceau de sculpture" : "Pinceau de peinture"));
  if (vp.tool === "sculpt") w.append(chips([["draw", "Dessiner"], ["inflate", "Gonfler"], ["smooth", "Lisser"], ["flatten", "Aplatir"], ["pinch", "Pincer"], ["crease", "Pli"]], (k) => b.kind === k, (k) => { b.kind = k; render(); }),
    chips([["invert", "Inverser"], ["symmetry", "Symétrie"]], (k) => b[k], (k) => { b[k] = !b[k]; render(); }));
  else { const c = h("input", { type: "color", value: b.color }); c.onchange = () => { b.color = c.value; }; w.append(field("Couleur", c)); }
  const r = h("input", { type: "range", min: "0.01", max: "1.5", step: "0.01", value: b.radius }); r.oninput = () => { b.radius = parseFloat(r.value); };
  const s = h("input", { type: "range", min: "0.05", max: "2", step: "0.05", value: b.strength }); s.oninput = () => { b.strength = parseFloat(s.value); };
  w.append(field("Taille du pinceau", r), field("Force", s), h("button", { class: "btn full", onclick: () => setTool("select") }, "Terminer"));
  return w;
}

function regenerate(o, spec, label) {
  busy("Reconstruction…");
  setTimeout(() => {
    try {
      const { root } = generate(spec, { quality: o.userData.params?.quality || S.quality });
      root.name = o.name;
      root.position.copy(o.position); root.rotation.copy(o.rotation); root.scale.copy(o.scale);
      storeRest(root);
      const i = vp.content.children.indexOf(o);
      vp.content.remove(o);
      vp.content.add(root);
      if (i >= 0) { vp.content.children.splice(vp.content.children.indexOf(root), 1); vp.content.children.splice(i, 0, root); }
      vp.select(root);
      commit(label);
      render();
    } catch (e) { toast(e.message, "err"); }
    busy(null);
  }, 30);
}

function modifyModel(o, text) {
  if (!text.trim()) return;
  const [spec, changes] = parseModification(text, o.userData.spec);
  if (!changes.length) return toast("Modification non reconnue. Ex. « ajoute 2 cornes », « agrandis les ailes », « yeux rouges lumineux ».", "err", 6000);
  toast(changes.join(" · "), "ok", 5000);
  regenerate(o, spec, "Modifier par texte");
}

// ------------------------------------------------------------------ Bibliothèque
async function saveToLibrary(o) {
  try {
    const g = new THREE.Group(); g.name = "Scène";
    const c = cloneDeep(o); c.position.set(0, 0, 0); g.add(c);
    const buf = await exportGLB(g);
    await store.put("library", { id: `m${Date.now()}`, name: o.name, category: o.userData.spec?.category_label || "Divers", glb: buf, thumb: vp.snapshot(256), created: Date.now() });
    toast(`« ${o.name} » enregistré dans la bibliothèque`, "ok");
  } catch (e) { toast(e.message, "err"); }
}

async function renderLibrary(body) {
  body.append(h("h3", {}, "Bibliothèque"));
  if (vp.selection) body.append(h("button", { class: "btn full", onclick: () => saveToLibrary(vp.selection).then(render) }, `Enregistrer « ${vp.selection.name} »`));
  const items = (await store.all("library")).sort((a, b) => b.created - a.created);
  if (S.tab !== "library") return;
  if (!items.length) { body.append(h("p", { class: "muted" }, "Vide. Sélectionnez un modèle puis « Enregistrer ».")); return; }
  const grid = h("div", { class: "grid", style: "margin-top:10px" });
  for (const it of items) grid.append(h("div", { class: "card" }, h("img", { src: it.thumb, alt: it.name }),
    h("div", { class: "m" }, h("b", {}, it.name), h("span", { class: "muted" }, it.category)),
    h("div", { class: "a" },
      h("button", { onclick: async () => { const objs = await deserialize(it.glb); for (const ob of objs) { ob.name = uniqueName(ob.name); storeRest(ob); placeBeside(ob); vp.add(ob); } commit(`Ajouter ${it.name}`); toast("Ajouté à la scène", "ok"); } }, "Ajouter"),
      h("button", { onclick: async () => { const n = await promptBox("Renommer", it.name); if (n) { it.name = n; await store.put("library", it); render(); } } }, "Nom"),
      h("button", { onclick: async () => { if (await confirmBox(`Supprimer « ${it.name} » de la bibliothèque ?`)) { await store.del("library", it.id); render(); } } }, "✕"))));
  body.append(grid);
}

// ------------------------------------------------------------------ Export & projets
function renderExport(body) {
  body.append(h("h3", {}, "Exporter"));
  const scope = h("select", {}, h("option", { value: "sel" }, vp.selection ? `Objet : ${vp.selection.name}` : "Objet sélectionné (aucun)"), h("option", { value: "all", selected: !vp.selection }, "Toute la scène"));
  body.append(field("Contenu", scope));
  const go = async (fmt) => {
    const target = scope.value === "sel" && vp.selection ? vp.selection : vp.content;
    if (!target.children?.length && target === vp.content) return toast("La scène est vide.");
    busy("Export…");
    await nextFrame();
    try {
      vp.stop();
      let buf, mime;
      const g = target === vp.content ? target : (() => { const gg = new THREE.Group(); gg.name = "Scène"; gg.add(cloneDeep(target)); return gg; })();
      if (fmt === "glb") { buf = await exportGLB(g); mime = "model/gltf-binary"; }
      else if (fmt === "obj") { buf = exportOBJ(g); mime = "text/plain"; }
      else { buf = exportSTL(g); mime = "model/stl"; }
      const info = verify(buf, fmt);
      const name = `${(target === vp.content ? S.project.name : target.name).replace(/[^\w\-àâéèêëîïôöùûüç ]/gi, "_")}.${fmt}`;
      saveFile(name, mime, buf);
      toast(`${name} vérifié (${Math.round(info.bytes / 1024)} Ko${info.animations ? `, ${info.animations} animation(s)` : ""})`, "ok", 5000);
    } catch (e) { toast(`Export impossible : ${e.message}`, "err", 6000); }
    busy(null);
  };
  body.append(h("div", { class: "row3" }, h("button", { class: "btn pri", onclick: () => go("glb") }, "GLB"), h("button", { class: "btn", onclick: () => go("obj") }, "OBJ"), h("button", { class: "btn", onclick: () => go("stl") }, "STL")));
  body.append(h("p", { class: "muted" }, "GLB : géométrie, matériaux, couleurs, squelette et animations (Unity, Unreal, Godot, Blender). STL : impression 3D. Chaque fichier est vérifié avant l'enregistrement."));
  body.append(h("h4", {}, "Projets"), h("div", { class: "row3" },
    h("button", { class: "btn", onclick: newProject }, "Nouveau"),
    h("button", { class: "btn", onclick: saveProjectAs }, "Enregistrer"),
    h("button", { class: "btn", onclick: openProject }, "Ouvrir")));
  body.append(h("button", { class: "btn full", style: "margin-top:8px", onclick: () => $("#glbin").click() }, "Importer un fichier GLB"));
}

async function newProject() {
  if (vp.content.children.length && !(await confirmBox("Commencer un nouveau projet ? La scène actuelle reste enregistrée dans « Ouvrir ».") )) return;
  await autosave();
  vp.stop(); vp.mixers.clear();
  vp.content.clear(); vp.select(null);
  S.project = { id: "courant", name: "Sans titre" };
  $("#pname").textContent = S.project.name;
  history.stack = []; history.index = -1; commit("Nouveau projet");
  render();
}
async function saveProjectAs() {
  const n = await promptBox("Nom du projet", S.project.name === "Sans titre" ? "Mon projet" : S.project.name);
  if (!n) return;
  S.project = { id: `p_${n}`, name: n };
  $("#pname").textContent = n;
  await autosave();
  toast(`Projet « ${n} » enregistré sur le téléphone`, "ok");
}
async function openProject() {
  const items = (await store.all("projects")).sort((a, b) => b.updated - a.updated);
  if (!items.length) return toast("Aucun projet enregistré.");
  const list = h("div", { class: "list" });
  const choice = await new Promise((res) => {
    for (const p of items) list.append(h("div", { class: "it", onclick: () => res(p) }, p.thumb ? h("img", { src: p.thumb, style: "width:48px;height:48px;border-radius:8px;object-fit:cover" }) : "", h("span", { class: "nm" }, p.id === "courant" ? `${p.name} (session)` : p.name), h("span", { class: "muted" }, new Date(p.updated).toLocaleDateString("fr"))));
    showModal("Ouvrir un projet", list, [["Annuler", () => res(null)]]);
  });
  closeModal();
  if (choice) await loadProject(choice);
}
async function loadProject(p) {
  busy("Ouverture…");
  try {
    vp.stop(); vp.mixers.clear(); vp.content.clear(); vp.select(null);
    for (const o of await deserialize(p.glb)) { storeRest(o); vp.content.add(o); }
    S.project = { id: p.id, name: p.name };
    $("#pname").textContent = p.name;
    history.stack = []; history.index = -1; commit("Ouverture");
    vp.frame();
  } catch (e) { toast(`Ouverture impossible : ${e.message}`, "err"); }
  busy(null);
  render();
}
$("#glbin").onchange = async (ev) => {
  const f = ev.target.files[0]; ev.target.value = "";
  if (!f) return;
  try {
    const objs = await deserialize(await f.arrayBuffer());
    for (const o of objs) { o.name = uniqueName(o.name || f.name); storeRest(o); placeBeside(o); vp.add(o); }
    commit(`Importer ${f.name}`); vp.frame(); toast("Importé", "ok");
  } catch (e) { toast(`Import impossible : ${e.message}`, "err"); }
};

// ------------------------------------------------------------------ menu & dialogues
function showModal(title, content, buttons) {
  const box = h("div", { class: "box" }, h("h3", {}, title), content, h("div", { class: "acts2" }, buttons.map(([l, fn, cls]) => h("button", { class: `btn ${cls || ""}`, onclick: fn }, l))));
  $("#modal").replaceChildren(h("div", { class: "back" }, box));
}
function closeModal() { $("#modal").replaceChildren(); }
function confirmBox(msg) { return new Promise((res) => showModal("Confirmation", h("p", {}, msg), [["Annuler", () => { closeModal(); res(false); }], ["Confirmer", () => { closeModal(); res(true); }, "dan"]])); }
function promptBox(title, value) {
  const i = h("input", { type: "text", value });
  return new Promise((res) => { showModal(title, i, [["Annuler", () => { closeModal(); res(null); }], ["Valider", () => { closeModal(); res(i.value.trim() || null); }, "pri"]]); setTimeout(() => i.focus(), 50); });
}
$("#menu").onclick = () => showModal("SUPERESSENCE 3D Mobile", h("div", {},
  h("div", { class: "list" },
    h("div", { class: "it", onclick: () => { closeModal(); newProject(); } }, "Nouveau projet"),
    h("div", { class: "it", onclick: () => { closeModal(); openProject(); } }, "Ouvrir un projet"),
    h("div", { class: "it", onclick: () => { closeModal(); saveProjectAs(); } }, "Enregistrer le projet"),
    h("div", { class: "it", onclick: () => { closeModal(); import("./claude.js").then((m) => m.openClaude()); } }, "✦ Claude (piloter l'app avec Claude)"),
    window.Android?.connectPc ? h("div", { class: "it", onclick: () => window.Android.connectPc() }, "Se connecter au studio PC (Blender)") : null),
  h("p", { class: "muted" }, "Version 0.2.0 — moteur 3D embarqué, fonctionne hors ligne. Génération procédurale (algorithmes), pas d'IA neuronale. Pour le moteur Blender complet et Claude (MCP), utilisez le studio PC.")), [["Fermer", closeModal]]);


// ------------------------------------------------------------------ API utilisée par Claude (relais MCP)
const findObj = (name) => {
  const o = vp.content.children.find((x) => x.name === name) || vp.content.children.find((x) => x.name.toLowerCase() === String(name || "").toLowerCase());
  if (!o) throw new Error(`Objet introuvable : « ${name} ». Objets : ${vp.content.children.map((x) => x.name).join(", ") || "aucun"}`);
  return o;
};
const colorArg = (c) => {
  if (!c) return null;
  if (Array.isArray(c)) return c.map(Number);
  if (/^#?[0-9a-f]{6}$/i.test(c)) return rgb(c.startsWith("#") ? c : `#${c}`);
  const p = parsePrompt(`objet ${c}`);
  return p.colors.primary || null;
};
function normSpec(spec) {
  for (const [k, v] of Object.entries(spec.colors || {})) spec.colors[k] = colorArg(v) || v;
  return spec;
}
function describe(o) {
  let tris = 0; o.traverse((m) => { if (m.isMesh) tris += (m.geometry.index ? m.geometry.index.count : m.geometry.attributes.position.count) / 3; });
  const sz = new THREE.Box3().setFromObject(o).getSize(new THREE.Vector3());
  return { name: o.name, generated: !!o.userData.generated, triangles: Math.round(tris), size_m: [sz.x, sz.y, sz.z].map((v) => +v.toFixed(3)), position: o.position.toArray().map((v) => +v.toFixed(3)), rigged: !!o.getObjectByProperty("isBone", true), animation: o.userData.clip?.name || null, spec: o.userData.spec ? { category: o.userData.spec.category, subtype: o.userData.spec.subtype, prompt: o.userData.spec.prompt, height: o.userData.spec.height, features: o.userData.spec.features, colors: o.userData.spec.colors } : null };
}
async function genAndAdd(spec, quality, replace = null) {
  busy("Claude génère…");
  await nextFrame();
  try {
    const { root, stats } = generate(spec, { quality: quality || S.quality });
    if (replace) {
      root.name = replace.name; root.position.copy(replace.position); root.rotation.copy(replace.rotation); root.scale.copy(replace.scale);
      vp.content.remove(replace);
    } else { root.name = uniqueName(root.name); placeBeside(root); }
    storeRest(root);
    vp.add(root);
    vp.frame(root);
    return { root, stats };
  } finally { busy(null); }
}
const after = (label) => { commit(label); render(); updatePlay(); };
window.SEAPI = {
  get_scene_info: () => ({ objects: vp.content.children.map(describe), lighting: vp.lightPreset, selected: vp.selection?.name || null, project: S.project.name }),
  async generate_model({ prompt, quality, spec_overrides }) {
    if (!prompt) throw new Error("prompt manquant");
    const spec = parsePrompt(prompt);
    if (spec_overrides) for (const [k, v] of Object.entries(spec_overrides)) spec[k] = v && typeof v === "object" && !Array.isArray(v) ? { ...(spec[k] || {}), ...v } : v;
    normSpec(spec);
    const { root, stats } = await genAndAdd(spec, quality);
    after(`Claude : ${root.name}`);
    return { created: root.name, triangles: stats.triangles, ms: stats.ms, interpretation: { category: spec.category, subtype: spec.subtype, features: spec.features, colors: spec.colors, height: spec.height, notes: spec.notes } };
  },
  async modify_model({ name, instruction, changes }) {
    const o = findObj(name);
    if (!o.userData.spec) throw new Error("Cet objet n'a pas été généré : modification par texte impossible.");
    let spec = JSON.parse(JSON.stringify(o.userData.spec)), list = [];
    if (instruction) [spec, list] = parseModification(instruction, spec);
    if (changes) for (const [k, v] of Object.entries(changes)) { spec[k] = v && typeof v === "object" && !Array.isArray(v) ? { ...(spec[k] || {}), ...v } : v; list.push(`${k} : ${JSON.stringify(v)}`); }
    if (!list.length) throw new Error("Aucune modification reconnue.");
    normSpec(spec);
    await genAndAdd(spec, o.userData.params?.quality, o);
    after(`Claude modifie ${name}`);
    return { modified: name, changes: list };
  },
  set_material({ name, preset, color, description }) {
    const o = findObj(name);
    let mat;
    if (description) { const r = parseMaterialText(description); mat = makeMaterial(r.preset, r.params); }
    else mat = makeMaterial(preset || "plain", { color: colorArg(color) || undefined });
    o.traverse((x) => { if (x.isMesh) { if (x.geometry.attributes.color) x.geometry.deleteAttribute("color"); x.material = mat; } });
    after("Claude : matériau");
    return { name, material: mat.userData.preset };
  },
  transform_object({ name, position, rotation_y, scale }) {
    const o = findObj(name);
    if (position) o.position.set(...position.map(Number));
    if (rotation_y != null) o.rotation.y = THREE.MathUtils.degToRad(+rotation_y);
    if (scale) o.scale.setScalar(Math.max(0.001, +scale));
    after("Claude : transformer");
    return describe(o);
  },
  animate({ name, motions, object_motion, amplitude }) {
    const o = findObj(name);
    vp.stop();
    let clip;
    if (object_motion) clip = makeObjectClip(o, object_motion, { amplitude: amplitude || 1 });
    else {
      const ms = (motions || ["idle"]).filter((m) => MOTIONS[m]);
      if (!ms.length) throw new Error(`Mouvements possibles : ${Object.keys(MOTIONS).join(", ")}`);
      clip = makeClip(o, ms, { amplitude: amplitude || 1, seconds: ms.includes("walk") ? 1.2 : ms.includes("run") ? 0.8 : 1.6 });
      if (!clip) throw new Error("Cet objet n'a pas de squelette : utilisez object_motion (spin, bounce, float, pulse, sway).");
    }
    o.userData.clip = clip;
    vp.select(o);
    vp.play(o, clip);
    after("Claude : animation");
    return { name, animation: clip.name, tracks: clip.tracks.length };
  },
  stop_animation() { vp.stop(); updatePlay(); return { stopped: true }; },
  add_shape({ shape, color, name }) {
    const labels = { box: "Cube", sphere: "Sphère", cylinder: "Cylindre", cone: "Cône", torus: "Tore", plane: "Plan" };
    if (!labels[shape]) throw new Error(`Formes : ${Object.keys(labels).join(", ")}`);
    addPrimitive(shape, name || labels[shape]);
    const o = vp.content.children.at(-1);
    if (color) o.material = makeMaterial("plain", { color: colorArg(color) });
    render();
    return describe(o);
  },
  duplicate_object({ name }) {
    const o = findObj(name);
    const c = cloneDeep(o); c.name = uniqueName(o.name);
    c.position.x += new THREE.Box3().setFromObject(o).getSize(new THREE.Vector3()).x + 0.3;
    storeRest(c); vp.add(c); after("Claude : dupliquer");
    return { created: c.name };
  },
  delete_object({ name, confirm }) {
    const o = findObj(name);
    if (!confirm) return { confirmation_required: true, message: `Supprimer « ${name} » ? Demandez l'accord de l'utilisateur puis relancez avec confirm=true.` };
    vp.content.remove(o); vp.select(null); after("Claude : supprimer");
    return { deleted: name };
  },
  set_lighting({ preset, background }) {
    if (background) vp.customBg = background.startsWith("#") ? background : `#${background}`;
    vp.setLighting(preset || vp.lightPreset);
    render();
    return { lighting: vp.lightPreset };
  },
  async view_scene({ name, view }) {
    const o = name ? findObj(name) : null;
    if (view) vp.view(view);
    const sheet = $("#sheet").className;
    $("#sheet").className = "closed";
    await nextFrame();
    vp.frame(o);
    await nextFrame();
    const img = vp.snapshot(640).split(",")[1];
    $("#sheet").className = sheet;
    return { image: img, objects: vp.content.children.length };
  },
  async save_to_library({ name }) { await saveToLibrary(findObj(name)); return { saved: name }; },
  async export_model({ name, format = "glb" }) {
    const target = name ? findObj(name) : vp.content;
    const g = target === vp.content ? target : (() => { const gg = new THREE.Group(); gg.name = "Scène"; gg.add(cloneDeep(target)); return gg; })();
    vp.stop();
    const buf = format === "obj" ? exportOBJ(g) : format === "stl" ? exportSTL(g) : await exportGLB(g);
    const info = verify(buf, format);
    saveFile(`${name || S.project.name}.${format}`, { glb: "model/gltf-binary", obj: "text/plain", stl: "model/stl" }[format] || "application/octet-stream", buf);
    return { verified: info, note: "L'utilisateur choisit l'emplacement d'enregistrement sur son téléphone." };
  },
  undo() { if (!history.canUndo()) return { undone: false }; const l = history.stack[history.index].label; history.restore(history.index - 1, vp.content); afterRestore(`Annulé : ${l}`); return { undone: l }; },
};

// --- modélisation libre (outils avancés pour Claude)
const parseColorWord = (c) => parsePrompt(`objet ${c}`).colors.primary || null;
function findPart(o, part) {
  if (!part) return null;
  let found = null;
  o.traverse((x) => { if (!found && (x.name === part || x.userData.part === part)) found = x; });
  if (!found) { const names = []; o.traverse((x) => { if (x !== o && (x.isMesh || x.userData.part)) names.push(x.name); }); throw new Error(`Partie introuvable : « ${part} ». Parties : ${[...new Set(names)].slice(0, 60).join(", ")}`); }
  return found;
}
function meshesOf(o) { const m = []; o.traverse((x) => { if (x.isMesh) m.push(x); }); return m; }
function nearestNormal(o, wp) {
  let best = Infinity, n = new THREE.Vector3(0, 1, 0);
  const tmp = new THREE.Vector3(), nn = new THREE.Vector3();
  for (const m of meshesOf(o)) {
    const pos = m.geometry.attributes.position, nor = m.geometry.attributes.normal;
    if (!nor) continue;
    const step = Math.max(1, Math.floor(pos.count / 4000));
    for (let i = 0; i < pos.count; i += step) {
      tmp.fromBufferAttribute(pos, i).applyMatrix4(m.matrixWorld);
      const d = tmp.distanceToSquared(wp);
      if (d < best) { best = d; nn.fromBufferAttribute(nor, i).transformDirection(m.matrixWorld); n.copy(nn); }
    }
  }
  return n;
}
async function multiView(o, views) {
  const sheet = $("#sheet").className;
  $("#sheet").className = "closed";
  vp.box.visible = false; vp.gizmo.detach();
  await nextFrame();
  const cam = vp.camera.clone(), target = vp.controls.target.clone();
  const tiles = [];
  for (const v of views) { vp.view(v); vp.frame(o); vp.controls.update(); await nextFrame(); tiles.push(vp.snapshot(360)); }
  vp.camera.position.copy(cam.position); vp.controls.target.copy(target); vp.camera.updateProjectionMatrix();
  $("#sheet").className = sheet;
  if (vp.selection) { vp.box.visible = true; vp._attach(); }
  const cols = tiles.length > 1 ? 2 : 1, rows = Math.ceil(tiles.length / cols);
  const cv = document.createElement("canvas"); cv.width = 360 * cols; cv.height = 360 * rows;
  const g = cv.getContext("2d");
  const imgs = await Promise.all(tiles.map((src) => new Promise((r) => { const i = new Image(); i.onload = () => r(i); i.src = src; })));
  imgs.forEach((im, i) => { g.drawImage(im, (i % cols) * 360, Math.floor(i / cols) * 360); g.fillStyle = "rgba(0,0,0,.55)"; g.fillRect((i % cols) * 360, Math.floor(i / cols) * 360, 90, 22); g.fillStyle = "#fff"; g.font = "14px sans-serif"; g.fillText(views[i], (i % cols) * 360 + 6, Math.floor(i / cols) * 360 + 16); });
  return cv.toDataURL("image/jpeg", 0.85).split(",")[1];
}
Object.assign(window.SEAPI, {
  async build_organic(def) {
    busy("Claude modélise…");
    await nextFrame();
    try {
      const root = buildOrganicModel(def, parseColorWord, def.quality || S.quality);
      const existing = vp.content.children.find((x) => x.name === root.name);
      if (existing && def.replace !== false) { root.position.copy(existing.position); vp.content.remove(existing); }
      else { root.name = uniqueName(root.name); placeBeside(root); }
      storeRest(root); vp.add(root); vp.frame(root);
      after(`Claude modélise ${root.name}`);
      return describe(root);
    } finally { busy(null); }
  },
  add_part(p) {
    let model = p.model ? vp.content.children.find((x) => x.name === p.model) : null;
    let isNew = false;
    if (p.model && !model) { model = new THREE.Group(); model.name = p.model; isNew = true; }
    const part = makePart(p, parseColorWord);
    if (model) {
      const parentBone = p.bone ? findPart(model, p.bone) : null;
      if (parentBone && parentBone.isBone) {
        model.updateMatrixWorld(true);
        const inv = parentBone.matrixWorld.clone().invert().multiply(model.matrixWorld);
        part.applyMatrix4(inv);
        parentBone.add(part);
      } else model.add(part);
      if (isNew) { placeBeside(model); vp.content.add(model); }
    } else { part.name = uniqueName(part.name); placeBeside(part); vp.content.add(part); }
    vp.select(model || part);
    after(`Claude : pièce ${part.name}`);
    return { added: part.name, model: model?.name || null, parts: model ? model.children.length : 1 };
  },
  remove_part({ name, part }) { const o = findObj(name), x = findPart(o, part); x.parent.remove(x); after("Claude : retirer une pièce"); return { removed: part }; },
  transform_part({ name, part, position, rotation, scale }) {
    const x = findPart(findObj(name), part);
    if (position) x.position.set(...position.map(Number));
    if (rotation) x.rotation.set(...rotation.map((d) => THREE.MathUtils.degToRad(+d)));
    if (scale !== undefined) Array.isArray(scale) ? x.scale.set(...scale.map(Number)) : x.scale.setScalar(+scale);
    after("Claude : ajuster une pièce");
    return { part, position: x.position.toArray(), scale: x.scale.toArray() };
  },
  get_object_details({ name }) {
    const o = findObj(name);
    const parts = [], bones = [];
    o.updateMatrixWorld(true);
    o.traverse((x) => {
      if (x.isBone) bones.push({ name: x.name, role: x.userData.role || "", world: x.getWorldPosition(new THREE.Vector3()).toArray().map((v) => +v.toFixed(3)) });
      else if (x.isMesh) { const b = new THREE.Box3().setFromObject(x); parts.push({ name: x.name, min: b.min.toArray().map((v) => +v.toFixed(3)), max: b.max.toArray().map((v) => +v.toFixed(3)), material: x.material.userData?.preset || null, vertices: x.geometry.attributes.position.count }); }
    });
    return { ...describe(o), parts: parts.slice(0, 80), bones: bones.slice(0, 120), definition: o.userData.def || null };
  },
  sculpt({ name, part, points, brush = "draw", radius = 0.1, strength = 0.6, invert = false, symmetry = false, repeat = 1 }) {
    const o = findObj(name);
    const target = part ? findPart(o, part) : o;
    if (!Array.isArray(points) || !points.length) throw new Error("points [[x,y,z], ...] requis (coordonnées monde)");
    vp.stop();
    o.updateMatrixWorld(true);
    const b = { kind: brush, radius: +radius, strength: +strength, invert: !!invert };
    let n = 0;
    for (let k = 0; k < Math.min(10, repeat); k++) for (const q of points) {
      const pts = [new THREE.Vector3(+q[0], +q[1], +q[2])];
      if (symmetry) pts.push(new THREE.Vector3(2 * o.position.x - q[0], +q[1], +q[2]));
      for (const wp of pts) { const nrm = q.length >= 6 ? new THREE.Vector3(+q[3], +q[4], +q[5]).normalize() : nearestNormal(target, wp); for (const m of meshesOf(target)) sculptMesh(m, wp, nrm, b); n++; }
    }
    after("Claude : sculpter");
    return { strokes: n, brush };
  },
  paint({ name, part, points, color, radius = 0.1, strength = 1, fill = false }) {
    const o = findObj(name);
    const target = part ? findPart(o, part) : o;
    const c = colorArg(color);
    if (!c) throw new Error("couleur invalide");
    const hexc = "#" + c.map((x) => Math.round(Math.max(0, Math.min(1, x)) * 255).toString(16).padStart(2, "0")).join("");
    o.updateMatrixWorld(true);
    if (fill) for (const m of meshesOf(target)) { m.geometry.deleteAttribute("color"); m.material = m.material.clone(); m.material.vertexColors = false; m.material.color.setRGB(c[0], c[1], c[2], THREE.SRGBColorSpace); }
    else for (const q of points || []) for (const m of meshesOf(target)) paintMesh(m, new THREE.Vector3(+q[0], +q[1], +q[2]), { color: hexc, radius: +radius, strength: +strength });
    after("Claude : peindre");
    return { painted: part || name };
  },
  deform({ name, part, operation, amount = 1, axis = "y" }) {
    const o = findObj(name);
    const target = part ? findPart(o, part) : o;
    for (const m of meshesOf(target)) deformMesh(m, operation, +amount, axis);
    after(`Claude : ${operation}`);
    return { deformed: part || name, operation };
  },
  async view_scene({ name, view, views }) {
    const o = name ? findObj(name) : null;
    const list = views && views.length ? views : view ? [view] : ["persp", "front", "right", "top"];
    return { image: await multiView(o, list), views: list, objects: vp.content.children.map((x) => x.name) };
  },
  set_material({ name, part, preset, color, description, emissive, roughness, metallic }) {
    const o = findObj(name);
    const target = part ? findPart(o, part) : o;
    let mat;
    if (description) { const r = parseMaterialText(description); mat = makeMaterial(r.preset, r.params); }
    else { mat = makeMaterial(preset || "plain", { color: colorArg(color) || undefined, roughness, metallic, emission: emissive ? colorArg(emissive === true ? color : emissive) : null, emission_strength: emissive ? 2 : 0 }); }
    target.traverse((x) => { if (x.isMesh) { if (x.geometry.attributes.color) x.geometry.deleteAttribute("color"); x.material = mat; } });
    after("Claude : matériau");
    return { name, part: part || null, material: mat.userData.preset };
  },
});

window.SEUI = { toast, render, showModal, closeModal };

// ------------------------------------------------------------------ démarrage
async function boot() {
  buildTools();
  updateUndo();
  try {
    const p = await store.get("projects", "courant");
    if (p?.glb) { await loadProject(p); toast("Session précédente restaurée"); }
    else commit("Départ");
  } catch { commit("Départ"); }
  render();
  vp.frame();
}
window.addEventListener("beforeunload", () => autosave());
document.addEventListener("visibilitychange", () => { if (document.hidden) autosave(); });
boot();
import("./claude.js").then((m) => m.autoStart());
window.SE = { vp, S, generate, parsePrompt };
