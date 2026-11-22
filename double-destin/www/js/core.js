"use strict";
// ============================================================
//  DOUBLE DESTIN — noyau : utilitaires, interface, sauvegarde
//  SuperEssence Studio
// ============================================================
const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];
const rnd = (a, b) => Math.floor(Math.random() * (b - a + 1)) + a;
const rf = (a, b) => Math.random() * (b - a) + a;
const chance = p => Math.random() < p;
const pick = a => a[Math.floor(Math.random() * a.length)];
const clamp = (v, a = 0, b = 100) => Math.max(a, Math.min(b, v));
const esc = s => String(s).replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const shuffle = a => { for (let i = a.length - 1; i > 0; i--) { const j = Math.floor(Math.random() * (i + 1)); [a[i], a[j]] = [a[j], a[i]]; } return a; };
function fmtMoney(v, cur = "€") {
  const n = Math.round(v), a = Math.abs(n);
  let s;
  if (a >= 1e9) s = (a / 1e9).toFixed(a >= 1e10 ? 0 : 1).replace(".", ",") + " Md";
  else if (a >= 1e6) s = (a / 1e6).toFixed(a >= 1e7 ? 0 : 1).replace(".", ",") + " M";
  else s = a.toLocaleString("fr-FR").replace(/ | /g, " ");
  return (n < 0 ? "-" : "") + s + " " + cur;
}
function fmtNum(v) {
  const a = Math.abs(v);
  if (a >= 1e9) return (v / 1e9).toFixed(1).replace(".", ",") + " Md";
  if (a >= 1e6) return (v / 1e6).toFixed(1).replace(".", ",") + " M";
  if (a >= 1e4) return Math.round(v / 1e3) + " k";
  return Math.round(v).toLocaleString("fr-FR");
}

// ---------- Sauvegarde ----------
const Store = {
  get(k, d = null) { try { const v = localStorage.getItem("dd_" + k); return v ? JSON.parse(v) : d; } catch (e) { return d; } },
  set(k, v) { try { localStorage.setItem("dd_" + k, JSON.stringify(v)); return true; } catch (e) { return false; } },
  del(k) { try { localStorage.removeItem("dd_" + k); } catch (e) {} },
};
const Settings = Object.assign({ sound: true, vibrate: true, fastSplash: false }, Store.get("settings", {}));
function saveSettings() { Store.set("settings", Settings); }

// ---------- Son (WebAudio, aucun fichier) ----------
const Sound = {
  ctx: null,
  init() { if (!this.ctx) try { this.ctx = new (window.AudioContext || window.webkitAudioContext)(); } catch (e) {} },
  tone(freq, dur = .12, type = "sine", vol = .12, delay = 0) {
    if (!Settings.sound) return; this.init(); const c = this.ctx; if (!c) return;
    const t = c.currentTime + delay, o = c.createOscillator(), g = c.createGain();
    o.type = type; o.frequency.setValueAtTime(freq, t);
    g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(vol, t + .01); g.gain.exponentialRampToValueAtTime(.0001, t + dur);
    o.connect(g); g.connect(c.destination); o.start(t); o.stop(t + dur + .02);
  },
  click() { this.tone(520, .06, "triangle", .06); },
  good() { this.tone(660, .1, "triangle", .1); this.tone(880, .14, "triangle", .1, .08); },
  bad() { this.tone(240, .18, "sawtooth", .07); this.tone(180, .22, "sawtooth", .06, .1); },
  age() { [523, 659, 784].forEach((f, i) => this.tone(f, .12, "triangle", .08, i * .06)); },
  boom() { this.tone(90, .5, "sawtooth", .15); this.tone(60, .6, "square", .08, .05); },
  coin() { this.tone(988, .08, "square", .05); this.tone(1319, .18, "square", .05, .07); },
  studio() { [392, 523, 659, 784, 1047].forEach((f, i) => this.tone(f, .5, "sine", .05, i * .09)); },
};
function vibrate(ms = 20) { if (!Settings.vibrate) return; try { if (window.Android) Android.vibrate(ms); else if (navigator.vibrate) navigator.vibrate(ms); } catch (e) {} }

