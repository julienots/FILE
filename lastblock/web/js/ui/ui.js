// Interface HTML : menu principal (qui se corrompt), chapitres, collection, paramètres
// (accessibilité, contrôles personnalisables), crédits, HUD, popups, messages, modales,
// faux écrans (chargement, menu), vue depuis la cachette, serrure-puzzle.
import { PALETTES, MATERIALS } from '../world/materials.js';
import { THEMES, GRIDS } from '../world/board3d.js';
import { SHAPES } from '../game/tetris.js';

const $ = (id) => document.getElementById(id);

export const CHAPTERS = [
  { n: 1, name: 'THE GAME', desc: 'Une partie normale. Enfin… presque.' },
  { n: 2, name: 'THE CRACK', desc: 'Quelque chose essaie de te parler.' },
  { n: 3, name: 'BREAK THE SCREEN', desc: "L'écran ne le retiendra plus." },
  { n: 4, name: 'RUN', desc: 'Cours.' },
  { n: 5, name: 'THE CORRUPTION', desc: 'Les pièces sont vivantes.' },
  { n: 6, name: 'THE KING', desc: 'Le roi de tous les blocs.' },
  { n: 7, name: 'THE LAST BLOCK', desc: 'Le dernier bloc. La dernière décision.' },
];

export const SECRETS = [
  { id: 'eye-1', ch: 1, hint: 'Réussir un LAST BLOCK (4 lignes) pendant le chapitre 1.' },
  { id: 'eye-2', ch: 2, hint: 'Lui donner la seule réponse qu\'il attendait.' },
  { id: 'eye-3', ch: 4, hint: 'Terminer la poursuite en étant touché au plus une fois.' },
  { id: 'eye-4', ch: 5, hint: 'Détruire 6 pièces vivantes.' },
  { id: 'eye-5', ch: 6, hint: 'Vaincre le Roi sans jamais perdre la partie.' },
];

