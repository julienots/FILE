// Modèle de l'univers : cubes, connexions, personnages, journal.
// Aucune dépendance au rendu : testable en Node.
import { CUBE, HALF, SIDES, OPPOSITE, MAX_PER_CUBE, NEEDS, TRAITS, EMOTIONS, SKILLS, key3, clamp, DAY_LENGTH } from './constants.js';
import { THEMES, OBJECT_TYPES } from './themes.js';
import { rand, rrange, rint, pick, newId } from './rng.js';
import { randomName, PALETTE } from './names.js';

export class World {
  constructor(name = 'Mon univers') {
    this.id = newId('u');
    this.name = name;
    this.created = Date.now();
    this.time = DAY_LENGTH * 0.3; // commence le matin
    this.cubes = new Map();      // id -> cube
    this.grid = new Map();       // "x,y,z" -> id
    this.chars = new Map();      // id -> personnage
    this.log = [];               // historique des événements
    this.groups = [];            // groupes d'amis
    this.events = [];            // événements en cours (non sauvegardés)
    this.fx = [];                // effets visuels transitoires
    this.version = 0;            // incrémenté à chaque changement de structure
    this.listeners = new Set();
    this.names = new Set();
  }

  // ---------- temps ----------
  get day() { return this.time / DAY_LENGTH; }
  get hour() { return ((this.time / DAY_LENGTH) % 1) * 24; }
  get isNight() { const h = this.hour; return h >= 21.5 || h < 6.5; }

  emit(type, data) { for (const l of this.listeners) l(type, data); }
  touch() { this.version++; this.emit('structure'); }

  // ---------- cubes ----------
  cubeAt(x, y, z) { const id = this.grid.get(key3(x, y, z)); return id ? this.cubes.get(id) : null; }
  isFree(x, y, z) { return y >= 0 && !this.grid.has(key3(x, y, z)); }

  addCube(x, y, z, theme = 'maison', opts = {}) {
    if (!this.isFree(x, y, z)) return null;
    const t = THEMES[theme] ? theme : 'maison';
    const cube = {
      id: opts.id || newId('c'),
      x, y, z, theme: t,
      name: opts.name || THEMES[t].label,
      tint: opts.tint || null,
      links: opts.links || { px: true, nx: true, pz: true, nz: true, py: true, ny: true },
      objects: opts.objects || THEMES[t].objects.map(([type, ox, oz, r]) => makeObject(type, ox, oz, r)),
      chars: new Set(),
      awake: false,
      created: opts.created || this.time,
    };
    this.cubes.set(cube.id, cube);
    this.grid.set(key3(x, y, z), cube.id);
    this.touch();
    return cube;
  }

  removeCube(id) {
    const c = this.cubes.get(id); if (!c) return;
    for (const chId of [...c.chars]) this.removeChar(chId, true);
    this.grid.delete(key3(c.x, c.y, c.z));
    this.cubes.delete(id);
    this.events = this.events.filter((e) => e.cubeId !== id);
    this.touch();
  }

  // Déplace un cube et tout son contenu (personnages et objets suivent).
  moveCube(id, x, y, z) {
    const c = this.cubes.get(id);
    if (!c || !this.isFree(x, y, z)) return false;
    this.grid.delete(key3(c.x, c.y, c.z));
    c.x = x; c.y = y; c.z = z;
    this.grid.set(key3(x, y, z), id);
    // Les personnages en transit vers/depuis ce cube recalculent leur trajet.
    for (const ch of this.chars.values()) if (ch.act && ch.act.path && ch.act.path.some((w) => w.cube === id)) ch.act = null;
    this.touch();
    return true;
  }

  setTheme(id, theme) {
    const c = this.cubes.get(id); if (!c || !THEMES[theme]) return;
    c.theme = theme;
    if (c.name === THEMES[c.theme]?.label || Object.values(THEMES).some((t) => t.label === c.name)) c.name = THEMES[theme].label;
    c.objects = THEMES[theme].objects.map(([type, ox, oz, r]) => makeObject(type, ox, oz, r));
    for (const chId of c.chars) { const ch = this.chars.get(chId); if (ch) ch.act = null; }
    this.touch();
  }

