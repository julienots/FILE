/**
 * AXON Relay — SuperEssence Studio
 *
 * Relie trois choses par Internet, sans ordinateur :
 *   - Claude (app mobile, web ou Code)  ──POST /mcp/<clé>──▶  connecteur MCP
 *   - le téléphone piloté (app AXON)    ──WS /device?key=──▶  exécute les actions
 *   - les écrans de contrôle (tablette) ──WS /view?key=────▶  écran en direct + demandes
 * Chaque clé = un salon privé (Durable Object) qui peut réunir PLUSIEURS appareils pilotés en même temps.
 * La clé est générée dans l'app AXON ; les autres appareils rejoignent le salon avec le lien de partage.
 */

const VERSION = "1.1.0";
/* Plusieurs appareils peuvent partager une même clé (un salon). Chaque outil d'appareil prend un
 * paramètre `device` : nom, identifiant ou numéro (voir list_devices), ou "all" pour tous à la fois. */
const DEVICE_PARAM = { type: "string", description: "Appareil ciblé : nom, identifiant ou numéro donné par list_devices. \"all\" = tous les appareils en même temps. Facultatif s'il n'y a qu'un appareil connecté." };
const BASE_TOOLS = [
  { name: "list_devices", description: "Liste les appareils connectés à ce salon AXON (numéro, nom, identifiant, app au premier plan, pause).", inputSchema: { type: "object", properties: {} } },
  { name: "status", description: "État d'AXON : téléphone connecté, app au premier plan, écrans de contrôle connectés.", inputSchema: { type: "object", properties: {} } },
  { name: "screenshot", description: "Capture de l'écran du téléphone. Les coordonnées pour tap sont en pixels réels (indiqués dans la réponse).", inputSchema: { type: "object", properties: {} } },
  { name: "get_ui_elements", description: "Liste numérotée des éléments visibles (textes, boutons, champs) avec positions. À utiliser avant tap_element.", inputSchema: { type: "object", properties: {} } },
  { name: "tap_element", description: "Touche un élément par son index (dernier get_ui_elements) ou par texte visible.", inputSchema: { type: "object", properties: { index: { type: "integer" }, text: { type: "string", description: "Texte ou description (insensible à la casse)" } } } },
  { name: "tap", description: "Touche l'écran aux coordonnées en pixels réels.", inputSchema: { type: "object", properties: { x: { type: "number" }, y: { type: "number" }, long: { type: "boolean", description: "Appui long" } }, required: ["x", "y"] } },
  { name: "swipe", description: "Glisse : direction (up = le doigt monte, fait défiler vers le bas) ou x1,y1,x2,y2.", inputSchema: { type: "object", properties: { direction: { type: "string", enum: ["up", "down", "left", "right"] }, x1: { type: "number" }, y1: { type: "number" }, x2: { type: "number" }, y2: { type: "number" }, duration_ms: { type: "integer" } } } },
  { name: "type_text", description: "Écrit dans le champ actif (touchez le champ d'abord). submit=true valide (Entrée/Rechercher).", inputSchema: { type: "object", properties: { text: { type: "string" }, submit: { type: "boolean" }, replace: { type: "boolean", description: "Remplacer le contenu au lieu d'ajouter" } }, required: ["text"] } },
  { name: "press_key", description: "Touche système.", inputSchema: { type: "object", properties: { key: { type: "string", enum: ["back", "home", "recents", "notifications", "quick_settings", "enter", "lock_screen"] } }, required: ["key"] } },
  { name: "list_apps", description: "Applications installées (nom + package). filter pour chercher.", inputSchema: { type: "object", properties: { filter: { type: "string" } } } },
  { name: "launch_app", description: "Ouvre une application par son nom affiché (ex. YouTube) ou son package.", inputSchema: { type: "object", properties: { app: { type: "string" } }, required: ["app"] } },
  { name: "open_url", description: "Ouvre un lien (site, lien d'app, tel:, mailto:, geo:…).", inputSchema: { type: "object", properties: { url: { type: "string" } }, required: ["url"] } },
  { name: "notify", description: "Affiche un message sur les téléphones et les écrans de contrôle (progression, question, résultat). device facultatif : sans lui, tous les appareils.", inputSchema: { type: "object", properties: { message: { type: "string" }, level: { type: "string", enum: ["info", "success", "warning", "question"] } }, required: ["message"] } },
  { name: "get_requests", description: "Demandes envoyées par l'utilisateur depuis l'app AXON (avec l'appareil d'origine).", inputSchema: { type: "object", properties: { only_pending: { type: "boolean" } } } },
  { name: "wait_for_request", description: "Attend une nouvelle demande depuis l'app AXON (max timeout_s, 50 par défaut).", inputSchema: { type: "object", properties: { timeout_s: { type: "integer", minimum: 1, maximum: 55 } } } },
  { name: "complete_request", description: "Marque une demande comme terminée et affiche la réponse dans l'app.", inputSchema: { type: "object", properties: { id: { type: "integer" }, result: { type: "string" } }, required: ["id", "result"] } },
];
const DEVICE_TOOLS = new Set(["screenshot", "get_ui_elements", "tap_element", "tap", "swipe", "type_text", "press_key", "list_apps", "launch_app", "open_url"]);
const TOOLS = BASE_TOOLS.map((t) => (DEVICE_TOOLS.has(t.name) || t.name === "notify"
  ? { ...t, inputSchema: { ...t.inputSchema, properties: { device: DEVICE_PARAM, ...t.inputSchema.properties } } }
  : t));

