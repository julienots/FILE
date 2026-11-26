// Effets visuels à budget fixe (aucune allocation pendant le jeu) : particules, ondes de choc,
// sphères d'énergie, éclairs, marques au sol, lumières d'impact, tremblement de caméra.
import * as THREE from 'three';
import { G, Q, canvasTexture, rand, clamp, lerp, haptic } from './core.js';

// ---------- Système de particules (un seul appel de dessin par mode de mélange) ----------
class Particles {
  constructor(scene, max, additive, tex) {
    this.max = max;
    this.pos = new Float32Array(max * 3); this.vel = new Float32Array(max * 3);
    this.col = new Float32Array(max * 4); this.size = new Float32Array(max);
    this.life = new Float32Array(max); this.maxLife = new Float32Array(max);
    this.grow = new Float32Array(max); this.grav = new Float32Array(max); this.drag = new Float32Array(max);
    this.a0 = new Float32Array(max);
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(this.pos, 3).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('color', new THREE.BufferAttribute(this.col, 4).setUsage(THREE.DynamicDrawUsage));
    g.setAttribute('size', new THREE.BufferAttribute(this.size, 1).setUsage(THREE.DynamicDrawUsage));
    this.mat = new THREE.ShaderMaterial({
      transparent: true, depthWrite: false, blending: additive ? THREE.AdditiveBlending : THREE.NormalBlending,
      uniforms: { map: { value: tex }, scale: { value: 600 } },
      vertexShader: `attribute float size; attribute vec4 color; varying vec4 vC; varying float vNear; uniform float scale;
        void main(){ vC = color; vec4 mv = modelViewMatrix * vec4(position, 1.0); gl_Position = projectionMatrix * mv;
          vC.a *= smoothstep(1.0, 6.0, -mv.z); // s'efface près de la caméra
          gl_PointSize = min(size * scale / max(-mv.z, 0.1), 512.0); if (color.a <= 0.0) gl_PointSize = 0.0; }`,
      fragmentShader: `uniform sampler2D map; varying vec4 vC; varying float vNear;
        void main(){ vec4 t = texture2D(map, gl_PointCoord); gl_FragColor = vec4(vC.rgb * t.rgb, vC.a * t.a); if (gl_FragColor.a < 0.004) discard; }`,
    });
    this.points = new THREE.Points(g, this.mat);
    this.points.frustumCulled = false;
    this.points.renderOrder = additive ? 9 : 8;
    scene.add(this.points);
    this.next = 0; this.alive = 0;
  }
  emit(x, y, z, vx, vy, vz, size, life, r, g, b, a, grow = 1, grav = 0, drag = 0.5) {
    const i = this.next; this.next = (this.next + 1) % this.max;
    this.pos[i * 3] = x; this.pos[i * 3 + 1] = y; this.pos[i * 3 + 2] = z;
    this.vel[i * 3] = vx; this.vel[i * 3 + 1] = vy; this.vel[i * 3 + 2] = vz;
    this.col[i * 4] = r; this.col[i * 4 + 1] = g; this.col[i * 4 + 2] = b; this.col[i * 4 + 3] = a; this.a0[i] = a;
    this.size[i] = size; this.life[i] = life; this.maxLife[i] = life; this.grow[i] = grow; this.grav[i] = grav; this.drag[i] = drag;
  }
  update(dt, w) {
    const P = this.pos, V = this.vel, C = this.col, S = this.size, L = this.life;
    for (let i = 0; i < this.max; i++) {
      if (L[i] <= 0) { if (C[i * 4 + 3] !== 0) C[i * 4 + 3] = 0; continue; }
      L[i] -= dt;
      const k = Math.exp(-this.drag[i] * dt);
      V[i * 3] = V[i * 3] * k + w.x * dt * 0.3; V[i * 3 + 1] = V[i * 3 + 1] * k - this.grav[i] * dt; V[i * 3 + 2] = V[i * 3 + 2] * k + w.z * dt * 0.3;
      P[i * 3] += V[i * 3] * dt; P[i * 3 + 1] += V[i * 3 + 1] * dt; P[i * 3 + 2] += V[i * 3 + 2] * dt;
      S[i] *= 1 + (this.grow[i] - 1) * dt;
      const t = L[i] / this.maxLife[i];
      C[i * 4 + 3] = L[i] <= 0 ? 0 : this.a0[i] * Math.min(1, t * 2.5) * Math.min(1, (1 - t) * 8 + 0.2);
    }
    const g = this.points.geometry;
    g.attributes.position.needsUpdate = true; g.attributes.color.needsUpdate = true; g.attributes.size.needsUpdate = true;
  }
}

