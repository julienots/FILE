// Radio : cinq stations dont les morceaux sont joués en direct par un séquenceur (aucun fichier audio), plus la station
// « Ma musique » qui lit les fichiers audio ajoutés par le joueur (gardés sur l'appareil).
// Chaque morceau a une structure (intro, couplet, refrain, pont, final), une suite d'accords, une mélodie tirée d'une graine,
// et passe par un écho, une réverbération et un effet de pompe sur la grosse caisse.
import { getCtx, noise, musicBus } from './audio.js';

const MIN = [0, 2, 3, 5, 7, 8, 10], MAJ = [0, 2, 4, 5, 7, 9, 11], PHR = [0, 1, 3, 5, 7, 8, 10], DOR = [0, 2, 3, 5, 7, 9, 10];
// Genres : motifs sur 16 doubles-croches. Basse : x = fondamentale, o = octave, 5 = quinte. arp : pas entre deux notes d'arpège.
const GENRES = {
  synth: { kick: 'x...x...x...x...', snare: '....x.......x...', hat: '..x...x...x.x.x.', ohat: '', bass: 'x.x.x.x.x.x.x.o.', arp: 2, stab: '', lead: 'sawtooth', pump: 0.45, rev: 0.5, bassCut: 520 },
  trance: { kick: 'x...x...x...x...', snare: '....x.......x...', hat: 'x.xxx.xxx.xxx.xx', ohat: '..x...x...x...x.', bass: '..x...x...x...x.', arp: 1, stab: '', lead: 'sawtooth', pump: 0.75, rev: 0.6, bassCut: 700, super: true },
  house: { kick: 'x...x...x...x...', snare: '....x.......x...', hat: 'x.x.x.x.x.x.x.x.', ohat: '..x...x...x...x.', bass: 'x..o..x.x..o.5..', arp: 4, stab: 'x..x..x...x..x..', lead: 'square', pump: 0.6, rev: 0.35, bassCut: 650, clap: true },
  dnb: { kick: 'x.........x.....', snare: '....x.......x...', hat: 'x.x.x.x.x.x.x.x.', ohat: '', bass: 'x.....x..x......', arp: 2, stab: '', lead: 'square', pump: 0.3, rev: 0.4, bassCut: 380, reese: true, ghost: '.......x.x.....x' },
  // phonk : cloche mélodique, basse 808 saturée, grosse caisse lourde
  phonk: { kick: 'x..x..x...x..x..', snare: '....x.......x...', hat: 'x.xxx.x.x.xxx.x.', ohat: '......x.......x.', bass: 'x..x..x...x..x..', arp: 0, stab: '', lead: 'square', pump: 0.55, rev: 0.45, bassCut: 320, clap: true, cow: true, b808: true },
  // hardstyle : grosse caisse saturée, basse à contretemps, mélodie hurlante
  hard: { kick: 'x...x...x...x...', snare: '....x.......x...', hat: '..x...x...x...x.', ohat: '..x...x...x...x.', bass: '..x.x.x...x.x.x.', arp: 1, stab: '', lead: 'sawtooth', pump: 0.85, rev: 0.55, bassCut: 900, super: true, hardkick: true, clap: true },
  // métal : double pédale, guitares serrées
  metal: { kick: 'x.xxx.xxx.xxx.xx', snare: '....x.......x...', hat: 'x.x.x.x.x.x.x.x.', ohat: '', bass: 'xxxxxxxxxxxxxxxx', arp: 0, stab: 'xxx.xxx.xxx.xxxx', lead: 'sawtooth', pump: 0.1, rev: 0.3, bassCut: 560, guitar: true },
  // épique : tambours, cordes en staccato, cuivres
  epic: { kick: 'x.....x...x.x...', snare: '....x.......x.x.', hat: '', ohat: '', bass: 'x...x...x...x.x.', arp: 2, stab: 'x.xxx.xxx.xxx.xx', lead: 'sawtooth', pump: 0.2, rev: 0.75, bassCut: 420, strings: true, brass: true, taiko: true },
  // funk et disco : basse slappée, clavinet, cuivres, charleston chaloupé
  funk: { kick: 'x.....x...x.....', snare: '....x.......x...', hat: 'x.xxx.xxx.xxx.xx', ohat: '......x.......x.', bass: 'x..xo.x...x.o.5.', arp: 0, stab: '..x..x....x..x.x', lead: 'square', pump: 0.2, rev: 0.35, bassCut: 1500, slap: true, clav: true, brass: true, swing: 0.1, ghost: '..x.....x.....x.' },
  // techno : grosse caisse qui gronde, basse acide (filtre résonant, glissés), charleston ouvert à contretemps
  techno: { kick: 'x...x...x...x...', snare: '....x.......x...', hat: 'x.x.x.x.x.x.x.x.', ohat: '..x...x...x...x.', bass: 'xxxxxxxxxxxxxxxx', arp: 0, stab: '', lead: 'square', pump: 0.5, rev: 0.55, bassCut: 420, clap: true, acid: true, rumble: true },
  // lo-fi : batterie feutrée et chaloupée, piano électrique, craquements de vinyle
  lofi: { kick: 'x......x..x.....', snare: '....x.......x...', hat: 'x.x.x.x.x.x.x.x.', ohat: '', bass: 'x.....x...x.....', arp: 0, stab: '', lead: 'triangle', pump: 0.25, rev: 0.6, bassCut: 280, keys: true, vinyl: true, swing: 0.2 },
  rock: { kick: 'x.....x.x.......', snare: '....x.......x...', hat: 'x.x.x.x.x.x.x.x.', ohat: '', bass: 'x.x.x.x.x.x.x.x.', arp: 0, stab: 'x.xxx.xxx.xxx.xx', lead: 'sawtooth', pump: 0.15, rev: 0.3, bassCut: 600, guitar: true },
};
// Structure d'un morceau : [mesures, instruments, effets]. K caisse, S caisse claire, H charleston, B basse, A arpège, P nappe, L mélodie, C accords plaqués.
const FORME = [[4, 'PA', ''], [8, 'KHBPA', 'c'], [8, 'KSHBPALC', 'c'], [4, 'PAL', 'r'], [8, 'KSHBPALC', 'c'], [8, 'KSHBPA', ''], [8, 'KSHBPALC', 'c'], [4, 'KHPA', '']];
const T = (nom, genre, bpm, root, scale, prog, seed, x = {}) => Object.assign({ nom, genre, bpm, root, scale, prog, seed }, x);
export const STATIONS = [
  { id: 'neon', nom: 'Néon FM', freq: '88.6', style: 'synthwave', pistes: [
    T('Néon Drive', 'synth', 108, 45, MIN, [0, 5, 3, 4], 11), T('Minuit Express', 'synth', 124, 43, DOR, [0, 3, 6, 4], 23),
    T('Rêve Chromé', 'synth', 92, 48, MAJ, [0, 4, 5, 3], 37, { lead: 'triangle' }), T('Horizon Rose', 'synth', 116, 41, MIN, [0, 6, 2, 4], 51), T('Autoroute Fantôme', 'synth', 128, 40, PHR, [0, 1, 5, 6], 109),
    T('Vitesse Lumière', 'synth', 120, 42, MIN, [0, 5, 6, 4], 277), T('Lignes Blanches', 'synth', 104, 44, DOR, [0, 3, 4, 6], 281, { lead: 'triangle' })] },
  { id: 'turbo', nom: 'Turbo FM', freq: '101.3', style: 'trance', pistes: [
    T('Turbo Cœur', 'trance', 140, 41, MIN, [0, 0, 5, 6], 5), T('Circuit Rouge', 'trance', 136, 40, PHR, [0, 1, 0, 6], 17), T('Hyperespace', 'trance', 144, 45, MIN, [0, 5, 2, 6], 29), T('Supernova', 'trance', 148, 43, MIN, [0, 3, 5, 4], 113), T('Aurore Électrique', 'trance', 138, 42, MIN, [0, 5, 3, 6], 283)] },
  { id: 'soleil', nom: 'Soleil FM', freq: '94.7', style: 'house', pistes: [
    T('Aube', 'house', 122, 46, MAJ, [0, 5, 3, 4], 7), T('Plage Dorée', 'house', 118, 48, DOR, [0, 3, 4, 3], 19), T('Disco Bitume', 'house', 126, 45, MIN, [0, 3, 5, 4], 43), T('Néon Tropical', 'house', 124, 43, MAJ, [0, 3, 4, 5], 131), T('Club au Couchant', 'house', 124, 44, MIN, [0, 5, 6, 4], 317)] },
  { id: 'basse', nom: 'Basse FM', freq: '107.9', style: 'drum and bass', pistes: [
    T('Adrénaline', 'dnb', 172, 41, MIN, [0, 0, 5, 3], 13), T('Tunnel', 'dnb', 168, 43, PHR, [0, 1, 5, 6], 31), T('Zone Rouge', 'dnb', 176, 38, MIN, [0, 6, 5, 3], 127), T('Tempête', 'dnb', 174, 40, MIN, [0, 3, 6, 5], 293)] },
  { id: 'furie', nom: 'Furie FM', freq: '98.2', style: 'rock', pistes: [
    T('Asphalte Brûlant', 'rock', 150, 40, MIN, [0, 5, 6, 4], 3), T('Dernier Tour', 'rock', 164, 38, MIN, [0, 3, 5, 6], 41),
    T('Métal Hurlant', 'metal', 188, 38, PHR, [0, 1, 5, 3], 103), T('Cylindrée', 'metal', 176, 40, MIN, [0, 5, 3, 6], 107), T('Pied au Plancher', 'rock', 158, 40, MIN, [0, 6, 5, 4], 331)] },
  { id: 'drift', nom: 'Drift FM', freq: '91.1', style: 'phonk', pistes: [
    T('Cloche de Minuit', 'phonk', 130, 38, PHR, [0, 0, 1, 0], 61), T('Gomme Brûlée', 'phonk', 140, 36, MIN, [0, 5, 0, 6], 67), T('Dérive à Tokyo', 'phonk', 124, 41, PHR, [0, 1, 3, 1], 71), T('Frein à Main', 'phonk', 134, 39, PHR, [0, 1, 0, 5], 307)] },
  { id: 'hyper', nom: 'Hyper FM', freq: '104.4', style: 'hardstyle', pistes: [
    T('Zinzin', 'hard', 150, 41, MIN, [0, 5, 3, 6], 73), T('Rupteur', 'hard', 155, 38, PHR, [0, 0, 1, 5], 79), T('10 000 km/h', 'hard', 160, 43, MIN, [0, 3, 5, 6], 83), T('Surcompresseur', 'hard', 158, 40, MIN, [0, 6, 5, 3], 311)] },
  { id: 'podium', nom: 'Podium FM', freq: '96.0', style: 'épique', pistes: [
    T('Grand Prix', 'epic', 132, 38, MIN, [0, 5, 3, 4], 89), T('Dernier Virage', 'epic', 140, 41, MIN, [0, 3, 6, 4], 97), T('Champions', 'epic', 126, 43, MAJ, [0, 4, 5, 3], 101), T("Ligne d'Arrivée", 'epic', 128, 45, MAJ, [0, 3, 4, 5], 313)] },
  { id: 'funk', nom: 'Funk FM', freq: '99.9', style: 'funk et disco', pistes: [
    T('Boulevard Disco', 'funk', 112, 40, DOR, [0, 3, 0, 4], 211), T('Groove de Minuit', 'funk', 106, 43, MIN, [0, 3, 5, 4], 223), T('Velours Chromé', 'funk', 118, 45, DOR, [0, 4, 3, 4], 227), T('Talons Aiguilles', 'funk', 114, 41, MIN, [0, 5, 3, 6], 229)] },
  { id: 'techno', nom: 'Techno FM', freq: '102.6', style: 'techno et acid', pistes: [
    T('Acide 303', 'techno', 132, 36, PHR, [0, 0, 0, 1], 233), T('Entrepôt', 'techno', 128, 38, MIN, [0, 0, 5, 0], 239), T('Hypnose', 'techno', 136, 41, MIN, [0, 6, 0, 5], 241), T('Circuit Fermé', 'techno', 130, 40, PHR, [0, 1, 0, 6], 251)] },
  { id: 'calme', nom: 'Nuit Calme', freq: '89.9', style: 'lo-fi', pistes: [
    T('Pluie sur le Pare-brise', 'lofi', 78, 45, DOR, [0, 3, 1, 4], 257), T('Feux Arrière', 'lofi', 72, 43, MAJ, [0, 5, 1, 4], 263), T('Station-service 3 h', 'lofi', 84, 41, MIN, [0, 5, 3, 4], 269), T("Retour à l'Aube", 'lofi', 80, 48, MAJ, [3, 4, 2, 5], 271)] },
  { id: 'perso', nom: 'Ma musique', freq: '', style: 'tes fichiers', pistes: [] },
];
export const MORCEAUX = STATIONS.flatMap(s => s.pistes);

