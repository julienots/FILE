import { Color } from 'three';
import { tracer } from './custom.js';
// Décors de nature (jour). flore : [type, nombre, distance mini au bord, étalement, taille mini, taille maxi]
const NATURE = {
  prairie: { env: 'nature', sky: [[0.10,0.28,0.72],[0.36,0.60,0.95],[0.80,0.88,0.96]], fog: 0xbcd4ee, dens: 0.0016, a: 0x19a0ff, b: 0xff5a1f, c: 0xffd23f,
    ground: 0x4f8a35, sol: { kind: 0, c: [0x2f5a1e, 0x5f9a35, 0xc9b25a, 0x6a5136] }, hill: 0x3f6f48, hillH: 110, sun: { dir: [0.449,0.419,-0.789], col: [1.0,0.86,0.62], road: 2.2 }, sunI: 2.8, ambI: 1.5, wet: 0.6,
    flore: [['feuillu', 420, 14, 240, 0.8, 1.8], ['rocher', 40, 10, 200, 0.4, 1.0]], grass: { c0: 0x2d5a1c, c1: 0x7fb83a, c2: 0xd8c060, h: 1.35 } },
  canyon: { env: 'nature', sky: [[0.16,0.18,0.42],[0.85,0.45,0.30],[1.0,0.62,0.30]], fog: 0xd89060, dens: 0.0016, a: 0xff8a1f, b: 0xffffff, c: 0x19e6ff, cloud: 0.75,
    ground: 0xb0603a, sol: { kind: 1, c: [0x9a5230, 0xc77a48, 0xd69a62, 0x7a3f26] }, relief: 55, hill: 0x8a3a22, hillH: 220, rock: 0x9a4a2c, sun: { dir: [-0.6,0.22,-0.77], col: [1.0,0.55,0.25], road: 2.4 }, sunI: 3.0, ambI: 1.3, wet: 0.12,
    flore: [['rocher', 260, 6, 200, 0.8, 3.2], ['cactus', 180, 5, 160, 0.7, 1.5]], grass: { c0: 0x6a5a2a, c1: 0xc8a850, c2: 0xd8b860, h: 0.7, dens: 0.35 } },
  neige: { env: 'nature', sky: [[0.25,0.32,0.50],[0.60,0.68,0.82],[0.86,0.90,0.96]], fog: 0xc8d4e6, dens: 0.0035, a: 0x19a0ff, b: 0xff2b50, c: 0xffd23f, cloud: 0.8,
    ground: 0xe8eef6, sol: { kind: 2, c: [0xdfe8f2, 0xffffff, 0xeaf2fb, 0xc8d4e2] }, relief: 60, ombre: 0.35, hill: 0xdfe8f4, hillH: 320, rock: 0x8a909a, snowTop: true, sun: { dir: [0.3,0.3,-0.906], col: [0.75,0.8,1.0], road: 1.4 }, sunI: 2.0, ambI: 1.7, wet: 0.85, rain: 'neige',
    flore: [['sapin', 520, 5, 220, 0.8, 1.7], ['rocher', 60, 8, 200, 0.6, 1.6]] },
  // bord de mer au soleil couchant : la piste longe une plage, la mer reflète le ciel
  couchant: { env: 'nature', sky: [[0.10,0.12,0.32],[0.80,0.40,0.34],[1.0,0.56,0.24]], fog: 0xe0906a, dens: 0.0017, a: 0xff8a1f, b: 0xffffff, c: 0x19e6ff, cloud: 0.72,
    ground: 0xd8b888, sol: { kind: 1, c: [0xd9b98a, 0xe8cfa0, 0xcfae7e, 0xb89060] }, relief: 70, hill: 0x6a4a5a, hillH: 150, rock: 0x7a5a50,
    mer: { niveau: -0.35, profond: 0x04202e }, phares: true,
    sun: { dir: [-0.15,0.07,-0.986], col: [1.0,0.56,0.28], road: 2.6 }, sunI: 2.6, ambI: 1.25, wet: 0.5, ombre: 0.45,
    flore: [['palmier', 260, 1.5, 12, 0.9, 1.35], ['rocher', 120, 4, 90, 0.6, 2.2]] },
  // forêt en automne : feuillages roux et dorés, lumière basse
  automne: { env: 'nature', sky: [[0.16, 0.26, 0.52], [0.62, 0.62, 0.72], [0.98, 0.80, 0.56]], fog: 0xdcc0a0, dens: 0.0022, a: 0xff8a1f, b: 0xffffff, c: 0xffd23f, cloud: 0.85,
    ground: 0x6f6030, sol: { kind: 0, c: [0x4a4020, 0x8a6a2a, 0xc98a3a, 0x5a4530] }, relief: 48, hill: 0x8a5a2a, hillH: 160, feuille: [2.7, 0.95, 0.3], sun: { dir: [-0.5, 0.3, -0.81], col: [1.0, 0.72, 0.42], road: 2.3 }, sunI: 2.6, ambI: 1.35, wet: 0.45,
    flore: [['feuillu', 640, 8, 220, 0.9, 2.0], ['rocher', 50, 9, 180, 0.4, 1.1]], grass: { c0: 0x5a4a1c, c1: 0xb88a30, c2: 0xe0a040, h: 1.0 } },
  // dunes : sable à perte de vue, soleil au zénith, palmiers et cactus
  dunes: { env: 'nature', sky: [[0.08, 0.30, 0.78], [0.45, 0.68, 0.95], [0.98, 0.90, 0.72]], fog: 0xead8b0, dens: 0.0015, a: 0xff8a1f, b: 0xffffff, c: 0x19e6ff, cloud: 0.95,
    ground: 0xd9b36a, sol: { kind: 1, c: [0xd2a862, 0xe6c482, 0xf0d49a, 0xb88a4a] }, relief: 46, hill: 0xcaa055, hillH: 95, rock: 0xa8804a, sun: { dir: [0.25, 0.85, -0.46], col: [1.0, 0.95, 0.8], road: 2.4 }, sunI: 3.2, ambI: 1.6, wet: 0.05,
    flore: [['palmier', 150, 4, 60, 0.9, 1.4], ['cactus', 260, 5, 180, 0.7, 1.6], ['rocher', 90, 8, 200, 0.5, 1.8]] },
  // volcan : basalte noir fendu de lave, ciel de cendre rougeoyant, braises dans l'air
  volcan: { env: 'nature', sky: [[0.02, 0.01, 0.01], [0.26, 0.06, 0.03], [0.95, 0.32, 0.08]], fog: 0x3a1408, dens: 0.0034, a: 0xff5a10, b: 0xffd23f, c: 0xff2a2a, cloud: 0.2,
    ground: 0x1a1412, sol: { kind: 1, c: [0x14100e, 0x2a1c16, 0xff5a10, 0x0a0808] }, relief: 95, hill: 0x1c0f0c, hillH: 420, rock: 0x2a2220, sun: { dir: [0.1, 0.18, -0.98], col: [1.0, 0.36, 0.1], road: 2.8 }, sunI: 1.9, ambI: 0.85, wet: 0.12,
    phares: true, nuit: true, braises: true, ombre: 0.3, flore: [['rocher', 420, 5, 200, 0.7, 3.4]] },
  // toundra de nuit sous l'aurore boréale
  aurore: { env: 'nature', sky: [[0.0, 0.02, 0.06], [0.02, 0.22, 0.2], [0.12, 0.55, 0.36]], fog: 0x0a2420, dens: 0.0026, a: 0x20ffa0, b: 0x19d0ff, c: 0xb6ff2b, cloud: 0.22,
    ground: 0xcfe0e6, sol: { kind: 2, c: [0xb8ccd6, 0xe6f2f6, 0xd4e6ee, 0x9ab0bc] }, relief: 55, ombre: 0.3, hill: 0xa8c4cc, hillH: 300, rock: 0x6a7480, snowTop: true, sun: { dir: [-0.3, 0.5, -0.81], col: [0.5, 0.95, 0.8], road: 1.2 }, sunI: 0.95, ambI: 0.7, wet: 0.8,
    rain: 'neige', phares: true, nuit: true, flare: 0.3, flore: [['sapin', 560, 5, 220, 0.8, 1.8], ['rocher', 70, 8, 200, 0.6, 1.6]] },
};
// lagon en plein jour : sable blanc, eau turquoise, soleil haut
NATURE.lagune = Object.assign({}, NATURE.couchant, { sky: [[0.05, 0.30, 0.78], [0.30, 0.66, 0.95], [0.78, 0.93, 0.98]], fog: 0xbfe6f2, dens: 0.0014, cloud: 0.95,
  ground: 0xf0e2bc, sol: { kind: 1, c: [0xeedfb6, 0xf8eccb, 0xe6d3a0, 0xcdb884] }, hill: 0x4f8a55, hillH: 130, rock: 0x8a8478, mer: { niveau: -0.35, profond: 0x0a6c7c }, phares: false,
  sun: { dir: [0.35, 0.72, -0.6], col: [1.0, 0.96, 0.86], road: 2.1 }, sunI: 3.1, ambI: 1.6, wet: 0.3, ombre: 0.6,
  flore: [['palmier', 320, 1.5, 14, 0.9, 1.4], ['rocher', 90, 4, 90, 0.5, 1.8]] });
