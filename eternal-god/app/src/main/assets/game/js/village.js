// Villages : plusieurs hameaux générés avec rues organiques, place, bâtiments orientés vers la rue,
// champs clôturés, accessoires scannés. Chaque bâtiment a une vie : dégâts, effondrement, ruines,
// reconstruction par les habitants.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { G, WORLD, mulberry32, clamp, lerp, smooth, rand, emit, noise } from './core.js';
import { REGIONS } from './terrain.js';
import { makeBuilding, windmillSails, buildingMaterial, Builder } from './buildings.js';

export const PROPS = ['barrel_03', 'wooden_crate_02', 'wooden_bucket_01', 'wine_barrel_01', 'wooden_barrels_01', 'stone_fire_pit', 'wooden_picnic_table', 'wooden_lantern_01'];

export async function loadProps(onEach) {
  const gl = new GLTFLoader(), out = {};
  for (const p of PROPS) {
    const g = await gl.loadAsync(`models/props/${p}.glb`);
    const parts = [];
    g.scene.updateMatrixWorld(true);
    g.scene.traverse((o) => { if (o.isMesh) { const geo = o.geometry.clone(); geo.applyMatrix4(o.matrixWorld); parts.push({ geo, mat: o.material }); } });
    const bb = new THREE.Box3(); for (const pt of parts) { pt.geo.computeBoundingBox(); bb.union(pt.geo.boundingBox); }
    out[p] = { parts, size: bb.getSize(new THREE.Vector3()), minY: bb.min.y };
    if (onEach) onEach();
  }
  return out;
}

// Définition des lieux habités
export const VILLAGES = [
  { id: 0, name: 'Vallon d\'Aube', x: 0, z: 0, r: 125, streets: 5, houses: 52, plaza: 22, cobble: true, main: true },
  { id: 1, name: 'Port-Lac', x: 0, z: 0, r: 60, streets: 3, houses: 14, plaza: 10, port: true },
  { id: 2, name: 'Clairière des Anciens', x: -330, z: 150, r: 62, streets: 3, houses: 12, plaza: 11, forest: true },
  { id: 3, name: 'Hameau du Col', x: -150, z: -300, r: 55, streets: 3, houses: 10, plaza: 9, mountain: true },
];

export class Villages {
  constructor(scene, props, tex) {
    this.scene = scene;
    this.props = props;
    this.mat = buildingMaterial(tex);
    this.buildings = [];
    this.occupied = [];       // [x, z, rayon] : zones à garder libres (arbres, rochers)
    this.propLists = {};
    this.fenceGeos = [];
    this.villages = VILLAGES.map((v) => ({ ...v, buildings: [], streetsPts: [], spots: {}, worship: 0.15, fear: 0.05, alive: true }));
  }

  // Avant la construction du maillage du terrain : choix et aplanissement des sites
  prepare() {
    const T = G.terrain;
    // Port-Lac : sur la rive ouest du lac, en direction du bourg
    const L = REGIONS.lake;
    const port = this.villages[1];
    const dir = new THREE.Vector2(-L.x, -L.z).normalize();
    for (let d = 60; d < 320; d += 3) {
      const x = L.x + dir.x * d, z = L.z + dir.y * d;
      if (T.heightAt(x, z) > WORLD.water + 2.2) { port.x = x + dir.x * 34; port.z = z + dir.y * 34; port.shore = { x, z }; port.lakeDir = dir.clone().negate(); break; }
    }
    for (const v of this.villages) {
      if (v.main) continue;
      // plateau doux pour le hameau
      let sum = 0, n = 0;
      for (let a = 0; a < 6.28; a += 0.5) for (const r of [0, v.r * 0.5, v.r]) { sum += T.heightAt(v.x + Math.cos(a) * r, v.z + Math.sin(a) * r); n++; }
      const y = Math.max(WORLD.water + 3, sum / n);
      T.flatten(v.x, v.z, v.r * 0.8, y);
    }
  }

  isFree(x, z, r) {
    for (const [ox, oz, or] of this.occupied) if (Math.hypot(x - ox, z - oz) < r + or) return false;
    return true;
  }

