import * as THREE from 'three';
import { toonMaterial, addOutline } from '../render/shaders/Toon.js';
import { EVOLUTIONS } from '../data/evolutions.js';
import { Assets } from '../assets/AssetLibrary.js';

// Héros évolutif. Si un GLB `hero_<stageId>` existe il remplace le corps procédural (même API).
// FABLE: pousser les formes (ailes, halo, cornes, cape d'énergie, particules) par stade.
export class Hero {
  constructor() {
    this.root = new THREE.Group(); this.body = new THREE.Group(); this.root.add(this.body);
    this.stage = -1; this.t = 0; this.speed = 0; this.parts = {};
    this.glow = new THREE.PointLight('#7fd0ff', 1.2, 14); this.root.add(this.glow); this.glow.position.y = .6;
    this.setStage(0);
  }
  setStage(i) {
    if (i === this.stage) return; this.stage = i; const st = EVOLUTIONS[i];
    while (this.body.children.length) this.body.remove(this.body.children[0]);
    this.parts = {};
    const model = Assets.get('hero_' + st.id);
    if (model) { this.body.add(model); }
    else this._buildProcedural(st, i);
    this.root.scale.setScalar(st.scale); this.glow.color.set(st.glow); this.glow.intensity = 1 + i * .5;
  }
  _buildProcedural(st, i) {
    const mat = toonMaterial({ color: st.color, rim: st.glow, rimStrength: .8 });
    const torso = new THREE.Mesh(new THREE.SphereGeometry(.5, 20, 16), mat); torso.position.y = .55; torso.scale.set(1, 1.1, .95); addOutline(torso, '#1a0f33', .035);
    const white = new THREE.MeshBasicMaterial({ color: '#ffffff' }), black = new THREE.MeshBasicMaterial({ color: '#10101a' });
    for (const s of [-1, 1]) {
      const eye = new THREE.Mesh(new THREE.SphereGeometry(.13, 12, 10), white); eye.position.set(.18 * s, .7, .38); const pup = new THREE.Mesh(new THREE.SphereGeometry(.07, 10, 8), black); pup.position.z = .08; eye.add(pup); this.body.add(eye);
      const foot = new THREE.Mesh(new THREE.SphereGeometry(.14, 10, 8), mat); foot.position.set(.2 * s, .1, .08); foot.scale.set(1, .6, 1.3); this.body.add(foot); this.parts['foot' + s] = foot;
      if (i >= 1) { const arm = new THREE.Mesh(new THREE.CapsuleGeometry(.07, .25, 4, 8), mat); arm.position.set(.5 * s, .5, 0); arm.rotation.z = -.6 * s; this.body.add(arm); this.parts['arm' + s] = arm; }
    }
    this.body.add(torso);
    const glowMat = new THREE.MeshBasicMaterial({ color: st.glow });
    if (i >= 2) { const tail = new THREE.Mesh(new THREE.ConeGeometry(.2, .9, 8), glowMat); tail.rotation.x = Math.PI / 2 + .3; tail.position.set(0, .5, -.7); this.body.add(tail); this.parts.tail = tail; }
    if (i >= 3) for (const s of [-1, 1]) { const w = new THREE.Mesh(new THREE.ConeGeometry(.35, 1.2, 4), glowMat); w.position.set(.7 * s, .9, -.2); w.rotation.set(0, 0, -1.1 * s); this.body.add(w); this.parts['wing' + s] = w; }
    if (i >= 4) { const h = new THREE.Mesh(new THREE.TorusGeometry(.45, .04, 8, 32), glowMat); h.position.y = 1.5; h.rotation.x = Math.PI / 2; this.body.add(h); this.parts.halo = h; }
    if (i >= 5) { const core = new THREE.Mesh(new THREE.IcosahedronGeometry(.25, 1), new THREE.MeshBasicMaterial({ color: '#fff' })); core.position.y = .65; core.position.z = .1; this.body.add(core); this.parts.core = core; }
  }
  // speed01: 0 immobile → 1 course. airborne: bool
  animate(dt, speed01, airborne) {
    this.t += dt; const t = this.t, b = this.body;
    const bob = Math.sin(t * (3 + speed01 * 9)) * (.03 + speed01 * .06);
    b.position.y = airborne ? .15 : bob; b.scale.set(1 + (airborne ? -.08 : -bob), 1 + (airborne ? .15 : bob * 1.5), 1 + (airborne ? -.08 : -bob));
    b.rotation.z = Math.sin(t * 9) * .08 * speed01;
    for (const s of [-1, 1]) {
      const f = this.parts['foot' + s]; if (f) f.position.z = .08 + Math.sin(t * 11 + (s > 0 ? Math.PI : 0)) * .15 * speed01;
      const w = this.parts['wing' + s]; if (w) w.rotation.z = (-1.1 + Math.sin(t * 5) * .25) * s;
    }
    if (this.parts.halo) this.parts.halo.rotation.z = t * 2;
    if (this.parts.core) this.parts.core.scale.setScalar(1 + Math.sin(t * 6) * .2);
    if (this.parts.tail) this.parts.tail.scale.y = 1 + Math.sin(t * 12) * .15;
  }
}
