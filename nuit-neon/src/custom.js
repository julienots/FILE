// Atelier de circuits : un circuit est décrit par des nombres (forme, largeur, obstacles, règles physiques…)
// et le tracé est tiré d'une graine, de sorte que les mêmes nombres redonnent toujours le même circuit.

// [clé, libellé, mini, maxi, pas, valeur de départ, unité]
export const CHAMPS = [
  ['Tracé'],
  ['graine', 'Graine du tracé', 0, 99999, 1, 2024, ''],
  ['virages', 'Nombre de virages', 3, 40, 1, 10, ''],
  ['taille', 'Taille du circuit', 100, 2500, 10, 320, 'm'],
  ['chaos', 'Irrégularité', 0, 100, 1, 45, '%'],
  ['largeur', 'Largeur de la route', 8, 160, 1, 32, 'm'],
  ['Sur la piste'],
  ['obstacles', 'Obstacles', 0, 600, 1, 30, ''],
  ['tremplins', 'Tremplins', 0, 120, 1, 4, ''],
  ['turbos', 'Zones turbo', 0, 120, 1, 4, ''],
  ['trafic', 'Voitures de circulation', 0, 80, 1, 0, ''],
  ['Règles'],
  ['vitesse', 'Vitesse de tous les véhicules', 0.2, 30, 0.1, 1, '×'],
  ['adherence', 'Adhérence', 0.1, 5, 0.1, 1, '×'],
  ['gravite', 'Gravité', 0.05, 5, 0.05, 1, '×'],
  ['saut', 'Hauteur des sauts', 0.2, 10, 0.1, 1, '×'],
  ['boost', 'Puissance du boost', 0, 10, 0.1, 1, '×'],
  ['brouillard', 'Brouillard', 0, 6, 0.1, 1, '×'],
];
export const REGLES_BASE = { vitesse: 1, adherence: 1, gravite: 1, saut: 1, boost: 1 };
export function defautCustom(i) {
  const c = { nom: 'Perso ' + (i + 1), theme: i % 9 };
  for (const f of CHAMPS) if (f.length > 1) c[f[0]] = f[5];
  c.graine = 2024 + i * 777; if (i === 1) { c.virages = 16; c.taille = 420; c.largeur = 24; c.obstacles = 80; } if (i === 2) { c.virages = 6; c.taille = 700; c.largeur = 60; c.vitesse = 3; c.tremplins = 20; c.turbos = 20; }
  return c;
}
export function borner(c) {
  for (const f of CHAMPS) if (f.length > 1) { const v = +c[f[0]]; c[f[0]] = Number.isFinite(v) ? Math.max(f[2], Math.min(f[3], v)) : f[5]; }
  return c;
}
// tracé en étoile autour d'un centre : les points gardent leur ordre angulaire, la route ne se recoupe donc jamais
export function tracer(c) {
  let s = (c.graine | 0) * 2654435761 % 4294967296 || 1;
  const rnd = () => { s ^= s << 13; s >>>= 0; s ^= s >> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; };
  const n = Math.round(c.virages), k = c.chaos / 100, rmin = Math.max(c.largeur * 1.4, c.taille * 0.3), pts = [];
  for (let i = 0; i < n; i++) {
    const a = (i + (rnd() - 0.5) * 0.7 * k) / n * Math.PI * 2, r = Math.max(rmin, c.taille * (1 + (rnd() * 2 - 1) * 0.6 * k));
    pts.push([Math.round(Math.sin(a) * r), Math.round(-Math.cos(a) * r)]);
  }
  return pts;
}
export function circuitDe(c, i, themes) {
  borner(c);
  const th = Object.assign({}, themes[c.theme % themes.length]); th.dens = th.dens * c.brouillard;
  return { id: 'perso' + (i + 1), nom: c.nom, desc: 'Circuit de l\'atelier', pts: tracer(c), echelle: 1, half: c.largeur / 2, theme: th, perso: i,
    regles: { vitesse: c.vitesse, adherence: c.adherence, gravite: c.gravite, saut: c.saut, boost: c.boost }, contenu: { obstacles: c.obstacles, tremplins: c.tremplins, turbos: c.turbos, trafic: c.trafic } };
}
