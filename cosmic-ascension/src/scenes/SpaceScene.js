import * as THREE from 'three';
import { Input } from '../core/Input.js';
import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { Config } from '../core/Config.js';
import { createNebula } from '../render/shaders/Nebula.js';
import { createStars } from '../render/shaders/Stars.js';
import { buildPlanet } from '../world/PlanetFactory.js';
import { PLANETS } from '../data/planets.js';
import { Ship } from '../entities/Ship.js';

// Scène d'exploration spatiale: vol libre du vaisseau entre les planètes.
// Contrat de scène: { scene, camera, enter(params), update(dt,t), exit() }  (voir docs/CONTRACTS.md)
export class SpaceScene {
  constructor(ctx) {
    this.ctx = ctx; this.scene = new THREE.Scene(); this.camera = new THREE.PerspectiveCamera(62, 1, 0.5, 12000);
    this.scene.add(new THREE.AmbientLight('#8a8cff', 0.45));
    this.sun = new THREE.DirectionalLight('#fff3dc', 1.25); this.sun.position.set(600, 400, 300); this.scene.add(this.sun);
    this.nebula = createNebula(); this.scene.add(this.nebula);
    this.stars = createStars(Config.qualityPresets[Config.quality.level].stars); this.scene.add(this.stars);
    this.planets = PLANETS.map(def => { const p = buildPlanet(def); p.getObjectByName('water')?.material.uniforms.uSun.value.set(600, 400, 300).normalize(); p.position.fromArray(def.pos); this.scene.add(p); return p; });
    this.ship = new Ship(); this.scene.add(this.ship.root);
    this.vel = new THREE.Vector3(); this.q = new THREE.Quaternion(); this.yaw = 0; this.pitch = 0; this.roll = 0;
    this.near = null; this._camPos = new THREE.Vector3();
  }
  enter({ from } = {}) {
    const p = from ? this.planets.find(x => x.userData.def.id === from) : null;
    if (p) { const r = p.userData.radius; this.ship.root.position.copy(p.position).add(new THREE.Vector3(0, r * 0.4, r * 2.4)); }
    else this.ship.root.position.set(0, 0, 60);
    this.yaw = 0; this.pitch = 0; this.vel.set(0, 0, 0); bus.emit('scene:entered', { id: 'space' });
  }
  exit() {}
  update(dt, t) {
    const s = this.ship, boost = Input.buttons.boost ? 1 : 0;
    this.yaw -= Input.move.x * dt * 1.4; this.pitch = THREE.MathUtils.clamp(this.pitch + Input.move.y * dt * 1.0, -1.2, 1.2);
    this.roll += (-Input.move.x * 0.8 - this.roll) * Math.min(1, dt * 4);
    this.q.setFromEuler(new THREE.Euler(this.pitch, this.yaw, this.roll, 'YXZ')); s.root.quaternion.copy(this.q);
    const fwd = new THREE.Vector3(0, 0, -1).applyQuaternion(this.q);
    const cruise = 70 * (State.data.ship.level * .15 + .85), target = cruise * (1 + boost * 2.8);
    this.vel.lerp(fwd.multiplyScalar(target), Math.min(1, dt * 1.8)); s.root.position.addScaledVector(this.vel, dt);
    s.setThrust(this.vel.length() / (cruise * 3.8), t);
    // caméra poursuite
    const back = new THREE.Vector3(0, 4.5, 15 + boost * 5).applyQuaternion(this.q), want = s.root.position.clone().add(back);
    this.camera.position.lerp(want, Math.min(1, dt * 5)); this.camera.lookAt(s.root.position.clone().add(fwd.multiplyScalar(30)));
    this.camera.fov += ((62 + boost * 18) - this.camera.fov) * Math.min(1, dt * 3); this.camera.updateProjectionMatrix();
    // décor
    this.nebula.position.copy(this.camera.position); this.nebula.userData.update(t);
    this.stars.position.copy(this.camera.position); this.stars.userData.update(t, this.ctx.renderer.renderer.getPixelRatio());
    let near = null, nd = Infinity;
    for (const p of this.planets) { p.userData.update(dt); p.rotation.y += dt * 0.01; const d = p.position.distanceTo(s.root.position) - p.userData.radius; if (d < nd) { nd = d; near = p; } }
    const def = near.userData.def, canLand = nd < near.userData.radius * 1.3;
    if (this.near !== near || this._can !== canLand) { this.near = near; this._can = canLand; bus.emit('space:near', { planet: def, canLand, distance: nd }); }
    if (canLand && Input.pressed.action) this.ctx.goto('planet', { id: def.id });
  }
}
