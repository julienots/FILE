// Faune : hardes de cerfs, renards et loups en lisière, chiens et chats dans les villages.
// Errance, broutage, fuite devant le dieu ou la destruction (modèles animés Quaternius, CC0).
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { MeshoptDecoder } from 'three/addons/libs/meshopt_decoder.module.js';
import * as SkeletonUtils from 'three/addons/utils/SkeletonUtils.js';
import { G, WORLD, Q, mulberry32, clamp, rand, dampAngle } from './core.js';
import { REGIONS } from './terrain.js';

const SPECIES = {
  deer: { file: 'deer', len: 1.7, walk: 1.2, run: 9, shy: 70 },
  stag: { file: 'stag', len: 2.0, walk: 1.2, run: 9, shy: 70 },
  fox: { file: 'fox', len: 0.9, walk: 1.4, run: 7, shy: 40 },
  wolf: { file: 'wolf', len: 1.5, walk: 1.5, run: 8, shy: 45 },
  dog: { file: 'husky', len: 1.0, walk: 1.5, run: 6, shy: 18 },
  cat: { file: 'cat', len: 0.5, walk: 0.8, run: 5, shy: 14 },
};

export async function loadFauna(onEach) {
  const gl = new GLTFLoader(); gl.setMeshoptDecoder(MeshoptDecoder);
  const out = {};
  for (const [k, s] of Object.entries(SPECIES)) {
    const g = await gl.loadAsync(`models/chars/${s.file}.glb`);
    g.scene.updateMatrixWorld(true);
    const bb = new THREE.Box3().setFromObject(g.scene), size = bb.getSize(new THREE.Vector3());
    const clip = (re) => g.animations.find((a) => re.test(a.name.split('|').pop()));
    out[k] = { gltf: g, length: Math.max(size.x, size.z), clips: { walk: clip(/^Walk/i), run: clip(/^Gallop$|^Run/i) || clip(/^Walk/i), idle: clip(/^Idle$/i) || clip(/Idle/i), eat: clip(/Eat/i) || clip(/Idle/i), death: clip(/Death/i) } };
    if (onEach) onEach();
  }
  return out;
}

class Animal {
  constructor(sys, kind, x, z, home) {
    const A = sys.assets[kind], S = SPECIES[kind];
    this.sys = sys; this.kind = kind; this.S = S; this.home = home;
    this.model = SkeletonUtils.clone(A.gltf.scene);
    this.model.scale.setScalar(S.len / A.length * rand(0.9, 1.1));
    this.model.traverse((o) => { if (o.isMesh) { o.castShadow = false; o.frustumCulled = false; } });
    this.mixer = new THREE.AnimationMixer(this.model);
    this.acts = {};
    for (const [k, c] of Object.entries(A.clips)) if (c) this.acts[k] = this.mixer.clipAction(c);
    if (this.acts.death) { this.acts.death.setLoop(THREE.LoopOnce); this.acts.death.clampWhenFinished = true; }
    this.pos = new THREE.Vector3(x, G.terrain.heightAt(x, z), z);
    this.heading = Math.random() * 6.28;
    this.target = null; this.state = 'idle'; this.t = rand(0, 6);
    this.alive = true;
    this.play('idle'); this.mixer.update(Math.random() * 3);
    sys.scene.add(this.model);
  }
  play(k, speed = 1) {
    const a = this.acts[k] || this.acts.idle; if (!a) return;
    a.timeScale = speed;
    if (this.cur === a) return;
    a.reset().play(); if (this.cur) a.crossFadeFrom(this.cur, 0.3, false);
    this.cur = a;
  }
  update(dt, near) {
    if (!this.alive) { this.deadT += dt; if (this.deadT > 12) { this.sys.scene.remove(this.model); this.removed = true; } else { this.mixer.update(dt); } return; }
    const god = G.god, T = G.terrain;
    this.t -= dt;
    let threat = null;
    if (god && god.pos.distanceTo(this.pos) < this.S.shy && god.pos.y - T.groundAt(god.pos.x, god.pos.z) < 30) threat = god.pos;
    for (const e of (G.people ? G.people.events : [])) if (e.kind !== 'bless' && Math.hypot(e.x - this.pos.x, e.z - this.pos.z) < e.r + 60) threat = new THREE.Vector3(e.x, 0, e.z);
    if (threat) {
      this.state = 'flee';
      const away = new THREE.Vector3(this.pos.x - threat.x, 0, this.pos.z - threat.z).normalize();
      this.target = this.pos.clone().addScaledVector(away, 40);
      this.t = 3;
    } else if (this.t <= 0) {
      const r = Math.random();
      if (r < 0.45) { const a = Math.random() * 6.28, d = rand(5, 25); this.target = new THREE.Vector3(this.home.x + Math.cos(a) * d * 2, 0, this.home.z + Math.sin(a) * d * 2); this.state = 'walk'; this.t = 20; }
      else { this.state = r < 0.8 ? 'eat' : 'idle'; this.target = null; this.t = rand(4, 12); }
    }
    if (this.target && (this.state === 'walk' || this.state === 'flee')) {
      const dx = this.target.x - this.pos.x, dz = this.target.z - this.pos.z, d = Math.hypot(dx, dz);
      if (d < 1.5) { this.target = null; this.state = 'idle'; this.t = rand(2, 6); }
      else {
        const sp = this.state === 'flee' ? this.S.run : this.S.walk;
        this.heading = dampAngle(this.heading, Math.atan2(dx, dz), 5, dt);
        const nx = this.pos.x + Math.sin(this.heading) * sp * dt, nz = this.pos.z + Math.cos(this.heading) * sp * dt;
        if (T.heightAt(nx, nz) > WORLD.water + 0.5 && T.inWorld(nx, nz, 40)) { this.pos.x = nx; this.pos.z = nz; } else { this.target = null; this.state = 'idle'; }
        if (G.villages) G.villages.pushOut(this.pos, 0.6);
        this.play(this.state === 'flee' ? 'run' : 'walk', this.state === 'flee' ? 1.2 : 1);
      }
    } else this.play(this.state === 'eat' ? 'eat' : 'idle');
    this.pos.y = T.heightAt(this.pos.x, this.pos.z);
    this.model.position.copy(this.pos); this.model.rotation.y = this.heading;
    this.acc = (this.acc || 0) + dt;
    if (near || this.acc > 0.15) { this.mixer.update(this.acc); this.acc = 0; }
  }
  kill(from) {
    if (!this.alive) return;
    this.alive = false; this.deadT = 0;
    if (this.acts.death) { this.cur && this.cur.stop(); this.acts.death.reset().play(); }
  }
}

