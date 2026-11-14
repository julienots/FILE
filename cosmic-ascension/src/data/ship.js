// Atelier du vaisseau (B1/B4). Niveaux 0..max; cost(n) = coût pour passer au niveau n+1.
// engine → State.ship.level (vitesse lue par A) · hull → réduit les dégâts reçus en espace · cargo → + poussière par orbe
export const SHIP_UPGRADES = {
  engine: { name: 'Moteur stellaire', glyph: '➤', max: 4, per: 'vitesse +1 niveau', cost: n => ({ cosmicDust: 60 + n * 90, scrapAlloy: 2 + n * 2, ...(n >= 2 ? { voltShard: n } : {}) }) },
  hull:   { name: 'Coque renforcée',  glyph: '⬢', max: 4, per: '−15 % dégâts en vol', cost: n => ({ cosmicDust: 50 + n * 80, starIron: 1 + n, ...(n >= 2 ? { frostCrystal: n } : {}) }) },
  cargo:  { name: 'Soute à poussière',glyph: '✧', max: 3, per: '+1 poussière par orbe', cost: n => ({ cosmicDust: 80 + n * 120, sporeSilk: 1 + n }) },
};