// Objets de collection et conditions de déblocage
export const ITEMS = [
  ...Object.entries(PALETTES).map(([id, p]) => ({ cat: 'palette', id, name: p.name, sw: ['I', 'O', 'T', 'Z'].map((k) => p[k]) })),
  ...Object.entries(MATERIALS).map(([id, m]) => ({ cat: 'material', id, name: m.name })),
  { cat: 'burst', id: 'shards', name: 'Éclats' }, { cat: 'burst', id: 'confetti', name: 'Confettis' }, { cat: 'burst', id: 'pixels', name: 'Pixels' }, { cat: 'burst', id: 'sparks', name: 'Étincelles' }, { cat: 'burst', id: 'void', name: 'Néant' },
  { cat: 'trail', id: 'none', name: 'Aucune' }, { cat: 'trail', id: 'light', name: 'Lumière' }, { cat: 'trail', id: 'rainbow', name: 'Arc-en-ciel' },
  ...Object.entries(GRIDS).map(([id, name]) => ({ cat: 'grid', id, name })),
  ...Object.entries(THEMES).map(([id, t]) => ({ cat: 'theme', id, name: t.name })),
  { cat: 'sound', id: 'classic', name: 'Classique' }, { cat: 'sound', id: 'chip', name: '8-bit' }, { cat: 'sound', id: 'soft', name: 'Doux' },
];
export const UNLOCKS = {
  'palette:neon': { txt: '5 000 pts en Arcade', ok: (s) => s.data.records.arcade >= 5000 },
  'palette:pastel': { txt: 'Finir le chapitre 1', ok: (s) => s.p.completed.includes(1) },
  'palette:candy': { txt: '100 lignes au total', ok: (s) => s.data.stats.linesTotal >= 100 },
  'palette:sunset': { txt: 'Finir le chapitre 4', ok: (s) => s.p.completed.includes(4) },
  'palette:obsidian': { txt: 'Finir le chapitre 5', ok: (s) => s.p.completed.includes(5) },
  'palette:gold': { txt: 'Vaincre le Roi', ok: (s) => s.p.completed.includes(6) },
  'material:matte': { txt: 'Finir le chapitre 2', ok: (s) => s.p.completed.includes(2) },
  'material:metal': { txt: 'Finir le chapitre 6', ok: (s) => s.p.completed.includes(6) },
  'material:jelly': { txt: '15 000 pts en Arcade', ok: (s) => s.data.records.arcade >= 15000 },
  'material:toon': { txt: 'Trouver 2 secrets', ok: (s) => s.data.secrets.length >= 2 },
  'burst:confetti': { txt: 'Finir le chapitre 1', ok: (s) => s.p.completed.includes(1) },
  'burst:pixels': { txt: '50 lignes au total', ok: (s) => s.data.stats.linesTotal >= 50 },
  'burst:sparks': { txt: 'Finir le chapitre 3', ok: (s) => s.p.completed.includes(3) },
  'burst:void': { txt: 'Finir le chapitre 5', ok: (s) => s.p.completed.includes(5) },
  'trail:light': { txt: 'Finir le chapitre 2', ok: (s) => s.p.completed.includes(2) },
  'trail:rainbow': { txt: 'Trouver 4 secrets', ok: (s) => s.data.secrets.length >= 4 },
  'grid:minimal': { txt: 'Finir le chapitre 1', ok: (s) => s.p.completed.includes(1) },
  'grid:retro': { txt: '10 000 pts en Arcade', ok: (s) => s.data.records.arcade >= 10000 },
  'grid:none': { txt: 'Finir le chapitre 4', ok: (s) => s.p.completed.includes(4) },
  'theme:sunset': { txt: 'Finir le chapitre 3', ok: (s) => s.p.completed.includes(3) },
  'theme:mint': { txt: '200 lignes au total', ok: (s) => s.data.stats.linesTotal >= 200 },
  'theme:void': { txt: 'Voir une fin', ok: (s) => s.p.endings.length > 0 },
  'sound:chip': { txt: 'Finir le chapitre 2', ok: (s) => s.p.completed.includes(2) },
  'sound:soft': { txt: '3 000 pts en Arcade', ok: (s) => s.data.records.arcade >= 3000 },
};
const CAT_NAMES = { palette: 'Couleurs', material: 'Matériaux', burst: 'Effets de destruction', trail: 'Traînées lumineuses', grid: 'Styles de grille', theme: 'Thèmes visuels', sound: 'Effets sonores' };
const KEY_NAMES = { left: 'Gauche', right: 'Droite', soft: 'Chute douce', hard: 'Chute instantanée', rotCW: 'Rotation ⟳', rotCCW: 'Rotation ⟲', hold: 'Réserve', pause: 'Pause' };

export class UI {
  constructor(game) {
    this.game = game;
    $('pauseBtn').addEventListener('click', () => { game.audio.sfx('pause'); game.pause(); });
    $('panel').addEventListener('pointerdown', () => game.audio.init());
  }

  // ——— menu ———
  buildMenu() {
    const g = this.game, s = g.save;
    const c = s.p.corruption;
    $('title').classList.toggle('corrupt', c >= 3);
    const items = [
      ['new', 'Nouvelle partie', 'primary'],
      ['continue', s.hasProgress() ? `Continuer · Ch. ${s.p.chapter}` : 'Continuer', '', !s.hasProgress()],
      ['chapters', 'Chapitres'],
      ['arcade', 'Mode Arcade'],
      ['collection', 'Collection'],
      ['settings', 'Paramètres'],
      ['credits', 'Crédits'],
    ];
    if (c >= 5) items[3][1] = 'M0de Arcad3';
    $('menuButtons').innerHTML = items.map(([a, l, cls, dis]) => `<button class="btn ${cls || ''}" data-m="${a}" ${dis ? 'disabled' : ''}>${l}</button>`).join('');
    $('menuButtons').querySelectorAll('[data-m]').forEach((b) => b.addEventListener('click', () => { g.audio.init(); g.audio.sfx('ui'); g.menuAction(b.dataset.m); }));
  }
  showMenu(on) { $('menu').classList.toggle('hidden', !on); }

