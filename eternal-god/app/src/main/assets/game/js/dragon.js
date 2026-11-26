// Le Dragon des cendres : immense dragon ailé (modèle animé Quaternius, CC0) qui tournoie autour du
// dieu, crache des salves de boules de feu, pique sur lui, encaisse les coups (projeté, chancelant)
// et s'écrase au sol à sa mort. Boules de feu pré-créées (aucune allocation de maillage en jeu).
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { MeshoptDecoder } from 'three/addons/libs/meshopt_decoder.module.js';
import { G, WORLD, clamp, lerp, damp, dampAngle, rand, emit } from './core.js';

const HP = 3600, SPAN = 30;
const V = (x = 0, y = 0, z = 0) => new THREE.Vector3(x, y, z);

export async function loadDragon() {
  const gl = new GLTFLoader(); gl.setMeshoptDecoder(MeshoptDecoder);
  return gl.loadAsync('models/chars/dragon.glb');
}

export class Dragon {
  constructor(scene, gltf, glowTex) {
    this.scene = scene; this.tag = 'dragon'; this.name = 'Dragon des cendres';
    this.root = new THREE.Group();
    const m = gltf.scene;
    m.updateMatrixWorld(true);
    const bb = new THREE.Box3().setFromObject(m), size = bb.getSize(V());
    const s = SPAN / Math.max(size.x, size.z, 1e-3);
    m.scale.setScalar(s);
    m.position.y = -bb.getCenter(V()).y * s;
    m.traverse((o) => { if (o.isMesh) { o.castShadow = true; o.frustumCulled = false; if (o.material) { o.material = o.material.clone(); o.material.emissive = new THREE.Color(0x3a0800); o.material.emissiveIntensity = 0.6; } } });
    this.mats = []; m.traverse((o) => { if (o.isMesh) this.mats.push(o.material); });
    this.model = m; this.root.add(m);
    this.mixer = new THREE.AnimationMixer(m);
    const clip = (re) => gltf.animations.find((a) => re.test(a.name.split('|').pop()));
    this.acts = {};
    for (const [k, re] of [['idle', /^Flying_Idle$/], ['fast', /^Fast_Flying$/], ['bite', /^Headbutt$/], ['hit', /^HitReact$/], ['death', /^Death$/], ['punch', /^Punch$/]]) {
      const c = clip(re); if (c) this.acts[k] = this.mixer.clipAction(c);
    }
    for (const k of ['bite', 'hit', 'death', 'punch']) if (this.acts[k]) { this.acts[k].setLoop(THREE.LoopOnce); this.acts[k].clampWhenFinished = true; }
    this.cur = null;
    this.root.visible = false;
    scene.add(this.root);
    // boules de feu
    this.balls = [];
    for (let i = 0; i < 10; i++) {
      const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: 0xff7a20, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
      sp.scale.setScalar(7); sp.visible = false; scene.add(sp);
      this.balls.push({ sp, vel: V(), t: 0, on: false });
    }
    this.alive = false; this.state = 'dormant';
    this.pos = V(); this.vel = V(); this.heading = 0; this.bankR = 0;
    this.radius = 11; this.respawnT = 150; this.t = 0; this.orbitA = 0;
  }

  center() { return this.pos.clone(); }

  play(k, fade = 0.3) {
    const a = this.acts[k]; if (!a || this.cur === a) return;
    a.reset().play(); if (this.cur) a.crossFadeFrom(this.cur, fade, false);
    this.cur = a;
  }

  spawn(near = false) {
    const god = G.god.pos, a = Math.random() * 6.28, d = near ? rand(160, 220) : rand(380, 500);
    this.pos.set(clamp(god.x + Math.cos(a) * d, -WORLD.half + 60, WORLD.half - 60), Math.max(god.y, G.terrain.heightAt(god.x, god.z)) + 90, clamp(god.z + Math.sin(a) * d, -WORLD.half + 60, WORLD.half - 60));
    this.vel.set(0, 0, 0);
    this.hp = HP; this.alive = true; this.state = 'approach'; this.t = 0; this.fired = 0;
    this.root.visible = true; this.root.rotation.set(0, 0, 0);
    this.play('fast', 0.01);
    emit('journal', { icon: '🐉', text: 'Un Dragon des cendres déchire les nuages !' });
    emit('monster', { state: 'spawn', name: this.name });
    if (G.audio) G.audio.roar(this.pos, 1);
  }

  damage(amount, from) {
    if (!this.alive) return;
    this.hp -= amount; this.hitT = 0.2;
    if (G.fx) G.fx.sparks(this.pos, 18, [1, 0.6, 0.2], 18);
    if (amount > 500 && this.state !== 'launched') { this.state = 'hit'; this.t = 0; this.play('hit', 0.08); }
    emit('monster', { state: 'hit', hp: this.hp / HP, name: this.name });
    if (this.hp <= 0) this.die();
  }

  launch(dir, power) {
    if (!this.alive) return;
    this.state = 'launched'; this.t = 0;
    this.vel.copy(dir).normalize().multiplyScalar(power).add(V(0, power * 0.35, 0));
    this.play('hit', 0.05);
  }

