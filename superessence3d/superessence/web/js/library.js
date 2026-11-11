// Bibliothèque de modèles : miniatures rendues par Blender + aperçu 3D (GLB) + versions.
import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { api, fileUrl } from "./api.js";
import { $, btn, h, modal, prompt, row, run, select, toast } from "./widgets.js";

let ctx;
const L = { query: "", category: "" };

export function initLibrary(c) { ctx = c; }

export async function refreshLibrary() {
  const view = $("#library-view");
  const r = await api("library_list", { query: L.query, category: L.category }, { quiet: true }).catch((e) => { toast(e.message, "error"); return null; });
  if (!r) return;
  view.innerHTML = "";
  const search = h("input", { type: "text", placeholder: "Rechercher (nom, tags, description)…", value: L.query });
  let t;
  search.addEventListener("input", () => { clearTimeout(t); t = setTimeout(() => { L.query = search.value; refreshLibrary(); }, 250); });
  const cat = select([["", "Toutes les catégories"], ...r.categories.map((c) => [c, c])], L.category, (v) => { L.category = v; refreshLibrary(); });
  view.append(h("div", { class: "lib-head" }, h("h2", {}, "Bibliothèque"), search, cat,
    btn("+ Enregistrer la sélection", async () => {
      const act = ctx.S.info?.active;
      if (!act) return toast("Sélectionnez un modèle dans le Studio.", "warn");
      let root = act;
      const byName = Object.fromEntries(ctx.S.info.objects.map((o) => [o.name, o]));
      while (byName[root]?.parent) root = byName[root].parent;
      await run("library_save", { name: root }, { success: `« ${root} » ajouté — miniature en cours de rendu par Blender` });
      refreshLibrary();
    }, "primary")));
  if (!r.models.length) {
    view.append(h("div", { class: "empty" }, "La bibliothèque est vide. Générez un modèle puis « Enregistrer la sélection », ou clic droit dans l'outliner › Enregistrer dans la bibliothèque."));
    return;
  }
  const grid = h("div", { class: "lib-grid" });
  for (const m of r.models) grid.append(card(m));
  view.append(grid);
}

function card(m) {
  const v = m.versions[0];
  const thumb = h("div", { class: "thumb", title: "Cliquer : aperçu 3D" });
  if (v?.thumbnail) thumb.append(h("img", { src: fileUrl(v.thumbnail), alt: m.name, loading: "lazy" }));
  else thumb.append(h("div", { class: "hint" }, "miniature en cours…"));
  thumb.addEventListener("click", () => v?.glb && preview3d(m, v));
  return h("div", { class: "card" }, thumb,
    h("div", { class: "meta" }, h("b", { title: m.name }, m.name), h("small", {}, `${m.category} · v${v?.version ?? "?"} · ${(v?.stats?.triangles ?? 0).toLocaleString("fr")} tris`),
      m.tags.length ? h("small", {}, m.tags.map((t) => `#${t}`).join(" ")) : ""),
    h("div", { class: "act" },
      btn("Ajouter à la scène", async () => { await run("library_load", { model_id: m.id }, { success: `« ${m.name} » ajouté à la scène` }); ctx.scheduleRefresh(0); ctx.setWorkspace("studio"); }, "small primary"),
      btn("Modifier", () => editModel(m), "small"),
      btn("Dupliquer", async () => { await run("library_duplicate", { model_id: m.id }, { success: "Copie créée" }); refreshLibrary(); }, "small"),
      btn("Versions", () => versions(m), "small"),
      btn("Exporter", async () => {
        const r = await run("library_export", { model_id: m.id }, { success: (r) => `Copié vers : ${r.files.join(", ")}` });
        if (r) for (const f of r.files) { const a = h("a", { href: fileUrl(f, true), download: "" }); document.body.append(a); a.click(); a.remove(); }
      }, "small"),
      btn("Supprimer", async () => { const r = await run("library_delete", { model_id: m.id }); if (r) { toast("Supprimé", "ok"); refreshLibrary(); } }, "small danger")));
}

