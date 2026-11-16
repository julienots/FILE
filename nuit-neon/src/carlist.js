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
// son : caractère du moteur (voir MOTEURS dans src/audio.js : v8, v10, v12, biturbo, f1, proto, buggy, moto, custom).
// teinte: false garde les couleurs d'origine du modèle ; moto: true fait pencher le véhicule dans les virages.
const T10 = [0, 0.04, 0.12, 0.25, 0.4, 0.55, 0.7, 0.85, 0.95, 1];
export const CAR_LIST = [
  { id: 'azur', son: 'v10', prix: 0, nom: 'Azur', modele: 'models/azur.glb', longueur: 4.4, teinte: false, couleur: 0x1668ff, stats: { vmax: 8, accel: 8, mania: 7, adher: 7 } },
  { id: 'rubis', son: 'v8', prix: 14000, nom: 'Rubis', modele: 'models/rubis.glb', longueur: 4.4, teinte: false, couleur: 0xc8102e, stats: { vmax: 9, accel: 8, mania: 6, adher: 6 } },
  { id: 'cobalt', son: 'biturbo', prix: 14000, nom: 'Cobalt', modele: 'models/cobalt.glb', longueur: 4.5, teinte: false, couleur: 0x1d3f9a, stats: { vmax: 9, accel: 7, mania: 6, adher: 8 } },
  { id: 'spectre', son: 'biturbo', prix: 6000, nom: 'Spectre', modele: 'models/spectre.glb', longueur: 4.4, teinte: false, couleur: 0x3a3d44, stats: { vmax: 7, accel: 7, mania: 8, adher: 7 } },
  { id: 'mono', son: 'f1', prix: 30000, nom: 'Monoplace', modele: 'models/mono.glb', longueur: 4.3, teinte: false, couleur: 0x8a4a1c, stats: { vmax: 10, accel: 9, mania: 8, adher: 9 } },
  { id: 'buggy', son: 'buggy', prix: 4000, nom: 'Buggy 22', modele: 'models/buggy.glb', longueur: 4.0, teinte: false, couleur: 0x1f4fd0, stats: { vmax: 6, accel: 8, mania: 9, adher: 6 } },
  { id: 'saphir', prix: 9000, nom: 'Saphir', modele: 'models/saphir.glb', longueur: 2.1, teinte: false, moto: true, couleur: 0x1668ff, stats: { vmax: 9, accel: 9, mania: 8, adher: 5 } },
  { id: 'tempete', prix: 9000, nom: 'Tempête', modele: 'models/tempete.glb', longueur: 2.1, teinte: false, moto: true, couleur: 0x1d2f7a, stats: { vmax: 9, accel: 8, mania: 8, adher: 5 } },
  { id: 'onyx', prix: 0, nom: 'Onyx', modele: 'models/onyx.glb', longueur: 2.1, teinte: false, moto: true, couleur: 0x14161c, stats: { vmax: 7, accel: 9, mania: 9, adher: 5 } },
  { id: 'corbeau', son: 'custom', prix: 5000, nom: 'Corbeau', modele: 'models/roadster.glb', longueur: 2.1, teinte: false, moto: true, couleur: 0x14161c, stats: { vmax: 7, accel: 8, mania: 9, adher: 6 } },
  // ---------- voitures dessinées par le jeu (src/procar.js) ----------
  // design : L, W = longueur et largeur (m) ; t = positions le long de la voiture (0 = nez, 1 = arrière) ; w = largeur relative ;
  // hc = hauteur de la ligne capot / coffre ; hs = hauteur de la ligne d'épaule ; b = garde au sol ; roues = [position, rayon, largeur] ;
  // toit = habitacle (t0..t1, h = hauteur, forme, peint = portion de toit en tôle) ; le reste règle les détails.
  { id: 'comete', son: 'biturbo', prix: 8000, nom: 'Comète', couleur: 0xf2f2f2, stats: { vmax: 7, accel: 7, mania: 7, adher: 6 },
    design: { L: 4.3, W: 1.9, t: T10, w: [0.80, 0.93, 0.97, 0.98, 0.98, 0.98, 0.99, 1.0, 0.99, 0.95], hc: [0.36, 0.41, 0.47, 0.55, 0.63, 0.71, 0.79, 0.86, 0.90, 0.88], hs: [0.36, 0.43, 0.50, 0.58, 0.65, 0.72, 0.79, 0.85, 0.88, 0.86], b: [0.22, 0.15, 0.13, 0.13, 0.13, 0.13, 0.13, 0.14, 0.18, 0.28],
      roues: [[0.19, 0.31, 0.24], [0.81, 0.33, 0.29]], toit: { t0: 0.27, t1: 0.74, h: 1.1, forme: [0, 0.55, 0.95, 1, 0.97, 0.8, 0], larg: [0.82, 0.78, 0.8], pince: 0.74, peint: [0.42, 0.8] },
      aileron: 'plat', persiennes: 0.75, bandes: 'flanc', accent: 0xff2bd0, neon: 0xff2bd0, jantes: 'turbine', jante: 0xe8e8ee, etrier: 0x19e6ff, phares: [0.02, 0.05, 9.8, 13.2], aile: 0.06, echap: [-0.5, 0.5] } },
  { id: 'nomade', son: 'v8', prix: 10000, nom: 'Nomade', couleur: 0x5a6b3a, stats: { vmax: 6, accel: 7, mania: 6, adher: 9 },
    design: { L: 4.5, W: 2.0, t: T10, w: [0.90, 0.96, 0.99, 1.0, 1.0, 1.0, 1.0, 1.0, 0.99, 0.95], hc: [0.86, 0.96, 1.03, 1.07, 1.10, 1.12, 1.12, 1.12, 1.10, 1.02], hs: [0.84, 0.94, 1.0, 1.04, 1.07, 1.09, 1.09, 1.09, 1.07, 1.0], b: [0.46, 0.36, 0.34, 0.34, 0.34, 0.34, 0.34, 0.34, 0.38, 0.48],
      roues: [[0.18, 0.43, 0.3], [0.82, 0.43, 0.3]], toit: { t0: 0.26, t1: 0.985, h: 1.74, forme: [0, 0.8, 1, 1, 1, 0.98, 0.55], u: [0, 0.14, 0.3, 0.5, 0.75, 0.93, 1], larg: [0.9, 0.9, 0.9], pince: 0.86, peint: [0.2, 0.96] },
      rampe: true, galerie: true, jantes: 'six', jante: 0x22242a, etrier: 0xffb020, phares: [0.012, 0.045, 8.2, 11.4], calandre: 0.8, aile: 0.1, feux: 'barre', echap: [-0.6], echapY: 0.1, bombe: 0.01 } },
  { id: 'titan', son: 'v8', prix: 12000, nom: 'Titan', couleur: 0xff6a00, stats: { vmax: 8, accel: 9, mania: 5, adher: 6 },
    design: { L: 4.85, W: 1.96, t: T10, w: [0.88, 0.95, 0.98, 1.0, 1.0, 0.99, 1.0, 1.0, 0.98, 0.92], hc: [0.60, 0.70, 0.77, 0.82, 0.85, 0.86, 0.86, 0.88, 0.87, 0.80], hs: [0.60, 0.70, 0.77, 0.82, 0.84, 0.85, 0.86, 0.88, 0.87, 0.80], b: [0.33, 0.24, 0.21, 0.2, 0.2, 0.2, 0.2, 0.21, 0.25, 0.34],
      roues: [[0.17, 0.35, 0.27], [0.80, 0.37, 0.33]], toit: { t0: 0.38, t1: 0.83, h: 1.34, forme: [0, 0.7, 0.98, 1, 0.96, 0.6, 0], larg: [0.86, 0.82, 0.8], pince: 0.76, peint: [0.3, 0.86] },
      bandes: 'double', accent: 0x101014, aileron: 'canard', feux: 'ronds', ouies: [0.16], jantes: 'etoile', jante: 0x2a2c32, etrier: 0xd8121e, phares: [0.012, 0.04, 8.4, 11.2], calandre: 0.85, aile: 0.07, echap: [-0.62, -0.46, 0.46, 0.62] } },
  { id: 'mirage', son: 'v12', prix: 18000, nom: 'Mirage', couleur: 0x1b6b5a, stats: { vmax: 9, accel: 7, mania: 7, adher: 8 },
    design: { L: 4.65, W: 1.94, t: T10, w: [0.66, 0.87, 0.95, 0.99, 0.98, 0.97, 1.0, 0.99, 0.92, 0.76], hc: [0.40, 0.51, 0.61, 0.70, 0.76, 0.80, 0.82, 0.79, 0.72, 0.60], hs: [0.40, 0.52, 0.63, 0.72, 0.76, 0.79, 0.84, 0.83, 0.75, 0.62], b: [0.25, 0.16, 0.14, 0.14, 0.14, 0.14, 0.14, 0.15, 0.19, 0.28],
      roues: [[0.18, 0.345, 0.26], [0.80, 0.355, 0.31]], toit: { t0: 0.36, t1: 0.9, h: 1.25, forme: [0, 0.7, 0.98, 1, 0.9, 0.55, 0.12], larg: [0.82, 0.78, 0.7], pince: 0.7, peint: [0.3, 0.6] },
      aileron: 'canard', priseFlanc: [0.27, 0.34], ouies: [0.2], jantes: 'double', jante: 0xd9c08a, etrier: 0x101014, neon: 0x20ffa0, phares: [0.035, 0.1, 9.4, 12.6], echap: [-0.34, -0.2, 0.2, 0.34] } },
  { id: 'eclipse', son: 'v12', prix: 35000, nom: 'Éclipse', couleur: 0x14161c, stats: { vmax: 10, accel: 8, mania: 7, adher: 8 },
    design: { L: 4.75, W: 2.0, t: T10, w: [0.60, 0.85, 0.95, 0.985, 0.95, 0.93, 0.98, 1.0, 0.96, 0.84], hc: [0.33, 0.41, 0.49, 0.57, 0.65, 0.72, 0.77, 0.78, 0.75, 0.68], hs: [0.33, 0.43, 0.54, 0.63, 0.66, 0.70, 0.78, 0.83, 0.80, 0.70], b: [0.22, 0.13, 0.105, 0.105, 0.105, 0.105, 0.105, 0.115, 0.16, 0.26],
      roues: [[0.2, 0.345, 0.27], [0.8, 0.365, 0.34]], toit: { t0: 0.27, t1: 0.8, h: 1.1, forme: [0, 0.62, 0.96, 1, 0.9, 0.55, 0.1], larg: [0.8, 0.72, 0.6], pince: 0.64 },
      aileron: 'canard', priseFlanc: [0.56, 0.68], bandes: 'centre', accent: 0xd9a400, jantes: 'turbine', jante: 0xd9a400, etrier: 0xd9a400, neon: 0x8a2be2, pharesCol: 0xfff0c0, echap: [-0.12, 0.12], echapY: 0.34 } },
  { id: 'vortex', son: 'proto', prix: 50000, nom: 'Vortex', couleur: 0x1668ff, stats: { vmax: 10, accel: 9, mania: 9, adher: 10 },
    design: { L: 4.7, W: 2.0, t: T10, w: [0.80, 0.94, 0.985, 1.0, 0.99, 0.985, 1.0, 1.0, 0.99, 0.96], hc: [0.20, 0.25, 0.30, 0.36, 0.45, 0.58, 0.68, 0.70, 0.69, 0.62], hs: [0.26, 0.40, 0.50, 0.50, 0.46, 0.52, 0.66, 0.72, 0.72, 0.64], b: [0.12, 0.07, 0.06, 0.06, 0.06, 0.06, 0.06, 0.07, 0.1, 0.2],
      roues: [[0.2, 0.34, 0.31], [0.81, 0.35, 0.36]], toit: { t0: 0.3, t1: 0.7, h: 1.0, forme: [0, 0.7, 0.98, 1, 0.92, 0.6, 0.1], larg: [0.56, 0.52, 0.42], pince: 0.6 },
      aileron: 'proto', aileronH: 1.02, derive: true, bandes: 'centre', accent: 0xffffff, jantes: 'six', jante: 0x15161a, etrier: 0xffd23f, neon: 0x19e6ff, aile: 0.1, phares: [0.07, 0.15, 9.2, 12.4], retro: 0.33, echap: [-0.55, 0.55], echapY: 0.12 } },
  { id: 'fulgur', son: 'v10', prix: 75000, nom: 'Fulgur', couleur: 0xc8102e, stats: { vmax: 10, accel: 10, mania: 9, adher: 9 },
    design: { L: 4.6, W: 2.02, t: T10, w: [0.62, 0.86, 0.95, 0.985, 0.94, 0.925, 0.985, 1.0, 0.97, 0.86], hc: [0.32, 0.40, 0.48, 0.56, 0.64, 0.72, 0.78, 0.80, 0.78, 0.72], hs: [0.32, 0.43, 0.54, 0.63, 0.66, 0.70, 0.79, 0.85, 0.83, 0.74], b: [0.21, 0.125, 0.1, 0.1, 0.1, 0.1, 0.1, 0.11, 0.15, 0.25],
      roues: [[0.2, 0.34, 0.28], [0.8, 0.37, 0.35]], toit: { t0: 0.26, t1: 0.77, h: 1.1, forme: [0, 0.64, 0.97, 1, 0.88, 0.5, 0.08], larg: [0.8, 0.72, 0.62], pince: 0.64 },
      aileron: 'gt', aileronH: 1.16, priseFlanc: [0.55, 0.69], ouies: [0.13], bandes: 'double', accent: 0xf2f2f2, jantes: 'double', jante: 0x1a1b20, etrier: 0xffd23f, neon: 0xff2a2a, echap: [-0.16, 0, 0.16], echapY: 0.36 } },
];

// Voitures de la circulation (mode Balade libre). Elles n'apparaissent pas dans le garage.
export const TRAFIC = [
  { id: 't-berline', nom: 'Berline', proc: { wing: 0, roof: 1.46, nose: 1.3, cab: 1.12, L: 0.98 }, stats: { vmax: 3, accel: 3, mania: 5, adher: 5 } },
  { id: 't-citadine', nom: 'Citadine', proc: { wing: 0, roof: 1.52, nose: 1.45, cab: 1.15, L: 0.84 }, stats: { vmax: 3, accel: 3, mania: 5, adher: 5 } },
  { id: 't-fourgon', nom: 'Fourgon', proc: { wing: 0, roof: 1.95, nose: 1.7, cab: 1.2, L: 1.08, W: 1.04 }, stats: { vmax: 3, accel: 3, mania: 5, adher: 5 } },
];
export const TRAFIC_PAINTS = [0xd8d8dc, 0x2a2c30, 0x7a7f88, 0x1f3a6a, 0x8a1c1c, 0xe8e0c8, 0x2f5a3a, 0xc8a020];
