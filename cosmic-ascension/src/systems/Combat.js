import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { ENEMIES } from '../data/enemies.js';
import { ABILITIES } from '../data/abilities.js';
import { BALANCE } from '../data/balance.js';
import { getPlanet } from '../data/planets.js';
import { ITEMS } from '../data/items.js';

// Combat (B5): stats, dégâts, résistances, IA à états, apparitions, butin, cooldowns, combos, esquive.
// A affiche (entities/Enemy) et rapporte: `enemy:sense {uid,dist}` (≤5 Hz, ennemis proches), `enemy:hit {uid,source,part?}`.
// B ordonne: `enemy:spawn`, `enemy:order {uid,order:'idle'|'move'|'attack'|'return'|'die',…}`. B résout seul les coups reçus par le héros.
const DMG_TYPE = { starbolt: 'star', shockwave: 'star', supernova: 'star', starforge: 'star', bigBang: 'star', gravity: 'void', blackHole: 'void', nebulaVeil: 'void' };

export const Combat = {
  // authoritative=false: A simule les ennemis (entities/Enemies.js, PV + IA) → B ne fait que dégâts héros, butin, combos, capacités.
  // authoritative=true: B possède PV/IA et ordonne via enemy:spawn/order (contrat ARCHITECTURE). Basculé quand A le supporte.
  authoritative: false,
  enemies: new Map(), cds: {}, t: 0, combo: 0, comboT: 0, iframes: 0, lastHurt: -99, paused: false, dead: false, uid: 1, planet: null,
  rnd: Math.random,

  // ── apparition ──
  spawnPlanet(planetId) {
    this.clear(); const p = getPlanet(planetId); if (!p) return; this.planet = p;
    for (const g of p.enemies || []) {
      const E = ENEMIES[g.type]; if (!E) continue;
      const spreadDeg = (g.radius ?? 10) / p.radius * 57.3;
      for (let i = 0; i < g.count; i++) {
        const level = E.level + (p.danger ?? 0), mult = 1 + BALANCE.enemyLevel * (level - 1);
        const e = { uid: this.uid++, type: g.type, E, level, hp: Math.round(E.hp * mult), maxHp: Math.round(E.hp * mult), dmg: Math.round(E.dmg * mult),
          state: 'idle', dist: Infinity, cd: this.rnd() * E.cd, wind: 0, pos: { lat: clampLat(g.lat + (this.rnd() * 2 - 1) * spreadDeg), lon: wrapLon(g.lon + (this.rnd() * 2 - 1) * spreadDeg) } };
        this.enemies.set(e.uid, e);
        bus.emit('enemy:spawn', { uid: e.uid, type: e.type, pos: e.pos, level, hp: e.hp, maxHp: e.maxHp, name: E.name, shape: E.shape, color: E.color, size: E.size, model: E.model, flying: !!E.flying, elite: !!E.elite, speed: E.speed });
      }
    }
  },
  clear() { for (const uid of this.enemies.keys()) bus.emit('enemy:despawn', { uid }); this.enemies.clear(); this.combo = 0; },

  // ── héros → ennemis ──
  heroDamage(source) {
    const st = State.data.hero.stage;
    const base = source && ABILITIES[source] ? ABILITIES[source].dmg * (1 + st * .12) : BALANCE.heroAttack(st);
    return base * Math.min(1 + this.combo * BALANCE.comboBonus, 1 + BALANCE.comboMax * BALANCE.comboBonus);
  },
  hit({ uid, source = 'attack', part }) {
    const e = this.enemies.get(uid); if (!e || e.state === 'dead') return 0;
    const type = DMG_TYPE[source] ?? 'physical', res = e.E.resist?.[type] ?? 1;
    this.combo = this.comboT > 0 ? this.combo + 1 : 1; this.comboT = BALANCE.comboWindow;
    let dmg = Math.max(1, Math.round(this.heroDamage(source) * res * (part ? 1.5 : 1) * (.9 + this.rnd() * .2)));
    const crit = this.rnd() < .08; if (crit) dmg *= 2;
    e.hp -= dmg; if (e.state === 'idle') this.setState(e, 'chase');
    bus.emit('enemy:damaged', { uid, dmg, hp: Math.max(0, e.hp), maxHp: e.maxHp, crit, resist: res < 1 ? 'resist' : res > 1 ? 'weak' : null });
    if (this.combo > 1) bus.emit('combat:combo', { n: this.combo });
    if (e.hp <= 0) this.kill(e);
    return dmg;
  },
  kill(e) {
    this.setState(e, 'dead'); this.enemies.delete(e.uid);
    const loot = [];
    for (const [id, ch, a, b] of e.E.loot || []) if (this.rnd() < ch) { const n = a + Math.floor(this.rnd() * (b - a + 1)); loot.push([id, n]); addItem(id, n); }
    bus.emit('enemy:killed', { uid: e.uid, type: e.type, pos: e.pos, level: e.level, elite: !!e.E.elite, loot });
    lootToast(loot);
  },
  // ennemi tué par le moteur (mode compat): butin selon data/enemies.js
  rollLoot(type) {
    const E = ENEMIES[type], loot = []; if (!E) return loot;
    for (const [id, ch, a, b] of E.loot || []) if (this.rnd() < ch) { const n = a + Math.floor(this.rnd() * (b - a + 1)); loot.push([id, n]); addItem(id, n); }
    lootToast(loot); return loot;
  },
  levelOf(type) { return (ENEMIES[type]?.level ?? 1) + (this.planet?.danger ?? getPlanet(State.data.world.location)?.danger ?? 0); },
  enemyDmg(type, fallback) { const E = ENEMIES[type]; return E ? Math.round(E.dmg * (1 + BALANCE.enemyLevel * (this.levelOf(type) - 1))) : fallback; },
  comboHit() { this.combo = this.comboT > 0 ? this.combo + 1 : 1; this.comboT = BALANCE.comboWindow; if (this.combo > 1) bus.emit('combat:combo', { n: this.combo }); },

  // ── capacités ──
  cast(id) {
    const h = State.data.hero; id ||= [...h.abilities].reverse().find(a => a !== 'hop');
    const A = ABILITIES[id];
    if (!A || !h.abilities.includes(id)) return fail(id, 'locked');
    if ((this.cds[id] ?? 0) > 0) return fail(id, 'cooldown');
    if (h.energy < A.cost) return fail(id, 'energy');
    h.energy -= A.cost; this.cds[id] = A.cd;
    if (id === 'dash') this.iframes = Math.max(this.iframes, BALANCE.dashIFrames);
    if (id === 'nebulaVeil') { h.hp = Math.min(h.maxHp, h.hp + h.maxHp * .3); this.iframes = 2; }
    const scale = 1 + h.stage * .15;
    bus.emit('ability:cast', { id, dmg: Math.round(A.dmg * (1 + h.stage * .12)), radius: A.radius ? A.radius * scale : undefined, range: A.range ? A.range * scale : undefined, cd: A.cd });
    bus.emit('hero:stats', h); return true;
  },

  // ── ennemis → héros ──
  hurt(amount, source) {
    const h = State.data.hero; if (this.dead || this.iframes > 0 || amount <= 0) return 0;
    amount = Math.round(amount); h.hp = Math.max(0, h.hp - amount); this.iframes = BALANCE.hitIFrames; this.lastHurt = this.t; this.combo = 0;
    bus.emit('hero:damaged', { amount, source, hp: h.hp, maxHp: h.maxHp });
    if (h.hp <= 0) this.die(source);
    return amount;
  },
  die(source) {
    const d = State.data; this.dead = true; d.stats.deaths++;
    const lost = Math.floor(d.inventory.cosmicDust * BALANCE.deathDustLoss); d.inventory.cosmicDust -= lost;
    bus.emit('hero:died', { source, lostDust: lost });
    for (const e of this.enemies.values()) this.setState(e, 'return');
    this._respawnAt = this.t + 2.5;
  },
  respawn() {
    const h = State.data.hero; this.dead = false; h.hp = Math.round(h.maxHp * BALANCE.respawnHpRatio); this.iframes = 2;
    bus.emit('hero:respawn', { planetId: this.planet?.id ?? null }); bus.emit('hero:stats', h);
  },

  // ── IA ──
  setState(e, s, extra) {
    if (e.state === s && !extra) return; e.state = s;
    const o = { uid: e.uid, order: s === 'chase' ? 'move' : s };
    if (s === 'chase') Object.assign(o, { target: 'hero', speed: e.E.speed * (e.E.behavior === 'charger' ? 1.6 : 1) });
    if (s === 'return') Object.assign(o, { target: 'home', speed: e.E.speed });
    bus.emit('enemy:order', Object.assign(o, extra));
  },
  sense({ uid, dist }) { const e = this.enemies.get(uid); if (e) e.dist = dist; },
  tickEnemy(e, dt) {
    const E = e.E; e.cd -= dt;
    switch (e.state) {
      case 'idle': if (e.dist < E.aggro && !this.dead) this.setState(e, 'chase'); break;
      case 'chase':
        if (e.dist > E.leash || this.dead) this.setState(e, 'return');
        else if (e.dist <= E.range && e.cd <= 0) { e.wind = E.windup; e.cd = E.cd; this.setState(e, 'attack', { windup: E.windup, kind: E.behavior, dmg: e.dmg }); }
        break;
      case 'attack':
        e.wind -= dt;
        if (e.wind <= 0) { if (e.dist <= E.range * 1.35) this.hurt(e.dmg, e.type); this.setState(e, 'chase'); }
        break;
      case 'return': if (e.dist > E.leash * 1.5 || e.dist === Infinity || this._returnT(e, dt)) { e.hp = e.maxHp; this.setState(e, 'idle'); } break;
    }
  },
  _returnT(e, dt) { e.rt = (e.rt ?? 0) + dt; if (e.rt > 4) { e.rt = 0; return true; } return false; },

  tick(dt) {
    if (this.paused) return;
    this.t += dt; this.iframes = Math.max(0, this.iframes - dt);
    if (this.comboT > 0 && (this.comboT -= dt) <= 0) this.combo = 0;
    for (const k in this.cds) if (this.cds[k] > 0) this.cds[k] = Math.max(0, this.cds[k] - dt);
    for (const e of this.enemies.values()) this.tickEnemy(e, dt);
    if (this.dead && this.t >= this._respawnAt) this.respawn();
    // régénération
    const h = State.data.hero; let ch = false;
    if (h.energy < h.maxEnergy) { h.energy = Math.min(h.maxEnergy, h.energy + BALANCE.energyRegen * dt); ch = true; }
    if (!this.dead && h.hp < h.maxHp && this.t - this.lastHurt > BALANCE.regenDelay) { h.hp = Math.min(h.maxHp, h.hp + BALANCE.hpRegenOutOfCombat * dt); ch = true; }
    if (ch) { this._statT = (this._statT ?? 0) + dt; if (this._statT > .2) { this._statT = 0; bus.emit('hero:stats', h); } }
  },
  cooldown(id) { const A = ABILITIES[id]; return A?.cd ? (this.cds[id] ?? 0) / A.cd : 0; },
};
function lootToast(loot) { if (loot.length) bus.emit('ui:toast', { text: loot.map(([id, n]) => `+${n} ${ITEMS[id]?.icon ?? ''} ${ITEMS[id]?.name ?? id}`).join('  '), kind: 'loot' }); }
function fail(id, reason) { bus.emit('ability:fail', { id, reason }); return false; }
function clampLat(v) { return Math.max(-85, Math.min(85, v)); }
function wrapLon(v) { return ((v + 540) % 360) - 180; }
export function addItem(id, n = 1) {
  const inv = State.data.inventory;
  if (id === 'cosmicDust') inv.cosmicDust += n; else inv.items[id] = (inv.items[id] ?? 0) + n;
  bus.emit('inventory:changed', { id, n });
}

