// Physique automobile : modèle « bicyclette » à angles de dérive, transfert de masse, traction limitée par l'adhérence,
// boîte de vitesses, frein à main, nitro, aspiration, suspension visuelle, sauts et collisions par impulsions.
// Module purement logique (testable sans moteur de rendu).
import { clamp, lerp, smoothstep, damp, wrapAngle } from '../core/util.js';
import * as MAP from '../world/map.js';

const G = 9.81;
export const SURF = {
  asphalt: { mu: 1.0, roll: 0.12, loose: 0, dust: 0 },
  cobble: { mu: 0.92, roll: 0.2, loose: 0, dust: 0 },
  pavement: { mu: 0.95, roll: 0.16, loose: 0, dust: 0 },
  dirt: { mu: 0.7, roll: 0.5, loose: 1, dust: 1 },
  sand: { mu: 0.58, roll: 1.1, loose: 1, dust: 1 },
  grass: { mu: 0.6, roll: 0.9, loose: 1, dust: 0.5 },
  water: { mu: 0.4, roll: 6, loose: 1, dust: 0 },
};
export const STYLES = {
  arcade: { name: 'Arcade', desc: 'Adhérence généreuse, contre-braquage automatique.', grip: 1.12, rear: 1.0, counter: 0.9, stab: 1.6, tcs: 1 },
  sport: { name: 'Sport', desc: 'Comportement réaliste, aides légères.', grip: 1.0, rear: 1.0, counter: 0.45, stab: 0.8, tcs: 0.5 },
  drift: { name: 'Drift', desc: 'Train arrière libéré, glisses faciles à tenir.', grip: 1.0, rear: 0.84, counter: 0.75, stab: 0.5, tcs: 0 },
  offroad: { name: 'Off-road', desc: 'Motricité renforcée sur terre, sable et herbe.', grip: 1.0, rear: 0.97, counter: 0.7, stab: 1.0, tcs: 0.6, off: 0.35 },
};
// réglage interne des pilotes IA : marge d'adhérence et stabilité (ils planifient sous la limite)
STYLES.ai = { name: 'IA', hidden: true, grip: 1.14, rear: 1.06, counter: 0.5, stab: 1.3, tcs: 1 };
export const DEFAULT_ASSIST = { style: 'arcade', steer: 0.7, tcs: true, abs: true, brake: false, auto: true };

// Braquage maximal utile : assez pour amener les pneus avant à leur pic d'adhérence, quelle que soit la vitesse.
export const maxSteerAt = (car, v) => Math.min(0.62, (car.wb * car.stats.grip * G) / Math.max(1, v * v) + 0.2);

