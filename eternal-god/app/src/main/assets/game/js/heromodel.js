// Héros jouables : le Dieu Céleste (modèle procédural) et des personnages importés (modèles 3D
// statiques auxquels l'outil tools/rig/rig.mjs a ajouté un squelette et des poids de peau).
// Un héros importé expose la même interface que le modèle procédural : racine, os nommés
// (hanches, colonne, bras, jambes…) et updateWings(), donc toutes les animations du jeu s'appliquent.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { buildGodModel } from './godmodel.js';
import { Q, G } from './core.js';

export const HEROES = {
  celeste: { name: 'Dieu Céleste', desc: 'Armure d\'or et d\'ivoire, ailes de lumière, auréole divine.' },
  void: { name: 'Seigneur du Néant', desc: 'Un trou noir à la place du visage. Il dévore la lumière.', file: 'models/heroes/void' },
};

const DOWN = new THREE.Vector3(0, -1, 0), UPV = new THREE.Vector3(0, 1, 0), FWD = new THREE.Vector3(0, -0.25, 1).normalize();

export async function loadHero(id) {
  const H = HEROES[id] || HEROES.celeste;
  if (!H.file) return buildGodModel();
  const [gltf, rig, bin] = await Promise.all([
    new GLTFLoader().loadAsync(H.file + '.glb'),
    fetch(H.file + '.rig.json').then((r) => r.json()),
    fetch(H.file + '.rig.bin').then((r) => r.arrayBuffer()),
  ]);
  return buildRiggedHero(gltf, rig, new Uint8Array(bin));
}

