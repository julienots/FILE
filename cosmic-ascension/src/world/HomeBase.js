import * as THREE from 'three';
import { toonMaterial } from '../render/shaders/Toon.js';
import { createAtmosphere } from '../render/shaders/Atmosphere.js';
import { Assets } from '../assets/AssetLibrary.js';
import { State } from '../core/State.js';
import { bus } from '../core/EventBus.js';

// Planète du joueur (rendu): emplacements de construction autour du point d'arrivée, bâtiments 3D qui poussent.
// Contrat: A émet `build:near {slot|null,cost,buildingId}` et `build:request {planetId,slot}` (⏎ sur un emplacement libre);
// B (ou core/Fallback) valide le coût puis émet `build:placed {planetId,slot,buildingId}` → le bâtiment apparaît. État: State.data.homePlanet.buildings [{slot,id}].
export const BUILD_ORDER = ['house', 'greenhouse', 'lab', 'forge', 'hangar', 'observatory', 'temple', 'tower'];
export const buildCost = (n) => 8 + n * 8;
const M = (c, o = {}) => toonMaterial({ color: c, rim: '#ffffff', rimStrength: .35, ...o }), G = (c) => new THREE.MeshBasicMaterial({ color: c });
const add = (g, geo, mat, x, y, z, rx = 0, rz = 0) => { const m = new THREE.Mesh(geo, mat); m.position.set(x, y, z); m.rotation.set(rx, 0, rz); g.add(m); return m; };
const BUILDERS = {
  house: (g) => { add(g, new THREE.CylinderGeometry(1.6, 1.8, 2, 10), M('#fff1d6'), 0, 1, 0); add(g, new THREE.ConeGeometry(2.3, 1.8, 10), M('#ff7a5a'), 0, 2.9, 0); add(g, new THREE.BoxGeometry(.7, 1.2, .2), M('#6a4630'), 0, .6, 1.72); add(g, new THREE.CircleGeometry(.3, 10), G('#ffe9a0'), .9, 1.3, 1.45).rotation.y = .55; },
  greenhouse: (g) => { add(g, new THREE.SphereGeometry(2.2, 14, 8, 0, 6.283, 0, Math.PI / 2), new THREE.MeshToonMaterial({ color: '#9ff2ff', transparent: true, opacity: .45 }), 0, .2, 0); add(g, new THREE.CylinderGeometry(2.3, 2.4, .4, 14), M('#e8ecff'), 0, .2, 0);
    for (let i = 0; i < 4; i++) { const a = i * 1.57; add(g, new THREE.IcosahedronGeometry(.55, 1), M('#4fd06a'), Math.cos(a) * .9, .9, Math.sin(a) * .9); } add(g, new THREE.IcosahedronGeometry(.3, 0), G('#ff7ac0'), 0, 1.5, 0); },
  lab: (g) => { add(g, new THREE.CylinderGeometry(2, 2.2, 1.6, 12), M('#e8ecff'), 0, .8, 0); add(g, new THREE.SphereGeometry(1.6, 14, 8, 0, 6.283, 0, Math.PI / 2), M('#59d7ff', { emissive: '#1a6fa0', emissiveIntensity: .5 }), 0, 1.6, 0); add(g, new THREE.CylinderGeometry(.05, .05, 2, 5), M('#8d8fa8'), 1.2, 3, 0); g.userData.spin = add(g, new THREE.OctahedronGeometry(.25), G('#6ff2ff'), 1.2, 4.1, 0); },
  forge: (g) => { add(g, new THREE.BoxGeometry(3, 2, 2.6), M('#5a4a5a'), 0, 1, 0); add(g, new THREE.CylinderGeometry(.5, .7, 3, 8), M('#3a2f3a'), .8, 3, -.5); add(g, new THREE.SphereGeometry(.75, 10, 8), G('#ff8a2a'), 0, .9, 1.25); g.userData.pulse = add(g, new THREE.SphereGeometry(.4, 8, 6), G('#ffd76a'), .8, 4.6, -.5); },
  hangar: (g) => { add(g, new THREE.CylinderGeometry(2.6, 2.6, 3.6, 14, 1, false, 0, Math.PI), M('#c9d2f0'), 0, .1, 0, 0, Math.PI / 2).rotation.y = Math.PI / 2; add(g, new THREE.CylinderGeometry(3, 3.2, .25, 16), M('#4a4f7a'), 0, .12, 3.6); add(g, new THREE.TorusGeometry(2.2, .08, 6, 24), G('#6ff2ff'), 0, .3, 3.6, Math.PI / 2); },
  observatory: (g) => { add(g, new THREE.CylinderGeometry(1.3, 1.6, 3.6, 10), M('#e4e0f4'), 0, 1.8, 0); add(g, new THREE.SphereGeometry(1.5, 12, 8, 0, 6.283, 0, Math.PI / 2), M('#b48aff'), 0, 3.6, 0); add(g, new THREE.CylinderGeometry(.3, .45, 2.4, 8), M('#4a4f7a'), .6, 4.6, .4, .7, -.5); },
  temple: (g) => { for (let i = 0; i < 3; i++) add(g, new THREE.CylinderGeometry(3 - i * .7, 3.2 - i * .7, .5, 8), M('#d8d0e8', { flatShading: true }), 0, .25 + i * .5, 0); for (let i = 0; i < 4; i++) { const a = i * 1.57 + .78; add(g, new THREE.CylinderGeometry(.18, .22, 2.4, 6), M('#f4f0ff'), Math.cos(a) * 1.3, 2.7, Math.sin(a) * 1.3); }
    g.userData.spin = add(g, new THREE.OctahedronGeometry(.6), G('#ffe46a'), 0, 3.2, 0); const h = createAtmosphere(1.2, '#ffe46a', 2.5, .9); h.position.y = 3.2; g.add(h); },
  tower: (g) => { add(g, new THREE.CylinderGeometry(.7, 1.2, 6, 8), M('#e8ecff'), 0, 3, 0); g.userData.spin = add(g, new THREE.TorusGeometry(1.5, .1, 6, 28), G('#ff7ad8'), 0, 5.4, 0, 1.2); add(g, new THREE.SphereGeometry(.6, 10, 8), G('#ff7ad8'), 0, 6.5, 0); },
};
export function buildHomeBase(planet, spawnDir, particles) {
  const P = planet.userData, group = new THREE.Group(), colliders = [], slots = [], up = new THREE.Vector3(0, 1, 0), q = new THREE.Quaternion(), qy = new THREE.Quaternion(), t1 = new THREE.Vector3(), t2 = new THREE.Vector3();
  t1.set(1, 0, 0).cross(spawnDir).normalize(); t2.crossVectors(spawnDir, t1);
  const ringG = new THREE.TorusGeometry(2.4, .12, 6, 32), plusG = new THREE.OctahedronGeometry(.35);
  for (let i = 0; i < 8; i++) { const a = i / 8 * 6.283 + .4, r = 15 + (i % 2) * 8, dir = new THREE.Vector3().copy(spawnDir).multiplyScalar(P.radius).addScaledVector(t1, Math.cos(a) * r).addScaledVector(t2, Math.sin(a) * r).normalize();
    if (P.sample(dir) - P.sea < .015) continue;
    const root = new THREE.Group(); root.position.copy(dir).multiplyScalar(P.heightAt(dir) - .1); root.quaternion.copy(q.setFromUnitVectors(up, dir)).multiply(qy.setFromAxisAngle(up, a + Math.PI / 2)); group.add(root);
    const marker = new THREE.Group(); const ring = new THREE.Mesh(ringG, G('#6ff2ff')); ring.rotation.x = Math.PI / 2; ring.position.y = .25; const plus = new THREE.Mesh(plusG, G('#ffe46a')); plus.position.y = 1.6; marker.add(ring, plus); root.add(marker);
    slots.push({ i, root, marker, plus, building: null, grow: 1, col: null }); }
  const place = (slotI, id, animate) => { const s = slots.find(x => x.i === slotI); if (!s || s.building) return; const g = new THREE.Group(), model = Assets.get('bld_' + id);
    if (model) g.add(model); else (BUILDERS[id] ?? BUILDERS.house)(g); s.root.add(g); s.building = g; s.marker.visible = false; s.grow = animate ? 0 : 1; s.col = { pos: s.root.position.clone(), r: 2.6 }; colliders.push(s.col);
    if (animate && particles) for (let k = 0; k < 40; k++) particles.emit(s.root.position, t1.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(14), k % 2 ? '#6ff2ff' : '#ffe46a', .5, .9, 2, 0); };
  for (const b of State.data.homePlanet.buildings) place(b.slot, b.id, false);
  const off = bus.on('build:placed', ({ planetId, slot, buildingId }) => { if (planetId === P.def.id) place(slot, buildingId, true); });
  const update = (dt, t, heroPos) => { let near = null, nd = 4;
    for (const s of slots) { if (s.building) { if (s.grow < 1) { s.grow = Math.min(1, s.grow + dt * 1.6); const k = s.grow, e = 1 + Math.sin(k * Math.PI) * .25; s.building.scale.set(k * e, k * (2 - e), k * e); }
        const u = s.building.userData; if (u.spin) u.spin.rotation.y += dt * 1.5; if (u.pulse) u.pulse.scale.setScalar(1 + Math.sin(t * 3) * .3); continue; }
      s.plus.rotation.y = t * 2; s.plus.position.y = 1.6 + Math.sin(t * 3 + s.i) * .2; const d = s.root.position.distanceTo(heroPos); if (d < nd) { nd = d; near = s; } }
    return near; };
  return { group, colliders, slots, update, dispose: off, count: () => slots.filter(s => s.building).length };
}
