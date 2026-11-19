// Sons du jeu (Web Audio, démarrés au premier appui).
//
// Chaîne de sortie unique : effets + radio -> compresseur doux -> limiteur -> écrêtage doux -> haut-parleurs.
// Rien ne peut donc dépasser le niveau maximal (c'était la cause des grésillements : la radio et les effets
// avaient chacun leur compresseur et s'additionnaient au-delà de 0 dB). Le contexte audio est ouvert en mode
// « lecture » (tampons plus grands) pour ne plus avoir de craquements quand le téléphone est très occupé.
//
// Moteurs : un petit processeur audio (AudioWorklet) simule les impulsions d'échappement de chaque cylindre
// selon l'ordre d'allumage réel (V8 à vilebrequin croisé, V10, V12, flat-4, bicylindre en V…), les fait résonner
// dans deux lignes d'échappement (une par banc, gauche et droite) et un silencieux, ajoute le bruit d'admission
// et la mécanique. Le régime suit une boîte à 7 rapports automatique (montée, rétrogradage avec coup de gaz).
// Si l'appareil ne sait pas faire tourner ce processeur, l'ancien moteur à oscillateurs prend le relais.
let ctx = null, master = null, analyser = null, on = true, bus = null, musicIn = null, wk = false;

// Caractère de chaque moteur pour le moteur de secours à oscillateurs (et volume relatif de chaque type).
export const MOTEURS = {
  v8: { idle: 34, red: 185, mix: [1, 1, 0.3], h: 2, lump: 0.5, drive: 4.5, cut: [260, 2500], roar: 0.55, turbo: 0, crackle: 1, vol: 1.1 },
  v10: { idle: 50, red: 300, mix: [1, 0.5, 0.6], h: 2.5, lump: 0.15, drive: 3.2, cut: [450, 4200], roar: 0.4, turbo: 0, crackle: 0.8, vol: 1 },
  v12: { idle: 56, red: 360, mix: [1, 0.35, 0.75], h: 3, lump: 0.08, drive: 2.6, cut: [550, 5200], roar: 0.35, turbo: 0, crackle: 0.5, vol: 1 },
  biturbo: { idle: 44, red: 260, mix: [1, 0.7, 0.45], h: 2, lump: 0.22, drive: 3.6, cut: [380, 3400], roar: 0.5, turbo: 1, crackle: 1, vol: 1.05 },
  f1: { idle: 80, red: 560, mix: [1, 0.15, 0.9], h: 2, lump: 0.03, drive: 2.2, cut: [800, 7000], roar: 0.25, turbo: 0.5, crackle: 0.3, vol: 0.95 },
  proto: { idle: 60, red: 420, mix: [1, 0.3, 0.8], h: 2, lump: 0.06, drive: 3, cut: [600, 6000], roar: 0.4, turbo: 0.8, crackle: 0.7, vol: 1 },
  buggy: { idle: 40, red: 230, mix: [1, 0.8, 0.5], h: 1.5, lump: 0.6, drive: 5, cut: [320, 3000], roar: 0.6, turbo: 0, crackle: 1, vol: 1 },
  moto: { idle: 72, red: 470, mix: [1, 0.2, 0.85], h: 2, lump: 0.1, drive: 3.4, cut: [600, 5600], roar: 0.3, turbo: 0, crackle: 0.6, vol: 0.9 },
  custom: { idle: 42, red: 250, mix: [1, 0.9, 0.5], h: 1.5, lump: 0.7, drive: 4.2, cut: [330, 3200], roar: 0.5, turbo: 0, crackle: 1, vol: 1 },   // bicylindre de roadster
};

// Moteurs « physiques » : nombre de cylindres, ordre d'allumage (phase dans le cycle de 720°, banc 0 = gauche, 1 = droite),
// régime de ralenti et rupteur (tr/min), rugosité (irrégularité du ralenti), largeur des impulsions, résonances de
// l'échappement [fréquence, Q, gain], longueur des tubes (Hz) et retour, ouverture du silencieux, admission, mécanique, saturation.
const even = (n, banks) => Array.from({ length: n }, (_, k) => [k / n, banks[k % banks.length]]);
const PHYS = {
  // V8 à vilebrequin croisé : ordre 1-8-4-3-6-5-7-2, les bancs reçoivent leurs impulsions de façon inégale -> le « glouglou » du V8
  v8: { fire: [0, 1, 1, 0, 1, 0, 0, 1].map((b, k) => [k / 8, b]), idle: 760, red: 7000, rough: 0.34, tau: 0.34, res: [[92, 1.5, 1], [410, 2.4, 0.5], [1250, 3, 0.16]], comb: [74, 0.46], muf: [650, 5200], intake: 0.32, mech: 0.25, drive: 2.3, vol: 1.05 },
  v10: { fire: even(10, [0, 1]), idle: 1000, red: 8800, rough: 0.12, tau: 0.26, res: [[128, 1.7, 1], [620, 2.6, 0.6], [1900, 3, 0.3]], comb: [108, 0.36], muf: [1100, 9000], intake: 0.42, mech: 0.3, drive: 2.6, vol: 1 },
  v12: { fire: even(12, [0, 1]), idle: 900, red: 9000, rough: 0.06, tau: 0.22, res: [[150, 1.5, 0.9], [780, 2.2, 0.6], [2400, 2.5, 0.35]], comb: [124, 0.3], muf: [1300, 10000], intake: 0.5, mech: 0.3, drive: 2.0, vol: 1 },
  // V8 à vilebrequin plat, deux turbos qui étouffent l'échappement
  biturbo: { fire: even(8, [0, 1]), idle: 850, red: 7600, rough: 0.16, tau: 0.3, res: [[108, 1.5, 1], [500, 2, 0.42], [1500, 2.5, 0.14]], comb: [88, 0.4], muf: [560, 3800], intake: 0.6, mech: 0.25, drive: 2.4, vol: 1.05 },
  // V10 atmosphérique de course : 18 000 tr/min
  f1: { fire: even(10, [0, 1]), idle: 4200, red: 18000, rough: 0.03, tau: 0.2, res: [[300, 1.4, 0.8], [1300, 2, 0.7], [3600, 2.4, 0.4]], comb: [220, 0.3], muf: [2400, 14000], intake: 0.6, mech: 0.35, drive: 2.8, vol: 0.85 },
  // V6 biturbo de prototype
  proto: { fire: even(6, [0, 1]), idle: 1500, red: 10500, rough: 0.08, tau: 0.24, res: [[180, 1.6, 0.9], [900, 2.2, 0.55], [2600, 2.6, 0.3]], comb: [150, 0.35], muf: [1400, 9000], intake: 0.55, mech: 0.3, drive: 2.5, vol: 1 },
  // quatre-cylindres à plat refroidi par air
  buggy: { fire: [[0, 0], [0.25, 1], [0.5, 0], [0.75, 1]], idle: 900, red: 6200, rough: 0.42, tau: 0.36, res: [[84, 1.4, 1], [350, 2.2, 0.6], [1100, 3, 0.25]], comb: [60, 0.5], muf: [760, 4500], intake: 0.3, mech: 0.55, drive: 3.0, vol: 1 },
  // quatre-cylindres en ligne de moto sportive : boîte à air qui hurle
  moto: { fire: [[0, 0], [0.25, 1], [0.5, 1], [0.75, 0]], idle: 1300, red: 14000, rough: 0.1, tau: 0.28, res: [[160, 1.6, 0.9], [700, 2.4, 0.6], [2300, 3, 0.35]], comb: [130, 0.35], muf: [1200, 9000], intake: 0.75, mech: 0.35, drive: 2.6, vol: 0.92 },
  // bicylindre en V à 45° : allumages à 315° puis 405° -> « patate »
  custom: { fire: [[0, 0], [315 / 720, 1]], idle: 850, red: 6800, rough: 0.5, tau: 0.38, res: [[74, 1.4, 1], [300, 2, 0.5], [1000, 3, 0.2]], comb: [55, 0.5], muf: [600, 4000], intake: 0.3, mech: 0.4, drive: 3.2, vol: 1.05 },
};

