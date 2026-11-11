// Pilote IA : suit une trajectoire, freine avant les virages, évite les obstacles, dépasse, défend sa position,
// commet des erreurs occasionnelles. Cinq personnalités aux comportements distincts.
import { clamp, lerp, wrapAngle, damp } from '../core/util.js';
import { SURF, maxSteerAt } from '../vehicle/physics.js';
import * as MAP from '../world/map.js';

export const PERSONALITIES = {
  prudent: { name: 'Prudent', pace: 0.93, gap: 1.5, risk: 0.2, block: 0, nitro: 0.3, err: 0.5, drift: 0, lineBias: 0 },
  aggressive: { name: 'Agressif', pace: 1.0, gap: 0.55, risk: 1, block: 1, nitro: 0.8, err: 1.3, drift: 0.2, lineBias: 0 },
  technical: { name: 'Technique', pace: 1.02, gap: 1, risk: 0.5, block: 0.4, nitro: 0.5, err: 0.35, drift: 0, lineBias: 0 },
  drifter: { name: 'Spécialiste du drift', pace: 0.97, gap: 0.9, risk: 0.7, block: 0.3, nitro: 0.5, err: 0.9, drift: 1, lineBias: 0 },
  speedster: { name: 'Expert de vitesse', pace: 0.98, gap: 0.8, risk: 0.8, block: 0.5, nitro: 1, err: 0.8, drift: 0, lineBias: 0, straight: 1.06 },
};
export const DIFFICULTY = {
  easy: { name: 'Facile', skill: 0.84, power: 0.9, band: 1 },
  normal: { name: 'Normal', skill: 0.92, power: 0.97, band: 0.7 },
  hard: { name: 'Difficile', skill: 0.99, power: 1.03, band: 0.35 },
  expert: { name: 'Expert', skill: 1.04, power: 1.08, band: 0 },
};
export const RIVAL_NAMES = ['Nova', 'Rico Vega', 'Kenji', 'Lola Cruz', 'Dax', 'Mireille', 'Tank', 'Suki', 'Bruno', 'Zed', 'Ines', 'Malik', 'Yuna', 'Viktor', 'Paloma', 'Jet', 'Amaya', 'Otto', 'Chiara', 'Kofi', 'Rhea', 'Milo', 'Sasha', 'Nolan'];
const surfMu = (type) => (type === 'dirt' ? SURF.dirt.mu + 0.15 : type === 'sand' ? SURF.sand.mu + 0.2 : type === 'alley' ? 0.92 : 1);
const _a = {}, _b = {};

