// Physique légère des débris : morceaux de bâtiments (texturés) et éclats de roche.
// Réserve fixe, collisions avec le terrain, rebonds, frottement, mise en sommeil : les décombres
// restent au sol (persistance), les plus anciens sont recyclés quand la réserve est pleine.
import * as THREE from 'three';
import { G, Q, rand, clamp } from './core.js';

const _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _q2 = new THREE.Quaternion(), _v = new THREE.Vector3(), _s = new THREE.Vector3(), _e = new THREE.Euler();

class DebrisPool {
  constructor(scene, geo, mat, max, castShadow = true) {
    this.max = max;
    this.mesh = new THREE.InstancedMesh(geo, mat, max);
    this.mesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    this.mesh.castShadow = castShadow; this.mesh.receiveShadow = true;
    this.mesh.frustumCulled = false;
    this.mesh.count = 0;
    this.mesh.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(max * 3).fill(1), 3);
    scene.add(this.mesh);
    this.p = new Float32Array(max * 3); this.v = new Float32Array(max * 3);
    this.q = new Float32Array(max * 4); this.w = new Float32Array(max * 3);
    this.s = new Float32Array(max * 3); this.awake = new Uint8Array(max); this.age = new Float32Array(max);
    this.used = 0; this.next = 0;
    for (let i = 0; i < max; i++) this.q[i * 4 + 3] = 1;
  }
  spawn(x, y, z, sx, sy, sz, vx, vy, vz, color) {
    let i;
    if (this.used < this.max) i = this.used++;
    else { i = this.next; this.next = (this.next + 1) % this.max; }
    this.p.set([x, y, z], i * 3); this.v.set([vx, vy, vz], i * 3);
    _q.setFromEuler(_e.set(rand(0, 6), rand(0, 6), rand(0, 6))); this.q.set([_q.x, _q.y, _q.z, _q.w], i * 4);
    this.w.set([rand(-6, 6), rand(-6, 6), rand(-6, 6)], i * 3);
    this.s.set([sx, sy, sz], i * 3);
    this.awake[i] = 1; this.age[i] = 0;
    if (color) this.mesh.instanceColor.setXYZ(i, color.r, color.g, color.b); else this.mesh.instanceColor.setXYZ(i, 1, 1, 1);
    this.mesh.instanceColor.needsUpdate = true;
    this.mesh.count = this.used;
    this.write(i);
    return i;
  }
  write(i) {
    _q.set(this.q[i * 4], this.q[i * 4 + 1], this.q[i * 4 + 2], this.q[i * 4 + 3]);
    _m.compose(_v.set(this.p[i * 3], this.p[i * 3 + 1], this.p[i * 3 + 2]), _q, _s.set(this.s[i * 3], this.s[i * 3 + 1], this.s[i * 3 + 2]));
    this.mesh.setMatrixAt(i, _m);
  }
  update(dt) {
    const T = G.terrain;
    let any = false;
    for (let i = 0; i < this.used; i++) {
      if (!this.awake[i]) continue;
      any = true;
      this.age[i] += dt;
      const k = i * 3;
      this.v[k + 1] -= 28 * dt;
      this.p[k] += this.v[k] * dt; this.p[k + 1] += this.v[k + 1] * dt; this.p[k + 2] += this.v[k + 2] * dt;
      // rotation
      const wx = this.w[k], wy = this.w[k + 1], wz = this.w[k + 2];
      _q.set(this.q[i * 4], this.q[i * 4 + 1], this.q[i * 4 + 2], this.q[i * 4 + 3]);
      _q2.setFromEuler(_e.set(wx * dt, wy * dt, wz * dt)); _q.multiply(_q2).normalize();
      this.q[i * 4] = _q.x; this.q[i * 4 + 1] = _q.y; this.q[i * 4 + 2] = _q.z; this.q[i * 4 + 3] = _q.w;
      const half = Math.min(this.s[k], this.s[k + 1], this.s[k + 2]) * 0.5;
      const gy = T.heightAt(this.p[k], this.p[k + 2]) + half * 0.8;
      if (this.p[k + 1] < gy) {
        this.p[k + 1] = gy;
        if (this.v[k + 1] < -3 && G.fx && Math.random() < 0.3) G.fx.dust(_v.set(this.p[k], gy, this.p[k + 2]), 1, 1);
        this.v[k + 1] = -this.v[k + 1] * 0.28;
        this.v[k] *= 0.55; this.v[k + 2] *= 0.55;
        this.w[k] *= 0.5; this.w[k + 1] *= 0.5; this.w[k + 2] *= 0.5;
        const sp = Math.hypot(this.v[k], this.v[k + 1], this.v[k + 2]);
        if (sp < 1.2 || this.age[i] > 12) { this.awake[i] = 0; this.v.fill(0, k, k + 3); }
      }
      this.write(i);
    }
    if (any) this.mesh.instanceMatrix.needsUpdate = true;
  }
  // réveille les débris posés près d'une explosion
  blast(x, z, r, power) {
    for (let i = 0; i < this.used; i++) {
      const k = i * 3, dx = this.p[k] - x, dz = this.p[k + 2] - z, d = Math.hypot(dx, dz);
      if (d > r) continue;
      const f = (1 - d / r) * power * 0.4;
      this.v[k] += dx / (d + 0.1) * f; this.v[k + 1] += f * 0.8; this.v[k + 2] += dz / (d + 0.1) * f;
      this.w[k] = rand(-6, 6); this.w[k + 2] = rand(-6, 6);
      this.awake[i] = 1; this.age[i] = 0;
    }
  }
}

