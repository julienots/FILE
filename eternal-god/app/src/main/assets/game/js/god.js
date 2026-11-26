// Le héros divin : déplacements (marche, course, saut, vol divin presque debout, vol « comète »,
// marche sur l'eau), atterrissage de super-héros, combos au corps à corps avec poursuite de l'ennemi,
// transformation (aura), projection sans jamais mourir.
// Animation procédurale amortie par ressorts : chaque os rejoint sa pose cible avec inertie.
import * as THREE from 'three';
import { G, WORLD, clamp, lerp, damp, dampAngle, angleDelta, rand, emit } from './core.js';
import { buildGodModel } from './godmodel.js';

const _e = new THREE.Euler(), _q = new THREE.Quaternion(), V = (x = 0, y = 0, z = 0) => new THREE.Vector3(x, y, z);

export class God {
  constructor(scene, model = null) {
    this.scene = scene;
    this.setModel(model || buildGodModel());
    this.pos = V(0, 60, 40);
    this.vel = V();
    this.heading = Math.PI;
    this.state = 'fly';            // ground | air | fly | knock | teleport
    this.speed = 0;
    this.phase = 0;
    this.lastStep = 0;
    this.anim = { walk: 0, run: 0, fly: 1, spread: 1, crouch: 0, pitch: 0, roll: 0, knock: 0, idleT: 0, boost: 0, hoverK: 1, land: 0, stance: 0 };
    this.action = null;            // { kind, t, dur }
    this.flapPhase = 0;
    this.airTime = 0;
    this.height = 5.2;
    this.stunned = 0;
    this.teleportT = 0;
    this.power = 1;                // multiplicateur de puissance (aura : ×2,5)
    this.speedMul = 1;
    this.landT = 0;                // atterrissage de super-héros
  }

  setModel(model) {
    if (this.root) this.scene.remove(this.root);
    this.model = model;
    this.root = model.root;
    this.B = model.bones;
    this.scene.add(this.root);
  }

  groundAt(x, z) {
    let g = G.terrain.heightAt(x, z);
    if (g < WORLD.water - 1.2) g = WORLD.water; // il marche sur l'eau profonde
    const isl = G.env ? G.env.islandGround(x, z, this.pos.y) : -Infinity;
    return Math.max(g, isl);
  }
  onWater() { return this.onWaterAt() && this.state === 'ground'; }
  onWaterAt() { return G.terrain.heightAt(this.pos.x, this.pos.z) < WORLD.water - 1.2; }

  // ---------- commandes ----------
  jump() {
    if (this.state === 'ground') { this.vel.y = 26; this.state = 'air'; this.airTime = 0; this.playAction('jump', 0.6); G.fx && G.fx.dust(this.pos, 3, 6); }
    else if (this.state === 'air') this.startFly();
  }
  toggleFly() { if (this.state === 'fly') this.stopFly(); else this.startFly(); }
  startFly() {
    const fromGround = this.state === 'ground';
    this.state = 'fly'; this.vel.y = Math.max(this.vel.y, fromGround ? 16 : 6);
    if (fromGround && G.fx) { G.fx.ring(this.pos, 28, 0.6, 0xe8d8b8); G.fx.dust(this.pos, 6, 12); G.fx.shake(0.12, this.pos); this.playAction('takeoff', 0.7); }
    emit('godFly', true); if (G.audio) G.audio.wingFlap(1);
  }
  stopFly() { this.state = 'air'; this.airTime = 0; emit('godFly', false); }
  playAction(kind, dur) { this.action = { kind, t: 0, dur }; }
  knock(dir, power, stun = 0.8) {
    if (this.aura && this.aura.active) power *= 0.4; // transformé, il encaisse
    this.state = 'knock';
    this.vel.copy(dir).setY(0).normalize().multiplyScalar(power).setY(power * 0.45 + 6);
    this.stunned = stun; this.anim.knock = 1;
    if (G.fx) G.fx.shake(0.5, this.pos);
    emit('godKnocked', { power });
  }
  teleport(x, z) {
    if (this.state === 'teleport') return;
    this.state = 'teleport'; this.teleportT = 0;
    this.tpTo = V(x, this.groundAt(x, z), z);
    if (G.fx) { G.fx.sphere(this.pos.clone().add(V(0, 2.6, 0)), 8, 0.5, 0xfff0c0); G.fx.ring(this.pos, 18, 0.6); }
    if (G.audio) G.audio.teleport();
  }
  // fonce sur une cible (combos) : déplacement éclair avec traînée
  rushTo(p, stop = 7) {
    const d = p.clone().sub(this.pos); d.y = 0;
    const L = d.length(); if (L < stop + 0.5) return;
    this.rush = { from: this.pos.clone(), to: this.pos.clone().addScaledVector(d.normalize(), L - stop), t: 0, dur: Math.min(0.22, 0.08 + L / 600) };
    this.rush.to.y = Math.max(p.y - 3, this.groundAt(this.rush.to.x, this.rush.to.z));
    this.heading = Math.atan2(d.x, d.z);
  }