const INSTRUCTIONS = [
  "AXON (SuperEssence Studio) : vous pilotez les vrais appareils Android de l'utilisateur ; il vous regarde faire en direct dans l'app AXON (sur un téléphone ou une tablette).",
  "Plusieurs appareils peuvent être connectés en même temps : appelez list_devices, puis passez device (nom ou numéro) à chaque outil. device=\"all\" exécute la même action sur tous les appareils à la fois. Une demande indique l'appareil d'où elle vient : agissez sur celui-là sauf indication contraire.",
  "Méthode : 1) screenshot pour voir ; 2) get_ui_elements pour la liste numérotée ; 3) tap_element par index (sinon tap x/y) ; 4) re-vérifiez avec screenshot après chaque action importante.",
  "Ouvrir une app : launch_app avec son nom. Écrire : touchez le champ puis type_text.",
  "L'utilisateur peut écrire des demandes dans l'app : wait_for_request / get_requests, puis complete_request. notify affiche un message sur ses écrans.",
  "Règles : jamais de paiement, achat, envoi d'argent, suppression de données ni modification de compte/mot de passe sans confirmation explicite. Écran de connexion : demandez-lui de se connecter lui-même. Si aucun appareil n'est connecté, dites-lui d'ouvrir AXON.",
].join("\n");

/* Écran de contrôle : fichiers de public/ intégrés au Worker (src/assets.gen.js). */
import ASSETS from "./assets.gen.js";
const decoded = new Map();
function staticFile(path, method) {
  if (path === "/") path = "/index.html";
  const a = ASSETS[path];
  if (!a) return null;
  if (!decoded.has(path)) decoded.set(path, Uint8Array.from(atob(a.b64), (c) => c.charCodeAt(0)));
  const body = decoded.get(path);
  const cache = /\.(png|svg)$/.test(path) ? "public, max-age=86400" : "no-cache";
  return new Response(method === "HEAD" ? null : body, { headers: { "Content-Type": a.type, "Cache-Control": cache, "Content-Length": String(body.length) } });
}

