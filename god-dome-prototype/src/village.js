import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { boxUV } from './textures.js';
import { mulberry32 } from './noise.js';
import { VILLAGE } from './terrain.js';

const rnd = mulberry32(77);

// Matériaux partagés du village
export function makeVillageMaterials(T) {
  const M = {
    plaster: new THREE.MeshStandardMaterial({ map: T.plaster, roughness: 0.92 }),
    wood: new THREE.MeshStandardMaterial({ map: T.wood, roughness: 0.85 }),
    timber: new THREE.MeshStandardMaterial({ map: T.wood, color: '#8a6a52', roughness: 0.9 }),
    roof: new THREE.MeshStandardMaterial({ map: T.roof, roughness: 0.8 }),
    slate: new THREE.MeshStandardMaterial({ map: T.slate, roughness: 0.65, metalness: 0.05 }),
    stone: new THREE.MeshStandardMaterial({ map: T.stone, roughness: 0.95 }),
    glass: new THREE.MeshStandardMaterial({ color: '#2a2016', emissive: '#ffb259', emissiveIntensity: 0.25, roughness: 0.3 }),
    lamp: new THREE.MeshStandardMaterial({ color: '#fff1c8', emissive: '#ffc070', emissiveIntensity: 1.2 }),
    cloth: new THREE.MeshStandardMaterial({ color: '#c94f3a', roughness: 0.95, side: THREE.DoubleSide }),
    sail: new THREE.MeshStandardMaterial({ color: '#efe4c8', roughness: 0.95, side: THREE.DoubleSide }),
    metal: new THREE.MeshStandardMaterial({ color: '#c9a04a', roughness: 0.35, metalness: 0.85 }),
    leafy: new THREE.MeshStandardMaterial({ color: '#4e8f2e', roughness: 0.9 }),
  };
  // auvent rayé du marché
  const c = document.createElement('canvas'); c.width = 64; c.height = 8; const g = c.getContext('2d');
  for (let i = 0; i < 8; i++) { g.fillStyle = i % 2 ? '#f3ead7' : '#3f8fa8'; g.fillRect(i * 8, 0, 8, 8); }
  const st = new THREE.CanvasTexture(c); st.colorSpace = THREE.SRGBColorSpace; st.wrapS = st.wrapT = THREE.RepeatWrapping; st.magFilter = THREE.NearestFilter;
  M.stripes = new THREE.MeshStandardMaterial({ map: st, roughness: 0.9, side: THREE.DoubleSide });
  return M;
}

// --- collecteur de géométries par matériau, fusionnées en un mesh par matériau ---
class Builder {
  constructor() { this.parts = new Map(); this.colliders = []; }
  add(mat, geo, matrix) {
    if (matrix) geo.applyMatrix4(matrix);
    if (geo.index) geo = geo.toNonIndexed();
    if (!geo.attributes.uv) geo.setAttribute('uv', new THREE.Float32BufferAttribute(new Float32Array(geo.attributes.position.count * 2), 2));
    if (!this.parts.has(mat)) this.parts.set(mat, []);
    this.parts.get(mat).push(geo);
  }
  box(mat, w, h, d, x, y, z, { rx = 0, ry = 0, rz = 0, uv = 0.25, collide = false } = {}) {
    const g = boxUV(new THREE.BoxGeometry(w, h, d), w, h, d, uv);
    const m = new THREE.Matrix4().compose(new THREE.Vector3(x, y, z), new THREE.Quaternion().setFromEuler(new THREE.Euler(rx, ry, rz)), new THREE.Vector3(1, 1, 1));
    this.add(mat, g, m);
    if (collide) this.colliders.push({ x, z, hw: w / 2, hd: d / 2, rot: ry, y0: y - h / 2, y1: y + h / 2 });
  }
  build(name) {
    const group = new THREE.Group(); group.name = name;
    for (const [mat, geos] of this.parts) {
      const mesh = new THREE.Mesh(mergeGeometries(geos), mat);
      mesh.castShadow = mat !== this.noShadow; mesh.receiveShadow = true;
      group.add(mesh);
    }
    return group;
  }
}

