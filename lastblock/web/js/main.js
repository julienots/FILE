// THE LAST BLOCK — point d'entrée : écran SuperEssence Studio, boucle de jeu, menus,
// chapitres, pause, jumpscares, progression et sauvegarde.
import { FX } from './core/fx.js';
import { AudioSys } from './core/audio.js';
import { Save } from './core/save.js';
import { Input } from './core/input.js';
import { Scheduler, ABORT } from './core/util.js';
import { UI, ITEMS, UNLOCKS } from './ui/ui.js';
import { TetrisWorld } from './world/board3d.js';
import { MenuScene } from './world/menuScene.js';
import { TetrisMode } from './game/tetrisMode.js';
import { CHAPTER_FNS, RETRY } from './game/story.js';

class Game {
  constructor() {
    this.save = new Save();
    this.audio = new AudioSys();
    this.fx = new FX(document.getElementById('gl'), this.save.s);
    this.sched = new Scheduler();
    this.ui = new UI(this);
    this.input = new Input(this);
    this.world = new TetrisWorld(this.fx, this.save);
    this.menuScene = new MenuScene(this.fx, this.save);
    this.mode = null; this.view = 'menu'; this.paused = false; this.time = 0;
    this.tickers = []; this.customUpdate = null; this.chapter = 0; this.scriptId = 0;
    this.pointer = { x: null, y: null };
    window.addEventListener('pointermove', (e) => { this.pointer.x = e.clientX / window.innerWidth; this.pointer.y = e.clientY / window.innerHeight; });
    window.addEventListener('pointerdown', (e) => { this.pointer.x = e.clientX / window.innerWidth; this.pointer.y = e.clientY / window.innerHeight; });
    window.addEventListener('resize', () => { this.fx.resize(); this.world.fitCamera(); });
    window.__onBack = () => this.back();
    window.__onHostPause = () => { if (this.mode && !this.paused && this.view !== 'menu') this.pause(); this.audio.suspend(); };
    window.__onHostResume = () => this.audio.resume();
    document.addEventListener('visibilitychange', () => { if (document.hidden) window.__onHostPause(); else window.__onHostResume(); });
    window.__game = this; // débogage / tests
    this.applySettings();
    this.fx.setView(this.menuScene.scene, this.menuScene.camera);
    this.last = performance.now();
    requestAnimationFrame((t) => this.loop(t));
  }

  // ——— boucle ———
  loop(now) {
    requestAnimationFrame((t) => this.loop(t));
    let dt = (now - this.last) / 1000; this.last = now;
    if (dt > 0.05) dt = 0.05;
    if (dt <= 0) return;
    dt *= this.timeScale || 1;
    if (!this.paused) {
      this.time += dt;
      this.sched.update(dt);
      if (this.mode && this.mode.update) {
        try { this.mode.update(dt); } catch (e) { console.error(e); }
      }
      for (let i = this.tickers.length - 1; i >= 0; i--) {
        let done = true;
        try { done = this.tickers[i](dt); } catch (e) { console.error(e); }
        if (done) this.tickers.splice(i, 1);
      }
      if (this.customUpdate) this.customUpdate(dt);
      if (this.view === 'world') this.world.update(dt, this.time);
      else if (this.view === 'menu') this.menuScene.update(dt, this.time);
      this.save.data.stats.playSeconds += dt;
    }
    this.fx.render(dt, this.time);
  }
  addTicker(fn) { this.tickers.push(fn); }
  setCustomUpdate(fn) { this.customUpdate = fn; }

  setMode(mode, view) {
    if (this.onModeExit) { const f = this.onModeExit; this.onModeExit = null; try { f(); } catch (e) { console.error(e); } }
    if (this.mode && this.mode.exit) this.mode.exit();
    this.mode = mode; this.view = view || 'world';
    if (mode && mode.enter) mode.enter();
  }

