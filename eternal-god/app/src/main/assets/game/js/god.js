// Le Dieu jouable : déplacements (marche, course, saut, vol, vol stationnaire, marche sur l'eau),
// atterrissages destructeurs, pas qui fissurent le sol, projection sans jamais mourir,
// animation procédurale du corps et des ailes.
import * as THREE from 'three';
import { G, WORLD, clamp, lerp, damp, dampAngle, angleDelta, rand, emit } from './core.js';
import { buildGodModel } from './godmodel.js';

const UP = new THREE.Vector3(0, 1, 0);

export class God {
  constructor(scene) {
    this.model = buildGodModel();
    this.root = this.model.root;
    this.B = this.model.bones;
    scene.add(this.root);
    this.pos = new THREE.Vector3(0, 60, 40);
    this.vel = new THREE.Vector3();
    this.heading = Math.PI;
    this.state = 'fly';            // ground | air | fly | knock | teleport
    this.hover = true;
    this.speed = 0;
    this.phase = 0;
    this.lastStep = 0;
    this.anim = { walk: 0, run: 0, fly: 1, flap: 0, spread: 1, crouch: 0, punch: 0, cast: 0, slam: 0, pitch: 0, roll: 0, knock: 0, look: 0, idleT: 0 };
    this.action = null;            // { kind, t, dur }
    this.flapPhase = 0;
    this.airTime = 0;
    this.height = 5.2;
    this.stunned = 0;
    this.destroying = 0;
    this.teleportT = 0;
  }

  groundAt(x, z) {
    const T = G.terrain;
    let g = T.heightAt(x, z);
    if (g < WORLD.water - 1.2) g = WORLD.water; // le dieu marche sur l'eau profonde
    const isl = G.env ? G.env.islandGround(x, z, this.pos.y) : -Infinity;
    return Math.max(g, isl);
  }

  onWater() { return G.terrain.heightAt(this.pos.x, this.pos.z) < WORLD.water - 1.2 && this.state === 'ground'; }

  // ---------- commandes ----------
  jump() {
    if (this.state === 'ground') { this.vel.y = 24; this.state = 'air'; this.airTime = 0; }
    else if (this.state === 'air') this.startFly();
  }
  toggleFly() { if (this.state === 'fly') this.stopFly(); else this.startFly(); }
  startFly() { this.state = 'fly'; this.vel.y = Math.max(this.vel.y, 8); emit('godFly', true); if (G.audio) G.audio.wingFlap(1); }
  stopFly() { this.state = 'air'; this.airTime = 0; emit('godFly', false); }
  playAction(kind, dur) { this.action = { kind, t: 0, dur }; }
  knock(dir, power, stun = 0.8) {
    this.state = 'knock';
    this.vel.copy(dir).setY(0).normalize().multiplyScalar(power).setY(power * 0.45 + 6);
    this.stunned = stun;
    this.anim.knock = 1;
    if (G.fx) G.fx.shake(0.5, this.pos);
    emit('godKnocked', { power });
  }
  teleport(x, z) {
    if (this.state === 'teleport') return;
    this.state = 'teleport'; this.teleportT = 0; this.tpFrom = this.pos.clone();
    this.tpTo = new THREE.Vector3(x, this.groundAt(x, z), z);
    if (G.fx) { G.fx.sphere(this.pos.clone().add(new THREE.Vector3(0, 2.6, 0)), 8, 0.5, 0xfff0c0); G.fx.ring(this.pos, 18, 0.6); }
    if (G.audio) G.audio.teleport();
  }

