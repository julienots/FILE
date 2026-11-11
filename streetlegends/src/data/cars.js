// Catalogue des véhicules — licence originale SUPERESSENCE: STREET LEGENDS.
// Marques et modèles fictifs. Chaque voiture a sa propre silhouette (paramètres de carrosserie) et ses propres réglages.

export const CATEGORIES = {
  compact: { name: 'Compactes', icon: '◐' },
  sport: { name: 'Sportives', icon: '◆' },
  super: { name: 'Supercars', icon: '▲' },
  hyper: { name: 'Hypercars', icon: '✦' },
  muscle: { name: 'Muscle cars', icon: '■' },
  drift: { name: 'Drift', icon: '≋' },
  rally: { name: 'Rallye', icon: '✚' },
  offroad: { name: 'Tout-terrain', icon: '⬣' },
  luxury: { name: 'Luxe', icon: '♛' },
};

// Gabarits de carrosserie. t = position le long de la voiture (0 = nez, 1 = poupe).
const BODY = {
  hatch: { L: 3.95, W: 1.76, H: 1.46, wb: 2.52, wr: 0.31, gc: 0.16, belt: 0.86, hood: 0.8, nose: 0.5, noseW: 0.82, tailW: 0.9, deck: 0.98, cab: [0.25, 0.42, 0.86, 0.96], tumble: 0.2, box: 0.42, flare: 0.03, fo: 0.19 },
  sedan: { L: 4.6, W: 1.82, H: 1.42, wb: 2.75, wr: 0.33, gc: 0.15, belt: 0.84, hood: 0.8, nose: 0.5, noseW: 0.82, tailW: 0.86, deck: 0.94, cab: [0.27, 0.42, 0.66, 0.83], tumble: 0.22, box: 0.42, flare: 0.03, fo: 0.19 },
  coupe: { L: 4.4, W: 1.86, H: 1.3, wb: 2.6, wr: 0.34, gc: 0.12, belt: 0.78, hood: 0.78, nose: 0.42, noseW: 0.8, tailW: 0.86, deck: 0.92, cab: [0.31, 0.47, 0.64, 0.86], tumble: 0.25, box: 0.45, flare: 0.05, fo: 0.2 },
  fastback: { L: 4.5, W: 1.9, H: 1.27, wb: 2.65, wr: 0.345, gc: 0.115, belt: 0.76, hood: 0.76, nose: 0.38, noseW: 0.78, tailW: 0.88, deck: 0.96, cab: [0.33, 0.48, 0.6, 0.93], tumble: 0.26, box: 0.46, flare: 0.06, fo: 0.2 },
  wedge: { L: 4.5, W: 2.0, H: 1.14, wb: 2.68, wr: 0.35, gc: 0.1, belt: 0.72, hood: 0.52, nose: 0.2, noseW: 0.74, tailW: 0.94, deck: 1.04, cab: [0.22, 0.4, 0.52, 0.7], tumble: 0.3, box: 0.4, flare: 0.07, fo: 0.21 },
  muscle: { L: 4.85, W: 1.94, H: 1.33, wb: 2.8, wr: 0.36, gc: 0.14, belt: 0.84, hood: 0.92, nose: 0.75, noseW: 0.93, tailW: 0.93, deck: 0.93, cab: [0.37, 0.5, 0.65, 0.84], tumble: 0.2, box: 0.34, flare: 0.05, fo: 0.18 },
  suv: { L: 4.6, W: 1.95, H: 1.82, wb: 2.75, wr: 0.42, gc: 0.3, belt: 1.12, hood: 0.92, nose: 0.75, noseW: 0.92, tailW: 0.95, deck: 1.0, cab: [0.26, 0.37, 0.9, 0.97], tumble: 0.14, box: 0.3, flare: 0.07, fo: 0.17 },
  pickup: { L: 5.2, W: 2.0, H: 1.86, wb: 3.2, wr: 0.44, gc: 0.32, belt: 1.15, hood: 0.93, nose: 0.8, noseW: 0.94, tailW: 0.96, deck: 0.92, cab: [0.25, 0.35, 0.55, 0.6], tumble: 0.13, box: 0.28, flare: 0.08, fo: 0.17 },
  roadster: { L: 4.1, W: 1.82, H: 1.18, wb: 2.4, wr: 0.33, gc: 0.12, belt: 0.76, hood: 0.8, nose: 0.4, noseW: 0.78, tailW: 0.84, deck: 0.96, cab: [0.4, 0.5, 0.5, 0.62], tumble: 0.25, box: 0.48, flare: 0.05, fo: 0.2, open: true },
  wagon: { L: 4.55, W: 1.84, H: 1.46, wb: 2.68, wr: 0.34, gc: 0.17, belt: 0.86, hood: 0.8, nose: 0.5, noseW: 0.84, tailW: 0.92, deck: 1.0, cab: [0.27, 0.41, 0.9, 0.975], tumble: 0.2, box: 0.4, flare: 0.05, fo: 0.19 },
  buggy: { L: 3.7, W: 1.9, H: 1.55, wb: 2.5, wr: 0.42, gc: 0.34, belt: 0.92, hood: 0.7, nose: 0.3, noseW: 0.6, tailW: 0.7, deck: 0.86, cab: [0.34, 0.44, 0.62, 0.74], tumble: 0.22, box: 0.5, flare: 0.0, fo: 0.14, open: true, cage: true, track: 1.12 },
  limo: { L: 5.25, W: 1.95, H: 1.48, wb: 3.2, wr: 0.36, gc: 0.15, belt: 0.9, hood: 0.88, nose: 0.7, noseW: 0.9, tailW: 0.9, deck: 0.94, cab: [0.27, 0.4, 0.7, 0.84], tumble: 0.18, box: 0.36, flare: 0.03, fo: 0.18 },
};