  build() {
    const T = G.terrain;
    for (const v of this.villages) this.layout(v, mulberry32(1000 + v.id * 77));
    T.markDirtyArea(0, 0, WORLD.size);
    // accessoires instanciés
    for (const [name, list] of Object.entries(this.propLists)) {
      const P = this.props[name];
      if (!P || !list.length) continue;
      for (const part of P.parts) {
        const m = new THREE.InstancedMesh(part.geo, part.mat, list.length);
        const M = new THREE.Matrix4(), q = new THREE.Quaternion();
        list.forEach(([x, y, z, ry, s], i) => { q.setFromAxisAngle(new THREE.Vector3(0, 1, 0), ry); M.compose(new THREE.Vector3(x, y - P.minY * s, z), q, new THREE.Vector3(s, s, s)); m.setMatrixAt(i, M); });
        m.castShadow = true; m.receiveShadow = true;
        this.scene.add(m);
      }
    }
    if (this.fenceGeos.length) {
      const f = new THREE.Mesh(mergeGeometries(this.fenceGeos), this.mat);
      f.castShadow = true; f.receiveShadow = true;
      this.scene.add(f);
    }
  }

  addProp(name, x, z, ry = Math.random() * 6.28, scale = 1) {
    (this.propLists[name] = this.propLists[name] || []).push([x, G.terrain.heightAt(x, z), z, ry, scale]);
  }

  place(v, type, x, z, ry, rnd, extra = {}) {
    const T = G.terrain;
    const b = makeBuilding(type, rnd);
    const rad = Math.max(b.w, b.d) * 0.55;
    if (!extra.force && !this.isFree(x, z, rad + 0.6)) return null;
    let y = T.heightAt(x, z);
    if (!extra.noFlatten) {
      // sol moyen sous l'emprise
      let s = 0; for (const [dx, dz] of [[0, 0], [rad, 0], [-rad, 0], [0, rad], [0, -rad]]) s += T.heightAt(x + dx, z + dz);
      y = Math.max(s / 5, extra.minY ?? -1e9);
      T.flatten(x, z, rad, y);
    }
    if (extra.y !== undefined) y = extra.y;
    const mesh = new THREE.Mesh(b.geo, this.mat);
    mesh.position.set(x, y, z); mesh.rotation.y = ry;
    mesh.castShadow = true; mesh.receiveShadow = true;
    mesh.updateMatrixWorld(true);
    this.scene.add(mesh);
    const local = (p) => p ? new THREE.Vector3(p[0], p[1] ?? 0, p.length > 2 ? p[2] : p[1]) : null;
    const door = b.door ? new THREE.Vector3(b.door[0], 0, b.door[1]).applyAxisAngle(new THREE.Vector3(0, 1, 0), ry).add(mesh.position) : mesh.position.clone();
    door.y = T.heightAt(door.x, door.z);
    const rec = {
      id: this.buildings.length, village: v.id, type, mesh, x, z, y, ry, w: b.w, d: b.d, h: b.h, rad, pieces: b.pieces,
      hp: type === 'temple' ? 900 : type === 'tower' ? 600 : type === 'bridge' || type === 'dock' ? 2000 : 260 + b.w * b.d * 2,
      state: 'intact', door, residents: [], workers: [], rebuild: 0, info: b,
    };
    rec.maxHp = rec.hp;
    if (b.chimney) rec.chimney = new THREE.Vector3(...b.chimney).applyAxisAngle(new THREE.Vector3(0, 1, 0), ry).add(mesh.position);
    if (b.forge) rec.forge = new THREE.Vector3(...b.forge).applyAxisAngle(new THREE.Vector3(0, 1, 0), ry).add(mesh.position);
    if (b.hub) { const s = new THREE.Mesh(windmillSails(), this.mat); s.position.set(...b.hub); mesh.add(s); rec.sails = s; s.castShadow = true; }
    this.buildings.push(rec); v.buildings.push(rec);
    this.occupied.push([x, z, rad + 0.4]);
    if (!extra.noPaint) T.paintRect(x, z, b.w + 2, b.d + 2, ry, 1);
    return rec;
  }

