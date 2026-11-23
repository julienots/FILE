// Aides DOM minimalistes.
export const $ = (s, r = document) => r.querySelector(s);
export const $$ = (s, r = document) => [...r.querySelectorAll(s)];
export function el(html) { const t = document.createElement('template'); t.innerHTML = html.trim(); return t.content.firstElementChild; }
export const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

let toastTimer = null;
export function toast(msg, ms = 2200) {
  document.querySelectorAll('.toast').forEach((t) => t.remove());
  const t = el(`<div class="toast">${esc(msg)}</div>`); document.body.appendChild(t);
  clearTimeout(toastTimer); toastTimer = setTimeout(() => t.remove(), ms);
}

// Délégation de clics : data-act="nom" -> handlers[nom](element, event)
export function bindActs(root, handlers) {
  root.addEventListener('click', (e) => {
    const b = e.target.closest('[data-act]'); if (!b || !root.contains(b)) return;
    const f = handlers[b.dataset.act]; if (f) { e.stopPropagation(); f(b, e); }
  });
}

export function fmtTime(world, t) {
  const day = Math.floor(t / 360) + 1; const h = ((t / 360) % 1) * 24;
  return `J${day} ${String(Math.floor(h)).padStart(2, '0')}h${String(Math.floor((h % 1) * 60)).padStart(2, '0')}`;
}

export function confirmBox(text, okLabel = 'Confirmer') {
  return new Promise((res) => {
    const d = el(`<div class="screen" style="background:rgba(0,0,0,.6);justify-content:center;align-items:center;z-index:95">
      <div class="card" style="background:#1b1d36;max-width:340px;width:88%;padding:18px">
        <div style="font-size:15px;margin-bottom:16px">${esc(text)}</div>
        <div class="grid2"><button class="btn" data-r="0">Annuler</button><button class="btn y" data-r="1">${esc(okLabel)}</button></div>
      </div></div>`);
    d.addEventListener('click', (e) => { const b = e.target.closest('[data-r]'); if (b) { d.remove(); res(b.dataset.r === '1'); } });
    document.body.appendChild(d);
  });
}

export function promptBox(text, value = '') {
  return new Promise((res) => {
    const d = el(`<div class="screen" style="background:rgba(0,0,0,.6);justify-content:center;align-items:center;z-index:95">
      <div class="card" style="background:#1b1d36;max-width:340px;width:88%;padding:18px">
        <div style="font-size:15px;margin-bottom:10px">${esc(text)}</div>
        <input maxlength="40" value="${esc(value)}"/>
        <div class="grid2" style="margin-top:14px"><button class="btn" data-r="0">Annuler</button><button class="btn y" data-r="1">Valider</button></div>
      </div></div>`);
    const inp = d.querySelector('input');
    d.addEventListener('click', (e) => { const b = e.target.closest('[data-r]'); if (b) { d.remove(); res(b.dataset.r === '1' ? inp.value.trim() : null); } });
    document.body.appendChild(d); setTimeout(() => { inp.focus(); inp.select(); }, 50);
  });
}