// transforme les colliders locaux en colliders monde
function placeColliders(local, px, pz, rotY, out, baseY) {
  const c = Math.cos(rotY), s = Math.sin(rotY);
  for (const k of local) {
    out.push({ x: px + k.x * c + k.z * s, z: pz - k.x * s + k.z * c, hw: k.hw, hd: k.hd, rot: rotY + k.rot, y0: baseY + k.y0, y1: baseY + k.y1 });
  }
}

/**
 * Maison à colombages : murs réels (porte ouverte, intérieur meublé), toit à deux pans, cheminée,
 * fenêtres éclairées, volets peints. Retourne { group, colliders, floor }.
 */
export function buildHouse(M, terrain, { x, z, rotY, w = 7, d = 6, H = 3.6, roofH = 2.6, roof = 'roof', shutter = '#4f7fa0', seed = 1 }) {
  const r = mulberry32(seed);
  const b = new Builder();
  const t = 0.3, door = 1.4, doorH = 2.3, bury = 2.0;
  const wallMid = (H - bury) / 2, wallH = H + bury;
  const ext = M.plaster, roofMat = roof === 'slate' ? M.slate : M.roof;

  // murs (avec ouverture de porte en façade)
  const sideW = (w - door) / 2;
  b.box(ext, w, wallH, t, 0, wallMid, -d / 2 + t / 2, { collide: true });
  b.box(ext, t, wallH, d, -w / 2 + t / 2, wallMid, 0, { collide: true });
  b.box(ext, t, wallH, d, w / 2 - t / 2, wallMid, 0, { collide: true });
  b.box(ext, sideW, wallH, t, -w / 2 + sideW / 2, wallMid, d / 2 - t / 2, { collide: true });
  b.box(ext, sideW, wallH, t, w / 2 - sideW / 2, wallMid, d / 2 - t / 2, { collide: true });
  b.box(ext, door, H - doorH, t, 0, doorH + (H - doorH) / 2, d / 2 - t / 2);
  // soubassement en pierre
  const pb = 0.7, pt = t + 0.12;
  b.box(M.stone, w + 0.12, pb + bury, pt, 0, (pb - bury) / 2, -d / 2 + t / 2);
  b.box(M.stone, pt, pb + bury, d + 0.12, -w / 2 + t / 2, (pb - bury) / 2, 0);
  b.box(M.stone, pt, pb + bury, d + 0.12, w / 2 - t / 2, (pb - bury) / 2, 0);
  b.box(M.stone, sideW + 0.06, pb + bury, pt, -w / 2 + sideW / 2 - 0.03, (pb - bury) / 2, d / 2 - t / 2);
  b.box(M.stone, sideW + 0.06, pb + bury, pt, w / 2 - sideW / 2 + 0.03, (pb - bury) / 2, d / 2 - t / 2);
  // colombages : poteaux d'angle, sablières, croix de Saint-André
  const tw = 0.24;
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) b.box(M.timber, tw, H, tw, sx * (w / 2 - 0.02), H / 2, sz * (d / 2 - 0.02));
  b.box(M.timber, w + 0.1, tw, tw, 0, H, d / 2 + 0.02); b.box(M.timber, w + 0.1, tw, tw, 0, H, -d / 2 - 0.02);
  b.box(M.timber, tw, tw, d + 0.1, w / 2 + 0.02, H, 0); b.box(M.timber, tw, tw, d + 0.1, -w / 2 - 0.02, H, 0);
  if (H > 4.5) { b.box(M.timber, w + 0.1, tw * 0.8, tw, 0, H * 0.5, d / 2 + 0.02); b.box(M.timber, tw, tw * 0.8, d + 0.1, w / 2 + 0.02, H * 0.5, 0); b.box(M.timber, tw, tw * 0.8, d + 0.1, -w / 2 - 0.02, H * 0.5, 0); }
  for (const sx of [-1, 1]) {
    const len = Math.hypot(d * 0.45, H * 0.8);
    b.box(M.timber, tw * 0.8, len, tw * 0.8, sx * (w / 2 + 0.03), H * 0.5, -d * 0.22, { rx: Math.atan2(d * 0.45, H * 0.8) });
  }
  // encadrement de porte + porte ouverte
  b.box(M.timber, 0.18, doorH, 0.4, -door / 2 - 0.05, doorH / 2, d / 2 - t / 2);
  b.box(M.timber, 0.18, doorH, 0.4, door / 2 + 0.05, doorH / 2, d / 2 - t / 2);
  b.box(M.timber, door + 0.3, 0.2, 0.4, 0, doorH + 0.05, d / 2 - t / 2);
  {
    const g = boxUV(new THREE.BoxGeometry(door - 0.1, doorH - 0.05, 0.08), door, doorH, 0.08);
    g.translate((door - 0.1) / 2, (doorH - 0.05) / 2, 0);
    const m = new THREE.Matrix4().makeRotationY(-1.35).premultiply(new THREE.Matrix4().makeTranslation(-door / 2 + 0.02, 0, d / 2 - 0.1));
    b.add(M.wood, g, m);
  }
  // fenêtres + volets + jardinières
  M._shutters = M._shutters || {};
  const shutterMat = M._shutters[shutter] || (M._shutters[shutter] = new THREE.MeshStandardMaterial({ color: shutter, roughness: 0.8, map: M.wood.map }));
  const windows = [];
  const fy = H > 4.5 ? [1.5, H * 0.5 + 1.2] : [1.6];
  for (const yy of fy) {
    windows.push([w / 4 + 0.4, yy, 'front'], [-w / 4 - 0.4, yy, 'front'], [0, yy, 'left'], [0, yy, 'right'], [-w / 4, yy, 'back'], [w / 4, yy, 'back']);
  }
  windows.forEach(([o, yy, side]) => {
    if (side === 'front' && Math.abs(o) < door) return;
    const ww = 0.95, wh = 1.1;
    const place = (lx, lz, ry) => {
      const m = new THREE.Matrix4().compose(new THREE.Vector3(lx, yy, lz), new THREE.Quaternion().setFromEuler(new THREE.Euler(0, ry, 0)), new THREE.Vector3(1, 1, 1));
      const add = (mat, gw, gh, gd, ox, oy, oz) => { const g = new THREE.BoxGeometry(gw, gh, gd); g.translate(ox, oy, oz); b.add(mat, g, m.clone()); };
      add(M.glass, ww, wh, 0.05, 0, 0, 0.02);
      add(M.timber, ww + 0.2, 0.14, 0.18, 0, wh / 2, 0.06); add(M.timber, ww + 0.3, 0.14, 0.26, 0, -wh / 2, 0.08);
      add(M.timber, 0.07, wh, 0.07, 0, 0, 0.06); add(M.timber, ww, 0.07, 0.07, 0, 0.05, 0.06);
      add(shutterMat, ww / 2, wh, 0.06, -ww * 0.82, 0, 0.12); add(shutterMat, ww / 2, wh, 0.06, ww * 0.82, 0, 0.12);
      if (r() > 0.35) {
        add(M.wood, ww + 0.2, 0.22, 0.3, 0, -wh / 2 - 0.2, 0.22);
        for (let k = 0; k < 5; k++) { const fl = new THREE.IcosahedronGeometry(0.11 + r() * 0.06, 0); fl.translate(-ww / 2 + 0.1 + k * ww / 4.5, -wh / 2 - 0.02, 0.24); b.add(r() > 0.5 ? M.cloth : M.leafy, fl, m.clone()); }
      }
    };
    if (side === 'front') place(o, d / 2 + 0.01, 0);
    if (side === 'back') place(o, -d / 2 - 0.01, Math.PI);
    if (side === 'left') place(-w / 2 - 0.01, o, -Math.PI / 2);
    if (side === 'right') place(w / 2 + 0.01, o, Math.PI / 2);
  });

  // pignons
  const gable = new THREE.Shape([new THREE.Vector2(-w / 2, 0), new THREE.Vector2(w / 2, 0), new THREE.Vector2(0, roofH)]);
  for (const sz of [-1, 1]) {
    const g = new THREE.ExtrudeGeometry(gable, { depth: t, bevelEnabled: false });
    const uv = g.attributes.uv; for (let i = 0; i < uv.count; i++) uv.setXY(i, uv.getX(i) * 0.25, uv.getY(i) * 0.25);
    g.translate(0, H, sz > 0 ? d / 2 - t : -d / 2);
    b.add(ext, g);
  }
  // toit à deux pans
  const ov = 0.7, slab = Math.hypot(w / 2, roofH) + ov, ang = Math.atan2(roofH, w / 2), rt = 0.24;
  for (const sx of [-1, 1]) {
    const cx = sx * (w / 4 + Math.cos(ang) * ov / 2 - 0.0), cy = H + roofH / 2 - Math.sin(ang) * ov / 2;
    const nx = -sx * Math.sin(ang) * 0, ny = Math.cos(ang);
    b.box(roofMat, slab, rt, d + 2 * 0.55, cx - sx * Math.sin(ang) * rt * 0.5 * 0 + nx, cy + ny * rt * 0.5, 0, { rz: -sx * ang, uv: 0.33 });
    // rive en bois
    b.box(M.timber, slab, 0.2, 0.16, cx, cy + 0.02, d / 2 + 0.6, { rz: -sx * ang });
    b.box(M.timber, slab, 0.2, 0.16, cx, cy + 0.02, -d / 2 - 0.6, { rz: -sx * ang });
  }
  b.box(M.timber, 0.34, 0.34, d + 1.3, 0, H + roofH + 0.12, 0);
  // cheminée
  const chx = (r() > 0.5 ? 1 : -1) * w * 0.22;
  b.box(M.stone, 0.9, roofH + 1.6, 0.9, chx, H + roofH * 0.55 + 0.4, -d * 0.18, { uv: 0.5 });
  b.box(M.stone, 1.1, 0.2, 1.1, chx, H + roofH * 1.05 + 0.85, -d * 0.18);

  // intérieur : plancher, table, tabouret, lit, étagère, lanterne
  b.box(M.wood, w - 2 * t, 0.14, d - 2 * t, 0, -0.07, 0, { uv: 0.4 });
  b.box(M.wood, 1.6, 0.1, 0.9, w * 0.15, 0.85, -d * 0.1);
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) b.box(M.timber, 0.1, 0.8, 0.1, w * 0.15 + sx * 0.7, 0.4, -d * 0.1 + sz * 0.35);
  b.box(M.wood, 0.5, 0.5, 0.5, w * 0.15 + 1.2, 0.25, -d * 0.1);
  b.box(M.timber, 1.0, 0.5, 2.0, -w / 2 + t + 0.6, 0.25, -d / 2 + t + 1.1);
  b.box(M.cloth, 1.0, 0.18, 1.6, -w / 2 + t + 0.6, 0.55, -d / 2 + t + 1.25);
  b.box(M.wood, 1.6, 0.08, 0.4, w * 0.1, 1.9, -d / 2 + t + 0.22);
  b.box(M.lamp, 0.25, 0.35, 0.25, 0, H - 0.6, 0);
  b.box(M.lamp, 0.22, 0.3, 0.22, -w / 2 - 0.35, 2.4, d / 2 + 0.35);
  b.noShadow = M.lamp;

  const group = b.build('house');
  // au sol : niveau du plancher = point le plus haut sous l'emprise
  let y = -Infinity;
  for (const [lx, lz] of [[0, 0], [-w / 2, -d / 2], [w / 2, -d / 2], [-w / 2, d / 2], [w / 2, d / 2]]) {
    const c = Math.cos(rotY), s = Math.sin(rotY);
    y = Math.max(y, terrain.heightAt(x + lx * c + lz * s, z - lx * s + lz * c));
  }
  y += 0.12;
  group.position.set(x, y, z); group.rotation.y = rotY;
  const colliders = [];
  placeColliders(b.colliders, x, z, rotY, colliders, y);
  const floor = { x, z, hw: w / 2 - 0.1, hd: d / 2 - 0.1, rot: rotY, y };
  return { group, colliders, floor, w, d };
}

