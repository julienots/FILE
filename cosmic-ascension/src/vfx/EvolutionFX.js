import * as THREE from 'three';
import { EVOLUTIONS } from '../data/evolutions.js';
import { bus } from '../core/EventBus.js';
// Cinématique de transformation: charge (lévitation, spirale de particules) → flash + onde de choc → nouvelle forme (pop d'échelle).
// Usage: fx.play(hero, toStage, up) ; fx.update(dt, up) renvoie true tant que la cinématique tourne ; fx.camPull ∈[0,1] pour la caméra.
const DUR = 3.0, SWAP = 1.5;
export class EvolutionFX {
  constructor(scene, particles) {
    this.particles = particles; this.active = false; this.camPull = 0;
    this.flash = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 2), new THREE.MeshBasicMaterial({ color: '#fff', transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false }));
    this.ring = new THREE.Mesh(new THREE.RingGeometry(.82, 1, 48), new THREE.MeshBasicMaterial({ color: '#fff', transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide }));
    this.flash.visible = this.ring.visible = false; scene.add(this.flash, this.ring);
    this._p = new THREE.Vector3(); this._v = new THREE.Vector3(); this._a = new THREE.Vector3(); this._b = new THREE.Vector3(); this._q = new THREE.Quaternion();
  }
  play(hero, toStage) {
    if (this.active) this._swap();
    this.hero = hero; this.to = toStage; this.t = 0; this.active = true; this.swapped = false;
    this.s0 = hero.root.scale.x; this.s1 = EVOLUTIONS[toStage].scale; this.color = EVOLUTIONS[toStage].glow;
    this.flash.material.color.set(this.color); this.ring.material.color.set(this.color);
    bus.emit('cinematic:start', { id: 'evolution', stage: toStage });
  }
  _swap() { this.hero.setStage(this.to); this.swapped = true; }
  update(dt, up) {
    if (!this.active) { this.camPull += (0 - this.camPull) * Math.min(1, dt * 3); return false; }
    const t = (this.t += dt), h = this.hero, P = this.particles, c = this._p.copy(h.root.position).addScaledVector(up, this.s0 * .7);
    this._a.set(1, 0, 0).cross(up); if (this._a.lengthSq() < .01) this._a.set(0, 0, 1).cross(up); this._a.normalize(); this._b.crossVectors(up, this._a);
    this.camPull += (1 - this.camPull) * Math.min(1, dt * 3);
    if (t < SWAP) {                                    // charge
      const k = t / SWAP; h.lift = k * k * 1.6 * this.s0; h.spin = (h.spin || 0) + dt * (3 + k * 30); h.scaleMul = 1 - k * .25 + Math.sin(t * 40) * .03 * k;
      for (let i = 0; i < 4; i++) { const a = Math.random() * 6.283, r = (3 + Math.random() * 3) * this.s1, y = (Math.random() - .3) * 3 * this.s1;
        this._v.copy(c).addScaledVector(this._a, Math.cos(a) * r).addScaledVector(this._b, Math.sin(a) * r).addScaledVector(up, y);
        const vx = this._q; P.emit(this._v, this._p.clone().sub(this._v).multiplyScalar(1.6).addScaledVector(up, 0), this.color, .35 * this.s1, .6, 0, 0); }
      h.glow.intensity = 2 + k * 14;
    } else {
      if (!this.swapped) {
        this._swap(); this.flash.visible = this.ring.visible = true; this.flash.position.copy(c); this.ring.position.copy(h.root.position).addScaledVector(up, .15);
        this.ring.quaternion.setFromUnitVectors(this._v.set(0, 0, 1), up);
        for (let i = 0; i < 140; i++) { const a = Math.random() * 6.283, e = Math.random() * 2 - .4, sp = (6 + Math.random() * 14) * this.s1;
          this._v.copy(this._a).multiplyScalar(Math.cos(a)).addScaledVector(this._b, Math.sin(a)).addScaledVector(up, e).normalize().multiplyScalar(sp);
          P.emit(c, this._v, i % 3 ? this.color : '#ffffff', (.3 + Math.random() * .5) * this.s1, .8 + Math.random() * .9, 2.2, 3); }
        bus.emit('hero:transformed', { stage: this.to });
      }
      const k = (t - SWAP) / (DUR - SWAP), e = 1 - Math.pow(1 - k, 3);
      h.lift = (1 - e) * 1.6 * this.s1; h.spin = (h.spin || 0) + dt * 30 * (1 - e); if (k > .85) h.spin = 0;
      h.scaleMul = 1 + Math.sin(Math.min(1, k * 2.2) * Math.PI) * .35 * (1 - k);            // pop élastique
      this.flash.scale.setScalar((1 + e * 9) * this.s1); this.flash.material.opacity = (1 - e) * .9;
      this.ring.scale.setScalar((1 + e * 26) * this.s1); this.ring.material.opacity = (1 - e) * .8;
      if (t >= DUR) { this.active = false; h.lift = 0; h.spin = 0; h.scaleMul = 1; this.flash.visible = this.ring.visible = false; bus.emit('cinematic:end', { id: 'evolution' }); }
    }
    return true;
  }
}