  layout(v, rnd) {
    const T = G.terrain;
    const C = new THREE.Vector2(v.x, v.z);
    // place centrale
    T.paintDisc(v.x, v.z, v.plaza, v.cobble ? 3 : 1);
    this.occupied.push([v.x, v.z, v.plaza * 0.55]);
    v.spots.plaza = new THREE.Vector3(v.x, T.heightAt(v.x, v.z), v.z);
    // rues : branches légèrement courbes depuis la place
    const streets = [];
    const a0 = rnd() * Math.PI * 2;
    for (let s = 0; s < v.streets; s++) {
      let a = a0 + s / v.streets * Math.PI * 2 + (rnd() - 0.5) * 0.5;
      if (v.port) a = Math.atan2(v.lakeDir.y, v.lakeDir.x) + (s === 0 ? Math.PI : Math.PI / 2 * (rnd() < 0.5 ? 1 : -1));
      const pts = [];
      let x = v.x + Math.cos(a) * v.plaza * 0.8, z = v.z + Math.sin(a) * v.plaza * 0.8;
      for (let d = 0; d < v.r; d += 6) {
        pts.push(new THREE.Vector3(x, 0, z));
        a += noise.noise(x * 0.02 + v.id, z * 0.02) * 0.12;
        x += Math.cos(a) * 6; z += Math.sin(a) * 6;
        if (T.heightAt(x, z) < WORLD.water + 1.5) break;
      }
      for (let i = 1; i < pts.length; i++) T.paintPath(pts[i - 1].x, pts[i - 1].z, pts[i].x, pts[i].z, v.main ? 5 : 4);
      T.paintPath(v.x, v.z, pts[0].x, pts[0].z, v.main ? 5 : 4);
      streets.push(pts);
    }
    v.streetsPts = streets;
    const face = (x, z, tx, tz) => Math.atan2(tx - x, tz - z);
    // édifices de la place
    if (v.main) {
      this.place(v, 'well', v.x, v.z, 0, rnd, { force: true });
      const spot = (ang, dist) => [v.x + Math.cos(ang) * dist, v.z + Math.sin(ang) * dist];
      // le temple ouvre sur la place, entre deux rues
      const gaps = streets.map((p) => Math.atan2(p[0].z - v.z, p[0].x - v.x)).sort((a, b) => a - b);
      let best = 0, bestGap = 0;
      for (let i = 0; i < gaps.length; i++) { const g = ((gaps[(i + 1) % gaps.length] - gaps[i]) + Math.PI * 2) % (Math.PI * 2); if (g > bestGap) { bestGap = g; best = gaps[i] + g / 2; } }
      const [tx, tz] = spot(best, v.plaza + 13);
      v.temple = this.place(v, 'temple', tx, tz, face(tx, tz, v.x, v.z), rnd);
      const others = [['tavern', best + bestGap * 0.3 + 0.9], ['forge', best - bestGap * 0.3 - 0.9]];
      for (const [type, ang] of others) {
        for (let k = 0; k < 8; k++) {
          const [x, z] = spot(ang + k * 0.35, v.plaza + 10);
          const r = this.place(v, type, x, z, face(x, z, v.x, v.z), rnd);
          if (r) { v[type] = r; break; }
        }
      }
      // étals du marché
      for (let i = 0; i < 6; i++) {
        const ang = best + Math.PI + (i - 2.5) * 0.42;
        const [x, z] = spot(ang, v.plaza * 0.62);
        const st = this.place(v, 'stall', x, z, face(x, z, v.x, v.z), rnd, { noPaint: true, noFlatten: true });
        if (st) (v.stalls = v.stalls || []).push(st);
      }
      for (let i = 0; i < 4; i++) { const [x, z] = spot(i * 1.57 + 0.4, v.plaza * 0.9); this.addProp('wooden_lantern_01', x, z, 0, 1.6); }
    } else {
      this.addProp('stone_fire_pit', v.x, v.z, 0, 1.2);
      this.occupied.push([v.x, v.z, 3]);
    }
    // maisons le long des rues, porte vers la rue
    let placed = 0, tries = 0;
    while (placed < v.houses && tries < 900) {
      tries++;
      const st = streets[Math.floor(rnd() * streets.length)];
      if (!st || st.length < 2) continue;
      const i = 1 + Math.floor(rnd() * (st.length - 1));
      const p = st[i], q = st[i - 1];
      const dx = p.x - q.x, dz = p.z - q.z, L = Math.hypot(dx, dz) || 1;
      const side = rnd() < 0.5 ? 1 : -1;
      const off = 7 + rnd() * 2.5;
      const x = p.x - dz / L * off * side, z = p.z + dx / L * off * side;
      if (T.heightAt(x, z) < WORLD.water + 1.2) continue;
      if (T.normalAt(x, z).y < 0.86 && !v.mountain) continue;
      const type = v.main && rnd() < 0.08 ? 'stable' : (!v.main && rnd() < 0.2 ? 'farmhouse' : 'house');
      const r = this.place(v, type, x, z, face(x, z, p.x, p.z), rnd);
      if (!r) continue;
      placed++;
      // accessoires contre les murs
      const n = 1 + Math.floor(rnd() * 3);
      for (let k = 0; k < n; k++) {
        const along = (rnd() - 0.5) * r.w * 0.8, out = r.d / 2 + 0.9;
        const lp = new THREE.Vector3(along, 0, rnd() < 0.5 ? out : -out).applyAxisAngle(new THREE.Vector3(0, 1, 0), r.ry);
        const prop = ['barrel_03', 'wooden_crate_02', 'wooden_bucket_01', 'wine_barrel_01'][Math.floor(rnd() * 4)];
        this.addProp(prop, r.x + lp.x, r.z + lp.z, rnd() * 6.28, prop === 'wooden_crate_02' ? 1.3 : 1.1);
      }
    }
    // tours de guet, moulin, écurie et champs pour le bourg
    if (v.main) {
      for (const k of [0, 2]) { const st = streets[k]; if (st) { const e = st[st.length - 1]; this.place(v, 'tower', e.x + 9, e.z + 6, 0, rnd); } }
      v.tavern && this.addProp('wooden_picnic_table', v.tavern.door.x + 2, v.tavern.door.z + 2, v.tavern.ry, 1.4);
      v.tavern && this.addProp('wooden_barrels_01', v.tavern.x + Math.cos(v.tavern.ry) * (v.tavern.w / 2 + 1.6), v.tavern.z - Math.sin(v.tavern.ry) * (v.tavern.w / 2 + 1.6), v.tavern.ry, 1.2);
      v.forge && this.addProp('barrel_03', v.forge.door.x - 1.5, v.forge.door.z, 0, 1.1);
      // moulin sur la hauteur la plus proche
      let bx = 0, bz = 0, by = -1e9;
      for (let a = 0; a < 6.28; a += 0.3) { const x = v.x + Math.cos(a) * (v.r + 20), z = v.z + Math.sin(a) * (v.r + 20), y = T.heightAt(x, z); if (y > by && y > WORLD.water + 3 && this.isFree(x, z, 8)) { by = y; bx = x; bz = z; } }
      v.mill = this.place(v, 'windmill', bx, bz, face(bx, bz, v.x, v.z), rnd);
    }
    // champs cultivés et clôturés autour du village
    const nFields = v.main ? 6 : v.mountain ? 1 : 2;
    v.fields = [];
    for (let f = 0, t2 = 0; f < nFields && t2 < 60; t2++) {
      const a = rnd() * 6.28, d = v.r + 12 + rnd() * 40;
      const x = v.x + Math.cos(a) * d, z = v.z + Math.sin(a) * d, w = 26 + rnd() * 18, dd = 18 + rnd() * 14, ry = a + Math.PI / 2;
      if (!this.isFree(x, z, Math.max(w, dd) * 0.6) || T.heightAt(x, z) < WORLD.water + 1.5 || T.normalAt(x, z).y < 0.93) continue;
      T.paintRect(x, z, w, dd, ry, 2);
      this.occupied.push([x, z, Math.max(w, dd) * 0.6]);
      this.fence(x, z, w + 2, dd + 2, ry);
      v.fields.push({ x, z, w, d: dd, ry });
      f++;
    }
    // ponton et barques pour le port
    if (v.port && v.shore) {
      const ry = Math.atan2(v.lakeDir.x, v.lakeDir.y);
      const dx = v.shore.x + v.lakeDir.x * 9, dz = v.shore.z + v.lakeDir.y * 9;
      v.dock = this.place(v, 'dock', dx, dz, ry, rnd, { force: true, noFlatten: true, y: WORLD.water + 0.9, noPaint: true });
      v.spots.dock = new THREE.Vector3(dx + v.lakeDir.x * 8, WORLD.water + 1.1, dz + v.lakeDir.y * 8);
      this.boats(v, dx, dz, ry);
    }
  }

