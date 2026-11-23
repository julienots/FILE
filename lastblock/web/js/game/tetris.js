// Moteur de jeu de blocs (logique pure, sans rendu) : 7 pièces, rotations avec kicks,
// verrouillage différé, réserve, file des prochaines pièces, lignes, combos, score, niveaux.
// Le plateau a l'axe y vers le HAUT : la ligne 0 est en bas. Lignes 20-21 = zone d'apparition cachée.

export const W = 10;
export const H = 22;
export const VISIBLE = 20;
export const TYPES = ['I', 'O', 'T', 'S', 'Z', 'J', 'L'];

// Formes en lignes de haut en bas dans leur boîte.
const BASE = {
  I: ['....', 'XXXX', '....', '....'],
  O: ['XX', 'XX'],
  T: ['.X.', 'XXX', '...'],
  S: ['.XX', 'XX.', '...'],
  Z: ['XX.', '.XX', '...'],
  J: ['X..', 'XXX', '...'],
  L: ['..X', 'XXX', '...'],
};

function rotCW(m) {
  const n = m.length;
  return Array.from({ length: n }, (_, r) => Array.from({ length: n }, (_, c) => m[n - 1 - c][r]).join(''));
}

// SHAPES[type][rot] = liste de [col, rowDown] dans la boîte
export const SHAPES = {};
for (const t of TYPES) {
  SHAPES[t] = [];
  let m = BASE[t];
  for (let r = 0; r < 4; r++) {
    const cells = [];
    m.forEach((row, y) => [...row].forEach((ch, x) => { if (ch === 'X') cells.push([x, y]); }));
    SHAPES[t].push(cells);
    if (t !== 'O') m = rotCW(m);
  }
}

const K_JLSTZ = {
  '0>1': [[0, 0], [-1, 0], [-1, 1], [0, -2], [-1, -2]], '1>0': [[0, 0], [1, 0], [1, -1], [0, 2], [1, 2]],
  '1>2': [[0, 0], [1, 0], [1, -1], [0, 2], [1, 2]], '2>1': [[0, 0], [-1, 0], [-1, 1], [0, -2], [-1, -2]],
  '2>3': [[0, 0], [1, 0], [1, 1], [0, -2], [1, -2]], '3>2': [[0, 0], [-1, 0], [-1, -1], [0, 2], [-1, 2]],
  '3>0': [[0, 0], [-1, 0], [-1, -1], [0, 2], [-1, 2]], '0>3': [[0, 0], [1, 0], [1, 1], [0, -2], [1, -2]],
};
const K_I = {
  '0>1': [[0, 0], [-2, 0], [1, 0], [-2, -1], [1, 2]], '1>0': [[0, 0], [2, 0], [-1, 0], [2, 1], [-1, -2]],
  '1>2': [[0, 0], [-1, 0], [2, 0], [-1, 2], [2, -1]], '2>1': [[0, 0], [1, 0], [-2, 0], [1, -2], [-2, 1]],
  '2>3': [[0, 0], [2, 0], [-1, 0], [2, 1], [-1, -2]], '3>2': [[0, 0], [-2, 0], [1, 0], [-2, -1], [1, 2]],
  '3>0': [[0, 0], [1, 0], [-2, 0], [1, -2], [-2, 1]], '0>3': [[0, 0], [-1, 0], [2, 0], [-1, 2], [2, -1]],
};

const LINE_SCORE = [0, 100, 300, 500, 800];

export class Tetris {
  constructor({ seed, startLevel = 1, listener } = {}) {
    this.listener = listener || (() => {});
    this.rng = seed !== undefined ? mulberry(seed) : Math.random;
    this.reset(startLevel);
  }

  reset(startLevel = 1) {
    this.grid = Array.from({ length: H }, () => new Array(W).fill(null));
    this.bag = []; this.queue = [];
    this.hold = null; this.holdUsed = false;
    this.score = 0; this.lines = 0; this.level = startLevel; this.startLevel = startLevel;
    this.combo = -1; this.b2b = false; this.pieces = 0;
    this.active = null; this.over = false; this.paused = false;
    this.gravityTimer = 0; this.lockTimer = 0; this.lockResets = 0; this.onGround = false;
    this.softDropping = false;
    // Règles modifiables par l'histoire (anomalies)
    this.rules = { noClear: false, gravityMul: 1, rotateBlocked: false, frozen: false, noLock: false, allowHold: true, forcedNext: [] };
    this.pieceId = 0;
    this.fillQueue();
  }