function buildTower(M, terrain, x, z, rotY) {
  const b = new Builder();
  const W = 4.6, H = 13;
  b.box(M.stone, W + 0.6, 2.2, W + 0.6, 0, 0.1, 0, { uv: 0.4 });
  b.box(M.plaster, W, H, W, 0, H / 2, 0, { collide: true });
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) b.box(M.stone, 0.6, H, 0.6, sx * W / 2, H / 2, sz * W / 2, { uv: 0.4 });
  b.box(M.timber, W + 0.3, 0.3, W + 0.3, 0, H, 0);
  // beffroi ajouré
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) b.box(M.timber, 0.4, 3.2, 0.4, sx * (W / 2 - 0.2), H + 1.6, sz * (W / 2 - 0.2));
  b.box(M.timber, W + 0.4, 0.35, W + 0.4, 0, H + 3.3, 0);
  b.box(M.wood, W + 0.2, 0.2, W + 0.2, 0, H + 0.1, 0);
  const bell = new THREE.CylinderGeometry(0.45, 0.9, 1.3, 14, 1, true); bell.translate(0, H + 1.7, 0); b.add(M.metal, bell);
  const roofG = new THREE.ConeGeometry(W * 0.95, 6.5, 4, 1); roofG.rotateY(Math.PI / 4); roofG.translate(0, H + 3.5 + 3.25, 0); b.add(M.slate, roofG);
  const spire = new THREE.CylinderGeometry(0.04, 0.08, 1.6, 5); spire.translate(0, H + 10.4, 0); b.add(M.metal, spire);
  // horloge / blason
  const clock = new THREE.CylinderGeometry(0.8, 0.8, 0.1, 20); clock.rotateX(Math.PI / 2); clock.translate(0, H - 2.4, W / 2 + 0.06); b.add(M.metal, clock);
  b.box(M.wood, 1.4, 2.4, 0.12, 0, 1.2, W / 2 + 0.05);
  const group = b.build('tower');
  const y = terrain.heightAt(x, z);
  group.position.set(x, y, z); group.rotation.y = rotY;
  const colliders = []; placeColliders(b.colliders, x, z, rotY, colliders, y);
  return { group, colliders };
}