  fence(x, z, w, d, ry) {
    const b = new Builder();
    const posts = [];
    const add = (lx, lz) => posts.push([lx, lz]);
    for (let i = 0; i <= Math.ceil(w / 3); i++) { const t = -w / 2 + i * w / Math.ceil(w / 3); add(t, -d / 2); add(t, d / 2); }
    for (let i = 1; i < Math.ceil(d / 3); i++) { const t = -d / 2 + i * d / Math.ceil(d / 3); add(-w / 2, t); add(w / 2, t); }
    const T = G.terrain, c = Math.cos(ry), s = Math.sin(ry);
    const world = (lx, lz) => [x + lx * c + lz * s, z - lx * s + lz * c];
    for (const [lx, lz] of posts) {
      const [wx, wz] = world(lx, lz), y = T.heightAt(wx, wz);
      b.box(0.16, 1.3, 0.16, wx, y + 0.55, wz, 0x664429, 0, 0, 0, false);
    }
    // traverses
    const rail = (ax, az, bx, bz) => {
      const [x1, z1] = world(ax, az), [x2, z2] = world(bx, bz);
      const y1 = T.heightAt(x1, z1), y2 = T.heightAt(x2, z2);
      const L = Math.hypot(x2 - x1, z2 - z1), ang = Math.atan2(x2 - x1, z2 - z1);
      for (const h of [0.55, 1.0]) b.box(0.07, 0.1, L, (x1 + x2) / 2, (y1 + y2) / 2 + h, (z1 + z2) / 2, 0x5a3b22, ang, 0, 0, false);
    };
    rail(-w / 2, -d / 2, w / 2, -d / 2); rail(-w / 2, d / 2, w / 2, d / 2); rail(-w / 2, -d / 2, -w / 2, d / 2 - 3); rail(w / 2, -d / 2, w / 2, d / 2);
    this.fenceGeos.push(b.geometry());
  }

