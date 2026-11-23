// Moteur audio procédural (Web Audio) : musiques originales séquencées, couches d'ambiance
// adaptatives (drone, battements de cœur, murmures, parasites) et effets sonores synthétisés.
// Aucun fichier audio externe : tout est généré en temps réel.

const N = (name) => {
  // 'C4' -> fréquence
  const m = /^([A-G])(#|b)?(-?\d)$/.exec(name);
  if (!m) return 0;
  const base = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 }[m[1]] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0);
  const midi = base + (parseInt(m[3], 10) + 1) * 12;
  return 440 * Math.pow(2, (midi - 69) / 12);
};
const seq = (str) => str.trim().split(/\s+/).map((t) => (t === '-' || t === '.' ? null : t));

// ——— Compositions originales ———
const TRACKS = {
  menu: {
    bpm: 84, steps: 32, swing: 0,
    lead: { wave: 'box', gain: 0.18, notes: seq(`
      D5 . A4 . F5 . E5 . D5 . A4 . C5 . A4 .
      Bb4 . F4 . D5 . C5 . A4 . E4 . G4 . A4 .`) },
    bass: { wave: 'sine', gain: 0.22, notes: seq(`
      D3 . . . . . . . F3 . . . . . . .
      Bb2 . . . . . . . A2 . . . . . . .`) },
    pad: ['D4', 'F4', 'A4'],
  },
  game: {
    bpm: 138, steps: 64, swing: 0.04,
    lead: { wave: 'square', gain: 0.075, notes: seq(`
      E5 G5 A5 G5 E5 D5 C5 D5  E5 E5 G5 E5 D5 C5 D5 .
      A4 C5 E5 D5 C5 A4 G4 A4  C5 D5 E5 G5 E5 D5 C5 .
      F5 E5 D5 E5 F5 A5 G5 F5  E5 D5 C5 D5 E5 G5 E5 .
      D5 E5 F5 E5 D5 C5 B4 C5  D5 . G4 B4 D5 C5 . .`) },
    bass: { wave: 'triangle', gain: 0.2, notes: seq(`
      C3 . C3 . G2 . G2 .  A2 . A2 . E2 . E2 .
      F2 . F2 . C3 . C3 .  G2 . G2 . G2 . B2 .
      F2 . F2 . A2 . A2 .  C3 . C3 . E3 . C3 .
      G2 . G2 . F2 . F2 .  G2 . G2 . G2 . . .`) },
    drums: seq(`k h s h k h s h k h s h k k s h`),
  },
  chase: {
    bpm: 164, steps: 32, swing: 0,
    lead: { wave: 'saw', gain: 0.06, notes: seq(`
      D5 . F5 E5 D5 . A4 .  Bb4 . A4 G4 A4 . . .
      D5 . F5 G5 A5 . G5 F5  E5 . C5 D5 E5 . D5 .`) },
    bass: { wave: 'saw', gain: 0.13, notes: seq(`
      D2 D3 D2 D3 D2 D3 D2 D3  Bb1 Bb2 Bb1 Bb2 A1 A2 A1 A2
      D2 D3 D2 D3 D2 D3 D2 D3  C2 C3 C2 C3 A1 A2 C2 C3`) },
    drums: seq(`k h s h k k s h k h s h k k s s`),
  },
  boss: {
    bpm: 118, steps: 32, swing: 0,
    lead: { wave: 'saw', gain: 0.055, notes: seq(`
      C5 . . Db5 C5 . G4 .  Ab4 . G4 . F4 . G4 .
      C5 . . Db5 C5 . Eb5 .  D5 . Db5 . C5 . . .`) },
    bass: { wave: 'saw', gain: 0.16, notes: seq(`
      C2 . C2 C2 Db2 . C2 .  C2 . C2 C2 Ab1 . G1 .
      C2 . C2 C2 Db2 . C2 .  C2 . Eb2 . D2 . Db2 .`) },
    drums: seq(`k . s . k k s . k . s . k . s s`),
    pad: ['C3', 'Eb3', 'G3'],
  },
  ending: {
    bpm: 72, steps: 16, swing: 0,
    lead: { wave: 'box', gain: 0.16, notes: seq(`C5 . E5 . G5 . E5 . D5 . F5 . A5 . G5 .`) },
    bass: { wave: 'sine', gain: 0.2, notes: seq(`C3 . . . . . . . F2 . . . G2 . . .`) },
    pad: ['C4', 'E4', 'G4'],
  },
};

