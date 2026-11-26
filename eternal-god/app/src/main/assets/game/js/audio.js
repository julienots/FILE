// Son entièrement synthétisé (Web Audio) : ambiances (vent, oiseaux, grillons, chœur divin)
// et effets (pas, ailes, impacts, explosions, rayon, tonnerre, Titan). Nombre de voix limité.
import { G, settings, clamp, rand } from './core.js';

export class Audio {
  constructor() {
    this.ctx = null; this.voices = 0; this.maxVoices = 14;
  }

  // doit être appelé depuis un geste de l'utilisateur (règle des navigateurs mobiles)
  unlock() {
    if (this.ctx) { if (this.ctx.state === 'suspended') this.ctx.resume(); return; }
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    try { this.ctx = new AC({ latencyHint: 'interactive' }); } catch (e) { return; }
    const c = this.ctx;
    this.master = c.createGain(); this.master.gain.value = settings.volume;
    this.comp = c.createDynamicsCompressor(); this.comp.threshold.value = -14; this.comp.ratio.value = 4;
    this.master.connect(this.comp).connect(c.destination);
    // bruit blanc réutilisable
    const len = c.sampleRate * 2, buf = c.createBuffer(1, len, c.sampleRate), d = buf.getChannelData(0);
    for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;
    this.noise = buf;
    // vent permanent
    this.wind = this.loopNoise(400, 0.0);
    // chœur divin discret (accord tenu)
    this.choir = c.createGain(); this.choir.gain.value = 0; this.choir.connect(this.master);
    for (const f of [220, 277.2, 329.6, 440]) {
      const o = c.createOscillator(); o.type = 'sine'; o.frequency.value = f;
      const lfo = c.createOscillator(); lfo.frequency.value = rand(0.1, 0.3); const lg = c.createGain(); lg.gain.value = 1.5; lfo.connect(lg).connect(o.frequency); lfo.start();
      const g = c.createGain(); g.gain.value = 0.05; o.connect(g).connect(this.choir); o.start();
    }
    this.birdT = 2; this.cricketT = 1;
  }

  setVolume(v) { if (this.master) this.master.gain.value = v; }
  suspend() { if (this.ctx && this.ctx.state === 'running') this.ctx.suspend(); }
  resume() { if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume(); }

  loopNoise(freq, gain) {
    const c = this.ctx, src = c.createBufferSource(); src.buffer = this.noise; src.loop = true;
    const f = c.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = freq;
    const g = c.createGain(); g.gain.value = gain;
    src.connect(f).connect(g).connect(this.master); src.start();
    return { src, f, g };
  }

  // volume selon la distance à la caméra
  att(p, range = 400) { if (!p || !G.camera) return 1; const d = G.camera.position.distanceTo(p); return clamp(1 - d / range, 0, 1) ** 1.5; }

  burst({ dur = 0.5, freq = 800, q = 0.7, gain = 0.5, type = 'lowpass', sweep = 0, at = null, range = 400 }) {
    if (!this.ctx || this.voices >= this.maxVoices) return;
    const k = this.att(at, range) * gain; if (k < 0.01) return;
    const c = this.ctx, t = c.currentTime;
    const src = c.createBufferSource(); src.buffer = this.noise; src.playbackRate.value = rand(0.8, 1.2);
    const f = c.createBiquadFilter(); f.type = type; f.frequency.setValueAtTime(freq, t); f.Q.value = q;
    if (sweep) f.frequency.exponentialRampToValueAtTime(Math.max(30, freq * sweep), t + dur);
    const g = c.createGain(); g.gain.setValueAtTime(k, t); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    src.connect(f).connect(g).connect(this.master);
    src.start(t, Math.random()); src.stop(t + dur + 0.05);
    this.voices++; src.onended = () => this.voices--;
  }
  tone({ f0 = 80, f1 = 40, dur = 0.4, gain = 0.5, type = 'sine', at = null, range = 400 }) {
    if (!this.ctx || this.voices >= this.maxVoices) return;
    const k = this.att(at, range) * gain; if (k < 0.01) return;
    const c = this.ctx, t = c.currentTime, o = c.createOscillator(), g = c.createGain();
    o.type = type; o.frequency.setValueAtTime(f0, t); o.frequency.exponentialRampToValueAtTime(Math.max(10, f1), t + dur);
    g.gain.setValueAtTime(k, t); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    o.connect(g).connect(this.master); o.start(t); o.stop(t + dur + 0.05);
    this.voices++; o.onended = () => this.voices--;
  }

