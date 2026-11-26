// Végétation photoréaliste : arbres et buissons scannés (imposteurs), rochers scannés simplifiés,
// herbe en cartes texturées (scan de touffe), vent et bourrasques du vol divin, destruction persistante.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { G, WORLD, Q, noise, mulberry32, clamp, smooth, lerp, rand, emit } from './core.js';
import { REGIONS } from './terrain.js';
import { loadImpostor, ImpostorField, crossedTree } from './impostor.js';

const TREE_ALIVE = 0, TREE_FALLEN = 1, TREE_BURNT = 2, TREE_GONE = 3;
// espèces : [imposteur, hauteur cible min, max]
const SPECIES = [
  { name: 'island_tree_01', hMin: 15, hMax: 27 },   // grand feuillu ancien
  { name: 'fir_sapling_medium', hMin: 16, hMax: 28 }, // sapin
  { name: 'island_tree_02', hMin: 10, hMax: 19 },   // feuillu tortueux
];

export async function loadVegetationAssets(onEach) {
  const A = {};
  for (const s of SPECIES) { A[s.name] = await loadImpostor(s.name); if (onEach) onEach(); }
  A.shrub = await loadImpostor('shrub_02'); if (onEach) onEach();
  A.grass = await loadImpostor('grass_medium_01'); if (onEach) onEach();
  A.rocks = await new GLTFLoader().loadAsync('models/rock_moss_set_01.glb'); if (onEach) onEach();
  return A;
}

export class Vegetation {
  constructor(scene, assets) {
    this.scene = scene;
    this.A = assets;
    this.trees = [];
    this.falling = [];
    this.burning = [];
    this.logs = [];
    this.dirty = true;
  }

