// Univers de départ et générateurs de structures.
import { World } from './world.js';
import { THEME_KEYS } from './themes.js';
import { pick, rint } from './rng.js';

export function starterWorld(name = 'Mon premier univers') {
  const w = new World(name);
  const a = w.addCube(0, 0, 0, 'maison');
  const b = w.addCube(1, 0, 0, 'parc');
  const c = w.addCube(2, 0, 0, 'restaurant');
  w.addCube(1, 1, 0, 'arcade');
  for (const [cube, n] of [[a, 8], [b, 10], [c, 8]]) for (let i = 0; i < n; i++) w.createChar(cube.id);
  w.addLog('univers', '🌱', `Naissance de l'univers « ${name} »`, a.id);
  return w;
}

export function emptyWorld(name) {
  const w = new World(name);
  const a = w.addCube(0, 0, 0, 'maison');
  for (let i = 0; i < 4; i++) w.createChar(a.id);
  w.addLog('univers', '🌱', `Naissance de l'univers « ${name} »`, a.id);
  return w;
}

// Cellules d'une nouvelle structure posée à côté de l'existant. kind: row | tower | grid | pyramid | city
export function structureCells(w, kind, size) {
  let maxX = -1;
  for (const c of w.cubes.values()) if (c.x > maxX) maxX = c.x;
  const ox = maxX + 2; const oz = 0;
  const cells = [];
  if (kind === 'row') for (let i = 0; i < size; i++) cells.push([ox + i, 0, oz]);
  if (kind === 'tower') for (let i = 0; i < size; i++) cells.push([ox, i, oz]);
  if (kind === 'grid') { const s = Math.ceil(Math.sqrt(size)); for (let i = 0; i < size; i++) cells.push([ox + (i % s), 0, oz + Math.floor(i / s)]); }
  if (kind === 'pyramid') {
    let base = 1; while ((base * (base + 1) * (2 * base + 1)) / 6 < size) base++;
    let n = 0;
    for (let y = 0; y < base && n < size; y++) for (let i = y; i < base - y && n < size; i++) for (let j = y; j < base - y && n < size; j++) { cells.push([ox + i, y, oz + j]); n++; }
  }
  if (kind === 'city') {
    const s = Math.ceil(Math.sqrt(size / 2)) + 1; let n = 0;
    for (let i = 0; i < s && n < size; i++) for (let j = 0; j < s && n < size; j++) {
      const h = rint(1, 3);
      for (let y = 0; y < h && n < size; y++) { cells.push([ox + i, y, oz + j]); n++; }
    }
    // complète si la ville est trop petite
    for (let i = 0; n < size; i++) { cells.push([ox + s + (i % s), 0, oz + Math.floor(i / s)]); n++; }
  }
  return cells;
}

export function buildStructure(w, kind, size, opts = {}) {
  const charsPer = opts.charsPer ?? 4;
  const themes = opts.theme ? [opts.theme] : THEME_KEYS;
  const made = [];
  for (const [x, y, z] of structureCells(w, kind, size)) {
    const c = w.addCube(x, y, z, pick(themes)); if (!c) continue;
    made.push(c);
    for (let k = 0; k < charsPer; k++) w.createChar(c.id);
  }
  if (made.length) w.addLog('construction', '🏗️', `Nouvelle structure (${kindLabel(kind)}) : ${made.length} cubes`, made[0].id);
  return made;
}
export function kindLabel(k) { return { row: 'rangée', tower: 'tour', grid: 'quartier', pyramid: 'pyramide', city: 'ville' }[k] || k; }