const C = (id, brand, model, cat, body, shape, stats, style, price, lvl, tok = 0, blurb = '') => ({
  id, brand, model, name: brand + ' ' + model, cat, body, shape: { ...BODY[body], ...shape }, stats, style, price, lvl, tok, blurb,
});
// stats : top km/h, a100 (0-100 en s), mass kg, grip (g latéral), brake (g), drive, wf (répartition avant),
//         susp (souplesse), drift (0..1), off (0..1), gears, eng (identité sonore)
export const CARS = [
  // ---------------- Compactes ----------------
  C('pico', 'Volta', 'Pico', 'compact', 'hatch', { L: 3.6, W: 1.66, H: 1.5, wb: 2.35, wr: 0.29, cab: [0.22, 0.4, 0.88, 0.97], box: 0.5 },
    { top: 168, a100: 10.4, mass: 940, grip: 0.92, brake: 0.95, drive: 'FWD', wf: 0.62, susp: 1.15, drift: 0.2, off: 0.3, gears: 5, eng: 'i3' },
    { head: 'round', tail: 'round', color: 0xffc928, grille: 'smile' }, 0, 1, 0, 'Petite, vive, increvable. Toute légende commence ici.'),
  C('lumen', 'Kaido', 'Lumen', 'compact', 'hatch', { L: 4.05, nose: 0.42, hood: 0.76, cab: [0.26, 0.44, 0.82, 0.95] },
    { top: 186, a100: 8.6, mass: 1090, grip: 0.96, brake: 1.0, drive: 'FWD', wf: 0.61, susp: 1.0, drift: 0.25, off: 0.25, gears: 5, eng: 'i4' },
    { head: 'angular', tail: 'slim', color: 0x21c7b7, grille: 'slot' }, 9000, 1, 0, 'Citadine affûtée au caractère joueur.'),
  C('bolt', 'Volta', 'Bolt RS', 'compact', 'hatch', { L: 4.0, W: 1.8, H: 1.42, wr: 0.32, flare: 0.06, gc: 0.13, cab: [0.26, 0.44, 0.84, 0.95] },
    { top: 212, a100: 6.7, mass: 1180, grip: 1.04, brake: 1.08, drive: 'FWD', wf: 0.6, susp: 0.85, drift: 0.3, off: 0.2, gears: 6, eng: 'i4t' },
    { head: 'angular', tail: 'rect', color: 0xff3d2e, grille: 'wide', spoiler: 'lip' }, 24000, 3, 0, 'La bombinette des rues : turbo, châssis ferme, sourire garanti.'),
  C('tamago', 'Raijin', 'Tamago', 'compact', 'hatch', { L: 3.45, W: 1.62, H: 1.62, wb: 2.4, wr: 0.28, box: 0.3, nose: 0.85, hood: 0.9, noseW: 0.94, tailW: 0.96, cab: [0.17, 0.26, 0.92, 0.985], tumble: 0.1 },
    { top: 158, a100: 11.5, mass: 890, grip: 0.9, brake: 0.92, drive: 'FWD', wf: 0.6, susp: 1.2, drift: 0.15, off: 0.3, gears: 5, eng: 'i3' },
    { head: 'round', tail: 'rect', color: 0x9ad1ff, grille: 'none' }, 6500, 1, 0, 'Cube de poche à la bouille irrésistible.'),
  // ---------------- Sportives ----------------
  C('kestrel', 'Vireo', 'Kestrel', 'sport', 'coupe', {},
    { top: 238, a100: 5.6, mass: 1290, grip: 1.1, brake: 1.12, drive: 'RWD', wf: 0.53, susp: 0.8, drift: 0.5, off: 0.1, gears: 6, eng: 'i4t' },
    { head: 'slim', tail: 'bar', color: 0x2a6cff, grille: 'slot' }, 42000, 4, 0, 'Propulsion légère et équilibrée : l\'école du pilotage.'),
  C('mistral', 'Solana', 'Mistral', 'sport', 'roadster', {},
    { top: 226, a100: 6.1, mass: 1080, grip: 1.12, brake: 1.1, drive: 'RWD', wf: 0.51, susp: 0.9, drift: 0.55, off: 0.1, gears: 6, eng: 'i4' },
    { head: 'round', tail: 'round', color: 0xe8283c, grille: 'smile' }, 38000, 4, 0, 'Roadster cheveux au vent pour la route de la Baie.'),
  C('azura', 'Brennar', 'Azura GT', 'sport', 'fastback', { L: 4.62, hood: 0.8 },
    { top: 262, a100: 4.8, mass: 1480, grip: 1.12, brake: 1.15, drive: 'RWD', wf: 0.54, susp: 0.82, drift: 0.45, off: 0.1, gears: 6, eng: 'v6' },
    { head: 'angular', tail: 'slim', color: 0x0f9d8c, grille: 'wide' }, 68000, 7, 0, 'Grand tourisme musclé, taillé pour avaler le périphérique.'),
  C('shiro', 'Raijin', 'Shiro R', 'sport', 'coupe', { L: 4.3, H: 1.27, nose: 0.3, hood: 0.72, flare: 0.07, cab: [0.33, 0.48, 0.62, 0.88] },
    { top: 252, a100: 4.9, mass: 1310, grip: 1.16, brake: 1.16, drive: 'AWD', wf: 0.56, susp: 0.75, drift: 0.35, off: 0.2, gears: 6, eng: 'b6' },
    { head: 'quad', tail: 'round4', color: 0xf2f4f8, grille: 'slot', spoiler: 'wing' }, 76000, 8, 0, 'Quatre roues motrices et précision chirurgicale.'),
  // ---------------- Supercars ----------------
  C('falcor', 'Corsaro', 'Falcor', 'super', 'wedge', {},
    { top: 318, a100: 3.3, mass: 1420, grip: 1.26, brake: 1.3, drive: 'RWD', wf: 0.43, susp: 0.62, drift: 0.4, off: 0.03, gears: 7, eng: 'v10' },
    { head: 'slim', tail: 'bar', color: 0xff7a00, grille: 'wide' }, 210000, 14, 0, 'Moteur central hurlant, lignes en coin : la supercar d\'affiche.'),
  C('seraph', 'Solana', 'Seraph', 'super', 'wedge', { L: 4.6, H: 1.2, nose: 0.3, hood: 0.6, cab: [0.26, 0.43, 0.56, 0.76], box: 0.5, flare: 0.08 },
    { top: 328, a100: 3.1, mass: 1480, grip: 1.28, brake: 1.3, drive: 'AWD', wf: 0.44, susp: 0.62, drift: 0.3, off: 0.03, gears: 7, eng: 'v8' },
    { head: 'angular', tail: 'slim', color: 0xd4002a, grille: 'slot' }, 260000, 17, 0, 'Sculpture italienne dans l\'âme, redoutable dans les courbes rapides.'),
  C('tempest', 'Brennar', 'Tempest', 'super', 'fastback', { L: 4.7, W: 1.98, H: 1.24, nose: 0.3, hood: 0.74, flare: 0.08, cab: [0.36, 0.5, 0.62, 0.92] },
    { top: 322, a100: 3.4, mass: 1590, grip: 1.22, brake: 1.26, drive: 'RWD', wf: 0.5, susp: 0.68, drift: 0.45, off: 0.04, gears: 7, eng: 'v12' },
    { head: 'slim', tail: 'slim', color: 0x34383f, grille: 'wide' }, 235000, 15, 0, 'V12 à l\'avant, élégance brutale.'),
  C('nyx', 'Helix', 'Nyx', 'super', 'wedge', { L: 4.4, W: 1.96, H: 1.12, box: 0.34, nose: 0.15, hood: 0.5, tailW: 0.98 },
    { top: 308, a100: 2.9, mass: 1650, grip: 1.3, brake: 1.28, drive: 'AWD', wf: 0.47, susp: 0.6, drift: 0.25, off: 0.03, gears: 1, eng: 'ev' },
    { head: 'bar', tail: 'bar', color: 0x8a5cff, grille: 'none' }, 285000, 19, 0, 'Supercar électrique : silence, puis catapulte.'),
  // ---------------- Hypercars ----------------
  C('aeon', 'Helix', 'Aeon', 'hyper', 'wedge', { L: 4.75, W: 2.06, H: 1.1, wb: 2.8, nose: 0.12, hood: 0.46, cab: [0.25, 0.41, 0.52, 0.74], box: 0.52, flare: 0.1, wr: 0.36 },
    { top: 392, a100: 2.4, mass: 1380, grip: 1.42, brake: 1.45, drive: 'AWD', wf: 0.44, susp: 0.52, drift: 0.25, off: 0.02, gears: 7, eng: 'v12' },
    { head: 'slim', tail: 'bar', color: 0x00d2ff, grille: 'none', spoiler: 'wing' }, 900000, 28, 40, 'Hybride de 1 400 ch. Au-delà, il n\'y a que le ciel.'),
  C('imperator', 'Corsaro', 'Imperator', 'hyper', 'wedge', { L: 4.9, W: 2.08, H: 1.16, wb: 2.85, nose: 0.25, hood: 0.56, cab: [0.24, 0.4, 0.55, 0.78], box: 0.44, flare: 0.1, wr: 0.365 },
    { top: 418, a100: 2.5, mass: 1790, grip: 1.36, brake: 1.4, drive: 'AWD', wf: 0.45, susp: 0.56, drift: 0.2, off: 0.02, gears: 7, eng: 'v12' },
    { head: 'quad', tail: 'slim', color: 0x101216, grille: 'wide' }, 1250000, 32, 60, 'Seize cylindres d\'orgueil. Le roi du périphérique.'),
  C('valkyr', 'Vireo', 'Valkyr', 'hyper', 'wedge', { L: 4.55, W: 2.02, H: 1.06, nose: 0.1, hood: 0.44, cab: [0.27, 0.42, 0.5, 0.7], box: 0.56, flare: 0.12, tailW: 0.98, gc: 0.085 },
    { top: 372, a100: 2.3, mass: 1150, grip: 1.5, brake: 1.5, drive: 'RWD', wf: 0.42, susp: 0.48, drift: 0.3, off: 0.02, gears: 7, eng: 'v10' },
    { head: 'slim', tail: 'round4', color: 0xc8ff00, grille: 'none', spoiler: 'gt' }, 1050000, 30, 50, 'Une voiture de course avec des plaques. À peine.'),
  // ---------------- Muscle cars ----------------
  C('brawler', 'Atlas', 'Brawler', 'muscle', 'muscle', {},
    { top: 248, a100: 5.2, mass: 1690, grip: 0.98, brake: 1.0, drive: 'RWD', wf: 0.56, susp: 1.0, drift: 0.7, off: 0.12, gears: 5, eng: 'v8m' },
    { head: 'round2', tail: 'rect3', color: 0x1f6f3a, grille: 'wide' }, 52000, 5, 0, 'Gros V8, ligne droite et pneus qui fument.'),
  C('rampage', 'Atlas', 'Rampage 440', 'muscle', 'muscle', { L: 5.05, W: 1.98, hood: 0.96, nose: 0.9, cab: [0.4, 0.52, 0.64, 0.82], box: 0.28 },
    { top: 262, a100: 4.7, mass: 1760, grip: 0.97, brake: 0.98, drive: 'RWD', wf: 0.57, susp: 1.05, drift: 0.75, off: 0.12, gears: 4, eng: 'v8m' },
    { head: 'hidden', tail: 'bar', color: 0x5a1ea8, grille: 'full', hood: 'blower' }, 88000, 9, 0, 'Classique américain à compresseur apparent.'),
  C('outlaw', 'Monarch', 'Outlaw', 'muscle', 'fastback', { L: 4.8, W: 1.95, H: 1.32, hood: 0.9, nose: 0.6, belt: 0.82, box: 0.36, noseW: 0.9, cab: [0.37, 0.5, 0.6, 0.9] },
    { top: 276, a100: 4.3, mass: 1720, grip: 1.04, brake: 1.08, drive: 'RWD', wf: 0.55, susp: 0.9, drift: 0.65, off: 0.1, gears: 6, eng: 'v8m' },
    { head: 'angular', tail: 'rect3', color: 0xf5a300, grille: 'wide', hood: 'scoop' }, 115000, 11, 0, 'Pony car moderne : 700 ch et aucune excuse.'),
  C('hammer', 'Atlas', 'Hammerhead', 'muscle', 'muscle', { L: 4.95, H: 1.28, W: 2.0, flare: 0.09, gc: 0.12, hood: 0.94, cab: [0.39, 0.51, 0.62, 0.86] },
    { top: 298, a100: 3.7, mass: 1810, grip: 1.08, brake: 1.12, drive: 'RWD', wf: 0.55, susp: 0.85, drift: 0.6, off: 0.08, gears: 6, eng: 'v8m' },
    { head: 'slim', tail: 'bar', color: 0xb5121b, grille: 'full', hood: 'vents', spoiler: 'duck' }, 190000, 16, 0, 'Version élargie et suralimentée : le drag strip sur route ouverte.'),
  // ---------------- Drift ----------------
  C('kitsune', 'Kaido', 'Kitsune S', 'drift', 'coupe', { L: 4.35, H: 1.29, nose: 0.36, hood: 0.74, cab: [0.34, 0.48, 0.62, 0.84] },
    { top: 232, a100: 5.4, mass: 1220, grip: 1.0, brake: 1.08, drive: 'RWD', wf: 0.53, susp: 0.82, drift: 0.95, off: 0.1, gears: 6, eng: 'i4t' },
    { head: 'popup', tail: 'round4', color: 0xff5fa2, grille: 'slot', spoiler: 'duck' }, 46000, 5, 0, 'La reine des parkings. Angle maximal, fumée assurée.'),
  C('ronin', 'Raijin', 'Ronin RX', 'drift', 'fastback', { L: 4.3, H: 1.23, wb: 2.45, nose: 0.25, hood: 0.66, box: 0.5, cab: [0.34, 0.49, 0.58, 0.9] },
    { top: 256, a100: 4.8, mass: 1280, grip: 1.04, brake: 1.1, drive: 'RWD', wf: 0.51, susp: 0.78, drift: 1.0, off: 0.08, gears: 6, eng: 'rotary' },
    { head: 'popup', tail: 'round2', color: 0xf3f3f3, grille: 'smile', spoiler: 'wing' }, 98000, 10, 0, 'Rotatif rageur et équilibre parfait pour enchaîner les travers.'),
  C('sidewind', 'Brennar', 'Sidewinder', 'drift', 'sedan', { L: 4.7, W: 1.88, H: 1.38, flare: 0.07, gc: 0.12, nose: 0.55 },
    { top: 268, a100: 4.5, mass: 1540, grip: 1.03, brake: 1.1, drive: 'RWD', wf: 0.54, susp: 0.85, drift: 0.9, off: 0.1, gears: 6, eng: 'v8' },
    { head: 'angular', tail: 'slim', color: 0x26d07c, grille: 'wide', spoiler: 'gt' }, 135000, 13, 0, 'Berline V8 de drift pro : quatre portes, aucune retenue.'),
  // ---------------- Rallye ----------------
  C('gravel', 'Kaido', 'Gravel WRX', 'rally', 'sedan', { L: 4.45, H: 1.45, gc: 0.2, flare: 0.07, wr: 0.34, cab: [0.28, 0.42, 0.68, 0.84] },
    { top: 224, a100: 5.0, mass: 1320, grip: 1.08, brake: 1.1, drive: 'AWD', wf: 0.57, susp: 1.15, drift: 0.55, off: 0.75, gears: 6, eng: 'b6' },
    { head: 'angular', tail: 'rect', color: 0x1451d9, grille: 'slot', spoiler: 'wing', hood: 'scoop', lightbar: true, mudflaps: true }, 72000, 7, 0, 'Berline de rallye : terre, neige, asphalte, peu importe.'),
  C('stratos', 'Corsaro', 'Stradale Terra', 'rally', 'wedge', { L: 3.85, W: 1.86, H: 1.16, wb: 2.2, gc: 0.19, wr: 0.34, nose: 0.3, hood: 0.6, cab: [0.24, 0.42, 0.56, 0.72], box: 0.44, flare: 0.1 },
    { top: 236, a100: 4.6, mass: 1010, grip: 1.1, brake: 1.12, drive: 'RWD', wf: 0.42, susp: 1.05, drift: 0.8, off: 0.65, gears: 5, eng: 'v6' },
    { head: 'popup', tail: 'round2', color: 0xffffff, grille: 'none', lightbar: true, spoiler: 'duck' }, 128000, 12, 0, 'Icône compacte à moteur central, née pour les spéciales.'),
  C('breva', 'Volta', 'Breva Cross', 'rally', 'wagon', { gc: 0.22, flare: 0.07, wr: 0.35 },
    { top: 218, a100: 5.6, mass: 1410, grip: 1.05, brake: 1.06, drive: 'AWD', wf: 0.56, susp: 1.2, drift: 0.5, off: 0.8, gears: 6, eng: 'i4t' },
    { head: 'slim', tail: 'rect', color: 0xe26a1b, grille: 'wide', roofrack: true, mudflaps: true }, 58000, 6, 0, 'Break surélevé, transmission intégrale, coffre plein d\'aventures.'),
  // ---------------- Tout-terrain ----------------
  C('nomad', 'Tundra', 'Nomad', 'offroad', 'suv', {},
    { top: 182, a100: 8.4, mass: 2050, grip: 0.92, brake: 0.92, drive: 'AWD', wf: 0.54, susp: 1.5, drift: 0.3, off: 1.0, gears: 6, eng: 'v6' },
    { head: 'round', tail: 'rect', color: 0x6b7a3a, grille: 'bars', bumper: 'offroad', spare: true }, 48000, 5, 0, '4x4 rustique à l\'ancienne : là où la route s\'arrête, il continue.'),
  C('titan', 'Tundra', 'Titan XL', 'offroad', 'pickup', {},
    { top: 196, a100: 6.8, mass: 2480, grip: 0.9, brake: 0.9, drive: 'AWD', wf: 0.57, susp: 1.45, drift: 0.35, off: 0.95, gears: 6, eng: 'v8m' },
    { head: 'rect', tail: 'rect', color: 0x9b1c1c, grille: 'full', bumper: 'offroad', lightbar: true }, 82000, 9, 0, 'Pick-up XXL. Les nids-de-poule en ont peur.'),
  C('scarab', 'Vireo', 'Scarab', 'offroad', 'buggy', {},
    { top: 174, a100: 5.9, mass: 720, grip: 1.0, brake: 1.0, drive: 'RWD', wf: 0.4, susp: 1.6, drift: 0.7, off: 1.0, gears: 5, eng: 'i4' },
    { head: 'round2', tail: 'round2', color: 0xffb703, grille: 'none', lightbar: true }, 36000, 4, 0, 'Buggy des dunes : léger comme une plume, joueur comme un chiot.'),
  C('raptor', 'Monarch', 'Sahara R', 'offroad', 'suv', { L: 4.85, W: 2.04, H: 1.78, gc: 0.33, wr: 0.45, flare: 0.11, nose: 0.6, hood: 0.88, box: 0.36, cab: [0.28, 0.4, 0.86, 0.96] },
    { top: 236, a100: 4.9, mass: 2250, grip: 1.0, brake: 1.02, drive: 'AWD', wf: 0.53, susp: 1.4, drift: 0.4, off: 0.95, gears: 7, eng: 'v8' },
    { head: 'slim', tail: 'bar', color: 0xd9c7a3, grille: 'full', bumper: 'offroad', roofrack: true }, 165000, 15, 0, 'SUV de raid : 600 ch pour survoler les pistes.'),
  // ---------------- Luxe ----------------
  C('sovereign', 'Monarch', 'Sovereign', 'luxury', 'limo', {},
    { top: 252, a100: 5.3, mass: 2380, grip: 0.98, brake: 1.02, drive: 'RWD', wf: 0.52, susp: 1.3, drift: 0.3, off: 0.1, gears: 7, eng: 'v12' },
    { head: 'rect', tail: 'rect', color: 0x14213d, grille: 'chrome' }, 240000, 14, 0, 'Limousine de prestige. Le silence est son luxe.'),
  C('celeste', 'Solana', 'Céleste', 'luxury', 'fastback', { L: 4.95, W: 1.96, H: 1.36, wb: 2.95, hood: 0.84, nose: 0.5, belt: 0.82, cab: [0.34, 0.47, 0.62, 0.9] },
    { top: 302, a100: 3.9, mass: 2010, grip: 1.1, brake: 1.15, drive: 'AWD', wf: 0.52, susp: 1.05, drift: 0.35, off: 0.08, gears: 7, eng: 'v8' },
    { head: 'slim', tail: 'bar', color: 0xc9b17a, grille: 'chrome' }, 320000, 18, 0, 'Grand coupé de luxe : traverser le continent en smoking.'),
  C('regent', 'Monarch', 'Regent Drophead', 'luxury', 'roadster', { L: 5.1, W: 1.96, H: 1.36, wb: 3.05, belt: 0.92, hood: 0.96, nose: 0.85, noseW: 0.92, cab: [0.4, 0.49, 0.49, 0.7], box: 0.34, wr: 0.37 },
    { top: 262, a100: 4.9, mass: 2290, grip: 1.0, brake: 1.04, drive: 'RWD', wf: 0.52, susp: 1.3, drift: 0.3, off: 0.08, gears: 7, eng: 'v12' },
    { head: 'round2', tail: 'slim', color: 0xf4efe6, grille: 'chrome' }, 410000, 21, 20, 'Cabriolet d\'apparat pour remonter la promenade au coucher du soleil.'),
];
export const CAR_BY_ID = Object.fromEntries(CARS.map((c) => [c.id, c]));