// processeur audio du moteur (tourne dans le fil audio, échantillon par échantillon)
const WORKLET = `
const TAU = Math.PI * 2;
class Moteur extends AudioWorkletProcessor {
  static get parameterDescriptors() { return [
    { name: 'rpm', defaultValue: 900, minValue: 0, maxValue: 40000, automationRate: 'k-rate' },
    { name: 'load', defaultValue: 0, minValue: 0, maxValue: 1, automationRate: 'k-rate' },
    { name: 'gain', defaultValue: 0, minValue: 0, maxValue: 4, automationRate: 'k-rate' } ]; }
  constructor() {
    super(); this.c = null; this.ph = 0; this.rpm = 900; this.load = 0; this.g = 0; this.seed = 12345;
    this.hp = [0, 0, 0, 0]; this.lp = [0, 0, 0, 0]; this.it = [0, 0]; this.dc = [0, 0, 0, 0];
    this.port.onmessage = (e) => this.cfg(e.data);
  }
  rnd() { this.seed = (this.seed * 1664525 + 1013904223) >>> 0; return this.seed / 4294967296; }
  // filtre à variable d'état (TPT) : sortie passe-bande de gain 1 au sommet
  bp(r, x) { const v3 = x - r.s2, v1 = r.a1 * r.s1 + r.a2 * v3, v2 = r.s2 + r.a2 * r.s1 + r.a3 * v3; r.s1 = 2 * v1 - r.s1; r.s2 = 2 * v2 - r.s2; return v1 * r.k; }
  svf(f, q) { const g = Math.tan(Math.PI * Math.min(f, sampleRate * 0.45) / sampleRate), k = 1 / q, a1 = 1 / (1 + g * (g + k)); return { g, k, a1, a2: g * a1, a3: g * g * a1, s1: 0, s2: 0 }; }
  cfg(P) {
    const n = P.fire.length, banks = [0, 1].map(b => ({ res: P.res.map(r => Object.assign(this.svf(r[0] * (b ? 1.06 : 1), r[1]), { amp: r[2] })),
      len: Math.max(8, Math.round(sampleRate / (P.comb[0] * (b ? 0.94 : 1)))), buf: null, w: 0, fb: P.comb[1], dl: 0 }));
    for (const b of banks) b.buf = new Float32Array(b.len + 2);
    this.c = { n, ph: P.fire.map(f => f[0]), bank: P.fire.map(f => f[1]), amp: P.fire.map(() => 1 + (this.rnd() - 0.5) * 0.18), jit: new Float32Array(n), last: new Float32Array(n).fill(1),
      rough: P.rough, tau: P.tau, banks, muf: P.muf, intake: P.intake, mech: P.mech, drive: P.drive, norm: 1 / Math.sqrt(Math.max(1, n / 4)), ib: this.svf(1800, 1.2), mb: this.svf(5200, 2) };
  }
  process(inp, out, prm) {
    const o = out[0], L = o[0], R = o[1] || o[0], N = L.length, C = this.c;
    if (!C) { L.fill(0); if (R !== L) R.fill(0); return true; }
    const sm = 1 - Math.exp(-1 / (0.012 * sampleRate)), sg = 1 - Math.exp(-1 / (0.03 * sampleRate));
    const tR = prm.rpm[0], tL = prm.load[0], tG = prm.gain[0];
    // silencieux : s'ouvre avec la charge et le régime (coefficient calculé par bloc, valeurs lissées)
    const rpmN = Math.min(1, this.rpm / 9000), fc = C.muf[0] + (C.muf[1] - C.muf[0]) * Math.min(1, 0.35 * rpmN + 0.75 * this.load), lpa = 1 - Math.exp(-TAU * fc / sampleRate);
    for (let i = 0; i < N; i++) {
      this.rpm += (tR - this.rpm) * sm; this.load += (tL - this.load) * sm; this.g += (tG - this.g) * sg;
      const cyc = this.rpm / 120, per = 1 / Math.max(1, cyc * C.n), tau = Math.min(0.0045, Math.max(0.00022, per * C.tau));
      this.ph += cyc / sampleRate; if (this.ph >= 1) this.ph -= 1;
      const ld = 0.18 + 0.82 * this.load, rough = C.rough * (1.2 - Math.min(1, this.rpm / 4000)) * (1.3 - this.load * 0.6) + 0.02;
      let b0 = 0, b1 = 0, env = 0;
      for (let k = 0; k < C.n; k++) {
        let d = this.ph - C.ph[k]; if (d < 0) d += 1;
        if (d < C.last[k]) C.jit[k] = (this.rnd() - 0.5) * 2 * rough;      // nouvelle explosion de ce cylindre : force un peu différente à chaque fois
        C.last[k] = d;
        const x = d / cyc / tau; if (x > 9) continue;
        const p = x * Math.exp(1 - x) * (1 - 0.38 * x) * C.amp[k] * (1 + C.jit[k]) * ld;
        if (C.bank[k]) b1 += p; else b0 += p; env += Math.abs(p);
      }
      // admission : souffle modulé par les impulsions, plus fort à pleine charge et haut régime
      const nz = this.rnd() * 2 - 1;
      const intake = this.bp(C.ib, nz) * C.intake * this.load * (0.25 + rpmN) * (0.3 + env * 0.7) * 0.22;
      // mécanique : cliquetis aigu des soupapes
      const mech = this.bp(C.mb, nz) * C.mech * 0.025 * (0.3 + rpmN) * (0.5 + env);
      const outB = this.ob || (this.ob = [0, 0]);
      for (let b = 0; b < 2; b++) {
        const B = C.banks[b]; let x = (b ? b1 : b0) * C.norm;
        // ligne d'échappement : guide d'onde (écho du tube, amorti dans les aigus)
        const rd = B.buf[B.w]; B.dl += (rd - B.dl) * 0.45; const y = x + B.fb * B.dl; B.buf[B.w] = y; B.w = (B.w + 1) % B.len;
        // résonances du collecteur et du silencieux (filtres passe-bande)
        let s = y * 0.35;
        for (const r of B.res) s += this.bp(r, y) * r.amp * 1.6;
        // silencieux (deux passe-bas)
        this.lp[b * 2] += (s - this.lp[b * 2]) * lpa; this.lp[b * 2 + 1] += (this.lp[b * 2] - this.lp[b * 2 + 1]) * lpa;
        let z = this.lp[b * 2 + 1] + intake * (b ? 0.8 : 1.2) + mech;
        // suppression de la composante continue
        const dcv = z - this.dc[b * 2] + 0.997 * this.dc[b * 2 + 1]; this.dc[b * 2] = z; this.dc[b * 2 + 1] = dcv; z = dcv;
        // saturation douce
        z *= C.drive; z = z / (1 + Math.abs(z)) * 1.35;
        outB[b] = z;
      }
      L[i] = (outB[0] + outB[1] * 0.42) * this.g; R[i] = (outB[1] + outB[0] * 0.42) * this.g;
    }
    return true;
  }
}
registerProcessor('moteur', Moteur);
`;