  boats(v, x, z, ry) {
    const b = new Builder();
    for (let i = 0; i < 2; i++) {
      const lx = (i ? 5 : -5), lz = -6 - i * 7;
      const p = new THREE.Vector3(lx, 0, lz).applyAxisAngle(new THREE.Vector3(0, 1, 0), ry);
      const bx = x + p.x, bz = z + p.z, y = WORLD.water + 0.15;
      b.box(1.6, 0.5, 4.6, bx, y, bz, 0x664429, ry, 0, 0, false);
      b.box(1.2, 0.2, 4.2, bx, y + 0.12, bz, 0x4b2f1b, ry, 0, 0, false);
    }
    const m = new THREE.Mesh(b.geometry(), this.mat); m.castShadow = true;
    this.scene.add(m);
    v.boatMesh = m;
  }

  // ---------- Dégâts, effondrement, reconstruction ----------
  damageArea(x, z, radius, power, from) {
    let hit = 0;
    for (const b of this.buildings) {
      if (b.state !== 'intact') continue;
      const d = Math.hypot(b.x - x, b.z - z) - b.rad;
      if (d > radius) continue;
      const k = clamp(1 - Math.max(0, d) / radius, 0, 1);
      b.hp -= power * (0.35 + k);
      hit++;
      if (b.hp <= 0) this.collapse(b, from || new THREE.Vector3(x, 0, z), power);
      else if (b.hp < b.maxHp * 0.5 && !b.cracked) { b.cracked = true; b.mesh.material = this.crackedMat(); }
    }
    return hit;
  }

  crackedMat() {
    if (!this._cracked) { this._cracked = this.mat.clone(); this._cracked.color = new THREE.Color(0.62, 0.58, 0.55); this._cracked.onBeforeCompile = this.mat.onBeforeCompile; }
    return this._cracked;
  }

  collapse(b, from, power) {
    b.state = 'ruin';
    b.mesh.visible = false;
    const v = this.villages[b.village];
    v.fear = Math.min(1, v.fear + 0.08);
    emit('buildingCollapsed', { b, village: v });
    if (G.physics) G.physics.shatter(b, from, power, this.mat);
    if (G.fx) { G.fx.dust(new THREE.Vector3(b.x, b.y + 2, b.z), Math.max(b.w, b.d) * 0.9, 26); }
    if (G.audio) G.audio.collapse(b.mesh.position, Math.min(1, b.w * b.d / 80));
    if (G.shake) G.shake(0.25, b.mesh.position);
    // ruine basse persistante
    if (!b.ruin) {
      const r = new Builder();
      const rnd = mulberry32(b.id * 13 + 1);
      for (const p of b.pieces) {
        if (p.y > 3.2 || rnd() < 0.45) continue;
        const h = Math.min(p.h, 0.4 + rnd() * 1.6);
        r.box(p.w, h, p.d, p.x, h / 2 + 0.3, p.z, p.color, p.ry || 0, 0, 0, false);
      }
      if (r.geos.length) {
        b.ruin = new THREE.Mesh(r.geometry(), this.mat);
        b.ruin.position.copy(b.mesh.position); b.ruin.rotation.copy(b.mesh.rotation);
        b.ruin.castShadow = true; b.ruin.receiveShadow = true;
        this.scene.add(b.ruin);
      }
    } else b.ruin.visible = true;
    G.terrain.burnArea(b.x, b.z, b.rad, 0.25);
  }