  // ---------- effets ----------
  footstep(p, run, big = 1) { this.tone({ f0: run ? 70 : 90, f1: 32, dur: run ? 0.35 : 0.22, gain: (run ? 0.55 : 0.25) * big, at: p, range: 200 * big }); this.burst({ dur: 0.25, freq: 500, gain: run ? 0.2 : 0.08, at: p, range: 150 }); }
  splash(p) { this.burst({ dur: 0.35, freq: 2500, type: 'bandpass', q: 0.6, gain: 0.3, at: p, range: 120 }); }
  wingFlap(k = 0.5) { this.burst({ dur: 0.35, freq: 700, sweep: 0.4, gain: 0.12 + k * 0.18, range: 1e9 }); }
  punch(p) { this.tone({ f0: 120, f1: 40, dur: 0.3, gain: 0.7, at: p }); this.burst({ dur: 0.3, freq: 1800, sweep: 0.2, gain: 0.4, at: p }); }
  impact(p, k = 1) { this.tone({ f0: 60, f1: 22, dur: 0.9 * k + 0.3, gain: 0.9 * k, at: p, range: 700 }); this.burst({ dur: 1.2 * k + 0.2, freq: 900, sweep: 0.15, gain: 0.6 * k, at: p, range: 700 }); }
  explosion(p, k = 1) { this.impact(p, k); this.burst({ dur: 2.5 * k, freq: 3000, sweep: 0.05, gain: 0.5 * k, at: p, range: 1200 }); }
  thunder(p, k = 1) { this.burst({ dur: 0.15, freq: 5000, type: 'highpass', gain: 0.7 * k, at: p, range: 1500 }); setTimeout(() => this.burst({ dur: 2.8, freq: 260, sweep: 0.4, gain: 0.8 * k, at: p, range: 2500 }), 120); }
  thud(p, k = 0.6) { this.tone({ f0: 55, f1: 25, dur: 0.6, gain: k, at: p, range: 300 }); }
  treeCrack(x, z) { this.burst({ dur: 0.25, freq: 1400, type: 'bandpass', q: 2, gain: 0.35, range: 1e9 }); }
  collapse(p, k = 1) { this.impact(p, 0.7 * k); for (let i = 0; i < 4; i++) setTimeout(() => this.burst({ dur: 0.5, freq: rand(600, 1500), type: 'bandpass', q: 1.2, gain: 0.35 * k, at: p }), i * 140); }
  teleport() { this.tone({ f0: 300, f1: 1200, dur: 0.5, gain: 0.3, type: 'triangle', range: 1e9 }); this.burst({ dur: 0.6, freq: 4000, sweep: 0.3, type: 'bandpass', gain: 0.25, range: 1e9 }); }
  meteorWhistle() { this.tone({ f0: 1400, f1: 200, dur: 2.4, gain: 0.25, type: 'sawtooth', range: 1e9 }); }
  whoosh(p) { this.burst({ dur: 0.8, freq: 600, sweep: 2, type: 'bandpass', gain: 0.4, at: p }); }
  roar(p, k = 1) {
    if (!this.ctx) return;
    this.tone({ f0: 70, f1: 45, dur: 2.2 * k, gain: 0.7 * k, type: 'sawtooth', at: p, range: 1500 });
    this.tone({ f0: 105, f1: 60, dur: 2 * k, gain: 0.4 * k, type: 'square', at: p, range: 1500 });
    this.burst({ dur: 2 * k, freq: 400, gain: 0.5 * k, at: p, range: 1500 });
  }
  laser(on) {
    if (!this.ctx) return;
    const c = this.ctx;
    if (on && !this.laserNode) {
      const o = c.createOscillator(), o2 = c.createOscillator(), f = c.createBiquadFilter(), g = c.createGain();
      o.type = 'sawtooth'; o.frequency.value = 110; o2.type = 'square'; o2.frequency.value = 113;
      f.type = 'lowpass'; f.frequency.value = 1800; g.gain.value = 0; g.gain.linearRampToValueAtTime(0.28, c.currentTime + 0.15);
      o.connect(f); o2.connect(f); f.connect(g).connect(this.master); o.start(); o2.start();
      this.laserNode = { o, o2, g };
    } else if (!on && this.laserNode) {
      const n = this.laserNode; this.laserNode = null;
      n.g.gain.linearRampToValueAtTime(0, c.currentTime + 0.2); n.o.stop(c.currentTime + 0.25); n.o2.stop(c.currentTime + 0.25);
    }
  }
  chirp() { // oiseau
    if (!this.ctx || this.voices >= this.maxVoices) return;
    const c = this.ctx, t = c.currentTime, o = c.createOscillator(), g = c.createGain();
    const f = rand(2200, 4200); o.type = 'sine';
    for (let i = 0; i < 3; i++) { o.frequency.setValueAtTime(f, t + i * 0.09); o.frequency.exponentialRampToValueAtTime(f * rand(1.2, 1.6), t + i * 0.09 + 0.05); }
    g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(0.04, t + 0.02); g.gain.exponentialRampToValueAtTime(0.001, t + 0.3);
    o.connect(g).connect(this.master); o.start(t); o.stop(t + 0.32); this.voices++; o.onended = () => this.voices--;
  }
  charge(dur = 1.4) { // montée de puissance avant la transformation
    if (!this.ctx) return;
    this.tone({ f0: 60, f1: 180, dur, gain: 0.45, type: 'sawtooth', range: 1e9 });
    this.burst({ dur, freq: 300, sweep: 8, type: 'bandpass', q: 1.5, gain: 0.5, range: 1e9 });
    setTimeout(() => this.scream(), dur * 700);
  }
  scream() { // cri de transformation (bruit formanté)
    this.burst({ dur: 1.1, freq: 900, type: 'bandpass', q: 3, gain: 0.45, sweep: 1.3, range: 1e9 });
    this.tone({ f0: 210, f1: 160, dur: 1.0, gain: 0.25, type: 'sawtooth', range: 1e9 });
  }
  swing() { this.burst({ dur: 0.16, freq: 1600, sweep: 0.4, type: 'bandpass', q: 1.4, gain: 0.25, range: 1e9 }); }
  cricket() { this.burst({ dur: 0.18, freq: 4800, type: 'bandpass', q: 18, gain: 0.05, range: 1e9 }); }

