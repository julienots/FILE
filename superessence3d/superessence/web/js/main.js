// SUPERESSENCE 3D STUDIO — contrôleur principal de l'interface.
import { api, connect, emit, fileUrl, getJSON, on, upload } from "./api.js";
import { Viewport } from "./viewport.js";
import {
  $, $$, btn, checkbox, colorInput, confirmBox, fmt, h, hexFromRgb, icon, ICONS, modal, numInput, pickFile, prompt,
  rnaEditor, row, run, section, select, slider, toast, vecInput,
} from "./widgets.js";
import { initGenerate } from "./generate.js";
import { initLibrary, refreshLibrary } from "./library.js";

export const S = {
  info: null, state: null, details: null, tool: "select", mode: "object", propTab: "object", bottomTab: "history",
  history: [], logs: [], errors: 0, jobs: new Map(), generations: [], ws: "studio", objectNames: [],
};

// --------------------------------------------------------------------------- viewport
const vp = new Viewport($("#viewport"), {
  onSelect: (name, add, alt) => selectObject(name, add, alt),
  onTransform: (updates) => transformObjects(updates),
  onStroke: (name, points, end) => strokeQueue(name, points, end),
  onEditSelection: () => renderProps(),
  onEditTranslate: (offset) => editOp("translate", { offset }),
  onInfo: (s) => { $("#vp-info").textContent = s; },
});
export { vp };

// --------------------------------------------------------------------------- rafraîchissement
let refreshTimer = null;
let refreshing = false;
let refreshAgain = false;
export function scheduleRefresh(delay = 40) {
  clearTimeout(refreshTimer);
  refreshTimer = setTimeout(refresh, delay);
}

export async function refresh() {
  if (refreshing) { refreshAgain = true; return; }
  refreshing = true;
  try {
    const [state, info] = await Promise.all([
      api("get_viewport_state", {}, { quiet: true }),
      api("get_scene_info", {}, { quiet: true }),
    ]);
    S.state = state;
    S.info = info;
    S.objectNames = info.objects.map((o) => o.name);
    await vp.sync(state);
    if (vp.mode === "edit" && vp.editData) await vp.loadEdit(vp.editData.name).catch(() => setMode("object"));
    renderOutliner();
    await loadDetails();
    renderProps();
    renderTimeline();
    $("#project-name").textContent = (info.status.project_path || "Sans titre").split(/[\\/]/).pop() + (info.status.dirty ? " •" : "");
  } catch (e) {
    console.error(e);
  } finally {
    refreshing = false;
    if (refreshAgain) { refreshAgain = false; scheduleRefresh(10); }
  }
}

async function loadDetails() {
  const act = S.info?.active;
  S.details = null;
  if (!act || !S.objectNames.includes(act)) return;
  try { S.details = await api("get_object_details", { name: act }, { quiet: true }); } catch { S.details = null; }
}

// --------------------------------------------------------------------------- sélection / transformation
async function selectObject(name, add, alt) {
  if (S.mode !== "object" && name && name !== S.info?.active) setMode("object");
  if (alt && name) { // Alt+clic : sélectionne la racine du modèle généré
    let n = name;
    const byName = Object.fromEntries((S.info?.objects || []).map((o) => [o.name, o]));
    while (byName[n]?.parent) n = byName[n].parent;
    name = n;
  }
  const params = name ? { mode: add ? "toggle" : "set", names: [name] } : { mode: "none" };
  const r = await api("select", params, { quiet: true });
  vp.setSelection(r.selected, r.active);
  scheduleRefresh(0);
}

async function transformObjects(updates) {
  try {
    for (const u of updates) await api("transform_object", { name: u.name, matrix_world: u.matrix_world }, { quiet: true });
  } catch (e) { toast(e.message, "error"); }
  scheduleRefresh(0);
}

// --------------------------------------------------------------------------- sculpture / peinture
let strokeBusy = Promise.resolve();
function strokeQueue(name, points, end) {
  if (!points.length) return;
  const b = vp.brush;
  strokeBusy = strokeBusy.then(async () => {
    try {
      if (S.tool === "sculpt") {
        await api("sculpt_object", { name, points, brush: b.kind, radius: b.radius, strength: b.strength, invert: b.invert, symmetry_x: b.symmetry }, { quiet: true });
      } else if (S.tool === "vpaint") {
        await api("vertex_paint", { name, points, radius: b.radius, color: b.color, strength: b.strength }, { quiet: true });
      } else if (S.tool === "wpaint") {
        if (!b.group) { toast("Choisissez un os / groupe de sommets dans les options du pinceau.", "warn"); return; }
        await api("weight_paint", { name, points, radius: b.radius, group: b.group, weight: b.strength, mode: b.invert ? "subtract" : "add" }, { quiet: true });
        await showWeights();
      }
    } catch (e) { toast(e.message, "error"); }
  });
}

async function showWeights() {
  const name = S.info?.active;
  const e = vp.objects.get(name);
  if (!e || !vp.brush.group) return;
  const d = await api("get_mesh", { name, weights: vp.brush.group }, { quiet: true });
  vp._buildMesh(e, d);
  e.mesh.material.forEach((m) => { m.vertexColors = true; m.color.set(0xffffff); m.needsUpdate = true; });
  e.signature = null;
}

// --------------------------------------------------------------------------- modes et outils
const TOOLS = [
  ["select", "select", "Sélection (Q)"],
  ["move", "move", "Déplacer (G)"],
  ["rotate", "rotate", "Rotation (R)"],
  ["scale", "scale", "Échelle (S)"],
  "-",
  ["model", "model", "Modélisation — mode édition (Tab)"],
  ["sculpt", "sculpt", "Sculpture"],
  ["vpaint", "paint", "Peinture de sommets"],
  ["wpaint", "weight", "Peinture de poids (rigging)"],
  "-",
  ["material", "material", "Matériaux"],
  ["texture", "texture", "Textures"],
];

function buildToolbar() {
  const tb = $("#toolbar");
  tb.innerHTML = "";
  for (const t of TOOLS) {
    if (t === "-") { tb.append(h("div", { class: "tool-sep" })); continue; }
    const [id, ic, tip] = t;
    const b = h("button", { class: "tool", "data-tool": id, "data-tip": tip, html: ICONS[ic], onclick: () => setTool(id) });
    tb.append(b);
  }
  updateToolbar();
}

function updateToolbar() {
  $$(".tool").forEach((b) => b.classList.toggle("active", b.dataset.tool === S.tool || (b.dataset.tool === "model" && S.mode === "edit")));
}

export function setTool(id) {
  if (id === "material") { S.propTab = "material"; renderProps(); return; }
  if (id === "texture") { S.propTab = "texture"; renderProps(); return; }
  if (id === "model") { setMode(S.mode === "edit" ? "object" : "edit"); return; }
  if (["sculpt", "vpaint", "wpaint"].includes(id)) {
    if (!S.info?.active || vp.objects.get(S.info.active)?.type !== "MESH") {
      toast("Sélectionnez d'abord un maillage (cliquez dessus).", "warn");
      return;
    }
    if (S.mode === "edit") setMode("object");
    S.mode = id;
    vp.setMode(id === "vpaint" ? "vpaint" : "object");
  } else if (S.mode !== "edit") {
    S.mode = "object";
    vp.setMode("object");
  }
  S.tool = id;
  vp.setTool(id);
  updateToolbar();
  renderToolOptions();
  renderModeSwitch();
  if (id === "wpaint") showWeights().catch(() => {});
  else if (id !== "wpaint") scheduleRefresh(0);
}

export async function setMode(mode) {
  if (mode === "edit") {
    const act = S.info?.active;
    if (!act || vp.objects.get(act)?.type !== "MESH") { toast("Sélectionnez un maillage à éditer.", "warn"); return; }
    S.mode = "edit";
    vp.setMode("edit");
    try { await vp.loadEdit(act); } catch (e) { toast(e.message, "error"); setMode("object"); return; }
    S.propTab = "geometry";
  } else {
    S.mode = "object";
    vp.setMode("object");
    if (["sculpt", "vpaint", "wpaint"].includes(S.tool)) { S.tool = "select"; vp.setTool("select"); }
  }
  updateToolbar();
  renderToolOptions();
  renderModeSwitch();
  renderProps();
}

function renderModeSwitch() {
  const ms = $("#mode-switch");
  ms.innerHTML = "";
  const modes = [["object", "Objet"], ["edit", "Édition"], ["sculpt", "Sculpture"], ["vpaint", "Peinture"], ["wpaint", "Poids"]];
  for (const [id, label] of modes) {
    ms.append(h("button", { class: S.mode === id ? "active" : "", onclick: () => (id === "object" ? setMode("object") : id === "edit" ? setMode("edit") : setTool(id)) }, label));
  }
  const sh = $("#shading-switch");
  sh.innerHTML = "";
  for (const [id, label] of [["solid", "Solide"], ["material", "Matériau"], ["wire", "Fil de fer"], ["xray", "Rayons X"]]) {
    sh.append(h("button", { class: vp.shading === id ? "active" : "", onclick: () => { vp.setShading(id); renderModeSwitch(); } }, label));
  }
  sh.append(h("button", { title: "Rendu réel par Blender (Cycles)", onclick: () => quickRender() }, "Rendu"));
  const vs = $("#view-switch");
  vs.innerHTML = "";
  for (const [id, label] of [["persp", "Persp."], ["front", "Face"], ["right", "Droite"], ["top", "Dessus"]]) {
    vs.append(h("button", { onclick: () => vp.view(id) }, label));
  }
  vs.append(h("button", { title: "Voir par la caméra active", onclick: () => vp.cameraView() || toast("Aucune caméra active.", "warn") }, "Caméra"));
  vs.append(h("button", { title: "Cadrer la sélection (F)", onclick: () => vp.frameObjects([...vp.selected]) }, "Cadrer"));
}

