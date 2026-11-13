import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { STORY } from '../data/story.js';
import { Dialogue, test, apply } from './Dialogue.js';

// Déclencheurs narratifs (B3): transmissions radio, révélations, effets sur événement. Données: data/story.js.
// {on: événement, match?: {clé: valeur du payload}, if?: cond, dialogue?, fx?, once?: id (une seule fois par partie)}
const queue = [];
function fire(tr) {
  const fl = State.data.world.flags, key = 'story_' + (tr.once ?? '');
  if (tr.once && fl[key]) return; if (!test(tr.if)) return;
  if (tr.once) fl[key] = true;
  apply(tr.fx);
  if (tr.dialogue) { if (Dialogue.open) queue.push(tr.dialogue); else setTimeout(() => Dialogue.open ? queue.push(tr.dialogue) : Dialogue.start(tr.dialogue), tr.delay ?? 600); }
}
export function initStory() {
  const evs = new Set(STORY.map(s => s.on));
  for (const ev of evs) bus.on(ev, (p = {}) => {
    for (const tr of STORY) if (tr.on === ev && (!tr.match || Object.entries(tr.match).every(([k, v]) => (p[k]?.id ?? p[k]) === v))) fire(tr);
  });
  bus.on('dialogue:end', () => { const n = queue.shift(); if (n) setTimeout(() => Dialogue.start(n), 500); });
}