export function initCombat({ autoTick = true } = {}) {
  const C = Combat;
  bus.on('planet:enter', ({ id }) => { C.planet = getPlanet(id); if (C.authoritative) C.spawnPlanet(id); });
  bus.on('planet:leave', () => { C.clear(); C.planet = null; });
  bus.on('enemy:sense', e => C.authoritative && C.sense(e));
  bus.on('enemy:hit', e => C.authoritative ? C.hit(e) : C.comboHit());
  // mode compat: le moteur a tué l'ennemi → butin + niveau (pour l'XP), sans ré-émettre enemy:killed
  bus.on('enemy:killed', e => { if (!C.authoritative && !e.loot) { e.level ??= C.levelOf(e.type); e.loot = C.rollLoot(e.type); } });
  bus.on('enemy:touch', ({ type, dmg }) => C.hurt(C.enemyDmg(type, dmg), type));
  bus.on('boss:hit-hero', ({ id, dmg }) => C.hurt(dmg, id));
  bus.on('ship:bump', ({ speed = 0 }) => C.hurt(Math.round(2 + speed * .03), 'bump'));
  // capacités: en compat, A déclenche déjà l'effet (attaque de base = 'starbolt') → B ne fait que consommer l'énergie, sans refus
  bus.on('hero:ability', ({ id } = {}) => { if (C.authoritative) return C.cast(id); const A = ABILITIES[id], h = State.data.hero;
    if (A && id !== 'starbolt' && A.cost) { h.energy = Math.max(0, h.energy - A.cost); C.cds[id] = A.cd; bus.emit('hero:stats', h); } });
  bus.on('hero:fell', () => Combat.hurt(State.data.hero.maxHp * .2, 'fall'));
  bus.on('game:pause', () => Combat.paused = true); bus.on('game:resume', () => Combat.paused = false);
  bus.on('dialogue:start', () => Combat.paused = true); bus.on('dialogue:end', () => Combat.paused = false);
  if (autoTick) { let last = performance.now(); setInterval(() => { const n = performance.now(); Combat.tick(Math.min(.25, (n - last) / 1000)); last = n; }, 100); }
}