export class Car {
  constructor(spec, stats, opts = {}) {
    this.spec = spec;
    this.id = opts.id ?? 0;
    this.isPlayer = !!opts.player;
    this.assist = { ...DEFAULT_ASSIST, ...(opts.assist || {}) };
    this.setStats(stats || spec.stats);
    const S = spec.shape;
    this.L = S.L; this.W = S.W; this.wb = S.wb; this.wr = S.wr;
    this.zf = S.L / 2 - S.fo * S.L; this.zr = this.zf - S.wb;
    this.trk = S.W / 2 - 0.12;
    this.rad = S.W / 2 + 0.05;
    this.x = 0; this.y = 0; this.z = 0; this.yaw = 0;
    this.vx = 0; this.vz = 0; this.vy = 0; this.yawRate = 0;
    this.vf = 0; this.vl = 0; this.speed = 0;
    this.pitch = 0; this.roll = 0; this.gPitch = 0; this.gRoll = 0; this.sPitch = 0; this.sRoll = 0; this.heave = 0; this.heaveV = 0;
    this.steer = 0; this.rpm = 900; this.gear = 1; this.shiftT = 0; this.wheelRot = 0; this.rearRot = 0;
    this.airborne = false; this.airTime = 0; this.lastAir = 0; this.jumpX = 0; this.jumpZ = 0; this.jumpDist = 0; this.landed = 0;
    this.nitro = 1; this.nitroOn = false; this.draft = 0;
    this.slipR = 0; this.slipF = 0; this.spin = 0; this.drifting = false; this.driftAngle = 0; this.beta = 0;
    this.surf = 'asphalt'; this.wetGrip = 1; this.onRoad = true;
    this.ax = 0; this.ay = 0; this.impact = 0; this.impactX = 0; this.impactZ = 0; this.damage = 0;
    this.in = { steer: 0, throttle: 0, brake: 0, handbrake: false, nitro: false, up: false, down: false };
    this.manual = false; this.reverse = false; this.frozen = false; this.limiter = 0;
    this.wy = [0, 0, 0, 0]; this.gy = 0;
  }
  setStats(st) {
    this.stats = st;
    this.mass = st.mass;
    this.vmax = st.top / 3.6;
    this.a0 = (27.78 / st.a100) * 1.16 * (1 + 0.5 * Math.pow(27.78 / this.vmax, 2));
    this.gears = st.gears;
    this.inertia = this.mass * (this.spec.shape.L ** 2 + this.spec.shape.W ** 2) / 12 * 1.25;
    this.dragC = (this.a0 * 0.15) / (this.vmax * this.vmax); // par unité de masse
    this.redline = st.eng === 'ev' ? 12000 : st.eng === 'v8m' ? 6200 : st.eng === 'rotary' ? 9000 : st.eng === 'v10' || st.eng === 'v12' ? 8600 : 7200;
    this.idle = st.eng === 'ev' ? 0 : 850;
    this.shiftTime = st.shift ?? 0.2;
  }
  place(x, z, yaw, speed = 0) {
    this.x = x; this.z = z; this.yaw = yaw; this.y = MAP.groundHeight(x, z); this.gy = this.y;
    this.vx = Math.sin(yaw) * speed; this.vz = Math.cos(yaw) * speed; this.vy = 0; this.yawRate = 0; this.vf = speed; this.vl = 0; this.speed = speed;
    this.airborne = false; this.steer = 0; this.sPitch = 0; this.sRoll = 0; this.heave = 0; this.heaveV = 0; this.gear = 1; this.impact = 0;
    this.pitch = this.gPitch = 0; this.roll = this.gRoll = 0;
    this.sampleGround();
    this.y = this.gy;
  }
  gearTop(g) { return this.gears <= 1 ? this.vmax * 1.12 : this.vmax * 1.08 * Math.pow(g / this.gears, 0.82); }
  sampleGround() {
    const s = Math.sin(this.yaw), c = Math.cos(this.yaw), w = this.wy, t = this.trk;
    // 0 AVG, 1 AVD, 2 ARG, 3 ARD  (gauche = +x local)
    w[0] = MAP.groundHeight(this.x + s * this.zf + c * t, this.z + c * this.zf - s * t);
    w[1] = MAP.groundHeight(this.x + s * this.zf - c * t, this.z + c * this.zf + s * t);
    w[2] = MAP.groundHeight(this.x + s * this.zr + c * t, this.z + c * this.zr - s * t);
    w[3] = MAP.groundHeight(this.x + s * this.zr - c * t, this.z + c * this.zr + s * t);
    this.gy = (w[0] + w[1] + w[2] + w[3]) / 4;
    this.gPitchT = Math.atan2((w[2] + w[3]) / 2 - (w[0] + w[1]) / 2, this.wb);
    this.gRollT = Math.atan2((w[0] + w[2]) / 2 - (w[1] + w[3]) / 2, t * 2);
  }
  step(dt, env) {
    if (this.frozen) { this.rpmFree(dt); return; }
    const I = this.in, st = this.stats, style = STYLES[this.assist.style] || STYLES.arcade;
    const s = Math.sin(this.yaw), c = Math.cos(this.yaw);
    let vf = this.vx * s + this.vz * c, vl = this.vx * c - this.vz * s;
    const speed = Math.hypot(vf, vl);
    // --- surface ---
    const surfName = MAP.surfaceAt(this.x, this.z);
    this.surf = surfName;
    const SF = SURF[surfName];
    const off = clamp(st.off + (style.off || 0), 0, 1);
    const wet = SF.loose ? 1 : (env && env.grip) || 1;
    const muS = SF.loose ? lerp(SF.mu, 0.96, off * 0.8) : SF.mu;
    const mu = st.grip * style.grip * muS * wet;
    this.onRoad = !SF.loose || surfName === 'dirt' || surfName === 'sand';
    // --- direction ---
    const maxSteer = maxSteerAt(this, speed);
    this.beta = speed > 3 ? Math.atan2(vl, Math.abs(vf)) : 0;
    let target = -I.steer * maxSteer;
    const ca = style.counter * this.assist.steer;
    if (speed > 6 && !this.airborne) target += clamp(this.beta * ca * 0.9, -0.5, 0.5);
    target = clamp(target, -0.62, 0.62);
    this.steer = damp(this.steer, target, 14 / (1 + speed / 55), dt);
    const d = this.steer;
    // --- boîte et moteur ---
    const n = this.gears;
    let throttle = I.throttle, brake = I.brake;
    if (this.assist.auto !== false && !this.manual) {
      if (brake > 0.1 && throttle < 0.1 && vf < 1.2) { this.reverse = true; }
      else if (throttle > 0.1 && vf > -1.2) this.reverse = false;
      else if (brake < 0.05 && throttle < 0.05 && Math.abs(vf) < 0.5) this.reverse = false;
    }
    if (this.reverse) { const t = throttle; throttle = brake; brake = t; }
    const dirSign = this.reverse ? -1 : 1, av = Math.abs(vf);
    if (this.shiftT > 0) this.shiftT -= dt;
    const gt = this.gearTop(this.gear), gPrev = this.gear > 1 ? this.gearTop(this.gear - 1) : 0;
    let rpmN = clamp(av / gt, 0, 1.05);
    if (this.manual) {
      if (I.up && this.gear < n && this.shiftT <= 0) { this.gear++; this.shiftT = this.shiftTime; this.shifted = 1; }
      if (I.down && this.gear > 1 && this.shiftT <= 0) { this.gear--; this.shiftT = this.shiftTime; this.shifted = -1; }
      I.up = I.down = false;
    } else if (n > 1 && this.shiftT <= 0 && !this.airborne) {
      if (rpmN > 0.94 && this.gear < n && throttle > 0.2) { this.gear++; this.shiftT = this.shiftTime; this.shifted = 1; }
      else if (this.gear > 1 && av < gPrev * (throttle > 0.8 ? 0.8 : 0.62)) { this.gear--; this.shiftT = this.shiftTime * 0.6; this.shifted = -1; }
    }
    rpmN = clamp(av / this.gearTop(this.gear), 0, 1.05);
    const band = st.eng === 'ev' ? 1 - 0.25 * rpmN : 0.74 + 0.26 * Math.sin(Math.PI * clamp(rpmN * 0.92 + 0.08, 0, 1));
    const overRev = rpmN > 1 ? 0 : 1;
    this.nitroOn = I.nitro && this.nitro > 0.01 && throttle > 0.1 && !this.reverse;
    const boost = this.nitroOn ? 1 : 0;
    if (this.nitroOn) this.nitro = Math.max(0, this.nitro - dt / (env && env.nitroDur ? env.nitroDur : 4.2));
    const vmaxE = this.vmax * (1 + 0.1 * boost + 0.05 * this.draft);
    const envl = Math.max(0, 1 - 0.8 * Math.pow(av / vmaxE, 1.7));
    const gearPull = this.manual ? lerp(1.15, 0.8, (this.gear - 1) / Math.max(1, n - 1)) * (0.55 + 0.45 * Math.sin(Math.PI * clamp(rpmN, 0.05, 1))) / 0.9 : 1;
    let drive = this.a0 * envl * band * (1 + 0.42 * boost + 0.18 * this.draft) * throttle * overRev * (this.shiftT > 0 ? 0.15 : 1) * gearPull;
    if (this.reverse) drive = av < 13 ? this.a0 * 0.55 * throttle : 0;
    if (env && env.speedLimit && av > env.speedLimit) drive = 0;
    // --- charges ---
    const hcg = 0.42 + 0.14 * st.susp, wf = st.wf;
    const transfer = clamp((this.ax * hcg) / (this.wb * G), -0.2, 0.2) * 0.8;
    const Nf = G * clamp(wf - transfer, 0.15, 0.85), Nr = G * clamp(1 - wf + transfer, 0.15, 0.85); // par unité de masse
    const fDrive = st.drive === 'FWD' ? 1 : st.drive === 'AWD' ? 0.4 : 0, rDrive = 1 - fDrive;
    // --- traction ---
    const tractionCap = mu * (Nf * fDrive + Nr * rDrive) * (st.drive === 'AWD' ? 1.5 : 1.12);
    let spinning = 0;
    const tcs = this.assist.tcs ? style.tcs : 0;
    if (drive > tractionCap) {
      spinning = clamp((drive - tractionCap) / (this.a0 * 0.5 + 0.1), 0, 1);
      drive = tractionCap * (0.97 - 0.12 * spinning * (1 - tcs));
      if (tcs >= 1) spinning *= 0.25;
    }
    // burnout : frein + gaz à l'arrêt (propulsion / intégrale)
    this.burnout = false;
    if (!this.reverse && I.throttle > 0.7 && (I.brake > 0.5 || I.handbrake) && av < 3 && st.drive !== 'FWD' && !this.airborne) {
      this.burnout = true; spinning = 1; drive = 0; brake = 1;
      this.nitro = Math.min(1, this.nitro + dt * 0.03);
    }
    this.spin = damp(this.spin, spinning, 10, dt);
    // --- freinage ---
    let brakeA = brake * st.brake * mu * G * (this.assist.abs ? 1 : (brake > 0.9 ? 0.82 : 1));
    if (I.handbrake) brakeA += 0.55 * mu * Nr;
    const rollA = SF.roll * (SF.loose ? 1 - off * 0.75 : 1) + 0.04 * av * (SF.loose ? (1 - off * 0.8) * 0.5 : 0);
    const dragA = this.dragC * (1 - 0.3 * this.draft) * av * av + rollA + (throttle < 0.05 ? 0.6 + av * 0.012 : 0);
    // --- forces latérales (angles de dérive) ---
    let fx = 0, fy = 0, torque = 0;
    if (!this.airborne) {
      // centre de gravité cohérent avec la répartition des masses (sinon le modèle est instable en lacet)
      const a = this.wb * (1 - wf), b = this.wb * wf;
      const vfa = Math.max(Math.abs(vf), 1.2);
      let af = Math.atan2(vl + a * this.yawRate, vfa) - d * Math.sign(vf || 1), ar = Math.atan2(vl - b * this.yawRate, vfa);
      const B = 8.5 + 3 * (1 - st.susp * 0.3);
      const curve = (al, fall, kB = 1) => { const x = B * kB * al, ax = Math.abs(x); return ax <= 1.9 ? Math.sin(1.55 * Math.atan(x)) : Math.sign(x) * lerp(1, fall, smoothstep(1.9, 6, ax)); };
      const driftiness = st.drift * (this.assist.style === 'drift' ? 1 : 0.6);
      const fallR = lerp(0.98, 0.76, driftiness), fallF = 0.88; // à la limite, l'avant décroche avant l'arrière (sous-virage stable) sauf réglage drift
      // cercle de traction arrière : la puissance et le frein à main consomment l'adhérence latérale
      const useR = clamp((Math.abs(drive) * rDrive) / Math.max(0.1, mu * Nr), 0, 1) * (st.drive === 'RWD' ? 1 : 0.6) * (1 - 0.6 * tcs * (1 - driftiness));
      let rearMu = mu * style.rear * Math.sqrt(Math.max(0.12, 1 - useR * useR * 0.8)) * (1 - 0.35 * this.spin * (st.drive === 'FWD' ? 0 : 1));
      if (I.handbrake) rearMu *= 0.42;
      let frontMu = mu * (1 - (st.drive !== 'RWD' ? 0.25 * this.spin : 0));
      if (brake > 0.95 && !this.assist.abs) frontMu *= 0.7;
      const Fyf = -curve(af, fallF) * frontMu * Nf, Fyr = -curve(ar, fallR, 1.22) * rearMu * Nr;
      this.slipF = Math.abs(af); this.slipR = Math.abs(ar);
      const cd = Math.cos(d), sd = Math.sin(d);
      fy = Fyf * cd + Fyr;
      fx = drive * dirSign * (fDrive * cd + rDrive) - Fyf * sd;
      torque = (a * Fyf * cd - b * Fyr) * this.mass + drive * dirSign * fDrive * sd * a * this.mass;
      // freinage (opposé au déplacement longitudinal)
      const bsig = av > 0.3 ? Math.sign(vf) : 0;
      fx -= bsig * Math.min(brakeA, av / dt);
      if (av <= 0.3 && brake > 0.1 && throttle < 0.05) { vf *= 0.5; }
      // pente
      fx += G * Math.sin(this.gPitch);
      fy -= G * Math.sin(this.gRoll) * 0.6;
      // stabilité (aide) : amortit le lacet excessif
      // contrôle de stabilité : amortit seulement le lacet en excès par rapport à l'intention du conducteur
      const rLim = (mu * G) / Math.max(4, Math.abs(vf)), rWant = clamp((vf * Math.tan(d)) / this.wb, -rLim, rLim);
      torque -= (this.yawRate - rWant) * this.inertia * style.stab * 0.9 * (I.handbrake ? 0.3 : 1);
    } else {
      torque = -this.yawRate * this.inertia * 0.4;
    }
    fx -= Math.sign(vf) * Math.min(dragA, av / dt + 0.001);
    // --- intégration plane ---
    // (les termes de rotation du repère sont pris en compte par le passage en coordonnées monde ci-dessous)
    const dvf = fx, dvl = fy;
    this.ax = damp(this.ax, fx, 12, dt); this.ay = damp(this.ay, fy, 12, dt);
    vf += dvf * dt; vl += dvl * dt;
    this.yawRate += (torque / this.inertia) * dt;
    // basse vitesse : cinématique (évite les oscillations du modèle à l'arrêt)
    if (!this.airborne) {
      const k = 1 - smoothstep(1.5, 6, Math.abs(vf));
      if (k > 0) {
        const rk = (vf * Math.tan(d)) / this.wb;
        this.yawRate = lerp(this.yawRate, rk, k * Math.min(1, dt * 20));
        vl = lerp(vl, 0, k * Math.min(1, dt * 12));
      }
    }
    this.yawRate = clamp(this.yawRate, -4.5, 4.5);
    this.yaw = wrapAngle(this.yaw + this.yawRate * dt);
    const s2 = Math.sin(this.yaw), c2 = Math.cos(this.yaw);
    // la vitesse monde est conservée ; seule la base locale tourne
    this.vx = vf * s + vl * c; this.vz = vf * c - vl * s;
    this.x += this.vx * dt; this.z += this.vz * dt;
    this.vf = this.vx * s2 + this.vz * c2; this.vl = this.vx * c2 - this.vz * s2;
    this.speed = Math.hypot(this.vx, this.vz);
    // --- vertical : suivi du sol, sauts, réceptions ---
    const pgy = this.gy;
    this.sampleGround();
    this.vy -= G * dt * 1.35;
    this.y += this.vy * dt;
    this.landed = 0;
    if (this.y <= this.gy + 0.02) {
      // vitesse verticale du sol, filtrée : une marche isolée du relief ne doit pas catapulter la voiture
      const gvRaw = clamp((this.gy - pgy) / dt, -40, this.speed * 0.55 + 1);
      this.gvS = damp(this.gvS || 0, gvRaw, 14, dt);
      const gv = Math.min(gvRaw, Math.max(0, this.gvS) + 0.6);
      if (this.airborne) {
        const hit = Math.max(0, gv - this.vy);
        if (this.airTime > 0.3) { this.landed = hit; this.lastAir = this.airTime; this.jumpDist = Math.hypot(this.x - this.jumpX, this.z - this.jumpZ); this.heaveV -= Math.min(4, hit * 0.35); }
        this.airborne = false;
      }
      this.y = this.gy;
      this.vy = Math.max(this.vy, gv);
      this.airTime = 0;
    } else if (this.y > this.gy + 0.22) {
      this.gvS = 0;
      if (!this.airborne) { this.airborne = true; this.jumpX = this.x; this.jumpZ = this.z; this.airTime = 0; }
    }
    if (this.airborne) this.airTime += dt;
    // --- attitude visuelle ---
    if (!this.airborne) { this.gPitch = damp(this.gPitch, this.gPitchT, 14, dt); this.gRoll = damp(this.gRoll, this.gRollT, 14, dt); }
    else { this.gPitch = damp(this.gPitch, clamp(-this.vy * 0.018, -0.3, 0.35), 2.5, dt); this.gRoll = damp(this.gRoll, 0, 1.5, dt); }
    const soft = 0.55 + 0.6 * st.susp;
    this.sPitch = damp(this.sPitch, clamp(-this.ax * 0.0075 * soft, -0.07, 0.07), 7, dt);
    this.sRoll = damp(this.sRoll, clamp(this.ay * 0.009 * soft, -0.09, 0.09), 7, dt);
    this.heaveV += (-this.heave * 90 - this.heaveV * 9) * dt;
    this.heave = clamp(this.heave + this.heaveV * dt, -0.14, 0.1);
    this.pitch = this.gPitch + this.sPitch; this.roll = this.gRoll + this.sRoll;
    // --- états dérivés ---
    const slipDrift = Math.abs(this.beta);
    this.drifting = !this.airborne && this.speed > 8 && slipDrift > 0.17 && vf > 2;
    this.driftAngle = this.drifting ? slipDrift : 0;
    this.skid = this.airborne ? 0 : clamp(Math.max((this.slipR - 0.11) * 5, this.spin * 0.9, I.handbrake && this.speed > 4 ? 1 : 0, brake > 0.95 && !this.assist.abs && this.speed > 6 ? 0.8 : 0), 0, 1);
    this.wheelRot += (this.vf / this.wr) * dt;
    this.rearRot += ((I.handbrake && !this.burnout ? 0 : this.vf / this.wr) + (st.drive !== 'FWD' ? this.spin * 60 : 0)) * dt;
    const rn = clamp(Math.abs(this.vf) / this.gearTop(this.gear), 0, 1.02);
    const tgtRpm = this.idle + (this.redline - this.idle) * Math.max(rn, this.burnout ? 0.9 : this.spin * 0.8, this.airborne ? throttle * 0.95 : 0);
    this.rpm = damp(this.rpm, tgtRpm, this.shiftT > 0 ? 9 : 16, dt);
    this.throttleOut = throttle; this.brakeOut = brake;
    if (this.nitro < 1 && !this.nitroOn) this.nitro = Math.min(1, this.nitro + dt * ((this.drifting ? 0.05 : 0) + (this.airborne ? 0.08 : 0) + this.draft * 0.035 + 0.004));
    this.draft = damp(this.draft, 0, 1.2, dt);
    if (this.impact > 0) this.impact = Math.max(0, this.impact - dt * 30);
  }
  rpmFree(dt) {
    const t = this.in.throttle;
    this.rpm = damp(this.rpm, this.idle + (this.redline - this.idle) * t * 0.92, t > 0.1 ? 9 : 5, dt);
    this.sampleGround(); this.y = this.gy; this.gPitch = this.gPitchT; this.gRoll = this.gRollT; this.pitch = this.gPitch; this.roll = this.gRoll;
    this.throttleOut = t; this.brakeOut = 1; this.speed = 0; this.vf = 0; this.vx = this.vz = 0; this.skid = 0; this.spin = 0; this.drifting = false;
  }
  get kmh() { return this.speed * 3.6; }
}

