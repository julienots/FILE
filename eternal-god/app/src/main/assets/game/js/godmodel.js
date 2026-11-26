// Modèle 3D du Dieu Céleste : armure dorée et ivoire, casque fermé orné, auréole,
// grandes ailes de plumes animées individuellement (une instance par plume).
import * as THREE from 'three';
import { canvasTexture, lerp } from './core.js';

function featherTexture() {
  return canvasTexture(128, 512, (ctx, w, h) => {
    ctx.clearRect(0, 0, w, h);
    // silhouette de plume : base étroite, ventre large, pointe arrondie
    const path = new Path2D();
    path.moveTo(w / 2, h * 0.995);
    path.bezierCurveTo(w * 0.62, h * 0.85, w * 0.98, h * 0.55, w * 0.9, h * 0.22);
    path.bezierCurveTo(w * 0.85, h * 0.07, w * 0.62, h * 0.0, w * 0.5, h * 0.005);
    path.bezierCurveTo(w * 0.36, h * 0.02, w * 0.12, h * 0.1, w * 0.1, h * 0.3);
    path.bezierCurveTo(w * 0.05, h * 0.55, w * 0.38, h * 0.85, w / 2, h * 0.995);
    ctx.save(); ctx.clip(path);
    const g = ctx.createLinearGradient(0, h, 0, 0);
    g.addColorStop(0, '#fffaf0'); g.addColorStop(0.55, '#fbf3e2'); g.addColorStop(0.82, '#f3dca6'); g.addColorStop(1, '#e2b453');
    ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
    // barbes de la plume
    ctx.strokeStyle = 'rgba(170,140,90,0.22)'; ctx.lineWidth = 1;
    for (let y = h * 0.98; y > 0; y -= 7) {
      ctx.beginPath(); ctx.moveTo(w / 2, y); ctx.quadraticCurveTo(w * 0.3, y - 10, 0, y - 34); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(w / 2, y); ctx.quadraticCurveTo(w * 0.7, y - 10, w, y - 34); ctx.stroke();
    }
    // échancrures
    ctx.globalCompositeOperation = 'destination-out';
    for (let i = 0; i < 4; i++) { ctx.beginPath(); const y = h * (0.25 + i * 0.16); ctx.moveTo(i % 2 ? w : 0, y); ctx.lineTo(w / 2, y - 22); ctx.lineWidth = 2; ctx.stroke(); }
    ctx.restore();
    // rachis doré
    ctx.strokeStyle = '#c9952e'; ctx.lineWidth = 3;
    ctx.beginPath(); ctx.moveTo(w / 2, h); ctx.quadraticCurveTo(w * 0.53, h * 0.5, w * 0.5, h * 0.03); ctx.stroke();
  });
}

function lathe(points, segs = 24) {
  return new THREE.LatheGeometry(points.map(([x, y]) => new THREE.Vector2(x, y)), segs);
}

export function createGodMaterials() {
  return {
    gold: new THREE.MeshStandardMaterial({ color: 0xd9a540, metalness: 1, roughness: 0.22 }),
    goldDark: new THREE.MeshStandardMaterial({ color: 0x9c6a22, metalness: 1, roughness: 0.34 }),
    ivory: new THREE.MeshStandardMaterial({ color: 0xcfc2a6, metalness: 0.45, roughness: 0.32 }),
    under: new THREE.MeshStandardMaterial({ color: 0x3b3346, metalness: 0.5, roughness: 0.55 }),
    cloth: new THREE.MeshStandardMaterial({ color: 0xd8ccb2, roughness: 0.8, side: THREE.DoubleSide }),
    glow: new THREE.MeshStandardMaterial({ color: 0xffe6a8, emissive: 0xffc96a, emissiveIntensity: 5, roughness: 0.3 }),
    feather: new THREE.MeshStandardMaterial({ map: featherTexture(), color: 0xe6dcc8, alphaTest: 0.45, side: THREE.DoubleSide, roughness: 0.6, metalness: 0.0 }),
  };
}

