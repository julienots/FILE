// Liste des voitures du garage.
//
// Pour ajouter un modèle importé : déposer le fichier .glb dans le dossier models/
// puis ajouter une ligne ici, par exemple :
//
//   { id: 'ma-voiture', nom: 'Ma Voiture', modele: 'models/ma-voiture.glb',
//     couleur: 0xc8102e, stats: { vmax: 8, accel: 7, mania: 6, adher: 6 } },
//
// Options facultatives pour un modèle importé :
//   rotY      angle en radians si la voiture roule de travers ou à l'envers (ex. Math.PI)
//   longueur  longueur voulue en mètres (4.5 par défaut)
//   hauteur   décalage vertical si les roues flottent ou s'enfoncent
//   teinte    false pour garder les couleurs d'origine du modèle
//   feux      false pour ne pas ajouter de feux arrière lumineux ; feuxX / feuxY pour les déplacer
//
// Les stats vont de 1 à 10. Ce sont les valeurs de départ : elles se règlent ensuite dans le garage du jeu.
// Tant qu'une voiture n'a pas de "modele", elle utilise la carrosserie générée par le jeu (réglages "proc").
export const CAR_LIST = [
  { id: 'vortex', nom: 'Vortex GT', couleur: 0xb9bec9, proc: { wing: 1 }, stats: { vmax: 7, accel: 7, mania: 6, adher: 6 } },
  { id: 'solaris', nom: 'Solaris', couleur: 0xf2c200, proc: { wing: 0, roof: 1.18, L: 0.97 }, stats: { vmax: 6, accel: 6, mania: 8, adher: 7 } },
  { id: 'nyx', nom: 'Nyx R', couleur: 0xc8102e, proc: { wing: 2, roof: 1.1, W: 1.03, nose: 0.92 }, stats: { vmax: 9, accel: 8, mania: 5, adher: 5 } },
  { id: 'eclair', nom: 'Moto Éclair', couleur: 0x19c37d, proc: { type: 'moto' }, stats: { vmax: 8, accel: 9, mania: 8, adher: 5 } },
  { id: 'fantome', nom: 'Fantôme', couleur: 0x14161c, proc: { wing: 0, roof: 1.12, L: 1.05, W: 1.02 }, stats: { vmax: 8, accel: 5, mania: 6, adher: 8 } },
];

// Voitures de la circulation (mode Balade libre). Elles n'apparaissent pas dans le garage.
export const TRAFIC = [
  { id: 't-berline', nom: 'Berline', proc: { wing: 0, roof: 1.46, nose: 1.3, cab: 1.12, L: 0.98 }, stats: { vmax: 3, accel: 3, mania: 5, adher: 5 } },
  { id: 't-citadine', nom: 'Citadine', proc: { wing: 0, roof: 1.52, nose: 1.45, cab: 1.15, L: 0.84 }, stats: { vmax: 3, accel: 3, mania: 5, adher: 5 } },
  { id: 't-fourgon', nom: 'Fourgon', proc: { wing: 0, roof: 1.95, nose: 1.7, cab: 1.2, L: 1.08, W: 1.04 }, stats: { vmax: 3, accel: 3, mania: 5, adher: 5 } },
];
export const TRAFIC_PAINTS = [0xd8d8dc, 0x2a2c30, 0x7a7f88, 0x1f3a6a, 0x8a1c1c, 0xe8e0c8, 0x2f5a3a, 0xc8a020];
