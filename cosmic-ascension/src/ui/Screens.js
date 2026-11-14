import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { Config } from '../core/Config.js';
import { EVOLUTIONS } from '../data/evolutions.js';
import { PLANETS } from '../data/planets.js';
import { h, fmtTime } from './dom.js';
import { t } from './i18n.js';
import { buzz } from './haptics.js';
import { journalPanel, inventoryPanel } from './Journal.js';
import { buildMenu, basePanel, workshopPanel } from './Base.js';
import { codexPanel } from './Codex.js';
import { galaxyPanel } from './GalaxyMap.js';
// Écrans plein cadre (B): titre, pause, réglages, confirmation. Un seul écran visible à la fois (pile).
export function mountScreens(root, { onPlay, onQuit } = {}) {
  const layer = h('div.layer.screens'); root.appendChild(layer);
  const stack = []; let mode = null;
  const push = (el) => { stack.at(-1)?.classList.add('hidden'); stack.push(el); layer.appendChild(el); };
  const pop = () => { stack.pop()?.remove(); stack.at(-1)?.classList.remove('hidden'); };
  const clear = () => { while (stack.length) stack.pop().remove(); };
  const btn = (label, cls, fn) => h('button.cbtn' + (cls ? '.' + cls : ''), { text: label, onClick: () => { buzz(10); bus.emit('ui:click'); fn(); } });

  function confirm(text, yes) {
    push(h('div.screen', {}, h('div.panel.confirm', {}, h('p', { text }), h('div.menu', {}, btn(t('no'), 'ghost', pop), btn(t('yes'), 'danger', () => { pop(); yes(); })))));
  }
  function title() {
    clear(); mode = 'title';
    const has = State.hasSave && State.data.meta.seenIntro;
    const start = () => { clear(); State.data.meta.seenIntro = true; State.save(); requestFullscreen(); onPlay?.(); };
    push(h('div.screen.title', {},
      h('div.logo', {}, 'COSMIC', h('span', { text: 'ASCENSION' })), h('div.tag.txt-out', { text: t('title.tagline') }),
      h('div.menu', {},
        has ? btn(t('title.continue'), '', start) : btn(t('title.play'), '', start),
        has ? btn(t('title.new'), 'alt', () => confirm(t('title.confirmNew'), () => { State.reset(); start(); })) : null,
        btn(t('title.settings'), 'ghost', settings)),
      h('div.ver', { text: 'v' + Config.version })));
  }
  function pause() {
    if (stack.length) return; mode = 'pause'; bus.emit('game:pause');
    const d = State.data, vis = PLANETS.filter(p => !p.hidden).length;
    const st = (k, v) => h('div', {}, t(k), h('b', { text: v }));
    const resume = () => { clear(); mode = null; bus.emit('game:resume'); };
    push(h('div.screen', {}, h('div.panel', {}, h('h2', { text: t('pause.title') }),
      h('div.stats', {}, st('stats.stage', EVOLUTIONS[d.hero.stage].name), st('stats.planets', `${d.world.discovered.length}/${vis}`), st('stats.kills', d.stats.kills), st('stats.time', fmtTime(d.stats.playTime))),
      h('div.menu', {}, btn(t('pause.resume'), '', resume),
        h('div.mgrid', {}, btn('★ ' + t('pause.quests'), 'alt', () => sub(t('pause.quests'), journalPanel())), btn('✧ ' + t('pause.inventory'), 'alt', () => sub(t('pause.inventory'), inventoryPanel())),
          btn('⌖ ' + t('pause.map'), 'alt', () => sub(t('pause.map'), galaxyPanel(() => { clear(); mode = null; bus.emit('game:resume'); }))), btn('◈ ' + t('pause.codex'), 'alt', () => sub(t('pause.codex'), codexPanel())),
          btn('🌍 Ma planète', 'alt', () => sub('Ma planète', basePanel())), btn('🛸 Atelier', 'alt', () => sub('Atelier du vaisseau', workshopPanel())),
          btn(t('pause.settings'), 'ghost', settings),
          btn(t('pause.quit'), 'ghost', () => { State.save(); clear(); bus.emit('game:resume'); onQuit?.(); title(); }))))));
  }
  // menu de construction: met le jeu en pause le temps du choix
  function build(req) { if (stack.length) return; mode = 'pause'; bus.emit('game:pause'); const close = () => { clear(); mode = null; bus.emit('game:resume'); };
    push(h('div.screen', {}, h('div.panel.wide', {}, h('h2', { text: 'Construire' }), buildMenu(req, close), h('div.menu', { style: { marginTop: '3vmin' } }, btn(t('back'), '', close))))); }
  bus.on('ui:buildMenu', build);
  function sub(title, content) { push(h('div.screen', {}, h('div.panel.wide', {}, h('h2', { text: title }), content, h('div.menu', { style: { marginTop: '3vmin' } }, btn(t('back'), '', pop))))); }
  function settings() {
    const s = State.data.settings, changed = () => { State.save(); bus.emit('settings:changed', s); };
    const seg = (opts, get, set) => { const w = h('div.seg'); const draw = () => { w.innerHTML = ''; for (const [v, l] of opts) w.appendChild(h('button' + (get() === v ? '.sel' : ''), { text: l, onClick: () => { set(v); changed(); draw(); buzz(8); } })); }; draw(); return w; };
    const range = (k) => h('input', { type: 'range', min: 0, max: 1, step: .05, value: s[k], onInput: e => { s[k] = +e.target.value; changed(); } });
    const tog = (k) => { const el = h('div.tog' + (s[k] ? '.on' : ''), { onClick: () => { s[k] = !s[k]; el.classList.toggle('on', s[k]); changed(); buzz(15); } }); return el; };
    const row = (k, ctl) => h('div.row', {}, h('span', { text: t(k) }), ctl);
    push(h('div.screen', {}, h('div.panel', {}, h('h2', { text: t('set.title') }),
      row('set.quality', seg([['auto', t('set.q.auto')], [0, t('set.q.0')], [1, t('set.q.1')], [2, t('set.q.2')]], () => s.quality, v => { s.quality = v; bus.emit('settings:quality', { level: v }); })),
      row('set.music', range('music')), row('set.sfx', range('sfx')), row('set.lefty', tog('leftHanded')), row('set.haptics', tog('haptics')),
      h('div.menu', { style: { marginTop: '3vmin' } }, btn(t('back'), '', pop),
        btn(t('set.reset'), 'danger', () => confirm(t('set.resetConfirm'), () => { State.reset(); clear(); title(); }))))));
  }
  // retour (Échap / bouton Android): referme le panneau du dessus; en pause, le dernier panneau reprend la partie
  function back() { if (!stack.length) return pause(); if (mode === 'title' && stack.length === 1) return; if (stack.length > 1) return pop(); if (mode === 'pause') { clear(); mode = null; bus.emit('game:resume'); } }
  return { layer, title, pause, settings, back, get open() { return stack.length > 0; } };
}
function requestFullscreen() {
  try { const el = document.documentElement; if (!document.fullscreenElement && el.requestFullscreen) el.requestFullscreen({ navigationUI: 'hide' }).catch(() => {}); screen.orientation?.lock?.('landscape').catch(() => {}); } catch { /* ignore */ }
}