export class AudioSys {
  constructor() {
    this.ctx = null; this.ready = false;
    this.music = null; this.musicName = null;
    this.tempoMul = 1; this.targetTempo = 1; this.detune = 0; this.corrupt = 0;
    this.step = 0; this.nextTime = 0; this.timer = null;
    this.layers = {}; this.vol = { music: 0.7, sfx: 0.85 }; this.soft = false; this.pack = 'classic';
  }

  init() {
    if (this.ctx) { if (this.ctx.state === 'suspended') this.ctx.resume(); return; }
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    const ctx = this.ctx = new AC({ latencyHint: 'interactive' });
    this.master = ctx.createGain(); this.master.gain.value = 0.9;
    const comp = ctx.createDynamicsCompressor();
    comp.threshold.value = -14; comp.ratio.value = 4; comp.attack.value = 0.004; comp.release.value = 0.2;
    this.master.connect(comp).connect(ctx.destination);
    this.musicBus = ctx.createGain(); this.musicBus.gain.value = this.vol.music;
    this.musicFilter = ctx.createBiquadFilter(); this.musicFilter.type = 'lowpass'; this.musicFilter.frequency.value = 18000;
    this.musicBus.connect(this.musicFilter).connect(this.master);
    this.sfxBus = ctx.createGain(); this.sfxBus.gain.value = this.vol.sfx; this.sfxBus.connect(this.master);
    this.ambBus = ctx.createGain(); this.ambBus.gain.value = this.vol.sfx; this.ambBus.connect(this.master);
    // Réverbération générée
    this.reverb = ctx.createConvolver(); this.reverb.buffer = this._impulse(2.8, 2.5);
    this.revSend = ctx.createGain(); this.revSend.gain.value = 0.35;
    this.revSend.connect(this.reverb).connect(this.master);
    // Tampons de bruit
    const len = ctx.sampleRate * 2;
    this.noise = ctx.createBuffer(1, len, ctx.sampleRate);
    const d = this.noise.getChannelData(0);
    for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;
    this.brown = ctx.createBuffer(1, len, ctx.sampleRate);
    const b = this.brown.getChannelData(0); let last = 0;
    for (let i = 0; i < len; i++) { last = (last + 0.02 * (Math.random() * 2 - 1)) / 1.02; b[i] = last * 3.5; }
    this.dist = ctx.createWaveShaper(); this.dist.curve = this._curve(60);
    this.ready = true;
    this.timer = setInterval(() => this._schedule(), 25);
  }

