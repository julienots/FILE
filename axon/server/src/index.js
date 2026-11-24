#!/usr/bin/env node
/**
 * AXON Bridge — SuperEssence Studio
 *
 * Un seul processus qui fait deux choses :
 *  1. Serveur MCP (stdio) : Claude (Desktop ou Code) l'utilise pour piloter
 *     des appareils Android via ADB (capture, éléments d'écran, tap, saisie…).
 *  2. Serveur HTTP + WebSocket (LAN) : sert l'app AXON (PWA) et retransmet en
 *     direct l'écran des appareils pilotés vers n'importe quel téléphone ou
 *     tablette connecté, qui peut aussi envoyer des demandes à Claude.
 *
 * IMPORTANT : en mode stdio, stdout est réservé au protocole MCP.
 * Tous les logs passent par stderr (console.error).
 */
import { execFile } from "node:child_process";
import { createServer } from "node:http";
import { readFile, mkdir, writeFile } from "node:fs/promises";
import { existsSync, readFileSync } from "node:fs";
import { networkInterfaces, homedir } from "node:os";
import { join, extname, dirname, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { randomBytes } from "node:crypto";
import { WebSocketServer } from "ws";
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

const __dirname = dirname(fileURLToPath(import.meta.url));
const APP_DIR = normalize(join(__dirname, "..", "..", "app"));
const PORT = Number(process.env.AXON_PORT || 8765);
const ADB = process.env.ADB_PATH || "adb";
const log = (...a) => console.error("[axon]", ...a);

let sharp = null;
try { sharp = (await import("sharp")).default; } catch { log("sharp absent : images envoyées en PNG brut"); }

/* ------------------------------------------------------------------ */
/* Code d'appairage (stable entre deux lancements)                     */
/* ------------------------------------------------------------------ */
const CONF_DIR = join(homedir(), ".axon");
const TOKEN_FILE = join(CONF_DIR, "pairing-code");
async function loadToken() {
  if (process.env.AXON_CODE) return process.env.AXON_CODE.toUpperCase();
  if (existsSync(TOKEN_FILE)) return readFileSync(TOKEN_FILE, "utf8").trim();
  const alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const bytes = randomBytes(6);
  const code = [...bytes].map((b) => alphabet[b % alphabet.length]).join("");
  await mkdir(CONF_DIR, { recursive: true });
  await writeFile(TOKEN_FILE, code, { mode: 0o600 });
  return code;
}
const PAIR_CODE = await loadToken();

function lanAddresses() {
  const out = [];
  for (const list of Object.values(networkInterfaces())) {
    for (const i of list || []) if (i.family === "IPv4" && !i.internal) out.push(i.address);
  }
  return out;
}

/* ------------------------------------------------------------------ */
/* ADB                                                                 */
/* ------------------------------------------------------------------ */
function adb(args, { serial, binary = false, timeout = 20000 } = {}) {
  const full = serial ? ["-s", serial, ...args] : args;
  return new Promise((resolve, reject) => {
    execFile(ADB, full, { encoding: binary ? "buffer" : "utf8", maxBuffer: 64 * 1024 * 1024, timeout }, (err, stdout, stderr) => {
      if (err) {
        const msg = (stderr && stderr.toString()) || err.message;
        if (err.code === "ENOENT") return reject(new Error("ADB introuvable. Installez Android Platform Tools ou définissez ADB_PATH."));
        return reject(new Error(msg.trim()));
      }
      resolve(stdout);
    });
  });
}

async function listDevices() {
  const out = await adb(["devices", "-l"]);
  return out.split("\n").slice(1).map((l) => l.trim()).filter(Boolean).map((line) => {
    const [serial, state, ...rest] = line.split(/\s+/);
    const props = Object.fromEntries(rest.map((p) => p.split(":")).filter((p) => p.length === 2));
    return { serial, state, model: (props.model || "").replace(/_/g, " "), device: props.device || "" };
  });
}

/** Résout l'appareil : celui donné, sinon le seul connecté. */
async function resolveSerial(serial) {
  if (serial) return serial;
  const ready = (await listDevices()).filter((d) => d.state === "device");
  if (ready.length === 0) throw new Error("Aucun appareil Android connecté (vérifiez `adb devices` et le débogage USB/sans fil).");
  if (ready.length > 1) throw new Error(`Plusieurs appareils connectés, précisez "serial" : ${ready.map((d) => d.serial).join(", ")}`);
  return ready[0].serial;
}

const sizes = new Map(); // serial -> {w,h}
function pngSize(buf) {
  // IHDR : largeur à l'octet 16, hauteur à 20
  if (buf.length < 24 || buf.readUInt32BE(0) !== 0x89504e47) return null;
  return { w: buf.readUInt32BE(16), h: buf.readUInt32BE(20) };
}

async function screencap(serial) {
  let png = await adb(["exec-out", "screencap", "-p"], { serial, binary: true });
  // Certains vieux appareils renvoient \r\n à la place de \n
  if (png.readUInt32BE(0) !== 0x89504e47) png = Buffer.from(png.toString("binary").replace(/\r\n/g, "\n"), "binary");
  const s = pngSize(png);
  if (s) sizes.set(serial, s);
  return png;
}

async function toJpeg(png, width) {
  if (!sharp) return { data: png, mime: "image/png" };
  const data = await sharp(png).resize({ width, withoutEnlargement: true }).jpeg({ quality: 70 }).toBuffer();
  return { data, mime: "image/jpeg" };
}

async function screenSize(serial) {
  if (sizes.has(serial)) return sizes.get(serial);
  const out = await adb(["shell", "wm", "size"], { serial });
  const m = out.match(/Override size: (\d+)x(\d+)/) || out.match(/Physical size: (\d+)x(\d+)/);
  const s = m ? { w: +m[1], h: +m[2] } : { w: 1080, h: 1920 };
  sizes.set(serial, s);
  return s;
}

/* --- Éléments d'interface (uiautomator) --- */
const uiCache = new Map(); // serial -> elements[]
function decode(s = "") {
  return s.replace(/&quot;/g, '"').replace(/&apos;/g, "'").replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&#10;/g, " ").replace(/&amp;/g, "&");
}
async function uiElements(serial) {
  let xml = await adb(["exec-out", "uiautomator", "dump", "/dev/tty"], { serial, timeout: 30000 });
  if (!xml.includes("<hierarchy")) {
    await adb(["shell", "uiautomator", "dump", "/sdcard/axon_ui.xml"], { serial, timeout: 30000 });
    xml = await adb(["exec-out", "cat", "/sdcard/axon_ui.xml"], { serial });
  }
  const els = [];
  for (const m of xml.matchAll(/<node\b([^>]*?)\/?>/g)) {
    const attrs = Object.fromEntries([...m[1].matchAll(/([\w-]+)="([^"]*)"/g)].map((a) => [a[1], decode(a[2])]));
    const b = (attrs.bounds || "").match(/\[(\d+),(\d+)\]\[(\d+),(\d+)\]/);
    if (!b) continue;
    const [x1, y1, x2, y2] = b.slice(1).map(Number);
    if (x2 - x1 < 4 || y2 - y1 < 4) continue;
    const text = attrs.text || "";
    const desc = attrs["content-desc"] || "";
    const clickable = attrs.clickable === "true" || attrs["long-clickable"] === "true";
    const editable = /EditText/.test(attrs.class || "");
    const scrollable = attrs.scrollable === "true";
    if (!text && !desc && !clickable && !editable && !scrollable) continue;
    els.push({
      index: els.length,
      text, desc,
      id: (attrs["resource-id"] || "").replace(/^.*:id\//, ""),
      type: (attrs.class || "").split(".").pop(),
      clickable, editable, scrollable,
      checked: attrs.checked === "true",
      x: Math.round((x1 + x2) / 2), y: Math.round((y1 + y2) / 2),
      bounds: [x1, y1, x2, y2],
    });
  }
  uiCache.set(serial, els);
  return els;
}
function formatElements(els) {
  if (!els.length) return "(aucun élément détecté — l'app utilise peut-être un rendu custom : utilisez la capture d'écran et tap x/y)";
  return els.map((e) => {
    const flags = [e.clickable && "cliquable", e.editable && "champ", e.scrollable && "défilant", e.checked && "coché"].filter(Boolean).join(",");
    const label = [e.text && `"${e.text}"`, e.desc && `desc="${e.desc}"`, e.id && `#${e.id}`].filter(Boolean).join(" ");
    return `[${e.index}] ${e.type} ${label} ${flags ? `(${flags})` : ""} @${e.x},${e.y}`;
  }).join("\n");
}

const KEYS = {
  home: "KEYCODE_HOME", back: "KEYCODE_BACK", recents: "KEYCODE_APP_SWITCH", enter: "KEYCODE_ENTER",
  delete: "KEYCODE_DEL", tab: "KEYCODE_TAB", power: "KEYCODE_POWER", wakeup: "KEYCODE_WAKEUP",
  volume_up: "KEYCODE_VOLUME_UP", volume_down: "KEYCODE_VOLUME_DOWN", search: "KEYCODE_SEARCH",
  menu: "KEYCODE_MENU", play_pause: "KEYCODE_MEDIA_PLAY_PAUSE", notifications: "notifications",
};
async function pressKey(serial, key) {
  if (key === "notifications") return adb(["shell", "cmd", "statusbar", "expand-notifications"], { serial });
  const code = KEYS[key] || key;
  return adb(["shell", "input", "keyevent", code], { serial });
}
async function tap(serial, x, y) { return adb(["shell", "input", "tap", String(Math.round(x)), String(Math.round(y))], { serial }); }
async function swipe(serial, x1, y1, x2, y2, ms = 300) {
  return adb(["shell", "input", "swipe", ...[x1, y1, x2, y2].map((v) => String(Math.round(v))), String(ms)], { serial });
}
async function typeText(serial, text) {
  // `input text` ne gère pas les espaces ni certains caractères : on échappe.
  const chunks = text.match(/.{1,60}/gsu) || [];
  for (const c of chunks) {
    const escaped = c.replace(/([\\"'`$&|;<>()*~!#?\[\]{}])/g, "\\$1").replace(/ /g, "%s");
    await adb(["shell", "input", "text", escaped], { serial });
  }
}

/* ------------------------------------------------------------------ */
/* Hub temps réel : viewers (téléphones / tablettes AXON)              */
/* ------------------------------------------------------------------ */
const viewers = new Set();      // ws authentifiés
const streams = new Map();      // serial -> { running, subscribers:Set<ws> }
const requests = [];            // demandes envoyées depuis l'app
const requestWaiters = [];      // resolve() en attente (wait_for_request)
let reqSeq = 0;

function send(ws, msg) { if (ws.readyState === 1) ws.send(JSON.stringify(msg)); }
function broadcast(msg) { for (const ws of viewers) send(ws, msg); }
function activity(kind, text, extra = {}) {
  broadcast({ type: "activity", kind, text, at: Date.now(), ...extra });
}

async function pushDevices(target) {
  let devices = [];
  let error = null;
  try { devices = await listDevices(); } catch (e) { error = e.message; }
  const msg = { type: "devices", devices, error };
  target ? send(target, msg) : broadcast(msg);
}

function startStream(serial) {
  let st = streams.get(serial);
  if (!st) { st = { running: false, subscribers: new Set() }; streams.set(serial, st); }
  if (st.running) return st;
  st.running = true;
  (async () => {
    while (st.subscribers.size > 0) {
      const t0 = Date.now();
      try {
        const png = await screencap(serial);
        const { data, mime } = await toJpeg(png, 720);
        const s = sizes.get(serial);
        const frame = JSON.stringify({ type: "frame", serial, mime, w: s?.w, h: s?.h, data: data.toString("base64") });
        for (const ws of st.subscribers) if (ws.readyState === 1 && ws.bufferedAmount < 2e6) ws.send(frame);
      } catch (e) {
        for (const ws of st.subscribers) send(ws, { type: "stream-error", serial, error: e.message });
        await new Promise((r) => setTimeout(r, 2000));
      }
      const wait = Math.max(0, 250 - (Date.now() - t0));
      await new Promise((r) => setTimeout(r, wait));
    }
    st.running = false;
  })();
  return st;
}

async function handleViewerMessage(ws, msg) {
  const serial = msg.serial;
  const toPx = async (nx, ny) => { const s = await screenSize(serial); return [nx * s.w, ny * s.h]; };
  switch (msg.type) {
    case "devices": return pushDevices(ws);
    case "subscribe": {
      for (const st of streams.values()) st.subscribers.delete(ws);
      if (serial) startStream(serial).subscribers.add(ws);
      return;
    }
    case "tap": { const [x, y] = await toPx(msg.nx, msg.ny); return tap(serial, x, y); }
    case "swipe": {
      const [x1, y1] = await toPx(msg.nx1, msg.ny1); const [x2, y2] = await toPx(msg.nx2, msg.ny2);
      return swipe(serial, x1, y1, x2, y2, msg.ms || 300);
    }
    case "longpress": { const [x, y] = await toPx(msg.nx, msg.ny); return swipe(serial, x, y, x, y, 700); }
    case "key": return pressKey(serial, msg.key);
    case "text": return typeText(serial, String(msg.text || ""));
    case "request": {
      const text = String(msg.text || "").trim().slice(0, 4000);
      if (!text) return;
      const r = { id: ++reqSeq, text, serial: serial || null, at: Date.now(), status: "en attente" };
      requests.push(r);
      broadcast({ type: "request", request: r });
      while (requestWaiters.length) requestWaiters.shift()();
      return;
    }
  }
}

/* ------------------------------------------------------------------ */
/* Serveur HTTP (app) + WebSocket                                      */
/* ------------------------------------------------------------------ */
const MIME = {
  ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8",
  ".json": "application/json", ".webmanifest": "application/manifest+json", ".svg": "image/svg+xml",
  ".png": "image/png", ".ico": "image/x-icon",
};
const http = createServer(async (req, res) => {
  try {
    const url = new URL(req.url, "http://x");
    let p = decodeURIComponent(url.pathname);
    if (p === "/") p = "/index.html";
    const file = normalize(join(APP_DIR, p));
    if (!file.startsWith(APP_DIR)) { res.writeHead(403).end(); return; }
    const body = await readFile(file);
    res.writeHead(200, { "Content-Type": MIME[extname(file)] || "application/octet-stream", "Cache-Control": "no-cache" });
    res.end(body);
  } catch {
    res.writeHead(404, { "Content-Type": "text/plain" }).end("404");
  }
});

const wss = new WebSocketServer({ server: http, path: "/ws", maxPayload: 1e6 });
wss.on("connection", (ws, req) => {
  const code = new URL(req.url, "http://x").searchParams.get("code") || "";
  if (code.toUpperCase() !== PAIR_CODE) { send(ws, { type: "auth", ok: false }); ws.close(4001, "code"); return; }
  viewers.add(ws);
  send(ws, { type: "auth", ok: true, version: "1.0.0" });
  send(ws, { type: "requests", requests: requests.slice(-30) });
  pushDevices(ws);
  broadcast({ type: "viewers", count: viewers.size });
  ws.on("message", async (raw) => {
    let msg; try { msg = JSON.parse(raw); } catch { return; }
    try { await handleViewerMessage(ws, msg); }
    catch (e) { send(ws, { type: "error", error: e.message }); }
  });
  ws.on("close", () => {
    viewers.delete(ws);
    for (const st of streams.values()) st.subscribers.delete(ws);
    broadcast({ type: "viewers", count: viewers.size });
  });
});

http.on("error", (e) => log("HTTP indisponible :", e.message, "(un autre AXON tourne peut-être déjà sur ce port)"));
http.listen(PORT, "0.0.0.0", () => {
  log(`App AXON : ${lanAddresses().map((a) => `http://${a}:${PORT}`).join("  ") || `http://localhost:${PORT}`}`);
  log(`Code d'appairage : ${PAIR_CODE}`);
});
setInterval(() => { if (viewers.size) pushDevices(); }, 5000).unref();

/* ------------------------------------------------------------------ */
/* Serveur MCP                                                          */
/* ------------------------------------------------------------------ */
const mcp = new McpServer(
  { name: "axon", version: "1.0.0" },
  {
    instructions: [
      "AXON (SuperEssence Studio) : vous pilotez de vrais appareils Android de l'utilisateur via ADB, et il vous regarde faire en direct dans l'app AXON sur son téléphone ou sa tablette.",
      "Méthode fiable : 1) screenshot pour voir ; 2) get_ui_elements pour la liste numérotée ; 3) tap_element par index (sinon tap x/y en pixels de l'écran) ; 4) re-vérifiez avec screenshot après chaque action importante.",
      "Pour lancer une app : list_apps (filtre) puis launch_app. Pour écrire : touchez d'abord le champ puis type_text.",
      "L'utilisateur peut aussi vous envoyer des demandes depuis l'app : get_requests / wait_for_request, puis complete_request quand c'est fait. Utilisez notify pour afficher un message sur ses écrans.",
      "Règles : ne faites aucun paiement, achat, envoi d'argent, suppression de données ni modification de comptes/mots de passe sans confirmation explicite de l'utilisateur. Si un écran de connexion apparaît, demandez-lui de se connecter lui-même.",
    ].join("\n"),
  },
);

const serialArg = z.string().optional().describe("Numéro de série ADB (facultatif s'il n'y a qu'un appareil)");
const text = (t) => ({ content: [{ type: "text", text: t }] });

function tool(name, description, inputSchema, handler) {
  mcp.registerTool(name, { description, inputSchema }, async (args) => {
    activity("tool", `${name}${args && Object.keys(args).length ? " " + JSON.stringify(args).slice(0, 140) : ""}`, { tool: name });
    try { return await handler(args || {}); }
    catch (e) {
      activity("error", `${name} : ${e.message}`);
      return { isError: true, content: [{ type: "text", text: `Erreur : ${e.message}` }] };
    }
  });
}

tool("connection_info", "Adresse de l'app AXON et code d'appairage à donner à l'utilisateur pour connecter son téléphone/tablette.", {}, async () => {
  const urls = lanAddresses().map((a) => `http://${a}:${PORT}`);
  return text(`Ouvrir sur le téléphone/tablette (même Wi-Fi) : ${urls.join(" ou ") || `http://localhost:${PORT}`}\nCode d'appairage : ${PAIR_CODE}\nÉcrans AXON connectés : ${viewers.size}`);
});

tool("list_devices", "Liste les appareils Android pilotables (ADB) et le nombre d'écrans AXON qui regardent.", {}, async () => {
  const d = await listDevices();
  const lines = d.map((x) => `- ${x.serial} — ${x.model || x.device || "?"} (${x.state})`);
  return text(`${lines.join("\n") || "Aucun appareil ADB."}\nÉcrans AXON connectés : ${viewers.size}`);
});

tool("screenshot", "Capture l'écran de l'appareil. Les coordonnées pour tap sont en pixels réels de l'écran (indiquées dans la réponse).", { serial: serialArg }, async ({ serial }) => {
  const s = await resolveSerial(serial);
  const png = await screencap(s);
  const { data, mime } = await toJpeg(png, 1080);
  const size = sizes.get(s);
  return { content: [
    { type: "image", data: data.toString("base64"), mimeType: mime },
    { type: "text", text: `Écran ${size?.w}x${size?.h} px (appareil ${s}).` },
  ] };
});

tool("get_ui_elements", "Liste numérotée des éléments visibles (textes, boutons, champs) avec leurs positions. À utiliser avant tap_element.", { serial: serialArg }, async ({ serial }) => {
  const s = await resolveSerial(serial);
  return text(formatElements(await uiElements(s)));
});

tool("tap_element", "Touche un élément par son index (issu du dernier get_ui_elements) ou par texte visible.", {
  serial: serialArg,
  index: z.number().int().optional().describe("Index de get_ui_elements"),
  text: z.string().optional().describe("Texte ou description à chercher (insensible à la casse)"),
}, async ({ serial, index, text: t }) => {
  const s = await resolveSerial(serial);
  let el;
  if (index !== undefined) {
    el = (uiCache.get(s) || [])[index];
    if (!el) throw new Error("Index inconnu : relancez get_ui_elements.");
  } else if (t) {
    const els = await uiElements(s);
    const q = t.toLowerCase();
    el = els.find((e) => e.text.toLowerCase() === q || e.desc.toLowerCase() === q)
      || els.find((e) => e.text.toLowerCase().includes(q) || e.desc.toLowerCase().includes(q));
    if (!el) throw new Error(`Aucun élément contenant "${t}".`);
  } else throw new Error("Donnez index ou text.");
  await tap(s, el.x, el.y);
  activity("tap", `Touché ${el.text || el.desc || el.id || el.type}`, { serial: s, nx: el.x / ((sizes.get(s)?.w) || 1), ny: el.y / ((sizes.get(s)?.h) || 1) });
  return text(`Touché [${el.index}] ${el.text || el.desc || el.id} @${el.x},${el.y}`);
});

tool("tap", "Touche l'écran aux coordonnées (pixels réels de l'écran).", { serial: serialArg, x: z.number(), y: z.number(), long: z.boolean().optional().describe("Appui long") }, async ({ serial, x, y, long }) => {
  const s = await resolveSerial(serial);
  long ? await swipe(s, x, y, x, y, 800) : await tap(s, x, y);
  const size = await screenSize(s);
  activity("tap", `${long ? "Appui long" : "Tap"} ${Math.round(x)},${Math.round(y)}`, { serial: s, nx: x / size.w, ny: y / size.h });
  return text("OK");
});

tool("swipe", "Glisse sur l'écran. Utilisez direction pour un défilement simple, ou x1,y1,x2,y2 pour un geste précis.", {
  serial: serialArg,
  direction: z.enum(["up", "down", "left", "right"]).optional().describe("up = faire défiler le contenu vers le bas (le doigt monte)"),
  x1: z.number().optional(), y1: z.number().optional(), x2: z.number().optional(), y2: z.number().optional(),
  duration_ms: z.number().int().optional(),
}, async ({ serial, direction, x1, y1, x2, y2, duration_ms }) => {
  const s = await resolveSerial(serial);
  const { w, h } = await screenSize(s);
  if (direction) {
    const m = { up: [w / 2, h * 0.75, w / 2, h * 0.25], down: [w / 2, h * 0.25, w / 2, h * 0.75], left: [w * 0.85, h / 2, w * 0.15, h / 2], right: [w * 0.15, h / 2, w * 0.85, h / 2] }[direction];
    [x1, y1, x2, y2] = m;
  }
  if ([x1, y1, x2, y2].some((v) => v === undefined)) throw new Error("Donnez direction ou x1,y1,x2,y2.");
  await swipe(s, x1, y1, x2, y2, duration_ms || 350);
  return text("OK");
});

tool("type_text", "Tape du texte dans le champ actif (touchez le champ d'abord). submit=true appuie sur Entrée ensuite.", {
  serial: serialArg, text: z.string(), submit: z.boolean().optional(),
}, async ({ serial, text: t, submit }) => {
  const s = await resolveSerial(serial);
  await typeText(s, t);
  if (submit) await pressKey(s, "enter");
  return text("OK");
});

tool("press_key", "Appuie sur une touche système.", {
  serial: serialArg,
  key: z.enum(Object.keys(KEYS)).describe("home, back, recents, enter, delete, notifications…"),
}, async ({ serial, key }) => {
  const s = await resolveSerial(serial);
  await pressKey(s, key);
  return text("OK");
});

tool("list_apps", "Liste les applications installées (packages). Utilisez filter pour chercher (ex. 'youtube').", {
  serial: serialArg, filter: z.string().optional(), include_system: z.boolean().optional(),
}, async ({ serial, filter, include_system }) => {
  const s = await resolveSerial(serial);
  const out = await adb(["shell", "pm", "list", "packages", ...(include_system ? [] : ["-3"])], { serial: s });
  let pkgs = out.split("\n").map((l) => l.replace("package:", "").trim()).filter(Boolean).sort();
  if (filter) pkgs = pkgs.filter((p) => p.toLowerCase().includes(filter.toLowerCase()));
  return text(pkgs.join("\n") || "Aucun résultat (essayez include_system=true).");
});

tool("launch_app", "Ouvre une application par son nom de package (ex. com.google.android.youtube).", { serial: serialArg, package: z.string() }, async ({ serial, package: pkg }) => {
  const s = await resolveSerial(serial);
  const out = await adb(["shell", "monkey", "-p", pkg, "-c", "android.intent.category.LAUNCHER", "1"], { serial: s });
  if (/No activities found|monkey aborted/i.test(out)) throw new Error(`Impossible d'ouvrir ${pkg}.`);
  activity("app", `Ouverture de ${pkg}`, { serial: s });
  return text(`${pkg} ouvert. Faites un screenshot pour vérifier.`);
});

tool("open_url", "Ouvre un lien (site web, lien profond d'app, tel:, mailto:…) sur l'appareil.", { serial: serialArg, url: z.string() }, async ({ serial, url }) => {
  const s = await resolveSerial(serial);
  await adb(["shell", "am", "start", "-a", "android.intent.action.VIEW", "-d", url], { serial: s });
  activity("app", `Ouverture de ${url}`, { serial: s });
  return text("OK");
});

tool("current_app", "Indique l'application et l'écran (activité) au premier plan.", { serial: serialArg }, async ({ serial }) => {
  const s = await resolveSerial(serial);
  const out = await adb(["shell", "dumpsys", "activity", "activities"], { serial: s });
  const m = out.match(/(?:mResumedActivity|topResumedActivity|ResumedActivity)[^\n]*?\{[^}]*?\s([\w.]+\/[\w.$]+)/);
  return text(m ? m[1] : "Inconnu");
});

tool("notify", "Affiche un message sur les écrans AXON de l'utilisateur (progression, question, résultat).", {
  message: z.string(), level: z.enum(["info", "success", "warning", "question"]).optional(),
}, async ({ message, level }) => {
  broadcast({ type: "notify", message, level: level || "info", at: Date.now() });
  return text(viewers.size ? `Affiché sur ${viewers.size} écran(s).` : "Aucun écran AXON connecté.");
});

tool("get_requests", "Demandes envoyées par l'utilisateur depuis l'app AXON (les plus récentes).", { only_pending: z.boolean().optional() }, async ({ only_pending }) => {
  const list = requests.filter((r) => !only_pending || r.status === "en attente").slice(-20);
  return text(list.map((r) => `#${r.id} [${r.status}] ${r.serial ? `(appareil ${r.serial}) ` : ""}${r.text}`).join("\n") || "Aucune demande.");
});

tool("wait_for_request", "Attend qu'une nouvelle demande arrive depuis l'app AXON (max timeout_s secondes).", { timeout_s: z.number().int().min(1).max(600).optional() }, async ({ timeout_s }) => {
  const pending = () => requests.filter((r) => r.status === "en attente");
  if (!pending().length) {
    await new Promise((resolve) => {
      const t = setTimeout(resolve, (timeout_s || 120) * 1000);
      requestWaiters.push(() => { clearTimeout(t); resolve(); });
    });
  }
  const p = pending();
  if (!p.length) return text("Aucune demande pour l'instant.");
  for (const r of p) { r.status = "en cours"; broadcast({ type: "request", request: r }); }
  return text(p.map((r) => `#${r.id} ${r.serial ? `(appareil ${r.serial}) ` : ""}${r.text}`).join("\n"));
});

tool("complete_request", "Marque une demande comme terminée et envoie la réponse à l'app.", { id: z.number().int(), result: z.string() }, async ({ id, result }) => {
  const r = requests.find((x) => x.id === id);
  if (!r) throw new Error("Demande inconnue.");
  r.status = "terminée"; r.result = result;
  broadcast({ type: "request", request: r });
  return text("OK");
});

const transport = new StdioServerTransport();
await mcp.connect(transport);
log("Serveur MCP prêt (stdio).");

// Mode autonome (sans Claude) : `node src/index.js --standalone` garde le serveur en vie.
if (!process.argv.includes("--standalone")) {
  process.stdin.on("end", () => process.exit(0)); // Claude s'est fermé : on s'arrête aussi
}