  build(skipFn = () => false) {
    const T = G.terrain, rnd = mulberry32(2024);
    // rien ne pousse sur les chemins, champs et places (ni juste à côté)
    const painted = (x, z, r) => T.paint[T.idx(x, z)] || T.paint[T.idx(x + r, z)] || T.paint[T.idx(x - r, z)] || T.paint[T.idx(x, z + r)] || T.paint[T.idx(x, z - r)];
    const skip = (x, z, r = 4) => painted(x, z, r) || skipFn(x, z);
    const density = Q().trees;
    for (let i = 0; i < 30000; i++) {
      const x = (rnd() - 0.5) * (WORLD.size - 80), z = (rnd() - 0.5) * (WORLD.size - 80);
      const h = T.heightAt(x, z);
      if (h < WORLD.water + 1.5) continue;
      const n = T.normalAt(x, z);
      if (n.y < 0.82) continue;
      const forest = smooth(-110, -220, x) * (1 - smooth(-330, -470, z));
      const grove = smooth(0.25, 0.6, noise.fbm(x * 0.006 + 20, z * 0.006, 3));
      const mountain = smooth(-200, -360, z) * (1 - smooth(120, 150, h));
      if (Math.hypot(x - REGIONS.volcano.x, z - REGIONS.volcano.z) < 230) continue;
      if (Math.hypot(x - REGIONS.village.x, z - REGIONS.village.z) < 165) continue;
      if (Math.hypot(x - REGIONS.lake.x, z - REGIONS.lake.z) < 150) continue;
      const p = forest * 0.95 + grove * 0.2 + mountain * 0.5;
      if (rnd() > p * density) continue;
      if (skip(x, z)) continue;
      const sp = (mountain > 0.3 || h > 70) ? 1 : (forest > 0.5 && rnd() < 0.15) ? 1 : (rnd() < 0.4 ? 2 : 0);
      const S = SPECIES[sp], meta = this.A[S.name].meta;
      const scale = lerp(S.hMin, S.hMax, Math.pow(rnd(), forest > 0.6 ? 0.7 : 1.3)) / meta.height;
      this.trees.push({ x, z, y: h - 0.3, rot: rnd() * Math.PI * 2, scale, sp, state: TREE_ALIVE, hp: 25 * scale * meta.height / 10, burn: 0 });
    }
    const counts = [0, 0, 0];
    for (const t of this.trees) t.slot = counts[t.sp]++;
    this.fields = SPECIES.map((S, i) => { const f = new ImpostorField(this.A[S.name], Math.max(1, counts[i])); this.scene.add(f.mesh); return f; });

    // buissons
    const shrubs = [];
    for (let i = 0; i < 12000 && shrubs.length < 2600 * density; i++) {
      const x = (rnd() - 0.5) * (WORLD.size - 60), z = (rnd() - 0.5) * (WORLD.size - 60), h = T.heightAt(x, z);
      if (h < WORLD.water + 1.2 || h > 125 || Math.hypot(x, z) < 110) continue;
      if (Math.hypot(x - REGIONS.volcano.x, z - REGIONS.volcano.z) < 200) continue;
      const f = smooth(-90, -200, x) * 0.7 + smooth(0.3, 0.7, noise.fbm(x * 0.01, z * 0.01 + 5, 2)) * 0.45;
      if (rnd() < f && !skip(x, z, 2)) shrubs.push([x, h - 0.2, z, (1.2 + rnd() * 1.6) / this.A.shrub.meta.height, rnd() * 6.28]);
    }
    this.shrubField = new ImpostorField(this.A.shrub, Math.max(1, shrubs.length), { shadows: false });
    shrubs.forEach(([x, y, z, s, r], i) => this.shrubField.set(i, x, y, z, s, r));
    this.shrubField.commit(shrubs.length);
    this.scene.add(this.shrubField.mesh);
    this.shrubData = shrubs;

    // rochers scannés (6 formes instanciées)
    const rockMeshes = [];
    this.A.rocks.scene.traverse((o) => { if (o.isMesh) rockMeshes.push(o); });
    const rockLists = rockMeshes.map(() => []);
    for (let i = 0; i < 6000; i++) {
      const x = (rnd() - 0.5) * (WORLD.size - 60), z = (rnd() - 0.5) * (WORLD.size - 60), h = T.heightAt(x, z);
      if (h < WORLD.water - 2 || Math.hypot(x, z) < 130 || skip(x, z, 2.5)) continue;
      const n = T.normalAt(x, z);
      const want = 0.06 + (1 - n.y) * 1.6 + smooth(-150, -300, x) * 0.25;
      if (rnd() < want) rockLists[Math.floor(rnd() * rockMeshes.length)].push([x, h, z, 0.8 + Math.pow(rnd(), 3) * 5, rnd() * 6.28, rnd() * 0.4]);
    }
    this.rocks = rockMeshes.map((src, k) => {
      const list = rockLists[k];
      const geo = src.geometry.clone();
      geo.applyMatrix4(src.matrixWorld);
      geo.computeBoundingBox(); const bb = geo.boundingBox; geo.translate(-(bb.min.x + bb.max.x) / 2, -bb.min.y - 0.25 * (bb.max.y - bb.min.y), -(bb.min.z + bb.max.z) / 2);
      const m = new THREE.InstancedMesh(geo, src.material, Math.max(1, list.length));
      const M = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler();
      list.forEach(([x, y, z, s, r, tilt], i) => { e.set(tilt * 0.3, r, tilt * 0.2); q.setFromEuler(e); M.compose(new THREE.Vector3(x, y, z), q, new THREE.Vector3(s, s, s)); m.setMatrixAt(i, M); });
      m.count = list.length; m.castShadow = true; m.receiveShadow = true;
      this.scene.add(m);
      return m;
    });
    this.updateTrees();
    if (Q().grass) this.buildGrass(Q().grass);
  }

