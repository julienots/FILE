// Aura divine (transformation) : flammes dorées ondulantes autour du corps, étincelles qui montent,
// arcs électriques, poussière soulevée et pierres qui lévitent au sol. Transformation : le héros se
// ramasse, hurle, et une déflagration de lumière souffle tout. Pendant l'aura : puissance ×2,5,
// vitesse ×1,6, il encaisse mieux. Aucun objet créé pendant le jeu (lumière et maillages pré-créés).
import * as THREE from 'three';
import { G, clamp, damp, rand, emit, haptic } from './core.js';

const _up = new THREE.Vector3(), _c = new THREE.Vector3(), _v = new THREE.Vector3(), _z = new THREE.Vector3(0, 0, 1), _ic = new THREE.Quaternion(), _qr = new THREE.Quaternion();
const VS = `varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`;
const FS = `
uniform float uT, uO, uSeed; uniform vec3 uC1, uC2; varying vec2 vUv;
float h(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float n(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);
  return mix(mix(h(i), h(i+vec2(1,0)), f.x), mix(h(i+vec2(0,1)), h(i+vec2(1,1)), f.x), f.y); }
float fbm(vec2 p){ float v = 0.0, a = 0.5; for (int i = 0; i < 3; i++){ v += a * n(p); p *= 2.03; a *= 0.5; } return v; }
void main(){
  vec2 p = vUv * 2.0 - 1.0;               // -1..1, y vers le haut
  // silhouette en goutte : large en bas autour du corps, effilée en pointes vers le haut
  float w = mix(0.62, 0.22, smoothstep(-0.2, 1.0, p.y)) * (1.0 + 0.25 * sin(p.y * 6.0 - uT * 7.0 + uSeed));
  vec2 q = vec2(p.x / w, (p.y + 0.15) / 1.05);
  float d = length(q);
  float flame = fbm(vec2(p.x * 3.2 + uSeed, p.y * 2.2 - uT * 3.4)) ;
  float tongues = fbm(vec2(p.x * 7.0 - uSeed, p.y * 1.4 - uT * 5.5));
  float edge = d - 0.55 - flame * 0.55 - tongues * 0.35 * smoothstep(-0.3, 0.8, p.y);
  float shell = smoothstep(0.08, -0.25, edge);           // enveloppe
  float hollow = smoothstep(0.55, 0.05, d * (0.85 + flame * 0.3));
  float rim = shell * (1.0 - hollow * 0.78);              // plus intense sur le pourtour
  float bottom = smoothstep(-1.0, -0.75, p.y);
  float a = rim * bottom * uO;
  vec3 col = mix(uC2, uC1, clamp(rim * 1.3 + tongues * 0.3, 0.0, 1.0));
  gl_FragColor = vec4(col * a * 1.5, a);
}`;

export class Aura {
  constructor(scene) {
    this.scene = scene;
    this.active = false;
    this.k = 0;            // intensité affichée 0..1
    this.t = 0;
    this.transforming = 0;
    this.group = new THREE.Group();
    this.layers = [];
    const geo = new THREE.PlaneGeometry(1, 1);
    for (const [s, seed, c1, c2, o] of [[1.0, 0.0, 0xfff2b0, 0xffa818, 0.8], [0.8, 3.7, 0xfff8e0, 0xffc830, 0.6]]) {
      const m = new THREE.Mesh(geo, new THREE.ShaderMaterial({
        vertexShader: VS, fragmentShader: FS, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
        uniforms: { uT: { value: 0 }, uO: { value: 0 }, uSeed: { value: seed }, uC1: { value: new THREE.Color(c1) }, uC2: { value: new THREE.Color(c2) } },
      }));
      m.userData = { s, o }; m.frustumCulled = false; m.renderOrder = 5;
      this.group.add(m); this.layers.push(m);
    }
    this.group.visible = false;
    scene.add(this.group);
    // lumière dorée (créée une seule fois, éteinte au repos)
    this.light = new THREE.PointLight(0xffc850, 0, 40, 1.6);
    scene.add(this.light);
    this.boltT = 0; this.rockT = 0;
  }

