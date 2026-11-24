/* AXON — SuperEssence Studio
 * Client : écran retransmis, contrôle tactile, demandes à Claude, journal d'activité. */
(() => {
  const $ = (s) => document.querySelector(s);
  const store = {
    get(k, d) { try { const v = localStorage.getItem("axon." + k); return v === null ? d : JSON.parse(v); } catch { return d; } },
    set(k, v) { try { localStorage.setItem("axon." + k, JSON.stringify(v)); } catch {} },
    del(k) { try { localStorage.removeItem("axon." + k); } catch {} },
  };

  let ws = null, retry = 0, retryTimer = null;
  let devices = [], current = store.get("device", null);
  let lastFrameAt = 0, claudeTimer = null;

  /* ---------- Splash ---------- */
  const SPLASH_MS = 4900;
  const splashStart = Date.now();
  function endSplash() {
    const s = $("#splash");
    if (!s || s.classList.contains("out")) return;
    s.classList.add("out");
    setTimeout(() => s.remove(), 650);
    boot();
  }
  setTimeout(endSplash, SPLASH_MS);
  $("#splash").addEventListener("click", () => { if (Date.now() - splashStart > 1500) endSplash(); });

  if ("serviceWorker" in navigator && location.protocol.startsWith("http")) {
    navigator.serviceWorker.register("sw.js").catch(() => {});
  }

  /* ---------- Démarrage / appairage ---------- */
  function defaultHost() { return location.protocol.startsWith("http") && location.host ? location.host : ""; }

  function boot() {
    const pair = store.get("pair", null);
    if (pair && pair.code) { showMain(); connect(pair); }
    else showPair();
  }
  function showPair(err) {
    $("#main").hidden = true; $("#pair").hidden = false;
    const p = store.get("pair", {}) || {};
    $("#pairHost").value = p.host || defaultHost();
    $("#pairCode").value = p.code || new URLSearchParams(location.search).get("code") || "";
    $("#pairErr").hidden = !err; $("#pairErr").textContent = err || "";
  }
  function showMain() { $("#pair").hidden = true; $("#main").hidden = false; }

  $("#pairBtn").addEventListener("click", () => {
    const host = $("#pairHost").value.trim().replace(/^https?:\/\//, "").replace(/\/.*$/, "");
    const code = $("#pairCode").value.trim().toUpperCase();
    if (!host || !code) return showPair("Renseignez l'adresse et le code.");
    const pair = { host, code };
    store.set("pair", pair);
    showMain(); connect(pair);
  });

  /* ---------- WebSocket ---------- */
  function connect(pair) {
    clearTimeout(retryTimer);
    if (ws) { ws.onclose = null; ws.close(); }
    const proto = location.protocol === "https:" ? "wss" : "ws";
    setStatus(false, "Connexion…");
    try { ws = new WebSocket(`${proto}://${pair.host}/ws?code=${encodeURIComponent(pair.code)}`); }
    catch { return showPair("Adresse invalide."); }
    let authed = false;
    ws.onmessage = (ev) => {
      const m = JSON.parse(ev.data);
      if (m.type === "auth") {
        if (!m.ok) { store.set("pair", { host: pair.host }); ws.onclose = null; return showPair("Code d'appairage incorrect."); }
        authed = true; retry = 0; setStatus(true);
        if (current) sendMsg({ type: "subscribe", serial: current });
        return;
      }
      onMessage(m);
    };
    ws.onclose = () => {
      setStatus(false);
      if (!authed && retry >= 2) return showPair("Serveur AXON injoignable. Vérifiez qu'il tourne et que vous êtes sur le même Wi-Fi.");
      retryTimer = setTimeout(() => connect(pair), Math.min(8000, 800 * 2 ** retry++));
    };
  }
  function sendMsg(m) { if (ws && ws.readyState === 1) ws.send(JSON.stringify(m)); }
  function setStatus(on, label) {
    const s = $("#status");
    s.className = "pill " + (on ? "on" : "off");
    s.textContent = label || (on ? "Connecté" : "Hors ligne");
  }

  /* ---------- Messages serveur ---------- */
  function onMessage(m) {
    switch (m.type) {
      case "devices": return renderDevices(m.devices || [], m.error);
      case "frame": return renderFrame(m);
      case "stream-error": return showEmpty(`Retransmission interrompue : ${m.error}`);
      case "activity": return onActivity(m);
      case "notify": toast(m.message, m.level); return addFeed({ icon: m.level === "question" ? "❓" : "💬", text: m.message, at: m.at, cls: m.level === "success" ? "done" : "" });
      case "request": return upsertRequest(m.request);
      case "requests": return m.requests.forEach(upsertRequest);
      case "viewers": $("#viewers").textContent = `${m.count} écran${m.count > 1 ? "s" : ""} connecté${m.count > 1 ? "s" : ""}`; return;
      case "error": return toast(m.error, "warning");
    }
  }

  function renderDevices(list, error) {
    devices = list.filter((d) => d.state === "device");
    const sel = $("#deviceSel");
    const before = sel.value;
    sel.innerHTML = "";
    if (!devices.length) {
      sel.append(new Option(error ? "ADB indisponible" : "Aucun appareil", ""));
      showEmpty(error ? `ADB indisponible sur l'ordinateur.<br><small>${escapeHtml(error)}</small>` : null);
      current = null; return;
    }
    for (const d of devices) sel.append(new Option(d.model || d.serial, d.serial));
    if (!devices.some((d) => d.serial === current)) current = devices[0].serial;
    sel.value = current;
    if (before !== current || Date.now() - lastFrameAt > 4000) sendMsg({ type: "subscribe", serial: current });
  }
  $("#deviceSel").addEventListener("change", (e) => {
    current = e.target.value || null; store.set("device", current);
    $("#screen").removeAttribute("src"); lastFrameAt = 0;
    sendMsg({ type: "subscribe", serial: current });
  });

  function showEmpty(html) {
    $("#empty").hidden = false;
    $("#emptyText").innerHTML = html || "Aucun appareil Android connecté.<br><small>Branchez-le en USB avec le débogage activé, ou utilisez le débogage sans fil.</small>";
  }

  let pendingUrl = null;
  function renderFrame(m) {
    if (m.serial !== current) return;
    lastFrameAt = Date.now();
    $("#empty").hidden = true;
    if (m.w && m.h) $("#phone").style.setProperty("--ar", `${m.w} / ${m.h}`);
    const blob = b64ToBlob(m.data, m.mime);
    const url = URL.createObjectURL(blob);
    const img = $("#screen");
    const old = pendingUrl; pendingUrl = url;
    img.onload = () => { if (old) URL.revokeObjectURL(old); };
    img.src = url;
  }
  function b64ToBlob(b64, mime) {
    const bin = atob(b64); const arr = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) arr[i] = bin.charCodeAt(i);
    return new Blob([arr], { type: mime });
  }

  const ICONS = { tool: "⚡", tap: "👆", app: "📱", error: "⚠️" };
  const TOOL_LABEL = {
    screenshot: "Regarde l'écran", get_ui_elements: "Analyse l'interface", tap_element: "Touche un élément", tap: "Touche l'écran",
    swipe: "Fait défiler", type_text: "Écrit du texte", press_key: "Appuie sur une touche", list_apps: "Cherche une app",
    launch_app: "Ouvre une app", open_url: "Ouvre un lien", current_app: "Vérifie l'app ouverte", notify: null,
    list_devices: "Liste les appareils", connection_info: null, get_requests: "Lit vos demandes",
    wait_for_request: "Attend votre demande", complete_request: null,
  };
  function onActivity(m) {
    if (m.kind === "tool") {
      if (TOOL_LABEL[m.tool] === null) return;
      if (m.tool !== "wait_for_request") flashClaude();
      return addFeed({ icon: ICONS.tool, text: TOOL_LABEL[m.tool] || m.text, sub: m.text, at: m.at });
    }
    if (m.kind === "tap" && m.serial === current && m.nx != null) ripple(m.nx, m.ny, true);
    addFeed({ icon: ICONS[m.kind] || "•", text: m.text, at: m.at, cls: m.kind === "error" ? "error" : "" });
  }
  function flashClaude() {
    $("#claudeBadge").hidden = false;
    clearTimeout(claudeTimer);
    claudeTimer = setTimeout(() => ($("#claudeBadge").hidden = true), 2500);
  }

  /* ---------- Journal ---------- */
  function addFeed({ icon, text, sub, at, cls = "", id }) {
    const li = document.createElement("li");
    if (cls) li.className = cls;
    if (id) li.id = id;
    li.innerHTML = `<span>${icon}</span><span>${escapeHtml(text)}</span><time>${fmtTime(at)}</time>`;
    if (sub && sub !== text) li.title = sub;
    const feed = $("#feed");
    feed.prepend(li);
    while (feed.children.length > 80) feed.lastChild.remove();
    return li;
  }
  function upsertRequest(r) {
    const id = "req-" + r.id;
    const old = document.getElementById(id);
    const icon = r.status === "terminée" ? "✅" : r.status === "en cours" ? "🧠" : "📨";
    const li = old || addFeed({ icon, text: r.text, at: r.at, id });
    li.className = r.status === "terminée" ? "done" : "req";
    li.innerHTML = `<span>${icon}</span><span><b>Vous :</b> ${escapeHtml(r.text)}<br><small class="muted">${escapeHtml(r.status)}</small></span><time>${fmtTime(r.at)}</time>${r.result ? `<span class="res">${escapeHtml(r.result)}</span>` : ""}`;
    if (old && r.status === "terminée") toast("Claude : " + (r.result || "terminé"), "success");
  }
  const fmtTime = (t) => new Date(t || Date.now()).toLocaleTimeString("fr-FR", { hour: "2-digit", minute: "2-digit" });
  const escapeHtml = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));

  let toastTimer = null;
  function toast(msg, level) {
    const t = $("#toast");
    t.textContent = msg; t.hidden = false;
    t.style.borderColor = level === "warning" ? "var(--red)" : level === "success" ? "var(--green)" : "var(--yellow)";
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => (t.hidden = true), level === "question" ? 9000 : 4500);
    if (navigator.vibrate && level === "question") navigator.vibrate(80);
  }

  /* ---------- Demandes à Claude ---------- */
  $("#askForm").addEventListener("submit", (e) => {
    e.preventDefault();
    const text = $("#askInput").value.trim();
    if (!text) return;
    if (!ws || ws.readyState !== 1) return toast("Pas connecté au serveur AXON.", "warning");
    sendMsg({ type: "request", text, serial: current });
    $("#askInput").value = "";
    toast("Envoyé à Claude ✨");
  });
  document.querySelectorAll(".chip").forEach((c) => c.addEventListener("click", () => { $("#askInput").value = c.textContent; $("#askInput").focus(); }));

  /* ---------- Contrôle tactile manuel ---------- */
  const layer = $("#touchLayer");
  let down = null;
  function norm(e) {
    // Zone réellement occupée par l'image (object-fit: contain)
    const img = $("#screen"); const r = img.getBoundingClientRect();
    const iw = img.naturalWidth || r.width, ih = img.naturalHeight || r.height;
    const scale = Math.min(r.width / iw, r.height / ih);
    const w = iw * scale, h = ih * scale, ox = r.left + (r.width - w) / 2, oy = r.top + (r.height - h) / 2;
    return { nx: (e.clientX - ox) / w, ny: (e.clientY - oy) / h, px: e.clientX - r.left, py: e.clientY - r.top };
  }
  layer.addEventListener("pointerdown", (e) => {
    if (!$("#optTouch").checked || !current || !lastFrameAt) return;
    layer.setPointerCapture(e.pointerId);
    down = { ...norm(e), t: Date.now() };
  });
  layer.addEventListener("pointerup", (e) => {
    if (!down) return;
    const up = norm(e); const dt = Date.now() - down.t;
    const dist = Math.hypot((up.nx - down.nx), (up.ny - down.ny));
    const inside = (p) => p.nx >= 0 && p.nx <= 1 && p.ny >= 0 && p.ny <= 1;
    if (inside(down)) {
      if (dist < 0.02) {
        sendMsg({ type: dt > 550 ? "longpress" : "tap", serial: current, nx: down.nx, ny: down.ny });
        ripple(down.nx, down.ny);
      } else {
        const c = (v) => Math.min(1, Math.max(0, v));
        sendMsg({ type: "swipe", serial: current, nx1: down.nx, ny1: down.ny, nx2: c(up.nx), ny2: c(up.ny), ms: Math.min(1200, Math.max(120, dt)) });
      }
    }
    down = null;
  });
  layer.addEventListener("pointercancel", () => (down = null));

  function ripple(nx, ny, claude) {
    const img = $("#screen"); const r = img.getBoundingClientRect(); const pr = $("#phone").getBoundingClientRect();
    const iw = img.naturalWidth || r.width, ih = img.naturalHeight || r.height;
    const s = Math.min(r.width / iw, r.height / ih); const w = iw * s, h = ih * s;
    const d = document.createElement("div");
    d.className = "ripple" + (claude ? " claude" : "");
    d.style.left = (r.left - pr.left + (r.width - w) / 2 + nx * w) + "px";
    d.style.top = (r.top - pr.top + (r.height - h) / 2 + ny * h) + "px";
    $("#phone").append(d);
    setTimeout(() => d.remove(), 650);
  }

  document.querySelectorAll("[data-key]").forEach((b) => b.addEventListener("click", () => current && sendMsg({ type: "key", serial: current, key: b.dataset.key })));
  $("#kbBtn").addEventListener("click", () => { $("#kbForm").hidden = !$("#kbForm").hidden; if (!$("#kbForm").hidden) $("#kbInput").focus(); });
  $("#kbForm").addEventListener("submit", (e) => {
    e.preventDefault();
    const t = $("#kbInput").value; if (!t || !current) return;
    sendMsg({ type: "text", serial: current, text: t }); $("#kbInput").value = "";
  });

  /* ---------- Réglages ---------- */
  $("#menuBtn").addEventListener("click", () => {
    const p = store.get("pair", {});
    $("#menuInfo").textContent = `Serveur : ${p.host || "?"}`;
    $("#menu").showModal();
  });
  $("#menu").addEventListener("close", () => {
    if ($("#menu").returnValue === "unpair") {
      if (ws) { ws.onclose = null; ws.close(); }
      store.del("pair"); showPair();
    }
  });
  $("#optTouch").checked = store.get("touch", true);
  $("#optFit").checked = store.get("fit", false);
  $("#phone").classList.toggle("fit", $("#optFit").checked);
  $("#optTouch").addEventListener("change", (e) => store.set("touch", e.target.checked));
  $("#optFit").addEventListener("change", (e) => { store.set("fit", e.target.checked); $("#phone").classList.toggle("fit", e.target.checked); });
})();
