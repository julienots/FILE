// Musique orchestrale épique, entièrement synthétisée et adaptative (Web Audio).
// Ré mineur, 100 BPM, cycle de 4 mesures (Rém – Si♭ – Fa – La). Trois niveaux d'intensité :
//  0 exploration : cordes tenues, cor héroïque, tambours lointains ;
//  1 combat      : + ostinato de cordes, cuivres en accents, taikos complets ;
//  2 aura        : + chœur, taikos déchaînés, thème à l'octave.
// Conçue pour le mobile (aucun craquement) : instruments à voix PERMANENTES dont on ne change que la
// hauteur et le volume (presque aucune création de nœuds pendant le jeu), réverbération légère par
// lignes de retard, planification régulière avec une marge confortable.
import { G, settings, rand } from './core.js';

const BPM = 100, STEP = 60 / BPM / 4;                 // double croche
const D3 = 146.83, f = (semi) => D3 * Math.pow(2, semi / 12);
const CHORDS = [
  { bass: -12, tri: [0, 3, 7] },     // Rém
  { bass: -16, tri: [-4, 0, 3] },    // Si♭
  { bass: -9, tri: [-3, 3, 7] },     // Fa
  { bass: -17, tri: [-5, -1, 4] },   // La
];
// thème héroïque : [demi-tons depuis Ré3, durée en temps]
const THEME = [
  [[0, 1.5], [7, 0.5], [7, 2]],
  [[3, 1], [5, 1], [8, 2]],
  [[10, 1.5], [8, 0.5], [7, 2]],
  [[7, 1], [11, 1], [14, 2]],
];
const THEME_B = [
  [[12, 1.5], [10, 0.5], [7, 2]],
  [[8, 1], [7, 1], [3, 2]],
  [[5, 1], [7, 1], [10, 1], [12, 1]],
  [[11, 1.5], [9, 0.5], [7, 2]],
];
const TAIKO = [
  [1, 0, 0, 0, 0, 0, 0, 0, 0.55, 0, 0, 0, 0, 0, 0, 0],
  [1, 0, 0, 0.5, 0, 0, 0.75, 0, 1, 0, 0.5, 0, 0, 0.6, 0.8, 0.9],
  [1, 0, 0.6, 0.5, 1, 0, 0.75, 0.5, 1, 0, 0.6, 0.5, 1, 0.75, 0.9, 1],
];
const OST = [0, 0, 2, 0, 1, 0, 2, 0, 0, 0, 2, 0, 1, 2, 1, 2];
const LEVELS = { 0: [0.5, 0.45, 0.5, 0, 0, 0], 1: [0.4, 0.35, 0.85, 0.5, 0.55, 0], 2: [0.38, 0.45, 1, 0.55, 0.6, 0.5] };

export class Music {
  constructor(audio) {
    this.audio = audio; this.ready = false;
    this.level = -1; this.combatT = 0;
    this.step = 0; this.bar = 0; this.next = 0;
    this.auraOn = false;
  }