  // ——— panneaux ———
  closePanel() { $('panel').classList.add('hidden'); $('panel').innerHTML = ''; this.panelOpen = null; }
  panel(name) {
    const P = $('panel'); this.panelOpen = name;
    P.classList.remove('hidden');
    const head = (title) => `<div class="panel-head"><button class="btn small" data-close>←</button><h2>${title}</h2></div>`;
    if (name === 'chapters') P.innerHTML = head('Chapitres') + `<div class="panel-body">${this._chapters()}</div>`;
    else if (name === 'collection') P.innerHTML = head('Collection') + `<div class="panel-body">${this._collection()}</div>`;
    else if (name === 'settings') P.innerHTML = head('Paramètres') + `<div class="panel-body">${this._settings()}</div>`;
    else if (name === 'credits') P.innerHTML = head('Crédits') + `<div class="panel-body">${this._credits()}</div>`;
    P.querySelector('[data-close]').addEventListener('click', () => { this.game.audio.sfx('uiBack'); this.closePanel(); });
    this._bind(name);
  }
  _chapters() {
    const s = this.game.save;
    return CHAPTERS.map((c) => {
      const locked = c.n > s.p.unlocked;
      const done = s.p.completed.includes(c.n);
      const sec = SECRETS.filter((x) => x.ch === c.n).map((x) => (s.data.secrets.includes(x.id) ? '👁' : '·')).join('');
      return `<div class="card chap ${locked ? 'locked' : ''}" data-ch="${c.n}"><div class="num">${locked ? '🔒' : c.n}</div><div class="info"><b>${locked ? '???' : c.name}</b><span>${locked ? 'Verrouillé' : c.desc}</span></div><div class="badge">${done ? '✔' : ''} ${sec}</div></div>`;
    }).join('') + `<div class="card"><b>Fins découvertes</b><br><span style="color:var(--muted);font-weight:500">${['peace', 'loop', 'true'].map((e) => (s.p.endings.includes(e) ? { peace: 'Le Repos', loop: 'La Boucle', true: 'La Vraie Couleur' }[e] : '???')).join(' · ')}</span></div>`;
  }
  _collection() {
    const s = this.game.save;
    const eq = s.data.collection.equipped;
    let html = '';
    for (const cat of Object.keys(CAT_NAMES)) {
      html += `<h3>${CAT_NAMES[cat]}</h3><div class="grid-items">`;
      for (const it of ITEMS.filter((i) => i.cat === cat)) {
        const key = `${cat}:${it.id}`;
        const owned = s.owns(key);
        const sw = it.sw ? `<div class="sw">${it.sw.map((c) => `<i style="background:#${c.toString(16).padStart(6, '0')}"></i>`).join('')}</div>` : '';
        html += `<div class="item ${owned ? '' : 'locked'} ${eq[cat] === it.id ? 'on' : ''}" data-item="${key}">${sw}${owned ? it.name : '🔒 ' + it.name}${owned ? '' : `<small>${UNLOCKS[key] ? UNLOCKS[key].txt : ''}</small>`}</div>`;
      }
      html += '</div>';
    }
    html += `<h3>Secrets (${s.data.secrets.length}/${SECRETS.length})</h3>`;
    html += SECRETS.map((x) => `<div class="card" style="padding:10px">${s.data.secrets.includes(x.id) ? '👁 <b>Œil trouvé</b> — ' + x.hint : `· Chapitre ${x.ch} — ???`}</div>`).join('');
    const st = s.data.stats;
    html += `<h3>Statistiques</h3><div class="card"><div class="row"><label>Record Arcade</label><b>${s.data.records.arcade}</b></div><div class="row"><label>Lignes totales</label><b>${st.linesTotal}</b></div><div class="row"><label>Pièces posées</label><b>${st.piecesTotal}</b></div><div class="row"><label>LAST BLOCK (4 lignes)</label><b>${st.tetrises}</b></div><div class="row"><label>Captures par le monstre</label><b>${s.p.deaths}</b></div></div>`;
    return html;
  }
  _settings() {
    const s = this.game.save.s;
    const sw = (k, label) => `<div class="row"><label>${label}</label><button class="switch ${s[k] ? 'on' : ''}" data-sw="${k}"></button></div>`;
    const seg = (k, label, opts) => `<div class="row"><label>${label}</label><div class="seg" data-seg="${k}">${opts.map(([v, l]) => `<button data-v="${v}" class="${s[k] === v ? 'on' : ''}">${l}</button>`).join('')}</div></div>`;
    const range = (k, label, min, max, step) => `<div class="row"><label>${label}</label><input type="range" min="${min}" max="${max}" step="${step}" value="${s[k]}" data-range="${k}"></div>`;
    return `<h3>Audio</h3><div class="card">${range('music', 'Musique', 0, 1, 0.05)}${range('sfx', 'Effets sonores', 0, 1, 0.05)}</div>
    <h3>Graphismes</h3><div class="card">${seg('quality', 'Qualité', [['performance', 'Performance'], ['auto', 'Auto'], ['quality', 'Qualité']])}${sw('ghost', 'Pièce fantôme')}</div>
    <h3>Contrôles</h3><div class="card">${seg('controls', 'Mode', [['buttons', 'Boutons'], ['gestures', 'Gestes']])}${sw('leftHanded', 'Gaucher (inverser les boutons)')}${range('buttonSize', 'Taille des boutons', 0.75, 1.3, 0.05)}${range('sensitivity', 'Sensibilité caméra / gestes', 0.4, 2, 0.1)}${sw('haptics', 'Vibrations')}</div>
    <h3>Clavier</h3><div class="card">${Object.keys(KEY_NAMES).map((k) => `<div class="row"><label>${KEY_NAMES[k]}</label><button class="keycap" data-key="${k}">${s.keys[k]}</button></div>`).join('')}</div>
    <h3>Accessibilité</h3><div class="card">${sw('reduceFlash', 'Réduire les flashs')}${sw('reduceShake', 'Réduire les mouvements de caméra')}${sw('softScares', 'Limiter les apparitions intenses')}</div>
    <h3>Données</h3><div class="card"><button class="btn danger small" data-reset style="width:100%">Effacer la progression</button></div>`;
  }
  _credits() {
    return `<div class="credits card"><div style="font-size:30px;margin:8px 0"><span style="color:#fff">Super</span><span style="color:var(--yellow)">Essence.</span></div>
      <b>THE LAST BLOCK</b><br>Un jeu SuperEssence Studio<br><br>
      <b>Conception, programmation, modèles 3D procéduraux</b><br>SuperEssence Studio<br><br>
      <b>Musique et sons</b><br>Synthèse procédurale en temps réel (Web Audio)<br><br>
      <b>Moteur 3D</b><br>three.js — licence MIT<br><br>
      <b>Police</b><br>Fredoka — SIL Open Font License<br><br>
      <span style="color:var(--muted)">Merci d'avoir joué.<br>Il te regarde encore.</span></div>`;
  }
  _bind(name) {
    const g = this.game, s = g.save, P = $('panel');
    if (name === 'chapters') P.querySelectorAll('[data-ch]').forEach((el) => el.addEventListener('click', () => {
      const n = +el.dataset.ch; if (n > s.p.unlocked) { g.audio.sfx('rotateFail'); return; }
      g.audio.sfx('ui'); this.closePanel(); g.startChapter(n);
    }));
    if (name === 'collection') P.querySelectorAll('[data-item]').forEach((el) => el.addEventListener('click', () => {
      const [cat, id] = el.dataset.item.split(':');
      if (!s.owns(el.dataset.item)) { g.audio.sfx('rotateFail'); return; }
      s.equip(cat, id); g.audio.sfx('ui'); g.applyCosmetics(); this.panel('collection');
    }));
    if (name === 'settings') {
      P.querySelectorAll('[data-sw]').forEach((el) => el.addEventListener('click', () => { const k = el.dataset.sw; s.s[k] = !s.s[k]; s.write(); el.classList.toggle('on', s.s[k]); g.audio.sfx('click'); g.applySettings(); }));
      P.querySelectorAll('[data-seg]').forEach((el) => el.querySelectorAll('button').forEach((b) => b.addEventListener('click', () => {
        s.s[el.dataset.seg] = b.dataset.v; s.write(); el.querySelectorAll('button').forEach((x) => x.classList.toggle('on', x === b)); g.audio.sfx('click'); g.applySettings();
      })));
      P.querySelectorAll('[data-range]').forEach((el) => el.addEventListener('input', () => { s.s[el.dataset.range] = parseFloat(el.value); s.write(); g.applySettings(); }));
      P.querySelectorAll('[data-key]').forEach((el) => el.addEventListener('click', () => {
        el.classList.add('wait'); el.textContent = '…';
        g.input.waitKey = (code) => { s.s.keys[el.dataset.key] = code; s.write(); el.classList.remove('wait'); el.textContent = code; };
      }));
      P.querySelector('[data-reset]').addEventListener('click', () => this.modal({
        title: 'Tout effacer ?', text: 'Progression, collection et records seront perdus.',
        buttons: [{ label: 'Effacer', danger: true, act: () => { s.reset(); this.modal(null); this.closePanel(); g.applySettings(); g.applyCosmetics(); g.showMenu(); this.toast('Progression effacée'); } }, { label: 'Annuler', act: () => this.modal(null) }],
      }));
    }
  }

