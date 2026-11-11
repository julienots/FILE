// Connexion à Claude par relais Supabase : Claude (connecteur MCP) → relais → ce téléphone.
// Le téléphone attend les commandes (long-polling), les exécute dans le moteur local et renvoie le résultat.

const K = { base: "se3d_relay_base", secret: "se3d_relay_secret", url: "se3d_relay_url", on: "se3d_relay_on" };
const get = (k) => { try { return localStorage.getItem(k) || ""; } catch { return ""; } };
const set = (k, v) => { try { v ? localStorage.setItem(k, v) : localStorage.removeItem(k); } catch { /* stockage indisponible */ } };

const state = { running: false, status: "Déconnecté", last: null, count: 0, ctrl: null };
const listeners = new Set();
const notify = () => listeners.forEach((f) => f());

function setStatus(s) { state.status = s; notify(); }

function normBase(u) {
  u = (u || "").trim().replace(/\/+$/, "");
  if (!u) return "";
  if (!/^https?:\/\//.test(u)) u = `https://${u}`;
  if (/\.supabase\.co$/.test(u)) u += "/functions/v1/superessence";
  return u;
}

async function register(base) {
  const r = await fetch(`${base}/device/register`, { method: "POST" });
  if (!r.ok) throw new Error(`Relais injoignable (${r.status}). Vérifiez l'adresse et que la fonction est déployée avec « Verify JWT » désactivé.`);
  const j = await r.json();
  if (!j.secret || !j.mcp_url) throw new Error("Réponse du relais invalide.");
  set(K.secret, j.secret); set(K.url, `${base}/mcp/${j.secret}`); set(K.base, base);
  return j;
}

async function run(cmd) {
  const fn = window.SEAPI?.[cmd.tool];
  if (!fn) return { ok: false, error: `Outil non pris en charge par l'application : ${cmd.tool}` };
  try {
    const result = await fn(cmd.args || {});
    return { ok: true, result: result ?? {} };
  } catch (e) {
    return { ok: false, error: String(e?.message || e) };
  }
}

async function loop() {
  const base = get(K.base), secret = get(K.secret);
  if (!base || !secret) { state.running = false; setStatus("Adresse du connecteur à créer"); return; }
  state.running = true;
  setStatus("Connecté — en attente de Claude");
  let failures = 0;
  while (state.running) {
    try {
      state.ctrl = new AbortController();
      const r = await fetch(`${base}/device/${secret}/wait`, { signal: state.ctrl.signal, cache: "no-store" });
      if (r.status === 404) { state.running = false; set(K.on, ""); setStatus("Appareil inconnu du relais : régénérez l'adresse"); break; }
      failures = 0;
      if (r.status === 204) { if (state.status.startsWith("Hors")) setStatus("Connecté — en attente de Claude"); continue; }
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      const cmd = await r.json();
      setStatus(`Claude : ${cmd.tool}…`);
      window.SEUI?.toast(`✦ Claude : ${cmd.tool.replace(/_/g, " ")}`);
      const res = await run(cmd);
      await fetch(`${base}/device/${secret}/result`, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ id: cmd.id, ...res }) });
      state.count++;
      state.last = `${cmd.tool} ${res.ok ? "✓" : "✗ " + res.error}`;
      setStatus("Connecté — en attente de Claude");
    } catch (e) {
      if (!state.running) break;
      failures++;
      setStatus(`Hors ligne (${e.message || e}) — nouvelle tentative…`);
      await new Promise((r) => setTimeout(r, Math.min(15000, 1000 * failures)));
    }
  }
  if (!state.running) setStatus("Déconnecté");
}

export function start() { if (!state.running) { set(K.on, "1"); loop(); } }
export function stop() { state.running = false; set(K.on, ""); state.ctrl?.abort(); setStatus("Déconnecté"); }
export function autoStart() { if (get(K.on) && get(K.secret)) loop(); }

async function copy(text) {
  try { await navigator.clipboard.writeText(text); return true; } catch { /* repli */ }
  const ta = document.createElement("textarea");
  ta.value = text; document.body.appendChild(ta); ta.select();
  const ok = document.execCommand("copy"); ta.remove();
  return ok;
}

