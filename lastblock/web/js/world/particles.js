// Système de particules instancié (un seul appel de rendu par type) : éclats, confettis,
// pixels, étincelles, débris, poussière. Physique simple : vitesse, gravité, rotation, durée de vie.
import * as THREE from 'three';
import { rand } from '../core/util.js';

const _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _e = new THREE.Euler(), _s = new THREE.Vector3(), _p = new THREE.Vector3();

class Pool {
  constructor(geo, mat, cap) {
    this.cap = cap;
    this.mesh = new THREE.InstancedMesh(geo, mat, cap);
    this.mesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    this.mesh.setColorAt(0, new THREE.Color());
    this.mesh.frustumCulled = false;
    this.mesh.count = 0;
    this.p = [];
  }
  add(o) {
    if (this.p.length >= this.cap) this.p.shift();
    this.p.push(o);
  }
  update(dt) {
    const arr = this.p; let n = 0;
    for (let i = arr.length - 1; i >= 0; i--) {
      const o = arr[i];
      o.life -= dt;
      if (o.life <= 0) { arr.splice(i, 1); continue; }
    }
    for (const o of arr) {
      o.vy -= o.g * dt; o.vx *= 1 - o.drag * dt; o.vy *= 1 - o.drag * dt; o.vz *= 1 - o.drag * dt;
      o.x += o.vx * dt; o.y += o.vy * dt; o.z += o.vz * dt;
      if (o.floor !== undefined && o.y < o.floor) { o.y = o.floor; o.vy *= -0.35; o.vx *= 0.7; o.vz *= 0.7; }
      o.rx += o.sx * dt; o.ry += o.sy * dt;
      const k = Math.min(1, o.life / o.fade);
      const sc = o.size * k;
      _e.set(o.rx, o.ry, 0); _q.setFromEuler(_e);
      _s.set(sc * (o.sxScale || 1), sc * (o.syScale || 1), sc);
      _p.set(o.x, o.y, o.z);
      _m.compose(_p, _q, _s);
      this.mesh.setMatrixAt(n, _m);
      this.mesh.setColorAt(n, o.color);
      n++;
    }
    this.mesh.count = n;
    this.mesh.instanceMatrix.needsUpdate = true;
    if (this.mesh.instanceColor) this.mesh.instanceColor.needsUpdate = true;
  }
  clear() { this.p.length = 0; this.mesh.count = 0; }
}

export class Particles {
  constructor(parent, cap = 700) {
    this.group = new THREE.Group();
    parent.add(this.group);
    const cube = new THREE.BoxGeometry(1, 1, 1);
    const quad = new THREE.PlaneGeometry(1, 1);
    this.solid = new Pool(cube, new THREE.MeshStandardMaterial({ roughness: 0.35, metalness: 0.1 }), cap);
    this.glow = new Pool(cube, new THREE.MeshBasicMaterial({ toneMapped: false, transparent: true, opacity: 0.95, blending: THREE.AdditiveBlending, depthWrite: false }), cap);
    this.flat = new Pool(quad, new THREE.MeshBasicMaterial({ side: THREE.DoubleSide, toneMapped: false }), cap / 2);
    this.solid.mesh.castShadow = true;
    this.group.add(this.solid.mesh, this.glow.mesh, this.flat.mesh);
    this.burstStyle = 'shards';
  }

  _o(x, y, z, color, o = {}) {
    return Object.assign({
      x, y, z, vx: 0, vy: 0, vz: 0, g: 18, drag: 0.4, rx: rand(6), ry: rand(6), sx: rand(-8, 8), sy: rand(-8, 8),
      life: rand(0.6, 1.2), fade: 0.4, size: 0.2, color: color instanceof THREE.Color ? color : new THREE.Color(color),
    }, o);
  }