// glacier : glace bleue, ciel limpide et froid
NATURE.glacier = Object.assign({}, NATURE.neige, { sky: [[0.06, 0.20, 0.55], [0.34, 0.60, 0.92], [0.80, 0.92, 1.0]], fog: 0xcfe6f8, dens: 0.0022, cloud: 0.95,
  ground: 0xdcefff, sol: { kind: 2, c: [0xcfe6f8, 0xffffff, 0xbfe0fa, 0xa8cdee] }, hill: 0xcfe4f6, hillH: 380, rock: 0x7f8fa6, sun: { dir: [-0.4, 0.5, -0.77], col: [0.9, 0.95, 1.0], road: 1.8 }, sunI: 2.6, ambI: 1.7, wet: 0.9, rain: false,
  flore: [['rocher', 220, 6, 220, 0.8, 3.0], ['sapin', 160, 8, 220, 0.8, 1.5]] });
// jungle tropicale sous une pluie chaude : brume verte, feuillages denses, piste détrempée
NATURE.jungle = { env: 'nature', sky: [[0.10, 0.16, 0.16], [0.30, 0.40, 0.36], [0.62, 0.70, 0.60]], fog: 0x5a7466, dens: 0.0042, a: 0x20ffa0, b: 0xffd23f, c: 0x19e6ff, cloud: 0.45,
  ground: 0x2f4a22, sol: { kind: 0, c: [0x1f3a14, 0x3f6a22, 0x5a4a2a, 0x3a2a18] }, relief: 52, hill: 0x24402a, hillH: 190, rock: 0x4a5048, feuille: [0.7, 1.15, 0.6],
  sun: { dir: [0.2, 0.7, -0.68], col: [0.85, 0.95, 0.85], road: 1.2 }, sunI: 1.7, ambI: 1.35, wet: 1, rain: true, ombre: 0.25, flare: 0,
  flore: [['feuillu', 900, 4, 200, 1.0, 2.3], ['palmier', 420, 2, 60, 1.0, 1.6], ['rocher', 60, 6, 160, 0.6, 2.0]], grass: { c0: 0x1a3a12, c1: 0x4a8a2a, c2: 0x8ac04a, h: 1.6 } };
