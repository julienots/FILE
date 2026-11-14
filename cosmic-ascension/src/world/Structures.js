import * as THREE from 'three';
import { toonMaterial, addOutline } from '../render/shaders/Toon.js';
import { createAtmosphere } from '../render/shaders/Atmosphere.js';
import { Assets } from '../assets/AssetLibrary.js';

// Structures & habitants d'une planète. Data: planet.structures [{kind,lat,lon,scale?}], planet.npcs [{id,name,species?,lat,lon,model?,line?}].
// Sans data → village, ruines et monolithe secret placés de façon déterministe. Renvoie {group, colliders, npcs, secrets, update(dt,t,heroPos)}.
export const latLonToDir = (lat, lon, out = new THREE.Vector3()) => { const a = THREE.MathUtils.degToRad(lat), o = THREE.MathUtils.degToRad(lon); return out.set(Math.cos(a) * Math.cos(o), Math.sin(a), Math.cos(a) * Math.sin(o)); };
const BIOME = {
  meadow: { wall: '#4a4660', roof: '#1f2a4a', stone: '#6a6680', folk: '#2a2440', glow: '#6ff2ff', lines: ['Bienvenue sur Pépin, petit grain d’étoile !', 'On dit qu’un serpent géant nage entre les mondes…', 'Ramasse la poussière cosmique: elle te fera grandir.'] },
  volcanic: { wall: '#3a2228', roof: '#1a1014', stone: '#3a2630', folk: '#2a1418', glow: '#ffb02e', lines: ['La lave ? On s’y baigne le dimanche.', 'Les brutes de roche gardent les cristaux de braise.', 'Tu brilles plus fort qu’hier, étranger.'] },
  ice: { wall: '#6a7a94', roof: '#24385a', stone: '#8a9ab4', folk: '#1c2a40', glow: '#ffffff', lines: ['Brrr… même les étoiles gèlent ici.', 'Les anciens ont laissé un monolithe. Touche-le.', 'Ton vaisseau tient le coup dans ce froid ?'] },
  jungle: { wall: '#3c3450', roof: '#4a1a6a', stone: '#5a4a7a', folk: '#1e1a34', glow: '#ff7ad8', lines: ['Sylva-Mère t’observe par chaque spore.', 'Les champignons chantent quand un dieu naît.', 'Méfie-toi des scarabées, ils adorent ce qui brille.'] },
};
function hut(B) { const g = new THREE.Group();
  const base = new THREE.Mesh(new THREE.CylinderGeometry(1.7, 1.9, 2.2, 10), toonMaterial({ color: B.wall, rim: B.glow, rimStrength: .3 })); base.position.y = 1.1; g.add(base);
  const roof = new THREE.Mesh(new THREE.SphereGeometry(2.4, 12, 6, 0, 6.283, 0, Math.PI / 2), toonMaterial({ color: B.roof, rim: '#ffffff', rimStrength: .4 })); roof.position.y = 2.1; roof.scale.y = .85; g.add(roof);
  const door = new THREE.Mesh(new THREE.BoxGeometry(.8, 1.3, .2), toonMaterial({ color: '#5a3a2a' })); door.position.set(0, .65, 1.82); g.add(door);
  const win = new THREE.Mesh(new THREE.CircleGeometry(.32, 12), new THREE.MeshBasicMaterial({ color: B.glow })); win.position.set(.95, 1.4, 1.52); win.rotation.y = .55; g.add(win);
  const chim = new THREE.Mesh(new THREE.CylinderGeometry(.22, .26, 1.2, 6), toonMaterial({ color: B.stone })); chim.position.set(-.9, 3.6, 0); g.add(chim);
  g.userData.r = 2.2; return g; }
function ruin(B) { const g = new THREE.Group(), st = toonMaterial({ color: B.stone, rim: B.glow, rimStrength: .4, flatShading: true });
  for (const s of [-1, 1]) { const p = new THREE.Mesh(new THREE.CylinderGeometry(.55, .7, 5, 7), st); p.position.set(s * 2.2, 2.5, 0); p.rotation.z = s * .04; g.add(p); }
  const top = new THREE.Mesh(new THREE.BoxGeometry(6, .9, 1.3), st); top.position.y = 5.3; top.rotation.z = .05; g.add(top);
  const glyph = new THREE.Mesh(new THREE.TorusGeometry(.5, .09, 6, 20), new THREE.MeshBasicMaterial({ color: B.glow })); glyph.position.set(0, 5.3, .68); g.add(glyph);
  const fallen = new THREE.Mesh(new THREE.CylinderGeometry(.5, .5, 2.6, 7), st); fallen.position.set(4, .4, 1.2); fallen.rotation.set(1.5, 0, .5); g.add(fallen);
  g.userData.r = 0; g.userData.posts = [[-2.2, 0, .8], [2.2, 0, .8]]; return g; }
