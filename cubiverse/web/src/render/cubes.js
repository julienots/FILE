// Rendu des cubes : sols, murs en coupe (côtés face caméra masqués), portes, cadres,
// échelles/trappes, objets instanciés, et version simplifiée pour les cubes lointains.
import * as THREE from 'three';
import { CUBE, HALF, DOOR_HALF, LADDER, SIDES, OPPOSITE } from '../sim/constants.js';
import { THEMES, OBJECT_TYPES, OBJECT_KEYS } from '../sim/themes.js';
import { propGeometry, ladderGeometry, hatchGeometry, dirtyGeometry } from './props.js';
import { roundedBox } from './geom.js';
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js';

const WALL_T = 0.08;
const DOOR_H = 1.05;
const NORMALS = { px: [1, 0], nx: [-1, 0], pz: [0, 1], nz: [0, -1] };

class Inst {
  constructor(group, geo, mat, { shadow = true, receive = true, color = false } = {}) {
    this.group = group; this.geo = geo; this.mat = mat; this.shadow = shadow; this.receive = receive; this.useColor = color;
    this.mesh = null; this.cap = 0; this.n = 0;
  }
  begin(maxN) {
    if (!this.mesh || maxN > this.cap) {
      if (this.mesh) { this.group.remove(this.mesh); this.mesh.dispose(); }
      this.cap = Math.max(16, Math.ceil(maxN * 1.5));
      this.mesh = new THREE.InstancedMesh(this.geo, this.mat, this.cap);
      this.mesh.castShadow = this.shadow; this.mesh.receiveShadow = this.receive;
      if (this.useColor) this.mesh.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(this.cap * 3).fill(1), 3);
      this.group.add(this.mesh);
    }
    this.n = 0;
  }
  add(m, c) { this.mesh.setMatrixAt(this.n, m); if (c && this.useColor) this.mesh.setColorAt(this.n, c); this.n++; }
  end() { this.mesh.count = this.n; this.mesh.instanceMatrix.needsUpdate = true; if (this.mesh.instanceColor) this.mesh.instanceColor.needsUpdate = true; this.mesh.computeBoundingSphere(); }
}

function tileTexture() {
  const c = document.createElement('canvas'); c.width = c.height = 256; const g = c.getContext('2d');
  g.fillStyle = '#ffffff'; g.fillRect(0, 0, 256, 256);
  for (let i = 0; i < 4; i++) for (let j = 0; j < 4; j++) { g.fillStyle = (i + j) % 2 ? '#f2f2f2' : '#ffffff'; g.fillRect(i * 64, j * 64, 64, 64); }
  g.strokeStyle = 'rgba(0,0,0,0.10)'; g.lineWidth = 3;
  for (let i = 0; i <= 4; i++) { g.beginPath(); g.moveTo(i * 64, 0); g.lineTo(i * 64, 256); g.stroke(); g.beginPath(); g.moveTo(0, i * 64); g.lineTo(256, i * 64); g.stroke(); }
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4; return t;
}
function wallTexture() {
  const c = document.createElement('canvas'); c.width = c.height = 256; const g = c.getContext('2d');
  g.fillStyle = '#ffffff'; g.fillRect(0, 0, 256, 256);
  g.fillStyle = 'rgba(0,0,0,0.05)'; for (let i = 0; i < 256; i += 32) g.fillRect(i, 0, 14, 256);
  g.fillStyle = 'rgba(0,0,0,0.12)'; g.fillRect(0, 236, 256, 20);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
}

function doorWallGeometry() {
  const w = DOOR_HALF + 0.02; const side = HALF - w;
  const l = new THREE.BoxGeometry(side, CUBE, WALL_T); l.translate(-(w + side / 2), CUBE / 2, 0);
  const r = new THREE.BoxGeometry(side, CUBE, WALL_T); r.translate(w + side / 2, CUBE / 2, 0);
  const t = new THREE.BoxGeometry(w * 2, CUBE - DOOR_H, WALL_T); t.translate(0, DOOR_H + (CUBE - DOOR_H) / 2, 0);
  return mergeGeometries([l, r, t]);
}
function fullWallGeometry() { const g = new THREE.BoxGeometry(CUBE, CUBE, WALL_T); g.translate(0, CUBE / 2, 0); return g; }
function windowGeometry() {
  const g = new THREE.Group();
  const b = new THREE.BoxGeometry(1.1, 0.9, 0.02); b.translate(0, 0, 0);
  return b;
}