function buildWell(M, terrain, x, z) {
  const b = new Builder();
  const ring = new THREE.CylinderGeometry(1.3, 1.4, 1.0, 18, 1, true); ring.translate(0, 0.5, 0); b.add(M.stone, ring);
  const inner = new THREE.CylinderGeometry(1.05, 1.05, 1.0, 18, 1, true); inner.scale(-1, 1, 1); inner.translate(0, 0.5, 0); b.add(M.stone, inner);
  const lip = new THREE.TorusGeometry(1.18, 0.16, 6, 20); lip.rotateX(Math.PI / 2); lip.translate(0, 1.0, 0); b.add(M.stone, lip);
  const water = new THREE.CircleGeometry(1.05, 18); water.rotateX(-Math.PI / 2); water.translate(0, 0.35, 0);
  b.add(new THREE.MeshStandardMaterial({ color: '#1d4a55', roughness: 0.1, metalness: 0.3 }), water);
  for (const sx of [-1, 1]) b.box(M.timber, 0.18, 2.6, 0.18, sx * 1.2, 1.3, 0);
  b.box(M.timber, 2.7, 0.14, 0.14, 0, 2.3, 0);
  b.box(M.roof, 1.6, 0.12, 2.0, -0.6, 2.85, 0, { rz: 0.6 }); b.box(M.roof, 1.6, 0.12, 2.0, 0.6, 2.85, 0, { rz: -0.6 });
  const group = b.build('well');
  group.position.set(x, terrain.heightAt(x, z), z);
  return { group, colliders: [{ x, z, hw: 1.4, hd: 1.4, rot: 0, y0: -10, y1: 100 }] };
}

