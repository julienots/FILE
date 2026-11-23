// Rendu : WebGL2, environnement de reflets, ombres douces, post-traitement (bloom + passe
// stylisée maison : aberration chromatique, glitch, flash, vignette, grain, flou radial de vitesse,
// déformation, profondeur de champ par bandes), tremblements de caméra.
import * as THREE from 'three';
import { EffectComposer } from './../lib/addons/postprocessing/EffectComposer.js';
import { RenderPass } from './../lib/addons/postprocessing/RenderPass.js';
import { ShaderPass } from './../lib/addons/postprocessing/ShaderPass.js';
import { UnrealBloomPass } from './../lib/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from './../lib/addons/postprocessing/OutputPass.js';
import { RoomEnvironment } from './../lib/addons/environments/RoomEnvironment.js';
import { rand, damp } from './util.js';

const StylizeShader = {
  uniforms: {
    tDiffuse: { value: null }, uTime: { value: 0 }, uRes: { value: new THREE.Vector2(1, 1) },
    uAberration: { value: 0.0015 }, uGlitch: { value: 0 }, uFlash: { value: new THREE.Vector4(1, 0, 0, 0) },
    uVignette: { value: 0.35 }, uGrain: { value: 0.035 }, uRadial: { value: 0 }, uWarp: { value: 0 },
    uDark: { value: 0 }, uDof: { value: 0 }, uScan: { value: 0 }, uRed: { value: 0 }, uInvert: { value: 0 },
  },
  vertexShader: /* glsl */`
    varying vec2 vUv;
    void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }`,
  fragmentShader: /* glsl */`
    uniform sampler2D tDiffuse; uniform float uTime; uniform vec2 uRes;
    uniform float uAberration, uGlitch, uVignette, uGrain, uRadial, uWarp, uDark, uDof, uScan, uRed, uInvert;
    uniform vec4 uFlash;
    varying vec2 vUv;
    float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898,78.233))) * 43758.5453); }
    vec3 sampleAb(vec2 uv, float ab){
      vec2 dir = uv - 0.5;
      float r = texture2D(tDiffuse, uv + dir * ab).r;
      float g = texture2D(tDiffuse, uv).g;
      float b = texture2D(tDiffuse, uv - dir * ab).b;
      return vec3(r,g,b);
    }
    void main(){
      vec2 uv = vUv;
      // déformation (onde + barillet) pendant les apparitions
      if (uWarp > 0.0) {
        vec2 d = uv - 0.5; float l = length(d);
        uv += d * (l * l) * uWarp * 0.6;
        uv.x += sin(uv.y * 24.0 + uTime * 9.0) * 0.006 * uWarp;
      }
      // glitch : bandes horizontales décalées
      float ab = uAberration;
      if (uGlitch > 0.0) {
        float band = floor(uv.y * 28.0 + floor(uTime * 18.0) * 3.0);
        float h = hash(vec2(band, floor(uTime * 24.0)));
        if (h < uGlitch * 0.55) uv.x += (hash(vec2(band, 3.1)) - 0.5) * 0.18 * uGlitch;
        ab += uGlitch * 0.02 * hash(vec2(floor(uTime * 30.0), 1.0));
      }
      vec3 col = sampleAb(uv, ab);
      // flou radial (vitesse)
      if (uRadial > 0.0) {
        vec3 acc = col; vec2 dir = (uv - 0.5);
        for (int i = 1; i < 7; i++) { acc += texture2D(tDiffuse, uv - dir * float(i) * 0.012 * uRadial).rgb; }
        col = mix(col, acc / 7.0, smoothstep(0.05, 0.45, length(uv - 0.5)));
      }
      // profondeur de champ cinématique (flou hors de la bande nette centrale)
      if (uDof > 0.0) {
        float blur = smoothstep(0.12, 0.5, abs(uv.y - 0.5)) * uDof;
        vec2 px = 1.0 / uRes * 3.0 * blur;
        vec3 b = texture2D(tDiffuse, uv + vec2(px.x, 0)).rgb + texture2D(tDiffuse, uv - vec2(px.x, 0)).rgb
               + texture2D(tDiffuse, uv + vec2(0, px.y)).rgb + texture2D(tDiffuse, uv - vec2(0, px.y)).rgb
               + texture2D(tDiffuse, uv + px).rgb + texture2D(tDiffuse, uv - px).rgb;
        col = mix(col, b / 6.0, clamp(blur, 0.0, 1.0));
      }
      if (uGlitch > 0.3) {
        float blk = step(0.985 - uGlitch * 0.04, hash(floor(uv * vec2(18.0, 36.0)) + floor(uTime * 12.0)));
        col = mix(col, vec3(col.g, col.b, col.r) * 1.4, blk);
      }
      // lignes de balayage (écran cathodique)
      if (uScan > 0.0) col *= 1.0 - uScan * (0.5 + 0.5 * sin(uv.y * uRes.y * 1.6)) * 0.35;
      // teinte rouge d'angoisse
      if (uRed > 0.0) { float lum = dot(col, vec3(0.299,0.587,0.114)); col = mix(col, vec3(lum * 1.5, lum * 0.25, lum * 0.25), uRed); }
      if (uInvert > 0.0) col = mix(col, 1.0 - col, uInvert);
      // grain
      col += (hash(uv * uRes + fract(uTime) * 100.0) - 0.5) * uGrain;
      // vignette
      float v = smoothstep(0.85, 0.2, length((vUv - 0.5) * vec2(1.0, 0.9)));
      col *= mix(1.0, v, uVignette);
      // flash
      col = mix(col, uFlash.rgb, clamp(uFlash.a, 0.0, 1.0));
      col *= 1.0 - uDark;
      gl_FragColor = vec4(col, 1.0);
    }`,
};

