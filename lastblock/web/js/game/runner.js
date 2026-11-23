// Mode « poursuite » : couloirs compacts scénarisés (pas de monde ouvert), 3 voies, sprint,
// sauts, glissades, obstacles, portes qui se ferment, blocs qui tombent, sols qui s'effondrent,
// cachettes, énigmes de serrure. Le monstre poursuit, accélère, disparaît, tend des embuscades,
// rampe sur les murs, saute par-dessus les blocs et détruit les obstacles.
import * as THREE from 'three';
import { Monster } from '../world/monster.js';
import { Particles } from '../world/particles.js';
import { PALETTES, MATERIALS, blockGeo, boxGeo, monsterMaterial } from '../world/materials.js';
import { SHAPES, TYPES } from './tetris.js';
import { rand, randi, pick, damp, clamp, chance } from '../core/util.js';

const LANES = [-2.4, 0, 2.4];
const CHUNK = 24, NCHUNK = 7;

const THEMES = {
  arcade: { bg: 0x120a24, fog: 0x120a24, floor: 0x2a1650, wall: 0x1b1238, light: 0xff4fd8, light2: 0x29d9ff, label: "SALLE D'ARCADE" },
  board: { bg: 0x0a0c22, fog: 0x0a0c22, floor: 0x161a40, wall: 0x22264f, light: 0x29d9ff, light2: 0xffd21f, label: 'PLATEAU GÉANT' },
  collapse: { bg: 0x14060a, fog: 0x14060a, floor: 0x2a1a22, wall: 0x1a1016, light: 0xff5020, light2: 0xff2040, label: 'GRILLE EN RUINE' },
  void: { bg: 0x030203, fog: 0x030203, floor: 0x0a0607, wall: 0x050304, light: 0xff1020, light2: 0x701020, label: 'ESPACE CORROMPU' },
  abstract: { bg: 0x1a1030, fog: 0x1a1030, floor: 0x241a40, wall: 0x30205a, light: 0xffd21f, light2: 0xa24bff, label: 'BLOCS GÉANTS' },
};

