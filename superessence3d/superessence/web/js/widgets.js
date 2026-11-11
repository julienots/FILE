// Éléments d'interface réutilisables : icônes, champs, sections, modales, notifications.
import { api, ApiError } from "./api.js";

export const $ = (sel, root = document) => root.querySelector(sel);
export const $$ = (sel, root = document) => [...root.querySelectorAll(sel)];

export function h(tag, attrs = {}, ...children) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v === undefined || v === null || v === false) continue;
    if (k === "class") el.className = v;
    else if (k === "style" && typeof v === "object") Object.assign(el.style, v);
    else if (k.startsWith("on") && typeof v === "function") el.addEventListener(k.slice(2), v);
    else if (k === "html") el.innerHTML = v;
    else if (v === true) el.setAttribute(k, "");
    else el.setAttribute(k, v);
  }
  for (const c of children.flat()) {
    if (c === null || c === undefined || c === false) continue;
    el.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return el;
}

const P = (d) => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">${d}</svg>`;
export const ICONS = {
  select: P('<path d="M5 3l14 8-6 2-3 6z"/>'),
  move: P('<path d="M12 2v20M2 12h20M12 2l-3 3M12 2l3 3M12 22l-3-3M12 22l3-3M2 12l3-3M2 12l3 3M22 12l-3-3M22 12l-3 3"/>'),
  rotate: P('<path d="M21 12a9 9 0 1 1-3-6.7"/><path d="M21 3v6h-6"/>'),
  scale: P('<rect x="3" y="3" width="8" height="8"/><path d="M21 21H11V11"/><path d="M14 14l7 7M21 15v6h-6"/>'),
  sculpt: P('<path d="M4 20c4 0 5-3 7-6s4-7 9-10"/><circle cx="17" cy="7" r="3"/>'),
  model: P('<path d="M12 2l9 5v10l-9 5-9-5V7z"/><path d="M12 22V12M3 7l9 5 9-5"/>'),
  material: P('<circle cx="12" cy="12" r="9"/><path d="M12 3a9 9 0 0 0 0 18"/><circle cx="8.5" cy="9" r="1.2" fill="currentColor"/>'),
  texture: P('<rect x="3" y="3" width="18" height="18" rx="2"/><path d="M3 9h18M3 15h18M9 3v18M15 3v18"/>'),
  paint: P('<path d="M19 3l2 2-9 9-3 1 1-3z"/><path d="M7 15c-2 0-4 2-4 6 3 0 6-1 6-4"/>'),
  weight: P('<path d="M6 8h12l2 13H4z"/><circle cx="12" cy="5" r="2"/>'),
  eye: P('<path d="M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>'),
  eyeoff: P('<path d="M3 3l18 18M10.6 5.1A10 10 0 0 1 12 5c6 0 10 7 10 7a17 17 0 0 1-3.2 3.9M6.6 6.6C3.9 8.4 2 12 2 12s4 7 10 7a9.7 9.7 0 0 0 5.4-1.6"/>'),
  mesh: P('<path d="M12 2l9 5v10l-9 5-9-5V7z"/>'),
  empty: P('<path d="M12 4v16M4 12h16M7 7l10 10M17 7 7 17"/>'),
  light: P('<circle cx="12" cy="10" r="5"/><path d="M9 18h6M10 21h4"/>'),
  camera: P('<rect x="2" y="7" width="14" height="11" rx="2"/><path d="M16 11l6-3v9l-6-3z"/>'),
  armature: P('<path d="M12 2l3 5-3 15-3-15z"/>'),
  collection: P('<path d="M3 7h7l2 2h9v10H3z"/>'),
  trash: P('<path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3"/>'),
  copy: P('<rect x="8" y="8" width="13" height="13" rx="2"/><path d="M16 8V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h3"/>'),
  plus: P('<path d="M12 5v14M5 12h14"/>'),
  play: P('<path d="M7 4v16l13-8z"/>'),
  pause: P('<path d="M7 4h4v16H7zM14 4h4v16h-4z"/>'),
  stop: P('<rect x="6" y="6" width="12" height="12"/>'),
  up: P('<path d="M12 19V5M5 12l7-7 7 7"/>'),
  down: P('<path d="M12 5v14M5 12l7 7 7-7"/>'),
  check: P('<path d="M4 12l5 5L20 6"/>'),
  focus: P('<circle cx="12" cy="12" r="3"/><path d="M3 8V3h5M16 3h5v5M21 16v5h-5M8 21H3v-5"/>'),
  render: P('<path d="M4 4h16v12H4z"/><path d="M8 20h8M12 16v4"/><circle cx="12" cy="10" r="3"/>'),
  wand: P('<path d="M15 4V2M15 10V8M11 6h2M17 6h2M18 3l-1.5 1.5M12 3l1.5 1.5M3 21l12-12"/>'),
  folder: P('<path d="M3 6h6l2 2h10v11H3z"/>'),
  save: P('<path d="M5 3h12l4 4v14H5z"/><path d="M8 3v6h8V3M8 21v-7h8v7"/>'),
};