function renderToolOptions() {
  const box = $("#vp-tool-options");
  box.innerHTML = "";
  const b = vp.brush;
  if (["sculpt", "vpaint", "wpaint"].includes(S.tool)) {
    box.classList.remove("hidden");
    const title = { sculpt: "Pinceau de sculpture", vpaint: "Peinture de sommets", wpaint: "Peinture de poids" }[S.tool];
    box.append(h("h4", {}, title));
    if (S.tool === "sculpt") {
      box.append(row("Pinceau", select([["draw", "Dessiner"], ["clay", "Argile"], ["inflate", "Gonfler"], ["smooth", "Lisser"], ["flatten", "Aplatir"], ["scrape", "Racler"], ["pinch", "Pincer"], ["crease", "Pli"], ["noise", "Bruit"]], b.kind, (v) => { b.kind = v; })));
      box.append(row("Inverser", checkbox(b.invert, (v) => { b.invert = v; })));
      box.append(row("Symétrie X", checkbox(b.symmetry, (v) => { b.symmetry = v; })));
    }
    if (S.tool === "vpaint") box.append(row("Couleur", colorInput(b.color, (v) => { b.color = v; })));
    if (S.tool === "wpaint") {
      const groups = S.details?.vertex_groups || [];
      if (!b.group && groups.length) b.group = groups[0];
      box.append(row("Os / groupe", select(groups.length ? groups.map((g) => [g, g]) : [["", "(créez un squelette)"]], b.group, (v) => { b.group = v; showWeights(); })));
      box.append(row("Retirer", checkbox(b.invert, (v) => { b.invert = v; })));
    }
    box.append(row("Rayon (m)", slider(b.radius, (v) => { b.radius = v; }, { min: 0.005, max: 2, step: 0.005 })));
    box.append(row(S.tool === "wpaint" ? "Poids" : "Force", slider(b.strength, (v) => { b.strength = v; }, { min: 0, max: S.tool === "sculpt" ? 3 : 1, step: 0.01 })));
    const tip = S.tool === "sculpt"
      ? "Clic gauche : sculpter. Clic molette : tourner. Clic droit : déplacer la vue."
      : "Clic gauche : peindre. Clic molette : tourner la vue.";
    box.append(h("div", { class: "hint" }, tip));
    if (S.tool === "sculpt") box.append(btn("Densifier le maillage (remaillage voxel)", async () => {
      const vs = await prompt("Densifier pour sculpter", "Taille des voxels (m)", "0.02");
      if (vs) { await run("remesh_for_sculpt", { name: S.info.active, voxel_size: parseFloat(vs) }, { success: "Maillage densifié" }); }
    }, "small"));
  } else if (S.mode === "edit") {
    box.classList.remove("hidden");
    box.append(h("h4", {}, "Mode édition"));
    box.append(row("Sélection", select([["face", "Faces"], ["vertex", "Sommets"]], vp.selectMode, (v) => { vp.selectMode = v; vp.editSel.clear(); vp._drawEdit(); renderProps(); })));
    box.append(h("div", { class: "hint" }, "Clic : sélectionner · Maj+clic : ajouter. Outil Déplacer (G) pour bouger la sélection. Opérations dans l'onglet Géométrie."));
  } else box.classList.add("hidden");
}

// --------------------------------------------------------------------------- édition polygonale
async function editOp(operation, extra = {}) {
  const name = vp.editData?.name || S.info?.active;
  if (!name) return;
  const sel = [...vp.editSel];
  const key = vp.selectMode === "face" ? "faces" : "vertices";
  const params = { name, operation, ...extra };
  if (sel.length && !("all" in extra)) params[key] = sel;
  try {
    const r = await run("modify_geometry", params);
    if (r?.selected_faces && vp.selectMode === "face") vp.editSel = new Set(r.selected_faces);
    scheduleRefresh(0);
  } catch { /* message déjà affiché */ }
}

// --------------------------------------------------------------------------- outliner
function renderOutliner() {
  const tree = $("#outliner-tree");
  const info = S.info;
  if (!info) return;
  tree.innerHTML = "";
  const objs = Object.fromEntries(info.objects.map((o) => [o.name, o]));
  const children = {};
  for (const o of info.objects) if (o.parent && objs[o.parent]) (children[o.parent] ||= []).push(o.name);
  const typeIcon = { MESH: "mesh", EMPTY: "empty", LIGHT: "light", CAMERA: "camera", ARMATURE: "armature" };
  S.collapsed ||= new Set();
  const addObj = (name, depth) => {
    const o = objs[name];
    const kids = children[name] || [];
    const collapsed = S.collapsed.has(name);
    const r = h("div", { class: `ol-row ${o.selected ? "selected" : ""} ${info.active === name ? "active" : ""} ${o.hidden ? "hidden-obj" : ""}`, style: { paddingLeft: `${8 + depth * 14}px` } },
      h("span", { class: "tw", onclick: (e) => { e.stopPropagation(); collapsed ? S.collapsed.delete(name) : S.collapsed.add(name); renderOutliner(); } }, kids.length ? (collapsed ? "▸" : "▾") : ""),
      icon(typeIcon[o.type] || "mesh"),
      h("span", { class: "nm", title: name }, name + (o.generated ? "  ✦" : "")),
      h("button", { class: "eye", title: o.hidden ? "Afficher" : "Masquer", html: o.hidden ? ICONS.eyeoff : ICONS.eye, style: { width: "18px", height: "16px" },
        onclick: (e) => { e.stopPropagation(); run("set_visibility", { name, hidden: !o.hidden }).then(() => scheduleRefresh(0)); } }));
    r.addEventListener("click", (e) => selectObject(name, e.shiftKey || e.ctrlKey, false));
    r.addEventListener("dblclick", async () => {
      const nn = await prompt("Renommer", "Nouveau nom", name);
      if (nn && nn !== name) { await run("rename_object", { name, new_name: nn }); scheduleRefresh(0); }
    });
    r.addEventListener("contextmenu", (e) => { e.preventDefault(); objectContextMenu(name, e); });
    tree.append(r);
    if (!collapsed) kids.forEach((k) => addObj(k, depth + 1));
  };
  const addCol = (col, depth) => {
    if (depth > 0) tree.append(h("div", { class: "ol-row ol-col", style: { paddingLeft: `${8 + (depth - 1) * 14}px` } }, h("span", { class: "tw" }, "▾"), icon("collection"), h("span", { class: "nm" }, col.name),
      h("button", { class: "eye", html: col.hide_viewport ? ICONS.eyeoff : ICONS.eye, style: { width: "18px", height: "16px" }, onclick: (e) => { e.stopPropagation(); run("set_collection_visibility", { collection: col.name, hidden: !col.hide_viewport }).then(() => scheduleRefresh(0)); } })));
    for (const n of col.objects) if (objs[n] && !(objs[n].parent && objs[objs[n].parent])) addObj(n, depth);
    for (const c of col.children) addCol(c, depth + 1);
  };
  addCol(info.collections, 0);
  if (!info.objects.length) tree.append(h("div", { class: "empty" }, "Scène vide — créez ou générez un modèle."));
  const acts = $("#outliner-actions");
  acts.innerHTML = "";
  acts.append(
    h("button", { class: "icon-btn", title: "Nouvelle collection", html: ICONS.collection, onclick: async () => { const n = await prompt("Nouvelle collection", "Nom", "Collection"); if (n) { await run("create_collection", { name: n }); scheduleRefresh(0); } } }),
    h("button", { class: "icon-btn", title: "Dupliquer la sélection (Maj+D)", html: ICONS.copy, onclick: () => duplicateSelection() }),
    h("button", { class: "icon-btn", title: "Supprimer la sélection (Suppr)", html: ICONS.trash, onclick: () => deleteSelection() }),
  );
}

function objectContextMenu(name, ev) {
  closeMenus();
  const items = [
    ["Renommer", async () => { const nn = await prompt("Renommer", "Nouveau nom", name); if (nn) { await run("rename_object", { name, new_name: nn }); scheduleRefresh(0); } }],
    ["Dupliquer", () => run("duplicate_object", { name, offset: [1, 0, 0] }).then(() => scheduleRefresh(0))],
    ["Cadrer", () => vp.frameObjects([name])],
    ["Déplacer vers une collection…", async () => { const c = await prompt("Collection", "Nom de la collection", "Collection"); if (c) { await run("move_to_collection", { name, collection: c }); scheduleRefresh(0); } }],
    ["Détacher du parent", () => run("set_parent", { name, parent: "" }).then(() => scheduleRefresh(0))],
    ["Enregistrer dans la bibliothèque", () => run("library_save", { name }, { success: "Ajouté à la bibliothèque" })],
    ["Inspecter le modèle", () => inspectModel(name)],
    ["Supprimer", () => run("delete_object", { name, children: true }).then(() => scheduleRefresh(0)), "danger"],
  ];
  const list = h("div", { class: "menu-list", style: { display: "block", position: "fixed", left: `${ev.clientX}px`, top: `${ev.clientY}px` } },
    ...items.map(([l, fn]) => h("button", { onclick: () => { list.remove(); fn(); } }, l)));
  document.body.append(list);
  setTimeout(() => document.addEventListener("mousedown", function off(e) { if (!list.contains(e.target)) { list.remove(); document.removeEventListener("mousedown", off); } }), 0);
}

async function inspectModel(name) {
  const r = await run("inspect_model", { name });
  if (!r) return;
  const issues = r.issues.length ? h("ul", {}, ...r.issues.map((i) => h("li", {}, i))) : h("p", {}, "Aucun problème détecté.");
  modal(`Inspection : ${name}`, h("div", {}, h("div", { class: "kv" },
    ...[["Sommets", r.vertices], ["Faces", r.faces], ["Triangles", r.triangles], ["Quads / tris / n-gones", `${r.quads} / ${r.tris} / ${r.ngons}`],
      ["Étanche", r.watertight ? "oui" : "non"], ["Îlots", r.islands], ["Dimensions (m)", r.dimensions_m.join(" × ")], ["Volume (m³)", r.volume_m3 ?? "—"],
      ["UV", r.uv_maps.join(", ") || "aucune"], ["Matériaux", r.materials.join(", ") || "aucun"]].flatMap(([k, v]) => [h("span", {}, k), h("span", {}, String(v))])),
    h("h4", {}, "Points à vérifier"), issues), [["Fermer", null], ["Optimiser maintenant", () => run("optimize_mesh", { name, fill_holes: true }, { success: "Maillage optimisé" }).then(() => scheduleRefresh(0)), "primary"]]);
}