function checkerTex(c1, c2, lines) {
  const c = document.createElement('canvas'); c.width = c.height = 128;
  const g = c.getContext('2d');
  g.fillStyle = c1; g.fillRect(0, 0, 128, 128);
  g.fillStyle = c2; g.fillRect(0, 0, 64, 64); g.fillRect(64, 64, 64, 64);
  if (lines) { g.strokeStyle = lines; g.lineWidth = 3; g.strokeRect(1, 1, 126, 126); }
  const t = new THREE.CanvasTexture(c); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

/** Petit personnage du joueur, en blocs arrondis. */
class Avatar {
  constructor() {
    this.root = new THREE.Group();
    const body = new THREE.MeshPhysicalMaterial({ color: 0xffc928, roughness: 0.3, clearcoat: 1, emissive: 0x402800, emissiveIntensity: 0.2 });
    const dark = new THREE.MeshStandardMaterial({ color: 0x2a2140, roughness: 0.6 });
    const white = new THREE.MeshBasicMaterial({ color: 0xffffff });
    const black = new THREE.MeshBasicMaterial({ color: 0x111111 });
    this.pelvis = new THREE.Group(); this.pelvis.position.y = 0.75; this.root.add(this.pelvis);
    this.torso = new THREE.Mesh(boxGeo(0.7, 0.75, 0.5, 0.18), body); this.torso.position.y = 0.4; this.torso.castShadow = true; this.pelvis.add(this.torso);
    this.head = new THREE.Group(); this.head.position.y = 1.08; this.pelvis.add(this.head);
    const hm = new THREE.Mesh(boxGeo(0.72, 0.62, 0.62, 0.2), body); hm.castShadow = true; this.head.add(hm);
    for (const x of [-0.16, 0.16]) {
      const e = new THREE.Mesh(new THREE.PlaneGeometry(0.16, 0.2), white); e.position.set(x, 0.04, -0.32); e.rotation.y = Math.PI; this.head.add(e);
      const p = new THREE.Mesh(new THREE.PlaneGeometry(0.08, 0.1), black); p.position.set(x, 0.03, -0.325); p.rotation.y = Math.PI; this.head.add(p);
    }
    this.legs = []; this.arms = [];
    for (const sx of [-1, 1]) {
      const leg = new THREE.Group(); leg.position.set(sx * 0.18, 0.05, 0); this.pelvis.add(leg);
      const lm = new THREE.Mesh(boxGeo(0.24, 0.6, 0.26, 0.08), dark); lm.position.y = -0.3; lm.castShadow = true; leg.add(lm);
      this.legs.push(leg);
      const arm = new THREE.Group(); arm.position.set(sx * 0.46, 0.68, 0); this.pelvis.add(arm);
      const am = new THREE.Mesh(boxGeo(0.18, 0.5, 0.2, 0.07), body); am.position.y = -0.25; am.castShadow = true; arm.add(am);
      this.arms.push(arm);
    }
    this.phase = 0;
  }
  update(dt, speed, air, slide) {
    this.phase += dt * speed * 0.9;
    const p = this.phase;
    if (slide) {
      this.pelvis.rotation.x = -1.1; this.pelvis.position.y = 0.35;
      this.legs.forEach((l) => (l.rotation.x = 1.3)); this.arms.forEach((a) => (a.rotation.x = -2.5));
    } else if (air) {
      this.pelvis.rotation.x = 0.15; this.pelvis.position.y = 0.75;
      this.legs[0].rotation.x = -0.8; this.legs[1].rotation.x = 0.4; this.arms.forEach((a, i) => (a.rotation.x = -2.6 + i * 0.3));
    } else {
      this.pelvis.rotation.x = 0.18; this.pelvis.position.y = 0.75 + Math.abs(Math.sin(p)) * 0.12;
      this.legs[0].rotation.x = Math.sin(p) * 0.9; this.legs[1].rotation.x = -Math.sin(p) * 0.9;
      this.arms[0].rotation.x = -Math.sin(p) * 0.9; this.arms[1].rotation.x = Math.sin(p) * 0.9;
      this.head.rotation.z = Math.sin(p * 0.5) * 0.05;
    }
  }
}

export class RunnerMode {
  constructor(game, cfg) {
    this.game = game; this.cfg = cfg;
    this.inputKind = 'runner';
    this.theme = THEMES[cfg.theme] || THEMES.arcade;
    this.length = cfg.length || 600;
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(62, 1, 0.1, 260);
    this.t = 0; this.dead = false; this.won = false; this.paused = false;
    this.checkpoint = cfg.checkpointAt || 0;
    this._build();
  }

  _build() {
    const s = this.scene, th = this.theme;
    s.background = new THREE.Color(th.bg); s.fog = new THREE.Fog(th.fog, 25, 110);
    this.hemi = new THREE.HemisphereLight(0xc0c8ff, 0x201030, 0.9); s.add(this.hemi);
    const key = this.key = new THREE.DirectionalLight(0xffffff, 1.8);
    key.position.set(-6, 14, 6); key.castShadow = true;
    const sz = this.game.fx.shadowSize; key.shadow.mapSize.set(sz, sz);
    const c = key.shadow.camera; c.left = -12; c.right = 12; c.top = 20; c.bottom = -20; c.near = 1; c.far = 50;
    key.shadow.bias = -0.0008;
    s.add(key); s.add(key.target);
    this.l1 = new THREE.PointLight(th.light, 50, 30, 1.5); s.add(this.l1);
    this.l2 = new THREE.PointLight(th.light2, 50, 30, 1.5); s.add(this.l2);
    this.redL = new THREE.PointLight(0xff1020, 0, 25, 1.5); s.add(this.redL);
    // matériaux
    const pal = PALETTES.classic;
    this.blockMats = TYPES.map((t) => MATERIALS.glossy.make(pal[t]));
    this.floorMat = new THREE.MeshStandardMaterial({ color: th.floor, roughness: 0.5, metalness: 0.2, map: checkerTex('#ffffff', '#cfcfe0', null) });
    this.floorMat.map.repeat.set(1, 3);
    if (this.cfg.theme === 'void') this.floorMat.map = checkerTex('#202020', '#101010', '#ff2030');
    if (this.cfg.theme === 'board') this.floorMat.map = checkerTex('#e0e6ff', '#c4ccff', '#29d9ff');
    this.wallMat = new THREE.MeshStandardMaterial({ color: th.wall, roughness: 0.6, metalness: 0.2 });
    this.darkMat = monsterMaterial();
    this.obstacleMat = MATERIALS.glossy.make(0xff3b5c);
    this.beamMat = MATERIALS.glossy.make(0xffd21f);
    this.chunks = [];
    for (let i = 0; i < NCHUNK; i++) this.chunks.push(this._chunk(i));
    // sol de secours sous le niveau (lueur)
    const abyss = new THREE.Mesh(new THREE.PlaneGeometry(200, 600), new THREE.MeshBasicMaterial({ color: this.cfg.theme === 'collapse' || this.cfg.theme === 'void' ? 0x300008 : 0x05040c }));
    abyss.rotation.x = -Math.PI / 2; abyss.position.set(0, -14, -200); s.add(abyss);
    this.abyss = abyss;
    // joueur
    this.avatar = new Avatar(); s.add(this.avatar.root);
    this.avatar.root.rotation.y = 0;
    this.p = { lane: 1, x: 0, y: 0, vy: 0, z: 0, speed: this.cfg.speed || 12, air: false, slideT: 0, stun: 0, queued: null };
    // monstre
    this.monster = new Monster(this.cfg.monster || 'humanoid');
    const ms = this.cfg.monster === 'crawler' ? 1.5 : this.cfg.monster === 'king' ? 0.7 : 0.62;
    this.monster.root.scale.setScalar(ms);
    this.monster.root.rotation.y = Math.PI;
    this.monster.setState(this.cfg.monster === 'crawler' ? 'run' : 'run');
    s.add(this.monster.root);
    this.gap = 9; this.monsterMode = 'chase'; this.mT = 0; this.ambushLane = 1;
    this.particles = new Particles(s, 500);
    this.obstacles = [];
    this._generate();
  }

  _chunk(i) {
    const g = new THREE.Group(); this.scene.add(g);
    const th = this.cfg.theme;
    const tiles = [];
    for (let l = 0; l < 3; l++) for (let r = 0; r < 4; r++) {
      const m = new THREE.Mesh(boxGeo(2.3, 0.6, 5.9, 0.12, 2), this.floorMat);
      m.position.set(LANES[l], -0.3, -r * 6 - 3); m.receiveShadow = true; g.add(m);
      tiles.push({ m, lane: l, r, base: m.position.clone(), fall: 0 });
    }
    // murs / décor selon le thème
    const deco = new THREE.Group(); g.add(deco);
    for (const sx of [-1, 1]) {
      if (th === 'arcade') {
        for (let k = 0; k < 3; k++) {
          const cab = new THREE.Group();
          const body = new THREE.Mesh(boxGeo(1.6, 3.6, 1.6, 0.25), this.wallMat); body.position.y = 1.8; body.castShadow = true; cab.add(body);
          const scr = new THREE.Mesh(new THREE.PlaneGeometry(1.1, 0.9), new THREE.MeshBasicMaterial({ color: new THREE.Color(pick([0x29d9ff, 0xff4fd8, 0xffd21f, 0x3be36b])).multiplyScalar(1.6), toneMapped: false }));
          scr.position.set(-sx * 0.81, 2.5, 0); scr.rotation.y = -sx * Math.PI / 2; cab.add(scr);
          cab.position.set(sx * 5.6, 0, -k * 8 - 4); cab.userData.lean = rand(10); deco.add(cab);
        }
        const strip = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.12, CHUNK), new THREE.MeshBasicMaterial({ color: new THREE.Color(sx > 0 ? 0xff4fd8 : 0x29d9ff).multiplyScalar(2), toneMapped: false }));
        strip.position.set(sx * 4.2, 5.5, -CHUNK / 2); deco.add(strip);
      } else if (th === 'board' || th === 'abstract') {
        for (let k = 0; k < 4; k++) {
          const t = pick(TYPES); const grp = new THREE.Group();
          for (const [c, r] of SHAPES[t][randi(0, 3)]) { const m = new THREE.Mesh(blockGeo(), this.blockMats[TYPES.indexOf(t)]); m.position.set(c, -r, 0); m.castShadow = true; grp.add(m); }
          const sc = th === 'abstract' ? rand(2.2, 3.6) : 1.8;
          grp.scale.setScalar(sc); grp.position.set(sx * rand(5.5, 8), rand(1, 6), -k * 6 - rand(0, 4)); grp.rotation.set(rand(-0.3, 0.3), rand(-0.5, 0.5), rand(-0.3, 0.3));
          grp.userData.spin = rand(-0.3, 0.3); deco.add(grp);
        }
      } else {
        // pylônes sombres / grille corrompue
        for (let k = 0; k < 3; k++) {
          const h = rand(3, 9);
          const pil = new THREE.Mesh(boxGeo(1.2, h, 1.2, 0.1), th === 'void' ? this.darkMat : this.wallMat);
          pil.position.set(sx * rand(5, 7), h / 2 - 1, -k * 8 - rand(0, 4)); pil.castShadow = true; deco.add(pil);
        }
        if (th === 'void') {
          const line = new THREE.Mesh(new THREE.BoxGeometry(0.06, 0.06, CHUNK), new THREE.MeshBasicMaterial({ color: new THREE.Color(3, 0.1, 0.1), toneMapped: false }));
          line.position.set(sx * 3.7, 0.02, -CHUNK / 2); deco.add(line);
        }
      }
    }
    g.position.z = -i * CHUNK;
    return { g, tiles, deco, index: i };
  }

  // ——— génération des obstacles ———
  _generate() {
    const L = this.length;
    const obs = this.obstacles;
    const add = (o) => { obs.push(o); return o; };
    const mix = this.cfg.mix || ['low', 'high', 'wall', 'fall', 'gap', 'door'];
    let z = 40;
    const script = (this.cfg.script || []).slice().sort((a, b) => a.at - b.at);
    while (z < L - 30) {
      const prog = z / L;
      const sc = script.find((s) => !s.done && s.at <= prog);
      if (sc) {
        sc.done = true;
        if (sc.type === 'hide') { add({ type: 'hide', z, lane: 1 }); z += 46; continue; }
        if (sc.type === 'lock') { add({ type: 'lock', z, shape: sc.shape || pick(['T', 'L', 'S', 'J', 'Z']) }); z += 30; continue; }
        if (sc.type === 'ambush' || sc.type === 'surge' || sc.type === 'vanish' || sc.type === 'wallcrawl' || sc.type === 'checkpoint' || sc.type === 'say' || sc.type === 'leap' || sc.type === 'collapse') { add({ type: 'event', ev: sc.type, z, data: sc }); z += 8; continue; }
      }
      const diff = clamp(prog * 1.2 + (this.cfg.difficulty || 0), 0, 1);
      const kind = pick(mix);
      const lanes = [0, 1, 2];
      if (kind === 'low' || kind === 'high') {
        const n = chance(0.3 + diff * 0.4) ? 3 : randi(1, 2);
        const ls = n === 3 ? lanes : lanes.sort(() => Math.random() - 0.5).slice(0, n);
        for (const l of ls) add({ type: kind, z, lane: l });
      } else if (kind === 'wall') {
        const free = randi(0, 2);
        for (const l of lanes) if (l !== free) add({ type: 'wall', z, lane: l });
      } else if (kind === 'fall') {
        add({ type: 'fall', z, lane: randi(0, 2) });
        if (diff > 0.4 && chance(0.5)) add({ type: 'fall', z: z + 6, lane: randi(0, 2) });
      } else if (kind === 'gap') {
        const gz = Math.round(z / 6) * 6;
        if (chance(0.5)) add({ type: 'gap', z: gz, lane: -1, len: 6 }); else add({ type: 'gap', z: gz, lane: randi(0, 2), len: 6 });
      } else if (kind === 'door') {
        add({ type: 'door', z, open: randi(0, 2), slide: chance(0.35) });
      }
      z += rand(15, 22) - diff * 5;
    }
    add({ type: 'exit', z: L });
    for (const o of obs) this._spawnObstacle(o);
    for (const c of this.chunks) this._refreshTiles(c);
  }

  _spawnObstacle(o) {
    const s = this.scene;
    const x = o.lane >= 0 ? LANES[o.lane] : 0;
    let m = null;
    if (o.type === 'low') {
      m = new THREE.Group();
      for (const dx of [-0.55, 0.55]) { const b = new THREE.Mesh(blockGeo(1.05, 0.15), this.blockMats[randi(0, 6)]); b.position.set(dx, 0.53, 0); b.castShadow = true; m.add(b); }
    } else if (o.type === 'high') {
      m = new THREE.Group();
      const beam = new THREE.Mesh(boxGeo(2.3, 0.7, 0.7, 0.2), this.beamMat); beam.position.y = 1.85; beam.castShadow = true; m.add(beam);
      for (const dx of [-1.05, 1.05]) { const p = new THREE.Mesh(boxGeo(0.25, 2.2, 0.25, 0.08), this.wallMat); p.position.set(dx, 1.1, 0); m.add(p); }
    } else if (o.type === 'wall') {
      m = new THREE.Group();
      const ti = randi(0, 6);
      for (let k = 0; k < 3; k++) { const b = new THREE.Mesh(blockGeo(1.15, 0.18), this.blockMats[ti]); b.position.set(k % 2 ? 0.55 : -0.55, 0.58 + Math.floor(k / 2) * 1.15, 0); b.castShadow = true; m.add(b); }
      const b = new THREE.Mesh(blockGeo(1.15, 0.18), this.blockMats[ti]); b.position.set(0.55, 1.73, 0); m.add(b);
    } else if (o.type === 'fall') {
      m = new THREE.Group();
      const t = pick(['O', 'T', 'L']);
      const blk = new THREE.Group();
      for (const [c, r] of SHAPES[t][0]) { const b = new THREE.Mesh(blockGeo(), this.blockMats[TYPES.indexOf(t)]); b.position.set(c - 1, -r + 1, 0); b.castShadow = true; blk.add(b); }
      blk.scale.setScalar(0.85); blk.position.y = 30; m.add(blk); m.userData.blk = blk;
      const sh = new THREE.Mesh(new THREE.CircleGeometry(1.2, 24), new THREE.MeshBasicMaterial({ color: 0xff2030, transparent: true, opacity: 0.0, depthWrite: false }));
      sh.rotation.x = -Math.PI / 2; sh.position.y = 0.03; m.add(sh); m.userData.shadow = sh;
      o.landed = false;
    } else if (o.type === 'door') {
      m = new THREE.Group();
      const frameMat = this.wallMat;
      const top = new THREE.Mesh(boxGeo(8.6, 0.8, 0.8, 0.2), frameMat); top.position.y = 3.6; m.add(top);
      for (const dx of [-4.0, 4.0]) { const p = new THREE.Mesh(boxGeo(0.8, 4, 0.8, 0.2), frameMat); p.position.set(dx, 2, 0); m.add(p); }
      o.panels = [];
      for (let l = 0; l < 3; l++) {
        const pm = new THREE.Mesh(boxGeo(2.3, 3.2, 0.4, 0.12), l === o.open ? MATERIALS.glossy.make(0x3be36b) : MATERIALS.glossy.make(0xff3b5c));
        pm.position.set(LANES[l], l === o.open ? 5.2 : 1.6, 0); pm.castShadow = true; m.add(pm);
        o.panels.push(pm);
      }
      const lamp = new THREE.Mesh(new THREE.SphereGeometry(0.25, 12, 8), new THREE.MeshBasicMaterial({ color: new THREE.Color(0, 3, 0.6), toneMapped: false }));
      lamp.position.set(LANES[o.open], 4.3, 0.4); m.add(lamp); o.lamp = lamp;
    } else if (o.type === 'hide') {
      m = new THREE.Group();
      o.locker = new THREE.Group();
      const lk = new THREE.Mesh(boxGeo(2.0, 3.4, 1.4, 0.15), new THREE.MeshStandardMaterial({ color: 0x4a5470, metalness: 0.7, roughness: 0.35 }));
      lk.position.y = 1.7; lk.castShadow = true; o.locker.add(lk);
      for (let k = 0; k < 4; k++) { const sl = new THREE.Mesh(new THREE.BoxGeometry(1.2, 0.06, 0.05), new THREE.MeshBasicMaterial({ color: 0x111111 })); sl.position.set(0, 2.6 - k * 0.15, 0.72); o.locker.add(sl); }
      o.locker.position.set(LANES[1], 0, -1.5); m.add(o.locker);
      const sign = new THREE.Mesh(new THREE.PlaneGeometry(2.2, 0.6), new THREE.MeshBasicMaterial({ color: new THREE.Color(0, 2.5, 0.8), toneMapped: false }));
      sign.position.set(LANES[1], 3.8, -0.7); m.add(sign);
    } else if (o.type === 'lock') {
      m = new THREE.Group();
      const wall = new THREE.Mesh(boxGeo(8.6, 5, 0.9, 0.2), this.wallMat); wall.position.y = 2.5; wall.castShadow = true; m.add(wall);
      const hole = new THREE.Group();
      for (const [c, r] of SHAPES[o.shape][0]) { const b = new THREE.Mesh(new THREE.BoxGeometry(0.62, 0.62, 0.1), new THREE.MeshBasicMaterial({ color: new THREE.Color(3, 2.4, 0.2), toneMapped: false })); b.position.set((c - 1) * 0.65, (-r + 0.5) * 0.65, 0.48); hole.add(b); }
      hole.position.y = 2.7; m.add(hole); o.door = wall;
    } else if (o.type === 'exit') {
      m = new THREE.Group();
      const portal = new THREE.Mesh(new THREE.TorusGeometry(3.2, 0.35, 12, 48), new THREE.MeshBasicMaterial({ color: new THREE.Color(3, 2.6, 0.6), toneMapped: false }));
      portal.position.y = 2.6; m.add(portal);
      const disc = new THREE.Mesh(new THREE.CircleGeometry(3, 40), new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.6 }));
      disc.position.y = 2.6; m.add(disc);
    }
    if (m) { m.position.set(o.type === 'door' || o.type === 'lock' || o.type === 'exit' || o.type === 'hide' ? 0 : x, 0, -o.z); s.add(m); o.mesh = m; }
  }

  // ——— cycle ———
  enter() {
    const g = this.game;
    g.fx.setView(this.scene, this.camera);
    g.ui.showRunHud(true, g.save.s.controls === 'buttons');
    g.audio.playMusic('chase');
    g.audio.layer('heartbeat', 0.5);
    g.fx.radial(0.35);
    this.cfg.onStart && this.cfg.onStart(this);
    if (this.checkpoint > 0) this._jumpTo(this.checkpoint);
  }
  exit() {
    const g = this.game;
    g.ui.showRunHud(false); g.ui.hideBtn(false); g.ui.lockPuzzle(null);
    g.audio.layer('heartbeat', 0); g.fx.radial(0); g.fx.red(0);
    this.scene.traverse((o) => { if (o.geometry && !o.geometry.parameters?.radius) { /* géométries partagées : conservées */ } });
  }
  _jumpTo(prog) {
    this.p.z = -prog * this.length;
    for (const o of this.obstacles) if (o.z < prog * this.length + 5) o.passed = true;
    this.gap = 10;
  }

  onAction(a, down) {
    if (!down || this.dead || this.won || this.game.paused) return;
    if (this.hiding) { if (this.hiding.phase === 'inside') this.hiding.noise = true; return; }
    if (this.locking) { return; }
    const p = this.p;
    if (p.stun > 0) { p.queued = a; return; }
    this._act(a);
  }
  _act(a) {
    const p = this.p, A = this.game.audio;
    if (a === 'left' && p.lane > 0) { p.lane--; A.sfx('whoosh', { vol: 0.4 }); }
    else if (a === 'right' && p.lane < 2) { p.lane++; A.sfx('whoosh', { vol: 0.4 }); }
    else if (a === 'jump' && !p.air) { p.vy = 9.2; p.air = true; p.slideT = 0; A.sfx('jump'); }
    else if (a === 'slide') { if (p.air) p.vy = -14; p.slideT = 0.75; A.sfx('slide'); }
    else if (a === 'hide' || a === 'tap') { /* rien hors cachette */ }
  }

  hit(severity = 4) {
    const p = this.p;
    if (p.inv > 0) return;
    this.gap -= severity; p.stun = 0.35; p.inv = 1.0;
    this.game.audio.sfx('hit'); this.game.fx.shake(0.5, 0.35); this.game.fx.flash(0xff0000, 0.35); this.game.input.haptic(80, 255);
    this.particles.debris(p.x, 1, p.z - 1, 0xffc928, 8, { floor: 0 });
    this.game.ui.popup('AÏE !', 'red');
    this.monster.setState('anger'); setTimeout(() => this.monster.state === 'anger' && this.monster.setState(this.cfg.monster === 'crawler' ? 'run' : 'run'), 500);
  }

  update(dt) {
    if (this.dead || this.won) { this.monster.update(dt); this.particles.update(dt); this._camera(dt); return; }
    this.t += dt;
    const p = this.p, A = this.game.audio, fx = this.game.fx;
    p.inv = Math.max(0, (p.inv || 0) - dt);
    if (this.hiding) this.hiding.t += dt;
    // vitesse
    const target = this.hiding || this.locking ? 0 : (this.cfg.speed || 12) + (-p.z / this.length) * 4;
    p.speed = damp(p.speed, target, this.hiding || this.locking ? 5 : 2, dt);
    if (p.stun > 0) { p.stun -= dt; p.speed *= 0.97; if (p.stun <= 0 && p.queued) { this._act(p.queued); p.queued = null; } }
    p.z -= p.speed * dt;
    // voies et saut
    p.x = damp(p.x, LANES[p.lane], 16, dt);
    if (p.air) { p.vy -= 26 * dt; p.y += p.vy * dt; if (p.y <= 0 && p.y > -0.6 && !this._overGap()) { p.y = 0; p.air = false; A.sfx('step'); } }
    if (!p.air && this._overGap()) { p.air = true; p.vy = 0; }
    if (p.y < -2.5) { this._fall(); return; }
    p.slideT = Math.max(0, p.slideT - dt);
    // pas
    this.stepT = (this.stepT || 0) + dt * p.speed;
    if (this.stepT > 2.6 && !p.air) { this.stepT = 0; A.sfx('step', { vol: 0.5 }); }
    this.avatar.root.position.set(p.x, p.y, p.z);
    this.avatar.root.rotation.z = (LANES[p.lane] - p.x) * -0.12;
    this.avatar.update(dt, p.speed, p.air, p.slideT > 0);
    this.avatar.root.visible = !(this.hiding && this.hiding.phase === 'inside') && !(p.inv > 0 && Math.floor(this.t * 20) % 2);

    // obstacles
    for (const o of this.obstacles) this._obstacle(o, dt);
    // tronçons recyclés
    for (const c of this.chunks) {
      if (c.g.position.z - CHUNK > p.z + 20) { c.g.position.z -= NCHUNK * CHUNK; this._refreshTiles(c); }
      this._animChunk(c, dt);
    }
    // monstre
    this._monster(dt);
    // progression
    const prog = clamp(-p.z / this.length, 0, 1);
    this.game.ui.runStats(prog, clamp(this.gap / 12, 0, 1));
    fx.red(clamp((5 - this.gap) / 5, 0, 0.6));
    A.layer('heartbeat', clamp(1 - this.gap / 12, 0.2, 1));
    // lumières qui suivent
    this.l1.position.set(-4, 4, p.z - 6); this.l2.position.set(4, 4, p.z - 14);
    this.key.position.set(-6, 14, p.z + 6); this.key.target.position.set(0, 0, p.z - 4);
    this.redL.position.set(p.x, 3, p.z + 3); this.redL.intensity = clamp((8 - this.gap) * 8, 0, 60);
    this.abyss.position.z = p.z - 100;
    this.particles.update(dt);
    this._camera(dt);
    this.cfg.onUpdate && this.cfg.onUpdate(dt, this);
  }

  _overGap() {
    const p = this.p, d = -p.z;
    let lane = 0, best = 99;
    LANES.forEach((x, i) => { const k = Math.abs(p.x - x); if (k < best) { best = k; lane = i; } });
    for (const c of this.chunks) {
      const base = -c.g.position.z;
      if (d < base || d >= base + CHUNK) continue;
      const r = Math.floor((d - base) / 6);
      const t = c.tiles.find((tl) => tl.lane === lane && tl.r === r);
      return !!t && (!t.m.visible || t.fall > 0.25);
    }
    return false;
  }

  _refreshTiles(c) {
    const base = -c.g.position.z;
    for (const t of c.tiles) {
      const start = base + t.r * 6;
      let hole = false;
      for (const o of this.obstacles) if (o.type === 'gap' && start < o.z + o.len - 0.01 && start + 6 > o.z + 0.01 && (o.lane < 0 || o.lane === t.lane)) hole = true;
      t.m.visible = !hole; t.fall = 0; t.m.position.copy(t.base); t.m.rotation.set(0, 0, 0);
    }
  }

  _animChunk(c, dt) {
    const th = this.cfg.theme, t = this.t;
    if (th === 'arcade') {
      const warp = this.cfg.warp || 0.3;
      c.deco.children.forEach((d) => { if (d.userData.lean !== undefined) { d.rotation.z = Math.sin(t * 1.3 + d.userData.lean) * 0.12 * warp; d.scale.y = 1 + Math.sin(t * 2 + d.userData.lean) * 0.08 * warp; } });
      c.tiles.forEach((tl) => { if (tl.m.visible) tl.m.position.y = tl.base.y + Math.sin(t * 2 + tl.r + tl.lane) * 0.06 * warp; });
    } else if (th === 'board' || th === 'abstract') {
      c.deco.children.forEach((d) => { if (d.userData.spin) d.rotation.y += d.userData.spin * dt; });
    }
    if (th === 'collapse' || th === 'void') {
      // le sol s'effondre derrière le joueur
      const behind = -c.g.position.z;
      c.tiles.forEach((tl) => {
        const zz = behind + tl.r * 6 + 3;
        if (zz < -this.p.z - 9) { tl.fall += dt; tl.m.position.y = tl.base.y - tl.fall * tl.fall * 9; tl.m.rotation.x = tl.fall * 0.8 * (tl.lane - 1 || 0.5); }
      });
    }
  }

  _obstacle(o, dt) {
    if (o.passed && o.type !== 'fall') return;
    const p = this.p, z = -p.z, dz = o.z - z;
    const A = this.game.audio;
    if (o.type === 'fall' && o.mesh) {
      const tArrive = dz / Math.max(1, p.speed);
      const blk = o.mesh.userData.blk, sh = o.mesh.userData.shadow;
      if (!o.landed && tArrive < 1.4) {
        sh.material.opacity = clamp(1 - tArrive / 1.4, 0, 0.7);
        const k = clamp(1 - (tArrive - 0.25) / 1.15, 0, 1);
        blk.position.y = 30 * (1 - k * k) + 1.2 * k * k;
        if (k >= 1) { o.landed = true; blk.position.y = 1.2; A.sfx('thud'); this.game.fx.shake(0.25, 0.2); this.particles.dust(LANES[o.lane], 0.2, -o.z, 8); sh.material.opacity = 0; }
      }
      if (o.passed) return;
    }
    if (o.type === 'door' && dz < 30 && dz > -2) {
      // la porte ouverte se referme progressivement
      const k = clamp(1 - dz / 30, 0, 1);
      const pm = o.panels[o.open];
      pm.position.y = 5.2 - (o.slide ? 2.0 : 1.0) * k * k;
      if (dz < 12 && !o.creak) { o.creak = true; A.sfx('door'); }
    }
    if (o.type === 'event' && dz < 0) { o.passed = true; this._event(o); return; }
    if (o.type === 'hide' && dz < 14 && !this.hiding) { o.passed = true; this._startHide(o); return; }
    if (o.type === 'lock' && dz < 6 && !this.locking) { o.passed = true; this._startLock(o); return; }
    if (o.type === 'exit' && dz < 0) { o.passed = true; this._win(); return; }
    if (o.type !== 'low' && o.type !== 'high' && o.type !== 'wall' && o.type !== 'fall' && o.type !== 'door') return;
    if (dz > 0.4 || dz < -0.6) return;
    o.passed = true;
    const inLane = o.lane === p.lane;
    const sliding = p.slideT > 0, jumping = p.y > 0.7;
    let hit = false;
    if (o.type === 'low' && inLane && !jumping) hit = true;
    if (o.type === 'high' && inLane && !sliding) hit = true;
    if (o.type === 'wall' && inLane) hit = true;
    if (o.type === 'fall' && inLane && o.landed && !(p.y > 1.9)) hit = true;
    if (o.type === 'door') {
      if (p.lane !== o.open) hit = true;
      else if (o.slide && !sliding) hit = true;
      if (!hit) { A.sfx('door', { vol: 0.6 }); o.panels[o.open].position.y = 1.6; }
    }
    if (hit) {
      this.hit(o.type === 'wall' || o.type === 'door' ? 5 : 3.5);
      if (o.mesh && o.type !== 'door') { this.particles.debris(o.mesh.position.x, 1, o.mesh.position.z, 0xff3b5c, 10, { floor: 0 }); o.mesh.visible = false; }
    } else if (o.type !== 'gap') {
      this.dodges = (this.dodges || 0) + 1;
      if (o.type === 'wall' || o.type === 'fall') this.monsterSmash = o;
    }
  }

  // ——— monstre ———
  _monster(dt) {
    const p = this.p, m = this.monster, A = this.game.audio, fx = this.game.fx;
    this.mT += dt;
    if (this.monsterMode === 'chase') {
      // récupération progressive de la distance
      if (!this.hiding && !this.locking) this.gap = Math.min(12, this.gap + dt * (this.cfg.regen || 0.55));
      if (this.surgeT > 0) { this.surgeT -= dt; this.gap -= dt * 3.2; }
      const mz = p.z + 1.6 + this.gap * 0.85;
      const lead = this.cfg.monster === 'crawler' ? 0.6 : 0;
      m.root.position.x = damp(m.root.position.x, this.wallSide ? this.wallSide * 4.3 : p.x * 0.85, 4, dt);
      m.root.position.z = mz - lead;
      m.root.position.y = this.wallSide ? 2.2 : 0;
      m.root.rotation.z = this.wallSide ? -this.wallSide * Math.PI / 2 : 0;
      m.root.rotation.y = Math.PI;
      m.speed = p.speed / 11;
      if (this.leapT > 0) { this.leapT -= dt; m.root.position.y = Math.sin((1 - this.leapT / 0.8) * Math.PI) * 3.2; }
      this.mStepT = (this.mStepT || 0) + dt * p.speed;
      if (this.mStepT > 4.5) { this.mStepT = 0; A.sfx(this.cfg.monster === 'crawler' ? 'skitter' : 'monsterStep', { vol: clamp(1.2 - this.gap / 12, 0.2, 1) }); }
      // le monstre bondit par-dessus / détruit l'obstacle esquivé
      if (this.monsterSmash && this.monsterSmash.mesh && this.monsterSmash.mesh.visible && mz < -this.monsterSmash.z + 1) {
        const o = this.monsterSmash; this.monsterSmash = null;
        if (chance(0.5)) { this.leapT = 0.8; A.sfx('whoosh'); }
        else { o.mesh.visible = false; this.particles.debris(o.mesh.position.x, 1.2, o.mesh.position.z, 0xff3b5c, 14, { floor: 0 }); A.sfx('punch', { vol: 0.6 }); fx.shake(0.2, 0.2); }
      }
      if (this.wallSide && this.wallT !== undefined) {
        this.wallT -= dt;
        if (this.wallT <= 0) {
          // il bondit du mur sur la voie du côté
          const lane = this.wallSide < 0 ? 0 : 2;
          if (p.lane === lane) this.hit(5);
          A.sfx('roar'); fx.shake(0.4, 0.3);
          this.wallSide = 0; this.wallT = undefined;
          this.game.ui.prompt('');
        }
      }
      if (this.gap <= 0) this._caught();
    } else if (this.monsterMode === 'vanished') {
      m.root.visible = false;
      this.mT2 -= dt;
      if (this.mT2 <= 0) {
        // réapparition en embuscade devant
        this.monsterMode = 'ambush'; this.ambushLane = p.lane; this.ambushZ = p.z - 26;
        m.root.visible = true; m.setState('reach'); m.root.rotation.y = 0;
        m.root.position.set(LANES[this.ambushLane], 0, this.ambushZ);
        A.sfx('stinger'); fx.glitch(0.8); fx.aberration(0.02); fx.flash(0xff0000, 0.25);
        this.game.ui.prompt('ÉVITE-LE !');
      }
    } else if (this.monsterMode === 'ambush') {
      m.root.position.x = damp(m.root.position.x, LANES[this.ambushLane], 2, dt);
      const dz = p.z - this.ambushZ;
      if (dz < 0.8) {
        this.game.ui.prompt('');
        if (Math.abs(p.x - m.root.position.x) < 1.3) { this.gap = Math.min(this.gap, 3); this.hit(4); }
        else { A.sfx('whoosh'); this.game.ui.popup('ESQUIVÉ !', 'gold'); }
        this.monsterMode = 'chase'; this.gap = Math.max(this.gap, 6); m.setState('run'); fx.glitch(0.6);
      }
    }
    m.lookAt(this.avatar.root.position.clone().add(new THREE.Vector3(0, 1, 0)));
    m.update(dt);
  }

  _event(o) {
    const A = this.game.audio, fx = this.game.fx, m = this.monster;
    const ev = o.ev;
    if (ev === 'surge') { this.surgeT = 1.6; A.sfx('roar'); fx.shake(0.4, 0.5); this.game.ui.popup('IL ACCÉLÈRE !', 'red'); m.setState('run'); m.speed = 2; }
    else if (ev === 'vanish' || ev === 'ambush') { this.monsterMode = 'vanished'; this.mT2 = 1.6; fx.glitch(0.9); A.sfx('glitch'); A.sfx('reverse'); m.root.visible = false; this.game.ui.popup('…OÙ EST-IL ?', 'red'); }
    else if (ev === 'wallcrawl') {
      this.wallSide = chance(0.5) ? -1 : 1; this.wallT = 2.2; m.setState('crawl'); A.sfx('skitter'); A.sfx('skitter', { delay: 0.3 });
      this.gap = Math.min(this.gap, 4);
      this.game.ui.prompt(this.wallSide < 0 ? 'IL RAMPE À GAUCHE → VA À DROITE' : 'IL RAMPE À DROITE → VA À GAUCHE');
      setTimeout(() => { if (this.wallT === undefined) m.setState('run'); }, 2600);
    }
    else if (ev === 'leap') { this.leapT = 0.8; A.sfx('roar', { vol: 0.6 }); }
    else if (ev === 'checkpoint') { this.checkpoint = o.z / this.length; this.game.ui.toast('Point de passage'); }
    else if (ev === 'say') { this.game.say(o.data.text, { dur: o.data.dur || 2, cls: o.data.cls }); }
    else if (ev === 'collapse') {
      // effondrement soudain devant
      const gz = Math.ceil((o.z + 30) / 6) * 6;
      for (let l = 0; l < 3; l++) if (l !== this.p.lane) this.obstacles.push({ type: 'gap', z: gz, lane: l, len: 12 });
      A.sfx('crack'); A.sfx('thud'); fx.shake(0.5, 0.6);
      this.chunks.forEach((c) => this._refreshTiles(c));
      this.game.ui.popup('LE SOL CÈDE !', 'red');
    }
    this.cfg.onEvent && this.cfg.onEvent(ev, this);
  }

  // ——— cachette ———
  _startHide(o) {
    const g = this.game, A = g.audio;
    this.hiding = { o, phase: 'approach', t: 0, noise: false };
    this.p.lane = 1;
    g.ui.prompt('CACHE-TOI !');
    g.ui.hideBtn(true, () => {
      if (this.hiding && this.hiding.phase === 'approach') {
        this.hiding.phase = 'inside'; this.hiding.t = 0; g.ui.hideBtn(false); g.ui.prompt('');
        A.sfx('door', { vol: 0.5 }); g.ui.lockerView(true);
        g.say('NE BOUGE PLUS. NE TOUCHE À RIEN.', { dur: 2.5, cls: 'white small' });
        A.layer('heartbeat', 1); A.layer('whispers', 0.4);
      }
    });
    const run = async () => {
      const S = g.sched;
      await S.until(() => this.hiding && (this.hiding.phase === 'inside' || this.hiding.t > 3.2));
      this.hiding.t = 0;
      if (this.hiding.phase !== 'inside') { g.ui.hideBtn(false); g.ui.prompt(''); this.hiding = null; this.gap = 0; this._caught(); return; }
      // le monstre passe et cherche
      const m = this.monster;
      this.monsterMode = 'search';
      m.root.visible = true; m.setState('observe'); m.root.rotation.y = 0;
      m.root.position.set(LANES[0] - 1.2, 0, this.p.z + 10);
      for (let i = 0; i < 70; i++) {
        await S.wait(0.05);
        m.root.position.z -= 0.22; m.root.rotation.y = Math.sin(i * 0.1) * 0.8 + Math.PI;
        if (i % 12 === 0) A.sfx('monsterStep', { vol: 0.8 });
        if (i === 30) { A.sfx('breath'); g.fx.shake(0.1, 1.5); }
        if (this.hiding.noise) break;
      }
      g.ui.lockerView(false); A.layer('whispers', 0);
      if (this.hiding.noise) { this.hiding = null; this.monsterMode = 'chase'; this.gap = 0; this._caught(); return; }
      this.hiding = null;
      this.monsterMode = 'chase'; this.gap = 11; m.setState('run');
      g.ui.popup('IL EST PASSÉ…', 'gold');
      A.sfx('roar', { delay: 1.5, vol: 0.7 });
    };
    run().catch(() => {});
  }

  // ——— énigme de serrure ———
  _startLock(o) {
    const g = this.game, A = g.audio;
    let rot = randi(1, 3);
    this.locking = { o, rot };
    const solve = (ok) => {
      if (!this.locking) return;
      if (ok) {
        A.sfx('unlock'); g.ui.popup('OUVERT !', 'gold'); g.ui.lockPuzzle(null);
        o.door.position.y = -3; this.particles.sparks(0, 2.7, -o.z, 0xffd21f, 30, 10);
        this.locking = null; this.p.speed = 6;
        this.cfg.onEvent && this.cfg.onEvent('lockSolved', this);
      } else {
        A.sfx('rotateFail'); g.fx.shake(0.2, 0.2); this.gap -= 1.5; g.ui.popup('MAUVAISE FORME', 'red');
      }
    };
    g.ui.lockPuzzle({
      shape: o.shape, get rot() { return rot; },
      rotate: () => { rot = (rot + 1) % 4; A.sfx('rotate'); },
      insert: () => solve(rot === 0 || (o.shape === 'S' || o.shape === 'Z') && rot === 2),
    });
    g.say('LA SERRURE : TOURNE LA PIÈCE POUR QU\'ELLE CORRESPONDE', { dur: 2.5, cls: 'white small' });
    this.lockTick = setInterval(() => { if (!this.locking) { clearInterval(this.lockTick); return; } if (!g.paused) { this.gap -= 0.4; this.monsterMode = 'chase'; if (this.gap <= 0) { clearInterval(this.lockTick); g.ui.lockPuzzle(null); this.locking = null; this._caught(); } } }, 300);
  }

  _fall() {
    this.game.audio.sfx('whoosh'); this.dead = true;
    this.game.fx.fade(1);
    setTimeout(() => this._failModal('TU ES TOMBÉ DANS LE VIDE'), 700);
  }

  _caught() {
    if (this.dead) return;
    this.dead = true;
    const g = this.game, m = this.monster;
    g.ui.prompt(''); g.ui.hideBtn(false); g.ui.lockPuzzle(null);
    this.locking = null; this.hiding = null;
    m.root.visible = true; m.setState('scream');
    m.root.position.set(this.p.x, 0, this.p.z + 1.8); m.root.rotation.set(0, Math.PI, 0);
    this.camShot = 'caught';
    g.jumpscare({ intensity: 1 });
    g.save.p.deaths++; g.save.write();
    setTimeout(() => this._failModal('IL T\'A ATTRAPÉ'), 1700);
  }
  _failModal(title) {
    const g = this.game;
    g.fx.fade(0);
    g.ui.modal({
      title, text: 'Réessayer depuis le dernier point de passage ?',
      buttons: [
        { label: 'Réessayer', primary: true, act: () => { g.ui.modal(null); this.cfg.onRetry ? this.cfg.onRetry(this.checkpoint) : null; } },
        { label: 'Menu principal', act: () => { g.ui.modal(null); g.showMenu(); } },
      ],
    });
  }
  _win() {
    this.won = true;
    this.game.audio.sfx('unlock');
    this.game.fx.flash(0xffffff, 0.9);
    this.cfg.onWin && this.cfg.onWin(this);
  }

  _camera(dt) {
    const cam = this.camera, p = this.p;
    const sens = this.game.save.s.sensitivity;
    if (this.camShot === 'caught') {
      const head = new THREE.Vector3(); this.monster.head.getWorldPosition(head);
      cam.position.set(head.x, head.y, head.z - 2.6);
      cam.lookAt(head);
      return;
    }
    if (this.hiding && this.hiding.phase === 'inside') {
      cam.position.set(LANES[1], 2.2, p.z - 1.1); cam.lookAt(LANES[0] - 2, 1.6, p.z + 6); return;
    }
    const tx = p.x * 0.6, ty = 4.3 + p.y * 0.4, tz = p.z + 7.2;
    cam.position.x = damp(cam.position.x, tx, 6 * sens, dt);
    cam.position.y = damp(cam.position.y, ty, 6, dt);
    cam.position.z = tz;
    cam.lookAt(p.x * 0.8, 1.4 + p.y * 0.3, p.z - 7);
    cam.rotation.z += (LANES[p.lane] - p.x) * 0.015;
  }
}
