import { createRenderer } from '../render/createRenderer.js';
import { Input } from './Input.js';
import { Quality } from './Quality.js';
import { bus } from './EventBus.js';
import { shaderTime } from '../render/shaders/Toon.js';
// Boucle principale + gestion des scènes. ctx = { renderer, goto(name, params), scenes, quality }
export class Game {
  constructor(canvas) {
    this.ctx = { renderer: createRenderer(canvas), scenes: {}, quality: new Quality(), goto: (n, p) => this.goto(n, p) };
    this.current = null; this.last = performance.now(); this.t = 0;
    bus.on('quality:changed', () => this.ctx.renderer.applyQuality());
  }
  register(name, SceneClass) { this.ctx.scenes[name] = new SceneClass(this.ctx); }
  goto(name, params) {
    this.current?.exit(); this.current = this.ctx.scenes[name]; this.currentName = name; this.current.enter(params); bus.emit('scene:changed', { name, params });
  }
  start() { this.ctx.renderer.resize(); requestAnimationFrame(this._loop.bind(this)); }
  _loop(now) {
    const dt = Math.min(0.05, (now - this.last) / 1000); this.last = now; this.t += dt; shaderTime.value = this.t;
    const sc = this.current;
    if (sc) {
      const cam = sc.camera; cam.aspect = innerWidth / innerHeight; cam.updateProjectionMatrix();
      sc.update(dt, this.t); this.ctx.renderer.render(sc.scene, sc.camera, dt);
    }
    Input.endFrame(); this.ctx.quality.update(dt); requestAnimationFrame(this._loop.bind(this));
  }
}
