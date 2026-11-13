// Courbes et constantes d'équilibrage (B). Toutes les valeurs de gain passent par ici.
export const BALANCE = {
  xp: {
    discoverPlanet: 25,   // première approche en vaisseau
    firstLanding: 75,     // premier atterrissage sur une planète
    pickup: { dust: 2, crystal: 8, relic: 30, creature: 40, artifact: 60 },
    killBase: 10,         // × niveau ennemi
    questBase: 120,
    dangerMult: [1, 1.3, 1.7, 2.2, 3], // selon planet.danger 0..4
  },
  energyRegen: 6,         // /s
  hpRegenOutOfCombat: 3,  // /s après 5 s sans dégât
  dustPerPickup: 5,
  // combat
  heroAttack: (stage) => Math.round(10 * (1 + stage * .6)),  // dégâts de l'attaque de base
  comboWindow: .9, comboMax: 10, comboBonus: .08,             // +8 % par coup enchaîné, max ×1.8
  enemyLevel: .25,                                            // +25 % PV/dégâts par niveau au-delà de 1
  dashIFrames: .45, hitIFrames: .6, regenDelay: 5,
  respawnHpRatio: .6, deathDustLoss: .1,
};
// gain d'XP modulé par la dangerosité de la planète
export const xpFor = (base, danger = 0) => Math.round(base * (BALANCE.xp.dangerMult[danger] ?? 1));
