import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { getPlanet } from '../data/planets.js';

// Audio procédural (B8) — WebAudio pur, aucun fichier. Musique générative adaptative (ambiance par lieu/biome, couche combat, boss)
// + SFX synthétisés. Volumes = State.settings.music / sfx. Démarre au premier geste (politique d'autoplay).
// Moods: gamme (demi-tons), racine MIDI, tempo, instruments. La couche combat/boss remplace l'ambiance à la mesure suivante.
const SC = { maj: [0, 2, 4, 7, 9], min: [0, 3, 5, 7, 10], lyd: [0, 2, 4, 6, 7, 9, 11], phr: [0, 1, 4, 5, 7, 8, 10], dor: [0, 2, 3, 5, 7, 9, 10], hex: [0, 2, 4, 6, 8, 10] };
const MOODS = {
  title:    { sc: 'lyd', root: 50, bpm: 64, pad: .5, arp: 'slow', bell: 1, bass: 0, drums: 0, prog: [0, 4, 5, 3] },
  space:    { sc: 'maj', root: 50, bpm: 70, pad: .55, arp: 'slow', bell: .7, bass: .3, drums: 0, prog: [0, 5, 3, 4] },
  meadow:   { sc: 'maj', root: 60, bpm: 98, pad: .3, arp: 'bounce', bell: .4, bass: .5, drums: 1, prog: [0, 3, 4, 3] },
  volcanic: { sc: 'phr', root: 45, bpm: 102, pad: .35, arp: 'run', bell: 0, bass: .7, drums: 2, prog: [0, 1, 0, 5] },
  ice:      { sc: 'lyd', root: 64, bpm: 78, pad: .45, arp: 'slow', bell: 1, bass: .2, drums: 0, prog: [0, 2, 4, 2] },
  jungle:   { sc: 'dor', root: 57, bpm: 104, pad: .3, arp: 'bounce', bell: .5, bass: .5, drums: 1.5, prog: [0, 3, 0, 4] },
  desert:   { sc: 'phr', root: 52, bpm: 90, pad: .3, arp: 'run', bell: .3, bass: .5, drums: 1, prog: [0, 1, 3, 1] },
  dark:     { sc: 'min', root: 45, bpm: 74, pad: .5, arp: 'slow', bell: .3, bass: .5, drums: .5, prog: [0, 5, 3, 6] },
  void:     { sc: 'hex', root: 41, bpm: 60, pad: .6, arp: 'slow', bell: .5, bass: .4, drums: 0, prog: [0, 3, 1, 4] },
  divine:   { sc: 'lyd', root: 62, bpm: 84, pad: .5, arp: 'bounce', bell: 1, bass: .4, drums: .5, prog: [0, 4, 6, 4] },
  combat:   { sc: 'min', root: 48, bpm: 132, pad: .25, arp: 'run', bell: 0, bass: .8, drums: 3, prog: [0, 5, 3, 4] },
  boss:     { sc: 'phr', root: 40, bpm: 144, pad: .4, arp: 'run', bell: 0, bass: 1, drums: 4, prog: [0, 1, 5, 4] },
};
const BIOME_MOOD = { meadow: 'meadow', volcanic: 'volcanic', ash: 'dark', ice: 'ice', crystal: 'ice', jungle: 'jungle', mushroom: 'jungle', ocean: 'meadow', desert: 'desert',
  lab: 'dark', storm: 'dark', fortress: 'combat', graveyard: 'dark', void: 'void', divine: 'divine', eden: 'divine' };
const mtof = m => 440 * Math.pow(2, (m - 69) / 12);

