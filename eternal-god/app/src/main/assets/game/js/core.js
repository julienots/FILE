// Noyau partagé : constantes du monde, réglages, bruit procédural, utilitaires.
import * as THREE from 'three';

export const WORLD = {
  size: 1536,      // mètres de côté
  half: 768,
  res: 3,          // mètres entre deux points de la grille d'altitude
  n: 513,          // points par côté (size / res + 1)
  water: 6,        // niveau de l'eau
  chunks: 8,       // tronçons de terrain par côté (64 au total : peu d'appels de dessin)
};
WORLD.chunkSize = WORLD.size / WORLD.chunks;
WORLD.chunkSeg = WORLD.chunkSize / WORLD.res;

// Le jeu entier partage cet objet (scène, systèmes, état).
export const G = {
  time: 0,          // secondes de jeu écoulées
  dt: 0,
  paused: true,
  started: false,
};

// ---------- Réglages graphiques ----------
export const QUALITY = {
  low:    { label: 'Bas',    pixelRatio: 0.7,  shadows: 0,    bloom: false, grass: 2600, trees: 0.55, far: 1000, particles: 0.5, tex: 512,  atlas: 0.5 },
  medium: { label: 'Moyen',  pixelRatio: 0.9,  shadows: 1024, bloom: false, grass: 5500, trees: 0.8,  far: 1400, particles: 0.8, tex: 1024, atlas: 0.5 },
  high:   { label: 'Élevé',  pixelRatio: 1.25, shadows: 2048, bloom: true,  grass: 9000, trees: 1.0,  far: 1900, particles: 1.0, tex: 1024, atlas: 1 },
};

const DEFAULTS = {
  quality: null,       // choisi automatiquement au premier lancement
  sensitivity: 1.0,
  invertY: false,
  volume: 0.8,
  music: 0.6,         // volume de la musique
  dayLength: 14,       // minutes réelles pour une journée
  haptics: true,
  showFps: false,
  hero: 'void',        // héros par défaut : le Seigneur du Néant
};

export const settings = (() => {
  let s = {};
  try { s = JSON.parse(localStorage.getItem('eg.settings') || '{}'); } catch (e) { s = {}; }
  return Object.assign({}, DEFAULTS, s);
})();

export function saveSettings() {
  try { localStorage.setItem('eg.settings', JSON.stringify(settings)); } catch (e) { /* stockage indisponible */ }
}

export function detectQuality() {
  // Estimation prudente : mémoire, cœurs et GPU.
  const mem = navigator.deviceMemory || 4;
  const cores = navigator.hardwareConcurrency || 4;
  let gpu = '';
  try {
    const c = document.createElement('canvas').getContext('webgl2');
    const ext = c && c.getExtension('WEBGL_debug_renderer_info');
    gpu = ext ? String(c.getParameter(ext.UNMASKED_RENDERER_WEBGL)) : '';
  } catch (e) { /* ignore */ }
  const strong = /Adreno \(TM\) (7[3-9]\d|8\d\d)|Mali-G7[1-9]|Mali-G[7-9]\d\d|Immortalis|Xclipse/i.test(gpu);
  const weak = /Adreno \(TM\) [1-5]\d\d|Mali-[TG][3-6]\d\b|PowerVR/i.test(gpu);
  if (weak || mem <= 3 || cores <= 4) return 'low';
  if (strong && mem >= 6) return 'high';
  return 'medium';
}

export const Q = () => QUALITY[settings.quality || 'medium'];