  fillQueue() {
    while (this.queue.length < 6) {
      if (!this.bag.length) {
        this.bag = TYPES.slice();
        for (let i = this.bag.length - 1; i > 0; i--) { const j = Math.floor(this.rng() * (i + 1)); [this.bag[i], this.bag[j]] = [this.bag[j], this.bag[i]]; }
      }
      this.queue.push(this.bag.pop());
    }
  }

  gravityInterval() {
    const l = Math.min(this.level, 20);
    return Math.pow(0.8 - (l - 1) * 0.007, l - 1) / this.rules.gravityMul;
  }

  cells(p = this.active, rot = p.rot, x = p.x, y = p.y) {
    return SHAPES[p.type][rot].map(([c, r]) => [x + c, y - r]);
  }

  inside(x, y) { return x >= 0 && x < W && y >= 0 && y < H; }
  occupied(x, y) { return !this.inside(x, y) || this.grid[y][x] !== null; }
  fits(p, rot, x, y) { return this.cells(p, rot, x, y).every(([cx, cy]) => !this.occupied(cx, cy)); }

  spawn(type) {
    let t = type;
    if (!t) {
      t = this.rules.forcedNext.length ? this.rules.forcedNext.shift() : this.queue.shift();
      this.fillQueue();
    }
    const box = t === 'I' ? 4 : t === 'O' ? 2 : 3;
    const x = t === 'O' ? 4 : 3;
    const y = H - 1;
    const p = { type: t, rot: 0, x, y, id: ++this.pieceId, tag: null };
    // Apparition : si ça ne rentre pas, game over
    if (!this.fits(p, 0, x, y)) {
      if (this.fits(p, 0, x, y + 1)) p.y = y + 1; // (sécurité)
      else { this.active = null; this.over = true; this.listener('gameover'); return null; }
    }
    // descend d'une ligne si possible pour être visible plus vite
    if (this.fits(p, 0, x, y - 1)) p.y = y - 1;
    void box;
    this.active = p;
    this.gravityTimer = 0; this.lockTimer = 0; this.lockResets = 0; this.onGround = false;
    this.listener('spawn', p);
    return p;
  }

  move(dx) {
    const p = this.active; if (!p || this.over || this.rules.frozen) return false;
    if (this.fits(p, p.rot, p.x + dx, p.y)) {
      p.x += dx; this._touch(); this.listener('move', dx); return true;
    }
    this.listener('wall', dx); return false;
  }

  rotate(dir) {
    const p = this.active; if (!p || this.over || this.rules.frozen) return false;
    if (this.rules.rotateBlocked) { this.listener('rotateFail', 'blocked'); return false; }
    if (p.type === 'O') { this.listener('rotate', dir); return true; }
    const to = (p.rot + (dir > 0 ? 1 : 3)) % 4;
    const table = p.type === 'I' ? K_I : K_JLSTZ;
    for (const [kx, ky] of table[`${p.rot}>${to}`]) {
      if (this.fits(p, to, p.x + kx, p.y + ky)) {
        p.x += kx; p.y += ky; p.rot = to; this._touch(); this.listener('rotate', dir); return true;
      }
    }
    this.listener('rotateFail', 'wall'); return false;
  }

  _touch() {
    // réinitialise le délai de verrouillage (limité) quand on bouge au sol
    if (this.onGround && this.lockResets < 15) { this.lockTimer = 0; this.lockResets++; }
  }

  ghostY() {
    const p = this.active; if (!p) return 0;
    let y = p.y; while (this.fits(p, p.rot, p.x, y - 1)) y--; return y;
  }

  stepDown() {
    const p = this.active; if (!p) return false;
    if (this.fits(p, p.rot, p.x, p.y - 1)) { p.y--; return true; }
    return false;
  }

  softDrop() {
    if (!this.active || this.over || this.rules.frozen) return;
    if (this.stepDown()) { this.score += 1; this.gravityTimer = 0; this.listener('soft'); }
  }

  hardDrop() {
    const p = this.active; if (!p || this.over || this.rules.frozen) return;
    const from = p.y; let d = 0;
    while (this.stepDown()) d++;
    this.score += d * 2;
    this.listener('hard', { distance: d, from, piece: p });
    this.lock();
  }