export class CubeRenderer {
  constructor(scene, gradientMap) {
    this.scene = scene;
    this.group = new THREE.Group(); scene.add(this.group);
    const floorMat = new THREE.MeshLambertMaterial({ map: tileTexture() });
    const wallMat = new THREE.MeshLambertMaterial({ map: wallTexture() });
    const toon = new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap, vertexColors: true });
    const toonPlain = new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap });
    this.toon = toon;
    const floorGeo = roundedBox(CUBE - 0.02, 0.16, CUBE - 0.02, 0.03); floorGeo.translate(0, -0.02, 0);
    this.floors = new Inst(this.group, floorGeo, floorMat, { shadow: false, color: true });
    this.walls = new Inst(this.group, fullWallGeometry(), wallMat, { shadow: false, color: true });
    this.doorWalls = new Inst(this.group, doorWallGeometry(), wallMat, { shadow: false, color: true });
    this.windows = new Inst(this.group, windowGeometry(), new THREE.MeshBasicMaterial({ color: 0xffffff }), { shadow: false, receive: false, color: true });
    const frameGeo = roundedBox(1, 1, 1, 0);
    this.frames = new Inst(this.group, frameGeo, toonPlain, { shadow: false, color: true });
    this.glass = new Inst(this.group, new THREE.PlaneGeometry(CUBE, CUBE), new THREE.MeshBasicMaterial({ color: 0xbfe8ff, transparent: true, opacity: 0.07, depthWrite: false, side: THREE.DoubleSide }), { shadow: false, receive: false });
    this.doorArch = new Inst(this.group, new THREE.TorusGeometry(DOOR_HALF + 0.02, 0.045, 6, 16, Math.PI), toonPlain, { shadow: false, color: true });
    this.ladders = new Inst(this.group, ladderGeometry(), toon);
    this.hatches = new Inst(this.group, hatchGeometry(), toon, { shadow: false });
    this.dirty = new Inst(this.group, dirtyGeometry(), toon);
    this.far = new Inst(this.group, roundedBox(CUBE * 0.98, CUBE * 0.98, CUBE * 0.98, 0.12), new THREE.MeshLambertMaterial({ color: 0xffffff }), { shadow: false, color: true });
    this.props = {};
    for (const k of OBJECT_KEYS) this.props[k] = new Inst(this.group, propGeometry(k), toon);
    // sélection
    const edges = new THREE.EdgesGeometry(new THREE.BoxGeometry(CUBE + 0.06, CUBE + 0.06, CUBE + 0.06));
    this.sel = new THREE.LineSegments(edges, new THREE.LineBasicMaterial({ color: 0xffd23f, transparent: true, opacity: 0.95 }));
    this.sel.visible = false; scene.add(this.sel);
    this.ghostMat = new THREE.MeshBasicMaterial({ color: 0xffd23f, transparent: true, opacity: 0.16, depthWrite: false });
    this.ghostEdgeMat = new THREE.LineBasicMaterial({ color: 0xffd23f, transparent: true, opacity: 0.6 });
    this.ghostGeo = new THREE.BoxGeometry(CUBE * 0.96, CUBE * 0.96, CUBE * 0.96);
    this.ghostEdges = new THREE.EdgesGeometry(this.ghostGeo);
    this.ghosts = [];
    this.plus = makePlusTexture();
    this.m = new THREE.Matrix4(); this.m2 = new THREE.Matrix4(); this.q = new THREE.Quaternion(); this.v = new THREE.Vector3(); this.s = new THREE.Vector3(); this.c = new THREE.Color();
    this.key = '';
    this.detailed = [];
  }

  // Reconstruit les instances si la structure, la zone détaillée, l'orientation de caméra ou la coupe changent.
  update(world, focus, camDir, opts, force = false) {
    const quad = Math.round(Math.atan2(camDir.x, camDir.z) / (Math.PI / 8));
    const fx = Math.round(focus.x / CUBE), fz = Math.round(focus.z / CUBE), fy = Math.round(focus.y / CUBE);
    const key = `${world.version}|${quad}|${fx},${fy},${fz}|${opts.cutY}|${opts.detail}`;
    if (key === this.key && !force) return;
    this.key = key;
    this.build(world, focus, camDir, opts);
  }

  build(world, focus, camDir, opts) {
    const { m, q, v, s, c } = this;
    const detailR = opts.detail;
    const cubes = [...world.cubes.values()].filter((cb) => cb.y <= opts.cutY);
    const near = [], far = [];
    for (const cb of cubes) {
      const dx = cb.x * CUBE - focus.x, dz = cb.z * CUBE - focus.z, dy = cb.y * CUBE - focus.y;
      const d = Math.sqrt(dx * dx + dz * dz + dy * dy) / CUBE;
      (d <= detailR ? near : far).push([d, cb]);
    }
    near.sort((a, b) => a[0] - b[0]);
    const maxDetail = opts.maxDetail || 260;
    while (near.length > maxDetail) far.push(near.pop());
    this.detailed = near.map((x) => x[1]);
    const N = near.length;
    this.floors.begin(N); this.walls.begin(N * 4); this.doorWalls.begin(N * 4); this.windows.begin(N * 4); this.frames.begin(N * 12);
    this.glass.begin(N * 5); this.doorArch.begin(N * 4); this.ladders.begin(N); this.hatches.begin(N); this.far.begin(far.length);
    const propCounts = {}; for (const k of OBJECT_KEYS) propCounts[k] = 0;
    for (const [, cb] of near) for (const o of cb.objects) propCounts[o.type] = (propCounts[o.type] || 0) + 1;
    for (const k of OBJECT_KEYS) this.props[k].begin(propCounts[k]);
    const cam = new THREE.Vector2(camDir.x, camDir.z).normalize();
    const one = new THREE.Vector3(1, 1, 1);
    for (const [, cb] of near) {
      const th = THEMES[cb.theme];
      const ox = cb.x * CUBE, oy = cb.y * CUBE, oz = cb.z * CUBE;
      c.set(cb.tint || th.floor);
      m.makeTranslation(ox, oy, oz); this.floors.add(m, c);
      const wallC = new THREE.Color(th.wall); const accent = new THREE.Color(th.accent);
      // murs latéraux
      for (const side of ['px', 'nx', 'pz', 'nz']) {
        const [nx, nz] = NORMALS[side];
        const facing = nx * cam.x + nz * cam.y > 0.25; // mur entre la caméra et l'intérieur
        const ry = Math.atan2(nx, nz);
        const wx = ox + nx * (HALF - WALL_T / 2), wz = oz + nz * (HALF - WALL_T / 2);
        const connected = !!world.neighbor(cb, side);
        q.setFromAxisAngle(UP, ry);
        if (facing) {
          v.set(ox + nx * HALF, oy + CUBE / 2, oz + nz * HALF); m.compose(v, q, one); this.glass.add(m);
          if (connected) { m.compose(v.set(ox + nx * (HALF - 0.01), oy + DOOR_H - DOOR_HALF - 0.02, oz + nz * (HALF - 0.01)), q, one); this.doorArch.add(m, accent); }
          continue;
        }
        v.set(wx, oy, wz); m.compose(v, q, one);
        if (connected) { this.doorWalls.add(m, wallC); m.compose(v.set(ox + nx * (HALF - WALL_T - 0.005), oy + DOOR_H - DOOR_HALF - 0.02, oz + nz * (HALF - WALL_T - 0.005)), q, one); this.doorArch.add(m, accent); }
        else this.walls.add(m, wallC);
        // fenêtre décorative
        const winOff = connected ? 1.05 : 0;
        v.set(ox + nx * (HALF - WALL_T - 0.012) - nz * winOff * 0, oy + 2.35, oz + nz * (HALF - WALL_T - 0.012) + nx * 0);
        m.compose(v, q, one); c.set(world.isNight ? '#2c3e74' : skyFor(cb.theme)); this.windows.add(m, c);
      }
      // verre du plafond
      q.setFromAxisAngle(XAX, -Math.PI / 2); v.set(ox, oy + CUBE, oz); m.compose(v, q, one); this.glass.add(m);
      // cadre (12 arêtes)
      for (const e of EDGES) {
        v.set(ox + e[0] * HALF, oy + (e[1] + 1) * HALF, oz + e[2] * HALF);
        s.set(e[3] ? CUBE + 0.1 : 0.1, e[4] ? CUBE + 0.1 : 0.1, e[5] ? CUBE + 0.1 : 0.1);
        q.identity(); m.compose(v, q, s); this.frames.add(m, accent);
      }
      // échelle vers le haut / trappe vers le bas
      if (world.neighbor(cb, 'py')) { q.setFromAxisAngle(UP, Math.PI / 4); v.set(ox + LADDER.x - 0.12, oy + 0.06, oz + LADDER.z - 0.12); m.compose(v, q, one); this.ladders.add(m); }
      if (world.neighbor(cb, 'ny')) { q.identity(); v.set(ox + LADDER.x + 0.05, oy + 0.065, oz + LADDER.z + 0.05); m.compose(v, q, one); this.hatches.add(m); }
      // objets
      for (const o of cb.objects) {
        q.setFromAxisAngle(UP, o.r); v.set(ox + o.x, oy + 0.06, oz + o.z); m.compose(v, q, one);
        this.props[o.type].add(m);
      }
    }
    for (const [, cb] of far) {
      const th = THEMES[cb.theme];
      c.set(cb.tint || th.floor).lerp(new THREE.Color(th.accent), 0.25);
      m.makeTranslation(cb.x * CUBE, cb.y * CUBE + CUBE / 2, cb.z * CUBE); this.far.add(m, c);
    }
    for (const inst of [this.floors, this.walls, this.doorWalls, this.windows, this.frames, this.glass, this.doorArch, this.ladders, this.hatches, this.far]) inst.end();
    for (const k of OBJECT_KEYS) this.props[k].end();
    this.updateDirty(world);
  }

  updateDirty(world) {
    const { m, q, v } = this; const one = new THREE.Vector3(1, 1, 1);
    let n = 0; for (const cb of this.detailed) for (const o of cb.objects) if (o.state.dirty) n++;
    this.dirty.begin(n);
    for (const cb of this.detailed) for (const o of cb.objects) if (o.state.dirty) {
      q.setFromAxisAngle(UP, o.r); v.set(cb.x * CUBE + o.x, cb.y * CUBE + 0.06, cb.z * CUBE + o.z); m.compose(v, q, one); this.dirty.add(m);
    }
    this.dirty.end();
  }

  setSelection(world, cube, editMode, time) {
    if (!cube) { this.sel.visible = false; this.setGhosts(world, null); return; }
    this.sel.visible = true;
    this.sel.position.set(cube.x * CUBE, cube.y * CUBE + CUBE / 2, cube.z * CUBE);
    this.sel.material.opacity = 0.6 + Math.sin(time * 4) * 0.35;
    this.setGhosts(world, editMode ? cube : null);
  }

  setGhosts(world, cube) {
    const want = [];
    if (cube) for (const s in SIDES) { const [dx, dy, dz] = SIDES[s]; const p = [cube.x + dx, cube.y + dy, cube.z + dz]; if (world.isFree(...p)) want.push(p); }
    const key = want.map((p) => p.join(',')).join('|');
    if (key === this.ghostKey) return;
    this.ghostKey = key;
    for (const g of this.ghosts) this.scene.remove(g);
    this.ghosts = want.map((p) => {
      const g = new THREE.Group();
      const mesh = new THREE.Mesh(this.ghostGeo, this.ghostMat); g.add(mesh);
      g.add(new THREE.LineSegments(this.ghostEdges, this.ghostEdgeMat));
      const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: this.plus, depthTest: false, transparent: true })); sp.scale.set(0.8, 0.8, 1); g.add(sp);
      g.position.set(p[0] * CUBE, p[1] * CUBE + CUBE / 2, p[2] * CUBE);
      g.userData.grid = p; mesh.userData.grid = p;
      this.scene.add(g); return g;
    });
  }
}

const UP = new THREE.Vector3(0, 1, 0); const XAX = new THREE.Vector3(1, 0, 0);
// arêtes : centre (x,y,z en demi-tailles) + axe d'étirement
const EDGES = [];
for (const y of [-1, 1]) for (const z of [-1, 1]) EDGES.push([0, y, z, 1, 0, 0]);
for (const x of [-1, 1]) for (const z of [-1, 1]) EDGES.push([x, 0, z, 0, 1, 0]);
for (const x of [-1, 1]) for (const y of [-1, 1]) EDGES.push([x, y, 0, 0, 0, 1]);

function skyFor(theme) { return theme === 'espace' ? '#1b1d4a' : theme === 'foret' ? '#bfe9c8' : theme === 'plage' || theme === 'piscine' ? '#9fe3ff' : '#bfe6ff'; }
function makePlusTexture() {
  const c = document.createElement('canvas'); c.width = c.height = 128; const g = c.getContext('2d');
  g.fillStyle = '#ffd23f'; g.beginPath(); g.arc(64, 64, 56, 0, Math.PI * 2); g.fill();
  g.fillStyle = '#1d1b26'; g.fillRect(56, 28, 16, 72); g.fillRect(28, 56, 72, 16);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
}
