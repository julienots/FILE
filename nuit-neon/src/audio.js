// Son du moteur et bips de départ (Web Audio, démarré au premier appui).
let ctx = null, o1, o2, filt, gain, on = true;
export function initAudio() {
  if (ctx) { if (ctx.state === 'suspended') ctx.resume(); return; }
  try {
    const AC = window.AudioContext || window.webkitAudioContext; if (!AC) return;
    ctx = new AC();
    o1 = ctx.createOscillator(); o1.type = 'sawtooth'; o2 = ctx.createOscillator(); o2.type = 'square';
    filt = ctx.createBiquadFilter(); filt.type = 'lowpass'; filt.frequency.value = 600;
    gain = ctx.createGain(); gain.gain.value = 0;
    const g2 = ctx.createGain(); g2.gain.value = 0.35;
    o1.connect(filt); o2.connect(g2); g2.connect(filt); filt.connect(gain); gain.connect(ctx.destination);
    o1.start(); o2.start();
  } catch (e) { ctx = null; }
}
export function setSound(v) { on = v; if (!v && gain) gain.gain.value = 0; }
export function engine(speedRatio, throttle, active) {
  if (!ctx || !gain) return;
  const t = ctx.currentTime;
  if (!on || !active) { gain.gain.setTargetAtTime(0, t, 0.08); return; }
  // boîte à 6 rapports simulée : le régime remonte dans chaque rapport
  const g = Math.min(5, Math.floor(speedRatio * 6)), inGear = speedRatio * 6 - g;
  const rpm = 0.25 + 0.75 * (g === 0 ? speedRatio * 6 : 0.35 + 0.65 * inGear);
  const f = 48 + rpm * 150;
  o1.frequency.setTargetAtTime(f, t, 0.04); o2.frequency.setTargetAtTime(f * 0.5, t, 0.04);
  filt.frequency.setTargetAtTime(350 + rpm * 1500 + throttle * 700, t, 0.05);
  gain.gain.setTargetAtTime(0.035 + throttle * 0.035, t, 0.06);
}
export function beep(freq, dur) {
  if (!ctx || !on) return;
  const o = ctx.createOscillator(), g = ctx.createGain(); o.type = 'square'; o.frequency.value = freq;
  g.gain.value = 0.06; g.gain.setTargetAtTime(0, ctx.currentTime + dur, 0.03);
  o.connect(g); g.connect(ctx.destination); o.start(); o.stop(ctx.currentTime + dur + 0.2);
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
  const g = ctx.createGain(); g.gain.setValueAtTime(0.1 + f * 0.3, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.18 + f * 0.25);
  s.connect(lp); lp.connect(g); g.connect(ctx.destination); s.start(t, Math.random() * 0.5, 0.5);
  const o = ctx.createOscillator(), og = ctx.createGain(); o.frequency.setValueAtTime(110 + f * 60, t); o.frequency.exponentialRampToValueAtTime(40, t + 0.2);
  og.gain.setValueAtTime(0.12 + f * 0.25, t); og.gain.exponentialRampToValueAtTime(0.001, t + 0.25); o.connect(og); og.connect(ctx.destination); o.start(t); o.stop(t + 0.3);
}
// crissement des pneus pendant un dérapage (0 = silence)
let skidG = null, skidF = null;
export function skid(amount) {
  if (!ctx) return;
  if (!skidG) { const s = ctx.createBufferSource(); s.buffer = noise(); s.loop = true; skidF = ctx.createBiquadFilter(); skidF.type = 'bandpass'; skidF.frequency.value = 1500; skidF.Q.value = 5;
    skidG = ctx.createGain(); skidG.gain.value = 0; s.connect(skidF); skidF.connect(skidG); skidG.connect(ctx.destination); s.start(); }
  const a = on ? Math.min(1, Math.max(0, amount)) : 0;
  skidG.gain.setTargetAtTime(a * 0.09, ctx.currentTime, 0.06); skidF.frequency.setTargetAtTime(1100 + a * 900, ctx.currentTime, 0.1);
}
// petit jingle (gain d'argent, achat)
export function chime(notes = [660, 880, 1320]) { notes.forEach((f, i) => setTimeout(() => beep(f, 0.07), i * 70)); }
export function suspend(v) { if (!ctx) return; try { v ? ctx.suspend() : ctx.resume(); } catch (e) { /* contexte fermé */ } }