function buildRiggedHero(gltf, rig, bin) {
  let src = null;
  gltf.scene.traverse((o) => { if (o.isMesh && !src) src = o; });
  const geo = src.geometry;
  const n = geo.attributes.position.count;
  if (n !== rig.vertices) throw new Error(`Squelette incompatible (${n} sommets au lieu de ${rig.vertices})`);
  const J = rig.joints;
  const bb = new THREE.Box3().setFromBufferAttribute(geo.attributes.position);
  const scale = rig.height / (bb.max.y - bb.min.y);
  geo.scale(scale, scale, scale);
  const P = (name) => new THREE.Vector3(...J[name]).multiplyScalar(scale);

  // ---- os : liaison (pose du modèle) puis pose canonique (membres droits) pour l'animation
  const ENDS = { hips: 'spine', spine: 'chest', chest: 'neck', neck: 'head', head: 'headTop', upperL: 'foreL', foreL: 'handL', handL: 'handTipL', upperR: 'foreR', foreR: 'handR', handR: 'handTipR', thighL: 'shinL', shinL: 'footL', footL: 'toeL', thighR: 'shinR', shinR: 'footR', footR: 'toeR' };
  const canonical = (name) => (/^(hips|spine|chest|neck|head)$/.test(name) ? UPV : /^foot/.test(name) ? FWD : DOWN);
  const bones = [], byName = {}, worldQ = {};
  for (const b of rig.bones) {
    const bone = new THREE.Bone(); bone.name = b.name;
    const p = P(b.name), dir = P(ENDS[b.name]).sub(p).normalize();
    const q = b.name === 'hips' ? new THREE.Quaternion() : new THREE.Quaternion().setFromUnitVectors(canonical(b.name), dir);
    worldQ[b.name] = q;
    if (b.parent) {
      const pp = P(b.parent), pq = worldQ[b.parent];
      bone.position.copy(p.clone().sub(pp).applyQuaternion(pq.clone().invert()));
      bone.quaternion.copy(pq.clone().invert().multiply(q));
      byName[b.parent].add(bone);
    } else { bone.position.copy(p); bone.quaternion.copy(q); }
    bones.push(bone); byName[b.name] = bone;
  }
  // os d'orbite pour les objets séparés (planètes, anneaux…)
  const orbits = [];
  for (const o of rig.objects) {
    const bone = new THREE.Bone(); bone.name = 'orbit' + o.label;
    const c = new THREE.Vector3(...o.center).multiplyScalar(scale);
    bone.position.copy(c);
    bones.push(bone);
    orbits.push({ bone, label: o.label, c, phase: Math.atan2(c.z - P('chest').z, c.x - P('chest').x), r: Math.hypot(c.x - P('chest').x, c.z - P('chest').z) + 0.6, speed: 0.25 + (o.label % 3) * 0.12 });
  }
  // poids de peau ; les objets séparés suivent leur os d'orbite
  const si = new Uint16Array(n * 4), sw = new Float32Array(n * 4);
  const orbitIndex = new Map(orbits.map((o) => [o.label, bones.indexOf(o.bone)]));
  for (let i = 0; i < n; i++) {
    const lab = bin[n * 8 + i];
    if (lab > 0 && orbitIndex.has(lab)) { si[i * 4] = orbitIndex.get(lab); sw[i * 4] = 1; continue; }
    for (let k = 0; k < 4; k++) { si[i * 4 + k] = bin[i * 4 + k]; sw[i * 4 + k] = bin[n * 4 + i * 4 + k] / 255; }
  }
  geo.setAttribute('skinIndex', new THREE.Uint16BufferAttribute(si, 4));
  geo.setAttribute('skinWeight', new THREE.Float32BufferAttribute(sw, 4));
  const mat = src.material;
  mat.envMapIntensity = 1.2;
  if (Q().label === 'Bas' && mat.map && mat.map.image) mat.map.anisotropy = 1;
  const mesh = new THREE.SkinnedMesh(geo, mat);
  mesh.castShadow = true; mesh.frustumCulled = false;
  const root = new THREE.Group();
  const holder = new THREE.Group();   // porte le squelette (la racine bouge, le dieu tourne)
  root.add(holder);
  holder.add(byName.hips);
  for (const o of orbits) holder.add(o.bone);
  holder.add(mesh);
  root.updateMatrixWorld(true);
  mesh.bind(new THREE.Skeleton(bones), mesh.matrixWorld);

  // pose canonique : rotations locales nulles (bras et jambes droits), positions de liaison gardées
  for (const b of rig.bones) {
    const bone = byName[b.name];
    if (b.parent) {
      const pp = P(b.parent);
      bone.position.copy(P(b.name).sub(pp).applyQuaternion(worldQ[b.parent].clone().invert()));
    }
    bone.quaternion.identity();
  }
  // les positions locales doivent rester celles du repère parent « canonique » : on les
  // recalcule pour que, rotations nulles, chaque membre parte de la bonne articulation
  for (const b of rig.bones) if (b.parent) byName[b.name].position.applyQuaternion(new THREE.Quaternion());

  // interface commune avec le modèle procédural
  const B = {
    hips: byName.hips, spine: byName.spine, chest: byName.chest, neck: byName.neck, head: byName.head,
    upperL: byName.upperL, foreL: byName.foreL, handL: byName.handL, upperR: byName.upperR, foreR: byName.foreR, handR: byName.handR,
    thighL: byName.thighL, shinL: byName.shinL, footL: byName.footL, thighR: byName.thighR, shinR: byName.shinR, footR: byName.footR,
    shoulderL: byName.chest, shoulderR: byName.chest,
    clothF: new THREE.Object3D(), clothB: new THREE.Object3D(), haloRays: new THREE.Object3D(), core: new THREE.Object3D(),
    haloGlow: { material: { opacity: 1 } },
  };
  // les os d'animation du jeu partent d'une rotation nulle = pose de repos : on garde la
  // hauteur des hanches du modèle (le dieu procédural a ses hanches à 2,5 m)
  const hipsY = byName.hips.position.y;
  const baseHips = byName.hips.position.clone();
  const hipsProxy = new Proxy(byName.hips, {});
  B.hips = hipsProxy;
  const t0 = performance.now() / 1000;
  function updateWings(spread, flap, time) {
    // pas d'ailes : les objets cosmiques gravitent, plus vite et plus loin en vol
    const k = 0.6 + spread * 0.6;
    for (const o of orbits) {
      const a = o.phase + time * o.speed * (1 + Math.abs(flap) * 0.5);
      const ch = P('chest');
      o.bone.position.set(ch.x + Math.cos(a) * o.r * k, o.c.y + Math.sin(time * 0.8 + o.label) * 0.25, ch.z + Math.sin(a) * o.r * k);
      o.bone.rotation.y = time * 0.7;
    }
    byName.hips.position.y = baseHips.y;
  }
  // trou noir animé sur le visage : disque d'accrétion, horizon, matière aspirée
  let bh = null;
  if (rig.blackhole) {
    const c = new THREE.Vector3(...rig.blackhole.center).multiplyScalar(scale);
    const R = rig.blackhole.radius * scale;
    bh = makeBlackHole(R);
    bh.position.copy(c.sub(P('head')).add(new THREE.Vector3(0, 0, R * 0.25)));
    byName.head.add(bh);
  }
  const updateAll = (spread, flap, time) => { updateWings(spread, flap, time); if (bh) bh.userData.tick(time); };
  return { root, bones: B, wings: {}, feathers: null, updateWings: updateAll, mats: { glow: mat }, imported: true, hipsY };
}