export class Physics {
  constructor(scene, buildingMat) {
    const q = Q().label === 'Bas' ? 0.5 : Q().label === 'Moyen' ? 0.75 : 1;
    // un réservoir par couche de texture des bâtiments (-1 = couleur unie)
    this.pools = new Map();
    for (let layer = -1; layer <= 6; layer++) {
      const g = new THREE.BoxGeometry(1, 1, 1);
      const n = g.attributes.position.count;
      g.setAttribute('color', new THREE.BufferAttribute(new Float32Array(n * 3).fill(1), 3));
      g.setAttribute('layer', new THREE.BufferAttribute(new Float32Array(n).fill(layer), 1));
      this.pools.set(layer, new DebrisPool(scene, g, buildingMat, Math.round((layer === 0 || layer === 5 ? 260 : 140) * q)));
    }
    const rockG = new THREE.DodecahedronGeometry(0.6, 0);
    const rockM = new THREE.MeshStandardMaterial({ color: 0x6e6256, roughness: 0.95, flatShading: true });
    this.rockPool = new DebrisPool(scene, rockG, rockM, Math.round(500 * q));
  }

  // Effondrement d'un bâtiment : chaque pièce se fragmente et part selon l'impact
  shatter(b, from, power, mat) {
    const c = Math.cos(b.ry), s = Math.sin(b.ry), maxPieces = Q().label === 'Bas' ? 26 : 48;
    const pieces = b.pieces.slice().sort(() => Math.random() - 0.5).slice(0, maxPieces);
    const col = new THREE.Color();
    for (const p of pieces) {
      const pool = this.pools.get(p.layer ?? -1) || this.pools.get(-1);
      const n = p.w * p.h * p.d > 6 ? 2 : 1;
      for (let k = 0; k < n; k++) {
        const lx = p.x + (n > 1 ? (k - 0.5) * p.w * 0.5 : 0), lz = p.z;
        const x = b.x + lx * c + lz * s, z = b.z - lx * s + lz * c, y = b.y + p.y;
        let dx = x - from.x, dz = z - from.z; const L = Math.hypot(dx, dz) || 1; dx /= L; dz /= L;
        const f = Math.min(40, 4 + power * 0.04) * rand(0.4, 1.2);
        col.setHex(p.layer >= 0 ? 0xffffff : p.color);
        pool.spawn(x, y, z, Math.max(0.3, p.w / n * rand(0.6, 1)), Math.max(0.25, p.h * rand(0.3, 0.6)), Math.max(0.25, p.d * rand(0.6, 1)),
          dx * f + rand(-2, 2), rand(2, 8) + f * 0.4, dz * f + rand(-2, 2), col);
      }
    }
  }

  rocks(p, n, speed) {
    n = Math.round(n * (Q().label === 'Bas' ? 0.5 : 1));
    for (let i = 0; i < n; i++) {
      const a = Math.random() * 6.28, up = rand(0.4, 1);
      const s = rand(0.4, 1.6);
      this.rockPool.spawn(p.x + rand(-1, 1), p.y + 0.5, p.z + rand(-1, 1), s, s * rand(0.6, 1), s, Math.cos(a) * speed * rand(0.3, 1), speed * up, Math.sin(a) * speed * rand(0.3, 1));
    }
  }

  blast(x, z, r, power) { for (const p of this.pools.values()) p.blast(x, z, r, power); this.rockPool.blast(x, z, r, power); }

  update(dt) { for (const p of this.pools.values()) p.update(dt); this.rockPool.update(dt); }
}