// champs de lavande au soleil couchant, cyprès et pierres claires
NATURE.lavande = { env: 'nature', sky: [[0.14, 0.12, 0.40], [0.78, 0.50, 0.62], [1.0, 0.72, 0.46]], fog: 0xd8a8b8, dens: 0.0018, a: 0xb45cff, b: 0xffffff, c: 0xffb347, cloud: 0.8,
  ground: 0x6a5a8a, sol: { kind: 0, c: [0x4a3a6a, 0x7a5aa8, 0xa88ad0, 0x6a5a40] }, relief: 40, hill: 0x6a5a7a, hillH: 120, rock: 0xc8b8a0,
  sun: { dir: [-0.55, 0.16, -0.82], col: [1.0, 0.66, 0.42], road: 2.5 }, sunI: 2.6, ambI: 1.35, wet: 0.35, ombre: 0.45,
  flore: [['sapin', 260, 6, 200, 0.7, 1.5], ['feuillu', 220, 12, 240, 0.8, 1.7], ['rocher', 70, 8, 200, 0.4, 1.0]], grass: { c0: 0x3a2a5a, c1: 0x7a5ac0, c2: 0xc8a0ff, h: 1.0 } };
// ambiances de ville spéciales
const VILLE = {
  // docks industriels sous la pluie : grues, sodium orangé et néons turquoise, flaques partout
  docks: { sky: [[0.01, 0.015, 0.03], [0.06, 0.08, 0.12], [0.30, 0.20, 0.12]], fog: 0x1a1c24, dens: 0.0032, a: 0x19e6ff, b: 0xff8a1f, c: 0xffd23f, lamp: 0xffb060, palms: false, rain: true, wet: 1,
    win: [['#ffb060', '#ffd8a0', '#ff9040'], ['#39d7ff', '#a8eaff', '#ffffff'], ['#ffe0b0', '#ffb060', '#40c8ff'], ['#ff8a40', '#ffd23f', '#a0e8ff']] },
  // périphérique de nuit : six voies, rose et bleu
  periph: { sky: [[0.02, 0.0, 0.08], [0.12, 0.03, 0.30], [0.55, 0.12, 0.50]], fog: 0x2a0e48, dens: 0.0022, a: 0xff2bd0, b: 0x2f8aff, c: 0xffffff, lamp: 0xf0e8ff, palms: false, rain: false, wet: 0.85,
    win: [['#ff4fd8', '#ff9be8', '#ffffff'], ['#2f8aff', '#a8d0ff', '#ffffff'], ['#ffd0f0', '#b45cff', '#40a0ff'], ['#ffffff', '#ff70c0', '#80b0ff']] },
  central: { sky: [[0.02, 0.01, 0.06], [0.16, 0.05, 0.30], [0.95, 0.55, 0.12]], fog: 0x24103a, dens: 0.0024, a: 0xffc23f, b: 0x8a5cff, c: 0xff3f7a, lamp: 0xffe6b8, palms: true, rain: false, wet: 0.7,
    win: [['#ffd070', '#fff0c8', '#ffb347'], ['#b48cff', '#e0d0ff', '#8a5cff'], ['#ff7aa8', '#ffd0e0', '#ffe9a0'], ['#ffe9a0', '#c8a8ff', '#ff9bb8']] },
  orbitale: { sky: [[0.0, 0.0, 0.0], [0.0, 0.0, 0.02], [0.03, 0.05, 0.14]], fog: 0x02030a, dens: 0.0015, a: 0x7fd8ff, b: 0xffffff, c: 0x8a5cff, lamp: 0xeaf4ff, palms: false, rain: false, wet: 0.9,
    win: [['#bfe4ff', '#ffffff', '#80c0ff'], ['#b48cff', '#e0d0ff', '#ffffff'], ['#50a0ff', '#d0e8ff', '#ffffff'], ['#ffffff', '#9fd0ff', '#c8a8ff']] },
  megapole: { sky: [[0.02, 0.0, 0.05], [0.18, 0.02, 0.30], [0.75, 0.9, 0.1]], fog: 0x1c0a34, dens: 0.0022, a: 0xb6ff2b, b: 0xff2bd0, c: 0x19e6ff, lamp: 0xf0ffd0, palms: false, rain: true,
    win: [['#c8ff60', '#f0ffc0', '#60ffa0'], ['#ff4fd8', '#ff9be8', '#ffd0f0'], ['#39d7ff', '#bfe9ff', '#ffffff'], ['#ffe9a0', '#c8ff60', '#ff5ad0']] },
};

