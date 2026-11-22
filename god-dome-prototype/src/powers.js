import * as THREE from 'three';
import { U, GLSL_NOISE } from './shared.js';
import { DOME_R, WORLD_R } from './terrain.js';

/*
 * POUVOIRS DE L'HABITANT — « le chaos du Créateur »
 *  - COMÈTE  : projectile d'énergie, explose en cratère (sol réellement creusé, lave, débris, onde de choc).
 *  - RAYON   : faisceau continu maintenu ; il laboure le sol en tranchée incandescente.
 *  - TEMPÊTE : douze éclairs s'abattent autour de la cible ; le paysage est criblé de cratères.
 *  - ENVOL   : vol libre (on vole là où l'on regarde), traînées lumineuses, lignes de vitesse.
 *  - IMPACT MÉTÉORE : piquer vers le sol en vol = atterrissage explosif.
 * Énergie : jauge propre aux pouvoirs, se recharge seule (la Foi reste réservée au Créateur).
 */
export function createPowers(ctx) {
  const { scene, terrain, world, player, camera, game, T, grassField, vegetation, circles, fx } = ctx;
  const S = { energy: 100, flying: false, beam: false, beamTick: 0, vel: new THREE.Vector3(), cds: { comet: 0, storm: 0, slam: 0 } };
  const COST = { comet: 14, beamPerSec: 34, storm: 70, flyPerSec: 5 };
  const glow = (c, op = 1) => new THREE.SpriteMaterial({ map: T.glow, color: c, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, opacity: op, fog: false });

  // ------------------------------------------------------------ visée : là où regarde la caméra
  const aim = new THREE.Vector3(), aimDir = new THREE.Vector3(), tmp = new THREE.Vector3();
  function updateAim() {
    camera.getWorldDirection(aimDir);
    const o = camera.position;
    let hit = null;
    for (let t = 2; t < 140; t += 0.6) {
      tmp.copy(o).addScaledVector(aimDir, t);
      if (tmp.y < terrain.heightAt(tmp.x, tmp.z) || tmp.length() > DOME_R - 1) { hit = t; break; }
      for (const e of game._enemies) if (!e.dead && e.g.position.distanceToSquared(tmp) < 1.2) { hit = t; break; }
      const b = game.boss; if (!hit && b && !b.dead && b.g.position.distanceToSquared(tmp) < 6) hit = t;
      if (hit) break;
    }
    aim.copy(o).addScaledVector(aimDir, hit || 140);
    return aim;
  }
  function staffTip(out = new THREE.Vector3()) {
    const p = player.root.position, yaw = player.root.rotation.y;
    return out.set(p.x + Math.sin(yaw + 0.6) * 0.45, p.y + 1.35, p.z + Math.cos(yaw + 0.6) * 0.45);
  }

  // ------------------------------------------------------------ débris de roche (physique simple)
  const DEB = 90;
  const debGeo = new THREE.DodecahedronGeometry(0.35, 0);
  const debMat = new THREE.MeshStandardMaterial({ color: '#6d5d50', roughness: 0.9, emissive: '#ff5a10', emissiveIntensity: 0 });
  const debris = new THREE.InstancedMesh(debGeo, debMat, DEB); debris.castShadow = true; debris.frustumCulled = false;
  const deb = Array.from({ length: DEB }, () => ({ p: new THREE.Vector3(0, -999, 0), v: new THREE.Vector3(), r: new THREE.Euler(), w: new THREE.Vector3(), s: 1, life: 0 }));
  let debI = 0; const dm = new THREE.Object3D();
  debris.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(DEB * 3).fill(1), 3);
  scene.add(debris);
  function spawnDebris(pos, n, power, hot = 0.6) {
    for (let i = 0; i < n; i++) {
      const d = deb[debI = (debI + 1) % DEB];
      const a = Math.random() * 6.28, up = 0.5 + Math.random();
      d.p.copy(pos).add(new THREE.Vector3(Math.cos(a), 0.3, Math.sin(a)).multiplyScalar(0.5));
      d.v.set(Math.cos(a) * power * (0.4 + Math.random()), power * up * 1.2, Math.sin(a) * power * (0.4 + Math.random()));
      d.w.set(Math.random() * 8, Math.random() * 8, Math.random() * 8); d.s = 0.3 + Math.random() * Math.random() * 1.1; d.life = 5 + Math.random() * 2;
      debris.setColorAt(debI, new THREE.Color().setScalar(0.7 + Math.random() * 0.5).lerp(new THREE.Color(3, 1.2, 0.3), Math.random() < hot ? 0.7 : 0));
    }
    debris.instanceColor.needsUpdate = true;
  }
  function updateDebris(dt) {
    for (let i = 0; i < DEB; i++) {
      const d = deb[i];
      if (d.life <= 0) { dm.position.set(0, -999, 0); dm.scale.setScalar(0.001); dm.updateMatrix(); debris.setMatrixAt(i, dm.matrix); continue; }
      d.life -= dt; d.v.y -= 22 * dt; d.p.addScaledVector(d.v, dt);
      const g = terrain.heightAt(d.p.x, d.p.z) + 0.15 * d.s;
      if (d.p.y < g) { d.p.y = g; d.v.y *= -0.35; d.v.x *= 0.6; d.v.z *= 0.6; d.w.multiplyScalar(0.6); }
      d.r.x += d.w.x * dt; d.r.y += d.w.y * dt; d.r.z += d.w.z * dt;
      dm.position.copy(d.p); if (d.life < 1) dm.position.y -= (1 - d.life) * 0.4; dm.rotation.copy(d.r); dm.scale.setScalar(d.s * Math.min(1, d.life)); dm.updateMatrix();
      debris.setMatrixAt(i, dm.matrix);
    }
    debris.instanceMatrix.needsUpdate = true;
  }

  // ------------------------------------------------------------ étincelles + poussière (particules GPU)
  const SPK = 700;
  const spkPos = new Float32Array(SPK * 3).fill(-999), spkVel = new Float32Array(SPK * 3), spkLife = new Float32Array(SPK), spkCol = new Float32Array(SPK * 3);
  const spkGeo = new THREE.BufferGeometry();
  spkGeo.setAttribute('position', new THREE.BufferAttribute(spkPos, 3));
  spkGeo.setAttribute('aLife', new THREE.BufferAttribute(spkLife, 1));
  spkGeo.setAttribute('aCol', new THREE.BufferAttribute(spkCol, 3));
  const sparks = new THREE.Points(spkGeo, new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
    vertexShader: `attribute float aLife; attribute vec3 aCol; varying float vL; varying vec3 vC;
      void main(){ vL = aLife; vC = aCol; vec4 mv = modelViewMatrix * vec4(position, 1.0); gl_Position = projectionMatrix * mv; gl_PointSize = min(14.0, clamp(aLife, 0.0, 1.0) * 120.0 / -mv.z); }`,
    fragmentShader: `varying float vL; varying vec3 vC; void main(){ float d = length(gl_PointCoord - 0.5); float a = smoothstep(0.5, 0.0, d) * clamp(vL, 0.0, 1.0); if (a < 0.01) discard; gl_FragColor = vec4(vC * a * 2.5, a); }`,
  }));
  sparks.frustumCulled = false; sparks.renderOrder = 8; scene.add(sparks);
  let spkI = 0;
  function emitSparks(pos, n, speed, col = [1, 0.55, 0.15], life = 1.2) {
    for (let i = 0; i < n; i++) {
      const k = spkI = (spkI + 1) % SPK;
      const a = Math.random() * 6.28, el = Math.random() * 1.3;
      spkPos[k * 3] = pos.x; spkPos[k * 3 + 1] = pos.y; spkPos[k * 3 + 2] = pos.z;
      const sp = speed * (0.3 + Math.random());
      spkVel[k * 3] = Math.cos(a) * Math.cos(el) * sp; spkVel[k * 3 + 1] = Math.sin(el) * sp + speed * 0.3; spkVel[k * 3 + 2] = Math.sin(a) * Math.cos(el) * sp;
      spkLife[k] = life * (0.5 + Math.random() * 0.7);
      spkCol[k * 3] = col[0]; spkCol[k * 3 + 1] = col[1]; spkCol[k * 3 + 2] = col[2];
    }
  }
  function updateSparks(dt) {
    for (let k = 0; k < SPK; k++) {
      if (spkLife[k] <= 0) continue;
      spkLife[k] -= dt;
      spkVel[k * 3 + 1] -= 9 * dt;
      spkPos[k * 3] += spkVel[k * 3] * dt; spkPos[k * 3 + 1] += spkVel[k * 3 + 1] * dt; spkPos[k * 3 + 2] += spkVel[k * 3 + 2] * dt;
      spkVel[k * 3] *= 0.985; spkVel[k * 3 + 2] *= 0.985;
    }
    spkGeo.attributes.position.needsUpdate = true; spkGeo.attributes.aLife.needsUpdate = true; spkGeo.attributes.aCol.needsUpdate = true;
  }
  // poussière / fumée : sprites qui gonflent
  const dusts = [];
  const dustMat = () => new THREE.SpriteMaterial({ map: T.cloud, color: '#8a7a6a', transparent: true, depthWrite: false, opacity: 0.8 });
  function emitDust(pos, n, size) {
    for (let i = 0; i < n; i++) {
      const s = new THREE.Sprite(dustMat()); s.position.copy(pos).add(new THREE.Vector3((Math.random() - 0.5) * size, Math.random() * size * 0.4, (Math.random() - 0.5) * size));
      s.scale.setScalar(size * 0.5); s.material.opacity = 0.55; scene.add(s);
      dusts.push({ s, t: 0, life: 2 + Math.random() * 1.5, grow: size * (1.2 + Math.random()), v: new THREE.Vector3((Math.random() - 0.5) * 2, 1 + Math.random() * 2, (Math.random() - 0.5) * 2) });
    }
  }
  function updateDust(dt) {
    for (let i = dusts.length - 1; i >= 0; i--) {
      const d = dusts[i]; d.t += dt; const k = d.t / d.life;
      d.s.position.addScaledVector(d.v, dt); d.s.scale.setScalar(d.grow * (0.5 + k)); d.s.material.opacity = 0.5 * (1 - k);
      if (k >= 1) { scene.remove(d.s); d.s.material.dispose(); dusts.splice(i, 1); }
    }
  }
  // lumière d'explosion (une seule, réutilisée)
  const boomLight = new THREE.PointLight(0xff8a3a, 0, 40, 1.6); scene.add(boomLight);

  // ------------------------------------------------------------ destruction : cratère + végétation soufflée
  const _m = new THREE.Matrix4(), _p = new THREE.Vector3(), _q = new THREE.Quaternion(), _s = new THREE.Vector3();
  function blowVegetation(x, z, r) {
    for (const mesh of vegetation) {
      let changed = false;
      for (let i = 0; i < mesh.count; i++) {
        mesh.getMatrixAt(i, _m); _m.decompose(_p, _q, _s);
        if (_s.x < 0.01) continue;
        const d = Math.hypot(_p.x - x, _p.z - z);
        if (d < r) {
          if (mesh.userData.isTree && mesh.userData.isTrunk) { spawnDebris(_p.clone().setY(_p.y + 2), 6, 9, 0.2); emitSparks(_p.clone().setY(_p.y + 4), 20, 6, [0.5, 0.9, 0.3], 1.5); }
          _s.setScalar(0.0001); _m.compose(_p, _q, _s); mesh.setMatrixAt(i, _m); changed = true;
        }
      }
      if (changed) mesh.instanceMatrix.needsUpdate = true;
    }
    for (let i = circles.length - 1; i >= 0; i--) if (Math.hypot(circles[i].x - x, circles[i].z - z) < r) circles.splice(i, 1);
  }
  function explode(pos, r, { dmg = 4, heat = 1, color = [1, 0.55, 0.15], shake = 1 } = {}) {
    const now = U.uTime.value;
    const g = terrain.heightAt(pos.x, pos.z);
    const ground = pos.y - g < r * 1.2;
    if (ground && Math.hypot(pos.x, pos.z) < WORLD_R - 3) {
      terrain.deform(pos.x, pos.z, r, r * 0.55, heat, now);
      grassField.userData.syncTerrain(pos.x, pos.z, r * 1.5);
      blowVegetation(pos.x, pos.z, r * 1.25);
    }
    const c = pos.clone(); if (ground) c.y = g + 0.4;
    spawnDebris(c, Math.round(6 + r * 4), 6 + r * 2.2, heat);
    emitSparks(c, Math.round(40 + r * 30), 8 + r * 3, color, 1.4);
    emitDust(c, Math.round(3 + r), r * 2.2);
    boomLight.position.copy(c).y += 3; boomLight.intensity = 14 * r; boomLight.color.setRGB(color[0], color[1] * 0.9, color[2] + 0.1);
    const ring = new THREE.Mesh(new THREE.RingGeometry(0.85, 1, 64), new THREE.MeshBasicMaterial({ color: new THREE.Color(color[0] * 2, color[1] * 2, color[2] * 2), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, fog: false }));
    ring.rotation.x = -Math.PI / 2; ring.position.copy(c); scene.add(ring); rings.push({ m: ring, t: 0, r: r * 3.2 });
    const flash = new THREE.Sprite(glow(new THREE.Color(color[0] * 0.55, color[1] * 0.55, color[2] * 0.55))); flash.position.copy(c).y += 0.8; flash.scale.setScalar(r * 3.5); scene.add(flash); flashes.push({ s: flash, t: 0 });
    game.blast(c, r * 2.2, dmg, 18);
    const dist = camera.position.distanceTo(c);
    fx.shake(Math.min(1.4, shake * r * 3 / Math.max(4, dist)));
    fx.shock(c, Math.min(1, r * 4 / Math.max(6, dist)));
  }
  const rings = [], flashes = [];

  // ------------------------------------------------------------ COMÈTE
  const comets = [];
  function comet() {
    if (S.cds.comet > 0 || !spend(COST.comet)) return;
    S.cds.comet = 0.35;
    const from = staffTip(), to = updateAim().clone();
    const head = new THREE.Sprite(glow('#5fc8e0')); head.scale.setScalar(1.0); head.position.copy(from);
    const core = new THREE.Sprite(glow('#ffffff')); core.scale.setScalar(0.7); head.add(core); core.scale.setScalar(0.45);
    scene.add(head);
    comets.push({ s: head, v: to.sub(from).normalize().multiplyScalar(48), life: 3, trail: 0 });
    player.state.attack = 1;
    emitSparks(from, 12, 4, [0.5, 0.95, 1], 0.5);
  }
  function updateComets(dt) {
    for (let i = comets.length - 1; i >= 0; i--) {
      const c = comets[i]; c.life -= dt;
      c.s.position.addScaledVector(c.v, dt);
      c.trail += dt; if (c.trail > 0.016) { c.trail = 0; emitSparks(c.s.position, 3, 1.2, [0.45, 0.9, 1], 0.45); }
      const p = c.s.position;
      let hit = p.y < terrain.heightAt(p.x, p.z) + 0.2 || p.length() > DOME_R - 1 || c.life <= 0;
      for (const e of game._enemies) if (!e.dead && e.g.position.distanceTo(p) < 1.3) hit = true;
      const b = game.boss; if (b && !b.dead && b.g.position.distanceTo(p) < 2.3) hit = true;
      if (hit) { explode(p.clone(), 2.6, { dmg: 3, color: [0.45, 0.9, 1.0], heat: 0.8 }); scene.remove(c.s); comets.splice(i, 1); }
    }
  }

  // ------------------------------------------------------------ RAYON (maintenu)
  const beamMat = new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide,
    uniforms: { uTime: U.uTime, uOn: { value: 0 } },
    vertexShader: `varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
    fragmentShader: `uniform float uTime, uOn; varying vec2 vUv; ${GLSL_NOISE}
      void main(){ float across = abs(vUv.x - 0.5) * 2.0;
        float n = gd_vn(vec2(vUv.y * 18.0 - uTime * 30.0, vUv.x * 4.0));
        float core = smoothstep(0.35, 0.0, across);
        float body = smoothstep(1.0, 0.1, across) * (0.5 + 0.6 * n);
        vec3 c = vec3(0.35, 0.85, 1.0) * body * 1.1 + vec3(1.0) * core * 1.5;
        gl_FragColor = vec4(c * uOn, (body + core) * uOn); }`,
  });
  const beamGeo = new THREE.CylinderGeometry(1, 1, 1, 16, 1, true); beamGeo.translate(0, 0.5, 0); beamGeo.rotateX(Math.PI / 2);
  const beam = new THREE.Group();
  const beamOuter = new THREE.Mesh(beamGeo, beamMat); beamOuter.scale.set(0.32, 0.32, 1); beam.add(beamOuter);
  const beamTipGlow = new THREE.Sprite(glow('#9ff6ff', 0.6)); beamTipGlow.scale.setScalar(1.2);
  const beamHitGlow = new THREE.Sprite(glow('#bfefff', 0.7)); beamHitGlow.scale.setScalar(2.5);
  beam.visible = false; scene.add(beam, beamTipGlow, beamHitGlow); beamTipGlow.visible = beamHitGlow.visible = false;
  function setBeam(on) { S.beam = on; }
  function updateBeam(dt) {
    const on = S.beam && S.energy > 2;
    beamMat.uniforms.uOn.value += ((on ? 1 : 0) - beamMat.uniforms.uOn.value) * Math.min(1, dt * 18);
    const vis = beamMat.uniforms.uOn.value > 0.02;
    beam.visible = beamTipGlow.visible = beamHitGlow.visible = vis;
    if (!vis) return;
    const from = staffTip(), to = updateAim().clone();
    const len = from.distanceTo(to);
    beam.position.copy(from); beam.lookAt(to); beam.scale.set(1 + Math.sin(U.uTime.value * 40) * 0.08, 1 + Math.sin(U.uTime.value * 40) * 0.08, len);
    beamTipGlow.position.copy(from); beamHitGlow.position.copy(to); beamHitGlow.scale.setScalar(2.2 + Math.random() * 1.0);
    if (!on) return;
    S.energy -= COST.beamPerSec * dt;
    player.state.attack = Math.max(player.state.attack, 0.5);
    game.rayHit(from, to, 1.3, 7 * dt);
    S.beamTick -= dt;
    if (S.beamTick <= 0) {
      S.beamTick = 0.09;
      if (to.y - terrain.heightAt(to.x, to.z) < 0.8) {
        terrain.deform(to.x, to.z, 1.25, 0.32, 1, U.uTime.value);
        grassField.userData.syncTerrain(to.x, to.z, 2);
        blowVegetation(to.x, to.z, 1.3);
        if (Math.random() < 0.5) spawnDebris(to, 1, 5, 0.9);
      }
      emitSparks(to, 14, 6, [0.6, 0.95, 1.0], 0.8);
      if (Math.random() < 0.2) emitDust(to, 1, 1.6);
      fx.shake(0.12);
    }
  }

  // ------------------------------------------------------------ TEMPÊTE DIVINE
  function storm() {
    if (S.cds.storm > 0 || !spend(COST.storm)) return;
    S.cds.storm = 8;
    const center = updateAim().clone();
    if (center.distanceTo(player.root.position) > 60) center.copy(player.root.position);
    fx.darken(3.4);
    for (let i = 0; i < 12; i++) {
      setTimeout(() => {
        const a = Math.random() * 6.28, r = 3 + Math.random() * 16;
        const x = center.x + Math.cos(a) * r, z = center.z + Math.sin(a) * r;
        const p = new THREE.Vector3(x, terrain.heightAt(x, z), z);
        if (Math.hypot(x, z) > WORLD_R - 5) return;
        bolt(p);
        explode(p, 3, { dmg: 8, color: [0.75, 0.85, 1.0], heat: 1, shake: 1.3 });
      }, 250 + i * 230 + Math.random() * 120);
    }
  }
  function bolt(point) {
    const top = new THREE.Vector3(point.x + (Math.random() - 0.5) * 20, Math.min(DOME_R * 0.85, point.y + 80), point.z + (Math.random() - 0.5) * 20);
    const pts = []; const N = 18;
    for (let i = 0; i <= N; i++) { const t = i / N; const q = top.clone().lerp(point, t); if (i > 0 && i < N) { q.x += (Math.random() - 0.5) * 5 * (1 - t * 0.5); q.z += (Math.random() - 0.5) * 5 * (1 - t * 0.5); } pts.push(q); }
    const curve = new THREE.CatmullRomCurve3(pts, false, 'catmullrom', 0.05);
    const geo = new THREE.TubeGeometry(curve, 80, 0.35, 6, false);
    const m = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ color: new THREE.Color(6, 6.5, 9), transparent: true, fog: false }));
    // ramification secondaire
    const br = [pts[6].clone(), pts[6].clone().add(new THREE.Vector3((Math.random() - 0.5) * 14, -10, (Math.random() - 0.5) * 14)), pts[6].clone().add(new THREE.Vector3((Math.random() - 0.5) * 20, -22, (Math.random() - 0.5) * 20))];
    const g2 = new THREE.TubeGeometry(new THREE.CatmullRomCurve3(br), 20, 0.15, 5, false);
    const m2 = new THREE.Mesh(g2, m.material);
    scene.add(m, m2);
    flashes.push({ s: m, t: 0, bolt: true, extra: m2 });
    fx.flash(0.35);
  }

  // ------------------------------------------------------------ ENVOL
  const aura = new THREE.Sprite(glow('#7fe3ff', 0)); aura.scale.setScalar(3.2); scene.add(aura);
  const TRL = 40;
  const trails = [0, 1].map(() => {
    const pos = new Float32Array(TRL * 2 * 3), a = new Float32Array(TRL * 2);
    for (let i = 0; i < TRL; i++) { a[i * 2] = a[i * 2 + 1] = 1 - i / TRL; }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(pos, 3)); g.setAttribute('aA', new THREE.BufferAttribute(a, 1));
    const idx = []; for (let i = 0; i < TRL - 1; i++) { const k = i * 2; idx.push(k, k + 1, k + 2, k + 1, k + 3, k + 2); } g.setIndex(idx);
    const m = new THREE.Mesh(g, new THREE.ShaderMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide, uniforms: { uOn: { value: 0 } },
      vertexShader: 'attribute float aA; varying float vA; void main(){ vA = aA; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
      fragmentShader: 'uniform float uOn; varying float vA; void main(){ gl_FragColor = vec4(vec3(0.45, 0.9, 1.0) * 1.2 * vA * vA * uOn, vA * vA * uOn * 0.8); }' }));
    m.frustumCulled = false; scene.add(m);
    return { m, hist: [] };
  });
  function toggleFlight() {
    if (!S.flying && S.energy < 15) { ctx.ui.toast('Énergie insuffisante pour voler', 'warn'); return; }
    S.flying = !S.flying;
    if (S.flying) { S.vel.set(0, 9, 0); emitSparks(player.root.position.clone().setY(player.root.position.y + 0.3), 40, 6, [0.5, 0.9, 1], 0.8); emitDust(player.root.position, 3, 2.5); fx.shock(player.root.position, 0.3); }
    ctx.onFlight(S.flying);
  }
  // déplacement en vol : appelé par main.js à la place de la marche
  function fly(dt, jx, jy, boost, ascend) {
    const p = player.root.position;
    const f = new THREE.Vector3(); camera.getWorldDirection(f);
    const r = new THREE.Vector3().crossVectors(f, new THREE.Vector3(0, 1, 0)).normalize();
    const want = new THREE.Vector3().addScaledVector(f, jy).addScaledVector(r, jx);
    if (ascend) want.y += 0.8;
    const speed = (boost ? 34 : 17);
    want.multiplyScalar(speed);
    S.vel.lerp(want, Math.min(1, dt * 3));
    const vy = S.vel.y;
    p.addScaledVector(S.vel, dt);
    p.y += Math.sin(U.uTime.value * 3) * 0.01;
    const g = world.groundAt(p.x, p.z, p.y);
    // impact météore : piqué rapide vers le sol
    if (p.y < g + 0.2) {
      p.y = g + 0.2;
      if (vy < -13 && S.cds.slam <= 0) { S.cds.slam = 1; explode(p.clone(), 3.6, { dmg: 10, color: [0.5, 0.9, 1.0], heat: 0.9, shake: 1.6 }); S.flying = false; ctx.onFlight(false); }
      else S.vel.y = Math.max(0, S.vel.y);
    }
    const ceil = Math.sqrt(Math.max(0, (DOME_R - 4) ** 2 - p.x * p.x - p.z * p.z));
    if (p.y > ceil) { p.y = ceil; S.vel.y = Math.min(0, S.vel.y); }
    const rr = Math.hypot(p.x, p.z), lim = WORLD_R - 4; if (rr > lim) { p.x *= lim / rr; p.z *= lim / rr; }
    // orientation : buste penché dans le sens du vol
    const hs = Math.hypot(S.vel.x, S.vel.z);
    if (hs > 0.5) { let d = Math.atan2(S.vel.x, S.vel.z) - player.root.rotation.y; d = Math.atan2(Math.sin(d), Math.cos(d)); player.root.rotation.y += d * Math.min(1, dt * 6); }
    player.root.rotation.x = Math.min(1.2, hs / 22) ;
    return S.vel.length();
  }
  function updateFlightFx(dt) {
    const on = S.flying ? 1 : 0;
    aura.material.opacity += (on * 0.22 - aura.material.opacity) * Math.min(1, dt * 6);
    aura.position.copy(player.root.position).y += 0.9;
    aura.scale.setScalar(2.2 + Math.sin(U.uTime.value * 9) * 0.2);
    if (!S.flying) player.root.rotation.x *= Math.pow(0.001, dt);
    const p = player.root.position, yaw = player.root.rotation.y;
    trails.forEach((t, i) => {
      const side = i ? 1 : -1;
      const anchor = new THREE.Vector3(p.x + Math.cos(yaw) * 0.32 * side, p.y + 0.9, p.z - Math.sin(yaw) * 0.32 * side);
      t.hist.unshift(anchor); if (t.hist.length > TRL) t.hist.pop();
      const pos = t.m.geometry.attributes.position;
      const w = 0.07;
      for (let k = 0; k < TRL; k++) {
        const h = t.hist[Math.min(k, t.hist.length - 1)];
        pos.setXYZ(k * 2, h.x, h.y + w * (1 - k / TRL), h.z); pos.setXYZ(k * 2 + 1, h.x, h.y - w * (1 - k / TRL), h.z);
      }
      pos.needsUpdate = true;
      t.m.material.uniforms.uOn.value += (on - t.m.material.uniforms.uOn.value) * Math.min(1, dt * 5);
    });
  }

  // ------------------------------------------------------------ énergie + boucle
  function spend(n) { if (S.energy < n) { ctx.ui.toast('Énergie insuffisante', 'warn'); return false; } S.energy -= n; return true; }
  function update(dt) {
    for (const k in S.cds) S.cds[k] = Math.max(0, S.cds[k] - dt);
    if (S.flying) { S.energy -= COST.flyPerSec * dt; if (S.energy <= 0) { S.energy = 0; S.flying = false; ctx.onFlight(false); ctx.ui.toast('Énergie épuisée : atterrissage', 'warn'); } }
    else if (!S.beam) S.energy = Math.min(100, S.energy + 16 * dt);
    updateComets(dt); updateBeam(dt); updateDebris(dt); updateSparks(dt); updateDust(dt); updateFlightFx(dt);
    boomLight.intensity *= Math.pow(0.002, dt);
    for (let i = rings.length - 1; i >= 0; i--) { const r = rings[i]; r.t += dt * 2.4; const k = Math.min(1, r.t); r.m.scale.setScalar(0.4 + k * r.r); r.m.material.opacity = (1 - k); if (k >= 1) { scene.remove(r.m); r.m.geometry.dispose(); rings.splice(i, 1); } }
    for (let i = flashes.length - 1; i >= 0; i--) {
      const f = flashes[i]; f.t += dt;
      const life = f.bolt ? 0.35 : 0.3;
      f.s.material.opacity = Math.max(0, 1 - f.t / life) * (f.bolt ? (0.6 + 0.4 * Math.sin(f.t * 80)) : 1);
      if (f.t > life) { scene.remove(f.s); if (f.extra) { scene.remove(f.extra); f.extra.geometry.dispose(); } if (f.s.geometry && f.bolt) f.s.geometry.dispose(); flashes.splice(i, 1); }
    }
    ctx.ui.energy?.(S.energy, { comet: S.cds.comet / 0.35, storm: S.cds.storm / 8 });
  }
  return { S, update, comet, setBeam, storm, toggleFlight, fly, explode, get flying() { return S.flying; } };
}
