import * as THREE from 'three';
import { mulberry32 } from './noise.js';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';

const rnd = mulberry32(31);
const SHEEP_LOD = new THREE.IcosahedronGeometry(1, 1);
const LOD_MAT = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.85 });

function mat(color, rough = 0.8, extra = {}) { return new THREE.MeshStandardMaterial({ color, roughness: rough, ...extra }); }
function part(geo, m, parent, x = 0, y = 0, z = 0) {
  const mesh = new THREE.Mesh(geo, m); mesh.position.set(x, y, z);
  geo.computeBoundingSphere(); mesh.userData.tiny = geo.boundingSphere.radius < 0.09;
  mesh.castShadow = !mesh.userData.tiny; mesh.receiveShadow = true; parent.add(mesh); return mesh;
}

/**
 * Personnage stylisé (~1,8 m) entièrement procédural, articulé (hanches, genoux, épaules, coudes, tête)
 * et animé par des cycles procéduraux (repos, marche, course, saut).
 */
export function buildHumanoid(o = {}) {
  const P = {
    skin: '#e8b48f', tunic: '#2f8a7e', tunic2: '#e9dfc7', pants: '#5a4634', boots: '#5b3a26', hair: '#4a3020',
    belt: '#3b2a1c', scarf: null, hood: false, scale: 1, cape: null, pack: false, staff: false, ...o,
  };
  const root = new THREE.Group();
  const body = new THREE.Group(); root.add(body);
  const M = { skin: mat(P.skin, 0.65), tunic: mat(P.tunic, 0.8), tunic2: mat(P.tunic2, 0.85), pants: mat(P.pants, 0.85), boots: mat(P.boots, 0.55), hair: mat(P.hair, 0.6), belt: mat(P.belt, 0.55), eye: mat('#1b1424', 0.15), white: new THREE.MeshBasicMaterial({ color: '#ffffff' }), blush: mat('#f08a8a', 0.9, { transparent: true, opacity: 0.55 }), gold: mat('#e2b85a', 0.3, { metalness: 0.7 }) };
  const cap = (r, len, segs = 10) => new THREE.CapsuleGeometry(r, len, 6, segs);

  // style « jeu » : grosse tête ronde, corps en cloche, membres en capsules, aucune arête vive
  const hips = new THREE.Group(); hips.position.y = 0.62; body.add(hips);
  const torso = new THREE.Group(); hips.add(torso);
  const bell = new THREE.LatheGeometry([[0, -0.2], [0.25, -0.2], [0.26, -0.15], [0.2, 0.05], [0.17, 0.2], [0.2, 0.32], [0.14, 0.42], [0, 0.45]].map(([x, y]) => new THREE.Vector2(x, y)), 28);
  part(bell, M.tunic, torso).scale.z = 0.82;
  const collar = part(new THREE.TorusGeometry(0.12, 0.035, 10, 24), M.tunic2, torso, 0, 0.42, 0); collar.rotation.x = Math.PI / 2;
  const belt = part(new THREE.TorusGeometry(0.2, 0.03, 10, 28), M.belt, torso, 0, 0.02, 0); belt.rotation.x = Math.PI / 2; belt.scale.y = 0.82;
  part(new THREE.SphereGeometry(0.04, 12, 8), M.gold, torso, 0, 0.02, 0.17).scale.z = 0.5;
  if (P.scarf) { const sc = part(new THREE.TorusGeometry(0.13, 0.06, 12, 24), mat(P.scarf, 0.85), torso, 0, 0.43, 0); sc.rotation.x = Math.PI / 2; sc.scale.set(1, 0.85, 1); }
  // tête
  const neck = new THREE.Group(); neck.position.y = 0.45; torso.add(neck);
  part(cap(0.065, 0.04), M.skin, neck, 0, 0.03, 0);
  const head = new THREE.Group(); head.position.y = 0.27; neck.add(head);
  part(new THREE.SphereGeometry(0.26, 32, 24), M.skin, head).scale.set(1, 0.95, 0.95);
  for (const sx of [-1, 1]) {
    const eye = part(new THREE.SphereGeometry(0.048, 16, 12), M.eye, head, sx * 0.092, -0.01, 0.222); eye.scale.set(0.8, 1.15, 0.45);
    part(new THREE.SphereGeometry(0.014, 8, 6), M.white, head, sx * 0.092 + 0.012, 0.018, 0.242);
    part(new THREE.SphereGeometry(0.045, 12, 8), M.blush, head, sx * 0.15, -0.085, 0.19).scale.set(1, 0.55, 0.35);
  }
  part(new THREE.SphereGeometry(0.022, 10, 8), M.skin, head, 0, -0.06, 0.245);
  // cheveux : calotte + frange en mèches arrondies
  const hairCap = part(new THREE.SphereGeometry(0.275, 28, 18, 0, Math.PI * 2, 0, Math.PI * 0.56), M.hair, head, 0, 0.015, -0.015);
  hairCap.rotation.x = -0.32;
  for (let i = -2; i <= 2; i++) part(new THREE.SphereGeometry(0.085, 14, 10), M.hair, head, i * 0.075, 0.15 - Math.abs(i) * 0.018, 0.17 - Math.abs(i) * 0.03).scale.set(1, 0.8, 0.7);
  part(new THREE.SphereGeometry(0.2, 18, 12), M.hair, head, 0, -0.02, -0.12).scale.set(1.25, 1.1, 0.9);
  if (P.ponytail) { const pt = part(cap(0.07, 0.22), M.hair, head, 0, -0.12, -0.27); pt.rotation.x = 0.5; }
  if (P.hood) { const h = part(new THREE.SphereGeometry(0.31, 24, 16, 0, Math.PI * 2, 0, Math.PI * 0.62), mat(P.hood, 0.85), head, 0, 0.01, -0.03); h.rotation.x = -0.45; }
  if (P.hat) { part(new THREE.CylinderGeometry(0.4, 0.42, 0.03, 32), mat(P.hat, 0.85), head, 0, 0.19, 0); part(new THREE.SphereGeometry(0.2, 24, 14, 0, Math.PI * 2, 0, Math.PI / 2), mat(P.hat, 0.85), head, 0, 0.19, 0).scale.y = 0.8; }

  // bras
  const arm = (side) => {
    const sh = new THREE.Group(); sh.position.set(side * 0.22, 0.36, 0); torso.add(sh);
    part(new THREE.SphereGeometry(0.075, 14, 10), M.tunic, sh);
    const upper = part(cap(0.055, 0.12), M.tunic2, sh, 0, -0.1, 0);
    const elbow = new THREE.Group(); elbow.position.y = -0.2; sh.add(elbow);
    part(cap(0.048, 0.1), M.skin, elbow, 0, -0.08, 0);
    part(new THREE.SphereGeometry(0.062, 14, 10), M.skin, elbow, 0, -0.18, 0.01);
    sh.rotation.z = side * 0.1;
    return { sh, elbow, upper };
  };
  const leg = (side) => {
    const hip = new THREE.Group(); hip.position.set(side * 0.1, -0.08, 0); hips.add(hip);
    part(cap(0.075, 0.12), M.pants, hip, 0, -0.12, 0);
    const knee = new THREE.Group(); knee.position.y = -0.25; hip.add(knee);
    part(cap(0.07, 0.1), M.boots, knee, 0, -0.1, 0);
    const foot = part(cap(0.068, 0.1), M.boots, knee, 0, -0.22, 0.05); foot.rotation.x = Math.PI / 2; foot.scale.set(1.05, 1, 0.75);
    return { hip, knee, foot };
  };
  const L = { armL: arm(-1), armR: arm(1), legL: leg(-1), legR: leg(1) };

  // accessoires : sac, bâton de cristal, cape
  if (P.pack) {
    part(new RoundedBoxGeometry(0.3, 0.3, 0.15, 3, 0.05), mat('#8a5a34', 0.75), torso, 0, 0.24, -0.22);
    const roll = part(cap(0.07, 0.24), mat('#c24a32', 0.85), torso, 0, 0.42, -0.24); roll.rotation.z = Math.PI / 2;
  }
  if (P.staff) {
    const st = part(cap(0.025, 1.2, 6), mat('#7a5232', 0.65), torso, 0, 0.2, -0.28); st.rotation.z = 0.6;
    const gem = part(new THREE.OctahedronGeometry(0.08, 1), mat('#9df0ff', 0.1, { emissive: '#3fd0ff', emissiveIntensity: 2.2 }), st, 0, 0.68, 0); gem.scale.y = 1.4;
  }
  let cape = null;
  if (P.cape) {
    const g = new THREE.PlaneGeometry(0.42, 0.6, 4, 6); g.translate(0, -0.3, 0);
    cape = part(g, mat(P.cape, 0.9, { side: THREE.DoubleSide }), torso, 0, 0.42, -0.18); cape.rotation.x = 0.12;
  }

  // LOD : silhouette simplifiée (1 appel de dessin) quand le personnage est loin de la caméra
  const lodGeo = (() => {
    const parts = [[new THREE.CylinderGeometry(0.18, 0.25, 0.65, 8), P.tunic, 0.82], [new THREE.CylinderGeometry(0.09, 0.09, 0.5, 6), P.pants, 0.27], [new THREE.SphereGeometry(0.26, 10, 8), P.skin, 1.36], [new THREE.SphereGeometry(0.27, 10, 6, 0, Math.PI * 2, 0, 1.6), P.hair, 1.4]];
    const geos = parts.map(([g, c, y]) => { g.translate(0, y, 0); const col = new THREE.Color(c), a = new Float32Array(g.attributes.position.count * 3); for (let i = 0; i < a.length; i += 3) { a[i] = col.r; a[i + 1] = col.g; a[i + 2] = col.b; } g.setAttribute('color', new THREE.BufferAttribute(a, 3)); g.deleteAttribute('uv'); return g.toNonIndexed(); });
    const out = new THREE.BufferGeometry();
    ['position', 'normal', 'color'].forEach(k => { const arrs = geos.map(g => g.attributes[k].array); const n = arrs.reduce((t, x) => t + x.length, 0); const m = new Float32Array(n); let o = 0; arrs.forEach(x => { m.set(x, o); o += x.length; }); out.setAttribute(k, new THREE.BufferAttribute(m, 3)); });
    return out;
  })();
  const lod = new THREE.Mesh(lodGeo, LOD_MAT); lod.visible = false; root.add(lod);
  root.userData.setLOD = (far) => { body.visible = !far; lod.visible = far; };
  root.scale.setScalar(P.scale);
  const state = { phase: rnd() * 10, speed: 0, air: 0, blend: 0, talk: 0, attack: 0 };

  function animate(dt, speed, grounded = true, vy = 0) {
    // speed en m/s ; 0..1.7 marche, >3 course
    state.speed += (speed - state.speed) * Math.min(1, dt * 10);
    const s = state.speed;
    const run = THREE.MathUtils.smoothstep(s, 2.2, 5);
    const move = THREE.MathUtils.smoothstep(s, 0.05, 0.6);
    state.phase += dt * (s * 1.9 / P.scale + (1 - move) * 1.2);
    const ph = state.phase;
    const amp = THREE.MathUtils.lerp(0.55, 0.95, run) * move;
    const sw = Math.sin(ph), sw2 = Math.sin(ph * 2);
    L.legL.hip.rotation.x = sw * amp; L.legR.hip.rotation.x = -sw * amp;
    L.legL.knee.rotation.x = Math.max(0, -Math.cos(ph)) * amp * 1.4 + 0.05;
    L.legR.knee.rotation.x = Math.max(0, Math.cos(ph)) * amp * 1.4 + 0.05;
    L.armL.sh.rotation.x = -sw * amp * 0.9; L.armR.sh.rotation.x = sw * amp * 0.9;
    L.armL.elbow.rotation.x = -0.25 - run * 1.1; L.armR.elbow.rotation.x = -0.25 - run * 1.1;
    const idle = (1 - move);
    L.armL.sh.rotation.z = -0.08 - idle * 0.04 * Math.sin(ph * 0.7);
    L.armR.sh.rotation.z = 0.08 + idle * 0.04 * Math.sin(ph * 0.7);
    torso.rotation.x = run * 0.22 + move * 0.04;
    torso.rotation.y = sw * 0.12 * move;
    body.position.y = Math.abs(sw2) * 0.05 * move - run * 0.04 + idle * Math.sin(ph * 1.3) * 0.008;
    head.rotation.y = state.talk ? Math.sin(ph * 0.8) * 0.15 : 0;
    if (cape) cape.rotation.x = 0.12 + run * 0.9 + move * 0.25 + Math.sin(ph * 3) * 0.05 * move;
    if (state.attack > 0) { // coup de bâton
      state.attack = Math.max(0, state.attack - dt * 2.6);
      const k = Math.sin((1 - state.attack) * Math.PI);
      L.armR.sh.rotation.x = -2.4 * k + 0.6 * (1 - k) * 0;
      L.armR.sh.rotation.z = 0.08 + k * 0.5;
      L.armR.elbow.rotation.x = -0.3 * k;
      torso.rotation.y = -0.6 * k;
    }
    if (!grounded) {
      state.air = Math.min(1, state.air + dt * 6);
    } else state.air = Math.max(0, state.air - dt * 8);
    if (state.air > 0) {
      const a = state.air, up = vy > 0 ? 1 : 0.5;
      L.legL.hip.rotation.x = THREE.MathUtils.lerp(L.legL.hip.rotation.x, -0.9 * up, a);
      L.legL.knee.rotation.x = THREE.MathUtils.lerp(L.legL.knee.rotation.x, 1.3, a);
      L.legR.hip.rotation.x = THREE.MathUtils.lerp(L.legR.hip.rotation.x, 0.3, a);
      L.legR.knee.rotation.x = THREE.MathUtils.lerp(L.legR.knee.rotation.x, 0.6, a);
      L.armL.sh.rotation.z = THREE.MathUtils.lerp(L.armL.sh.rotation.z, -1.1, a);
      L.armR.sh.rotation.z = THREE.MathUtils.lerp(L.armR.sh.rotation.z, 1.1, a);
    }
  }
  return { root, animate, state, height: 1.66 * P.scale };
}

