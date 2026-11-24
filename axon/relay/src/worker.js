/**
 * AXON Relay — SuperEssence Studio
 *
 * Relie trois choses par Internet, sans ordinateur :
 *   - Claude (app mobile, web ou Code)  ──POST /mcp/<clé>──▶  connecteur MCP
 *   - le téléphone piloté (app AXON)    ──WS /device?key=──▶  exécute les actions
 *   - les écrans de contrôle (tablette) ──WS /view?key=────▶  écran en direct + demandes
 * Chaque clé = un salon privé (Durable Object). La clé est générée dans l'app AXON.
 */

const VERSION = "1.0.0";
const serial = { type: "string", description: "Ignoré (un seul appareil par clé)" };
const TOOLS = [
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
  { name: "notify", description: "Affiche un message sur le téléphone et les écrans de contrôle (progression, question, résultat).", inputSchema: { type: "object", properties: { message: { type: "string" }, level: { type: "string", enum: ["info", "success", "warning", "question"] } }, required: ["message"] } },
  { name: "get_requests", description: "Demandes envoyées par l'utilisateur depuis l'app AXON.", inputSchema: { type: "object", properties: { only_pending: { type: "boolean" } } } },
  { name: "wait_for_request", description: "Attend une nouvelle demande depuis l'app AXON (max timeout_s, 50 par défaut).", inputSchema: { type: "object", properties: { timeout_s: { type: "integer", minimum: 1, maximum: 55 } } } },
  { name: "complete_request", description: "Marque une demande comme terminée et affiche la réponse dans l'app.", inputSchema: { type: "object", properties: { id: { type: "integer" }, result: { type: "string" } }, required: ["id", "result"] } },
];
const DEVICE_TOOLS = new Set(["screenshot", "get_ui_elements", "tap_element", "tap", "swipe", "type_text", "press_key", "list_apps", "launch_app", "open_url"]);

const INSTRUCTIONS = [
  "AXON (SuperEssence Studio) : vous pilotez le vrai téléphone Android de l'utilisateur ; il vous regarde faire en direct dans l'app AXON (sur ce téléphone ou une tablette).",
  "Méthode : 1) screenshot pour voir ; 2) get_ui_elements pour la liste numérotée ; 3) tap_element par index (sinon tap x/y) ; 4) re-vérifiez avec screenshot après chaque action importante.",
  "Ouvrir une app : launch_app avec son nom. Écrire : touchez le champ puis type_text.",
  "L'utilisateur peut écrire des demandes dans l'app : wait_for_request / get_requests, puis complete_request. notify affiche un message sur ses écrans.",
  "Règles : jamais de paiement, achat, envoi d'argent, suppression de données ni modification de compte/mot de passe sans confirmation explicite. Écran de connexion : demandez-lui de se connecter lui-même. Si le téléphone n'est pas connecté, dites-lui d'ouvrir AXON.",
].join("\n");

