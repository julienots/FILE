import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';
import { Config } from '../core/Config.js';
// Renderer + post-process (bloom half-res, désactivable). Retourne {renderer, render(scene,camera), resize(), applyQuality()}.
export function createRenderer(canvas) {
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance', alpha: false });
  renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = 1.05;
  let composer, renderPass, bloom, w = 1, h = 1;
  const api = {
    renderer,
    setScene(scene, camera) {
      renderPass = new RenderPass(scene, camera);
      bloom = new UnrealBloomPass(new THREE.Vector2(w / 2, h / 2), 0.55, 0.5, 0.9);
      composer = new EffectComposer(renderer); composer.addPass(renderPass); composer.addPass(bloom); composer.addPass(new OutputPass());
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
      composer.render(dt);
    },
  };
  addEventListener('resize', api.resize);
  return api;
}
