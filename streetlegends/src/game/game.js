// Moteur de jeu : rendu, boucle à pas fixe, monde, joueur, entités physiques, caméra, effets.
import * as THREE from 'three';
import { World } from '../world/city.js';
import { uniforms } from '../world/city.js';
import { Sky } from '../world/sky.js';
import * as MAP from '../world/map.js';
import { Car, collideStatic, collidePair, resync } from '../vehicle/physics.js';
import { makeView, syncView } from '../vehicle/carView.js';
import { Particles, SkidMarks } from '../vehicle/effects.js';
import { CameraRig } from './camera.js';
import { Input } from './input.js';
import { QUALITY, detectQuality, DynRes } from '../core/quality.js';
import { CAR_BY_ID } from '../data/cars.js';
import { computeStats } from '../data/parts.js';
import { clamp, damp } from '../core/util.js';
import { save } from '../core/save.js';

const STEP = 1 / 120;

export class Game {
  constructor(canvas, profile, hooks = {}) {
    this.profile = profile; this.hooks = hooks; this.canvas = canvas;
    const S = profile.settings;
    if (!S.quality || !QUALITY[S.quality]) S.quality = detectQuality();
    this.q = QUALITY[S.quality];
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance', stencil: false });
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1.05;
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(62, 1, 0.4, 3600);
    this.world = new World(this.scene, this.q);
    this.sky = new Sky(this.scene, this.renderer, this.q);
    this.particles = new Particles(this.scene, 2400);
    this.skids = new SkidMarks(this.scene, 1800);
    this.fx = { particles: this.particles, skids: this.skids, rand: Math.random, wet: 0 };
    this.rig = new CameraRig(this.camera);
    this.rig.mode = S.camera || 'far';
    this.input = new Input(document.getElementById('touch'), S.controls, (a, v) => this.action(a, v));
    this.dyn = new DynRes();
    this.player = null; this.bodies = []; this.systems = []; this.session = null;
    this.paused = true; this.running = false; this.time = 0; this.acc = 0; this.last = 0; this.frame = 0; this.fpsCap = 0;
    this.overlay = null; // scène alternative (garage / showroom)
    this.headlight = new THREE.SpotLight(0xfff1d0, 0, 90, 0.5, 0.6, 1.2);
    this.headlight.castShadow = false;
    this.scene.add(this.headlight, this.headlight.target);
    this.timeScale = 1; this.slowmo = 0;
    this.resize();
    addEventListener('resize', () => this.resize());
    if (screen.orientation) screen.orientation.addEventListener('change', () => setTimeout(() => this.resize(), 150));
    document.addEventListener('visibilitychange', () => { if (document.hidden && this.session && !this.paused) this.hooks.autoPause && this.hooks.autoPause(); this.last = 0; });
    this._loop = (t) => this.loop(t);
  }
  // ------------------------------------------------------------------ réglages
  applySettings() {
    const S = this.profile.settings;
    this.q = QUALITY[S.quality] || QUALITY.medium;
    this.world.setQuality(this.q); this.sky.setQuality(this.q);
    this.resize();
    this.input.s = S.controls; this.input.build();
    if (this.player) { this.player.car.assist = { ...S.assist }; }
    for (const s of this.systems) if (s.setQuality) s.setQuality(this.q);
    this.hooks.settingsChanged && this.hooks.settingsChanged();
  }
  resize() {
    const w = innerWidth, h = innerHeight;
    const pr = Math.min(devicePixelRatio || 1, this.q.pr) * this.dyn.scale * (this.profile.settings.battery ? 0.85 : 1);
    this.renderer.setPixelRatio(Math.max(0.5, pr));
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h;
    // petits écrans en portrait : champ plus large pour garder la voiture lisible
    this.rig.baseFov = w < h ? 78 : 62;
    this.camera.updateProjectionMatrix();
    if (this.overlay && this.overlay.resize) this.overlay.resize(w, h);
  }
  // ------------------------------------------------------------------ joueur
  carStats(id) { const st = this.profile.cars[id] || { upg: {}, cfg: {} }; return computeStats(CAR_BY_ID[id], st.upg, st.cfg); }
  spawnPlayer(carId = this.profile.car, x, z, yaw) {
    const prev = this.player;
    if (prev) { this.scene.remove(prev.view); prev.view.userData.car.dispose(); this.bodies.splice(this.bodies.indexOf(prev.car), 1); }
    const spec = CAR_BY_ID[carId], st = this.profile.cars[carId] || { cfg: {}, upg: {} };
    const car = new Car(spec, computeStats(spec, st.upg, st.cfg), { player: true, id: 0, assist: this.profile.settings.assist });
    const view = makeView(spec, st.cfg, { lod: 0, physical: this.q.physical });
    this.scene.add(view);
    if (x === undefined) { if (prev) { x = prev.car.x; z = prev.car.z; yaw = prev.car.yaw; } else { x = -75; z = 60; yaw = Math.PI / 2; } }
    car.place(x, z, yaw);
    this.player = { car, view, carId };
    this.bodies.unshift(car);
    this.rig.snap();
    return this.player;
  }
  // Replace la voiture sur la route la plus proche (ou sur un point donné).
  resetPlayer(pt) {
    const car = this.player.car;
    if (!pt) {
      const n = MAP.nearestRoad(car.x, car.z, 600, undefined, (e) => e.type !== 'sand' || MAP.surfaceAt(car.x, car.z) === 'sand');
      if (!n) return;
      let yaw = Math.atan2(n.tx, n.tz);
      if (Math.cos(yaw - car.yaw) < 0) yaw += Math.PI;
      const R = MAP.ROAD[n.edge.type], off = R.lanes ? n.hw * 0.45 : 0;
      pt = { x: n.x - Math.cos(yaw) * off, z: n.z + Math.sin(yaw) * off, yaw };
    }
    car.place(pt.x, pt.z, pt.yaw, pt.speed || 0);
    car.damage = 0;
    this.rig.snap();
  }
  addBody(b) { this.bodies.push(b); }
  removeBody(b) { const i = this.bodies.indexOf(b); if (i >= 0) this.bodies.splice(i, 1); }
  action(a, v) {
    if (a === 'camera') { this.profile.settings.camera = this.rig.next(); save(); this.hooks.toast && this.hooks.toast('Caméra : ' + this.hooks.camName(this.rig.mode)); return; }
    if (a === 'lookBack') { this.rig.lookBack = v; return; }
    if (a === 'shiftUp' && this.player) { this.player.car.in.up = true; }
    if (a === 'shiftDown' && this.player) { this.player.car.in.down = true; }
    if (this.session && this.session.onAction) this.session.onAction(a, v);
    this.hooks.action && this.hooks.action(a, v);
  }
  // ------------------------------------------------------------------ boucle
  start() { if (!this.running) { this.running = true; this.last = 0; requestAnimationFrame(this._loop); } }
  loop(t) {
    if (!this.running) return;
    requestAnimationFrame(this._loop);
    const S = this.profile.settings, cap = S.battery ? 30 : S.fps;
    if (this.last && cap < 60 && t - this.last < 1000 / cap - 1.5) return;
    let dt = this.last ? (t - this.last) / 1000 : 1 / 60;
    this.last = t;
    if (this.dyn.tick(dt, Math.min(cap, 60), S.dynRes)) this.resize();
    dt = Math.min(dt, 0.1);
    this.tick(dt);
    this.render();
  }
  // Avance la simulation de dt secondes (utilisé aussi par les tests automatisés).
  tick(dt) {
    this.frame++;
    if (this.overlay) { this.overlay.update(dt); this.hooks.frame && this.hooks.frame(dt); return; }
    if (!this.player) return;
    const car = this.player.car;
    if (!this.paused) {
      const sdt = dt * this.timeScale;
      this.time += sdt;
      const inp = this.input.update(dt);
      if (!car.ai) { car.in.steer = inp.steer; car.in.throttle = inp.throttle; car.in.brake = inp.brake; car.in.handbrake = inp.handbrake; car.in.nitro = inp.nitro; }
      if (this.session && this.session.control) this.session.control(car, dt);
      for (const s of this.systems) s.update(sdt, this);
      if (this.session) this.session.update(sdt, this);
      this.acc += sdt;
      let n = 0;
      const env = this.env || (this.env = { grip: 1 });
      env.grip = this.sky.grip;
      while (this.acc >= STEP && n < 8) {
        this.acc -= STEP; n++;
        this.stepPhysics(STEP, env);
      }
      if (n === 8) this.acc = 0;
      this.sky.update(this.profile.settings.timeMode === 'cycle' ? sdt : 0, this.camera, this.player.view.position, null);
      this.afterPhysics(sdt);
    }
    const pending = this.world.update(car.x, car.z, 1);
    this.pending = pending;
    this.fx.wet = this.sky.wet;
    const lights = this.sky.lightsOn || this.forceLights;
    syncView(car, this.player.view, dt, this.paused ? null : this.fx, lights);
    for (const s of this.systems) if (s.sync) s.sync(dt, this, lights);
    this.rig.update(dt, car, this.player.view);
    // phares du joueur
    const hl = this.headlight;
    if (lights && this.q.carLights) {
      const s = Math.sin(car.yaw), c = Math.cos(car.yaw);
      hl.intensity = 260; hl.position.set(car.x + s * 1.2, car.y + 0.9, car.z + c * 1.2); hl.target.position.set(car.x + s * 30, car.y - 1, car.z + c * 30);
    } else hl.intensity = 0;
    this.world.setNight(this.sky.night, clamp((this.sky.night - 0.25) * 3 + this.sky.fogAmt * 0.6, 0, 1));
    uniforms.uTime.value = this.time;
    this.particles.update(this.paused ? 0 : dt, this.camera, this.scene.fog, innerHeight * this.renderer.getPixelRatio());
    this.skids.flush();
    this.hooks.frame && this.hooks.frame(dt);
  }
  stepPhysics(dt, env) {
    const B = this.bodies;
    for (let i = 0; i < B.length; i++) { const b = B[i]; if (b.step) b.step(dt, b.env || env); }
    for (let i = 0; i < B.length; i++) {
      const a = B[i];
      if (a.noCollide) continue;
      for (let k = i + 1; k < B.length; k++) {
        const b = B[k];
        if (b.noCollide || (a.kin && b.kin)) continue;
        const hit = collidePair(a, b);
        if (hit > 0.5) { this.onHit(a, b, hit); }
      }
    }
    for (let i = 0; i < B.length; i++) {
      const b = B[i];
      if (b.kin && !b.hit) continue;
      const hit = collideStatic(b);
      if (hit > 0 || b.impact > 0) resync(b);
      if (hit > 1.5) this.onHit(b, null, hit);
    }
    for (const s of this.systems) if (s.step) s.step(dt, this);
    if (this.session && this.session.step) this.session.step(dt, this);
  }
  onHit(a, b, v) {
    const pc = this.player.car;
    resync(a); if (b) resync(b);
    if (a.onHit) a.onHit(b, v); if (b && b.onHit) b.onHit(a, v);
    const near = a === pc || b === pc || Math.hypot(a.x - pc.x, a.z - pc.z) < 60;
    if (near && v > 2) this.particles.spark(a.impactX ?? a.x, a.y ?? 0, a.impactZ ?? a.z, a.vx, a.vz);
    if (a === pc || b === pc) {
      const api = this.player.view.userData.car;
      if (v > 4) {
        // déformation locale de la carrosserie au point d'impact
        const dx = pc.impactX - pc.x, dz = pc.impactZ - pc.z, s = Math.sin(pc.yaw), c = Math.cos(pc.yaw);
        api.dent(dx * c - dz * s, 0.5, dx * s + dz * c, v * 0.012);
        pc.damage = Math.min(1, (pc.damage || 0) + v * 0.012);
      }
      this.input.haptic(Math.min(80, 10 + v * 5));
      this.hooks.playerHit && this.hooks.playerHit(v, b);
      if (this.session && this.session.playerHit) this.session.playerHit(v, b);
    }
  }
  afterPhysics(dt) {
    const car = this.player.car, P = this.profile;
    P.stats.km += (car.speed * dt) / 1000;
    P.stats.playTime += dt;
    if (car.kmh > P.stats.topSpeed) P.stats.topSpeed = car.kmh;
    const st = P.cars[this.player.carId]; if (st) st.km += (car.speed * dt) / 1000;
    // hors du monde ou dans l'eau : remise en piste
    if (car.y < MAP.SEA_LEVEL - 1.2 || car.x < MAP.BOUNDS.x0 || car.x > MAP.BOUNDS.x1 || car.z < MAP.BOUNDS.z0) {
      this.particles.debris(car.x, MAP.SEA_LEVEL, car.z, 0.8, 0.9, 1);
      this.resetPlayer(this.session && this.session.respawnPoint ? this.session.respawnPoint() : null);
      this.hooks.toast && this.hooks.toast('Remise en piste');
    }
  }
  render() {
    if (this.overlay) { this.overlay.render(this.renderer); return; }
    this.renderer.render(this.scene, this.camera);
  }
  // Précharge les secteurs autour d'un point (écran de chargement).
  async preload(x, z, onProgress) {
    this.world.update(x, z, 0);
    const total = Math.max(1, this.world.queue.length);
    while (this.world.queue.length) {
      this.world.update(x, z, 3);
      onProgress && onProgress(1 - this.world.queue.length / total);
      await new Promise((r) => setTimeout(r, 0));
    }
  }
}