function shaper(k) { const n = 512, c = new Float32Array(n); for (let i = 0; i < n; i++) { const x = i / (n - 1) * 2 - 1; c[i] = Math.tanh(x * k) / Math.tanh(k); } return c; }
// écrêtage doux : linéaire jusqu'à 0,7 puis courbe qui n'atteint jamais 1
function softClip() { const n = 2048, c = new Float32Array(n); for (let i = 0; i < n; i++) { const x = (i / (n - 1)) * 4 - 2, a = Math.abs(x); c[i] = Math.sign(x) * (a < 0.7 ? a : 0.7 + 0.29 * Math.tanh((a - 0.7) / 0.29)); } return c; }

// ---------- moteur de secours : trois oscillateurs + souffle -> saturation -> filtre -> battement du ralenti ----------
function makeVoice() {
  const v = { o: [], g: [] };
  v.pre = ctx.createGain(); v.sh = ctx.createWaveShaper(); v.sh.oversample = '4x'; v.filt = ctx.createBiquadFilter(); v.filt.type = 'lowpass'; v.filt.Q.value = 1.4;
  v.am = ctx.createGain(); v.out = ctx.createGain(); v.out.gain.value = 0;
  for (const type of ['sawtooth', 'square', 'sawtooth']) { const o = ctx.createOscillator(); o.type = type; const g = ctx.createGain(); g.gain.value = 0; o.connect(g); g.connect(v.pre); o.start(); v.o.push(o); v.g.push(g); }
  v.ns = ctx.createBufferSource(); v.ns.buffer = noise(); v.ns.loop = true; v.nf = ctx.createBiquadFilter(); v.nf.type = 'bandpass'; v.nf.Q.value = 0.9; v.ng = ctx.createGain(); v.ng.gain.value = 0;
  v.ns.connect(v.nf); v.nf.connect(v.ng); v.ng.connect(v.pre); v.ns.start();
  v.lfo = ctx.createOscillator(); v.lfo.type = 'sine'; v.ld = ctx.createGain(); v.ld.gain.value = 0; v.lfo.connect(v.ld); v.ld.connect(v.am.gain); v.lfo.start();
  v.pre.connect(v.sh); v.sh.connect(v.filt); v.filt.connect(v.am); v.am.connect(v.out);
  v.prof = null; return v;
}
function setVoice(v, P, rpm, thr, vol, pitch = 1) {
  const t = ctx.currentTime;
  if (v.prof !== P) { v.prof = P; v.sh.curve = shaper(P.drive); }
  const f = (P.idle + (P.red - P.idle) * rpm) * pitch, load = 0.35 + 0.65 * thr;
  v.o[0].frequency.setTargetAtTime(f, t, 0.03); v.o[1].frequency.setTargetAtTime(f * 0.5, t, 0.03); v.o[2].frequency.setTargetAtTime(f * P.h * 1.004, t, 0.03);
  v.g[0].gain.setTargetAtTime(P.mix[0] * 0.5, t, 0.05); v.g[1].gain.setTargetAtTime(P.mix[1] * 0.5 * (1.1 - rpm * 0.5), t, 0.05); v.g[2].gain.setTargetAtTime(P.mix[2] * 0.4 * (0.3 + rpm * 0.9) * load, t, 0.05);
  v.nf.frequency.setTargetAtTime(f * 2.2 + 200, t, 0.05); v.ng.gain.setTargetAtTime(P.roar * (0.15 + thr * 0.85) * (0.3 + rpm), t, 0.05);
  v.filt.frequency.setTargetAtTime(P.cut[0] + (P.cut[1] - P.cut[0]) * Math.min(1, rpm * 0.55 + thr * 0.5), t, 0.05);
  v.lfo.frequency.setTargetAtTime(Math.max(4, f * 0.25), t, 0.05); v.ld.gain.setTargetAtTime(P.lump * (1 - rpm * 0.8), t, 0.1);
  v.out.gain.setTargetAtTime(vol * P.vol * (0.11 + 0.2 * rpm + 0.34 * thr), t, 0.05);
}