export function icon(name) {
  const s = document.createElement("span");
  s.className = "ic";
  s.style.display = "inline-flex";
  s.innerHTML = ICONS[name] || ICONS.mesh;
  return s;
}

// ---------------------------------------------------------------- champs
export function row(label, input, title) {
  return h("div", { class: "row", title: title || "" }, h("label", {}, label), input);
}

export function numInput(value, onchange, { step = 0.1, min, max, digits = 3 } = {}) {
  const el = h("input", { type: "number", step, min, max, value: fmt(value, digits) });
  el.addEventListener("change", () => {
    const v = parseFloat(el.value);
    if (!Number.isNaN(v)) onchange(v);
  });
  return el;
}

export function fmt(v, digits = 3) {
  if (typeof v !== "number") return v ?? "";
  return Number.isInteger(v) ? String(v) : String(+v.toFixed(digits));
}

export function vecInput(values, onchange, opts = {}) {
  const wrap = h("div", { class: `vec ${values.length === 4 ? "v4" : ""}` });
  const cur = [...values];
  values.forEach((v, i) => wrap.append(numInput(v, (nv) => { cur[i] = nv; onchange([...cur]); }, opts)));
  return wrap;
}

export function select(options, value, onchange) {
  const el = h("select", {}, ...options.map(([v, l]) => h("option", { value: v, selected: String(v) === String(value) }, l)));
  el.addEventListener("change", () => onchange(el.value));
  return el;
}

export function checkbox(value, onchange) {
  const el = h("input", { type: "checkbox", checked: !!value });
  el.addEventListener("change", () => onchange(el.checked));
  return el;
}

export function colorInput(hex, onchange) {
  const el = h("input", { type: "color", value: hex || "#cccccc" });
  el.addEventListener("change", () => onchange(el.value));
  return el;
}

export function slider(value, onchange, { min = 0, max = 1, step = 0.01 } = {}) {
  const wrap = h("div", { style: { display: "grid", gridTemplateColumns: "1fr 54px", gap: "6px", alignItems: "center" } });
  const r = h("input", { type: "range", min, max, step, value });
  const n = h("input", { type: "number", min, max, step, value: fmt(value, 3) });
  r.addEventListener("input", () => { n.value = r.value; });
  r.addEventListener("change", () => onchange(parseFloat(r.value)));
  n.addEventListener("change", () => { r.value = n.value; onchange(parseFloat(n.value)); });
  wrap.append(r, n);
  wrap.get = () => parseFloat(n.value);
  return wrap;
}

export function section(title, open = true, ...children) {
  const d = h("details", { class: "section", open });
  const sum = h("summary", {}, h("span", { class: "sum-title" }, title));
  d.append(sum, h("div", { class: "body" }, ...children));
  d.summaryEl = sum;
  return d;
}

export function btn(label, onclick, cls = "") {
  return h("button", { class: `btn ${cls}`, onclick }, label);
}

export function hexFromRgb(c) {
  if (!c) return "#cccccc";
  return "#" + c.slice(0, 3).map((x) => Math.round(Math.max(0, Math.min(1, x)) * 255).toString(16).padStart(2, "0")).join("");
}
export function rgbFromHex(hex) {
  const n = parseInt(hex.slice(1), 16);
  return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255];
}

