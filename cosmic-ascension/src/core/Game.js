import { createRenderer } from '../render/createRenderer.js';
import { Input } from './Input.js';
import { Quality } from './Quality.js';
import { bus } from './EventBus.js';
import { shaderTime } from '../render/shaders/Toon.js';
// Boucle principale + gestion des scènes. ctx = { renderer, goto(name, params), scenes, quality }
export class Game {
  constructor(canvas) {
    this.ctx = { renderer: createRenderer(canvas), scenes: {}, quality: new Quality(), goto: (n, p) => this.goto(n, p), transition: (n, p, o) => this.transition(n, p, o) };
    this.current = null; this.last = performance.now(); this.t = 0;
    bus.on('quality:changed', () => this.ctx.renderer.applyQuality());
  }
  register(name, SceneClass) { this.ctx.scenes[name] = new SceneClass(this.ctx); }
  goto(name, params) {
    this.current?.exit(); this.current = this.ctx.scenes[name]; this.currentName = name; this.current.enter(params); bus.emit('scene:changed', { name, params });
  }
  // Transition avec fondu coloré (atterrissage, décollage, warp). Appelable via ctx.transition(name, params, {color, out, in}).
  transition(name, params, { color = '#ffffff', out = .45, in: fin = .7 } = {}) {
    if (this._fading) return; this._fading = true;
    const f = this._fade || (this._fade = Object.assign(document.createElement('div'), { style: 'position:fixed;inset:0;pointer-events:none;opacity:0;z-index:50' }));
    if (!f.parentNode) document.body.appendChild(f);
    f.style.background = color; f.style.transition = `opacity ${out}s ease-in`; requestAnimationFrame(() => { f.style.opacity = 1; });
    setTimeout(() => { this.goto(name, params); f.style.transition = `opacity ${fin}s ease-out`; f.style.opacity = 0; this._fading = false; }, out * 1000 + 30);
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