// ---------- moteur « physique » (processeur audio) ----------
function makeEngine() {
  const n = new AudioWorkletNode(ctx, 'moteur', { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
  const e = { node: n, rpm: n.parameters.get('rpm'), load: n.parameters.get('load'), gain: n.parameters.get('gain'), type: null };
  e.lp = ctx.createBiquadFilter(); e.lp.type = 'lowpass'; e.lp.frequency.value = 18000; e.lp.Q.value = 0.5;
  e.pan = ctx.createStereoPanner ? ctx.createStereoPanner() : ctx.createGain();
  e.out = ctx.createGain(); e.out.gain.value = 1;
  n.connect(e.lp); e.lp.connect(e.pan); e.pan.connect(e.out);
  return e;
}
function engType(e, type) { if (e.type === type) return; e.type = type; e.node.port.postMessage(PHYS[type] || PHYS.v12); }

let me = null, rival = null, whistle = null, whistleG = null, whN = null, whNF = null, windF = null, windG = null, jetG = null, cutG = null;
let roadG = null, roadF = null, hissG = null, superG = null, superO = null, wetLvl = 0;
const st = { rpm: 0.1, gear: 0, spool: 0, thr: 0, last: 0, limit: 0, cut: 0, blip: 0, type: null };
const rv = { rpm: 0.2, gear: 0 };

async function loadWorklet() {
  try {
    if (!ctx.audioWorklet || typeof AudioWorkletNode === 'undefined') return;
    const url = URL.createObjectURL(new Blob([WORKLET], { type: 'application/javascript' }));
    await ctx.audioWorklet.addModule(url); URL.revokeObjectURL(url);
    const a = makeEngine(), b = makeEngine();
    a.out.connect(cutG); b.out.connect(master);
    // le moteur de secours se tait, le moteur physique prend le relais
    me.out.gain.cancelScheduledValues(0); me.out.gain.value = 0; me.out.disconnect(); rival.out.disconnect();
    me = a; rival = b; wk = true;
  } catch (e) { wk = false; if (typeof window !== "undefined") window.__wkErr = String(e && e.message || e); }
}

export function initAudio() {
  if (ctx) { if (ctx.state === 'suspended') ctx.resume(); return; }
  try {
    const AC = window.AudioContext || window.webkitAudioContext; if (!AC) return;
    try { ctx = new AC({ latencyHint: 'playback' }); } catch (e) { ctx = new AC(); }
    // sortie commune : compresseur doux, limiteur rapide, écrêtage doux suréchantillonné
    bus = ctx.createGain(); bus.gain.value = 1;
    const glue = ctx.createDynamicsCompressor(); glue.threshold.value = -12; glue.knee.value = 10; glue.ratio.value = 3; glue.attack.value = 0.008; glue.release.value = 0.25;
    const lim = ctx.createDynamicsCompressor(); lim.threshold.value = -3; lim.knee.value = 0; lim.ratio.value = 20; lim.attack.value = 0.0015; lim.release.value = 0.12;
    const clip = ctx.createWaveShaper(); clip.curve = softClip(); clip.oversample = '4x';
    const out = ctx.createGain(); out.gain.value = 0.92;
    bus.connect(glue); glue.connect(lim); lim.connect(clip); clip.connect(out); out.connect(ctx.destination);
    analyser = ctx.createAnalyser(); analyser.fftSize = 1024; out.connect(analyser);
    master = ctx.createGain(); master.gain.value = on ? 0.55 : 0; master.connect(bus);
    musicIn = ctx.createGain(); musicIn.gain.value = 1; musicIn.connect(bus);
    cutG = ctx.createGain(); cutG.connect(master);   // cutG : coupure brève aux passages de rapport et au rupteur
    me = makeVoice(); me.out.connect(cutG); rival = makeVoice(); rival.out.connect(master);
    // turbo : sifflement + souffle d'air
    whistle = ctx.createOscillator(); whistle.type = 'sine'; whistleG = ctx.createGain(); whistleG.gain.value = 0; whistle.connect(whistleG); whistleG.connect(master); whistle.start();
    const w = ctx.createBufferSource(); w.buffer = noise(); w.loop = true;
    whNF = ctx.createBiquadFilter(); whNF.type = 'bandpass'; whNF.Q.value = 7; whN = ctx.createGain(); whN.gain.value = 0; w.connect(whNF); whNF.connect(whN); whN.connect(master);
    windF = ctx.createBiquadFilter(); windF.type = 'bandpass'; windF.Q.value = 0.5; windG = ctx.createGain(); windG.gain.value = 0;
    w.connect(windF); windF.connect(windG); windG.connect(master);
    const jf = ctx.createBiquadFilter(); jf.type = 'lowpass'; jf.frequency.value = 520; jetG = ctx.createGain(); jetG.gain.value = 0; w.connect(jf); jf.connect(jetG); jetG.connect(master);
    // roulement des pneus sur l'asphalte (grondement sourd) et chuintement sur route mouillée
    const bn = ctx.createBufferSource(); bn.buffer = brown(); bn.loop = true; roadF = ctx.createBiquadFilter(); roadF.type = 'lowpass'; roadF.frequency.value = 300; roadF.Q.value = 0.7;
    roadG = ctx.createGain(); roadG.gain.value = 0; bn.connect(roadF); roadF.connect(roadG); roadG.connect(master); bn.start();
    const hf = ctx.createBiquadFilter(); hf.type = 'bandpass'; hf.frequency.value = 3800; hf.Q.value = 0.6; hissG = ctx.createGain(); hissG.gain.value = 0; w.connect(hf); hf.connect(hissG); hissG.connect(master);
    w.start();
    // compresseur volumétrique (hypercars) : sifflement mécanique qui suit le régime
    superO = ctx.createOscillator(); superO.type = 'triangle'; const sf = ctx.createBiquadFilter(); sf.type = 'bandpass'; sf.Q.value = 3; sf.frequency.value = 3000; superO.connect(sf);
    superG = ctx.createGain(); superG.gain.value = 0; sf.connect(superG); superG.connect(master); superO.start(); superO.sf = sf;
    loadWorklet();
  } catch (e) { ctx = null; }
}
export function setSound(v) { on = v; if (master) master.gain.setTargetAtTime(v ? 0.55 : 0, ctx.currentTime, 0.05); }
// entrée de la radio dans la sortie commune
export const musicBus = () => musicIn;

// bruit bref filtré : pétarade, soupape, détonation (attaque de 2 ms pour ne pas claquer)
function burst(type, f0, f1, dur, gain, q = 1, at = 0) {
  const t = ctx.currentTime + at, s = ctx.createBufferSource(); s.buffer = noise(); const f = ctx.createBiquadFilter(); f.type = type; f.Q.value = q;
  f.frequency.setValueAtTime(f0, t); f.frequency.exponentialRampToValueAtTime(f1, t + dur);
  const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(gain, t + 0.002); g.gain.exponentialRampToValueAtTime(0.0005, t + dur);
  s.connect(f); f.connect(g); g.connect(master); s.start(t, Math.random() * 0.6, dur + 0.05);
}
// pétarade : détonation sourde dans l'échappement, parfois suivie de deux ou trois claquements
export function pop(force = 1) {
  if (!ctx || !on) return;
  const n = force > 0.6 && Math.random() < 0.5 ? 2 + (Math.random() * 2 | 0) : 1;
  for (let i = 0; i < n; i++) {
    const at = i * (0.03 + Math.random() * 0.05), k = force * (i ? 0.6 + Math.random() * 0.4 : 1);
    burst('lowpass', 1600, 260, 0.07, 0.42 * k, 1.6, at); burst('bandpass', 3200, 1400, 0.03, 0.14 * k, 1.2, at);
    const t = ctx.currentTime + at, o = ctx.createOscillator(), g = ctx.createGain(); o.frequency.setValueAtTime(120 + Math.random() * 40, t); o.frequency.exponentialRampToValueAtTime(45, t + 0.08);
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.32 * k, t + 0.002); g.gain.exponentialRampToValueAtTime(0.0005, t + 0.1); o.connect(g); g.connect(master); o.start(t); o.stop(t + 0.12);
  }
}

// boîte à 7 rapports : vitesse maximale (en part de la vitesse de pointe) atteinte au rupteur dans chaque rapport
const TOPS = [0.19, 0.32, 0.45, 0.58, 0.71, 0.85, 1.0];
// régime (0 = ralenti, 1 = rupteur) dans le rapport g pour la part de vitesse vr
const inGear = (vr, g) => vr / TOPS[g];

// s : { v, vmax, thr, boosting, nitro, air, type, sur } pour le joueur ; r : { type, ratio, dist, rel, pan } pour l'adversaire le plus proche (ou null)
export function engine(s, r) {
  if (!ctx || !me) return;
  const t = ctx.currentTime, dt = Math.min(0.1, Math.max(0.001, t - st.last)); st.last = t;
  if (!s) {
    if (wk) { me.gain.setTargetAtTime(0, t, 0.08); rival.gain.setTargetAtTime(0, t, 0.08); } else { me.out.gain.setTargetAtTime(0, t, 0.08); rival.out.gain.setTargetAtTime(0, t, 0.08); }
    for (const g of [whistleG, whN, windG, jetG, roadG, hissG, superG]) g.gain.setTargetAtTime(0, t, 0.1);
    return;
  }
  const P = MOTEURS[s.type] || MOTEURS.v12, ratio = Math.min(1.12, Math.abs(s.v) / s.vmax), thr = s.thr, sp = Math.abs(s.v);
  // boîte automatique : monte au rupteur à fond, plus tôt à mi-gaz ; rétrograde en décélérant (avec un coup de gaz)
  let g = st.gear;
  const upAt = thr > 0.6 ? 0.97 : 0.5 + 0.45 * thr, downAt = thr > 0.6 ? 0.62 : 0.34;
  if (g < 6 && inGear(ratio, g) > upAt && sp > 3) {
    g++;
    if (thr > 0.4) { st.cut = 0.09; pop(0.35 + P.crackle * 0.35); }
  } else if (g > 0 && inGear(ratio, g - 1) < downAt) { g--; if (thr < 0.4 && sp > 8) { st.blip = 0.16; if (Math.random() < 0.6 * P.crackle) pop(0.3); } }
  if (sp < 1) g = 0;
  st.gear = g;
  let target = inGear(ratio, g);
  if (g === 0) target = Math.max(target, Math.min(0.55, 0.08 + thr * 0.5));    // embrayage qui patine au départ
  if (sp < 2 || s.air) target = Math.max(target, thr * 0.97);                    // à l'arrêt ou en l'air : le moteur monte librement
  if (st.blip > 0) { st.blip -= dt; target = Math.max(target, inGear(ratio, g) + 0.18); }
  target = Math.min(1.02, Math.max(0, target));
  const rate = st.cut > 0 ? 16 : target > st.rpm ? 8 : 5;
  st.rpm += (target - st.rpm) * Math.min(1, rate * dt);
  // rupteur : le moteur coupe l'injection par à-coups quand il plafonne
  let load = thr;
  if (st.rpm > 0.97 && thr > 0.6) { st.limit += dt; if ((st.limit * 14) % 1 < 0.45) load = 0.05; } else st.limit = 0;
  if (st.cut > 0) { st.cut -= dt; load = 0.05; }
  if (st.blip > 0) load = Math.max(load, 0.7);
  if (wk) {
    const E = PHYS[s.type] || PHYS.v12; engType(me, s.type);
    me.rpm.setTargetAtTime(E.idle + (E.red - E.idle) * st.rpm, t, 0.02);
    me.load.setTargetAtTime(load, t, 0.015);
    me.gain.setTargetAtTime(E.vol * (0.5 + 0.18 * st.rpm + 0.32 * thr), t, 0.05);
    cutG.gain.setTargetAtTime(st.cut > 0 ? 0.55 : 1, t, 0.012);
  } else {
    if (st.cut > 0 || load < 0.1 && thr > 0.6) cutG.gain.setTargetAtTime(0.3, t, 0.01); else cutG.gain.setTargetAtTime(1, t, 0.02);
    me.out.connect && setVoice(me, P, st.rpm, thr, 1);
  }
  // turbo : sifflement et souffle qui montent en pression, soupape de décharge (avec son « flutter ») au lever de pied
  const tb = P.turbo || 0;
  if (tb) {
    st.spool += ((thr > 0.5 ? 0.25 + st.rpm * 0.75 : 0) - st.spool) * Math.min(1, (thr > 0.5 ? 2.2 : 6) * dt);
    if (st.thr > 0.5 && thr < 0.2 && st.spool > 0.45) { burst('highpass', 5200, 1600, 0.32, 0.3 * tb); burst('bandpass', 900, 500, 0.2, 0.22 * tb, 3); for (let i = 1; i < 4; i++) burst('bandpass', 1300, 700, 0.04, 0.1 * tb, 2, i * 0.045); st.spool = 0; }
    whistle.frequency.setTargetAtTime(1500 + st.spool * 5200, t, 0.05); whistleG.gain.setTargetAtTime(st.spool * st.spool * 0.035 * tb, t, 0.05);
    whNF.frequency.setTargetAtTime(2000 + st.spool * 4000, t, 0.05); whN.gain.setTargetAtTime(st.spool * st.spool * 0.12 * tb, t, 0.05);
  } else { whistleG.gain.setTargetAtTime(0, t, 0.05); whN.gain.setTargetAtTime(0, t, 0.05); }
  // compresseur mécanique des hypercars
  if (s.sur) { superO.frequency.setTargetAtTime(500 + st.rpm * 2600, t, 0.03); superO.sf.frequency.setTargetAtTime(1000 + st.rpm * 5200, t, 0.03); superG.gain.setTargetAtTime((0.008 + 0.03 * thr) * (0.3 + st.rpm) * Math.min(1, (s.sur - 1) * 3), t, 0.05); }
  else superG.gain.setTargetAtTime(0, t, 0.05);
  // pétarades à la décélération
  if (thr < 0.15 && st.rpm > 0.45 && Math.random() < dt * 7 * P.crackle * st.rpm) pop(0.2 + Math.random() * 0.45);
  st.thr = thr;
  // vent, roulement, route mouillée et réacteur du boost
  const w = Math.min(1, sp / 95), x = Math.min(1, sp / 900), air = s.air ? 0.25 : 1;
  windF.frequency.setTargetAtTime(350 + w * 900 + x * 2500, t, 0.1); windG.gain.setTargetAtTime(w * w * 0.13 + x * 0.25, t, 0.1);
  roadF.frequency.setTargetAtTime(160 + w * 520, t, 0.1); roadG.gain.setTargetAtTime(Math.pow(w, 0.8) * 0.5 * air, t, 0.08);
  hissG.gain.setTargetAtTime(wetLvl * Math.pow(w, 0.9) * 0.22 * air, t, 0.08);
  jetG.gain.setTargetAtTime(s.boosting ? (s.nitro ? 0.6 : 0.36) : 0, t, s.boosting ? 0.04 : 0.15);
  // l'adversaire le plus proche : volume et filtrage selon la distance, effet Doppler, position gauche-droite
  if (r && r.dist < 60) {
    const k = 1 - r.dist / 60, pitch = Math.max(0.8, Math.min(1.25, 1 + r.rel / 140)), vr = Math.max(0, r.ratio);
    let gr = rv.gear; if (gr < 6 && inGear(vr, gr) > 0.95) gr++; else if (gr > 0 && inGear(vr, gr - 1) < 0.6) gr--; rv.gear = gr;
    rv.rpm += (Math.min(1, Math.max(0.1, inGear(vr, gr))) - rv.rpm) * Math.min(1, 6 * dt);
    if (wk) { const E = PHYS[r.type] || PHYS.v12; engType(rival, r.type);
      rival.rpm.setTargetAtTime((E.idle + (E.red - E.idle) * rv.rpm) * pitch, t, 0.03); rival.load.setTargetAtTime(0.85, t, 0.05); rival.gain.setTargetAtTime(E.vol * k * k * 0.75, t, 0.06);
      rival.lp.frequency.setTargetAtTime(1200 + 14000 * k * k, t, 0.06); if (rival.pan.pan) rival.pan.pan.setTargetAtTime(Math.max(-0.85, Math.min(0.85, r.pan || 0)), t, 0.06); }
    else setVoice(rival, MOTEURS[r.type] || MOTEURS.v12, rv.rpm, 0.8, k * k * 0.8, pitch);
  } else if (wk) rival.gain.setTargetAtTime(0, t, 0.1); else rival.out.gain.setTargetAtTime(0, t, 0.1);
}
// niveau sonore mesuré en sortie (tests)
export function level() { if (!analyser) return 0; const d = new Float32Array(analyser.fftSize); analyser.getFloatTimeDomainData(d); let s = 0; for (const x of d) s += x * x; return Math.sqrt(s / d.length); }
export const engineKind = () => wk ? 'worklet' : 'oscillateurs';
// explosion (missile, mine, élimination)
export function boom(force = 1) { if (!ctx || !on) return; burst('lowpass', 2400, 120, 0.7, 0.9 * force, 0.7); const t = ctx.currentTime, o = ctx.createOscillator(), g = ctx.createGain(); o.frequency.setValueAtTime(90, t); o.frequency.exponentialRampToValueAtTime(28, t + 0.5);
  g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.8 * force, t + 0.004); g.gain.exponentialRampToValueAtTime(0.0005, t + 0.6); o.connect(g); g.connect(master); o.start(t); o.stop(t + 0.65); }
