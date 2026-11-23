// Générateur pseudo-aléatoire rapide (mulberry32) — remplaçable pour les tests.
let seed = (Date.now() ^ 0x9e3779b9) >>> 0;
export function setSeed(s) { seed = s >>> 0; }
export function rand() {
  seed = (seed + 0x6d2b79f5) >>> 0;
  let t = seed;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}
export const rrange = (a, b) => a + rand() * (b - a);
export const rint = (a, b) => Math.floor(a + rand() * (b - a + 1));
export const pick = (arr) => arr[Math.floor(rand() * arr.length)];
export const chance = (p) => rand() < p;
export function weightedPick(items, weightFn) {
  let total = 0;
  const w = items.map((it) => { const v = Math.max(0, weightFn(it)); total += v; return v; });
  if (total <= 0) return null;
  let r = rand() * total;
  for (let i = 0; i < items.length; i++) { r -= w[i]; if (r <= 0) return items[i]; }
  return items[items.length - 1];
}
let uid = 0;
export function newId(prefix) {
  uid = (uid + 1) % 1e6;
  return prefix + Date.now().toString(36) + uid.toString(36) + Math.floor(rand() * 1296).toString(36);
}
