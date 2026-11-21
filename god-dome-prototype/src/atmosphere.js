import * as THREE from 'three';
import { Pass, FullScreenQuad } from 'three/addons/postprocessing/Pass.js';
import { U } from './shared.js';

/*
 * PASSE « ATMOSPHÈRE » (post-traitement à partir du tampon de profondeur)
 *  1. Occlusion ambiante en espace écran (contacts sombres au pied des murs, des arbres, dans l'herbe).
 *  2. Brouillard de hauteur avec diffusion solaire : l'air s'éclaire en direction du soleil, s'épaissit au fond des vallées.
 *  3. Rayons crépusculaires : flou radial des zones de ciel visibles vers le soleil → faisceaux à travers le feuillage.
 * Active seulement à l'intérieur du dôme (en vue Créateur, le monde est une maquette nette).
 */
export class AtmospherePass extends Pass {
  constructor(camera, { aoSamples = 8, raySamples = 24 } = {}) {
    super();
    this.camera = camera;
    this.material = new THREE.ShaderMaterial({
      defines: { AO_SAMPLES: aoSamples, RAY_SAMPLES: raySamples },
      uniforms: {
        tDiffuse: { value: null }, tDepth: { value: null },
        uNear: { value: 0.1 }, uFar: { value: 1000 },
        uProjInv: { value: new THREE.Matrix4() }, uCamWorld: { value: new THREE.Matrix4() }, uProj: { value: new THREE.Matrix4() },
        uRes: { value: new THREE.Vector2(1, 1) },
        uSunDir: U.uSunDir, uSunColor: U.uSunColor, uHorizon: U.uSkyHorizon, uNight: U.uNight,
        uSunUV: { value: new THREE.Vector3(0.5, 0.5, 0) },
        uInside: { value: 0 }, uAO: { value: 0.9 }, uRays: { value: 0.55 }, uFog: { value: 0.0045 }, uTime: U.uTime,
      },
      vertexShader: `varying vec2 vUv; void main(){ vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }`,
      fragmentShader: /* glsl */`
        #include <packing>
        uniform sampler2D tDiffuse, tDepth;
        uniform float uNear, uFar, uInside, uAO, uRays, uFog, uNight, uTime;
        uniform mat4 uProjInv, uCamWorld, uProj;
        uniform vec2 uRes; uniform vec3 uSunDir, uSunColor, uHorizon, uSunUV;
        varying vec2 vUv;
        float rawDepth(vec2 uv){ return texture2D(tDepth, uv).x; }
        float viewZ(float d){ return perspectiveDepthToViewZ(d, uNear, uFar); }
        vec3 viewPos(vec2 uv, float d){ vec4 p = uProjInv * vec4(vec3(uv, d) * 2.0 - 1.0, 1.0); return p.xyz / p.w; }
        float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
        void main(){
          vec4 col = texture2D(tDiffuse, vUv);
          if (uInside < 0.01) { gl_FragColor = col; return; }
          float d = rawDepth(vUv);
          bool sky = d >= 0.99999;
          vec3 vp = viewPos(vUv, d);
          vec3 wp = (uCamWorld * vec4(vp, 1.0)).xyz;
          vec3 camP = (uCamWorld * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
          vec3 rd = normalize(wp - camP);
          float dist = sky ? 400.0 : length(vp);

          // --- 1. occlusion ambiante (disque de Vogel, rotation aléatoire par pixel) ---
          float ao = 1.0;
          if (!sky && dist < 60.0) {
            float vz = vp.z;
            float rad = clamp(0.55 * uProj[1][1] * 0.5 * uRes.y / -vz, 3.0, 48.0);
            float rot = hash(vUv * uRes + fract(uTime)) * 6.2831;
            float occ = 0.0;
            for (int i = 0; i < AO_SAMPLES; i++) {
              float fi = float(i) + 0.5;
              float r = sqrt(fi / float(AO_SAMPLES));
              float a = fi * 2.39996 + rot;
              vec2 off = vec2(cos(a), sin(a)) * r * rad / uRes;
              float sz = viewZ(rawDepth(vUv + off));
              float diff = sz - vz; // > 0 : l'échantillon est devant
              occ += smoothstep(0.03, 0.35, diff) * (1.0 - smoothstep(0.9, 2.5, diff));
            }
            ao = 1.0 - uAO * occ / float(AO_SAMPLES) * (1.0 - smoothstep(30.0, 60.0, dist));
          }
          col.rgb *= ao;

          // --- 2. brouillard de hauteur + diffusion solaire ---
          float sunAmt = pow(max(dot(rd, uSunDir), 0.0), 6.0);
          vec3 fogCol = mix(uHorizon, uSunColor * 1.25, sunAmt * 0.6 * (1.0 - uNight));
          if (!sky) {
            float hAvg = max(0.0, (camP.y + wp.y) * 0.5);
            float f = 1.0 - exp(-dist * uFog * exp(-hAvg * 0.045));
            col.rgb = mix(col.rgb, fogCol, f * uInside);
          }

          // --- 3. rayons crépusculaires ---
          if (uSunUV.z > 0.0 && uRays > 0.0) {
            vec2 delta = (uSunUV.xy - vUv) / float(RAY_SAMPLES) * 0.85;
            vec2 uv = vUv + delta * hash(vUv * uRes);
            float illum = 0.0, decay = 1.0;
            for (int i = 0; i < RAY_SAMPLES; i++) {
              uv += delta;
              float s = step(0.99999, rawDepth(clamp(uv, 0.001, 0.999)));
              float g = 1.0 - smoothstep(0.0, 0.55, length((uv - uSunUV.xy) * vec2(uRes.x / uRes.y, 1.0)));
              illum += s * g * decay;
              decay *= 0.965;
            }
            illum /= float(RAY_SAMPLES);
            col.rgb += uSunColor * illum * uRays * uSunUV.z * (1.0 - uNight) * uInside;
          }
          gl_FragColor = col;
        }`,
    });
    this.fsQuad = new FullScreenQuad(this.material);
  }
  setSize(w, h) { this.material.uniforms.uRes.value.set(w, h); }
  update(sunWorldPos, inside) {
    const u = this.material.uniforms, c = this.camera;
    u.uNear.value = c.near; u.uFar.value = c.far;
    u.uProjInv.value.copy(c.projectionMatrixInverse); u.uProj.value.copy(c.projectionMatrix); u.uCamWorld.value.copy(c.matrixWorld);
    u.uInside.value = inside;
    const p = sunWorldPos.clone().project(c);
    const facing = new THREE.Vector3().subVectors(sunWorldPos, c.position).normalize().dot(c.getWorldDirection(new THREE.Vector3()));
    u.uSunUV.value.set(p.x * 0.5 + 0.5, p.y * 0.5 + 0.5, THREE.MathUtils.smoothstep(facing, 0.1, 0.6));
  }
  render(renderer, writeBuffer, readBuffer) {
    this.material.uniforms.tDiffuse.value = readBuffer.texture;
    this.material.uniforms.tDepth.value = readBuffer.depthTexture;
    renderer.setRenderTarget(this.renderToScreen ? null : writeBuffer);
    this.fsQuad.render(renderer);
  }
}