  // Duplique un cube (objets + copies des personnages) sur l'emplacement libre le plus proche.
  duplicateCube(id, withChars = true) {
    const c = this.cubes.get(id); if (!c) return null;
    const spot = this.findFreeNear(c.x, c.y, c.z);
    const copy = this.addCube(spot[0], spot[1], spot[2], c.theme, {
      name: c.name + ' (copie)', tint: c.tint, links: { ...c.links },
      objects: c.objects.map((o) => makeObject(o.type, o.x, o.z, o.r)),
    });
    if (withChars) {
      for (const chId of c.chars) {
        const src = this.chars.get(chId);
        const ch = this.createChar(copy.id, { color: src.color, traits: { ...src.traits }, x: src.x, z: src.z });
        if (!ch) break;
      }
    }
    return copy;
  }

  findFreeNear(x, y, z) {
    const order = ['px', 'nx', 'pz', 'nz', 'py'];
    for (let r = 1; r < 64; r++) {
      for (const s of order) {
        const [dx, dy, dz] = SIDES[s];
        const nx = x + dx * r, ny = y + dy * r, nz = z + dz * r;
        if (this.isFree(nx, ny, nz)) return [nx, ny, nz];
      }
    }
    // spirale de secours
    for (let r = 1; ; r++) for (let i = -r; i <= r; i++) for (let j = -r; j <= r; j++) if (this.isFree(x + i, 0, z + j)) return [x + i, 0, z + j];
  }

  // Voisin réellement connecté (adjacent ET les deux côtés ouverts).
  neighbor(cube, side) {
    if (!cube.links[side]) return null;
    const [dx, dy, dz] = SIDES[side];
    const n = this.cubeAt(cube.x + dx, cube.y + dy, cube.z + dz);
    if (!n || !n.links[OPPOSITE[side]]) return null;
    return n;
  }
  adjacent(cube, side) { const [dx, dy, dz] = SIDES[side]; return this.cubeAt(cube.x + dx, cube.y + dy, cube.z + dz); }
  neighbors(cube) {
    const out = [];
    for (const s in SIDES) { const n = this.neighbor(cube, s); if (n) out.push([s, n]); }
    return out;
  }
  setLink(cube, side, on) {
    cube.links[side] = on;
    const n = this.adjacent(cube, side);
    if (n) n.links[OPPOSITE[side]] = on;
    this.touch();
  }
  // Composante connexe (structure) contenant un cube.
  structureOf(cube, limit = 100000) {
    const seen = new Set([cube.id]); const q = [cube];
    while (q.length && seen.size < limit) {
      const c = q.shift();
      for (const [, n] of this.neighbors(c)) if (!seen.has(n.id)) { seen.add(n.id); q.push(n); }
    }
    return seen;
  }
  cubeWorldPos(c) { return [c.x * CUBE, c.y * CUBE, c.z * CUBE]; }

  // ---------- objets ----------
  addObject(cubeId, type, x, z, r = 0) {
    const c = this.cubes.get(cubeId); if (!c || !OBJECT_TYPES[type]) return null;
    const o = makeObject(type, clamp(x, -HALF + 0.25, HALF - 0.25), clamp(z, -HALF + 0.25, HALF - 0.25), r);
    c.objects.push(o); this.touch(); return o;
  }
  removeObject(cubeId, objId) {
    const c = this.cubes.get(cubeId); if (!c) return;
    c.objects = c.objects.filter((o) => o.id !== objId);
    for (const chId of c.chars) { const ch = this.chars.get(chId); if (ch && ch.act && ch.act.obj === objId) ch.act = null; }
    this.touch();
  }

