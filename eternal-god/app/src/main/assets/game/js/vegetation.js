// Végétation : arbres instanciés avec deux niveaux de détail, vent et bourrasques du vol divin,
// herbe dense autour du joueur (lit la texture d'altitude), arbres destructibles et persistants.
import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { G, WORLD, Q, noise, mulberry32, clamp, smooth, lerp, rand, emit } from './core.js';
import { REGIONS } from './terrain.js';

function colorize(g, c, varAmt = 0.08, darkBottom = 0, y0 = 0, y1 = 1) {
  g = g.index ? g.toNonIndexed() : g;
  g.deleteAttribute('uv');
  const p = g.attributes.position, n = p.count, a = new Float32Array(n * 3);
  const col = new THREE.Color(c);
  for (let i = 0; i < n; i += 3) {
    const v = 1 - varAmt + Math.random() * varAmt * 2;
    for (let k = 0; k < 3; k++) {
      const y = p.getY(i + k);
      const sh = darkBottom ? lerp(1 - darkBottom, 1, clamp((y - y0) / (y1 - y0), 0, 1)) : 1;
      a[(i + k) * 3] = col.r * v * sh; a[(i + k) * 3 + 1] = col.g * v * sh; a[(i + k) * 3 + 2] = col.b * v * sh;
    }
  }
  g.setAttribute('color', new THREE.BufferAttribute(a, 3));
  return g;
}

function jitter(g, amt, seed) {
  const r = mulberry32(seed), p = g.attributes.position;
  const map = new Map();
  for (let i = 0; i < p.count; i++) {
    const key = `${p.getX(i).toFixed(3)},${p.getY(i).toFixed(3)},${p.getZ(i).toFixed(3)}`;
    let d = map.get(key);
    if (!d) { d = [(r() - 0.5) * amt, (r() - 0.5) * amt, (r() - 0.5) * amt]; map.set(key, d); }
    p.setXYZ(i, p.getX(i) + d[0], p.getY(i) + d[1], p.getZ(i) + d[2]);
  }
  g.computeVertexNormals();
  return g;
}

// ---------- Espèces ----------
export function treeGeometries() {
  const S = {};
  // Chêne ancien : tronc noueux, branches, houppier de grosses masses
  {
    const parts = [];
    const trunk = jitter(new THREE.CylinderGeometry(0.75, 1.35, 11, 9, 5).translate(0, 5.5, 0), 0.35, 1);
    parts.push(colorize(trunk, 0x5b4632, 0.1, 0.4, 0, 11));
    for (const [rx, rz, ry, l] of [[0.0, 0.9, 0, 6], [0.0, -0.8, 1.2, 5.5], [0.8, 0.2, 2.4, 5]]) {
      const br = new THREE.CylinderGeometry(0.3, 0.6, l, 6).translate(0, l / 2, 0);
      br.applyMatrix4(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(rx, ry, rz)));
      br.translate(0, 8, 0);
      parts.push(colorize(br, 0x564230, 0.1));
    }
    S.oakTrunk = mergeGeometries(parts);
    const leaves = [];
    const r = mulberry32(11);
    const blobs = [[0, 14.5, 0, 5.6], [3.6, 12.6, 1.2, 4.4], [-3.4, 12.8, -1.4, 4.6], [1.2, 12.0, -3.8, 4.2], [-1.4, 12.2, 3.6, 4.3], [0.6, 17.2, 0.4, 4.0], [4.4, 10.6, -2.5, 3.2], [-4.2, 10.5, 2.4, 3.3]];
    for (const [x, y, z, rad] of blobs) {
      const g = jitter(new THREE.IcosahedronGeometry(rad, 1), rad * 0.28, Math.floor(r() * 1000));
      g.scale(1, 0.78, 1); g.translate(x, y, z);
      leaves.push(colorize(g, new THREE.Color(0x3f6e2a).offsetHSL((r() - 0.5) * 0.04, 0, (r() - 0.5) * 0.06), 0.12, 0.45, y - rad, y + rad));
    }
    S.oakLeaves = mergeGeometries(leaves);
    S.oakTrunkFar = colorize(new THREE.CylinderGeometry(0.8, 1.3, 11, 5).translate(0, 5.5, 0), 0x5b4632, 0.05);
    S.oakLeavesFar = colorize(new THREE.IcosahedronGeometry(8.2, 0).scale(1, 0.75, 1).translate(0, 13.5, 0), 0x3f6a2a, 0.12, 0.35, 7, 20);
    S.oakHeight = 20;
  }
  // Pin des montagnes
  {
    S.pineTrunk = colorize(new THREE.CylinderGeometry(0.28, 0.65, 20, 7).translate(0, 10, 0), 0x4e3a2a, 0.08);
    const cones = [];
    for (let i = 0; i < 6; i++) {
      const y = 5 + i * 2.6, rad = 4.6 - i * 0.62;
      const g = jitter(new THREE.ConeGeometry(rad, 5.2, 9, 1, true).translate(0, y + 2.6, 0), 0.35, 30 + i);
      cones.push(colorize(g, 0x2c5232, 0.1, 0.5, y, y + 5.2));
    }
    S.pineLeaves = mergeGeometries(cones);
    S.pineTrunkFar = colorize(new THREE.CylinderGeometry(0.3, 0.6, 20, 4).translate(0, 10, 0), 0x4e3a2a, 0.05);
    S.pineLeavesFar = colorize(new THREE.ConeGeometry(4.6, 17, 6).translate(0, 13, 0), 0x2b4f30, 0.1, 0.4, 5, 21);
    S.pineHeight = 21;
  }
  // Buisson
  S.bush = colorize(jitter(new THREE.IcosahedronGeometry(1.6, 1), 0.5, 5).scale(1.3, 0.8, 1.3).translate(0, 0.9, 0), 0x45702e, 0.15, 0.4, 0, 2);
  // Rocher
  S.rock = colorize(jitter(new THREE.DodecahedronGeometry(2, 1), 0.9, 8).scale(1.3, 0.8, 1.1), 0x84807a, 0.1, 0.3, -2, 2);
  return S;
}