// ---------- Toast ----------
let toastT = 0;
function toast(msg, ms = 2000) {
  const t = $("#toast"); t.textContent = msg; t.classList.add("on");
  clearTimeout(toastT); toastT = setTimeout(() => t.classList.remove("on"), ms);
}

// ---------- Modales ----------
const Modal = {
  stack: [],
  open({ icon = "", title = "", sub = "", html = "", centered = false, closable = true, onClose = null, cls = "" }) {
    const bg = document.createElement("div");
    bg.className = "modal-bg" + (centered ? " centered" : "");
    bg.innerHTML = `<div class="sheet ${cls}">
      ${title || icon ? `<div class="sheet-h">${icon ? `<div class="em">${icon}</div>` : ""}<div><b>${title}</b>${sub ? `<small>${sub}</small>` : ""}</div>${closable ? `<button class="x">✕</button>` : ""}</div>` : ""}
      <div class="sheet-b">${html}</div></div>`;
    $("#modalLayer").appendChild(bg);
    const m = { el: bg, body: $(".sheet-b", bg), closable, onClose, close: () => Modal.close(m) };
    if (closable) {
      bg.addEventListener("click", e => { if (e.target === bg) m.close(); });
      const x = $(".x", bg); if (x) x.onclick = () => { Sound.click(); m.close(); };
    }
    this.stack.push(m);
    return m;
  },
  close(m) {
    m = m || this.stack[this.stack.length - 1]; if (!m) return;
    const i = this.stack.indexOf(m); if (i >= 0) this.stack.splice(i, 1);
    m.el.remove(); if (m.onClose) m.onClose();
  },
  closeAll() { while (this.stack.length) this.close(); },
  top() { return this.stack[this.stack.length - 1]; },
  // message simple
  alert(title, text, icon = "", btn = "OK") {
    return new Promise(res => {
      const m = this.open({ icon, title, centered: true, closable: false, html: `<p>${text}</p><button class="btn gold block">${btn}</button>` });
      $(".btn", m.el).onclick = () => { Sound.click(); m.close(); res(); };
    });
  },
  // choix multiples ; options : [{label, sub, value, cls, disabled}]
  choice(title, text, options, icon = "", closable = false) {
    return new Promise(res => {
      const m = this.open({ icon, title, centered: true, closable, onClose: () => res(null),
        html: `${text ? `<p>${text}</p>` : ""}<div class="choices">${options.map((o, i) => `<button class="btn ${o.cls || ""}" data-i="${i}" ${o.disabled ? "disabled" : ""}><div>${o.label}${o.sub ? `<small>${o.sub}</small>` : ""}</div></button>`).join("")}</div>` });
      $$(".choices .btn", m.el).forEach(b => b.onclick = () => { Sound.click(); m.onClose = null; m.close(); res(options[+b.dataset.i].value !== undefined ? options[+b.dataset.i].value : +b.dataset.i); });
    });
  },
  confirm(title, text, yes = "Oui", no = "Non", icon = "") {
    return this.choice(title, text, [{ label: yes, value: true, cls: "gold" }, { label: no, value: false }], icon).then(v => !!v);
  },
  // saisie numérique avec curseur
  slider(title, text, min, max, def, fmt = v => v, okLabel = "Valider", icon = "") {
    return new Promise(res => {
      const m = this.open({ icon, title, centered: true, onClose: () => res(null),
        html: `<p>${text}</p><div class="center" style="font-family:Fredoka;font-size:26px;margin:8px 0" id="slv">${fmt(def)}</div>
        <input type="range" min="${min}" max="${max}" value="${def}" step="1" id="sl"><div class="row" style="margin-top:14px"><button class="btn" id="slc">Annuler</button><button class="btn gold" id="slo">${okLabel}</button></div>` });
      const s = $("#sl", m.el); s.oninput = () => $("#slv", m.el).textContent = fmt(+s.value);
      $("#slc", m.el).onclick = () => m.close();
      $("#slo", m.el).onclick = () => { m.onClose = null; m.close(); res(+s.value); };
    });
  },
  input(title, text, def = "", icon = "") {
    return new Promise(res => {
      const m = this.open({ icon, title, centered: true, onClose: () => res(null),
        html: `<p>${text}</p><div class="field"><input id="inp" value="${esc(def)}" maxlength="24"></div><button class="btn gold block" id="ok">Valider</button>` });
      $("#ok", m.el).onclick = () => { const v = $("#inp", m.el).value.trim(); m.onClose = null; m.close(); res(v || null); };
    });
  },
};

