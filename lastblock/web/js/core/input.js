// Entrées : clavier (touches personnalisables), boutons tactiles (maintien), gestes (glisser/toucher).
// Tout est converti en actions nommées envoyées au mode de jeu actif : onAction(nom, appuyé).

const RUNNER_KEYS = { ArrowLeft: 'left', ArrowRight: 'right', ArrowUp: 'jump', ArrowDown: 'slide', KeyA: 'left', KeyD: 'right', KeyW: 'jump', KeyS: 'slide', Space: 'jump', KeyE: 'hide', Enter: 'hide' };

export class Input {
  constructor(game) {
    this.game = game;
    this.waitKey = null; // remappage en cours
    window.addEventListener('keydown', (e) => this._key(e, true));
    window.addEventListener('keyup', (e) => this._key(e, false));
    const bindPads = (root) => {
      root.querySelectorAll('[data-a]').forEach((b) => {
        const a = b.dataset.a;
        const down = (e) => { e.preventDefault(); b.setPointerCapture && b.setPointerCapture(e.pointerId); b.classList.add('down'); this.game.audio.init(); this.emit(a, true); this.haptic(8); };
        const up = (e) => { e.preventDefault(); if (!b.classList.contains('down')) return; b.classList.remove('down'); this.emit(a, false); };
        b.addEventListener('pointerdown', down);
        b.addEventListener('pointerup', up);
        b.addEventListener('pointercancel', up);
        b.addEventListener('lostpointercapture', up);
      });
    };
    bindPads(document.getElementById('controls'));
    bindPads(document.getElementById('runpad'));
    // gestes sur le canvas
    const cv = document.getElementById('gl');
    this.g = null;
    cv.addEventListener('pointerdown', (e) => this._gStart(e));
    window.addEventListener('pointermove', (e) => this._gMove(e));
    window.addEventListener('pointerup', (e) => this._gEnd(e));
    window.addEventListener('pointercancel', (e) => this._gEnd(e));
  }

  emit(action, down) {
    const m = this.game.mode;
    if (m && m.onAction) m.onAction(action, down);
  }

  haptic(ms, amp = 80) {
    if (!this.game.save.s.haptics) return;
    try {
      if (window.AndroidHost && window.AndroidHost.vibrate) window.AndroidHost.vibrate(ms, amp);
      else if (navigator.vibrate) navigator.vibrate(ms);
    } catch (e) { /* ignore */ }
  }

  _key(e, down) {
    if (this.waitKey && down) {
      e.preventDefault();
      const cb = this.waitKey; this.waitKey = null; cb(e.code); return;
    }
    this.game.audio.init();
    const m = this.game.mode;
    if (e.code === 'Escape' || e.code === this.game.save.s.keys.pause) {
      if (down) { e.preventDefault(); this.game.back(); }
      return;
    }
    if (!m) return;
    let action = null;
    if (m.inputKind === 'runner') action = RUNNER_KEYS[e.code];
    else {
      const k = this.game.save.s.keys;
      for (const name of Object.keys(k)) if (k[name] === e.code) action = name;
      if (!action && e.code === 'KeyX') action = 'rotCW';
      if (!action && (e.code === 'ShiftLeft' || e.code === 'ShiftRight')) action = 'hold';
    }
    if (!action) { if (down && m.onKeyAny) m.onKeyAny(e.code); return; }
    e.preventDefault();
    if (e.repeat) return;
    this.emit(action, down);
  }

  _gStart(e) {
    this.game.audio.init();
    const m = this.game.mode;
    if (!m) return;
    if (m.onTap && m.inputKind !== 'runner' && m.inputKind !== 'tetris') { m.onTap(e); return; }
    this.g = { id: e.pointerId, x: e.clientX, y: e.clientY, sx: e.clientX, sy: e.clientY, t: performance.now(), moved: false, fired: false, softSteps: 0 };
  }
  _gMove(e) {
    const g = this.g; if (!g || g.id !== e.pointerId) return;
    const m = this.game.mode; if (!m) return;
    const step = 30 * Math.max(0.6, 1 / this.game.save.s.sensitivity);
    if (m.inputKind === 'tetris' && this.game.save.s.controls === 'gestures') {
      const dx = e.clientX - g.x;
      while (Math.abs(e.clientX - g.x) >= step) {
        const dir = Math.sign(e.clientX - g.x);
        this.emit(dir > 0 ? 'right' : 'left', true); this.emit(dir > 0 ? 'right' : 'left', false);
        g.x += dir * step; g.moved = true;
      }
      void dx;
      const dy = e.clientY - g.y;
      const vy = (e.clientY - g.sy) / Math.max(1, performance.now() - g.t);
      if (dy > step && vy < 1.0) { this.emit('softStep', true); g.y += step; g.moved = true; }
    } else if (m.inputKind === 'runner' && !g.fired) {
      const dx = e.clientX - g.sx, dy = e.clientY - g.sy;
      if (Math.hypot(dx, dy) > 36) {
        g.fired = true;
        const a = Math.abs(dx) > Math.abs(dy) ? (dx > 0 ? 'right' : 'left') : (dy > 0 ? 'slide' : 'jump');
        this.emit(a, true); this.emit(a, false);
      }
    }
  }
  _gEnd(e) {
    const g = this.g; if (!g || g.id !== e.pointerId) return;
    this.g = null;
    const m = this.game.mode; if (!m) return;
    const dt = performance.now() - g.t;
    const dx = e.clientX - g.sx, dy = e.clientY - g.sy;
    if (m.inputKind === 'tetris' && this.game.save.s.controls === 'gestures') {
      const vy = dy / Math.max(1, dt);
      if (dy > 60 && vy > 0.9) { this.emit('hard', true); this.emit('hard', false); return; }
      if (dy < -60 && Math.abs(dx) < 60) { this.emit('hold', true); this.emit('hold', false); return; }
      if (!g.moved && dt < 300 && Math.hypot(dx, dy) < 14) {
        const a = e.clientX < window.innerWidth * 0.35 ? 'rotCCW' : 'rotCW';
        this.emit(a, true); this.emit(a, false);
      }
    } else if (m.inputKind === 'runner' && !g.fired) {
      if (Math.hypot(dx, dy) < 14 && dt < 300) { this.emit('tap', true); }
    }
  }
}
