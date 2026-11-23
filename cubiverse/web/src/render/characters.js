// Rendu des personnages en vraie 3D, par instanciation (quelques appels de dessin
// quel que soit le nombre de personnages) avec squelette procédural.
import * as THREE from 'three';
import { computePose, POSE_KEYS } from './poses.js';
import { dominantEmotion } from '../sim/social.js';
import { CUBE } from '../sim/constants.js';

const PIVOT = 0.3;
const mkMat = (grad, extra = {}) => new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap: grad, ...extra });

class Pool {
  constructor(scene, geo, mat, perChar, castShadow = true) {
    this.scene = scene; this.geo = geo; this.mat = mat; this.per = perChar; this.cap = 0; this.mesh = null; this.cast = castShadow;
    this.ensure(64);
  }
  ensure(nChars) {
    const need = nChars * this.per;
    if (this.mesh && need <= this.cap) return;
    const cap = Math.max(need, this.cap * 2, 64);
    if (this.mesh) { this.scene.remove(this.mesh); this.mesh.dispose(); }
    this.mesh = new THREE.InstancedMesh(this.geo, this.mat, cap);
    this.mesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    this.mesh.castShadow = this.cast; this.mesh.receiveShadow = false;
    this.mesh.frustumCulled = false;
    this.mesh.count = 0;
    this.mesh.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(cap * 3).fill(1), 3);
    this.mesh.instanceColor.setUsage(THREE.DynamicDrawUsage);
    this.scene.add(this.mesh); this.cap = cap;
  }
}

