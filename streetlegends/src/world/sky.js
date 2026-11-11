// Ciel, cycle jour / nuit, météo (pluie, brouillard, routes mouillées), éclairage et ombres.
import * as THREE from 'three';
import { clamp, lerp, smoothstep, damp } from '../core/util.js';
import { uniforms } from './city.js';

const K = (h, top, hor, sun, sunI, amb, ambI, fog) => ({ h, top: new THREE.Color(top), hor: new THREE.Color(hor), sun: new THREE.Color(sun), sunI, amb: new THREE.Color(amb), ambI, fog: new THREE.Color(fog) });
const KEYS = [
  K(0, 0x040818, 0x101c3c, 0x8fa8ff, 0.32, 0x2a3a6a, 0.62, 0x0c1530),
  K(4.5, 0x050a1c, 0x16224a, 0x8fa8ff, 0.3, 0x2a3a6a, 0.62, 0x0e1836),
  K(6, 0x2b4a8c, 0xff9a6b, 0xffb37a, 1.3, 0x8a7aa0, 0.75, 0xd9a48c),
  K(7.5, 0x3d8fe0, 0xbfe3ff, 0xfff0d0, 2.1, 0xa9c8f0, 0.9, 0xbddcf5),
  K(12, 0x1f7ae0, 0xa8dcff, 0xfffaf0, 2.5, 0xbcd8ff, 0.95, 0xb5dbf7),
  K(16.5, 0x2a7fe0, 0xb5dcff, 0xffedcc, 2.4, 0xb9d2f5, 0.92, 0xb8d8f2),
  K(18.5, 0x35459a, 0xff7a3c, 0xff8a4a, 1.7, 0xa0789a, 0.8, 0xe08a5c),
  K(19.6, 0x1a1f5a, 0xd2456a, 0xff6a5a, 0.6, 0x5a4a8a, 0.66, 0x7a3f6a),
  K(21, 0x050a1e, 0x141f48, 0x8fa8ff, 0.32, 0x2a3a6a, 0.62, 0x0d1634),
  K(24, 0x040818, 0x101c3c, 0x8fa8ff, 0.32, 0x2a3a6a, 0.62, 0x0c1530),
];
const WHITE = new THREE.Color(0xffffff), GROUND = new THREE.Color(0.62, 0.56, 0.5);
export const WEATHERS = { clear: 'Dégagé', cloudy: 'Nuageux', rain: 'Pluie', fog: 'Brouillard' };
export const phaseName = (h) => (h < 5 || h >= 20.5 ? 'Nuit' : h < 7.5 ? 'Aube' : h < 17.5 ? 'Jour' : 'Coucher du soleil');