// ---------- Aléatoire déterministe ----------
export function mulberry32(seed) {
  let a = seed >>> 0;
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// ---------- Bruit de simplexe 2D ----------
export class Simplex {
  constructor(seed = 1) {
    const rnd = mulberry32(seed);
    const p = new Uint8Array(256);
    for (let i = 0; i < 256; i++) p[i] = i;
    for (let i = 255; i > 0; i--) { const j = Math.floor(rnd() * (i + 1)); const t = p[i]; p[i] = p[j]; p[j] = t; }
    this.perm = new Uint8Array(512);
    this.permMod12 = new Uint8Array(512);
    for (let i = 0; i < 512; i++) { this.perm[i] = p[i & 255]; this.permMod12[i] = this.perm[i] % 12; }
  }
  noise(xin, yin) {
    const grad3 = Simplex.G3;
    const F2 = 0.5 * (Math.sqrt(3) - 1), G2 = (3 - Math.sqrt(3)) / 6;
    const s = (xin + yin) * F2;
    const i = Math.floor(xin + s), j = Math.floor(yin + s);
    const t = (i + j) * G2;
    const x0 = xin - (i - t), y0 = yin - (j - t);
    const i1 = x0 > y0 ? 1 : 0, j1 = x0 > y0 ? 0 : 1;
    const x1 = x0 - i1 + G2, y1 = y0 - j1 + G2;
    const x2 = x0 - 1 + 2 * G2, y2 = y0 - 1 + 2 * G2;
    const ii = i & 255, jj = j & 255;
    let n0 = 0, n1 = 0, n2 = 0;
    let t0 = 0.5 - x0 * x0 - y0 * y0;
    if (t0 > 0) { const g = this.permMod12[ii + this.perm[jj]] * 3; t0 *= t0; n0 = t0 * t0 * (grad3[g] * x0 + grad3[g + 1] * y0); }
    let t1 = 0.5 - x1 * x1 - y1 * y1;
    if (t1 > 0) { const g = this.permMod12[ii + i1 + this.perm[jj + j1]] * 3; t1 *= t1; n1 = t1 * t1 * (grad3[g] * x1 + grad3[g + 1] * y1); }
    let t2 = 0.5 - x2 * x2 - y2 * y2;
    if (t2 > 0) { const g = this.permMod12[ii + 1 + this.perm[jj + 1]] * 3; t2 *= t2; n2 = t2 * t2 * (grad3[g] * x2 + grad3[g + 1] * y2); }
    return 70 * (n0 + n1 + n2);
  }
  fbm(x, y, oct = 4, lac = 2, gain = 0.5) {
    let a = 1, f = 1, s = 0, n = 0;
    for (let o = 0; o < oct; o++) { s += a * this.noise(x * f, y * f); n += a; a *= gain; f *= lac; }
    return s / n;
  }
  ridged(x, y, oct = 5) {
    let a = 1, f = 1, s = 0, n = 0, w = 1;
    for (let o = 0; o < oct; o++) {
      let v = 1 - Math.abs(this.noise(x * f, y * f));
      v *= v; v *= w; w = Math.min(1, v * 1.6);
      s += v * a; n += a; a *= 0.5; f *= 2.05;
    }
    return s / n;
  }
}
Simplex.G3 = new Float32Array([1, 1, 0, -1, 1, 0, 1, -1, 0, -1, -1, 0, 1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, -1, 0, 1, 1, 0, -1, 1, 0, 1, -1, 0, -1, -1]);

export const noise = new Simplex(7331);

// ---------- Mathématiques ----------
export const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
export const lerp = (a, b, t) => a + (b - a) * t;
export const smooth = (a, b, v) => { const t = clamp((v - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
export const damp = (a, b, k, dt) => lerp(a, b, 1 - Math.exp(-k * dt));
export const rand = (a = 0, b = 1) => a + Math.random() * (b - a);
export const pick = (arr) => arr[Math.floor(Math.random() * arr.length)];
export function angleDelta(a, b) { let d = (b - a) % (Math.PI * 2); if (d > Math.PI) d -= Math.PI * 2; if (d < -Math.PI) d += Math.PI * 2; return d; }
export function dampAngle(a, b, k, dt) { return a + angleDelta(a, b) * (1 - Math.exp(-k * dt)); }

export const tmpV = new THREE.Vector3();
export const tmpV2 = new THREE.Vector3();
export const tmpQ = new THREE.Quaternion();
export const tmpM = new THREE.Matrix4();
export const UP = new THREE.Vector3(0, 1, 0);

// ---------- Textures procédurales ----------
export function canvasTexture(w, h, draw, opts = {}) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  const ctx = c.getContext('2d');
  draw(ctx, w, h);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = opts.srgb === false ? THREE.NoColorSpace : THREE.SRGBColorSpace;
  if (opts.repeat) { t.wrapS = t.wrapT = THREE.RepeatWrapping; }
  t.anisotropy = 4;
  return t;
}

export function softDotTexture(size = 64, inner = 0.0) {
  return canvasTexture(size, size, (ctx, w) => {
    const g = ctx.createRadialGradient(w / 2, w / 2, w * inner * 0.5, w / 2, w / 2, w / 2);
    g.addColorStop(0, 'rgba(255,255,255,1)');
    g.addColorStop(0.35, 'rgba(255,255,255,0.55)');
    g.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.fillStyle = g; ctx.fillRect(0, 0, w, w);
  });
}

// Petit bus d'événements (journal, réputation, réactions des PNJ…)
const listeners = {};
export function on(name, fn) { (listeners[name] = listeners[name] || []).push(fn); }
export function emit(name, data) { const l = listeners[name]; if (l) for (const fn of l) fn(data); }

// Retour haptique natif (Android) si disponible
export function haptic(ms, amp = 120) {
  if (!settings.haptics) return;
  try { if (window.Android && window.Android.vibrate) window.Android.vibrate(ms | 0, amp | 0); else if (navigator.vibrate) navigator.vibrate(ms); } catch (e) { /* ignore */ }
}