  toggle() { if (this.transforming > 0) return; if (this.active) this.calm(); else this.transform(); }

  transform() {
    const god = G.god;
    this.transforming = 1.5;
    god.playAction('transform', 1.5);
    if (god.state === 'fly') god.vel.multiplyScalar(0.1);
    if (G.audio) G.audio.charge && G.audio.charge(1.4);
    if (G.music) G.music.setAura(true);
    haptic(60);
  }

  burst() {
    const god = G.god, p = god.pos.clone().add(new THREE.Vector3(0, 2.6, 0));
    this.active = true; god.aura = this;
    god.power = 2.5; god.speedMul = 1.6;
    const g = G.terrain.heightAt(p.x, p.z);
    G.fx.sphere(p, 26, 0.7, 0xffe7a0); G.fx.ring(p.clone().setY(Math.max(g, p.y - 30)), 140, 1.4, 0xffd27a);
    G.fx.flash(p, 6000, 0.6, 0xffd890); G.fx.shake(0.8, p); G.fx.sparks(p, 90, [1, 0.85, 0.35], 40);
    if (p.y - g < 14) { G.fx.dust(p.clone().setY(g), 26, 40); G.fx.rocks(p.clone().setY(g), 10, 14); G.terrain.crater(p.x, p.z, 10, 1.5, { rim: 0.5, burn: 0.1 }); }
    if (G.env) G.env.flash = 0.5;
    if (G.powers) G.powers.areaDamage(p, 45, 700, 'push', 'aura');
    if (G.audio) { G.audio.explosion(p, 0.9); G.audio.roar && G.audio.roar(p, 0.4); }
    emit('journal', { icon: '⚡', text: 'L\'aura divine s\'embrase. Sa puissance n\'a plus de limite.' });
    emit('aura', true);
    haptic(180, 255);
  }

  calm() {
    const god = G.god;
    this.active = false; god.power = 1; god.speedMul = 1;
    if (G.fx) G.fx.ring(god.pos, 30, 0.6, 0xffd27a);
    if (G.music) G.music.setAura(false);
    emit('aura', false);
  }