export class FX {
  constructor(canvas, settings) {
    this.settings = settings;
    const r = this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance', alpha: false });
    r.outputColorSpace = THREE.SRGBColorSpace;
    r.toneMapping = THREE.NeutralToneMapping;
    r.toneMappingExposure = 1.0;
    r.shadowMap.enabled = true;
    r.shadowMap.type = THREE.PCFSoftShadowMap;
    const pm = new THREE.PMREMGenerator(r);
    this.envMap = pm.fromScene(new RoomEnvironment(), 0.04).texture;
    pm.dispose();
    this.scene = null; this.camera = null;
    this.composer = new EffectComposer(r);
    this.renderPass = new RenderPass(new THREE.Scene(), new THREE.PerspectiveCamera());
    this.bloom = new UnrealBloomPass(new THREE.Vector2(256, 256), 0.55, 0.5, 0.82);
    this.stylize = new ShaderPass(StylizeShader);
    this.output = new OutputPass();
    this.composer.addPass(this.renderPass);
    this.composer.addPass(this.bloom);
    this.composer.addPass(this.stylize);
    this.composer.addPass(this.output);
    this.u = this.stylize.uniforms;
    // états animés
    this.shakeAmt = 0; this.shakeT = 0; this.flashA = 0; this.flashColor = new THREE.Color(1, 1, 1);
    this.glitchBase = 0; this.glitchPulse = 0; this.aberrBase = 0.0015; this.aberrPulse = 0;
    this.warpPulse = 0; this.warpBase = 0;
    this.darkTarget = 0; this.dark = 0;
    this.radialTarget = 0; this.dofTarget = 0; this.redTarget = 0;
    this.fpsAcc = 0; this.fpsFrames = 0; this.fps = 60;
    this.applyQuality();
    this.resize();
  }

  applyQuality() {
    const q = this.quality();
    const dpr = window.devicePixelRatio || 1;
    this.pixelRatio = q === 'quality' ? Math.min(dpr, 2) : q === 'performance' ? Math.min(dpr, 1.1) : Math.min(dpr, 1.6);
    this.renderer.setPixelRatio(this.pixelRatio);
    this.bloom.enabled = q !== 'performance';
    this.renderer.shadowMap.enabled = q !== 'performance' || true;
    this.shadowSize = q === 'quality' ? 2048 : q === 'performance' ? 512 : 1024;
    this.resize();
  }
  quality() {
    const s = this.settings.quality;
    if (s === 'auto') return this.autoQ || 'balanced';
    return s;
  }

  resize() {
    const w = window.innerWidth, h = window.innerHeight;
    this.renderer.setSize(w, h, false);
    this.composer.setPixelRatio(this.pixelRatio);
    this.composer.setSize(w, h);
    this.bloom.resolution.set(w / 2, h / 2);
    this.u.uRes.value.set(w * this.pixelRatio, h * this.pixelRatio);
    if (this.camera) { this.camera.aspect = w / h; this.camera.updateProjectionMatrix(); }
    this.onResize && this.onResize(w, h);
  }