export class Sky {
  constructor(scene, renderer, q) {
    this.scene = scene; this.renderer = renderer; this.q = q;
    this.hour = 16.5; this.weather = 'clear'; this.wet = 0; this.rainAmt = 0; this.fogAmt = 0; this.cloud = 0;
    this.night = 0; this.timeScale = 1; // minutes de jeu par seconde réelle
    this.hemi = new THREE.HemisphereLight(0xffffff, 0x6b7a5a, 1);
    this.sun = new THREE.DirectionalLight(0xffffff, 3);
    this.sun.shadow.bias = -0.0004; this.sun.shadow.normalBias = 0.04;
    scene.add(this.hemi, this.sun, this.sun.target);
    this.u = { top: { value: new THREE.Color() }, hor: { value: new THREE.Color() }, sunDir: { value: new THREE.Vector3(0, 1, 0) }, sunCol: { value: new THREE.Color() }, stars: { value: 0 }, cloud: { value: 0 }, time: { value: 0 } };
    const mat = new THREE.ShaderMaterial({
      uniforms: this.u, side: THREE.BackSide, depthWrite: false, fog: false,
      vertexShader: 'varying vec3 vDir; void main(){ vDir = normalize(position); vec4 p = modelViewMatrix * vec4(position, 1.0); gl_Position = projectionMatrix * p; }',
      fragmentShader: `varying vec3 vDir; uniform vec3 top; uniform vec3 hor; uniform vec3 sunDir; uniform vec3 sunCol; uniform float stars; uniform float cloud; uniform float time;
        float h21(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
        float vn(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f); return mix(mix(h21(i), h21(i+vec2(1,0)), f.x), mix(h21(i+vec2(0,1)), h21(i+vec2(1,1)), f.x), f.y); }
        void main(){
          vec3 d = normalize(vDir); float y = max(d.y, 0.0);
          vec3 col = mix(hor, top, pow(y, 0.55));
          float s = max(dot(d, sunDir), 0.0);
          col += sunCol * (pow(s, 600.0) * 3.0 + pow(s, 12.0) * 0.28 + pow(s, 3.0) * 0.1);
          if (stars > 0.01 && d.y > 0.0) { vec2 g = d.xz / (d.y + 0.25) * 60.0; float st = step(0.992, h21(floor(g))); col += vec3(st) * stars * (0.5 + 0.5 * sin(time * 2.0 + h21(floor(g)) * 30.0)) * smoothstep(0.0, 0.2, d.y); }
          if (d.y > 0.0) { vec2 c = d.xz / (d.y + 0.12) * 1.6 + time * 0.006; float n = vn(c) * 0.6 + vn(c * 2.3) * 0.3 + vn(c * 5.1) * 0.1; float m = smoothstep(0.62 - cloud * 0.42, 0.9 - cloud * 0.4, n) * smoothstep(0.0, 0.12, d.y); vec3 cc = mix(hor, vec3(1.0), 0.55) * (0.55 + 0.5 * (1.0 - cloud * 0.5)) + sunCol * 0.12; col = mix(col, cc, m * (0.75 + cloud * 0.2)); }
          col = mix(col, hor, smoothstep(0.0, -0.08, d.y) * 0.6);
          gl_FragColor = vec4(col, 1.0);
          #include <colorspace_fragment>
        }`,
    });
    this.dome = new THREE.Mesh(new THREE.SphereGeometry(3000, 24, 14), mat);
    this.dome.renderOrder = -10; this.dome.frustumCulled = false;
    scene.add(this.dome);
    scene.fog = new THREE.Fog(0xb5dbf7, 200, 900);
    // pluie
    const N = 1600, rp = new Float32Array(N * 6), ro = new Float32Array(N * 2);
    for (let i = 0; i < N; i++) {
      const x = Math.random() * 40 - 20, y = Math.random() * 24, z = Math.random() * 40 - 20;
      rp.set([x, y, z, x, y, z], i * 6); ro[i * 2] = 0; ro[i * 2 + 1] = 1;
    }
    const rg = new THREE.BufferGeometry();
    rg.setAttribute('position', new THREE.BufferAttribute(rp, 3)); rg.setAttribute('end', new THREE.BufferAttribute(ro, 1));
    this.rainU = { time: { value: 0 }, cam: { value: new THREE.Vector3() }, vel: { value: new THREE.Vector3() }, alpha: { value: 0 } };
    this.rain = new THREE.LineSegments(rg, new THREE.ShaderMaterial({
      uniforms: this.rainU, transparent: true, depthWrite: false, fog: false,
      vertexShader: `attribute float end; uniform float time; uniform vec3 cam; uniform vec3 vel; varying float vA;
        void main(){ vec3 p = position; p.y = mod(p.y - time * 26.0, 24.0) - 6.0; p.xz = mod(p.xz - cam.xz + 20.0, 40.0) - 20.0; p.xz += cam.xz; p.y += cam.y;
          p += end * (vec3(0.0, 0.9, 0.0) + vel * 0.03); vA = 1.0 - end * 0.6; gl_Position = projectionMatrix * viewMatrix * vec4(p, 1.0); }`,
      fragmentShader: 'uniform float alpha; varying float vA; void main(){ gl_FragColor = vec4(0.75, 0.85, 1.0, alpha * vA * 0.55); }',
    }));
    this.rain.frustumCulled = false; this.rain.visible = false; this.rain.renderOrder = 5;
    scene.add(this.rain);
    this.pmrem = new THREE.PMREMGenerator(renderer);
    this.envScene = new THREE.Scene();
    this.envDome = new THREE.Mesh(new THREE.SphereGeometry(50, 16, 10), mat);
    this.envGround = new THREE.Mesh(new THREE.CircleGeometry(60, 16).rotateX(-Math.PI / 2), new THREE.MeshBasicMaterial({ color: 0x33373d }));
    this.envGround.position.y = -3;
    this.envScene.add(this.envDome, this.envGround);
    this.envHour = -99; this.envTarget = null;
    this.setQuality(q);
    this.apply(0, true);
  }
  setQuality(q) {
    this.q = q;
    this.sun.castShadow = q.shadows > 0;
    if (q.shadows) {
      this.sun.shadow.mapSize.set(q.shadows, q.shadows);
      if (this.sun.shadow.map) { this.sun.shadow.map.dispose(); this.sun.shadow.map = null; }
      const c = this.sun.shadow.camera, r = q.shadowRange;
      c.left = -r; c.right = r; c.top = r; c.bottom = -r; c.near = 1; c.far = 400; c.updateProjectionMatrix();
    }
  }
  setWeather(w) { this.weather = w; }
  get grip() { return 1 - 0.2 * this.wet; }
  get lightsOn() { return this.night > 0.35 || this.fogAmt > 0.5 || this.rainAmt > 0.6; }
  apply(dt, force = false) {
    const h = ((this.hour % 24) + 24) % 24;
    let i = 0;
    while (i < KEYS.length - 2 && KEYS[i + 1].h <= h) i++;
    const a = KEYS[i], b = KEYS[i + 1], t = clamp((h - a.h) / (b.h - a.h), 0, 1);
    const w = this.weather, tr = force ? 1e3 : 0.5;
    this.rainAmt = damp(this.rainAmt, w === 'rain' ? 1 : 0, tr, dt + (force ? 1 : 0));
    this.fogAmt = damp(this.fogAmt, w === 'fog' ? 1 : w === 'rain' ? 0.35 : 0, tr, dt + (force ? 1 : 0));
    this.cloud = damp(this.cloud, w === 'clear' ? 0.15 : w === 'cloudy' ? 0.75 : 1, tr, dt + (force ? 1 : 0));
    this.wet = damp(this.wet, w === 'rain' ? 1 : 0, force ? 1e3 : w === 'rain' ? 0.25 : 0.04, dt + (force ? 1 : 0));
    uniforms.uWet.value = this.wet;
    const grey = this.cloud > 0.4 ? (this.cloud - 0.4) / 0.6 : 0;
    const top = this.u.top.value.copy(a.top).lerp(b.top, t), hor = this.u.hor.value.copy(a.hor).lerp(b.hor, t);
    const g1 = new THREE.Color(0x6b7885).multiplyScalar(0.25 + lerp(a.sunI, b.sunI, t) / 3.6);
    top.lerp(g1, grey * 0.8); hor.lerp(g1.clone().multiplyScalar(1.3), grey * 0.75);
    const sunI = lerp(a.sunI, b.sunI, t) * (1 - grey * 0.7), ambI = lerp(a.ambI, b.ambI, t) * (1 + grey * 0.12);
    this.sun.color.copy(a.sun).lerp(b.sun, t); this.sun.intensity = sunI;
    this.hemi.color.copy(a.amb).lerp(b.amb, t).lerp(WHITE, 0.6 * (1 - this.night)); this.hemi.intensity = ambI * 1.15;
    this.hemi.groundColor.copy(this.hemi.color).multiply(GROUND);
    // direction du soleil (ou de la lune la nuit)
    const day = h > 5.6 && h < 19.9;
    const ang = day ? ((h - 5.6) / 14.3) * Math.PI : ((((h + 24 - 19.9) % 24) / 9.7)) * Math.PI;
    const el = Math.max(0.16, Math.sin(ang)) * (day ? 1 : 0.8);
    const dir = this.u.sunDir.value.set(Math.cos(ang) * 0.8, el, 0.45 + (day ? 0 : 0.2)).normalize();
    this.u.sunCol.value.copy(this.sun.color).multiplyScalar(day ? 1 - grey * 0.85 : 0.5 * (1 - grey));
    this.night = 1 - smoothstep(0.5, 1.7, lerp(a.sunI, b.sunI, t));
    this.u.stars.value = this.night * (1 - this.cloud);
    this.u.cloud.value = this.cloud;
    const fogC = new THREE.Color().copy(a.fog).lerp(b.fog, t).lerp(g1.clone().multiplyScalar(1.25), grey * 0.7);
    this.scene.fog.color.copy(fogC);
    const far = this.q.drawDistance;
    this.scene.fog.near = lerp(far * 0.35, 12, this.fogAmt);
    this.scene.fog.far = lerp(far, 240, this.fogAmt * 0.92);
    this.renderer.setClearColor(fogC);
    this.sunDir = dir;
  }
  update(dt, cam, focus, vel) {
    this.hour = (this.hour + (dt * this.timeScale) / 60) % 24;
    this.apply(dt);
    this.u.time.value += dt;
    this.dome.position.copy(cam.position);
    this.sun.position.copy(focus).addScaledVector(this.sunDir, 160);
    this.sun.target.position.copy(focus);
    this.rain.visible = this.rainAmt > 0.02;
    if (this.rain.visible) { this.rainU.time.value += dt; this.rainU.cam.value.copy(cam.position); this.rainU.alpha.value = this.rainAmt; if (vel) this.rainU.vel.value.copy(vel); }
    // reflets : carte d'environnement régénérée seulement quand le ciel a sensiblement changé
    const key = Math.round(this.hour * 2) + Math.round(this.cloud * 4) * 100;
    if (key !== this.envHour) {
      this.envHour = key;
      const old = this.envTarget;
      this.envGround.material.color.copy(this.scene.fog.color).multiplyScalar(0.35);
      this.envTarget = this.pmrem.fromScene(this.envScene, 0, 0.1, 100);
      this.scene.environment = this.envTarget.texture;
      this.scene.environmentIntensity = 0.5 + 0.5 * (1 - this.night);
      if (old) old.dispose();
    }
  }
}