export class CharacterRenderer {
  constructor(scene, gradientMap) {
    this.scene = scene;
    const limb = new THREE.CylinderGeometry(1, 1, 1, 7, 1); limb.translate(0, -0.5, 0);
    const sph = new THREE.SphereGeometry(1, 14, 10);
    const sphLow = new THREE.SphereGeometry(1, 8, 6);
    const torso = new THREE.CapsuleGeometry(1, 1, 4, 8);
    const arc = new THREE.TorusGeometry(1, 0.28, 5, 10, Math.PI); arc.rotateZ(Math.PI);
    const bodyMat = mkMat(gradientMap);
    const dark = new THREE.MeshBasicMaterial({ color: 0x1d1b26 });
    this.limbs = new Pool(scene, limb, bodyMat, 8);
    this.torso = new Pool(scene, torso, bodyMat, 1);
    this.heads = new Pool(scene, sph, mkMat(gradientMap), 1);
    this.ends = new Pool(scene, sphLow, bodyMat, 4);    // mains + pieds
    this.eyes = new Pool(scene, sphLow, dark, 2, false);
    this.brows = new Pool(scene, new THREE.BoxGeometry(1, 1, 1), dark, 2, false);
    this.mouthArc = new Pool(scene, arc, dark, 1, false);
    this.mouthO = new Pool(scene, sphLow, new THREE.MeshBasicMaterial({ color: 0x7a1f2b }), 1, false);
    this.cheeks = new Pool(scene, sphLow, new THREE.MeshBasicMaterial({ color: 0xff8fa3, transparent: true, opacity: 0.55, depthWrite: false }), 2, false);
    this.pose = {}; for (const k of POSE_KEYS) this.pose[k] = 0;
    // matrices de travail
    this.M = {}; for (const n of ['root', 'pelvis', 'torso', 'neck', 'head', 'a', 'b', 'c', 't']) this.M[n] = new THREE.Matrix4();
    this.tmpC = new THREE.Color(); this.tmpC2 = new THREE.Color();
    this.visible = [];   // [{ch, wx, wy, wz}] pour la sélection
    // anneau de sélection
    this.ring = new THREE.Mesh(new THREE.TorusGeometry(0.17, 0.018, 6, 28), new THREE.MeshBasicMaterial({ color: 0xffd23f }));
    this.ring.rotation.x = -Math.PI / 2; this.ring.visible = false; scene.add(this.ring);
    // bulles d'émotion (sprites)
    this.atlas = new EmojiAtlas();
    this.sprites = [];
    for (let i = 0; i < 90; i++) {
      const tex = this.atlas.texture.clone(); tex.repeat.set(1 / 8, 1 / 8);
      const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex, depthWrite: false, transparent: true }));
      s.scale.set(0.3, 0.3, 1); s.visible = false; s.renderOrder = 10; scene.add(s); this.sprites.push(s);
    }
  }

  update(world, list, camera, selectedId, opts) {
    const n = list.length;
    for (const p of [this.limbs, this.torso, this.heads, this.ends, this.eyes, this.brows, this.mouthArc, this.mouthO, this.cheeks]) p.ensure(n);
    const { M } = this; const P = this.pose;
    const R = M.root, T = M.t;
    let iL = 0, iE = 0, iEye = 0, iBrow = 0, iMa = 0, iMo = 0, iCh = 0;
    const col = this.tmpC, head = this.tmpC2;
    this.visible.length = 0;
    let spriteIdx = 0;
    const showBubbles = opts.bubbles;
    for (let i = 0; i < n; i++) {
      const ch = list[i];
      const cube = world.cubes.get(ch.cubeId); if (!cube) continue;
      computePose(ch, P);
      const wx = cube.x * CUBE + ch.x, wy = cube.y * CUBE + ch.y + 0.06, wz = cube.z * CUBE + ch.z;
      this.visible.push({ ch, wx, wy: wy + 0.6, wz });
      col.set(ch.color); head.copy(col).lerp(WHITE, 0.55);
      // racine
      R.makeTranslation(wx, wy, wz);
      R.multiply(T.makeRotationY(ch.ry + P.ry));
      if (P.oy || P.rx) {
        R.multiply(T.makeTranslation(0, PIVOT + P.oy, 0)); R.multiply(T.makeRotationX(P.rx)); R.multiply(T.makeTranslation(0, -PIVOT, 0));
      }
      const pelvis = M.pelvis.copy(R).multiply(T.makeTranslation(0, 0.26 + P.by, 0));
      const torso = M.torso.copy(pelvis).multiply(T.makeRotationX(P.bl)); if (P.br) torso.multiply(T.makeRotationZ(P.br));
      // buste
      M.a.copy(torso).multiply(T.makeTranslation(0, 0.1, 0)).multiply(T.makeScale(0.048, 0.1, 0.042));
      this.torso.mesh.setMatrixAt(i, M.a); this.torso.mesh.setColorAt(i, col);
      // tête
      const neck = M.neck.copy(torso).multiply(T.makeTranslation(0, 0.2, 0));
      const hd = M.head.copy(neck).multiply(T.makeRotationX(P.hp)); if (P.hr) hd.multiply(T.makeRotationZ(P.hr));
      hd.multiply(T.makeTranslation(0, 0.085, 0));
      M.a.copy(hd).multiply(T.makeScale(0.088, 0.088, 0.088));
      this.heads.mesh.setMatrixAt(i, M.a); this.heads.mesh.setColorAt(i, head);
      // visage
      const face = P.face || faceOf(ch);
      const eyeY = face === 'sleep' ? 0.15 : face === 'surprise' ? 1.25 : face === 'laugh' ? 0.35 : 1;
      for (const sx of [1, -1]) {
        M.a.copy(hd).multiply(T.makeTranslation(0.032 * sx, 0.014, 0.074)).multiply(T.makeScale(0.014, 0.019 * eyeY, 0.01));
        this.eyes.mesh.setMatrixAt(iEye++, M.a);
        const browA = face === 'anger' ? -0.5 : face === 'sadness' ? 0.45 : 0;
        const browS = face === 'anger' || face === 'sadness' || face === 'surprise' ? 1 : 0;
        M.a.copy(hd).multiply(T.makeTranslation(0.034 * sx, face === 'surprise' ? 0.052 : 0.045, 0.07)).multiply(T.makeRotationZ(browA * sx)).multiply(T.makeScale(0.03 * browS, 0.007 * browS, 0.008));
        this.brows.mesh.setMatrixAt(iBrow++, M.a);
        const ck = face === 'joy' || face === 'laugh' ? 1 : 0;
        M.a.copy(hd).multiply(T.makeTranslation(0.05 * sx, -0.02, 0.064)).multiply(T.makeScale(0.016 * ck, 0.011 * ck, 0.006 * ck));
        this.cheeks.mesh.setMatrixAt(iCh++, M.a);
      }
      const open = face === 'laugh' || face === 'surprise' || (face === 'talk' && Math.sin(ch.animT * 14) > 0);
      if (open) {
        M.a.copy(hd).multiply(T.makeTranslation(0, -0.03, 0.078)).multiply(T.makeScale(0.02, face === 'surprise' ? 0.022 : 0.016, 0.01));
        this.mouthO.mesh.setMatrixAt(iMo++, M.a);
        this.mouthArc.mesh.setMatrixAt(iMa++, ZERO);
      } else {
        const sad = face === 'sadness' || face === 'anger';
        const w = face === 'joy' ? 0.024 : face === 'sleep' ? 0.012 : 0.018;
        M.a.copy(hd).multiply(T.makeTranslation(0, sad ? -0.044 : -0.022, 0.079));
        if (sad) M.a.multiply(T.makeRotationZ(Math.PI));
        M.a.multiply(T.makeScale(w, face === 'joy' ? w * 0.9 : w * 0.55, 0.012));
        this.mouthArc.mesh.setMatrixAt(iMa++, M.a);
        this.mouthO.mesh.setMatrixAt(iMo++, ZERO);
      }
      // bras
      for (const side of [1, -1]) {
        const ap = side > 0 ? P.aLp : P.aRp, ar = side > 0 ? P.aLr : P.aRr, el = side > 0 ? P.eL : P.eR;
        const sh = M.b.copy(torso).multiply(T.makeTranslation(0.058 * side, 0.175, 0)).multiply(T.makeRotationX(ap)).multiply(T.makeRotationZ(ar * side));
        M.a.copy(sh).multiply(T.makeScale(0.02, 0.11, 0.02)); this.limbs.mesh.setMatrixAt(iL, M.a); this.limbs.mesh.setColorAt(iL++, col);
        sh.multiply(T.makeTranslation(0, -0.11, 0)).multiply(T.makeRotationX(-el));
        M.a.copy(sh).multiply(T.makeScale(0.018, 0.105, 0.018)); this.limbs.mesh.setMatrixAt(iL, M.a); this.limbs.mesh.setColorAt(iL++, col);
        M.a.copy(sh).multiply(T.makeTranslation(0, -0.11, 0)).multiply(T.makeScale(0.03, 0.03, 0.03));
        this.ends.mesh.setMatrixAt(iE, M.a); this.ends.mesh.setColorAt(iE++, head);
      }
      // jambes
      for (const side of [1, -1]) {
        const lp = side > 0 ? P.lLp : P.lRp, kn = side > 0 ? P.kL : P.kR;
        const hp = M.c.copy(pelvis).multiply(T.makeTranslation(0.036 * side, 0, 0)).multiply(T.makeRotationX(lp));
        M.a.copy(hp).multiply(T.makeScale(0.024, 0.13, 0.024)); this.limbs.mesh.setMatrixAt(iL, M.a); this.limbs.mesh.setColorAt(iL++, col);
        hp.multiply(T.makeTranslation(0, -0.13, 0)).multiply(T.makeRotationX(kn));
        M.a.copy(hp).multiply(T.makeScale(0.022, 0.13, 0.022)); this.limbs.mesh.setMatrixAt(iL, M.a); this.limbs.mesh.setColorAt(iL++, col);
        M.a.copy(hp).multiply(T.makeTranslation(0, -0.13, 0.02)).multiply(T.makeScale(0.03, 0.022, 0.05));
        this.ends.mesh.setMatrixAt(iE, M.a); this.ends.mesh.setColorAt(iE++, col);
      }
      // bulle
      if (showBubbles && ch.bubble && spriteIdx < this.sprites.length) {
        const s = this.sprites[spriteIdx++];
        const slot = this.atlas.slot(ch.bubble.icon);
        s.material.map.offset.set((slot % 8) / 8, 1 - (Math.floor(slot / 8) + 1) / 8);
        s.position.set(wx, wy + 0.86 + Math.sin(ch.animT * 3) * 0.015, wz);
        const k = Math.min(1, ch.bubble.t * 4);
        s.scale.set(0.34 * k, 0.34 * k, 1); s.visible = true;
      }
      if (ch.id === selectedId) { this.ring.visible = true; this.ring.position.set(wx, cube.y * CUBE + 0.075, wz); this.ring.rotation.z += 0.03; }
    }
    if (!list.some((c) => c.id === selectedId)) this.ring.visible = false;
    for (let i = spriteIdx; i < this.sprites.length; i++) this.sprites[i].visible = false;
    this.limbs.mesh.count = iL; this.torso.mesh.count = n; this.heads.mesh.count = n; this.ends.mesh.count = iE;
    this.eyes.mesh.count = iEye; this.brows.mesh.count = iBrow; this.mouthArc.mesh.count = iMa; this.mouthO.mesh.count = iMo; this.cheeks.mesh.count = iCh;
    for (const p of [this.limbs, this.torso, this.heads, this.ends, this.eyes, this.brows, this.mouthArc, this.mouthO, this.cheeks]) {
      p.mesh.instanceMatrix.needsUpdate = true; if (p.mesh.instanceColor) p.mesh.instanceColor.needsUpdate = true;
    }
  }
}