async function editModel(m) {
  const name = h("input", { type: "text", value: m.name });
  const cat = h("input", { type: "text", value: m.category });
  const tags = h("input", { type: "text", value: m.tags.join(", ") });
  const desc = h("textarea", {}, m.description);
  const r = await modal(`Modifier « ${m.name} »`, h("div", { style: { display: "grid", gap: "8px" } }, row("Nom", name), row("Catégorie", cat), row("Tags", tags), row("Description", desc),
    h("div", { class: "hint" }, "Pour modifier la géométrie : « Ajouter à la scène », éditez dans le Studio, puis « Versions › Nouvelle version depuis la sélection ».")),
    [["Annuler", null], ["Enregistrer", true, "primary"]]);
  if (!r) return;
  await run("library_update", { model_id: m.id, model_name: name.value, category: cat.value, tags: tags.value.split(",").map((s) => s.trim()).filter(Boolean), description: desc.value }, { success: "Modèle mis à jour" });
  refreshLibrary();
}

async function versions(m) {
  const body = h("div", { style: { display: "grid", gap: "6px" } },
    ...m.versions.map((v) => h("div", { class: "hist-row" }, h("b", {}, `v${v.version}`), h("span", { class: "hint" }, new Date(v.created * 1000).toLocaleString("fr")), h("span", { style: { flex: 1 } }, v.note || ""),
      btn("Charger", async () => { await run("library_load", { model_id: m.id, version: v.version }, { success: `v${v.version} ajoutée à la scène` }); ctx.scheduleRefresh(0); }, "small"))),
    btn("Nouvelle version depuis la sélection du Studio", async () => {
      const act = ctx.S.info?.active;
      if (!act) return toast("Sélectionnez le modèle dans le Studio.", "warn");
      const note = await prompt("Nouvelle version", "Note", "");
      await run("library_save", { name: act, model_id: m.id, note: note || "" }, { success: "Nouvelle version enregistrée" });
      refreshLibrary();
    }, "primary"));
  modal(`Versions — ${m.name}`, body, [["Fermer", null]]);
}

function preview3d(m, v) {
  const holder = h("div", { style: { height: "460px", position: "relative", background: "#0e0f13", borderRadius: "8px" } });
  modal(`Aperçu 3D — ${m.name} v${v.version}`, holder, [["Fermer", null]], { wide: true }).then(() => { stop = true; renderer.dispose(); });
  let stop = false;
  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  holder.append(renderer.domElement);
  const scene = new THREE.Scene();
  scene.add(new THREE.HemisphereLight(0xffffff, 0x333333, 2));
  const d = new THREE.DirectionalLight(0xffffff, 2);
  d.position.set(3, 5, 4);
  scene.add(d);
  const cam = new THREE.PerspectiveCamera(40, 1, 0.01, 1000);
  cam.up.set(0, 1, 0);
  const controls = new OrbitControls(cam, renderer.domElement);
  controls.autoRotate = true;
  const size = () => { const w = holder.clientWidth, hh = holder.clientHeight; renderer.setSize(w, hh); cam.aspect = w / hh; cam.updateProjectionMatrix(); };
  setTimeout(size, 20);
  new GLTFLoader().load(fileUrl(v.glb), (g) => {
    scene.add(g.scene);
    const box = new THREE.Box3().setFromObject(g.scene);
    const c = box.getCenter(new THREE.Vector3());
    const r = box.getSize(new THREE.Vector3()).length() / 2;
    controls.target.copy(c);
    cam.position.copy(c).add(new THREE.Vector3(r * 1.3, r * 0.7, r * 1.6));
    cam.near = r / 100; cam.far = r * 100; cam.updateProjectionMatrix();
    if (g.animations.length) {
      const mixer = new THREE.AnimationMixer(g.scene);
      g.animations.forEach((a) => mixer.clipAction(a).play());
      const clock = new THREE.Clock();
      scene.userData.mixer = () => mixer.update(clock.getDelta());
    }
  }, undefined, (e) => toast(`Aperçu impossible : ${e.message || e}`, "error"));
  const loop = () => { if (stop) return; requestAnimationFrame(loop); controls.update(); scene.userData.mixer?.(); renderer.render(scene, cam); };
  loop();
}