  // ——— HUD ———
  showHud(on, controls) {
    $('hud').classList.toggle('hidden', !on);
    $('goal').classList.toggle('hidden', !on || !$('goal').textContent);
    $('controls').classList.toggle('hidden', !on || controls !== 'buttons');
    $('gesturehint').classList.toggle('hidden', !on || controls !== 'gestures');
    $('controls').classList.toggle('left', !!this.game.save.s.leftHanded);
    document.documentElement.style.setProperty('--btn', this.game.save.s.buttonSize);
  }
  stats(score, level, lines) { $('hScore').textContent = score; $('hLevel').textContent = level; $('hLines').textContent = lines; }
  goal(t) { $('goal').textContent = t || ''; $('goal').classList.toggle('hidden', !t || $('hud').classList.contains('hidden')); }
  popup(text, cls = '') {
    if (!text) return;
    const el = document.createElement('div'); el.className = 'pop ' + cls; el.textContent = text;
    $('popup').appendChild(el); setTimeout(() => el.remove(), 1100);
  }
  toast(text, ms = 2200) {
    const t = $('toast'); t.textContent = text; t.classList.add('on');
    clearTimeout(this._tt); this._tt = setTimeout(() => t.classList.remove('on'), ms);
  }
  setPadEnabled(action, on) { document.querySelectorAll(`#controls [data-a="${action}"]`).forEach((b) => b.classList.toggle('disabled', !on)); }