async function duplicateSelection() {
  if (!vp.selected.size) return toast("Rien de sélectionné.", "warn");
  await run("duplicate_object", { names: [...vp.selected], offset: [1, 0, 0] });
  scheduleRefresh(0);
}

async function deleteSelection() {
  if (!vp.selected.size) return toast("Rien de sélectionné.", "warn");
  await run("delete_object", { names: [...vp.selected], children: true });
  scheduleRefresh(0);
}

// --------------------------------------------------------------------------- propriétés
const PROP_TABS = [["object", "Objet"], ["geometry", "Géométrie"], ["modifiers", "Modificateurs"], ["material", "Matériau"],
  ["texture", "Texture"], ["rig", "Rig & Anim"], ["scene", "Scène"], ["export", "Export"]];

function renderPropTabs() {
  const t = $("#prop-tabs");
  t.innerHTML = "";
  for (const [id, label] of PROP_TABS) t.append(h("button", { class: S.propTab === id ? "active" : "", onclick: () => { S.propTab = id; renderPropTabs(); renderProps(); } }, label));
}

export function renderProps() {
  renderPropTabs();
  const body = $("#prop-body");
  const scrollTop = body.scrollTop;
  body.innerHTML = "";
  const d = S.details;
  const tab = S.propTab;
  try {
    if (tab === "scene") body.append(...sceneProps());
    else if (tab === "export") body.append(...exportProps());
    else if (!d) body.append(h("div", { class: "empty" }, "Aucun objet actif.\nCliquez sur un objet dans le viewport ou l'outliner."));
    else if (tab === "object") body.append(...objectProps(d));
    else if (tab === "geometry") body.append(...geometryProps(d));
    else if (tab === "modifiers") modifierProps(d, body);
    else if (tab === "material") materialProps(d, body);
    else if (tab === "texture") body.append(...textureProps(d));
    else if (tab === "rig") body.append(...rigProps(d));
  } catch (e) { console.error(e); body.append(h("div", { class: "warnbox" }, e.message)); }
  body.scrollTop = scrollTop;
}

const deg = (r) => r.map((x) => +(x * 180 / Math.PI).toFixed(2));
const rad = (dg) => dg.map((x) => x * Math.PI / 180);
const set = (cmd, params, msg) => run(cmd, params, msg ? { success: msg } : {}).then(() => scheduleRefresh(0)).catch(() => {});

function objectProps(d) {
  const out = [];
  const n = d.name;
  const name = h("input", { type: "text", value: n });
  name.addEventListener("change", () => set("rename_object", { name: n, new_name: name.value }));
  out.push(section("Objet", true, row("Nom", name), row("Type", h("span", {}, d.type)), row("Parent", h("span", {}, d.parent || "—")),
    row("Collections", h("span", {}, d.collections.join(", ")))));
  out.push(section("Transformations", true,
    row("Position", vecInput(d.location, (v) => set("transform_object", { name: n, location: v }), { step: 0.1 })),
    row("Rotation (°)", vecInput(deg(d.rotation), (v) => set("transform_object", { name: n, rotation: rad(v) }), { step: 5, digits: 2 })),
    row("Échelle", vecInput(d.scale, (v) => set("transform_object", { name: n, scale: v }), { step: 0.1 })),
    row("Dimensions", h("span", { class: "hint" }, d.dimensions.map((x) => fmt(x, 3)).join(" × ") + " m")),
    h("div", { class: "btns" },
      btn("Appliquer l'échelle/rotation", () => set("apply_transform", { name: n }), "small"),
      btn("Origine en bas", () => set("set_origin", { name: n, mode: "bottom" }), "small"),
      btn("Origine au centre", () => set("set_origin", { name: n, mode: "center" }), "small"))));
  if (d.generation_spec) {
    const sp = d.generation_spec;
    const instr = h("textarea", { placeholder: "Ex. : agrandis ses ailes, ajoute six tentacules et remplace les yeux par des yeux blancs lumineux" });
    out.push(section("Modèle généré ✦", true,
      h("div", { class: "kv" }, h("span", {}, "Catégorie"), h("span", {}, `${sp.category_label || sp.category} · ${sp.subtype || ""}`),
        h("span", {}, "Description"), h("span", {}, sp.prompt || "—"), h("span", {}, "Hauteur"), h("span", {}, `${sp.height ?? "?"} m`)),
      row("Modifier par texte", instr),
      btn("Appliquer la modification", async () => {
        if (!instr.value.trim()) return;
        const r = await run("modify_model", { name: n, instruction: instr.value }, { success: (r) => `Modifié : ${r.changes.join(", ")}` });
        if (r) scheduleRefresh(0);
      }, "primary")));
  }
  if (d.type === "LIGHT") {
    out.push(section("Lumière", true,
      row("Type", select([["POINT", "Point"], ["SUN", "Soleil"], ["SPOT", "Spot"], ["AREA", "Surface"]], d.light.type, (v) => set("set_light", { name: n, type: v }))),
      row("Puissance (W)", numInput(d.light.energy, (v) => set("set_light", { name: n, energy: v }), { step: 50 })),
      row("Couleur", colorInput(d.light.color, (v) => set("set_light", { name: n, color: v })))));
  }
  if (d.type === "CAMERA") {
    out.push(section("Caméra", true,
      row("Focale (mm)", numInput(d.camera.lens, (v) => set("set_camera", { name: n, lens: v }), { step: 1 })),
      h("div", { class: "btns" }, btn("Caméra active", () => set("set_camera", { name: n, active: true }), "small"),
        btn("Cadrer la scène", () => set("set_camera", { name: n, frame_objects: true }), "small"))));
  }
  if (d.mesh) {
    out.push(section("Statistiques", false, h("div", { class: "kv" },
      ...[["Sommets", d.mesh.vertices], ["Arêtes", d.mesh.edges], ["Faces", d.mesh.faces], ["Triangles", d.mesh.triangles],
        ["UV", (d.uv_maps || []).join(", ") || "aucune"], ["Couleurs", (d.color_attributes || []).join(", ") || "—"],
        ["Groupes", (d.vertex_groups || []).length]].flatMap(([k, v]) => [h("span", {}, k), h("span", {}, String(v))])),
      btn("Inspecter / diagnostiquer", () => inspectModel(n), "small")));
  }
  return out;
}

function geometryProps(d) {
  const out = [];
  if (d.type !== "MESH") return [h("div", { class: "empty" }, "La géométrie concerne les maillages.")];
  const n = d.name;
  const inEdit = S.mode === "edit";
  if (!inEdit) {
    out.push(h("div", { class: "note" }, "Passez en mode Édition (Tab) pour sélectionner faces et sommets. Les opérations ci-dessous sans sélection s'appliquent à tout l'objet."));
    out.push(btn("Entrer en mode édition", () => setMode("edit"), "primary"));
  } else {
    out.push(h("div", { class: "note" }, `${vp.editSel.size} ${vp.selectMode === "face" ? "face(s)" : "sommet(s)"} sélectionné(s)`));
    out.push(h("div", { class: "btns" },
      btn("Tout", () => vp.setEditSelection([...Array(vp.selectMode === "face" ? vp.editData.d.faces : vp.editData.d.vertices).keys()]), "small"),
      btn("Aucun", () => vp.setEditSelection([]), "small"),
      btn("Liées", async () => { if (vp.selectMode !== "face") return; const r = await api("select_linked_faces", { name: n, faces: [...vp.editSel] }); vp.setEditSelection(r.faces); }, "small"),
      btn("Faces du dessus", async () => { vp.selectMode = "face"; const r = await api("select_by_normal", { name: n, direction: [0, 0, 1] }); vp.setEditSelection(r.faces); renderToolOptions(); }, "small")));
  }
  const dist = numInput(0.3, () => {}, { step: 0.05 });
  const thick = numInput(0.05, () => {}, { step: 0.01 });
  const width = numInput(0.05, () => {}, { step: 0.01 });
  const segs = numInput(2, () => {}, { step: 1, min: 1 });
  const cuts = numInput(1, () => {}, { step: 1, min: 1 });
  const val = (el) => parseFloat(el.value);
  const all = inEdit && vp.editSel.size ? {} : { all: true };
  out.push(section("Opérations polygonales", true,
    row("Distance", dist), h("div", { class: "btns" },
      btn("Extruder", () => editOp("extrude", { distance: val(dist), ...all })),
      btn("Extruder (individuel)", () => editOp("extrude", { distance: val(dist), individual: true, ...all })),
      btn("Pousser (normales)", () => editOp("push", { distance: val(dist), ...all }))),
    row("Épaisseur inset", thick), h("div", { class: "btns" },
      btn("Inset", () => editOp("inset", { thickness: val(thick), ...all })),
      btn("Inset + creux", () => editOp("inset", { thickness: val(thick), depth: -val(thick), ...all }))),
    row("Largeur bevel", width), row("Segments", segs),
    btn("Bevel (biseau)", () => editOp("bevel", { width: val(width), segments: Math.round(val(segs)), ...all })),
    row("Coupes", cuts), h("div", { class: "btns" },
      btn("Subdiviser", () => editOp("subdivide", { cuts: Math.round(val(cuts)), ...all })),
      btn("Subdiviser lisse", () => editOp("subdivide", { cuts: Math.round(val(cuts)), smooth: 1, ...all }))),
    h("div", { class: "btns" },
      btn("Lisser", () => editOp("smooth", { factor: 0.5, iterations: 3, ...all })),
      btn("Agrandir ×1.2", () => editOp("scale", { factor: 1.2, ...all })),
      btn("Réduire ×0.8", () => editOp("scale", { factor: 0.8, ...all })),
      btn("Fusionner", () => editOp("merge")),
      btn("Supprimer", () => editOp("delete"), "danger"))));
  out.push(section("Nettoyage", !inEdit, h("div", { class: "btns" },
    btn("Fusion des doublons", () => editOp("merge_by_distance", { distance: 0.0005, all: true })),
    btn("Recalculer normales", () => editOp("recalc_normals", { all: true })),
    btn("Inverser normales", () => editOp("flip_normals", { ...all })),
    btn("Fermer les trous", () => editOp("fill_holes", { all: true })),
    btn("Trianguler", () => editOp("triangulate", { ...all })),
    btn("Symétrie X", () => editOp("mirror", { axis: "x", all: true })),
    btn("Épaissir", () => editOp("solidify", { thickness: 0.05, all: true }))),
    h("div", { class: "btns" },
      btn("Ombrage lisse", () => set("shade", { name: n, mode: "smooth" }), "small"),
      btn("Ombrage plat", () => set("shade", { name: n, mode: "flat" }), "small"),
      btn("Auto (30°)", () => set("shade", { name: n, mode: "auto", angle: 30 }), "small"))));
  const target = numInput(5000, () => {}, { step: 500, min: 0 });
  out.push(section("Optimisation", false, row("Triangles cible", target),
    btn("Optimiser le maillage", () => set("optimize_mesh", { name: n, target_triangles: Math.round(val(target)), fill_holes: true }, "Maillage optimisé"))));
  out.push(section("UV", false, h("div", { class: "btns" },
    ...[["smart", "Projection intelligente"], ["unwrap", "Déplier"], ["cube", "Cube"], ["cylinder", "Cylindre"], ["sphere", "Sphère"], ["lightmap", "Lightmap"]]
      .map(([m, l]) => btn(l, () => set("create_uv_map", { name: n, method: m }, `UV : ${l}`), "small"))),
    h("div", { class: "hint" }, "Affichage du damier UV : mode Matériau du viewport après cuisson d'une texture.")));
  return out;
}

