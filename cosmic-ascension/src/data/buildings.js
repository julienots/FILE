// Bâtiments de la planète du joueur (B6). Rendu: A (world/HomeBase.js, GLB `bld_<id>` sinon forme procédurale; ids inconnus → maison).
// cost: {itemId: n} (cosmicDust inclus) · req: {level, stage, flag, building} · prod: {itemId: par heure} (s'accumule hors ligne, max 12 h)
// bonus: hp / energy / xp (multiplicateurs additifs, ex. .1 = +10 %), ship (+niveau vaisseau), slots (+emplacements) · tier: ordre d'arbre techno
export const BUILDINGS = {
  house:       { name: 'Hutte Pipion',        icon: '🏠', tier: 1, cost: { cosmicDust: 20 },                                  bonus: { hp: .05 },               desc: 'Des Pipions viennent vivre sur ta planète.' },
  farm:        { name: 'Jardin Flottant',     icon: '🌾', tier: 1, cost: { cosmicDust: 30 },                                  prod: { cosmicDust: 40 },         desc: 'Fait pousser de la poussière d’étoile.' },
  greenhouse:  { name: 'Serre Cosmique',      icon: '🌿', tier: 1, cost: { cosmicDust: 40, sporeSilk: 1 },                    prod: { lumenSap: 1, dreamMoss: 1 }, desc: 'Plantes géantes venues de Sylva.' },
  well:        { name: 'Puits d’Étoiles',     icon: '⛲', tier: 1, cost: { cosmicDust: 60 }, req: { level: 2 },                prod: { cosmicDust: 80 },         desc: 'Capte la poussière qui tombe du ciel.' },
  lab:         { name: 'Labo de Boulon',      icon: '🧪', tier: 2, cost: { cosmicDust: 80, scrapAlloy: 3 }, req: { level: 2 },  bonus: { xp: .1 },                desc: 'Boulon étudie tes évolutions. +10 % XP.' },
  forge:       { name: 'Forge d’Étoiles',     icon: '⚒', tier: 2, cost: { cosmicDust: 100, emberCore: 3 }, req: { level: 2 },  prod: { starIron: 1 }, bonus: { hp: .1 }, desc: 'Forgée avec l’aide de Forga.' },
  hangar:      { name: 'Hangar',              icon: '🛸', tier: 2, cost: { cosmicDust: 120, scrapAlloy: 5 }, req: { level: 2 }, bonus: { ship: 1 },              desc: 'Améliore ton vaisseau (+1 niveau, plus rapide).' },
  market:      { name: 'Marché Alien',        icon: '🎪', tier: 2, cost: { cosmicDust: 120, sandPearl: 2 }, req: { level: 3 },  prod: { cosmicDust: 60, tideGlass: 1 }, desc: 'Les caravanes de Zaya font escale.' },
  observatory: { name: 'Observatoire',        icon: '🔭', tier: 2, cost: { cosmicDust: 150, frostCrystal: 3 }, req: { level: 3 }, bonus: { reveal: 1 },          desc: 'Révèle les planètes du système où tu te trouves.' },
  tower:       { name: 'Tour d’Énergie',      icon: '🗼', tier: 3, cost: { cosmicDust: 180, voltShard: 3 }, req: { level: 3 },  bonus: { energy: .15 },           desc: '+15 % énergie maximale.' },
  temple:      { name: 'Temple Cosmique',     icon: '⛩', tier: 3, cost: { cosmicDust: 200, lumenSap: 3 }, req: { level: 3, stage: 3 }, bonus: { xp: .1, hp: .05 }, desc: 'Les fidèles prient pour ton ascension.' },
  academy:     { name: 'Académie Stellaire',  icon: '🎓', tier: 3, cost: { cosmicDust: 250, echoStone: 2 }, req: { level: 4 },  bonus: { xp: .15 },               desc: 'Les jeunes Pipions apprennent les étoiles.' },
  sanctuary:   { name: 'Sanctuaire des Bêtes',icon: '🐾', tier: 3, cost: { cosmicDust: 220, sporeSilk: 3 }, req: { level: 4 },  prod: { lumenSap: 1 }, bonus: { hp: .1 }, desc: 'Tes créatures collectées y vivent.' },
  library:     { name: 'Bibliothèque d’Écho', icon: '📚', tier: 3, cost: { cosmicDust: 260, echoStone: 3 }, req: { level: 4 },  bonus: { xp: .1 },                desc: 'Archive les voix des peuples disparus.' },
  reactor:     { name: 'Réacteur à Foudre',   icon: '⚡', tier: 4, cost: { cosmicDust: 320, voltShard: 5, starIron: 2 }, req: { level: 5 }, prod: { voltShard: 1 }, bonus: { energy: .15 }, desc: 'Une tempête de Voltaria en bouteille.' },
  garden:      { name: 'Jardin de Rêve',      icon: '🍄', tier: 4, cost: { cosmicDust: 300, dreamMoss: 4 }, req: { level: 5 },  prod: { dreamMoss: 1, lumenSap: 1 }, desc: 'Les spores de Mycélia chantent la nuit.' },
  beacon:      { name: 'Phare Céleste',       icon: '🔆', tier: 4, cost: { cosmicDust: 350, tideGlass: 4 }, req: { level: 5 },  bonus: { ship: 1 },               desc: 'Guide les baleines… et ton vaisseau (+1 niveau).' },
  portal:      { name: 'Portail Stellaire',   icon: '🌀', tier: 5, cost: { cosmicDust: 500, voidEssence: 3, starIron: 4 }, req: { level: 6, flag: 'act4_started' }, bonus: { ship: 1, energy: .1 }, desc: 'Ouvre des raccourcis entre les systèmes.' },
  monument:    { name: 'Monument au Héros',   icon: '🗿', tier: 5, cost: { cosmicDust: 600, starIron: 5, sandPearl: 4 }, req: { level: 6, stage: 6 }, bonus: { hp: .15, xp: .1 }, desc: 'Ta silhouette gravée pour l’éternité.' },
  citadel:     { name: 'Citadelle Divine',    icon: '🏰', tier: 5, cost: { cosmicDust: 1000, primeSpark: 2, voidEssence: 4 }, req: { level: 7, flag: 'act5_started' }, bonus: { hp: .2, energy: .2, xp: .15 }, desc: 'Le trône d’un futur dieu.' },
};
export const BASE_SLOTS = 8;            // emplacements rendus par A (HomeBase); les bâtiments supplémentaires attendront plus d'emplacements
export const planetLevel = (n) => 1 + Math.floor(n / 2);   // niveau de la planète selon le nombre de bâtiments (formule partagée avec A)
export const MAX_OFFLINE_H = 12;
