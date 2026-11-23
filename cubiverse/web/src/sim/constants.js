// Constantes partagées par la simulation et le rendu.
export const CUBE = 4;                 // taille d'un cube (unités monde)
export const HALF = CUBE / 2;
export const WALL_MARGIN = 0.16;       // distance minimale aux parois
export const DOOR_HALF = 0.48;         // demi-largeur des passages latéraux
export const MAX_PER_CUBE = 32;        // personnages maximum par cube
export const LADDER = { x: -1.55, z: -1.55 }; // coin de l'échelle / trappe
export const CHAR_RADIUS = 0.12;
export const DAY_LENGTH = 360;         // secondes réelles pour une journée (x1)

// Côtés d'un cube : décalages de grille.
export const SIDES = {
  px: [1, 0, 0], nx: [-1, 0, 0],
  pz: [0, 0, 1], nz: [0, 0, -1],
  py: [0, 1, 0], ny: [0, -1, 0],
};
export const OPPOSITE = { px: 'nx', nx: 'px', pz: 'nz', nz: 'pz', py: 'ny', ny: 'py' };
export const SIDE_LABELS = {
  px: 'Droite', nx: 'Gauche', pz: 'Devant', nz: 'Derrière', py: 'Dessus', ny: 'Dessous',
};
// Point de passage local (x,z) au bord de chaque côté horizontal.
export const DOOR_POINT = {
  px: [HALF, 0], nx: [-HALF, 0], pz: [0, HALF], nz: [0, -HALF],
};

export const NEEDS = ['energy', 'hunger', 'fun', 'social', 'comfort', 'curiosity'];
export const NEED_LABELS = {
  energy: 'Énergie', hunger: 'Satiété', fun: 'Amusement', social: 'Social',
  comfort: 'Confort', curiosity: 'Découverte',
};
export const TRAITS = ['sociability', 'energy', 'curiosity', 'kindness', 'playfulness', 'temper', 'diligence'];
export const TRAIT_LABELS = {
  sociability: 'Sociable', energy: 'Énergique', curiosity: 'Curieux', kindness: 'Gentil',
  playfulness: 'Joueur', temper: 'Colérique', diligence: 'Travailleur',
};
export const EMOTIONS = ['joy', 'sadness', 'anger', 'fear', 'surprise'];
export const EMOTION_LABELS = {
  joy: 'Joie', sadness: 'Tristesse', anger: 'Colère', fear: 'Peur', surprise: 'Surprise',
};
export const SKILLS = ['cuisine', 'sport', 'art', 'science', 'jardinage', 'pêche', 'construction', 'musique', 'social', 'lecture'];

export const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
export const lerp = (a, b, t) => a + (b - a) * t;
export const key3 = (x, y, z) => x + ',' + y + ',' + z;