  // ---------- personnages ----------
  createChar(cubeId, opts = {}) {
    const c = this.cubes.get(cubeId);
    if (!c || c.chars.size >= MAX_PER_CUBE) return null;
    const traits = {}; for (const t of TRAITS) traits[t] = opts.traits?.[t] ?? clamp(rand() * 0.9 + 0.05 + (rand() - 0.5) * 0.2, 0, 1);
    const needs = {}; for (const n of NEEDS) needs[n] = rrange(0.55, 0.95);
    const emo = {}; for (const e of EMOTIONS) emo[e] = 0; emo.joy = 0.3;
    const skills = {}; for (const s of SKILLS) skills[s] = rand() < 0.25 ? rrange(0.5, 2.5) : 0;
    const ch = {
      id: opts.id || newId('p'),
      name: opts.name || randomName(this.names),
      color: opts.color || pick(PALETTE),
      cubeId,
      x: opts.x ?? rrange(-1.2, 1.2), z: opts.z ?? rrange(-1.2, 1.2), y: 0, ry: rrange(0, Math.PI * 2),
      traits, needs, emo, skills,
      habits: {},           // actionId -> { n, h:[6 tranches horaires] }
      inventory: [],        // [{type}]
      rel: {},              // autreId -> { a: affinité -100..100, f: familiarité 0..100, n, last }
      memory: [],           // [{ t, text, v }]
      bday: rint(0, 11),    // jour d'anniversaire dans le cycle de 12 jours
      lastBday: -1,
      born: this.time,
      stats: { steps: 0, crossings: 0, friendsMade: 0, fights: 0, gifts: 0, wins: 0 },
      // état d'exécution (non sauvegardé)
      act: null, anim: 'idle', animT: 0, prevAnim: 'idle', blend: 1, bubble: null, speedMul: 0.9 + traits.energy * 0.3,
    };
    if (opts.x === undefined) placeFree(this, c, ch);
    this.chars.set(ch.id, ch);
    this.names.add(ch.name);
    c.chars.add(ch.id);
    this.emit('chars');
    return ch;
  }

  removeChar(id, silent) {
    const ch = this.chars.get(id); if (!ch) return;
    const c = this.cubes.get(ch.cubeId); if (c) c.chars.delete(id);
    this.chars.delete(id);
    this.names.delete(ch.name);
    for (const o of this.chars.values()) {
      if (o.rel[id]) delete o.rel[id];
      if (o.act && (o.act.partner === id)) o.act = null;
    }
    for (const g of this.groups) g.members = g.members.filter((m) => m !== id);
    this.groups = this.groups.filter((g) => g.members.length >= 2);
    if (!silent) this.emit('chars');
  }

  // Transfert d'un personnage vers un cube voisin (coordonnées locales recalculées).
  transfer(ch, dest) {
    const src = this.cubes.get(ch.cubeId);
    if (!dest || dest === src) return false;
    if (dest.chars.size >= MAX_PER_CUBE) return false;
    if (src) {
      ch.x += (src.x - dest.x) * CUBE;
      ch.z += (src.z - dest.z) * CUBE;
      ch.y += (src.y - dest.y) * CUBE;
      src.chars.delete(ch.id);
    }
    dest.chars.add(ch.id);
    ch.cubeId = dest.id;
    ch.stats.crossings++;
    return true;
  }

  charWorldPos(ch, out = [0, 0, 0]) {
    const c = this.cubes.get(ch.cubeId);
    out[0] = c.x * CUBE + ch.x; out[1] = c.y * CUBE + ch.y; out[2] = c.z * CUBE + ch.z;
    return out;
  }

  // ---------- journal ----------
  addLog(type, icon, text, cubeId, chars = [], important = true) {
    const e = { t: this.time, type, icon, text, cubeId, chars, important };
    this.log.push(e);
    if (this.log.length > 3000) this.log.splice(0, this.log.length - 3000);
    this.emit('log', e);
    return e;
  }
}

export function makeObject(type, x, z, r = 0) {
  return { id: newId('o'), type, x, z, r, state: {} };
}

// Place un personnage sur un point libre du cube (évite les objets).
export function placeFree(world, cube, ch) {
  for (let i = 0; i < 30; i++) {
    const x = rrange(-HALF + 0.3, HALF - 0.3), z = rrange(-HALF + 0.3, HALF - 0.3);
    let ok = true;
    for (const o of cube.objects) {
      const r = OBJECT_TYPES[o.type].r + 0.12;
      if ((o.x - x) ** 2 + (o.z - z) ** 2 < r * r) { ok = false; break; }
    }
    if (ok) { ch.x = x; ch.z = z; return; }
  }
  ch.x = rrange(-0.5, 0.5); ch.z = rrange(-0.5, 0.5);
}