export function beep(freq, dur) {
  if (!ctx || !on) return;
  const t = ctx.currentTime, o = ctx.createOscillator(), g = ctx.createGain(); o.type = 'square'; o.frequency.value = freq;
  g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.08, t + 0.005); g.gain.setTargetAtTime(0, t + dur, 0.02);
  o.connect(g); g.connect(master); o.start(t); o.stop(t + dur + 0.2);
}
export const getCtx = () => ctx;
export const soundOn = () => on;
let noiseBuf = null, brownBuf = null;
export function noise() {
  if (!ctx) return null;
  if (!noiseBuf) { noiseBuf = ctx.createBuffer(1, ctx.sampleRate * 2, ctx.sampleRate); const d = noiseBuf.getChannelData(0); for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
    const f = 256; for (let i = 0; i < f; i++) { const a = i / f; d[d.length - f + i] = d[d.length - f + i] * (1 - a) + d[i] * a; } }   // bouclage sans raccord audible
  return noiseBuf;
}
// bruit « brun » (grave), pour le roulement
function brown() {
  if (!brownBuf) { brownBuf = ctx.createBuffer(1, ctx.sampleRate * 2, ctx.sampleRate); const d = brownBuf.getChannelData(0); let l = 0;
    for (let i = 0; i < d.length; i++) { l = (l + 0.02 * (Math.random() * 2 - 1)) / 1.02; d[i] = l * 3.5; }
    const f = 512; for (let i = 0; i < f; i++) { const a = i / f; d[d.length - f + i] = d[d.length - f + i] * (1 - a) + d[i] * a; } }
  return brownBuf;
}
// choc : bruit sourd dont la force suit la violence de l'impact (0..1)
export function thud(force) {
  if (!ctx || !on || force < 0.05) return;
  const t = ctx.currentTime, f = Math.min(1, force);
  const s = ctx.createBufferSource(); s.buffer = noise(); const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 300 + f * 1400;
  const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.2 + f * 0.6, t + 0.003); g.gain.exponentialRampToValueAtTime(0.0005, t + 0.18 + f * 0.25);
  s.connect(lp); lp.connect(g); g.connect(master); s.start(t, Math.random() * 0.5, 0.5);
  const o = ctx.createOscillator(), og = ctx.createGain(); o.frequency.setValueAtTime(110 + f * 60, t); o.frequency.exponentialRampToValueAtTime(40, t + 0.2);
  og.gain.setValueAtTime(0.0001, t); og.gain.linearRampToValueAtTime(0.25 + f * 0.5, t + 0.003); og.gain.exponentialRampToValueAtTime(0.0005, t + 0.25); o.connect(og); og.connect(master); o.start(t); o.stop(t + 0.3);
  // tôle froissée : quelques résonances métalliques brèves
  if (f > 0.35) for (let i = 0; i < 3; i++) burst('bandpass', 900 + Math.random() * 2600, 600 + Math.random() * 800, 0.12 + f * 0.15, 0.12 * f, 9, i * 0.012);
}
// crissement des pneus pendant un dérapage (0 = silence) : deux résonances étroites qui tremblent + souffle
let skidG = null, skidF = null, skidF2 = null, skidN = null;
export function skid(amount) {
  if (!ctx) return;
  if (!skidG) { const s = ctx.createBufferSource(); s.buffer = noise(); s.loop = true; skidF = ctx.createBiquadFilter(); skidF.type = 'bandpass'; skidF.frequency.value = 1100; skidF.Q.value = 14;
    skidF2 = ctx.createBiquadFilter(); skidF2.type = 'bandpass'; skidF2.frequency.value = 2300; skidF2.Q.value = 10;
    const bb = ctx.createBiquadFilter(); bb.type = 'bandpass'; bb.frequency.value = 1500; bb.Q.value = 1.2; skidN = ctx.createGain(); skidN.gain.value = 0.18;
    skidG = ctx.createGain(); skidG.gain.value = 0; s.connect(skidF); s.connect(skidF2); s.connect(bb); bb.connect(skidN);
    const g1 = ctx.createGain(); g1.gain.value = 2.6; const g2 = ctx.createGain(); g2.gain.value = 1.4; skidF.connect(g1); skidF2.connect(g2); g1.connect(skidG); g2.connect(skidG); skidN.connect(skidG); skidG.connect(master); s.start(); }
  const a = on ? Math.min(1, Math.max(0, amount)) : 0, t = ctx.currentTime, wob = Math.random() * 140 - 70;
  skidG.gain.setTargetAtTime(a * 0.2, t, 0.06);
  if (a > 0) { skidF.frequency.setTargetAtTime(950 + a * 450 + wob, t, 0.05); skidF2.frequency.setTargetAtTime(2050 + a * 700 + wob * 2, t, 0.05); }
}
// petit jingle (gain d'argent, achat)
export function chime(notes = [660, 880, 1320]) { notes.forEach((f, i) => setTimeout(() => beep(f, 0.07), i * 70)); }
export function suspend(v) { if (!ctx) return; try { v ? ctx.suspend() : ctx.resume(); } catch (e) { /* contexte fermé */ } }