function buildStall(M, terrain, x, z, rotY) {
  const b = new Builder();
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) b.box(M.timber, 0.14, 2.6, 0.14, sx * 1.4, 1.3, sz * 0.8);
  b.box(M.wood, 3.0, 0.9, 0.9, 0, 0.45, 0.45, { collide: true });
  const canopy = new THREE.PlaneGeometry(3.4, 2.2, 1, 1); canopy.rotateX(-Math.PI / 2 + 0.3); canopy.translate(0, 2.75, 0.1);
  const uv = canopy.attributes.uv; for (let i = 0; i < uv.count; i++) uv.setXY(i, uv.getX(i) * 6, uv.getY(i));
  b.add(M.stripes, canopy);
  // marchandises : fruits et tonneaux
  const fruitMat = new THREE.MeshStandardMaterial({ color: '#e0a32c', roughness: 0.6 });
  for (let i = 0; i < 14; i++) { const f = new THREE.IcosahedronGeometry(0.12, 1); f.translate(-1.2 + (i % 7) * 0.38, 0.98, 0.2 + Math.floor(i / 7) * 0.4); b.add(i % 3 ? fruitMat : M.cloth, f); }
  const barrel = new THREE.CylinderGeometry(0.35, 0.3, 0.8, 12); barrel.translate(1.9, 0.4, 0.6); b.add(M.wood, barrel);
  const group = b.build('stall');
  const y = terrain.heightAt(x, z);
  group.position.set(x, y, z); group.rotation.y = rotY;
  const colliders = []; placeColliders(b.colliders, x, z, rotY, colliders, y);
  return { group, colliders };
}

