/* AXON — SuperEssence Studio
 * Écran de contrôle (tablette, téléphone, ordinateur) : écran en direct du téléphone piloté,
 * contrôle tactile, demandes à Claude, journal d'activité. Passe par le relais AXON. */
(() => {
  const $ = (s) => document.querySelector(s);
  const store = {
    get(k, d) { try { const v = localStorage.getItem("axon." + k); return v === null ? d : JSON.parse(v); } catch { return d; } },
    set(k, v) { try { localStorage.setItem("axon." + k, JSON.stringify(v)); } catch {} },
    del(k) { try { localStorage.removeItem("axon." + k); } catch {} },
  };
  const params = new URLSearchParams(location.search);
  let ws = null, retry = 0, retryTimer = null, claudeTimer = null;
  /* Plusieurs appareils par salon : devices = liste du relais, sel = id affiché ou "all" (mosaïque). */
  let devices = [], sel = store.get("sel", null);
  const frames = new Map(); // id -> { url, w, h, at }
  const devById = (id) => devices.find((d) => d.id === id) || null;
  const current = () => (sel && sel !== "all" ? devById(sel) : null);
  const lastFrameAt = () => (current() ? frames.get(current().id)?.at || 0 : 0);

  /* ---------- Clé depuis le lien (#k=…) ---------- */
  const hashKey = new URLSearchParams(location.hash.slice(1)).get("k");
  if (hashKey) { store.set("key", hashKey); history.replaceState(null, "", location.pathname + location.search); }

  /* ---------- Splash ---------- */
  const noSplash = params.has("nosplash");
  const SPLASH_MS = noSplash ? 0 : 4900;
  const splashStart = Date.now();
  function endSplash() {
    const s = $("#splash");
    if (!s || s.classList.contains("out")) return;
    s.classList.add("out");
    setTimeout(() => s.remove(), noSplash ? 0 : 650);
    boot();
  }
  if (noSplash) { $("#splash").remove(); setTimeout(boot); } else setTimeout(endSplash, SPLASH_MS);
  $("#splash")?.addEventListener("click", () => { if (Date.now() - splashStart > 1500) endSplash(); });
  if ("serviceWorker" in navigator && !window.AxonNative) navigator.serviceWorker.register("sw.js").catch(() => {});

  /* ---------- Jumelage ---------- */
  function boot() { const k = store.get("key", null); if (k) { showMain(); connect(k); } else showPair(); }
  function showPair(err) {
    $("#main").hidden = true; $("#pair").hidden = false;
    $("#pairErr").hidden = !err; $("#pairErr").textContent = err || "";
  }
  function showMain() { $("#pair").hidden = true; $("#main").hidden = false; }
  function parseKey(v) {
    v = v.trim();
    const m = v.match(/[#&]k=([A-Za-z0-9_-]{16,64})/) || v.match(/^([A-Za-z0-9_-]{16,64})$/);
    return m ? m[1] : null;
  }
  $("#pairBtn").addEventListener("click", () => {
    const k = parseKey($("#pairCode").value);
    if (!k) return showPair("Lien ou clé invalide.");
    store.set("key", k); showMain(); connect(k);
  });

  /* ---------- WebSocket ---------- */
  function connect(key) {
    clearTimeout(retryTimer);
    if (ws) { ws.onclose = null; ws.close(); }
    setStatus(false, "Connexion…");
    ws = new WebSocket(`${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/view?key=${encodeURIComponent(key)}`);
    ws.onopen = () => { retry = 0; setStatus(true); };
    ws.onmessage = (ev) => { try { onMessage(JSON.parse(ev.data)); } catch {} };
    ws.onclose = (e) => {
      setStatus(false);
      if (e.code === 1008 || e.code === 4003) return showPair("Clé refusée.");
      retryTimer = setTimeout(() => connect(key), Math.min(8000, 800 * 2 ** retry++));
    };
  }
  const sendMsg = (m) => { if (ws && ws.readyState === 1) ws.send(JSON.stringify(m)); };
  function setStatus(on, label) {
    const s = $("#status");
    s.className = "pill " + (on ? "on" : "off");
    s.textContent = label || (on ? "Connecté" : "Hors ligne");
  }

  /* ---------- Messages ---------- */
  function onMessage(m) {
    switch (m.type) {
      case "hello": setDevices(m.devices || []); (m.requests || []).forEach(upsertRequest); return;
      case "presence": setDevices(m.devices || []); $("#viewers").textContent = [devices.length > 1 ? `${devices.length} appareils` : "", m.viewers > 1 ? `${m.viewers} écrans` : ""].filter(Boolean).join(" · "); return;
      case "frame": return renderFrame(m);
      case "activity": return onActivity(m);
      case "notify": toast(m.message, m.level); return addFeed({ icon: m.level === "question" ? "❓" : "💬", text: m.message, at: m.at, cls: m.level === "success" ? "done" : "" });
      case "request": return upsertRequest(m.request);
    }
  }
  function setDevices(list) {
    devices = list;
    for (const id of [...frames.keys()]) if (!devById(id)) { URL.revokeObjectURL(frames.get(id).url); frames.delete(id); }
    if (sel !== "all" && !devById(sel)) sel = devices.length > 1 ? "all" : devices[0]?.id || null;
    if (sel === "all" && devices.length === 1) sel = devices[0].id;
    renderTabs(); renderView();
  }
  function select(id) { sel = id; store.set("sel", id); renderTabs(); renderView(); }
  function renderTabs() {
    const tabs = $("#devTabs");
    tabs.hidden = devices.length < 2;
    const items = devices.length > 1 ? [{ id: "all", name: `Tous (${devices.length})` }, ...devices] : [];
    tabs.innerHTML = items.map((d) => `<button class="dev-tab" data-id="${esc(d.id)}" aria-pressed="${d.id === sel}">${d.id === "all" ? "▦" : `<i class="${d.paused ? "paused" : ""}"></i>`}<span>${esc(d.name)}</span></button>`).join("");
    tabs.querySelectorAll(".dev-tab").forEach((b) => b.addEventListener("click", () => select(b.dataset.id)));
    // Cible des demandes écrites
    const row = $("#askTargetRow"), target = $("#askTarget"), prev = target.value;
    row.hidden = devices.length < 2;
    target.innerHTML = `<option value="">Celui affiché${current() ? ` (${esc(current().name)})` : " / au choix de Claude"}</option>` + devices.map((d) => `<option value="${esc(d.id)}">${esc(d.name)}</option>`).join("");
    if ([...target.options].some((o) => o.value === prev)) target.value = prev;
  }
  function renderView() {
    const d = current(), grid = $("#grid"), all = sel === "all" && devices.length > 1;
    grid.hidden = !all; $("#phone").hidden = all;
    $("#deviceName").textContent = all ? `${devices.length} appareils connectés` : d ? `${d.name}${d.app ? " · " + d.app : ""}` : "Aucun appareil connecté";
    if (all) {
      grid.innerHTML = devices.map((x) => `<button class="tile" data-id="${esc(x.id)}" style="${frames.get(x.id)?.w ? `--ar:${frames.get(x.id).w} / ${frames.get(x.id).h}` : ""}">
        <img alt="" ${frames.get(x.id) ? `src="${frames.get(x.id).url}"` : "hidden"}>
        ${frames.get(x.id) ? "" : `<span class="tile-wait">Connexion à l'écran…</span>`}
        <span class="tile-name">${x.paused ? "⏸ " : ""}${esc(x.name)}${x.app ? " · " + esc(x.app) : ""}</span></button>`).join("");
      grid.querySelectorAll(".tile").forEach((t) => t.addEventListener("click", () => select(t.dataset.id)));
      return;
    }
    const b = $("#claudeBadge");
    b.classList.toggle("paused-badge", !!d?.paused);
    if (d?.paused) { b.hidden = false; b.textContent = "Contrôle en pause"; }
    else if (b.textContent === "Contrôle en pause") b.hidden = true;
    const f = d && frames.get(d.id);
    if (!d) { $("#screen").removeAttribute("src"); showEmpty("Aucun appareil piloté n'est connecté.<br><small>Ouvrez AXON sur un téléphone et vérifiez que le service d'accessibilité est activé.</small>"); }
    else if (!f) { $("#screen").removeAttribute("src"); showEmpty("Connexion à l'écran du téléphone…<br><small>Android 11 minimum pour la retransmission.</small>"); }
    else { $("#empty").hidden = true; $("#phone").style.setProperty("--ar", `${f.w} / ${f.h}`); $("#screen").src = f.url; }
  }
  function showEmpty(html) { $("#empty").hidden = false; $("#emptyText").innerHTML = html; }

  function renderFrame(m) {
    const id = m.device; if (!id) return;
    const bin = atob(m.data); const arr = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) arr[i] = bin.charCodeAt(i);
    const url = URL.createObjectURL(new Blob([arr], { type: m.mime || "image/jpeg" }));
    const old = frames.get(id);
    frames.set(id, { url, w: m.w, h: m.h, at: Date.now() });
    const revoke = () => old && setTimeout(() => URL.revokeObjectURL(old.url), 1000);
    if (sel === "all" && devices.length > 1) {
      const t = [...$("#grid").children].find((x) => x.dataset.id === id);
      if (!t) return revoke();
      const img = t.querySelector("img"); img.hidden = false; img.src = url;
      t.querySelector(".tile-wait")?.remove();
      if (m.w && m.h) t.style.setProperty("--ar", `${m.w} / ${m.h}`);
    } else if (current()?.id === id) {
      $("#empty").hidden = true;
      if (m.w && m.h) $("#phone").style.setProperty("--ar", `${m.w} / ${m.h}`);
      $("#screen").src = url;
    }
    revoke();
  }
  /** Cible des actions manuelles : l'appareil affiché, ou tous en mode mosaïque. */
  const targetId = () => (sel === "all" ? "all" : current()?.id);

  const TOOL_LABEL = {
    do_actions: "Enchaîne plusieurs actions", screenshot: "Regarde l'écran", get_ui_elements: "Analyse l'interface", tap_element: null, tap: null,
    swipe: "Fait défiler", type_text: "Écrit du texte", press_key: "Appuie sur une touche", list_apps: "Cherche une app",
    launch_app: null, open_url: null, status: "Vérifie la connexion", notify: null, get_requests: "Lit vos demandes",
    wait_for_request: "Attend vos demandes", complete_request: null,
  };
  function onActivity(m) {
    const who = m.deviceName || (m.device && m.device !== "all" ? devById(m.device)?.name : m.device === "all" ? "tous" : "");
    if (m.kind === "tool") {
      if (TOOL_LABEL[m.tool] === null) { flashClaude(); return; }
      if (m.tool !== "wait_for_request") flashClaude();
      return addFeed({ icon: "⚡", text: TOOL_LABEL[m.tool] || m.text, sub: m.text, at: m.at, who: devices.length > 1 ? who : "" });
    }
    if (m.kind === "tap" && m.nx != null && m.device === current()?.id) ripple(m.nx, m.ny, true);
    addFeed({ icon: { tap: "👆", app: "📱", error: "⚠️" }[m.kind] || "•", text: m.text, at: m.at, cls: m.kind === "error" ? "error" : "", who: devices.length > 1 ? who : "" });
  }
  function flashClaude() {
    if (current()?.paused) return;
    const b = $("#claudeBadge"); b.textContent = "Claude agit…"; b.hidden = false;
    clearTimeout(claudeTimer); claudeTimer = setTimeout(() => (b.hidden = true), 2500);
  }

  /* ---------- Journal ---------- */
  function addFeed({ icon, text, sub, at, cls = "", id, who }) {
    const li = document.createElement("li");
    if (cls) li.className = cls;
    if (id) li.id = id;
    li.innerHTML = `<span>${icon}</span><span>${who ? `<span class="dev-label">${esc(who)}</span><br>` : ""}${esc(text)}</span><time>${fmt(at)}</time>`;
    if (sub && sub !== text) li.title = sub;
    const feed = $("#feed"); feed.prepend(li);
    while (feed.children.length > 80) feed.lastChild.remove();
    return li;
  }
  function upsertRequest(r) {
    const id = "req-" + r.id; const old = document.getElementById(id);
    const icon = r.status === "terminée" ? "✅" : r.status === "en cours" ? "🧠" : "📨";
    const li = old || addFeed({ icon, text: r.text, at: r.at, id });
    li.className = r.status === "terminée" ? "done" : "req";
    li.innerHTML = `<span>${icon}</span><span><b>Vous${r.deviceName ? ` → ${esc(r.deviceName)}` : ""} :</b> ${esc(r.text)}<br><small class="muted">${esc(r.status)}</small></span><time>${fmt(r.at)}</time>${r.result ? `<span class="res">${esc(r.result)}</span>` : ""}`;
    if (old && r.status === "terminée") toast("Claude : " + (r.result || "terminé"), "success");
  }
  const fmt = (t) => new Date(t || Date.now()).toLocaleTimeString("fr-FR", { hour: "2-digit", minute: "2-digit" });
  const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  let toastTimer = null;
  function toast(msg, level) {
    const t = $("#toast"); t.textContent = msg; t.hidden = false;
    t.style.borderColor = level === "warning" ? "var(--red)" : level === "success" ? "var(--green)" : "var(--yellow)";
    clearTimeout(toastTimer); toastTimer = setTimeout(() => (t.hidden = true), level === "question" ? 9000 : 4500);
    if (navigator.vibrate && level === "question") navigator.vibrate(80);
  }

  /* ---------- Demandes ---------- */
  $("#askForm").addEventListener("submit", (e) => {
    e.preventDefault();
    const text = $("#askInput").value.trim(); if (!text) return;
    if (!ws || ws.readyState !== 1) return toast("Pas connecté au relais AXON.", "warning");
    sendMsg({ type: "request", text, device: $("#askTarget").value || current()?.id || undefined }); $("#askInput").value = "";
    toast("Envoyé à Claude ✨ — dans Claude, dites « traite mes demandes AXON ».");
  });
  document.querySelectorAll(".chip").forEach((c) => c.addEventListener("click", () => { $("#askInput").value = c.textContent; $("#askInput").focus(); }));

  /* ---------- Contrôle tactile ---------- */
  const layer = $("#touchLayer"); let down = null;
  function geom() {
    const img = $("#screen"); const r = img.getBoundingClientRect();
    const iw = img.naturalWidth || r.width, ih = img.naturalHeight || r.height;
    const s = Math.min(r.width / iw, r.height / ih); const w = iw * s, h = ih * s;
    return { r, w, h, ox: r.left + (r.width - w) / 2, oy: r.top + (r.height - h) / 2 };
  }
  const norm = (e) => { const g = geom(); return { nx: (e.clientX - g.ox) / g.w, ny: (e.clientY - g.oy) / g.h }; };
  layer.addEventListener("pointerdown", (e) => {
    if (!$("#optTouch").checked || !lastFrameAt()) return;
    layer.setPointerCapture(e.pointerId); down = { ...norm(e), t: Date.now() };
  });
  layer.addEventListener("pointerup", (e) => {
    if (!down) return;
    const up = norm(e), dt = Date.now() - down.t, dist = Math.hypot(up.nx - down.nx, up.ny - down.ny);
    if (down.nx >= 0 && down.nx <= 1 && down.ny >= 0 && down.ny <= 1) {
      const device = targetId();
      if (dist < 0.02) { sendMsg({ type: dt > 550 ? "longpress" : "tap", device, nx: down.nx, ny: down.ny }); ripple(down.nx, down.ny); }
      else { const c = (v) => Math.min(1, Math.max(0, v)); sendMsg({ type: "swipe", device, nx1: down.nx, ny1: down.ny, nx2: c(up.nx), ny2: c(up.ny), ms: Math.min(1200, Math.max(120, dt)) }); }
    }
    down = null;
  });
  layer.addEventListener("pointercancel", () => (down = null));
  function ripple(nx, ny, claude) {
    const g = geom(); const pr = $("#phone").getBoundingClientRect();
    const d = document.createElement("div"); d.className = "ripple" + (claude ? " claude" : "");
    d.style.left = (g.ox - pr.left + nx * g.w) + "px"; d.style.top = (g.oy - pr.top + ny * g.h) + "px";
    $("#phone").append(d); setTimeout(() => d.remove(), 650);
  }
  document.querySelectorAll("[data-key]").forEach((b) => b.addEventListener("click", () => sendMsg({ type: "key", device: targetId(), key: b.dataset.key })));
  $("#kbBtn").addEventListener("click", () => { $("#kbForm").hidden = !$("#kbForm").hidden; if (!$("#kbForm").hidden) $("#kbInput").focus(); });
  $("#kbForm").addEventListener("submit", (e) => { e.preventDefault(); const t = $("#kbInput").value; if (!t) return; sendMsg({ type: "text", device: targetId(), text: t }); $("#kbInput").value = ""; });

  /* ---------- Réglages ---------- */
  $("#menuBtn").addEventListener("click", () => { $("#menuInfo").textContent = `Relais : ${location.host}`; $("#menu").showModal(); });
  $("#claudeBtn").addEventListener("click", async () => {
    const url = `${location.origin}/mcp/${store.get("key", "")}`;
    try { await navigator.clipboard.writeText(url); toast("Adresse du connecteur copiée. Claude › Paramètres › Connecteurs › Ajouter un connecteur personnalisé."); }
    catch { prompt("Adresse du connecteur Claude :", url); }
  });
  $("#menu").addEventListener("close", () => {
    if ($("#menu").returnValue === "unpair") { if (ws) { ws.onclose = null; ws.close(); } store.del("key"); showPair(); }
  });
  $("#optTouch").checked = store.get("touch", true);
  $("#optFit").checked = store.get("fit", false);
  $("#phone").classList.toggle("fit", $("#optFit").checked);
  $("#optTouch").addEventListener("change", (e) => store.set("touch", e.target.checked));
  $("#optFit").addEventListener("change", (e) => { store.set("fit", e.target.checked); $("#phone").classList.toggle("fit", e.target.checked); });
})();
