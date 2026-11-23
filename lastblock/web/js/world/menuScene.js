// Décor 3D du menu principal : pièces flottantes glossy, caméra cinématique, lumières
// colorées. Se corrompt selon la progression (blocs noirs aux yeux rouges, lumière rouge).
import * as THREE from 'three';
import { SHAPES, TYPES } from '../game/tetris.js';
import { PALETTES, MATERIALS, blockGeo, monsterMaterial } from './materials.js';
import { Monster } from './monster.js';
import { rand, damp } from '../core/util.js';

export class MenuScene {
  constructor(fx, save) {
    this.fx = fx; this.save = save;
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(50, 1, 0.1, 300);
    this.scene.background = new THREE.Color(0x120f2c);
    this.scene.fog = new THREE.Fog(0x120f2c, 18, 70);
    this.hemi = new THREE.HemisphereLight(0xb0c0ff, 0x301848, 1.2); this.scene.add(this.hemi);
    const key = new THREE.DirectionalLight(0xffffff, 2.2); key.position.set(5, 10, 8); this.scene.add(key);
    this.pA = new THREE.PointLight(0x29d9ff, 60, 40); this.pA.position.set(-6, 3, 4); this.scene.add(this.pA);
    this.pB = new THREE.PointLight(0xff4fd8, 60, 40); this.pB.position.set(6, -3, 4); this.scene.add(this.pB);
    this.red = new THREE.PointLight(0xff1020, 0, 40); this.red.position.set(0, 0, 6); this.scene.add(this.red);
    this.pieces = [];
    const pal = PALETTES.classic;
    this.darkMat = monsterMaterial();
    for (let i = 0; i < 26; i++) {
      const t = TYPES[i % TYPES.length];
      const g = new THREE.Group();
      const mat = MATERIALS.glossy.make(pal[t]);
      for (const [c, r] of SHAPES[t][0]) { const m = new THREE.Mesh(blockGeo(), mat); m.position.set(c - 1, -r + 1, 0); g.add(m); }
      g.userData = { mat, t, rot: new THREE.Vector3(rand(-0.6, 0.6), rand(-0.6, 0.6), rand(-0.4, 0.4)), speed: rand(0.4, 1.1), phase: rand(10) };
      this._place(g, true);
      this.scene.add(g); this.pieces.push(g);
      // yeux pour les versions corrompues
      const eyes = new THREE.Group();
      for (const x of [-0.18, 0.18]) { const e = new THREE.Mesh(new THREE.BoxGeometry(0.16, 0.12, 0.05), new THREE.MeshBasicMaterial({ color: new THREE.Color(2.2, 0, 0.02), toneMapped: false })); e.position.set(x, 0.1, 0.48); eyes.add(e); }
      eyes.position.copy(g.children[1].position); eyes.visible = false; g.add(eyes); g.userData.eyes = eyes;
    }
    this.watcher = null;
    this.time = 0; this.corruption = 0;
  }
  _place(g, initial) {
    g.position.set(rand(-14, 14), initial ? rand(-16, 16) : -18, rand(-30, -4));
    g.scale.setScalar(rand(0.6, 1.3));
  }
  setCorruption(level) {
    // level : 0 (sain) → 1 (complètement corrompu)
    this.corruption = level;
    this.pieces.forEach((g, i) => {
      const dark = (i / this.pieces.length) < level * 0.8;
      g.children.forEach((m) => { if (m.isMesh) m.material = dark ? this.darkMat : g.userData.mat; });
      g.userData.eyes.visible = dark && level > 0.3;
    });
    const bg = new THREE.Color(0x120f2c).lerp(new THREE.Color(0x070306), level);
    this.scene.background = bg; this.scene.fog.color = bg;
    this.red.intensity = level * 50;
    this.pA.color.set(0x29d9ff).lerp(new THREE.Color(0xff2030), level);
    if (level >= 0.7 && !this.watcher) {
      this.watcher = new Monster('humanoid', { shadows: false });
      this.watcher.root.position.set(0, -9, -26); this.watcher.root.scale.setScalar(2.2);
      this.watcher.setState('observe');
      this.scene.add(this.watcher.root);
    }
    if (this.watcher) this.watcher.root.visible = level >= 0.7;
  }
  update(dt, t) {
    this.time = t;
    const c = this.corruption;
    for (const g of this.pieces) {
      const u = g.userData;
      g.position.y += u.speed * dt * (c > 0.5 ? 0.3 : 1);
      g.rotation.x += u.rot.x * dt; g.rotation.y += u.rot.y * dt; g.rotation.z += u.rot.z * dt;
      if (g.position.y > 18) this._place(g, false);
      if (u.eyes.visible) u.eyes.lookAt(this.camera.position);
    }
    const cam = this.camera;
    cam.position.set(Math.sin(t * 0.08) * 3, Math.sin(t * 0.11) * 1.2, 14 + Math.sin(t * 0.05) * 2);
    cam.lookAt(0, 0, -10);
    if (c > 0.3 && Math.random() < 0.004 * c) { this.fx.glitch(0.5); }
    if (this.watcher) { this.watcher.lookAt(cam.position); this.watcher.update(dt); }
  }
}