// ---------- Villageois autonomes ----------
export function buildVillagers(terrain, world, count = 10) {
  const looks = [
    { tunic: '#b5523b', tunic2: '#efe2c4', hair: '#2b1d14', scarf: '#e0b04a' },
    { tunic: '#4f6fa8', tunic2: '#e9dfc7', hair: '#c79a5a', ponytail: true },
    { tunic: '#7a9a3f', tunic2: '#f1e6cc', hair: '#5a3a24', hat: '#c8a36a' },
    { tunic: '#9a6bb0', tunic2: '#efe5d1', hair: '#1c1612', ponytail: true },
    { tunic: '#c98d3a', tunic2: '#f3ead2', hair: '#8a5a32' },
    { tunic: '#3f8f86', tunic2: '#e8dcc0', hair: '#d8d0c0', hat: '#7a5a3a', scale: 0.95 },
    { tunic: '#a83f52', tunic2: '#efe2c4', hair: '#40281a', scarf: '#4a7fbf' },
    { tunic: '#5b7d5a', tunic2: '#e9dfc7', hair: '#6b4426', scale: 0.72 },
    { tunic: '#d0a24a', tunic2: '#f4ecd8', hair: '#2b1d14', scale: 0.68, ponytail: true },
    { tunic: '#6a5aa0', tunic2: '#efe2c4', hair: '#a07040', hat: '#3f5f8f' },
  ];
  const names = ['Maëlle', 'Ardan', 'Iselle', 'Corentin', 'Brune', 'Vieil Oswin', 'Talia', 'Pip', 'Lou', 'Garrick'];
  const lines = [
    ['Bienvenue à Clairval, voyageur !', 'Le ciel est si clair aujourd\'hui... on voit même les reflets du grand dôme.'],
    ['Tu as vu ? Les nuages ne dépassent jamais la voûte.', 'Ma grand-mère disait que le monde avait été posé là par quelqu\'un.'],
    ['Le marché ouvre à l\'aube.', 'Des pommes de la vallée, fraîches comme la rosée !'],
    ['Le soleil tourne en rond, tu ne trouves pas ça étrange ?', 'Certains soirs, je jure que deux yeux immenses nous regardent.'],
    ['Le moulin tourne grâce au vent du Créateur.', 'Moi, je fais confiance au vent. Il ne m\'a jamais menti.'],
    ['Quand j\'étais jeune, j\'ai marché jusqu\'au bord du monde.', 'Il y a un mur de verre. Derrière, des étoiles... et une ombre géante.'],
    ['La cascade du nord est magnifique au lever du soleil.', 'Fais attention aux rochers glissants.'],
    ['Tu joues avec nous ?', 'On fait la course jusqu\'au puits !'],
    ['Chut ! Je cherche des grenouilles.', 'Il y en a plein près du lac.'],
    ['Les gardes disent que des créatures rôdent dans la forêt de l\'est.', 'Si tu y vas, prends de quoi te défendre.'],
  ];
  const V = world.village;
  const list = [];
  for (let i = 0; i < count; i++) {
    const h = buildHumanoid(looks[i % looks.length]);
    const a = rnd() * 6.28, r = 3 + rnd() * 9;
    h.root.position.set(V.x + Math.cos(a) * r, 0, V.y + Math.sin(a) * r);
    h.root.position.y = terrain.heightAt(h.root.position.x, h.root.position.z);
    list.push({ ...h, name: names[i], lines: lines[i], target: null, wait: rnd() * 4, speed: 1.1 + rnd() * 0.4, child: (looks[i].scale || 1) < 0.8 });
  }
  // points d'intérêt : puits, étals, portes des maisons, abords du pont
  const pois = [[V.x, V.y + 2.2], [V.x + 4.5, V.y + 3], [V.x - 5, V.y + 2.2], [V.x + 8, V.y - 3], [V.x - 8, V.y - 4], [V.x - 2, V.y + 9], [V.x + 12, V.y + 8], [V.x - 12, V.y + 6], [-12, 12], [-4, 10], [-30, 32], [-40, 12]];
  function update(dt, solve) {
    for (const n of list) {
      const p = n.root.position;
      if (n.talking) { n.animate(dt, 0); continue; }
      if (!n.target) {
        n.wait -= dt;
        if (n.wait <= 0) { const q = pois[Math.floor(rnd() * pois.length)]; n.target = new THREE.Vector2(q[0] + (rnd() - 0.5) * 3, q[1] + (rnd() - 0.5) * 3); }
        n.animate(dt, 0);
      } else {
        const dx = n.target.x - p.x, dz = n.target.y - p.z, d = Math.hypot(dx, dz);
        if (d < 0.6) { n.target = null; n.wait = 2 + rnd() * 6; n.animate(dt, 0); continue; }
        const sp = n.child ? 2.6 : n.speed;
        const yaw = Math.atan2(dx, dz);
        let dy = yaw - n.root.rotation.y; dy = Math.atan2(Math.sin(dy), Math.cos(dy));
        n.root.rotation.y += dy * Math.min(1, dt * 5);
        const nx = p.x + Math.sin(n.root.rotation.y) * sp * dt, nz = p.z + Math.cos(n.root.rotation.y) * sp * dt;
        const res = solve(nx, nz, 0.3);
        if (Math.hypot(res.x - nx, res.z - nz) > 0.02 * sp) { n.stuck = (n.stuck || 0) + dt; if (n.stuck > 1.5) { n.target = null; n.stuck = 0; } }
        p.x = res.x; p.z = res.z;
        n.animate(dt, sp);
      }
      p.y = world.groundAt(p.x, p.z, p.y);
    }
  }
  return { list, update };
}

