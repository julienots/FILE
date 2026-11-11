// Profil du joueur : sauvegarde automatique locale, reprise de partie, export / import.
import { DEFAULT_CONTROLS } from '../game/input.js';
import { DEFAULT_ASSIST } from '../vehicle/physics.js';
import { emptyUpg } from '../data/parts.js';

const KEY = 'superessence.streetlegends.v1';
export const SAVE_VERSION = 1;

export function newProfile() {
  return {
    v: SAVE_VERSION, name: 'Pilote', created: Date.now(), coins: 2500, tokens: 5, prestige: 0, xp: 0, level: 1, rep: 0,
    car: 'pico', cars: { pico: newCarState() },
    settings: {
      quality: null, dynRes: true, fps: 60, battery: false, camera: 'far', units: 'kmh', minimapRotate: true,
      vol: { master: 0.8, engine: 0.8, sfx: 0.8, music: 0.55, ambient: 0.6 },
      controls: { ...DEFAULT_CONTROLS }, assist: { ...DEFAULT_ASSIST }, difficulty: 'normal', timeMode: 'cycle', hour: 16.5, weather: 'auto', traffic: 1, police: true,
    },
    stats: { km: 0, races: 0, wins: 0, podiums: 0, driftBest: 0, driftTotal: 0, jumpBest: 0, airBest: 0, topSpeed: 0, nearMiss: 0, escapes: 0, busted: 0, coinsEarned: 0, playTime: 0, crashes: 0, skillBest: 0, burnouts: 0, deliveries: 0 },
    events: {}, found: {}, radars: {}, drifts: {}, jumps: {}, zones: {}, achievements: {}, owned: {},
    daily: { last: '', streak: 0, objectives: [], objDay: '' },
    session: null, ghosts: {}, seenIntro: false,
  };
}
export function newCarState() {
  return { cfg: {}, upg: emptyUpg(), km: 0, parts: {} };
}
let profile = null, timer = null;
const store = () => { try { return window.localStorage; } catch (e) { return null; } };

export function loadProfile() {
  const s = store();
  let p = null;
  try { const raw = s && s.getItem(KEY); if (raw) p = JSON.parse(raw); } catch (e) { p = null; }
  const base = newProfile();
  if (p && p.v === SAVE_VERSION) {
    // fusion défensive : les champs ajoutés par une mise à jour prennent leur valeur par défaut
    profile = { ...base, ...p, settings: { ...base.settings, ...p.settings, vol: { ...base.settings.vol, ...(p.settings || {}).vol }, controls: { ...base.settings.controls, ...(p.settings || {}).controls }, assist: { ...base.settings.assist, ...(p.settings || {}).assist } }, stats: { ...base.stats, ...p.stats }, daily: { ...base.daily, ...p.daily } };
    for (const id in profile.cars) profile.cars[id] = { ...newCarState(), ...profile.cars[id], upg: { ...emptyUpg(), ...profile.cars[id].upg } };
  } else profile = base;
  return profile;
}
export function saveNow() {
  const s = store();
  if (!s || !profile) return false;
  try { s.setItem(KEY, JSON.stringify(profile)); return true; } catch (e) { return false; }
}
// Sauvegarde automatique différée (regroupe les écritures).
export function save() {
  if (timer) return;
  timer = setTimeout(() => { timer = null; saveNow(); }, 600);
}
export function resetProfile() { const s = store(); if (s) s.removeItem(KEY); profile = newProfile(); saveNow(); return profile; }
export const exportProfile = () => JSON.stringify(profile);
export function importProfile(text) {
  const p = JSON.parse(text);
  if (!p || p.v !== SAVE_VERSION || typeof p.coins !== 'number' || !p.cars) throw new Error('Fichier de sauvegarde invalide');
  const s = store();
  if (s) s.setItem(KEY, JSON.stringify(p));
  return loadProfile();
}
if (typeof document !== 'undefined') {
  document.addEventListener('visibilitychange', () => { if (document.hidden) saveNow(); });
  addEventListener('pagehide', saveNow);
}