  // ---------- simulation ----------
  update(dt, input) {
    const T = G.terrain, cam = G.cam;
    const a = this.anim;
    if (this.state === 'teleport') return this.updateTeleport(dt);
    // direction voulue relative à la caméra
    const mx = input.moveX, my = input.moveY, mag = Math.min(1, Math.hypot(mx, my));
    const yaw = cam.yaw;
    const fwd = new THREE.Vector3(-Math.sin(yaw), 0, -Math.cos(yaw));
    const right = new THREE.Vector3(-fwd.z, 0, fwd.x);
    const wish = fwd.clone().multiplyScalar(my).addScaledVector(right, mx);
    if (wish.lengthSq() > 1) wish.normalize();
    const g = this.groundAt(this.pos.x, this.pos.z);

    if (this.state === 'ground') {
      const run = mag > 0.82;
      const target = mag < 0.08 ? 0 : (run ? 21 : 4 + mag * 7);
      this.speed = damp(this.speed, target, run ? 2.5 : 5, dt);
      if (mag > 0.08) this.heading = dampAngle(this.heading, Math.atan2(wish.x, wish.z), 7, dt);
      this.vel.x = Math.sin(this.heading) * this.speed; this.vel.z = Math.cos(this.heading) * this.speed; this.vel.y = 0;
      this.pos.x += this.vel.x * dt; this.pos.z += this.vel.z * dt;
      // pente trop raide : il glisse le long, mais rien ne l'arrête vraiment (c'est un dieu)
      const ng = this.groundAt(this.pos.x, this.pos.z);
      if (ng < this.pos.y - 3) { this.state = 'air'; this.airTime = 0; this.vel.y = 0; } // falaise : il tombe
      else this.pos.y = ng; // aucune pente ne l'arrête : il gravit tout
      this.footsteps(dt, run);
      this.bodyCollisions(dt);
      if (this.onWater() && Math.random() < dt * (2 + this.speed * 0.4) && G.env) { G.env.addRipple(this.pos.x, this.pos.z, 0.6 + this.speed * 0.05); }
    } else if (this.state === 'air' || this.state === 'knock') {
      this.airTime += dt;
      this.vel.y -= (this.state === 'knock' ? 30 : 42) * dt;
      if (this.state === 'air' && mag > 0.08) { this.vel.x = lerp(this.vel.x, wish.x * 16, dt * 2); this.vel.z = lerp(this.vel.z, wish.z * 16, dt * 2); this.heading = dampAngle(this.heading, Math.atan2(wish.x, wish.z), 4, dt); }
      this.pos.addScaledVector(this.vel, dt);
      this.bodyCollisions(dt);
      if (this.pos.y <= g) {
        const impact = -this.vel.y;
        this.pos.y = g;
        this.land(impact);
      }
    } else if (this.state === 'fly') {
      const pitch = cam.pitch;
      const boost = mag > 0.9 ? 1.9 : 1;
      const speedMax = 38 * boost;
      // en vol, avancer suit le regard (monter en regardant vers le haut)
      const look = new THREE.Vector3(-Math.sin(yaw) * Math.cos(pitch), Math.sin(-pitch) * 0.9, -Math.cos(yaw) * Math.cos(pitch));
      const desire = look.multiplyScalar(my).addScaledVector(right, mx).multiplyScalar(speedMax);
      if (input.up) desire.y += 22;
      if (input.down) desire.y -= 26;
      this.vel.lerp(desire, 1 - Math.exp(-2.2 * dt));
      this.pos.addScaledVector(this.vel, dt);
      const hs = Math.hypot(this.vel.x, this.vel.z);
      if (hs > 2) this.heading = dampAngle(this.heading, Math.atan2(this.vel.x, this.vel.z), 3, dt);
      this.pos.y = Math.min(this.pos.y, 1100);
      if (this.pos.y < g + 0.5) {
        this.pos.y = g + 0.5;
        if (input.down || this.vel.y < -15) { const impact = -this.vel.y; this.state = 'ground'; this.land(impact); }
        else this.vel.y = Math.max(this.vel.y, 0);
      }
      // souffle divin : rase les arbres et l'herbe en vol bas
      const alt = this.pos.y - g, sp = this.vel.length();
      G.uniforms.gust.value.set(this.pos.x, this.pos.y, this.pos.z, alt < 45 && sp > 8 ? clamp((45 - alt) / 45, 0, 1) * Math.min(1, sp / 40) * 6 : 0);
      if (alt < 12 && sp > 30 && G.veg && Math.random() < dt * 4) G.veg.damageArea(this.pos.x, this.pos.z, 8, sp * 0.8, 'push', this.pos);
      if (alt < 6 && sp > 25 && G.fx && Math.random() < dt * 8) G.fx.dust(new THREE.Vector3(this.pos.x, g, this.pos.z), 4, 2);
    }
    if (this.state !== 'fly') G.uniforms.gust.value.w = damp(G.uniforms.gust.value.w, 0, 3, dt);
    // l'herbe se couche sous ses pieds
    G.uniforms.press.value.set(this.pos.x, this.pos.z, this.state === 'ground' ? 3.5 : 0, this.state === 'ground' ? 1 : 0);
    // bords du monde
    const lim = WORLD.half - 25;
    this.pos.x = clamp(this.pos.x, -lim, lim); this.pos.z = clamp(this.pos.z, -lim, lim);
    if (this.state === 'knock') { this.stunned -= dt; }
    this.animate(dt, mag);
  }

