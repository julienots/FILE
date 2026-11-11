// Communication avec le moteur SUPERESSENCE (HTTP + WebSocket).
const TOKEN = window.SE_TOKEN;
const listeners = new Map();

export function on(type, fn) {
  if (!listeners.has(type)) listeners.set(type, new Set());
  listeners.get(type).add(fn);
  return () => listeners.get(type).delete(fn);
}

export function emit(type, data) {
  for (const fn of listeners.get(type) || []) { try { fn(data); } catch (e) { console.error(e); } }
  for (const fn of listeners.get("*") || []) { try { fn({ type, ...data }); } catch (e) { console.error(e); } }
}

export class ApiError extends Error {
  constructor(message, data) { super(message); this.data = data || {}; }
  get confirmation() { return !!this.data.confirmation_required; }
}

let pending = 0;
function busy(delta, name) {
  pending += delta;
  emit("busy", { pending, name });
}

// Exécute une commande du moteur. Lève ApiError en cas d'échec.
export async function api(name, params = {}, { quiet = false } = {}) {
  if (!quiet) busy(1, name);
  try {
    const r = await fetch(`/api/cmd/${name}`, {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-SE-Token": TOKEN },
      body: JSON.stringify(params),
    });
    let data;
    try { data = await r.json(); } catch { throw new ApiError(`Réponse invalide du serveur (${r.status}).`); }
    if (!data.ok) throw new ApiError(data.error || "Erreur inconnue", data);
    return data.result;
  } catch (e) {
    if (e instanceof ApiError) throw e;
    throw new ApiError(`Connexion au moteur impossible : ${e.message}`);
  } finally {
    if (!quiet) busy(-1, name);
  }
}

export async function getJSON(path) {
  const r = await fetch(path, { headers: { "X-SE-Token": TOKEN } });
  const data = await r.json();
  if (!data.ok) throw new ApiError(data.error || "Erreur");
  return data;
}

export async function upload(file) {
  const fd = new FormData();
  fd.append("file", file);
  busy(1, "upload");
  try {
    const r = await fetch("/api/upload", { method: "POST", headers: { "X-SE-Token": TOKEN }, body: fd });
    const data = await r.json();
    if (!data.ok) throw new ApiError(data.error || "Envoi impossible");
    return data.path;
  } finally { busy(-1, "upload"); }
}

export function fileUrl(path, download = false) {
  return `/api/file?path=${encodeURIComponent(path)}&token=${encodeURIComponent(TOKEN)}${download ? "&download=1" : ""}&t=${Date.now()}`;
}

let ws = null;
export function connect() {
  const proto = location.protocol === "https:" ? "wss" : "ws";
  ws = new WebSocket(`${proto}://${location.host}/ws?token=${encodeURIComponent(TOKEN)}`);
  ws.onopen = () => emit("connection", { connected: true });
  ws.onclose = () => { emit("connection", { connected: false }); setTimeout(connect, 1500); };
  ws.onmessage = (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); } catch { return; }
    emit(msg.type, msg);
  };
  setInterval(() => { if (ws && ws.readyState === 1) ws.send("ping"); }, 20000);
}

export function b64ToArray(b64, Type) {
  const bin = atob(b64);
  const buf = new ArrayBuffer(bin.length);
  const u8 = new Uint8Array(buf);
  for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i);
  return new Type(buf);
}