  // ---------- Herbe : cartes croisées texturées (scan), recyclées autour de la caméra ----------
  buildGrass(count) {
    const meta = this.A.grass.meta;
    const W = 1.4, Hh = 0.78;
    const pos = [], uv = [], nor = [];
    for (let q = 0; q < 2; q++) {
      const a = q * Math.PI / 2, cx = Math.cos(a) * W / 2, cz = Math.sin(a) * W / 2;
      const quad = [[-cx, 0, -cz, 0, 0], [cx, 0, cz, 1, 0], [cx, Hh, cz, 1, 1], [-cx, 0, -cz, 0, 0], [cx, Hh, cz, 1, 1], [-cx, Hh, -cz, 0, 1]];
      for (const [x, y, z, u, v] of quad) { pos.push(x, y, z); uv.push(u, v); nor.push(0, 1, 0); }
    }
    const geo = new THREE.InstancedBufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
    geo.setAttribute('normal', new THREE.Float32BufferAttribute(nor, 3));
    const R = Q().grass > 6000 ? 48 : 38, off = new Float32Array(count * 4);
    for (let i = 0; i < count; i++) { off[i * 4] = Math.random() * R * 2; off[i * 4 + 1] = Math.random() * R * 2; off[i * 4 + 2] = 0.6 + Math.random() * 0.9; off[i * 4 + 3] = Math.random(); }
    geo.setAttribute('aOff', new THREE.InstancedBufferAttribute(off, 4));
    geo.instanceCount = count;
    this.grassCenter = { value: new THREE.Vector2() };
    const mat = new THREE.ShaderMaterial({
      side: THREE.DoubleSide, fog: true,
      uniforms: THREE.UniformsUtils.merge([THREE.UniformsLib.fog, {
        tMap: { value: this.A.grass.alb }, uHeight: { value: G.terrain.dataTex }, uR: { value: R }, uHalf: { value: WORLD.half },
        uSunDir: { value: new THREE.Vector3(0, 1, 0) }, uSunCol: { value: new THREE.Color(1, 1, 1) }, uSky: { value: new THREE.Color(0.5, 0.6, 0.7) },
      }]),
      vertexShader: `
        #include <fog_pars_vertex>
        attribute vec4 aOff; uniform float uTime, uR, uHalf; uniform vec4 uGust; uniform vec4 uPress; uniform vec2 uCenter; uniform sampler2D uHeight;
        varying vec2 vUv; varying float vH;
        void main(){
          vUv = (vec2(floor(aOff.w * ${meta.az.toFixed(1)}), 0.0) + vec2(mix(0.02, 0.98, uv.x), mix(0.25, 0.78, uv.y))) / vec2(${meta.az.toFixed(1)}, ${meta.el.length.toFixed(1)});
          vec2 wp = uCenter + mod(aOff.xy - uCenter + uR, 2.0 * uR) - uR;
          vec4 tex = texture2D(uHeight, (wp + uHalf) / (uHalf * 2.0));
          float ok = step(fract(aOff.z * 7.31), tex.g);
          float d = length(wp - uCenter); float fade = 1.0 - smoothstep(uR * 0.65, uR, d);
          float ang = aOff.w * 6.2831;
          vec3 p = position; p.xz = mat2(cos(ang), -sin(ang), sin(ang), cos(ang)) * p.xz;
          vec3 toCam = normalize(cameraPosition - vec3(wp.x, tex.r, wp.y));
          float side = smoothstep(0.92, 0.55, toCam.y); // vue plongeante : les cartes s'effacent au profit du sol
          vec3 transformed = p * aOff.z * ok * fade * side;
          float h = transformed.y; vH = uv.y;
          float sway = sin(uTime * 1.7 + wp.x * 0.3 + wp.y * 0.2) * 0.16 + sin(uTime * 3.3 + wp.y * 0.5) * 0.05;
          vec2 gd = wp - uGust.xz; float gl = length(gd) + 0.01; float g = uGust.w * exp(-gl / 30.0);
          vec2 pd = wp - uPress.xy; float pl = length(pd) + 0.01; float pr = (1.0 - smoothstep(0.0, max(uPress.z, 0.01), pl)) * uPress.w;
          transformed.xz += (vec2(sway, sway * 0.6) + (gd / gl) * g * 0.2 + (pd / pl) * pr * 0.9) * h;
          transformed.y *= 1.0 - pr * 0.7;
          transformed += vec3(wp.x, tex.r - 0.05, wp.y);
          vec4 mvPosition = viewMatrix * vec4(transformed, 1.0);
          gl_Position = projectionMatrix * mvPosition;
          #include <fog_vertex>
        }`,
      fragmentShader: `
        #include <common>
        #include <fog_pars_fragment>
        uniform sampler2D tMap; uniform vec3 uSunDir, uSunCol, uSky; varying vec2 vUv; varying float vH;
        void main(){
          vec4 c = texture2D(tMap, vUv); if (c.a < 0.45) discard;
          float light = 0.55 + 0.45 * vH;
          vec3 col = c.rgb * vec3(0.92, 1.18, 0.72) * 1.7 * (uSunCol * (0.55 * max(uSunDir.y, 0.0) + 0.25) * light + uSky);
          gl_FragColor = vec4(col, 1.0);
          #include <tonemapping_fragment>
          #include <colorspace_fragment>
          #include <fog_fragment>
        }`,
    });
    mat.uniforms.uTime = G.uniforms.time; mat.uniforms.uGust = G.uniforms.gust; mat.uniforms.uPress = G.uniforms.press; mat.uniforms.uCenter = this.grassCenter;
    this.grassMat = mat;
    this.grass = new THREE.Mesh(geo, mat);
    this.grass.frustumCulled = false;
    this.grass.receiveShadow = false;
    this.scene.add(this.grass);
  }