  // ---------- simulation ----------
  update(dt, input) {
    const cam = G.cam, a = this.anim;
    if (this.state === 'teleport') return this.updateTeleport(dt);
    if (this.rush) { // poursuite éclair
      const r = this.rush; r.t += dt;
      const k = Math.min(1, r.t / r.dur);
      const prev = this.pos.clone();
      this.pos.lerpVectors(r.from, r.to, 1 - Math.pow(1 - k, 3));
      if (G.fx) for (let i = 0; i < 3; i++) { const q = prev.clone().lerp(this.pos, i / 3).add(V(0, 2.6, 0)); const c = G.god.theme(); G.fx.add.emit(q.x, q.y, q.z, 0, 0, 0, 2.4, 0.25, c.r, c.g, c.b, 0.7, 0.8, 0, 0); }
      if (k >= 1) { this.rush = null; if (this.state === 'ground') this.pos.y = this.groundAt(this.pos.x, this.pos.z); }
      this.animate(dt, 1); return;
    }
    if (this.dash) { // charge de comète : trajectoire imposée
      const d = this.dash; d.t += dt;
      this.pos.addScaledVector(d.dir, d.speed * dt);
      const gnd = this.groundAt(this.pos.x, this.pos.z);
      if (this.pos.y < gnd + 0.5) this.pos.y = gnd + 0.5;
      this.vel.copy(d.dir).multiplyScalar(d.speed);
      this.heading = Math.atan2(d.dir.x, d.dir.z);
      const lim = WORLD.half - 25; this.pos.x = clamp(this.pos.x, -lim, lim); this.pos.z = clamp(this.pos.z, -lim, lim);
      if (d.t >= d.dur) { this.dash = null; this.vel.multiplyScalar(0.1); if (this.state !== 'fly') this.state = this.pos.y - gnd > 2 ? 'air' : 'ground'; G.fx && G.fx.ring(this.pos, 50, 0.7); }
      this.animate(dt, 1); return;
    }
    const mx = input.moveX, my = input.moveY, mag = Math.min(1, Math.hypot(mx, my));
    const yaw = cam.yaw;
    const fwd = V(-Math.sin(yaw), 0, -Math.cos(yaw));
    const right = V(-fwd.z, 0, fwd.x);
    const wish = fwd.clone().multiplyScalar(my).addScaledVector(right, mx);
    if (wish.lengthSq() > 1) wish.normalize();
    const g = this.groundAt(this.pos.x, this.pos.z);
    const busy = this.landT > 0.35; // relevé d'atterrissage : on ne bouge pas encore

    if (this.state === 'ground') {
      const run = mag > 0.82;
      const target = busy || mag < 0.08 ? 0 : (run ? 22 * this.speedMul : 3.5 + mag * 7);
      this.speed = damp(this.speed, target, target > this.speed ? (run ? 2.2 : 4) : 7, dt);
      if (mag > 0.08 && !busy) this.heading = dampAngle(this.heading, Math.atan2(wish.x, wish.z), run ? 5 : 8, dt);
      this.vel.set(Math.sin(this.heading) * this.speed, 0, Math.cos(this.heading) * this.speed);
      this.pos.x += this.vel.x * dt; this.pos.z += this.vel.z * dt;
      const ng = this.groundAt(this.pos.x, this.pos.z);
      if (ng < this.pos.y - 3) { this.state = 'air'; this.airTime = 0; this.vel.y = 0; }
      else this.pos.y = ng;
      this.footsteps(dt, run);
      this.bodyCollisions(dt);
      if (this.onWater() && Math.random() < dt * (2 + this.speed * 0.4) && G.env) G.env.addRipple(this.pos.x, this.pos.z, 0.6 + this.speed * 0.05);
    } else if (this.state === 'air' || this.state === 'knock') {
      this.airTime += dt;
      this.vel.y -= (this.state === 'knock' ? 30 : 44) * dt;
      if (this.state === 'air' && mag > 0.08) { this.vel.x = lerp(this.vel.x, wish.x * 18, dt * 2); this.vel.z = lerp(this.vel.z, wish.z * 18, dt * 2); this.heading = dampAngle(this.heading, Math.atan2(wish.x, wish.z), 4, dt); }
      this.pos.addScaledVector(this.vel, dt);
      this.bodyCollisions(dt);
      if (this.pos.y <= g) { const impact = -this.vel.y; this.pos.y = g; this.land(impact); }
    } else if (this.state === 'fly') {
      // vol divin : presque debout et lent ; joystick à fond = vol « comète » allongé
      const boost = mag > 0.92;
      const cruise = (boost ? 62 : 6 + mag * 16) * this.speedMul;
      let desire;
      if (boost) {
        const p = cam.pitch;
        desire = V(-Math.sin(yaw) * Math.cos(p), -Math.sin(p) * 0.9, -Math.cos(yaw) * Math.cos(p)).multiplyScalar(my).addScaledVector(right, mx);
        desire.normalize().multiplyScalar(cruise);
      } else desire = wish.clone().multiplyScalar(cruise);
      if (input.up) desire.y += 14 * this.speedMul;
      if (input.down) desire.y -= 16 * this.speedMul;
      this.vel.lerp(desire, 1 - Math.exp(-(boost ? 2.0 : 1.4) * dt));
      this.pos.addScaledVector(this.vel, dt);
      const hs = Math.hypot(this.vel.x, this.vel.z);
      if (hs > 1.2) this.heading = dampAngle(this.heading, Math.atan2(this.vel.x, this.vel.z), boost ? 3 : 2, dt);
      else if (mag < 0.05) this.heading = dampAngle(this.heading, yaw + Math.PI, 1.2, dt); // face au regard en vol stationnaire
      this.pos.y = Math.min(this.pos.y, 1100);
      if (this.pos.y < g + 0.6) {
        this.pos.y = g + 0.6;
        if (input.down || this.vel.y < -18) { const impact = -this.vel.y; this.state = 'ground'; emit('godFly', false); this.land(impact); }
        else this.vel.y = Math.max(this.vel.y, 0);
      }
      const alt = this.pos.y - g, sp = this.vel.length();
      G.uniforms.gust.value.set(this.pos.x, this.pos.y, this.pos.z, alt < 45 && sp > 8 ? clamp((45 - alt) / 45, 0, 1) * Math.min(1, sp / 40) * 6 : (this.aura && this.aura.active && alt < 20 ? 3 : 0));
      if (alt < 12 && sp > 30 && G.veg && Math.random() < dt * 4) G.veg.damageArea(this.pos.x, this.pos.z, 8, sp * 0.8, 'push', this.pos);
      if (alt < 6 && sp > 25 && G.fx && Math.random() < dt * 8) G.fx.dust(V(this.pos.x, g, this.pos.z), 4, 2);
    }
    if (this.state !== 'fly') G.uniforms.gust.value.w = damp(G.uniforms.gust.value.w, this.aura && this.aura.active ? 2.5 : 0, 3, dt);
    G.uniforms.press.value.set(this.pos.x, this.pos.z, this.state === 'ground' ? 3.5 : 0, this.state === 'ground' ? 1 : 0);
    const lim = WORLD.half - 25;
    this.pos.x = clamp(this.pos.x, -lim, lim); this.pos.z = clamp(this.pos.z, -lim, lim);
    if (this.state === 'knock') this.stunned -= dt;
    this.landT = Math.max(0, this.landT - dt);
    this.animate(dt, mag);
  }

