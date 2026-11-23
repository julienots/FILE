// Mode « plateau » : relie le moteur logique, le monde 3D, les contrôles (avec répétition
// DAS/ARR), le HUD, les sons, les vibrations et les crochets des chapitres (anomalies, puzzles, boss).
import { Tetris, W, H, VISIBLE } from './tetris.js';
import { cellX, cellY } from '../world/board3d.js';
import { rand, pick } from '../core/util.js';

const DAS = 0.16, ARR = 0.045, SOFT_ARR = 0.035;

export class TetrisMode {
  constructor(game, opts = {}) {
    this.game = game;
    this.world = game.world;
    this.inputKind = 'tetris';
    this.opts = opts;
    this.hooks = opts.hooks || {};
    this.name = opts.name || 'arcade';
    this.engine = new Tetris({ startLevel: opts.startLevel || 1, seed: opts.seed, listener: (ev, d) => this._event(ev, d) });
    this.held = {}; this.repeat = {};
    this.controlsEnabled = true; this.lockedActions = new Set();
    this.statKey = '';
    this.gameOverHandled = false;
  }

  enter() {
    const g = this.game;
    g.fx.setView(this.world.scene, this.world.camera);
    this.world.applyCosmetics();
    this.world.engine = this.engine;
    this.world.camOverride = null; this.world.hideGhost = false;
    this.world.setTargets([]); this.world.clearWord(); this.world.showWatcher(0); this.world.resetGlass();
    this.world.cabinet.visible = true; this.world.board.visible = true;
    this.world.bezels.forEach((b) => (b.visible = true));
    if (this.opts.noGlass) this.world.glass.visible = false;
    this.world.corruption = this.opts.corruption || 0;
    g.ui.showHud(true, this.game.save.s.controls);
    this.world.setLayout(0.1, this.game.save.s.controls === 'buttons' ? 0.26 : 0.07);
    this.setGoal(this.opts.goal || '');
    this.engine.spawn();
    if (!this.opts.noMusic) g.audio.playMusic(this.opts.music || 'game');
    this.hooks.onStart && this.hooks.onStart(this);
  }
  exit() {
    this.game.ui.showHud(false);
    this.world.camOverride = null;
  }

  setGoal(text) { this.game.ui.goal(text); }

  // ——— entrées ———
  onAction(a, down) {
    if (this.game.paused) return;
    if (a === 'softStep') { if (this._can('soft')) this.engine.softDrop(); return; }
    if (!down) {
      this.held[a] = false;
      if (a === 'soft') this.engine.softDropping = false;
      return;
    }
    if (!this.controlsEnabled || !this._can(a)) { if (down) this.game.audio.sfx('rotateFail'); return; }
    this.held[a] = true; this.repeat[a] = -DAS;
    this._do(a);
  }
  _can(a) { return !this.lockedActions.has(a) && !this.lockedActions.has('all'); }
  _do(a) {
    const e = this.engine;
    const inv = this.invertControls;
    switch (a) {
      case 'left': e.move(inv ? 1 : -1); break;
      case 'right': e.move(inv ? -1 : 1); break;
      case 'rotCW': e.rotate(1); break;
      case 'rotCCW': e.rotate(-1); break;
      case 'soft': e.softDropping = true; e.softDrop(); break;
      case 'hard': e.hardDrop(); break;
      case 'hold': if (e.holdPiece()) this.game.audio.sfx('hold'); break;
    }
  }