// --------------------------------------------------------------------------------------------
// Collisions
// --------------------------------------------------------------------------------------------
const OFFS = [1, 0, -1];
// Applique une impulsion J (monde) au point de contact (rx, rz relatifs au centre).
function impulse(b, jx, jz, rx, rz) {
  b.vx += jx / b.mass; b.vz += jz / b.mass;
  b.yawRate += (rz * jx - rx * jz) / b.inertia;
}
// vitesse du point de contact
const pvx = (b, rz) => b.vx + b.yawRate * rz;
const pvz = (b, rx) => b.vz - b.yawRate * rx;

// Collision d'un corps avec le décor statique. Renvoie l'intensité du choc (m/s).
export function collideStatic(b) {
  const s = Math.sin(b.yaw), c = Math.cos(b.yaw), half = Math.max(0, b.L / 2 - b.rad);
  let worst = 0;
  const W = MAP.walls, P = MAP.pillars;
  for (let k = 0; k < 3; k++) {
    const ox = s * half * OFFS[k], oz = c * half * OFFS[k];
    MAP.queryStatic(b.x + ox, b.z + oz, b.rad + 0.5, (id) => {
      const cx = b.x + ox, cz = b.z + oz;
      let px, pz, rr = b.rad;
      if (id >= 0) {
        const x1 = W[id * 4], z1 = W[id * 4 + 1], dx = W[id * 4 + 2] - x1, dz = W[id * 4 + 3] - z1;
        const t = clamp(((cx - x1) * dx + (cz - z1) * dz) / (dx * dx + dz * dz), 0, 1);
        px = x1 + dx * t; pz = z1 + dz * t;
      } else { const q = (-1 - id) * 3; px = P[q]; pz = P[q + 1]; rr += P[q + 2]; }
      let nx = cx - px, nz = cz - pz;
      const d2 = nx * nx + nz * nz;
      if (d2 >= rr * rr || d2 < 1e-8) return;
      const dd = Math.sqrt(d2);
      nx /= dd; nz /= dd;
      const pen = rr - dd;
      b.x += nx * pen; b.z += nz * pen;
      const rx = ox - nx * b.rad, rz = oz - nz * b.rad;
      const vn = pvx(b, rz) * nx + pvz(b, rx) * nz;
      if (vn < 0) {
        const rn = rz * nx - rx * nz;
        const j = (-(1 + 0.18) * vn) / (1 / b.mass + (rn * rn) / b.inertia);
        impulse(b, nx * j, nz * j, rx, rz);
        // frottement de raclage
        const tx = -nz, tz = nx, vt = pvx(b, rz) * tx + pvz(b, rx) * tz;
        const jt = clamp(-vt * b.mass * 0.35, -j * 0.3, j * 0.3);
        impulse(b, tx * jt, tz * jt, rx, rz);
        if (-vn > worst) { worst = -vn; b.impactX = cx - nx * b.rad; b.impactZ = cz - nz * b.rad; }
      }
    });
  }
  if (worst > 0.5) { b.impact = Math.max(b.impact || 0, worst); b.yawRate = clamp(b.yawRate, -3.5, 3.5); }
  return worst;
}