const GAIN = { synth: 3.8, trance: 4.2, house: 2.9, dnb: 1.05, rock: 1.25, phonk: 1.5, hard: 2.5, metal: 1.1, epic: 2.4, funk: 2.2, techno: 2.0, lofi: 2.5 };   // niveau de chaque genre, pour que les stations sonnent aussi fort les unes que les autres
const hz = (m) => 440 * Math.pow(2, (m - 69) / 12);
let an = null, master = null, dry = null, pump = null, send = null, dist = null, timer = null, vol = 0.5, duck = 1;
let st = STATIONS[0], ti = 0, cur = null, step = 0, nextT = 0, seed = 1, melody = [], acid = [], acidF = 0, onChange = null, total = 0;
const rnd = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
const deg = (m, d) => { const n = m.scale.length, o = Math.floor(d / n); return m.root + m.scale[((d % n) + n) % n] + 12 * o; };
function setup() {
  const c = getCtx(); if (master) return;
  // la radio passe par la sortie commune du jeu (compresseur, limiteur) : jamais de saturation numérique
  master = c.createGain(); master.gain.value = 0; const comp = c.createDynamicsCompressor(); comp.threshold.value = -18; comp.knee.value = 12; comp.ratio.value = 3; comp.attack.value = 0.01; comp.release.value = 0.2;
  const trim = c.createGain(); trim.gain.value = 0.5; master.connect(comp); comp.connect(trim); trim.connect(musicBus() || c.destination); an = c.createAnalyser(); an.fftSize = 2048; trim.connect(an);
  dry = c.createGain(); dry.connect(master); pump = c.createGain(); pump.connect(master);
  // écho (croche pointée) et réverbération (réponse fabriquée : bruit qui s'éteint)
  send = c.createGain(); const dl = c.createDelay(1); dl.delayTime.value = 0.33; const fb = c.createGain(); fb.gain.value = 0.38; const dlp = c.createBiquadFilter(); dlp.type = 'lowpass'; dlp.frequency.value = 2600;
  send.connect(dl); dl.connect(dlp); dlp.connect(fb); fb.connect(dl); dlp.connect(master); send.dl = dl;
  const len = Math.floor(c.sampleRate * 2.2), ir = c.createBuffer(2, len, c.sampleRate);
  for (let ch = 0; ch < 2; ch++) { const d = ir.getChannelData(ch); for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 2.6); }
  const cv = c.createConvolver(); cv.buffer = ir; const rg = c.createGain(); rg.gain.value = 0.5; send.connect(cv); cv.connect(rg); rg.connect(master);
  // saturation des guitares
  dist = c.createWaveShaper(); const n = 256, k = new Float32Array(n); for (let i = 0; i < n; i++) { const x = i / (n - 1) * 2 - 1; k[i] = Math.tanh(x * 9); } dist.curve = k;
  const dlp2 = c.createBiquadFilter(); dlp2.type = 'lowpass'; dlp2.frequency.value = 2400; const dg = c.createGain(); dg.gain.value = 0.16; dist.connect(dlp2); dlp2.connect(dg); dg.connect(pump); dg.connect(send);
}
// une note : oscillateur -> filtre qui se referme -> enveloppe -> sortie (directe ou pompée) et envoi vers l'écho
function note(type, f, t, dur, gain, cut, o = {}) {
  const c = getCtx(), os = c.createOscillator(), fl = c.createBiquadFilter(), g = c.createGain();
  os.type = type; if (o.glide) { os.frequency.setValueAtTime(o.glide, t); os.frequency.exponentialRampToValueAtTime(f, t + 0.07); } else os.frequency.value = f; if (o.det) os.detune.value = o.det;
  if (o.vib) { const l = c.createOscillator(), lg = c.createGain(); l.frequency.value = 5.5; lg.gain.value = f * 0.012; l.connect(lg); lg.connect(os.frequency); l.start(t + 0.12); l.stop(t + dur + 0.1); }
  fl.type = 'lowpass'; fl.Q.value = o.q ?? 1; fl.frequency.setValueAtTime(cut, t); fl.frequency.exponentialRampToValueAtTime(Math.max(90, cut * (o.close ?? 0.3)), t + dur);
  const rel = Math.min(0.6, dur * 0.22), off = t + dur * (o.hold ?? 0.5);
  g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(gain, t + (o.att ?? 0.006)); g.gain.setTargetAtTime(0, off, rel);
  os.connect(fl); fl.connect(g); g.connect(o.out ? gen.map.get(o.out) || o.out : gen.dry); if (o.fx) { const sg = c.createGain(); sg.gain.value = o.fx; g.connect(sg); sg.connect(gen.send); }
  os.start(t); os.stop(off + rel * 7 + 0.02);   // arrêt quand l'enveloppe est à moins de 0,1 % : aucun clic
}
function hit(kind, t, v = 1) {
  const c = getCtx();
  if (kind === 'k') { const o = c.createOscillator(), g = c.createGain(); o.frequency.setValueAtTime(165, t); o.frequency.exponentialRampToValueAtTime(40, t + 0.13);
    g.gain.setValueAtTime(1.0 * v, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.26); o.connect(g); g.connect(gen.dry); o.start(t); o.stop(t + 0.3);
    const G_ = GENRES[cur.genre];
    if (G_.hardkick) { const o2 = c.createOscillator(), g2 = c.createGain(); o2.frequency.setValueAtTime(210, t); o2.frequency.exponentialRampToValueAtTime(48, t + 0.16); g2.gain.setValueAtTime(0.9 * v, t); g2.gain.exponentialRampToValueAtTime(0.001, t + 0.3); o2.connect(g2); g2.connect(gen.dist); o2.start(t); o2.stop(t + 0.32); }   // queue saturée
    if (G_.rumble) { const o2 = c.createOscillator(), g2 = c.createGain(), lp = c.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 180; o2.frequency.setValueAtTime(58, t); o2.frequency.exponentialRampToValueAtTime(42, t + 0.4);
      g2.gain.setValueAtTime(0.0001, t + 0.02); g2.gain.linearRampToValueAtTime(0.35 * v, t + 0.06); g2.gain.exponentialRampToValueAtTime(0.0005, t + 0.45); o2.connect(lp); lp.connect(g2); g2.connect(gen.send); o2.start(t); o2.stop(t + 0.5); }   // grondement dans la réverbération
    if (G_.taiko) { const o2 = c.createOscillator(), g2 = c.createGain(); o2.frequency.setValueAtTime(95, t); o2.frequency.exponentialRampToValueAtTime(52, t + 0.3); g2.gain.setValueAtTime(0.8 * v, t); g2.gain.exponentialRampToValueAtTime(0.001, t + 0.55); o2.connect(g2); g2.connect(gen.dry); const sg = c.createGain(); sg.gain.value = 0.5; g2.connect(sg); sg.connect(gen.send); o2.start(t); o2.stop(t + 0.6); }   // tambour grave et résonnant
    const m = G_.pump; pump.gain.cancelScheduledValues(t); pump.gain.setTargetAtTime(1 - m, t, 0.004); pump.gain.setTargetAtTime(1, t + 0.03, 0.07); return; }   // la caisse fait « pomper » nappes et basse
  if (kind === 't') { const o = c.createOscillator(), g = c.createGain(); o.frequency.setValueAtTime(190 * v, t); o.frequency.exponentialRampToValueAtTime(70 * v, t + 0.2); g.gain.setValueAtTime(0.5, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.24); o.connect(g); g.connect(gen.dry); o.start(t); o.stop(t + 0.27); return; }
  const s = c.createBufferSource(); s.buffer = noise(); const f = c.createBiquadFilter(), g = c.createGain(); let dur = 0.2, fx = 0;
  if (kind === 's') { f.type = 'bandpass'; f.frequency.value = 1900; f.Q.value = 0.7; g.gain.setValueAtTime(0.6 * v, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.19); fx = GENRES[cur.genre].rev;
    const o = c.createOscillator(), og = c.createGain(); o.frequency.setValueAtTime(210, t); o.frequency.exponentialRampToValueAtTime(140, t + 0.08); og.gain.setValueAtTime(0.35 * v, t); og.gain.exponentialRampToValueAtTime(0.001, t + 0.1); o.connect(og); og.connect(gen.dry); o.start(t); o.stop(t + 0.12); }
  else if (kind === 'c') { f.type = 'bandpass'; f.frequency.value = 1300; f.Q.value = 1.4; g.gain.setValueAtTime(0.0001, t); for (let i = 0; i < 3; i++) { g.gain.setValueAtTime(0.5 * v, t + i * 0.012); g.gain.exponentialRampToValueAtTime(0.05, t + i * 0.012 + 0.01); } g.gain.exponentialRampToValueAtTime(0.001, t + 0.2); fx = 0.4; }
  else if (kind === 'o') { f.type = 'highpass'; f.frequency.value = 6500; g.gain.setValueAtTime(0.13 * v, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.22); dur = 0.25; }
  else if (kind === 'v') { f.type = 'highpass'; f.frequency.value = 2500 + rnd() * 4000; g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.05 * v, t + 0.0015); g.gain.exponentialRampToValueAtTime(0.0005, t + 0.012); dur = 0.02; }   // craquement de vinyle
  else if (kind === 'x') { f.type = 'highpass'; f.frequency.value = 4200; g.gain.setValueAtTime(0.3 * v, t); g.gain.exponentialRampToValueAtTime(0.001, t + 1.5); dur = 1.6; fx = 0.5; }            // cymbale
  else if (kind === 'r') { f.type = 'bandpass'; f.Q.value = 1.5; f.frequency.setValueAtTime(400, t); f.frequency.exponentialRampToValueAtTime(7000, t + v); g.gain.setValueAtTime(0.001, t); g.gain.exponentialRampToValueAtTime(0.3, t + v); g.gain.setTargetAtTime(0, t + v, 0.03); dur = v + 0.25; fx = 0.4; }   // montée
  else { f.type = 'highpass'; f.frequency.value = 8000; g.gain.setValueAtTime(0.11 * v, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.035); dur = 0.05; }
  s.connect(f); f.connect(g); g.connect(gen.dry); if (fx) { const sg = c.createGain(); sg.gain.value = fx; g.connect(sg); sg.connect(gen.send); }
  s.start(t, rnd() * 0.3, dur + 0.05);
}
// mélodie de 8 mesures, forme A A' B A' : un motif de deux mesures, repris, varié, puis transposé
function compose(m) {
  seed = m.seed * 7919 + 101; melody = [];
  const rythmes = [[0, 3, 6, 8, 12, 16, 19, 22, 24, 28], [0, 4, 6, 8, 14, 16, 20, 22, 24, 30], [0, 2, 4, 8, 11, 14, 16, 18, 20, 24, 27], [0, 6, 8, 12, 14, 16, 22, 24, 26, 28]];
  const ry = rythmes[Math.floor(rnd() * rythmes.length)], ryB = rythmes[Math.floor(rnd() * rythmes.length)];
  let d = 4; const A = ry.map(() => { d = Math.max(0, Math.min(9, d + Math.round((rnd() - 0.5) * 4))); return d; }), A2 = A.slice(); A2[A2.length - 1] = A[0] + 2; A2[A2.length - 2] = A[0] + 3;
  let e = 6; const B = ryB.map(() => { e = Math.max(2, Math.min(11, e + Math.round((rnd() - 0.45) * 4))); return e; });
  const put = (bar, r, notes) => r.forEach((s, i) => { melody[bar * 16 + s] = { d: notes[i], len: (r[i + 1] ?? 32) - s }; });
  put(0, ry, A); put(2, ry, A2); put(4, ryB, B); put(6, ry, A2);
  // motif de basse acide sur 16 pas : note, accent, glissé, silence
  acid = []; for (let i = 0; i < 16; i++) acid.push({ semi: [0, 0, 12, 0, 7, 0, 3, 10, 0, 12][Math.floor(rnd() * 10)], acc: rnd() < 0.3, slide: rnd() < 0.22, rest: i % 4 !== 0 && rnd() < 0.22 });
}
function schedule(m, s, t) {
  const g = GENRES[m.genre], six = 60 / m.bpm / 4, i = s % 16, bar = Math.floor(s / 16);
  if (g.swing && (i & 1)) t += six * g.swing;   // croches chaloupées
  let b = bar, sec = null, sb = 0; for (const f of FORME) { if (b < f[0]) { sec = f; sb = b; break; } b -= f[0]; }
  if (!sec) return false;
  const L = sec[1], last = sb === sec[0] - 1, chord = m.prog[bar % m.prog.length], has = (k) => L.includes(k);
  if (i === 0 && sb === 0 && sec[2].includes('c')) hit(g.vinyl ? 'v' : 'x', t, g.vinyl ? 1.6 : 1);
  if (g.vinyl && rnd() < 0.45) hit('v', t + rnd() * six, 0.3 + rnd() * 0.7);   // craquements du disque
  if (sec[2].includes('r') && last && i === 0) hit('r', t, six * 16);
  // batterie, avec un roulement à la fin de chaque partie
  if (has('K') && g.kick[i] === 'x' && !(last && i > 11 && has('S'))) hit('k', t);
  if (has('S')) { if (last && i >= 12) { hit('s', t, 0.5 + (i - 12) * 0.15); if (i % 2 === 0) hit('t', t, 1.3 - (i - 12) * 0.12); } else if (g.snare[i] === 'x') hit(g.clap ? 'c' : 's', t); else if (g.ghost && g.ghost[i] === 'x') hit('s', t, 0.3); }
  if (has('H')) { if (g.ohat[i] === 'x') hit('o', t); else if (g.hat[i] === 'x') hit('h', t, i % 4 === 2 ? 1 : 0.6); }
  // basse
  if (has('B') && g.bass[i] !== '.') { const semi = g.bass[i] === 'o' ? 12 : g.bass[i] === '5' ? 7 : 0, f = hz(deg(m, chord) - 12 + semi);
    if (g.acid) { const a = acid[i], prev = acid[(i + 15) % 16];
      if (!a.rest) { const fa = hz(deg(m, chord) - 12 + a.semi), sweep = 0.45 + 0.55 * (0.5 + 0.5 * Math.sin(s / 16 * 0.55));
        note('sawtooth', fa, t, six * (a.slide ? 1.9 : 0.85), a.acc ? 0.3 : 0.2, g.bassCut + (a.acc ? 2600 : 1300) * sweep, { out: pump, q: 13, close: 0.12, hold: a.slide ? 0.95 : 0.45, att: 0.003, glide: prev.slide && !prev.rest ? acidF : 0 });
        acidF = fa; } }
    else if (g.slap) { const pop_ = g.bass[i] === 'o'; note('square', f * (pop_ ? 1 : 1), t, six * (pop_ ? 0.7 : 1.4), pop_ ? 0.22 : 0.26, pop_ ? 3200 : 1800, { out: pump, q: 5, close: 0.18, hold: 0.35, att: 0.002 }); note('sine', f, t, six * 1.4, 0.3, 300, { out: pump, hold: 0.5 }); }
    else if (g.b808) { const len = six * (g.bass[(i + 1) % 16] === '.' ? 3.2 : 1.6); note('sine', f, t, len, 0.75, 500, { out: dist, hold: 0.85, close: 0.9, att: 0.004 }); note('sine', f / 2, t, len, 0.5, 300, { out: pump, hold: 0.85 }); }
    else if (g.reese) { for (const dt of [-14, 14]) note('sawtooth', f, t, six * 5, 0.26, g.bassCut, { out: pump, det: dt, close: 0.7, hold: 0.8 }); note('sine', f / 2, t, six * 5, 0.4, 300, { out: pump, hold: 0.8 }); }
    else { note('sawtooth', f, t, six * 1.8, 0.34, g.bassCut + (i % 4 === 0 ? 350 : 0), { out: pump, q: 4 }); note('sine', f, t, six * 1.6, 0.22, 400, { out: pump }); } }
  // nappe : l'accord tenu toute la mesure
  if (has('P') && g.keys) { if (i === 0 || i === 10) for (const dd of [0, 2, 4, 6]) { const fk = hz(deg(m, chord + dd)), v = i ? 0.7 : 1;
      note('triangle', fk, t + dd * 0.012, six * (i ? 6 : 10), 0.06 * v, 2400, { att: 0.004, hold: 0.55, close: 0.4, fx: 0.45 }); note('sine', fk * 2, t + dd * 0.012, six * 4, 0.018 * v, 4000, { att: 0.002, hold: 0.2, fx: 0.3 }); } }
  else if (has('P') && i === 0) for (const dd of [0, 2, 4, 6]) for (const dt of [-9, 9]) note('sawtooth', hz(deg(m, chord + dd)), t, six * 16, dd === 6 ? 0.012 : 0.024, 1100, { out: pump, det: dt, att: 0.25, hold: 0.9, close: 0.6, fx: 0.4 });
  // arpège
  if (has('A') && g.arp && i % g.arp === 0) { const k = (i / g.arp) % 8, tone = [0, 2, 4, 7, 4, 2, 4, 9][k]; note(m.lead || g.lead, hz(deg(m, chord + tone) + 12), t, six * g.arp * 1.4, 0.06, 2400 + (bar % 4) * 300, { fx: 0.5, q: 3 }); }
  // accords plaqués, ou guitare saturée
  if (g.stab && g.stab[i] === 'x' && (has('C') || (g.guitar && has('B')))) {
    if (g.clav) { for (const dd of [0, 2, 4]) note('square', hz(deg(m, chord + dd) + 12), t, six * 0.55, 0.032, 4200, { q: 6, close: 0.18, hold: 0.3, att: 0.002, fx: 0.12 }); }
    else if (g.strings) { for (const dd of [0, 2, 4]) for (const dt of [-7, 7]) note('sawtooth', hz(deg(m, chord + dd) + (has('C') ? 12 : 0)), t, six * 0.9, i % 4 === 0 ? 0.05 : 0.032, 3400, { det: dt, fx: 0.45, hold: 0.3, close: 0.5 }); }
    else if (g.guitar) { const mute = i % 4 !== 0 && !has('C'); for (const semi of [0, 7, 12]) note('sawtooth', hz(deg(m, chord) + semi), t, six * (mute ? 0.9 : 2.2), 0.5, mute ? 900 : 3000, { out: dist, hold: mute ? 0.4 : 0.8, close: 0.6 }); }
    else for (const dd of [0, 2, 4]) note('sawtooth', hz(deg(m, chord + dd) + 12), t, six * 1.6, 0.05, 2600, { out: pump, fx: 0.35, q: 2 }); }
  // mélodie
  const n = melody[(bar % 8) * 16 + i];
  if (has('L') && n) { const dd = i % 8 === 0 ? chord + [0, 2, 4, 7, 9][Math.min(4, Math.round(n.d / 2.5))] : n.d, f = hz(deg(m, dd) + (g.guitar ? 12 : 24)), dur = six * Math.min(8, n.len) * 0.95;
    if (g.cow) { const fc = hz(deg(m, dd) + 24), dc = Math.min(dur, six * 3); note('square', fc, t, dc, 0.075, 5200, { fx: 0.35, hold: 0.12, close: 0.45, q: 5 }); note('square', fc * 1.4983, t, dc, 0.05, 5200, { fx: 0.35, hold: 0.12, close: 0.45, q: 5 }); }   // cloche : deux carrés à la quinte, très brefs
    else if (g.brass) { for (const dt of [-8, 8]) note('sawtooth', hz(deg(m, dd) + 12), t, dur, 0.075, 1900, { det: dt, att: 0.05, fx: 0.6, hold: 0.8, close: 0.75, q: 2 }); note('sawtooth', hz(deg(m, dd)), t, dur, 0.06, 1300, { att: 0.06, fx: 0.5, hold: 0.8, close: 0.8 }); }
    else if (g.guitar) note('sawtooth', f, t, dur, 0.55, 3200, { out: dist, vib: true, hold: 0.8, close: 0.7 });
    else if (g.super) for (const dt of [-16, 0, 16]) note('sawtooth', f, t, dur, 0.045, 3600, { det: dt, fx: 0.6, hold: 0.7, close: 0.5 });
    else note(m.lead || g.lead, f, t, dur, 0.085, 3000, { vib: dur > 0.3, fx: 0.55, hold: 0.7, close: 0.5 }); }
  return true;
}
function tick() {
  const c = getCtx(); if (!c || !cur) return;
  const six = 60 / cur.bpm / 4;
  if (nextT < c.currentTime) nextT = c.currentTime + 0.05;
  while (cur && nextT < c.currentTime + 0.45) { if (!schedule(cur, step, nextT)) { nextTrack(); return; } step++; nextT += six; }
}
const level = () => { if (master) master.gain.setTargetAtTime(cur ? vol * duck * (GAIN[cur.genre] || 2) : 0, getCtx().currentTime, 0.08); el.volume = Math.min(1, vol * duck); };
// chaque morceau joue dans sa propre « génération » de sorties : au changement, l'ancienne s'éteint en douceur
// (les notes déjà programmées ne débordent plus sur le morceau suivant)
let gen = null;
function newGen() {
  const c = getCtx(), t = c.currentTime;
  if (gen) { const old = gen; for (const n of old.all) { n.gain.cancelScheduledValues(t); n.gain.setTargetAtTime(0, t, 0.02); } setTimeout(() => { for (const n of old.all) n.disconnect(); }, 1500); }
  const mk = (to) => { const g = c.createGain(); g.connect(to); return g; };
  gen = { dry: mk(dry), pump: mk(pump), send: mk(send), dist: mk(dist) }; gen.all = [gen.dry, gen.pump, gen.send, gen.dist];
  gen.map = new Map([[dry, gen.dry], [pump, gen.pump], [send, gen.send], [dist, gen.dist]]);
}
function start(m) {
  setup(); el.pause(); newGen(); cur = m; step = 0; nextT = getCtx().currentTime + 0.25; send.dl.delayTime.value = 60 / m.bpm * 0.75; compose(m);
  clearInterval(timer); timer = setInterval(tick, 60); level(); if (onChange) onChange(now());
}
// ---------- « Ma musique » : fichiers du joueur, gardés dans la base de l'appareil ----------
const el = typeof Audio !== 'undefined' ? new Audio() : { pause() {}, play() { return Promise.resolve(); }, volume: 1 };
let perso = [], url = null;
const idb = () => new Promise((res, rej) => { const r = indexedDB.open('nn_radio', 1); r.onupgradeneeded = () => r.result.createObjectStore('pistes', { keyPath: 'id', autoIncrement: true }); r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error); });
const tx = async (mode, fn) => { const db = await idb(); return new Promise((res, rej) => { const t = db.transaction('pistes', mode), q = fn(t.objectStore('pistes')); t.oncomplete = () => res(q && q.result); t.onerror = () => rej(t.error); }); };
export async function loadPerso() { try { perso = (await tx('readonly', s => s.getAll())) || []; } catch (e) { perso = []; } return perso.length; }
export async function addFiles(files) {
  let n = 0;
  for (const f of files) { if (!/^audio\//.test(f.type) && !/\.(mp3|m4a|aac|ogg|oga|opus|wav|flac)$/i.test(f.name)) continue;
    try { await tx('readwrite', s => s.add({ nom: f.name.replace(/\.[^.]+$/, ''), blob: f })); n++; } catch (e) { break; } }   // arrêt si l'appareil n'a plus de place
  await loadPerso(); return n;
}
export async function clearPerso() { try { await tx('readwrite', s => s.clear()); } catch (e) { /* base indisponible */ } perso = []; if (st.id === 'perso') stop(); }
export const persoCount = () => perso.length;
function startPerso() {
  clearInterval(timer); timer = null; cur = null; if (gen) newGen(); level();
  if (!perso.length) { if (onChange) onChange(now()); return; }
  ti = ((ti % perso.length) + perso.length) % perso.length;
  if (url) URL.revokeObjectURL(url); url = URL.createObjectURL(perso[ti].blob); el.src = url; el.volume = Math.min(1, vol * duck);
  el.play().catch(() => {}); if (onChange) onChange(now());
}
el.onended = () => { if (st.id === 'perso') { ti++; startPerso(); } };
el.onerror = () => { if (st.id === 'perso' && perso.length > 1 && ++total < perso.length) { ti++; startPerso(); } };

// ---------- commandes de la radio ----------
function crackle() { const c = getCtx(); if (!c || !master) return; const t = c.currentTime, s = c.createBufferSource(); s.buffer = noise(); const f = c.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = 2; f.frequency.setValueAtTime(600, t); f.frequency.exponentialRampToValueAtTime(5000, t + 0.22);
  const g = c.createGain(); g.gain.setValueAtTime(0.12 * vol, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.24); s.connect(f); f.connect(g); g.connect(musicBus() || c.destination); s.start(t, Math.random() * 0.5, 0.3); }
export function now() { return { station: st, titre: st.id === 'perso' ? (perso.length ? perso[ti % perso.length].nom : 'Aucun morceau : ajoute-en dans les options') : cur ? cur.nom : '', on: !!cur || (st.id === 'perso' && perso.length > 0 && !el.paused) }; }
export function play(id) {
  if (!getCtx()) return; setup();
  const s = STATIONS.find(x => x.id === id) || STATIONS[0];
  if (s === st && (cur || (s.id === 'perso' && !el.paused && perso.length))) return;
  st = s; total = 0;
  if (s.id === 'perso') { ti = Math.floor(Math.random() * Math.max(1, perso.length)); startPerso(); }
  else { ti = Math.floor(Math.random() * s.pistes.length); start(s.pistes[ti]); }
}
export function stop() { clearInterval(timer); timer = null; cur = null; el.pause(); if (gen) newGen(); if (master) level(); if (onChange) onChange(now()); }
export function nextTrack(dir = 1) {
  if (!getCtx()) return; total = 0;
  if (st.id === 'perso') { ti += dir; startPerso(); } else { ti = (ti + dir + st.pistes.length) % st.pistes.length; start(st.pistes[ti]); }
}
export function nextStation(dir = 1) { if (!getCtx()) return st; setup(); crackle(); const i = STATIONS.indexOf(st); const s = STATIONS[(i + dir + STATIONS.length) % STATIONS.length]; cur = null; el.pause(); play(s.id); return s; }
export function setVolume(v) { vol = v; if (getCtx()) level(); else el.volume = Math.min(1, v); }
export function setDuck(k) { duck = k; setVolume(vol); }
export function pauseAll(v) { if (st.id !== 'perso') return; if (v) el.pause(); else if (perso.length && el.src) el.play().catch(() => {}); }
export function listen(fn) { onChange = fn; }
export const current = () => cur || (st.id === 'perso' && !el.paused ? st : null);
export const station = () => st;
// mesures pour les tests : niveau en sortie, position dans le morceau, saut à une mesure
export function probe() { let r = 0; if (an) { const d = new Float32Array(an.fftSize); an.getFloatTimeDomainData(d); for (const x of d) r += x * x; r = Math.sqrt(r / d.length); } return { rms: r, bar: Math.floor(step / 16), titre: now().titre, station: st.id, paused: el.paused, t: el.currentTime }; }
export function seek(bar) { step = bar * 16; }