export class AIDriver {
  constructor(car, route, persona = 'technical', diff = 'normal', seed = 1) {
    this.car = car; this.route = route; this.pk = persona; this.P = PERSONALITIES[persona]; this.D = DIFFICULTY[diff] || DIFFICULTY.normal;
    car.ai = this;
    this.i = 0; this.off = 0; this.offT = 0; this.lap = 0; this.prog = 0; this.stuck = 0; this.rev = 0; this.errT = 4 + (seed % 7); this.err = 0; this.errKind = 0;
    this.seed = seed; this.rnd = seed * 9301 + 49297; this.hbT = 0; this.finished = false; this.band = 1; this.targetV = 0; this.lineOff = ((seed % 5) - 2) * 0.35;
    this.rebuild();
  }
  rand() { this.rnd = (this.rnd * 9301 + 49297) % 233280; return this.rnd / 233280; }
  rebuild() {
    const st = this.car.stats, sk = this.D.skill * this.P.pace;
    this.v = this.route.profile(st.grip * sk * 0.78, st.brake * 0.8 * sk, this.car.vmax * (this.P.straight || 1), surfMu);
  }
  setRoute(r, i = 0) { this.route = r; this.i = i; this.rebuild(); }
  // ctx : { bodies: [corps physiques proches], playerProg, myProg } ; renvoie rien, écrit car.in
  update(dt, ctx) {
    const car = this.car, R = this.route, I = car.in;
    if (this.finished) { I.throttle = 0; I.brake = car.speed > 2 ? 0.6 : 0.2; I.steer = 0; I.handbrake = car.speed < 2; I.nitro = false; return; }
    const prev = this.i;
    this.i = R.nearest(car.x, car.z, this.i, 4, 26);
    if (R.closed && prev > R.n * 0.8 && this.i < R.n * 0.2) this.lap++;
    if (R.closed && prev < R.n * 0.2 && this.i > R.n * 0.8) this.lap--;
    const dOff = R._d;
    this.prog = this.lap * R.len + R.project(car.x, car.z, this.i);
    const v = car.speed;
    // --- erreurs occasionnelles ---
    this.errT -= dt;
    if (this.errT <= 0 && this.err <= 0) { this.err = 0.5 + this.rand() * 0.6; this.errKind = this.rand() < 0.5 ? 0 : 1; this.errT = (9 + this.rand() * 16) / Math.max(0.2, this.P.err) / (2 - this.D.skill); }
    if (this.err > 0) this.err -= dt;
    // --- obstacles : dépassement, évitement, défense ---
    const myLat = R.lateral(car.x, car.z, this.i), hw = R.p[this.i].hw, room = Math.max(0.5, hw - 1.6);
    let want = this.lineOff, slow = Infinity;
    const range = 10 + v * 1.25 * this.P.gap + 6;
    if (ctx && ctx.bodies) for (const o of ctx.bodies) {
      if (o === car) continue;
      const dx = o.x - car.x, dz = o.z - car.z;
      if (dx * dx + dz * dz > (range + 15) * (range + 15)) continue;
      const fwd = dx * R.tx[this.i] + dz * R.tz[this.i];
      const oi = R.nearest(o.x, o.z, this.i, 3, 14);
      const oLat = R.lateral(o.x, o.z, oi), dl = oLat - myLat;
      const ov = (o.vx || 0) * R.tx[this.i] + (o.vz || 0) * R.tz[this.i];
      const width = (car.W + (o.W || 1.9)) / 2 + 0.7;
      if (fwd > 1 && fwd < range) {
        if (Math.abs(dl) < width + 0.5 && ov < v + 1) {
          // passer du côté le plus dégagé
          const left = room - oLat, right = room + oLat;
          const side = left > right ? 1 : -1, spaceOk = Math.max(left, right) > width + 0.4;
          if (spaceOk) want = clamp(oLat + side * (width + 0.5), -room, room);
          const closing = v - ov;
          if (Math.abs(dl) < width && closing > 0) {
            const safe = 3 + ((o.L || 4.5) + car.L) / 2 + closing * (0.35 + 0.5 * this.P.gap * (spaceOk ? 0.4 : 1));
            if (fwd < safe) slow = Math.min(slow, Math.max(0, ov - (spaceOk ? 0 : 2)));
          }
        }
      } else if (fwd < -1 && fwd > -14 && this.P.block > 0 && o.isRacer && ov > v - 0.5) {
        // défendre : se décaler vers la ligne du poursuivant
        want = clamp(lerp(want, oLat, 0.6 * this.P.block), -room, room);
      } else if (Math.abs(fwd) <= (car.L + (o.L || 4.5)) / 2 + 0.5 && Math.abs(dl) < width + 0.6 && this.P.risk < 0.6) {
        want = clamp(myLat - Math.sign(dl || 1) * 0.8, -room, room); // côte à côte : laisser de la place
      }
    }
    // tremplins : traités comme des obstacles fixes (sauf épreuve de cascade)
    if (!this.useRamps) for (const rp of MAP.ramps) {
      const dx = rp.x + rp.s * rp.len * 0.5 - car.x, dz = rp.z + rp.c * rp.len * 0.5 - car.z;
      if (dx * dx + dz * dz > 8100) continue;
      const fwd = dx * R.tx[this.i] + dz * R.tz[this.i];
      if (fwd < -4 || fwd > 30 + v) continue;
      const rl = R.lateral(rp.x, rp.z, R.nearest(rp.x, rp.z, this.i, 2, 12)), wdt = rp.width / 2 + car.W / 2 + 0.8;
      if (Math.abs(rl - want) < wdt) { const side = room - rl > room + rl ? 1 : -1; want = clamp(rl + side * (wdt + 0.3), -room, room); if (Math.abs(rl - want) < wdt - 0.5) want = clamp(rl - side * (wdt + 0.3), -room - 2, room + 2); }
    }
    this.offT = want;
    this.off = damp(this.off, this.offT, 1.6 + this.P.risk, dt);
    // --- point visé (poursuite pure) ---
    // --- direction : contrôleur de Stanley (cap de la trajectoire + écart latéral) avec anticipation de courbure ---
    const sNow = R.project(car.x, car.z, this.i);
    R.at(sNow + 2.5 + v * 0.16, 0, _a);
    // courbure désirée = courbure de la trajectoire + rappel (cap, écart latéral), puis modèle inverse du véhicule :
    // braquage = empattement × courbure + différence des angles de dérive avant / arrière nécessaires.
    const kPath = R.k[R.idx(_a.i + 1)];
    const psi = wrapAngle(_a.yaw - car.yaw), eLat = this.off - myLat;
    let kd = kPath + clamp((2.0 * psi + (1.2 * eLat) / (v * 0.5 + 5)) / (v * 0.8 + 6), -0.09, 0.09);
    if (this.err > 0 && this.errKind === 1) kd += Math.sin(this.err * 14) * 0.004;
    const mu = car.stats.grip, Bt = 8.5 + 3 * (1 - car.stats.susp * 0.3);
    const u = Math.min(0.97, (v * v * Math.abs(kd)) / (mu * 9.81)), al = Math.tan(Math.asin(u) / 1.55);
    const delta = car.wb * kd + Math.sign(kd) * (al / Bt - al / (Bt * 1.22));
    const maxSteer = maxSteerAt(car, v);
    let steer = clamp(-delta / maxSteer, -1, 1);
    // --- vitesse cible ---
    // freinage anticipé : décélération requise pour respecter le profil sur l'horizon de freinage
    const aBrk = car.stats.brake * 9.81 * 0.82 * this.D.skill;
    let vt = this.v[this.i], need = 0;
    const horizon = (v * v) / (2 * aBrk) + 25 + v * 0.3;
    for (let q = 1, dd = 0; q < 60 && dd < horizon; q++) {
      const j = R.idx(this.i + q);
      if (!R.closed && this.i + q >= R.n) break;
      dd = (R.s[j] - R.s[this.i] + R.len) % R.len;
      const vj = this.v[j] * (this.err > 0 && this.errKind === 0 ? 1.15 : 1);
      if (vj < v) need = Math.max(need, (v * v - vj * vj) / (2 * Math.max(1.5, dd - v * 0.12)));
      if (q <= 2) vt = Math.min(vt, this.v[j]);
    }
    // élastique discret selon la difficulté
    if (ctx && ctx.playerProg !== undefined && this.D.band > 0) {
      const gap = this.prog - ctx.playerProg;
      this.band = damp(this.band, 1 + clamp(-gap / 400, -0.07, 0.09) * this.D.band, 1, dt);
      vt *= this.band;
    }
    vt = Math.min(vt, slow);
    if (!R.closed && this.i >= R.n - 2) vt = Math.min(vt, 30);
    this.targetV = vt;
    let th = 0, br = 0;
    const bn = need / aBrk;
    if (v > vt + 1 || bn > 0.55) br = clamp(Math.max((v - vt) * 0.25, (bn - 0.5) * 2.2), 0, 1);
    else if (v < vt - 0.8 && bn < 0.4) th = clamp((vt - v) * 0.5, 0.25, 1) * Math.min(1, this.D.power + 0.1);
    else if (bn < 0.3) th = 0.2;
    // glisse excessive : lever le pied (sauf drifteur)
    if (Math.abs(car.beta) > 0.25 && this.P.drift < 0.5) th *= 0.4;
    // --- drifteur : frein à main à l'entrée des virages serrés ---
    let hb = false;
    if (this.P.drift > 0.5) {
      const k = Math.abs(R.k[R.idx(this.i + 3)]);
      if (k > 0.02 && v > 15 && this.hbT <= 0 && Math.abs(car.beta) < 0.1) this.hbT = 0.28;
      if (this.hbT > 0) { this.hbT -= dt; hb = true; th = 1; }
      if (car.drifting) th = Math.max(th, 0.75);
    }
    // --- nitro en ligne droite ---
    let straight = true;
    for (let q = 0; q < 12; q++) if (Math.abs(R.k[R.idx(this.i + q)]) > 0.006) { straight = false; break; }
    I.nitro = straight && th > 0.9 && car.nitro > (1 - this.P.nitro) * 0.8 + 0.15 && v > 18 && v < vt - 4;
    // --- blocage : marche arrière puis remise en piste ---
    if (v < 1.2 && !car.frozen) this.stuck += dt; else if (v > 3) this.stuck = 0;
    if (this.stuck > 1.6 && this.rev <= 0) { this.rev = 1.3; }
    if (this.rev > 0) { this.rev -= dt; th = 0; br = 1; steer = -steer; if (this.rev <= 0) this.stuck = 0.5; }
    if (this.stuck > 6 || dOff > hw + 45) this.respawn();
    I.steer = steer; I.throttle = th; I.brake = br; I.handbrake = hb;
  }
  respawn() {
    const R = this.route, car = this.car;
    const s = R.project(car.x, car.z, this.i);
    R.at(s + 6, this.lineOff, _b);
    car.place(_b.x, _b.z, _b.yaw, 8);
    this.stuck = 0; this.rev = 0; this.ghost = 1.5;
  }
}
