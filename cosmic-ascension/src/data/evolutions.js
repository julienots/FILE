// Stades d'évolution du héros. OPUS: complète (objectif 10 stades, dont 3 formes "divines"). FABLE: visuels via Hero.js.
// scale = taille visuelle relative; xp = XP cumulée requise; tripo = clé de modèle optionnelle (models manifest).
export const EVOLUTIONS = [
  { id: 'mote',   name: 'Éclat du Vide',   xp: 0,    scale: 1.0, color: '#9fe8ff', glow: '#7fd0ff', abilities: ['hop'],          desc: 'Une minuscule créature échappée du Laboratoire.' },
  { id: 'sprout', name: 'Rôdeur Stellaire',  xp: 100,  scale: 1.5, color: '#8affb0', glow: '#4dff9a', abilities: ['dash'],         desc: 'Des membres apparaissent, l’énergie circule.' },
  { id: 'comet',  name: 'Porteur de Comète',    xp: 350,  scale: 2.0, color: '#ffd76a', glow: '#ffb02e', abilities: ['starbolt'],     desc: 'Une traînée de feu stellaire le suit.' },
  { id: 'nova',   name: 'Spectre Nova',     xp: 900,  scale: 2.8, color: '#ff8ad8', glow: '#ff3cc0', abilities: ['shockwave'],    desc: 'Ailes de lumière et halo de nova.' },
  { id: 'titan',  name: 'Titan d’Obsidienne',  xp: 2000, scale: 4.0, color: '#b48aff', glow: '#7a3cff', abilities: ['gravity'],      desc: 'Sa silhouette éclipse les lunes.' },
  { id: 'god',    name: 'Dieu du Néant',xp: 5000, scale: 7.0, color: '#ffffff', glow: '#ffe9a0', abilities: ['supernova'],    desc: 'Forme finale: un astre vivant.' },
];