// Les circuits. Points de contrôle en mètres (x, z), boucle fermée, départ au premier point.
export const TRACKS = [
  { id: 'baie', nom: 'Baie Néon', desc: 'Rapide et large',
    pts: [[0,0],[0,-220],[60,-330],[200,-360],[300,-290],[320,-150],[250,-40],[300,80],[220,190],[80,170],[20,90]],
    theme: { sky: [[0.012,0.02,0.11],[0.10,0.05,0.42],[0.42,0.10,0.55]], fog: 0x22104c, dens: 0.0027,
      a: 0x19e6ff, b: 0xff2bd0, c: 0x2f58ff, lamp: 0xfff1dd, palms: true, rain: true,
      win: [['#39d7ff','#5fa8ff','#bfe9ff'],['#ff4fd8','#b45cff','#ff9be8'],['#ffd9a0','#fff2d0','#6fd0ff'],['#4b6bff','#27e0ff','#ff5ad0']] } },
  { id: 'solaire', nom: 'Boulevard Solaire', desc: 'Longues lignes droites',
    pts: [[0,0],[0,-380],[70,-470],[180,-450],[230,-350],[200,-230],[260,-120],[240,20],[140,90],[40,70]],
    theme: { sky: [[0.05,0.02,0.16],[0.42,0.08,0.38],[1.0,0.36,0.12]], fog: 0x4a1638, dens: 0.0024,
      a: 0xff8a1f, b: 0xff2b7a, c: 0xffd23f, lamp: 0xffd9a8, palms: true, rain: false,
      win: [['#ffb347','#ffd9a0','#fff2d0'],['#ff5a8a','#ff9bb8','#ffd0a0'],['#ffe9c0','#ffc070','#ff7a50'],['#c050ff','#ff70c0','#ffb060']] } },
  { id: 'jade', nom: 'Quartier Jade', desc: 'Technique, virages serrés',
    pts: [[0,0],[0,-150],[-60,-240],[-20,-340],[90,-370],[170,-300],[150,-200],[220,-120],[300,-170],[380,-110],[360,10],[260,60],[170,20],[90,80],[20,70]],
    theme: { sky: [[0.0,0.03,0.05],[0.02,0.16,0.16],[0.08,0.50,0.36]], fog: 0x06302c, dens: 0.0030,
      a: 0x20ffa0, b: 0x19d0ff, c: 0xb6ff2b, lamp: 0xe0fff0, palms: false, rain: true,
      win: [['#40ffb0','#a0ffd8','#d8fff0'],['#20d0ff','#80e8ff','#c0f8ff'],['#c8ff60','#f0ffc0','#60ffa0'],['#ffe9a0','#60ffc0','#30c0ff']] } },
  { id: 'port', nom: 'Port Glacé', desc: 'Fluide, grandes courbes',
    pts: [[0,0],[0,-200],[-80,-290],[-200,-300],[-280,-220],[-260,-100],[-170,-60],[-150,40],[-230,130],[-180,230],[-60,240],[10,150]],
    theme: { sky: [[0.01,0.02,0.08],[0.06,0.12,0.36],[0.42,0.62,0.90]], fog: 0x142a52, dens: 0.0030,
      a: 0x7fd8ff, b: 0xffffff, c: 0x3f6bff, lamp: 0xeaf4ff, palms: false, rain: true,
      win: [['#bfe4ff','#ffffff','#80c0ff'],['#6fa8ff','#a8d0ff','#e0f0ff'],['#fff2d0','#c0e0ff','#80b0ff'],['#50a0ff','#d0e8ff','#ffffff']] } },
  { id: 'ecarlate', nom: 'Circuit Écarlate', desc: 'Long et difficile',
    pts: [[0,0],[0,-180],[70,-260],[40,-360],[-60,-400],[-150,-340],[-140,-230],[-220,-160],[-320,-200],[-400,-130],[-380,-10],[-280,30],[-240,130],[-140,170],[-60,110],[-10,60]],
    theme: { sky: [[0.05,0.0,0.02],[0.28,0.02,0.06],[0.85,0.14,0.08]], fog: 0x3c0a12, dens: 0.0028,
      a: 0xff2a2a, b: 0xffb020, c: 0xff5a1f, lamp: 0xffe0c0, palms: true, rain: false,
      win: [['#ff6a4a','#ffb090','#ffd8c0'],['#ffc040','#ffe0a0','#fff0d0'],['#ff3a5a','#ff90a0','#ffd0b0'],['#ffe9c0','#ff8040','#ff4040']] } },
  { id: 'couchant', nom: 'Côte du Couchant', desc: 'Bord de mer au coucher du soleil',
    pts: [[0,0],[0,-280],[80,-380],[220,-400],[340,-320],[360,-180],[300,-60],[360,60],[300,180],[160,200],[60,140]],
    theme: NATURE.couchant },
  { id: 'prairie', nom: 'Prairie du Soleil', desc: 'Campagne au soleil',
    pts: [[0,0],[0,-260],[90,-380],[250,-400],[360,-300],[340,-160],[430,-60],[400,90],[270,150],[150,90],[60,130]],
    theme: NATURE.prairie },
  { id: 'canyon', nom: 'Canyon Rouge', desc: 'Désert au couchant',
    pts: [[0,0],[0,-200],[-70,-300],[-190,-330],[-270,-250],[-240,-140],[-320,-50],[-290,70],[-190,130],[-90,120],[-15,70]],
    theme: NATURE.canyon },
  { id: 'neige', nom: 'Col Blanc', desc: 'Montagne enneigée',
    pts: [[0,0],[0,-240],[80,-330],[50,-440],[-60,-470],[-150,-400],[-140,-290],[-220,-200],[-200,-80],[-130,20],[-70,90],[-5,70]],
    theme: NATURE.neige },
  // ---------- nouveaux circuits (tracés tirés d'une graine, comme ceux de l'atelier) ----------
  { id: 'autodrome', nom: 'Autodrome du Lac', desc: 'Grand circuit rapide', echelle: 1, half: 19, pts: tracer({ graine: 4711, virages: 13, taille: 430, chaos: 55, largeur: 28 }), theme: NATURE.prairie },
  { id: 'automne', nom: "Forêt d'Automne", desc: 'Sous-bois roux et dorés', echelle: 1, half: 16, pts: tracer({ graine: 9031, virages: 15, taille: 340, chaos: 60, largeur: 24 }), theme: NATURE.automne },
  { id: 'dunes', nom: "Dunes d'Or", desc: 'Désert, grandes courbes', echelle: 1, half: 22, pts: tracer({ graine: 1207, virages: 9, taille: 470, chaos: 40, largeur: 34 }), theme: NATURE.dunes },
  { id: 'volcan', nom: 'Volcan Noir', desc: 'Lave, cendre et braises', echelle: 1, half: 16, pts: tracer({ graine: 6660, virages: 14, taille: 330, chaos: 70, largeur: 26 }), theme: NATURE.volcan },
  { id: 'aurore', nom: 'Aurore Boréale', desc: 'Toundra de nuit sous la neige', echelle: 1, half: 19, pts: tracer({ graine: 3158, virages: 11, taille: 380, chaos: 50, largeur: 28 }), theme: NATURE.aurore },
  { id: 'orbitale', nom: 'Station Orbitale', desc: 'Spécial · gravité lunaire, sauts géants', echelle: 1, half: 22, pts: tracer({ graine: 2099, virages: 10, taille: 360, chaos: 45, largeur: 36 }), theme: VILLE.orbitale,
    regles: { vitesse: 1.15, adherence: 1, gravite: 0.3, saut: 2.4, boost: 1.6 }, contenu: { obstacles: 10, tremplins: 14, turbos: 10, trafic: 0 } },
  { id: 'megapole', nom: 'Mégapole 2099', desc: 'Spécial · tout va 60 % plus vite', echelle: 1, half: 25, pts: tracer({ graine: 7777, virages: 8, taille: 520, chaos: 35, largeur: 40 }), theme: VILLE.megapole,
    regles: { vitesse: 1.6, adherence: 1.2, gravite: 1, saut: 1, boost: 1.5 }, contenu: { obstacles: 14, tremplins: 4, turbos: 16, trafic: 0 } },
  // ---------- circuits de la Coupe Vitesse (Grand Prix) ----------
  { id: 'anneau', nom: 'Anneau de Vitesse', desc: 'Presque un ovale : à fond', echelle: 1, half: 23, pts: tracer({ graine: 515, virages: 6, taille: 520, chaos: 22, largeur: 36 }), theme: NATURE.prairie },
  { id: 'lagune', nom: 'Lagune Turquoise', desc: 'Sable blanc et eau claire', echelle: 1, half: 19, pts: tracer({ graine: 8080, virages: 12, taille: 400, chaos: 50, largeur: 28 }), theme: NATURE.lagune },
  { id: 'glacier', nom: 'Glacier Bleu', desc: 'Technique, sur la glace', echelle: 1, half: 16, pts: tracer({ graine: 4242, virages: 16, taille: 360, chaos: 62, largeur: 24 }), theme: NATURE.glacier },
  // ---------- nouveaux circuits (2.1) ----------
  { id: 'jungle', nom: 'Jungle Émeraude', desc: 'Forêt tropicale sous la pluie', nouveau: true, echelle: 1, half: 20, pts: tracer({ graine: 31337, virages: 14, taille: 420, chaos: 58, largeur: 40 }), theme: NATURE.jungle },
  { id: 'docks', nom: 'Docks sous la Pluie', desc: 'Port industriel, flaques et sodium', nouveau: true, echelle: 1.7, half: 22,
    pts: [[0,0],[0,-230],[50,-320],[170,-340],[260,-280],[250,-170],[330,-110],[420,-150],[470,-60],[430,60],[320,110],[220,80],[140,150],[40,140]], theme: VILLE.docks },
  { id: 'periph', nom: 'Périphérique', desc: 'Six voies, très rapide', nouveau: true, echelle: 1, half: 27, pts: tracer({ graine: 7070, virages: 9, taille: 560, chaos: 30, largeur: 54 }), theme: VILLE.periph },
  { id: 'lavande', nom: 'Champs de Lavande', desc: 'Provence au soleil couchant', nouveau: true, echelle: 1, half: 21, pts: tracer({ graine: 1313, virages: 12, taille: 450, chaos: 48, largeur: 42 }), theme: NATURE.lavande },
  { id: 'central', nom: 'Néon Central', desc: 'Le grand circuit de la ville', echelle: 1, half: 20, pts: tracer({ graine: 1999, virages: 12, taille: 390, chaos: 55, largeur: 30 }), theme: VILLE.central },
];

