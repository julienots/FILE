// Pool d'objets générique (particules, projectiles, ennemis, textes flottants…).
export class Pool {
  constructor(factory, reset = () => {}, prewarm = 0) {
    this.factory = factory; this.reset = reset; this.free = [];
    for (let i = 0; i < prewarm; i++) this.free.push(factory());
  }
  get() { return this.free.pop() || this.factory(); }
  release(o) { this.reset(o); this.free.push(o); }
}
