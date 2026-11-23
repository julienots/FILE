// Monde 3D du plateau : borne/écran, grille, blocs instanciés glossy, pièce active animée,
// fantôme, aperçus, réserve, lumières dynamiques, rayons lumineux, effacement de lignes
// spectaculaire, vitre de l'écran (fissures + explosion en éclats), silhouette observatrice.
import * as THREE from 'three';
import { W, H, VISIBLE, SHAPES } from '../game/tetris.js';
import { PALETTES, SPECIAL, MATERIALS, blockGeo, boxGeo, monsterMaterial, faceTexture, monsterUniforms } from './materials.js';
import { Particles } from './particles.js';
import { damp, rand, clamp, easeOutBack } from '../core/util.js';

const _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _s = new THREE.Vector3(), _p = new THREE.Vector3(), _e = new THREE.Euler();
const DEFAULT_MAT = new THREE.MeshStandardMaterial({ color: 0x888888 });

export const cellX = (x) => x - (W - 1) / 2;
export const cellY = (y) => y - (VISIBLE - 1) / 2;

const THEMES = {
  night: { name: 'Nuit arcade', bg: 0x0d0b22, fog: 0x0d0b22, hemiSky: 0x8fa8ff, hemiGround: 0x241038, rim1: 0x29d9ff, rim2: 0xff4fd8, panel: 0x14122e },
  sunset: { name: 'Coucher de soleil', bg: 0x2a0f2e, fog: 0x2a0f2e, hemiSky: 0xffb38a, hemiGround: 0x3a1030, rim1: 0xff8a3d, rim2: 0xff3d7a, panel: 0x2b1430 },
  mint: { name: 'Menthe', bg: 0x072a2a, fog: 0x072a2a, hemiSky: 0xa0fff0, hemiGround: 0x0a2830, rim1: 0x3dffc5, rim2: 0x3db2ff, panel: 0x0c2b2e },
  void: { name: 'Néant', bg: 0x050306, fog: 0x050306, hemiSky: 0x553040, hemiGround: 0x100408, rim1: 0xff2030, rim2: 0x701020, panel: 0x0a0508 },
};
export { THEMES };
export const GRIDS = { neon: 'Néon', minimal: 'Minimal', retro: 'Rétro', none: 'Invisible' };

// Police 3x5 pour écrire avec des blocs
const FONT = {
  A: '010101111101101', B: '110101110101110', C: '011100100100011', D: '110101101101110', E: '111100110100111',
  G: '011100101101011', H: '101101111101101', I: '111010010010111', K: '101101110101101', L: '100100100100111',
  M: '101111111101101', N: '110101101101101', O: '010101101101010', P: '110101110100100', R: '110101110101101',
  S: '011100010001110', T: '111010010010010', U: '101101101101111', V: '101101101101010', W: '101101111111101',
  Y: '101101010010010', '?': '111001010000010', '!': '010010010000010', ' ': '000000000000000', F: '111100110100100',
};

export class TetrisWorld {
  constructor(fx, save) {
    this.fx = fx; this.save = save;
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(36, 1, 0.1, 400);
    this.root = new THREE.Group(); this.scene.add(this.root);
    this.board = new THREE.Group(); this.root.add(this.board);
    this.corruption = 0; this.time = 0;
    this.camBase = new THREE.Vector3(0, 0, 40); this.camLook = new THREE.Vector3(0, 0, 0);
    this.camOverride = null; this.sway = new THREE.Vector2();
    this.layout = { top: 0.1, bottom: 0.25 };
    this.materialsCache = {};
    this.pulse = Array.from({ length: H }, () => new Array(W).fill(0));
    this.dropOffset = new Array(H).fill(0);
    this.dying = [];
    this._buildLights();
    this._buildCabinet();
    this._buildCells();
    this._buildActive();
    this._buildPreviews();
    this._buildWatcher();
    this.particles = new Particles(this.root, 800);
    this.applyCosmetics();
    this.fitCamera();
  }

  // ——— construction ———
  _buildLights() {
    const s = this.scene;
    this.hemi = new THREE.HemisphereLight(0x8fa8ff, 0x241038, 1.1); s.add(this.hemi);
    const key = this.key = new THREE.DirectionalLight(0xffffff, 2.4);
    key.position.set(-8, 16, 18); key.castShadow = true;
    const sz = this.fx.shadowSize; key.shadow.mapSize.set(sz, sz);
    const c = key.shadow.camera; c.left = -9; c.right = 9; c.top = 14; c.bottom = -13; c.near = 1; c.far = 60;
    key.shadow.bias = -0.0008; key.shadow.normalBias = 0.02; key.shadow.radius = 4;
    s.add(key);
    this.rimA = new THREE.PointLight(0x29d9ff, 40, 30, 1.6); this.rimA.position.set(-8, 4, 4); s.add(this.rimA);
    this.rimB = new THREE.PointLight(0xff4fd8, 40, 30, 1.6); this.rimB.position.set(8, -4, 4); s.add(this.rimB);
    this.redLight = new THREE.PointLight(0xff1020, 0, 40, 1.4); this.redLight.position.set(0, 0, 6); s.add(this.redLight);
  }

