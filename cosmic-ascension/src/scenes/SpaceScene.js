import * as THREE from 'three';
import { Input } from '../core/Input.js';
import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { Config } from '../core/Config.js';
import { createNebula } from '../render/shaders/Nebula.js';
import { createStars } from '../render/shaders/Stars.js';
import { buildPlanet } from '../world/PlanetFactory.js';
import { createAsteroidField, createStation, createSun, createGalaxies } from '../world/SpaceDecor.js';
import { createSpeedLines } from '../vfx/SpeedLines.js';
import { Particles } from '../vfx/Particles.js';
import { PLANETS } from '../data/planets.js';
import { Ship } from '../entities/Ship.js';
import { Serpent } from '../entities/boss/Serpent.js';
import { Devourer } from '../entities/boss/Devourer.js';

const SUN_POS = new THREE.Vector3(600, 400, 300).normalize().multiplyScalar(3600);
// Exploration spatiale: vol libre, boost warp, astéroïdes, station, soleil, atterrissage cinématique.
// Contrat de scène: { scene, camera, enter(params), update(dt,t), exit() }
export class SpaceScene {
  constructor(ctx) {
    this.ctx = ctx; this.scene = new THREE.Scene(); this.camera = new THREE.PerspectiveCamera(62, 1, 0.5, 14000); this.scene.add(this.camera);
    this.scene.add(new THREE.AmbientLight('#8a8cff', 0.45));
    this.sunLight = new THREE.DirectionalLight('#fff3dc', 1.25); this.sunLight.position.copy(SUN_POS); this.scene.add(this.sunLight);
    this.nebula = createNebula(); this.stars = createStars(Config.qualityPresets[Config.quality.level].stars); this.galaxies = createGalaxies();
    this.scene.add(this.nebula, this.stars, this.galaxies);
    this.sun = createSun(); this.sun.position.copy(SUN_POS); this.scene.add(this.sun);
    this.planets = PLANETS.map(def => { const p = buildPlanet(def); p.getObjectByName('water')?.material.uniforms.uSun.value.copy(SUN_POS).normalize(); p.position.fromArray(def.pos); this.scene.add(p); return p; });
    this.colliders = [];
    for (const f of [{ center: [150, 30, -1050], radius: 420, count: 200, seed: 3 }, { center: [-500, -120, -2300], radius: 380, count: 160, seed: 9, color: '#9fc4e8' }]) { const a = createAsteroidField(f); this.scene.add(a.mesh); this.colliders.push(...a.colliders); }
    this.station = createStation(); this.station.position.set(190, 50, -230); this.station.rotation.set(.4, .6, 0); this.scene.add(this.station);
    this.serpent = new Serpent(); this.scene.add(this.serpent.root); this.sighted = false; this.awe = 0;
    this.ship = new Ship(); this.scene.add(this.ship.root);
    this.lines = createSpeedLines(); this.camera.add(this.lines);
    this.trail = new Particles(300); this.scene.add(this.trail.points);
    this.vel = new THREE.Vector3(); this.q = new THREE.Quaternion(); this.e = new THREE.Euler(0, 0, 0, 'YXZ'); this.yaw = 0; this.pitch = 0; this.roll = 0; this.boost = 0; this.shake = 0;
    this.near = null; this.landing = null; this.fireCd = 0;
    this.fx = new Particles(500); this.scene.add(this.fx.points); this.serpent.particles = this.fx;
    this.devourer = new Devourer(); this.devourer.particles = this.fx; this.scene.add(this.devourer.root); if (State.data.world.flags.devourerDefeated) { this.devourer.dead = 99; this.devourer.root.visible = false; }
    this.bosses = [this.serpent, this.devourer]; if (State.data.world.flags.serpentDefeated) { this.serpent.dead = 99; this.serpent.root.visible = false; this.sighted = true; }
    const lg = new THREE.CapsuleGeometry(.35, 5, 3, 6).rotateX(Math.PI / 2);
    this.lasers = Array.from({ length: 18 }, () => { const m = new THREE.Mesh(lg, new THREE.MeshBasicMaterial({ color: '#9fffe0' })); m.visible = false; this.scene.add(m); return { mesh: m, vel: new THREE.Vector3(), life: 0 }; });
    bus.on('boss:hit-hero', () => { this.shake = 1; });
    bus.on('boss:defeated', ({ id }) => { State.data.world.flags[id + 'Defeated'] = true; });
    bus.on('hero:respawn', () => { if (this.ctx.scenes.space === this && this.isActive) this.ctx.transition('space', {}, { color: '#ffffff' }); });
    this._f = new THREE.Vector3(); this._a = new THREE.Vector3(); this._b = new THREE.Vector3(); this._c = new THREE.Vector3();
  }
  enter({ from } = {}) {
    const p = from ? this.planets.find(x => x.userData.def.id === from) : null, s = this.ship.root;
    if (p) { const r = p.userData.radius; s.position.copy(p.position).add(this._a.set(.3, .35, 1).normalize().multiplyScalar(r * 1.35)); this._a.copy(s.position).sub(p.position).normalize(); this.yaw = Math.atan2(-this._a.x, -this._a.z); this.pitch = Math.asin(this._a.y); this.vel.copy(this._a).multiplyScalar(160); this.boost = 1; }
    else { s.position.set(0, 0, 60); this.yaw = 0; this.pitch = 0; this.vel.set(0, 0, 0); }
    this.isActive = true; this.landing = null; this.near = null; this.camera.position.copy(s.position).addScaledVector(this.vel, -.08).add(this._b.set(0, 4, 0));
    bus.emit('scene:entered', { id: 'space' });
  }
  exit() { this.isActive = false; }
  update(dt, t) {
    const s = this.ship, sp = s.root.position, { _f, _a, _b, _c } = this, L = this.landing;
    let wantBoost = Input.buttons.boost ? 1 : 0;
    if (L) {                                   // pilote automatique d'atterrissage
      L.t += dt; _a.copy(L.planet.position).sub(sp).normalize(); const ty = Math.atan2(-_a.x, -_a.z), tp = Math.asin(_a.y), k = Math.min(1, dt * 4);
      this.yaw += Math.atan2(Math.sin(ty - this.yaw), Math.cos(ty - this.yaw)) * k; this.pitch += (tp - this.pitch) * k; wantBoost = 1; this.shake = Math.min(1, L.t);
      if (L.t > .9 && !L.done) { L.done = true; this.ctx.transition('planet', { id: L.planet.userData.def.id, arrive: true }, { color: L.planet.userData.def.atmosphere?.color ?? '#ffffff', out: .5, in: .9 }); }
    } else {
      this.yaw -= Input.move.x * dt * 1.5; this.pitch = THREE.MathUtils.clamp(this.pitch + Input.move.y * dt * 1.1, -1.35, 1.35); this.shake = Math.max(0, this.shake - dt * 2);
    }
    this.boost += (wantBoost - this.boost) * Math.min(1, dt * 3); const boost = this.boost;
    this.roll += (-(L ? 0 : Input.move.x) * 0.8 - this.roll) * Math.min(1, dt * 4);
    this.q.setFromEuler(this.e.set(this.pitch, this.yaw, this.roll)); s.root.quaternion.copy(this.q);
    _f.set(0, 0, -1).applyQuaternion(this.q);
    const cruise = 70 * (State.data.ship.level * .15 + .85), target = cruise * (1 + boost * 3.2);
    this.vel.lerp(_a.copy(_f).multiplyScalar(target), Math.min(1, dt * 1.8)); sp.addScaledVector(this.vel, dt);
    // collisions astéroïdes: rebond
    if (!L) for (let i = 0, cs = this.colliders; i < cs.length; i++) { const c = cs[i], rr = c.r + 2; if (Math.abs(sp.x - c.pos.x) > rr || Math.abs(sp.z - c.pos.z) > rr) continue;
      const d2 = sp.distanceToSquared(c.pos); if (d2 < rr * rr) { _a.copy(sp).sub(c.pos).normalize(); sp.copy(c.pos).addScaledVector(_a, rr); this.vel.reflect(_a).multiplyScalar(.5); this.shake = 1; bus.emit('ship:bump', { speed: this.vel.length() }); } }
    const speed01 = this.vel.length() / (cruise * 4.2); s.setThrust(speed01, t);
    // traînée moteur
    _a.set(0, 0, 2.2).applyQuaternion(this.q).add(sp); _b.copy(_f).multiplyScalar(-12 - boost * 30);
    this.trail.emit(_a, _b, boost > .4 ? '#b48aff' : '#7fe0ff', .9 + boost * 1.2, .35 + boost * .3, 1, 0); this.trail.update(dt, innerHeight);
    // Serpent céleste: nage, rebond sur son corps, première rencontre cinématique (la caméra se tourne vers lui)
    const sv = this.serpent; sv.update(dt, t, sp); if (!L) this.devourer.update(dt, t, sp, this.vel); const hd = sv.headPos.distanceTo(sp);
    if (!L && !sv.dead) for (let i = 0, sg = sv.segments; i < sg.length; i += 2) { const c = sg[i], rr = c.r + 3; if (sp.distanceToSquared(c.pos) < rr * rr) { _a.copy(sp).sub(c.pos).normalize(); sp.copy(c.pos).addScaledVector(_a, rr); this.vel.reflect(_a).multiplyScalar(.6); this.shake = 1; bus.emit('ship:bump', { speed: this.vel.length(), boss: 'serpent' }); break; } }
    if (!this.sighted && hd < 900) { this.sighted = true; this.awe = 4; bus.emit('boss:spawn', { id: 'serpent' }); bus.emit('cinematic:start', { id: 'boss-sighting', boss: 'serpent' }); }
    if (this.awe > 0) { this.awe -= dt; this.shake = Math.max(this.shake, .5); if (this.awe <= 0) bus.emit('cinematic:end', { id: 'boss-sighting' }); }
    // lasers du vaisseau (✸): visée assistée sur le cœur stellaire le plus proche dans l'axe
    this.fireCd -= dt;
    if (!L && Input.buttons.attack && this.fireCd <= 0) { this.fireCd = .14; const l = this.lasers.find(x => x.life <= 0);
      if (l) { _b.copy(_f); let best = .8; for (const bo of this.bosses) if (!bo.dead) for (const c of bo.cores) { if (c.hp <= 0) continue; _c.copy(c.pos).sub(sp); const d = _c.length(); if (d > 1100) continue; const dot = _c.normalize().dot(_f); if (dot > best) { best = dot; _b.copy(_c); } }
        l.life = 1.8; l.mesh.visible = true; l.mesh.position.copy(sp).addScaledVector(_b, 4); l.vel.copy(_b).multiplyScalar(650); l.mesh.lookAt(_c.copy(l.mesh.position).add(_b)); bus.emit('ship:fire', {}); } }
    for (const l of this.lasers) { if (l.life <= 0) continue; l.life -= dt; l.mesh.position.addScaledVector(l.vel, dt);
      if ((!sv.dead && sv.damageAt(l.mesh.position, 1)) || (!this.devourer.dead && this.devourer.damageAt(l.mesh.position, 1))) l.life = 0;
      else for (let i = 0, cs = this.colliders; i < cs.length; i++) if (cs[i].r > 10 && l.mesh.position.distanceToSquared(cs[i].pos) < cs[i].r * cs[i].r) { l.life = 0; for (let k = 0; k < 5; k++) this.fx.emit(l.mesh.position, _c.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(40), '#9fffe0', 3, .3, 2, 0); break; }
      if (l.life <= 0) l.mesh.visible = false; }
    this.fx.update(dt, innerHeight);
    // caméra poursuite + tremblement + FOV
    _a.set(0, 4.5 - boost, 15 + boost * 7).applyQuaternion(this.q).add(sp);
    this.camera.position.lerp(_a, Math.min(1, dt * (5 + boost * 3)));
    if (this.shake > 0) { const k = this.shake * (.12 + boost * .1); this.camera.position.x += (Math.random() - .5) * k; this.camera.position.y += (Math.random() - .5) * k; }
    this.camera.up.set(0, 1, 0).applyQuaternion(this.q); _a.copy(sp).addScaledVector(_f, 30); if (this.awe > 0) _a.lerp(sv.headPos, Math.min(1, this.awe) * Math.min(1, (4 - this.awe) * 2) * .75); this.camera.lookAt(_a);
    this.camera.fov += ((62 + boost * 22) - this.camera.fov) * Math.min(1, dt * 3);
    this.lines.userData.update(t, Math.max(0, boost - .15));
    // décor
    const px = this.ctx.renderer.renderer.getPixelRatio();
    for (const o of [this.nebula, this.stars, this.galaxies]) o.position.copy(this.camera.position);
    this.nebula.userData.update(t); this.stars.userData.update(t, px); this.sun.userData.update(dt, t); this.station.userData.update(dt, t);
    let near = null, nd = Infinity;
    for (const p of this.planets) { p.userData.update(dt); p.rotation.y += dt * 0.01; const d = p.position.distanceTo(sp) - p.userData.radius; if (d < nd) { nd = d; near = p; } }
    if (nd < 6 && !L) { _a.copy(sp).sub(near.position).normalize(); sp.copy(near.position).addScaledVector(_a, near.userData.radius + 6); this.vel.reflect(_a).multiplyScalar(.4); }   // pas de traversée de planète
    const def = near.userData.def, canLand = nd < near.userData.radius * 1.3;
    if (this.near !== near || this._can !== canLand) { this.near = near; this._can = canLand; bus.emit('space:near', { planet: def, canLand, distance: nd }); }
    if (canLand && !L && Input.pressed.action) { this.landing = { t: 0, planet: near }; bus.emit('cinematic:start', { id: 'landing', planet: def.id }); }
  }
}
