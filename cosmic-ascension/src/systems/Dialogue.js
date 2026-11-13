import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { DIALOGUES } from '../data/dialogues.js';
import { PLANETS } from '../data/planets.js';
import { Quests, itemCount, setFlag } from './Quests.js';
import { addItem } from './Combat.js';
import { gainXp } from './Progression.js';

// Moteur de dialogues (B3). Données: data/dialogues.js. L'UI (ui/DialogueBox) écoute `dialogue:node` et répond par `dialogue:choose {i}`.
// Nœud: {t, s?, n?, c?:[{l, n?, fx?, if?}], fx?}. entry: [[cond, nodeId]...] choisit le nœud de départ selon l'état.
// cond: {quest, step:[id,i], done, canStart, flag, not, stage, item:[id,n]} (toutes doivent être vraies) · fx: {quest, flag, unflag, give, take, xp, reveal, end}
export const NPCS = Object.fromEntries(PLANETS.flatMap(p => (p.npcs || []).map(n => [n.id, { ...n, planet: p.id }])));
export function test(c) {
  if (!c) return true; const d = State.data;
  if (c.quest && !Quests.isActive(c.quest)) return false;
  if (c.step && !(Quests.isActive(c.step[0]) && d.quests.active[c.step[0]].step === c.step[1])) return false;
  if (c.done && !Quests.isDone(c.done)) return false;
  if (c.canStart && !Quests.canStart(c.canStart)) return false;
  if (c.flag && !d.world.flags[c.flag]) return false;
  if (c.not && d.world.flags[c.not]) return false;
  if (c.stage != null && d.hero.stage < c.stage) return false;
  if (c.item && itemCount(c.item[0]) < c.item[1]) return false;
  return true;
}
export function apply(fx) {
  if (!fx) return;
  for (const f of [].concat(fx.flag ?? [])) setFlag(f);
  for (const f of [].concat(fx.unflag ?? [])) setFlag(f, false);
  for (const [id, n] of fx.take ?? []) addItem(id, -n);
  for (const [id, n] of fx.give ?? []) addItem(id, n);
  if (fx.xp) gainXp(fx.xp, 'quest');
  for (const q of [].concat(fx.quest ?? [])) Quests.start(q);
  for (const p of [].concat(fx.reveal ?? [])) { const w = State.data.world; if (!w.discovered.includes(p)) { w.discovered.push(p); bus.emit('world:discovered', { id: p }); } }
}
export const Dialogue = {
  cur: null, // {id, npcId, node, D}
  paused: false, pending: null, // un dialogue demandé pendant la pause attend la reprise
  get open() { return !!this.cur; },
  start(id, npcId = null) {
    const D = DIALOGUES[id]; if (!D || this.cur) return false;
    if (this.paused) { this.pending = [id, npcId]; return false; }
    const entry = (D.entry || []).find(([c]) => test(c));
    this.cur = { id, npcId, D }; bus.emit('dialogue:start', { id, npcId, radio: !!D.radio });
    this.goto(entry ? entry[1] : 'start'); return true;
  },
  goto(nid) {
    const c = this.cur; if (!c) return;
    const node = nid && c.D.nodes[nid];
    if (!node) return this.end();
    c.node = node; apply(node.fx);
    if (!this.cur) return; // un effet a pu fermer
    const choices = (node.c || []).map((ch, i) => ({ ...ch, i })).filter(ch => test(ch.if));
    const sp = node.s ?? c.D.speaker;
    bus.emit('dialogue:node', { text: node.t, speaker: sp, portrait: node.p ?? c.D.portrait, radio: !!c.D.radio, choices: choices.map(ch => ({ i: ch.i, label: ch.l })) });
  },
  choose(i) {
    const c = this.cur; if (!c) return; const node = c.node;
    if (node.c?.length) { const ch = node.c[i]; if (!ch || !test(ch.if)) return; apply(ch.fx); if (ch.fx?.end) return this.end(); return this.goto(ch.n); }
    this.goto(node.n);
  },
  end() {
    const c = this.cur; if (!c) return; this.cur = null;
    bus.emit('dialogue:end', { id: c.id, npcId: c.npcId });
    if (c.npcId) bus.emit('npc:talked', { npcId: c.npcId });
    if (c.D.talked) bus.emit('npc:talked', { npcId: c.D.talked });
  },
};
export function initDialogue() {
  bus.on('npc:interact', ({ npcId }) => { const n = NPCS[npcId]; if (n?.dialogue) Dialogue.start(n.dialogue, npcId); });
  bus.on('dialogue:choose', ({ i } = {}) => Dialogue.choose(i ?? 0));
  bus.on('dialogue:request', ({ id, npcId }) => Dialogue.start(id, npcId));
  bus.on('game:pause', () => { Dialogue.paused = true; });
  bus.on('game:resume', () => { Dialogue.paused = false; const p = Dialogue.pending; Dialogue.pending = null; if (p) setTimeout(() => Dialogue.start(...p), 300); });
}
