import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { QUESTS } from '../data/quests.js';
import { getPlanet } from '../data/planets.js';
import { addItem } from './Combat.js';

// Moteur de quêtes (B3). Étapes pilotées par le bus. Types d'étape:
// visit{target planet} · discover{target} · talk{target npcId} · collect{kind, count} (pickups) · item{target itemId, count} (inventaire)
// kill{target type|'any', count, planet?} · evolve{count=stage} · flag{target} · boss{target bossId}
export const Quests = {
  get s() { return State.data.quests; },
  isActive: (id) => !!State.data.quests.active[id], isDone: (id) => State.data.quests.done.includes(id),
  canStart(id) {
    const q = QUESTS[id]; if (!q || this.isActive(id) || this.isDone(id)) return false;
    const r = q.requires || {}, d = State.data;
    if (r.stage != null && d.hero.stage < r.stage) return false;
    if (r.flags && !r.flags.every(f => d.world.flags[f])) return false;
    if (r.quests && !r.quests.every(x => this.isDone(x))) return false;
    return true;
  },
  start(id, { silent } = {}) {
    if (!this.canStart(id)) return false;
    this.s.active[id] = { step: 0, n: 0 };
    bus.emit('quest:started', { id, title: QUESTS[id].title, kind: QUESTS[id].kind });
    if (!silent) bus.emit('ui:toast', { text: `${QUESTS[id].kind === 'main' ? '★' : '◆'} ${QUESTS[id].title}`, kind: 'quest' });
    if (!this.s.tracked || QUESTS[id].kind === 'main') this.s.tracked = id;
    this.checkInstant(id); this.emitTracked(); return true;
  },
  step(id) { const a = this.s.active[id]; return a && QUESTS[id].steps[a.step]; },
  // une étape déjà satisfiée (lieu actuel, stade, flag, inventaire…) avance tout de suite
  checkInstant(id) {
    for (let guard = 0; guard < 20; guard++) {
      const st = this.step(id); if (!st) return; const d = State.data; let ok = false;
      switch (st.type) {
        case 'visit': ok = d.world.location === st.target; break;
        case 'discover': ok = d.world.discovered.includes(st.target); break;
        case 'evolve': ok = d.hero.stage >= st.count; break;
        case 'flag': ok = !!d.world.flags[st.target]; break;
        case 'item': ok = itemCount(st.target) >= st.count; if (!ok) this.s.active[id].n = itemCount(st.target); break;
      }
      if (!ok) return; this.advance(id);
    }
  },
  // un événement de jeu: avance toutes les quêtes actives dont l'étape courante correspond
  progress(type, match, amount = 1) {
    for (const id of Object.keys(this.s.active)) {
      const st = this.step(id); if (!st || st.type !== type || !match(st)) continue;
      const a = this.s.active[id]; a.n += amount;
      if (a.n >= (st.count ?? 1)) this.advance(id);
      else { bus.emit('quest:progress', { id, step: a.step, n: a.n, count: st.count }); this.emitTracked(); }
    }
  },
  advance(id) {
    const a = this.s.active[id]; if (!a) return; a.step++; a.n = 0;
    if (a.step >= QUESTS[id].steps.length) return this.complete(id);
    bus.emit('quest:progress', { id, step: a.step, n: 0 }); this.checkInstant(id); this.emitTracked();
  },
  complete(id) {
    const q = QUESTS[id], r = q.rewards || {}, d = State.data;
    delete this.s.active[id]; if (!this.s.done.includes(id)) this.s.done.push(id);
    if (r.dust) addItem('cosmicDust', r.dust);
    for (const [it, n] of r.items || []) addItem(it, n);
    for (const it of r.reveal || []) if (!d.world.discovered.includes(it)) { d.world.discovered.push(it); bus.emit('world:discovered', { id: it }); bus.emit('ui:toast', { text: `Révélée: ${getPlanet(it)?.name ?? it}`, kind: 'discover' }); }
    bus.emit('quest:done', { id, title: q.title, kind: q.kind, xp: r.xp ?? 0 });
    bus.emit('ui:toast', { text: `✔ ${q.title}`, kind: 'questDone' });
    for (const f of r.flags || []) setFlag(f);
    if (this.s.tracked === id) this.s.tracked = null;
    if (q.next) this.start(q.next);
    this.autoStart(); if (!this.s.tracked) this.s.tracked = Object.keys(this.s.active).sort((x, y) => (QUESTS[x].kind === 'main' ? -1 : 0) - (QUESTS[y].kind === 'main' ? -1 : 0))[0] ?? null;
    this.emitTracked(); State.save();
  },
  // quêtes `auto` (pas de donneur): démarrent dès que leurs conditions sont réunies
  autoStart() { for (const id in QUESTS) if (QUESTS[id].auto && this.canStart(id)) this.start(id); },
  track(id) { if (this.isActive(id)) { this.s.tracked = id; this.emitTracked(); } },
  emitTracked() {
    const id = this.s.tracked, st = id && this.step(id);
    if (!st) return bus.emit('quest:tracked', {});
    const a = this.s.active[id], cnt = st.count ?? 1, showN = ['collect', 'kill', 'item'].includes(st.type) && cnt > 1;
    bus.emit('quest:tracked', { id, title: QUESTS[id].title, kind: QUESTS[id].kind, progress: st.text + (showN ? ` (${Math.min(a.n, cnt)}/${cnt})` : ''), planet: st.type === 'visit' ? st.target : QUESTS[id].planet });
  },
};
export function itemCount(id) { const inv = State.data.inventory; return id === 'cosmicDust' ? inv.cosmicDust : inv.items[id] ?? 0; }
export function setFlag(f, v = true) { const fl = State.data.world.flags; if (fl[f] === v) return; fl[f] = v; bus.emit('world:flag', { flag: f, value: v }); }

export function initQuests() {
  const Q = Quests;
  bus.on('scene:entered', ({ id, planet }) => { if (id === 'planet' && planet) Q.progress('visit', st => st.target === planet.id); });
  bus.on('world:discovered', ({ id }) => Q.progress('discover', st => st.target === id));
  bus.on('npc:talked', ({ npcId }) => Q.progress('talk', st => st.target === npcId));
  bus.on('pickup:collected', ({ kind, amount = 1 }) => Q.progress('collect', st => st.kind === kind || (st.kind === 'dust' && kind === 'cosmicDust'), amount));
  bus.on('inventory:changed', () => { for (const id of Object.keys(Q.s.active)) { const st = Q.step(id); if (st?.type === 'item') Q.checkInstant(id) || Q.emitTracked(); } });
  bus.on('enemy:killed', ({ type }) => Q.progress('kill', st => (st.target === 'any' || st.target === type) && (!st.planet || st.planet === State.data.world.location)));
  bus.on('hero:evolved', ({ stage }) => { for (const id of Object.keys(Q.s.active)) if (Q.step(id)?.type === 'evolve' && stage >= Q.step(id).count) Q.advance(id); Q.autoStart(); });
  bus.on('world:flag', ({ flag }) => { Q.progress('flag', st => st.target === flag); Q.autoStart(); });
  bus.on('boss:defeated', ({ id }) => Q.progress('boss', st => st.target === id));
  bus.on('quest:start', ({ id }) => Q.start(id));
  bus.on('quest:track', ({ id }) => Q.track(id));
  bus.on('state:loaded', () => Q.emitTracked());
  bus.on('game:start', () => { Q.autoStart(); Q.emitTracked(); });
}