  init() {
    const A = this.audio; if (!A.ctx || this.ready) return !!this.ready;
    const c = A.ctx; this.c = c;
    this.out = c.createGain(); this.out.gain.value = settings.music * 0.7;
    this.out.connect(A.limiter || A.comp);
    // réverbération légère : deux lignes de retard rebouclées, filtrées (sonne « grande salle »)
    this.bus = c.createGain(); this.bus.connect(this.out);
    const send = c.createGain(); send.gain.value = 0.35; this.bus.connect(send);
    for (const [dt, fb] of [[0.137, 0.55], [0.211, 0.5]]) {
      const d = c.createDelay(1), g = c.createGain(), lp = c.createBiquadFilter();
      d.delayTime.value = dt; g.gain.value = fb; lp.type = 'lowpass'; lp.frequency.value = 2200;
      send.connect(d); d.connect(lp).connect(g).connect(d); lp.connect(this.out);
    }
    // couches (volume de chaque section)
    const layer = () => { const g = c.createGain(); g.gain.value = 0; g.connect(this.bus); return g; };
    this.L = { pad: layer(), mel: layer(), drum: layer(), ost: layer(), brass: layer(), choir: layer() };
    // --- voix permanentes ---
    const saw = (freq, det, dest, type = 'sawtooth') => { const o = c.createOscillator(); o.type = type; o.frequency.value = freq; o.detune.value = det; o.connect(dest); o.start(); return o; };
    const voice = (dest, cutoff, oscs, q = 0.5) => {
      const fl = c.createBiquadFilter(); fl.type = 'lowpass'; fl.frequency.value = cutoff; fl.Q.value = q;
      const g = c.createGain(); g.gain.value = 0; fl.connect(g).connect(dest);
      return { fl, g, o: oscs.map(([det, type]) => saw(110, det, fl, type)) };
    };
    // cordes tenues : basse + 3 notes de l'accord
    this.pad = [voice(this.L.pad, 700, [[-6], [6]]), ...[0, 1, 2].map(() => voice(this.L.pad, 1800, [[-7], [7]]))];
    // chœur « Ah » : 3 voix passées dans des formants communs
    const choirIn = c.createGain(); choirIn.gain.value = 1;
    for (const [fr, q, a] of [[800, 6, 1], [1150, 7, 0.6], [2900, 8, 0.25]]) { const bp = c.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = fr; bp.Q.value = q; const ga = c.createGain(); ga.gain.value = a * 2.5; choirIn.connect(bp).connect(ga).connect(this.L.choir); }
    this.choirV = [0, 1, 2].map(() => { const g = c.createGain(); g.gain.value = 0; g.connect(choirIn); return { g, o: [saw(220, -8, g), saw(220, 8, g)] }; });
    // ostinato : une voix rapide à enveloppe courte
    this.ost = voice(this.L.ost, 1400, [[-4], [4]], 1);
    // cuivres : 3 voix (fondamentale, quinte, octave), enveloppe et filtre animés
    this.brassV = [0, 1, 2].map(() => voice(this.L.brass, 600, [[0], [5, 'square']], 2));
    // cor (thème) : une voix avec vibrato
    this.hornV = voice(this.L.mel, 1500, [[0], [0, 'triangle']], 1);
    const lfo = c.createOscillator(), lg = c.createGain(); lfo.frequency.value = 5; lg.gain.value = 6; lfo.connect(lg); for (const o of this.hornV.o) lg.connect(o.detune); lfo.start();
    // tambour : un bruit court pré-calculé (pas de génération en jeu)
    this.next = c.currentTime + 0.3;
    this.ready = true;
    return true;
  }

  setVolume(v) { if (this.out) this.out.gain.setTargetAtTime(v * 0.7, this.c.currentTime, 0.2); }
  setAura(on) { this.auraOn = on; if (on && this.ready) { const t = this.c.currentTime + 0.05; this.taiko(t, 1.2); this.taiko(t + 0.12, 0.9); } }

  // ---------- jeu des voix ----------
  setF(v, freq, t, glide = 0.01) { for (const o of v.o) o.frequency.setTargetAtTime(freq, t, glide); }
  swell(g, t, peak, att, dur, rel) { // attaque douce, tenue, extinction : aucune discontinuité (pas de clic)
    g.gain.cancelScheduledValues(t);
    g.gain.setTargetAtTime(peak, t, att / 3);
    g.gain.setTargetAtTime(0, t + dur, rel / 3);
  }
  taiko(t, k, low = true) {
    const c = this.c, o = c.createOscillator(), g = c.createGain();
    const f0 = low ? 92 : 180;
    o.frequency.setValueAtTime(f0, t); o.frequency.exponentialRampToValueAtTime(f0 * 0.45, t + 0.25);
    g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(0.75 * k, t + 0.006); g.gain.setTargetAtTime(0, t + 0.02, low ? 0.18 : 0.07);
    o.connect(g).connect(this.L.drum); o.start(t); o.stop(t + (low ? 1.2 : 0.5));
    o.onended = () => g.disconnect();
  }

