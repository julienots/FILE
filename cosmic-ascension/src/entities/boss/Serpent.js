import * as THREE from 'three';
import { toonMaterial } from '../../render/shaders/Toon.js';
import { createAtmosphere } from '../../render/shaders/Atmosphere.js';
import { Assets } from '../../assets/AssetLibrary.js';
import { bus } from '../../core/EventBus.js';

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
    // Points faibles: 6 cœurs stellaires le long du corps. Tous détruits → le serpent est vaincu.
    this.cores = [8, 22, 36, 50, 64, 78].map(seg => { const m = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 2), new THREE.MeshBasicMaterial({ color: '#ffe46a' })); m.add(createAtmosphere(1.25, '#ffb02e', 2.5, 1.2)); this.root.add(m); return { seg, hp: 6, mesh: m, pos: m.position, r: radius * .6 }; });
    this.shots = Array.from({ length: 12 }, () => { const m = new THREE.Mesh(new THREE.IcosahedronGeometry(7, 1), new THREE.MeshBasicMaterial({ color: '#ff5ad0' })); m.visible = false; this.root.add(m); return { mesh: m, vel: new THREE.Vector3(), life: 0 }; });
    this.spitT = 3; this.dead = 0; this.rage = 0; this.particles = null;
    this.segments = Array.from({ length: N }, () => ({ pos: new THREE.Vector3(), r: 1 }));
  }
  // Position sur la trajectoire (courbe fermée + ondulation de nage).
  at(u, out) { const [A, B, C] = this.size; return out.set(Math.cos(u) * A, Math.sin(u * 2) * B + Math.sin(u * 9) * 60, Math.sin(u) * C + Math.cos(u * 7) * 80).add(this.center); }
  get headPos() { return this.head.position; }
  get alive() { return this.cores.some(c => c.hp > 0); }
  // Dégâts d'un tir du vaisseau en `pos`. Renvoie true si un cœur est touché.
  damageAt(pos, dmg) {
    if (this.hostile === false) return false;      // tant que l'histoire ne l'a pas réveillé: invulnérable et pacifique
    for (const c of this.cores) { if (c.hp <= 0 || c.pos.distanceTo(pos) > c.r + 6) continue; c.hp -= dmg; c.flash = .15;
      if (this.particles) for (let k = 0; k < 10; k++) this.particles.emit(pos, this._o.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(120), '#ffe46a', 9, .5, 2, 0);
      if (c.hp <= 0) { c.mesh.visible = false; this.rage++; const left = this.cores.filter(x => x.hp > 0).length;
        if (this.particles) for (let k = 0; k < 60; k++) this.particles.emit(c.pos, this._o.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(260), k % 2 ? '#ffe46a' : '#ffffff', 16, 1.2, 1.5, 0);
        bus.emit('boss:phase', { id: 'serpent', phase: this.rage, coresLeft: left });
        if (!left) { this.dead = .001; bus.emit('boss:defeated', { id: 'serpent', xp: 1500 }); } }
      return true; }
    return false;
  }
  update(dt, t, shipPos) {
    if (this.dead) { this.dead += dt; if (this.dead > 7) { this.root.visible = false; return; } }
    this.u += dt * this.speed * (1 + this.rage * .25) * (this.dead ? .2 : 1); const R = this.radius, sp = .021;
    for (let i = 0; i <= N; i++) {
      const u = this.u - i * sp; this.at(u, this._p); this.at(u + .01, this._n); this._m.lookAt(this._p, this._n, this._up); this._q.setFromRotationMatrix(this._m);   // +Z vers l'avant
      if (i === 0) { this.head.position.copy(this._p); this.head.quaternion.copy(this._q); if (this.dead) this.head.scale.setScalar(Math.max(.001, 1 - this.dead * .9)); if (this.jaw) this.jaw.rotation.x = .25 + Math.sin(t * 1.3) * .2; continue; }
      const dk = this.dead ? Math.max(0, 1 - Math.max(0, this.dead * 20 - i * .2) * .25) : 1; if (this.dead && dk > 0 && dk < 1 && this.particles && Math.random() < .3) this.particles.emit(this._p, this._o.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(150), i % 2 ? '#6ff2ff' : '#ffffff', 14, 1, 1.5, 0);
      const k = i / N, taper = dk * Math.min(1, k * 6 + .55) * (1 - Math.pow(k, 3) * .92), r = R * taper * (1 + Math.sin(t * 2 - i * .5) * .04), j = i - 1;
      this._m.compose(this._p, this._q, this._s.set(r, r * .9, r)); this.body.setMatrixAt(j, this._m); this.segments[j].pos.copy(this._p); this.segments[j].r = r * 1.3;
      const pulse = .16 + .08 * Math.sin(t * 3 - i * .4);
      for (const s of [0, 1]) { this._o.set(s ? r * .55 : -r * .55, r * .78, 0).applyQuaternion(this._q).add(this._p); this._m.compose(this._o, this._q, this._s.setScalar(r * (s ? pulse : pulse * .8))); this.orbs.setMatrixAt(j * 2 + s, this._m); }
    }
    this.body.instanceMatrix.needsUpdate = true; this.orbs.instanceMatrix.needsUpdate = true;
    for (const c of this.cores) { if (c.hp <= 0) continue; const sg = this.segments[c.seg]; this._o.copy(sg.pos).sub(this.center).normalize(); c.pos.copy(sg.pos).addScaledVector(this._up, sg.r * .75); c.flash = Math.max(0, (c.flash || 0) - dt);
      c.mesh.scale.setScalar(c.r * (1 + Math.sin(t * 4 + c.seg) * .12) * (c.flash ? 1.4 : 1)); c.mesh.material.color.set(c.flash ? '#ffffff' : this.hostile === false ? '#3a4a8a' : '#ffe46a'); }
    // crachats d'énergie vers le vaisseau
    if (shipPos && !this.dead && this.hostile !== false) {
      const d = this.head.position.distanceTo(shipPos); this.spitT -= dt;
      if (d < 950 && this.spitT <= 0) { this.spitT = Math.max(.9, 2.6 - this.rage * .3); const sh = this.shots.find(x => x.life <= 0);
        if (sh) { sh.life = 6; sh.mesh.visible = true; sh.mesh.position.copy(this.head.position); sh.vel.copy(shipPos).sub(this.head.position).normalize().multiplyScalar(210 + this.rage * 25); } }
      for (const sh of this.shots) { if (sh.life <= 0) continue; sh.life -= dt; const p = sh.mesh.position; this._o.copy(shipPos).sub(p).normalize().multiplyScalar(260 * dt); sh.vel.add(this._o).setLength(210 + this.rage * 25);   // légèrement téléguidé
        p.addScaledVector(sh.vel, dt); sh.mesh.scale.setScalar(1 + Math.sin(t * 20) * .2); if (this.particles) this.particles.emit(p, this._o.set(0, 0, 0), '#ff5ad0', 10, .3, 0, 0);
        if (p.distanceTo(shipPos) < 9) { sh.life = 0; bus.emit('boss:hit-hero', { id: 'serpent', dmg: 14 }); }
        if (sh.life <= 0) sh.mesh.visible = false; }
    } else for (const sh of this.shots) { sh.life = 0; sh.mesh.visible = false; }
  }
}
