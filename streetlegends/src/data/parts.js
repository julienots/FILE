// Pièces mécaniques et esthétiques, tarifs, effets sur les performances.
import { perfIndex } from './cars.js';
import { TIRES, RIMS, LIVERIES, STICKERS, NEONS, PAINT_FINISH } from '../vehicle/carBuilder.js';

export const UPGRADES = {
  engine: { name: 'Moteur', icon: '⚙', max: 5, desc: 'Préparation moteur : plus de puissance et de vitesse de pointe.', eff: 'Accélération et vitesse max' },
  turbo: { name: 'Turbo', icon: '🌀', max: 4, desc: 'Suralimentation : couple et reprises.', eff: 'Accélération' },
  transmission: { name: 'Transmission', icon: '⛓', max: 3, desc: 'Différentiel et arbres renforcés : la puissance passe mieux au sol.', eff: 'Motricité et accélération' },
  gearbox: { name: 'Boîte de vitesses', icon: '🕹', max: 3, desc: 'Passages de rapports plus rapides, étagement course.', eff: 'Temps de passage et vitesse max' },
  suspension: { name: 'Suspension', icon: '🪛', max: 4, desc: 'Combinés filetés : caisse tenue, appuis plus francs.', eff: 'Tenue de route' },
  brakes: { name: 'Freins', icon: '🛑', max: 4, desc: 'Disques et étriers haute performance.', eff: 'Freinage' },
  tires: { name: 'Gommes', icon: '◎', max: 4, desc: 'Mélange de gomme plus tendre.', eff: 'Adhérence' },
  weight: { name: 'Allègement', icon: '🪶', max: 4, desc: 'Carbone, vitrages fins, intérieur dépouillé.', eff: 'Poids, accélération, freinage' },
};
export const emptyUpg = () => ({ engine: 0, turbo: 0, transmission: 0, gearbox: 0, suspension: 0, brakes: 0, tires: 0, weight: 0 });

export function computeStats(spec, upg = {}, cfg = {}) {
  const b = spec.stats, u = { ...emptyUpg(), ...upg };
  const tire = TIRES[cfg.tire] || TIRES.street;
  const st = { ...b };
  st.top = b.top * (1 + 0.022 * u.engine + 0.01 * u.turbo + 0.01 * u.gearbox);
  st.a100 = b.a100 * (1 - 0.035 * u.engine) * (1 - 0.03 * u.turbo) * (1 - 0.02 * u.transmission) * (1 - 0.015 * u.weight);
  st.mass = b.mass * (1 - 0.035 * u.weight);
  st.grip = b.grip * (1 + 0.02 * u.suspension + 0.03 * u.tires) + tire.grip;
  st.brake = b.brake * (1 + 0.06 * u.brakes + 0.012 * u.weight);
  st.susp = b.susp * (1 - 0.08 * u.suspension);
  st.off = Math.min(1, Math.max(0, b.off + tire.off + 0.02 * u.suspension));
  st.drift = Math.min(1, b.drift + (cfg.tire === 'drift' ? 0.2 : 0) + 0.02 * u.transmission);
  st.shift = 0.22 - 0.05 * u.gearbox;
  st.pi = perfIndex(st);
  return st;
}
export function upgradeCost(spec, key, level) {
  const base = Math.max(700, spec.price * 0.014 + 500);
  const f = { engine: 1.5, turbo: 1.3, transmission: 1, gearbox: 1, suspension: 0.9, brakes: 0.8, tires: 0.7, weight: 1.1 }[key];
  return Math.round((base * f * Math.pow(level, 1.55)) / 50) * 50;
}
export const upgradeLevelReq = (level) => Math.max(1, level * 3 - 3);