  // ---------- séquenceur ----------
  intensity() {
    const god = G.god;
    const near = (e) => e && e.alive && e.pos && e.pos.distanceTo(god.pos) < 320;
    if (near(G.monster) || near(G.dragon)) this.combatT = 6;
    if ((G.ultimates && G.ultimates.effects.length) || (G.powers && (G.powers.laserOn || G.powers.meteors.length))) this.combatT = Math.max(this.combatT, 5);
    return this.auraOn ? 2 : (this.combatT > 0 ? 1 : 0);
  }

  update(dt) {
    if (!this.ready && !this.init()) return;
    const c = this.c;
    if (c.state !== 'running') return;
    this.combatT = Math.max(0, this.combatT - dt);
    const lvl = this.intensity();
    if (lvl !== this.level) {
      const T = LEVELS[lvl], L = this.L, now = c.currentTime;
      [L.pad, L.mel, L.drum, L.ost, L.brass, L.choir].forEach((g, i) => g.gain.setTargetAtTime(T[i], now, lvl > this.level ? 0.5 : 2));
      this.level = lvl;
    }
    if (settings.music <= 0.01) { this.next = c.currentTime + 0.3; return; }
    if (this.next < c.currentTime) this.next = c.currentTime + 0.1; // après une pause ou une saccade
    while (this.next < c.currentTime + 0.3) { this.tick(this.next); this.next += STEP; }
  }

  tick(t) {
    const s = this.step % 16, barI = this.bar % 4, C = CHORDS[barI], lvl = this.level, bar = STEP * 16;
    if (s === 0) {
      // cordes : chaque voix glisse vers sa nouvelle note (legato, pas de nouvelle attaque)
      const notes = [C.bass, ...C.tri];
      this.pad.forEach((v, i) => { this.setF(v, f(notes[i]), t, 0.06); if (this.bar === 0) this.swell(v.g, t, i ? 0.05 : 0.08, 1.5, 1e5, 1); });
      if (lvl >= 2) this.choirV.forEach((v, i) => { this.setF(v, f(C.tri[i] + 12), t, 0.08); this.swell(v.g, t, 0.06, 0.6, bar * 0.95, 0.5); });
      // thème au cor (variation B un cycle sur deux)
      const theme = (Math.floor(this.bar / 4) % 2 ? THEME_B : THEME)[barI];
      let at = t;
      for (const [semi, beats] of theme) {
        const d = beats * STEP * 4;
        this.setF(this.hornV, f(semi + (lvl >= 2 ? 12 : 0)), at, 0.03);
        this.swell(this.hornV.g, at, 0.08, 0.12, d * 0.85, 0.12);
        at += d;
      }
    }
    // taikos
    const k = TAIKO[Math.max(0, lvl)][s];
    if (k) this.taiko(t, k, !(lvl >= 1 && (s === 4 || s === 12)));
    if (lvl >= 1) {
      // ostinato en double croches : même voix, hauteur et enveloppe changent
      const tones = [C.bass + 12, C.bass + 19, C.bass + 24];
      const v = this.ost;
      v.o.forEach((o) => o.frequency.setValueAtTime(f(tones[OST[s]]), t));
      v.g.gain.setTargetAtTime(0.07, t, 0.004); v.g.gain.setTargetAtTime(0, t + STEP * 0.45, 0.025);
      // cuivres en accents (temps 1, « et » du 2, et temps 4 en aura)
      if (s === 0 || s === 6 || (lvl >= 2 && s === 12)) {
        const dur = s === 0 ? 0.5 : 0.25;
        [C.bass, C.bass + 7, C.bass + 12].forEach((n, i) => {
          const b = this.brassV[i];
          this.setF(b, f(n), t, 0.005);
          b.fl.frequency.setTargetAtTime(2400, t, 0.02); b.fl.frequency.setTargetAtTime(700, t + 0.08, dur * 0.4);
          this.swell(b.g, t, 0.05, 0.03, dur, 0.15);
        });
      }
    }
    this.step++;
    if (this.step % 16 === 0) this.bar++;
  }
}