const cors = { "Access-Control-Allow-Origin": "*", "Access-Control-Allow-Headers": "*", "Access-Control-Allow-Methods": "GET,POST,DELETE,OPTIONS" };
const json = (o, status = 200, extra = {}) => new Response(JSON.stringify(o), { status, headers: { "Content-Type": "application/json", ...cors, ...extra } });
const validKey = (k) => typeof k === "string" && /^[A-Za-z0-9_-]{16,64}$/.test(k);

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    if (req.method === "OPTIONS") return new Response(null, { headers: cors });
    if (url.pathname === "/version") return new Response(`AXON Relay ${VERSION} — SuperEssence Studio`, { headers: { "Content-Type": "text/plain; charset=utf-8" } });
    if (req.method === "GET" || req.method === "HEAD") { const r = staticFile(url.pathname, req.method); if (r) return r; }
    let key = null;
    const m = url.pathname.match(/^\/mcp\/([^/]+)\/?$/);
    if (m) key = m[1];
    else if (url.pathname === "/device" || url.pathname === "/view") key = url.searchParams.get("key");
    else return new Response("Not found", { status: 404, headers: cors });
    if (!validKey(key)) return new Response("Clé invalide", { status: 403, headers: cors });
    const stub = env.HUB.get(env.HUB.idFromName(key));
    return stub.fetch(req);
  },
};

export class AxonHub {
  constructor(ctx, env) {
    this.ctx = ctx;
    this.pending = new Map(); // id -> {resolve, timer}
    this.waiters = [];
    this.seq = 0;
  }

  /* ---------------- HTTP / WebSocket entrants ---------------- */
  async fetch(req) {
    const url = new URL(req.url);
    if (url.pathname === "/device" || url.pathname === "/view") {
      if (req.headers.get("Upgrade") !== "websocket") return new Response("WebSocket attendu", { status: 426 });
      const role = url.pathname.slice(1);
      const [client, server] = Object.values(new WebSocketPair());
      const model = (url.searchParams.get("model") || "").slice(0, 60);
      // Les anciennes versions de l'app n'envoient pas d'identifiant : on en dérive un du modèle.
      const rawId = url.searchParams.get("id") || "m-" + (model || "android");
      const id = rawId.replace(/[^A-Za-z0-9_-]/g, "").slice(0, 40) || "android";
      const tags = [role];
      let since = Date.now();
      if (role === "device") {
        // Un même appareil qui se reconnecte remplace son ancienne connexion (et garde son numéro) ; les autres restent.
        for (const old of this.ctx.getWebSockets("dev:" + id)) {
          since = Math.min(since, old.deserializeAttachment()?.info?.since || since);
          try { old.close(4000, "remplacé"); } catch {}
        }
        tags.push("dev:" + id);
      }
      this.ctx.acceptWebSocket(server, tags);
      const name = (url.searchParams.get("name") || "").trim().slice(0, 40);
      server.serializeAttachment(role === "device"
        ? { role, id, info: { id, model, name: name || model || "Android", since } }
        : { role });
      queueMicrotask(() => this.onOpen(server, role));
      return new Response(null, { status: 101, webSocket: client });
    }
    return this.mcp(req);
  }

  /** Appareils connectés, dans l'ordre d'arrivée (le numéro sert de raccourci pour Claude). */
  devices() {
    const list = this.ctx.getWebSockets("device").filter((w) => w.readyState === 1 || w.readyState === undefined);
    return list
      .map((ws) => ({ ws, info: ws.deserializeAttachment()?.info || {} }))
      .sort((a, b) => (a.info.since || 0) - (b.info.since || 0));
  }
  deviceInfos() { return this.devices().map((d, i) => ({ n: i + 1, ...d.info })); }
  views() { return this.ctx.getWebSockets("view"); }
  send(ws, o) { try { ws.send(typeof o === "string" ? o : JSON.stringify(o)); } catch {} }
  toViews(o) { const s = JSON.stringify(o); for (const v of this.views()) this.send(v, s); }
  toDevices(o) { const s = JSON.stringify(o); for (const d of this.devices()) this.send(d.ws, s); }
  toAll(o) { this.toViews(o); this.toDevices(o); }