export const Audio = {
  ctx: null, master: null, music: null, sfx: null, noise: null, mood: 'title', want: 'title', base: 'title', step: 0, next: 0, engaged: new Set(), boss: false, muted: false,
  init() {
    if (this.ctx) return; const AC = globalThis.AudioContext || globalThis.webkitAudioContext; if (!AC) return;
    const c = this.ctx = new AC(); this.master = c.createGain(); this.master.connect(c.destination);
    const comp = c.createDynamicsCompressor(); comp.threshold.value = -14; comp.ratio.value = 4; comp.connect(this.master);
    this.music = c.createGain(); this.sfx = c.createGain(); this.music.connect(comp); this.sfx.connect(comp);
    // écho doux (delay + feedback) pour l'espace
    this.delay = c.createDelay(1); this.delay.delayTime.value = .38; const fb = c.createGain(); fb.gain.value = .32; const wet = c.createGain(); wet.gain.value = .35;
    this.delay.connect(fb); fb.connect(this.delay); this.delay.connect(wet); wet.connect(this.music);
    const len = c.sampleRate, buf = c.createBuffer(1, len, c.sampleRate), d = buf.getChannelData(0); for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1; this.noise = buf;
    this.volumes(); this.next = c.currentTime + .1;
    setInterval(() => this.schedule(), 60);
  },
  volumes() { if (!this.ctx) return; const s = State.data.settings, t = this.ctx.currentTime; this.music.gain.setTargetAtTime(this.muted ? 0 : s.music * .55, t, .2); this.sfx.gain.setTargetAtTime(this.muted ? 0 : s.sfx * .8, t, .05); },
  setMood(m) { this.base = m; this.refreshMood(); },
  refreshMood() { this.want = this.boss ? 'boss' : this.engaged.size ? 'combat' : this.base; },

  // ── séquenceur (double-tampon: programme ~0.2 s à l'avance) ──
  schedule() {
    const c = this.ctx; if (!c || c.state !== 'running') return;
    while (this.next < c.currentTime + .2) {
      if (this.step % 16 === 0 && this.want !== this.mood) this.mood = this.want; // changement à la mesure
      this.play16(this.step, this.next); const M = MOODS[this.mood];
      this.next += 60 / M.bpm / 4; this.step++;
    }
  },
  play16(s, t) {
    const M = MOODS[this.mood], sc = SC[M.sc], bar = Math.floor(s / 16) % M.prog.length, deg = M.prog[bar], i16 = s % 16;
    const note = (d, oct = 0) => { const n = sc.length; return M.root + 12 * (oct + Math.floor((d) / n)) + sc[((d % n) + n) % n]; };
    if (i16 === 0 && M.pad) for (const k of [0, 2, 4]) this.pad(mtof(note(deg + k)), t, 60 / M.bpm * 4, M.pad * .05);
    if (M.bass && (i16 % 8 === 0 || (M.drums >= 3 && i16 % 4 === 2))) this.pluck(mtof(note(deg, -2)), t, .3, M.bass * .16, 'triangle', 600);
    const arpOn = M.arp === 'slow' ? i16 % 4 === 0 : M.arp === 'bounce' ? [0, 3, 6, 8, 10, 12, 14].includes(i16) : i16 % 2 === 0;
    if (arpOn) { const pat = [0, 2, 4, 2, 5, 4, 2, 4], k = pat[(s >> (M.arp === 'slow' ? 2 : 1)) % pat.length]; this.pluck(mtof(note(deg + k, 1)), t, M.arp === 'slow' ? .9 : .22, .05, M.arp === 'run' ? 'square' : 'triangle', M.arp === 'run' ? 1800 : 3000, M.arp === 'slow'); }
    if (M.bell && i16 === 10 && Math.random() < .5 * M.bell) this.bell(mtof(note(deg + 4 + (Math.random() * 3 | 0), 2)), t, .04);
    const D = M.drums;
    if (D >= 1 && (i16 === 0 || (D >= 2 && i16 === 8) || (D >= 3 && i16 % 4 === 0))) this.kick(t, .5);
    if (D >= 1.5 && (i16 === 4 || i16 === 12)) this.snare(t, D >= 3 ? .22 : .14);
    if (D >= 1 && i16 % 2 === 0) this.hat(t, i16 % 4 === 2 ? .05 : .025);
    if (D >= 4 && i16 % 2 === 1 && Math.random() < .5) this.hat(t, .03);
  },

  // ── instruments ──
  env(g, t, a, peak, dur) { g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(peak, t + a); g.gain.exponentialRampToValueAtTime(.0001, t + dur); },
  osc(type, f, t, dur, peak, dest, a = .005, filt) {
    const c = this.ctx, o = c.createOscillator(), g = c.createGain(); o.type = type; o.frequency.setValueAtTime(f, t);
    let n = o; if (filt) { const lp = c.createBiquadFilter(); lp.frequency.value = filt; o.connect(lp); n = lp; }
    n.connect(g); g.connect(dest); this.env(g, t, a, peak, dur); o.start(t); o.stop(t + dur + .05); return o;
  },
  pad(f, t, dur, v) { for (const det of [-7, 7]) { const o = this.osc('sawtooth', f, t, dur, v, this.music, dur * .35, 900); o.detune.value = det; } },
  pluck(f, t, dur, v, type = 'triangle', filt = 3000, echo = false) { this.osc(type, f, t, dur, v, echo ? this.delay : this.music, .004, filt); if (echo) this.osc(type, f, t, dur, v * .7, this.music, .004, filt); },
  bell(f, t, v) { this.osc('sine', f, t, 1.6, v, this.delay); this.osc('sine', f * 2.76, t, .6, v * .3, this.music); },
  kick(t, v, dest = this.music) { const o = this.osc('sine', 150, t, .32, v, dest, .002); o.frequency.exponentialRampToValueAtTime(42, t + .25); },
  noiseHit(t, dur, v, type, freq, dest = this.music) {
    const c = this.ctx, s = c.createBufferSource(), f = c.createBiquadFilter(), g = c.createGain(); s.buffer = this.noise; f.type = type; f.frequency.value = freq;
    s.connect(f); f.connect(g); g.connect(dest); this.env(g, t, .002, v, dur); s.start(t, Math.random() * .5); s.stop(t + dur + .05); return f;
  },
  snare(t, v) { this.noiseHit(t, .18, v, 'bandpass', 1800); this.osc('triangle', 190, t, .1, v * .5, this.music); },
  hat(t, v) { this.noiseHit(t, .05, v, 'highpass', 7000); },

  // ── effets sonores ──
  sfxPlay(name, arg) {
    const c = this.ctx; if (!c || c.state !== 'running') return; const t = c.currentTime, S = this.sfx;
    const up = (fs, d = .07, v = .14, type = 'triangle') => fs.forEach((f, i) => this.osc(type, f, t + i * d, .25, v, S));
    switch (name) {
      case 'click': this.osc('sine', 880, t, .06, .12, S); break;
      case 'pickup': up([1320, 1760, 2640], .045, .1, 'sine'); break;
      case 'xp': up([784, 1047], .06, .06, 'sine'); break;
      case 'evolve': { const o = this.osc('sawtooth', 110, t, 2.2, .14, S, .4, 2400); o.frequency.exponentialRampToValueAtTime(880, t + 1.8); [523, 659, 784, 1047, 1319].forEach((f, i) => this.osc('triangle', f, t + 1.6 + i * .08, 1.4, .1, S)); this.noiseHit(t + 1.6, 1.2, .2, 'highpass', 3000, S); break; }
      case 'hit': this.noiseHit(t, .08, .3, 'bandpass', 2500, S); this.osc('square', 220 + Math.random() * 60, t, .07, .07, S); break;
      case 'crit': this.noiseHit(t, .15, .4, 'bandpass', 3500, S); up([660, 990], .03, .1, 'square'); break;
      case 'kill': this.noiseHit(t, .3, .3, 'lowpass', 1200, S); up([392, 523, 784], .05, .09, 'square'); break;
      case 'hurt': { const o = this.osc('sawtooth', 180, t, .25, .2, S, .002, 900); o.frequency.exponentialRampToValueAtTime(60, t + .22); break; }
      case 'jump': { const o = this.osc('square', 300, t, .14, .06, S, .002, 1500); o.frequency.exponentialRampToValueAtTime(700, t + .12); break; }
      case 'ability': { const f = { dash: 400, starbolt: 1200, shockwave: 180, gravity: 90, supernova: 70 }[arg] ?? 300;
        const o = this.osc('sawtooth', f, t, .5, .14, S, .01, 3000); o.frequency.exponentialRampToValueAtTime(f * (arg === 'starbolt' ? .5 : 2.5), t + .4); this.noiseHit(t, .4, .18, 'bandpass', f * 4, S); if (f < 200) this.kick(t, .6, S); break; }
      case 'fail': this.osc('square', 140, t, .12, .08, S); this.osc('square', 120, t + .1, .15, .08, S); break;
      case 'quest': up([523, 659, 784], .09, .12); break;
      case 'questDone': up([523, 659, 784, 1047], .1, .13); this.bell(1568, t + .45, .06); break;
      case 'discover': up([392, 587, 784, 1175], .11, .09, 'sine'); break;
      case 'dialogue': this.osc('square', 520 + Math.random() * 200, t, .05, .035, S, .002, 1800); break;
      case 'land': this.noiseHit(t, .8, .25, 'lowpass', 500, S); this.kick(t + .5, .7, S); break;
      case 'takeoff': { const f = this.noiseHit(t, 1.4, .25, 'bandpass', 300, S); f.frequency.exponentialRampToValueAtTime(3000, t + 1.3); break; }
      case 'death': [392, 330, 262, 196].forEach((f, i) => this.osc('triangle', f, t + i * .18, .4, .14, S)); break;
      case 'boss': { this.noiseHit(t, 2.5, .35, 'lowpass', 180, S); const o = this.osc('sawtooth', 55, t, 2.5, .2, S, .3, 400); o.frequency.linearRampToValueAtTime(41, t + 2.4); break; }
      case 'combo': this.osc('triangle', 600 + Math.min(arg, 10) * 80, t, .08, .06, S); break;
    }
  },
};