  // ——— écrans ———
  showMenu() {
    this.sched.abort();
    this.scriptId++;
    this.setMode(null, 'menu');
    this.tickers.length = 0; this.customUpdate = null;
    this.paused = false;
    this.fx.clearAll(); this.fx.setDark(0); this.fx.fade(0);
    this.audio.stopLayers(); this.audio.setTempo(1); this.audio.setCorruption(0);
    this.ui.modal(null); this.ui.say(''); this.ui.banner(null); this.ui.clearChoice(); this.ui.fake(null, false);
    this.ui.showHud(false); this.ui.showRunHud(false); this.ui.hideBtn(false); this.ui.lockPuzzle(null); this.ui.lockerView(false);
    this.world.camOverride = null; this.world.corruption = 0; this.world.applyCosmetics();
    this.fx.setView(this.menuScene.scene, this.menuScene.camera);
    const c = this.save.p.healed ? 0 : this.save.p.corruption;
    this.menuScene.setCorruption(c >= 2 ? Math.min(1, (c - 1) / 5) : 0);
    this.audio.playMusic(c >= 3 ? 'menu' : 'menu');
    this.audio.setCorruption(c >= 3 ? Math.min(0.5, c * 0.07) : 0);
    this.audio.setTempo(c >= 5 ? 0.85 : 1);
    this.ui.buildMenu(); this.ui.showMenu(true);
  }
  /** Faux menu (chapitre 5) : s'affiche sans interrompre le script en cours. */
  showFakeMenu(on) {
    this.fakeMenu = on;
    if (on) {
      this.view = 'menu';
      this.fx.setView(this.menuScene.scene, this.menuScene.camera);
      this.menuScene.setCorruption(0);
      this.ui.showHud(false); this.ui.buildMenu(); this.ui.showMenu(true);
      document.getElementById('title').classList.remove('corrupt');
      this.audio.playMusic('menu'); this.audio.setCorruption(0); this.audio.setTempo(1);
    } else {
      this.ui.showMenu(false);
      this.audio.stopMusic(true);
    }
  }

  menuAction(a) {
    if (this.fakeMenu) { this.audio.sfx('glitch'); this.fx.glitch(0.6); return; }
    const s = this.save;
    if (a === 'new') {
      if (s.hasProgress()) this.ui.modal({ title: 'Nouvelle partie', text: 'Recommencer au chapitre 1 ? Ta collection et tes chapitres débloqués sont conservés.', buttons: [{ label: 'Commencer', primary: true, act: () => { this.ui.modal(null); this.startChapter(1); } }, { label: 'Annuler', act: () => this.ui.modal(null) }] });
      else this.startChapter(1);
    } else if (a === 'continue') this.startChapter(s.p.chapter);
    else if (a === 'arcade') this.startArcade();
    else this.ui.panel(a);
  }

  runScript(fn) {
    const id = ++this.scriptId;
    Promise.resolve().then(fn).catch((e) => {
      if (e === ABORT) return;
      if (e === RETRY) {
        if (id !== this.scriptId) return;
        const info = this.resumeInfo; this.resumeInfo = null;
        this.sched.abort();
        this.runScript(() => CHAPTER_FNS[this.chapter](this, info));
        return;
      }
      console.error(e);
    });
  }

  startChapter(n) {
    this.audio.init();
    this.sched.abort();
    this.tickers.length = 0; this.customUpdate = null;
    this.ui.showMenu(false); this.ui.closePanel(); this.ui.modal(null); this.ui.say(''); this.ui.clearChoice(); this.ui.fake(null, false);
    this.fx.clearAll(); this.fx.setDark(0); this.fx.fade(0);
    this.audio.stopLayers(); this.audio.setTempo(1); this.audio.setCorruption(0); this.audio.stopMusic();
    this.paused = false;
    this.chapter = n;
    this.save.startChapter(n);
    // fond pendant le titre du chapitre
    this.setMode(null, 'world');
    this.world.corruption = Math.min(0.6, (n - 1) * 0.12);
    this.world.applyCosmetics();
    this.world.engine = null;
    this.fx.setView(this.world.scene, this.world.camera);
    this.runScript(() => CHAPTER_FNS[n](this));
  }

  startArcade() {
    this.audio.init();
    this.sched.abort();
    this.tickers.length = 0;
    this.ui.showMenu(false); this.ui.closePanel();
    this.fx.clearAll(); this.fx.setDark(0);
    this.audio.stopLayers(); this.audio.setTempo(1); this.audio.setCorruption(0); this.audio.stopMusic();
    this.chapter = 0; this.paused = false;
    const mode = new TetrisMode(this, { name: 'arcade', goal: `Record : ${this.save.data.records.arcade}` });
    this.setMode(mode, 'world');
    this.audio.playMusic('game');
  }

  gameOver(mode) {
    const s = this.save, e = mode.engine;
    let title = 'PARTIE TERMINÉE', text = `Score : ${e.score} · Lignes : ${e.lines}`;
    if (mode.name === 'arcade') {
      if (s.record('arcade', e.score)) text += '<br><b style="color:var(--yellow)">Nouveau record !</b>';
      s.record('arcadeLines', e.lines);
    }
    s.write(); this.checkUnlocks();
    this.ui.modal({
      title, text,
      buttons: [
        { label: mode.name === 'arcade' ? 'Rejouer' : 'Recommencer le chapitre', primary: true, act: () => { this.ui.modal(null); mode.name === 'arcade' ? this.startArcade() : this.startChapter(this.chapter); } },
        { label: 'Menu principal', act: () => { this.ui.modal(null); this.showMenu(); } },
      ],
    });
  }

