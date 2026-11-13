// Capacités (B: coût/cooldown/dégâts; A: VFX via `hero:ability {id}`). range/radius en m (× scale du héros).
export const ABILITIES = {
  hop:        { name: 'Bond',            icon: '⤒', cost: 0,   cd: 0,   dmg: 0,   desc: 'Saut de base.' },
  dash:       { name: 'Ruée',            icon: '➤', cost: 10,  cd: 1.2, dmg: 8,   range: 6,  desc: 'Esquive rapide, invulnérable un instant.' },
  starbolt:   { name: 'Éclat Stellaire', icon: '✦', cost: 15,  cd: 0.8, dmg: 25,  range: 30, desc: 'Projectile d’étoile à tête chercheuse.' },
  shockwave:  { name: 'Onde Nova',       icon: '◎', cost: 30,  cd: 4,   dmg: 45,  radius: 10, desc: 'Onde de choc circulaire qui repousse.' },
  gravity:    { name: 'Puits Gravitique',icon: '◉', cost: 45,  cd: 7,   dmg: 20,  radius: 14, desc: 'Attire et immobilise les ennemis.' },
  supernova:  { name: 'Supernova',       icon: '☀', cost: 80,  cd: 15,  dmg: 160, radius: 25, desc: 'Explosion d’étoile dévastatrice.' },
  nebulaVeil: { name: 'Voile Nébuleux',  icon: '☁', cost: 60,  cd: 12,  dmg: 0,   radius: 20, desc: 'Brume qui rend invisible et soigne.' },
  blackHole:  { name: 'Trou Noir',       icon: '●', cost: 100, cd: 18,  dmg: 240, radius: 30, desc: 'Avale tout ce qui approche.' },
  starforge:  { name: 'Forge d’Astres',  icon: '✸', cost: 120, cd: 20,  dmg: 300, radius: 40, desc: 'Fait pleuvoir des étoiles nouvellement forgées.' },
  bigBang:    { name: 'Big Bang',        icon: '✺', cost: 200, cd: 30,  dmg: 600, radius: 80, desc: 'Recommence l’univers autour de soi.' },
};