  /** Trouve le ou les appareils visés. Renvoie { list } ou { error }. */
  resolve(sel) {
    const all = this.devices();
    if (!all.length) return { error: "Aucun appareil connecté : demandez à l'utilisateur d'ouvrir AXON et d'activer le service d'accessibilité AXON." };
    const listing = () => all.map((d, i) => `${i + 1}. ${d.info.name} (${d.info.id})`).join("\n");
    if (sel === undefined || sel === null || String(sel).trim() === "") {
      if (all.length === 1) return { list: all };
      return { error: `Plusieurs appareils sont connectés : précisez device (nom, numéro, ou "all").\n${listing()}` };
    }
    const s = String(sel).trim().toLowerCase();
    if (s === "all" || s === "tous" || s === "*") return { list: all };
    const n = Number(s);
    if (Number.isInteger(n) && n >= 1 && n <= all.length) return { list: [all[n - 1]] };
    const exact = all.filter((d) => d.info.id.toLowerCase() === s || (d.info.name || "").toLowerCase() === s);
    if (exact.length) return { list: exact.slice(0, 1) };
    const part = all.filter((d) => (d.info.name || "").toLowerCase().includes(s) || (d.info.model || "").toLowerCase().includes(s));
    if (part.length === 1) return { list: part };
    return { error: `${part.length ? "Plusieurs appareils correspondent" : "Appareil introuvable"} : « ${sel} ».\n${listing()}` };
  }

  async onOpen(ws, role) {
    if (role === "view") {
      this.send(ws, { type: "hello", devices: this.deviceInfos(), requests: (await this.requests()).slice(-30) });
    }
    this.syncStream();
  }
  syncStream() {
    const viewers = this.views().length;
    const devices = this.deviceInfos();
    for (const d of this.devices()) this.send(d.ws, { type: "stream", on: viewers > 0, viewers, devices: devices.length });
    this.toViews({ type: "presence", devices, viewers });
  }

  async webSocketMessage(ws, raw) {
    const att = ws.deserializeAttachment() || {};
    if (typeof raw !== "string") return;
    let m; try { m = JSON.parse(raw); } catch { return; }
    if (att.role === "device") {
      const id = att.id;
      if (m.type === "result" && this.pending.has(m.id)) {
        const p = this.pending.get(m.id); this.pending.delete(m.id); clearTimeout(p.timer); p.resolve(m);
      } else if (m.type === "frame" || m.type === "activity" || m.type === "state") {
        if (m.type === "state") {
          const info = { ...att.info, ...m.info, id, since: att.info.since };
          if (!info.name) info.name = info.model || "Android";
          att.info = info; ws.serializeAttachment(att);
          this.toViews({ type: "presence", devices: this.deviceInfos(), viewers: this.views().length });
          return;
        }
        m.device = id; m.deviceName = att.info.name;
        this.toViews(m);
      } else if (m.type === "request") {
        await this.addRequest(m.text, att.info);
      }
      return;
    }
    // Écran de contrôle → actions manuelles sur l'appareil choisi
    if (m.type === "request") {
      const target = m.device ? this.resolve(m.device).list?.[0]?.info : null;
      return this.addRequest(m.text, target);
    }
    if (["tap", "swipe", "key", "text", "longpress"].includes(m.type)) {
      const { type, device, ...rest } = m;
      const r = this.resolve(device);
      for (const d of r.list || []) this.send(d.ws, { type: "manual", action: type, ...rest });
    }
  }
  webSocketClose(ws) {
    const att = ws.deserializeAttachment() || {};
    // Un appareil parti : ses appels en cours échouent tout de suite au lieu d'attendre 30 s.
    if (att.role === "device") for (const [id, p] of this.pending) if (p.device === att.id && p.ws === ws) {
      this.pending.delete(id); clearTimeout(p.timer);
      p.resolve({ isError: true, content: [{ type: "text", text: `${att.info?.name || "L'appareil"} s'est déconnecté.` }] });
    }
    try { ws.close(); } catch {}
    this.syncStream();
  }
  webSocketError(ws) { this.syncStream(); }