  updateTrees() {
    const n = [0, 0, 0];
    const fill = this.fields.map(() => []);
    for (const t of this.trees) {
      const f = this.fields[t.sp];
      const visible = t.state === TREE_ALIVE || t.state === TREE_BURNT ? 1 : 0;
      f.set(t.slot, t.x, t.y, t.z, t.scale, t.rot, t.state === TREE_BURNT ? 1 : 0, visible);
      n[t.sp] = Math.max(n[t.sp], t.slot + 1);
    }
    this.fields.forEach((f, i) => f.commit(n[i]));
  }

  // ---------- Destruction ----------
  // type : 'push' (renverse), 'blast' (déracine et projette), 'burn' (calcine)
  damageArea(x, z, radius, power, type = 'push', from = null) {
    let hit = 0;
    for (const t of this.trees) {
      if (t.state !== TREE_ALIVE && !(t.state === TREE_BURNT && type !== 'burn')) continue;
      const dx = t.x - x, dz = t.z - z, d = Math.hypot(dx, dz);
      if (d > radius) continue;
      const k = 1 - d / radius;
      if (type === 'burn') {
        t.burn += power * k;
        if (t.burn > 1 && t.state === TREE_ALIVE) { t.state = TREE_BURNT; hit++; this.burning.push({ t, life: rand(8, 16) }); }
        continue;
      }
      t.hp -= power * (0.3 + k);
      if (t.hp <= 0) {
        const ox = from ? from.x : x, oz = from ? from.z : z;
        let dirx = t.x - ox, dirz = t.z - oz; const L = Math.hypot(dirx, dirz) || 1; dirx /= L; dirz /= L;
        this.fell(t, dirx, dirz, type === 'blast' ? power * k : 0);
        hit++;
      }
    }
    if (hit) { this.dirty = true; emit('treesDestroyed', { x, z, count: hit }); }
    return hit;
  }

  fell(t, dx, dz, launch) {
    const wasBurnt = t.state === TREE_BURNT;
    t.state = TREE_FALLEN;
    const grp = crossedTree(this.A[SPECIES[t.sp].name], wasBurnt);
    grp.scale.setScalar(t.scale);
    grp.position.set(t.x, t.y, t.z);
    grp.rotation.y = t.rot;
    this.scene.add(grp);
    const axis = new THREE.Vector3(dz, 0, -dx).normalize();
    this.falling.push({ grp, t, axis, angle: 0, av: launch > 0 ? rand(1.5, 3) : 0.15, vel: new THREE.Vector3(dx * launch * 0.5, launch * 0.35, dz * launch * 0.5), flying: launch > 8, settled: false, baseQ: grp.quaternion.clone() });
    if (G.fx) G.fx.dust(new THREE.Vector3(t.x, t.y + 1, t.z), 6, 4);
    if (G.audio) G.audio.treeCrack(t.x, t.z);
  }