  land(impact) {
    const p = this.pos;
    this.state = 'ground';
    this.vel.set(0, 0, 0);
    if (impact > 16) {
      const k = clamp((impact - 16) / 60, 0, 1) * (this.power > 1 ? 1.4 : 1);
      const r = 4 + k * 16;
      if (!this.onWaterAt()) {
        G.terrain.crater(p.x, p.z, r, 1 + k * 6, { rim: 0.35, burn: 0.15 * k });
        if (G.fx) { G.fx.decal(p, r * 1.6); G.fx.rocks(p, 6 + k * 24, 8 + k * 18); }
      } else if (G.fx) G.fx.splash(p, 30);
      if (G.fx) { G.fx.ring(p, r * 4, 0.9); G.fx.dust(p, r, 14 + k * 20); G.fx.shake(0.25 + k * 0.6, p); }
      if (G.powers) G.powers.areaDamage(p, r * 2.2, 60 + k * 400, 'push', 'landing');
      if (G.audio) G.audio.impact(p, 0.4 + k * 0.6);
      if (impact > 24 && this.state !== 'knock') { this.landT = 1.15; this.action = null; } // atterrissage de super-héros
      else this.anim.crouch = 0.6;
    } else this.anim.crouch = 0.25;
    if (this.stunned > 0) this.anim.knock = 1;
  }