  /** Explosion d'un bloc détruit, selon le style équipé dans la collection. */
  blockBurst(x, y, z, color, power = 1, style = this.burstStyle) {
    const c = new THREE.Color(color);
    if (style === 'shards') {
      for (let i = 0; i < 7; i++) this.solid.add(this._o(x + rand(-0.4, 0.4), y + rand(-0.4, 0.4), z + rand(-0.3, 0.3), c, { vx: rand(-6, 6) * power, vy: rand(2, 9) * power, vz: rand(1, 8) * power, size: rand(0.12, 0.3) }));
      for (let i = 0; i < 4; i++) this.glow.add(this._o(x, y, z, c.clone().multiplyScalar(2.2), { vx: rand(-5, 5), vy: rand(-3, 6), vz: rand(0, 6), g: 4, size: rand(0.05, 0.12), life: rand(0.3, 0.7) }));
    } else if (style === 'confetti') {
      const cols = [0xff3b5c, 0xffd21f, 0x19d3ff, 0x3be36b, 0xa24bff];
      for (let i = 0; i < 9; i++) this.flat.add(this._o(x, y, z, cols[i % cols.length], { vx: rand(-5, 5), vy: rand(4, 11), vz: rand(0, 6), g: 9, drag: 1.6, size: rand(0.18, 0.3), syScale: 0.55, life: rand(1, 1.8), sx: rand(-14, 14), sy: rand(-14, 14) }));
    } else if (style === 'pixels') {
      for (let i = 0; i < 10; i++) this.glow.add(this._o(x + rand(-0.4, 0.4), y + rand(-0.4, 0.4), z, c.clone().multiplyScalar(1.6), { vx: rand(-1, 1), vy: rand(1, 4), vz: rand(0, 2), g: -2, drag: 1, size: rand(0.12, 0.2), sx: 0, sy: 0, rx: 0, ry: 0, life: rand(0.6, 1.1) }));
    } else if (style === 'sparks') {
      for (let i = 0; i < 12; i++) {
        const a = rand(Math.PI * 2), s = rand(5, 13);
        this.glow.add(this._o(x, y, z, c.clone().multiplyScalar(3), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, vz: rand(0, 5), g: 10, drag: 2, size: rand(0.05, 0.1), sxScale: 3, life: rand(0.3, 0.6) }));
      }
    } else if (style === 'void') {
      for (let i = 0; i < 6; i++) this.solid.add(this._o(x, y, z, 0x050505, { vx: rand(-3, 3), vy: rand(-3, 3), vz: rand(0, 3), g: -1, drag: 1.5, size: rand(0.15, 0.3), life: rand(0.8, 1.5) }));
      for (let i = 0; i < 4; i++) this.glow.add(this._o(x, y, z, new THREE.Color(3, 0.1, 0.1), { vx: rand(-4, 4), vy: rand(-4, 4), vz: rand(0, 4), g: 0, drag: 2, size: rand(0.05, 0.1) }));
    }
  }

  /** Débris lourds (verre, sol, monstre) */
  debris(x, y, z, color, n = 10, o = {}) {
    for (let i = 0; i < n; i++) this.solid.add(this._o(x + rand(-0.5, 0.5), y + rand(-0.5, 0.5), z + rand(-0.5, 0.5), color, Object.assign({ vx: rand(-6, 6), vy: rand(0, 8), vz: rand(-6, 6), size: rand(0.15, 0.45), life: rand(1, 2), floor: o.floor }, o)));
  }
  sparks(x, y, z, color, n = 12, speed = 10) {
    for (let i = 0; i < n; i++) {
      const a = rand(Math.PI * 2), b = rand(-1, 1);
      this.glow.add(this._o(x, y, z, new THREE.Color(color).multiplyScalar(2.5), { vx: Math.cos(a) * speed * rand(0.4, 1), vy: Math.sin(a) * speed * rand(0.4, 1), vz: b * speed, g: 8, drag: 2, size: rand(0.05, 0.12), sxScale: 2.5, life: rand(0.25, 0.6) }));
    }
  }
  dust(x, y, z, n = 6, color = 0x8888aa) {
    for (let i = 0; i < n; i++) this.glow.add(this._o(x + rand(-1, 1), y, z + rand(-1, 1), new THREE.Color(color).multiplyScalar(0.5), { vx: rand(-1, 1), vy: rand(0.5, 2), vz: rand(-1, 1), g: 0, drag: 1.2, size: rand(0.1, 0.25), life: rand(0.5, 1) }));
  }
  trail(x, y, z, color) {
    this.glow.add(this._o(x + rand(-0.3, 0.3), y + rand(-0.3, 0.3), z, new THREE.Color(color).multiplyScalar(1.4), { g: 0, drag: 0, size: rand(0.08, 0.16), life: rand(0.25, 0.45), fade: 0.3, sx: 0, sy: 0 }));
  }

  update(dt) { this.solid.update(dt); this.glow.update(dt); this.flat.update(dt); }
  clear() { this.solid.clear(); this.glow.clear(); this.flat.clear(); }
}