  restore(b, divine = false) {
    b.state = 'intact'; b.hp = b.maxHp; b.cracked = false; b.rebuild = 0;
    b.mesh.visible = true; b.mesh.material = this.mat; b.mesh.scale.set(1, 1, 1);
    if (b.ruin) b.ruin.visible = false;
    if (b.scaffold) { this.scene.remove(b.scaffold); b.scaffold = null; }
    emit('buildingRestored', { b, divine, village: this.villages[b.village] });
  }

  // progression du chantier (appelée par les habitants qui travaillent)
  work(b, amount) {
    if (b.state !== 'ruin') return;
    b.rebuild += amount;
    if (!b.scaffold) {
      const s = new Builder();
      for (const [sx, sz] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) s.box(0.18, b.h * 0.8, 0.18, sx * b.w / 2, b.h * 0.4, sz * b.d / 2, 0x8a6440, 0, 0, 0, false);
      for (let y = 1.5; y < b.h * 0.8; y += 2) { s.box(b.w, 0.12, 0.12, 0, y, b.d / 2, 0x8a6440, 0, 0, 0, false); s.box(b.w, 0.12, 0.12, 0, y, -b.d / 2, 0x8a6440, 0, 0, 0, false); }
      b.scaffold = new THREE.Mesh(s.geometry(), this.mat);
      b.scaffold.position.copy(b.mesh.position); b.scaffold.rotation.copy(b.mesh.rotation);
      this.scene.add(b.scaffold);
    }
    // le bâtiment remonte depuis le sol pendant les travaux
    b.mesh.visible = b.rebuild > 0.15;
    b.mesh.scale.set(1, clamp(b.rebuild, 0.05, 1), 1);
    if (b.rebuild >= 1) this.restore(b);
  }

  nearestBuilding(x, z, filter = () => true) {
    let best = null, bd = 1e9;
    for (const b of this.buildings) { if (!filter(b)) continue; const d = Math.hypot(b.x - x, b.z - z); if (d < bd) { bd = d; best = b; } }
    return best;
  }

  // collision simple (habitants, dieu) : repousse hors des bâtiments intacts
  pushOut(p, r = 0.4) {
    for (const b of this.buildings) {
      if (b.state !== 'intact' || b.type === 'dock' || b.type === 'bridge') continue;
      const dx = p.x - b.x, dz = p.z - b.z;
      if (Math.abs(dx) > b.rad + 4 || Math.abs(dz) > b.rad + 4) continue;
      const c = Math.cos(-b.ry), s = Math.sin(-b.ry);
      let lx = dx * c + dz * s, lz = -dx * s + dz * c;
      const hw = b.w / 2 + r, hd = b.d / 2 + r;
      if (Math.abs(lx) < hw && Math.abs(lz) < hd) {
        const px = hw - Math.abs(lx), pz = hd - Math.abs(lz);
        if (px < pz) lx = Math.sign(lx) * hw; else lz = Math.sign(lz) * hd;
        p.x = b.x + lx * c - lz * s; p.z = b.z + lx * s + lz * c;
        return true;
      }
    }
    return false;
  }

  update(dt) {
    for (const b of this.buildings) {
      if (b.sails && b.state === 'intact') b.sails.rotation.z += dt * 0.9;
      if (b.state === 'intact' && G.fx && b.chimney && Math.random() < dt * 0.6 && G.camera.position.distanceTo(b.mesh.position) < 300) G.fx.smoke(b.chimney.clone(), 0.5, 4, 0.18);
      if (b.state === 'intact' && G.fx && b.forge && Math.random() < dt * 4 && G.camera.position.distanceTo(b.mesh.position) < 150) G.fx.fire(b.forge.clone(), 0.5);
    }
    // humeur des villages : la peur retombe lentement, la vénération reste
    for (const v of this.villages) v.fear = Math.max(0, v.fear - dt * 0.002);
  }

  serialize() { return this.buildings.map((b) => (b.state === 'intact' ? 0 : 1)).join(''); }
  restoreState(s) {
    if (!s || s.length !== this.buildings.length) return;
    for (let i = 0; i < s.length; i++) if (s[i] === '1') { const b = this.buildings[i]; b.hp = 0; this.collapse(b, b.mesh.position, 0); }
  }
}
