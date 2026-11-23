// Outils génériques : maths, easing, aléatoire, ordonnanceur de cinématiques annulable.
export const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
export const lerp = (a, b, t) => a + (b - a) * t;
export const damp = (a, b, lambda, dt) => lerp(a, b, 1 - Math.exp(-lambda * dt));
export const rand = (a = 1, b) => (b === undefined ? Math.random() * a : a + Math.random() * (b - a));
export const randi = (a, b) => Math.floor(rand(a, b + 1));
export const pick = (arr) => arr[Math.floor(Math.random() * arr.length)];
export const chance = (p) => Math.random() < p;
export const easeOutBack = (t) => { const c1 = 1.70158, c3 = c1 + 1; return 1 + c3 * Math.pow(t - 1, 3) + c1 * Math.pow(t - 1, 2); };
export const easeOutCubic = (t) => 1 - Math.pow(1 - t, 3);
export const easeInCubic = (t) => t * t * t;
export const easeInOut = (t) => (t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2);
export const smooth = (t) => t * t * (3 - 2 * t);

export const ABORT = Symbol('abort');

/**
 * Ordonnanceur basé sur le temps de jeu (il s'arrête pendant la pause).
 * Les scripts de chapitres sont des fonctions async qui font `await S.wait(2)`.
 * abort() rejette toutes les attentes en cours : quitter un chapitre coupe proprement son script.
 */
export class Scheduler {
  constructor() { this.t = 0; this.tasks = []; this.gen = 0; }
  update(dt) {
    this.t += dt;
    if (!this.tasks.length) return;
    const list = this.tasks; this.tasks = [];
    for (const k of list) {
      if (k.gen !== this.gen) { k.rej(ABORT); continue; }
      let done = false;
      if (k.at !== undefined) done = this.t >= k.at;
      else if (k.cond) { try { done = !!k.cond(); } catch (e) { console.error(e); done = true; } }
      if (done) k.res(this.t); else this.tasks.push(k);
    }
  }
  wait(s) { return new Promise((res, rej) => this.tasks.push({ at: this.t + Math.max(0, s), res, rej, gen: this.gen })); }
  until(cond, timeout) {
    if (timeout === undefined) return new Promise((res, rej) => this.tasks.push({ cond, res, rej, gen: this.gen }));
    const end = this.t + timeout;
    return new Promise((res, rej) => this.tasks.push({ cond: () => cond() || this.t >= end, res, rej, gen: this.gen }));
  }
  abort() {
    this.gen++;
    const list = this.tasks; this.tasks = [];
    for (const k of list) k.rej(ABORT);
  }
}

export function guard(promise) {
  return promise.catch((e) => { if (e !== ABORT) console.error(e); });
}

export function shuffle(a) {
  for (let i = a.length - 1; i > 0; i--) { const j = Math.floor(Math.random() * (i + 1)); [a[i], a[j]] = [a[j], a[i]]; }
  return a;
}