// frottement de tôle contre un mur ou une autre voiture (0 = silence) : grincement aigu instable + grondement
let scrG = null, scrF = null, scrG2 = null;
export function scrape(amount) {
  if (!ctx) return;
  if (!scrG) { const s = ctx.createBufferSource(); s.buffer = noise(); s.loop = true; scrF = ctx.createBiquadFilter(); scrF.type = 'bandpass'; scrF.frequency.value = 2800; scrF.Q.value = 6;
    scrG = ctx.createGain(); scrG.gain.value = 0; const lf = ctx.createBiquadFilter(); lf.type = 'lowpass'; lf.frequency.value = 420; scrG2 = ctx.createGain(); scrG2.gain.value = 0;
    s.connect(scrF); scrF.connect(scrG); scrG.connect(master); s.connect(lf); lf.connect(scrG2); scrG2.connect(master); s.start(); }
  const a = Math.min(1, Math.max(0, amount)), t = ctx.currentTime;
  scrG.gain.setTargetAtTime(a * 0.42, t, 0.03); scrG2.gain.setTargetAtTime(a * 0.55, t, 0.04);
  if (a > 0) scrF.frequency.setTargetAtTime(1900 + a * 1500 + Math.random() * 1400, t, 0.03);
}
// pluie : crépitement de gouttes (tampon fabriqué une fois : des milliers de petites gouttes qui résonnent), souffle aigu, grondement
let rainG = null, rainBuf = null;
function rainBuffer() {
  const sr = ctx.sampleRate, len = sr * 3, b = ctx.createBuffer(2, len, sr);
  for (let ch = 0; ch < 2; ch++) {
    const d = b.getChannelData(ch); let seed = 991 + ch * 7717; const rnd = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
    for (let n = 0; n < 5200; n++) {
      const at = Math.floor(rnd() * len), f = 1800 + rnd() * rnd() * 7000, amp = 0.015 + rnd() * rnd() * 0.18, dec = 0.0012 + rnd() * 0.004, w = 2 * Math.PI * f / sr, m = Math.floor(dec * sr * 5);
      for (let i = 0; i < m; i++) d[(at + i) % len] += Math.sin(w * i) * Math.exp(-i / (dec * sr)) * amp;   // une goutte : petite résonance qui s'éteint (bouclée sur la fin)
    }
  }
  return b;
}
export function rain(levelR) {
  if (!ctx) return;
  wetLvl = levelR;
  if (!rainG) { const s = ctx.createBufferSource(); s.buffer = noise(); s.loop = true; const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 2400;
    const hs = ctx.createGain(); hs.gain.value = 0.55;
    const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 240; const g2 = ctx.createGain(); g2.gain.value = 1.3;
    rainBuf = rainBuffer(); const dr = ctx.createBufferSource(); dr.buffer = rainBuf; dr.loop = true; const dg = ctx.createGain(); dg.gain.value = 1.6;
    rainG = ctx.createGain(); rainG.gain.value = 0; s.connect(hp); hp.connect(hs); hs.connect(rainG); s.connect(lp); lp.connect(g2); g2.connect(rainG); dr.connect(dg); dg.connect(rainG); rainG.connect(master); s.start(); dr.start(); }
  rainG.gain.setTargetAtTime(levelR * 0.17, ctx.currentTime, 0.4);
}
// tonnerre : claquement sec si la foudre est proche, puis long roulement ; il arrive après l'éclair, selon la distance
export function thunder(dist) {
  if (!ctx || !on) return;
  const t = ctx.currentTime + Math.min(3, dist / 343), near = Math.max(0, 1 - dist / 900), vol = 0.35 + near * 0.9;
  const mk = (type, f0, f1, dur, gain, at) => { const s = ctx.createBufferSource(); s.buffer = noise(); const f = ctx.createBiquadFilter(); f.type = type; f.frequency.setValueAtTime(f0, at); f.frequency.exponentialRampToValueAtTime(f1, at + dur);
    const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, at); g.gain.linearRampToValueAtTime(gain, at + 0.03); g.gain.exponentialRampToValueAtTime(0.0005, at + dur); s.connect(f); f.connect(g); g.connect(master); s.start(at, Math.random() * 0.5, dur + 0.1); };
  if (near > 0.4) mk('highpass', 1800, 600, 0.25, vol * 0.8, t);
  mk('lowpass', 900, 70, 1.6 + Math.random(), vol, t + 0.03); mk('lowpass', 260, 50, 3.2 + Math.random() * 1.5, vol * 0.8, t + 0.25 + Math.random() * 0.4);
}