  land(impact) {
    const p = this.pos;
    this.state = 'ground';
    this.vel.set(0, 0, 0);
    if (impact > 16) {
      const k = clamp((impact - 16) / 60, 0, 1);
      const r = 4 + k * 14;
      if (!this.onWaterAt()) {
        G.terrain.crater(p.x, p.z, r, 1 + k * 6, { rim: 0.35, burn: 0.15 * k });
        if (G.fx) { G.fx.decal(p, r * 1.6); G.fx.rocks(p, 6 + k * 20, 8 + k * 16); }
      } else if (G.fx) G.fx.splash(p, 30);
      if (G.fx) { G.fx.ring(p, r * 4, 0.9); G.fx.dust(p, r, 14 + k * 20); G.fx.shake(0.25 + k * 0.6, p); }
      if (G.powers) G.powers.areaDamage(p, r * 2.2, 40 + k * 260, 'push', 'landing');
      if (G.audio) G.audio.impact(p, 0.4 + k * 0.6);
      this.anim.crouch = 0.6 + k * 0.4;
    } else this.anim.crouch = 0.25;
    if (this.stunned > 0) this.anim.knock = 1;
  }
  onWaterAt() { return G.terrain.heightAt(this.pos.x, this.pos.z) < WORLD.water - 1.2; }

  footsteps(dt, run) {
    const step = Math.floor(this.phase / Math.PI);
    if (step === this.lastStep || this.speed < 1.5) { this.lastStep = step; return; }
    this.lastStep = step;
    const side = step % 2 ? 1 : -1;
    const fp = new THREE.Vector3(Math.cos(this.heading) * 0.35 * side, 0, -Math.sin(this.heading) * 0.35 * side).add(this.pos);
    fp.y = this.groundAt(fp.x, fp.z);
    const water = this.onWaterAt();
    if (water) { if (G.fx) G.fx.splash(fp, 10); if (G.env) G.env.addRipple(fp.x, fp.z, 1.2); if (G.audio) G.audio.splash(fp); return; }
    if (G.fx) { G.fx.dust(fp, run ? 2.5 : 1.2, run ? 6 : 3); if (Math.random() < (run ? 0.8 : 0.35)) G.fx.decal(fp, run ? 3.2 : 2.0); }
    if (run) {
      G.terrain.crater(fp.x, fp.z, 2.2, 0.35, { rim: 0.4 });
      if (G.fx) { G.fx.rocks(fp, 2, 7); G.fx.shake(0.08, fp); if (step % 4 === 0) G.fx.ring(fp, 9, 0.5, 0xd8c8a8); }
      if (G.powers && step % 2 === 0) G.powers.areaDamage(fp, 7, 35, 'push', 'step');
    } else if (G.fx) G.fx.shake(0.03, fp);
    if (G.audio) G.audio.footstep(fp, run);
    if (G.people) G.people.notify(run ? 'quake' : 'step', fp.x, fp.z, run ? 25 : 8, run ? 18 : 4);
  }

  // le corps divin traverse et renverse ce qu'il touche
  bodyCollisions(dt) {
    const V = G.villages, sp = Math.hypot(this.vel.x, this.vel.z);
    if (V && sp > 2) {
      for (const b of V.buildings) {
        if (b.state !== 'intact') continue;
        if (Math.hypot(b.x - this.pos.x, b.z - this.pos.z) > b.rad + 1.5) continue;
        if (this.pos.y > b.y + b.h + 1) continue;
        V.damageArea(this.pos.x, this.pos.z, 2, sp * 18 * dt * 10, this.pos.clone().addScaledVector(this.vel, -0.2));
        if (G.fx && Math.random() < dt * 10) G.fx.dust(this.pos.clone().add(new THREE.Vector3(0, 2, 0)), 3, 3);
      }
    }
    if (G.veg && sp > 9 && this.pos.y - G.terrain.heightAt(this.pos.x, this.pos.z) < 15) G.veg.damageArea(this.pos.x, this.pos.z, 3.5, sp * 2, 'push', this.pos.clone().addScaledVector(this.vel, -0.2));
  }