// ---------- Écrans ----------
const Screens = {
  cur: "splash",
  show(id) {
    $$(".screen").forEach(s => s.classList.toggle("active", s.id === id));
    this.cur = id;
  },
};

// ---------- Logo du jeu (SVG original) ----------
let logoN = 0;
function logoSVG(size = 180) {
  const u = "l" + (++logoN);
  return `<svg width="${size}" height="${size}" viewBox="0 -10 200 210" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="lgL${u}" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ff8aa3"/><stop offset="1" stop-color="#d6264f"/></linearGradient>
    <linearGradient id="lgR${u}" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#6fc0ff"/><stop offset="1" stop-color="#1b5fd1"/></linearGradient>
    <linearGradient id="lgG${u}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffe27a"/><stop offset="1" stop-color="#ffb000"/></linearGradient>
    <clipPath id="cpL${u}"><rect x="0" y="0" width="100" height="200"/></clipPath>
    <clipPath id="cpR${u}"><rect x="100" y="0" width="100" height="200"/></clipPath>
  </defs>
  <circle cx="100" cy="108" r="80" fill="#111" opacity=".5"/>
  <g clip-path="url(#cpL${u})"><circle cx="100" cy="104" r="80" fill="url(#lgL${u})"/>
    <circle cx="66" cy="78" r="15" fill="#fff"/><path d="M40 146 q0 -42 26 -42 q26 0 26 42 z" fill="#fff"/>
    <polyline points="28,128 44,128 52,112 60,144 68,120 76,128 100,128" fill="none" stroke="#ffe27a" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
  </g>
  <g clip-path="url(#cpR${u})"><circle cx="100" cy="104" r="80" fill="url(#lgR${u})"/>
    <g fill="none" stroke="#fff" stroke-opacity=".9" stroke-width="4">
      <circle cx="100" cy="104" r="54"/><ellipse cx="100" cy="104" rx="26" ry="54"/><line x1="100" y1="50" x2="100" y2="158"/>
      <line x1="48" y1="86" x2="152" y2="86"/><line x1="48" y1="122" x2="152" y2="122"/></g>
    <path d="M128 70 l10 -4 l8 6 l-2 10 l-10 4 l-6 -6 z M118 120 l14 -2 l6 10 l-8 10 l-10 -4 z" fill="#35d07f"/>
  </g>
  <rect x="97" y="22" width="6" height="164" rx="3" fill="#ffe27a"/>
  <circle cx="100" cy="104" r="80" fill="none" stroke="url(#lgG${u})" stroke-width="8"/>
  <path d="M62 30 l14 -22 l12 16 l12 -22 l12 22 l12 -16 l14 22 z" fill="url(#lgG${u})" stroke="#8a5a00" stroke-width="2" stroke-linejoin="round"/>
  <circle cx="76" cy="8" r="4" fill="#fff6cc"/><circle cx="100" cy="2" r="4" fill="#fff6cc"/><circle cx="124" cy="8" r="4" fill="#fff6cc"/>
</svg>`;
}
function logoHTML(size = 180, font = 44, tag = true) {
  return `<div class="logo">${logoSVG(size)}<div class="logo-name" style="font-size:${font}px"><span class="l1">DOUBLE</span><span class="l2">DESTIN</span></div>${tag ? `<div class="logo-tag">UNE VIE · UN EMPIRE</div>` : ""}</div>`;
}

// ---------- Bouton retour Android ----------
const BackHandlers = [];
window.onBackButton = function () {
  if (Modal.top()) { if (Modal.top().closable) Modal.close(); return true; }
  for (let i = BackHandlers.length - 1; i >= 0; i--) if (BackHandlers[i]()) return true;
  return false;
};

document.addEventListener("click", e => { if (e.target.closest("button")) vibrate(8); }, true);