  footsteps(dt, run) {
    const step = Math.floor(this.phase / Math.PI);
    if (step === this.lastStep || this.speed < 1.5) { this.lastStep = step; return; }
    this.lastStep = step;
    const side = step % 2 ? 1 : -1;
    const fp = V(Math.cos(this.heading) * 0.35 * side, 0, -Math.sin(this.heading) * 0.35 * side).add(this.pos);
    fp.y = this.groundAt(fp.x, fp.z);
    if (this.onWaterAt()) { if (G.fx) G.fx.splash(fp, 10); if (G.env) G.env.addRipple(fp.x, fp.z, 1.2); if (G.audio) G.audio.splash(fp); return; }
    if (G.fx) { G.fx.dust(fp, run ? 2.5 : 1.2, run ? 6 : 3); if (Math.random() < (run ? 0.8 : 0.35)) G.fx.decal(fp, run ? 3.2 : 2.0); }
    if (run) {
      G.terrain.crater(fp.x, fp.z, 2.2, 0.35, { rim: 0.4 });
      if (G.fx) { G.fx.rocks(fp, 2, 7); G.fx.shake(0.08, fp); if (step % 4 === 0) G.fx.ring(fp, 9, 0.5, 0xd8c8a8); }
      if (G.powers && step % 2 === 0) G.powers.areaDamage(fp, 7, 35, 'push', 'step');
    } else if (G.fx) G.fx.shake(0.03, fp);
    if (G.audio) G.audio.footstep(fp, run);
    if (G.people) G.people.notify(run ? 'quake' : 'step', fp.x, fp.z, run ? 25 : 8, run ? 18 : 4);
  }

  bodyCollisions(dt) {
    const Vg = G.villages, sp = Math.hypot(this.vel.x, this.vel.z);
    if (Vg && sp > 2) {
      for (const b of Vg.buildings) {
        if (b.state !== 'intact') continue;
        if (Math.hypot(b.x - this.pos.x, b.z - this.pos.z) > b.rad + 1.5) continue;
        if (this.pos.y > b.y + b.h + 1) continue;
        Vg.damageArea(this.pos.x, this.pos.z, 2, sp * 18 * dt * 10, this.pos.clone().addScaledVector(this.vel, -0.2));
        if (G.fx && Math.random() < dt * 10) G.fx.dust(this.pos.clone().add(V(0, 2, 0)), 3, 3);
      }
    }
    if (G.veg && sp > 9 && this.pos.y - G.terrain.heightAt(this.pos.x, this.pos.z) < 15) G.veg.damageArea(this.pos.x, this.pos.z, 3.5, sp * 2, 'push', this.pos.clone().addScaledVector(this.vel, -0.2));
  }

  updateTeleport(dt) {
    this.teleportT += dt;
    const k = this.teleportT / 0.7;
    this.root.visible = k < 0.35 || k > 0.65;
    if (k > 0.5 && !this.tpDone) {
      this.tpDone = true; this.pos.copy(this.tpTo);
      if (G.fx) { G.fx.sphere(this.pos.clone().add(V(0, 2.6, 0)), 10, 0.6, 0xfff0c0); G.fx.ring(this.pos, 24, 0.8); G.fx.flash(this.pos.clone().add(V(0, 4, 0)), 2500, 0.5, 0xfff0c0); }
    }
    if (k >= 1) { this.state = 'ground'; this.tpDone = false; this.root.visible = true; }
    this.animate(dt, 0);
  }

