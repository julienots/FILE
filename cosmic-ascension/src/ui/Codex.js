import { State } from '../core/State.js';
import { CREATURES, ARTIFACTS, ACHIEVEMENTS } from '../data/collections.js';
import { ENEMIES } from '../data/enemies.js';
import { getPlanet } from '../data/planets.js';
import { h } from './dom.js';
// Codex (B7): onglets Créatures / Artefacts / Bestiaire / Succès.
export function codexPanel() {
  const c = State.data.collection, body = h('div.codex-body'), tabs = h('div.seg.codex-tabs');
  const T = {
    'Créatures': () => grid(Object.entries(CREATURES).map(([id, C]) => { const got = c.creatures.includes(id);
      return h('div.cx' + (got ? '' : '.unk') + '.r-' + (C.rarity === 'légendaire' ? 'leg' : C.rarity === 'rare' ? 'rare' : 'com'), {}, h('span.g', { text: got ? C.glyph : '?' }), h('b', { text: got ? C.name : '???' }), h('small', { text: got ? C.desc : getPlanet(C.planet)?.name ?? '' })); }), `${c.creatures.length}/${Object.keys(CREATURES).length}`),
    'Artefacts': () => grid(Object.entries(ARTIFACTS).map(([id, A]) => { const got = c.artifacts.includes(id);
      const fx = Object.entries(A.bonus).map(([k, v]) => ({ hp: `PV +${v * 100}%`, energy: `Énergie +${v * 100}%`, xp: `XP +${v * 100}%`, ship: `Vaisseau +${v}` }[k])).join(' · ');
      return h('div.cx' + (got ? '.r-leg' : '.unk'), {}, h('span.g', { text: got ? A.glyph : '?' }), h('b', { text: got ? A.name : '???' }), h('small', { text: got ? `${A.desc} — ${fx}` : 'Non découvert' })); }), `${c.artifacts.length}/${Object.keys(ARTIFACTS).length}`),
    'Bestiaire': () => grid(Object.entries(ENEMIES).map(([id, E]) => { const n = c.bestiary[id] ?? 0;
      return h('div.cx' + (n ? '' : '.unk'), {}, h('span.g', { text: n ? '☠' : '?' }), h('b', { text: n ? E.name : '???' }), h('small', { text: n ? `Vaincus : ${n} · PV ${E.hp} · ${E.behavior}${E.elite ? ' · élite' : ''}` : 'Jamais affronté' })); }), `${Object.keys(c.bestiary).length}/${Object.keys(ENEMIES).length}`),
    'Succès': () => grid(ACHIEVEMENTS.map(a => { const got = c.achievements.includes(a.id);
      return h('div.cx' + (got ? '.r-rare' : '.unk'), {}, h('span.g', { text: got ? a.glyph : '🔒' }), h('b', { text: a.name }), h('small', { text: a.desc })); }), `${c.achievements.length}/${ACHIEVEMENTS.length}`),
  };
  function grid(items, count) { return h('div', {}, h('div.cx-count', { text: count }), h('div.cxgrid', {}, items)); }
  const show = (k) => { tabs.querySelectorAll('button').forEach(b => b.classList.toggle('sel', b.textContent === k)); body.innerHTML = ''; body.appendChild(T[k]()); };
  for (const k in T) tabs.appendChild(h('button', { text: k, onClick: () => show(k) }));
  const wrap = h('div.codex', {}, tabs, body); show('Créatures'); return wrap;
}