// Grand Prix : chaque coupe enchaîne quatre circuits ; les points de chaque course s'additionnent.
export const COUPES = [
  { id: 'neon', nom: 'Coupe Néon', desc: 'Les nuits de la ville', col: '#19e6ff', pistes: ['baie', 'solaire', 'jade', 'port'] },
  { id: 'nature', nom: 'Coupe Nature', desc: 'Prairie, mer, désert et neige', col: '#7fdc4a', pistes: ['prairie', 'couchant', 'canyon', 'neige'] },
  { id: 'braise', nom: 'Coupe Braise', desc: 'Du sable au volcan', col: '#ff6a1f', pistes: ['dunes', 'automne', 'ecarlate', 'volcan'] },
  { id: 'tempete', nom: 'Coupe Tempête', desc: 'Quatre courses sous l\'orage', col: '#8fa8ff', amb: 'orage', pistes: ['port', 'prairie', 'jade', 'autodrome'] },
  { id: 'vitesse', nom: 'Coupe Vitesse', desc: 'Quatre circuits inédits', col: '#ffd23f', pistes: ['anneau', 'lagune', 'glacier', 'central'] },
  { id: 'horizon', nom: 'Coupe Horizon', desc: 'Les quatre nouveaux circuits', col: '#20ffa0', pistes: ['docks', 'jungle', 'lavande', 'periph'] },
  { id: 'legende', nom: 'Coupe Légende', desc: 'Les plus durs, jusqu\'à Mégapole', col: '#ff2bd0', pistes: ['autodrome', 'aurore', 'orbitale', 'megapole'] },
];
export const GP_POINTS = [25, 18, 15, 12, 10, 8, 6, 4, 2, 1, 0, 0];