  update(dt) {
    if (!this.ctx || !G.god) return;
    const god = G.god, alt = god.pos.y - G.terrain.groundAt(god.pos.x, god.pos.z);
    const sp = god.vel.length();
    const w = clamp(0.04 + alt / 900 + sp / 120 + (G.env ? G.env.weatherT * 0.2 : 0), 0, 0.55);
    this.wind.g.gain.setTargetAtTime(w, this.ctx.currentTime, 0.4);
    this.wind.f.frequency.setTargetAtTime(300 + sp * 18 + alt * 0.6, this.ctx.currentTime, 0.4);
    const divine = (G.powers && (G.powers.laserOn || G.powers.meteors.length)) ? 0.6 : (god.state === 'fly' && alt > 150 ? 0.25 : 0.05);
    this.choir.gain.setTargetAtTime(settings.music > 0.02 ? 0 : divine * 0.5, this.ctx.currentTime, 1.2); // la musique remplace le chœur
    const night = G.env ? G.env.nightF : 0;
    this.birdT -= dt; this.cricketT -= dt;
    if (alt < 60 && night < 0.5 && this.birdT <= 0) { this.birdT = rand(1.5, 6); this.chirp(); }
    if (alt < 60 && night > 0.5 && this.cricketT <= 0) { this.cricketT = rand(0.3, 1.2); this.cricket(); }
  }
}
