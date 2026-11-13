import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { QUESTS } from '../data/quests.js';
import { ITEMS } from '../data/items.js';
import { getPlanet } from '../data/planets.js';
import { h } from './dom.js';
// Panneaux du menu pause (B): journal de quêtes (suivre une quête) et inventaire.
export function journalPanel() {
  const d = State.data.quests, wrap = h('div.journal');
  const act = Object.keys(d.active).sort((a, b) => (QUESTS[a].kind === 'main' ? -1 : 1) - (QUESTS[b].kind === 'main' ? -1 : 1));
  if (!act.length) wrap.appendChild(h('p.empty', { text: 'Aucune quête en cours.' }));
  for (const id of act) {
    const q = QUESTS[id], a = d.active[id], st = q.steps[a.step], tr = d.tracked === id;
    const card = h('div.qcard' + (tr ? '.tracked' : '') + '.' + q.kind, { onClick: () => { bus.emit('quest:track', { id }); wrap.querySelectorAll('.qcard').forEach(c => c.classList.remove('tracked')); card.classList.add('tracked'); } },
      h('div.qh', {}, h('b', { text: (q.kind === 'main' ? '★ ' : q.kind === 'secret' ? '✦ ' : '◆ ') + q.title }), q.planet ? h('small', { text: getPlanet(q.planet)?.name ?? '' }) : null),
      h('div.qd', { text: q.desc }),
      h('div.qs', {}, ...q.steps.map((s, i) => h('div' + (i < a.step ? '.ok' : i === a.step ? '.cur' : '.todo'), { text: (i < a.step ? '✔ ' : i === a.step ? '▸ ' : '· ') + (i <= a.step ? s.text : '???') + (i === a.step && s.count > 1 ? ` (${Math.min(a.n, s.count)}/${s.count})` : '') }))));
    wrap.appendChild(card);
  }
  wrap.appendChild(h('div.qdone', { text: `Quêtes terminées: ${d.done.length} / ${Object.keys(QUESTS).length}` }));
  return wrap;
}
export function inventoryPanel() {
  const inv = State.data.inventory, grid = h('div.inv');
  grid.appendChild(slot('cosmicDust', inv.cosmicDust));
  for (const [id, n] of Object.entries(inv.items)) if (n > 0) grid.appendChild(slot(id, n));
  if (grid.children.length === 1) grid.appendChild(h('p.empty', { text: 'Explore et combats pour trouver des ressources.' }));
  return grid;
  function slot(id, n) { const it = ITEMS[id] ?? { name: id, icon: '?' }; return h('div.slot', { title: it.desc ?? '' }, h('span.ic', { text: it.icon }), h('b', { text: n }), h('small', { text: it.name })); }
}
