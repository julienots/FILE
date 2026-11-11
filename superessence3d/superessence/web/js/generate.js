// Espace « Génération IA » : description → spécification → modèle procédural Blender.
import { api, upload } from "./api.js";
import { $, btn, checkbox, colorInput, h, hexFromRgb, modal, numInput, row, run, section, select, slider, toast } from "./widgets.js";

const QUICK = [
  ["Personnage", "un chevalier en armure"], ["Monstre", "un monstre violet avec six tentacules et trois yeux jaunes lumineux"],
  ["Dragon", "Crée un dragon noir de 2 mètres, avec quatre ailes, des cornes, des écailles détaillées, des yeux rouges et une queue avec des pointes."],
  ["Animal", "un loup gris"], ["Bâtiment", "un château médiéval en pierre"], ["Objet", "un coffre au trésor"], ["Décor", "une forêt"],
  ["Véhicule", "un vaisseau spatial blanc"], ["Arme fictive", "une épée en acier avec des runes lumineuses bleues"], ["Ressource jeu", "un tonneau"],
];

const FEATURES = [["wings", "Ailes"], ["horns", "Cornes"], ["tails", "Queues"], ["tentacles", "Tentacules"], ["heads", "Têtes"], ["eyes", "Yeux"], ["legs", "Pattes"], ["arms", "Bras"]];
const FLAGS = [["spikes", "Pointes"], ["scales", "Écailles"], ["claws", "Griffes"], ["fangs", "Crocs"], ["fur", "Fourrure"], ["feathers", "Plumes"], ["shell", "Carapace"]];