async function modifierProps(d, body) {
  if (d.type !== "MESH") { body.append(h("div", { class: "empty" }, "Modificateurs disponibles pour les maillages.")); return; }
  const n = d.name;
  const [mods, types] = await Promise.all([api("get_modifiers", { name: n }, { quiet: true }), S.modTypes || api("list_modifier_types", {}, { quiet: true })]);
  S.modTypes = types;
  if (S.propTab !== "modifiers" || S.details?.name !== n) return;
  const add = select([["", "+ Ajouter un modificateur…"], ...types.types.map((t) => [t.type, t.label])], "", (v) => { if (v) set("add_modifier", { name: n, type: v }); });
  body.append(add);
  const quick = h("div", { class: "btns", style: { margin: "8px 0" } },
    ...[["SUBSURF", "Subdivision"], ["MIRROR", "Miroir"], ["ARRAY", "Array"], ["BEVEL", "Bevel"], ["SOLIDIFY", "Épaisseur"], ["BOOLEAN", "Booléen"], ["DECIMATE", "Décimation"], ["REMESH", "Remesh"]]
      .map(([t, l]) => btn(l, () => set("add_modifier", { name: n, type: t }), "small")));
  body.append(quick);
  if (!mods.modifiers.length) body.append(h("div", { class: "empty" }, "Aucun modificateur."));
  for (const m of mods.modifiers) {
    const card = h("div", { class: "mod-card" },
      h("header", {}, h("span", { class: "t" }, `${m.label} — ${m.name}`),
        h("button", { class: "icon-btn", title: m.show_viewport ? "Désactiver" : "Activer", html: m.show_viewport ? ICONS.eye : ICONS.eyeoff, onclick: () => set("set_modifier", { name: n, modifier: m.name, show_viewport: !m.show_viewport }) }),
        h("button", { class: "icon-btn", title: "Monter", html: ICONS.up, onclick: () => set("set_modifier", { name: n, modifier: m.name, move: "up" }) }),
        h("button", { class: "icon-btn", title: "Descendre", html: ICONS.down, onclick: () => set("set_modifier", { name: n, modifier: m.name, move: "down" }) }),
        h("button", { class: "icon-btn", title: "Appliquer", html: ICONS.check, onclick: () => set("apply_modifier", { name: n, modifier: m.name }, `${m.name} appliqué`) }),
        h("button", { class: "icon-btn", title: "Retirer", html: ICONS.trash, onclick: () => set("remove_modifier", { name: n, modifier: m.name }) })),
      h("div", { class: "body" }, ...rnaEditor(m.properties, (pid, v) => set("set_modifier", { name: n, modifier: m.name, settings: { [pid]: v } }), S.objectNames.filter((x) => x !== n))));
    body.append(card);
  }
  if (mods.modifiers.length) body.append(btn("Appliquer tous les modificateurs", () => set("apply_modifier", { name: n, all: true }, "Modificateurs appliqués"), "small"));
}

async function materialProps(d, body) {
  if (d.type !== "MESH") { body.append(h("div", { class: "empty" }, "Les matériaux s'appliquent aux maillages.")); return; }
  const n = d.name;
  const lst = await api("list_materials", {}, { quiet: true });
  if (S.propTab !== "material" || S.details?.name !== n) return;
  const presets = lst.presets;
  const cur = d.materials?.[0];
  body.append(section("Matériau de l'objet", true,
    row("Matériau", select([["", "— aucun —"], ...lst.materials.map((m) => [m.name, m.name])], cur || "", (v) => v && set("assign_material", { name: n, material: v }))),
    btn("Nouveau matériau", () => newMaterialDialog(n, presets), "primary")));
  if (!cur) return;
  const m = await api("get_material", { material: cur }, { quiet: true });
  if (S.propTab !== "material") return;
  const pr = m.principled || {};
  const mset = (k, v) => set("set_material", { material: cur, [k]: v });
  const color = (v) => (typeof v === "string" && v.startsWith("#") ? v : null);
  const sec = section(`Principled BSDF — ${cur}${m.preset ? ` (${m.preset})` : ""}`, true);
  const b = sec.querySelector(".body");
  const fields = [["base_color", "Base Color", "color"], ["metallic", "Metallic"], ["roughness", "Roughness"], ["alpha", "Alpha"], ["subsurface", "Subsurface Weight"],
    ["transmission", "Transmission Weight"], ["coat", "Coat Weight"], ["sheen", "Sheen Weight"], ["emission", "Emission Color", "color"], ["emission_strength", "Emission Strength", "num"]];
  for (const [k, label, kind] of fields) {
    const v = pr[label];
    if (v === "(texture)") { b.append(row(label, h("span", { class: "hint" }, "relié à une texture procédurale"))); continue; }
    if (kind === "color") b.append(row(label, colorInput(color(v) || "#cccccc", (x) => mset(k, x))));
    else if (kind === "num") b.append(row(label, numInput(v ?? 0, (x) => mset(k, x), { step: 0.5, min: 0 })));
    else b.append(row(label, slider(v ?? 0, (x) => mset(k, x))));
  }
  b.append(row("Renommer", (() => { const i = h("input", { type: "text", value: cur }); i.addEventListener("change", () => mset("new_name", i.value)); return i; })()));
  body.append(sec);
  body.append(section("Préréglage procédural", false,
    row("Type", select(presets.map((p) => [p.id, p.label]), m.preset || "plain", (v) => set("set_material", { material: cur, rebuild_preset: v, color: color(pr["Base Color"]) || "#bbbbbb" })))));
  const nodes = section(`Nœuds (${(m.nodes || []).length})`, false);
  for (const node of m.nodes || []) {
    const items = [];
    for (const inp of node.inputs) {
      let input;
      const setIn = (v) => set("set_material", { material: cur, node: node.name, input: inp.identifier, value: v });
      if (inp.type === "RGBA") input = colorInput(hexFromRgb(inp.value.map((c) => Math.pow(Math.max(0, c), 1 / 2.2))), setIn);
      else if (Array.isArray(inp.value)) input = vecInput(inp.value, setIn, { step: 0.05 });
      else if (typeof inp.value === "boolean") input = checkbox(inp.value, setIn);
      else if (typeof inp.value === "number") input = numInput(inp.value, setIn, { step: 0.05 });
      else continue;
      items.push(row(inp.name, input));
    }
    for (const pp of node.properties) items.push(row(pp.id, select(pp.items, pp.value, (v) => set("set_material", { material: cur, node: node.name, property: pp.id, value: v }))));
    if (!items.length) continue;
    const s = section(`${node.label}`, false, ...items);
    nodes.querySelector(".body").append(s);
  }
  body.append(nodes);
}

async function newMaterialDialog(objName, presets) {
  const name = h("input", { type: "text", value: "Matériau" });
  const preset = select(presets.map((p) => [p.id, p.label]), "plain", () => {});
  const c1 = colorInput("#b0b0b0", () => {});
  const c2 = colorInput("#404040", () => {});
  const rough = slider(0.5, () => {});
  const desc = h("textarea", { placeholder: "Ou décrivez-le : « métal rouillé sombre », « peau de monstre noire, humide, veines violettes »…" });
  const r = await modal("Nouveau matériau", h("div", { style: { display: "grid", gap: "8px" } }, row("Nom", name), row("Préréglage", preset), row("Couleur", c1), row("Couleur 2", c2), row("Rugosité", rough), row("Description", desc)),
    [["Annuler", null], ["Créer", true, "primary"]]);
  if (!r) return;
  const params = { name: name.value, objects: [objName] };
  if (desc.value.trim()) params.description = desc.value.trim();
  else Object.assign(params, { preset: preset.value, color: c1.value, color2: c2.value, roughness: rough.get() });
  await set("create_material", params, "Matériau créé");
}