// Trou noir : horizon noir, anneau de photons, disque d'accrétion tourbillonnant (shader),
// halo violet, et grains de lumière qui spiralent vers le centre.
function makeBlackHole(R) {
  const g = new THREE.Group();
  const disk = new THREE.Mesh(new THREE.PlaneGeometry(R * 4.4, R * 4.4), new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide,
    uniforms: { uTime: { value: 0 } },
    vertexShader: 'varying vec2 vUv; void main(){ vUv = uv * 2.0 - 1.0; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
    fragmentShader: `uniform float uTime; varying vec2 vUv;
      float h(vec2 p){ return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
      float n(vec2 p){ vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f); return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y); }
      void main(){
        float r = length(vUv) * 2.2; float a = atan(vUv.y, vUv.x);
        float swirl = a + uTime * 1.6 + 2.2 / max(r, 0.3);
        float bands = n(vec2(swirl * 3.0, r * 4.0 - uTime * 0.8)) * 0.7 + n(vec2(swirl * 7.0, r * 9.0)) * 0.3;
        float disk = smoothstep(1.05, 1.2, r) * smoothstep(2.2, 1.3, r) * (0.3 + bands * 1.2);
        float ring = exp(-pow((r - 1.1) / 0.09, 2.0)) * 2.2;            // anneau de photons
        float halo = exp(-r * 1.4) * 0.3 * smoothstep(1.0, 1.3, r) * smoothstep(2.2, 1.6, r);
        vec3 col = mix(vec3(1.6, 0.55, 0.12), vec3(1.4, 1.2, 1.0), smoothstep(1.2, 2.2, r) * 0.0 + bands * 0.35) * disk
          + vec3(1.0, 0.92, 0.8) * ring + vec3(0.5, 0.25, 1.0) * halo;
        col *= 1.0 - smoothstep(1.02, 0.98, r); // rien dans l'horizon
        gl_FragColor = vec4(col, 1.0);
      }`,
  }));
  const horizon = new THREE.Mesh(new THREE.CircleGeometry(R * 1.05, 32), new THREE.MeshBasicMaterial({ color: 0x000000 }));
  horizon.position.z = R * 0.02;
  disk.position.z = R * 0.04;
  g.add(horizon, disk);
  const light = new THREE.PointLight(0x8a5cff, 0, R * 40, 2);
  g.add(light);
  g.userData.tick = (t) => {
    disk.material.uniforms.uTime.value = t;
    light.intensity = 25 + Math.sin(t * 3.1) * 8;
    disk.rotation.z = Math.sin(t * 0.4) * 0.15;
    // matière aspirée
    if (G.fx && Math.random() < 0.35) {
      const p = new THREE.Vector3(); g.getWorldPosition(p);
      const a = Math.random() * 6.28, d = R * (4 + Math.random() * 4);
      const off = new THREE.Vector3(Math.cos(a) * d, Math.sin(a) * d, 0).applyQuaternion(g.getWorldQuaternion(new THREE.Quaternion()));
      const s = p.clone().add(off), v = off.clone().multiplyScalar(-1.8).add(new THREE.Vector3(-off.y, off.x, 0).multiplyScalar(1.2));
      G.fx.add.emit(s.x, s.y, s.z, v.x, v.y, v.z, R * 0.5, 0.5, 1, 0.6, 0.3, 0.9, 0.5, 0, 0);
    }
  };
  return g;
}