  // ——— poursuite ———
  showRunHud(on, pads) { $('runhud').classList.toggle('hidden', !on); $('runpad').classList.toggle('hidden', !on || !pads); document.documentElement.style.setProperty('--btn', this.game.save.s.buttonSize); }
  runStats(prog, gap) { $('runProg').textContent = Math.round(prog * 100) + '%'; $('runMeter').style.width = (gap * 100).toFixed(0) + '%'; }
  prompt(t) { $('runPrompt').innerHTML = t ? `<div class="prompt">${t}</div>` : ''; }
  hideBtn(on, cb) {
    const b = $('hidebtn'); b.classList.toggle('hidden', !on);
    b.onclick = on ? () => { this.game.audio.sfx('ui'); cb && cb(); } : null;
  }
  lockerView(on) {
    let v = $('lockerv');
    if (on && !v) {
      v = document.createElement('div'); v.id = 'lockerv';
      v.style.cssText = 'position:fixed;inset:0;z-index:7;pointer-events:none;background:repeating-linear-gradient(180deg,#000 0,#000 7vh,transparent 7vh,transparent 9vh);box-shadow:inset 0 0 25vw #000';
      document.body.appendChild(v);
    } else if (!on && v) v.remove();
  }
  lockPuzzle(p) {
    const el = $('lock');
    if (!p) { el.classList.add('hidden'); el.innerHTML = ''; return; }
    el.classList.remove('hidden');
    el.innerHTML = `<div style="font-size:13px;color:var(--muted)">SERRURE</div><canvas width="200" height="120"></canvas><div style="display:flex;gap:8px;justify-content:center"><button class="btn small" data-r>⟳ Tourner</button><button class="btn primary small" data-i>Insérer</button></div>`;
    const cv = el.querySelector('canvas'), g = cv.getContext('2d');
    const draw = () => {
      g.clearRect(0, 0, 200, 120);
      const cell = 18;
      const drawShape = (rot, ox, oy, fill, stroke) => {
        for (const [c, r] of SHAPES[p.shape][rot]) { g.fillStyle = fill; g.strokeStyle = stroke; g.lineWidth = 2; g.fillRect(ox + c * cell, oy + r * cell, cell - 2, cell - 2); g.strokeRect(ox + c * cell, oy + r * cell, cell - 2, cell - 2); }
      };
      drawShape(0, 20, 30, 'rgba(255,200,40,0.15)', '#ffc928');
      g.fillStyle = '#fff'; g.font = '700 12px Fredoka'; g.fillText('TROU', 24, 22); g.fillText('PIÈCE', 120, 22);
      drawShape(p.rot, 118, 30, '#29d9ff', '#0a5a70');
    };
    draw();
    el.querySelector('[data-r]').addEventListener('pointerdown', (e) => { e.preventDefault(); p.rotate(); draw(); });
    el.querySelector('[data-i]').addEventListener('pointerdown', (e) => { e.preventDefault(); p.insert(); });
  }