// Matériau de feuillage avec vent et bourrasque (souffle du dieu)
export function windMaterial(params) {
  const m = new THREE.MeshStandardMaterial(Object.assign({ vertexColors: true, roughness: 0.88, metalness: 0 }, params));
  m.onBeforeCompile = (sh) => {
    sh.uniforms.uTime = G.uniforms.time;
    sh.uniforms.uGust = G.uniforms.gust;
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', '#include <common>\nuniform float uTime; uniform vec4 uGust;')
      .replace('#include <project_vertex>', `
        vec4 mvPosition = vec4(transformed, 1.0);
        #ifdef USE_INSTANCING
          mvPosition = instanceMatrix * mvPosition;
          vec3 ip = instanceMatrix[3].xyz;
        #else
          vec3 ip = vec3(0.0);
        #endif
        vec4 wp = modelMatrix * mvPosition;
        vec3 root = (modelMatrix * vec4(ip, 1.0)).xyz;
        float hgt = max(wp.y - root.y, 0.0);
        float sway = sin(uTime*1.1 + root.x*0.07 + root.z*0.05)*0.5 + sin(uTime*2.3 + root.z*0.13)*0.25;
        wp.x += sway * hgt * 0.012; wp.z += sway * hgt * 0.008;
        vec2 gd = root.xz - uGust.xz; float dist = length(gd) + 0.001;
        float g = uGust.w * exp(-dist / 38.0) * (0.75 + 0.25*sin(uTime*9.0 + root.x));
        wp.xz += (gd/dist) * g * hgt * hgt * 0.004;
        wp.y -= g * hgt * 0.01;
        mvPosition = viewMatrix * wp;
        gl_Position = projectionMatrix * mvPosition;`);
  };
  return m;
}

const TREE_ALIVE = 0, TREE_FALLEN = 1, TREE_BURNT = 2, TREE_GONE = 3;

export class Vegetation {
  constructor(scene) {
    this.scene = scene;
    this.trees = [];
    this.falling = [];
    this.burning = [];
    this.lodTimer = 0;
    this.logs = [];
  }