  setView(scene, camera) {
    this.scene = scene; this.camera = camera;
    this.renderPass.scene = scene; this.renderPass.camera = camera;
    camera.aspect = window.innerWidth / window.innerHeight; camera.updateProjectionMatrix();
    if (!scene.environment) scene.environment = this.envMap;
  }

  // ——— effets déclenchés ———
  shake(amount, dur = 0.4) {
    const k = this.settings.reduceShake ? 0.2 : 1;
    this.shakeAmt = Math.max(this.shakeAmt, amount * k); this.shakeT = Math.max(this.shakeT, dur);
  }
  flash(color = 0xffffff, amount = 0.8) {
    const k = this.settings.reduceFlash ? 0.25 : 1;
    this.flashColor.set(color); this.flashA = Math.max(this.flashA, amount * k);
  }
  glitch(amount = 0.8, keep = false) { if (keep) this.glitchBase = amount; else this.glitchPulse = Math.max(this.glitchPulse, amount); }
  aberration(amount) { this.aberrPulse = Math.max(this.aberrPulse, this.settings.reduceFlash ? amount * 0.4 : amount); }
  warp(amount, keep = false) { if (keep) this.warpBase = amount; else this.warpPulse = Math.max(this.warpPulse, amount); }
  fade(to) { this.darkTarget = to; }
  setDark(v) { this.dark = this.darkTarget = v; }
  radial(v) { this.radialTarget = v; }
  dof(v) { this.dofTarget = v; }
  red(v) { this.redTarget = v; }
  clearAll() {
    this.glitchBase = 0; this.glitchPulse = 0; this.warpBase = 0; this.warpPulse = 0; this.aberrPulse = 0;
    this.radialTarget = 0; this.dofTarget = 0; this.redTarget = 0; this.flashA = 0; this.shakeAmt = 0; this.u.uScan.value = 0; this.u.uInvert.value = 0;
  }

  render(dt, t) {
    if (!this.scene || !this.camera) return;
    // auto-qualité : baisse si la fréquence chute
    this.fpsAcc += dt; this.fpsFrames++;
    if (this.fpsAcc > 2) {
      this.fps = this.fpsFrames / this.fpsAcc; this.fpsAcc = 0; this.fpsFrames = 0;
      if (this.settings.quality === 'auto') {
        const prev = this.autoQ || 'balanced';
        if (this.fps < 40 && prev !== 'performance') this.autoQ = prev === 'quality' ? 'balanced' : 'performance';
        if (prev !== this.autoQ) this.applyQuality();
      }
    }
    const u = this.u;
    u.uTime.value = t;
    this.glitchPulse = damp(this.glitchPulse, 0, 6, dt);
    this.aberrPulse = damp(this.aberrPulse, 0, 5, dt);
    this.warpPulse = damp(this.warpPulse, 0, 4, dt);
    this.flashA = damp(this.flashA, 0, 7, dt);
    this.dark = damp(this.dark, this.darkTarget, 4, dt);
    u.uGlitch.value = Math.min(1, this.glitchBase + this.glitchPulse);
    u.uAberration.value = this.aberrBase + this.aberrPulse;
    u.uWarp.value = this.warpBase + this.warpPulse;
    u.uFlash.value.set(this.flashColor.r, this.flashColor.g, this.flashColor.b, this.flashA);
    u.uDark.value = this.dark;
    u.uRadial.value = damp(u.uRadial.value, this.radialTarget, 4, dt);
    u.uDof.value = damp(u.uDof.value, this.dofTarget, 3, dt);
    u.uRed.value = damp(u.uRed.value, this.redTarget, 3, dt);

    const cam = this.camera;
    let ox = 0, oy = 0, oz = 0;
    if (this.shakeT > 0) {
      this.shakeT -= dt;
      const a = this.shakeAmt * Math.max(0, Math.min(1, this.shakeT * 3));
      ox = rand(-a, a); oy = rand(-a, a); oz = rand(-a, a) * 0.3;
      cam.position.x += ox; cam.position.y += oy; cam.position.z += oz;
    } else this.shakeAmt = 0;
    this.composer.render(dt);
    cam.position.x -= ox; cam.position.y -= oy; cam.position.z -= oz;
  }
}