  // ——— pause ———
  pause() {
    if (this.paused || !this.mode) return;
    if (this.mode.dead || this.mode.won) return;
    this.paused = true;
    this.audio.suspend();
    this.ui.modal({
      title: 'Pause', text: this.chapter ? `Chapitre ${this.chapter}` : 'Mode Arcade',
      buttons: [
        { label: 'Reprendre', primary: true, act: () => this.resume() },
        { label: this.chapter ? 'Recommencer le chapitre' : 'Recommencer', act: () => { this.ui.modal(null); this.audio.resume(); this.chapter ? this.startChapter(this.chapter) : this.startArcade(); } },
        { label: 'Paramètres', act: () => { this.ui.panel('settings'); } },
        { label: 'Menu principal', act: () => { this.ui.modal(null); this.audio.resume(); if (this.mode && this.mode.name === 'arcade') { this.save.record('arcade', this.mode.engine.score); this.checkUnlocks(); } this.showMenu(); } },
      ],
    });
  }
  resume() { this.ui.modal(null); this.ui.closePanel(); this.paused = false; this.audio.resume(); }
  back() {
    if (this.ui.panelOpen) { this.ui.closePanel(); return 'ok'; }
    if (this.paused) { this.resume(); return 'ok'; }
    if (this.view === 'menu' && !this.mode && !this.fakeMenu) { return document.getElementById('modal').classList.contains('hidden') ? 'exit' : (this.ui.modal(null), 'ok'); }
    this.pause(); return 'ok';
  }

  // ——— réglages / collection ———
  applySettings() {
    const s = this.save.s;
    this.audio.setVolumes(s.music, s.sfx);
    this.audio.soft = s.softScares;
    this.audio.pack = this.save.data.collection.equipped.sound || 'classic';
    this.fx.applyQuality();
    if (this.world) this.world.fitCamera();
    if (this.mode && this.mode.inputKind === 'tetris') { this.ui.showHud(true, s.controls); this.world.setLayout(this.world.layout.top, s.controls === 'buttons' ? 0.26 : 0.07); }
  }
  applyCosmetics() { this.world.applyCosmetics(); this.audio.pack = this.save.data.collection.equipped.sound || 'classic'; }
  checkUnlocks() {
    const s = this.save; const got = [];
    for (const [key, u] of Object.entries(UNLOCKS)) if (!s.owns(key) && u.ok(s)) { s.own(key); got.push(key); }
    if (got.length) {
      const names = got.map((k) => { const [cat, id] = k.split(':'); const it = ITEMS.find((i) => i.cat === cat && i.id === id); return it ? it.name : k; });
      setTimeout(() => { this.ui.toast('🎁 Débloqué : ' + names.join(', '), 3200); this.audio.sfx('unlock'); }, 600);
    }
    s.write();
  }

