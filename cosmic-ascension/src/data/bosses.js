// Boss cosmiques (B: stats/phases/récompenses; A: entités géantes `entities/boss/` + cinématiques).
// phases: seuils de PV (fraction restante) où le boss change de pattern. attacks: ids que A télégraphie/anime.
// weakPoints: ids de points faibles (A les place; `enemy:hit {uid, part}` × mult). arena: planète ou 'space'.
// ids émis par le moteur (A) → ids de données
export const BOSS_ALIAS = { serpent: 'ouroboros' };
export const bossId = (id) => BOSS_ALIAS[id] ?? id;
export const BOSSES = {
  lab_warden: { name: 'Le Gardien Prime', title: 'Geôlier du Laboratoire', arena: 'lab', scale: 12, hp: 900, dmg: 18, act: 1,
    phases: [{ at: 1, attacks: ['laserSweep', 'droneSwarm'] }, { at: .5, attacks: ['laserSweep', 'overload', 'droneSwarm'] }],
    weakPoints: { core: 2.5 }, rewards: { xp: 300, items: [['labKeycard', 1]], flags: ['warden_defeated'] }, music: 'boss1' },
  ouroboros: { name: 'Ouroboros', title: 'Serpent Céleste', arena: 'space', scale: 400, hp: 6000, dmg: 45, act: 4,
    phases: [{ at: 1, attacks: ['coilCharge', 'starBreath'] }, { at: .66, attacks: ['coilCharge', 'starBreath', 'tailMeteors'] }, { at: .33, attacks: ['wrapPlanet', 'starBreath', 'tailMeteors'] }],
    weakPoints: { eye: 3, scaleGap: 1.8 }, rewards: { xp: 2500, items: [['primeSpark', 1]], flags: ['serpent_defeated'] }, music: 'boss2' },
  ancient_titan: { name: 'Kaal', title: 'Titan Ancien', arena: 'dune', scale: 180, hp: 9000, dmg: 60, act: 4,
    phases: [{ at: 1, attacks: ['groundSlam', 'rockRain'] }, { at: .5, attacks: ['groundSlam', 'eyeBeam', 'rockRain'] }],
    weakPoints: { eye: 3, runes: 1.6 }, rewards: { xp: 3500, items: [['starIron', 10]], flags: ['titan_defeated'] }, music: 'boss2' },
  devourer: { name: 'Gloutonde', title: 'Dévoreuse de Planètes', arena: 'space', scale: 600, hp: 14000, dmg: 75, act: 4,
    phases: [{ at: 1, attacks: ['suction', 'toothRing'] }, { at: .6, attacks: ['suction', 'spitMoon', 'tentacles'] }, { at: .25, attacks: ['devourPlanet', 'tentacles'] }],
    weakPoints: { throat: 3.5, veins: 1.5 }, rewards: { xp: 5000, items: [['voidEssence', 5]], flags: ['devourer_defeated'] }, music: 'boss2' },
  emperor: { name: 'Empereur Rubis', title: 'Souverain Écarlate', arena: 'bastion', scale: 8, hp: 5000, dmg: 50, act: 3,
    phases: [{ at: 1, attacks: ['rubySlash', 'summonKnights'] }, { at: .5, attacks: ['rubySlash', 'crimsonNova', 'mechSuit'] }],
    weakPoints: { crown: 2.5 }, rewards: { xp: 2000, items: [['starIron', 5]], flags: ['emperor_defeated'] }, music: 'boss1' },
  celestial_god: { name: 'Aelion', title: 'Dieu Céleste', arena: 'heart', scale: 300, hp: 30000, dmg: 120, act: 5,
    phases: [{ at: 1, attacks: ['wingStorm', 'judgment'] }, { at: .66, attacks: ['wingStorm', 'starfall', 'clones'] }, { at: .33, attacks: ['judgment', 'starfall', 'genesisBeam'] }],
    weakPoints: { mask: 3 }, rewards: { xp: 10000, flags: ['god_defeated', 'final_choice'] }, music: 'final' },
  void_whale: { name: 'Léviathan Abyssal', title: 'Entité Secrète', arena: 'umbra', scale: 500, hp: 20000, dmg: 90, act: 0, secret: true,
    phases: [{ at: 1, attacks: ['galaxyBreath', 'tentacleStars'] }, { at: .4, attacks: ['blackHoleSong', 'tentacleStars'] }],
    weakPoints: { heart: 3 }, rewards: { xp: 8000, items: [['voidEssence', 10]], flags: ['whale_defeated'] }, music: 'boss2' },
};