// ------------------------------------------------- éditeur de propriétés RNA
// Construit automatiquement les champs d'une structure Blender (modificateurs, nœuds).
export function rnaEditor(props, onset, objectNames = []) {
  const out = [];
  for (const p of props) {
    let input;
    const set = (v) => onset(p.id, v);
    if (p.type === "BOOLEAN" && !p.array) input = checkbox(p.value, set);
    else if (p.type === "BOOLEAN" && p.array) {
      input = h("div", { class: "btns" }, ...p.value.map((v, i) => {
        const lab = ["X", "Y", "Z", "W"][i] || i;
        const cb = checkbox(v, (nv) => { const arr = [...p.value]; arr[i] = nv; p.value = arr; set(arr); });
        return h("label", { style: { display: "flex", gap: "3px", alignItems: "center" } }, cb, lab);
      }));
    } else if (p.type === "ENUM") input = select(p.items, p.value, set);
    else if ((p.type === "INT" || p.type === "FLOAT") && p.array) input = vecInput(p.value, set, { step: p.type === "INT" ? 1 : 0.05 });
    else if (p.type === "INT") input = numInput(p.value, (v) => set(Math.round(v)), { step: 1, min: p.min ?? undefined, max: p.max ?? undefined });
    else if (p.type === "FLOAT") {
      if (p.subtype === "FACTOR" || (p.min === 0 && p.max === 1)) input = slider(p.value, set, { min: p.min ?? 0, max: p.max ?? 1, step: 0.01 });
      else input = numInput(p.value, set, { step: p.subtype === "ANGLE" ? 0.05 : 0.01 });
    } else if (p.type === "POINTER") {
      input = select([["", "— aucun —"], ...objectNames.map((n) => [n, n])], p.value || "", set);
    } else if (p.type === "STRING") {
      input = h("input", { type: "text", value: p.value || "" });
      input.addEventListener("change", () => set(input.value));
    } else continue;
    out.push(row(p.label, input, p.description));
  }
  return out;
}

// ---------------------------------------------------------------- modales
export function modal(title, content, buttons = [], { wide = false } = {}) {
  return new Promise((resolve) => {
    const root = document.getElementById("modal-root");
    const close = (v) => { back.remove(); document.removeEventListener("keydown", onKey); resolve(v); };
    const onKey = (e) => { if (e.key === "Escape") close(null); };
    const foot = h("div", { class: "foot" }, ...buttons.map(([label, value, cls]) => h("button", { class: `btn ${cls || ""}`, onclick: () => close(typeof value === "function" ? value() : value) }, label)));
    const box = h("div", { class: `modal ${wide ? "wide" : ""}` }, h("h3", {}, title), h("div", { class: "content" }, content), foot);
    const back = h("div", { class: "modal-back", onmousedown: (e) => { if (e.target === back) close(null); } }, box);
    root.append(back);
    document.addEventListener("keydown", onKey);
    const first = box.querySelector("input, textarea, select");
    if (first) setTimeout(() => first.focus(), 30);
  });
}

export async function confirmBox(message, okLabel = "Confirmer") {
  const r = await modal("Confirmation", h("p", { style: { margin: 0, whiteSpace: "pre-wrap" } }, message),
    [["Annuler", false], [okLabel, true, "danger"]]);
  return r === true;
}

export async function prompt(title, label, value = "") {
  const input = h("input", { type: "text", value });
  input.addEventListener("keydown", (e) => { if (e.key === "Enter") input.closest(".modal").querySelector(".foot .primary").click(); });
  const r = await modal(title, row(label, input), [["Annuler", null], ["Valider", () => input.value, "primary"]]);
  return r;
}

export function toast(message, kind = "", ms = 4200) {
  const t = h("div", { class: `toast ${kind}` }, message);
  document.getElementById("toasts").append(t);
  setTimeout(() => t.remove(), ms);
}

// Exécute une commande avec gestion des confirmations et des erreurs.
export async function run(name, params = {}, { success, quiet } = {}) {
  try {
    const r = await api(name, params);
    if (success) toast(typeof success === "function" ? success(r) : success, "ok");
    return r;
  } catch (e) {
    if (e instanceof ApiError && e.confirmation) {
      if (await confirmBox(e.message.replace(/ Relancez avec confirm=true pour confirmer\.$/, ""))) {
        return run(name, { ...params, confirm: true }, { success, quiet });
      }
      return null;
    }
    if (!quiet) toast(e.message, "error", 7000);
    throw e;
  }
}

export async function pickFile(accept) {
  return new Promise((resolve) => {
    const input = document.getElementById("file-input");
    input.value = "";
    input.accept = accept || "";
    input.multiple = false;
    input.onchange = () => resolve(input.files[0] || null);
    input.click();
  });
}