function buildWindmill(M, terrain, x, z) {
  const b = new Builder();
  const tower = new THREE.CylinderGeometry(2.0, 3.0, 10, 10); tower.translate(0, 5, 0); b.add(M.plaster, tower);
  const base = new THREE.CylinderGeometry(3.2, 3.4, 1.6, 10); base.translate(0, 0.2, 0); b.add(M.stone, base);
  const cap = new THREE.ConeGeometry(2.7, 3.2, 10); cap.translate(0, 11.6, 0); b.add(M.roof, cap);
  b.box(M.wood, 1.3, 2.2, 0.2, 0, 1.6, 2.9, { rx: -0.08 });
  const group = b.build('windmill');
  const y = terrain.heightAt(x, z);
  group.position.set(x, y, z);
  // ailes (animées)
  const blades = new THREE.Group(); blades.position.set(0, 9.6, 2.4);
  const hub = new THREE.Mesh(new THREE.CylinderGeometry(0.35, 0.35, 0.8, 10).rotateX(Math.PI / 2), M.timber); blades.add(hub);
  for (let i = 0; i < 4; i++) {
    const arm = new THREE.Group(); arm.rotation.z = i * Math.PI / 2;
    const spar = new THREE.Mesh(new THREE.BoxGeometry(0.22, 7.5, 0.16), M.timber); spar.position.y = 3.75; arm.add(spar);
    const sail = new THREE.Mesh(new THREE.PlaneGeometry(1.5, 5.6), M.sail); sail.position.set(0.85, 4.4, 0.1); sail.rotation.y = 0.2; arm.add(sail);
    for (let k = 0; k < 6; k++) { const rung = new THREE.Mesh(new THREE.BoxGeometry(1.7, 0.06, 0.06), M.timber); rung.position.set(0.85, 1.8 + k * 1.05, 0.06); arm.add(rung); }
    blades.add(arm);
  }
  blades.traverse(o => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } });
  // orienter vers le centre du monde
  const yaw = Math.atan2(-x, -z);
  group.rotation.y = yaw;
  group.add(blades);
  return { group, blades, colliders: [{ x, z, hw: 3, hd: 3, rot: 0, y0: -10, y1: 100 }] };
}

function buildLanterns(M, terrain, T, points) {
  const b = new Builder();
  const glows = [];
  points.forEach(([x, z]) => {
    const y = terrain.heightAt(x, z);
    b.box(M.timber, 0.18, 3.4, 0.18, x, y + 1.6, z);
    b.box(M.timber, 0.9, 0.12, 0.12, x + 0.4, y + 3.2, z);
    b.box(M.lamp, 0.32, 0.42, 0.32, x + 0.8, y + 2.85, z);
    b.box(M.timber, 0.42, 0.08, 0.42, x + 0.8, y + 3.1, z);
    glows.push(new THREE.Vector3(x + 0.8, y + 2.85, z));
  });
  b.noShadow = M.lamp;
  const group = b.build('lanterns');
  // halos additifs (visibles surtout au crépuscule et la nuit)
  const gm = new THREE.SpriteMaterial({ fog: false, map: T.glow, color: '#ffbb66', blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, opacity: 0.0 });
  glows.forEach(p => { const s = new THREE.Sprite(gm); s.position.copy(p); s.scale.setScalar(3.2); group.add(s); });
  group.userData.glowMat = gm;
  return group;
}

