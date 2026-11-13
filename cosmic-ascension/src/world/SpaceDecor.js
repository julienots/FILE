import * as THREE from 'three';
import { toonMaterial } from '../render/shaders/Toon.js';
import { createAtmosphere } from '../render/shaders/Atmosphere.js';
import { NOISE_GLSL } from '../render/shaders/noise.glsl.js';
import { Config } from '../core/Config.js';

function rng(seed) { let s = seed >>> 0 || 1; return () => (s = (s * 16807) % 2147483647) / 2147483647; }
// Champ d'astéroïdes instancié (1 draw call). Renvoie mesh + colliders [{pos,r}] (repère monde, statiques).
export function createAsteroidField({ center = [0, 0, 0], radius = 300, count = 180, seed = 1, color = '#8a7fa8', flat = .35 } = {}) {
  const rnd = rng(seed), geo = new THREE.IcosahedronGeometry(1, 1), p = geo.attributes.position, v = new THREE.Vector3();
  for (let i = 0; i < p.count; i++) { v.fromBufferAttribute(p, i); const k = 1 + Math.sin(v.x * 3.1 + seed) * .18 + Math.cos(v.y * 4.3 + v.z * 2.7) * .16; p.setXYZ(i, v.x * k, v.y * k * .8, v.z * k); }
  geo.computeVertexNormals();
  count = Math.round(count * Config.qualityPresets[Config.quality.level].particles);
  const mesh = new THREE.InstancedMesh(geo, toonMaterial({ color, rim: '#c9b8ff', rimStrength: .5, flatShading: true }), count), colliders = [];
  const m = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler(), s = new THREE.Vector3(), c = new THREE.Vector3().fromArray(center);
  for (let i = 0; i < count; i++) {
    const a = rnd() * 6.283, r = radius * (.25 + .75 * Math.sqrt(rnd())), big = rnd() < .08, sc = big ? 14 + rnd() * 22 : 1.5 + rnd() * 6;
    v.set(Math.cos(a) * r, (rnd() - .5) * radius * flat, Math.sin(a) * r).add(c);
    q.setFromEuler(e.set(rnd() * 6, rnd() * 6, rnd() * 6)); m.compose(v, q, s.setScalar(sc)); mesh.setMatrixAt(i, m); colliders.push({ pos: v.clone(), r: sc * 1.05 });
  }
  mesh.frustumCulled = false; mesh.name = 'asteroids'; return { mesh, colliders };
}
// Station spatiale procédurale (anneau tournant + modules + balises lumineuses).
export function createStation() {
  const g = new THREE.Group(), hull = toonMaterial({ color: '#e8ecff', rim: '#7fd0ff' }), dark = toonMaterial({ color: '#4a4f7a', rim: '#9fb0ff' }), glow = new THREE.MeshBasicMaterial({ color: '#6ff2ff' }), warm = new THREE.MeshBasicMaterial({ color: '#ffd76a' });
  const ring = new THREE.Group(); ring.add(new THREE.Mesh(new THREE.TorusGeometry(22, 2.6, 8, 40), hull));
  for (let i = 0; i < 4; i++) { const sp = new THREE.Mesh(new THREE.CylinderGeometry(.8, .8, 44, 6), dark); sp.rotation.z = i * Math.PI / 4; ring.add(sp); }
  for (let i = 0; i < 12; i++) { const w = new THREE.Mesh(new THREE.BoxGeometry(1.6, 1, .4), i % 3 ? warm : glow), a = i / 12 * 6.283; w.position.set(Math.cos(a) * 24.8, Math.sin(a) * 24.8, 0); w.rotation.z = a; ring.add(w); }
  g.add(ring);
  const core = new THREE.Mesh(new THREE.CapsuleGeometry(5, 14, 6, 14), hull); core.rotation.x = Math.PI / 2; g.add(core);
  const dome = new THREE.Mesh(new THREE.SphereGeometry(4.2, 14, 10), new THREE.MeshToonMaterial({ color: '#59d7ff', emissive: '#2a8fd0', emissiveIntensity: .6 })); dome.position.z = 12; g.add(dome);
  for (const s of [-1, 1]) { const pan = new THREE.Mesh(new THREE.BoxGeometry(26, .3, 8), toonMaterial({ color: '#3a5fe0', rim: '#7fd0ff', emissive: '#1a2a80', emissiveIntensity: .5 })); pan.position.set(s * 21, 0, -8); g.add(pan); }
  const beacon = new THREE.Mesh(new THREE.SphereGeometry(1.2, 10, 8), new THREE.MeshBasicMaterial({ color: '#ff5a8a' })); beacon.position.z = -14; g.add(beacon);
  g.userData.update = (dt, t) => { ring.rotation.z += dt * .15; beacon.scale.setScalar(1 + Math.sin(t * 4) * .4); };
  return g;
}
// Soleil stylisé: sphère à surface animée + double halo.
export function createSun(radius = 260, color = '#ffd76a') {
  const g = new THREE.Group();
  const mat = new THREE.ShaderMaterial({ uniforms: { uT: { value: 0 }, uC: { value: new THREE.Color(color) } }, fog: false,
    vertexShader: 'varying vec3 vP; varying vec3 vN; varying vec3 vV; void main(){ vP=position; vN=normalize(normalMatrix*normal); vec4 mv=modelViewMatrix*vec4(position,1.); vV=normalize(-mv.xyz); gl_Position=projectionMatrix*mv; }',
    fragmentShader: `${NOISE_GLSL} uniform float uT; uniform vec3 uC; varying vec3 vP; varying vec3 vN; varying vec3 vV;
      void main(){ vec3 p=normalize(vP)*3.; float n=fbm(p+vec3(uT*.05,uT*.03,0.)); n=floor(n*5.)/5.; float f=pow(1.-max(dot(vN,vV),0.),2.);
        vec3 col=mix(uC*1.1,vec3(1.,.45,.15),smoothstep(.35,.7,n))+vec3(1.,.95,.8)*f*.8; gl_FragColor=vec4(col*1.6,1.); }` });
  g.add(new THREE.Mesh(new THREE.SphereGeometry(radius, 32, 24), mat));
  g.add(createAtmosphere(radius * 1.05, color, 2.2, 1.6)); g.add(createAtmosphere(radius * 1.45, '#ff8a3a', 3.5, .7));
  g.userData.update = (dt, t) => { mat.uniforms.uT.value = t; };
  return g;
}
// Galaxies lointaines (sprites shader spirale) — donne l'échelle "Andromède".
export function createGalaxies(n = 7, seed = 5) {
  const rnd = rng(seed), g = new THREE.Group(), geo = new THREE.PlaneGeometry(1, 1);
  for (let i = 0; i < n; i++) {
    const mat = new THREE.ShaderMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, fog: false, side: THREE.DoubleSide,
      uniforms: { uA: { value: new THREE.Color().setHSL(rnd(), .8, .6) }, uB: { value: new THREE.Color().setHSL(rnd(), .7, .75) }, uS: { value: rnd() * 10 }, uArms: { value: 2 + Math.floor(rnd() * 3) } },
      vertexShader: 'varying vec2 vU; void main(){ vU=uv-.5; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.); gl_Position.z=gl_Position.w*.9998; }',
      fragmentShader: `uniform vec3 uA,uB; uniform float uS,uArms; varying vec2 vU;
        void main(){ float r=length(vU)*2.; float a=atan(vU.y,vU.x); float arm=.5+.5*cos(a*uArms-r*9.+uS); float core=exp(-r*7.);
          float d=smoothstep(1.,.1,r)*pow(arm,2.5)*.8+core*1.6; gl_FragColor=vec4(mix(uA,uB,core+arm*.3)*d,d); }` });
    const m = new THREE.Mesh(geo, mat), u = rnd() * 2 - 1, t = rnd() * 6.283, rr = Math.sqrt(1 - u * u);
    m.position.set(rr * Math.cos(t), u * .7, rr * Math.sin(t)).multiplyScalar(4200); m.scale.setScalar(500 + rnd() * 900); m.lookAt(0, 0, 0); m.rotateX((rnd() - .5) * 1.6); m.rotateZ(rnd() * 6); m.frustumCulled = false; m.renderOrder = -8.5; g.add(m);
  }
  return g;
}
