import * as THREE from 'three';
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js';
import { toonMaterial } from '../render/shaders/Toon.js';
import { Config } from '../core/Config.js';

// Décors instanciés par biome (1 draw call par type). Géométries low-poly à couleurs de sommets, générées une fois.
// Data: planet.props = [{kind, density (instances / 1000 m²), scale?, colors?}] — sinon défauts du biome ci-dessous.
const paint = (g, color) => { g = g.index ? g.toNonIndexed() : g; g.deleteAttribute('uv'); const c = new THREE.Color(color), n = g.attributes.position.count, a = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) { a[i * 3] = c.r; a[i * 3 + 1] = c.g; a[i * 3 + 2] = c.b; } g.setAttribute('color', new THREE.BufferAttribute(a, 3)); return g; };
const at = (g, x, y, z, sx = 1, sy = sx, sz = sx) => g.scale(sx, sy, sz).translate(x, y, z);
const ico = (r, d = 1) => new THREE.IcosahedronGeometry(r, d);
const BUILD = {
  tree: (c = ['#2a1a2a', '#1f6a5a', '#2f8a7a']) => mergeGeometries([
    paint(at(new THREE.CylinderGeometry(.22, .34, 2.2, 6), 0, 1.1, 0), c[0]),
    paint(at(ico(1.25), 0, 3.0, 0, 1, .9, 1), c[1]), paint(at(ico(.85), .75, 2.5, .2), c[1]), paint(at(ico(.8), -.6, 2.7, -.4), c[2]), paint(at(ico(.7), 0, 3.9, .1), c[2])]),
  pine: (c = ['#2a2030', '#1a4a5a', '#9ad0f0']) => mergeGeometries([
    paint(at(new THREE.CylinderGeometry(.16, .24, 1.2, 6), 0, .6, 0), c[0]),
    paint(at(new THREE.ConeGeometry(1.1, 1.8, 7), 0, 1.9, 0), c[1]), paint(at(new THREE.ConeGeometry(.85, 1.5, 7), 0, 2.9, 0), c[1]), paint(at(new THREE.ConeGeometry(.55, 1.2, 7), 0, 3.8, 0), c[2])]),
  rock: (c = ['#3a3a54', '#54546e']) => mergeGeometries([paint(at(ico(.8, 0), 0, .35, 0, 1.2, .8, 1), c[0]), paint(at(ico(.45, 0), .7, .2, .3), c[1])]),
  crystal: (c = ['#6ff2ff', '#b48aff']) => mergeGeometries([
    paint(at(new THREE.OctahedronGeometry(.4), 0, 1.0, 0, 1, 2.6, 1), c[0]),
    paint(at(new THREE.OctahedronGeometry(.3), .45, .6, .1, 1, 2.1, 1).rotateZ(-.35), c[1]), paint(at(new THREE.OctahedronGeometry(.26), -.4, .5, -.15, 1, 2, 1).rotateZ(.4), c[0])]),
  mushroom: (c = ['#4a4060', '#5a1a8a', '#ff7ad8']) => mergeGeometries([
    paint(at(new THREE.CylinderGeometry(.3, .42, 2.4, 7), 0, 1.2, 0), c[0]),
    paint(at(new THREE.SphereGeometry(1.5, 9, 5, 0, Math.PI * 2, 0, Math.PI / 2), 0, 2.2, 0, 1, .7, 1), c[1]), paint(at(ico(.28, 0), .7, 2.85, .3), c[2]), paint(at(ico(.22, 0), -.5, 3.0, -.4), c[2])]),
  grass: (c = ['#1f6a5a', '#3a9a8a']) => mergeGeometries([0, 1, 2, 3, 4].map(i => { const a = i * 1.26;
    return paint(at(new THREE.ConeGeometry(.07, .5 + (i % 3) * .14, 3), Math.cos(a) * .16, .28, Math.sin(a) * .16).rotateZ(Math.cos(a) * .18), c[i % 2]); })),
  flower: (c = ['#1f5a4a', '#6ff2ff', '#ffffff']) => mergeGeometries([
    paint(at(new THREE.CylinderGeometry(.03, .04, .7, 4), 0, .35, 0), c[0]), paint(at(ico(.2, 0), 0, .78, 0, 1, .55, 1), c[1]), paint(at(ico(.09, 0), 0, .88, 0), c[2])]),
  spire: (c = ['#bfeaff', '#ffffff']) => mergeGeometries([
    paint(at(new THREE.ConeGeometry(.7, 4.2, 5), 0, 2.1, 0), c[0]), paint(at(new THREE.ConeGeometry(.4, 2.2, 5), .7, 1.1, .2).rotateZ(-.2), c[1])]),
};
// kind → {sway, glow, collide (rayon à l'échelle 1), min/max échelle, slopeOK, shore (accepte près de l'eau)}
const KIND = {
  tree: { sway: .035, collide: .45, s: [.8, 1.5] }, pine: { sway: .02, collide: .35, s: [.8, 1.6] }, rock: { collide: .8, s: [.5, 2.2] },
  crystal: { glow: true, collide: .5, s: [.6, 1.8] }, mushroom: { sway: .015, collide: .45, s: [.6, 1.8] },
  grass: { sway: .22, s: [.7, 1.5] }, flower: { sway: .18, s: [.8, 1.6] }, spire: { collide: .7, s: [.6, 2] },
};
export const BIOME_PROPS = {
  meadow:   [{ kind: 'tree', density: .55 }, { kind: 'grass', density: 14 }, { kind: 'flower', density: 3 }, { kind: 'rock', density: .35 }, { kind: 'crystal', density: .06 }],
  volcanic: [{ kind: 'rock', density: 1.1, colors: ['#3a2630', '#5a3540'] }, { kind: 'spire', density: .35, colors: ['#4a2a34', '#ff8a3a'] }, { kind: 'crystal', density: .3, colors: ['#ffb02e', '#ff5a2a'] }],
  ice:      [{ kind: 'spire', density: .5 }, { kind: 'pine', density: .45 }, { kind: 'rock', density: .3, colors: ['#bcd6ea', '#e4f4ff'] }, { kind: 'crystal', density: .2, colors: ['#aef4ff', '#ffffff'] }],
  jungle:   [{ kind: 'mushroom', density: .7 }, { kind: 'tree', density: .7, colors: ['#2a1a3a', '#145a5a', '#6a2aaa'] }, { kind: 'grass', density: 10, colors: ['#146a5a', '#7a3ad0'] }, { kind: 'flower', density: 3, colors: ['#145a4a', '#ff7ad8', '#ffffff'] }, { kind: 'crystal', density: .15, colors: ['#ff7ad8', '#b48aff'] }],
  desert:   [{ kind: 'rock', density: .9, colors: ['#c98a5a', '#e0a878'] }, { kind: 'spire', density: .25, colors: ['#d89a6a', '#f0c090'] }, { kind: 'crystal', density: .1, colors: ['#6ff2ff', '#ffe46a'] }],
};
const geoCache = new Map();
function geoFor(kind, colors) { const k = kind + (colors ? colors.join() : ''); if (!geoCache.has(k)) geoCache.set(k, (BUILD[kind] ?? BUILD.rock)(colors)); return geoCache.get(k); }
function rng(seed) { let s = seed >>> 0 || 1; return () => ((s = Math.imul(s ^ (s >>> 15), 2246822507) ^ Math.imul(s ^ (s >>> 13), 3266489909), (s ^= s >>> 16) >>> 0) / 4294967296); }

