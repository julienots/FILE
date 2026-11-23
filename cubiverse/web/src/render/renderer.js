// Scène 3D : caméra tactile, lumière jour/nuit, ombres, sélection.
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { CUBE, HALF } from '../sim/constants.js';
import { OBJECT_TYPES } from '../sim/themes.js';
import { CubeRenderer } from './cubes.js';
import { CharacterRenderer } from './characters.js';
import { FxRenderer } from './fx.js';

function toonGradient() {
  const data = new Uint8Array([120, 120, 120, 255, 185, 185, 185, 255, 235, 235, 235, 255, 255, 255, 255, 255]);
  const t = new THREE.DataTexture(data, 4, 1, THREE.RGBAFormat);
  t.minFilter = t.magFilter = THREE.NearestFilter; t.needsUpdate = true; return t;
}

const SKY_DAY = new THREE.Color('#bfe3ff'), SKY_DUSK = new THREE.Color('#ffb38a'), SKY_NIGHT = new THREE.Color('#141a3a');

export class Renderer3D {
  constructor(canvas, settings) {
    this.canvas = canvas;
    this.settings = settings;
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: settings.quality !== 'low', powerPreference: 'high-performance', preserveDrawingBuffer: false });
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.shadowMap.enabled = settings.shadows;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.scene = new THREE.Scene();
    this.scene.background = SKY_DAY.clone();
    this.scene.fog = new THREE.Fog(this.scene.background, 60, 220);
    this.camera = new THREE.PerspectiveCamera(42, 1, 0.1, 900);
    this.camera.position.set(9, 9, 14);
    this.controls = new OrbitControls(this.camera, canvas);
    this.controls.enableDamping = true; this.controls.dampingFactor = 0.09;
    this.controls.minDistance = 1.6; this.controls.maxDistance = 420;
    this.controls.maxPolarAngle = Math.PI * 0.49;
    this.controls.screenSpacePanning = false;
    this.controls.touches = { ONE: THREE.TOUCH.ROTATE, TWO: THREE.TOUCH.DOLLY_PAN };
    this.controls.target.set(CUBE, 1, 0);
    this.controls.zoomSpeed = 1.1; this.controls.rotateSpeed = 0.7; this.controls.panSpeed = 0.9;

    const grad = toonGradient();
    this.hemi = new THREE.HemisphereLight(0xffffff, 0x9bb07a, 1.25); this.scene.add(this.hemi);
    this.sun = new THREE.DirectionalLight(0xfff4e0, 1.6);
    this.sun.castShadow = true; this.sun.shadow.mapSize.set(settings.quality === 'high' ? 2048 : 1024, settings.quality === 'high' ? 2048 : 1024);
    const sc = this.sun.shadow.camera; sc.left = -14; sc.right = 14; sc.top = 14; sc.bottom = -14; sc.near = 1; sc.far = 80;
    this.sun.shadow.bias = -0.0008; this.sun.shadow.normalBias = 0.02;
    this.scene.add(this.sun); this.scene.add(this.sun.target);
    // sol
    const ground = new THREE.Mesh(new THREE.CircleGeometry(600, 64), new THREE.MeshLambertMaterial({ color: '#a6d98c' }));
    ground.rotation.x = -Math.PI / 2; ground.position.y = -0.12; ground.receiveShadow = true; this.scene.add(ground);
    this.ground = ground;
    const grid = new THREE.GridHelper(400, 100, 0x000000, 0x000000); grid.material.opacity = 0.05; grid.material.transparent = true; grid.position.y = -0.11; grid.position.x = CUBE / 2 * 0; this.scene.add(grid);

    this.cubes = new CubeRenderer(this.scene, grad);
    this.chars = new CharacterRenderer(this.scene, grad);
    this.fx = new FxRenderer(this.scene, grad);
    this.ray = new THREE.Raycaster();
    this.tmpV = new THREE.Vector3(); this.tmpV2 = new THREE.Vector3(); this.camDir = new THREE.Vector3();
    this.flyTo = null;
    this.followId = null;
    this.cutY = 999;
    this.resize();
  }

  applySettings(s) {
    this.settings = s;
    this.renderer.shadowMap.enabled = s.shadows;
    this.sun.castShadow = s.shadows;
    this.scene.traverse((o) => { if (o.material) o.material.needsUpdate = true; });
    this.resize();
  }

  resize() {
    const w = this.canvas.clientWidth || window.innerWidth, h = this.canvas.clientHeight || window.innerHeight;
    const pr = Math.min(window.devicePixelRatio || 1, this.settings.quality === 'high' ? 2 : this.settings.quality === 'low' ? 1 : 1.5);
    this.renderer.setPixelRatio(pr);
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h; this.camera.updateProjectionMatrix();
  }

  focusCube(cube, dist = 9) {
    dist = Math.max(dist, 7);
    const t = new THREE.Vector3(cube.x * CUBE, cube.y * CUBE + 1.0, cube.z * CUBE);
    const dir = this.camera.position.clone().sub(this.controls.target).normalize();
    if (dir.y < 0.5) { dir.y = 0.62; dir.normalize(); }
    this.flyTo = { target: t, pos: t.clone().add(dir.multiplyScalar(dist)), k: 0 };
  }
  focusPoint(p, dist) {
    const dir = this.camera.position.clone().sub(this.controls.target).normalize();
    this.flyTo = { target: p.clone(), pos: p.clone().add(dir.multiplyScalar(dist ?? this.camera.position.distanceTo(this.controls.target))), k: 0 };
  }

  dayLight(world) {
    const h = world.hour;
    const sunAng = ((h - 6) / 24) * Math.PI * 2; // 6h lever, 18h coucher
    const elev = Math.sin(sunAng);
    const day = THREE.MathUtils.clamp(elev * 2.2 + 0.25, 0, 1);
    const dusk = THREE.MathUtils.clamp(1 - Math.abs(elev) * 4, 0, 1) * (elev > -0.3 ? 1 : 0);
    const bg = this.scene.background;
    bg.copy(SKY_NIGHT).lerp(SKY_DAY, day).lerp(SKY_DUSK, dusk * 0.55);
    this.scene.fog.color.copy(bg);
    this.sun.intensity = 0.25 + day * 1.45;
    this.sun.color.setHSL(0.1, 0.6, 0.65 + day * 0.3);
    this.hemi.intensity = 0.65 + day * 0.7;
    this.hemi.color.copy(bg).lerp(new THREE.Color(0xffffff), 0.55);
    this.ground.material.color.set(day > 0.3 ? '#a6d98c' : '#4a6a5a');
    const t = this.controls.target;
    const sx = Math.cos(sunAng) * 20, sy = Math.max(6, Math.abs(elev) * 30 + 6), sz = 12;
    this.sun.position.set(t.x + sx, t.y + sy, t.z + sz);
    this.sun.target.position.copy(t);
  }

  render(world, dt, state) {
    // caméra : vol animé ou suivi d'un personnage
    if (this.followId) {
      const ch = world.chars.get(this.followId);
      if (ch) {
        const c = world.cubes.get(ch.cubeId);
        const p = this.tmpV.set(c.x * CUBE + ch.x, c.y * CUBE + ch.y + 0.4, c.z * CUBE + ch.z);
        const d = p.clone().sub(this.controls.target);
        this.controls.target.add(d.multiplyScalar(Math.min(1, dt * 4)));
        this.camera.position.add(d);
      } else this.followId = null;
    }
    if (this.flyTo) {
      const f = this.flyTo; f.k = Math.min(1, f.k + dt * 2.2);
      const e = 1 - Math.pow(1 - f.k, 3);
      this.controls.target.lerp(f.target, e * 0.35 + 0.05);
      this.camera.position.lerp(f.pos, e * 0.35 + 0.05);
      if (f.k >= 1 && this.controls.target.distanceTo(f.target) < 0.05) this.flyTo = null;
    }
    this.controls.update();
    this.dayLight(world);
    this.camDir.copy(this.camera.position).sub(this.controls.target).normalize();
    const focus = this.controls.target;
    const dist = this.camera.position.distanceTo(focus);
    const detail = Math.max(4, Math.min(this.settings.detail, dist / CUBE + 3));
    this.cubes.update(world, focus, this.camDir, { cutY: this.cutY, detail, maxDetail: this.settings.maxDetail }, state.forceRebuild);
    state.forceRebuild = false;
    this.dirtyTimer = (this.dirtyTimer || 0) + dt;
    if (this.dirtyTimer > 1) { this.dirtyTimer = 0; this.cubes.updateDirty(world); }
    // personnages visibles : cubes détaillés, triés par distance, plafonnés
    const vis = [];
    const visCubes = new Set();
    const maxChars = this.settings.maxChars;
    for (const cb of this.cubes.detailed) {
      if (vis.length >= maxChars) break;
      visCubes.add(cb.id);
      for (const id of cb.chars) { const ch = world.chars.get(id); if (ch) { vis.push(ch); if (!cb.awake) { ch.animT += dt; if (ch.blend < 1) ch.blend = Math.min(1, ch.blend + dt * 4); } } }
    }
    if (vis.length > maxChars) vis.length = maxChars;
    this.chars.update(world, vis, this.camera, state.selectedChar, { bubbles: this.settings.bubbles });
    const sel = state.selectedCube ? world.cubes.get(state.selectedCube) : null;
    this.cubes.setSelection(world, sel, state.editMode, performance.now() / 1000);
    this.fx.update(world, visCubes, performance.now() / 1000);
    // l'ombre suit la zone regardée
    const sc = this.sun.shadow.camera; const r = THREE.MathUtils.clamp(dist * 0.9, 8, 30);
    if (Math.abs(sc.right - r) > 1) { sc.left = -r; sc.right = r; sc.top = r; sc.bottom = -r; sc.updateProjectionMatrix(); }
    this.renderer.render(this.scene, this.camera);
    return { visibleChars: vis.length, detailed: this.cubes.detailed.length };
  }

  // ----- sélection tactile -----
  pick(world, clientX, clientY, editMode) {
    const rect = this.canvas.getBoundingClientRect();
    const nx = ((clientX - rect.left) / rect.width) * 2 - 1, ny = -((clientY - rect.top) / rect.height) * 2 + 1;
    this.ray.setFromCamera({ x: nx, y: ny }, this.camera);
    // 1) cubes fantômes (ajout)
    if (editMode && this.cubes.ghosts.length) {
      const hits = this.ray.intersectObjects(this.cubes.ghosts.map((g) => g.children[0]), false);
      if (hits.length) return { type: 'ghost', grid: hits[0].object.userData.grid };
    }
    // 2) cube touché (rayon / boîtes)
    const o = this.ray.ray.origin, d = this.ray.ray.direction;
    let best = null, bt = Infinity;
    for (const cb of world.cubes.values()) {
      if (cb.y > this.cutY) continue;
      const t = rayBox(o, d, cb.x * CUBE - HALF, cb.y * CUBE, cb.z * CUBE - HALF, cb.x * CUBE + HALF, cb.y * CUBE + CUBE, cb.z * CUBE + HALF);
      if (t !== null && t < bt) { bt = t; best = cb; }
    }
    // 3) personnage proche du doigt (projection écran)
    let bc = null, bd = (34 * (window.devicePixelRatio || 1)) ** 2 / (window.devicePixelRatio || 1) ** 2;
    const v = this.tmpV2;
    for (const it of this.chars.visible) {
      v.set(it.wx, it.wy - 0.28, it.wz).project(this.camera);
      if (v.z > 1) continue;
      const sx = (v.x + 1) / 2 * rect.width, sy = (1 - v.y) / 2 * rect.height;
      const dd = (sx - (clientX - rect.left)) ** 2 + (sy - (clientY - rect.top)) ** 2;
      if (dd < bd) { bd = dd; bc = it.ch; }
    }
    if (bc && !editMode) return { type: 'char', ch: bc };
    if (best) {
      // point du sol touché, pour poser ou choisir un objet
      let local = null;
      const floorY = best.y * CUBE + 0.06;
      if (Math.abs(d.y) > 1e-4) {
        const t = (floorY - o.y) / d.y;
        if (t > 0) { const px = o.x + d.x * t - best.x * CUBE, pz = o.z + d.z * t - best.z * CUBE; if (Math.abs(px) <= HALF && Math.abs(pz) <= HALF) local = [px, pz]; }
      }
      let obj = null;
      if (local) for (const ob of best.objects) { const r = OBJECT_TYPES[ob.type].r + 0.12; if ((ob.x - local[0]) ** 2 + (ob.z - local[1]) ** 2 < r * r) obj = ob; }
      if (bc && editMode) return { type: 'char', ch: bc, cube: best };
      return { type: 'cube', cube: best, local, obj };
    }
    if (bc) return { type: 'char', ch: bc };
    // sol : point au niveau 0 pour déplacer
    const t = (0 - o.y) / d.y;
    if (t > 0) return { type: 'ground', x: o.x + d.x * t, z: o.z + d.z * t };
    return null;
  }

  groundPoint(clientX, clientY, y = 0) {
    const rect = this.canvas.getBoundingClientRect();
    this.ray.setFromCamera({ x: ((clientX - rect.left) / rect.width) * 2 - 1, y: -((clientY - rect.top) / rect.height) * 2 + 1 }, this.camera);
    const o = this.ray.ray.origin, d = this.ray.ray.direction;
    if (Math.abs(d.y) < 1e-5) return null;
    const t = (y - o.y) / d.y; if (t < 0) return null;
    return [o.x + d.x * t, o.z + d.z * t];
  }

  snapshot(w = 320, h = 200) {
    this.renderer.render(this.scene, this.camera);
    const src = this.renderer.domElement;
    const c = document.createElement('canvas'); c.width = w; c.height = h;
    const g = c.getContext('2d');
    const r = Math.max(w / src.width, h / src.height); const sw = w / r, sh = h / r;
    g.drawImage(src, (src.width - sw) / 2, (src.height - sh) / 2, sw, sh, 0, 0, w, h);
    try { return c.toDataURL('image/jpeg', 0.7); } catch (e) { return null; }
  }
}

function rayBox(o, d, x0, y0, z0, x1, y1, z1) {
  let tmin = -Infinity, tmax = Infinity;
  for (const [oa, da, a0, a1] of [[o.x, d.x, x0, x1], [o.y, d.y, y0, y1], [o.z, d.z, z0, z1]]) {
    if (Math.abs(da) < 1e-9) { if (oa < a0 || oa > a1) return null; continue; }
    let t0 = (a0 - oa) / da, t1 = (a1 - oa) / da; if (t0 > t1) [t0, t1] = [t1, t0];
    tmin = Math.max(tmin, t0); tmax = Math.min(tmax, t1); if (tmin > tmax) return null;
  }
  return tmax < 0 ? null : Math.max(0, tmin);
}
