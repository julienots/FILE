// Le monstre : créature faite de blocs noirs brillants. Quatre formes (THE BLOCK, THE CRAWLER,
// THE GLITCH, THE KING), squelette procédural, animations saccadées (respiration, observation,
// colère, attaque, course, rampement, transformation, apparition), yeux émissifs, bouche géométrique,
// décomposition / reconstruction du corps et détachement de morceaux (dégâts du boss).
import * as THREE from 'three';
import { blockGeo, monsterMaterial, monsterUniforms } from './materials.js';
import { rand, damp, clamp } from '../core/util.js';

const TETRO = {
  I: [[0, 0], [0, 1], [0, 2], [0, 3]], O: [[0, 0], [1, 0], [0, 1], [1, 1]], T: [[0, 0], [1, 0], [2, 0], [1, 1]],
  L: [[0, 0], [0, 1], [0, 2], [1, 0]], S: [[0, 0], [1, 0], [1, 1], [2, 1]],
};
const KING_TINTS = [0x0a0a12, 0x0d0716, 0x07100c, 0x120707, 0x0a0a0a, 0x07101a];

export class Monster {
  constructor(form = 'humanoid', opts = {}) {
    this.form = form;
    this.root = new THREE.Group();
    this.body = new THREE.Group(); this.root.add(this.body);
    this.mat = monsterMaterial();
    this.cubes = []; // {mesh, base: Vector3, scatter: Vector3, baseRot}
    this.state = 'idle'; this.stateT = 0; this.t = 0;
    this.eyeOpen = 1; this.eyeGlow = 1; this.mouthOpen = 0; this.scatter = 0; this.scatterTarget = 0;
    this.lookTarget = null; this.speed = 1; this.anger = 0; this.flicker = 0; this.glitch = 0;
    this.punchT = -1; this.runPhase = 0; this.breath = 1;
    this.detached = [];
    this.shadowCast = opts.shadows !== false;
    if (form === 'block') this._buildBlock();
    else if (form === 'crawler') this._buildCrawler();
    else if (form === 'king') this._buildHumanoid(1, true);
    else this._buildHumanoid(1, false);
    if (form === 'glitch') this.glitch = 0.7;
  }

  // ——— construction ———
  _cube(parent, x, y, z, s = 1, mat = this.mat, jitter = 0.08) {
    const m = new THREE.Mesh(blockGeo(0.94, 0.16, 2), mat);
    m.scale.setScalar(s);
    m.position.set(x + rand(-jitter, jitter) * s, y + rand(-jitter, jitter) * s, z + rand(-jitter, jitter) * s);
    m.rotation.set(rand(-0.06, 0.06), rand(-0.06, 0.06), rand(-0.06, 0.06));
    m.castShadow = this.shadowCast;
    parent.add(m);
    this.cubes.push({ mesh: m, base: m.position.clone(), baseRot: m.rotation.clone(), scatter: new THREE.Vector3(rand(-6, 6), rand(-4, 7), rand(-5, 5)), spin: new THREE.Vector3(rand(-6, 6), rand(-6, 6), rand(-6, 6)) });
    return m;
  }