function monolith(B) { const g = new THREE.Group();
  const m = new THREE.Mesh(new THREE.OctahedronGeometry(1.2), new THREE.MeshBasicMaterial({ color: B.glow })); m.scale.set(1, 3.4, 1); m.position.y = 4.6; g.add(m);
  const base = new THREE.Mesh(new THREE.CylinderGeometry(2.2, 2.8, .8, 8), toonMaterial({ color: B.stone, rim: B.glow, flatShading: true })); base.position.y = .3; g.add(base);
  const halo = createAtmosphere(3.2, B.glow, 2.5, .9); halo.position.y = 4.6; g.add(halo);
  g.userData.r = 2.6; g.userData.crystal = m; return g; }
function villager(B, i, key) { const g = new THREE.Group(), model = key && Assets.get(key);
  if (model) { g.add(model); } else {
    const mat = toonMaterial({ color: new THREE.Color(B.folk).offsetHSL(i * .04, 0, 0), rim: B.glow, rimStrength: 1.2 });
    const hood = new THREE.Mesh(new THREE.ConeGeometry(.62, 1.1, 10), mat); hood.position.set(0, 2.05, -.12); hood.rotation.x = -.25; g.add(hood);
    const robe = new THREE.Mesh(new THREE.ConeGeometry(.7, 1.5, 10), mat); robe.position.y = .75; g.add(robe);
    const body = new THREE.Mesh(new THREE.CapsuleGeometry(.42, .7, 6, 12), mat); body.position.y = .8; addOutline(body, '#1a0f33', .03); g.add(body);
    const head = new THREE.Mesh(new THREE.SphereGeometry(.48, 14, 12), mat); head.position.y = 1.75; addOutline(head, '#1a0f33', .03); g.add(head);
    for (const s of [-1, 1]) { const e = new THREE.Mesh(new THREE.SphereGeometry(.1, 8, 8), new THREE.MeshBasicMaterial({ color: B.glow })); e.position.set(.17 * s, 1.8, .43); e.scale.set(1.3, .45, .6); e.rotation.z = .3 * s; g.add(e);
      const ear = new THREE.Mesh(new THREE.ConeGeometry(.12, .5, 6), mat); ear.position.set(.34 * s, 2.25, 0); ear.rotation.z = -.4 * s; g.add(ear); }
    const hat = new THREE.Mesh(new THREE.ConeGeometry(.3, .35, 8), new THREE.MeshBasicMaterial({ color: B.glow })); hat.position.y = 2.38; if (i % 2) g.add(hat); }
  const mark = new THREE.Mesh(new THREE.OctahedronGeometry(.18), new THREE.MeshBasicMaterial({ color: '#ffe46a' })); mark.position.y = 3; g.add(mark); g.userData.mark = mark; return g; }

