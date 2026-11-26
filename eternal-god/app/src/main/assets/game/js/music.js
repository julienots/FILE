// Musique orchestrale épique, entièrement synthétisée et adaptative (Web Audio).
// Ré mineur, 100 BPM, cycle de 4 mesures (Rém – Si♭ – Fa – La). Trois niveaux d'intensité :
//  0 exploration : cordes tenues, cor héroïque, tambours lointains ;
//  1 combat      : + ostinato de cordes, cuivres en accents, taikos complets ;
//  2 aura        : + chœur, taikos déchaînés, mélodie à l'octave.
// Les couches se croisent en douceur ; le moteur ne planifie qu'un court instant à l'avance.
import { G, settings, clamp, rand } from './core.js';

const BPM = 100, STEP = 60 / BPM / 4;                 // double croche
const D3 = 146.83, f = (semi) => D3 * Math.pow(2, semi / 12);
const CHORDS = [
  { bass: -12, tri: [0, 3, 7], name: 'Dm' },
  { bass: -16, tri: [-4, 0, 3], name: 'Bb' },
  { bass: -9, tri: [-3, 3, 7], name: 'F' },
  { bass: -17, tri: [-5, -1, 4], name: 'A' },
];
// thème héroïque : [demi-tons depuis Ré4, durée en temps]
const THEME = [
  [[12, 1.5], [19, 0.5], [19, 2]],
  [[15, 1], [17, 1], [20, 2]],
  [[22, 1.5], [20, 0.5], [19, 2]],
  [[19, 1], [23, 1], [26, 2]],
];
const THEME_B = [
  [[24, 1.5], [22, 0.5], [19, 2]],
  [[20, 1], [19, 1], [15, 2]],
  [[17, 1], [19, 1], [22, 1], [24, 1]],
  [[23, 1.5], [21, 0.5], [19, 2]],
];
const TAIKO = [
  [1, 0, 0, 0, 0, 0, 0, 0, 0.55, 0, 0, 0, 0, 0, 0, 0],
  [1, 0, 0, 0.5, 0, 0, 0.75, 0, 1, 0, 0.5, 0, 0, 0.6, 0.8, 0.9],
  [1, 0, 0.6, 0.5, 1, 0, 0.75, 0.5, 1, 0, 0.6, 0.5, 1, 0.75, 0.9, 1],
];
const OST = [0, 0, 2, 0, 1, 0, 2, 0, 0, 0, 2, 0, 1, 2, 1, 2];

export class Music {
  constructor(audio) {
    this.audio = audio; this.ready = false;
    this.level = 0; this.target = 0; this.combatT = 0;
    this.step = 0; this.bar = 0; this.next = 0;
    this.auraOn = false;
  }

