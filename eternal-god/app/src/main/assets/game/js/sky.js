// Ciel photographique : 8 ciels HDR réels (même lieu, du matin à la nuit) fondus selon l'heure.
// La direction et la couleur du soleil sont mesurées sur les photos ; l'éclairage d'environnement
// (reflets de l'or, lumière ambiante) est recalculé à partir du mélange des deux ciels voisins.
import * as THREE from 'three';
import { RGBELoader } from 'three/addons/loaders/RGBELoader.js';
import { lerp, clamp, smooth } from './core.js';

const BASE = 'img/sky/';

export class PhotoSky {
  async load(renderer, onEach) {
    this.renderer = renderer;
    this.meta = await (await fetch(BASE + 'sky.json')).json();
    const keys = this.meta.keys;
    const tl = new THREE.TextureLoader(), rl = new RGBELoader();
    for (const k of keys) {
      k.tex = await tl.loadAsync(BASE + k.key + '.jpg');
      k.tex.colorSpace = THREE.SRGBColorSpace;
      k.tex.mapping = THREE.EquirectangularReflectionMapping;
      k.tex.minFilter = THREE.LinearFilter; k.tex.generateMipmaps = false;
      k.hdr = await rl.loadAsync(BASE + k.key + '.hdr');
      k.hdr.mapping = THREE.EquirectangularReflectionMapping;
      if (onEach) onEach();
    }
    // directions du soleil pour les ciels sans soleil visible (aube, crépuscule) : dérivées des voisins
    const byKey = Object.fromEntries(keys.map((k) => [k.key, k]));
    const below = (src, deg) => { const v = new THREE.Vector3().fromArray(src.sun); const e = Math.asin(v.y) - THREE.MathUtils.degToRad(deg); const hz = Math.hypot(v.x, v.z); return [v.x / hz * Math.cos(e), Math.sin(e), v.z / hz * Math.cos(e)]; };
    byKey.dawn.sun = below(byKey.sunrise, 7);
    byKey.dusk.sun = below(byKey.sunset, 10);
    byKey.night.moon = byKey.night.sun; byKey.night.sun = [0, -1, 0];
    for (const k of keys) { k.sunV = new THREE.Vector3().fromArray(k.sun).normalize(); k.hor = new THREE.Color().setRGB(...k.horizon, THREE.SRGBColorSpace); }
    this.keys = keys.sort((a, b) => a.t - b.t);

    this.mesh = new THREE.Mesh(new THREE.SphereGeometry(4500, 48, 24), new THREE.ShaderMaterial({
      side: THREE.BackSide, depthWrite: false, fog: false,
      uniforms: { tA: { value: null }, tB: { value: null }, uMix: { value: 0 }, uStorm: { value: 0 }, uFlash: { value: 0 }, uBright: { value: 1 } },
      vertexShader: `varying vec3 vDir; void main(){ vDir = normalize((modelMatrix * vec4(position, 0.0)).xyz);
        vec4 p = projectionMatrix * modelViewMatrix * vec4(position, 1.0); gl_Position = p.xyww; }`,
      fragmentShader: `uniform sampler2D tA, tB; uniform float uMix, uStorm, uFlash, uBright; varying vec3 vDir;
        #include <common>
        void main(){ vec3 d = normalize(vDir);
          vec2 uv = vec2(atan(d.z, d.x) * RECIPROCAL_PI2 + 0.5, asin(clamp(d.y, -1.0, 1.0)) * RECIPROCAL_PI + 0.5);
          vec3 c = mix(texture2D(tA, uv).rgb, texture2D(tB, uv).rgb, uMix);
          float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
          c = mix(c, vec3(l) * vec3(0.78, 0.8, 0.86) * 0.55, uStorm);
          c += vec3(0.8, 0.85, 1.0) * uFlash;
          gl_FragColor = vec4(c * uBright, 1.0);
          #include <tonemapping_fragment>
          #include <colorspace_fragment>
        }`,
    }));
    this.mesh.frustumCulled = false;
    this.mesh.renderOrder = -10;

    // mélange des HDR pour l'éclairage d'environnement
    this.pmrem = new THREE.PMREMGenerator(renderer);
    this.blendRT = new THREE.WebGLRenderTarget(256, 128, { type: THREE.HalfFloatType, depthBuffer: false });
    this.blendRT.texture.mapping = THREE.EquirectangularReflectionMapping;
    this.blendScene = new THREE.Scene();
    this.blendCam = new THREE.OrthographicCamera(-1, 1, 1, -1, 0, 1);
    this.blendMat = new THREE.ShaderMaterial({
      uniforms: { tA: { value: null }, tB: { value: null }, uMix: { value: 0 }, uStorm: { value: 0 } },
      vertexShader: 'varying vec2 vUv; void main(){ vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }',
      fragmentShader: `uniform sampler2D tA, tB; uniform float uMix, uStorm; varying vec2 vUv;
        void main(){ vec3 c = mix(texture2D(tA, vUv).rgb, texture2D(tB, vUv).rgb, uMix);
          float l = dot(c, vec3(0.2126, 0.7152, 0.0722)); c = mix(c, vec3(l) * 0.5, uStorm); gl_FragColor = vec4(c, 1.0); }`,
    });
    this.blendScene.add(new THREE.Mesh(new THREE.PlaneGeometry(2, 2), this.blendMat));
    this.sunDir = new THREE.Vector3(0, 1, 0);
    this.moonDir = new THREE.Vector3().fromArray(byKey.night.moon).normalize();
    this.fog = new THREE.Color();
    this.sunColor = new THREE.Color();
    this.lastEnv = -1;
  }

