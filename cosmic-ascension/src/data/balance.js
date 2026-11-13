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
};
// gain d'XP modulé par la dangerosité de la planète
export const xpFor = (base, danger = 0) => Math.round(base * (BALANCE.xp.dangerMult[danger] ?? 1));