function textureProps(d) {
  const out = [];
  if (d.type !== "MESH") return [h("div", { class: "empty" }, "Textures pour les maillages.")];
  const n = d.name;
  const desc = h("textarea", { placeholder: "Ex. : Crée une peau de monstre noire, humide, avec des veines violettes et des reliefs organiques." });
  const res = select([["0", "Procédural (pas de cuisson)"], ["512", "512 px"], ["1024", "1024 px"], ["2048", "2048 px"], ["4096", "4096 px"]], "0", () => {});
  out.push(section("Texture par description", true, desc, row("Cuisson", res),
    btn("Générer la texture", async () => {
      if (!desc.value.trim()) return toast("Décrivez la texture.", "warn");
      const r = await run("create_texture", { description: desc.value, objects: [n], bake_resolution: parseInt(res.value, 10) || undefined },
        { success: (r) => `Texture « ${r.material} » : ${r.interpretation.notes.join(", ")}` });
      if (r) scheduleRefresh(0);
    }, "primary"),
    h("div", { class: "hint" }, "Interprétation transparente (type, couleurs, humidité, veines, relief, émission). Résultat visible en mode Matériau ou Rendu.")));
  const maps = ["base_color", "roughness", "metallic", "normal", "height", "ao", "emission"];
  const labels = { base_color: "Base Color", roughness: "Roughness", metallic: "Metallic", normal: "Normal", height: "Height", ao: "Ambient Occlusion", emission: "Émission" };
  const chosen = new Set(["base_color", "roughness", "metallic", "normal", "ao"]);
  const resB = select([["512", "512"], ["1024", "1024"], ["2048", "2048"], ["4096", "4096"]], "1024", () => {});
  out.push(section("Cuisson des cartes PBR", true,
    h("div", { class: "grid2" }, ...maps.map((m) => h("label", { style: { display: "flex", gap: "6px", alignItems: "center" } }, checkbox(chosen.has(m), (v) => (v ? chosen.add(m) : chosen.delete(m))), labels[m]))),
    row("Résolution", resB),
    btn("Cuire les textures (Cycles)", async () => {
      const r = await run("bake_textures", { name: n, resolution: parseInt(resB.value, 10), maps: [...chosen] },
        { success: (r) => `Cuisson terminée : ${Object.keys(r.results[0].maps).join(", ")} (${r.results[0].seconds} s)` });
      if (r) scheduleRefresh(0);
    }, "accent"),
    h("div", { class: "hint" }, "Les images PNG sont enregistrées dans SuperEssence3D/textures et le matériau devient exportable (glTF/FBX). 4096 px peut prendre plusieurs minutes sur processeur.")));
  return out;
}

function rigProps(d) {
  const out = [];
  const n = d.name;
  const tmpl = select([["auto", "Automatique (structure du modèle)"], ["humanoid", "Humanoïde"], ["quadruped", "Quadrupède"], ["winged_quadruped", "Quadrupède ailé"], ["serpent", "Serpent"], ["chain", "Chaîne simple"]], "auto", () => {});
  out.push(section("Squelette", true, row("Gabarit", tmpl),
    btn("Créer le squelette et lier (poids automatiques)", async () => {
      const r = await run("create_rig", { name: n, template: tmpl.value }, { success: (r) => `Squelette : ${r.bones} os (${r.source}). Poids auto : ${r.binding.automatic.length}, enveloppes : ${r.binding.envelope.length}, rigides : ${r.binding.rigid.length}` });
      if (r) scheduleRefresh(0);
    }, "primary"),
    h("div", { class: "hint" }, "Peinture de poids : outil « Poids » de la barre de gauche.")));
  const motions = [["walk", "Marche"], ["run", "Course"], ["jump", "Saut"], ["attack", "Attaque"], ["fly", "Vol"], ["wing_flap", "Battement d'ailes"], ["tail_wave", "Queue"], ["breathe", "Respiration"], ["idle", "Repos"], ["head_look", "Regard"], ["tentacle_wave", "Tentacules"], ["slither", "Reptation"], ["roar", "Rugissement"]];
  const chosen = new Set(["idle"]);
  const amp = slider(1, () => {}, { min: 0.1, max: 2.5, step: 0.05 });
  const frames = numInput(0, () => {}, { step: 4, min: 0 });
  const cycles = numInput(2, () => {}, { step: 1, min: 1 });
  out.push(section("Animations procédurales", true,
    h("div", { class: "chips" }, ...motions.map(([id, l]) => {
      const c = h("button", { class: `chip ${chosen.has(id) ? "active" : ""}`, onclick: () => { chosen.has(id) ? chosen.delete(id) : chosen.add(id); c.classList.toggle("active"); } }, l);
      return c;
    })),
    row("Amplitude", amp), row("Durée cycle (0 = auto)", frames), row("Répétitions", cycles),
    btn("Créer l'animation (images clés Blender)", async () => {
      if (!chosen.size) return toast("Choisissez au moins un mouvement.", "warn");
      const r = await run("create_animation", { name: n, motions: [...chosen], amplitude: amp.get(), frames: Math.round(parseFloat(frames.value)) || undefined, cycles: Math.round(parseFloat(cycles.value)) },
        { success: (r) => `Animation « ${r.action} » : ${r.animated_channels} canaux, images ${r.frames.join("–")}` });
      if (r) { scheduleRefresh(0); playPreview(); }
    }, "accent"),
    h("div", { class: "hint" }, "Les mouvements se combinent (ex. Battement d'ailes + Queue + Respiration).")));
  const om = select([["spin", "Rotation"], ["bounce", "Rebond"], ["float", "Flottement"], ["pulse", "Pulsation"], ["sway", "Balancement"]], "spin", () => {});
  out.push(section("Animation d'objet (sans squelette)", false, row("Mouvement", om),
    btn("Animer l'objet", () => set("create_animation", { name: n, object_motion: om.value }, "Animation créée"))));
  out.push(section("Images clés", false, h("div", { class: "btns" },
    btn("Clé à l'image courante", () => set("insert_keyframe", { name: n }, "Clé insérée"), "small"),
    btn("Effacer l'animation", () => set("clear_animation", { name: n }), "small danger"))));
  return out;
}

function sceneProps() {
  const info = S.info;
  if (!info) return [];
  const out = [];
  out.push(section("Scène", true,
    row("Scène active", select(info.scenes.map((s) => [s, s]), info.scene, (v) => set("scene_manage", { action: "switch", name: v }))),
    h("div", { class: "btns" },
      btn("Nouvelle scène", async () => { const n = await prompt("Nouvelle scène", "Nom", "Scène 2"); if (n) set("scene_manage", { action: "create", name: n }); }, "small"),
      btn("Renommer", async () => { const n = await prompt("Renommer la scène", "Nom", info.scene); if (n) set("scene_manage", { action: "rename", name: info.scene, new_name: n }); }, "small"),
      btn("Supprimer", () => set("scene_manage", { action: "delete", name: info.scene }), "small danger")),
    h("div", { class: "kv" }, h("span", {}, "Objets"), h("span", {}, info.object_count), h("span", {}, "Triangles"), h("span", {}, info.totals.triangles.toLocaleString("fr")),
      h("span", {}, "Unités"), h("span", {}, info.unit))));
  out.push(section("Éclairage", true, h("div", { class: "btns" },
    ...[["studio", "Studio 3 points"], ["outdoor", "Extérieur"], ["dramatic", "Dramatique"], ["night", "Nuit"], ["product", "Produit"]].map(([p, l]) => btn(l, () => set("create_lighting", { preset: p }, `Éclairage ${l}`), "small"))),
    h("div", { class: "btns" },
      btn("+ Point", () => set("create_light", { type: "POINT" }), "small"), btn("+ Soleil", () => set("create_light", { type: "SUN" }), "small"),
      btn("+ Spot", () => set("create_light", { type: "SPOT" }), "small"), btn("+ Surface", () => set("create_light", { type: "AREA" }), "small")),
    row("Fond", colorInput(info.world?.color || "#0e0f12", (v) => set("set_world", { color: v, strength: info.world?.strength ?? 1 }))),
    row("Intensité du fond", numInput(info.world?.strength ?? 1, (v) => set("set_world", { color: info.world?.color || "#0e0f12", strength: v }), { step: 0.1, min: 0 }))));
  const eng = select([["cycles", "Cycles (fonctionne partout)"], ["eevee", "EEVEE (GPU requis)"], ["workbench", "Workbench"]], "cycles", () => {});
  const samples = numInput(info.render.samples || 64, () => {}, { step: 16, min: 1 });
  const rx = numInput(info.render.resolution[0], () => {}, { step: 10 });
  const ry = numInput(info.render.resolution[1], () => {}, { step: 10 });
  const transp = checkbox(false, () => {});
  out.push(section("Caméra & rendu", true,
    h("div", { class: "btns" }, btn("+ Caméra (cadrage auto)", () => set("create_camera", {}), "small"), btn("Cadrer la caméra active", () => set("set_camera", { frame_objects: true }), "small")),
    row("Moteur", eng), row("Échantillons", samples), row("Largeur", rx), row("Hauteur", ry), row("Fond transparent", transp),
    btn("Rendre l'image", () => fullRender({ engine: eng.value, samples: parseInt(samples.value, 10), resolution: [parseInt(rx.value, 10), parseInt(ry.value, 10)], transparent: transp.checked }), "accent")));
  return out;
}