  update(dt) {
    const god = G.god;
    this.t += dt;
    if (this.transforming > 0) {
      this.transforming -= dt;
      // montée : aspiration de poussière et grondement
      const p = god.pos;
      if (G.fx && Math.random() < dt * 30) { const a = rand(0, 6.28), r = rand(6, 16); const q = new THREE.Vector3(p.x + Math.cos(a) * r, p.y + rand(0, 5), p.z + Math.sin(a) * r); G.fx.add.emit(q.x, q.y, q.z, -Math.cos(a) * r * 1.6, rand(1, 4), -Math.sin(a) * r * 1.6, rand(0.4, 0.9), 0.5, 1, 0.85, 0.4, 0.9, 0.2, 0, 0); }
      if (G.fx) G.fx.shake(0.06, p);
      this.k = damp(this.k, 0.35, 3, dt);
      if (this.transforming <= 0.25 && !this.active) this.burst();
    }
    this.k = damp(this.k, this.active ? 1 : (this.transforming > 0 ? this.k : 0), this.active ? 6 : 3, dt);
    const vis = this.k > 0.01 && god.root.visible;
    this.group.visible = vis;
    this.light.intensity = vis ? (450 + Math.sin(this.t * 23) * 80) * this.k : 0;
    if (!vis) return;
    const cam = G.camera;
    // centre = torse du héros (suit l'inclinaison du corps en vol), axe = axe du corps
    const rq = god.root.quaternion;
    _up.set(0, 1, 0).applyQuaternion(rq);
    _c.copy(god.root.position).addScaledVector(_up, 2.7);
    this.group.position.copy(_c);
    this.light.position.copy(_c);
    // la flamme se dresse dans l'axe du corps vu par la caméra (couché en vol « comète »)
    _ic.copy(cam.quaternion).invert();
    _v.copy(_up).applyQuaternion(_ic);
    const roll = Math.atan2(-_v.x, _v.y);
    _qr.setFromAxisAngle(_z, roll);
    const pulse = 1 + Math.sin(this.t * 17) * 0.03 + Math.sin(this.t * 5.3) * 0.04;
    for (const m of this.layers) {
      m.quaternion.copy(cam.quaternion).multiply(_qr);
      const s = m.userData.s * pulse;
      m.scale.set(6.0 * s, 9.2 * s, 1);
      m.position.copy(_up).multiplyScalar(0.6 * s - 0.5);
      m.material.uniforms.uT.value = this.t * (1 + m.userData.s * 0.15);
      m.material.uniforms.uO.value = m.userData.o * this.k;
    }
    if (!this.active) return;
    const p = god.pos, gv = god.vel;
    // étincelles dorées qui montent le long du corps : elles héritent de la vitesse du héros
    const n = Math.round(dt * 60);
    for (let i = 0; i < n; i++) {
      const a = rand(0, 6.28), r = rand(0.8, 2.4), h = rand(-2.2, 2.2);
      const x = _c.x + Math.cos(a) * r + _up.x * h, y = _c.y + _up.y * h, z = _c.z + Math.sin(a) * r + _up.z * h;
      const up = rand(8, 15);
      G.fx.add.emit(x, y, z, gv.x + _up.x * up, gv.y + _up.y * up, gv.z + _up.z * up, rand(0.25, 0.55), rand(0.3, 0.6), 1, rand(0.75, 0.95), 0.35, 1, 0, -1, 0);
    }
    // arcs électriques autour du corps (seulement quand il est presque immobile : sinon ils traînent)
    this.boltT -= dt;
    if (this.boltT <= 0 && gv.length() < 15) {
      this.boltT = rand(0.15, 0.45);
      const a = rand(0, 6.28), b = a + rand(0.6, 1.6), y0 = rand(-1.8, 1.8);
      const A = new THREE.Vector3(_c.x + Math.cos(a) * 1.6, _c.y + y0, _c.z + Math.sin(a) * 1.6);
      const Bp = new THREE.Vector3(_c.x + Math.cos(b) * 1.8, _c.y + y0 + rand(-1.2, 1.2), _c.z + Math.sin(b) * 1.8);
      G.fx.bolt(A, Bp, 0.1);
    }
    // au sol : poussière en anneau et cailloux qui lévitent
    const g = G.terrain.heightAt(p.x, p.z), alt = p.y - g;
    if (alt < 18) {
      const k = 1 - alt / 18;
      if (Math.random() < dt * 14 * k) { const a = rand(0, 6.28), r = rand(3, 9); G.fx.add.emit(p.x + Math.cos(a) * r, g + 0.3, p.z + Math.sin(a) * r, Math.cos(a) * 6, rand(0.5, 2), Math.sin(a) * 6, rand(1.5, 3), rand(0.8, 1.4), 0.62, 0.55, 0.46, 0.25, 1.6, 0, 1.2); }
      this.rockT -= dt;
      if (this.rockT <= 0) {
        this.rockT = rand(0.05, 0.15) / k;
        const a = rand(0, 6.28), r = rand(2.5, 10);
        G.fx.add.emit(p.x + Math.cos(a) * r, g + 0.2, p.z + Math.sin(a) * r, 0, rand(1.5, 4), 0, rand(0.15, 0.35), rand(1.5, 2.5), 0.32, 0.28, 0.24, 1, 0, -0.15, 0.5);
      }
      if (Math.random() < dt * 0.8 * k) G.fx.ring(new THREE.Vector3(p.x, g, p.z), 18, 0.7, 0xffd27a);
    }
  }
}
