// Sons du jeu (Web Audio, démarrés au premier appui) : moteurs synthétisés, turbo, soupape de décharge, pétarades,
// passages de rapport, souffle du vent, réacteur du boost, moteur de l'adversaire le plus proche, chocs, crissements.
let ctx = null, master = null, analyser = null, on = true;
// Caractère de chaque moteur. idle / red : fréquence d'allumage au ralenti et au rupteur (Hz) ; mix : fondamentale, grave (demi-régime), aigu ;
// lump : irrégularité du ralenti (gros V8) ; drive : saturation ; cut : ouverture du filtre ; turbo : sifflement ; crackle : pétarades au lever de pied.
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
function shaper(k) { const n = 512, c = new Float32Array(n); for (let i = 0; i < n; i++) { const x = i / (n - 1) * 2 - 1; c[i] = Math.tanh(x * k) / Math.tanh(k); } return c; }
// une voix de moteur : trois oscillateurs + souffle d'échappement -> saturation -> filtre -> battement du ralenti -> volume
function makeVoice() {
  const v = { o: [], g: [] };
  v.pre = ctx.createGain(); v.sh = ctx.createWaveShaper(); v.sh.oversample = '2x'; v.filt = ctx.createBiquadFilter(); v.filt.type = 'lowpass'; v.filt.Q.value = 1.4;
  v.am = ctx.createGain(); v.out = ctx.createGain(); v.out.gain.value = 0;
  for (const type of ['sawtooth', 'square', 'sawtooth']) { const o = ctx.createOscillator(); o.type = type; const g = ctx.createGain(); g.gain.value = 0; o.connect(g); g.connect(v.pre); o.start(); v.o.push(o); v.g.push(g); }
  v.ns = ctx.createBufferSource(); v.ns.buffer = noise(); v.ns.loop = true; v.nf = ctx.createBiquadFilter(); v.nf.type = 'bandpass'; v.nf.Q.value = 0.9; v.ng = ctx.createGain(); v.ng.gain.value = 0;
  v.ns.connect(v.nf); v.nf.connect(v.ng); v.ng.connect(v.pre); v.ns.start();
  v.lfo = ctx.createOscillator(); v.lfo.type = 'sine'; v.ld = ctx.createGain(); v.ld.gain.value = 0; v.lfo.connect(v.ld); v.ld.connect(v.am.gain); v.lfo.start();
  v.pre.connect(v.sh); v.sh.connect(v.filt); v.filt.connect(v.am); v.am.connect(v.out); v.out.connect(master);
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
let me = null, rival = null, whistle = null, whistleG = null, windF = null, windG = null, jetG = null, cutG = null;
const st = { rpm: 0.1, gear: 0, spool: 0, thr: 0, last: 0, limit: 0 };
export function initAudio() {
  if (ctx) { if (ctx.state === 'suspended') ctx.resume(); return; }
  try {
    const AC = window.AudioContext || window.webkitAudioContext; if (!AC) return;
    ctx = new AC();
    master = ctx.createGain(); master.gain.value = on ? 0.9 : 0;
    const comp = ctx.createDynamicsCompressor(); comp.threshold.value = -14; comp.ratio.value = 6; comp.attack.value = 0.004; comp.release.value = 0.18;
    analyser = ctx.createAnalyser(); analyser.fftSize = 1024;
    master.connect(comp); comp.connect(analyser); comp.connect(ctx.destination);
    cutG = ctx.createGain(); me = makeVoice(); me.out.disconnect(); me.out.connect(cutG); cutG.connect(master);   // cutG : coupure brève aux passages de rapport et au rupteur
    rival = makeVoice();
    whistle = ctx.createOscillator(); whistle.type = 'sine'; whistleG = ctx.createGain(); whistleG.gain.value = 0; whistle.connect(whistleG); whistleG.connect(master); whistle.start();
    const w = ctx.createBufferSource(); w.buffer = noise(); w.loop = true; windF = ctx.createBiquadFilter(); windF.type = 'bandpass'; windF.Q.value = 0.5; windG = ctx.createGain(); windG.gain.value = 0;
    w.connect(windF); windF.connect(windG); windG.connect(master);
    const jf = ctx.createBiquadFilter(); jf.type = 'lowpass'; jf.frequency.value = 520; jetG = ctx.createGain(); jetG.gain.value = 0; w.connect(jf); jf.connect(jetG); jetG.connect(master); w.start();
  } catch (e) { ctx = null; }
}
export function setSound(v) { on = v; if (master) master.gain.setTargetAtTime(v ? 0.9 : 0, ctx.currentTime, 0.05); }
// bruit bref filtré : pétarade, soupape, détonation
function burst(type, f0, f1, dur, gain, q = 1) {
  const t = ctx.currentTime, s = ctx.createBufferSource(); s.buffer = noise(); const f = ctx.createBiquadFilter(); f.type = type; f.Q.value = q;
  f.frequency.setValueAtTime(f0, t); f.frequency.exponentialRampToValueAtTime(f1, t + dur);
  const g = ctx.createGain(); g.gain.setValueAtTime(gain, t); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
  s.connect(f); f.connect(g); g.connect(master); s.start(t, Math.random() * 0.6, dur + 0.05);
}
export function pop(force = 1) { if (!ctx || !on) return; burst('lowpass', 1400, 300, 0.07, 0.5 * force, 2); const t = ctx.currentTime, o = ctx.createOscillator(), g = ctx.createGain(); o.frequency.setValueAtTime(140, t); o.frequency.exponentialRampToValueAtTime(50, t + 0.08);
  g.gain.setValueAtTime(0.4 * force, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.1); o.connect(g); g.connect(master); o.start(t); o.stop(t + 0.12); }
// s : { v, vmax, thr, boosting, nitro, air, type } pour le joueur ; r : { type, ratio, dist, rel } pour l'adversaire le plus proche (ou null)
export function engine(s, r) {
  if (!ctx || !me) return;
  const t = ctx.currentTime, dt = Math.min(0.1, Math.max(0.001, t - st.last)); st.last = t;
  if (!s) { me.out.gain.setTargetAtTime(0, t, 0.08); rival.out.gain.setTargetAtTime(0, t, 0.08); whistleG.gain.setTargetAtTime(0, t, 0.05); windG.gain.setTargetAtTime(0, t, 0.1); jetG.gain.setTargetAtTime(0, t, 0.1); return; }
  const P = MOTEURS[s.type] || MOTEURS.v12, ratio = Math.min(1.12, Math.abs(s.v) / s.vmax), thr = s.thr;
  // boîte à 7 rapports : le régime retombe à chaque rapport ; à l'arrêt ou en l'air, le moteur monte librement dans les tours
  const g = Math.min(6, Math.floor(ratio * 7)), inG = ratio * 7 - g;
  let target = g === 0 ? 0.12 + 0.88 * inG : 0.42 + 0.58 * Math.min(1, inG);
  if (Math.abs(s.v) < 2 || s.air) target = Math.max(target, thr * 0.97);
  st.rpm += (target - st.rpm) * Math.min(1, (target > st.rpm ? 9 : 5) * dt);
  if (g > st.gear && thr > 0.4 && Math.abs(s.v) > 3) { cutG.gain.cancelScheduledValues(t); cutG.gain.setValueAtTime(0.15, t); cutG.gain.linearRampToValueAtTime(1, t + 0.11); pop(0.5 + P.crackle * 0.5); }   // passage de rapport : coupure et détonation
  st.gear = g;
  // rupteur : le moteur hache quand il plafonne
  if (st.rpm > 0.95 && thr > 0.6) { st.limit += dt; cutG.gain.setTargetAtTime((st.limit * 13 % 1) < 0.5 ? 1 : 0.3, t, 0.008); } else if (st.limit) { st.limit = 0; cutG.gain.setTargetAtTime(1, t, 0.02); }
  setVoice(me, P, st.rpm, thr, 1);
  // turbo : sifflement qui monte en pression, soupape de décharge au lever de pied
  if (P.turbo) {
    st.spool += ((thr > 0.5 ? 0.25 + st.rpm * 0.75 : 0) - st.spool) * Math.min(1, (thr > 0.5 ? 2.2 : 6) * dt);
    if (st.thr > 0.5 && thr < 0.2 && st.spool > 0.45) { burst('highpass', 5200, 1600, 0.32, 0.35 * P.turbo); burst('bandpass', 900, 500, 0.2, 0.25 * P.turbo, 3); st.spool = 0; }
    whistle.frequency.setTargetAtTime(1500 + st.spool * 5200, t, 0.05); whistleG.gain.setTargetAtTime(st.spool * st.spool * 0.05 * P.turbo, t, 0.05);
  } else whistleG.gain.setTargetAtTime(0, t, 0.05);
  // pétarades à la décélération
  if (thr < 0.15 && st.rpm > 0.45 && Math.random() < dt * 9 * P.crackle * st.rpm) pop(0.25 + Math.random() * 0.5);
  st.thr = thr;
  // vent et réacteur du boost
  const sp = Math.abs(s.v), w = Math.min(1, sp / 95), x = Math.min(1, sp / 900);
  windF.frequency.setTargetAtTime(350 + w * 900 + x * 2500, t, 0.1); windG.gain.setTargetAtTime(w * w * 0.16 + x * 0.3, t, 0.1);
  jetG.gain.setTargetAtTime(s.boosting ? (s.nitro ? 0.75 : 0.45) : 0, t, s.boosting ? 0.04 : 0.15);
  // l'adversaire le plus proche : volume selon la distance, hauteur selon la vitesse d'approche (effet Doppler accentué)
  if (r && r.dist < 50) { const gr = Math.min(6, Math.floor(r.ratio * 7)), ir = r.ratio * 7 - gr, k = 1 - r.dist / 50;
    setVoice(rival, MOTEURS[r.type] || MOTEURS.v12, gr === 0 ? 0.12 + 0.88 * ir : 0.42 + 0.58 * Math.min(1, ir), 0.8, k * k * 0.8, Math.max(0.82, Math.min(1.22, 1 + r.rel / 140))); }
  else rival.out.gain.setTargetAtTime(0, t, 0.1);
}
// niveau sonore mesuré en sortie (tests)
export function level() { if (!analyser) return 0; const d = new Float32Array(analyser.fftSize); analyser.getFloatTimeDomainData(d); let s = 0; for (const x of d) s += x * x; return Math.sqrt(s / d.length); }
// explosion (missile, mine, élimination)
export function boom(force = 1) { if (!ctx || !on) return; burst('lowpass', 2400, 120, 0.7, 0.9 * force, 0.7); const t = ctx.currentTime, o = ctx.createOscillator(), g = ctx.createGain(); o.frequency.setValueAtTime(90, t); o.frequency.exponentialRampToValueAtTime(28, t + 0.5);
  g.gain.setValueAtTime(0.8 * force, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.6); o.connect(g); g.connect(master); o.start(t); o.stop(t + 0.65); }
export function beep(freq, dur) {
  if (!ctx || !on) return;
  const o = ctx.createOscillator(), g = ctx.createGain(); o.type = 'square'; o.frequency.value = freq;
  g.gain.value = 0.1; g.gain.setTargetAtTime(0, ctx.currentTime + dur, 0.03);
  o.connect(g); g.connect(master); o.start(); o.stop(ctx.currentTime + dur + 0.2);
}
export const getCtx = () => ctx;
export const soundOn = () => on;
let noiseBuf = null;
export function noise() {
  if (!ctx) return null;
  if (!noiseBuf) { noiseBuf = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate); const d = noiseBuf.getChannelData(0); for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1; }
  return noiseBuf;
}
// choc : bruit sourd dont la force suit la violence de l'impact (0..1)
export function thud(force) {
  if (!ctx || !on || force < 0.05) return;
  const t = ctx.currentTime, f = Math.min(1, force);
  const s = ctx.createBufferSource(); s.buffer = noise(); const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 300 + f * 1400;
  const g = ctx.createGain(); g.gain.setValueAtTime(0.2 + f * 0.6, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.18 + f * 0.25);
  s.connect(lp); lp.connect(g); g.connect(master); s.start(t, Math.random() * 0.5, 0.5);
  const o = ctx.createOscillator(), og = ctx.createGain(); o.frequency.setValueAtTime(110 + f * 60, t); o.frequency.exponentialRampToValueAtTime(40, t + 0.2);
  og.gain.setValueAtTime(0.25 + f * 0.5, t); og.gain.exponentialRampToValueAtTime(0.001, t + 0.25); o.connect(og); og.connect(master); o.start(t); o.stop(t + 0.3);
}
// crissement des pneus pendant un dérapage (0 = silence)
let skidG = null, skidF = null;
export function skid(amount) {
  if (!ctx) return;
  if (!skidG) { const s = ctx.createBufferSource(); s.buffer = noise(); s.loop = true; skidF = ctx.createBiquadFilter(); skidF.type = 'bandpass'; skidF.frequency.value = 1500; skidF.Q.value = 5;
    skidG = ctx.createGain(); skidG.gain.value = 0; s.connect(skidF); skidF.connect(skidG); skidG.connect(master); s.start(); }
  const a = on ? Math.min(1, Math.max(0, amount)) : 0;
  skidG.gain.setTargetAtTime(a * 0.22, ctx.currentTime, 0.06); skidF.frequency.setTargetAtTime(1100 + a * 900, ctx.currentTime, 0.1);
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
  scrG.gain.setTargetAtTime(a * 0.5, t, 0.03); scrG2.gain.setTargetAtTime(a * 0.6, t, 0.04);
  if (a > 0) scrF.frequency.setTargetAtTime(1900 + a * 1500 + Math.random() * 1400, t, 0.02);
}
// pluie battante et vent de l'orage
let rainG = null;
export function rain(levelR) {
  if (!ctx) return;
  if (!rainG) { const s = ctx.createBufferSource(); s.buffer = noise(); s.loop = true; const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 2200;
    const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 260; const g2 = ctx.createGain(); g2.gain.value = 1.6;
    rainG = ctx.createGain(); rainG.gain.value = 0; s.connect(hp); hp.connect(rainG); s.connect(lp); lp.connect(g2); g2.connect(rainG); rainG.connect(master); s.start(); }
  rainG.gain.setTargetAtTime(levelR * 0.16, ctx.currentTime, 0.3);
}
// tonnerre : claquement sec si la foudre est proche, puis long roulement ; il arrive après l'éclair, selon la distance
export function thunder(dist) {
  if (!ctx || !on) return;
  const t = ctx.currentTime + Math.min(3, dist / 343), near = Math.max(0, 1 - dist / 900), vol = 0.35 + near * 0.9;
  const mk = (type, f0, f1, dur, gain, at) => { const s = ctx.createBufferSource(); s.buffer = noise(); const f = ctx.createBiquadFilter(); f.type = type; f.frequency.setValueAtTime(f0, at); f.frequency.exponentialRampToValueAtTime(f1, at + dur);
    const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, at); g.gain.linearRampToValueAtTime(gain, at + 0.03); g.gain.exponentialRampToValueAtTime(0.001, at + dur); s.connect(f); f.connect(g); g.connect(master); s.start(at, Math.random() * 0.5, dur + 0.1); };
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