function exportProps() {
  const out = [];
  const sel = S.info?.active;
  const scope = select([["scene", "Toute la scène visible"], ...(sel ? [["active", `Modèle actif : ${sel}`]] : [])], sel ? "active" : "scene", () => {});
  const fmtSel = select([["glb", "GLB (glTF binaire)"], ["gltf", "glTF + fichiers"], ["fbx", "FBX"], ["obj", "OBJ"], ["stl", "STL"], ["ply", "PLY"], ["blend", "BLEND"]], "glb", () => {});
  const applyMods = checkbox(true, () => {});
  const anims = checkbox(true, () => {});
  const fname = h("input", { type: "text", placeholder: "nom du fichier (facultatif)" });
  const doExport = async (extra) => {
    const params = { apply_modifiers: applyMods.checked, animations: anims.checked, ...extra };
    if (scope.value === "active" && sel) params.name = sel;
    if (fname.value.trim()) params.filename = fname.value.trim();
    const r = await run("export_model", params, { success: (r) => `Export vérifié : ${r.path.split(/[\\/]/).pop()} (${Math.round(r.verification.bytes / 1024)} Ko)` });
    if (r) {
      const a = h("a", { href: fileUrl(r.path, true), download: "" });
      document.body.append(a); a.click(); a.remove();
      S.lastExport = r;
      renderProps();
    }
  };
  out.push(section("Exporter", true, row("Contenu", scope), row("Format", fmtSel), row("Nom", fname), row("Appliquer modificateurs", applyMods), row("Animations", anims),
    btn("Exporter et télécharger", () => doExport({ format: fmtSel.value }), "primary")));
  out.push(section("Profils moteurs de jeu", true, h("div", { class: "grid2" },
    ...[["unity", "Unity (FBX)"], ["unreal", "Unreal Engine (FBX)"], ["godot", "Godot (GLB)"], ["mobile", "Jeux mobiles (GLB léger)"], ["print3d", "Impression 3D (STL mm)"], ["high_quality", "Haute qualité (GLB)"]]
      .map(([p, l]) => btn(l, () => doExport({ profile: p })))),
    h("div", { class: "hint" }, "Chaque fichier est relu et vérifié (en-têtes, contenu) après l'export. Dossier : SuperEssence3D/exports.")));
  if (S.lastExport) {
    const v = S.lastExport.verification;
    out.push(section("Dernier export", true, h("div", { class: "kv" }, ...Object.entries(v).filter(([k]) => k !== "print_checks").flatMap(([k, val]) => [h("span", {}, k), h("span", {}, String(val))])),
      ...(S.lastExport.notes || []).map((n) => h("div", { class: "hint" }, n))));
  }
  out.push(section("Importer", false, btn("Importer un fichier (glb, gltf, fbx, obj, stl, ply, blend)…", () => importFile(), "")));
  return out;
}

// --------------------------------------------------------------------------- rendu
async function quickRender() {
  const box = $("#vp-render");
  const w = Math.min(960, $("#viewport").clientWidth), hh = Math.round(w * $("#viewport").clientHeight / $("#viewport").clientWidth);
  showBusy("Rendu Blender (Cycles)…");
  try {
    const r = await api("render_scene", { engine: "cycles", samples: 24, resolution: [w, hh], filename: "apercu_viewport" });
    $("img", box).src = fileUrl(r.path);
    $(".caption", box).textContent = `Rendu Blender Cycles · ${r.resolution.join("×")} · ${r.samples} échantillons · ${r.seconds} s — caméra active de la scène`;
    box.classList.remove("hidden");
  } catch (e) { toast(e.message, "error", 8000); } finally { showBusy(null); }
}

async function fullRender(params) {
  showBusy("Rendu en cours…");
  try {
    const r = await api("render_scene", params);
    const box = $("#vp-render");
    $("img", box).src = fileUrl(r.path);
    $(".caption", box).innerHTML = "";
    $(".caption", box).append(`Rendu ${r.engine} · ${r.resolution.join("×")} · ${r.seconds} s — `, h("a", { href: fileUrl(r.path, true), download: "", style: { color: "var(--accent2)" } }, "Télécharger le PNG"));
    box.classList.remove("hidden");
  } catch (e) { toast(e.message, "error", 9000); } finally { showBusy(null); }
}

function showBusy(msg) {
  const b = $("#vp-busy");
  if (!msg) { b.classList.add("hidden"); return; }
  $("span", b).textContent = msg;
  b.classList.remove("hidden");
}
export { showBusy };

// --------------------------------------------------------------------------- timeline
function renderTimeline() {
  const st = S.state;
  if (!st) return;
  const tl = $("#timeline");
  if (!tl.dataset.built) {
    tl.dataset.built = "1";
    tl.append(
      h("button", { class: "icon-btn", id: "tl-play", title: "Lire l'animation (aperçu glTF exporté par Blender)", html: ICONS.play, onclick: () => playPreview() }),
      h("button", { class: "icon-btn", title: "Arrêter", html: ICONS.stop, onclick: () => { vp.stopAnimation(); } }),
      h("input", { type: "range", id: "tl-range", min: 1, max: 120, value: 1 }),
      h("span", { class: "frame", id: "tl-frame" }),
      h("label", { class: "hint" }, "Début"), h("input", { type: "number", id: "tl-start" }),
      h("label", { class: "hint" }, "Fin"), h("input", { type: "number", id: "tl-end" }),
    );
    const range = $("#tl-range");
    let t = null;
    range.addEventListener("input", () => {
      $("#tl-frame").textContent = `Image ${range.value}`;
      clearTimeout(t);
      t = setTimeout(() => api("set_frame", { frame: parseInt(range.value, 10) }, { quiet: true }).then(() => scheduleRefresh(0)), 60);
    });
    $("#tl-start").addEventListener("change", (e) => set("set_frame", { start: parseInt(e.target.value, 10) }));
    $("#tl-end").addEventListener("change", (e) => set("set_frame", { end: parseInt(e.target.value, 10) }));
  }
  const range = $("#tl-range");
  range.min = st.frame_start; range.max = st.frame_end; range.value = st.frame;
  $("#tl-frame").textContent = `Image ${st.frame} / ${st.frame_end}`;
  $("#tl-start").value = st.frame_start;
  $("#tl-end").value = st.frame_end;
}

async function playPreview() {
  showBusy("Préparation de l'aperçu d'animation…");
  try {
    const r = await api("export_preview", {});
    const n = await vp.playAnimation(r.path, r.fps);
    if (!n) { toast("Aucune animation dans la scène. Créez-en une (onglet Rig & Anim).", "warn"); vp.stopAnimation(); }
    else toast(`Lecture de ${n} animation(s) — bouton ■ pour revenir à l'édition.`, "ok");
  } catch (e) { toast(e.message, "error"); } finally { showBusy(null); }
}

// --------------------------------------------------------------------------- panneau inférieur
const BOTTOM_TABS = [["history", "Historique"], ["console", "Console"], ["jobs", "Opérations"], ["errors", "Erreurs"], ["generations", "Générations"]];

function renderBottomTabs() {
  const t = $("#bottom-tabs");
  t.innerHTML = "";
  for (const [id, label] of BOTTOM_TABS) {
    const b = h("button", { class: S.bottomTab === id ? "active" : "", onclick: () => { S.bottomTab = id; if (id === "errors") S.errors = 0; renderBottom(); } }, label);
    if (id === "errors" && S.errors) b.append(h("span", { class: "badge" }, S.errors));
    t.append(b);
  }
}

export function renderBottom() {
  renderBottomTabs();
  const b = $("#bottom-body");
  b.innerHTML = "";
  if (S.bottomTab === "history") {
    b.append(h("div", { class: "btns", style: { marginBottom: "6px" } }, btn("↶ Annuler (Ctrl+Z)", () => undo(), "small"), btn("↷ Rétablir (Ctrl+Y)", () => redo(), "small")));
    for (const e of [...S.history].reverse()) {
      const cur = S.history.find((x) => x.current);
      b.append(h("div", { class: `hist-row ${e.current ? "current" : ""} ${cur && e.index > cur.index ? "future" : ""}`, onclick: () => run("history_goto", { index: e.index }).then(() => scheduleRefresh(0)) },
        h("span", { class: "hint" }, new Date(e.time * 1000).toLocaleTimeString("fr")), h("span", {}, e.label), e.current ? h("b", { style: { color: "var(--accent2)" } }, "◀ actuel") : ""));
    }
  } else if (S.bottomTab === "console" || S.bottomTab === "errors") {
    const lines = S.bottomTab === "errors" ? S.logs.filter((l) => l.level === "error" || l.level === "warning") : S.logs;
    for (const l of lines.slice(-300)) b.append(h("div", { class: `log-line ${l.level}` }, h("time", {}, new Date(l.time * 1000).toLocaleTimeString("fr")), h("span", {}, l.message)));
    if (!lines.length) b.append(h("div", { class: "empty" }, S.bottomTab === "errors" ? "Aucune erreur." : "Journal vide."));
    if (S.bottomTab === "console") {
      const inp = h("input", { type: "text", placeholder: 'Commande moteur : nom {"paramètre": valeur}  — ex. create_mesh {"primitive":"sphere"}' });
      inp.addEventListener("keydown", async (e) => {
        if (e.key !== "Enter") return;
        const m = inp.value.trim().match(/^(\w+)\s*(\{.*\})?$/s);
        if (!m) return toast("Format : nom_commande {json}", "warn");
        let params = {};
        try { params = m[2] ? JSON.parse(m[2]) : {}; } catch { return toast("JSON invalide", "error"); }
        log("info", `> ${inp.value}`);
        try { const r = await run(m[1], params); log("info", JSON.stringify(r).slice(0, 1500)); scheduleRefresh(0); } catch { /* affiché */ }
        inp.value = "";
      });
      b.append(h("div", { class: "console-input" }, inp, btn("Commandes", () => listCommands(), "small")));
    }
    b.scrollTop = b.scrollHeight;
  } else if (S.bottomTab === "jobs") {
    const jobs = [...S.jobs.values()].reverse();
    if (!jobs.length) b.append(h("div", { class: "empty" }, "Aucune opération en cours."));
    for (const j of jobs) b.append(h("div", { class: "job" }, h("span", {}, j.message), h("span", { class: "hint" }, j.status || ""), h("div", { class: "bar" }, h("i", { style: { width: `${Math.round((j.fraction ?? (j.status === "done" ? 1 : 0.05)) * 100)}%` } }))));
  } else if (S.bottomTab === "generations") {
    api("list_generations", { limit: 40 }, { quiet: true }).then((r) => {
      if (S.bottomTab !== "generations") return;
      if (!r.generations.length) b.append(h("div", { class: "empty" }, "Aucune génération."));
      for (const g of r.generations) b.append(h("div", { class: "hist-row" }, h("span", { class: "hint" }, new Date(g.created * 1000).toLocaleString("fr")),
        h("span", {}, `${g.spec.category_label || g.spec.category} · ${g.prompt || g.spec.subtype}`), h("span", { class: "hint" }, g.objects.join(", ")),
        btn("Regénérer", () => run("generate_from_prompt", { spec: g.spec, params: g.spec._params || {} }).then(() => scheduleRefresh(0)), "small")));
    });
  }
}

