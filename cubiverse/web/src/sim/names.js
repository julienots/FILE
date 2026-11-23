import { pick, rand } from './rng.js';

const A = ['Bo', 'Pi', 'Lu', 'Mo', 'Ti', 'Za', 'Ki', 'No', 'Fé', 'Ru', 'Ja', 'Lé', 'Mi', 'Su', 'Ga', 'Do', 'Vi', 'Ra', 'Ce', 'Yo', 'Po', 'Ma', 'Né', 'Bi', 'Lo', 'Té', 'Wa', 'Xi', 'Ou', 'Sa'];
const B = ['lo', 'xi', 'mi', 'ra', 'to', 'nou', 'bi', 'line', 'ko', 'zé', 'pi', 'lou', 'na', 'do', 'chi', 'ka', 'mo', 'li', 'ro', 'ne', 'zo', 'ly', 'ti', 'pa', 'gu', 'é'];
const C = ['', '', '', 'n', 's', 'x', 'm', 'r', 'tte', 'o'];

export function randomName(taken) {
  for (let i = 0; i < 40; i++) {
    let n = pick(A) + pick(B) + (rand() < 0.4 ? pick(C) : '');
    if (rand() < 0.15) n = n + '-' + pick(A).toLowerCase() + pick(B);
    if (!taken || !taken.has(n)) return n;
  }
  return pick(A) + pick(B) + Math.floor(rand() * 99);
}

export const PALETTE = [
  '#ff5d5d', '#ff9f43', '#ffd23f', '#7bd389', '#3ec1d3', '#5b8def', '#9b72ff', '#ff7eb6',
  '#f4f1ea', '#2d3047', '#00b894', '#e17055', '#6c5ce7', '#fdcb6e', '#74b9ff', '#a29bfe',
];

const GROUP_ADJ = ['Joyeux', 'Rapides', 'Curieux', 'Rêveurs', 'Inséparables', 'Fous rires', 'Bricoleurs', 'Étoiles', 'Pirates', 'Champions'];
const GROUP_N = ['Cubes', 'Copains', 'Lutins', 'Comètes', 'Pingouins', 'Biscuits', 'Fusées', 'Radis', 'Hiboux', 'Galets'];
export function groupName() { return 'Les ' + pick(GROUP_N) + ' ' + pick(GROUP_ADJ); }
