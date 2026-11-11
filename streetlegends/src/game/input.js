// Commandes : tactile (4 configurations), clavier, manette. Produit un état unifié pour la voiture du joueur.
import { clamp } from '../core/util.js';

export const LAYOUTS = {
  buttons: { name: 'Flèches', desc: 'Flèches gauche / droite et pédales.' },
  wheel: { name: 'Volant', desc: 'Volant virtuel à faire tourner du pouce.' },
  stick: { name: 'Joystick', desc: 'Joystick analogique pour la direction.' },
  tilt: { name: 'Inclinaison', desc: 'Inclinez le téléphone pour tourner.' },
};
export const DEFAULT_CONTROLS = { layout: 'buttons', sens: 1, scale: 1, opacity: 0.75, swap: false, autoGas: false, haptics: true, tiltSens: 1 };

export class Input {
  constructor(root, settings, onAction) {
    this.root = root; this.s = settings; this.onAction = onAction;
    this.state = { steer: 0, throttle: 0, brake: 0, handbrake: false, nitro: false, up: false, down: false };
    this.keys = new Set();
    this.t = { left: false, right: false, gas: false, brake: false, hb: false, nitro: false, analog: null };
    this.tilt = 0; this.enabled = false; this.dragMode = false; this.touchSeen = false; this.padSeen = false;
    this._kd = (e) => this.key(e, true); this._ku = (e) => this.key(e, false);
    addEventListener('keydown', this._kd); addEventListener('keyup', this._ku);
    addEventListener('blur', () => { this.keys.clear(); this.releaseAll(); });
    this._tilt = (e) => {
      const land = (screen.orientation && screen.orientation.angle) || window.orientation || 0;
      const g = land === 90 ? e.beta : land === -90 || land === 270 ? -e.beta : e.gamma;
      if (g != null) this.tilt = clamp((g / 28) * this.s.tiltSens, -1, 1);
    };
    this.build();
  }
  key(e, down) {
    if (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA')) return;
    const c = e.code;
    if (down) {
      if (this.keys.has(c)) return;
      this.keys.add(c);
      const A = { KeyC: 'camera', Escape: 'pause', KeyP: 'pause', KeyR: 'reset', KeyE: 'interact', Enter: 'interact', KeyM: 'map', KeyH: 'horn', KeyL: 'lights', KeyG: 'shiftUp', KeyF: 'shiftDown', KeyB: 'lookBack' }[c];
      if (A && this.enabled) { this.onAction(A, true); e.preventDefault(); }
      if (c === 'Space' && this.dragMode && this.enabled) this.onAction('shiftUp', true);
    } else {
      this.keys.delete(c);
      if (c === 'KeyB' && this.enabled) this.onAction('lookBack', false);
    }
    if (this.enabled && (c.startsWith('Arrow') || c === 'Space')) e.preventDefault();
  }
  releaseAll() { Object.assign(this.t, { left: false, right: false, gas: false, brake: false, hb: false, nitro: false, analog: null }); for (const el of this.root.querySelectorAll('.on')) el.classList.remove('on'); }
  setEnabled(v) { this.enabled = v; this.root.classList.toggle('hidden', !v || !this.touchSeen); if (!v) this.releaseAll(); }
  showTouch() { this.touchSeen = true; this.root.classList.toggle('hidden', !this.enabled); }
  setDragMode(v) { this.dragMode = v; this.root.classList.toggle('drag', v); }
  haptic(ms) { if (this.s.haptics && navigator.vibrate && this.touchSeen) { try { navigator.vibrate(ms); } catch (e) { /* non pris en charge */ } } }
  // (Re)construit les commandes tactiles selon la configuration choisie.
  build() {
    const s = this.s, r = this.root;
    r.innerHTML = '';
    r.className = 'touch ' + s.layout + (s.swap ? ' swap' : '') + (this.enabled && this.touchSeen ? '' : ' hidden') + (this.dragMode ? ' drag' : '');
    r.style.setProperty('--ts', s.scale); r.style.setProperty('--to', s.opacity);
    const btn = (cls, label, key, action) => {
      const b = document.createElement('div');
      b.className = 'tbtn ' + cls; b.innerHTML = label;
      const set = (v, e) => {
        e.preventDefault(); this.showTouch();
        if (key) this.t[key] = v;
        b.classList.toggle('on', v);
        if (v && action) this.onAction(action, true);
        if (!v && action === 'lookBack') this.onAction(action, false);
        if (v) this.haptic(8);
      };
      b.addEventListener('pointerdown', (e) => { b.setPointerCapture(e.pointerId); set(true, e); });
      b.addEventListener('pointerup', (e) => set(false, e)); b.addEventListener('pointercancel', (e) => set(false, e));
      b.addEventListener('contextmenu', (e) => e.preventDefault());
      r.appendChild(b);
      return b;
    };
    if (s.layout === 'buttons') { btn('left', '◀', 'left'); btn('right', '▶', 'right'); }
    else if (s.layout === 'wheel' || s.layout === 'stick') {
      const w = document.createElement('div');
      w.className = s.layout === 'wheel' ? 'twheel' : 'tstick';
      w.innerHTML = s.layout === 'wheel' ? '<div class="rim"><i></i><i></i><i></i><b></b></div>' : '<div class="knob"></div>';
      const inner = w.firstChild;
      let id = null, a0 = 0, cur = 0;
      const ang = (e) => { const b = w.getBoundingClientRect(); return Math.atan2(e.clientX - (b.left + b.width / 2), -(e.clientY - (b.top + b.height / 2))); };
      w.addEventListener('pointerdown', (e) => { e.preventDefault(); this.showTouch(); id = e.pointerId; w.setPointerCapture(id); a0 = ang(e) - cur; move(e); });
      const move = (e) => {
        if (e.pointerId !== id) return;
        const b = w.getBoundingClientRect();
        if (s.layout === 'wheel') {
          let a = ang(e) - a0; while (a > Math.PI) a -= 2 * Math.PI; while (a < -Math.PI) a += 2 * Math.PI;
          cur = clamp(a, -1.9, 1.9); inner.style.transform = `rotate(${cur}rad)`; this.t.analog = clamp((cur / 1.5) * s.sens, -1, 1);
        } else {
          const dx = clamp((e.clientX - (b.left + b.width / 2)) / (b.width * 0.36), -1, 1);
          inner.style.transform = `translateX(${dx * b.width * 0.3}px)`; this.t.analog = clamp(dx * s.sens, -1, 1);
        }
      };
      const up = (e) => { if (e.pointerId !== id) return; id = null; cur = 0; this.t.analog = null; inner.style.transform = ''; };
      w.addEventListener('pointermove', move); w.addEventListener('pointerup', up); w.addEventListener('pointercancel', up);
      r.appendChild(w);
    }
    btn('gas', '<span>GAZ</span>', 'gas'); btn('brake', '<span>FREIN</span>', 'brake');
    btn('hb', 'Ⓟ', 'hb'); btn('nitro', 'N₂O', 'nitro');
    btn('shift', '▲<small>RAPPORT</small>', null, 'shiftUp');
    btn('cam', '🎥', null, 'camera'); btn('back', '⟲', null, 'lookBack'); btn('rst', '↺', null, 'reset');
    if (s.layout === 'tilt') this.enableTilt(); else removeEventListener('deviceorientation', this._tilt);
  }
  enableTilt() {
    const start = () => addEventListener('deviceorientation', this._tilt);
    // iOS exige une autorisation explicite déclenchée par un geste de l'utilisateur
    if (typeof DeviceOrientationEvent !== 'undefined' && typeof DeviceOrientationEvent.requestPermission === 'function') DeviceOrientationEvent.requestPermission().then((p) => { if (p === 'granted') start(); }).catch(() => {});
    else start();
  }
  update(dt) {
    const k = this.keys, st = this.state, t = this.t, s = this.s;
    let steerT = 0, analog = false, gas = 0, brake = 0, hb = false, nitro = false;
    if (k.has('ArrowLeft') || k.has('KeyA') || t.left) steerT -= 1;
    if (k.has('ArrowRight') || k.has('KeyD') || t.right) steerT += 1;
    if (t.analog !== null) { steerT = t.analog; analog = true; }
    if (s.layout === 'tilt' && this.touchSeen && Math.abs(this.tilt) > 0.02) { steerT = this.tilt; analog = true; }
    if (k.has('ArrowUp') || k.has('KeyW') || t.gas) gas = 1;
    if (k.has('ArrowDown') || k.has('KeyS') || t.brake) brake = 1;
    if (k.has('Space') || t.hb) hb = !this.dragMode || t.hb;
    if (k.has('ShiftLeft') || k.has('ShiftRight') || k.has('KeyN') || t.nitro) nitro = true;
    const gp = navigator.getGamepads ? [...navigator.getGamepads()].find((g) => g && g.connected) : null;
    if (gp) {
      const ax = gp.axes[0] || 0;
      if (Math.abs(ax) > 0.08) { steerT = Math.sign(ax) * Math.pow(Math.abs(ax), 1.6) * s.sens; analog = true; }
      const b = (i) => (gp.buttons[i] ? gp.buttons[i].value : 0);
      gas = Math.max(gas, b(7)); brake = Math.max(brake, b(6));
      if (b(0) > 0.5) hb = true;
      if (b(1) > 0.5 || b(2) > 0.5) nitro = true;
      for (const [i, a] of [[3, 'camera'], [9, 'pause'], [5, 'shiftUp'], [4, 'shiftDown'], [8, 'map']]) { const on = b(i) > 0.5; if (on && !this['gp' + i] && this.enabled) this.onAction(a, true); this['gp' + i] = on; }
    }
    if (s.autoGas && this.touchSeen && !brake && this.enabled && !this.dragMode) gas = 1;
    steerT = clamp(steerT, -1, 1);
    if (analog) st.steer = steerT;
    else { const rate = (steerT === 0 ? 5.5 : 3.2 * s.sens + (Math.sign(steerT) !== Math.sign(st.steer) ? 4 : 0)) * dt; st.steer += clamp(steerT - st.steer, -rate, rate); }
    if (!this.enabled) { st.steer = 0; gas = 0; brake = 0; hb = false; nitro = false; }
    st.throttle = gas; st.brake = brake; st.handbrake = hb; st.nitro = nitro;
    return st;
  }
}