export class FX {
  constructor(scene) {
    this.scene = scene;
    const q = Q().particles;
    const soft = canvasTexture(64, 64, (ctx, w) => { const g = ctx.createRadialGradient(w / 2, w / 2, 0, w / 2, w / 2, w / 2); g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.4, 'rgba(255,255,255,0.45)'); g.addColorStop(1, 'rgba(255,255,255,0)'); ctx.fillStyle = g; ctx.fillRect(0, 0, w, w); });
    const puff = canvasTexture(128, 128, (ctx, w) => { for (let i = 0; i < 16; i++) { const x = w * (0.3 + Math.random() * 0.4), y = w * (0.3 + Math.random() * 0.4), r = w * (0.15 + Math.random() * 0.2); const g = ctx.createRadialGradient(x, y, 0, x, y, r); g.addColorStop(0, 'rgba(255,255,255,0.35)'); g.addColorStop(1, 'rgba(255,255,255,0)'); ctx.fillStyle = g; ctx.fillRect(0, 0, w, w); } });
    this.add = new Particles(scene, Math.round(1600 * q), true, soft);
    this.norm = new Particles(scene, Math.round(1400 * q), false, puff);
    this.softTex = soft;
    // ondes de choc (anneaux au sol)
    this.rings = [];
    const ringMat = () => new THREE.ShaderMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide,
      uniforms: { uA: { value: 0 }, uC: { value: new THREE.Color() } },
      vertexShader: 'varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
      fragmentShader: 'uniform float uA; uniform vec3 uC; varying vec2 vUv; void main(){ float r = length(vUv - 0.5) * 2.0; float a = smoothstep(0.72, 0.93, r) * smoothstep(1.0, 0.94, r) + smoothstep(0.6, 0.0, r) * 0.12; gl_FragColor = vec4(uC * a * uA, 1.0); }' });
    for (let i = 0; i < 8; i++) { const m = new THREE.Mesh(new THREE.PlaneGeometry(2, 2).rotateX(-Math.PI / 2), ringMat()); m.visible = false; m.renderOrder = 7; scene.add(m); this.rings.push({ m, t: 0, dur: 1, r0: 1, r1: 10 }); }
    // sphères d'énergie (explosions)
    this.spheres = [];
    for (let i = 0; i < 4; i++) {
      const m = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 3), new THREE.ShaderMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
        uniforms: { uA: { value: 0 }, uC: { value: new THREE.Color() } },
        vertexShader: 'varying float vF; void main(){ vec3 n = normalize(normalMatrix * normal); vec4 mv = modelViewMatrix * vec4(position, 1.0); vF = 1.0 - abs(dot(n, normalize(-mv.xyz))); gl_Position = projectionMatrix * mv; }',
        fragmentShader: 'uniform float uA; uniform vec3 uC; varying float vF; void main(){ gl_FragColor = vec4(uC * (0.25 + pow(vF, 2.0) * 1.6) * uA, 1.0); }' }));
      m.visible = false; m.renderOrder = 9; scene.add(m); this.spheres.push({ m, t: 0, dur: 1, r1: 10 });
    }
    // lumières d'impact (nombre fixe : aucune recompilation des shaders)
    this.lights = [];
    for (let i = 0; i < 2; i++) { const l = new THREE.PointLight(0xffaa55, 0, 120, 1.6); scene.add(l); this.lights.push({ l, t: 0, dur: 1, i0: 0 }); }
    // marques au sol (fissures, brûlures) qui épousent le terrain
    this.decals = [];
    const crackTex = canvasTexture(256, 256, (ctx, w) => {
      ctx.clearRect(0, 0, w, w); ctx.strokeStyle = 'rgba(20,14,8,0.85)'; ctx.lineCap = 'round';
      for (let i = 0; i < 11; i++) {
        let x = w / 2, y = w / 2, a = i / 11 * 6.28 + Math.random() * 0.4;
        ctx.lineWidth = 5; ctx.beginPath(); ctx.moveTo(x, y);
        for (let s = 0; s < 9; s++) { a += (Math.random() - 0.5) * 0.9; x += Math.cos(a) * 13; y += Math.sin(a) * 13; ctx.lineTo(x, y); ctx.lineWidth = Math.max(1, 5 - s * 0.5); }
        ctx.stroke();
      }
      const g = ctx.createRadialGradient(w / 2, w / 2, 0, w / 2, w / 2, w * 0.3); g.addColorStop(0, 'rgba(25,18,10,0.6)'); g.addColorStop(1, 'rgba(25,18,10,0)'); ctx.fillStyle = g; ctx.fillRect(0, 0, w, w);
    });
    const dmat = new THREE.MeshStandardMaterial({ map: crackTex, transparent: true, depthWrite: false, roughness: 1, polygonOffset: true, polygonOffsetFactor: -4 });
    for (let i = 0; i < 40; i++) { const m = new THREE.Mesh(new THREE.PlaneGeometry(1, 1, 6, 6), dmat); m.visible = false; m.frustumCulled = false; m.renderOrder = 3; scene.add(m); this.decals.push(m); }
    this.decalNext = 0;
    // éclairs
    this.bolts = [];
    const boltMat = new THREE.MeshBasicMaterial({ color: 0xe8f0ff, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false });
    for (let i = 0; i < 3; i++) { const m = new THREE.Mesh(new THREE.BufferGeometry(), boltMat.clone()); m.visible = false; m.frustumCulled = false; scene.add(m); this.bolts.push({ m, t: 0 }); }
    this.trauma = 0;
    this.wind = new THREE.Vector3(1.5, 0, 0.6);
  }

  // ---------- émetteurs ----------
  dust(p, radius = 4, n = 10, col = [0.55, 0.48, 0.38]) {
    n = Math.round(n * Q().particles);
    for (let i = 0; i < n; i++) {
      const a = Math.random() * 6.28, r = Math.random() * radius;
      this.norm.emit(p.x + Math.cos(a) * r, p.y + Math.random() * 1.5, p.z + Math.sin(a) * r, Math.cos(a) * rand(2, 6), rand(1, 5), Math.sin(a) * rand(2, 6),
        Math.min(9, rand(2, 4) * (radius / 6 + 0.6)), rand(2, 4.5), col[0], col[1], col[2], 0.3, 1.35, -0.3, 1.4);
    }
  }
  smoke(p, n = 1, size = 6, alpha = 0.4) {
    for (let i = 0; i < n; i++) this.norm.emit(p.x + rand(-1, 1), p.y, p.z + rand(-1, 1), rand(-0.5, 0.5), rand(2, 4), rand(-0.5, 0.5), size * rand(0.7, 1.2), rand(5, 9), 0.22, 0.21, 0.2, alpha, 1.25, -0.2, 0.3);
  }
  fire(p, size = 2) {
    this.add.emit(p.x + rand(-0.5, 0.5), p.y, p.z + rand(-0.5, 0.5), rand(-0.4, 0.4), rand(2, 4.5), rand(-0.4, 0.4), size * rand(0.8, 1.4), rand(0.5, 1.1), 1.0, rand(0.35, 0.6), 0.1, 0.9, 0.6, -1, 0.8);
  }
  sparks(p, n = 20, col = [1, 0.75, 0.3], speed = 12) {
    n = Math.round(n * Q().particles);
    for (let i = 0; i < n; i++) {
      const v = new THREE.Vector3(rand(-1, 1), rand(0.2, 1.2), rand(-1, 1)).normalize().multiplyScalar(rand(0.3, 1) * speed);
      this.add.emit(p.x, p.y, p.z, v.x, v.y, v.z, rand(0.25, 0.6), rand(0.4, 1.2), col[0], col[1], col[2], 1, 0.6, 12, 0.6);
    }
  }
  glow(p, size = 2, col = [1, 0.85, 0.5], life = 0.8) { this.add.emit(p.x, p.y, p.z, 0, 0.6, 0, size, life, col[0], col[1], col[2], 1, 1.3, 0, 1); }
  motes(p, r, n = 2) { for (let i = 0; i < n; i++) { const a = Math.random() * 6.28, d = rand(1, r); this.add.emit(p.x + Math.cos(a) * d, p.y + rand(0, 6), p.z + Math.sin(a) * d, 0, rand(0.3, 1), 0, rand(0.15, 0.35), rand(1.5, 3), 1, 0.86, 0.5, 0.9, 1, -0.1, 0.2); } }
  spirit(p) { for (let i = 0; i < 6; i++) this.add.emit(p.x, p.y + i * 0.3, p.z, rand(-0.2, 0.2), rand(2, 3.5), rand(-0.2, 0.2), rand(0.6, 1.2), rand(2.5, 4), 0.8, 0.9, 1, 0.8, 0.9, -0.3, 0.1); }
  splash(p, n = 16) { for (let i = 0; i < n; i++) { const a = Math.random() * 6.28; this.norm.emit(p.x, p.y, p.z, Math.cos(a) * rand(1, 4), rand(3, 8), Math.sin(a) * rand(1, 4), rand(0.6, 1.4), rand(0.6, 1.2), 0.85, 0.9, 0.95, 0.7, 1.3, 14, 0.3); } }
  rocks(p, n, speed) { // éclats de roche (via la physique si disponible)
    if (G.physics) G.physics.rocks(p, n, speed);
  }

  ring(p, r1, dur = 1, col = 0xffd890, r0 = 1) {
    const s = this.rings.reduce((a, b) => (a.t / a.dur > b.t / b.dur || !a.m.visible ? a : b));
    const r = this.rings.find((x) => !x.m.visible) || s;
    r.m.visible = true; r.m.position.copy(p); r.m.position.y += 0.6; r.t = 0; r.dur = dur; r.r0 = r0; r.r1 = r1; r.m.material.uniforms.uC.value.set(col);
  }
  sphere(p, r1, dur = 0.8, col = 0xffe2a0) {
    const s = this.spheres.find((x) => !x.m.visible) || this.spheres[0];
    s.m.visible = true; s.m.position.copy(p); s.t = 0; s.dur = dur; s.r1 = r1; s.m.material.uniforms.uC.value.set(col);
  }
  flash(p, intensity = 3000, dur = 0.5, col = 0xffaa55) {
    const L = this.lights.find((x) => x.t >= x.dur) || this.lights[0];
    L.l.position.copy(p); L.l.color.set(col); L.i0 = intensity * (Q().label === 'Bas' ? 0.6 : 1); L.t = 0; L.dur = dur;
  }
  decal(p, size, rot = Math.random() * 6.28) {
    const m = this.decals[this.decalNext]; this.decalNext = (this.decalNext + 1) % this.decals.length;
    const pos = m.geometry.attributes.position, T = G.terrain;
    const c = Math.cos(rot), s = Math.sin(rot);
    const base = m.userData.base || (m.userData.base = Float32Array.from(pos.array));
    for (let i = 0; i < pos.count; i++) {
      const lx = base[i * 3] * size, lz = -base[i * 3 + 1] * size;
      const x = p.x + lx * c - lz * s, z = p.z + lx * s + lz * c;
      pos.setXYZ(i, x, T.heightAt(x, z) + 0.06, z);
    }
    pos.needsUpdate = true; m.geometry.computeVertexNormals(); m.geometry.computeBoundingSphere();
    m.position.set(0, 0, 0); m.rotation.set(0, 0, 0); m.visible = true;
  }
  bolt(from, to, width = 0.7) {
    const b = this.bolts.find((x) => !x.m.visible) || this.bolts[0];
    const pts = [], n = 14;
    for (let i = 0; i <= n; i++) {
      const p = from.clone().lerp(to, i / n);
      if (i > 0 && i < n) { p.x += rand(-1, 1) * 6; p.z += rand(-1, 1) * 6; p.y += rand(-1, 1) * 2; }
      pts.push(p);
    }
    const curve = new THREE.CatmullRomCurve3(pts);
    b.m.geometry.dispose();
    b.m.geometry = new THREE.TubeGeometry(curve, 40, width, 5, false);
    b.m.visible = true; b.t = 0; b.m.material.opacity = 1;
  }
  shake(amount, at) {
    let k = 1;
    if (at && G.camera) k = clamp(1 - G.camera.position.distanceTo(at) / 350, 0, 1);
    this.trauma = Math.min(1, this.trauma + amount * k);
    if (amount * k > 0.25) haptic(Math.round(40 + amount * k * 120), Math.round(80 + amount * k * 170));
  }

  update(dt) {
    const w = this.wind;
    this.add.update(dt, w); this.norm.update(dt, w);
    const px = Math.max(innerHeight, 1) * G.renderer.getPixelRatio() * 0.5 / Math.tan(THREE.MathUtils.degToRad(G.camera.fov / 2));
    this.add.mat.uniforms.scale.value = px; this.norm.mat.uniforms.scale.value = px;
    for (const r of this.rings) {
      if (!r.m.visible) continue;
      r.t += dt; const k = r.t / r.dur;
      if (k >= 1) { r.m.visible = false; continue; }
      const rad = lerp(r.r0, r.r1, 1 - Math.pow(1 - k, 2.2));
      r.m.scale.set(rad, 1, rad);
      r.m.material.uniforms.uA.value = (1 - k) * 1.5;
    }
    for (const s of this.spheres) {
      if (!s.m.visible) continue;
      s.t += dt; const k = s.t / s.dur;
      if (k >= 1) { s.m.visible = false; continue; }
      s.m.scale.setScalar(Math.max(0.1, s.r1 * (1 - Math.pow(1 - k, 3))));
      s.m.material.uniforms.uA.value = Math.pow(1 - k, 1.5) * 0.9;
    }
    for (const L of this.lights) { L.t += dt; L.l.intensity = L.t < L.dur ? L.i0 * Math.pow(1 - L.t / L.dur, 2) : 0; }
    for (const b of this.bolts) { if (!b.m.visible) continue; b.t += dt; b.m.material.opacity = Math.max(0, 1 - b.t * 3) * (Math.random() < 0.5 ? 1 : 0.5); if (b.t > 0.35) b.m.visible = false; }
    this.trauma = Math.max(0, this.trauma - dt * 1.4);
  }
}
