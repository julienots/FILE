import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { BOSSES, bossId } from '../data/bosses.js';
import { QUESTS } from '../data/quests.js';
import { Combat, addItem } from './Combat.js';
import { setFlag } from './Quests.js';

// Boss (B, logique): PV, phases, points faibles, récompenses. A: entités géantes, attaques télégraphiées, cinématique d'entrée.
// Démarrage auto quand une étape de quête `boss` est active et que le joueur est dans l'arène (planète ou 'space').
// A → B: `boss:hit {id, source, part?}`, `boss:attackHit {id, attack}` (le héros est touché). B → A: `boss:spawn`, `boss:phase`, `boss:damaged`, `boss:defeated`, `boss:despawn`.
export const Bosses = {
  cur: null,
  autoStart: false, // true quand A rendra les boss démarrés par B (boss:spawn de B); sinon A lance ses boss lui-même
  // boss vaincu côté moteur (ex. serpent): récompenses de data/bosses.js, une seule fois
  external(id) { const k = bossId(id), B = BOSSES[k]; if (!B || this.cur?.id === k || State.data.world.flags['boss_' + k]) return; this.cur = { id: k, B, hp: 0, phase: 0, ext: true }; this.defeat(true); },
  start(id) {
    const B = BOSSES[id]; if (!B || this.cur || State.data.world.flags['boss_' + id]) return false;
    this.cur = { id, B, hp: B.hp, phase: 0 };
    bus.emit('boss:spawn', { id, name: B.name, title: B.title, hp: B.hp, maxHp: B.hp, scale: B.scale, arena: B.arena, attacks: B.phases[0].attacks, weakPoints: Object.keys(B.weakPoints || {}), music: B.music });
    return true;
  },
  hit({ source = 'attack', part }) {
    const c = this.cur; if (!c) return 0;
    const wp = (part && c.B.weakPoints?.[part]) || 1, dmg = Math.round(Combat.heroDamage(source) * wp);
    c.hp = Math.max(0, c.hp - dmg);
    bus.emit('boss:damaged', { id: c.id, dmg, hp: c.hp, maxHp: c.B.hp, weak: wp > 1 });
    const frac = c.hp / c.B.hp, ph = c.B.phases.reduce((acc, p, i) => frac <= p.at ? i : acc, 0);
    if (ph > c.phase) { c.phase = ph; bus.emit('boss:phase', { id: c.id, phase: ph, attacks: c.B.phases[ph].attacks }); }
    if (c.hp <= 0) this.defeat();
    return dmg;
  },
  attackHit({ attack }) { const c = this.cur; if (c) Combat.hurt(c.B.dmg * (attack === 'devourPlanet' || attack === 'genesisBeam' ? 2 : 1), c.id); },
  defeat(external = false) {
    const c = this.cur; this.cur = null; const r = c.B.rewards || {};
    setFlag('boss_' + c.id);
    for (const [it, n] of r.items || []) addItem(it, n);
    if (!external) bus.emit('boss:defeated', { id: c.id, name: c.B.name });
    bus.emit('hero:xp', { amount: r.xp ?? 0, reason: 'kill' });
    for (const f of r.flags || []) setFlag(f);
    State.save();
  },
  abort() { if (this.cur) { bus.emit('boss:despawn', { id: this.cur.id }); this.cur = null; } },
  // boss attendu par une quête active pour cette arène
  pending(arena) {
    const q = State.data.quests.active;
    for (const id in q) { const st = QUESTS[id]?.steps[q[id].step]; if (st?.type === 'boss' && BOSSES[st.target]?.arena === arena) return st.target; }
    return null;
  },
};
export function initBosses() {
  let timer = null;
  const tryStart = (arena) => { clearTimeout(timer); if (!Bosses.autoStart) return; const id = Bosses.pending(arena); if (id) timer = setTimeout(() => Bosses.start(id), arena === 'space' ? 6000 : 4000); };
  bus.on('scene:entered', ({ id, planet }) => { Bosses.abort(); tryStart(id === 'planet' ? planet?.id : 'space'); });
  bus.on('quest:progress', () => { if (!Bosses.cur) tryStart(State.data.world.location); });
  bus.on('boss:hit', e => Bosses.hit(e));
  bus.on('boss:defeated', ({ id }) => { if (!Bosses.cur || Bosses.cur.id !== bossId(id)) Bosses.external(id); });
  bus.on('boss:attackHit', e => Bosses.attackHit(e));
  bus.on('boss:start', ({ id }) => Bosses.start(id));
  bus.on('hero:died', () => Bosses.cur && setTimeout(() => Bosses.abort(), 2000));
}