  holdPiece() {
    const p = this.active; if (!p || this.over || this.holdUsed || !this.rules.allowHold || this.rules.frozen) return false;
    const t = p.type;
    if (this.hold) { const h = this.hold; this.hold = t; this.spawn(h); }
    else { this.hold = t; this.spawn(); }
    this.holdUsed = true;
    this.listener('hold', t);
    return true;
  }

  lock() {
    const p = this.active; if (!p) return;
    const placed = [];
    for (const [x, y] of this.cells(p)) {
      if (this.inside(x, y)) { this.grid[y][x] = { t: p.type, tag: p.tag, pid: p.id }; placed.push([x, y]); }
    }
    this.active = null; this.pieces++;
    this.holdUsed = false;
    this.listener('lock', { piece: p, cells: placed });
    // au-dessus de la zone visible = perdu
    const topOut = placed.every(([, y]) => y >= VISIBLE);
    this.clearLines();
    if (topOut) { this.over = true; this.listener('gameover'); return; }
    if (!this.over) this.spawnPending = true;
  }

  fullRows() {
    const rows = [];
    for (let y = 0; y < H; y++) if (this.grid[y].every((c) => c !== null && c.t !== 'W')) rows.push(y);
    return rows;
  }

  clearLines() {
    const rows = this.rules.noClear ? [] : this.fullRows();
    if (!rows.length) { this.combo = -1; this.listener('noclear'); return 0; }
    const removed = rows.map((y) => this.grid[y].slice());
    for (let i = rows.length - 1; i >= 0; i--) {
      this.grid.splice(rows[i], 1);
      this.grid.push(new Array(W).fill(null));
    }
    const n = rows.length;
    this.combo++;
    const diff = n === 4;
    let pts = LINE_SCORE[Math.min(4, n)] * this.level;
    if (diff && this.b2b) pts = Math.floor(pts * 1.5);
    this.b2b = diff ? true : (n > 0 ? false : this.b2b);
    if (this.combo > 0) pts += 50 * this.combo * this.level;
    this.score += pts;
    const prevLevel = this.level;
    this.lines += n;
    this.level = Math.max(this.startLevel, Math.floor(this.lines / 10) + 1);
    this.listener('clear', { rows, removed, n, combo: this.combo, points: pts, b2b: diff && this.b2b });
    if (this.level > prevLevel) this.listener('level', this.level);
    return n;
  }

  /** Ajoute des lignes « déchets » par le bas (attaque du boss). */
  addGarbage(n, holeX = -1, kind = 'G') {
    for (let i = 0; i < n; i++) {
      const hole = holeX >= 0 ? holeX : Math.floor(this.rng() * W);
      const row = Array.from({ length: W }, (_, x) => (x === hole ? null : { t: kind }));
      this.grid.pop(); this.grid.unshift(row);
    }
    if (this.active) {
      // repousser la pièce active vers le haut si besoin
      while (!this.fits(this.active, this.active.rot, this.active.x, this.active.y) && this.active.y < H + 3) this.active.y++;
    }
    this.listener('garbage', n);
  }

  update(dt) {
    if (this.over || this.paused) return;
    if (this.spawnPending) { this.spawnPending = false; this.spawn(); }
    const p = this.active; if (!p || this.rules.frozen) return;
    const grounded = !this.fits(p, p.rot, p.x, p.y - 1);
    if (grounded) {
      if (!this.onGround) { this.onGround = true; this.listener('land'); }
      if (!this.rules.noLock) {
        this.lockTimer += dt;
        if (this.lockTimer >= 0.5) this.lock();
      }
      return;
    }
    this.onGround = false;
    this.gravityTimer += dt;
    const g = this.softDropping ? Math.min(0.04, this.gravityInterval() / 20) : this.gravityInterval();
    while (this.gravityTimer >= g && this.active) {
      this.gravityTimer -= g;
      if (!this.stepDown()) break;
      if (this.softDropping) { this.score += 1; this.listener('soft'); }
    }
  }

  countHeight() {
    for (let y = H - 1; y >= 0; y--) if (this.grid[y].some((c) => c)) return y + 1;
    return 0;
  }
}

export function mulberry(a) {
  return function () {
    a |= 0; a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