  /* ---------------- Demandes utilisateur ---------------- */
  async requests() { return (await this.ctx.storage.get("requests")) || []; }
  async saveRequests(list) { await this.ctx.storage.put("requests", list.slice(-50)); }
  async addRequest(text, from) {
    text = String(text || "").trim().slice(0, 4000);
    if (!text) return;
    const list = await this.requests();
    const r = { id: (list.at(-1)?.id || 0) + 1, text, at: Date.now(), status: "en attente" };
    if (from?.id) { r.device = from.id; r.deviceName = from.name; }
    list.push(r); await this.saveRequests(list);
    this.toAll({ type: "request", request: r });
    while (this.waiters.length) this.waiters.shift()();
  }
  reqLine(r) { return `#${r.id}${r.deviceName ? ` (depuis ${r.deviceName})` : ""} ${r.text}`; }

  /* ---------------- Appels vers les appareils ---------------- */
  callOne(d, tool, args, timeoutMs = 30000) {
    const id = ++this.seq + ":" + Date.now();
    return new Promise((resolve) => {
      const timer = setTimeout(() => { this.pending.delete(id); resolve({ isError: true, content: [{ type: "text", text: `${d.info.name} n'a pas répondu à temps (écran éteint ou app en veille ?).` }] }); }, timeoutMs);
      this.pending.set(id, { resolve, timer, device: d.info.id, ws: d.ws });
      this.send(d.ws, { type: "call", id, tool, args });
    });
  }

  async callDevices(tool, args) {
    const { device, ...rest } = args;
    const r = this.resolve(device);
    if (r.error) return { isError: true, content: [{ type: "text", text: r.error }] };
    if (r.list.length === 1) {
      const out = await this.callOne(r.list[0], tool, rest);
      if (out.isError) this.toAll({ type: "activity", kind: "error", device: r.list[0].info.id, deviceName: r.list[0].info.name, text: `${tool} : ${out.content?.[0]?.text || "erreur"}`, at: Date.now() });
      return out;
    }
    // Plusieurs appareils : on lance tout en parallèle et on regroupe les réponses.
    const results = await Promise.all(r.list.map((d) => this.callOne(d, tool, rest)));
    const content = [];
    results.forEach((res, i) => {
      content.push({ type: "text", text: `── ${r.list[i].info.name} (${r.list[i].info.id})${res.isError ? " — ERREUR" : ""} ──` });
      content.push(...(res.content || []));
    });
    return { content, isError: results.every((x) => x.isError) };
  }