  theme() { const t = (this.model && this.model.theme) || { main: 0xffd27a }; return new THREE.Color(t.main); }

  // ---------- animation procédurale à ressorts ----------
  // pose cible de chaque os (angles d'Euler), puis interpolation amortie vers la cible
  pose(name, x, y, z, k) { const b = this.B[name]; if (!b) return; _e.set(x, y, z); _q.setFromEuler(_e); b.quaternion.slerp(_q, 1 - Math.exp(-k * this._dt)); }

  animate(dt, mag) {
    this._dt = Math.max(dt, 1e-4);
    const a = this.anim, B = this.B, t = G.time;
    const onGround = this.state === 'ground', flying = this.state === 'fly';
    const hs = Math.hypot(this.vel.x, this.vel.z), sp = this.vel.length();
    const aura = !!(this.aura && this.aura.active);
    a.walk = damp(a.walk, onGround ? clamp(this.speed / 7, 0, 1) : 0, 6, dt);
    a.run = damp(a.run, onGround ? clamp((this.speed - 8) / 10, 0, 1) : 0, 4, dt);
    a.fly = damp(a.fly, flying ? 1 : 0, 3, dt);
    a.boost = damp(a.boost, flying ? clamp((sp - 28) / 24, 0, 1) : 0, 2.5, dt);
    const cruise = flying ? clamp(hs / 20, 0, 1) : 0;
    const climb = flying ? clamp(this.vel.y / 15, -1, 1) : 0;
    const landK = this.landT > 0 ? Math.min(1, this.landT / 0.35) : 0; // 1 = au sol genou à terre, puis relevé
    a.land = damp(a.land, landK, 14, dt);
    a.stance = damp(a.stance, aura && onGround && this.speed < 1 ? 1 : 0, 4, dt);
    const targetPitch = flying ? 0.06 + cruise * 0.28 + a.boost * 1.05 - climb * 0.12 : (this.state === 'knock' ? -0.6 : (this.state === 'air' ? 0.1 : a.run * 0.12));
    a.pitch = damp(a.pitch, targetPitch, flying ? 2.2 : 5, dt);
    a.crouch = damp(a.crouch, 0, 3, dt);
    a.knock = damp(a.knock, this.state === 'knock' ? 1 : 0, 5, dt);
    if (onGround) this.phase += dt * (1.7 + this.speed * 0.4);
    const s = Math.sin(this.phase), c = Math.cos(this.phase);
    const W = a.walk * (1 - a.run), R = a.run;
    // ---- racine : position, balancement, inclinaison
    const bank = flying ? clamp(angleDelta(this.prevHeading ?? this.heading, this.heading) / Math.max(dt, 1e-3) * 0.3, -0.7, 0.7) : 0;
    this.prevHeading = this.heading;
    a.roll = damp(a.roll, -bank, 2.5, dt);
    const hoverBob = flying ? Math.sin(t * 1.4) * 0.35 * (1 - a.boost) : 0;
    this.root.position.copy(this.pos);
    this.root.position.y += (onGround ? -Math.abs(c) * (0.1 * W + 0.22 * R) + 0.05 * a.walk : hoverBob) - a.crouch * 0.9 - a.land * 1.55 - a.stance * 0.25;
    this.root.rotation.order = 'YXZ';
    this.root.rotation.set(a.pitch, this.heading, a.roll + (flying ? Math.sin(t * 0.9) * 0.04 * (1 - a.boost) : 0));
    // ---- bassin, colonne, tête
    const breathe = Math.sin(t * (aura ? 2.2 : 1.25)) * (aura ? 0.035 : 0.022);
    const shift = Math.sin(t * 0.45) * 0.04 * (1 - a.walk) * (1 - a.fly);
    this.pose('hips', 0, s * (0.1 * W + 0.16 * R), c * (0.05 * W) + shift, 16);
    this.pose('spine', 0.04 * W + 0.3 * R + a.crouch * 0.4 + a.land * 0.55 - a.knock * 0.4 + a.stance * 0.1, -s * (0.12 * W + 0.2 * R), -shift * 0.6, 14);
    this.pose('chest', breathe + 0.05 * R - a.stance * 0.12 - 0.12 * a.fly * (1 - a.boost), -s * (0.06 * W + 0.12 * R), 0, 12);
    a.idleT = onGround && this.speed < 0.5 && !this.action ? a.idleT + dt : 0;
    const look = a.idleT > 5 ? Math.sin((a.idleT - 5) * 0.35) * 0.55 : 0;
    this.pose('head', -a.pitch * 0.75 + a.knock * 0.3 - a.land * 0.35 * (this.landT > 0.6 ? 1 : 0) + a.stance * 0.06, look, 0, 10);
    // ---- jambes
    let tL, tR, sL, sR, fL = 0, fR = 0, zL = 0, zR = 0;
    // au sol : marche (genou fléchi pendant le balancier) et course (genoux hauts)
    tL = -s * (0.5 * W + 1.0 * R); tR = s * (0.5 * W + 1.0 * R);
    sL = (Math.max(0, c) * 0.9 + 0.08) * W + (Math.max(0, c) * 1.7 + 0.2) * R;
    sR = (Math.max(0, -c) * 0.9 + 0.08) * W + (Math.max(0, -c) * 1.7 + 0.2) * R;
    fL = -s * 0.2 * W + Math.max(0, c) * 0.3 * R; fR = s * 0.2 * W + Math.max(0, -c) * 0.3 * R;
    // posture de combat (aura) : jambes écartées, genoux souples
    zL += 0.17 * a.stance; zR -= 0.17 * a.stance; tL -= 0.25 * a.stance; tR -= 0.25 * a.stance; sL += 0.45 * a.stance; sR += 0.45 * a.stance;
    // vol divin : une jambe repliée vers l'avant, l'autre tendue, pieds pointés ; en comète, jambes tendues
    const hov = a.fly * (1 - a.boost);
    // lévitation : jambes presque tendues, genou droit à peine fléchi, pointes de pied vers le bas
    const sway = Math.sin(t * 1.3);
    tL = lerp(tL, 0.05 + sway * 0.03 + cruise * 0.2, hov); sL = lerp(sL, 0.12 + cruise * 0.25, hov);
    tR = lerp(tR, -0.16 - sway * 0.04 + cruise * 0.38, hov); sR = lerp(sR, 0.55 + sway * 0.05 + cruise * 0.1, hov);
    zL += 0.04 * hov; zR -= 0.04 * hov;
    fL = lerp(fL, 0.7, a.fly); fR = lerp(fR, 0.75, a.fly);
    tL = lerp(tL, 0.08, a.boost * a.fly); tR = lerp(tR, 0.02, a.boost * a.fly); sL = lerp(sL, 0.12, a.boost * a.fly); sR = lerp(sR, 0.22, a.boost * a.fly);
    // saut / chute
    if (this.state === 'air') { const up = clamp(this.vel.y / 20, -1, 1); tL = -0.7 * Math.max(0, up) + 0.15; tR = -0.4 * Math.max(0, up) - 0.25; sL = 1.2 * Math.max(0, up) + 0.3; sR = 0.9 * Math.max(0, up) + 0.5; }
    // atterrissage de super-héros : genou gauche au sol, jambe droite en avant
    tL = lerp(tL, -0.25, a.land); sL = lerp(sL, 2.35, a.land); tR = lerp(tR, -1.55, a.land); sR = lerp(sR, 1.7, a.land); fL = lerp(fL, 0.9, a.land);
    // accroupi (réception, frappe)
    tL -= a.crouch * 0.9; tR -= a.crouch * 0.9; sL += a.crouch * 1.4; sR += a.crouch * 1.4;
    this.pose('thighL', tL, 0, zL, 18); this.pose('thighR', tR, 0, zR, 18);
    this.pose('shinL', sL, 0, 0, 18); this.pose('shinR', sR, 0, 0, 18);
    this.pose('footL', fL, 0, 0, 14); this.pose('footR', fR, 0, 0, 14);
    // ---- bras
    let uL = [s * (0.35 * W + 0.95 * R) + 0.05, 0, 0.14], uR = [-s * (0.35 * W + 0.95 * R) + 0.05, 0, -0.14];
    let fAL = -0.25 - 0.15 * W - 1.45 * R, fAR = -0.25 - 0.15 * W - 1.45 * R;
    // posture de combat : poings serrés devant
    uL = [lerp(uL[0], -0.35, a.stance), 0, lerp(uL[2], 0.32, a.stance)]; uR = [lerp(uR[0], -0.35, a.stance), 0, lerp(uR[2], -0.32, a.stance)];
    fAL = lerp(fAL, -1.2, a.stance); fAR = lerp(fAR, -1.2, a.stance);
    // vol divin : bras détendus, écartés ; en croisière légèrement en arrière ; en comète : poing droit devant
    // lévitation : bras écartés et détendus, coudes souples ; en croisière ils passent en arrière
    uL = [lerp(uL[0], -0.1 + cruise * 0.5, hov), 0, lerp(uL[2], 0.5 + sway * 0.05 - cruise * 0.15, hov)];
    uR = [lerp(uR[0], -0.1 + cruise * 0.5, hov), 0, lerp(uR[2], -0.5 - sway * 0.05 + cruise * 0.15, hov)];
    fAL = lerp(fAL, -0.55 + cruise * 0.3, hov); fAR = lerp(fAR, -0.55 + cruise * 0.3, hov);
    const bst = a.boost * a.fly;
    uR = [lerp(uR[0], -3.0, bst), 0, lerp(uR[2], -0.08, bst)]; fAR = lerp(fAR, -0.02, bst);
    uL = [lerp(uL[0], 0.25, bst), 0, lerp(uL[2], 0.22, bst)]; fAL = lerp(fAL, -0.2, bst);
    // atterrissage de super-héros : poing droit au sol, bras gauche en arrière
    uR = [lerp(uR[0], -0.85, a.land), 0, lerp(uR[2], -0.1, a.land)]; fAR = lerp(fAR, -0.05, a.land);
    uL = [lerp(uL[0], 0.65, a.land), 0, lerp(uL[2], 0.55, a.land)]; fAL = lerp(fAL, -0.3, a.land);
    // saut : bras levés
    if (this.state === 'air') { uL = [-0.6, 0, 0.45]; uR = [-0.6, 0, -0.45]; }
    let armK = 12;
    if (this.action) {
      const ac = this.action; ac.t += dt;
      const k = clamp(ac.t / ac.dur, 0, 1), env = Math.sin(k * Math.PI);
      armK = 30;
      const strike = (u, f, sideZ) => { const p = k < 0.3 ? -k / 0.3 : (k < 0.55 ? 1 : 1 - (k - 0.55) / 0.45); return [[-1.55 * Math.max(0, p) + 0.55 * Math.max(0, -p), 0, sideZ], -0.05 - 1.2 * Math.max(0, -p)]; };
      switch (ac.kind) {
        case 'punch': case 'jabR': { const [u, f] = strike(uR, fAR, -0.1); uR = u; fAR = f; this.pose('chest', 0, -0.55 * env, 0, 30); break; }
        case 'jabL': { const [u, f] = strike(uL, fAL, 0.1); uL = u; fAL = f; this.pose('chest', 0, 0.55 * env, 0, 30); break; }
        case 'hook': { uR = [-1.4 * env, 0, -1.2 * env]; fAR = -1.4 * env; this.pose('chest', 0, -0.9 * env, 0, 30); break; }
        case 'uppercut': { uR = [-2.6 * env, 0, -0.2]; fAR = -1.8 + env; this.pose('spine', -0.3 * env, -0.3 * env, 0, 30); a.crouch = Math.max(a.crouch, (1 - k) * 0.4); break; }
        case 'cast': uR = [-1.5 * env - 0.1, 0, -0.2]; fAR = -0.1; uL = [-0.6 * env, 0, 0.7 * env]; break;
        case 'raise': uR = [-2.9 * env, 0, -0.3]; uL = [-2.9 * env, 0, 0.3]; fAR = fAL = -0.2; this.pose('head', -0.5 * env, 0, 0, 20); break;
        case 'slam': { const up = k < 0.45 ? k / 0.45 : 1 - (k - 0.45) / 0.55 * 2; uR = [-2.8 * Math.max(0, up) + 0.6 * Math.max(0, -up), 0, -0.2]; uL = [-2.8 * Math.max(0, up) + 0.6 * Math.max(0, -up), 0, 0.2]; a.crouch = Math.max(a.crouch, k > 0.45 ? 0.8 : 0); break; }
        case 'pose': uR = [-0.4, 0, -1.2 * env]; uL = [-0.4, 0, 1.2 * env]; break;
        case 'push': uR = [-1.5 * env, 0, 0.15]; uL = [-1.5 * env, 0, -0.15]; fAR = fAL = -0.05; break;
        case 'summon': uR = [-2.6 * env, 0, -0.5 * env]; uL = [-2.6 * env, 0, 0.5 * env]; fAR = fAL = -0.15; this.pose('head', -0.6 * env, 0, 0, 20); break;
        case 'charge': case 'transform': { const pw = Math.min(1, k * 2.5); const scream = ac.kind === 'transform' && k > 0.75; uR = [-0.2, 0, -(scream ? 1.4 : 0.6) * pw]; uL = [-0.2, 0, (scream ? 1.4 : 0.6) * pw]; fAR = fAL = scream ? -0.3 : -1.6 * pw; this.pose('head', (scream ? -0.6 : 0.35) * pw, 0, 0, 20); this.pose('chest', (scream ? -0.3 : 0.25) * pw, 0, 0, 20); a.crouch = Math.max(a.crouch, scream ? 0.05 : 0.35 * pw); break; }
        case 'dash': uR = [-3.0, 0, -0.1]; uL = [0.5, 0, 0.35]; fAR = 0; a.pitch = Math.max(a.pitch, 1.25); break;
        case 'dive': uR = [-3.0, 0, -0.15]; uL = [-3.0, 0, 0.15]; fAR = fAL = 0; a.pitch = 1.6; break;
        case 'takeoff': uR = [-2.8 * env, 0, -0.15]; uL = [0.2, 0, 0.3]; fAR = 0; a.crouch = Math.max(a.crouch, (1 - k) * 0.3); break;
        case 'jump': break;
      }
      if (ac.t >= ac.dur) this.action = null;
    }
    this.pose('upperL', uL[0], uL[1], uL[2], armK); this.pose('upperR', uR[0], uR[1], uR[2], armK);
    this.pose('foreL', fAL, 0, 0, armK); this.pose('foreR', fAR, 0, 0, armK);
    this.pose('handL', 0, 0, 0, 10); this.pose('handR', 0, 0, 0, 10);
    // ---- accessoires du modèle procédural
    if (B.clothF.rotation) { B.clothF.rotation.x = -Math.max(0, s) * 0.3 * a.walk - a.fly * 0.5 - a.run * 0.2; B.clothB.rotation.x = Math.max(0, -s) * 0.3 * a.walk + a.fly * 0.5 + a.run * 0.3; }
    if (B.haloRays.rotation) B.haloRays.rotation.z += dt * (aura ? 1.2 : 0.25);
    if (B.haloGlow.material) B.haloGlow.material.opacity = 0.75 + Math.sin(t * 2) * 0.15;
    if (B.core.rotation) B.core.rotation.y += dt * 1.5;
    // ailes
    const effort = flying ? clamp(0.3 + Math.abs(climb) * 0.6 + (1 - cruise) * 0.25 + a.boost * 0.3, 0, 1) : 0;
    a.spread = damp(a.spread, flying ? (a.boost > 0.6 ? 0.8 : 1) : (this.state === 'air' || this.state === 'knock' ? 0.85 : (a.run > 0.5 ? 0.42 : 0.28)), 3, dt);
    this.flapPhase += dt * (flying ? 2.2 + effort * 3 : 1.0);
    const flap = Math.sin(this.flapPhase) * (flying ? 0.25 + effort * 0.5 : (this.state === 'air' ? 0.3 : 0.05));
    if (flying && Math.sin(this.flapPhase) > 0.98 && !this._flapped) { this._flapped = true; if (G.audio) G.audio.wingFlap(effort); } else if (Math.sin(this.flapPhase) < 0.5) this._flapped = false;
    this.model.updateWings(a.spread, flap, t, 0.2 + effort * 0.6);
  }
}
