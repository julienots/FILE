import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { PLANETS, SYSTEMS, getPlanet, getSystem } from '../data/planets.js';
import { QUESTS } from '../data/quests.js';
import { h } from './dom.js';
import { buzz } from './haptics.js';
// Carte galactique (B1): vue de dessus (x,z) des systèmes et planètes découvertes, quête suivie, saut warp.
// Warp: `warp:request {planetId}` (main.js → transition de A vers la scène espace, vaisseau près de la planète). Coût en poussière entre systèmes.
export const WARP_COST = 25;
const NS = 'http://www.w3.org/2000/svg', svg = (tag, a = {}) => { const e = document.createElementNS(NS, tag); for (const k in a) e.setAttribute(k, a[k]); return e; };
export function warpCost(to) { const d = State.data, here = getPlanet(d.world.location)?.system ?? d.world.system; return getPlanet(to)?.system === here ? 0 : WARP_COST * (1 + (getPlanet(to)?.danger ?? 0)); }
export function galaxyPanel(close) {
  const d = State.data, disc = new Set(d.world.discovered), vis = PLANETS.filter(p => !p.hidden || disc.has(p.id));
  const xs = vis.map(p => p.pos[0]), zs = vis.map(p => p.pos[2]), pad = 900;
  const minX = Math.min(...xs) - pad, maxX = Math.max(...xs) + pad, minZ = Math.min(...zs) - pad, maxZ = Math.max(...zs) + pad;
  const W = 1000, H = Math.round(W * .52), sx = x => (x - minX) / (maxX - minX) * W, sz = z => (z - minZ) / (maxZ - minZ) * H;
  const map = svg('svg', { viewBox: `0 0 ${W} ${H}`, class: 'gmap' });
  const tracked = d.quests.tracked && QUESTS[d.quests.tracked], target = tracked && (QUESTS[d.quests.tracked].steps[d.quests.active[d.quests.tracked]?.step]?.target ?? tracked.planet);
  for (const s of SYSTEMS) { const ps = vis.filter(p => p.system === s.id && disc.has(p.id)); if (!ps.length) continue;
    const cx = ps.reduce((a, p) => a + sx(p.pos[0]), 0) / ps.length, cy = ps.reduce((a, p) => a + sz(p.pos[2]), 0) / ps.length;
    map.appendChild(svg('circle', { cx, cy, r: 120, fill: s.color, opacity: .07 }));
    const t = svg('text', { x: cx, y: cy - 95, class: 'gsys', fill: s.color }); t.textContent = s.name.toUpperCase(); map.appendChild(t); }
  const info = h('div.ginfo');
  for (const p of vis) {
    const known = disc.has(p.id), x = sx(p.pos[0]), y = sz(p.pos[2]), r = Math.max(9, p.radius / 9);
    const g = svg('g', { class: 'gp' + (known ? '' : ' unk') + (p.id === d.world.location ? ' here' : '') + (p.id === target ? ' tgt' : '') });
    if (p.id === target) g.appendChild(svg('circle', { cx: x, cy: y, r: r + 14, class: 'gring' }));
    g.appendChild(svg('circle', { cx: x, cy: y, r, fill: known ? p.palette.peak : '#333', stroke: known ? p.atmosphere.color : '#555' }));
    const t = svg('text', { x, y: y + r + 22, class: 'gname' }); t.textContent = known ? p.name : '?'; g.appendChild(t);
    if (known) g.addEventListener('click', () => { buzz(8); select(p); });
    map.appendChild(g);
  }
  function select(p) {
    const cost = warpCost(p.id), here = p.id === d.world.location, sys = getSystem(p.system);
    const quests = Object.values(QUESTS).filter(q => q.planet === p.id).length;
    info.innerHTML = '';
    info.append(h('b', { text: p.name }), h('small', { text: `${sys?.name ?? ''} · danger ${'◆'.repeat(p.danger ?? 0) || '—'}` }), h('p', { text: p.desc ?? '' }),
      h('small', { text: `${p.species ?? ''}${quests ? ` · ${quests} quête(s)` : ''}${d.world.visited.includes(p.id) ? ' · visitée' : ''}` }),
      here ? h('div.cost', { text: 'Tu es ici' }) : h('button.cbtn', { text: cost ? `Saut warp · ${cost} ✧` : 'Saut warp', disabled: d.inventory.cosmicDust < cost, onClick: () => {
        if (d.inventory.cosmicDust < cost) return; d.inventory.cosmicDust -= cost; buzz(40); close(); bus.emit('warp:request', { planetId: p.id, cost }); } }));
  }
  select(getPlanet(target) && disc.has(target) ? getPlanet(target) : getPlanet(d.world.location) ?? vis[0]);
  return h('div.galaxy', {}, map, info);
}