// Construit le dieu (≈ 5,2 m). Renvoie l'objet racine et son squelette procédural.
export function buildGodModel(mats = createGodMaterials()) {
  const M = mats;
  const root = new THREE.Group();
  const B = {};
  const add = (parent, geo, mat, x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1) => {
    const m = new THREE.Mesh(geo, mat);
    m.position.set(x, y, z); m.rotation.set(rx, ry, rz); m.scale.set(sx, sy, sz);
    m.castShadow = true; parent.add(m); return m;
  };
  const joint = (parent, name, x, y, z) => { const g = new THREE.Group(); g.position.set(x, y, z); parent.add(g); B[name] = g; return g; };

  // ---------- Bassin ----------
  const hips = joint(root, 'hips', 0, 2.5, 0);
  add(hips, new THREE.SphereGeometry(0.42, 20, 12), M.under, 0, -0.05, 0, 0, 0, 0, 1.15, 0.7, 0.85);
  add(hips, new THREE.TorusGeometry(0.47, 0.07, 10, 32), M.gold, 0, 0.1, 0, Math.PI / 2, 0, 0, 1.05, 0.82, 1);
  add(hips, new THREE.CylinderGeometry(0.11, 0.11, 0.06, 6), M.glow, 0, 0.1, 0.4, Math.PI / 2);
  // tassettes (plaques) autour des hanches
  for (let i = -2; i <= 2; i++) {
    const a = i * 0.42;
    const plate = add(hips, new THREE.BoxGeometry(0.34, 0.5, 0.05), i === 0 ? M.gold : M.ivory, Math.sin(a) * 0.5, -0.2, Math.cos(a) * 0.44, 0.15, a, 0);
    add(plate, new THREE.BoxGeometry(0.36, 0.05, 0.07), M.gold, 0, -0.25, 0);
  }
  // pagne de tissu sacré (avant et arrière)
  const clothGeo = new THREE.PlaneGeometry(0.4, 1.05, 1, 8);
  clothGeo.translate(0, -0.52, 0);
  const cp = clothGeo.attributes.position;
  for (let i = 0; i < cp.count; i++) { const y = cp.getY(i); cp.setZ(i, Math.sin(-y * 1.6) * 0.12); cp.setX(i, cp.getX(i) * (1 + -y * 0.25)); }
  clothGeo.computeVertexNormals();
  B.clothF = add(hips, clothGeo, M.cloth, 0, -0.08, 0.43);
  B.clothB = add(hips, clothGeo, M.cloth, 0, -0.08, -0.4, 0, Math.PI, 0);
  add(B.clothF, new THREE.BoxGeometry(0.42, 0.05, 0.04), M.gold, 0, -1.03, 0.1);

  // ---------- Torse ----------
  const spine = joint(hips, 'spine', 0, 0.3, 0);
  // abdomen segmenté
  for (let i = 0; i < 3; i++) add(spine, new THREE.CylinderGeometry(0.4 - i * 0.01 + 0.02, 0.4, 0.15, 20), i === 1 ? M.goldDark : M.ivory, 0, 0.08 + i * 0.15, 0.02, 0, 0, 0, 1.08, 1, 0.78);
  const chest = joint(spine, 'chest', 0, 0.55, 0);
  // cuirasse : profil tourné, aplati en profondeur
  const plate = lathe([[0.0, -0.25], [0.48, -0.22], [0.6, 0.05], [0.62, 0.35], [0.55, 0.6], [0.3, 0.72], [0.0, 0.74]], 28);
  add(chest, plate, M.ivory, 0, 0, 0.0, 0, 0, 0, 1.05, 1, 0.72);
  // bordure dorée et pectoraux ornés
  add(chest, new THREE.TorusGeometry(0.6, 0.045, 8, 40), M.gold, 0, 0.32, 0, Math.PI / 2, 0, 0, 1.06, 0.74, 1);
  add(chest, new THREE.TorusGeometry(0.36, 0.035, 8, 32, Math.PI), M.gold, 0, 0.48, 0.3, 0.25, 0, Math.PI);
  for (const s of [-1, 1]) {
    add(chest, new THREE.SphereGeometry(0.26, 16, 10, 0, Math.PI * 2, 0, Math.PI / 2), M.gold, s * 0.24, 0.33, 0.28, Math.PI / 2 - 0.25, 0, s * -0.2, 1, 0.45, 0.85);
  }
  // cœur lumineux et rayons gravés
  B.core = add(chest, new THREE.OctahedronGeometry(0.13, 0), M.glow, 0, 0.24, 0.47, 0, 0, 0, 1, 1.4, 0.6);
  for (let i = 0; i < 8; i++) {
    const a = i / 8 * Math.PI * 2;
    add(chest, new THREE.BoxGeometry(0.025, 0.22, 0.02), M.glow, Math.sin(a) * 0.24, 0.24 + Math.cos(a) * 0.24, 0.43 - Math.abs(Math.sin(a)) * 0.05, 0, 0, -a);
  }
  // filigrane doré : chevrons et arêtes de la cuirasse
  for (let i = 0; i < 3; i++) for (const s of [-1, 1]) add(chest, new THREE.BoxGeometry(0.34 - i * 0.06, 0.035, 0.03), M.gold, s * (0.13 - i * 0.02), -0.05 - i * 0.13, 0.42 - i * 0.02, 0, s * 0.35, s * 0.5);
  add(chest, new THREE.BoxGeometry(0.05, 0.5, 0.04), M.gold, 0, -0.02, 0.44, -0.15, 0, 0);
  for (const s of [-1, 1]) add(chest, new THREE.BoxGeometry(0.04, 0.6, 0.04), M.gold, s * 0.5, 0.2, 0.22, 0, s * 0.6, s * -0.12);
  // col
  add(chest, new THREE.CylinderGeometry(0.28, 0.36, 0.22, 20), M.gold, 0, 0.72, 0);

  // ---------- Tête et casque fermé ----------
  const neck = joint(chest, 'neck', 0, 0.82, 0);
  add(neck, new THREE.CylinderGeometry(0.15, 0.18, 0.25, 12), M.under, 0, 0.08, 0);
  const head = joint(neck, 'head', 0, 0.22, 0);
  add(head, new THREE.SphereGeometry(0.3, 24, 18), M.ivory, 0, 0.22, -0.02, 0, 0, 0, 0.95, 1.12, 1.08);
  // masque facial doré
  add(head, new THREE.SphereGeometry(0.3, 24, 16, -Math.PI * 0.42, Math.PI * 0.84, Math.PI * 0.25, Math.PI * 0.55), M.gold, 0, 0.18, 0.025, 0, 0, 0, 0.98, 1.1, 1.12);
  // fente du visage en T, lumineuse
  add(head, new THREE.BoxGeometry(0.34, 0.035, 0.05), M.glow, 0, 0.25, 0.32);
  add(head, new THREE.BoxGeometry(0.04, 0.2, 0.05), M.glow, 0, 0.14, 0.33);
  // crête frontale et ornements
  const crest = new THREE.Shape();
  crest.moveTo(0, 0); crest.quadraticCurveTo(0.08, 0.35, -0.02, 0.62); crest.quadraticCurveTo(-0.1, 0.4, -0.16, 0.0); crest.lineTo(0, 0);
  const crestGeo = new THREE.ExtrudeGeometry(crest, { depth: 0.04, bevelEnabled: true, bevelThickness: 0.015, bevelSize: 0.015, bevelSegments: 1 });
  add(head, crestGeo, M.gold, 0.02, 0.42, 0.06, 0, Math.PI / 2, -0.35);
  // ailettes latérales du casque
  for (const s of [-1, 1]) {
    for (let i = 0; i < 3; i++) {
      const f = add(head, new THREE.ConeGeometry(0.05, 0.55 - i * 0.12, 4), M.gold, s * 0.3, 0.3 + i * 0.05, -0.05 - i * 0.08, -0.5 - i * 0.25, 0, s * (-1.2 + i * 0.1));
      f.scale.z = 0.3;
    }
    add(head, new THREE.CylinderGeometry(0.08, 0.08, 0.04, 16), M.goldDark, s * 0.28, 0.2, 0.02, 0, 0, Math.PI / 2);
  }
  // couronne de pointes
  for (let i = 0; i < 7; i++) {
    const a = (i - 3) * 0.32;
    add(head, new THREE.ConeGeometry(0.035, 0.22 - Math.abs(i - 3) * 0.03, 4), M.gold, Math.sin(a) * 0.27, 0.47, Math.cos(a) * 0.2 - 0.02, Math.cos(a) * 0.3, 0, -Math.sin(a) * 0.5);
  }

  // ---------- Auréole ----------
  const halo = joint(chest, 'halo', 0, 1.25, -0.55);
  const haloMat = new THREE.MeshStandardMaterial({ color: 0xffd98a, metalness: 1, roughness: 0.2, emissive: 0xffb84a, emissiveIntensity: 1.6 });
  add(halo, new THREE.TorusGeometry(0.85, 0.05, 10, 64), haloMat);
  add(halo, new THREE.TorusGeometry(0.68, 0.018, 6, 64), M.glow);
  const rays = joint(halo, 'haloRays', 0, 0, 0);
  for (let i = 0; i < 24; i++) {
    const a = i / 24 * Math.PI * 2, L = i % 2 ? 0.16 : 0.3;
    add(rays, new THREE.BoxGeometry(0.03, L, 0.02), i % 2 ? M.gold : M.glow, Math.cos(a) * (0.93 + L / 2), Math.sin(a) * (0.93 + L / 2), 0, 0, 0, a - Math.PI / 2);
  }
  const haloGlow = new THREE.Sprite(new THREE.SpriteMaterial({ map: canvasTexture(128, 128, (ctx, w) => {
    const g = ctx.createRadialGradient(w / 2, w / 2, w * 0.3, w / 2, w / 2, w / 2);
    g.addColorStop(0, 'rgba(255,220,140,0)'); g.addColorStop(0.62, 'rgba(255,214,130,0.55)'); g.addColorStop(0.72, 'rgba(255,200,110,0.18)'); g.addColorStop(1, 'rgba(255,190,90,0)');
    ctx.fillStyle = g; ctx.fillRect(0, 0, w, w);
  }), blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
  haloGlow.scale.setScalar(2.6);
  halo.add(haloGlow);
  B.haloGlow = haloGlow;

  // ---------- Bras ----------
  for (const s of [-1, 1]) {
    const k = s < 0 ? 'L' : 'R';
    const sh = joint(chest, 'shoulder' + k, s * 0.66, 0.5, -0.02);
    // épaulières superposées
    add(sh, new THREE.SphereGeometry(0.36, 20, 12, 0, Math.PI * 2, 0, Math.PI * 0.55), M.gold, s * 0.08, 0.06, 0, 0, 0, s * -0.35, 1.1, 0.9, 1.05);
    add(sh, new THREE.SphereGeometry(0.33, 20, 10, 0, Math.PI * 2, 0, Math.PI * 0.5), M.ivory, s * 0.16, -0.08, 0, 0, 0, s * -0.7, 1.05, 0.8, 1);
    add(sh, new THREE.TorusGeometry(0.33, 0.03, 6, 28, Math.PI), M.gold, s * 0.17, -0.1, 0, Math.PI / 2, 0, s * -0.7 + (s < 0 ? Math.PI : 0));
    for (let i = 0; i < 3; i++) add(sh, new THREE.ConeGeometry(0.05, 0.42 - i * 0.08, 4), M.gold, s * (0.05 + i * 0.12), 0.32 - i * 0.04, -0.12, -0.3, 0, s * (-0.25 - i * 0.25)).scale.z = 0.35;
    const upper = joint(sh, 'upper' + k, s * 0.12, -0.12, 0);
    add(upper, new THREE.CylinderGeometry(0.15, 0.13, 0.75, 14), M.under, 0, -0.38, 0);
    add(upper, new THREE.CylinderGeometry(0.19, 0.17, 0.48, 14), M.ivory, 0, -0.32, 0);
    const fore = joint(upper, 'fore' + k, 0, -0.78, 0);
    add(fore, new THREE.SphereGeometry(0.16, 14, 10), M.gold, 0, 0, 0);
    add(fore, new THREE.CylinderGeometry(0.17, 0.13, 0.65, 14), M.gold, 0, -0.36, 0);
    add(fore, new THREE.CylinderGeometry(0.19, 0.19, 0.06, 14), M.goldDark, 0, -0.08, 0);
    add(fore, new THREE.BoxGeometry(0.04, 0.4, 0.06), M.glow, 0, -0.38, 0.15);
    const hand = joint(fore, 'hand' + k, 0, -0.72, 0);
    add(hand, new THREE.BoxGeometry(0.2, 0.22, 0.12), M.gold, 0, -0.08, 0.0);
    for (let f = 0; f < 4; f++) add(hand, new THREE.BoxGeometry(0.042, 0.16, 0.06), M.goldDark, -0.075 + f * 0.05, -0.25, 0.0, 0.25, 0, 0);
    add(hand, new THREE.BoxGeometry(0.05, 0.12, 0.06), M.goldDark, s * -0.12, -0.12, 0.04, 0, 0, s * 0.5);
    B['handGlow' + k] = add(hand, new THREE.SphereGeometry(0.06, 8, 6), M.glow, 0, -0.14, 0.07);
  }

  // ---------- Jambes ----------
  for (const s of [-1, 1]) {
    const k = s < 0 ? 'L' : 'R';
    const thigh = joint(hips, 'thigh' + k, s * 0.25, -0.12, 0);
    add(thigh, new THREE.CylinderGeometry(0.22, 0.17, 1.1, 14), M.under, 0, -0.55, 0);
    add(thigh, new THREE.CylinderGeometry(0.25, 0.2, 0.8, 14), M.ivory, 0, -0.42, 0.02, 0, 0, 0, 1, 1, 1.05);
    add(thigh, new THREE.BoxGeometry(0.05, 0.6, 0.04), M.gold, 0, -0.42, 0.25);
    const shin = joint(thigh, 'shin' + k, 0, -1.12, 0);
    add(shin, new THREE.SphereGeometry(0.19, 14, 10), M.gold, 0, 0, 0.05);
    add(shin, new THREE.ConeGeometry(0.08, 0.38, 4), M.gold, 0, 0.18, 0.18, 0.4, 0, 0).scale.x = 0.4;
    add(shin, new THREE.CylinderGeometry(0.2, 0.14, 1.0, 14), M.gold, 0, -0.52, 0.02);
    add(shin, new THREE.CylinderGeometry(0.21, 0.21, 0.07, 14), M.goldDark, 0, -0.15, 0.02);
    const foot = joint(shin, 'foot' + k, 0, -1.06, 0);
    add(foot, new THREE.BoxGeometry(0.26, 0.16, 0.42), M.gold, 0, -0.04, 0.08);
    add(foot, new THREE.ConeGeometry(0.13, 0.26, 4), M.gold, 0, -0.06, 0.38, Math.PI / 2, Math.PI / 4, 0).scale.set(1, 1, 0.5);
    add(foot, new THREE.BoxGeometry(0.28, 0.04, 0.5), M.goldDark, 0, -0.12, 0.1);
  }

  // ---------- Ailes ----------
  const wingRoot = joint(chest, 'wingRoot', 0, 0.42, -0.42);
  add(wingRoot, new THREE.BoxGeometry(0.7, 0.4, 0.18), M.goldDark, 0, 0, 0.05);
  add(wingRoot, new THREE.SphereGeometry(0.12, 10, 8), M.glow, 0, 0.02, -0.05);
  const feathers = [];
  const wings = {};
  for (const s of [-1, 1]) {
    const k = s < 0 ? 'L' : 'R';
    const base = joint(wingRoot, 'wing' + k, s * 0.3, 0.05, -0.05);
    const arm = joint(base, 'wingArm' + k, 0, 0, 0);
    add(arm, new THREE.CylinderGeometry(0.11, 0.08, 1.7, 8), M.ivory, s * 0.85, 0, 0, 0, 0, Math.PI / 2);
    add(arm, new THREE.SphereGeometry(0.16, 10, 8), M.gold, 0, 0, 0);
    const fore = joint(arm, 'wingFore' + k, s * 1.7, 0, 0);
    add(fore, new THREE.SphereGeometry(0.11, 10, 8), M.gold, 0, 0, 0);
    add(fore, new THREE.CylinderGeometry(0.08, 0.06, 2.0, 8), M.ivory, s * 1.0, 0, 0, 0, 0, Math.PI / 2);
    const handW = joint(fore, 'wingHand' + k, s * 2.0, 0, 0);
    add(handW, new THREE.SphereGeometry(0.08, 8, 6), M.gold, 0, 0, 0);
    add(handW, new THREE.CylinderGeometry(0.05, 0.03, 1.4, 6), M.ivory, s * 0.7, 0, 0, 0, 0, Math.PI / 2);
    wings[k] = { base, arm, fore, hand: handW, side: s };
    // plumes : [os, position le long de l'os, longueur, largeur, angle, couche]
    const P = (bone, len0) => ({ bone, len: len0 });
    const boneArm = P(arm, 1.7), boneFore = P(fore, 2.0), boneHand = P(handW, 1.4);
    // rémiges primaires (main) : de la verticale vers l'extérieur
    for (let i = 0; i < 11; i++) {
      const t = i / 10;
      feathers.push({ wing: k, side: s, bone: boneHand, at: 0.1 + t * 0.95, L: 2.3 + t * 0.9, W: 0.42, ang: 0.35 + t * 1.05, z: -0.02 - t * 0.012, layer: 0, idx: i });
    }
    // rémiges secondaires (avant-bras)
    for (let i = 0; i < 13; i++) {
      const t = i / 12;
      feathers.push({ wing: k, side: s, bone: boneFore, at: 0.02 + t * 0.98, L: 2.5 - Math.abs(t - 0.6) * 0.6, W: 0.44, ang: 0.02 + t * 0.32, z: -0.04, layer: 0, idx: 11 + i });
    }
    // tertiaires (bras)
    for (let i = 0; i < 8; i++) {
      const t = i / 7;
      feathers.push({ wing: k, side: s, bone: boneArm, at: 0.15 + t * 0.85, L: 1.5 + t * 0.9, W: 0.4, ang: -0.25 + t * 0.25, z: -0.05, layer: 0, idx: 24 + i });
    }
    // couvertures (deux rangs plus courts par-dessus)
    for (const [bone, n, L, z, a0, a1] of [[boneHand, 8, 1.15, 0.03, 0.3, 0.9], [boneFore, 11, 1.1, 0.04, 0.0, 0.3], [boneArm, 7, 0.95, 0.05, -0.2, 0.05], [boneFore, 11, 0.62, 0.08, 0.0, 0.25], [boneArm, 7, 0.55, 0.09, -0.1, 0.0], [boneHand, 6, 0.6, 0.07, 0.2, 0.6]]) {
      for (let i = 0; i < n; i++) {
        const t = i / (n - 1);
        feathers.push({ wing: k, side: s, bone, at: 0.04 + t * 0.92, L: L * (0.9 + Math.sin(t * 3) * 0.1), W: 0.36, ang: lerp(a0, a1, t), z, layer: 1, idx: i });
      }
    }
  }
  const fGeo = new THREE.PlaneGeometry(1, 1, 1, 3);
  fGeo.translate(0, -0.5, 0); // base à l'origine, pointe vers le bas
  const fp = fGeo.attributes.position;
  for (let i = 0; i < fp.count; i++) { const y = fp.getY(i); fp.setZ(i, Math.sin(-y * Math.PI) * 0.05); }
  fGeo.computeVertexNormals();
  const fMesh = new THREE.InstancedMesh(fGeo, M.feather, feathers.length);
  fMesh.castShadow = true;
  fMesh.frustumCulled = false;
  root.add(fMesh);

  // ---------- Pose et animation ----------
  const inv = new THREE.Matrix4(), mB = new THREE.Matrix4(), mF = new THREE.Matrix4();
  const q = new THREE.Quaternion(), e = new THREE.Euler(), v = new THREE.Vector3(), sc = new THREE.Vector3();

  // spread : 0 replié, 1 déployé ; flap : -1..1 battement ; ruffle : agitation
  function updateWings(spread, flap, time, ruffle = 0.2) {
    for (const k of ['L', 'R']) {
      const w = wings[k], s = w.side;
      const up = flap * 0.75;
      w.base.rotation.set(0, s * lerp(0.85, 0.25, spread) , 0);
      w.arm.rotation.set(0, s * lerp(0.35, -0.05, spread) + s * flap * -0.25, s * (lerp(-1.05, 0.42, spread) + up));
      w.fore.rotation.set(0, s * lerp(-0.6, 0.12, spread) + s * flap * -0.2, s * (lerp(2.35, -0.12, spread) - up * 0.35));
      w.hand.rotation.set(0, s * lerp(0.5, 0.1, spread), s * (lerp(-2.3, -0.08, spread) - up * 0.45));
    }
    root.updateMatrixWorld(true);
    inv.copy(root.matrixWorld).invert();
    for (let i = 0; i < feathers.length; i++) {
      const f = feathers[i], s = f.side;
      mB.multiplyMatrices(inv, f.bone.bone.matrixWorld);
      // éventail : les plumes se resserrent quand l'aile est repliée
      const fan = lerp(0.18, 1, spread);
      const flutter = Math.sin(time * (7 + f.idx * 0.37) + f.idx) * 0.03 * ruffle * (f.layer ? 0.6 : 1);
      const lag = flap * 0.12 * (f.L / 2.5);
      v.set(s * f.at * f.bone.len, 0, f.z);
      e.set(lag * -0.6 + flutter, s * 0.08, s * (f.ang * fan) + flutter * s);
      q.setFromEuler(e);
      const len = f.L * lerp(0.85, 1, spread);
      sc.set(f.W, len, 1);
      mF.compose(v, q, sc);
      mB.multiply(mF);
      fMesh.setMatrixAt(i, mB);
    }
    fMesh.instanceMatrix.needsUpdate = true;
  }

  root.traverse((o) => { if (o.isMesh) o.castShadow = true; });
  return { root, bones: B, wings, feathers: fMesh, updateWings, mats: M };
}
