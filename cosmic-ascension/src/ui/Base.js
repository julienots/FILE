import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { BUILDINGS, BASE_SLOTS } from '../data/buildings.js';
import { ITEMS } from '../data/items.js';
import { Home } from '../systems/HomePlanet.js';
import { bonuses } from '../systems/bonuses.js';
import { h } from './dom.js';
import { buzz } from './haptics.js';
import { SHIP_UPGRADES } from '../data/ship.js';
import { upgradeShip } from '../systems/Combat.js';
// UI de la planète du joueur (B6): menu de construction (sur `ui:buildMenu`) et panneau « Ma planète » (menu pause).
const costLine = (cost) => Object.entries(cost).map(([it, n]) => h('span.cost', { text: `${n} ${ITEMS[it]?.icon ?? it}` }));
function card(id, onPick) {
  const B = BUILDINGS[id], c = Home.check(id);
  const eff = [B.prod && 'Produit ' + Object.entries(B.prod).map(([it, n]) => `${n} ${ITEMS[it]?.icon}/h`).join(' '),
    B.bonus && Object.entries(B.bonus).map(([k, v]) => ({ hp: `+${v * 100}% PV`, energy: `+${v * 100}% énergie`, xp: `+${v * 100}% XP`, ship: `vaisseau +${v}`, reveal: 'révèle le système' }[k])).join(' · ')].filter(Boolean).join(' · ');
  return h('div.bcard' + (c.ok ? '' : c.locked ? '.locked' : '.poor') + (Home.built(id) ? '.built' : ''), { onClick: () => { if (c.ok) { buzz(15); onPick(id); } else { buzz(8); bus.emit('ui:toast', { text: c.reason, kind: 'warn' }); } } },
    h('div.bi', { text: c.locked ? '⊘' : B.icon }), h('b', { text: B.name }), h('small', { text: B.desc }), eff ? h('div.eff', { text: eff }) : null,
    h('div.costs', {}, Home.built(id) ? h('span.cost', { text: '✔ construit' }) : costLine(B.cost)), !c.ok && !Home.built(id) ? h('div.why', { text: c.reason }) : null);
}
export function buildMenu({ planetId, slot }, close) {
  const ids = Object.keys(BUILDINGS).filter(id => !Home.built(id)).sort((a, b) => (Home.check(b).ok - Home.check(a).ok) || (BUILDINGS[a].tier - BUILDINGS[b].tier));
  return h('div.bgrid', {}, ids.map(id => card(id, (pick) => { close(); bus.emit('build:choose', { planetId, slot, id: pick }); })));
}
export function basePanel() {
  const hp = State.data.homePlanet, r = Home.rates(), b = bonuses(State.data);
  const wrap = h('div.base');
  wrap.appendChild(h('div.stats', {}, h('div', {}, 'Niveau', h('b', { text: hp.level })), h('div', {}, 'Bâtiments', h('b', { text: `${hp.buildings.length}/${BASE_SLOTS}` })),
    h('div', {}, 'Production / h', h('b', { text: Object.entries(r).map(([it, n]) => `${n}${ITEMS[it]?.icon}`).join(' ') || '—' })),
    h('div', {}, 'Bonus', h('b', { text: [b.hp && `PV +${Math.round(b.hp * 100)}%`, b.energy && `Énergie +${Math.round(b.energy * 100)}%`, b.xp && `XP +${Math.round(b.xp * 100)}%`, b.ship && `Vaisseau niv. ${1 + b.ship}`].filter(Boolean).join(' · ') || '—' }))));
  wrap.appendChild(h('div.bgrid.small', {}, hp.buildings.length ? hp.buildings.map(({ id }) => card(id, () => {})) : h('p.empty', { text: 'Pose-toi sur Pépin et approche-toi d’un emplacement ✚ pour construire.' })));
  wrap.appendChild(h('button.cbtn.alt', { text: 'Récolter la production', style: { marginTop: '2vmin', width: '100%' }, onClick: () => { const got = Home.collect(); if (!got.length) bus.emit('ui:toast', { text: 'Rien à récolter pour l’instant', kind: 'warn' }); } }));
  return wrap;
}
// Atelier du vaisseau
export function workshopPanel() {
  const wrap = h('div.bgrid');
  const draw = () => { wrap.innerHTML = ''; const d = State.data;
    wrap.appendChild(h('div.bcard.built', {}, h('div.bi', { text: '⏏' }), h('b', { text: `Vaisseau niveau ${d.ship.level}` }), h('small', { text: 'Moteur, bâtiments (Hangar, Phare, Portail) et artefacts augmentent le niveau.' })));
    for (const [part, U] of Object.entries(SHIP_UPGRADES)) { const n = d.ship.parts[part] ?? 0, max = n >= U.max, cost = max ? null : U.cost(n);
      const can = cost && Object.entries(cost).every(([it, k]) => (it === 'cosmicDust' ? d.inventory.cosmicDust : d.inventory.items[it] ?? 0) >= k);
      wrap.appendChild(h('div.bcard' + (max ? '.built' : can ? '' : '.poor'), { onClick: () => { if (max) return; if (upgradeShip(part, SHIP_UPGRADES)) { buzz(30); draw(); } else { buzz(8); bus.emit('ui:toast', { text: 'Ressources insuffisantes', kind: 'warn' }); } } },
        h('div.bi', { text: U.glyph }), h('b', { text: `${U.name} · ${n}/${U.max}` }), h('div.eff', { text: U.per }),
        h('div.costs', {}, max ? h('span.cost', { text: '✔ maximum' }) : costLine(cost)))); } };
  draw(); return wrap;
}