  // ——— histoire ———
  modal(cfg) {
    const m = $('modal');
    if (!cfg) { m.classList.add('hidden'); m.innerHTML = ''; return; }
    m.classList.remove('hidden');
    m.innerHTML = `<div class="box"><h2>${cfg.title || ''}</h2>${cfg.text ? `<p>${cfg.text}</p>` : ''}${cfg.buttons.map((b, i) => `<button class="btn ${b.primary ? 'primary' : ''} ${b.danger ? 'danger' : ''}" data-b="${i}">${b.label}</button>`).join('')}</div>`;
    m.querySelectorAll('[data-b]').forEach((el) => el.addEventListener('click', () => { this.game.audio.sfx('ui'); cfg.buttons[+el.dataset.b].act(); }));
  }
  banner(kicker, name, desc) {
    const b = $('banner');
    if (!name) { b.classList.add('hidden'); return; }
    b.innerHTML = `<div class="k">${kicker}</div><div class="n">${name}</div>${desc ? `<div class="d">${desc}</div>` : ''}`;
    b.classList.remove('hidden');
  }
  choice(question, options) {
    return new Promise((res) => {
      const c = $('choice');
      c.innerHTML = `<div class="q">${question}</div>` + options.map((o, i) => `<button class="btn ${o.primary ? 'primary' : ''}" data-c="${i}" style="width:min(84vw,340px)">${o.label}</button>`).join('');
      c.classList.remove('hidden');
      c.querySelectorAll('[data-c]').forEach((el) => el.addEventListener('click', () => { this.game.audio.sfx('ui'); c.classList.add('hidden'); c.innerHTML = ''; res(options[+el.dataset.c].value); }));
    });
  }
  clearChoice() { $('choice').classList.add('hidden'); $('choice').innerHTML = ''; }
  say(text, cls = '') {
    const s = $('story');
    if (!text) { s.innerHTML = ''; return; }
    s.innerHTML = `<div class="say ${cls}">${text}</div>`;
  }
  fake(kind, on) {
    const f = $('fake');
    if (!on) { f.classList.add('hidden'); f.innerHTML = ''; return null; }
    f.classList.remove('hidden');
    if (kind === 'loading') {
      f.innerHTML = `<div style="font-size:22px;letter-spacing:0.3em">CHARGEMENT</div><div class="loading"><i id="fakebar"></i></div><div style="margin-top:12px;color:#555;font-size:12px" id="fakelbl">Chapitre suivant…</div>`;
    } else if (kind === 'menu') {
      f.style.background = 'transparent';
      f.innerHTML = '';
    } else if (kind === 'black') {
      f.style.background = '#000'; f.innerHTML = '';
    } else if (kind === 'gameover') {
      f.style.background = 'rgba(0,0,0,0.75)';
      f.innerHTML = `<div class="box" style="text-align:center"><h2 style="font-size:40px;margin:0">PARTIE TERMINÉE</h2><p style="color:var(--muted)">Score : ${this.game.mode && this.game.mode.engine ? this.game.mode.engine.score : 0}</p><button class="btn primary" id="fakeRetry">Rejouer</button></div>`;
    }
    return f;
  }
  fade(on) { $('fade').classList.toggle('on', !!on); }
}