// Collision entre deux corps (3 cercles chacun). Renvoie l'intensité du choc.
export function collidePair(a, b) {
  const dx0 = b.x - a.x, dz0 = b.z - a.z, reach = (a.L + b.L) / 2 + 0.6;
  if (dx0 * dx0 + dz0 * dz0 > reach * reach) return 0;
  if (Math.abs((a.y || 0) - (b.y || 0)) > 1.6) return 0;
  const sa = Math.sin(a.yaw), ca = Math.cos(a.yaw), sb = Math.sin(b.yaw), cb = Math.cos(b.yaw);
  const ha = Math.max(0, a.L / 2 - a.rad), hb = Math.max(0, b.L / 2 - b.rad);
  let worst = 0;
  for (let i = 0; i < 3; i++) for (let k = 0; k < 3; k++) {
    const aox = sa * ha * OFFS[i], aoz = ca * ha * OFFS[i], box = sb * hb * OFFS[k], boz = cb * hb * OFFS[k];
    let nx = b.x + box - (a.x + aox), nz = b.z + boz - (a.z + aoz);
    const rr = a.rad + b.rad, d2 = nx * nx + nz * nz;
    if (d2 >= rr * rr || d2 < 1e-8) continue;
    const dd = Math.sqrt(d2);
    nx /= dd; nz /= dd;
    const pen = rr - dd, ma = b.mass / (a.mass + b.mass), mb = 1 - ma;
    a.x -= nx * pen * ma; a.z -= nz * pen * ma; b.x += nx * pen * mb; b.z += nz * pen * mb;
    const rax = aox + nx * a.rad, raz = aoz + nz * a.rad, rbx = box - nx * b.rad, rbz = boz - nz * b.rad;
    const vn = (pvx(b, rbz) - pvx(a, raz)) * nx + (pvz(b, rbx) - pvz(a, rax)) * nz;
    if (vn < 0) {
      const rna = raz * nx - rax * nz, rnb = rbz * nx - rbx * nz;
      const j = (-(1 + 0.25) * vn) / (1 / a.mass + 1 / b.mass + (rna * rna) / a.inertia + (rnb * rnb) / b.inertia);
      impulse(a, -nx * j, -nz * j, rax, raz); impulse(b, nx * j, nz * j, rbx, rbz);
      if (-vn > worst) { worst = -vn; a.impactX = b.impactX = a.x + aox + nx * a.rad; a.impactZ = b.impactZ = a.z + aoz + nz * a.rad; }
    }
  }
  if (worst > 0.5) { a.impact = Math.max(a.impact || 0, worst); b.impact = Math.max(b.impact || 0, worst); }
  return worst;
}
// Après une collision, la vitesse monde a changé : resynchroniser les composantes locales.
export function resync(b) {
  const s = Math.sin(b.yaw), c = Math.cos(b.yaw);
  b.vf = b.vx * s + b.vz * c; b.vl = b.vx * c - b.vz * s; b.speed = Math.hypot(b.vx, b.vz);
}
