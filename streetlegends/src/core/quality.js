// Préréglages graphiques et stratégie d'adaptation aux appareils modestes.
export const QUALITY = {
  low: { name: 'Bas', pr: 0.8, shadows: 0, shadowRange: 0, chunkRadius: 3, drawDistance: 540, terrainRes: 10, traffic: 9, peds: 10, particles: 350, lightPools: false, physical: false, skid: 300, carLights: false },
  medium: { name: 'Moyen', pr: 1.15, shadows: 1024, shadowRange: 55, chunkRadius: 4, drawDistance: 740, terrainRes: 12, traffic: 15, peds: 22, particles: 800, lightPools: true, physical: true, skid: 700, carLights: true },
  high: { name: 'Élevé', pr: 1.75, shadows: 2048, shadowRange: 75, chunkRadius: 5, drawDistance: 950, terrainRes: 16, traffic: 21, peds: 34, particles: 1400, lightPools: true, physical: true, skid: 1200, carLights: true },
  ultra: { name: 'Ultra', pr: 2.5, shadows: 4096, shadowRange: 95, chunkRadius: 6, drawDistance: 1150, terrainRes: 20, traffic: 27, peds: 46, particles: 2200, lightPools: true, physical: true, skid: 1800, carLights: true },
};
// Choix initial selon l'appareil (mémoire, cœurs, taille d'écran).
export function detectQuality() {
  const mem = navigator.deviceMemory || 4, cores = navigator.hardwareConcurrency || 4;
  const mobile = /Android|iPhone|iPad|iPod/i.test(navigator.userAgent) || (navigator.maxTouchPoints > 1 && Math.min(screen.width, screen.height) < 900);
  if (mobile) return mem <= 2 || cores <= 4 ? 'low' : mem >= 8 && cores >= 8 ? 'high' : 'medium';
  return mem >= 8 && cores >= 8 ? 'high' : 'medium';
}
// Résolution dynamique : maintient la cadence cible en ajustant l'échelle de rendu.
export class DynRes {
  constructor() { this.scale = 1; this.acc = 0; this.frames = 0; this.fps = 60; }
  tick(dt, target, enabled) {
    this.acc += dt; this.frames++;
    if (this.acc < 1.5) return false;
    this.fps = this.frames / this.acc; this.acc = 0; this.frames = 0;
    if (!enabled) return false;
    const old = this.scale;
    if (this.fps < target * 0.85) this.scale = Math.max(0.55, this.scale * 0.9);
    else if (this.fps > target * 0.97 && this.scale < 1) this.scale = Math.min(1, this.scale * 1.06);
    return old !== this.scale;
  }
}