// Balade libre : une longue route de campagne à deux voies, sans barrières
export const ROUTE_LIBRE = { id: 'libre', nom: 'Route des Prés', desc: 'Balade libre', echelle: 1, half: 4.6, limit: 12.4, libre: true,
  pts: [[0,0],[0,-600],[200,-1000],[700,-1200],[1200,-1000],[1500,-500],[1300,0],[1500,500],[1100,900],[500,800],[200,400]],
  theme: Object.assign({}, NATURE.prairie, { flore: [['feuillu', 900, 20, 260, 0.8, 1.9], ['rocher', 80, 18, 200, 0.4, 1.0]] }) };

// Ligne Droite : deux lignes droites de 3 km à travers les champs, pour se balader sans circulation
export const ROUTE_LIGNE = { id: 'ligne', nom: 'Ligne Droite', desc: 'Balade sans circulation', echelle: 1, half: 5.5, limit: 14, libre: true,
  pts: [[0,0],[0,-1500],[0,-3000],[120,-3220],[330,-3290],[540,-3220],[660,-3000],[660,-1500],[660,0],[540,220],[330,290],[120,220]],
  theme: Object.assign({}, NATURE.prairie, { flore: [['feuillu', 700, 22, 300, 0.8, 2.0], ['rocher', 60, 18, 220, 0.4, 1.0]] }) };

