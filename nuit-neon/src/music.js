// Musique : six morceaux électroniques joués en direct par un petit séquenceur (aucun fichier audio).
// Chaque morceau a son tempo, sa gamme, sa suite d'accords et ses motifs de batterie, de basse et d'arpège.
import { getCtx, noise } from './audio.js';

const MIN = [0, 2, 3, 5, 7, 8, 10], MAJ = [0, 2, 4, 5, 7, 9, 11], PHR = [0, 1, 3, 5, 7, 8, 10], DOR = [0, 2, 3, 5, 7, 9, 10];
export const MORCEAUX = [
  { id: 'drive', nom: 'Néon Drive', bpm: 108, root: 45, scale: MIN, prog: [0, 5, 3, 4], kick: 'x...x...x...x...', snare: '....x.......x...', hat: '..x...x...x...xx', bass: 'x.xx.xx.x.xx.xxx', arp: [0, 2, 4, 7, 4, 2, 4, 7], lead: 1, wave: 'sawtooth', cut: 1500 },
  { id: 'minuit', nom: 'Minuit Express', bpm: 126, root: 43, scale: DOR, prog: [0, 3, 6, 4], kick: 'x...x...x...x...', snare: '....x.......x..x', hat: 'x.x.x.x.x.x.x.xx', bass: 'x..x..x.x..x..x.', arp: [0, 4, 7, 9, 7, 4, 2, 4], lead: 2, wave: 'square', cut: 1900 },
  { id: 'turbo', nom: 'Turbo Cœur', bpm: 148, root: 41, scale: MIN, prog: [0, 0, 5, 6], kick: 'x...x...x...x.x.', snare: '....x.......x...', hat: 'xxxxxxxxxxxxxxxx', bass: 'xxxxxxxxxxxxxxxx', arp: [0, 7, 4, 7, 2, 7, 4, 7], lead: 3, wave: 'sawtooth', cut: 2400 },
  { id: 'chrome', nom: 'Rêve Chromé', bpm: 88, root: 48, scale: MAJ, prog: [0, 4, 5, 3], kick: 'x.......x.x.....', snare: '....x.......x...', hat: '..x...x...x...x.', bass: 'x......xx.......', arp: [0, 2, 4, 6, 7, 6, 4, 2], lead: 4, wave: 'triangle', cut: 1300 },
  { id: 'rouge', nom: 'Circuit Rouge', bpm: 138, root: 40, scale: PHR, prog: [0, 1, 0, 6], kick: 'x..x..x.x..x..x.', snare: '....x.......x...', hat: 'x.xxx.xxx.xxx.xx', bass: 'x.x.x.xxx.x.x.xx', arp: [0, 1, 4, 7, 4, 1, 7, 4], lead: 5, wave: 'sawtooth', cut: 2000 },
  { id: 'aube', nom: 'Aube', bpm: 120, root: 46, scale: MAJ, prog: [0, 5, 3, 4], kick: 'x...x...x...x...', snare: '....x.......x...', hat: '..x...x...x...x.', bass: 'x.x.x.x.x.x.x.x.', arp: [0, 4, 7, 11, 7, 4, 9, 7], lead: 6, wave: 'square', cut: 2200 },
];
const hz = (m) => 440 * Math.pow(2, (m - 69) / 12);
let master = null, cur = null, step = 0, nextT = 0, timer = null, vol = 0.5, duck = 1, seed = 1, melody = [];
const rnd = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
function deg(m, d) { const n = m.scale.length, o = Math.floor(d / n); return m.root + m.scale[((d % n) + n) % n] + 12 * o; }
function voice(type, freq, t, dur, gain, cut, att = 0.005) {
  const c = getCtx(), o = c.createOscillator(), f = c.createBiquadFilter(), g = c.createGain();
  o.type = type; o.frequency.value = freq; f.type = 'lowpass'; f.frequency.setValueAtTime(cut, t); f.frequency.exponentialRampToValueAtTime(Math.max(120, cut * 0.3), t + dur);
  g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(gain, t + att); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  o.connect(f); f.connect(g); g.connect(master); o.start(t); o.stop(t + dur + 0.05);
}
function drum(kind, t) {
  const c = getCtx();
  if (kind === 'k') { const o = c.createOscillator(), g = c.createGain(); o.frequency.setValueAtTime(150, t); o.frequency.exponentialRampToValueAtTime(42, t + 0.12);
    g.gain.setValueAtTime(0.9, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.22); o.connect(g); g.connect(master); o.start(t); o.stop(t + 0.25); return; }
  const s = c.createBufferSource(); s.buffer = noise(); const f = c.createBiquadFilter(), g = c.createGain();
  if (kind === 's') { f.type = 'bandpass'; f.frequency.value = 1800; f.Q.value = 0.8; g.gain.setValueAtTime(0.5, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.16); }
  else { f.type = 'highpass'; f.frequency.value = 7500; g.gain.setValueAtTime(0.14, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.04); }
  s.connect(f); f.connect(g); g.connect(master); s.start(t, rnd() * 0.5, 0.2);
}
function schedule(m, s, t) {
  const sixteenth = 60 / m.bpm / 4, i = s % 16, bar = Math.floor(s / 16), chord = m.prog[bar % m.prog.length], section = Math.floor(bar / 4) % 4;
  if (m.kick[i] === 'x') drum('k', t);
  if (m.snare[i] === 'x' && section !== 0) drum('s', t);
  if (m.hat[i] === 'x') drum('h', t);
  if (m.bass[i] === 'x') voice('sawtooth', hz(deg(m, chord) - 12), t, sixteenth * 1.7, 0.3, 500 + (i % 4 === 0 ? 400 : 0));
  if (section >= 1 && i % 2 === 0) voice(m.wave, hz(deg(m, chord + m.arp[(i / 2) % m.arp.length]) + 12), t, sixteenth * 1.5, 0.075, m.cut);
  if (i === 0) for (const d of [0, 2, 4]) for (const det of [-0.06, 0.06]) voice('sawtooth', hz(deg(m, chord + d) + det), t, sixteenth * 16, 0.028, 900, 0.3);   // nappe
  if (section >= 2 && melody[(bar % 4) * 16 + i] !== undefined) voice('square', hz(deg(m, chord + melody[(bar % 4) * 16 + i]) + 24), t, sixteenth * 3, 0.06, 2600);
}
function tick() {
  const c = getCtx(); if (!c || !cur) return;
  const sixteenth = 60 / cur.bpm / 4;
  if (nextT < c.currentTime) nextT = c.currentTime + 0.05;
  while (nextT < c.currentTime + 0.25) { schedule(cur, step, nextT); step++; nextT += sixteenth; }
}
export function play(id) {
  const c = getCtx(); if (!c) return;
  const m = id === 'alea' || !id ? MORCEAUX[Math.floor(Math.random() * MORCEAUX.length)] : MORCEAUX.find(x => x.id === id);
  if (!m) return stop();
  if (cur === m && timer) return;
  if (!master) { master = c.createGain(); const comp = c.createDynamicsCompressor(); master.connect(comp); comp.connect(c.destination); }
  master.gain.setTargetAtTime(vol * duck * 0.55, c.currentTime, 0.05);
  cur = m; step = 0; nextT = c.currentTime + 0.1; seed = m.lead * 7919 + 13; melody = [];
  for (let i = 0; i < 64; i++) if (i % 2 === 0 && rnd() < 0.42) melody[i] = [0, 2, 4, 5, 7, 9][Math.floor(rnd() * 6)];   // mélodie propre à chaque morceau
  clearInterval(timer); timer = setInterval(tick, 80); tick();
}
export function stop() { clearInterval(timer); timer = null; cur = null; if (master) master.gain.setTargetAtTime(0, getCtx().currentTime, 0.05); }
export function setVolume(v) { vol = v; if (master && cur) master.gain.setTargetAtTime(vol * duck * 0.55, getCtx().currentTime, 0.1); }
export function setDuck(k) { duck = k; setVolume(vol); }
export const current = () => cur;
export function next() { const i = cur ? MORCEAUX.indexOf(cur) : -1; const m = MORCEAUX[(i + 1) % MORCEAUX.length]; play(m.id); return m; }
export function shuffle() { let m; do { m = MORCEAUX[Math.floor(Math.random() * MORCEAUX.length)]; } while (m === cur && MORCEAUX.length > 1); if (getCtx()) play(m.id); }
