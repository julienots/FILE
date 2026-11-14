import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
// Écran de fin: affiché quand le joueur choisit son destin (drapeau `ending_chosen`, dernière étape de l'acte 5).
const TEXT = {
  ending_create: ['CRÉER', 'Tu sèmes de nouveaux mondes dans le vide. Là où il n’y avait rien, des soleils s’allument.'],
  ending_protect: ['PROTÉGER', 'Tu restes parmi les tiens. Tant que tu veilles, aucune ombre ne dévorera plus leurs ciels.'],
  ending_transcend: ['TRANSCENDER', 'Tu te dissous dans la trame de l’univers. Chaque étoile porte désormais un peu de toi.'],
};
export function mountEnding(root) {
  let shown = false;
  const show = () => { if (shown) return; const f = State.data.world.flags, key = Object.keys(TEXT).find(k => f[k]) ?? 'ending_protect', [title, text] = TEXT[key]; shown = true;
    const s = State.data.stats ?? {}, el = document.createElement('div');
    el.style.cssText = 'position:fixed;inset:0;z-index:80;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:2.2vmin;background:radial-gradient(circle at 50% 40%,#101830ee,#02030aee);color:#e8f0ff;font-family:system-ui,sans-serif;text-align:center;opacity:0;transition:opacity 2s;padding:6vmin;pointer-events:auto';
    el.innerHTML = `<div style="letter-spacing:.5em;font-size:2.6vmin;color:#7fd0ff">COSMIC ASCENSION</div>
      <div style="font-size:9vmin;font-weight:800;letter-spacing:.2em;text-shadow:0 0 30px #6ff2ff">FIN</div>
      <div style="font-size:3.4vmin;letter-spacing:.3em;color:#ffe9a0">${title}</div>
      <div style="max-width:70vw;font-size:3vmin;line-height:1.5;opacity:.9">${text}</div>
      <div style="font-size:2.3vmin;opacity:.6">Ennemis vaincus: ${s.kills ?? 0} · Objets absorbés: ${s.pickups ?? 0} · Mondes visités: ${State.data.world.visited.length}</div>
      <button style="margin-top:2vmin;padding:1.8vmin 5vmin;font-size:2.8vmin;letter-spacing:.2em;color:#e8f0ff;background:#0a1428;border:1px solid #6ff2ff;border-radius:.8vmin">CONTINUER À EXPLORER</button>`;
    el.querySelector('button').onclick = () => { el.style.opacity = 0; setTimeout(() => el.remove(), 2000); bus.emit('game:resume', {}); };
    root.appendChild(el); requestAnimationFrame(() => { el.style.opacity = 1; }); bus.emit('game:pause', {}); bus.emit('game:ended', { ending: key }); State.save?.(); };
  bus.on('world:flag', ({ flag }) => { if (flag === 'ending_chosen') setTimeout(show, 2500); });
  bus.on('quest:done', ({ id }) => { if (id === 'main_5_ascension') setTimeout(show, 2500); });
}