  async runTool(name, args = {}) {
    const text = (t) => ({ content: [{ type: "text", text: t }] });
    let who = null;
    if (args.device && (DEVICE_TOOLS.has(name) || name === "notify")) { const r = this.resolve(args.device); who = r.list ? (r.list.length > 1 ? "Tous les appareils" : r.list[0].info.name) : null; }
    if (name !== "wait_for_request") this.toAll({ type: "activity", kind: "tool", tool: name, deviceName: who, text: name + (Object.keys(args).length ? " " + JSON.stringify(args).slice(0, 140) : ""), at: Date.now() });
    if (DEVICE_TOOLS.has(name)) {
      const r = await this.callDevices(name, args);
      return { content: r.content || [], isError: !!r.isError };
    }
    switch (name) {
      case "list_devices":
      case "status": {
        const list = this.deviceInfos();
        const lines = list.map((d) => `${d.n}. ${d.name} — id ${d.id}${d.model && d.model !== d.name ? ` — ${d.model}` : ""}${d.app ? ` — app : ${d.app}` : ""}${d.paused ? " — EN PAUSE (contrôle suspendu)" : ""}`);
        return text((list.length ? `${list.length} appareil${list.length > 1 ? "s" : ""} connecté${list.length > 1 ? "s" : ""} :\n${lines.join("\n")}` : "Aucun appareil connecté : demandez à l'utilisateur d'ouvrir l'app AXON.") + `\nÉcrans de contrôle connectés : ${this.views().length}`);
      }
      case "notify": {
        const msg = { type: "notify", message: String(args.message || ""), level: args.level || "info", at: Date.now() };
        this.toViews(msg);
        if (args.device) { const r = this.resolve(args.device); if (r.error) return { isError: true, content: [{ type: "text", text: r.error }] }; for (const d of r.list) this.send(d.ws, msg); }
        else this.toDevices(msg);
        return text("Affiché.");
      }
      case "get_requests": {
        const list = (await this.requests()).filter((r) => !args.only_pending || r.status === "en attente").slice(-20);
        return text(list.map((r) => `${this.reqLine(r).replace(/^#(\d+)/, "#$1 [" + r.status + "]")}`).join("\n") || "Aucune demande.");
      }
      case "wait_for_request": {
        this.toAll({ type: "activity", kind: "tool", tool: name, text: "En attente de vos demandes…", at: Date.now() });
        const pending = async () => (await this.requests()).filter((r) => r.status === "en attente");
        if (!(await pending()).length) {
          await new Promise((resolve) => {
            const t = setTimeout(resolve, Math.min(55, args.timeout_s || 50) * 1000);
            this.waiters.push(() => { clearTimeout(t); resolve(); });
          });
        }
        const list = await this.requests();
        const p = list.filter((r) => r.status === "en attente");
        if (!p.length) return text("Aucune demande pour l'instant (rappelez wait_for_request pour continuer d'attendre).");
        for (const r of p) { r.status = "en cours"; this.toAll({ type: "request", request: r }); }
        await this.saveRequests(list);
        return text(p.map((r) => this.reqLine(r)).join("\n"));
      }
      case "complete_request": {
        const list = await this.requests();
        const r = list.find((x) => x.id === args.id);
        if (!r) return { isError: true, content: [{ type: "text", text: "Demande inconnue." }] };
        r.status = "terminée"; r.result = String(args.result || "");
        await this.saveRequests(list);
        this.toAll({ type: "request", request: r });
        return text("OK");
      }
    }
    return { isError: true, content: [{ type: "text", text: `Outil inconnu : ${name}` }] };
  }

  /* ---------------- MCP (Streamable HTTP, réponses JSON) ---------------- */
  async mcp(req) {
    if (req.method === "GET") return new Response("Utilisez POST (MCP Streamable HTTP).", { status: 405, headers: { Allow: "POST", ...cors } });
    if (req.method === "DELETE") return new Response(null, { status: 204, headers: cors });
    if (req.method !== "POST") return new Response(null, { status: 405, headers: cors });
    let body; try { body = await req.json(); } catch { return json({ jsonrpc: "2.0", id: null, error: { code: -32700, message: "Parse error" } }, 400); }
    const batch = Array.isArray(body);
    const msgs = batch ? body : [body];
    const out = [];
    for (const msg of msgs) {
      if (msg.id === undefined || msg.id === null) continue; // notification
      out.push(await this.handleRpc(msg));
    }
    if (!out.length) return new Response(null, { status: 202, headers: cors });
    const sessionHeaders = { "Mcp-Session-Id": req.headers.get("Mcp-Session-Id") || crypto.randomUUID() };
    return json(batch ? out : out[0], 200, sessionHeaders);
  }

  async handleRpc({ id, method, params }) {
    const ok = (result) => ({ jsonrpc: "2.0", id, result });
    switch (method) {
      case "initialize":
        return ok({
          protocolVersion: params?.protocolVersion || "2025-06-18",
          capabilities: { tools: { listChanged: false } },
          serverInfo: { name: "axon", title: "AXON — SuperEssence Studio", version: VERSION },
          instructions: INSTRUCTIONS,
        });
      case "ping": return ok({});
      case "tools/list": return ok({ tools: TOOLS });
      case "tools/call": return ok(await this.runTool(params?.name, params?.arguments || {}));
      case "resources/list": return ok({ resources: [] });
      case "prompts/list": return ok({ prompts: [] });
      default: return { jsonrpc: "2.0", id, error: { code: -32601, message: `Méthode inconnue : ${method}` } };
    }
  }
}
