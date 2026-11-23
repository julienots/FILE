// Sauvegarde locale (localStorage) : progression, records, réglages, collection, secrets.
const KEY = 'lastblock.save.v1';

export const DEFAULTS = {
  version: 1,
  progress: { chapter: 1, unlocked: 1, completed: [], endings: [], deaths: 0, corruption: 0 },
  records: { arcade: 0, arcadeLines: 0, chapter1: 0, chapter2: 0, chapter5: 0 },
  secrets: [],
  collection: {
    owned: ['palette:classic', 'material:glossy', 'burst:shards', 'trail:none', 'grid:neon', 'theme:night', 'sound:classic'],
    equipped: { palette: 'classic', material: 'glossy', burst: 'shards', trail: 'none', grid: 'neon', theme: 'night', sound: 'classic' },
  },
  settings: {
    music: 0.7, sfx: 0.85, sensitivity: 1.0, quality: 'auto', // 'performance' | 'quality' | 'auto'
    reduceFlash: false, reduceShake: false, softScares: false, haptics: true,
    controls: 'buttons', // 'buttons' | 'gestures'
    leftHanded: false, buttonSize: 1.0, ghost: true,
    keys: { left: 'ArrowLeft', right: 'ArrowRight', soft: 'ArrowDown', hard: 'Space', rotCW: 'ArrowUp', rotCCW: 'KeyZ', hold: 'KeyC', pause: 'Escape' },
  },
  stats: { linesTotal: 0, piecesTotal: 0, tetrises: 0, playSeconds: 0, livingKilled: 0 },
};

function merge(base, over) {
  if (Array.isArray(base)) return Array.isArray(over) ? over.slice() : base.slice();
  if (base && typeof base === 'object') {
    const out = {};
    for (const k of Object.keys(base)) out[k] = merge(base[k], over ? over[k] : undefined);
    if (over && typeof over === 'object') for (const k of Object.keys(over)) if (!(k in out)) out[k] = over[k];
    return out;
  }
  return over === undefined ? base : over;
}

export class Save {
  constructor() { this.data = merge(DEFAULTS, this._read()); }
  _read() {
    try { const s = localStorage.getItem(KEY); return s ? JSON.parse(s) : undefined; } catch (e) { return undefined; }
  }
  write() {
    try { localStorage.setItem(KEY, JSON.stringify(this.data)); return true; } catch (e) { return false; }
  }
  reset() { this.data = merge(DEFAULTS, undefined); this.write(); }
  get s() { return this.data.settings; }
  get p() { return this.data.progress; }

  hasProgress() { return this.p.unlocked > 1 || this.p.completed.length > 0 || this.p.chapter > 1; }
  completeChapter(n) {
    if (!this.p.completed.includes(n)) this.p.completed.push(n);
    this.p.unlocked = Math.max(this.p.unlocked, Math.min(7, n + 1));
    this.p.chapter = Math.min(7, n + 1);
    this.p.corruption = Math.max(this.p.corruption, n);
    this.write();
  }
  startChapter(n) { this.p.chapter = n; this.write(); }
  record(key, value) {
    if (value > (this.data.records[key] || 0)) { this.data.records[key] = value; this.write(); return true; }
    return false;
  }
  addSecret(id) {
    if (this.data.secrets.includes(id)) return false;
    this.data.secrets.push(id); this.write(); return true;
  }
  addEnding(id) { if (!this.p.endings.includes(id)) { this.p.endings.push(id); this.write(); } }
  own(item) { const o = this.data.collection.owned; if (!o.includes(item)) { o.push(item); this.write(); return true; } return false; }
  owns(item) { return this.data.collection.owned.includes(item); }
  equip(cat, id) { if (this.owns(cat + ':' + id)) { this.data.collection.equipped[cat] = id; this.write(); } }
}