  init() {
    const A = this.audio; if (!A.ctx || this.ready) return !!this.ready;
    const c = A.ctx; this.c = c;
    this.out = c.createGain(); this.out.gain.value = settings.music;
    this.out.connect(A.comp);
    // réverbération de cathédrale (impulsion générée)
    const len = Math.floor(c.sampleRate * 3.2), ir = c.createBuffer(2, len, c.sampleRate);
    for (let ch = 0; ch < 2; ch++) { const d = ir.getChannelData(ch); for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 3.2); }
    this.rev = c.createConvolver(); this.rev.buffer = ir;
    const wet = c.createGain(); wet.gain.value = 0.5;
    this.bus = c.createGain(); this.bus.gain.value = 1;
    this.bus.connect(this.out); this.bus.connect(this.rev); this.rev.connect(wet).connect(this.out);
    // couches
    const layer = (v) => { const g = c.createGain(); g.gain.value = v; g.connect(this.bus); return g; };
    this.L = { pad: layer(0.0), mel: layer(0.0), drum: layer(0.0), ost: layer(0.0), brass: layer(0.0), choir: layer(0.0) };
    this.next = c.currentTime + 0.2;
    this.ready = true;
    return true;
  }

  setVolume(v) { if (this.out) this.out.gain.setTargetAtTime(v, this.c.currentTime, 0.2); }
  setAura(on) { this.auraOn = on; if (on) this.stinger(); }

  // ---------- instruments ----------
  env(g, t, a, peak, d, rel = 0.3) {
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(peak, t + a);
    g.gain.setTargetAtTime(peak * 0.7, t + a, d * 0.4);
    g.gain.setTargetAtTime(0.0001, t + d, rel);
  }
  strings(freq, t, dur, gain, dest, att = 0.5) {
    const c = this.c, g = c.createGain(), fl = c.createBiquadFilter();
    fl.type = 'lowpass'; fl.frequency.value = 1500 + freq * 1.5; fl.Q.value = 0.4;
    for (const dt of [-7, 0, 7]) { const o = c.createOscillator(); o.type = 'sawtooth'; o.frequency.value = freq; o.detune.value = dt + rand(-2, 2); o.connect(fl); o.start(t); o.stop(t + dur + 1.5); }
    this.env(g, t, att, gain, dur, 0.5);
    fl.connect(g).connect(dest);
  }
  ostNote(freq, t, gain) {
    const c = this.c, o = c.createOscillator(), o2 = c.createOscillator(), g = c.createGain(), fl = c.createBiquadFilter();
    o.type = 'sawtooth'; o2.type = 'sawtooth'; o.frequency.value = freq; o2.frequency.value = freq * 1.003;
    fl.type = 'lowpass'; fl.frequency.setValueAtTime(3200, t); fl.frequency.exponentialRampToValueAtTime(900, t + 0.12);
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(gain, t + 0.008); g.gain.exponentialRampToValueAtTime(0.001, t + STEP * 0.95);
    o.connect(fl); o2.connect(fl); fl.connect(g).connect(this.L.ost);
    o.start(t); o2.start(t); o.stop(t + STEP + 0.05); o2.stop(t + STEP + 0.05);
  }
  brass(freqs, t, dur, gain, dest = this.L.brass) {
    const c = this.c, g = c.createGain(), fl = c.createBiquadFilter();
    fl.type = 'lowpass'; fl.Q.value = 2;
    fl.frequency.setValueAtTime(300, t); fl.frequency.linearRampToValueAtTime(2600, t + 0.07); fl.frequency.setTargetAtTime(1100, t + 0.08, dur * 0.4);
    for (const fr of freqs) for (const ty of ['sawtooth', 'square']) { const o = c.createOscillator(); o.type = ty; o.frequency.value = fr; o.detune.value = rand(-5, 5); o.connect(fl); o.start(t); o.stop(t + dur + 0.6); }
    this.env(g, t, 0.04, gain, dur, 0.18);
    fl.connect(g).connect(dest);
  }
  horn(freq, t, dur, gain) { // cor : cuivre doux, attaque lente, léger vibrato
    const c = this.c, o = c.createOscillator(), o2 = c.createOscillator(), g = c.createGain(), fl = c.createBiquadFilter(), lfo = c.createOscillator(), lg = c.createGain();
    o.type = 'sawtooth'; o2.type = 'triangle'; o.frequency.value = freq; o2.frequency.value = freq * 0.5;
    lfo.frequency.value = 5.2; lg.gain.value = freq * 0.006; lfo.connect(lg); lg.connect(o.frequency); lfo.start(t); lfo.stop(t + dur + 0.6);
    fl.type = 'lowpass'; fl.frequency.setValueAtTime(500, t); fl.frequency.linearRampToValueAtTime(1700, t + 0.15); fl.Q.value = 1;
    this.env(g, t, 0.09, gain, dur, 0.15);
    o.connect(fl); o2.connect(fl); fl.connect(g).connect(this.L.mel);
    o.start(t); o2.start(t); o.stop(t + dur + 0.6); o2.stop(t + dur + 0.6);
  }
  choir(freq, t, dur, gain) { // voix « Ah » : formants
    const c = this.c, g = c.createGain(), mix = c.createGain(); mix.gain.value = 1;
    for (const dt of [-10, 0, 9]) { const o = c.createOscillator(); o.type = 'sawtooth'; o.frequency.value = freq; o.detune.value = dt; o.connect(mix); o.start(t); o.stop(t + dur + 2); }
    for (const [fr, q, a] of [[800, 8, 1], [1150, 9, 0.6], [2900, 10, 0.25]]) { const bp = c.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = fr; bp.Q.value = q; const ga = c.createGain(); ga.gain.value = a * 3; mix.connect(bp).connect(ga).connect(g); }
    this.env(g, t, 0.8, gain, dur, 0.9);
    g.connect(this.L.choir);
  }
  taiko(t, k, low = true) {
    const c = this.c, A = this.audio;
    const o = c.createOscillator(), g = c.createGain();
    o.type = 'sine'; const f0 = low ? 95 : 190; o.frequency.setValueAtTime(f0, t); o.frequency.exponentialRampToValueAtTime(f0 * 0.42, t + 0.25);
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.9 * k, t + 0.005); g.gain.exponentialRampToValueAtTime(0.001, t + (low ? 0.9 : 0.35));
    o.connect(g).connect(this.L.drum); o.start(t); o.stop(t + 1);
    const n = c.createBufferSource(), nf = c.createBiquadFilter(), ng = c.createGain();
    n.buffer = A.noise; nf.type = 'lowpass'; nf.frequency.value = low ? 900 : 2200;
    ng.gain.setValueAtTime(0.5 * k, t); ng.gain.exponentialRampToValueAtTime(0.001, t + 0.12);
    n.connect(nf).connect(ng).connect(this.L.drum); n.start(t, rand(0, 1.5)); n.stop(t + 0.15);
  }
  cymbal(t, dur, gain) { // crescendo de cymbale
    const c = this.c, n = c.createBufferSource(), hf = c.createBiquadFilter(), g = c.createGain();
    n.buffer = this.audio.noise; n.loop = true; hf.type = 'highpass'; hf.frequency.value = 5000;
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(gain, t + dur); g.gain.exponentialRampToValueAtTime(0.001, t + dur + 1.2);
    n.connect(hf).connect(g).connect(this.L.drum); n.start(t); n.stop(t + dur + 1.3);
  }
  stinger() { // coup orchestral (transformation)
    if (!this.ready) return;
    const t = this.c.currentTime + 0.05, C = CHORDS[0];
    this.taiko(t, 1.2); this.taiko(t + 0.02, 1, false);
    this.brass([f(C.bass), f(C.bass + 12), f(C.bass + 19), f(3), f(7)], t, 1.6, 0.16, this.bus);
    this.cymbal(t - 1.2, 1.2, 0.12);
  }

  // ---------- séquenceur ----------
  intensity() {
    let lvl = 0;
    const god = G.god;
    const near = (e) => e && e.alive && e.pos && e.pos.distanceTo(god.pos) < 320;
    if (near(G.monster) || near(G.dragon)) this.combatT = 6;
    if (G.ultimates && G.ultimates.effects.length) this.combatT = Math.max(this.combatT, 5);
    if (G.powers && (G.powers.laserOn || G.powers.meteors.length)) this.combatT = Math.max(this.combatT, 5);
    if (this.combatT > 0) lvl = 1;
    if (this.auraOn) lvl = 2;
    return lvl;
  }

  update(dt) {
    if (!this.ready && !this.init()) return;
    const c = this.c;
    if (settings.music <= 0.01) return;
    this.combatT = Math.max(0, this.combatT - dt);
    const lvl = this.intensity();
    if (lvl !== this.level) {
      this.level = lvl;
      const T = { 0: [0.5, 0.42, 0.25, 0, 0, 0], 1: [0.38, 0.28, 0.75, 0.55, 0.6, 0], 2: [0.35, 0.4, 0.9, 0.6, 0.7, 0.55] }[lvl];
      const L = this.L, now = c.currentTime;
      [L.pad, L.mel, L.drum, L.ost, L.brass, L.choir].forEach((g, i) => g.gain.setTargetAtTime(T[i], now, lvl ? 0.6 : 2.5));
      if (lvl) this.combatStart = true;
    }
    if (this.next < c.currentTime) this.next = c.currentTime + 0.05; // après une pause
    while (this.next < c.currentTime + 0.15) { this.tick(this.next); this.next += STEP; }
  }

  tick(t) {
    const s = this.step % 16, barI = this.bar % 4, C = CHORDS[barI], lvl = this.level;
    if (s === 0) {
      // cordes tenues (une mesure), basse, et chœur en aura
      const bar = STEP * 16;
      this.strings(f(C.bass), t, bar, 0.08, this.L.pad, 0.3);
      for (const n of C.tri) this.strings(f(n), t, bar, 0.035, this.L.pad, 0.6);
      if (lvl >= 2) for (const n of C.tri) this.choir(f(n + 12), t, bar, 0.05);
      // thème au cor (variation B un cycle sur deux)
      const theme = (Math.floor(this.bar / 4) % 2 ? THEME_B : THEME)[barI];
      let at = t;
      for (const [semi, beats] of theme) { const d = beats * STEP * 4; this.horn(f(semi - 12 + (lvl >= 2 ? 12 : 0)), at, d * 0.92, 0.07); at += d; }
      if (barI === 3 && lvl >= 1) this.cymbal(t + bar * 0.5, bar * 0.5, 0.06);
      if (this.combatStart) { this.combatStart = false; this.taiko(t, 1.2); }
    }
    // taikos
    const k = TAIKO[lvl][s];
    if (k) this.taiko(t, k, !(lvl >= 1 && (s === 4 || s === 12)));
    if (lvl >= 1) {
      // ostinato de cordes en double croches
      const tones = [C.bass + 12, C.bass + 19, C.bass + 24];
      this.ostNote(f(tones[OST[s]]), t, 0.06);
      // cuivres en accents (temps 1 et « et » du 2)
      if (s === 0 || s === 6 || (lvl >= 2 && s === 12)) this.brass([f(C.bass), f(C.bass + 7), f(C.bass + 12)], t, s === 0 ? 0.55 : 0.3, 0.07);
    }
    this.step++;
    if (this.step % 16 === 0) this.bar++;
  }
}
