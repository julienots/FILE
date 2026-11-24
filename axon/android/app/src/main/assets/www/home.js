/* AXON — app native : écran de chargement, choix du rôle, tableau de bord de l'appareil piloté. */
(() => {
  const $ = (s) => document.querySelector(s);
  const N = window.AxonNative || {
    // Mode aperçu dans un navigateur (sans Android)
    _s: { role: "", relay: "https://axon-relay.exemple.workers.dev", key: "DemoKey1234567890abcdefXYZ", paused: false, serviceEnabled: false, connected: false, viewers: 0, model: "Aperçu", version: "dev", viewerLink: "" },
    state() { const s = this._s; return JSON.stringify({ ...s, connector: `${s.relay}/mcp/${s.key}`, viewerUrl: `${s.relay}/#k=${s.key}` }); },
    setRole(r) { this._s.role = r; }, setRelay(u) { this._s.relay = u; }, newKey() { return this._s.key; },
    setPaused(p) { this._s.paused = p; }, sendRequest() {}, setDeviceName(n) { this._s.deviceName = n; },
    joinRoom(l) { const m = l.match(/#k=([A-Za-z0-9_-]{16,64})/); if (!m) return false; this._s.key = m[1]; return true; }, openAccessibility() { this._s.serviceEnabled = true; this._s.connected = true; },
    copy() {}, share() {}, openClaude() {}, openUrl(u) { location.href = u; }, setViewerLink(l) { this._s.viewerLink = l; },
  };
  const S = () => JSON.parse(N.state());

  /* ---------- Splash ---------- */
  const start = Date.now();
  function endSplash() {
    const s = $("#splash"); if (!s || s.classList.contains("out")) return;
    s.classList.add("out"); setTimeout(() => s.remove(), 650); route();
  }
  setTimeout(endSplash, 4900);
  $("#splash").addEventListener("click", () => { if (Date.now() - start > 1500) endSplash(); });

  /* ---------- Navigation ---------- */
  function show(id) { for (const s of document.querySelectorAll(".screen")) s.hidden = s.id !== id; }
  function route() {
    const st = S();
    if (st.role === "device") { show("device"); refresh(); }
    else if (st.role === "viewer") { show("viewer"); $("#viewerLink").value = st.viewerLink || ""; if (st.viewerLink) openViewer(st.viewerLink); }
    else show("roles");
  }
  document.querySelectorAll(".role-card").forEach((b) => b.addEventListener("click", () => { N.setRole(b.dataset.role); route(); }));
  document.querySelectorAll("[data-goto-roles]").forEach((b) => b.addEventListener("click", () => { $("#settings").open && $("#settings").close(); N.setRole(""); route(); }));

  /* ---------- Écran de contrôle ---------- */
  function openViewer(link) {
    const m = link.match(/(https?:\/\/[^/#?]+)[^#]*#k=([A-Za-z0-9_-]{16,64})/);
    if (!m) { $("#viewerErr").hidden = false; $("#viewerErr").textContent = "Lien invalide (il doit contenir #k=…)."; return; }
    N.setViewerLink(link);
    N.openUrl(`${m[1]}/?nosplash=1#k=${m[2]}`);
  }
  $("#btnView").addEventListener("click", () => openViewer($("#viewerLink").value.trim()));

  /* ---------- Appareil piloté ---------- */
  function refresh() {
    const st = S();
    $("#model").textContent = st.deviceName || st.model || "";
    $("#roomInfo").textContent = st.connected && st.devices ? (st.devices > 1 ? `${st.devices} appareils pilotés sont connectés à ce salon.` : "Ce téléphone est le seul appareil piloté du salon pour l'instant.") : "";
    $("#credit").textContent = `AXON CONNECT ${st.version || ""} · SuperEssence Studio`;
    $("#connector").textContent = st.connector || "(réglez d'abord le relais)";
    $("#relayInfo").textContent = st.relay ? (st.connected ? `Connecté — ${host(st.relay)}` : `Hors ligne — ${host(st.relay)}`) : "Aucun relais réglé.";
    $("#stepService").classList.toggle("done", st.serviceEnabled);
    $("#btnService").textContent = st.serviceEnabled ? "Ouvrir" : "Activer";
    $("#stepRelay").classList.toggle("done", st.connected);
    $("#pause").checked = st.paused;
    const s = $("#status");
    s.className = "pill " + (st.connected ? "on" : "off");
    s.textContent = st.paused ? "En pause" : st.connected ? "Prêt" : "Hors ligne";
    $("#viewers").textContent = [st.devices > 1 ? `${st.devices} appareils` : "", st.viewers ? `${st.viewers} écran${st.viewers > 1 ? "s" : ""} de contrôle` : ""].filter(Boolean).join(" · ");
    const hero = $("#hero");
    hero.classList.toggle("ok", st.connected && !st.paused);
    if (!st.serviceEnabled) setHero("Activez AXON", "Étape 1 : autorisez AXON dans l'accessibilité pour que Claude puisse agir.");
    else if (st.sdk && st.sdk < 30) setHero("Android 11 conseillé", "Sur cette version, Claude ne peut pas voir l'écran en image, seulement lire les éléments.");
    else if (!st.relay) setHero("Réglez le relais", "Étape 2 : indiquez l'adresse de votre relais AXON.");
    else if (!st.connected) setHero("Connexion au relais…", "Vérifiez votre connexion Internet.");
    else if (st.paused) setHero("Claude est en pause", "Décochez la pause pour lui rendre la main.");
    else setHero("Claude peut piloter ce téléphone", "Demandez-lui une tâche dans l'app Claude, ou écrivez-la ci-dessous.");
  }
  const setHero = (t, s) => { $("#heroTitle").textContent = t; $("#heroSub").textContent = s; };
  const host = (u) => { try { return new URL(u).host; } catch { return u; } };
  setInterval(() => { if (!$("#device").hidden) refresh(); }, 2000);

  $("#btnService").addEventListener("click", () => N.openAccessibility());
  $("#btnRelay").addEventListener("click", openSettings);
  $("#btnSettings").addEventListener("click", openSettings);
  $("#btnCopy").addEventListener("click", () => { const c = S().connector; if (c) { N.copy(c); toast("Adresse copiée. Collez-la dans Claude › Connecteurs."); } });
  $("#btnClaude").addEventListener("click", () => N.openClaude());
  $("#btnShare").addEventListener("click", () => { const v = S().viewerUrl; if (v) N.share(`Rejoindre AXON (autre téléphone à piloter ou écran de contrôle) : ${v}`); });
  $("#pause").addEventListener("change", (e) => { N.setPaused(e.target.checked); refresh(); });

  function openSettings() {
    const st = S();
    $("#relayInput").value = st.relay || "";
    $("#nameInput").value = st.deviceName || "";
    $("#nameInput").placeholder = st.model || "Ex. : Téléphone de Léa";
    // Lien reçu (partagé vers AXON) d'un autre salon : proposé pour rejoindre.
    $("#joinInput").value = st.viewerLink && !st.viewerLink.includes(st.key) ? st.viewerLink : "";
    $("#settings").showModal();
  }
  $("#btnSaveName").addEventListener("click", () => { N.setDeviceName($("#nameInput").value.trim()); refresh(); toast("Nom enregistré."); });
  $("#btnJoin").addEventListener("click", () => {
    const v = $("#joinInput").value.trim(); if (!v) return toast("Collez d'abord le lien partagé par l'autre téléphone.");
    if (!confirm("Rejoindre ce salon ? Ce téléphone sera piloté avec le même connecteur Claude que l'autre appareil.")) return;
    if (!N.joinRoom(v)) return toast("Lien invalide (il doit contenir #k=…).");
    $("#settings").close(); refresh(); toast("Salon rejoint ✨ Claude voit maintenant les deux appareils.");
  });
  $("#btnSaveRelay").addEventListener("click", () => {
    const v = $("#relayInput").value.trim().replace(/\/+$/, "");
    if (v && !/^https:\/\/[^/]+$/.test(v)) return toast("L'adresse doit commencer par https:// (sans chemin).");
    N.setRelay(v); $("#settings").close(); refresh(); toast("Relais enregistré.");
  });
  $("#btnNewKey").addEventListener("click", () => {
    if (!confirm("Créer une nouvelle clé ? Claude et les écrans de contrôle devront être reliés à nouveau.")) return;
    N.newKey(); $("#settings").close(); refresh(); toast("Nouvelle clé créée : recopiez l'adresse dans Claude.");
  });

  $("#askForm").addEventListener("submit", (e) => {
    e.preventDefault();
    const t = $("#askInput").value.trim(); if (!t) return;
    if (!S().connected) return toast("Pas connecté au relais.");
    N.sendRequest(t); $("#askInput").value = ""; toast("Envoyé ✨ Dans Claude : « traite mes demandes AXON ».");
  });

  /* ---------- Événements du service ---------- */
  window.axonEvent = (m) => {
    switch (m.type) {
      case "connection": case "service": case "resume": refresh(); return;
      case "activity": if (m.kind !== "tool" || m.tool !== "wait_for_request") addFeed(m.kind === "error" ? "⚠️" : m.kind === "tap" ? "👆" : m.kind === "app" ? "📱" : "⚡", m.text, m.at, m.kind === "error" ? "error" : ""); return;
      case "notify": addFeed("💬", m.message, m.at, m.level === "success" ? "done" : ""); return;
      case "request": {
        const r = m.request; const id = "req-" + r.id; let li = document.getElementById(id);
        if (!li) { li = addFeed("📨", "", r.at); li.id = id; }
        li.className = r.status === "terminée" ? "done" : "req";
        li.innerHTML = `<span>${r.status === "terminée" ? "✅" : r.status === "en cours" ? "🧠" : "📨"}</span><span><b>Vous :</b> ${esc(r.text)}<br><small class="muted">${esc(r.status)}</small></span><time>${fmt(r.at)}</time>${r.result ? `<span class="res">${esc(r.result)}</span>` : ""}`;
      }
    }
  };
  function addFeed(icon, text, at, cls = "") {
    const li = document.createElement("li"); if (cls) li.className = cls;
    li.innerHTML = `<span>${icon}</span><span>${esc(text)}</span><time>${fmt(at)}</time>`;
    const f = $("#feed"); f.prepend(li); while (f.children.length > 60) f.lastChild.remove();
    return li;
  }
  const fmt = (t) => new Date(t || Date.now()).toLocaleTimeString("fr-FR", { hour: "2-digit", minute: "2-digit" });
  const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  let tt = null;
  function toast(msg) { const t = $("#toast"); t.textContent = msg; t.hidden = false; clearTimeout(tt); tt = setTimeout(() => (t.hidden = true), 4000); }
})();
