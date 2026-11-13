import { Config } from './Config.js';
import { bus } from './EventBus.js';
// Mesure le FPS moyen; baisse / monte le niveau de qualité automatiquement.
export class Quality {
  constructor() { this.acc = 0; this.n = 0; this.cool = 0; }
  get preset() { return Config.qualityPresets[Config.quality.level]; }
  setLevel(l) {
    l = Math.max(0, Math.min(Config.qualityPresets.length - 1, l));
    if (l === Config.quality.level) return;
    Config.quality.level = l; bus.emit('quality:changed', this.preset);
  }
  update(dt) {
    this.acc += dt; this.n++; this.cool -= dt;
    if (this.acc < 3) return;
    const fps = this.n / this.acc; this.acc = 0; this.n = 0;
    if (this.cool > 0) return;
    if (fps < 40) { this.setLevel(Config.quality.level - 1); this.cool = 6; }
    else if (fps > 58 && Config.quality.level < 2) { this.setLevel(Config.quality.level + 1); this.cool = 15; }
  }
}