function buildBridge(M, terrain, river, bed) {
  // pont à l'endroit où le chemin croise la rivière
  let bi = 0, bd = Infinity;
  river.forEach((p, i) => { const d = Math.hypot(p.x - 7, p.y - 8); if (d < bd) { bd = d; bi = i; } });
  const p = river[bi], q = river[Math.min(bi + 2, river.length - 1)];
  const along = Math.atan2(q.x - p.x, q.y - p.y);
  const rotY = along + Math.PI / 2; // perpendiculaire au courant
  const L = 15, Wd = 2.6, rise = 1.0;
  const bankY = Math.max(terrain.heightAt(p.x + Math.sin(rotY) * L / 2, p.y + Math.cos(rotY) * L / 2), terrain.heightAt(p.x - Math.sin(rotY) * L / 2, p.y - Math.cos(rotY) * L / 2));
  const y0 = Math.max(bed[bi] + 2.0, bankY - 0.15);
  const b = new Builder();
  const N = 18;
  const archY = u => y0 + rise * (1 - (2 * u - 1) ** 2);
  for (let i = 0; i < N; i++) {
    const u = (i + 0.5) / N, zz = -L / 2 + u * L;
    const yy = archY(u);
    const slope = Math.atan2(archY(u + 0.02) - archY(u - 0.02), 0.04 * L);
    b.box(M.wood, Wd, 0.14, L / N - 0.04, 0, yy - 0.07, zz, { rx: -slope, uv: 0.5 });
  }
  for (const sx of [-1, 1]) {
    for (let i = 0; i <= 6; i++) { const u = i / 6, zz = -L / 2 + u * L; b.box(M.timber, 0.14, 1.1, 0.14, sx * (Wd / 2 - 0.05), archY(u) + 0.45, zz); }
    for (let i = 0; i < 6; i++) {
      const u0 = i / 6, u1 = (i + 1) / 6, ya = archY(u0) + 0.95, yb = archY(u1) + 0.95;
      b.box(M.timber, 0.1, 0.1, Math.hypot(L / 6, yb - ya), sx * (Wd / 2 - 0.05), (ya + yb) / 2, -L / 2 + (u0 + u1) / 2 * L, { rx: -Math.atan2(yb - ya, L / 6) });
    }
    // piles
    b.box(M.stone, 0.6, 4, 0.6, sx * (Wd / 2 - 0.2), y0 - 2.2, -L * 0.18); b.box(M.stone, 0.6, 4, 0.6, sx * (Wd / 2 - 0.2), y0 - 2.2, L * 0.18);
  }
  const group = b.build('bridge');
  group.position.set(p.x, 0, p.y); group.rotation.y = rotY - Math.PI / 2 + Math.PI / 2;
  // le tablier est orienté sur l'axe local z ; rotY oriente cet axe perpendiculairement à la rivière
  group.rotation.y = rotY;
  const surface = { x: p.x, z: p.y, hw: Wd / 2, hd: L / 2, rot: rotY, heightAt: lz => archY((lz + L / 2) / L) };
  const colliders = [-1, 1].map(sx => ({ x: p.x + Math.cos(rotY) * sx * (Wd / 2 + 0.05), z: p.y - Math.sin(rotY) * sx * (Wd / 2 + 0.05), hw: 0.08, hd: L / 2 - 0.8, rot: rotY, y0: -100, y1: 100 }));
  return { group, surface, colliders };
}