// Véhicules de circulation et de police (non collectionnables).
export const TRAFFIC_CARS = ['pico', 'lumen', 'tamago', 'kestrel', 'nomad', 'breva', 'sovereign', 'brawler', 'gravel'];
export const POLICE_SPEC = { ...CAR_BY_ID.sidewind, id: 'police', name: 'Patrouille VCPD', stats: { ...CAR_BY_ID.sidewind.stats, top: 285, a100: 4.2, grip: 1.12, mass: 1700, drift: 0.3 }, style: { ...CAR_BY_ID.sidewind.style, color: 0xf4f6fa, spoiler: 'none', police: true } };
export const POLICE_FAST = { ...CAR_BY_ID.seraph, id: 'police2', name: 'Intercepteur VCPD', stats: { ...CAR_BY_ID.seraph.stats, top: 345, a100: 3.0 }, style: { ...CAR_BY_ID.seraph.style, color: 0xf4f6fa, police: true } };

// Indice de performance (PI) d'une voiture avec ses améliorations.
export function perfIndex(stats) {
  const s = (stats.top - 150) / 270 * 38 + (12 - stats.a100) / 9.7 * 34 + (stats.grip - 0.85) / 0.65 * 20 + (stats.brake - 0.85) / 0.65 * 8;
  return Math.round(100 + s * 8.99);
}
export const classOf = (pi) => (pi >= 860 ? 'S+' : pi >= 720 ? 'S' : pi >= 580 ? 'A' : pi >= 450 ? 'B' : pi >= 320 ? 'C' : 'D');