// traversée d'une flaque : claque d'eau puis ruissellement
export function splash(force = 1) {
  if (!ctx || !on) return;
  const f = Math.min(1.4, force); burst('bandpass', 2600, 700, 0.35 + f * 0.25, 0.5 * f, 0.8); burst('highpass', 5200, 3000, 0.5 + f * 0.3, 0.22 * f, 0.6); burst('lowpass', 500, 120, 0.22, 0.4 * f, 0.9);
}
// roues dans l'herbe, le blé ou le gravier (0 = silence) : frottement sourd et irrégulier
let offG = null, offF = null;
export function offroad(amount) {
  if (!ctx) return;
  if (!offG) { const s = ctx.createBufferSource(); s.buffer = noise(); s.loop = true; offF = ctx.createBiquadFilter(); offF.type = 'bandpass'; offF.frequency.value = 700; offF.Q.value = 0.8;
    offG = ctx.createGain(); offG.gain.value = 0; s.connect(offF); offF.connect(offG); offG.connect(master); s.start(); }
  const a = on ? Math.min(1, Math.max(0, amount)) : 0, t = ctx.currentTime;
  offG.gain.setTargetAtTime(a * (0.32 + Math.random() * 0.12), t, 0.05); if (a > 0) offF.frequency.setTargetAtTime(500 + a * 900 + Math.random() * 300, t, 0.08);
}