export function buildVillage(M, terrain, T) {
  const group = new THREE.Group(); group.name = 'village';
  const colliders = [], floors = [], houses = [];
  const V = VILLAGE;
  const shutters = ['#3f7fa6', '#5d8f4a', '#a8503d', '#3e5f8a', '#7a5ba0', '#2f7d7a'];
  const layout = [
    // angle (deg), distance, largeur, profondeur, hauteur murs, toit
    [20, 16, 7.5, 6, 3.6, 'roof'], [52, 17, 8, 6.5, 5.8, 'roof'], [78, 16, 6.5, 5.5, 3.4, 'slate'],
    [128, 16, 7, 6, 3.6, 'roof'], [160, 17, 8.5, 6.5, 6.0, 'roof'], [188, 16, 6.5, 5.5, 3.5, 'slate'],
    [245, 17, 7.5, 6, 3.8, 'roof'], [322, 16, 7, 6, 3.6, 'roof'],
  ];
  layout.forEach(([a, dist, w, d, H, roof], i) => {
    const ar = a * Math.PI / 180;
    const x = V.x + Math.cos(ar) * dist, z = V.y + Math.sin(ar) * dist;
    const rotY = Math.atan2(V.x - x, V.y - z) + (rnd() - 0.5) * 0.2;
    const h = buildHouse(M, terrain, { x, z, rotY, w, d, H, roofH: 2.4 + w * 0.08, roof, shutter: shutters[i % shutters.length], seed: i + 1 });
    group.add(h.group); colliders.push(...h.colliders); floors.push(h.floor); houses.push(h);
  });
  // maisons isolées
  [[-6, 30, 6.5, 5.5, 3.4, 'roof'], [-50, -16, 7, 6, 3.6, 'slate'], [-44, 38, 6.5, 5.5, 3.4, 'roof']].forEach(([x, z, w, d, H, roof], i) => {
    const rotY = Math.atan2(V.x - x, V.y - z);
    const h = buildHouse(M, terrain, { x, z, rotY, w, d, H, roofH: 2.6, roof, shutter: shutters[(i + 3) % shutters.length], seed: 40 + i });
    group.add(h.group); colliders.push(...h.colliders); floors.push(h.floor); houses.push(h);
  });
  const tower = buildTower(M, terrain, V.x + Math.cos(5.2) * 12, V.y + Math.sin(5.2) * 12, -0.4);
  group.add(tower.group); colliders.push(...tower.colliders);
  const well = buildWell(M, terrain, V.x, V.y); group.add(well.group); colliders.push(...well.colliders);
  const stall = buildStall(M, terrain, V.x + 5.5, V.y + 4.5, Math.atan2(-5.5, -4.5)); group.add(stall.group); colliders.push(...stall.colliders);
  const stall2 = buildStall(M, terrain, V.x - 6, V.y + 3.5, Math.atan2(6, -3.5)); group.add(stall2.group); colliders.push(...stall2.colliders);
  const mill = buildWindmill(M, terrain, -58, -6); group.add(mill.group); colliders.push(...mill.colliders);
  const bridge = buildBridge(M, terrain, terrain.river, terrain.bed); group.add(bridge.group); colliders.push(...bridge.colliders);
  const lanternPts = [[-22, 10], [-14, 12.5], [-4, 9.5], [-36, 9], [-28, 26], [-31, 40], [-42, 15], [-20, 22], [14, 6]];
  const lanterns = buildLanterns(M, terrain, T, lanternPts); group.add(lanterns);
  mergeStatic(group, o => { let p = o; while (p) { if (p === mill.blades) return true; p = p.parent; } return false; });
  return { group, colliders, floors, surfaces: [bridge.surface], mill, lanterns, houses };
}

// Fusionne toutes les géométries statiques du village par matériau : ~180 objets → ~25 appels de dessin.
function mergeStatic(root, keep) {
  root.updateMatrixWorld(true);
  const byMat = new Map(), remove = [];
  root.traverse(o => {
    if (!o.isMesh || keep(o)) return;
    let g = o.geometry.clone().applyMatrix4(o.matrixWorld);
    if (g.index) g = g.toNonIndexed();
    if (!g.attributes.uv) g.setAttribute('uv', new THREE.Float32BufferAttribute(new Float32Array(g.attributes.position.count * 2), 2));
    for (const k of Object.keys(g.attributes)) if (!['position', 'normal', 'uv'].includes(k)) g.deleteAttribute(k);
    if (!byMat.has(o.material)) byMat.set(o.material, { geos: [], cast: o.castShadow });
    byMat.get(o.material).geos.push(g);
    remove.push(o);
  });
  remove.forEach(o => o.parent.remove(o));
  const inv = new THREE.Matrix4().copy(root.matrixWorld).invert();
  for (const [mat, { geos, cast }] of byMat) {
    const m = new THREE.Mesh(mergeGeometries(geos), mat);
    m.geometry.applyMatrix4(inv);
    m.castShadow = cast; m.receiveShadow = true;
    root.add(m);
  }
}