export function initGenerate(ctx) {
  const { S, vp, scheduleRefresh, showBusy } = ctx;
  const panel = $("#gen-panel");
  const G = { spec: null, options: null, params: null, images: [] };

  async function load() {
    G.options = await api("get_generation_options", {}, { quiet: true });
    G.params = { ...G.options.defaults };
    render();
  }

  function render() {
    panel.innerHTML = "";
    panel.append(h("div", { class: "gen-hero" }, h("h2", {}, "SUPERESSENCE TRIPO — génération 3D"),
      h("p", {}, "Décrivez un modèle en français ou en anglais. Le moteur procédural construit une vraie géométrie Blender éditable, avec matériaux, UV et squelette.")));
    const ta = h("textarea", { id: "gen-prompt", placeholder: "Ex. : Crée un dragon noir de 2 mètres, avec quatre ailes, des cornes, des écailles détaillées, des yeux rouges et une queue avec des pointes." });
    ta.value = G.lastPrompt || "";
    ta.addEventListener("input", () => { G.lastPrompt = ta.value; });
    ta.addEventListener("keydown", (e) => { if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) generate(); });
    panel.append(ta);
    panel.append(h("div", { class: "chips", style: { margin: "8px 0 10px" } }, ...QUICK.map(([l, p]) => h("button", { class: "chip", onclick: () => { ta.value = p; G.lastPrompt = p; analyze(); } }, l))));
    panel.append(h("div", { class: "grid2", style: { marginBottom: "10px" } },
      btn("Analyser la description", () => analyze()),
      h("button", { class: "btn primary", onclick: () => generate() }, "✦ Générer le modèle")));
    if (G.spec) panel.append(specEditor());
    panel.append(paramsPanel());
    panel.append(modifyPanel());
    panel.append(imagePanel());
    panel.append(h("div", { class: "note", style: { marginTop: "8px" } }, G.options?.honesty || ""));
  }

  async function analyze() {
    const text = $("#gen-prompt").value.trim();
    if (!text) return toast("Écrivez une description.", "warn");
    const r = await run("parse_prompt", { prompt: text });
    if (r) { G.spec = r.spec; render(); }
  }

  async function generate() {
    const text = $("#gen-prompt").value.trim();
    if (!text && !G.spec) return toast("Écrivez une description.", "warn");
    let spec = G.spec;
    if (!spec || spec.prompt !== text) {
      const r = await api("parse_prompt", { prompt: text });
      spec = r.spec;
      G.spec = spec;
    }
    showBusy("Génération procédurale en cours…");
    try {
      const r = await run("generate_from_prompt", { spec, params: G.params }, { success: (r) => `Modèle « ${r.name} » généré en ${r.seconds} s — ${r.stats.triangles_evaluated.toLocaleString("fr")} triangles` });
      if (r) {
        render();
        scheduleRefresh(0);
        setTimeout(() => vp.frameObjects([r.name, ...r.parts]), 500);
      }
    } finally { showBusy(null); }
  }

  function specEditor() {
    const sp = G.spec;
    const cats = G.options.categories;
    const catSel = select(Object.entries(cats).map(([k, v]) => [k, v.label]), sp.category, (v) => { sp.category = v; sp.subtype = cats[v].subtypes[0]; render(); });
    const subSel = select((cats[sp.category]?.subtypes || []).map((s) => [s, s]), sp.subtype, (v) => { sp.subtype = v; });
    const height = numInput(sp.height ?? 0, (v) => { sp.height = v || undefined; }, { step: 0.1, min: 0 });
    const colors = sp.colors || (sp.colors = {});
    const colorRow = (key, label) => row(label, h("div", { style: { display: "grid", gridTemplateColumns: "1fr auto", gap: "6px", alignItems: "center" } },
      colorInput(colors[key] ? hexFromRgb(colors[key]) : "#808080", (v) => { colors[key] = v; }),
      h("button", { class: "icon-btn", title: "Couleur automatique", onclick: () => { delete colors[key]; render(); } }, colors[key] ? "✕" : "auto")));
    const feats = sp.features || (sp.features = {});
    const featGrid = h("div", { class: "feat-grid" }, ...FEATURES.map(([k, l]) => row(l, numInput(feats[k] ?? 0, (v) => { feats[k] = Math.max(0, Math.round(v)); }, { step: 1, min: 0 }))));
    const flagGrid = h("div", { class: "chips" }, ...FLAGS.map(([k, l]) => {
      const c = h("button", { class: `chip ${feats[k] ? "active" : ""}`, onclick: () => { feats[k] = !feats[k]; c.classList.toggle("active"); } }, l);
      return c;
    }));
    const presets = ["", "scales", "skin", "fur", "organic", "metal", "gold", "rust", "wood", "stone", "brick", "fabric", "leather", "plastic", "glass", "ice", "crystal", "lava", "emission"];
    const mat = select(presets.map((p) => [p, p || "automatique"]), sp.material || "", (v) => { sp.material = v || undefined; });
    const style = select([["", "automatique"], ["realistic", "Réaliste"], ["cartoon", "Cartoon"], ["lowpoly", "Low-poly"]], sp.style || "", (v) => { sp.style = v || undefined; });
    const build = select([["", "normale"], ["thin", "Mince"], ["heavy", "Massive"]], sp.build || "", (v) => { sp.build = v || undefined; });
    const glowEyes = checkbox(sp.glow_eyes, (v) => { sp.glow_eyes = v; });
    const glow = checkbox(sp.glow, (v) => { sp.glow = v; });
    const s = section("Interprétation (modifiable)", true,
      h("div", { class: "hint" }, (sp.notes || []).join(" · ") || "Vérifiez et ajustez avant de générer."),
      row("Catégorie", catSel), row("Type", subSel), row("Hauteur (m, 0 = auto)", height),
      colorRow("primary", "Couleur principale"), colorRow("secondary", "Couleur secondaire"), colorRow("eyes", "Yeux"),
      row("Matériau", mat), row("Style", style), row("Carrure", build), row("Yeux lumineux", glowEyes), row("Parties lumineuses", glow),
      h("div", { class: "hint" }, "Nombre de parties"), featGrid, flagGrid);
    return s;
  }

  function paramsPanel() {
    const p = G.params;
    const q = h("div", { class: "chips" }, ...[["draft", "Draft"], ["standard", "Standard"], ["high", "High"], ["ultra", "Ultra"]].map(([k, l]) => {
      const c = h("button", { class: `chip ${p.quality === k ? "active" : ""}`, onclick: () => { p.quality = k; render(); } }, l);
      return c;
    }));
    const sub = select([["", "selon la qualité"], ["0", "0"], ["1", "1"], ["2", "2"], ["3", "3"], ["4", "4"]], p.subdivision ?? "", (v) => { p.subdivision = v === "" ? null : parseInt(v, 10); });
    const target = numInput(p.target_polycount || 0, (v) => { p.target_polycount = Math.max(0, Math.round(v)); }, { step: 1000, min: 0 });
    const res = select([["", "aucun remaillage"], ["0.04", "faible (voxels 4 %)"], ["0.02", "moyenne (2 %)"], ["0.01", "élevée (1 %)"], ["0.005", "très élevée (0,5 %)"]], p.resolution ?? "", (v) => { p.resolution = v ? parseFloat(v) : null; p.remesh = !!v; });
    const tex = select([["0", "Aucune (procédural)"], ["512", "512"], ["1024", "1024"], ["2048", "2048"], ["4096", "4096"]], String(p.texture_resolution || 0), (v) => { p.texture_resolution = parseInt(v, 10); });
    const maps = [["base_color", "Base Color"], ["metallic", "Metallic"], ["roughness", "Roughness"], ["normal", "Normal"], ["height", "Height"], ["ao", "Ambient Occlusion"], ["emission", "Émission"]];
    const mapSet = new Set(p.texture_maps || []);
    const mapBox = h("div", { class: "grid2" }, ...maps.map(([k, l]) => h("label", { style: { display: "flex", gap: "6px", alignItems: "center" } }, checkbox(mapSet.has(k), (v) => { v ? mapSet.add(k) : mapSet.delete(k); p.texture_maps = [...mapSet]; }), l)));
    const opt = (k, l) => h("label", { style: { display: "flex", gap: "6px", alignItems: "center" } }, checkbox(p[k], (v) => { p[k] = v; }), l);
    const lods = numInput(p.lods || 0, (v) => { p.lods = Math.max(0, Math.min(4, Math.round(v))); }, { step: 1, min: 0, max: 4 });
    return section("Paramètres de génération", true,
      h("div", { class: "hint" }, "Qualité"), q,
      h("div", { class: "hint" }, "Draft : rapide, subdivision 1 · Standard : subdivision 2 · High : + détails géométriques (écailles) · Ultra : subdivision 3."),
      section("Géométrie", false,
        row("Subdivision", sub), row("Résolution (remesh)", res), row("Triangles cible (0 = libre)", target),
        h("div", { class: "grid2" }, opt("smoothing", "Lissage"), opt("symmetry", "Symétrie"))),
      section("Textures & matériaux", false,
        row("Résolution de cuisson", tex), h("div", { class: "hint" }, "Cartes PBR à cuire (si résolution choisie)"), mapBox,
        h("div", { class: "grid2" }, opt("generate_materials", "Générer les matériaux"), opt("generate_uv", "Générer les UV")),
        h("div", { class: "hint" }, "La cuisson utilise Cycles sur processeur : 2048–4096 px peut prendre plusieurs minutes.")),
      section("Options", false,
        h("div", { class: "grid2" }, opt("auto_optimize", "Optimisation auto"), opt("cleanup", "Nettoyage géométrie"), opt("fix_normals", "Correction des normales"),
          opt("fill_holes", "Fermeture des trous"), opt("collision", "Génération de collision"), opt("game_ready", "Préparer pour moteur de jeu"), opt("apply_modifiers", "Appliquer les modificateurs")),
        row("Niveaux de détail (LOD)", lods),
        h("div", { class: "hint" }, "Contrôle de l'échelle : hauteur dans l'interprétation (mètres réels).")));
  }

  function modifyPanel() {
    const target = S.info?.active;
    const ta = h("textarea", { placeholder: "Ex. : Agrandis ses ailes, ajoute six tentacules et remplace les yeux par des yeux blancs lumineux." });
    return section("Modifier un modèle existant", false,
      h("div", { class: "hint" }, target ? `Modèle visé : ${target} (objet actif — une partie suffit)` : "Sélectionnez d'abord un modèle généré dans le viewport."),
      ta, btn("Appliquer la modification", async () => {
        if (!target) return toast("Sélectionnez un modèle.", "warn");
        if (!ta.value.trim()) return;
        showBusy("Modification du modèle…");
        try {
          const r = await run("modify_model", { name: target, instruction: ta.value }, { success: (r) => `Modifications : ${r.changes.join(" · ")}` });
          if (r) scheduleRefresh(0);
        } finally { showBusy(null); }
      }, "primary"));
  }

  function imagePanel() {
    const list = h("div", { class: "hint" }, G.images.length ? G.images.map((p) => p.split(/[\\/]/).pop()).join(" · ") : "Aucune image chargée.");
    const height = numInput(1.5, () => {}, { step: 0.1, min: 0.01 });
    const depth = slider(0.5, () => {}, { min: 0.05, max: 1.5, step: 0.05 });
    const backends = select([["", "Méthode géométrique (incluse)"]], "", () => {});
    api("list_ai_backends", {}, { quiet: true }).then((r) => {
      for (const b of r.backends) backends.append(h("option", { value: b.name, disabled: !b.available }, `${b.name} (neuronal, ${b.available ? "disponible" : "introuvable"})`));
    }).catch(() => {});
    const add = h("input", { type: "file", accept: "image/*", multiple: true, style: { display: "none" } });
    add.addEventListener("change", async () => {
      for (const f of add.files) {
        try { G.images.push(await upload(f)); } catch (e) { toast(e.message, "error"); }
      }
      G.images = G.images.slice(-3);
      render();
    });
    return section("Image(s) vers 3D", false,
      h("div", { class: "warnbox" }, "Méthode incluse : géométrique, pas neuronale. 1 image = silhouette gonflée et texturée. 2–3 images (face, profil droit, dessus) = enveloppe visuelle par silhouettes. Utilisez des images sur fond uni ou transparent."),
      add, h("div", { class: "btns" }, btn("Choisir des images…", () => add.click()), btn("Vider", () => { G.images = []; render(); })), list,
      row("Hauteur (m)", height), row("Épaisseur (1 image)", depth), row("Moteur", backends),
      btn("Créer le modèle depuis les images", async () => {
        if (!G.images.length) return toast("Ajoutez au moins une image.", "warn");
        const params = { height: parseFloat(height.value), depth: depth.get() };
        if (backends.value) { params.backend = backends.value; params.image = G.images[0]; }
        else if (G.images.length >= 2) params.images = G.images;
        else params.image = G.images[0];
        showBusy("Reconstruction depuis les images…");
        try {
          const r = await run("image_to_model", params, { success: (r) => r.method ? `Modèle « ${r.name} » — ${r.method}` : r.message });
          if (r?.name) { scheduleRefresh(0); setTimeout(() => vp.frameObjects([r.name]), 500); }
        } finally { showBusy(null); }
      }, "primary"));
  }

  load().catch((e) => { panel.append(h("div", { class: "warnbox" }, e.message)); });
}