  _impulse(sec, decay) {
    const ctx = this.ctx, len = Math.floor(ctx.sampleRate * sec);
    const buf = ctx.createBuffer(2, len, ctx.sampleRate);
    for (let c = 0; c < 2; c++) {
      const d = buf.getChannelData(c);
      for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, decay);
    }
    return buf;
  }
  _curve(k) {
    const n = 1024, c = new Float32Array(n);
    for (let i = 0; i < n; i++) { const x = (i * 2) / n - 1; c[i] = ((3 + k) * x * 20 * Math.PI / 180) / (Math.PI + k * Math.abs(x)); }
    return c;
  }

  setVolumes(music, sfx) {
    this.vol.music = music; this.vol.sfx = sfx;
    if (!this.ready) return;
    const t = this.ctx.currentTime;
    this.musicBus.gain.setTargetAtTime(music * 0.9, t, 0.05);
    this.sfxBus.gain.setTargetAtTime(sfx, t, 0.05);
    this.ambBus.gain.setTargetAtTime(sfx, t, 0.05);
  }
  suspend() { if (this.ctx && this.ctx.state === 'running') this.ctx.suspend(); }
  resume() { if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume(); }

  // ——— Musique ———
  playMusic(name, { tempo = 1 } = {}) {
    if (!this.ready) { this.pending = name; return; }
    if (this.musicName === name && this.music) return;
    this.musicName = name; this.music = TRACKS[name] || null;
    this.step = 0; this.tempoMul = this.targetTempo = tempo; this.detune = 0;
    this.nextTime = this.ctx.currentTime + 0.08;
    const t = this.ctx.currentTime;
    this.musicBus.gain.cancelScheduledValues(t);
    this.musicBus.gain.setValueAtTime(0, t);
    this.musicBus.gain.linearRampToValueAtTime(this.vol.music * 0.9, t + 0.6);
    this._pad(this.music && this.music.pad);
  }
  stopMusic(abrupt = false) {
    this.music = null; this.musicName = null; this._pad(null);
    if (!this.ready) return;
    const t = this.ctx.currentTime;
    if (abrupt) {
      this.musicBus.gain.cancelScheduledValues(t); this.musicBus.gain.setValueAtTime(0, t);
      setTimeout(() => this.ready && this.musicBus.gain.setValueAtTime(this.vol.music * 0.9, this.ctx.currentTime), 400);
    }
  }
  /** Ralentit/accélère la musique (1 = normal). La hauteur suit, comme une cassette qui faiblit. */
  setTempo(mul) { this.targetTempo = mul; }
  /** 0..1 : désaccord, filtre étouffé, notes qui sautent. */
  setCorruption(x) {
    this.corrupt = x;
    if (!this.ready) return;
    this.musicFilter.frequency.setTargetAtTime(18000 - x * 16500, this.ctx.currentTime, 0.3);
  }

  _schedule() {
    if (!this.ready || this.ctx.state !== 'running') return;
    const ctx = this.ctx;
    this.tempoMul += (this.targetTempo - this.tempoMul) * 0.08;
    if (!this.music) return;
    const tr = this.music;
    while (this.nextTime < ctx.currentTime + 0.12) {
      const stepDur = 60 / (tr.bpm * this.tempoMul) / 2;
      const i = this.step % tr.steps;
      const t = this.nextTime + (i % 2 ? tr.swing * stepDur : 0);
      const pitch = this.tempoMul < 0.98 ? Math.log2(this.tempoMul) * 1200 * 0.5 : 0;
      const wob = this.corrupt > 0 ? (Math.random() - 0.5) * this.corrupt * 120 : 0;
      const skip = this.corrupt > 0.3 && Math.random() < this.corrupt * 0.25;
      if (tr.lead && !skip) { const n = tr.lead.notes[i % tr.lead.notes.length]; if (n) this._note(t, N(n), stepDur * 1.7, tr.lead.wave, tr.lead.gain, pitch + wob); }
      if (tr.bass) { const n = tr.bass.notes[i % tr.bass.notes.length]; if (n) this._note(t, N(n), stepDur * 1.9, tr.bass.wave, tr.bass.gain, pitch + wob * 0.5); }
      if (tr.drums && this.corrupt < 0.85) {
        const dn = tr.drums[i % tr.drums.length];
        if (dn === 'k') this._kick(t, 0.5); else if (dn === 's') this._snare(t, 0.25); else if (dn === 'h') this._hat(t, 0.08);
      }
      this.nextTime += stepDur; this.step++;
    }
  }

  _note(t, f, dur, wave, gain, cents = 0) {
    const ctx = this.ctx;
    const g = ctx.createGain();
    const out = this.musicBus;
    let o;
    if (wave === 'box') {
      // boîte à musique : sinus + harmonique, attaque nette, longue extinction
      o = ctx.createOscillator(); o.type = 'sine'; o.frequency.value = f; o.detune.value = cents;
      const o2 = ctx.createOscillator(); o2.type = 'sine'; o2.frequency.value = f * 4.02; o2.detune.value = cents;
      const g2 = ctx.createGain(); g2.gain.value = 0.18; o2.connect(g2).connect(g);
      o2.start(t); o2.stop(t + dur * 3);
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(gain, t + 0.005);
      g.gain.exponentialRampToValueAtTime(0.0001, t + dur * 3);
      o.connect(g); g.connect(out); g.connect(this.revSend);
      o.start(t); o.stop(t + dur * 3 + 0.05);
      return;
    }
    o = ctx.createOscillator(); o.type = wave === 'saw' ? 'sawtooth' : wave; o.frequency.value = f; o.detune.value = cents;
    const filt = ctx.createBiquadFilter(); filt.type = 'lowpass'; filt.frequency.value = wave === 'saw' ? 1800 : 6000; filt.Q.value = 2;
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(gain, t + 0.01);
    g.gain.setTargetAtTime(gain * 0.6, t + 0.02, 0.08);
    g.gain.setTargetAtTime(0.0001, t + dur * 0.8, 0.05);
    o.connect(filt).connect(g).connect(out);
    o.start(t); o.stop(t + dur + 0.3);
  }
  _kick(t, v) {
    const ctx = this.ctx, o = ctx.createOscillator(), g = ctx.createGain();
    o.frequency.setValueAtTime(150, t); o.frequency.exponentialRampToValueAtTime(40, t + 0.12);
    g.gain.setValueAtTime(v, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.25);
    o.connect(g).connect(this.musicBus); o.start(t); o.stop(t + 0.3);
  }
  _snare(t, v) {
    const ctx = this.ctx, s = ctx.createBufferSource(), f = ctx.createBiquadFilter(), g = ctx.createGain();
    s.buffer = this.noise; f.type = 'bandpass'; f.frequency.value = 1800; f.Q.value = 0.7;
    g.gain.setValueAtTime(v, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.16);
    s.connect(f).connect(g).connect(this.musicBus); s.start(t, Math.random()); s.stop(t + 0.2);
  }
  _hat(t, v) {
    const ctx = this.ctx, s = ctx.createBufferSource(), f = ctx.createBiquadFilter(), g = ctx.createGain();
    s.buffer = this.noise; f.type = 'highpass'; f.frequency.value = 7000;
    g.gain.setValueAtTime(v, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.05);
    s.connect(f).connect(g).connect(this.musicBus); s.start(t, Math.random()); s.stop(t + 0.06);
  }
  _pad(notes) {
    if (!this.ready) return;
    const t = this.ctx.currentTime;
    if (this.padNodes) { for (const n of this.padNodes) { n.g.gain.setTargetAtTime(0, t, 0.4); n.o.stop(t + 2); } this.padNodes = null; }
    if (!notes) return;
    this.padNodes = notes.map((nm, i) => {
      const o = this.ctx.createOscillator(); o.type = 'sawtooth'; o.frequency.value = N(nm); o.detune.value = (i - 1) * 7;
      const f = this.ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 700;
      const g = this.ctx.createGain(); g.gain.value = 0; g.gain.setTargetAtTime(0.025, t, 1.5);
      o.connect(f).connect(g); g.connect(this.musicBus); g.connect(this.revSend); o.start();
      return { o, g };
    });
  }

  // ——— Couches d'ambiance (0..1) ———
  layer(name, level) {
    if (!this.ready) return;
    const ctx = this.ctx, t = ctx.currentTime;
    let L = this.layers[name];
    if (!L && level <= 0) return;
    if (!L) {
      L = this.layers[name] = { g: ctx.createGain(), nodes: [] };
      L.g.gain.value = 0; L.g.connect(this.ambBus);
      if (name === 'drone') {
        for (const [f, dt] of [[55, 0], [55.6, 0], [82.4, 5], [110.3, -8]]) {
          const o = ctx.createOscillator(); o.type = 'sawtooth'; o.frequency.value = f; o.detune.value = dt;
          const fl = ctx.createBiquadFilter(); fl.type = 'lowpass'; fl.frequency.value = 220;
          const lfo = ctx.createOscillator(); lfo.frequency.value = 0.07 + Math.random() * 0.1;
          const lg = ctx.createGain(); lg.gain.value = 120; lfo.connect(lg).connect(fl.frequency);
          o.connect(fl).connect(L.g); o.start(); lfo.start(); L.nodes.push(o, lfo);
        }
        L.g.connect(this.revSend);
      } else if (name === 'static') {
        const s = ctx.createBufferSource(); s.buffer = this.noise; s.loop = true;
        const f = ctx.createBiquadFilter(); f.type = 'highpass'; f.frequency.value = 3000;
        const am = ctx.createGain(); am.gain.value = 0.5;
        const lfo = ctx.createOscillator(); lfo.type = 'square'; lfo.frequency.value = 13;
        const lg = ctx.createGain(); lg.gain.value = 0.5; lfo.connect(lg).connect(am.gain);
        s.connect(f).connect(am).connect(L.g); s.start(); lfo.start(); L.nodes.push(s, lfo);
      } else if (name === 'whispers') {
        // bruit filtré par des formants qui bougent : murmures inintelligibles
        for (let i = 0; i < 3; i++) {
          const s = ctx.createBufferSource(); s.buffer = this.noise; s.loop = true; s.playbackRate.value = 0.8 + i * 0.15;
          const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = 9; f.frequency.value = 600 + i * 500;
          const lfo = ctx.createOscillator(); lfo.frequency.value = 2.5 + i * 1.7;
          const lg = ctx.createGain(); lg.gain.value = 400; lfo.connect(lg).connect(f.frequency);
          const am = ctx.createGain(); am.gain.value = 0;
          const lfo2 = ctx.createOscillator(); lfo2.frequency.value = 0.6 + i * 0.37;
          const lg2 = ctx.createGain(); lg2.gain.value = 0.6; lfo2.connect(lg2).connect(am.gain);
          const pan = ctx.createStereoPanner(); pan.pan.value = [-0.8, 0.8, 0][i];
          s.connect(f).connect(am).connect(pan).connect(L.g); s.start(); lfo.start(); lfo2.start();
          L.nodes.push(s, lfo, lfo2);
        }
        L.g.connect(this.revSend);
      } else if (name === 'heartbeat') {
        L.beat = 0; L.bpm = 70;
      } else if (name === 'wind') {
        const s = ctx.createBufferSource(); s.buffer = this.brown; s.loop = true;
        const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 500;
        s.connect(f).connect(L.g); s.start(); L.nodes.push(s);
      }
    }
    L.level = level;
    const gains = { drone: 0.35, static: 0.06, whispers: 0.5, heartbeat: 1, wind: 0.6 };
    L.g.gain.setTargetAtTime(level * (gains[name] || 0.5), t, 0.6);
    if (name === 'heartbeat') { L.bpm = 60 + level * 90; this._heartLoop(); }
  }
  _heartLoop() {
    const L = this.layers.heartbeat;
    if (!L || L.running) return;
    L.running = true;
    const tick = () => {
      if (!this.layers.heartbeat || L.level <= 0.01) { L.running = false; return; }
      if (this.ctx.state === 'running') {
        const t = this.ctx.currentTime;
        this._thump(t, L.level * 0.9, L.g); this._thump(t + 0.22, L.level * 0.6, L.g);
      }
      setTimeout(tick, 60000 / L.bpm);
    };
    tick();
  }
  _thump(t, v, dest) {
    const o = this.ctx.createOscillator(), g = this.ctx.createGain();
    o.frequency.setValueAtTime(70, t); o.frequency.exponentialRampToValueAtTime(38, t + 0.15);
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(v + 0.0001, t + 0.015); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.2);
    o.connect(g).connect(dest || this.sfxBus); o.start(t); o.stop(t + 0.25);
  }
  stopLayers() { for (const k of Object.keys(this.layers)) this.layer(k, 0); }

  // ——— Effets sonores ———
  sfx(name, opt = {}) {
    if (!this.ready || this.ctx.state !== 'running') return;
    const ctx = this.ctx, t = ctx.currentTime + (opt.delay || 0), out = this.sfxBus;
    const v = opt.vol === undefined ? 1 : opt.vol;
    const pack = this.pack;
    const blip = (f, dur, type = 'square', vol = 0.15, slide = 0, dest = out) => {
      const o = ctx.createOscillator(), g = ctx.createGain();
      o.type = pack === 'soft' ? 'sine' : pack === 'chip' && type === 'triangle' ? 'square' : type;
      o.frequency.setValueAtTime(f, t); if (slide) o.frequency.exponentialRampToValueAtTime(Math.max(20, f * slide), t + dur);
      g.gain.setValueAtTime(vol * v, t); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
      o.connect(g).connect(dest); o.start(t); o.stop(t + dur + 0.02);
      return o;
    };
    const noise = (dur, f = 1000, q = 1, vol = 0.3, type = 'bandpass', dest = out, at = t, buf = this.noise) => {
      const s = ctx.createBufferSource(), fl = ctx.createBiquadFilter(), g = ctx.createGain();
      s.buffer = buf; fl.type = type; fl.frequency.value = f; fl.Q.value = q;
      g.gain.setValueAtTime(vol * v, at); g.gain.exponentialRampToValueAtTime(0.0001, at + dur);
      s.connect(fl).connect(g).connect(dest); s.start(at, Math.random() * 1.5); s.stop(at + dur + 0.02);
      return { s, fl, g };
    };
    switch (name) {
      case 'move': blip(520, 0.04, 'square', 0.05); break;
      case 'rotate': blip(700, 0.06, 'triangle', 0.09, 1.4); break;
      case 'rotateFail': blip(180, 0.12, 'square', 0.08, 0.7); break;
      case 'soft': blip(300, 0.03, 'triangle', 0.03); break;
      case 'hold': blip(440, 0.1, 'triangle', 0.1, 1.5); blip(660, 0.1, 'triangle', 0.08, 1.5); break;
      case 'lock': blip(160, 0.1, 'sine', 0.3, 0.5); noise(0.05, 2000, 1, 0.08); break;
      case 'hard': blip(120, 0.18, 'sine', 0.5, 0.4); noise(0.12, 800, 0.8, 0.25, 'lowpass'); break;
      case 'clear': {
        const n = opt.n || 1;
        const notes = [523, 659, 784, 1047, 1319];
        for (let i = 0; i < n + 1; i++) {
          const o = ctx.createOscillator(), g = ctx.createGain(); o.type = pack === 'soft' ? 'sine' : 'triangle';
          const tt = t + i * 0.06; o.frequency.setValueAtTime(notes[i] * (opt.pitch || 1), tt);
          g.gain.setValueAtTime(0.0001, tt); g.gain.exponentialRampToValueAtTime(0.16 * v, tt + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, tt + 0.35);
          o.connect(g).connect(out); g.connect(this.revSend); o.start(tt); o.stop(tt + 0.4);
        }
        noise(0.4, 5000, 0.5, 0.06 * n, 'highpass');
        break;
      }
      case 'tetris': for (let i = 0; i < 6; i++) blip(523 * Math.pow(1.122, i), 0.25, 'square', 0.07, 1, out); this.sfx('clear', { n: 4 }); break;
      case 'combo': blip(600 + (opt.n || 1) * 80, 0.15, 'triangle', 0.12, 1.5); break;
      case 'levelup': [523, 659, 784, 1047].forEach((f, i) => setTimeout(() => this.sfx('ping', { f }), i * 90)); break;
      case 'ping': blip(opt.f || 880, 0.3, 'sine', 0.15); break;
      case 'ui': blip(880, 0.05, 'triangle', 0.08, 1.3); break;
      case 'uiBack': blip(500, 0.07, 'triangle', 0.07, 0.7); break;
      case 'pause': blip(400, 0.15, 'sine', 0.12, 0.5); break;
      case 'gameover': [400, 300, 220, 140].forEach((f, i) => this.sfx('ping', { f, delay: i * 0.18 })); break;
      case 'glitch': {
        for (let i = 0; i < 6; i++) {
          const tt = t + i * 0.03;
          const o = ctx.createOscillator(), g = ctx.createGain(); o.type = 'square'; o.frequency.value = 80 + Math.random() * 2000;
          g.gain.setValueAtTime(0.06 * v, tt); g.gain.setValueAtTime(0, tt + 0.025);
          o.connect(g).connect(out); o.start(tt); o.stop(tt + 0.03);
        }
        noise(0.2, 3000, 0.5, 0.15);
        break;
      }
      case 'whisper': {
        const { fl } = noise(1.4, 900, 8, 0.5 * (opt.vol || 1), 'bandpass', this.revSend);
        fl.frequency.setValueAtTime(700, t); fl.frequency.linearRampToValueAtTime(1400, t + 0.5); fl.frequency.linearRampToValueAtTime(500, t + 1.3);
        noise(1.4, 2500, 6, 0.2, 'bandpass', out);
        break;
      }
      case 'reverse': {
        // son « inversé » : montée lente puis coupure sèche
        const o = ctx.createOscillator(), g = ctx.createGain(), f = ctx.createBiquadFilter();
        o.type = 'sawtooth'; o.frequency.setValueAtTime(opt.f || 220, t); f.type = 'lowpass'; f.frequency.setValueAtTime(300, t); f.frequency.exponentialRampToValueAtTime(4000, t + 1.2);
        g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.25 * v, t + 1.2); g.gain.setValueAtTime(0, t + 1.21);
        o.connect(f).connect(g).connect(out); g.connect(this.revSend); o.start(t); o.stop(t + 1.25);
        const s = noise(1.2, 1200, 1, 0.0001, 'bandpass'); s.g.gain.cancelScheduledValues(t); s.g.gain.setValueAtTime(0.0001, t); s.g.gain.exponentialRampToValueAtTime(0.3 * v, t + 1.2); s.g.gain.setValueAtTime(0, t + 1.21);
        break;
      }
      case 'stinger': {
        // accord dissonant brutal
        for (const f of [146.8, 155.6, 207.7, 311.1, 622]) {
          const o = ctx.createOscillator(), g = ctx.createGain(); o.type = 'sawtooth'; o.frequency.value = f;
          g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.09 * v, t + 0.02); g.gain.exponentialRampToValueAtTime(0.0001, t + 1.8);
          o.connect(g).connect(out); g.connect(this.revSend); o.start(t); o.stop(t + 1.9);
        }
        noise(0.6, 200, 0.5, 0.4, 'lowpass');
        break;
      }
      case 'creak': {
        const o = ctx.createOscillator(), g = ctx.createGain(), f = ctx.createBiquadFilter();
        o.type = 'sawtooth'; o.frequency.setValueAtTime(40, t); o.frequency.linearRampToValueAtTime(55, t + 0.8);
        f.type = 'bandpass'; f.frequency.value = 900; f.Q.value = 12;
        const lfo = ctx.createOscillator(), lg = ctx.createGain(); lfo.frequency.value = 22; lg.gain.value = 30; lfo.connect(lg).connect(o.frequency);
        g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.5 * v, t + 0.1); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.9);
        o.connect(f).connect(g).connect(out); o.start(t); lfo.start(t); o.stop(t + 1); lfo.stop(t + 1);
        break;
      }
      case 'eyeOpen': blip(60, 0.8, 'sine', 0.4, 0.6); noise(0.5, 300, 3, 0.2); blip(2400, 0.4, 'sine', 0.03, 0.9); break;
      case 'breath': {
        const n1 = noise(1.2, 700, 0.8, 0.0001, 'bandpass', this.revSend);
        n1.g.gain.cancelScheduledValues(t); n1.g.gain.setValueAtTime(0.0001, t); n1.g.gain.exponentialRampToValueAtTime(0.35 * v, t + 0.5); n1.g.gain.exponentialRampToValueAtTime(0.0001, t + 1.2);
        break;
      }
      case 'knock': case 'punch': {
        blip(name === 'punch' ? 55 : 90, 0.35, 'sine', 0.9, 0.5);
        noise(0.25, 400, 0.7, 0.6, 'lowpass');
        if (name === 'punch') noise(0.5, 3000, 0.5, 0.25, 'highpass');
        break;
      }
      case 'crack': {
        for (let i = 0; i < 14; i++) {
          const at = t + i * 0.012 + Math.random() * 0.03;
          noise(0.05 + Math.random() * 0.05, 2500 + Math.random() * 5000, 4, 0.35, 'bandpass', out, at);
        }
        blip(3000, 0.4, 'sine', 0.04, 0.7);
        break;
      }
      case 'shatter': {
        noise(1.4, 3000, 0.4, 0.7, 'highpass');
        noise(0.8, 200, 0.5, 0.8, 'lowpass');
        for (let i = 0; i < 26; i++) {
          const o = ctx.createOscillator(), g = ctx.createGain(); const at = t + Math.random() * 1.1;
          o.type = 'sine'; o.frequency.value = 2500 + Math.random() * 6000;
          g.gain.setValueAtTime(0.05 * v, at); g.gain.exponentialRampToValueAtTime(0.0001, at + 0.15);
          o.connect(g).connect(out); o.start(at); o.stop(at + 0.16);
        }
        break;
      }
      case 'scream': case 'roar': {
        const scream = name === 'scream';
        const dur = opt.dur || (scream ? 2.0 : 1.4);
        const vol = (this.soft ? 0.35 : 1) * v;
        const sh = ctx.createWaveShaper(); sh.curve = this._curve(scream ? 120 : 200);
        const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.5 * vol, t + 0.06);
        g.gain.setValueAtTime(0.5 * vol, t + dur * 0.6); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
        const base = scream ? 330 : 70;
        for (const r of [1, 1.07, 1.51, 2.02]) {
          const o = ctx.createOscillator(); o.type = 'sawtooth';
          o.frequency.setValueAtTime(base * r * 0.6, t); o.frequency.exponentialRampToValueAtTime(base * r * 1.4, t + 0.25);
          o.frequency.exponentialRampToValueAtTime(base * r * 0.7, t + dur);
          const vib = ctx.createOscillator(), vg = ctx.createGain(); vib.frequency.value = 17 + Math.random() * 9; vg.gain.value = base * r * 0.06;
          vib.connect(vg).connect(o.frequency);
          o.connect(sh); o.start(t); vib.start(t); o.stop(t + dur + 0.05); vib.stop(t + dur + 0.05);
        }
        const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.frequency.value = scream ? 1500 : 500; f.Q.value = 0.8;
        sh.connect(f).connect(g); g.connect(out); g.connect(this.revSend);
        noise(dur, scream ? 2200 : 600, 1.2, 0.45 * vol, 'bandpass');
        if (!scream) blip(45, dur, 'sine', 0.7 * vol, 0.7);
        break;
      }
      case 'jumpscare': this.sfx('stinger', { vol: v }); this.sfx('scream', { vol: v }); this.sfx('punch', { vol: v * 0.8 }); break;
      case 'thud': blip(70, 0.3, 'sine', 0.7, 0.4); noise(0.15, 300, 1, 0.3, 'lowpass'); break;
      case 'step': noise(0.06, 300 + Math.random() * 200, 1, 0.18, 'lowpass'); break;
      case 'monsterStep': blip(50, 0.18, 'sine', 0.6, 0.5); noise(0.1, 200, 1, 0.3, 'lowpass'); noise(0.05, 4000, 2, 0.08); break;
      case 'skitter': for (let i = 0; i < 8; i++) noise(0.02, 3000 + Math.random() * 2000, 5, 0.12, 'bandpass', out, t + i * 0.035); break;
      case 'jump': blip(300, 0.18, 'square', 0.08, 2); break;
      case 'slide': noise(0.35, 1200, 0.6, 0.2, 'bandpass'); break;
      case 'hit': blip(110, 0.25, 'sawtooth', 0.25, 0.5); noise(0.2, 900, 1, 0.4); break;
      case 'door': blip(60, 0.6, 'sawtooth', 0.15, 0.8); noise(0.6, 250, 2, 0.4, 'lowpass'); break;
      case 'whoosh': { const n = noise(0.5, 400, 1.5, 0.35, 'bandpass'); n.fl.frequency.setValueAtTime(300, t); n.fl.frequency.exponentialRampToValueAtTime(3000, t + 0.45); break; }
      case 'key': [988, 1319, 1976].forEach((f, i) => blip(f, 0.25, 'sine', 0.12, 1, out)); break;
      case 'unlock': [523, 784, 1047, 1568].forEach((f, i) => this.sfx('ping', { f, delay: i * 0.08 })); break;
      case 'bossHit': blip(90, 0.5, 'sawtooth', 0.3, 0.4); noise(0.6, 600, 0.6, 0.5, 'lowpass'); this.sfx('glitch'); break;
      case 'heart': this._thump(t, 0.9 * v); this._thump(t + 0.22, 0.6 * v); break;
      case 'click': blip(1500, 0.02, 'square', 0.06); break;
      case 'drop': blip(900, 0.5, 'sine', 0.15, 0.3); break;
      case 'tv': noise(0.3, 6000, 0.5, 0.25, 'highpass'); blip(15700, 0.4, 'sine', 0.02); break;
      default: break;
    }
  }
}