export function initAudio() {
  const A = Audio, s = (n, a) => A.sfxPlay(n, a);
  const unlock = () => { A.init(); A.ctx?.resume?.(); };
  addEventListener('pointerdown', unlock, { capture: true }); addEventListener('keydown', unlock, { capture: true });
  document.addEventListener('visibilitychange', () => { if (!A.ctx) return; document.hidden ? A.ctx.suspend() : A.ctx.resume(); });
  bus.on('settings:changed', () => A.volumes());
  bus.on('game:start', () => A.setMood('space'));
  bus.on('scene:entered', ({ id, planet }) => { A.engaged.clear(); A.setMood(id === 'planet' ? (BIOME_MOOD[planet?.biome] ?? 'meadow') : 'space'); s(id === 'planet' ? 'land' : 'takeoff'); });
  bus.on('enemy:order', ({ uid, order, target }) => { if (order === 'move' && target === 'hero' || order === 'attack') A.engaged.add(uid); else A.engaged.delete(uid); A.refreshMood(); });
  bus.on('enemy:killed', ({ uid }) => { A.engaged.delete(uid); A.refreshMood(); s('kill'); });
  bus.on('planet:leave', () => { A.engaged.clear(); A.refreshMood(); });
  bus.on('boss:spawn', () => { A.boss = true; A.refreshMood(); s('boss'); });
  for (const ev of ['boss:defeated', 'boss:despawn']) bus.on(ev, () => { A.boss = false; A.refreshMood(); });
  bus.on('ui:click', () => s('click'));
  bus.on('pickup:collected', () => s('pickup'));
  bus.on('hero:evolved', () => s('evolve'));
  bus.on('enemy:damaged', ({ crit }) => s(crit ? 'crit' : 'hit'));
  bus.on('boss:damaged', () => s('hit'));
  bus.on('hero:damaged', () => s('hurt'));
  bus.on('hero:died', () => s('death'));
  bus.on('ability:cast', ({ id }) => s('ability', id));
  bus.on('ability:fail', () => s('fail'));
  bus.on('quest:started', () => s('quest'));
  bus.on('quest:done', () => s('questDone'));
  bus.on('world:discovered', () => s('discover'));
  bus.on('dialogue:node', () => s('dialogue'));
  bus.on('combat:combo', ({ n }) => s('combo', n));
  bus.on('game:pause', () => { A.muted = true; A.volumes(); A.muted = false; if (A.music) A.music.gain.setTargetAtTime(State.data.settings.music * .15, A.ctx.currentTime, .3); });
  bus.on('game:resume', () => A.volumes());
  globalThis.__audio = A;
}
// pour info: humeur d'une planète (tests / UI)
export const moodOf = (planetId) => BIOME_MOOD[getPlanet(planetId)?.biome] ?? 'meadow';
