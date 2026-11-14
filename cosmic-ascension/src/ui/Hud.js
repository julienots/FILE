import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { EVOLUTIONS } from '../data/evolutions.js';
import { ABILITIES } from '../data/abilities.js';
import { PLANETS, getSystem } from '../data/planets.js';
import { xpProgress } from '../systems/Progression.js';
import { h } from './dom.js';
import { t } from './i18n.js';
import { buzz } from './haptics.js';
// HUD paysage (B): portrait+PV/énergie/XP (haut-gauche), poussière+pause (haut-droite), lieu+indice (haut-centre),
// suivi d'objectif (droite), toasts, bannière d'évolution. Tout est piloté par le bus + State.
const STAGE_ICON = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X'];  // numéros de forme (HUD sobre)  // glyphes (pas d'emoji: style sombre)
export function mountHud(root, { onPause } = {}) {
  const layer = h('div.layer.hud'); root.appendChild(layer);
  const portrait = h('div.portrait.noev', {}, h('span'), h('b'));
  const nm = h('div.nm.txt-out'), hp = bar('hp'), en = bar('en'), xp = bar('xp'), xpl = h('div.xpline.txt-out');
  layer.appendChild(h('div.hud-tl.noev', {}, portrait, h('div.bars', {}, nm, hp.el, en.el, xp.el, xpl)));
  const dust = h('span'), pauseBtn = h('div.ibtn', { text: '❚❚', onPointerdown: e => { e.stopPropagation(); buzz(10); onPause?.(); } });
  layer.appendChild(h('div.hud-tr', {}, h('div.chip.noev', {}, '✧', dust), pauseBtn));
  const loc = h('div.loc.txt-out'), hint = h('div.hint');
  layer.appendChild(h('div.hud-top.noev', {}, loc, hint));
  const qTitle = h('div'), qProg = h('div.pg');
  const quest = h('div.quest.noev', {}, h('h4', { text: t('hud.objective') }), qTitle, qProg); layer.appendChild(quest);
  const toasts = h('div.toasts.noev'); layer.appendChild(toasts);

  function bar(kind) { const i = h('i'), s = h('span'), el = h('div.bar.' + kind, {}, i, s); return { el, set(v, max, label = true) { i.style.transform = `scaleX(${max ? Math.max(0, Math.min(1, v / max)) : 0})`; s.textContent = label ? `${Math.ceil(v)}/${max}` : ''; } }; }
  function stats() {
    const d = State.data.hero; hp.set(d.hp, d.maxHp); en.set(d.energy, d.maxEnergy);
  }
  function progress() {
    const d = State.data.hero, e = EVOLUTIONS[d.stage], p = xpProgress(d.xp, d.stage);
    nm.textContent = e.name; portrait.firstChild.textContent = STAGE_ICON[d.stage] ?? '✦'; portrait.lastChild.textContent = t('hud.stage', { n: d.stage + 1 });
    portrait.style.setProperty('--c1', e.color); portrait.style.setProperty('--c2', e.glow);
    xp.set(p.ratio, 1, false);
    const nx = EVOLUTIONS[d.stage + 1];
    xpl.textContent = nx ? t('hud.xpTo', { need: p.need, name: nx.name }) : t('hud.max');
    dust.textContent = State.data.inventory.cosmicDust;
  }
  let trackedQ = false; // une quête suivie a priorité sur l'objectif d'exploration
  function objective() {
    if (trackedQ) return;
    // M1: objectif d'exploration. B3 remplacera par la quête suivie (`quest:tracked`).
    const vis = PLANETS.filter(p => !p.hidden).length, disc = State.data.world.discovered.filter(id => PLANETS.find(p => p.id === id && !p.hidden)).length;
    qTitle.textContent = t('obj.m1'); qProg.textContent = `${t('hud.discovered')}: ${disc}/${vis}`;
  }
  function refresh() { stats(); progress(); objective(); }
  refresh();
  bus.on('state:loaded', refresh); bus.on('hero:stats', stats); bus.on('hero:damaged', stats);
  bus.on('progress:xp', ({ gained, reason }) => { progress(); if (gained > 0) toast(`+${gained} XP · ${t('xp.' + reason)}`, 'xp'); });
  bus.on('world:discovered', objective);
  bus.on('quest:tracked', ({ title, progress: pg } = {}) => { trackedQ = !!title; if (title) { qTitle.textContent = title; qProg.textContent = pg ?? ''; } else objective(); });
  bus.on('ui:toast', ({ text, kind }) => toast(text, kind));
  bus.on('space:near', ({ planet, canLand, distance }) => {
    const sys = getSystem(planet.system);
    loc.innerHTML = ''; loc.append(planet.name, h('small', { text: sys?.name ?? '' }));
    hint.textContent = canLand ? t('near.land', { name: planet.name }) : t('near.far', { name: planet.name, dist: Math.max(0, Math.round(distance / 10)) });
    hint.classList.toggle('go', canLand);
  });
  bus.on('scene:entered', ({ id, planet }) => {
    if (id !== 'planet' || !planet) return;
    const ph = planet.phys, f = (v, d = 2) => String(v.toFixed(d)).replace('.', ',');
    const scan = ph ? `GRAV ${f(ph.g)} g · ${ph.temp > 0 ? '+' : ''}${ph.temp} °C · ${ph.atmo}${ph.day ? ` · jour ${ph.day} h` : ''}` : (planet.desc ?? '');
    loc.innerHTML = ''; loc.append(planet.name.toUpperCase(), h('small', { text: scan })); hint.textContent = ''; hint.classList.remove('go');
  });
  bus.on('hero:evolved', ({ stage }) => { refresh(); banner(stage); buzz(60); });
  bus.on('cinematic:start', () => layer.classList.add('hidden')); bus.on('cinematic:end', () => layer.classList.remove('hidden'));

  function toast(text, kind = '') {
    const el = h('div.toast' + (kind ? '.' + kind : ''), { text }); toasts.appendChild(el);
    while (toasts.children.length > 4) toasts.firstChild.remove();
    setTimeout(() => el.remove(), 3100);
  }
  let evoEl = null;
  function banner(stage) {
    const e = EVOLUTIONS[stage]; evoEl?.remove();
    const ab = e.abilities.map(a => ABILITIES[a]).filter(Boolean)[0];
    evoEl = h('div.evo.noev', {}, h('div', {}, h('h1', { text: t('evo.title') }), h('h2.txt-out', { text: e.name }), h('p.txt-out', { text: e.desc }),
      ab ? h('div.ab', { text: `${ab.icon} ${t('evo.ability', { name: ab.name })}` }) : null));
    layer.appendChild(evoEl); const me = evoEl; setTimeout(() => me.remove(), 4300);
  }
  return { layer, toast, show(v) { layer.classList.toggle('hidden', !v); } };
}