  die() {
    this.alive = false; this.state = 'dying'; this.t = 0; this.crashed = false;
    this.play('death', 0.15);
    emit('monster', { state: 'dead', name: this.name });
    emit('journal', { icon: '⚔', text: 'Le Dragon des cendres s\'abat du ciel, vaincu.' });
    if (G.people) G.people.notify('bless', this.pos.x, this.pos.z, 260, 0);
    this.respawnT = rand(300, 480);
  }

  fire() {
    const b = this.balls.find((x) => !x.on); if (!b) return;
    const god = G.god;
    const mouth = this.pos.clone().add(V(Math.sin(this.heading) * 9, 1, Math.cos(this.heading) * 9));
    const to = god.pos.clone().add(V(0, 2.5, 0)).addScaledVector(god.vel, 0.6);
    b.sp.position.copy(mouth); b.vel.copy(to.sub(mouth).normalize().multiplyScalar(70)); b.t = 0; b.on = true; b.sp.visible = true;
    if (G.audio) G.audio.whoosh(mouth);
    if (G.fx) G.fx.fire(mouth, 8);
  }

  explode(p) {
    const T = G.terrain, g = T.heightAt(p.x, p.z);
    if (p.y - g < 4) { T.crater(p.x, p.z, 7, 2.5, { rim: 0.4, burn: 1, heat: 0.7 }); G.fx.decal(p.clone().setY(g), 12); if (G.veg) G.veg.damageArea(p.x, p.z, 12, 2, 'burn'); }
    G.fx.sphere(p, 10, 0.5, 0xff9a40); G.fx.flash(p, 3000, 0.3, 0xff7a30); G.fx.shake(0.3, p);
    for (let i = 0; i < 8; i++) G.fx.fire(p.clone().add(V(rand(-4, 4), rand(0, 3), rand(-4, 4))), 6);
    if (G.powers) G.powers.areaDamage(p, 12, 320, 'burn', 'dragon');
    const god = G.god;
    if (god.pos.distanceTo(p) < 9 && god.state !== 'teleport') god.knock(god.pos.clone().sub(p), 26, 0.6);
    if (G.audio) G.audio.explosion(p, 0.5);
  }

