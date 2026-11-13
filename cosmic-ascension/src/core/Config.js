export const Config = {
  version: '0.1.0',
  saveKey: 'cosmic-ascension-save-v1',
  // Qualité adaptative: ajustée automatiquement par Quality.js selon le FPS mesuré.
  quality: { level: 2 }, // 0 bas · 1 moyen · 2 haut
  qualityPresets: [
    { pixelRatio: 1.0,  bloom: false, planetDetail: 14, surfaceDetail: 30, stars: 2500,  shadows: false, particles: 0.4 },
    { pixelRatio: 1.25, bloom: true,  planetDetail: 20, surfaceDetail: 42, stars: 5000,  shadows: false, particles: 0.7 },
    { pixelRatio: 1.75, bloom: true,  planetDetail: 28, surfaceDetail: 54, stars: 9000,  shadows: false, particles: 1.0 },
  ],
};