export function buildStructures(planet) {
  const P = planet.userData, def = P.def, B = BIOME[def.biome] ?? BIOME.meadow, group = new THREE.Group(), colliders = [], npcs = [], secrets = [];
  let s = def.seed * 331 + 17; const rnd = () => (s = (s * 16807) % 2147483647) / 2147483647;
  const up = new THREE.Vector3(0, 1, 0), q = new THREE.Quaternion(), qy = new THREE.Quaternion(), d = new THREE.Vector3(), t1 = new THREE.Vector3(), t2 = new THREE.Vector3();
  const landDir = (out, minH = .05) => { for (let i = 0; i < 300; i++) { const u = rnd() * 1.6 - .8, a = rnd() * 6.283, r = Math.sqrt(1 - u * u); out.set(r * Math.cos(a), u, r * Math.sin(a)); const h = P.sample(out) - P.sea; if (h > minH && h < .3) return out; } return out; };
  const place = (obj, dir, yaw = rnd() * 6.283, sink = .15) => { obj.position.copy(dir).multiplyScalar(P.heightAt(dir) - sink); q.setFromUnitVectors(up, dir); obj.quaternion.copy(q).multiply(qy.setFromAxisAngle(up, yaw)); group.add(obj);
    if (obj.userData.r) colliders.push({ pos: obj.position.clone(), r: obj.userData.r * obj.scale.x });
    for (const p of obj.userData.posts ?? []) colliders.push({ pos: obj.localToWorld(new THREE.Vector3(p[0], p[1], 0)).clone(), r: p[2] }); };
  const offset = (base, dx, dz, out) => { t1.set(1, 0, 0).cross(base); if (t1.lengthSq() < .01) t1.set(0, 0, 1).cross(base); t1.normalize(); t2.crossVectors(base, t1); return out.copy(base).multiplyScalar(P.radius).addScaledVector(t1, dx).addScaledVector(t2, dz).normalize(); };
  const builders = { hut, ruin, monolith };
  const addNpc = (dir, i, spec = {}) => { const v = villager(B, i, spec.model ?? ('npc_' + (spec.species ?? def.biome))); place(v, dir, 0, 0);
    npcs.push({ id: spec.id ?? `${def.id}:villager:${i}`, name: spec.name ?? ['Milo', 'Zia', 'Orbe', 'Tiko', 'Nuna', 'Fen'][i % 6], line: spec.line ?? B.lines[i % B.lines.length], root: v, dir: dir.clone(), t: rnd() * 6 }); };
  if (def.structures?.length) for (const st of def.structures) { if (st.kind === 'ship_pad') continue; const o = (builders[st.kind] ?? (/village|city|camp|oasis|dome|refinery|citadel/.test(st.kind) ? hut : /temple|tomb|shrine|beacon|portal|lighthouse|monolith/.test(st.kind) ? monolith : ruin))(B); o.scale.setScalar(st.scale ?? 1); place(o, latLonToDir(st.lat, st.lon, d).clone()); if (o.userData.crystal) secrets.push({ id: `${def.id}:monolith:${secrets.length}`, obj: o, found: false }); }
  else {
    const c = landDir(new THREE.Vector3(), .08).clone();                                   // village
    for (let i = 0; i < 5; i++) { const a = i / 5 * 6.283; offset(c, Math.cos(a) * 9, Math.sin(a) * 9, d); if (P.sample(d) - P.sea > .02) place(hut(B), d.clone(), a + Math.PI / 2); }
    for (let i = 0; i < 4; i++) { const a = i / 4 * 6.283 + .6; offset(c, Math.cos(a) * 3.5, Math.sin(a) * 3.5, d); addNpc(d.clone(), i); }
    for (let i = 0; i < 3; i++) place(ruin(B), landDir(d).clone());
    const m = monolith(B); place(m, landDir(d, .12).clone()); secrets.push({ id: `${def.id}:monolith:0`, obj: m, found: false });
  }
  if (def.npcs?.length) def.npcs.forEach((n, i) => addNpc(latLonToDir(n.lat, n.lon, d).clone(), i + 10, n));
  const m4 = new THREE.Matrix4(), tq = new THREE.Quaternion();
  const update = (dt, t, heroPos) => {
    let nearest = null, nd = 5;
    for (const n of npcs) { n.t += dt; const dist = n.root.position.distanceTo(heroPos); n.root.userData.mark.position.y = 3 + Math.sin(n.t * 3) * .15; n.root.userData.mark.rotation.y = n.t * 2;
      n.root.scale.y = 1 + Math.sin(n.t * 2.5) * .03;
      if (dist < 14) { t1.copy(heroPos).sub(n.root.position); t1.addScaledVector(n.dir, -t1.dot(n.dir)).normalize(); t2.crossVectors(n.dir, t1).normalize(); n.root.quaternion.slerp(tq.setFromRotationMatrix(m4.makeBasis(t2, n.dir, t1)), Math.min(1, dt * 5)); }
      if (dist < nd) { nd = dist; nearest = n; } }
    for (const sc of secrets) { const cr = sc.obj.userData.crystal; if (cr) { cr.rotation.y += dt * (sc.found ? .3 : 1.2); cr.position.y = 4.6 + Math.sin(t * 1.5) * .3; } }
    return nearest;
  };
  return { group, colliders, npcs, secrets, update };
}
// Nuages bas-poly vus du sol (1 draw call), en orbite lente.
export function buildSkyClouds(planet, color = '#ffffff') {
  const P = planet.userData, n = 46, geo = new THREE.IcosahedronGeometry(1, 1), mesh = new THREE.InstancedMesh(geo, new THREE.MeshToonMaterial({ color: new THREE.Color(color).multiplyScalar(.7), emissive: color, emissiveIntensity: .12, transparent: true, opacity: .9 }), n * 3);
  let s = P.def.seed * 71 + 5; const rnd = () => (s = (s * 16807) % 2147483647) / 2147483647, m = new THREE.Matrix4(), q = new THREE.Quaternion(), d = new THREE.Vector3(), p = new THREE.Vector3(), sc = new THREE.Vector3(), up = new THREE.Vector3(0, 1, 0), t = new THREE.Vector3();
  for (let i = 0; i < n; i++) { const u = rnd() * 2 - 1, a = rnd() * 6.283, r = Math.sqrt(1 - u * u); d.set(r * Math.cos(a), u, r * Math.sin(a)); q.setFromUnitVectors(up, d); const alt = P.radius * (1 + P.relief) + 22 + rnd() * 22, big = 5 + rnd() * 7;
    t.set(1, 0, 0).applyQuaternion(q);
    for (let k = 0; k < 3; k++) { p.copy(d).multiplyScalar(alt + (k === 1 ? big * .25 : 0)).addScaledVector(t, (k - 1) * big * .95); const w = big * (k === 1 ? 1 : .7); m.compose(p, q, sc.set(w, w * .45, w * .8)); mesh.setMatrixAt(i * 3 + k, m); } }
  mesh.frustumCulled = false; mesh.renderOrder = 2; return mesh;
}
