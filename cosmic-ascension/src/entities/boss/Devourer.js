import * as THREE from 'three';
import { toonMaterial } from '../../render/shaders/Toon.js';
import { createAtmosphere } from '../../render/shaders/Atmosphere.js';
import { NOISE_GLSL } from '../../render/shaders/noise.glsl.js';
import { bus } from '../../core/EventBus.js';

// Dévoreur de Planètes — gueule colossale qui aspire le vaisseau. Points faibles: 4 yeux sur le pourtour. Même interface que Serpent:
// { root, cores[{pos,hp,r}], dead, particles, update(dt,t,shipPos,shipVel), damageAt(pos,dmg) }
export class Devourer {
  constructor({ pos = [1250, -260, -3300], radius = 230 } = {}) {
    this.root = new THREE.Group(); this.root.position.fromArray(pos); this.R = radius; this.dead = 0; this.rage = 0; this.particles = null; this.biteCd = 0; this.sighted = false;
    const R = radius, flesh = toonMaterial({ color: '#4a1f6a', rim: '#ff8a3a', rimStrength: 1.2, rimPower: 1.8 });
    // corps: sphère ouverte (gueule vers +Z) + veines lumineuses en shader
    const veins = new THREE.ShaderMaterial({ uniforms: { uT: { value: 0 } }, fog: false,
      vertexShader: 'varying vec3 vP; varying vec3 vN; varying vec3 vV; void main(){ vP=position; vN=normalize(normalMatrix*normal); vec4 mv=modelViewMatrix*vec4(position,1.); vV=normalize(-mv.xyz); gl_Position=projectionMatrix*mv; }',
      fragmentShader: `${NOISE_GLSL} uniform float uT; varying vec3 vP; varying vec3 vN; varying vec3 vV;
        void main(){ vec3 p=normalize(vP)*4.; float n=fbm(p+uT*.03); float v=smoothstep(.06,.0,abs(n-.5))*(.6+.4*sin(uT*2.+n*20.)); float l=clamp(dot(vN,normalize(vec3(.5,.7,.4)))*.5+.5,0.,1.); l=floor(l*4.)/3.;
          vec3 col=mix(vec3(.16,.05,.26),vec3(.36,.14,.5),l)+vec3(1.,.45,.1)*v*1.6+vec3(1.,.5,.2)*pow(1.-max(dot(vN,vV),0.),2.5)*.7; gl_FragColor=vec4(col,1.); }` });
    this.veins = veins;
    const body = new THREE.Mesh(new THREE.SphereGeometry(R, 40, 28, 0, 6.283, .62, Math.PI - .62), veins); body.rotation.x = Math.PI / 2; this.root.add(body);
    // vortex intérieur
    this.vortex = new THREE.Mesh(new THREE.CircleGeometry(R * .8, 48), new THREE.ShaderMaterial({ uniforms: { uT: { value: 0 } }, fog: false, side: THREE.DoubleSide,
      vertexShader: 'varying vec2 vU; void main(){ vU=uv-.5; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.); }',
      fragmentShader: 'uniform float uT; varying vec2 vU; void main(){ float r=length(vU)*2.; float a=atan(vU.y,vU.x); float s=.5+.5*sin(a*5.+r*14.-uT*5.); s=floor(s*3.)/2.; vec3 col=mix(vec3(.05,0.,.1),vec3(1.,.35,.1),s*(1.-r))+vec3(1.,.9,.5)*smoothstep(.25,0.,r); gl_FragColor=vec4(col*1.4,1.); }' }));
    this.vortex.position.z = R * .5; this.root.add(this.vortex);
    // anneaux de dents (2 draw calls) + tentacules (1)
    const tooth = new THREE.ConeGeometry(R * .09, R * .42, 5); tooth.translate(0, -R * .21, 0);
    this.teeth = [0, 1].map(k => { const n = 22 - k * 6, m = new THREE.InstancedMesh(tooth, new THREE.MeshBasicMaterial({ color: k ? '#ffd76a' : '#aef4ff' }), n), mm = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler();
      for (let i = 0; i < n; i++) { const a = i / n * 6.283, rr = R * (.8 - k * .2); q.setFromEuler(e.set(0, 0, a + Math.PI / 2 + Math.PI)); mm.compose(new THREE.Vector3(Math.cos(a) * rr, Math.sin(a) * rr, 0), q, new THREE.Vector3(1, 1 - k * .2, 1)); m.setMatrixAt(i, mm); }
      m.position.z = R * (.8 - k * .2); this.root.add(m); return m; });
    const tg = new THREE.ConeGeometry(R * .12, R * 1.5, 6, 6); tg.translate(0, R * .75, 0);
    this.tent = new THREE.InstancedMesh(tg, flesh, 10); this.tentBase = [];
    for (let i = 0; i < 10; i++) { const a = i / 10 * 6.283; this.tentBase.push(a); } this.root.add(this.tent);
    this.root.add(createAtmosphere(R * 1.05, '#ff6a2a', 2.6, 1));
    // yeux = points faibles
    this.cores = [0, 1, 2, 3].map(i => { const a = i / 4 * 6.283 + .78, m = new THREE.Mesh(new THREE.SphereGeometry(1, 16, 12), new THREE.MeshBasicMaterial({ color: '#ffe46a' })); const pupil = new THREE.Mesh(new THREE.SphereGeometry(.5, 10, 8), new THREE.MeshBasicMaterial({ color: '#2a0a3a' })); pupil.position.z = .7; m.add(pupil);
      m.position.set(Math.cos(a) * R * 1.0, Math.sin(a) * R * 1.0, R * .45); m.scale.setScalar(R * .17); this.root.add(m); return { hp: 9, mesh: m, pos: new THREE.Vector3(), r: R * .2 }; });
    this._a = new THREE.Vector3(); this._m = new THREE.Matrix4(); this._q = new THREE.Quaternion(); this._e = new THREE.Euler(); this._s = new THREE.Vector3(1, 1, 1); this._p = new THREE.Vector3();
  }
  get headPos() { return this.root.position; }
  damageAt(pos, dmg) {
    for (const c of this.cores) { if (c.hp <= 0 || c.pos.distanceTo(pos) > c.r + 8) continue; c.hp -= dmg; c.flash = .15; const P = this.particles;
      if (P) for (let k = 0; k < 10; k++) P.emit(pos, this._a.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(140), '#ffe46a', 10, .5, 2, 0);
      if (c.hp <= 0) { c.mesh.visible = false; this.rage++; const left = this.cores.filter(x => x.hp > 0).length;
        if (P) for (let k = 0; k < 70; k++) P.emit(c.pos, this._a.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(300), k % 2 ? '#ff8a3a' : '#ffffff', 18, 1.2, 1.5, 0);
        bus.emit('boss:phase', { id: 'devourer', phase: this.rage, coresLeft: left }); if (!left) { this.dead = .001; bus.emit('boss:defeated', { id: 'devourer', xp: 3000 }); } }
      return true; }
    return false;
  }
  update(dt, t, shipPos, shipVel) {
    const R = this.R, root = this.root;
    if (this.dead) { this.dead += dt; const k = Math.max(.001, 1 - this.dead * .25); root.scale.setScalar(k); root.rotation.z += dt * (2 + this.dead * 3);
      if (this.particles && k > .01) for (let i = 0; i < 3; i++) this.particles.emit(this._a.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(R * 2 * k).add(root.position), this._p.set(0, 0, 0), i ? '#ff8a3a' : '#ffffff', 22, .8, 0, 0);
      if (this.dead > 4) root.visible = false; return; }
    this.veins.uniforms.uT.value = t; this.vortex.material.uniforms.uT.value = t; this.teeth[0].rotation.z = t * .4; this.teeth[1].rotation.z = -t * .7;
    const d = root.position.distanceTo(shipPos), aware = d < 1500;
    // se tourne lentement vers le vaisseau, avance vers lui quand il est repéré
    if (aware) { this._m.lookAt(shipPos, root.position, root.up); this._q.setFromRotationMatrix(this._m); root.quaternion.slerp(this._q, Math.min(1, dt * (.5 + this.rage * .15)));
      root.position.addScaledVector(this._a.copy(shipPos).sub(root.position).normalize(), dt * (22 + this.rage * 10)); }
    for (let i = 0; i < 10; i++) { const a = this.tentBase[i]; this._e.set(-1.9 + Math.sin(t * 1.3 + i) * .35, 0, a - Math.PI / 2 + Math.sin(t * .9 + i * 2) * .2, 'ZXY'); this._q.setFromEuler(this._e);
      this._m.compose(this._p.set(Math.cos(a) * R * .75, Math.sin(a) * R * .75, -R * .55), this._q, this._s); this.tent.setMatrixAt(i, this._m); }
    this.tent.instanceMatrix.needsUpdate = true;
    for (const c of this.cores) { if (c.hp <= 0) continue; c.mesh.getWorldPosition(c.pos); c.flash = Math.max(0, (c.flash || 0) - dt); c.mesh.material.color.set(c.flash ? '#ffffff' : '#ffe46a'); c.mesh.lookAt(shipPos); }
    // aspiration: plus forte devant la gueule; morsure si avalé
    if (d < 1100 && shipVel) { const front = this._a.set(0, 0, 1).applyQuaternion(root.quaternion).dot(this._p.copy(shipPos).sub(root.position).normalize()), pull = (1 - d / 1100) * (60 + this.rage * 25) * Math.max(.25, front);
      shipVel.addScaledVector(this._p.copy(root.position).sub(shipPos).normalize(), pull * dt);
      if (this.particles && Math.random() < .6) { this._a.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(260).add(shipPos); this.particles.emit(this._a, this._p.copy(root.position).sub(this._a).normalize().multiplyScalar(260), '#ff8a3a', 6, 1.2, 0, 0); }
      this.biteCd -= dt;
      if (d < R * .75 && this.biteCd <= 0) { this.biteCd = 1.5; bus.emit('boss:hit-hero', { id: 'devourer', dmg: 30 }); shipVel.copy(this._a.set(0, 0, 1).applyQuaternion(root.quaternion)).multiplyScalar(420); } }
    if (!this.sighted && d < 1300) { this.sighted = true; bus.emit('boss:spawn', { id: 'devourer' }); }
  }
}