export const COLORS = [
  ['Rouge Corsa', 0xd4002a], ['Orange Lava', 0xff7a00], ['Jaune Solar', 0xffc928], ['Vert Lime', 0xa6e22e], ['Vert Racing', 0x1f6f3a], ['Turquoise Baie', 0x21c7b7],
  ['Bleu Riviera', 0x2a6cff], ['Bleu Nuit', 0x14213d], ['Violet Néon', 0x8a5cff], ['Rose Vice', 0xff5fa2], ['Blanc Glacier', 0xf2f4f8], ['Gris Titane', 0x7d8794],
  ['Gris Graphite', 0x34383f], ['Noir Onyx', 0x101216], ['Champagne', 0xc9b17a], ['Bronze', 0x8a5a36], ['Cyan Électrique', 0x00d2ff], ['Bordeaux', 0x6b0f1a],
  ['Sable', 0xd9c7a3], ['Kaki', 0x6b7a3a], ['Menthe', 0x9ae6c1], ['Corail', 0xff6f59], ['Or', 0xd4a017], ['Lilas', 0xc7a6ff],
];
// Pièces esthétiques : [id, nom, prix en Street Coins, niveau requis, prix en Essence Tokens]
const P = (id, name, price, lvl = 1, tok = 0) => ({ id, name, price, lvl, tok });
export const VISUAL = {
  finish: { name: 'Finition peinture', icon: '🎨', items: Object.entries(PAINT_FINISH).map(([id, f]) => P(id, f.name, { gloss: 0, metal: 1500, satin: 2500, matte: 3000, pearl: 6000, candy: 8000, chrome: 0 }[id], { gloss: 1, metal: 1, satin: 3, matte: 4, pearl: 8, candy: 12, chrome: 18 }[id], id === 'chrome' ? 25 : 0)) },
  rim: { name: 'Jantes', icon: '◎', items: Object.entries(RIMS).map(([id, r], i) => P(id, r.name, [0, 1800, 2600, 3400, 4200, 5200, 1500][i], [1, 2, 4, 6, 9, 12, 3][i])) },
  tire: { name: 'Pneus', icon: '⬤', items: Object.entries(TIRES).map(([id, t], i) => P(id, t.name, [0, 2500, 6000, 3500, 3000][i], [1, 3, 8, 5, 2][i])) },
  spoiler: { name: 'Aileron', icon: '🪽', items: [P('none', 'Sans', 0), P('lip', 'Becquet', 1200, 2), P('duck', 'Queue de canard', 2200, 4), P('wing', 'Aileron sport', 3800, 6), P('gt', 'Aileron GT', 6500, 10), P('mega', 'Double plan course', 12000, 16)] },
  bumper: { name: 'Pare-chocs', icon: '▭', items: [P('stock', 'Origine', 0), P('sport', 'Lame sport', 2400, 3), P('race', 'Lame + ailettes', 5200, 9), P('offroad', 'Pare-buffle', 3000, 5)] },
  hood: { name: 'Capot', icon: '⬒', items: [P('stock', 'Origine', 0), P('scoop', 'Prise d\'air', 2000, 3), P('vents', 'Extracteurs', 2600, 5), P('carbon', 'Carbone', 5000, 9), P('blower', 'Compresseur apparent', 9000, 13)] },
  skirts: { name: 'Bas de caisse', icon: '▂', items: [P('none', 'Origine', 0), P('sport', 'Sport', 1600, 2), P('wide', 'Larges carbone', 3800, 8)] },
  diffuser: { name: 'Diffuseur', icon: '⫼', items: [P('none', 'Sans', 0), P('sport', 'Sport', 2200, 4), P('race', 'Course', 4800, 10)] },
  exhaust: { name: 'Échappement', icon: '≡', items: [P('dual', 'Double', 0), P('single', 'Simple', 500), P('center', 'Central', 2400, 4), P('quad', 'Quadruple', 3600, 7), P('side', 'Latéral', 4200, 9)] },
  neon: { name: 'Néons', icon: '✺', items: Object.keys(NEONS).map((id, i) => P(id, id === 'none' ? 'Sans' : 'Néon ' + ['', 'cyan', 'rose', 'lime', 'orange', 'violet', 'rouge', 'blanc', 'or'][i], id === 'none' ? 0 : 3500 + i * 250, id === 'none' ? 1 : 5, id === 'gold' ? 8 : 0)) },
  livery: { name: 'Livrée', icon: '▚', items: Object.entries(LIVERIES).filter(([, l]) => !l.hidden).map(([id, l], i) => P(id, l.name, id === 'none' ? 0 : 1800 + i * 450, id === 'none' ? 1 : Math.min(14, 1 + i), id === 'superessence' ? 15 : 0)) },
  sticker: { name: 'Stickers', icon: '★', items: Object.entries(STICKERS).map(([id, n], i) => P(id, n, id === 'none' ? 0 : 600 + i * 150, 1)) },
  wheel: { name: 'Volant', icon: '⊚', items: [P('sport', 'Sport', 0), P('classic', 'Bois classique', 1500, 3), P('race', 'Course', 2800, 7)] },
  seats: { name: 'Sièges', icon: '💺', items: [P('stock', 'Origine', 0), P('bucket', 'Baquets', 3200, 5), P('luxury', 'Cuir luxe', 5200, 9)] },
  dash: { name: 'Tableau de bord', icon: '▤', items: [P('stock', 'Origine', 0), P('carbon', 'Carbone', 2600, 6), P('wood', 'Ronce de noyer', 3000, 8)] },
};
export const PAINT_PRICE = 800;
export const TINT_PRICE = 600;
// Emplacements de couleur libres (prix fixe par changement)
export const COLOR_SLOTS = {
  color: { name: 'Carrosserie', price: 800 }, rimColor: { name: 'Jantes', price: 400 }, caliper: { name: 'Étriers', price: 300 }, liveryColor: { name: 'Livrée 1', price: 300 }, liveryColor2: { name: 'Livrée 2', price: 300 },
  stickerColor: { name: 'Sticker', price: 150 }, seatColor: { name: 'Sellerie', price: 400 }, ambient: { name: 'Éclairage intérieur', price: 300 }, headColor: { name: 'Phares', price: 300 },
};
