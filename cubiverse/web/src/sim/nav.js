// Navigation entre cubes : recherche en largeur sur le graphe des connexions,
// puis génération de points de passage (portes latérales, échelle/trappe).
import { HALF, DOOR_POINT, LADDER, MAX_PER_CUBE } from './constants.js';

// Renvoie la liste des cubes (ids) de start à goal, ou null.
export function cubeRoute(world, startId, goalId, maxNodes = 600) {
  if (startId === goalId) return [startId];
  const prev = new Map([[startId, null]]);
  const q = [startId];
  while (q.length && prev.size < maxNodes) {
    const id = q.shift();
    const c = world.cubes.get(id);
    for (const [, n] of world.neighbors(c)) {
      if (prev.has(n.id)) continue;
      prev.set(n.id, id);
      if (n.id === goalId) {
        const route = [goalId]; let p = id;
        while (p) { route.push(p); p = prev.get(p); }
        return route.reverse();
      }
      q.push(n.id);
    }
  }
  return null;
}

// Distance (en cubes) depuis un cube vers tous les cubes atteignables (limitée).
export function cubeDistances(world, startId, maxDepth = 6) {
  const dist = new Map([[startId, 0]]); const q = [startId];
  while (q.length) {
    const id = q.shift(); const d = dist.get(id);
    if (d >= maxDepth) continue;
    for (const [, n] of world.neighbors(world.cubes.get(id))) if (!dist.has(n.id)) { dist.set(n.id, d + 1); q.push(n.id); }
  }
  return dist;
}

function sideBetween(a, b) {
  if (b.x === a.x + 1) return 'px'; if (b.x === a.x - 1) return 'nx';
  if (b.z === a.z + 1) return 'pz'; if (b.z === a.z - 1) return 'nz';
  if (b.y === a.y + 1) return 'py'; return 'ny';
}

// Construit les points de passage. Chaque point : { cube, x, z, kind: 'walk'|'door'|'climb', dir }
export function buildPath(world, ch, goalCubeId, gx, gz) {
  const route = cubeRoute(world, ch.cubeId, goalCubeId);
  if (!route) return null;
  const path = [];
  for (let i = 0; i < route.length - 1; i++) {
    const a = world.cubes.get(route[i]); const b = world.cubes.get(route[i + 1]);
    if (i > 0 && b.chars.size >= MAX_PER_CUBE) return null;
    const s = sideBetween(a, b);
    if (s === 'py' || s === 'ny') {
      path.push({ cube: a.id, x: LADDER.x + 0.22, z: LADDER.z + 0.22, kind: 'walk' });
      path.push({ cube: b.id, x: LADDER.x + 0.22, z: LADDER.z + 0.22, kind: 'climb', dir: s === 'py' ? 1 : -1 });
    } else {
      const [dx, dz] = DOOR_POINT[s];
      // point d'approche à l'intérieur de A, puis point juste à l'intérieur de B
      path.push({ cube: a.id, x: dx * 0.78, z: dz * 0.78, kind: 'walk' });
      path.push({ cube: b.id, x: -dx * 0.8, z: -dz * 0.8, kind: 'door', side: s });
    }
  }
  path.push({ cube: goalCubeId, x: gx, z: gz, kind: 'walk' });
  return path;
}

// Coordonnées d'un point de passage exprimées dans le repère local du cube courant du personnage.
export function waypointLocal(world, ch, wp, out) {
  if (wp.cube === ch.cubeId) { out[0] = wp.x; out[1] = wp.z; return out; }
  const a = world.cubes.get(ch.cubeId); const b = world.cubes.get(wp.cube);
  if (!a || !b) return null;
  out[0] = wp.x + (b.x - a.x) * 4; out[1] = wp.z + (b.z - a.z) * 4;
  return out;
}

export { HALF };