  // ——— horreur ———
  say(text, { dur = 2.5, cls = '' } = {}) {
    this.ui.say(text, cls);
    if (text && !cls.includes('white')) { this.audio.sfx('glitch', { vol: 0.5 }); this.fx.glitch(0.35); }
    clearTimeout(this._sayT);
    if (text) this._sayT = setTimeout(() => this.ui.say(''), dur * 1000);
  }
  jumpscare({ intensity = 1 } = {}) {
    const soft = this.save.s.softScares;
    const k = soft ? 0.35 : intensity;
    this.audio.sfx('jumpscare', { vol: soft ? 0.5 : 1 });
    this.fx.shake(1.2 * k, 0.9); this.fx.glitch(1); this.fx.aberration(0.06 * k); this.fx.warp(1.2 * k);
    if (!soft) this.fx.flash(0xff0000, 0.6 * k);
    this.input.haptic(500, 255);
  }
  /** Visage 2D plein écran qui surgit (faux écrans). */
  scare2D(intensity = 1) {
    const soft = this.save.s.softScares;
    const cv = document.getElementById('scare');
    const W = cv.width = window.innerWidth, H = cv.height = window.innerHeight;
    const g = cv.getContext('2d');
    this.jumpscare({ intensity });
    const dur = soft ? 450 : 900;
    const t0 = performance.now();
    return new Promise((res) => {
      const draw = () => {
        const k = (performance.now() - t0) / dur;
        g.clearRect(0, 0, W, H);
        if (k >= 1) { res(); return; }
        const s = Math.min(W, H) * (soft ? 0.5 : 0.65 + k * 0.9);
        const jx = (Math.random() - 0.5) * 30 * (1 - k), jy = (Math.random() - 0.5) * 30 * (1 - k);
        g.fillStyle = `rgba(0,0,0,${soft ? 0.6 : 0.92})`; g.fillRect(0, 0, W, H);
        g.save(); g.translate(W / 2 + jx, H / 2 + jy);
        g.fillStyle = '#07070a'; g.strokeStyle = '#ff1a2a'; g.lineWidth = s * 0.02; g.shadowColor = '#ff0000'; g.shadowBlur = 30;
        const r = s * 0.12; const x0 = -s / 2, y0 = -s / 2;
        g.beginPath(); g.moveTo(x0 + r, y0); g.arcTo(x0 + s, y0, x0 + s, y0 + s, r); g.arcTo(x0 + s, y0 + s, x0, y0 + s, r); g.arcTo(x0, y0 + s, x0, y0, r); g.arcTo(x0, y0, x0 + s, y0, r); g.closePath(); g.fill(); g.stroke();
        g.fillStyle = '#ff2030'; g.shadowBlur = 50;
        g.fillRect(-s * 0.32, -s * 0.2, s * 0.2, s * 0.14 * (soft ? 0.5 : 1)); g.fillRect(s * 0.12, -s * 0.2, s * 0.2, s * 0.14 * (soft ? 0.5 : 1));
        g.fillStyle = '#ffe0d0'; g.shadowBlur = 0; g.fillRect(-s * 0.25, -s * 0.17, s * 0.05, s * 0.05); g.fillRect(s * 0.19, -s * 0.17, s * 0.05, s * 0.05);
        if (!soft) { g.fillStyle = '#000'; g.fillRect(-s * 0.28, s * 0.08, s * 0.56, s * 0.28); }
        g.strokeStyle = '#ff2030'; g.lineWidth = s * 0.025; g.shadowBlur = 30; g.beginPath();
        for (let i = 0; i <= 8; i++) g.lineTo(-s * 0.28 + i * s * 0.07, s * (soft ? 0.18 : 0.08) + (i % 2 ? s * 0.06 : 0));
        g.stroke();
        if (!soft) { g.beginPath(); for (let i = 0; i <= 8; i++) g.lineTo(-s * 0.28 + i * s * 0.07, s * 0.36 - (i % 2 ? s * 0.06 : 0)); g.stroke(); }
        g.restore();
        requestAnimationFrame(draw);
      };
      draw();
    });
  }
}

// ——— écran SuperEssence Studio ———
function splash(done) {
  const logo = document.getElementById('seLogo');
  const word = 'SuperEssence';
  logo.innerHTML = [...word].map((c, i) => `<span class="ch ${i >= 5 ? 'y' : ''}">${c}</span>`).join('') + '<span class="se-dot"></span><span class="shine"></span>';
  const chars = logo.querySelectorAll('.ch');
  let finished = false;
  const finish = () => {
    if (finished) return; finished = true;
    const sp = document.getElementById('splash');
    sp.classList.add('out');
    setTimeout(() => { sp.remove(); done(); }, 700);
  };
  const go = () => {
    chars.forEach((c, i) => setTimeout(() => c.classList.add('in'), 120 + i * 70));
    const tDot = 120 + chars.length * 70 + 150;
    setTimeout(() => logo.querySelector('.se-dot').classList.add('in'), tDot);
    setTimeout(() => { logo.querySelector('.shine').classList.add('go'); window.__game && window.__game.audio.ready && window.__game.audio.sfx('ping', { f: 1320 }); }, tDot + 900);
    setTimeout(() => document.getElementById('seSub').classList.add('in'), tDot + 1000);
    setTimeout(finish, tDot + 2600);
  };
  document.getElementById('splash').addEventListener('pointerdown', () => { window.__game && window.__game.audio.init(); finish(); });
  if (document.fonts && document.fonts.load) document.fonts.load('700 40px Fredoka').then(go, go); else go();
}

const game = new Game();
splash(() => {
  if (!game.save.data.seenWarning) {
    const w = document.getElementById('warn'); w.classList.remove('hidden');
    document.getElementById('warnOk').addEventListener('click', () => { game.audio.init(); game.save.data.seenWarning = true; game.save.write(); w.classList.add('hidden'); game.showMenu(); });
  } else game.showMenu();
});