export function log(level, message) {
  S.logs.push({ level, message, time: Date.now() / 1000 });
  if (S.logs.length > 800) S.logs.splice(0, S.logs.length - 800);
  if (level === "error") S.errors++;
  if (["console", "errors"].includes(S.bottomTab)) renderBottom();
  else renderBottomTabs();
}

async function listCommands() {
  const r = await getJSON("/api/commands");
  modal("Commandes du moteur", h("div", { style: { display: "grid", gap: "6px" } }, ...r.commands.map((c) => h("div", {}, h("b", { style: { fontFamily: "var(--mono)" } }, c.name), " — ", h("span", { class: "hint" }, c.description.split("\n")[0])))), [["Fermer", null]], { wide: true });
}

// --------------------------------------------------------------------------- annuler / rétablir
async function undo() { await run("undo").catch(() => {}); scheduleRefresh(0); }
async function redo() { await run("redo").catch(() => {}); scheduleRefresh(0); }

// --------------------------------------------------------------------------- fichiers
async function importFile() {
  const f = await pickFile(".glb,.gltf,.fbx,.obj,.stl,.ply,.blend");
  if (!f) return;
  try {
    const path = await upload(f);
    const r = await run("import_model", { path }, { success: (r) => `Importé : ${r.imported.length} objet(s)` });
    if (r) { scheduleRefresh(0); setTimeout(() => vp.frameObjects(r.imported), 400); }
  } catch (e) { toast(e.message, "error"); }
}

async function openProject() {
  const r = await api("list_projects", {});
  if (!r.projects.length) return toast("Aucun projet enregistré dans " + r.folder, "warn");
  const choice = await modal("Ouvrir un projet", h("div", { style: { display: "grid", gap: "4px" } },
    ...r.projects.map((p) => h("button", { class: "btn", style: { justifyContent: "space-between" }, onclick: (e) => { e.target.closest(".modal").querySelector(".foot button").dataset.v = p.path; e.target.closest(".modal-back").querySelector(".foot button").click(); } },
      h("span", {}, p.name), h("span", { class: "hint" }, new Date(p.modified * 1000).toLocaleString("fr"))))),
    [["Fermer", () => document.querySelector(".modal .foot button")?.dataset.v || null]]);
  if (choice) { await run("load_project", { path: choice }, { success: "Projet ouvert" }); scheduleRefresh(0); setTimeout(() => vp.frameObjects(), 500); }
}

async function saveProject(as = false) {
  let params = {};
  if (as || !S.info?.status.project_path) {
    const n = await prompt("Enregistrer le projet", "Nom du projet", "Mon projet");
    if (!n) return;
    params = { name: n };
  }
  await run("save_project", params, { success: (r) => `Projet enregistré : ${r.path}` });
  scheduleRefresh(0);
}

// --------------------------------------------------------------------------- menus
function addPrimitive(p) { return () => run("create_mesh", { primitive: p, location: [0, 0, 1] }).then(() => scheduleRefresh(0)); }

function buildMenus() {
  const menus = {
    "Fichier": [["Nouveau projet", () => run("new_project").then(() => scheduleRefresh(0)), "Ctrl+N"], ["Ouvrir un projet…", openProject, "Ctrl+O"], ["Enregistrer", () => saveProject(), "Ctrl+S"], ["Enregistrer sous…", () => saveProject(true)], "-",
      ["Importer…", importFile, "Ctrl+I"], ["Exporter…", () => { S.propTab = "export"; renderProps(); }, "Ctrl+E"]],
    "Édition": [["Annuler", undo, "Ctrl+Z"], ["Rétablir", redo, "Ctrl+Y"], "-", ["Dupliquer", duplicateSelection, "Maj+D"], ["Supprimer", deleteSelection, "Suppr"], ["Tout sélectionner", () => api("select", { mode: "all" }).then(() => scheduleRefresh(0)), "A"],
      ["Joindre la sélection", () => run("join_objects", { names: [...vp.selected] }).then(() => scheduleRefresh(0)), "Ctrl+J"], ["Séparer en parties", () => run("separate_object", { name: S.info?.active }).then(() => scheduleRefresh(0))], "-",
      ["Mode édition", () => setMode(S.mode === "edit" ? "object" : "edit"), "Tab"]],
    "Projet": [["Informations du projet", projectInfo], ["Scènes et rendu", () => { S.propTab = "scene"; renderProps(); }], ["Fichiers exportés", () => filesDialog("exports")], ["Rendus", () => filesDialog("renders")]],
    "Création": [{ label: "Primitives" }, ["Cube", addPrimitive("cube")], ["Sphère", addPrimitive("sphere")], ["Icosphère", addPrimitive("icosphere")], ["Cylindre", addPrimitive("cylinder")], ["Cône", addPrimitive("cone")], ["Tore", addPrimitive("torus")], ["Plan", addPrimitive("plane")], ["Grille", addPrimitive("grid")], ["Singe (test)", addPrimitive("monkey")], "-",
      { label: "Scène" }, ["Lumière ponctuelle", () => set("create_light", { type: "POINT" })], ["Soleil", () => set("create_light", { type: "SUN" })], ["Caméra", () => set("create_camera", {})], ["Éclairage studio", () => set("create_lighting", { preset: "studio" })]],
    "Génération IA": [["Espace de génération", () => setWorkspace("generate")], "-", ["Personnage", () => quickGen("un personnage")], ["Monstre", () => quickGen("un monstre")], ["Dragon", () => quickGen("un dragon")], ["Animal", () => quickGen("un loup")], ["Bâtiment", () => quickGen("une maison")], ["Véhicule", () => quickGen("une voiture")], ["Arme fictive", () => quickGen("une épée")], ["Objet", () => quickGen("un coffre au trésor")], ["Décor", () => quickGen("une forêt")]],
    "Animation": [["Squelette & animations", () => { S.propTab = "rig"; renderProps(); }], ["Lire l'animation", playPreview, "Espace"], ["Arrêter la lecture", () => vp.stopAnimation()]],
    "Export": [["GLB", () => quickExport({ format: "glb" })], ["FBX", () => quickExport({ format: "fbx" })], ["OBJ", () => quickExport({ format: "obj" })], ["STL", () => quickExport({ format: "stl" })], ["PLY", () => quickExport({ format: "ply" })], ["BLEND", () => quickExport({ format: "blend" })], "-",
      ["Profil Unity", () => quickExport({ profile: "unity" })], ["Profil Unreal Engine", () => quickExport({ profile: "unreal" })], ["Profil Godot", () => quickExport({ profile: "godot" })], ["Profil mobile", () => quickExport({ profile: "mobile" })], ["Profil impression 3D", () => quickExport({ profile: "print3d" })], ["Profil haute qualité", () => quickExport({ profile: "high_quality" })]],
    "Paramètres": [["Préférences…", settingsDialog], ["Connexion Claude (MCP)…", mcpDialog], ["Moteurs IA facultatifs…", aiDialog], ["Raccourcis clavier", shortcutsDialog], ["À propos", aboutDialog]],
  };
  const bar = $("#menubar");
  bar.innerHTML = "";
  for (const [title, items] of Object.entries(menus)) {
    const list = h("div", { class: "menu-list" });
    for (const it of items) {
      if (it === "-") list.append(h("hr"));
      else if (it.label) list.append(h("div", { class: "label" }, it.label));
      else list.append(h("button", { onclick: () => { closeMenus(); it[1](); } }, h("span", {}, it[0]), it[2] ? h("kbd", {}, it[2]) : ""));
    }
    const m = h("div", { class: "menu" }, h("button", { onclick: (e) => { const open = m.classList.contains("open"); closeMenus(); if (!open) m.classList.add("open"); e.stopPropagation(); } }, title), list);
    m.addEventListener("mouseenter", () => { if ($(".menu.open") && !m.classList.contains("open")) { closeMenus(); m.classList.add("open"); } });
    bar.append(m);
  }
  document.addEventListener("click", (e) => { if (!e.target.closest(".menu")) closeMenus(); });
}
function closeMenus() { $$(".menu.open").forEach((m) => m.classList.remove("open")); }

async function quickExport(params) {
  const sel = S.info?.active;
  if (sel) params.name = sel;
  const r = await run("export_model", params, { success: (r) => `Export vérifié : ${r.path}` });
  if (r) { const a = h("a", { href: fileUrl(r.path, true), download: "" }); document.body.append(a); a.click(); a.remove(); }
}

async function quickGen(text) {
  setWorkspace("generate");
  const ta = $("#gen-prompt");
  if (ta) { ta.value = text; ta.focus(); }
}

async function projectInfo() {
  const r = await api("get_project_info", {});
  modal("Projet", h("div", { class: "kv" }, ...[["Fichier", r.status.project_path || "non enregistré"], ["Modifié", r.status.dirty ? "oui" : "non"], ["Blender", r.status.blender_version], ["Dossier de travail", r.home], ...Object.entries(r.folders)].flatMap(([k, v]) => [h("span", {}, k), h("span", {}, String(v))])), [["Fermer", null]]);
}

async function filesDialog(kind) {
  const r = await api("list_files", { kind });
  modal(`Fichiers — ${kind}`, h("div", { style: { display: "grid", gap: "4px" } }, h("div", { class: "hint" }, r.folder),
    ...(r.files.length ? r.files.map((f) => h("div", { class: "hist-row" }, h("span", { style: { flex: 1 } }, f.name), h("span", { class: "hint" }, `${Math.round(f.bytes / 1024)} Ko`),
      h("a", { class: "btn small", href: fileUrl(f.path, true), download: "" }, "Télécharger"))) : [h("div", { class: "empty" }, "Aucun fichier.")])), [["Fermer", null]], { wide: true });
}

