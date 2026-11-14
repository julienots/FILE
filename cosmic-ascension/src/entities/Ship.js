import * as THREE from 'three';
import { toonMaterial, addOutline, shaderTime } from '../render/shaders/Toon.js';
import { Assets } from '../assets/AssetLibrary.js';
// Vaisseau du joueur. FABLE: améliorer visuels par niveau (State.data.ship.level), traînées, boost warp.
export class Ship {
  constructor() {
    this.root = new THREE.Group();
    const m = Assets.get('ship_basic');
    if (m) this.root.add(m); else this._procedural();
    // Flamme de réacteur: deux cônes additifs en shader (cœur blanc → couleur → transparent, ondes de choc qui défilent).
    const box = new THREE.Box3().setFromObject(this.root), w = Math.max(.6, (box.max.x - box.min.x) * .14);
    const cy = (box.min.y + box.max.y) / 2, sx = (box.max.x - box.min.x) * .27;
    const mk = (r, len, color, power, x = 0) => { const g = new THREE.ConeGeometry(r, len, 16, 8, true); g.translate(0, -len / 2, 0); g.rotateX(-Math.PI / 2);   // base à l'origine, pointe vers +Z (arrière)
      const m = new THREE.Mesh(g, new THREE.ShaderMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide, fog: false,
        uniforms: { uT: shaderTime, uC: { value: new THREE.Color(color) }, uK: { value: 0 }, uL: { value: len }, uP: { value: power } },
        vertexShader: 'uniform float uL,uK,uT; varying float vT; varying vec3 vN; varying vec3 vV; void main(){ vT=position.z/uL; vec3 p=position; p.z*=.35+uK*1.1; p.xy*=1.+sin(uT*30.+vT*14.)*.06*vT; vec4 mv=modelViewMatrix*vec4(p,1.); vN=normalize(normalMatrix*normal); vV=normalize(-mv.xyz); gl_Position=projectionMatrix*mv; }',
        fragmentShader: 'uniform vec3 uC; uniform float uT,uK,uP; varying float vT; varying vec3 vN; varying vec3 vV; void main(){ float f=pow(abs(dot(normalize(vN),vV)),uP); float fade=pow(1.-vT,1.6); float rings=.75+.25*sin(vT*26.-uT*38.); vec3 col=mix(vec3(1.),uC,smoothstep(0.,.45,vT)); float a=f*fade*rings*(.35+uK*.9); gl_FragColor=vec4(col*a*1.6,a); }' }));
      m.position.set(x, cy, box.max.z * .8); m.renderOrder = 4; this.root.add(m); return m; };
    this.flames = [mk(w, 5, '#3fa0ff', 1.2), mk(w * .5, 3.2, '#bfe8ff', .6), mk(w * .6, 3.6, '#3fa0ff', 1.2, sx), mk(w * .6, 3.6, '#3fa0ff', 1.2, -sx)]; this.flame = this.flames[0];
    this.engine = new THREE.PointLight('#5fb0ff', 0, 30); this.engine.position.set(0, cy, box.max.z + 1); this.root.add(this.engine);
  }
  _procedural() {
    const hull = toonMaterial({ color: '#2a2c48', rim: '#6ff2ff', rimStrength: 1.3 }), accent = toonMaterial({ color: '#6a1438', rim: '#ff5a9a', rimStrength: 1.2, emissive: '#ff2a6a', emissiveIntensity: .35 });
    const body = new THREE.Mesh(new THREE.SphereGeometry(1, 20, 14), hull); body.scale.set(.9, .55, 2); addOutline(body, '#1a0f33', .04); this.root.add(body);
    const cock = new THREE.Mesh(new THREE.SphereGeometry(.5, 14, 10), new THREE.MeshToonMaterial({ color: '#59d7ff' })); cock.position.set(0, .4, -.5); this.root.add(cock);
    for (const s of [-1, 1]) { const w = new THREE.Mesh(new THREE.BoxGeometry(2, .12, 1.1), accent); w.position.set(1.3 * s, -.05, .6); w.rotation.z = .12 * s; this.root.add(w); }
    const fin = new THREE.Mesh(new THREE.BoxGeometry(.1, .8, .9), accent); fin.position.set(0, .6, 1.2); this.root.add(fin);
  }
  // k: poussée 0..1 (>.6 = postcombustion: la flamme vire au violet)
  setThrust(k) { k = Math.max(0, Math.min(1, k)); for (const f of this.flames) { f.material.uniforms.uK.value = k; f.visible = k > .02; } for (const i of [0, 2, 3]) this.flames[i].material.uniforms.uC.value.set(k > .6 ? '#9a6aff' : '#3fa0ff'); this.engine.intensity = k * 60; }
}