// ---------- Moutons ----------
export function buildSheep(terrain, count = 9) {
  const wool = mat('#f4f0e6', 1), face = mat('#2e2620', 0.8);
  const list = [];
  for (let i = 0; i < count; i++) {
    const g = new THREE.Group();
    const bodyG = new THREE.Group(); g.add(bodyG);
    for (let k = 0; k < 7; k++) part(new THREE.IcosahedronGeometry(0.28 + rnd() * 0.08, 1), wool, bodyG, (rnd() - 0.5) * 0.3, 0.7 + (rnd() - 0.5) * 0.15, (rnd() - 0.5) * 0.6);
    const head = part(new THREE.SphereGeometry(0.15, 10, 8), face, g, 0, 0.82, 0.52); head.scale.set(0.85, 1, 1.25);
    part(new THREE.SphereGeometry(0.13, 8, 6), wool, head, 0, 0.1, -0.05);
    const legs = [];
    for (const sx of [-1, 1]) for (const sz of [-1, 1]) { const l = part(new THREE.CylinderGeometry(0.04, 0.035, 0.5, 5), face, g, sx * 0.17, 0.25, sz * 0.25); legs.push(l); }
    const x = -20 + rnd() * 18, z = 42 + rnd() * 16;
    g.position.set(x, terrain.heightAt(x, z), z); g.rotation.y = rnd() * 6.28;
    const lod = new THREE.Mesh(SHEEP_LOD, wool); lod.position.y = 0.65; lod.scale.set(0.45, 0.4, 0.65); lod.visible = false; g.add(lod);
    g.userData.setLOD = (far) => { g.children.forEach(c => { c.visible = c === lod ? far : !far; }); };
    list.push({ root: g, head, legs, t: rnd() * 10, target: null, wait: rnd() * 5 });
  }
  function update(dt, world) {
    for (const s of list) {
      s.t += dt;
      const p = s.root.position;
      if (!s.target) {
        s.wait -= dt;
        s.head.rotation.x = 0.6 + Math.sin(s.t * 2) * 0.1; // broute
        if (s.wait < 0) s.target = new THREE.Vector2(p.x + (rnd() - 0.5) * 8, p.z + (rnd() - 0.5) * 8);
      } else {
        const dx = s.target.x - p.x, dz = s.target.y - p.z, d = Math.hypot(dx, dz);
        if (d < 0.3 || !terrain.vegetationOK(s.target.x, s.target.y, 0.4)) { s.target = null; s.wait = 3 + rnd() * 6; continue; }
        s.root.rotation.y += (Math.atan2(dx, dz) - s.root.rotation.y) * Math.min(1, dt * 3);
        p.x += Math.sin(s.root.rotation.y) * dt * 0.7; p.z += Math.cos(s.root.rotation.y) * dt * 0.7;
        s.head.rotation.x = 0;
        s.legs.forEach((l, i) => { l.rotation.x = Math.sin(s.t * 9 + i * Math.PI * (i % 3 ? 1 : 0)) * 0.4; });
      }
      p.y = world.groundAt(p.x, p.z, p.y);
    }
  }
  return { list, update };
}