const cors = { "Access-Control-Allow-Origin": "*", "Access-Control-Allow-Headers": "*", "Access-Control-Allow-Methods": "GET,POST,DELETE,OPTIONS" };
const json = (o, status = 200, extra = {}) => new Response(JSON.stringify(o), { status, headers: { "Content-Type": "application/json", ...cors, ...extra } });
const validKey = (k) => typeof k === "string" && /^[A-Za-z0-9_-]{16,64}$/.test(k);

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    if (req.method === "OPTIONS") return new Response(null, { headers: cors });
    if (url.pathname === "/" ) return new Response(`AXON Relay ${VERSION} — SuperEssence Studio`, { headers: { "Content-Type": "text/plain; charset=utf-8" } });
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
      if (role === "device") for (const old of this.ctx.getWebSockets("device")) { try { old.close(4000, "remplacé"); } catch {} }
      this.ctx.acceptWebSocket(server, [role]);
      server.serializeAttachment({ role, info: { model: url.searchParams.get("model") || "" } });
      queueMicrotask(() => this.onOpen(server, role));
      return new Response(null, { status: 101, webSocket: client });
    }
    return this.mcp(req);
  }

  device() { return this.ctx.getWebSockets("device")[0] || null; }
  views() { return this.ctx.getWebSockets("view"); }
  send(ws, o) { try { ws.send(typeof o === "string" ? o : JSON.stringify(o)); } catch {} }
  toViews(o) { const s = JSON.stringify(o); for (const v of this.views()) this.send(v, s); }
  toAll(o) { this.toViews(o); const d = this.device(); if (d) this.send(d, o); }

  async onOpen(ws, role) {
    if (role === "view") {
      const d = this.device();
      this.send(ws, { type: "hello", device: d ? d.deserializeAttachment().info : null, requests: (await this.requests()).slice(-30) });
    }
    this.syncStream();
  }
  syncStream() {
    const d = this.device();
    const viewers = this.views().length;
    if (d) this.send(d, { type: "stream", on: viewers > 0, viewers });
    this.toViews({ type: "presence", device: d ? d.deserializeAttachment().info : null, viewers });
  }

  async webSocketMessage(ws, raw) {
    const { role } = ws.deserializeAttachment() || {};
    if (typeof raw !== "string") return;
    let m; try { m = JSON.parse(raw); } catch { return; }
    if (role === "device") {
      if (m.type === "result" && this.pending.has(m.id)) {
        const p = this.pending.get(m.id); this.pending.delete(m.id); clearTimeout(p.timer); p.resolve(m);
      } else if (m.type === "frame" || m.type === "activity" || m.type === "state") {
        if (m.type === "state") { const a = ws.deserializeAttachment(); a.info = { ...a.info, ...m.info }; ws.serializeAttachment(a); }
        this.toViews(m);
      } else if (m.type === "request") {
        await this.addRequest(m.text);
      }
      return;
    }
    // Écran de contrôle → actions manuelles sur le téléphone
    if (m.type === "request") return this.addRequest(m.text);
    if (["tap", "swipe", "key", "text", "longpress"].includes(m.type)) {
      const d = this.device();
      const { type, ...rest } = m;
      if (d) this.send(d, { type: "manual", action: type, ...rest });
    }
  }
  webSocketClose(ws) { try { ws.close(); } catch {} this.syncStream(); }
  webSocketError(ws) { this.syncStream(); }

  /* ---------------- Demandes utilisateur ---------------- */
  async requests() { return (await this.ctx.storage.get("requests")) || []; }
  async saveRequests(list) { await this.ctx.storage.put("requests", list.slice(-50)); }
  async addRequest(text) {
    text = String(text || "").trim().slice(0, 4000);
    if (!text) return;
    const list = await this.requests();
    const r = { id: (list.at(-1)?.id || 0) + 1, text, at: Date.now(), status: "en attente" };
    list.push(r); await this.saveRequests(list);
    this.toAll({ type: "request", request: r });
    while (this.waiters.length) this.waiters.shift()();
  }

  /* ---------------- Appels vers le téléphone ---------------- */
  callDevice(tool, args, timeoutMs = 30000) {
    const d = this.device();
    if (!d) return Promise.resolve({ isError: true, content: [{ type: "text", text: "Le téléphone n'est pas connecté : ouvrez l'app AXON sur le téléphone et vérifiez que le service d'accessibilité AXON est activé." }] });
    const id = ++this.seq + ":" + Date.now();
    return new Promise((resolve) => {
      const timer = setTimeout(() => { this.pending.delete(id); resolve({ isError: true, content: [{ type: "text", text: "Le téléphone n'a pas répondu à temps (écran éteint ou app en veille ?)." }] }); }, timeoutMs);
      this.pending.set(id, { resolve, timer });
      this.send(d, { type: "call", id, tool, args });
    });
  }

  async runTool(name, args = {}) {
    const text = (t) => ({ content: [{ type: "text", text: t }] });
    if (name !== "wait_for_request") this.toAll({ type: "activity", kind: "tool", tool: name, text: name + (Object.keys(args).length ? " " + JSON.stringify(args).slice(0, 140) : ""), at: Date.now() });
    if (DEVICE_TOOLS.has(name)) {
      const r = await this.callDevice(name, args);
      if (r.isError) this.toAll({ type: "activity", kind: "error", text: `${name} : ${r.content?.[0]?.text || "erreur"}`, at: Date.now() });
      return { content: r.content || [], isError: !!r.isError };
    }
    switch (name) {
      case "status": {
        const d = this.device();
        const info = d ? d.deserializeAttachment().info : null;
        return text(d ? `Téléphone connecté : ${info.model || "Android"}${info.app ? ` — app au premier plan : ${info.app}` : ""}${info.paused ? " — EN PAUSE (l'utilisateur a suspendu le contrôle)" : ""}\nÉcrans de contrôle connectés : ${this.views().length}` : "Téléphone NON connecté : demandez à l'utilisateur d'ouvrir l'app AXON.");
      }
      case "notify":
        this.toAll({ type: "notify", message: String(args.message || ""), level: args.level || "info", at: Date.now() });
        return text("Affiché.");
      case "get_requests": {
        const list = (await this.requests()).filter((r) => !args.only_pending || r.status === "en attente").slice(-20);
        return text(list.map((r) => `#${r.id} [${r.status}] ${r.text}`).join("\n") || "Aucune demande.");
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
        return text(p.map((r) => `#${r.id} ${r.text}`).join("\n"));
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