async function settingsDialog() {
  const info = await api("get_project_info", {});
  const s = info.settings;
  const eng = select([["CYCLES", "Cycles"], ["BLENDER_EEVEE", "EEVEE (GPU)"], ["BLENDER_WORKBENCH", "Workbench"]], s.render_engine, () => {});
  const samples = numInput(s.render_samples, () => {}, { step: 16, min: 1 });
  const hist = numInput(s.history_limit, () => {}, { step: 5, min: 5 });
  const py = checkbox(s.allow_python, () => {});
  const r = await modal("Préférences", h("div", { style: { display: "grid", gap: "8px" } },
    row("Moteur de rendu", eng), row("Échantillons", samples), row("Étapes d'historique", hist),
    row("Autoriser le code Python (Claude)", py),
    h("div", { class: "warnbox" }, "Le code Python permet à Claude d'exécuter n'importe quelle opération Blender via l'outil run_python. N'activez cette option que si vous faites confiance aux demandes envoyées.")),
    [["Annuler", null], ["Enregistrer", true, "primary"]]);
  if (r) await run("set_settings", { render_engine: eng.value, render_samples: parseInt(samples.value, 10), history_limit: parseInt(hist.value, 10), allow_python: py.checked }, { success: "Préférences enregistrées" });
}

function mcpDialog() {
  modal("Connexion à Claude Desktop (MCP)", h("div", { style: { display: "grid", gap: "8px" } },
    h("p", { style: { margin: 0 } }, "Claude Desktop lance le serveur MCP local de SUPERESSENCE (transport stdio). Ce serveur pilote ce studio par l'API locale protégée par jeton."),
    h("ol", {}, h("li", {}, "Ouvrez Claude Desktop › Paramètres › Développeur › Modifier la configuration."),
      h("li", {}, "Le script d'installation l'a déjà ajouté automatiquement (avec sauvegarde). Sinon, collez le contenu de superessence3d/claude_desktop_config.json."),
      h("li", {}, "Redémarrez Claude Desktop : l'icône d'outils affiche « superessence ».")),
    h("p", { class: "hint" }, "Le studio démarre automatiquement si Claude l'appelle alors qu'il est fermé. Les opérations destructrices demandent une confirmation (confirm=true). Documentation complète : README.md, section « Connexion à Claude »."),
    h("pre", {}, '{\n  "mcpServers": {\n    "superessence": {\n      "command": "<dossier>/superessence3d/.venv/bin/python",\n      "args": ["-m", "superessence.mcp_server"],\n      "env": { "PYTHONPATH": "<dossier>/superessence3d" }\n    }\n  }\n}')), [["Fermer", null]], { wide: true });
}

async function aiDialog() {
  const r = await api("list_ai_backends", {});
  const info = await api("get_project_info", {});
  modal("Moteurs IA neuronaux (facultatifs)", h("div", { style: { display: "grid", gap: "8px" } },
    h("div", { class: "note" }, "SUPERESSENCE fonctionne entièrement sans IA neuronale (génération procédurale). Si vous installez un modèle open source local (ex. TripoSR, licence MIT), déclarez sa commande ici : le studio l'exécute, vérifie le fichier produit et l'importe."),
    r.backends.length ? h("div", {}, ...r.backends.map((b) => h("div", { class: "hist-row" }, h("b", {}, b.name), h("span", { class: "hint" }, `licence ${b.license}`), h("span", { style: { color: b.available ? "var(--ok)" : "var(--err)" } }, b.available ? "disponible" : "introuvable")))) : h("div", { class: "empty" }, "Aucun moteur configuré."),
    h("p", { class: "hint" }, "Configuration : fichier settings.json (clé « ai_backends ») dans " + info.home + ". Exemple dans README.md. Vérifiez toujours la licence du modèle avant un usage commercial.")), [["Fermer", null]], { wide: true });
}

function shortcutsDialog() {
  const k = [["Q / G / R / S", "Sélection / Déplacer / Rotation / Échelle"], ["Tab", "Mode édition"], ["F", "Cadrer la sélection"], ["Suppr", "Supprimer"], ["Maj+D", "Dupliquer"], ["A", "Tout sélectionner"], ["Ctrl+Z / Ctrl+Y", "Annuler / Rétablir"], ["Ctrl+S", "Enregistrer"], ["1 / 3 / 7 / 0", "Vue face / droite / dessus / caméra"], ["Espace", "Lire l'animation"], ["Échap", "Revenir au mode objet"], ["Alt+clic", "Sélectionner le modèle entier"], ["Double-clic", "Cadrer l'objet"]];
  modal("Raccourcis clavier", h("div", { class: "kv" }, ...k.flatMap(([a, b]) => [h("span", { style: { fontFamily: "var(--mono)" } }, a), h("span", {}, b)])), [["Fermer", null]]);
}

async function aboutDialog() {
  const st = await getJSON("/api/status");
  modal("À propos", h("div", { style: { display: "grid", gap: "8px" } },
    h("p", { style: { margin: 0 } }, h("b", {}, "SUPERESSENCE 3D STUDIO "), `v${window.SE_VERSION}`),
    h("p", { style: { margin: 0 } }, `Moteur : Blender ${st.engine.blender_version} (bpy) — source de vérité de la scène. Affichage : three.js. Serveur : FastAPI. Claude : protocole MCP.`),
    h("p", { class: "hint", style: { margin: 0 } }, "Génération procédurale : géométrie construite par algorithmes (graphes de squelette, révolutions, lofts, membranes, textures procédurales). Ce n'est pas un réseau neuronal. Licences des composants : voir LICENSES.md.")), [["Fermer", null]]);
}

// --------------------------------------------------------------------------- espaces de travail
export function setWorkspace(ws) {
  S.ws = ws;
  $("#layout").dataset.ws = ws;
  $$("#workspaces button").forEach((b) => b.classList.toggle("active", b.dataset.ws === ws));
  if (ws === "library") refreshLibrary();
  setTimeout(() => vp.resize(), 30);
}

// --------------------------------------------------------------------------- clavier
function bindKeys() {
  document.addEventListener("keydown", (e) => {
    if (e.target.matches("input, textarea, select")) return;
    const k = e.key.toLowerCase();
    const ctrl = e.ctrlKey || e.metaKey;
    if (ctrl && k === "z") { e.preventDefault(); e.shiftKey ? redo() : undo(); }
    else if (ctrl && k === "y") { e.preventDefault(); redo(); }
    else if (ctrl && k === "s") { e.preventDefault(); saveProject(); }
    else if (ctrl && k === "o") { e.preventDefault(); openProject(); }
    else if (ctrl && k === "i") { e.preventDefault(); importFile(); }
    else if (ctrl && k === "e") { e.preventDefault(); S.propTab = "export"; renderProps(); }
    else if (ctrl && k === "j") { e.preventDefault(); run("join_objects", { names: [...vp.selected] }).then(() => scheduleRefresh(0)); }
    else if (ctrl) return;
    else if (k === "tab") { e.preventDefault(); setMode(S.mode === "edit" ? "object" : "edit"); }
    else if (k === "q") setTool("select");
    else if (k === "g") setTool("move");
    else if (k === "r") setTool("rotate");
    else if (k === "s") setTool("scale");
    else if (k === "f" || k === ".") vp.frameObjects([...vp.selected]);
    else if (k === "delete" || k === "x") { if (S.mode === "edit") editOp("delete"); else deleteSelection(); }
    else if (k === "d" && e.shiftKey) duplicateSelection();
    else if (k === "a") { if (S.mode === "edit") vp.setEditSelection([...Array(vp.selectMode === "face" ? vp.editData.d.faces : vp.editData.d.vertices).keys()]); else api("select", { mode: "all" }).then(() => scheduleRefresh(0)); }
    else if (k === "e" && S.mode === "edit") editOp("extrude", { distance: 0.3 });
    else if (k === "i" && S.mode === "edit") editOp("inset", { thickness: 0.05 });
    else if (k === "1") vp.view("front");
    else if (k === "3") vp.view("right");
    else if (k === "7") vp.view("top");
    else if (k === "5") vp.view("persp");
    else if (k === "0") vp.cameraView();
    else if (k === " ") { e.preventDefault(); vp.anim ? vp.stopAnimation() : playPreview(); }
    else if (k === "escape") { $("#vp-render").classList.add("hidden"); if (S.mode !== "object") setMode("object"); setTool("select"); }
  });
}

// --------------------------------------------------------------------------- événements moteur
function bindEngineEvents() {
  on("connection", ({ connected }) => {
    $("#conn").classList.toggle("on", connected);
    $("#conn").title = connected ? "Connecté au moteur Blender" : "Déconnecté — reconnexion…";
    if (connected) scheduleRefresh(0);
  });
  on("scene_changed", () => scheduleRefresh(60));
  on("history", (m) => { S.history = m.history; if (S.bottomTab === "history") renderBottom(); });
  on("log", (m) => log(m.level, m.message));
  on("progress", (m) => {
    const j = S.jobs.get("progress") || { message: "", status: "" };
    j.message = m.message; j.fraction = m.fraction; j.status = m.fraction >= 1 ? "done" : "running";
    S.jobs.set("progress", j);
    showBusy(m.fraction >= 1 ? null : m.message);
    if (S.bottomTab === "jobs") renderBottom();
  });
  on("job", (m) => {
    S.jobs.set(m.id, { message: m.message, status: m.status, fraction: m.status === "done" ? 1 : m.status === "error" ? 1 : 0.3 });
    toast(m.message, m.status === "error" ? "error" : m.status === "done" ? "ok" : "");
    if (S.bottomTab === "jobs") renderBottom();
  });
  on("library_changed", () => { if (S.ws === "library") refreshLibrary(); });
  on("busy", ({ pending, name }) => { $("#conn").classList.toggle("busy", pending > 0); });
}

// --------------------------------------------------------------------------- démarrage
function init() {
  buildMenus();
  buildToolbar();
  renderModeSwitch();
  renderPropTabs();
  renderBottom();
  bindKeys();
  bindEngineEvents();
  $$("#workspaces button").forEach((b) => b.addEventListener("click", () => setWorkspace(b.dataset.ws)));
  $("#vp-render .close").addEventListener("click", () => $("#vp-render").classList.add("hidden"));
  initGenerate({ S, vp, scheduleRefresh, setWorkspace, showBusy });
  initLibrary({ S, vp, scheduleRefresh, setWorkspace });
  connect();
  api("get_history", {}, { quiet: true }).then((r) => { S.history = r.history; S.logs.push(...r.log); renderBottom(); });
  refresh().then(() => vp.frameObjects());
}

init();
