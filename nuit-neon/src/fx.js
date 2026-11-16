// Particules : fumée (opaque douce) et flammes / étincelles (lumineuses).
import * as THREE from 'three';

const VERT = `attribute vec4 aCol; attribute float aSize; varying vec4 vCol; uniform float scale;
  void main(){ vCol = aCol; vec4 mv = modelViewMatrix * vec4(position, 1.0); gl_PointSize = aSize * scale / max(0.1, -mv.z); gl_Position = projectionMatrix * mv; }`;
const FRAG = `varying vec4 vCol; void main(){ vec2 d = gl_PointCoord - 0.5; float a = smoothstep(0.5, 0.08, length(d)); if (a < 0.01) discard; gl_FragColor = vec4(vCol.rgb, vCol.a * a);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

class Pool {
  constructor(n, additive) {
    this.n = n; this.i = 0;
    this.p = new Float32Array(n * 3); this.c = new Float32Array(n * 4); this.s = new Float32Array(n);
    this.v = new Float32Array(n * 3); this.life = new Float32Array(n); this.max = new Float32Array(n);
    this.s0 = new Float32Array(n); this.s1 = new Float32Array(n); this.a0 = new Float32Array(n); this.grav = new Float32Array(n); this.drag = new Float32Array(n);
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(this.p, 3).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('aCol', new THREE.BufferAttribute(this.c, 4).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('aSize', new THREE.BufferAttribute(this.s, 1).setUsage(THREE.DynamicDrawUsage));
    this.mat = new THREE.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG, uniforms: { scale: { value: 600 } }, transparent: true, depthWrite: false, blending: additive ? THREE.AdditiveBlending : THREE.NormalBlending });
    this.points = new THREE.Points(g, this.mat); this.points.frustumCulled = false; this.points.layers.set(1); this.points.renderOrder = additive ? 9 : 8;
  }
  spawn(x, y, z, vx, vy, vz, life, s0, s1, r, g, b, a, grav = 0, drag = 1) {
    const i = this.i; this.i = (i + 1) % this.n;
    this.p[i * 3] = x; this.p[i * 3 + 1] = y; this.p[i * 3 + 2] = z; this.v[i * 3] = vx; this.v[i * 3 + 1] = vy; this.v[i * 3 + 2] = vz;
    this.life[i] = life; this.max[i] = life; this.s0[i] = s0; this.s1[i] = s1; this.a0[i] = a; this.grav[i] = grav; this.drag[i] = drag;
    this.c[i * 4] = r; this.c[i * 4 + 1] = g; this.c[i * 4 + 2] = b; this.c[i * 4 + 3] = a;
  }
  update(dt) {
    for (let i = 0; i < this.n; i++) {
      if (this.life[i] <= 0) { this.s[i] = 0; continue; }
      this.life[i] -= dt;
      const t = 1 - Math.max(0, this.life[i]) / this.max[i], k = Math.exp(-this.drag[i] * dt);
      this.v[i * 3] *= k; this.v[i * 3 + 2] *= k; this.v[i * 3 + 1] = this.v[i * 3 + 1] * k - this.grav[i] * dt;
      this.p[i * 3] += this.v[i * 3] * dt; this.p[i * 3 + 1] += this.v[i * 3 + 1] * dt; this.p[i * 3 + 2] += this.v[i * 3 + 2] * dt;
      if (this.p[i * 3 + 1] < 0.03) { this.p[i * 3 + 1] = 0.03; this.v[i * 3 + 1] *= -0.4; }
      this.s[i] = this.s0[i] + (this.s1[i] - this.s0[i]) * t;
      this.c[i * 4 + 3] = this.a0[i] * (1 - t) * (1 - t);
    }
    const a = this.points.geometry.attributes; a.position.needsUpdate = true; a.aCol.needsUpdate = true; a.aSize.needsUpdate = true;
  }
  clear() { this.life.fill(0); }
}

export class FX {
  constructor(scene) {
    this.smokeP = new Pool(720, false); this.fireP = new Pool(420, true);
    scene.add(this.smokeP.points, this.fireP.points);
    this.smokeTint = [0.75, 0.77, 0.82];
  }
  setScale(h) { this.smokeP.mat.uniforms.scale.value = h * 0.9; this.fireP.mat.uniforms.scale.value = h * 0.9; }
  // lumière ambiante : la fumée est claire de jour, sombre et teintée de nuit
  setTint(r, g, b) { this.smokeTint = [r, g, b]; }
  smoke(x, y, z, vx, vy, vz, size = 1, alpha = 0.3, life = 0.9) {
    const t = this.smokeTint, k = 0.85 + Math.random() * 0.3;
    this.smokeP.spawn(x, y, z, vx + (Math.random() - 0.5) * 1.2, vy + Math.random() * 0.8, vz + (Math.random() - 0.5) * 1.2, life * (0.7 + Math.random() * 0.6), 0.5 * size, 3.2 * size, t[0] * k, t[1] * k, t[2] * k, alpha, -0.6, 1.6);
  }
  flame(x, y, z, vx, vy, vz, size = 1, blue = 0) {
    const h = Math.random();
    this.fireP.spawn(x, y, z, vx + (Math.random() - 0.5) * 1.5, vy + (Math.random() - 0.5) * 1.5, vz + (Math.random() - 0.5) * 1.5, 0.1 + Math.random() * 0.14, 0.55 * size, 0.1 * size,
      blue ? 0.3 + h * 0.5 : 2.0, blue ? 0.9 + h * 0.6 : 0.55 + h * 0.7, blue ? 2.6 : 0.12, 0.75, 0, 3);
  }
  spark(x, y, z, vx, vy, vz) {
    this.fireP.spawn(x, y, z, vx + (Math.random() - 0.5) * 9, vy + Math.random() * 5, vz + (Math.random() - 0.5) * 9, 0.3 + Math.random() * 0.4, 0.22, 0.05, 4.0, 2.4, 0.6, 1, 18, 0.6);
  }
  ember(x, y, z, vx, vy, vz) {
    const h = Math.random();
    this.fireP.spawn(x, y, z, vx + (Math.random() - 0.5) * 5, vy + Math.random() * 3.5, vz + (Math.random() - 0.5) * 5, 0.5 + Math.random() * 0.7, 0.3, 0.04, 4.5, 1.6 + h * 1.6, 0.25 + h * 0.5, 1, 14, 0.5);
  }
  dust(x, y, z, vx, vy, vz, size = 1) {
    this.smokeP.spawn(x, y, z, vx + (Math.random() - 0.5) * 2, vy + Math.random(), vz + (Math.random() - 0.5) * 2, 0.5 + Math.random() * 0.5, 0.3 * size, 1.8 * size, 0.55, 0.52, 0.5, 0.28, -0.3, 2.2);
  }
  burst(x, y, z, r, g, b, n = 60, speed = 22) {
    for (let i = 0; i < n; i++) {
      const a = Math.random() * 6.283, e = Math.random() * 1.5 - 0.3, s = speed * (0.3 + Math.random());
      this.fireP.spawn(x, y, z, Math.cos(a) * Math.cos(e) * s, Math.sin(e) * s, Math.sin(a) * Math.cos(e) * s, 0.5 + Math.random() * 0.7, 1.4, 0.2, r, g, b, 1, 9, 1.2);
    }
  }
  // gerbe d'eau soulevée par un pneu sur route mouillée
  splash(x, y, z, vx, vz, amount = 1) {
    const k = 0.7 + Math.random() * 0.3;
    this.smokeP.spawn(x, y, z, vx * 0.35 + (Math.random() - 0.5) * 2.5, 1.4 + Math.random() * 2.6 * amount, vz * 0.35 + (Math.random() - 0.5) * 2.5, 0.35 + Math.random() * 0.35,
      0.25, 1.5 * amount, 0.82 * k, 0.86 * k, 0.94 * k, 0.32 * amount, 7, 2.2);
  }
  update(dt) { this.smokeP.update(dt); this.fireP.update(dt); }
  clear() { this.smokeP.clear(); this.fireP.clear(); }
}

// Traces de pneus laissées au sol pendant les dérapages et les gros freinages (anneau de rectangles réutilisés).
export class Skids {
  constructor(scene, n = 900) {
    this.n = n; this.i = 0; this.p = new Float32Array(n * 12);
    const idx = new Uint16Array(n * 6); for (let q = 0; q < n; q++) idx.set([q * 4, q * 4 + 1, q * 4 + 2, q * 4 + 1, q * 4 + 3, q * 4 + 2], q * 6);
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(this.p, 3).setUsage(THREE.DynamicDrawUsage)); g.setIndex(new THREE.BufferAttribute(idx, 1));
    this.mesh = new THREE.Mesh(g, new THREE.MeshBasicMaterial({ color: 0x050506, transparent: true, opacity: 0.5, depthWrite: false, side: THREE.DoubleSide }));
    this.mesh.frustumCulled = false; this.mesh.layers.set(1); this.mesh.renderOrder = 2; scene.add(this.mesh);
  }
  // prolonge la trace d'une roue : tr = { x, z } du point précédent (ou null), renvoie le nouveau point
  mark(tr, x, z, w = 0.24) {
    if (tr) { const dx = x - tr.x, dz = z - tr.z, l = Math.hypot(dx, dz);
      if (l > 0.25 && l < 6) { const nx = -dz / l * w / 2, nz = dx / l * w / 2, k = this.i * 12, p = this.p; this.i = (this.i + 1) % this.n;
        p[k] = tr.x - nx; p[k + 1] = 0.022; p[k + 2] = tr.z - nz; p[k + 3] = tr.x + nx; p[k + 4] = 0.022; p[k + 5] = tr.z + nz;
        p[k + 6] = x - nx; p[k + 7] = 0.022; p[k + 8] = z - nz; p[k + 9] = x + nx; p[k + 10] = 0.022; p[k + 11] = z + nz;
        this.mesh.geometry.attributes.position.needsUpdate = true; }
      else if (l <= 0.25) return tr; }
    return { x, z };
  }
  // rayure laissée sur un mur : bande verticale entre le point précédent et le point courant
  markWall(tr, x, z, y0 = 0.28, y1 = 0.62) {
    if (tr) { const l = Math.hypot(x - tr.x, z - tr.z);
      if (l > 0.3 && l < 8) { const k = this.i * 12, p = this.p; this.i = (this.i + 1) % this.n;
        p[k] = tr.x; p[k + 1] = y0; p[k + 2] = tr.z; p[k + 3] = tr.x; p[k + 4] = y1; p[k + 5] = tr.z; p[k + 6] = x; p[k + 7] = y0 + (Math.random() - 0.5) * 0.06; p[k + 8] = z; p[k + 9] = x; p[k + 10] = y1 + (Math.random() - 0.5) * 0.06; p[k + 11] = z;
        this.mesh.geometry.attributes.position.needsUpdate = true; }
      else if (l <= 0.3) return tr; }
    return { x, z };
  }
  clear() { this.p.fill(0); this.mesh.geometry.attributes.position.needsUpdate = true; }
}