  updateTeleport(dt) {
    this.teleportT += dt;
    const k = this.teleportT / 0.7;
    this.root.visible = k < 0.35 || k > 0.65;
    if (k > 0.5 && !this.tpDone) {
      this.tpDone = true;
      this.pos.copy(this.tpTo);
      if (G.fx) { G.fx.sphere(this.pos.clone().add(new THREE.Vector3(0, 2.6, 0)), 10, 0.6, 0xfff0c0); G.fx.ring(this.pos, 24, 0.8); G.fx.flash(this.pos.clone().add(new THREE.Vector3(0, 4, 0)), 2500, 0.5, 0xfff0c0); }
    }
    if (k >= 1) { this.state = 'ground'; this.tpDone = false; this.root.visible = true; }
    this.animate(dt, 0);
  }

  // ---------- animation procédurale ----------
  animate(dt, mag) {
    const a = this.anim, B = this.B, t = G.time;
    const onGround = this.state === 'ground';
    const flying = this.state === 'fly';
    const hs = Math.hypot(this.vel.x, this.vel.z);
    a.walk = damp(a.walk, onGround ? clamp(this.speed / 7, 0, 1) : 0, 6, dt);
    a.run = damp(a.run, onGround ? clamp((this.speed - 8) / 10, 0, 1) : 0, 4, dt);
    a.fly = damp(a.fly, flying ? 1 : 0, 4, dt);
    const fwdFly = flying ? clamp(hs / 45, 0, 1) : 0;
    a.pitch = damp(a.pitch, flying ? fwdFly * 1.15 - clamp(this.vel.y / 40, -0.3, 0.4) : (this.state === 'knock' ? -0.6 : 0), 3, dt);
    a.crouch = damp(a.crouch, 0, 3, dt);
    a.knock = damp(a.knock, this.state === 'knock' ? 1 : 0, 5, dt);
    // phase de marche : foulée lourde et majestueuse
    if (onGround) this.phase += dt * (1.6 + this.speed * 0.42);
    const s = Math.sin(this.phase), c = Math.cos(this.phase);
    const walkAmp = a.walk * (0.45 + a.run * 0.4);
    // racine
    this.root.position.copy(this.pos);
    this.root.position.y += (onGround ? -Math.abs(c) * 0.12 * a.walk + 0.06 * a.walk : Math.sin(t * 1.6) * 0.25 * a.fly) - a.crouch * 0.9;
    const bank = flying ? clamp(angleDelta(this.prevHeading ?? this.heading, this.heading) / Math.max(dt, 1e-3) * 0.25, -0.6, 0.6) : 0;
    this.prevHeading = this.heading;
    a.roll = damp(a.roll, -bank, 3, dt);
    this.root.rotation.set(0, 0, 0);
    this.root.rotation.order = 'YXZ';
    this.root.rotation.y = this.heading;
    this.root.rotation.x = a.pitch;
    this.root.rotation.z = a.roll;
    // bassin et colonne
    B.hips.rotation.set(0, s * 0.08 * a.walk, c * 0.04 * a.walk);
    B.spine.rotation.set(0.06 * a.run + a.crouch * 0.4 - a.knock * 0.4, -s * 0.12 * a.walk, 0);
    const breathe = Math.sin(t * 1.3) * 0.02;
    B.chest.rotation.set(breathe + a.run * 0.08, -s * 0.06 * a.walk, 0);
    // tête : regard vers l'horizon au repos
    a.idleT = this.speed < 0.5 && onGround ? a.idleT + dt : 0;
    const lookAround = a.idleT > 6 ? Math.sin((a.idleT - 6) * 0.4) * 0.5 : 0;
    B.head.rotation.set(-a.pitch * 0.6 + a.knock * 0.3, lookAround, 0);
    // jambes
    const legSwing = walkAmp;
    B.thighL.rotation.set(-s * legSwing - a.crouch * 0.9 + a.fly * (0.15 + fwdFly * 0.25), 0, 0);
    B.thighR.rotation.set(s * legSwing - a.crouch * 0.9 + a.fly * (0.05 + fwdFly * 0.3), 0, 0);
    B.shinL.rotation.set(Math.max(0, c) * legSwing * 1.4 + a.crouch * 1.4 + a.fly * (0.35 + Math.sin(t * 1.6) * 0.08), 0, 0);
    B.shinR.rotation.set(Math.max(0, -c) * legSwing * 1.4 + a.crouch * 1.4 + a.fly * (0.5 + Math.sin(t * 1.6 + 1) * 0.08), 0, 0);
    B.footL.rotation.set(a.fly * 0.5, 0, 0); B.footR.rotation.set(a.fly * 0.55, 0, 0);
    // bras : balancement opposé, puis actions (poing, invocation, frappe au sol)
    let uL = new THREE.Euler(s * walkAmp * 0.8 + a.fly * (0.3 + fwdFly * 0.5), 0, 0.18 + a.fly * 0.25);
    let uR = new THREE.Euler(-s * walkAmp * 0.8 + a.fly * (0.3 + fwdFly * 0.5), 0, -0.18 - a.fly * 0.25);
    let fL = -0.25 - a.run * 0.6, fR = -0.25 - a.run * 0.6;
    if (this.action) {
      const ac = this.action; ac.t += dt;
      const k = clamp(ac.t / ac.dur, 0, 1);
      const env = Math.sin(k * Math.PI);
      if (ac.kind === 'punch') { const p = k < 0.35 ? -k / 0.35 * 0.6 : (k < 0.55 ? 1 : 1 - (k - 0.55) / 0.45); uR.set(-1.55 * Math.max(0, p) + 0.5 * Math.min(0, p), 0, -0.1); fR = -0.05; B.chest.rotation.y += -0.5 * env; }
      else if (ac.kind === 'cast') { uR.set(-1.5 * env - 0.1, 0, -0.2); fR = -0.1; uL.set(-0.6 * env, 0, 0.7 * env); }
      else if (ac.kind === 'raise') { uR.set(-2.9 * env, 0, -0.3); uL.set(-2.9 * env, 0, 0.3); fR = fL = -0.2; B.head.rotation.x -= 0.5 * env; }
      else if (ac.kind === 'slam') { const up = k < 0.45 ? k / 0.45 : 1 - (k - 0.45) / 0.55 * 2; uR.set(-2.8 * Math.max(0, up) + 0.6 * Math.max(0, -up), 0, -0.2); uL.set(-2.8 * Math.max(0, up) + 0.6 * Math.max(0, -up), 0, 0.2); a.crouch = Math.max(a.crouch, k > 0.45 ? 0.8 : 0); }
      else if (ac.kind === 'pose') { uR.set(-0.4, 0, -1.2 * env); uL.set(-0.4, 0, 1.2 * env); }
      if (ac.t >= ac.dur) this.action = null;
    }
    B.upperL.rotation.copy(uL); B.upperR.rotation.copy(uR);
    B.foreL.rotation.set(fL, 0, 0); B.foreR.rotation.set(fR, 0, 0);
    B.handL.rotation.set(0, 0, 0); B.handR.rotation.set(0, 0, 0);
    // tissu
    B.clothF.rotation.x = -Math.max(0, s) * 0.3 * a.walk - a.fly * fwdFly * 0.9 - a.run * 0.2;
    B.clothB.rotation.x = Math.max(0, -s) * 0.3 * a.walk + a.fly * fwdFly * 0.9 + a.run * 0.3;
    // auréole
    B.haloRays.rotation.z += dt * 0.25;
    B.haloGlow.material.opacity = 0.75 + Math.sin(t * 2) * 0.15;
    B.core.rotation.y += dt * 1.5;
    // ailes : repliées au sol, déployées en vol, battements selon l'effort
    const climb = flying ? clamp(this.vel.y / 20, -1, 1) : 0;
    const effort = flying ? clamp(0.35 + Math.abs(climb) * 0.6 + (1 - fwdFly) * 0.4 + (this.vel.length() > 50 ? 0.3 : 0), 0, 1) : 0;
    a.spread = damp(a.spread, flying ? (fwdFly > 0.75 ? 0.82 : 1) : (this.state === 'air' || this.state === 'knock' ? 0.85 : (a.run > 0.5 ? 0.42 : 0.28)), 3, dt);
    this.flapPhase += dt * (flying ? 3.2 + effort * 3.5 : 1.2);
    const flapAmp = flying ? 0.35 + effort * 0.55 : (this.state === 'air' ? 0.3 : 0.05);
    const flap = Math.sin(this.flapPhase) * flapAmp;
    if (flying && Math.sin(this.flapPhase) > 0.98 && !this._flapped) { this._flapped = true; if (G.audio) G.audio.wingFlap(effort); } else if (Math.sin(this.flapPhase) < 0.5) this._flapped = false;
    this.model.updateWings(a.spread, flap, t, 0.2 + effort * 0.6);
  }
}
