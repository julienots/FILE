// Export (GLB/OBJ/STL), enregistrement sur le téléphone, projets et bibliothèque (IndexedDB), historique.
import * as THREE from "three";
import { GLTFExporter } from "../vendor/three/addons/exporters/GLTFExporter.js";
import { OBJExporter } from "../vendor/three/addons/exporters/OBJExporter.js";
import { STLExporter } from "../vendor/three/addons/exporters/STLExporter.js";
import { GLTFLoader } from "../vendor/three/addons/loaders/GLTFLoader.js";
import * as SkeletonUtils from "../vendor/three/addons/utils/SkeletonUtils.js";
import { addDetail } from "./materials.js";

function redetail(m) {
  const kind = m.userData?.seDetail?.kind || m.userData?.preset;
  if (!kind) return m;
  delete m.userData.seDetail;
  return addDetail(m, kind);
}

// ------------------------------------------------------------------ export
function clipsOf(root) {
  const clips = [];
  root.traverse((o) => { if (o.userData.clip) clips.push(o.userData.clip); });
  return clips;
}

export async function exportGLB(root, { animations = true } = {}) {
  const exp = new GLTFExporter();
  root.updateMatrixWorld(true);
  const clips = animations ? clipsOf(root) : [];
  return await exp.parseAsync(root, { binary: true, animations: clips, onlyVisible: true });
}

export function exportOBJ(root) { root.updateMatrixWorld(true); return new TextEncoder().encode(new OBJExporter().parse(root)).buffer; }
export function exportSTL(root) {
  // impression 3D : axe Z vers le haut (convention des trancheurs et de Blender)
  const holder = new THREE.Group();
  const parent = root.parent;
  holder.rotation.x = Math.PI / 2;
  holder.add(root);
  holder.updateMatrixWorld(true);
  const out = new STLExporter().parse(holder, { binary: true }).buffer;
  holder.remove(root);
  if (parent) parent.add(root);
  root.updateMatrixWorld(true);
  return out;
}

/** Vérifie réellement le fichier produit (en-têtes et contenu). */
export function verify(buf, fmt) {
  const u8 = new Uint8Array(buf);
  if (!u8.length) throw new Error("fichier vide");
  if (fmt === "glb") {
    const dv = new DataView(buf);
    if (dv.getUint32(0, true) !== 0x46546c67 || dv.getUint32(4, true) !== 2 || dv.getUint32(8, true) !== u8.length) throw new Error("GLB invalide");
    const jl = dv.getUint32(12, true);
    const js = JSON.parse(new TextDecoder().decode(u8.subarray(20, 20 + jl)));
    return { meshes: (js.meshes || []).length, animations: (js.animations || []).length, skins: (js.skins || []).length, bytes: u8.length };
  }
  if (fmt === "stl") { const n = new DataView(buf).getUint32(80, true); if (84 + n * 50 !== u8.length) throw new Error("STL invalide"); return { triangles: n, bytes: u8.length }; }
  const txt = new TextDecoder().decode(u8);
  const v = (txt.match(/^v /gm) || []).length;
  if (!v) throw new Error("OBJ invalide");
  return { vertices: v, bytes: u8.length };
}

function toBase64(buf) {
  const u8 = new Uint8Array(buf);
  let s = "";
  for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000));
  return btoa(s);
}

/** Enregistre un fichier : sur Android via le sélecteur de fichiers du système, sinon téléchargement. */
export function saveFile(name, mime, buf) {
  if (window.Android?.saveFile) {
    window.Android.saveFile(name, mime, toBase64(buf));
    return "android";
  }
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([buf], { type: mime }));
  a.download = name;
  document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 4000);
  return "download";
}

export async function loadGLB(buf) {
  const gltf = await new GLTFLoader().parseAsync(buf, "");
  return gltf;
}

// ------------------------------------------------------------------ IndexedDB
let dbp = null;
function db() {
  return (dbp ||= new Promise((res, rej) => {
    const r = indexedDB.open("superessence3d", 1);
    r.onupgradeneeded = () => { const d = r.result; d.createObjectStore("projects", { keyPath: "id" }); d.createObjectStore("library", { keyPath: "id" }); };
    r.onsuccess = () => res(r.result);
    r.onerror = () => rej(r.error);
  }));
}
async function tx(store, mode, fn) {
  const d = await db();
  return new Promise((res, rej) => {
    const t = d.transaction(store, mode);
    const s = t.objectStore(store);
    const out = fn(s);
    t.oncomplete = () => res(out?.result ?? out);
    t.onerror = () => rej(t.error);
  });
}
export const store = {
  put: (s, v) => tx(s, "readwrite", (o) => o.put(v)),
  get: (s, id) => tx(s, "readonly", (o) => o.get(id)),
  del: (s, id) => tx(s, "readwrite", (o) => o.delete(id)),
  all: (s) => tx(s, "readonly", (o) => o.getAll()),
};

/** Sérialise la scène (GLB : géométrie, matériaux, squelettes, animations, spec dans les extras). */
export async function serialize(content) {
  return await exportGLB(content, { animations: true });
}

export async function deserialize(buf) {
  const gltf = await loadGLB(buf);
  const scene = gltf.scene;
  const root = scene.children.length === 1 && scene.children[0].name === "Scène" ? scene.children[0] : scene;
  const objs = [...root.children];
  root.traverse((o) => { if (o.isMesh && !Array.isArray(o.material)) redetail(o.material); });
  for (const clip of gltf.animations) {
    // rattache chaque clip à l'objet qui contient ses pistes
    const first = clip.tracks[0]?.name.split(".")[0];
    const owner = objs.find((o) => o.name === first || o.getObjectByName(first));
    if (owner) owner.userData.clip = clip;
  }
  return objs;
}

// ------------------------------------------------------------------ historique (annuler / rétablir)
export function cloneDeep(obj) {
  const c = SkeletonUtils.clone(obj);
  c.traverse((o) => {
    if (o.isMesh) {
      o.geometry = o.geometry.clone();
      o.material = Array.isArray(o.material) ? o.material.map((m) => redetail(m.clone())) : redetail(o.material.clone());
    }
  });
  c.userData = JSON.parse(JSON.stringify({ ...obj.userData, clip: undefined, skeleton: undefined }));
  if (obj.userData.clip) c.userData.clip = obj.userData.clip;
  return c;
}

export class History {
  constructor(limit = 20) { this.stack = []; this.index = -1; this.limit = limit; }
  push(label, content) {
    this.stack = this.stack.slice(0, this.index + 1);
    this.stack.push({ label, objs: content.children.map(cloneDeep), time: Date.now() });
    while (this.stack.length > this.limit) this.stack.shift();
    this.index = this.stack.length - 1;
  }
  canUndo() { return this.index > 0; }
  canRedo() { return this.index < this.stack.length - 1; }
  restore(i, content) {
    this.index = i;
    content.clear();
    for (const o of this.stack[i].objs) content.add(cloneDeep(o));
    return this.stack[i].label;
  }
}
