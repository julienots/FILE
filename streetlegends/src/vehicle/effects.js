// Effets visuels : particules (fumée, poussière, étincelles, flammes de nitro, embruns), traces de pneus.
import * as THREE from 'three';
import { glowTex } from './carBuilder.js';

export class Particles {
  constructor(scene, max) {
    this.max = max; this.n = 0; this.cursor = 0;
    this.p = new Float32Array(max * 3); this.v = new Float32Array(max * 3); this.c = new Float32Array(max * 4);
    this.s = new Float32Array(max); this.life = new Float32Array(max); this.ttl = new Float32Array(max); this.grow = new Float32Array(max); this.grav = new Float32Array(max); this.a0 = new Float32Array(max);
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(this.p, 3).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('pcolor', new THREE.BufferAttribute(this.c, 4).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('psize', new THREE.BufferAttribute(this.s, 1).setUsage(THREE.DynamicDrawUsage));
    const mk = (blend) => new THREE.ShaderMaterial({
      uniforms: { map: { value: glowTex() }, scale: { value: 600 }, fogColor: { value: new THREE.Color() }, fogFar: { value: 900 } },
      transparent: true, depthWrite: false, blending: blend,
      vertexShader: 'attribute vec4 pcolor; attribute float psize; varying vec4 vC; uniform float scale; uniform float fogFar; void main(){ vec4 mv = modelViewMatrix * vec4(position, 1.0); vC = pcolor; vC.a *= 1.0 - smoothstep(fogFar * 0.5, fogFar, -mv.z); gl_PointSize = psize * scale / max(0.5, -mv.z); gl_Position = projectionMatrix * mv; }',
      fragmentShader: 'uniform sampler2D map; varying vec4 vC; void main(){ float a = texture2D(map, gl_PointCoord).r; gl_FragColor = vec4(vC.rgb, vC.a * a); if (gl_FragColor.a < 0.01) discard; }',
    });
    this.mat = mk(THREE.NormalBlending);
    this.points = new THREE.Points(g, this.mat);
    this.points.frustumCulled = false; this.points.renderOrder = 4;
    scene.add(this.points);
    this.geo = g;
  }
  emit(x, y, z, vx, vy, vz, r, gcol, b, alpha, size, ttl, grow = 1, grav = 0) {
    const i = this.cursor;
    this.cursor = (this.cursor + 1) % this.max;
    this.p[i * 3] = x; this.p[i * 3 + 1] = y; this.p[i * 3 + 2] = z;
    this.v[i * 3] = vx; this.v[i * 3 + 1] = vy; this.v[i * 3 + 2] = vz;
    this.c[i * 4] = r; this.c[i * 4 + 1] = gcol; this.c[i * 4 + 2] = b; this.c[i * 4 + 3] = alpha;
    this.s[i] = size; this.life[i] = ttl; this.ttl[i] = ttl; this.grow[i] = grow; this.grav[i] = grav; this.a0[i] = alpha;
  }
  smoke(x, y, z, vx, vz, k = 1, dark = 0) { const c = 0.9 - dark * 0.5; this.emit(x + (Math.random() - 0.5) * 0.3, y + 0.15, z + (Math.random() - 0.5) * 0.3, vx * 0.25 + (Math.random() - 0.5), 0.6 + Math.random() * 0.8, vz * 0.25 + (Math.random() - 0.5), c, c, c + 0.03, 0.34 * k, 0.9, 1.1 + Math.random() * 0.7, 3.2, -0.2); }
  dust(x, y, z, vx, vz, sand) { const r = sand ? 0.86 : 0.55, g = sand ? 0.76 : 0.42, b = sand ? 0.55 : 0.28; this.emit(x, y + 0.15, z, vx * 0.3 + (Math.random() - 0.5) * 1.5, 0.8 + Math.random(), vz * 0.3 + (Math.random() - 0.5) * 1.5, r, g, b, 0.42, 1.0, 1.2 + Math.random() * 0.6, 3.4, -0.1); }
  spark(x, y, z, vx, vz) { for (let i = 0; i < 6; i++) this.emit(x, y + 0.3 + Math.random() * 0.4, z, vx * 0.4 + (Math.random() - 0.5) * 9, 1 + Math.random() * 4, vz * 0.4 + (Math.random() - 0.5) * 9, 1, 0.7 + Math.random() * 0.3, 0.25, 1, 0.16, 0.3 + Math.random() * 0.35, 0.6, 12); }
  flame(x, y, z, vx, vz) { const hot = Math.random(); this.emit(x, y, z, vx + (Math.random() - 0.5), (Math.random() - 0.3), vz + (Math.random() - 0.5), 0.3 + hot * 0.5, 0.6 + hot * 0.3, 1, 0.9, 0.42, 0.12 + Math.random() * 0.1, 0.5, 0); }
  spray(x, y, z, vx, vz) { this.emit(x, y + 0.1, z, vx * 0.35 + (Math.random() - 0.5) * 1.5, 1 + Math.random() * 1.5, vz * 0.35 + (Math.random() - 0.5) * 1.5, 0.85, 0.9, 0.95, 0.22, 0.7, 0.6, 2.6, 3); }
  debris(x, y, z, r, g, b) { for (let i = 0; i < 8; i++) this.emit(x, y + 0.5, z, (Math.random() - 0.5) * 10, 2 + Math.random() * 5, (Math.random() - 0.5) * 10, r, g, b, 1, 0.3, 0.8, 0.7, 14); }
  update(dt, cam, fog, h) {
    const { p, v, c, s, life, ttl, grow, grav, a0, max } = this;
    for (let i = 0; i < max; i++) {
      if (life[i] <= 0) continue;
      life[i] -= dt;
      if (life[i] <= 0) { c[i * 4 + 3] = 0; s[i] = 0; continue; }
      v[i * 3 + 1] -= grav[i] * dt;
      p[i * 3] += v[i * 3] * dt; p[i * 3 + 1] += v[i * 3 + 1] * dt; p[i * 3 + 2] += v[i * 3 + 2] * dt;
      const k = 1 - 1.8 * dt; v[i * 3] *= k; v[i * 3 + 2] *= k;
      s[i] += grow[i] * dt;
      const f = life[i] / ttl[i];
      c[i * 4 + 3] = a0[i] * Math.min(1, f * 1.6);
    }
    this.geo.attributes.position.needsUpdate = true; this.geo.attributes.pcolor.needsUpdate = true; this.geo.attributes.psize.needsUpdate = true;
    this.mat.uniforms.scale.value = h / (2 * Math.tan((cam.fov * Math.PI) / 360));
    this.mat.uniforms.fogFar.value = fog.far;
  }
}

export class SkidMarks {
  constructor(scene, max) {
    this.max = max; this.i = 0;
    this.pos = new Float32Array(max * 18); this.col = new Float32Array(max * 24);
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(this.pos, 3).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('color', new THREE.BufferAttribute(this.col, 4).setUsage(THREE.DynamicDrawUsage));
    this.mesh = new THREE.Mesh(g, new THREE.MeshBasicMaterial({ vertexColors: true, transparent: true, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -4, polygonOffsetUnits: -4 }));
    this.mesh.frustumCulled = false; this.mesh.renderOrder = 1;
    scene.add(this.mesh);
    this.geo = g; this.last = new Map(); this.dirty = false;
  }
  // key = identifiant de roue ; (x,y,z) point de contact ; (nx,nz) normale latérale ; a = intensité
  add(key, x, y, z, nx, nz, w, a, dark = 0.1) {
    const L = this.last.get(key);
    const cur = [x + nx * w, y + 0.06, z + nz * w, x - nx * w, y + 0.06, z - nz * w, a];
    this.last.set(key, cur);
    if (!L || L[6] <= 0 || a <= 0) return;
    if (Math.hypot(cur[0] - L[0], cur[2] - L[2]) > 6) return;
    const o = this.i * 18, c = this.i * 24;
    this.i = (this.i + 1) % this.max;
    this.pos.set([L[0], L[1], L[2], L[3], L[4], L[5], cur[3], cur[4], cur[5], L[0], L[1], L[2], cur[3], cur[4], cur[5], cur[0], cur[1], cur[2]], o);
    const A = [L[6], L[6], a, L[6], a, a];
    for (let k = 0; k < 6; k++) { this.col[c + k * 4] = dark; this.col[c + k * 4 + 1] = dark; this.col[c + k * 4 + 2] = dark; this.col[c + k * 4 + 3] = A[k] * 0.72; }
    this.dirty = true;
  }
  stop(key) { const L = this.last.get(key); if (L) L[6] = 0; }
  flush() { if (this.dirty) { this.geo.attributes.position.needsUpdate = true; this.geo.attributes.color.needsUpdate = true; this.dirty = false; } }
  clear() { this.pos.fill(0); this.col.fill(0); this.last.clear(); this.dirty = true; }
}
