// Particules : fumée (opaque douce) et flammes / étincelles (lumineuses).
import * as THREE from 'three';

const VERT = `attribute vec4 aCol; attribute float aSize; attribute float aSeed; varying vec4 vCol; varying float vSeed; uniform float scale;
  void main(){ vCol = aCol; vSeed = aSeed; vec4 mv = modelViewMatrix * vec4(position, 1.0); gl_PointSize = aSize * scale / max(0.1, -mv.z); gl_Position = projectionMatrix * mv; }`;
const FRAG = `varying vec4 vCol; void main(){ vec2 d = gl_PointCoord - 0.5; float a = smoothstep(0.5, 0.08, length(d)); if (a < 0.01) discard; gl_FragColor = vec4(vCol.rgb, vCol.a * a);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

// Fumée : chaque bouffée est un nuage au contour déchiqueté (bruit propre à la particule, qui tourne lentement), éclairé par le haut
// et plus sombre au cœur, pour donner du volume au lieu d'un simple disque flou.
const FRAG_SMOKE = `varying vec4 vCol; varying float vSeed; uniform float time; uniform vec3 lum;
  float h(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
  float n(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); return mix(mix(h(i),h(i+vec2(1,0)),f.x), mix(h(i+vec2(0,1)),h(i+vec2(1,1)),f.x), f.y); }
  void main(){ vec2 d = gl_PointCoord - 0.5; float r = length(d) * 2.0; if (r > 1.0) discard;
    float a0 = vSeed * 6.283 + time * (vSeed - 0.5) * 0.9, c = cos(a0), s = sin(a0); vec2 q = vec2(d.x*c - d.y*s, d.x*s + d.y*c);
    float f = n(q*4.0 + vSeed*37.0) * 0.6 + n(q*9.0 - vSeed*11.0 + time*0.25) * 0.4;
    float a = smoothstep(1.0, 0.15, r + (f - 0.5) * 0.9);
    if (a < 0.01) discard;
    float top = clamp(0.5 - d.y * 1.5, 0.0, 1.0);                       // le haut de la bouffée reçoit la lumière
    vec3 col = vCol.rgb * (0.62 + 0.5 * top) * (0.8 + 0.4 * f) + lum * top * 0.25 * a;
    gl_FragColor = vec4(col, vCol.a * a);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
  }`;

class Pool {
  constructor(n, additive, frag = FRAG, die = false) {
    this.n = n; this.i = 0;
    this.p = new Float32Array(n * 3); this.c = new Float32Array(n * 4); this.s = new Float32Array(n);
    this.v = new Float32Array(n * 3); this.life = new Float32Array(n); this.max = new Float32Array(n);
    this.die = die; this.seed = new Float32Array(n); for (let i = 0; i < n; i++) this.seed[i] = Math.random();
    this.s0 = new Float32Array(n); this.s1 = new Float32Array(n); this.a0 = new Float32Array(n); this.grav = new Float32Array(n); this.drag = new Float32Array(n);
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(this.p, 3).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('aCol', new THREE.BufferAttribute(this.c, 4).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('aSize', new THREE.BufferAttribute(this.s, 1).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('aSeed', new THREE.BufferAttribute(this.seed, 1));
    this.mat = new THREE.ShaderMaterial({ vertexShader: VERT, fragmentShader: frag, uniforms: { scale: { value: 600 }, time: { value: 0 }, lum: { value: new THREE.Color(0.5, 0.5, 0.5) } }, transparent: true, depthWrite: false, blending: additive ? THREE.AdditiveBlending : THREE.NormalBlending });
    this.points = new THREE.Points(g, this.mat); this.points.frustumCulled = false; this.points.layers.set(1); this.points.renderOrder = additive ? 9 : 8;
  }
  spawn(x, y, z, vx, vy, vz, life, s0, s1, r, g, b, a, grav = 0, drag = 1) {
    y += OFF;
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
      if (this.p[i * 3 + 1] < 0.03) { this.p[i * 3 + 1] = 0.03; this.v[i * 3 + 1] *= -0.4; if (this.die) this.life[i] = Math.min(this.life[i], 0.06); }
      this.s[i] = this.s0[i] + (this.s1[i] - this.s0[i]) * t;
      this.c[i * 4 + 3] = this.a0[i] * (1 - t) * (1 - t);
    }
    const a = this.points.geometry.attributes; a.position.needsUpdate = true; a.aCol.needsUpdate = true; a.aSize.needsUpdate = true;
  }
  clear() { this.life.fill(0); }
}

// monde ouvert : hauteur du sol sous la voiture qui émet (les effets naissent au bon niveau sur les collines)
let OFF = 0;
export class FX {
  constructor(scene) {
    this.smokeP = new Pool(1100, false, FRAG_SMOKE); this.fireP = new Pool(420, true); this.dropP = new Pool(900, false, FRAG, true);
    this.dropP.points.renderOrder = 8;
    scene.add(this.smokeP.points, this.fireP.points, this.dropP.points);
    this.smokeTint = [0.75, 0.77, 0.82]; this.t = 0;
  }
  setScale(h) { for (const p of [this.smokeP, this.fireP, this.dropP]) p.mat.uniforms.scale.value = h * 0.9; }
  // lumière ambiante : la fumée est claire de jour, sombre et teintée de nuit
  setTint(r, g, b) { this.smokeTint = [r, g, b]; this.smokeP.mat.uniforms.lum.value.setRGB(r, g, b); }
  smoke(x, y, z, vx, vy, vz, size = 1, alpha = 0.3, life = 0.9) {
    const t = this.smokeTint, k = 0.85 + Math.random() * 0.3;
    this.smokeP.spawn(x, y, z, vx + (Math.random() - 0.5) * 1.2, vy + Math.random() * 0.8, vz + (Math.random() - 0.5) * 1.2, life * (0.7 + Math.random() * 0.6), 0.5 * size, 3.2 * size, t[0] * k, t[1] * k, t[2] * k, alpha, -0.6, 1.6);
  }
  // fumée de pneu : dense et blanche à la roue, elle s'étale, monte lentement et met longtemps à se dissiper
  tyre(x, y, z, vx, vy, vz, size = 1, alpha = 0.5) {
    const t = this.smokeTint, k = 0.95 + Math.random() * 0.25;
    this.smokeP.spawn(x, y, z, vx + (Math.random() - 0.5) * 1.6, vy + 0.4 + Math.random() * 0.9, vz + (Math.random() - 0.5) * 1.6, 1.5 + Math.random() * 1.3, 0.55 * size, 5.2 * size, Math.min(1, t[0] * 1.15) * k, Math.min(1, t[1] * 1.15) * k, Math.min(1, t[2] * 1.15) * k, alpha, -0.45, 1.25);
  }
  // gaz d'échappement : petit panache gris qui sort du pot, plus sombre quand le moteur est en charge
  exhaust(x, y, z, vx, vy, vz, dark = 0, size = 1) {
    const t = this.smokeTint, k = (0.75 - 0.5 * dark) * (0.85 + Math.random() * 0.3);
    this.smokeP.spawn(x, y, z, vx + (Math.random() - 0.5) * 0.5, vy + 0.25 + Math.random() * 0.4, vz + (Math.random() - 0.5) * 0.5, 0.7 + Math.random() * 0.7, 0.16 * size, 1.5 * size, t[0] * k, t[1] * k, t[2] * k, 0.2 + 0.16 * dark, -0.5, 1.9);
  }
  // poussière, sable ou neige soulevés par les roues, de la couleur du sol
  dirt(x, y, z, vx, vy, vz, size, r, g, b, alpha = 0.3) {
    const k = 0.85 + Math.random() * 0.3;
    this.smokeP.spawn(x, y, z, vx + (Math.random() - 0.5) * 2.2, vy + 0.5 + Math.random() * 1.6, vz + (Math.random() - 0.5) * 2.2, 0.9 + Math.random() * 1.1, 0.4 * size, 3.6 * size, r * k, g * k, b * k, alpha, -0.15, 1.5);
  }
  // mottes, gravillons, brins de paille : petits éclats qui retombent
  bits(x, y, z, vx, vy, vz, r, g, b, size = 0.12, grav = 16) {
    const k = 0.75 + Math.random() * 0.5;
    this.dropP.spawn(x, y, z, vx + (Math.random() - 0.5) * 5, vy + Math.random() * 4, vz + (Math.random() - 0.5) * 5, 0.6 + Math.random() * 0.8, size, size * 0.7, r * k, g * k, b * k, 1, grav, 0.9);
  }
  // feuille morte qui descend en tournoyant
  leaf(x, y, z, r, g, b) {
    this.dropP.spawn(x, y, z, 2.5 + (Math.random() - 0.5) * 3, -0.4, (Math.random() - 0.5) * 3, 3 + Math.random() * 2, 0.16, 0.14, r, g, b, 1.6, 0.9, 0.5);
  }
  flame(x, y, z, vx, vy, vz, size = 1, blue = 0) {
    const h = Math.random();
    this.fireP.spawn(x, y, z, vx + (Math.random() - 0.5) * 1.5, vy + (Math.random() - 0.5) * 1.5, vz + (Math.random() - 0.5) * 1.5, 0.1 + Math.random() * 0.14, 0.55 * size, 0.1 * size,
      blue ? 0.3 + h * 0.5 : 2.0, blue ? 0.9 + h * 0.6 : 0.55 + h * 0.7, blue ? 2.6 : 0.12, 0.75, 0, 3);
  }
  spark(x, y, z, vx, vy, vz) {
    this.fireP.spawn(x, y, z, vx + (Math.random() - 0.5) * 9, vy + Math.random() * 5, vz + (Math.random() - 0.5) * 9, 0.3 + Math.random() * 0.4, 0.22, 0.05, 4.0, 2.4, 0.6, 1, 18, 0.6);
  }
  // étincelle colorée (dérapage : bleu, orange puis violet selon la durée)
  sparkC(x, y, z, vx, vy, vz, r, g, b) {
    this.fireP.spawn(x, y, z, vx + (Math.random() - 0.5) * 7, vy + Math.random() * 4.5, vz + (Math.random() - 0.5) * 7, 0.25 + Math.random() * 0.35, 0.26, 0.04, r, g, b, 1, 16, 0.6);
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
  // gerbe d'eau soulevée par un pneu sur route mouillée : brume fine + gouttes qui retombent
  splash(x, y, z, vx, vz, amount = 1) {
    const k = 0.7 + Math.random() * 0.3, t = this.smokeTint, w = 0.55 + 0.45 * Math.max(t[0], t[1], t[2]);
    this.smokeP.spawn(x, y, z, vx * 0.35 + (Math.random() - 0.5) * 2.5, 1.2 + Math.random() * 2.2 * amount, vz * 0.35 + (Math.random() - 0.5) * 2.5, 0.45 + Math.random() * 0.5,
      0.3, 2.3 * amount, 0.82 * k * w, 0.86 * k * w, 0.94 * k * w, 0.26 * amount, 3, 2.0);
    if (Math.random() < 0.7 * amount) this.drop(x, y, z, vx * 0.3 + (Math.random() - 0.5) * 3, 1.5 + Math.random() * 3 * amount, vz * 0.3 + (Math.random() - 0.5) * 3);
  }
  // une goutte d'eau : petit point clair, lourd, qui disparaît en touchant le sol
  drop(x, y, z, vx, vy, vz, size = 0.1) {
    const t = this.smokeTint, w = 0.5 + 0.6 * Math.max(t[0], t[1], t[2]), k = 0.8 + Math.random() * 0.4;
    this.dropP.spawn(x, y, z, vx, vy, vz, 0.5 + Math.random() * 0.7, size * (0.7 + Math.random() * 0.7), size * 0.5, 0.8 * w * k, 0.9 * w * k, 1.0 * w * k, 0.85, 19, 0.35);
  }
  // traversée d'une flaque : deux murs d'eau projetés sur les côtés, une couronne de gouttes et un nuage d'embruns
  plouf(x, z, vx, vz, power = 1, half = 1) {
    const sp = Math.hypot(vx, vz) || 1, fx_ = vx / sp, fz_ = vz / sp, n = Math.round(10 + 22 * power);
    for (let i = 0; i < n; i++) {
      const s = i % 2 ? 1 : -1, out = (3 + Math.random() * 7) * power, up = (2.5 + Math.random() * 6.5) * power, along = sp * (0.25 + Math.random() * 0.35);
      this.drop(x - fz_ * s * half + (Math.random() - 0.5), 0.1, z + fx_ * s * half + (Math.random() - 0.5), -fz_ * s * out + fx_ * along, up, fx_ * s * out + fz_ * along, 0.09 + Math.random() * 0.12 * power);
    }
    const t = this.smokeTint, w = 0.55 + 0.45 * Math.max(t[0], t[1], t[2]);
    for (let i = 0; i < 5 + 7 * power; i++) { const s = i % 2 ? 1 : -1, k = 0.8 + Math.random() * 0.2;
      this.smokeP.spawn(x - fz_ * s * half, 0.2, z + fx_ * s * half, -fz_ * s * (2 + Math.random() * 4) + vx * 0.3, 1.5 + Math.random() * 3.5 * power, fx_ * s * (2 + Math.random() * 4) + vz * 0.3, 0.7 + Math.random() * 0.7, 0.6, 4.2 * power, 0.85 * k * w, 0.9 * k * w, 0.97 * k * w, 0.34, 2.2, 1.6); }
  }
  setGround(h) { OFF = h; }
  update(dt) { this.t += dt; this.smokeP.mat.uniforms.time.value = this.t; this.smokeP.update(dt); this.fireP.update(dt); this.dropP.update(dt); }
  clear() { this.smokeP.clear(); this.fireP.clear(); this.dropP.clear(); }
}

// Traces de pneus laissées au sol pendant les dérapages et les gros freinages (anneau de rectangles réutilisés).
export class Skids {
  constructor(scene, n = 3200) {
    this.n = n; this.i = 0; this.p = new Float32Array(n * 12);
    const idx = new Uint32Array(n * 6); for (let q = 0; q < n; q++) idx.set([q * 4, q * 4 + 1, q * 4 + 2, q * 4 + 1, q * 4 + 3, q * 4 + 2], q * 6);
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(this.p, 3).setUsage(THREE.DynamicDrawUsage)); g.setIndex(new THREE.BufferAttribute(idx, 1));
    this.mesh = new THREE.Mesh(g, new THREE.MeshBasicMaterial({ color: 0x050506, transparent: true, opacity: 0.5, depthWrite: false, side: THREE.DoubleSide }));
    this.mesh.frustumCulled = false; this.mesh.layers.set(1); this.mesh.renderOrder = 2; scene.add(this.mesh);
  }
  // prolonge la trace d'une roue : tr = { x, z } du point précédent (ou null), renvoie le nouveau point
  mark(tr, x, z, w = 0.24) {
    if (tr) { const dx = x - tr.x, dz = z - tr.z, l = Math.hypot(dx, dz);
      if (l > 0.25 && l < 6) { const nx = -dz / l * w / 2, nz = dx / l * w / 2, k = this.i * 12, p = this.p; this.i = (this.i + 1) % this.n;
        p[k] = tr.x - nx; p[k + 1] = 0.022 + OFF; p[k + 2] = tr.z - nz; p[k + 3] = tr.x + nx; p[k + 4] = 0.022 + OFF; p[k + 5] = tr.z + nz;
        p[k + 6] = x - nx; p[k + 7] = 0.022 + OFF; p[k + 8] = z - nz; p[k + 9] = x + nx; p[k + 10] = 0.022 + OFF; p[k + 11] = z + nz;
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
