// Sauvegarde : sérialisation compacte de l'univers et stockage IndexedDB
// (avec repli localStorage si IndexedDB est indisponible).
import { World } from './world.js';
import { makeObject } from './world.js';

const SAVE_VERSION = 1;

export function serialize(world) {
  const cubes = [];
  for (const c of world.cubes.values()) {
    cubes.push({ id: c.id, x: c.x, y: c.y, z: c.z, theme: c.theme, name: c.name, tint: c.tint, links: c.links, created: c.created,
      objects: c.objects.map((o) => ({ id: o.id, type: o.type, x: +o.x.toFixed(3), z: +o.z.toFixed(3), r: +o.r.toFixed(3), state: o.state })) });
  }
  const chars = [];
  for (const ch of world.chars.values()) {
    const rel = {};
    for (const k in ch.rel) { const r = ch.rel[k]; rel[k] = [Math.round(r.a * 10) / 10, Math.round(r.f * 10) / 10, r.n, Math.round(r.last)]; }
    chars.push({
      id: ch.id, name: ch.name, color: ch.color, cubeId: ch.cubeId, x: +ch.x.toFixed(3), z: +ch.z.toFixed(3), ry: +ch.ry.toFixed(2),
      traits: ch.traits, needs: ch.needs, emo: ch.emo, skills: ch.skills, habits: ch.habits, inventory: ch.inventory, rel,
      memory: ch.memory, bday: ch.bday, lastBday: ch.lastBday, born: ch.born, stats: ch.stats, seen: ch.seen || [],
    });
  }
  return { v: SAVE_VERSION, id: world.id, name: world.name, created: world.created, time: world.time, cubes, chars, groups: world.groups, log: world.log.slice(-1500) };
}

export function deserialize(data) {
  const w = new World(data.name);
  w.id = data.id; w.created = data.created; w.time = data.time || 0;
  for (const c of data.cubes) {
    const cube = w.addCube(c.x, c.y, c.z, c.theme, { id: c.id, name: c.name, tint: c.tint, links: c.links, created: c.created,
      objects: c.objects.map((o) => { const m = makeObject(o.type, o.x, o.z, o.r); m.id = o.id; m.state = o.state || {}; return m; }) });
    if (!cube) continue;
  }
  for (const d of data.chars) {
    const cube = w.cubes.get(d.cubeId); if (!cube) continue;
    const rel = {};
    for (const k in d.rel) { const r = d.rel[k]; rel[k] = { a: r[0], f: r[1], n: r[2], last: r[3] }; }
    const ch = {
      ...d, rel, y: 0, act: null, anim: 'idle', animT: 0, prevAnim: 'idle', blend: 1, bubble: null,
      speedMul: 0.9 + d.traits.energy * 0.3,
    };
    w.chars.set(ch.id, ch); w.names.add(ch.name); cube.chars.add(ch.id);
  }
  w.groups = data.groups || [];
  w.log = data.log || [];
  return w;
}

// ---------- stockage ----------
const DB = 'cubiverse'; const STORE = 'worlds'; const META = 'meta';
let dbp = null;
function db() {
  if (dbp) return dbp;
  dbp = new Promise((res, rej) => {
    if (typeof indexedDB === 'undefined') return rej(new Error('no idb'));
    const r = indexedDB.open(DB, 1);
    r.onupgradeneeded = () => { r.result.createObjectStore(STORE); r.result.createObjectStore(META); };
    r.onsuccess = () => res(r.result);
    r.onerror = () => rej(r.error);
  });
  return dbp;
}
function tx(store, mode, fn) {
  return db().then((d) => new Promise((res, rej) => {
    const t = d.transaction(store, mode); const s = t.objectStore(store);
    const out = fn(s);
    t.oncomplete = () => res(out && out.result !== undefined ? out.result : out);
    t.onerror = () => rej(t.error);
  }));
}

export async function saveWorld(world, thumb) {
  const data = serialize(world);
  const meta = { id: world.id, name: world.name, created: world.created, updated: Date.now(), cubes: world.cubes.size, chars: world.chars.size, day: Math.floor(world.day) + 1, thumb: thumb || null };
  try {
    const old = await getMeta(world.id);
    if (!thumb && old && old.thumb) meta.thumb = old.thumb;
    await tx(STORE, 'readwrite', (s) => s.put(data, world.id));
    await tx(META, 'readwrite', (s) => s.put(meta, world.id));
  } catch (e) {
    try { localStorage.setItem('cv_world_' + world.id, JSON.stringify(data)); localStorage.setItem('cv_meta_' + world.id, JSON.stringify(meta)); } catch (e2) { console.warn('save failed', e2); }
  }
  try { localStorage.setItem('cv_last', world.id); } catch (e) { /* ignore */ }
  return meta;
}

async function getMeta(id) { try { return await tx(META, 'readonly', (s) => s.get(id)); } catch (e) { return null; } }

export async function loadWorld(id) {
  let data = null;
  try { data = await tx(STORE, 'readonly', (s) => s.get(id)); } catch (e) { /* ignore */ }
  if (!data) { try { data = JSON.parse(localStorage.getItem('cv_world_' + id)); } catch (e) { /* ignore */ } }
  return data ? deserialize(data) : null;
}

export async function listWorlds() {
  let metas = [];
  try { metas = await tx(META, 'readonly', (s) => s.getAll()); } catch (e) {
    for (let i = 0; i < localStorage.length; i++) { const k = localStorage.key(i); if (k.startsWith('cv_meta_')) metas.push(JSON.parse(localStorage.getItem(k))); }
  }
  return (metas || []).sort((a, b) => b.updated - a.updated);
}

export async function deleteWorld(id) {
  try { await tx(STORE, 'readwrite', (s) => s.delete(id)); await tx(META, 'readwrite', (s) => s.delete(id)); } catch (e) { /* ignore */ }
  try { localStorage.removeItem('cv_world_' + id); localStorage.removeItem('cv_meta_' + id); } catch (e) { /* ignore */ }
}

export function lastWorldId() { try { return localStorage.getItem('cv_last'); } catch (e) { return null; } }

const SETTINGS_KEY = 'cv_settings';
export function loadSettings(defaults) {
  try { return { ...defaults, ...JSON.parse(localStorage.getItem(SETTINGS_KEY) || '{}') }; } catch (e) { return { ...defaults }; }
}
export function saveSettings(s) { try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(s)); } catch (e) { /* ignore */ } }