export class Fauna {
  constructor(scene, assets) { this.scene = scene; this.assets = assets; this.list = []; }

  populate() {
    const T = G.terrain, rnd = mulberry32(77), k = Q().label === 'Bas' ? 0.5 : 1;
    const spot = (test, tries = 400) => { for (let i = 0; i < tries; i++) { const x = (rnd() - 0.5) * (WORLD.size - 200), z = (rnd() - 0.5) * (WORLD.size - 200); if (T.heightAt(x, z) > WORLD.water + 2 && T.normalAt(x, z).y > 0.88 && test(x, z)) return [x, z]; } return null; };
    const herd = (kinds, n, test) => {
      const s = spot(test); if (!s) return;
      const home = { x: s[0], z: s[1] };
      for (let i = 0; i < n; i++) this.list.push(new Animal(this, kinds[i % kinds.length], s[0] + rand(-8, 8), s[1] + rand(-8, 8), home));
    };
    // cerfs et biches dans les prairies en lisière de forêt
    for (let h = 0; h < 6 * k; h++) herd(['stag', 'deer', 'deer', 'deer'], 4 + Math.floor(rnd() * 3), (x, z) => x < -60 && Math.hypot(x, z) > 180);
    for (let h = 0; h < 3 * k; h++) herd(['fox'], 1, (x, z) => x < -150);
    for (let h = 0; h < 3 * k; h++) herd(['wolf'], 3 + Math.floor(rnd() * 2), (x, z) => z < -250 && T.heightAt(x, z) < 120);
    // chiens et chats au village
    if (G.villages) for (const v of G.villages.villages) {
      for (let i = 0; i < (v.main ? 4 : 1); i++) { const a = rnd() * 6.28, d = 5 + rnd() * 25; this.list.push(new Animal(this, i % 2 ? 'cat' : 'dog', v.x + Math.cos(a) * d, v.z + Math.sin(a) * d, { x: v.x, z: v.z })); }
    }
  }

  hitArea(x, z, r) { for (const a of this.list) if (a.alive && Math.hypot(a.pos.x - x, a.pos.z - z) < r) a.kill(); }

  update(dt) {
    const cam = G.camera.position;
    for (const a of this.list) {
      const d = a.pos.distanceTo(cam);
      a.model.visible = d < 140;
      a.update(dt, d < 80);
    }
    this.list = this.list.filter((a) => !a.removed);
  }
}
