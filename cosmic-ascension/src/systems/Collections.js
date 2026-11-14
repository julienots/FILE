import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { CREATURES, ARTIFACTS, ARTIFACT_SOURCES, ACHIEVEMENTS } from '../data/collections.js';
import { bossId } from '../data/bosses.js';
import { refreshStats } from './bonuses.js';

// Collections (B7): créatures, artefacts (bonus passifs), bestiaire, succès.
export const Collections = {
  rnd: Math.random, tameChance: .06,
  get c() { return State.data.collection; },
  addCreature(planet) {
    const id = Object.keys(CREATURES).find(k => CREATURES[k].planet === planet && !this.c.creatures.includes(k)); if (!id) return null;
    this.c.creatures.push(id); const C = CREATURES[id];
    bus.emit('collection:creature', { id }); bus.emit('ui:toast', { text: `${C.glyph} Créature : ${C.name}${C.rarity !== 'commune' ? ' (' + C.rarity + ')' : ''}`, kind: 'discover' });
    return id;
  },
  addArtifact(id) {
    if (!id) id = Object.keys(ARTIFACTS).find(k => !this.c.artifacts.includes(k)); if (!id || this.c.artifacts.includes(id)) return null;
    this.c.artifacts.push(id); refreshStats(State.data); const A = ARTIFACTS[id];
    bus.emit('collection:artifact', { id }); bus.emit('ui:toast', { text: `${A.glyph} Artefact légendaire : ${A.name}`, kind: 'questDone' }); bus.emit('hero:stats', State.data.hero);
    return id;
  },
  check() {
    const d = State.data, got = this.c.achievements;
    for (const a of ACHIEVEMENTS) if (!got.includes(a.id) && a.test(d)) { got.push(a.id); bus.emit('achievement', { id: a.id }); bus.emit('ui:toast', { text: `Succès — ${a.name}`, kind: 'questDone' }); }
  },
};
export function initCollections() {
  const C = Collections, loc = () => State.data.world.location;
  bus.on('pickup:collected', ({ kind }) => { if (kind === 'creature') C.addCreature(loc()); else if (kind === 'artifact') C.addArtifact(ARTIFACT_SOURCES[loc()] && !C.c.artifacts.includes(ARTIFACT_SOURCES[loc()]) ? ARTIFACT_SOURCES[loc()] : null); });
  bus.on('enemy:killed', ({ type }) => { const b = C.c.bestiary; b[type] = (b[type] ?? 0) + 1; if (C.rnd() < C.tameChance) C.addCreature(loc()); });
  bus.on('boss:defeated', ({ id }) => { const k = ARTIFACT_SOURCES['boss_' + bossId(id)]; if (k) C.addArtifact(k); });
  for (const ev of ['scene:entered', 'hero:evolved', 'enemy:killed', 'boss:defeated', 'quest:done', 'build:placed', 'world:discovered', 'world:flag', 'collection:creature', 'collection:artifact', 'inventory:changed'])
    bus.on(ev, () => setTimeout(() => C.check(), 0));
}