// Ambiance choisie au départ : chaque circuit peut se courir de jour, de nuit ou sous un orage violent.
// On part du décor d'origine et on change le ciel, la lumière, le brouillard et la météo.
export function ambiance(th, mode) {
  if (!mode || mode === 'def') return th;
  const city = th.env !== 'nature', o = Object.assign({}, th);
  if (mode === 'jour') {
    Object.assign(o, { sky: [[0.10, 0.28, 0.72], [0.36, 0.60, 0.95], [0.80, 0.88, 0.96]], sun: { dir: [0.42, 0.62, -0.66], col: [1.0, 0.93, 0.8], road: 2.0 }, sunI: 2.9, ambI: 1.5, cloud: 0.9, phares: false, flare: 1, ombre: 0.6 });
    o.fog = city ? 0xb4c6dc : (th.rain === 'neige' ? 0xc8d4e6 : 0xbcd4ee); o.rain = th.rain === 'neige' ? 'neige' : false;
    if (city) { o.jourVille = true; o.wet = 0.3; o.dens = th.dens * 0.6; }
  } else if (mode === 'nuit') {
    if (city) return th;                                   // les circuits de ville sont déjà de nuit
    Object.assign(o, { sky: [[0.004, 0.008, 0.03], [0.015, 0.03, 0.09], [0.04, 0.07, 0.16]], fog: 0x070b18, dens: th.dens * 1.15, sun: { dir: [0.3, 0.55, -0.78], col: [0.6, 0.7, 1.0], road: 0.9 },
      sunI: 0.6, ambI: 0.42, cloud: 0.16, phares: true, flare: 0.3, ombre: 0.25, nuit: true });
  } else if (mode === 'orage') {
    Object.assign(o, { orage: true, rain: th.rain === 'neige' ? 'neige' : true, wet: 1, phares: true, flare: 0, dens: th.dens * 2.1, ombre: 0.2 });
    if (city) { o.sky = [[0.0, 0.0, 0.015], [0.03, 0.03, 0.07], [0.09, 0.08, 0.14]]; o.fog = new Color(th.fog).multiplyScalar(0.45).getHex(); }
    else Object.assign(o, { sky: [[0.03, 0.035, 0.05], [0.09, 0.10, 0.13], [0.17, 0.18, 0.21]], fog: 0x23262e, sun: { dir: [0.2, 0.75, -0.63], col: [0.55, 0.58, 0.68], road: 0.8 }, sunI: 0.85, ambI: 0.75, cloud: 0.28, nuit: true });
  }
  return o;
}
