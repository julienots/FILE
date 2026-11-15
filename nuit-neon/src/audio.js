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