  _face(head, size, z) {
    // yeux
    this.eyes = [];
    const eyeMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(2.4, 0, 0.02), toneMapped: false });
    const coreMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(3, 1.6, 1.4), toneMapped: false });
    for (const sx of [-1, 1]) {
      const e = new THREE.Group();
      const iris = new THREE.Mesh(new THREE.BoxGeometry(size * 0.26, size * 0.2, 0.08), eyeMat);
      const core = new THREE.Mesh(new THREE.BoxGeometry(size * 0.07, size * 0.07, 0.1), coreMat);
      core.position.set(-sx * size * 0.04, size * 0.03, 0.02);
      e.add(iris, core);
      e.position.set(sx * size * 0.24, size * 0.12, z);
      head.add(e); this.eyes.push(e);
    }
    this.eyeMat = eyeMat; this.coreMat = coreMat;
    // lueur des yeux
    this.eyeLight = new THREE.PointLight(0xff1020, 6 * size, 6 * size, 2);
    this.eyeLight.position.set(0, size * 0.12, z + 0.6);
    head.add(this.eyeLight);
    // bouche en dents de scie
    this.mouth = [];
    const mouthMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(2, 0, 0.03), toneMapped: false });
    const n = 7;
    for (let i = 0; i < n; i++) {
      const tooth = new THREE.Mesh(new THREE.BoxGeometry(size * 0.09, size * 0.09, 0.06), mouthMat);
      const x = (i / (n - 1) - 0.5) * size * 0.62;
      tooth.userData = { x, up: i % 2 === 0 };
      tooth.position.set(x, -size * 0.22 + (i % 2 ? -0.05 : 0.05) * size, z);
      tooth.rotation.z = Math.PI / 4;
      head.add(tooth); this.mouth.push(tooth);
    }
    this.mouthInner = new THREE.Mesh(new THREE.PlaneGeometry(size * 0.6, size * 0.2), new THREE.MeshBasicMaterial({ color: 0x000000 }));
    this.mouthInner.position.set(0, -size * 0.22, z - 0.01); head.add(this.mouthInner);
    this.faceSize = size;
  }

  _buildBlock() {
    const S = 4;
    this.head = new THREE.Group(); this.body.add(this.head);
    const core = new THREE.Mesh(blockGeo(S, 0.5, 4), this.mat); core.castShadow = true; this.head.add(core);
    this.core = core;
    // petits blocs qui frémissent à la surface
    for (let i = 0; i < 18; i++) {
      const f = Math.floor(rand(6));
      const p = new THREE.Vector3(rand(-1.6, 1.6), rand(-1.6, 1.6), rand(-1.6, 1.6));
      p.setComponent(f % 3, (f < 3 ? 1 : -1) * 2.1);
      if (p.z > 1.9) continue; // garder la face avant lisse
      this._cube(this.head, p.x, p.y, p.z, rand(0.4, 0.8), this.mat, 0);
    }
    this._face(this.head, S, S / 2 + 0.03);
    // bras rétractable (pour frapper l'écran)
    this.arm = new THREE.Group(); this.arm.position.set(1.2, -0.6, 0.5); this.head.add(this.arm);
    this.armSegs = [];
    let parent = this.arm;
    for (let i = 0; i < 4; i++) {
      const seg = new THREE.Group(); seg.position.set(0, 0, i === 0 ? 0 : 1.05); parent.add(seg);
      this._cube(seg, 0, 0, 0.5, 0.95, this.mat, 0.03);
      this.armSegs.push(seg); parent = seg;
    }
    // main
    this.hand = new THREE.Group(); this.hand.position.set(0, 0, 1.3); parent.add(this.hand);
    this._cube(this.hand, 0, 0, 0, 1.3, this.mat, 0);
    for (let f = 0; f < 4; f++) {
      const fg = new THREE.Group(); fg.position.set((f - 1.5) * 0.36, 0.2, 0.6); this.hand.add(fg);
      for (let k = 0; k < 3; k++) this._cube(fg, 0, 0, k * 0.36, 0.32, this.mat, 0.02);
    }
    this.armExtend = 0; this.arm.visible = false;
  }

  _buildCrawler() {
    this.spine = [];
    let parent = this.body;
    for (let i = 0; i < 3; i++) {
      const seg = new THREE.Group(); seg.position.set(0, 0, i === 0 ? 0 : -0.9); parent.add(seg);
      this._cube(seg, 0, 0, 0, i === 0 ? 1.0 : 0.85);
      this._cube(seg, 0, 0.45, 0, 0.4);
      this.spine.push(seg); parent = seg;
    }
    this.head = new THREE.Group(); this.head.position.set(0, 0.25, 0.85); this.body.add(this.head);
    this._cube(this.head, 0, 0, 0, 0.95, this.mat, 0);
    this._face(this.head, 0.95, 0.49);
    this.legs = [];
    for (let i = 0; i < 3; i++) for (const sx of [-1, 1]) {
      const hip = new THREE.Group(); hip.position.set(sx * 0.5, 0, 0); this.spine[i].add(hip);
      const up = new THREE.Group(); hip.add(up);
      this._cube(up, sx * 0.45, 0.25, 0, 0.35); this._cube(up, sx * 0.85, 0.35, 0, 0.32);
      const low = new THREE.Group(); low.position.set(sx * 1.05, 0.35, 0); up.add(low);
      this._cube(low, sx * 0.15, -0.35, 0, 0.28); this._cube(low, sx * 0.25, -0.7, 0, 0.24);
      this.legs.push({ hip, up, low, sx, phase: i * 2.1 + (sx > 0 ? Math.PI : 0) });
    }
    this.body.position.y = 0.75;
  }

  _limb(parent, len, n, size, king) {
    // chaîne de blocs vers le bas
    const step = len / n;
    for (let i = 0; i < n; i++) {
      if (king && i % 2 === 0) this._tetro(parent, 0, -step * (i + 0.5), 0, size * 0.55);
      else this._cube(parent, 0, -step * (i + 0.5), 0, size, this._kmat(king));
    }
  }
  _kmat(king) {
    if (!king) return this.mat;
    if (!this.kingMats) this.kingMats = KING_TINTS.map((c) => monsterMaterial({ color: c }));
    return this.kingMats[Math.floor(rand(this.kingMats.length))];
  }
  _tetro(parent, x, y, z, s) {
    const keys = Object.keys(TETRO); const shape = TETRO[keys[Math.floor(rand(keys.length))]];
    const mat = this._kmat(true);
    const g = new THREE.Group(); g.position.set(x, y, z); g.rotation.set(rand(-0.4, 0.4), rand(6), rand(6)); parent.add(g);
    for (const [cx, cy] of shape) this._cube(g, (cx - 0.5) * s, (cy - 1) * s, 0, s, mat, 0.02);
  }

  _buildHumanoid(scale, king) {
    const s = scale;
    const K = king;
    this.hips = new THREE.Group(); this.hips.position.y = 3.1 * s; this.body.add(this.hips);
    this.spine1 = new THREE.Group(); this.spine1.position.y = 0.6 * s; this.hips.add(this.spine1);
    this._cube(this.hips, 0, 0, 0, 1.0 * s, this._kmat(K)); this._cube(this.hips, 0.6 * s, 0, 0, 0.7 * s, this._kmat(K)); this._cube(this.hips, -0.6 * s, 0, 0, 0.7 * s, this._kmat(K));
    this._cube(this.spine1, 0, 0.2 * s, 0, 0.85 * s, this._kmat(K));
    this.chest = new THREE.Group(); this.chest.position.y = 0.95 * s; this.spine1.add(this.chest);
    for (const [x, y] of [[-0.55, 0], [0.55, 0], [-0.55, 0.9], [0.55, 0.9], [0, 0.45]]) this._cube(this.chest, x * s, y * s, rand(-0.1, 0.1), 1.0 * s, this._kmat(K));
    if (K) { for (let i = 0; i < 8; i++) this._tetro(this.chest, rand(-1.4, 1.4) * s, rand(-0.2, 1.4) * s, rand(-0.6, 0.6) * s, 0.5 * s); }
    this.neck = new THREE.Group(); this.neck.position.y = 1.55 * s; this.chest.add(this.neck);
    this._cube(this.neck, 0, 0.15 * s, 0, 0.5 * s);
    this.head = new THREE.Group(); this.head.position.y = 0.95 * s; this.neck.add(this.head);
    const hs = 1.85 * s;
    const hm = new THREE.Mesh(blockGeo(hs, 0.2 * s, 3), this.mat); hm.castShadow = this.shadowCast; this.head.add(hm);
    this.cubes.push({ mesh: hm, base: hm.position.clone(), baseRot: hm.rotation.clone(), scatter: new THREE.Vector3(0, 6, 2), spin: new THREE.Vector3(2, 3, 1) });
    this._cube(this.head, 0.5 * s, 0.75 * s, -0.2 * s, 0.45 * s); this._cube(this.head, -0.55 * s, 0.72 * s, 0.1 * s, 0.35 * s);
    this._face(this.head, hs, hs / 2 + 0.02);
    if (K) {
      // couronne : pointes de blocs
      this.crown = new THREE.Group(); this.crown.position.y = hs * 0.62; this.head.add(this.crown);
      const cm = new THREE.MeshPhysicalMaterial({ color: 0x2a0005, emissive: 0xff1a10, emissiveIntensity: 1.4, roughness: 0.2, clearcoat: 1 });
      for (let i = 0; i < 5; i++) {
        const h = i % 2 === 0 ? 3 : 2;
        for (let k = 0; k < h; k++) this._cube(this.crown, (i - 2) * 0.34 * s, k * 0.32 * s, 0, 0.3 * s, cm, 0.01);
      }
    }
    // bras longs
    this.arms = [];
    for (const sx of [-1, 1]) {
      const sh = new THREE.Group(); sh.position.set(sx * 1.6 * s, 1.15 * s, 0); this.chest.add(sh);
      this._cube(sh, 0, 0, 0, 0.75 * s, this._kmat(K));
      const upper = new THREE.Group(); sh.add(upper);
      this._limb(upper, 2.8 * s, 4, 0.58 * s, K);
      const elbow = new THREE.Group(); elbow.position.y = -2.8 * s; upper.add(elbow);
      this._limb(elbow, 2.8 * s, 4, 0.5 * s, K);
      const hand = new THREE.Group(); hand.position.y = -3.0 * s; elbow.add(hand);
      this._cube(hand, 0, 0, 0, 0.6 * s);
      const fingers = [];
      for (let f = 0; f < 4; f++) {
        const fg = new THREE.Group(); fg.position.set((f - 1.5) * 0.17 * s, -0.25 * s, 0.05 * s); hand.add(fg);
        for (let k = 0; k < 3; k++) this._cube(fg, 0, -k * 0.2 * s, 0, 0.17 * s, this.mat, 0.01);
        fg.rotation.z = (f - 1.5) * 0.12; fingers.push(fg);
      }
      this.arms.push({ sh, upper, elbow, hand, fingers, sx });
    }
    // jambes plus courtes
    this.legs = [];
    for (const sx of [-1, 1]) {
      const hip = new THREE.Group(); hip.position.set(sx * 0.55 * s, -0.3 * s, 0); this.hips.add(hip);
      this._limb(hip, 1.4 * s, 2, 0.7 * s, K);
      const knee = new THREE.Group(); knee.position.y = -1.4 * s; hip.add(knee);
      this._limb(knee, 1.3 * s, 2, 0.62 * s, K);
      this._cube(knee, 0, -1.45 * s, 0.25 * s, 0.6 * s);
      this.legs.push({ hip, knee, sx });
    }
    this.height = 7.5 * s;
  }

  // ——— contrôle ———
  setState(st) { if (this.state !== st) { this.state = st; this.stateT = 0; } }
  lookAt(v) { this.lookTarget = v; }
  punch() { this.punchT = 0; }
  /** 1 = éclaté en morceaux, 0 = assemblé. */
  setScatter(v, instant = false) { this.scatterTarget = v; if (instant) this.scatter = v; }

  /** Détache un morceau du corps (dégâts). Retourne la position monde du morceau. */
  breakChunk(scene) {
    const candidates = this.cubes.filter((c) => c.mesh.visible && c.mesh.parent && !this.eyes.includes(c.mesh.parent) && c.mesh.geometry && c.mesh.scale.x < 1.4);
    if (!candidates.length) return null;
    const c = candidates[Math.floor(rand(candidates.length))];
    const wp = new THREE.Vector3(); c.mesh.getWorldPosition(wp);
    const ws = new THREE.Vector3(); c.mesh.getWorldScale(ws);
    c.mesh.visible = false;
    const m = new THREE.Mesh(c.mesh.geometry, c.mesh.material); m.position.copy(wp); m.scale.copy(ws); m.castShadow = true;
    scene.add(m);
    this.detached.push({ mesh: m, v: new THREE.Vector3(rand(-6, 6), rand(4, 10), rand(2, 8)), w: new THREE.Vector3(rand(-8, 8), rand(-8, 8), rand(-8, 8)), life: 2.5, scene });
    return wp;
  }
  intactRatio() { const v = this.cubes.filter((c) => c.mesh.visible).length; return v / this.cubes.length; }

  update(dt) {
    this.t += dt; this.stateT += dt;
    const t = this.t;
    // animation « saccadée » : temps quantifié + secousses aléatoires
    const tq = Math.floor(t * 12) / 12;
    const twitch = Math.random() < 0.015 ? rand(-0.3, 0.3) : 0;
    monsterUniforms.uGlitch.value = Math.max(this.glitch, this.state === 'transform' ? 0.6 : 0, this.anger * 0.2);
    // yeux
    if (this.eyes) {
      const blink = Math.sin(t * 0.7) > 0.985 ? 0.1 : 1;
      for (const e of this.eyes) e.scale.y = Math.max(0.02, this.eyeOpen * blink);
      const g = this.eyeGlow * (1 + this.anger * 0.8);
      this.eyeMat.color.setRGB(2.4 * g, 0, 0.02);
      if (this.eyeLight) this.eyeLight.intensity = 6 * this.faceSize * this.eyeOpen * g;
    }
    if (this.mouth) {
      const open = this.mouthOpen + (this.state === 'anger' ? 0.4 + Math.sin(t * 30) * 0.2 : 0);
      for (const th of this.mouth) th.position.y = -this.faceSize * 0.22 + (th.userData.up ? 1 : -1) * (0.05 + open * 0.22) * this.faceSize;
      this.mouthInner.scale.y = 0.2 + open * 2.2;
    }
    // décomposition / reconstruction
    this.scatter = damp(this.scatter, this.scatterTarget, 3, dt);
    if (this.scatter > 0.001 || this._wasScattered) {
      this._wasScattered = this.scatter > 0.001;
      const k = this.scatter;
      for (const c of this.cubes) {
        c.mesh.position.set(c.base.x + c.scatter.x * k, c.base.y + c.scatter.y * k, c.base.z + c.scatter.z * k);
        c.mesh.rotation.set(c.baseRot.x + c.spin.x * k, c.baseRot.y + c.spin.y * k, c.baseRot.z + c.spin.z * k);
      }
    }
    // clignotement (forme glitch)
    if (this.form === 'glitch' || this.flicker > 0) {
      const f = Math.max(this.flicker, this.form === 'glitch' ? 0.15 : 0);
      this.body.visible = Math.random() > f * 0.5;
      this.body.position.x = Math.random() < f * 0.3 ? rand(-0.6, 0.6) : 0;
    }
    // morceaux détachés
    for (let i = this.detached.length - 1; i >= 0; i--) {
      const d = this.detached[i]; d.life -= dt; d.v.y -= 18 * dt;
      d.mesh.position.addScaledVector(d.v, dt); d.mesh.rotation.x += d.w.x * dt; d.mesh.rotation.y += d.w.y * dt;
      if (d.life <= 0) { d.scene.remove(d.mesh); this.detached.splice(i, 1); }
    }

    if (this.form === 'block') return this._animBlock(dt, t, tq);
    if (this.form === 'crawler') return this._animCrawler(dt, t);
    return this._animHumanoid(dt, t, tq, twitch);
  }

  _animBlock(dt, t, tq) {
    const br = 1 + Math.sin(t * 2.2) * 0.035 * this.breath;
    this.head.scale.set(br, 1 / br * 1.0 + 0.02, br);
    if (this.state === 'anger') { this.head.position.x = rand(-0.08, 0.08); this.head.position.y = rand(-0.08, 0.08); }
    else { this.head.position.x = damp(this.head.position.x, 0, 8, dt); this.head.position.y = damp(this.head.position.y, 0, 8, dt); }
    if (this.lookTarget) {
      const wp = new THREE.Vector3(); this.root.getWorldPosition(wp);
      const d = this.lookTarget.clone().sub(wp);
      this.head.rotation.y = damp(this.head.rotation.y, clamp(Math.atan2(d.x, d.z), -0.5, 0.5), 3, dt);
      this.head.rotation.x = damp(this.head.rotation.x, clamp(-Math.atan2(d.y, d.z), -0.4, 0.4), 3, dt);
    }
    // bras : sortir / frapper
    if (this.arm) {
      this.arm.visible = this.armExtend > 0.02;
      const ext = this.armExtend;
      this.armSegs.forEach((s, i) => { if (i > 0) s.position.z = 1.05 * ext; });
      this.arm.position.z = 0.5 + ext * 1.2;
      if (this.punchT >= 0) {
        this.punchT += dt;
        const p = this.punchT;
        // armer puis frapper
        const k = p < 0.35 ? -p / 0.35 : p < 0.5 ? -1 + (p - 0.35) / 0.15 * 2.2 : 1.2 - (p - 0.5) * 2;
        this.arm.position.z += k * 1.6;
        if (p > 1.1) this.punchT = -1;
      }
    }
  }

  _animCrawler(dt, t) {
    const sp = this.state === 'run' ? 14 * this.speed : this.state === 'idle' ? 2 : 6;
    this.runPhase += dt * sp;
    for (const L of this.legs) {
      const ph = this.runPhase + L.phase;
      L.up.rotation.z = L.sx * (0.25 + Math.sin(ph) * 0.35);
      L.up.rotation.y = Math.cos(ph) * 0.5;
      L.low.rotation.z = L.sx * Math.max(0, Math.sin(ph)) * 0.5;
    }
    this.spine.forEach((s, i) => { s.rotation.y = Math.sin(this.runPhase * 0.5 - i) * 0.15; });
    this.body.position.y = 0.75 + Math.abs(Math.sin(this.runPhase)) * 0.08;
    if (this.state === 'anger') this.head.rotation.z = Math.sin(t * 40) * 0.15;
    else this.head.rotation.z = damp(this.head.rotation.z, 0, 8, dt);
    if (this.lookTarget) {
      const wp = new THREE.Vector3(); this.head.getWorldPosition(wp);
      const d = this.lookTarget.clone().sub(wp);
      this.head.rotation.x = damp(this.head.rotation.x, clamp(-Math.atan2(d.y, Math.hypot(d.x, d.z)), -0.8, 0.5), 6, dt);
    }
  }

  _animHumanoid(dt, t, tq, twitch) {
    const st = this.state;
    const A = this.arms, L = this.legs;
    const breathe = Math.sin(t * 1.8) * 0.04 * this.breath;
    this.chest.scale.set(1 + breathe * 0.5, 1 + breathe, 1 + breathe * 0.5);
    let lean = 0.38, headX = 0.1, headY = 0, headZ = twitch;
    const arm = [{ x: -0.15, z: 0.32, e: -0.35 }, { x: -0.15, z: -0.32, e: -0.35 }];
    const leg = [{ h: 0, k: 0 }, { h: 0, k: 0 }];
    this.root.position.y = this.baseY || 0;
    if (st === 'idle' || st === 'observe') {
      arm[0].x = -0.15 + Math.sin(tq * 1.1) * 0.08; arm[1].x = -0.15 + Math.sin(tq * 1.1 + 1) * 0.08;
      headZ += Math.sin(tq * 0.7) * 0.15;
      if (st === 'observe') { headZ += 0.35; lean = 0.35; }
    } else if (st === 'anger') {
      lean = 0.45; headX = -0.2 + Math.sin(t * 35) * 0.06; headZ = Math.sin(t * 27) * 0.1;
      arm[0].z = 0.9 + Math.sin(t * 20) * 0.1; arm[1].z = -0.9 - Math.sin(t * 22) * 0.1; arm[0].e = -1.2; arm[1].e = -1.2;
      this.root.position.x = rand(-0.03, 0.03);
    } else if (st === 'run') {
      this.runPhase += dt * 11 * this.speed;
      const p = this.runPhase;
      lean = 0.55;
      leg[0].h = Math.sin(p) * 0.9; leg[1].h = -Math.sin(p) * 0.9;
      leg[0].k = Math.max(0, -Math.cos(p)) * 1.3; leg[1].k = Math.max(0, Math.cos(p)) * 1.3;
      arm[0].x = -Math.sin(p) * 1.1 - 0.4; arm[1].x = Math.sin(p) * 1.1 - 0.4; arm[0].e = -0.9; arm[1].e = -0.9;
      arm[0].z = 0.35; arm[1].z = -0.35;
      this.root.position.y = (this.baseY || 0) + Math.abs(Math.sin(p)) * 0.35;
      headX = -0.3;
    } else if (st === 'crawl') {
      this.runPhase += dt * 9 * this.speed;
      const p = this.runPhase;
      lean = 1.35;
      arm[0].x = -1.6 + Math.sin(p) * 0.6; arm[1].x = -1.6 - Math.sin(p) * 0.6; arm[0].e = -0.3; arm[1].e = -0.3;
      leg[0].h = -0.6 + Math.sin(p) * 0.5; leg[1].h = -0.6 - Math.sin(p) * 0.5; leg[0].k = 1.2; leg[1].k = 1.2;
      headX = -1.0;
      this.root.position.y = (this.baseY || 0) - 1.6;
    } else if (st === 'attack') {
      const p = this.stateT;
      lean = 0.5;
      const wind = Math.min(1, p / 0.35), strike = clamp((p - 0.35) / 0.15, 0, 1);
      arm[1].x = -2.6 * wind + strike * 1.1; arm[1].z = -0.4; arm[1].e = -0.4 * (1 - strike);
      arm[0].x = -0.4; arm[0].e = -1.0;
      headX = -0.25;
      if (p > 1.0) this.setState('idle');
    } else if (st === 'reach') {
      // bras tendus vers la caméra (sortir de l'écran)
      lean = 0.6; headX = -0.15;
      const w = Math.sin(t * 3) * 0.1;
      arm[0].x = -1.45 + w; arm[1].x = -1.45 - w; arm[0].z = 0.15; arm[1].z = -0.15; arm[0].e = -0.15; arm[1].e = -0.15;
    } else if (st === 'scream') {
      lean = 0.15; headX = 0.45 + Math.sin(t * 50) * 0.04;
      arm[0].z = 1.2; arm[1].z = -1.2; arm[0].x = -0.5; arm[1].x = -0.5; arm[0].e = -0.6; arm[1].e = -0.6;
      this.mouthOpen = 1;
    } else if (st === 'transform') {
      lean = 0.3; headZ = Math.sin(t * 13) * 0.5;
    } else if (st === 'slam') {
      const p = this.stateT;
      const up = Math.min(1, p / 0.5), down = clamp((p - 0.5) / 0.15, 0, 1);
      arm[0].x = -3.0 * up + down * 2.4; arm[1].x = -3.0 * up + down * 2.4; arm[0].e = -0.3; arm[1].e = -0.3;
      lean = 0.2 + down * 0.7;
      if (p > 1.4) this.setState('idle');
    }
    // coup de poing ponctuel (bras droit vers la caméra)
    if (this.punchT >= 0) {
      this.punchT += dt; const p = this.punchT;
      const k = p < 0.3 ? p / 0.3 : p < 0.45 ? 1 : Math.max(0, 1 - (p - 0.45) * 2);
      arm[1].x = -1.6 * k + arm[1].x * (1 - k); arm[1].e = -0.05 * k + arm[1].e * (1 - k);
      if (p > 1) this.punchT = -1;
    }
    // regard
    if (this.lookTarget && (st === 'idle' || st === 'observe' || st === 'reach')) {
      const wp = new THREE.Vector3(); this.head.getWorldPosition(wp);
      const d = this.lookTarget.clone().sub(wp);
      const yaw = clamp(Math.atan2(d.x, d.z) - this.root.rotation.y, -0.8, 0.8);
      headY = Math.round(yaw * 6) / 6; // saccadé
      headX += clamp(-Math.atan2(d.y, Math.hypot(d.x, d.z)), -0.5, 0.5) * 0.6;
    }
    const k = 14;
    this.spine1.rotation.x = damp(this.spine1.rotation.x, lean * 0.6, k, dt);
    this.chest.rotation.x = damp(this.chest.rotation.x, lean * 0.4, k, dt);
    this.head.rotation.x = damp(this.head.rotation.x, headX, k, dt);
    this.head.rotation.y = damp(this.head.rotation.y, headY, k, dt);
    this.head.rotation.z = damp(this.head.rotation.z, headZ, k, dt);
    A.forEach((a, i) => {
      a.sh.rotation.x = damp(a.sh.rotation.x, arm[i].x, k, dt);
      a.sh.rotation.z = damp(a.sh.rotation.z, arm[i].z, k, dt);
      a.elbow.rotation.x = damp(a.elbow.rotation.x, arm[i].e, k, dt);
      a.fingers.forEach((f, j) => { f.rotation.x = Math.sin(tq * 3 + j) * 0.25 - (st === 'anger' ? 0.8 : 0.2); });
    });
    L.forEach((l, i) => {
      l.hip.rotation.x = damp(l.hip.rotation.x, leg[i].h, k, dt);
      l.knee.rotation.x = damp(l.knee.rotation.x, leg[i].k, k, dt);
    });
    if (this.crown) this.crown.children.forEach((c, i) => { c.position.y += Math.sin(t * 4 + i) * 0.002; });
  }

  dispose() {
    this.root.parent && this.root.parent.remove(this.root);
    for (const d of this.detached) d.scene.remove(d.mesh);
  }
}
