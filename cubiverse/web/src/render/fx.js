// Effets visuels : confettis, nuages de bagarre, fumée, ballon lancé, gâteau d'anniversaire.
import * as THREE from 'three';
import { CUBE } from '../sim/constants.js';
import { cakeGeometry } from './props.js';

export class FxRenderer {
  constructor(scene, gradientMap) {
    this.scene = scene;
    const toon = new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap });
    this.balls = new THREE.InstancedMesh(new THREE.SphereGeometry(0.045, 10, 8), new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap }), 64);
    this.balls.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(64 * 3).fill(1), 3);
    this.puffs = new THREE.InstancedMesh(new THREE.SphereGeometry(1, 8, 6), new THREE.MeshLambertMaterial({ color: 0xffffff, transparent: true, opacity: 0.85 }), 400);
    this.puffs.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(400 * 3).fill(1), 3);
    this.confetti = new THREE.InstancedMesh(new THREE.PlaneGeometry(0.035, 0.05), new THREE.MeshBasicMaterial({ side: THREE.DoubleSide }), 1200);
    this.confetti.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(1200 * 3).fill(1), 3);
    this.cakes = new THREE.InstancedMesh(cakeGeometry(), new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap, vertexColors: true }), 16);
    this.stars = new THREE.InstancedMesh(new THREE.OctahedronGeometry(0.03), new THREE.MeshBasicMaterial({ color: 0xffd23f }), 200);
    for (const m of [this.balls, this.puffs, this.confetti, this.cakes, this.stars]) { m.frustumCulled = false; m.count = 0; scene.add(m); }
    this.balls.castShadow = true; this.cakes.castShadow = true;
    this.m = new THREE.Matrix4(); this.q = new THREE.Quaternion(); this.e = new THREE.Euler(); this.v = new THREE.Vector3(); this.s = new THREE.Vector3(); this.c = new THREE.Color();
  }

  update(world, visibleCubes, time) {
    const { m, q, e, v, s, c } = this;
    let nb = 0, np = 0, nc = 0, nk = 0, ns = 0;
    for (const f of world.fx) {
      const cube = world.cubes.get(f.cube); if (!cube || !visibleCubes.has(cube.id)) continue;
      const ox = cube.x * CUBE, oy = cube.y * CUBE + 0.06, oz = cube.z * CUBE;
      const k = f.t / f.dur;
      if (f.type === 'ball' && nb < 64) {
        const x = f.from[0] + (f.to[0] - f.from[0]) * k, z = f.from[1] + (f.to[1] - f.from[1]) * k;
        const y = 0.42 + Math.sin(k * Math.PI) * 0.45;
        m.makeTranslation(ox + x, oy + y, oz + z); this.balls.setMatrixAt(nb, m); this.balls.setColorAt(nb++, c.set('#ff5d5d'));
      } else if (f.type === 'juggle' && nb < 64) {
        const ch = world.chars.get(f.ch); if (!ch || ch.cubeId !== cube.id) continue;
        const y = 0.5 + Math.sin(k * Math.PI) * 0.35;
        m.makeTranslation(ox + ch.x + Math.sin(ch.ry) * 0.1, oy + y, oz + ch.z + Math.cos(ch.ry) * 0.1); this.balls.setMatrixAt(nb, m); this.balls.setColorAt(nb++, c.set('#ffffff'));
      } else if ((f.type === 'dust' || f.type === 'smoke') && np < 380) {
        const n = 12;
        for (let i = 0; i < n; i++) {
          const a = i * 2.39996 + time * (f.type === 'dust' ? 7 : 1.5);
          const r = (f.type === 'dust' ? 0.16 : 0.1 + k * 0.3) + Math.sin(i * 7.1) * 0.05;
          const h = f.type === 'dust' ? 0.25 + Math.sin(a * 1.3 + i) * 0.12 : 0.3 + k * 0.9 + (i % 4) * 0.06;
          const sc = (f.type === 'dust' ? 0.1 : 0.08 + k * 0.1) * (1 - Math.max(0, k - 0.8) * 5) * (0.8 + (i % 3) * 0.15);
          v.set(ox + f.x + Math.cos(a) * r, oy + h, oz + f.z + Math.sin(a) * r); s.set(sc, sc, sc); q.identity(); m.compose(v, q, s);
          this.puffs.setMatrixAt(np, m); this.puffs.setColorAt(np++, c.set(f.type === 'dust' ? '#f1e4cf' : '#8c8c8c'));
        }
        if (f.type === 'dust') for (let i = 0; i < 4 && ns < 200; i++) {
          const a = time * 5 + i * Math.PI / 2;
          e.set(time * 3, time * 4, 0); q.setFromEuler(e); v.set(ox + f.x + Math.cos(a) * 0.3, oy + 0.55 + Math.sin(a * 2) * 0.05, oz + f.z + Math.sin(a) * 0.3); s.set(1, 1, 1); m.compose(v, q, s); this.stars.setMatrixAt(ns++, m);
        }
      } else if (f.type === 'confetti') {
        for (let i = 0; i < 60 && nc < 1200; i++) {
          const seed = i * 12.9898; const rx = Math.sin(seed) * 0.9, rz = Math.cos(seed * 1.7) * 0.9;
          const y = 2.6 - k * 2.5 * (0.7 + (i % 5) * 0.08);
          e.set(time * (3 + (i % 4)), time * 2 + i, 0); q.setFromEuler(e);
          v.set(ox + f.x + rx + Math.sin(time * 2 + i) * 0.08, oy + Math.max(0.02, y), oz + f.z + rz); s.set(1, 1, 1); m.compose(v, q, s);
          this.confetti.setMatrixAt(nc, m); this.confetti.setColorAt(nc++, c.setHSL((i * 0.137) % 1, 0.85, 0.6));
        }
      } else if (f.type === 'cake' && nk < 16) {
        m.makeTranslation(ox + f.x, oy, oz + f.z); this.cakes.setMatrixAt(nk++, m);
      }
    }
    this.balls.count = nb; this.puffs.count = np; this.confetti.count = nc; this.cakes.count = nk; this.stars.count = ns;
    for (const mm of [this.balls, this.puffs, this.confetti, this.cakes, this.stars]) { mm.instanceMatrix.needsUpdate = true; if (mm.instanceColor) mm.instanceColor.needsUpdate = true; }
  }
}
