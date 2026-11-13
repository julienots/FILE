import * as THREE from 'three';
// Système de particules poolé (1 draw call, Points additifs). emit() ne fait aucune allocation.
export class Particles {
  constructor(max = 500) {
    this.max = max; this.n = 0; this.cursor = 0;
    this.pos = new Float32Array(max * 3); this.vel = new Float32Array(max * 3); this.col = new Float32Array(max * 3);
    this.life = new Float32Array(max); this.ttl = new Float32Array(max); this.size = new Float32Array(max); this.sz0 = new Float32Array(max); this.drag = new Float32Array(max); this.grav = new Float32Array(max);
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(this.pos, 3)); g.setAttribute('color', new THREE.BufferAttribute(this.col, 3)); g.setAttribute('size', new THREE.BufferAttribute(this.size, 1));
    this.mat = new THREE.ShaderMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, fog: false, uniforms: { uS: { value: 600 } },
      vertexShader: 'attribute float size; attribute vec3 color; uniform float uS; varying vec3 vC; void main(){ vC=color; vec4 mv=modelViewMatrix*vec4(position,1.); gl_Position=projectionMatrix*mv; gl_PointSize=size*uS/max(.1,-mv.z); }',
      fragmentShader: 'varying vec3 vC; void main(){ float d=length(gl_PointCoord-.5); float a=smoothstep(.5,.0,d); gl_FragColor=vec4(vC*(a+smoothstep(.2,0.,d)),a); }' });
    this.points = new THREE.Points(g, this.mat); this.points.frustumCulled = false; this.points.renderOrder = 5; this.gravityDir = new THREE.Vector3(0, -1, 0);
    this._c = new THREE.Color();
  }
  // p: Vector3, v: Vector3
  emit(p, v, color, size = .4, ttl = 1, drag = 1.5, grav = 0) {
    const i = this.cursor; this.cursor = (i + 1) % this.max; const c = this._c.set(color);
    this.pos[i * 3] = p.x; this.pos[i * 3 + 1] = p.y; this.pos[i * 3 + 2] = p.z; this.vel[i * 3] = v.x; this.vel[i * 3 + 1] = v.y; this.vel[i * 3 + 2] = v.z;
    this.col[i * 3] = c.r; this.col[i * 3 + 1] = c.g; this.col[i * 3 + 2] = c.b; this.life[i] = this.ttl[i] = ttl; this.sz0[i] = size; this.drag[i] = drag; this.grav[i] = grav;
  }
  update(dt, heightPx = 600) {
    this.mat.uniforms.uS.value = heightPx; const g = this.gravityDir;
    for (let i = 0; i < this.max; i++) {
      if (this.life[i] <= 0) { this.size[i] = 0; continue; }
      this.life[i] -= dt; const k = Math.max(0, this.life[i] / this.ttl[i]), dr = Math.max(0, 1 - this.drag[i] * dt), j = i * 3, gv = this.grav[i] * dt;
      this.vel[j] = this.vel[j] * dr + g.x * gv; this.vel[j + 1] = this.vel[j + 1] * dr + g.y * gv; this.vel[j + 2] = this.vel[j + 2] * dr + g.z * gv;
      this.pos[j] += this.vel[j] * dt; this.pos[j + 1] += this.vel[j + 1] * dt; this.pos[j + 2] += this.vel[j + 2] * dt; this.size[i] = this.sz0[i] * Math.sqrt(k);
    }
    const a = this.points.geometry.attributes; a.position.needsUpdate = a.size.needsUpdate = a.color.needsUpdate = true;
  }
}