  update(dt) {
    if (this.dirty) { this.updateTrees(); this.dirty = false; }
    if (this.grassCenter && G.camera) this.grassCenter.value.set(G.camera.position.x, G.camera.position.z);
    if (G.env && G.env.sun) {
      const env = G.env, sunCol = env.sun.color.clone().multiplyScalar(env.sun.intensity);
      const sky = env.fogColor.clone().multiplyScalar(0.55 * (0.4 + env.dayF * 0.6) * (G.scene.environmentIntensity || 1));
      const ground = new THREE.Color(0.16, 0.14, 0.1).multiplyScalar(0.3 + env.dayF * 0.7);
      for (const f of [...this.fields, this.shrubField, ...(this.extraFields || [])]) f.light(env.sunDir.y > -0.04 ? env.sunDir : env.sky.moonDir, sunCol, sky, ground);
      if (this.grassMat) { const u = this.grassMat.uniforms; u.uSunDir.value.copy(env.sunDir); u.uSunCol.value.copy(sunCol); u.uSky.value.copy(sky); }
    }
    const T = G.terrain, q = new THREE.Quaternion();
    for (let i = this.falling.length - 1; i >= 0; i--) {
      const f = this.falling[i];
      if (f.flying) {
        f.vel.y -= 30 * dt;
        f.grp.position.addScaledVector(f.vel, dt);
        f.angle += f.av * dt;
        const gy = T.heightAt(f.grp.position.x, f.grp.position.z);
        if (f.grp.position.y < gy && f.vel.y < 0) {
          f.grp.position.y = gy; f.flying = false; f.vel.set(0, 0, 0);
          if (G.fx) G.fx.dust(f.grp.position.clone(), 8, 5);
          if (G.audio) G.audio.thud(f.grp.position, 0.6);
        }
      } else {
        f.av += 2.6 * Math.sin(Math.max(0.15, f.angle)) * dt;
        f.angle += f.av * dt;
        if (f.angle >= Math.PI / 2 - 0.05) {
          f.angle = Math.PI / 2 - 0.05;
          const tip = f.grp.position.clone().addScaledVector(new THREE.Vector3().crossVectors(f.axis, new THREE.Vector3(0, -1, 0)), 8 * f.t.scale);
          if (G.fx) G.fx.dust(tip, 10, 6);
          if (G.audio) G.audio.thud(tip, 0.8);
          if (G.shake) G.shake(0.12, f.grp.position);
          this.falling.splice(i, 1);
          this.logs.push(f);
          if (this.logs.length > 90) { const old = this.logs.shift(); this.scene.remove(old.grp); old.t.state = TREE_GONE; }
        }
      }
      q.setFromAxisAngle(f.axis, f.angle);
      f.grp.quaternion.copy(q).multiply(f.baseQ);
    }
    for (let i = this.burning.length - 1; i >= 0; i--) {
      const b = this.burning[i];
      b.life -= dt;
      if (G.fx && Math.random() < dt * 6) {
        const h = SPECIES[b.t.sp].hMin * 0.8;
        G.fx.fire(new THREE.Vector3(b.t.x + rand(-2, 2), b.t.y + rand(3, h), b.t.z + rand(-2, 2)), 2);
        if (Math.random() < 0.3) G.fx.smoke(new THREE.Vector3(b.t.x, b.t.y + h, b.t.z), 1, 6, 0.5);
      }
      if (b.life <= 0) this.burning.splice(i, 1);
    }
  }

  serialize() { return this.trees.map((t) => (t.state === TREE_ALIVE ? 0 : t.state === TREE_BURNT ? 2 : 3)).join(''); }
  restore(s) {
    if (!s || s.length !== this.trees.length) return;
    for (let i = 0; i < s.length; i++) { const v = +s[i]; if (v === 2) this.trees[i].state = TREE_BURNT; else if (v === 3) this.trees[i].state = TREE_GONE; }
    this.dirty = true;
  }
}
