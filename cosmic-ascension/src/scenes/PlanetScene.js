import * as THREE from 'three';
import { Input } from '../core/Input.js';
import { bus } from '../core/EventBus.js';
import { createNebula } from '../render/shaders/Nebula.js';
import { createStars } from '../render/shaders/Stars.js';
import { buildPlanet } from '../world/PlanetFactory.js';
import { getPlanet } from '../data/planets.js';
import { Hero } from '../entities/Hero.js';
import { State } from '../core/State.js';

// Exploration à pied sur une petite planète sphérique (style "galaxie"): gravité radiale, caméra alignée sur la normale.
export class PlanetScene {
  constructor(ctx) {
    this.ctx = ctx; this.scene = new THREE.Scene(); this.camera = new THREE.PerspectiveCamera(55, 1, 0.1, 6000);
    this.scene.add(new THREE.HemisphereLight('#bfe3ff', '#5a3d7a', 0.55));
    const sun = new THREE.DirectionalLight('#fff2d8', 1.3); sun.position.set(200, 300, 150); this.scene.add(sun);
    this.hero = new Hero(); this.scene.add(this.hero.root);
    this.up = new THREE.Vector3(0, 1, 0); this.fwd = new THREE.Vector3(0, 0, -1); this.vh = 0; this.h = 0; this.ground = true;
  }
  enter({ id = 'home' } = {}) {
    if (this.planet) this.scene.remove(this.planet);
    for (const o of [this.nebula, this.stars]) o && this.scene.remove(o);
    this.def = getPlanet(id); this.planet = buildPlanet({ ...this.def, seed: this.def.seed }, 5); this.planet.position.set(0, 0, 0); this.scene.add(this.planet);
    // vu depuis la surface: pas de coque d'atmosphère/nuages (voile), on utilise un brouillard teinté
    for (const n of ['atmo', 'clouds']) { const o = this.planet.getObjectByName(n); if (o) o.visible = false; }
    this.scene.fog = new THREE.FogExp2(this.def.atmosphere?.color ?? "#8fd0ff", 0.0011);
    this.nebula = createNebula({ seed: this.def.seed }); this.scene.add(this.nebula); this.stars = createStars(3500); this.scene.add(this.stars);
    this.hero.setStage(State.data.hero.stage);
    this.dir = new THREE.Vector3(0, 1, 0.2).normalize(); this.fwd.set(0, 0, -1); this.vh = 0; this.h = 0;
    bus.emit('scene:entered', { id: 'planet', planet: this.def });
  }
  exit() {}
  update(dt, t) {
    const R = this.planet.userData.radius, hero = this.hero, m = Input.move;
    this.up.copy(this.dir);
    // repère caméra: "avant" caméra projeté dans le plan tangent
    const camFwd = this.fwd.clone().addScaledVector(this.up, -this.fwd.dot(this.up)).normalize();
    const right = new THREE.Vector3().crossVectors(camFwd, this.up).normalize();
    const mv = camFwd.clone().multiplyScalar(m.y).addScaledVector(right, m.x), sp = Math.min(1, mv.length());
    const scale = hero.root.scale.x, speed = 7 * Math.sqrt(scale);
    if (sp > .05) {
      mv.normalize(); const ang = (speed * sp * dt) / (R + this.h);
      const axis = new THREE.Vector3().crossVectors(this.up, mv).normalize();
      this.dir.applyAxisAngle(axis, ang); this.fwd.applyAxisAngle(axis, ang); this.heroFace = mv.clone();
    }
    // saut / gravité radiale
    if (this.ground && Input.pressed.jump) { this.vh = 9; this.ground = false; }
    this.vh -= 22 * dt; this.h += this.vh * dt;
    const gh = this.planet.userData.heightAt(this.dir) - R + 0.0; // hauteur sol relative
    if (this.h <= gh) { this.h = gh; this.vh = 0; this.ground = true; }
    // placement + orientation du héros
    hero.root.position.copy(this.dir).multiplyScalar(R + this.h);
    const faceDir = (this.heroFace || camFwd).clone().addScaledVector(this.dir, -(this.heroFace || camFwd).dot(this.dir)).normalize();
    const zAxis = faceDir, yAxis = this.dir, xAxis = new THREE.Vector3().crossVectors(yAxis, zAxis).normalize();
    hero.root.quaternion.slerp(new THREE.Quaternion().setFromRotationMatrix(new THREE.Matrix4().makeBasis(xAxis, yAxis, zAxis)), Math.min(1, dt * 12));
    hero.animate(dt, sp, !this.ground);
    // caméra: derrière + au-dessus, tourne avec Input.look
    if (Input.look.x) { this.fwd.applyAxisAngle(this.dir, -Input.look.x * .006); }
    const dist = 7 * Math.sqrt(scale), want = hero.root.position.clone().addScaledVector(camFwd, -dist).addScaledVector(this.dir, dist * .55);
    this.camera.position.lerp(want, Math.min(1, dt * 6)); this.camera.up.copy(this.dir).lerp(this.camera.up, .0);
    this.camera.up.copy(this.dir); this.camera.lookAt(hero.root.position.clone().addScaledVector(this.dir, scale * .8));
    this.planet.userData.update(dt); this.nebula.position.copy(this.camera.position); this.nebula.userData.update(t);
    this.stars.position.copy(this.camera.position); this.stars.userData.update(t, this.ctx.renderer.renderer.getPixelRatio());
    if (Input.pressed.action) this.ctx.goto('space', { from: this.def.id });
    // debug/prototype: évolution rapide
    if (Input.pressed.special) { const s = State.data.hero; s.stage = (s.stage + 1) % 6; hero.setStage(s.stage); bus.emit('hero:evolved', { stage: s.stage }); }
  }
}
