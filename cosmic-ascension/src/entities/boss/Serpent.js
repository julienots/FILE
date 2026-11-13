import * as THREE from 'three';
import { toonMaterial } from '../../render/shaders/Toon.js';
import { createAtmosphere } from '../../render/shaders/Atmosphere.js';
import { Assets } from '../../assets/AssetLibrary.js';

// Serpent Céleste — colosse plus grand que les planètes, nage dans l'espace le long d'une trajectoire fermée.
// 3 draw calls: corps (InstancedMesh), orbes dorsales lumineuses (InstancedMesh), tête. GLB `boss_serpent` utilisé pour la tête s'il existe.
const N = 90;
export class Serpent {
  constructor({ center = [0, 0, -1500], size = [1900, 520, 1500], radius = 55, speed = .045, colorA = '#3a5fff', colorB = '#b45cff', glow = '#6ff2ff' } = {}) {
    this.root = new THREE.Group(); this.center = new THREE.Vector3().fromArray(center); this.size = size; this.radius = radius; this.speed = speed; this.u = 0;
    const geo = new THREE.SphereGeometry(1, 14, 10); geo.scale(1, 1, 1.7);
    this.body = new THREE.InstancedMesh(geo, toonMaterial({ color: '#ffffff', rim: glow, rimStrength: 1.1, rimPower: 2 }), N); this.body.frustumCulled = false;
    this.orbs = new THREE.InstancedMesh(new THREE.IcosahedronGeometry(1, 1), new THREE.MeshBasicMaterial({ color: glow }), N * 2); this.orbs.frustumCulled = false;
    const a = new THREE.Color(colorA), b = new THREE.Color(colorB), c = new THREE.Color();
    for (let i = 0; i < N; i++) this.body.setColorAt(i, c.copy(a).lerp(b, .5 + .5 * Math.sin(i * .35)));
    this.root.add(this.body, this.orbs);
    // tête
    this.head = new THREE.Group(); const hm = Assets.get('boss_serpent');
    if (hm) { hm.scale.multiplyScalar(radius * 4); this.head.add(hm); }
    else {
      const skin = toonMaterial({ color: colorA, rim: glow, rimStrength: 1.1 }), light = new THREE.MeshBasicMaterial({ color: glow }), R = radius;
      const skull = new THREE.Mesh(new THREE.SphereGeometry(R * 1.5, 20, 14), skin); skull.scale.set(1, .85, 1.5); this.head.add(skull);
      const snout = new THREE.Mesh(new THREE.SphereGeometry(R * 1.05, 16, 12), skin); snout.scale.set(1, .6, 1.5); snout.position.set(0, R * .15, R * 2); this.head.add(snout);
      this.jaw = new THREE.Mesh(new THREE.SphereGeometry(R, 16, 12), toonMaterial({ color: colorB, rim: glow })); this.jaw.scale.set(.9, .35, 1.5); this.jaw.position.set(0, -R * .55, R * 1.7); this.head.add(this.jaw);
      const maw = new THREE.Mesh(new THREE.SphereGeometry(R * .6, 12, 8), light); maw.position.set(0, -R * .2, R * 2.3); maw.scale.set(1, .3, 1); this.head.add(maw);
      for (const s of [-1, 1]) {
        const eye = new THREE.Mesh(new THREE.SphereGeometry(R * .28, 12, 10), new THREE.MeshBasicMaterial({ color: '#fff3a0' })); eye.position.set(s * R * .95, R * .5, R * 1.3); this.head.add(eye);
        const horn = new THREE.Mesh(new THREE.ConeGeometry(R * .3, R * 2.6, 8), light); horn.position.set(s * R * .9, R * 1.5, -R * .6); horn.rotation.set(-.9, 0, -.35 * s); this.head.add(horn);
        const fin = new THREE.Mesh(new THREE.ConeGeometry(R * .9, R * 3, 4), toonMaterial({ color: colorB, rim: glow, emissive: colorB, emissiveIntensity: .4 })); fin.position.set(s * R * 2, 0, -R * .5); fin.rotation.set(0, 0, -1.2 * s); fin.scale.z = .15; this.head.add(fin);
      }
    }
    this.head.add(createAtmosphere(radius * 2.2, glow, 3, .6)); this.root.add(this.head);
    this._p = new THREE.Vector3(); this._n = new THREE.Vector3(); this._m = new THREE.Matrix4(); this._q = new THREE.Quaternion(); this._s = new THREE.Vector3(); this._up = new THREE.Vector3(0, 1, 0); this._o = new THREE.Vector3();
    this.segments = Array.from({ length: N }, () => ({ pos: new THREE.Vector3(), r: 1 }));
  }
  // Position sur la trajectoire (courbe fermée + ondulation de nage).
  at(u, out) { const [A, B, C] = this.size; return out.set(Math.cos(u) * A, Math.sin(u * 2) * B + Math.sin(u * 9) * 60, Math.sin(u) * C + Math.cos(u * 7) * 80).add(this.center); }
  get headPos() { return this.head.position; }
  update(dt, t) {
    this.u += dt * this.speed; const R = this.radius, sp = .021;
    for (let i = 0; i <= N; i++) {
      const u = this.u - i * sp; this.at(u, this._p); this.at(u + .01, this._n); this._m.lookAt(this._p, this._n, this._up); this._q.setFromRotationMatrix(this._m);   // +Z vers l'avant
      if (i === 0) { this.head.position.copy(this._p); this.head.quaternion.copy(this._q); if (this.jaw) this.jaw.rotation.x = .25 + Math.sin(t * 1.3) * .2; continue; }
      const k = i / N, taper = Math.min(1, k * 6 + .55) * (1 - Math.pow(k, 3) * .92), r = R * taper * (1 + Math.sin(t * 2 - i * .5) * .04), j = i - 1;
      this._m.compose(this._p, this._q, this._s.set(r, r * .9, r)); this.body.setMatrixAt(j, this._m); this.segments[j].pos.copy(this._p); this.segments[j].r = r * 1.3;
      const pulse = .16 + .08 * Math.sin(t * 3 - i * .4);
      for (const s of [0, 1]) { this._o.set(s ? r * .55 : -r * .55, r * .78, 0).applyQuaternion(this._q).add(this._p); this._m.compose(this._o, this._q, this._s.setScalar(r * (s ? pulse : pulse * .8))); this.orbs.setMatrixAt(j * 2 + s, this._m); }
    }
    this.body.instanceMatrix.needsUpdate = true; this.orbs.instanceMatrix.needsUpdate = true;
  }
}