  build(skip = () => false) {
    const S = this.geo = treeGeometries();
    this.trunkMat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.95 });
    this.leafMat = windMaterial({});
    this.burntMat = new THREE.MeshStandardMaterial({ color: 0x1a1612, roughness: 1 });
    const T = G.terrain, rnd = mulberry32(2024);
    const density = Q().trees;
    // placement
    const tries = 26000;
    for (let i = 0; i < tries; i++) {
      const x = (rnd() - 0.5) * (WORLD.size - 80), z = (rnd() - 0.5) * (WORLD.size - 80);
      const h = T.heightAt(x, z);
      if (h < WORLD.water + 1.5) continue;
      const n = T.normalAt(x, z);
      if (n.y < 0.8) continue;
      const forest = smooth(-110, -220, x) * (1 - smooth(-330, -470, z));
      const grove = smooth(0.25, 0.6, noise.fbm(x * 0.006 + 20, z * 0.006, 3));
      const mountain = smooth(-230, -380, z) * (1 - smooth(130, 175, h));
      const dv = Math.hypot(x - REGIONS.volcano.x, z - REGIONS.volcano.z);
      if (dv < 230) continue;
      const dVill = Math.hypot(x - REGIONS.village.x, z - REGIONS.village.z);
      if (dVill < 165) continue;
      if (Math.hypot(x - REGIONS.lake.x, z - REGIONS.lake.z) < 150) continue;
      let p = forest * 0.95 + grove * 0.22 + mountain * 0.45;
      if (rnd() > p * density) continue;
      if (skip(x, z)) continue;
      const pine = mountain > 0.3 || (forest > 0.5 && rnd() < 0.18) || h > 70;
      const scale = pine ? 0.75 + rnd() * 0.55 : (forest > 0.6 ? 0.9 + rnd() * 0.8 : 0.6 + rnd() * 0.5);
      this.trees.push({ x, z, y: h - 0.4, rot: rnd() * Math.PI * 2, scale, sp: pine ? 1 : 0, state: TREE_ALIVE, hp: 60 * scale, burn: 0 });
    }
    const counts = [0, 0];
    for (const t of this.trees) t.slot = counts[t.sp]++;
    const mk = (geo, mat, n) => { const m = new THREE.InstancedMesh(geo, mat, Math.max(1, n)); m.count = 0; m.castShadow = true; m.receiveShadow = true; m.frustumCulled = false; this.scene.add(m); return m; };
    this.meshes = [
      { near: [mk(S.oakTrunk, this.trunkMat, counts[0]), mk(S.oakLeaves, this.leafMat, counts[0])], far: [mk(S.oakTrunkFar, this.trunkMat, counts[0]), mk(S.oakLeavesFar, this.leafMat, counts[0])] },
      { near: [mk(S.pineTrunk, this.trunkMat, counts[1]), mk(S.pineLeaves, this.leafMat, counts[1])], far: [mk(S.pineTrunkFar, this.trunkMat, counts[1]), mk(S.pineLeavesFar, this.leafMat, counts[1])] },
    ];
    for (const sp of this.meshes) for (const m of sp.far) m.castShadow = false;
    this.burntMesh = [mk(S.oakTrunk, this.burntMat, counts[0]), mk(S.pineTrunk, this.burntMat, counts[1])];

    // buissons et rochers (statiques, instanciés)
    const bushes = [], rocks = [];
    for (let i = 0; i < 9000 && bushes.length < 2200 * density; i++) {
      const x = (rnd() - 0.5) * (WORLD.size - 60), z = (rnd() - 0.5) * (WORLD.size - 60), h = T.heightAt(x, z);
      if (h < WORLD.water + 1.2 || h > 120 || Math.hypot(x, z) < 120) continue;
      if (Math.hypot(x - REGIONS.volcano.x, z - REGIONS.volcano.z) < 200) continue;
      const f = smooth(-90, -200, x) * 0.7 + smooth(0.3, 0.7, noise.fbm(x * 0.01, z * 0.01 + 5, 2)) * 0.4;
      if (rnd() < f && !skip(x, z)) bushes.push([x, h, z, 0.6 + rnd() * 1.2, rnd() * 6]);
    }
    for (let i = 0; i < 4000 && rocks.length < 500; i++) {
      const x = (rnd() - 0.5) * (WORLD.size - 60), z = (rnd() - 0.5) * (WORLD.size - 60), h = T.heightAt(x, z);
      if (h < WORLD.water || Math.hypot(x, z) < 140 || skip(x, z)) continue;
      const n = T.normalAt(x, z);
      if (rnd() < 0.25 + (1 - n.y) * 2) rocks.push([x, h, z, 0.5 + rnd() * 2.2, rnd() * 6]);
    }
    const inst = (geo, mat, list) => {
      const m = new THREE.InstancedMesh(geo, mat, list.length), M = new THREE.Matrix4(), q = new THREE.Quaternion();
      list.forEach(([x, y, z, s, r], i) => { q.setFromAxisAngle(new THREE.Vector3(0, 1, 0), r); M.compose(new THREE.Vector3(x, y, z), q, new THREE.Vector3(s, s, s)); m.setMatrixAt(i, M); });
      m.castShadow = true; m.receiveShadow = true; this.scene.add(m); return m;
    };
    this.bushData = bushes; this.rockData = rocks;
    this.bushes = inst(S.bush, this.leafMat, bushes);
    this.rocks = inst(S.rock, new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.92, flatShading: true }), rocks);
    this.updateLOD(true);
    if (Q().grass) this.buildGrass(Q().grass);
  }

  // ---------- Herbe : touffes recyclées autour de la caméra, posées par le shader ----------
  buildGrass(count) {
    const blade = new THREE.BufferGeometry();
    const pos = [], col = [];
    for (let b = 0; b < 5; b++) {
      const a = b / 5 * Math.PI + Math.random() * 0.4, ox = (Math.random() - 0.5) * 0.6, oz = (Math.random() - 0.5) * 0.6;
      const h = 0.55 + Math.random() * 0.5, w = 0.09;
      const dx = Math.cos(a) * w, dz = Math.sin(a) * w, lean = (Math.random() - 0.5) * 0.25;
      pos.push(ox - dx, 0, oz - dz, ox + dx, 0, oz + dz, ox + lean, h, oz + lean * 0.5);
      col.push(0.18, 0.3, 0.08, 0.18, 0.3, 0.08, 0.55, 0.7, 0.3);
    }
    blade.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    blade.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    blade.computeVertexNormals();
    const geo = new THREE.InstancedBufferGeometry();
    geo.setAttribute('position', blade.attributes.position);
    geo.setAttribute('color', blade.attributes.color);
    geo.setAttribute('normal', blade.attributes.normal);
    const R = 62, off = new Float32Array(count * 3);
    for (let i = 0; i < count; i++) { off[i * 3] = Math.random() * R * 2; off[i * 3 + 1] = Math.random() * R * 2; off[i * 3 + 2] = 0.7 + Math.random() * 0.8; }
    geo.setAttribute('aOff', new THREE.InstancedBufferAttribute(off, 3));
    geo.instanceCount = count;
    const mat = new THREE.MeshLambertMaterial({ vertexColors: true, side: THREE.DoubleSide });
    mat.onBeforeCompile = (sh) => {
      sh.uniforms.uTime = G.uniforms.time; sh.uniforms.uGust = G.uniforms.gust; sh.uniforms.uHeight = { value: G.terrain.dataTex };
      sh.uniforms.uCenter = this.grassCenter = { value: new THREE.Vector2() }; sh.uniforms.uR = { value: R }; sh.uniforms.uHalf = { value: WORLD.half };
      sh.uniforms.uPress = G.uniforms.press;
      sh.vertexShader = sh.vertexShader
        .replace('#include <common>', `#include <common>
          attribute vec3 aOff; uniform float uTime, uR, uHalf; uniform vec4 uGust; uniform vec4 uPress; uniform vec2 uCenter; uniform sampler2D uHeight; varying float vFade;`)
        .replace('#include <begin_vertex>', `
          vec2 wp = uCenter + mod(aOff.xy - uCenter + uR, 2.0*uR) - uR;
          vec4 tex = texture2D(uHeight, (wp + uHalf) / (uHalf*2.0));
          float dens = tex.g; float ok = step(fract(aOff.z*7.31), dens);
          float d = length(wp - uCenter); vFade = 1.0 - smoothstep(uR*0.7, uR, d);
          vec3 transformed = position * aOff.z * ok * vFade;
          float h = transformed.y;
          float sway = sin(uTime*1.8 + wp.x*0.3 + wp.y*0.2)*0.18 + sin(uTime*3.1 + wp.y*0.5)*0.06;
          vec2 gd = wp - uGust.xz; float gl = length(gd)+0.01; float g = uGust.w*exp(-gl/30.0);
          vec2 pd = wp - uPress.xy; float pl = length(pd)+0.01; float pr = smoothstep(uPress.z, 0.0, pl) * uPress.w;
          transformed.xz += (vec2(sway, sway*0.6) + (gd/gl)*g*0.18 + (pd/pl)*pr*0.9) * h;
          transformed.y *= 1.0 - pr*0.7;
          transformed += vec3(wp.x, tex.r, wp.y);`);
    };
    this.grass = new THREE.Mesh(geo, mat);
    this.grass.frustumCulled = false;
    this.grass.receiveShadow = true;
    this.scene.add(this.grass);
  }

  matrixFor(t, M) {
    const q = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 1, 0), t.rot);
    return M.compose(new THREE.Vector3(t.x, t.y, t.z), q, new THREE.Vector3(t.scale, t.scale * (t.sp ? 1 : 0.95), t.scale));
  }

  // répartit les arbres entre détail proche et silhouettes lointaines
  updateLOD(force) {
    const cam = G.camera ? G.camera.position : new THREE.Vector3();
    const M = new THREE.Matrix4();
    const n = [[0, 0], [0, 0]], burnt = [0, 0];
    const nearD2 = 230 * 230;
    for (const t of this.trees) {
      if (t.state === TREE_FALLEN || t.state === TREE_GONE) continue;
      this.matrixFor(t, M);
      if (t.state === TREE_BURNT) { this.burntMesh[t.sp].setMatrixAt(burnt[t.sp]++, M); continue; }
      const d2 = (t.x - cam.x) ** 2 + (t.z - cam.z) ** 2;
      const lod = d2 < nearD2 ? 0 : 1;
      const set = lod ? this.meshes[t.sp].far : this.meshes[t.sp].near;
      const k = n[t.sp][lod]++;
      set[0].setMatrixAt(k, M); set[1].setMatrixAt(k, M);
    }
    for (let sp = 0; sp < 2; sp++) {
      for (const [lod, key] of [[0, 'near'], [1, 'far']]) for (const m of this.meshes[sp][key]) { m.count = n[sp][lod]; m.instanceMatrix.needsUpdate = true; }
      this.burntMesh[sp].count = burnt[sp]; this.burntMesh[sp].instanceMatrix.needsUpdate = true;
    }
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
    const grp = new THREE.Group();
    const S = this.geo;
    const trunk = new THREE.Mesh(t.sp ? S.pineTrunk : S.oakTrunk, wasBurnt ? this.burntMat : this.trunkMat);
    trunk.castShadow = true; grp.add(trunk);
    if (!wasBurnt) { const lv = new THREE.Mesh(t.sp ? S.pineLeaves : S.oakLeaves, this.leafMat); lv.castShadow = true; grp.add(lv); }
    grp.scale.setScalar(t.scale);
    grp.position.set(t.x, t.y, t.z);
    grp.rotation.y = t.rot;
    this.scene.add(grp);
    const axis = new THREE.Vector3(dz, 0, -dx).normalize();
    const f = { grp, t, axis, angle: 0, av: launch > 0 ? rand(1.5, 3) : 0.15, vel: new THREE.Vector3(dx * launch * 0.5, launch * 0.35, dz * launch * 0.5), flying: launch > 8, life: 0, settled: false, baseQ: grp.quaternion.clone() };
    this.falling.push(f);
    if (G.fx) G.fx.dust(new THREE.Vector3(t.x, t.y + 1, t.z), 6, 4);
    if (G.audio) G.audio.treeCrack(t.x, t.z);
  }

  update(dt) {
    this.lodTimer -= dt;
    if (this.dirty || this.lodTimer <= 0) { this.updateLOD(); this.lodTimer = 0.5; this.dirty = false; }
    if (this.grassCenter && G.camera) this.grassCenter.value.set(G.camera.position.x, G.camera.position.z);
    const T = G.terrain, q = new THREE.Quaternion();
    for (let i = this.falling.length - 1; i >= 0; i--) {
      const f = this.falling[i];
      if (f.settled) continue;
      f.life += dt;
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
          f.angle = Math.PI / 2 - 0.05; f.settled = true;
          const tip = f.grp.position.clone().addScaledVector(new THREE.Vector3().crossVectors(f.axis, new THREE.Vector3(0, -1, 0)), 10 * f.t.scale);
          if (G.fx) G.fx.dust(tip, 10, 6);
          if (G.audio) G.audio.thud(tip, 0.8);
          if (G.shake) G.shake(0.12, f.grp.position);
          this.logs.push(f);
          if (this.logs.length > 90) { const old = this.logs.shift(); this.scene.remove(old.grp); old.t.state = TREE_GONE; }
        }
      }
      q.setFromAxisAngle(f.axis, f.angle);
      f.grp.quaternion.copy(q).multiply(f.baseQ);
    }
    // arbres en feu
    for (let i = this.burning.length - 1; i >= 0; i--) {
      const b = this.burning[i];
      b.life -= dt;
      if (G.fx && Math.random() < dt * 6) {
        const h = (b.t.sp ? 14 : 12) * b.t.scale;
        G.fx.fire(new THREE.Vector3(b.t.x + rand(-2, 2), b.t.y + rand(3, h), b.t.z + rand(-2, 2)), 1.6 * b.t.scale);
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