  // ——— événements du moteur ———
  _event(ev, d) {
    const A = this.game.audio, w = this.world, inp = this.game.input;
    switch (ev) {
      case 'move': A.sfx('move'); w.sway.x += d * 0.25; break;
      case 'rotate': A.sfx('rotate'); break;
      case 'rotateFail': A.sfx('rotateFail'); if (d === 'blocked') { w.particles.sparks(cellX(this.engine.active?.x + 1 || 4), cellY(this.engine.active?.y - 1 || 10), 0.5, 0xff2030, 6, 4); } break;
      case 'soft': if (Math.random() < 0.3) A.sfx('soft'); break;
      case 'hard': A.sfx('hard'); inp.haptic(25, 160); w.onHardDrop(d.from, d.piece, this.engine); w.sway.y -= 0.4; this._hardFlag = true; break;
      case 'land': A.sfx('lock', { vol: 0.5 }); break;
      case 'lock':
        if (!this._hardFlag) A.sfx('lock');
        w.onLock(d.cells, this._hardFlag); this._hardFlag = false;
        this.game.save.data.stats.piecesTotal++;
        this.hooks.onLock && this.hooks.onLock(d, this);
        break;
      case 'clear': {
        w.onClear(d);
        const n = d.n;
        if (n >= 4) { A.sfx('tetris'); this.game.ui.popup('LAST BLOCK !', 'gold'); this.game.save.data.stats.tetrises++; inp.haptic(60, 255); }
        else { A.sfx('clear', { n }); this.game.ui.popup(['', 'SIMPLE', 'DOUBLE', 'TRIPLE'][n]); inp.haptic(20 + n * 10, 120); }
        if (d.combo > 0) { A.sfx('combo', { n: d.combo }); setTimeout(() => this.game.ui.popup(`COMBO ×${d.combo + 1}`, 'gold'), 250); }
        if (d.b2b) setTimeout(() => this.game.ui.popup('ENCHAÎNEMENT !', 'gold'), 450);
        this.game.save.data.stats.linesTotal += n;
        this.hooks.onClear && this.hooks.onClear(d, this);
        break;
      }
      case 'level': A.sfx('levelup'); this.game.ui.popup(`NIVEAU ${d}`); break;
      case 'spawn': this.hooks.onSpawn && this.hooks.onSpawn(d, this); break;
      case 'gameover':
        if (this.gameOverHandled) return;
        this.gameOverHandled = true;
        A.sfx('gameover');
        setTimeout(() => { if (this.hooks.onGameOver) this.hooks.onGameOver(this); else this.game.gameOver(this); }, 50);
        break;
    }
  }

  update(dt) {
    const e = this.engine;
    // répétition des touches maintenues
    for (const a of ['left', 'right', 'soft']) {
      if (!this.held[a] || !this.controlsEnabled || !this._can(a)) continue;
      this.repeat[a] += dt;
      const r = a === 'soft' ? SOFT_ARR : ARR;
      while (this.repeat[a] >= r) { this.repeat[a] -= r; if (a !== 'soft') this._do(a); }
    }
    e.update(dt);
    this.hooks.onUpdate && this.hooks.onUpdate(dt, this);
    this.world.sync(e, dt);
    const key = `${e.score}|${e.level}|${e.lines}`;
    if (key !== this.statKey) { this.statKey = key; this.game.ui.stats(this.displayScore !== undefined ? this.displayScore : e.score, e.level, e.lines); }
  }

  // ——— outils pour les chapitres ———
  activeCenter() {
    const p = this.engine.active; if (!p) return null;
    const cells = this.engine.cells(p);
    const x = cells.reduce((s, c) => s + c[0], 0) / 4, y = cells.reduce((s, c) => s + c[1], 0) / 4;
    return { x, y, wx: cellX(x), wy: cellY(y) };
  }
  /** Bloc qui change de direction tout seul */
  nudge() { const p = this.engine.active; if (!p) return false; const d = pick([-1, 1]); if (!this.engine.move(d)) this.engine.move(-d); this.game.audio.sfx('glitch', { vol: 0.4 }); this.game.fx.glitch(0.25); return true; }
  /** Fait apparaître un visage furtif sur un bloc posé */
  flashFace(kind = 'stare', dur = 0.35) {
    const cells = [];
    for (let y = 0; y < VISIBLE; y++) for (let x = 0; x < W; x++) if (this.engine.grid[y][x]) cells.push(this.engine.grid[y][x]);
    const target = cells.length ? pick(cells) : null;
    if (target) { target.face = kind; setTimeout(() => { delete target.face; }, dur * 1000); return true; }
    const p = this.engine.active; if (p) { p.face = kind; setTimeout(() => { delete p.face; }, dur * 1000); return true; }
    return false;
  }
}
