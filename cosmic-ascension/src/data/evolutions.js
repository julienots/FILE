// Stades d'évolution du héros (B). Visuels: A via Hero.js (formes 0-5 procédurales; 6-9 à ajouter par A, GLB `hero_<id>` optionnel).
// xp = XP cumulée requise · scale = taille visuelle relative · hp/energy = max au stade · abilities = débloquées à ce stade
// divine:true = formes divines (actes 4-5). requires: flag de quête nécessaire en plus de l'XP (verrou narratif).
export const EVOLUTIONS = [
  { id: 'mote',     name: 'Éclat du Vide',      xp: 0,     scale: 1.0,  hp: 100,  energy: 100, color: '#9fe8ff', glow: '#7fd0ff', abilities: ['hop'],        desc: 'Une étincelle sombre échappée du Laboratoire.' },
  { id: 'sprout',   name: 'Rôdeur Stellaire',     xp: 100,   scale: 1.5,  hp: 140,  energy: 120, color: '#8affb0', glow: '#4dff9a', abilities: ['dash'],       desc: 'Des membres apparaissent, l’énergie circule.' },
  { id: 'comet',    name: 'Porteur de Comète',       xp: 350,   scale: 2.0,  hp: 200,  energy: 150, color: '#ffd76a', glow: '#ffb02e', abilities: ['starbolt'],   desc: 'Une traînée de feu stellaire le suit.' },
  { id: 'nova',     name: 'Spectre Nova',        xp: 900,   scale: 2.8,  hp: 290,  energy: 190, color: '#ff8ad8', glow: '#ff3cc0', abilities: ['shockwave'],  desc: 'Ailes de lumière et halo de nova.' },
  { id: 'titan',    name: 'Titan d’Obsidienne',     xp: 2000,  scale: 4.0,  hp: 420,  energy: 240, color: '#b48aff', glow: '#7a3cff', abilities: ['gravity'],    desc: 'Sa silhouette éclipse les lunes.' },
  { id: 'god',      name: 'Dieu du Néant',   xp: 4200,  scale: 5.5,  hp: 600,  energy: 300, color: '#ffffff', glow: '#ffe9a0', abilities: ['supernova'],  desc: 'Un astre noir vivant, encore incomplet.', divine: true },
  { id: 'chimera',  name: 'Chimère Abyssale',xp: 7500,  scale: 7.0,  hp: 820,  energy: 380, color: '#7af0ff', glow: '#c06bff', abilities: ['nebulaVeil'], desc: 'Fumée d’abysse et mille yeux luisants.', divine: true, requires: 'act4_started' },
  { id: 'voidlord', name: 'Seigneur du Vide',    xp: 12000, scale: 9.0,  hp: 1100, energy: 470, color: '#2a1840', glow: '#9a4dff', abilities: ['blackHole'],  desc: 'Une armure de nuit et une cape de galaxie fêlée.', divine: true, requires: 'titan_defeated' },
  { id: 'architect',name: 'Architecte Noir',  xp: 18000, scale: 12.0, hp: 1450, energy: 580, color: '#ffe7a8', glow: '#ffd04a', abilities: ['starforge'],  desc: 'Des membres d’obsidienne qui tordent les orbites.', divine: true, requires: 'act5_started' },
  { id: 'cosmos',   name: 'Cosmos Incarné',      xp: 26000, scale: 16.0, hp: 2000, energy: 720, color: '#ffffff', glow: '#a8c8ff', abilities: ['bigBang'],    desc: 'Une galaxie entière a pris forme humaine.', divine: true, requires: 'final_choice' },
];
export const stageIndex = (id) => EVOLUTIONS.findIndex(e => e.id === id);