const WHITE = new THREE.Color(1, 1, 1);
const ZERO = new THREE.Matrix4().makeScale(0, 0, 0);

function faceOf(ch) {
  if (ch.anim === 'sleep') return 'sleep';
  const e = dominantEmotion(ch);
  if (e === 'joy') return 'joy';
  if (e === 'sadness') return 'sadness';
  if (e === 'anger') return 'anger';
  if (e === 'surprise' || e === 'fear') return 'surprise';
  return 'neutral';
}

// Atlas d'émojis dessinés dans des bulles (8x8 cases de 64 px).
class EmojiAtlas {
  constructor() {
    this.canvas = document.createElement('canvas'); this.canvas.width = this.canvas.height = 512;
    this.ctx = this.canvas.getContext('2d');
    this.map = new Map();
    this.texture = new THREE.CanvasTexture(this.canvas);
    this.texture.colorSpace = THREE.SRGBColorSpace;
  }
  slot(icon) {
    let i = this.map.get(icon);
    if (i !== undefined) return i;
    i = this.map.size % 64; this.map.set(icon, i);
    const x = (i % 8) * 64, y = Math.floor(i / 8) * 64; const c = this.ctx;
    c.clearRect(x, y, 64, 64);
    c.fillStyle = 'rgba(255,255,255,0.96)'; c.strokeStyle = 'rgba(30,30,40,0.25)'; c.lineWidth = 2;
    c.beginPath(); c.arc(x + 32, y + 29, 25, 0, Math.PI * 2); c.fill(); c.stroke();
    c.beginPath(); c.moveTo(x + 26, y + 50); c.lineTo(x + 32, y + 62); c.lineTo(x + 38, y + 50); c.fill();
    c.font = '32px "Noto Color Emoji","Apple Color Emoji","Segoe UI Emoji",sans-serif'; c.textAlign = 'center'; c.textBaseline = 'middle';
    c.fillStyle = '#000'; c.fillText(icon, x + 32, y + 31);
    this.texture.needsUpdate = true;
    return i;
  }
}