// ---------- Oiseaux ----------
export function buildBirds(count = 14) {
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute([0, 0, 0.25, -0.7, 0, -0.1, 0, 0, -0.15, 0, 0, 0.25, 0, 0, -0.15, 0.7, 0, -0.1], 3));
  geo.computeVertexNormals();
  const m = new THREE.MeshStandardMaterial({ color: '#2a2a30', side: THREE.DoubleSide, roughness: 0.9 });
  const list = [];
  const group = new THREE.Group();
  for (let i = 0; i < count; i++) {
    const b = new THREE.Mesh(geo.clone(), m); b.castShadow = false;
    b.scale.setScalar(0.8 + rnd() * 0.4);
    group.add(b);
    list.push({ mesh: b, r: 25 + rnd() * 50, h: 38 + rnd() * 25, sp: (0.08 + rnd() * 0.08) * (rnd() > 0.5 ? 1 : -1), a: rnd() * 6.28, cx: (rnd() - 0.5) * 40, cz: (rnd() - 0.5) * 40, ph: rnd() * 6 });
  }
  function update(dt, time) {
    for (const b of list) {
      b.a += b.sp * dt;
      const x = b.cx + Math.cos(b.a) * b.r, z = b.cz + Math.sin(b.a) * b.r, y = b.h + Math.sin(time * 0.5 + b.ph) * 3;
      b.mesh.position.set(x, y, z);
      b.mesh.rotation.y = Math.atan2(-Math.sin(b.a) * Math.sign(b.sp), Math.cos(b.a) * Math.sign(b.sp)) ;
      b.mesh.rotation.y = -b.a + (b.sp > 0 ? Math.PI : 0);
      const flap = Math.sin(time * 9 + b.ph) * 0.6;
      const p = b.mesh.geometry.attributes.position;
      p.setY(1, flap); p.setY(5, flap); p.needsUpdate = true;
    }
  }
  return { group, update };
}