export function openClaude() {
  const { showModal, closeModal, toast } = window.SEUI;
  const h = (tag, attrs = {}, ...kids) => {
    const e = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs)) { if (k === "class") e.className = v; else if (k.startsWith("on")) e.addEventListener(k.slice(2), v); else e.setAttribute(k, v); }
    for (const c of kids.flat()) if (c != null && c !== false) e.append(c instanceof Node ? c : String(c));
    return e;
  };
  const root = h("div", {});
  const draw = () => {
    root.replaceChildren();
    const on = state.running;
    const dot = h("span", { style: `display:inline-block;width:12px;height:12px;border-radius:50%;background:${on ? (state.status.startsWith("Hors") ? "#f5b942" : "#3ccf8e") : "#6b7184"};margin-right:8px` });
    root.append(h("div", { class: "it", style: "display:flex;align-items:center;justify-content:space-between;border:1px solid var(--line);border-radius:14px;padding:12px;margin-bottom:12px" },
      h("div", {}, h("b", {}, dot, "Connecté à Claude"), h("div", { class: "muted" }, state.status)),
      h("button", { class: `btn ${on ? "acc" : "pri"}`, onclick: () => { on ? stop() : start(); draw(); } }, on ? "Désactiver" : "Activer")));
    const url = get(K.base) && get(K.secret) ? `${get(K.base)}/mcp/${get(K.secret)}` : get(K.url);
    if (url) {
      root.append(h("b", {}, "Ajouter SUPERESSENCE dans Claude"),
        h("p", { class: "muted" }, "Copiez cette adresse et ajoutez-la comme connecteur personnalisé dans Claude (une seule fois) :"),
        h("div", { class: "spec", style: "word-break:break-all" }, url),
        h("div", { class: "row2", style: "margin-top:8px" },
          h("button", { class: "btn pri", onclick: async () => toast((await copy(url)) ? "Adresse copiée" : "Copie impossible : sélectionnez le texte", "ok") }, "Copier"),
          h("button", { class: "btn", onclick: async () => { try { await register(get(K.base)); toast("Nouvelle adresse créée : mettez à jour le connecteur dans Claude", "ok"); draw(); } catch (e) { toast(e.message, "err", 6000); } } }, "Régénérer")),
        h("ol", { class: "muted", style: "padding-left:18px" },
          h("li", {}, "claude.ai ou l'app Claude : Paramètres › Connecteurs › Ajouter un connecteur personnalisé."),
          h("li", {}, "Nom : SUPERESSENCE, URL : l'adresse ci-dessus."),
          h("li", {}, "Activez « Connecté à Claude », gardez cette application ouverte, puis demandez votre modèle dans une conversation Claude (activez le connecteur dans la conversation).")),
        h("p", { class: "muted" }, "Vos modèles restent sur votre téléphone : Claude reçoit seulement les descriptions de la scène et les images d'aperçu qu'il demande."),
        h("p", { style: "color:#ffb36b;font-size:13px" }, "⚠️ Cette adresse donne accès à votre studio : ne la partagez avec personne."));
      if (state.last) root.append(h("p", { class: "muted" }, `Dernière commande : ${state.last} · ${state.count} au total`));
    }
    const base = h("input", { type: "text", placeholder: "https://VOTRE-PROJET.supabase.co/functions/v1/superessence", value: get(K.base) });
    root.append(h("h4", {}, url ? "Relais" : "1. Adresse de votre relais Supabase"), base,
      h("p", { class: "muted" }, "C'est l'adresse de la fonction « superessence » déployée dans votre projet Supabase (voir le guide dans le dossier superessence3d/supabase)."),
      h("button", { class: "btn full", onclick: async () => {
        const b = normBase(base.value);
        if (!b) return toast("Collez l'adresse du relais.", "err");
        try { await register(b); toast("Adresse du connecteur créée", "ok"); draw(); } catch (e) { toast(e.message, "err", 7000); }
      } }, url ? "Changer de relais" : "Créer l'adresse du connecteur"));
  };
  const unsub = () => listeners.delete(draw);
  listeners.add(draw);
  draw();
  showModal("✦ Claude", root, [["Fermer", () => { unsub(); closeModal(); }]]);
}
