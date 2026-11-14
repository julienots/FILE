import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { ShaderPass } from 'three/examples/jsm/postprocessing/ShaderPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';
import { Config } from '../core/Config.js';
// Renderer + post-process (bloom half-res, désactivable). Retourne {renderer, render(scene,camera), resize(), applyQuality()}.
export function createRenderer(canvas) {
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance', alpha: false });
  renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = 1.05;
  let composer, renderPass, bloom, w = 1, h = 1;
  // Passe d'étalonnage: vignette + saturation + aberration chromatique pilotable (api.fx.aberration: boost, coups, cinématiques).
  const grade = new ShaderPass({ uniforms: { tDiffuse: { value: null }, uAb: { value: 0 }, uSat: { value: 1.12 }, uVig: { value: .42 } },
    vertexShader: 'varying vec2 vUv; void main(){ vUv=uv; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.); }',
    fragmentShader: `uniform sampler2D tDiffuse; uniform float uAb,uSat,uVig; varying vec2 vUv;
      void main(){ vec2 c=vUv-.5; float r2=dot(c,c); vec2 o=c*r2*uAb*.12; vec3 col=vec3(texture2D(tDiffuse,vUv+o).r,texture2D(tDiffuse,vUv).g,texture2D(tDiffuse,vUv-o).b);
        float l=dot(col,vec3(.299,.587,.114)); col=mix(vec3(l),col,uSat); col*=1.-r2*uVig*1.6; gl_FragColor=vec4(col,1.); }` });
  const api = {
    renderer, fx: { aberration: 0 },
    setScene(scene, camera) {
      renderPass = new RenderPass(scene, camera);
      bloom = new UnrealBloomPass(new THREE.Vector2(w / 2, h / 2), 0.55, 0.5, 0.9);
      composer = new EffectComposer(renderer); composer.addPass(renderPass); composer.addPass(bloom); composer.addPass(grade); composer.addPass(new OutputPass());
      api.applyQuality();
    },
    applyQuality() {
      const q = Config.qualityPresets[Config.quality.level];
      renderer.setPixelRatio(Math.min(devicePixelRatio, q.pixelRatio));
      renderer.shadowMap.enabled = q.shadows; if (bloom) bloom.enabled = q.bloom; api.resize();
    },
    resize() {
      w = innerWidth; h = innerHeight; renderer.setSize(w, h, false);
      if (composer) { composer.setPixelRatio(renderer.getPixelRatio()); composer.setSize(w, h); bloom.resolution.set(w / 2, h / 2); }
    },
    render(scene, camera, dt) {
      if (!composer || renderPass.scene !== scene || renderPass.camera !== camera) api.setScene(scene, camera);
      grade.uniforms.uAb.value += (api.fx.aberration - grade.uniforms.uAb.value) * .2; api.fx.aberration *= .9;
      composer.render(dt);
    },
  };
  addEventListener('resize', api.resize);
  return api;
}