// Parsème les props sur une planète (repère local de la planète). Renvoie { group, colliders:[{pos,r}] }.
export function scatterProps(planet) {
  const { def, radius: R, sea, sample, heightAt } = planet.userData, list = def.props?.length ? def.props : (BIOME_PROPS[def.biome] ?? BIOME_PROPS.meadow);
  const group = new THREE.Group(); group.name = 'props'; const colliders = [];
  const area = 4 * Math.PI * R * R / 1000, qk = Config.qualityPresets[Config.quality.level].particles, rnd = rng(def.seed * 977 + 13);
  const m = new THREE.Matrix4(), q = new THREE.Quaternion(), qy = new THREE.Quaternion(), up = new THREE.Vector3(0, 1, 0), d = new THREE.Vector3(), p = new THREE.Vector3(), sc = new THREE.Vector3();
  for (const spec of list) {
    const K = KIND[spec.kind] ?? KIND.rock, small = !K.collide, count = Math.min(small ? 3000 : 700, Math.round(spec.density * area * (small ? qk : 1)));
    if (count <= 0) continue;
    const mat = K.glow ? new THREE.MeshBasicMaterial({ vertexColors: true }) : toonMaterial({ vertexColors: true, sway: K.sway ?? 0, rimStrength: .25, flatShading: true });
    const mesh = new THREE.InstancedMesh(geoFor(spec.kind, spec.colors), mat, count); let n = 0;
    for (let i = 0; i < count * 3 && n < count; i++) {
      const u = rnd() * 2 - 1, t = rnd() * 6.2832, r = Math.sqrt(1 - u * u); d.set(r * Math.cos(t), u, r * Math.sin(t));
      const h = sample(d) - sea; if (h < 0.012 || (spec.kind !== 'rock' && spec.kind !== 'spire' && spec.kind !== 'crystal' && h > 0.36)) continue;   // pas sous l'eau, végétation pas sur les sommets
      const s = (K.s[0] + rnd() * (K.s[1] - K.s[0])) * (spec.scale ?? 1);
      p.copy(d).multiplyScalar(heightAt(d) - .05 * s); q.setFromUnitVectors(up, d); qy.setFromAxisAngle(up, rnd() * 6.2832); q.multiply(qy);
      m.compose(p, q, sc.set(s, s * (.85 + rnd() * .3), s)); mesh.setMatrixAt(n++, m);
      if (K.collide) colliders.push({ pos: p.clone(), r: K.collide * s });
    }
    mesh.count = n; mesh.instanceMatrix.needsUpdate = true; mesh.frustumCulled = false; mesh.name = 'prop:' + spec.kind; group.add(mesh);
  }
  return { group, colliders };
}
