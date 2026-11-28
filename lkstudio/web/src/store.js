// LK Studio — projects: the settings in localStorage, the heavy files (models, textures, sounds) in IndexedDB.
const KEY = 'lks.projects.v1';
let dbp = null;
function db() {
  if (!dbp) dbp = new Promise((res, rej) => { const r = indexedDB.open('lks', 1); r.onupgradeneeded = () => r.result.createObjectStore('files'); r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error); });
  return dbp;
}
export async function putFile(key, data) { const d = await db(); return new Promise((res, rej) => { const t = d.transaction('files', 'readwrite'); t.objectStore('files').put(data, key); t.oncomplete = res; t.onerror = () => rej(t.error); }); }
export async function getFile(key) { const d = await db(); return new Promise((res, rej) => { const r = d.transaction('files').objectStore('files').get(key); r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error); }); }
export async function delFile(key) { const d = await db(); return new Promise(res => { const t = d.transaction('files', 'readwrite'); t.objectStore('files').delete(key); t.oncomplete = res; }); }

export function loadProjects() { try { return JSON.parse(localStorage.getItem(KEY) || '[]'); } catch (e) { return []; } }
export function saveProjects(list) { try { localStorage.setItem(KEY, JSON.stringify(list)); } catch (e) { console.warn(e); } }

export function newProject(template = 'vide') {
  const id = 'p' + Date.now().toString(36);
  return { id, name: 'Mon mod', author: 'Moi', version: '1.0', description: '', color: '#c9a45c', template, created: Date.now(),
    creatures: [], items: [], edits: [], world: {}, npcs: [], lore: [], models: [], sounds: [] };
}