  // renvoie les deux ciels voisins et le facteur de mélange pour une heure t (0..1)
  segment(t) {
    const K = this.keys, n = K.length;
    for (let i = 0; i < n; i++) {
      const a = K[i], b = K[(i + 1) % n];
      const ta = a.t, tb = i + 1 < n ? b.t : b.t + 1;
      let tt = t; if (tt < ta) tt += 1;
      if (tt >= ta && tt < tb) {
        let f = (tt - ta) / (tb - ta);
        // la nuit dure : on reste sur le ciel nocturne la majeure partie de l'intervalle
        if (a.key === 'night') f = smooth(0.8, 1.0, f);
        if (b.key === 'night') f = smooth(0.0, 0.25, f);
        return [a, b, f];
      }
    }
    return [K[0], K[1], 0];
  }

  update(t, storm, flash, camera, scene) {
    const [a, b, f] = this.segment(t);
    const u = this.mesh.material.uniforms;
    u.tA.value = a.tex; u.tB.value = b.tex; u.uMix.value = f; u.uStorm.value = storm * 0.85; u.uFlash.value = flash;
    this.mesh.position.copy(camera.position);
    // soleil : interpolation sphérique entre directions mesurées
    const qa = new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 0, 1), a.sunV);
    const qb = new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 0, 1), b.sunV);
    this.sunDir.set(0, 0, 1).applyQuaternion(qa.slerp(qb, f)).normalize();
    const elev = this.sunDir.y;
    this.day = smooth(-0.05, 0.25, elev);
    this.night = 1 - smooth(-0.2, 0.0, elev);
    this.dusk = clamp(1 - Math.abs(elev - 0.06) / 0.2, 0, 1);
    this.fog.copy(a.hor).lerp(b.hor, f);
    const sa = new THREE.Color().fromArray(a.sunColor), sb = new THREE.Color().fromArray(b.sunColor);
    this.sunColor.copy(sa).lerp(sb, f);
    // éclairage d'environnement recalculé quand l'heure a assez changé
    const key = a.key + b.key + Math.round(f * 20) + Math.round(storm * 5);
    if (key !== this.lastEnv) {
      this.lastEnv = key;
      this.blendMat.uniforms.tA.value = a.hdr; this.blendMat.uniforms.tB.value = b.hdr;
      this.blendMat.uniforms.uMix.value = f; this.blendMat.uniforms.uStorm.value = storm * 0.7;
      const r = this.renderer, prev = r.getRenderTarget(), tm = r.toneMapping;
      r.toneMapping = THREE.NoToneMapping;
      r.setRenderTarget(this.blendRT); r.render(this.blendScene, this.blendCam); r.setRenderTarget(prev);
      r.toneMapping = tm;
      if (this.envRT) this.envRT.dispose();
      this.envRT = this.pmrem.fromEquirectangular(this.blendRT.texture);
      scene.environment = this.envRT.texture;
    }
  }
}
