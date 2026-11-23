// Boucle de simulation avec niveaux de détail :
//  - cubes actifs (proches de la caméra) : simulation complète image par image ;
//  - cubes en veille : simulation simplifiée en tâche de fond, par tranches.
import { CUBE } from './constants.js';
import { updateChar, coarseUpdate, abortAct } from './ai.js';
import { EventManager } from './events.js';
import { updateGroups } from './social.js';

export class Simulation {
  constructor(world) {
    this.world = world;
    this.events = new EventManager(world);
    this.speed = 1;
    this.focus = [0, 0, 0];
    this.radius = 3.2;        // rayon actif, en cubes
    this.maxActive = 48;      // budget de cubes simulés finement
    this.active = [];
    this.activeSet = new Set();
    this.coarseQueue = [];
    this.coarseIdx = 0;
    this.groupTimer = 0;
    this.lodTimer = 0;
    this.stats = { active: 0, activeChars: 0, simMs: 0 };
  }

  setWorld(world) { this.world = world; this.events = new EventManager(world); this.active = []; this.activeSet = new Set(); this.coarseQueue = []; }

  refreshLOD() {
    const w = this.world; const [fx, fy, fz] = this.focus;
    const r = this.radius; const list = [];
    for (const c of w.cubes.values()) {
      const dx = c.x * CUBE - fx, dy = c.y * CUBE - fy, dz = c.z * CUBE - fz;
      const d = Math.sqrt(dx * dx + dy * dy * 1.5 + dz * dz) / CUBE;
      if (d <= r) list.push([d, c]);
    }
    list.sort((a, b) => a[0] - b[0]);
    const next = new Set(); const active = [];
    for (const [, c] of list.slice(0, this.maxActive)) { next.add(c.id); active.push(c); }
    for (const id of this.activeSet) {
      if (!next.has(id)) {
        const c = w.cubes.get(id); if (!c) continue;
        c.awake = false; c._lastCoarse = w.time;
        for (const chId of c.chars) { const ch = w.chars.get(chId); if (ch && ch.act) abortAct(w, ch); if (ch) { ch.y = 0; ch.anim = 'idle'; ch.blend = 1; } }
        for (const ev of [...w.events]) if (ev.cubeId === id) { ev.t = 1e9; }
      }
    }
    for (const c of active) if (!c.awake) { c.awake = true; for (const chId of c.chars) { const ch = w.chars.get(chId); if (ch) { ch.act = null; ch.y = 0; } } }
    this.active = active; this.activeSet = next;
    this.coarseQueue = [...w.cubes.values()].filter((c) => !next.has(c.id));
  }

  step(realDt) {
    const t0 = performance.now();
    const w = this.world;
    if (this.speed <= 0) return;
    let remaining = Math.min(realDt, 0.25) * this.speed;
    this.lodTimer -= realDt;
    if (this.lodTimer <= 0) { this.refreshLOD(); this.lodTimer = 0.5; }
    while (remaining > 1e-4) {
      const dt = Math.min(remaining, 0.1);
      remaining -= dt;
      w.time += dt;
      let n = 0;
      for (const c of this.active) {
        if (!w.cubes.has(c.id)) continue;
        for (const id of [...c.chars]) { const ch = w.chars.get(id); if (ch && ch.cubeId === c.id) { updateChar(w, ch, dt); n++; } }
      }
      this.stats.activeChars = n;
      this.events.update(dt, this.active);
      for (let i = w.fx.length - 1; i >= 0; i--) { const f = w.fx[i]; f.t += dt; if (f.t > f.dur) w.fx.splice(i, 1); }
    }
    // tranche de simulation de fond : ~1/40 des cubes en veille par image
    const q = this.coarseQueue;
    if (q.length) {
      const slice = Math.max(1, Math.ceil(q.length / 40));
      for (let i = 0; i < slice; i++) {
        this.coarseIdx = (this.coarseIdx + 1) % q.length;
        const c = q[this.coarseIdx];
        if (!w.cubes.has(c.id) || c.awake) continue;
        const dt = Math.min(120, w.time - (c._lastCoarse ?? w.time));
        c._lastCoarse = w.time;
        if (dt > 0.5) coarseUpdate(w, c, dt);
      }
    }
    this.groupTimer += realDt * this.speed;
    if (this.groupTimer > 20) { this.groupTimer = 0; updateGroups(w); }
    this.stats.active = this.active.length;
    this.stats.simMs = performance.now() - t0;
  }
}