  _buildCabinet() {
    const g = this.cabinet = new THREE.Group(); this.root.add(g);
    // panneau arrière
    this.panelMat = new THREE.MeshPhysicalMaterial({ color: 0x14122e, roughness: 0.4, metalness: 0.2, clearcoat: 0.6 });
    const panel = new THREE.Mesh(boxGeo(W + 0.6, VISIBLE + 0.6, 0.6, 0.25, 3), this.panelMat);
    panel.position.set(0, 0, -0.85); panel.receiveShadow = true; g.add(panel);
    this.panel = panel;
    // lignes de grille
    const pts = [];
    for (let x = 0; x <= W; x++) pts.push(x - W / 2, -VISIBLE / 2, -0.53, x - W / 2, VISIBLE / 2, -0.53);
    for (let y = 0; y <= VISIBLE; y++) pts.push(-W / 2, y - VISIBLE / 2, -0.53, W / 2, y - VISIBLE / 2, -0.53);
    const lg = new THREE.BufferGeometry(); lg.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
    this.gridMat = new THREE.LineBasicMaterial({ color: 0x5a6cff, transparent: true, opacity: 0.22 });
    this.gridLines = new THREE.LineSegments(lg, this.gridMat); g.add(this.gridLines);
    // tubes néon latéraux
    this.neonMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(0x29d9ff).multiplyScalar(1.5), toneMapped: false });
    this.neonMat2 = new THREE.MeshBasicMaterial({ color: new THREE.Color(0xff4fd8).multiplyScalar(1.5), toneMapped: false });
    const tube = new THREE.CylinderGeometry(0.09, 0.09, VISIBLE + 0.8, 10);
    const l = new THREE.Mesh(tube, this.neonMat); l.position.set(-W / 2 - 0.45, 0, 0.1); g.add(l);
    const r = new THREE.Mesh(tube, this.neonMat2); r.position.set(W / 2 + 0.45, 0, 0.1); g.add(r);
    const btm = new THREE.Mesh(new THREE.CylinderGeometry(0.09, 0.09, W + 1, 10), this.neonMat); btm.rotation.z = Math.PI / 2; btm.position.set(0, -VISIBLE / 2 - 0.45, 0.1); g.add(btm);
    this.neons = [l, r, btm];
    // cadre de l'écran (bezel) : 4 barres arrondies
    const bez = new THREE.MeshPhysicalMaterial({ color: 0x1a1830, roughness: 0.35, metalness: 0.4, clearcoat: 1 });
    this.bezelMat = bez;
    const BW = W + 4.6, BH = VISIBLE + 8.2, T = 1.1, cy = 1.6;
    this.screenRect = { w: BW - T * 2, h: BH - T * 2, cy };
    const parts = [[0, cy + BH / 2 - T / 2, BW, T], [0, cy - BH / 2 + T / 2, BW, T], [-BW / 2 + T / 2, cy, T, BH], [BW / 2 - T / 2, cy, T, BH]];
    this.bezels = [];
    for (const [x, y, w, h] of parts) {
      const m = new THREE.Mesh(boxGeo(w, h, 1.4, 0.35, 3), bez); m.position.set(x, y, 0.4); m.castShadow = true; m.receiveShadow = true; g.add(m); this.bezels.push(m);
    }
    // vitre de l'écran
    this.crackCanvas = document.createElement('canvas'); this.crackCanvas.width = 512; this.crackCanvas.height = 1024;
    this.crackTex = new THREE.CanvasTexture(this.crackCanvas); this.crackTex.colorSpace = THREE.SRGBColorSpace;
    this.glassMat = new THREE.MeshPhysicalMaterial({ color: 0xffffff, roughness: 0.05, metalness: 0, transparent: true, opacity: 0.12, clearcoat: 1, map: this.crackTex, depthWrite: false });
    this.glass = new THREE.Mesh(new THREE.PlaneGeometry(this.screenRect.w, this.screenRect.h), this.glassMat);
    this.glass.position.set(0, cy, 1.05); g.add(this.glass);
    this.cracks = [];
    // reflet balayant la vitre
    const sheen = new THREE.Mesh(new THREE.PlaneGeometry(3, this.screenRect.h * 1.4), new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.05, depthWrite: false, blending: THREE.AdditiveBlending }));
    sheen.rotation.z = -0.5; sheen.position.set(-6, cy, 1.08); g.add(sheen); this.sheen = sheen;
    // rayons lumineux volumétriques (cônes additifs)
    const rayMat = new THREE.MeshBasicMaterial({ color: 0x6f7cff, transparent: true, opacity: 0.06, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide });
    this.rays = [];
    for (let i = 0; i < 3; i++) {
      const ray = new THREE.Mesh(new THREE.ConeGeometry(4 + i, 34, 24, 1, true), rayMat.clone());
      ray.position.set(-6 + i * 6, 14, -6 - i); ray.rotation.z = 0.35 - i * 0.35; this.scene.add(ray); this.rays.push(ray);
    }
    // sol et décor flottant
    const floor = new THREE.Mesh(new THREE.PlaneGeometry(200, 200), new THREE.MeshStandardMaterial({ color: 0x0c0a1e, roughness: 0.6, metalness: 0.3 }));
    floor.rotation.x = -Math.PI / 2; floor.position.y = -VISIBLE / 2 - 4.2; floor.receiveShadow = true; this.scene.add(floor); this.floor = floor;
    this.floaters = new THREE.Group(); this.scene.add(this.floaters);
    const types = Object.keys(SHAPES);
    for (let i = 0; i < 16; i++) {
      const t = types[i % types.length];
      const grp = new THREE.Group();
      for (const [c, r] of SHAPES[t][0]) {
        const m = new THREE.Mesh(blockGeo(), DEFAULT_MAT); m.position.set(c, -r, 0); grp.add(m);
      }
      grp.userData = { t, speed: rand(0.2, 0.6), spin: rand(-0.4, 0.4), phase: rand(10) };
      const side = i % 2 ? 1 : -1;
      grp.position.set(side * rand(12, 30), rand(-20, 25), rand(-40, -12));
      grp.scale.setScalar(rand(0.7, 1.6));
      this.floaters.add(grp);
    }
  }

  _buildCells() {
    const geo = blockGeo();
    this.cellMesh = new THREE.InstancedMesh(geo, new THREE.MeshStandardMaterial(), W * H + 20);
    this.cellMesh.castShadow = true; this.cellMesh.receiveShadow = true;
    this.cellMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    this.cellMesh.setColorAt(0, new THREE.Color()); this.cellMesh.frustumCulled = false;
    this.board.add(this.cellMesh);
    this.darkMesh = new THREE.InstancedMesh(geo, monsterMaterial(), W * H);
    this.darkMesh.castShadow = true; this.darkMesh.frustumCulled = false; this.darkMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    this.board.add(this.darkMesh);
    this.goldMesh = new THREE.InstancedMesh(geo, new THREE.MeshPhysicalMaterial({ color: 0xffc400, metalness: 0.9, roughness: 0.2, emissive: 0xffa000, emissiveIntensity: 0.6, clearcoat: 1 }), 30);
    this.goldMesh.frustumCulled = false; this.board.add(this.goldMesh);
    // yeux sur les blocs vivants / visages
    this.faceTex = { stare: faceTexture('stare'), cute: faceTexture('cute'), scream: faceTexture('scream') };
    this.faces = [];
    for (let i = 0; i < 40; i++) {
      const f = new THREE.Mesh(new THREE.PlaneGeometry(0.86, 0.86), new THREE.MeshBasicMaterial({ map: this.faceTex.stare, transparent: true, toneMapped: false, depthWrite: false }));
      f.visible = false; this.board.add(f); this.faces.push(f);
    }
    // texte en blocs (messages du monstre)
    this.msgMesh = new THREE.InstancedMesh(geo, monsterMaterial(), 200);
    this.msgMesh.count = 0; this.msgMesh.frustumCulled = false; this.board.add(this.msgMesh);
    this.msgCells = []; this.msgAlpha = 0;
    // blocs de cible (puzzle) : contours lumineux
    this.targetMesh = new THREE.InstancedMesh(blockGeo(0.96, 0.12, 2), new THREE.MeshBasicMaterial({ color: 0x66ffee, transparent: true, opacity: 0.22, toneMapped: false, depthWrite: false }), W * H);
    this.targetMesh.count = 0; this.targetMesh.frustumCulled = false; this.board.add(this.targetMesh);
    this.targetCells = [];
    // colonnes d'alerte (boss)
    this.warnMat = new THREE.MeshBasicMaterial({ color: 0xff1020, transparent: true, opacity: 0, toneMapped: false, depthWrite: false, blending: THREE.AdditiveBlending });
    this.warnCol = new THREE.Mesh(new THREE.PlaneGeometry(1, VISIBLE), this.warnMat); this.warnCol.position.z = -0.4; this.board.add(this.warnCol);
  }

  _buildActive() {
    this.activeGroup = new THREE.Group(); this.board.add(this.activeGroup);
    this.activeMeshes = []; this.ghostMeshes = [];
    for (let i = 0; i < 4; i++) {
      const m = new THREE.Mesh(blockGeo(), DEFAULT_MAT); m.castShadow = true; this.activeGroup.add(m); this.activeMeshes.push(m);
      const gmat = new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.16, depthWrite: false, toneMapped: false });
      const g = new THREE.Mesh(blockGeo(0.9, 0.12, 2), gmat); this.board.add(g); this.ghostMeshes.push(g);
    }
    this.activeFace = new THREE.Mesh(new THREE.PlaneGeometry(0.86, 0.86), new THREE.MeshBasicMaterial({ map: null, transparent: true, toneMapped: false, depthWrite: false }));
    this.activeFace.visible = false; this.activeGroup.add(this.activeFace);
    this.activeVis = { x: 0, y: 0, rot: 0, rotAnim: 0, id: -1 };
  }

  _buildPreviews() {
    this.previewGroup = new THREE.Group(); this.board.add(this.previewGroup);
    this.nextSlots = []; this.holdSlot = null;
    const mk = () => { const g = new THREE.Group(); const ms = []; for (let i = 0; i < 4; i++) { const m = new THREE.Mesh(blockGeo(), DEFAULT_MAT); m.castShadow = true; g.add(m); ms.push(m); } g.userData.meshes = ms; this.previewGroup.add(g); return g; };
    for (let i = 0; i < 3; i++) this.nextSlots.push(mk());
    this.holdSlot = mk();
    this.previewKey = '';
  }

  _buildWatcher() {
    // silhouette sombre derrière la grille + yeux rouges
    const c = document.createElement('canvas'); c.width = 256; c.height = 512;
    const g = c.getContext('2d');
    const grd = g.createRadialGradient(128, 200, 10, 128, 230, 230);
    grd.addColorStop(0, 'rgba(0,0,0,0.95)'); grd.addColorStop(0.6, 'rgba(0,0,0,0.8)'); grd.addColorStop(1, 'rgba(0,0,0,0)');
    g.fillStyle = grd;
    g.fillRect(78, 110, 100, 90); // tête carrée
    g.fillRect(40, 200, 176, 300); // corps
    g.fillRect(10, 230, 40, 260); g.fillRect(206, 230, 40, 260);
    const tex = new THREE.CanvasTexture(c);
    this.watcher = new THREE.Mesh(new THREE.PlaneGeometry(10, 20), new THREE.MeshBasicMaterial({ map: tex, transparent: true, opacity: 0, depthWrite: false }));
    this.watcher.position.set(0, 2, -0.52); this.board.add(this.watcher);
    this.watcherEyes = new THREE.Group();
    for (const x of [-0.8, 0.8]) {
      const e = new THREE.Mesh(new THREE.PlaneGeometry(0.7, 0.45), new THREE.MeshBasicMaterial({ color: new THREE.Color(2.2, 0, 0.02), transparent: true, opacity: 0, toneMapped: false, depthWrite: false }));
      e.position.set(x, 5.6, -0.5); this.watcherEyes.add(e);
    }
    this.board.add(this.watcherEyes);
    this.watcherLevel = 0; this.watcherTarget = 0;
  }

  // ——— cosmétiques (collection) ———
  applyCosmetics() {
    const eq = this.save.data.collection.equipped;
    this.palette = PALETTES[eq.palette] || PALETTES.classic;
    this.materialKind = MATERIALS[eq.material] ? eq.material : 'glossy';
    this.materialsCache = {};
    const base = MATERIALS[this.materialKind].make(0xffffff);
    this.cellMesh.material = base;
    if (this.materialKind === 'jelly') { this.cellMesh.material.transparent = true; }
    this.particles.burstStyle = eq.burst || 'shards';
    this.trail = eq.trail || 'none';
    const theme = THEMES[eq.theme] || THEMES.night;
    this.setTheme(theme);
    const gs = eq.grid || 'neon';
    this.gridLines.visible = gs !== 'none';
    this.gridMat.opacity = gs === 'minimal' ? 0.1 : gs === 'retro' ? 0.4 : 0.22;
    this.gridMat.color.set(gs === 'retro' ? 0x39ff14 : 0x5a6cff);
    for (const n of this.neons) n.visible = gs === 'neon' || gs === 'retro';
    // floaters
    this.floaters.children.forEach((grp) => grp.children.forEach((m) => { m.material = this.matFor(grp.userData.t); }));
    this.previewKey = '';
  }
  setTheme(theme) {
    this.theme = theme;
    this.scene.background = new THREE.Color(theme.bg);
    this.scene.fog = new THREE.Fog(theme.fog, 40, 110);
    this.hemi.color.set(theme.hemiSky); this.hemi.groundColor.set(theme.hemiGround);
    this.rimA.color.set(theme.rim1); this.rimB.color.set(theme.rim2);
    this.panelMat.color.set(theme.panel);
    this.neonMat.color.set(theme.rim1).multiplyScalar(1.5); this.neonMat2.color.set(theme.rim2).multiplyScalar(1.5);
  }

  colorOf(t) {
    if (SPECIAL[t] !== undefined) return SPECIAL[t];
    return this.palette[t] !== undefined ? this.palette[t] : 0xffffff;
  }
  matFor(t) {
    if (this.materialsCache[t]) return this.materialsCache[t];
    const m = MATERIALS[this.materialKind].make(this.colorOf(t));
    this.materialsCache[t] = m; return m;
  }

  // ——— caméra / mise en page ———
  setLayout(top, bottom) { this.layout = { top, bottom }; this.fitCamera(); }
  fitCamera() {
    const aspect = window.innerWidth / window.innerHeight;
    const { top, bottom } = this.layout;
    const fh = 1 - top - bottom;
    const contentH = VISIBLE + 5.2, contentW = W + 4.8;
    const vfov = THREE.MathUtils.degToRad(this.camera.fov);
    const frustH = Math.max(contentH / fh, contentW / aspect);
    const dist = frustH / 2 / Math.tan(vfov / 2);
    const cy = 1.5; // centre du contenu (grille + aperçus au-dessus)
    const rc = top + fh / 2;
    const camY = cy - (0.5 - rc) * frustH;
    // légère plongée pour révéler l'épaisseur des blocs
    this.camBase.set(0, camY + 5.5, dist + 1.2);
    this.camLook.set(0, camY + 0.6, 0);
    this.frustH = frustH;
    // aperçus au-dessus de la grille
    const py = VISIBLE / 2 + 2.0;
    this.holdSlot.position.set(-W / 2 + 1.2, py, 0);
    this.nextSlots.forEach((g, i) => { g.position.set(W / 2 - 1.1 - i * 2.35, py, 0); g.scale.setScalar(i === 0 ? 0.62 : 0.45); });
    this.nextSlots[0].position.x = W / 2 - 1.2;
    this.nextSlots[1].position.x = W / 2 - 3.5;
    this.nextSlots[2].position.x = W / 2 - 5.4;
  }

  /** Position écran (0..1, depuis le haut) d'un point du monde : utile pour placer du HTML. */
  project(v) {
    const p = v.clone().project(this.camera);
    return { x: (p.x + 1) / 2, y: (1 - p.y) / 2 };
  }

  // ——— mises à jour depuis la logique ———
  sync(engine, dt) {
    this.engine = engine;
    const grid = engine.grid;
    let n = 0, nd = 0, ng = 0, nf = 0;
    const cm = this.cellMesh, dm = this.darkMesh, gm = this.goldMesh;
    const color = new THREE.Color();
    const jitter = this.corruption > 0.5 ? (this.corruption - 0.5) * 0.08 : 0;
    for (let y = 0; y < H; y++) {
      this.dropOffset[y] = damp(this.dropOffset[y], 0, 12, dt);
      for (let x = 0; x < W; x++) {
        const c = grid[y][x];
        let pl = this.pulse[y][x];
        if (pl > 0) this.pulse[y][x] = pl = Math.max(0, pl - dt * 4);
        if (!c) continue;
        if (y >= VISIBLE + 1) continue;
        const vy = cellY(y + this.dropOffset[y]);
        const sc = 1 + Math.sin(pl * Math.PI) * 0.18;
        let px = cellX(x), py = vy, pz = 0;
        if (jitter && Math.random() < 0.05) { px += rand(-jitter, jitter); py += rand(-jitter, jitter); }
        if (c.dx) px += c.dx; if (c.dy) py += c.dy;
        _p.set(px, py, pz); _s.set(sc, sc, sc); _q.identity();
        if (c.t === 'X' || c.t === 'M') {
          _q.setFromEuler(_e.set(0, 0, c.t === 'M' ? Math.sin(this.time * 6 + x) * 0.08 : 0));
          _m.compose(_p, _q, _s); dm.setMatrixAt(nd++, _m);
        } else if (c.t === 'K') {
          _q.setFromEuler(_e.set(0, this.time * 2, 0)); _s.setScalar(0.85 + Math.sin(this.time * 5) * 0.06);
          _m.compose(_p, _q, _s); gm.setMatrixAt(ng++, _m);
        } else {
          _m.compose(_p, _q, _s); cm.setMatrixAt(n, _m);
          color.set(this.colorOf(c.t));
          if (pl > 0) color.lerp(new THREE.Color(1, 1, 1), pl * 0.6);
          cm.setColorAt(n, color); n++;
        }
        if ((c.live || c.face) && nf < this.faces.length) {
          const f = this.faces[nf++]; f.visible = true; f.position.set(px, py, 0.47);
          f.material.map = this.faceTex[c.face || 'stare'];
          f.scale.setScalar(c.live ? 1 + Math.sin(this.time * 8 + x) * 0.05 : 1);
        }
      }
    }
    // cellules mourantes (flash d'effacement)
    for (let i = this.dying.length - 1; i >= 0; i--) {
      const d = this.dying[i]; d.t += dt;
      if (d.t > d.dur) { this.dying.splice(i, 1); continue; }
      const k = d.t / d.dur;
      const sc = 1 + k * 0.5;
      _p.set(d.x, d.y, k * 0.8); _s.set(sc * (1 - k * 0.6), sc * (1 - k), sc); _q.identity();
      _m.compose(_p, _q, _s); cm.setMatrixAt(n, _m);
      color.set(d.color).lerp(new THREE.Color(4, 4, 4), 1 - k * 0.5);
      cm.setColorAt(n, color); n++;
      if (n >= cm.instanceMatrix.count) break;
    }
    cm.count = n; dm.count = nd; gm.count = ng;
    cm.instanceMatrix.needsUpdate = true; if (cm.instanceColor) cm.instanceColor.needsUpdate = true;
    dm.instanceMatrix.needsUpdate = true; gm.instanceMatrix.needsUpdate = true;
    for (let i = nf; i < this.faces.length; i++) this.faces[i].visible = false;

    this._syncActive(engine, dt);
    this._syncPreviews(engine);
  }

  _syncActive(engine, dt) {
    const p = engine.active;
    const av = this.activeVis;
    const ghostOn = this.save.s.ghost && !this.hideGhost;
    if (!p) {
      this.activeGroup.visible = false; this.ghostMeshes.forEach((g) => (g.visible = false));
      return;
    }
    this.activeGroup.visible = true;
    const cells = engine.cells(p);
    // pivot = centre de la boîte
    const box = p.type === 'I' ? 4 : p.type === 'O' ? 2 : 3;
    const pivX = cellX(p.x + (box - 1) / 2), pivY = cellY(p.y - (box - 1) / 2);
    if (av.id !== p.id) { av.id = p.id; av.x = pivX; av.y = pivY + 1.2; av.rot = p.rot; av.rotAnim = 0; this.activeGroup.scale.setScalar(0.4); }
    if (av.rot !== p.rot) {
      const d = ((p.rot - av.rot + 4) % 4) === 1 ? 1 : -1;
      av.rotAnim += d * Math.PI / 2; av.rot = p.rot;
    }
    av.x = damp(av.x, pivX, 32, dt); av.y = damp(av.y, pivY, 32, dt);
    av.rotAnim = damp(av.rotAnim, 0, 22, dt);
    const s = damp(this.activeGroup.scale.x, 1, 14, dt);
    this.activeGroup.scale.setScalar(s);
    this.activeGroup.position.set(av.x + (p.wobble || 0), av.y, 0.02);
    this.activeGroup.rotation.z = -av.rotAnim;
    const mat = this.matFor(p.type === 'X' ? 'G' : p.type);
    cells.forEach(([x, y], i) => {
      const m = this.activeMeshes[i];
      m.material = p.dark ? this.darkMesh.material : mat;
      m.position.set(cellX(x) - pivX, cellY(y) - pivY, 0);
      m.visible = y < VISIBLE + 2;
    });
    // visage sur la pièce active
    if (p.face) {
      this.activeFace.visible = true; this.activeFace.material.map = this.faceTex[p.face]; this.activeFace.material.needsUpdate = true;
      const m = this.activeMeshes[p.faceIndex || 1];
      this.activeFace.position.set(m.position.x, m.position.y, 0.47);
    } else this.activeFace.visible = false;
    // fantôme
    const gy = engine.ghostY();
    const gcells = engine.cells(p, p.rot, p.x, gy);
    gcells.forEach(([x, y], i) => {
      const g = this.ghostMeshes[i]; g.visible = ghostOn && y < VISIBLE;
      g.position.set(cellX(x), cellY(y), 0); g.material.color.set(this.colorOf(p.type));
    });
    if (this.trail !== 'none' && Math.random() < 0.5) {
      const [x, y] = cells[Math.floor(Math.random() * 4)];
      const c = this.trail === 'rainbow' ? new THREE.Color().setHSL((this.time * 0.5) % 1, 1, 0.6) : this.colorOf(p.type);
      this.particles.trail(cellX(x), cellY(y), -0.1, c);
    }
  }

  _syncPreviews(engine) {
    const q = engine.rules.forcedNext.concat(engine.queue).slice(0, 3);
    const key = q.join('') + '|' + engine.hold + '|' + engine.holdUsed + '|' + this.palette.name + this.materialKind;
    if (key === this.previewKey) return;
    this.previewKey = key;
    const put = (slot, t, dim) => {
      slot.visible = !!t;
      if (!t) return;
      const cells = SHAPES[t][0];
      const xs = cells.map((c) => c[0]), ys = cells.map((c) => c[1]);
      const cx = (Math.min(...xs) + Math.max(...xs)) / 2, cy = (Math.min(...ys) + Math.max(...ys)) / 2;
      cells.forEach(([c, r], i) => {
        const m = slot.userData.meshes[i]; m.position.set(c - cx, -(r - cy), 0);
        m.material = this.matFor(t);
      });
      slot.userData.dim = dim;
      slot.userData.pop = 0.6;
    };
    q.forEach((t, i) => put(this.nextSlots[i], t, false));
    put(this.holdSlot, engine.hold, engine.holdUsed);
  }

  // ——— événements visuels ———
  onLock(cells, hard) {
    for (const [x, y] of cells) if (y < H) this.pulse[y][x] = 1;
    if (hard) {
      for (const [x, y] of cells) this.particles.dust(cellX(x), cellY(y) - 0.5, 0.3, 2, this.colorOf(this.engine?.grid[y]?.[x]?.t || 'I'));
    }
  }
  onHardDrop(from, piece, engine) {
    const cells = engine.cells(piece, piece.rot, piece.x, piece.y);
    for (const [x, y] of cells) {
      for (let yy = y; yy < Math.min(VISIBLE, y + (from - piece.y)); yy++) {
        if (Math.random() < 0.6) this.particles.trail(cellX(x), cellY(yy), 0, this.colorOf(piece.type));
      }
    }
    this.fx.shake(0.18, 0.2);
  }
  onClear({ rows, removed, n }) {
    // cellules qui flashent puis éclatent
    rows.forEach((y, i) => {
      removed[i].forEach((c, x) => {
        if (!c) return;
        const col = this.colorOf(c.t);
        const px = cellX(x), py = cellY(y);
        this.dying.push({ x: px, y: py, color: col, t: -x * 0.012, dur: 0.28 + x * 0.012 });
        const delay = x * 12;
        setTimeout(() => this.particles.blockBurst(px, py, 0.3, c.t === 'X' || c.t === 'M' ? 0x101010 : col, 0.7 + n * 0.15, c.t === 'X' ? 'void' : undefined), delay);
      });
    });
    // décalage visuel des lignes au-dessus (chute animée)
    const cleared = new Set(rows);
    const remaining = []; for (let y = 0; y < H; y++) if (!cleared.has(y)) remaining.push(y);
    for (let i = 0; i < H; i++) {
      const old = i < remaining.length ? remaining[i] : i + rows.length;
      this.dropOffset[i] += old - i;
    }
    this.fx.shake(0.12 + n * 0.08, 0.25);
    this.fx.flash(0xffffff, n >= 4 ? 0.35 : 0.12);
    if (n >= 4) this.fx.aberration(0.012);
    this.lightBurst = 1 + n * 0.5;
  }

  /** Écrit un mot avec des blocs noirs dans la grille (superposition, sans toucher la logique). */
  writeWord(word, row = 12) {
    const cells = [];
    const letters = [...word.toUpperCase()];
    const totalW = letters.length * 4 - 1;
    let x0 = Math.floor((W - totalW) / 2);
    if (totalW > W) x0 = 0;
    letters.forEach((ch, li) => {
      const f = FONT[ch] || FONT[' '];
      for (let i = 0; i < 15; i++) if (f[i] === '1') cells.push([x0 + li * 4 + (i % 3), row - Math.floor(i / 3)]);
    });
    this.msgCells = cells; this.msgAlpha = 0; this.msgT = 0;
    return cells;
  }
  clearWord() { this.msgCells = []; this.msgMesh.count = 0; }

  setTargets(cells) { this.targetCells = cells || []; }

  showWatcher(level) { this.watcherTarget = level; }
  warnColumn(x, a) { this.warnCol.position.x = cellX(x); this.warnMat.opacity = a; }

  // ——— vitre ———
  resetGlass() {
    const g = this.crackCanvas.getContext('2d'); g.clearRect(0, 0, 512, 1024);
    this.crackTex.needsUpdate = true; this.glass.visible = true; this.glassMat.opacity = 0.12; this.cracks = [];
    if (this.shards) { this.shards.forEach((s) => this.root.remove(s)); this.shards = null; }
  }
  /** Ajoute des fissures depuis un point d'impact (coordonnées monde x,y). */
  crack(wx, wy, strength = 1) {
    const g = this.crackCanvas.getContext('2d');
    const sr = this.screenRect;
    const u = (wx + sr.w / 2) / sr.w * 512, v = (1 - (wy - sr.cy + sr.h / 2) / sr.h) * 1024;
    g.strokeStyle = 'rgba(255,255,255,0.95)'; g.shadowColor = 'rgba(200,230,255,0.9)'; g.shadowBlur = 6;
    const branches = Math.floor(9 * strength + 4);
    for (let b = 0; b < branches; b++) {
      let x = u, y = v, a = rand(Math.PI * 2);
      const len = rand(80, 260) * strength;
      g.lineWidth = rand(1.5, 3.5);
      g.beginPath(); g.moveTo(x, y);
      for (let d = 0; d < len; d += 14) {
        a += rand(-0.5, 0.5); x += Math.cos(a) * 14; y += Math.sin(a) * 14; g.lineTo(x, y);
        if (Math.random() < 0.12) { const bx = x, by = y; let ba = a + rand(-1.2, 1.2); g.moveTo(bx, by); g.lineTo(bx + Math.cos(ba) * 40, by + Math.sin(ba) * 40); g.moveTo(x, y); }
      }
      g.stroke();
    }
    // anneaux concentriques
    g.lineWidth = 1.5;
    for (let r = 20; r < 60 * strength + 20; r += 22) {
      g.beginPath();
      for (let a = 0; a <= Math.PI * 2 + 0.01; a += 0.4) { const rr = r + rand(-6, 6); g.lineTo(u + Math.cos(a) * rr, v + Math.sin(a) * rr); }
      g.stroke();
    }
    g.fillStyle = 'rgba(255,255,255,0.6)'; g.beginPath(); g.arc(u, v, 10 * strength, 0, 7); g.fill();
    this.crackTex.needsUpdate = true;
    this.glassMat.opacity = Math.min(0.9, this.glassMat.opacity + 0.25 * strength);
    this.cracks.push([wx, wy]);
  }
  /** La vitre explose en éclats qui volent vers la caméra. */
  shatter(cx = 0, cy = 2) {
    this.glass.visible = false;
    const sr = this.screenRect;
    const mat = new THREE.MeshPhysicalMaterial({ color: 0xdff4ff, roughness: 0.05, transparent: true, opacity: 0.55, clearcoat: 1, side: THREE.DoubleSide, emissive: 0x334455 });
    this.shards = [];
    const N = this.fx.quality() === 'performance' ? 60 : 130;
    for (let i = 0; i < N; i++) {
      const s = rand(0.3, 1.5);
      const geo = new THREE.BufferGeometry();
      const v = [0, 0, 0, rand(0.4, 1) * s, rand(-0.3, 0.3) * s, 0, rand(-0.3, 0.6) * s, rand(0.4, 1) * s, 0];
      geo.setAttribute('position', new THREE.Float32BufferAttribute(v, 3)); geo.computeVertexNormals();
      const m = new THREE.Mesh(geo, mat);
      const x = rand(-sr.w / 2, sr.w / 2), y = sr.cy + rand(-sr.h / 2, sr.h / 2);
      m.position.set(x, y, 1.05);
      const dx = x - cx, dy = y - cy, d = Math.max(0.5, Math.hypot(dx, dy));
      const sp = rand(10, 28) / Math.sqrt(d);
      m.userData = { v: new THREE.Vector3(dx / d * sp * 3 + rand(-3, 3), dy / d * sp * 3 + rand(-2, 5), rand(18, 45)), w: new THREE.Vector3(rand(-12, 12), rand(-12, 12), rand(-12, 12)), life: 3 };
      this.root.add(m); this.shards.push(m);
    }
    this.particles.sparks(cx, cy, 1.2, 0xbfe8ff, 40, 18);
  }

  // ——— boucle ———
  update(dt, t) {
    this.time = t;
    monsterUniforms.uTime.value = t;
    const c = this.corruption;
    // caméra
    const cam = this.camera;
    if (this.camOverride) {
      cam.position.copy(this.camOverride.pos); cam.lookAt(this.camOverride.look);
    } else {
      const sens = this.save.s.sensitivity;
      this.sway.x = damp(this.sway.x, 0, 3, dt); this.sway.y = damp(this.sway.y, 0, 3, dt);
      const bx = this.camBase.x + Math.sin(t * 0.3) * 0.25 + this.sway.x * sens;
      const by = this.camBase.y + Math.sin(t * 0.23) * 0.2 + this.sway.y * sens;
      cam.position.set(bx, by, this.camBase.z);
      cam.lookAt(this.camLook.x + this.sway.x * 0.2 * sens, this.camLook.y, 0);
    }
    // lumières
    this.lightBurst = damp(this.lightBurst || 0, 0, 5, dt);
    this.key.intensity = 2.4 * (1 - c * 0.55) + this.lightBurst * 0.8;
    this.rimA.intensity = 40 * (1 - c * 0.7) + this.lightBurst * 30;
    this.rimB.intensity = 40 * (1 - c * 0.7) + this.lightBurst * 30;
    if (c > 0.3 && Math.random() < c * 0.02) { this.key.intensity *= 0.2; this.rimA.intensity = 0; }
    this.redLight.intensity = damp(this.redLight.intensity, c * 18 + (this.redPulse || 0), 4, dt);
    this.redPulse = damp(this.redPulse || 0, 0, 3, dt);
    // néons qui grésillent
    for (const n of this.neons) n.visible = n.visible && true;
    if (c > 0.2) this.neons.forEach((n, i) => (n.material.color.multiplyScalar(Math.random() < c * 0.05 ? 0.2 : 1)));
    if (Math.random() < 0.1) { this.neonMat.color.set(this.theme.rim1).lerp(new THREE.Color(0xff1020), c).multiplyScalar(1.5); this.neonMat2.color.set(this.theme.rim2).lerp(new THREE.Color(0x400008), c).multiplyScalar(1.5); }
    // décor
    for (const grp of this.floaters.children) {
      const u = grp.userData;
      grp.position.y += u.speed * dt * (1 - c * 1.5);
      if (grp.position.y > 30) grp.position.y = -25; if (grp.position.y < -26) grp.position.y = 29;
      grp.rotation.z += u.spin * dt; grp.rotation.y += u.spin * 0.5 * dt;
    }
    this.rays.forEach((r, i) => { r.material.opacity = 0.05 + Math.sin(t * 0.7 + i) * 0.02; r.material.color.set(c > 0.4 ? 0xff2030 : 0x6f7cff); });
    this.sheen.position.x = -14 + ((t * 2.2) % 28);
    // silhouette
    this.watcherLevel = damp(this.watcherLevel, this.watcherTarget, 2.5, dt);
    this.watcher.material.opacity = this.watcherLevel * 0.85;
    this.watcherEyes.children.forEach((e) => (e.material.opacity = this.watcherLevel > 0.05 ? Math.min(1, this.watcherLevel * 1.4) * (Math.sin(t * 0.9) > -0.95 ? 1 : 0) : 0));
    // mot écrit
    if (this.msgCells.length) {
      this.msgT += dt;
      const shown = Math.min(this.msgCells.length, Math.floor(this.msgT * 40));
      const mm = this.msgMesh;
      for (let i = 0; i < shown; i++) {
        const [x, y] = this.msgCells[i];
        _p.set(cellX(x) + (Math.random() < 0.03 ? rand(-0.1, 0.1) : 0), cellY(y), 0.1); _q.identity(); _s.setScalar(0.95);
        _m.compose(_p, _q, _s); mm.setMatrixAt(i, _m);
      }
      mm.count = shown; mm.instanceMatrix.needsUpdate = true;
    }
    // cibles puzzle
    if (this.targetCells.length) {
      const tm = this.targetMesh; let i = 0;
      for (const [x, y] of this.targetCells) {
        const filled = this.engine && this.engine.grid[y] && this.engine.grid[y][x];
        if (filled) continue;
        _p.set(cellX(x), cellY(y), 0); _q.identity(); _s.setScalar(0.96 + Math.sin(t * 4 + x + y) * 0.03);
        _m.compose(_p, _q, _s); tm.setMatrixAt(i++, _m);
      }
      tm.count = i; tm.instanceMatrix.needsUpdate = true;
    } else this.targetMesh.count = 0;
    // aperçus : petit rebond
    for (const s of [...this.nextSlots, this.holdSlot]) {
      if (s.userData.pop > 0) s.userData.pop = Math.max(0, s.userData.pop - dt * 3);
      s.rotation.y = Math.sin(t * 1.3 + s.position.x) * 0.25;
      s.rotation.x = -0.15;
      const base = s === this.nextSlots[0] ? 0.72 : s === this.holdSlot ? 0.62 : 0.5;
      s.scale.setScalar(base * (1 + Math.sin(s.userData.pop * Math.PI) * 0.25) * (s.userData.dim ? 0.8 : 1));
    }
    // éclats de vitre
    if (this.shards) {
      for (const s of this.shards) {
        const u = s.userData; u.life -= dt;
        u.v.y -= 9 * dt; s.position.addScaledVector(u.v, dt);
        s.rotation.x += u.w.x * dt; s.rotation.y += u.w.y * dt; s.rotation.z += u.w.z * dt;
        s.visible = u.life > 0 && s.position.z < cam.position.z - 0.5;
      }
    }
    this.particles.update(dt);
  }
}