  update(dt) {
    const god = G.god;
    // boules de feu
    for (const b of this.balls) {
      if (!b.on) continue;
      b.t += dt; b.sp.position.addScaledVector(b.vel, dt);
      const p = b.sp.position;
      if (G.fx && Math.random() < 0.7) G.fx.fire(p, 5);
      const hit = p.distanceTo(god.pos.clone().add(V(0, 2.5, 0))) < 4 || p.y < G.terrain.heightAt(p.x, p.z) || b.t > 5;
      if (hit) { b.on = false; b.sp.visible = false; this.explode(p.clone()); }
    }
    if (!this.alive && this.state !== 'dying') {
      this.respawnT -= dt;
      if (this.respawnT <= 0 && G.started) this.spawn();
      return;
    }
    this.t += dt;
    this.mixer.update(dt);
    const toGod = god.pos.clone().add(V(0, 2.5, 0)).sub(this.pos), dGod = toGod.length();
    const ground = G.terrain.heightAt(this.pos.x, this.pos.z);
    let want = null, speed = 0, face = null;
    switch (this.state) {
      case 'approach':
        want = god.pos.clone().add(V(0, 35, 0)); speed = 55;
        if (dGod < 110) { this.state = 'circle'; this.t = 0; this.play('idle'); this.orbitA = Math.atan2(this.pos.z - god.pos.z, this.pos.x - god.pos.x); }
        break;
      case 'circle':
        this.orbitA += dt * 0.45;
        want = god.pos.clone().add(V(Math.cos(this.orbitA) * 75, 30 + Math.sin(this.t * 0.7) * 8, Math.sin(this.orbitA) * 75)); speed = 38;
        if (this.t > 4.5) { this.t = 0; if (Math.random() < 0.6) { this.state = 'volley'; this.fired = 0; this.play('punch', 0.2); } else { this.state = 'dive'; this.play('fast', 0.2); this.diveTo = god.pos.clone().add(V(0, 2, 0)); if (G.audio) G.audio.roar(this.pos, 0.6); } }
        break;
      case 'volley':
        face = toGod; want = this.pos.clone(); speed = 0;
        if (this.t > 0.5 + this.fired * 0.38 && this.fired < 5) { this.fired++; this.fire(); if (this.acts.punch) { this.acts.punch.reset().play(); } }
        if (this.t > 2.8) { this.state = 'circle'; this.t = 0; this.play('idle'); }
        break;
      case 'dive': {
        want = this.diveTo; speed = 85;
        const d = this.pos.distanceTo(this.diveTo);
        if (d < 12 || this.t > 3) {
          if (god.pos.distanceTo(this.pos) < 16 && god.state !== 'teleport') { god.knock(toGod.clone().negate().multiplyScalar(-1), 34, 0.9); if (G.fx) G.fx.shake(0.5, god.pos); }
          this.play('bite', 0.1);
          this.state = 'climb'; this.t = 0;
        }
        break;
      }
      case 'climb':
        want = this.pos.clone().add(V(Math.sin(this.heading) * 60, 50, Math.cos(this.heading) * 60)); speed = 50;
        if (this.t > 2) { this.state = 'circle'; this.t = 0; this.play('idle'); }
        break;
      case 'hit':
        speed = 0; want = null; this.vel.multiplyScalar(Math.exp(-3 * dt));
        if (this.t > 0.7) { this.state = 'circle'; this.t = 0; this.play('idle'); }
        break;
      case 'launched':
        this.vel.y -= 25 * dt; this.vel.multiplyScalar(Math.exp(-0.6 * dt));
        this.root.rotation.x -= dt * 6;
        if (this.pos.y < ground + 4) {
          this.pos.y = ground + 4; this.vel.set(0, 12, 0);
          G.terrain.crater(this.pos.x, this.pos.z, 12, 3, { rim: 0.5 });
          if (G.fx) { G.fx.dust(this.pos.clone().setY(ground), 14, 26); G.fx.rocks(this.pos.clone().setY(ground), 18, 16); G.fx.shake(0.6, this.pos); }
          if (G.powers) G.powers.areaDamage(this.pos, 18, 300, 'push', 'dragon');
          this.damage(250, this.pos);
          if (this.state === 'launched') { this.state = 'hit'; this.t = 0; }
        }
        if (this.t > 2.2 && this.state === 'launched') { this.state = 'circle'; this.t = 0; this.play('idle'); }
        break;
      case 'dying':
        this.vel.y -= 30 * dt; this.vel.x *= Math.exp(-dt); this.vel.z *= Math.exp(-dt);
        if (this.pos.y < ground + 3 && !this.crashed) {
          this.crashed = true; this.pos.y = ground + 3; this.vel.set(0, 0, 0);
          G.terrain.crater(this.pos.x, this.pos.z, 20, 5, { rim: 0.6, burn: 0.6, heat: 0.6 });
          if (G.fx) { G.fx.dust(this.pos.clone().setY(ground), 26, 50); G.fx.rocks(this.pos.clone().setY(ground), 40, 22); G.fx.shake(1, this.pos); G.fx.ring(this.pos.clone().setY(ground), 120, 1.4); }
          if (G.audio) G.audio.impact(this.pos, 1);
        }
        if (this.crashed) this.pos.y = Math.max(ground + 3 - (this.t - 3) * 0.6, ground - 6);
        for (const m of this.mats) m.emissiveIntensity = Math.max(0, 0.6 - this.t * 0.1);
        if (this.t > 9) { this.root.visible = false; this.state = 'dormant'; for (const m of this.mats) m.emissiveIntensity = 0.6; }
        break;
    }
    if (want && this.state !== 'launched' && this.state !== 'dying') {
      const d = want.clone().sub(this.pos), L = d.length();
      const des = L > 1 ? d.multiplyScalar(Math.min(speed, L * 2) / L) : d.set(0, 0, 0);
      this.vel.lerp(des, 1 - Math.exp(-1.8 * dt));
    }
    this.pos.addScaledVector(this.vel, dt);
    if (this.state !== 'dying' && this.state !== 'launched') this.pos.y = Math.max(this.pos.y, ground + 12);
    const lim = WORLD.half - 30; this.pos.x = clamp(this.pos.x, -lim, lim); this.pos.z = clamp(this.pos.z, -lim, lim);
    // orientation : face au déplacement (ou à la cible), roulis dans les virages
    const hs = Math.hypot(this.vel.x, this.vel.z);
    const prev = this.heading;
    if (face) this.heading = dampAngle(this.heading, Math.atan2(face.x, face.z), 3, dt);
    else if (hs > 3) this.heading = dampAngle(this.heading, Math.atan2(this.vel.x, this.vel.z), 2, dt);
    const turn = (this.heading - prev) / Math.max(dt, 1e-3);
    this.bankR = damp(this.bankR, clamp(-turn * 0.6, -0.7, 0.7), 3, dt);
    this.root.position.copy(this.pos);
    if (this.state !== 'launched') {
      const pitch = this.state === 'dying' ? (this.crashed ? 0 : 0.5) : clamp(-this.vel.y / 60, -0.6, 0.6);
      this.root.rotation.set(damp(this.root.rotation.x, pitch, 3, dt), this.heading, this.bankR, 'YXZ');
    } else this.root.rotation.y = this.heading;
    if (this.hitT > 0) { this.hitT -= dt; for (const m of this.mats) m.emissiveIntensity = 2.5; } else if (this.state !== 'dying') for (const m of this.mats) m.emissiveIntensity = 0.6;
    if (this.state !== 'dying' && Math.random() < dt * 3 && G.fx) G.fx.smoke(this.pos.clone().add(V(-Math.sin(this.heading) * 6, 0, -Math.cos(this.heading) * 6)), 1, 6, 0.25);
  }
}
