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
// Les modèles de models/ sont passés par tools/prep-model.mjs : allégés, à l'échelle, l'avant vers -z, roues découpées.
// teinte: false garde les couleurs d'origine du modèle ; moto: true fait pencher le véhicule dans les virages.
export const CAR_LIST = [
  { id: 'azur', nom: 'Azur', modele: 'models/azur.glb', longueur: 4.4, teinte: false, couleur: 0x1668ff, stats: { vmax: 8, accel: 8, mania: 7, adher: 7 } },
  { id: 'rubis', nom: 'Rubis', modele: 'models/rubis.glb', longueur: 4.4, teinte: false, couleur: 0xc8102e, stats: { vmax: 9, accel: 8, mania: 6, adher: 6 } },
  { id: 'cobalt', nom: 'Cobalt', modele: 'models/cobalt.glb', longueur: 4.5, teinte: false, couleur: 0x1d3f9a, stats: { vmax: 9, accel: 7, mania: 6, adher: 8 } },
  { id: 'spectre', nom: 'Spectre', modele: 'models/spectre.glb', longueur: 4.4, teinte: false, couleur: 0x3a3d44, stats: { vmax: 7, accel: 7, mania: 8, adher: 7 } },
  { id: 'mono', nom: 'Monoplace', modele: 'models/mono.glb', longueur: 4.3, teinte: false, couleur: 0x8a4a1c, feux: false, stats: { vmax: 10, accel: 9, mania: 8, adher: 9 } },
  { id: 'buggy', nom: 'Buggy 22', modele: 'models/buggy.glb', longueur: 4.0, teinte: false, couleur: 0x1f4fd0, feux: false, stats: { vmax: 6, accel: 8, mania: 9, adher: 6 } },
  { id: 'saphir', nom: 'Saphir', modele: 'models/saphir.glb', longueur: 2.1, teinte: false, moto: true, feux: false, couleur: 0x1668ff, stats: { vmax: 9, accel: 9, mania: 8, adher: 5 } },
  { id: 'tempete', nom: 'Tempête', modele: 'models/tempete.glb', longueur: 2.1, teinte: false, moto: true, feux: false, couleur: 0x1d2f7a, stats: { vmax: 9, accel: 8, mania: 8, adher: 5 } },
  { id: 'onyx', nom: 'Onyx', modele: 'models/onyx.glb', longueur: 2.1, teinte: false, moto: true, feux: false, couleur: 0x14161c, stats: { vmax: 7, accel: 9, mania: 9, adher: 5 } },
  { id: 'corbeau', nom: 'Corbeau', modele: 'models/roadster.glb', longueur: 2.1, teinte: false, moto: true, feux: false, couleur: 0x14161c, stats: { vmax: 7, accel: 8, mania: 9, adher: 6 } },
];

// Voitures de la circulation (mode Balade libre). Elles n'apparaissent pas dans le garage.
export const TRAFIC = [
  { id: 't-berline', nom: 'Berline', proc: { wing: 0, roof: 1.46, nose: 1.3, cab: 1.12, L: 0.98 }, stats: { vmax: 3, accel: 3, mania: 5, adher: 5 } },
  { id: 't-citadine', nom: 'Citadine', proc: { wing: 0, roof: 1.52, nose: 1.45, cab: 1.15, L: 0.84 }, stats: { vmax: 3, accel: 3, mania: 5, adher: 5 } },
  { id: 't-fourgon', nom: 'Fourgon', proc: { wing: 0, roof: 1.95, nose: 1.7, cab: 1.2, L: 1.08, W: 1.04 }, stats: { vmax: 3, accel: 3, mania: 5, adher: 5 } },
];
export const TRAFIC_PAINTS = [0xd8d8dc, 0x2a2c30, 0x7a7f88, 0x1f3a6a, 0x8a1c1c, 0xe8e0c8, 0x2f5a3a, 0xc8a020];
