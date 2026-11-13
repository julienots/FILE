import * as THREE from 'three';
import { Input } from '../core/Input.js';
import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { createNebula } from '../render/shaders/Nebula.js';
import { createStars } from '../render/shaders/Stars.js';
import { createSkyDome } from '../render/shaders/SkyDome.js';
import { buildPlanet, disposePlanet } from '../world/PlanetFactory.js';
import { scatterProps } from '../world/Props.js';
import { buildStructures, buildSkyClouds } from '../world/Structures.js';
import { Ship } from '../entities/Ship.js';
import { buildHomeBase, BUILD_ORDER, buildCost } from '../world/HomeBase.js';
import { getPlanet } from '../data/planets.js';
import { EVOLUTIONS } from '../data/evolutions.js';
import { Hero } from '../entities/Hero.js';
import { Particles } from '../vfx/Particles.js';
import { EvolutionFX } from '../vfx/EvolutionFX.js';
import { Enemies } from '../entities/Enemies.js';

const V = () => new THREE.Vector3();
const SUN = new THREE.Vector3(200, 300, 150).normalize();
// Exploration à pied sur une planète sphérique: gravité radiale, caméra alignée sur la normale, eau, végétation, collectes.
export class PlanetScene {
  constructor(ctx) {
    this.ctx = ctx; this.scene = new THREE.Scene(); this.camera = new THREE.PerspectiveCamera(55, 1, 0.1, 6000);
    this.hemi = new THREE.HemisphereLight('#cfe9ff', '#6a4d8a', 0.75); this.scene.add(this.hemi);
    this.sun = new THREE.DirectionalLight('#fff2d8', 1.3); this.sun.position.copy(SUN).multiplyScalar(500); this.scene.add(this.sun);
    this.hero = new Hero(); this.scene.add(this.hero.root);
    this.shadow = new THREE.Mesh(new THREE.CircleGeometry(.55, 20), new THREE.MeshBasicMaterial({ color: '#0a0620', transparent: true, opacity: .32, depthWrite: false }));
    this.shadow.renderOrder = 3; this.scene.add(this.shadow);
    this.particles = new Particles(700); this.scene.add(this.particles.points);
    this.evo = new EvolutionFX(this.scene, this.particles); this.enemies = new Enemies(this.scene, this.particles); this.atkCd = 0; this.hurtT = 0; this.waveCd = 0; this.waveT = 0;
    this.wave = new THREE.Mesh(new THREE.RingGeometry(.78, 1, 56), new THREE.MeshBasicMaterial({ color: '#fff', transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide })); this.wave.visible = false; this.scene.add(this.wave);
    bus.on('hero:died', () => { if (this.active) { this.deadT = .001; this._burst(30, this.hero.baseScale * 2, '#ff5a8a'); } });
    bus.on('hero:respawn', () => { if (this.active) this.ctx.transition('planet', { id: this.def.id }, { color: '#ffffff' }); });
    bus.on('enemy:touch', ({ pos }) => { if (!this.active) return; this.hurtT = .4; this.shakeT = .3; this.vh = 7; this.ground = false; this.knock = this.hero.root.position.clone().sub(pos).normalize(); bus.emit('hero:hit', { pos }); });
    this.stars = createStars(3000); this.scene.add(this.stars); this.sky = createSkyDome(); this.scene.add(this.sky); this.sky.material.uniforms.uSun.value.copy(SUN);
    // pickups: orbes de poussière cosmique (instanciés)
    this.orbs = new THREE.InstancedMesh(new THREE.IcosahedronGeometry(.28, 1), new THREE.MeshBasicMaterial({ color: '#fff3a0' }), 64); this.orbs.frustumCulled = false; this.scene.add(this.orbs);
    this.orbData = [];
    // état
    this.dir = V().set(0, 1, 0); this.fwd = V().set(0, 0, -1); this.face = V().set(0, 0, -1); this.vh = 0; this.alt = 0; this.ground = true; this.camPitch = .5; this.stepT = 0;
    this._a = V(); this._b = V(); this._c = V(); this._d = V(); this._e = V(); this._m = new THREE.Matrix4(); this._q = new THREE.Quaternion(); this._s = V();
    bus.on('hero:evolved', ({ stage }) => { if (this.active && stage !== this.hero.stage) this.evo.play(this.hero, stage); });
  }
  enter({ id = 'home', arrive = false } = {}) {
    this.active = true;
    for (const o of [this.planet, this.props, this.nebula, this.structs?.group, this.clouds]) if (o) { this.scene.remove(o); disposePlanet(o); }
    const def = this.def = getPlanet(id); this.planet = buildPlanet(def, { surface: true }); this.scene.add(this.planet);
    const water = this.planet.getObjectByName('water'); if (water) water.material.uniforms.uSun.value.copy(SUN);
    this.enemies.populate(this.planet);
    const sc = scatterProps(this.planet); this.props = sc.group; this.colliders = sc.colliders; this.scene.add(this.props);
    this.structs = buildStructures(this.planet); this.scene.add(this.structs.group); this.colliders.push(...this.structs.colliders);
    // retire les décors qui chevauchent les structures
    this.clouds = def.clouds ? buildSkyClouds(this.planet, def.clouds) : null; if (this.clouds) this.scene.add(this.clouds);
    this.ambient = { meadow: ['#fff3a0', 1, .6], volcanic: ['#ff8a3a', 3, 1.2], ice: ['#ffffff', -2.5, 1.5], jungle: ['#ff7ad8', .8, 1] }[def.biome] ?? ['#ffffff', 1, .5];
    const atmo = def.atmosphere?.color ?? '#8fd0ff';
    this.scene.fog = new THREE.FogExp2(atmo, 0.9 / (def.radius * 6)); this.sky.material.uniforms.uC.value.set(atmo); this.hemi.color.set(atmo).lerp(new THREE.Color('#fff'), .6);
    this.nebula = createNebula({ seed: def.seed }); this.scene.add(this.nebula);
    this.hero.setStage(State.data.hero.stage);
    // point d'apparition: première terre émergée proche du pôle
    const P = this.planet.userData; this.dir.set(0.15, 1, 0.25).normalize();
    for (let i = 0; i < 200 && P.sample(this.dir) - P.sea < .04; i++) this.dir.set(Math.sin(i * 2.4) * i * .02, 1, Math.cos(i * 2.4) * i * .02).normalize();
    this.fwd.set(0, 0, -1).addScaledVector(this.dir, -this.fwd.dot(this.dir)).normalize(); this.face.copy(this.fwd); this.vh = 0; this.alt = 0;
    if (!this.parked) { this.parked = new Ship(); this.parked.flame.visible = false; this.scene.add(this.parked.root); }
    { const pd = V().copy(this.dir).multiplyScalar(P.radius).addScaledVector(this.fwd, -7).normalize(), pr = this.parked.root; pr.position.copy(pd).multiplyScalar(P.heightAt(pd) + .9); pr.quaternion.setFromUnitVectors(V().set(0, 1, 0), pd); this.colliders.push({ pos: pr.position.clone(), r: 2.2 }); }
    this.base?.dispose(); if (this.base) { this.scene.remove(this.base.group); disposePlanet(this.base.group); } this.base = null;
    if (def.type === 'home') { this.base = buildHomeBase(this.planet, this.dir, this.particles); this.scene.add(this.base.group); this.colliders.push(...this.base.colliders); }
    // orbes
    this.orbData.length = 0; let s = def.seed * 131 + 7; const rnd = () => (s = (s * 16807) % 2147483647) / 2147483647;
    for (let i = 0; i < 400 && this.orbData.length < 48; i++) { const u = rnd() * 2 - 1, t = rnd() * 6.283, r = Math.sqrt(1 - u * u), d = V().set(r * Math.cos(t), u, r * Math.sin(t));
      if (P.sample(d) - P.sea > .02) this.orbData.push({ id: `${def.id}:dust:${this.orbData.length}`, dir: d, r: P.heightAt(d) + 1.1, ph: rnd() * 6, alive: true }); }
    this.orbs.count = this.orbData.length;
    this.leaving = 0; this.ground = !arrive; this.deadT = 0; this.hero.scaleMul = 1; this.knock = null;
    if (arrive) { this.alt = P.heightAt(this.dir) - P.radius + 70; this.vh = -8; this.arriving = true; }   // chute depuis le ciel
    this.camera.position.copy(this.dir).multiplyScalar(P.heightAt(this.dir) + (arrive ? 95 : 30)).addScaledVector(this.fwd, -20);
    bus.emit('scene:entered', { id: 'planet', planet: def }); bus.emit('planet:enter', { id: def.id });
  }
  exit() { this.active = false; bus.emit('planet:leave', { id: this.def.id }); }

  update(dt, t) {
    const P = this.planet.userData, R = P.radius, hero = this.hero, m = Input.move, up = this.dir, { _a, _b, _c, _d, _e } = this;
    const cine = this.evo.update(dt, up), scale = hero.baseScale;
    // repère tangent caméra
    const camFwd = _a.copy(this.fwd).addScaledVector(up, -this.fwd.dot(up)).normalize(), right = _b.crossVectors(camFwd, up).normalize();
    if (this.deadT) { this.deadT += dt; hero.scaleMul = Math.max(.05, 1 - this.deadT * 1.2); } const frozen = cine || this.leaving || this.deadT; const mv = _c.copy(camFwd).multiplyScalar(frozen ? 0 : m.y).addScaledVector(right, frozen ? 0 : m.x); let sp = Math.min(1, mv.length());
    const groundR0 = Math.max(P.heightAt(up), P.seaRadius - .35 * scale), wading = groundR0 > P.heightAt(up) + .01;
    const speed = 7.5 * Math.sqrt(scale) * (Input.buttons.boost ? 1.6 : 1) * (wading ? .65 : 1);
    if (sp > .05) {
      mv.normalize(); const ang = (speed * sp * dt) / (R + this.alt), axis = _d.crossVectors(up, mv).normalize();
      up.applyAxisAngle(axis, ang).normalize(); this.fwd.applyAxisAngle(axis, ang); this.face.copy(mv).applyAxisAngle(axis, ang);
    } else sp = 0;
    // collisions props (les grandes formes écrasent tout)
    if (scale < 3) { const hp = _d.copy(up).multiplyScalar(groundR0), hr = .4 * scale;
      for (let i = 0, cs = this.colliders; i < cs.length; i++) { const c = cs[i], dx = hp.x - c.pos.x, dy = hp.y - c.pos.y, dz = hp.z - c.pos.z, rr = c.r + hr, d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < rr * rr && d2 > 1e-6) { const k = (rr - Math.sqrt(d2)) / Math.sqrt(d2); hp.x += dx * k; hp.y += dy * k; hp.z += dz * k; } }
      up.copy(hp).normalize(); }
    // saut / gravité radiale
    const groundR = Math.max(P.heightAt(up), P.seaRadius - .35 * scale), gAlt = groundR - R;
    if (!cine && this.ground && Input.pressed.jump) { this.vh = 10 * Math.sqrt(scale); this.ground = false; this._burst(8, scale, '#ffffff'); }
    this.vh -= (this.leaving ? -60 : 24) * Math.sqrt(scale) * dt; if (this.arriving) this.vh = Math.max(this.vh, -55); this.alt += this.vh * dt;
    if (this.alt <= gAlt) { if (!this.ground && this.vh < -4) this._burst(this.arriving ? 36 : 10, scale * (this.arriving ? 2 : 1), wading ? '#bff4ff' : '#ffe9c0'); if (this.arriving) { this.arriving = false; this.shakeT = .5; bus.emit('cinematic:end', { id: 'landing' }); } this.alt = gAlt; this.vh = 0; this.ground = true; } else if (this.alt > gAlt + .05) this.ground = false;
    // héros: position + orientation
    hero.root.position.copy(up).multiplyScalar(R + this.alt);
    const z = _d.copy(this.face).addScaledVector(up, -this.face.dot(up)).normalize(), x = _e.crossVectors(up, z).normalize();
    hero.root.quaternion.slerp(this._q.setFromRotationMatrix(this._m.makeBasis(x, up, z)), Math.min(1, dt * 12));
    hero.animate(dt, sp, !this.ground, this.vh);
    // attaque: projectile stellaire avec visée auto sur l'ennemi le plus proche
    this.atkCd -= dt;
    if (!frozen && Input.buttons.attack && this.atkCd <= 0) { this.atkCd = .28; const tgt = this.enemies.nearest(hero.root.position, 28 * Math.sqrt(scale));
      if (tgt) this.face.copy(tgt.root.position).sub(hero.root.position).addScaledVector(up, -_c.copy(tgt.root.position).sub(hero.root.position).dot(up)).normalize();
      _c.copy(this.face).addScaledVector(up, -this.face.dot(up)).normalize(); this.enemies.fire(_e.copy(hero.root.position).addScaledVector(up, .6 * scale).addScaledVector(_c, .6 * scale), _c, scale, EVOLUTIONS[hero.stage].glow);
      hero._land = .6; bus.emit('hero:ability', { id: 'starbolt', pos: hero.root.position, dir: _c }); }
    if (this.knock) { up.copy(_c.copy(hero.root.position).addScaledVector(this.knock, 14 * dt)).normalize(); this.hurtT -= dt; if (this.hurtT <= 0) this.knock = null; }
    this.enemies.update(dt, hero.root.position, scale);
    // habitants, secrets, nuages, particules d'ambiance
    const npc = this.structs.update(dt, t, hero.root.position);
    if (npc !== this.nearNpc) { this.nearNpc = npc; bus.emit('npc:near', npc ? { npcId: npc.id, name: npc.name } : { npcId: null }); }
    for (const sc of this.structs.secrets) if (!sc.found && sc.obj.position.distanceTo(hero.root.position) < 4.5 + scale) { sc.found = true; this._burst(40, 3, '#ffffff'); this.shakeT = .4; bus.emit('secret:found', { id: sc.id, planet: this.def.id, xp: 120 }); }
    if (this.base) { const sl = this.base.update(dt, t, hero.root.position), n = this.base.count(); if (sl !== this.nearSlot) { this.nearSlot = sl; bus.emit('build:near', sl ? { slot: sl.i, cost: buildCost(n), buildingId: BUILD_ORDER[n % BUILD_ORDER.length] } : { slot: null }); } } else this.nearSlot = null;
    if (this.clouds) this.clouds.rotation.y += dt * .012;
    if (Math.random() < this.ambient[2] * .5) { _c.copy(hero.root.position).addScaledVector(right, (Math.random() - .5) * 40).addScaledVector(z, (Math.random() - .3) * 40).addScaledVector(up, Math.random() * 10 * (this.ambient[1] < 0 ? 2 : 1));
      this.particles.emit(_c, _e.copy(up).multiplyScalar(this.ambient[1]).addScaledVector(right, Math.random() - .5), this.ambient[0], .22, 3.5, 0, 0); }
    // ombre blob
    const sh = this.shadow, hAbove = this.alt - gAlt + hero.lift; sh.position.copy(up).multiplyScalar(groundR + .06); sh.quaternion.setFromUnitVectors(this._s.set(0, 0, 1), up);
    sh.scale.setScalar(scale * Math.max(.3, 1 - hAbove * .08 / scale)); sh.material.opacity = wading ? 0 : .32 * Math.max(.2, 1 - hAbove * .1 / scale);
    // particules: pas, éclaboussures, traînée d'énergie
    this.stepT -= dt * sp;
    if (this.ground && sp > .4 && this.stepT <= 0) { this.stepT = .09; _e.copy(up).multiplyScalar(1.2 * scale).addScaledVector(z, -1.5 * scale); this.particles.emit(_c.copy(hero.root.position).addScaledVector(up, .1 * scale), _e, wading ? '#bff4ff' : '#bfae9a', (wading ? .5 : .3) * scale, .45, 3, 4); }
    if (hero.stage >= 2 && Math.random() < .5) { _e.copy(up).multiplyScalar(.6 * scale); this.particles.emit(_c.copy(hero.root.position).addScaledVector(up, (.3 + Math.random() * .6) * scale).addScaledVector(z, -.4 * scale), _e, EVOLUTIONS[hero.stage].glow, .28 * scale, .7, 1, 0); }
    this.particles.gravityDir.copy(up).negate(); this.particles.update(dt, innerHeight);
    // orbes: flottement + collecte (aimantation)
    const hpz = hero.root.position, reach = 1.4 * scale + .6;
    for (let i = 0; i < this.orbData.length; i++) { const o = this.orbData[i]; if (!o.alive) continue;
      _c.copy(o.dir).multiplyScalar(o.r + Math.sin(t * 2 + o.ph) * .25); const d = _c.distanceTo(hpz);
      if (d < reach * 2.5) { o.pull = (o.pull || 0) + dt * 3; _c.lerp(hpz, Math.min(1, o.pull)); }
      if (d < reach || o.pull > 1) { o.alive = false; this._s.setScalar(0); this._m.compose(_c, this._q.identity(), this._s); this.orbs.setMatrixAt(i, this._m);
        for (let k = 0; k < 10; k++) this.particles.emit(_c, _e.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(8), '#fff3a0', .35, .5, 3, 0);
        bus.emit('pickup:collected', { id: o.id, kind: 'cosmicDust', amount: 1 }); continue; }
      this._m.compose(_c, this._q.identity(), this._s.setScalar(1 + Math.sin(t * 5 + o.ph) * .15)); this.orbs.setMatrixAt(i, this._m); }
    this.orbs.instanceMatrix.needsUpdate = true;
    // caméra: orbite tactile, recul selon taille + cinématique
    if (Input.look.x) this.fwd.applyAxisAngle(up, -Input.look.x * .006);
    this.camPitch = THREE.MathUtils.clamp(this.camPitch + Input.look.y * .004, .15, 1.3);
    if (cine) this.fwd.applyAxisAngle(up, dt * .8);
    const pull = this.evo.camPull, dist = (7.5 + scale * 3.2) * (1 + pull * .5), cf = _a.copy(this.fwd).addScaledVector(up, -this.fwd.dot(up)).normalize();
    const want = _c.copy(hero.root.position).addScaledVector(cf, -dist * Math.cos(this.camPitch)).addScaledVector(up, dist * Math.sin(this.camPitch) + scale * .5);
    const wr = want.length(), minR = Math.max(P.heightAt(_e.copy(want).normalize()), P.seaRadius) + .8; if (wr < minR) want.multiplyScalar(minR / wr);
    this.camera.position.lerp(want, Math.min(1, dt * 6)); this.camera.up.copy(up); this.camera.lookAt(_e.copy(hero.root.position).addScaledVector(up, scale * (.9 + pull * .6) + hero.lift));
    // ciel
    for (const o of [this.nebula, this.stars, this.sky]) o.position.copy(this.camera.position);
    this.nebula.userData.update(t); this.stars.userData.update(t, this.ctx.renderer.renderer.getPixelRatio()); this.sky.material.uniforms.uUp.value.copy(up);
    const day = THREE.MathUtils.smoothstep(up.dot(SUN), -.3, .3); this.hemi.intensity = .35 + .45 * day;
    if (this.shakeT > 0) { this.shakeT -= dt; this.ctx.renderer.fx.aberration = Math.max(this.ctx.renderer.fx.aberration, this.shakeT * 2.5); this.camera.position.addScaledVector(up, (Math.random() - .5) * this.shakeT * 1.2); }
    if (this.arriving || this.leaving) for (let k = 0; k < 3; k++) { _e.copy(up).multiplyScalar(this.leaving ? -14 : 10); this.particles.emit(_c.copy(hero.root.position).addScaledVector(right, (Math.random() - .5) * scale), _e, EVOLUTIONS[hero.stage].glow, .6 * scale, .5, 1, 0); }
    if (this.leaving && (this.leaving += dt) > .35 && !this._left) { this._left = true; this.ctx.transition('space', { from: this.def.id }, { color: this.def.atmosphere?.color ?? '#ffffff', out: .45, in: .8 }); }
    if (!cine && !this.leaving && !this.arriving && Input.pressed.action && this.nearNpc) bus.emit('npc:interact', { npcId: this.nearNpc.id, name: this.nearNpc.name, line: this.nearNpc.line });
    else if (!cine && !this.leaving && !this.arriving && Input.pressed.action && this.nearSlot) { const n = this.base.count(); bus.emit('build:request', { planetId: this.def.id, slot: this.nearSlot.i, buildingId: BUILD_ORDER[n % BUILD_ORDER.length], cost: buildCost(n) }); if (this.nearSlot.col === null) this.colliders.push(...this.base.colliders.slice(-1)); }
    else if (!cine && !this.leaving && !this.arriving && Input.pressed.action) { this.leaving = .001; this._left = false; this.ground = false; this.vh = 12 * Math.sqrt(scale); bus.emit('cinematic:start', { id: 'takeoff' }); }
    // ✦ Onde de choc (grandit avec le stade; "supernova" au stade divin)
    this.waveCd -= dt;
    if (!frozen && Input.pressed.special && this.waveCd <= 0) { this.waveCd = 2.5; const rad = (5 + hero.stage * 2.5) * Math.sqrt(scale), col = EVOLUTIONS[hero.stage].glow;
      this.wave.visible = true; this.waveT = 0; this.waveR = rad; this.wave.material.color.set(col); this.wave.position.copy(hero.root.position).addScaledVector(up, .2 * scale); this.wave.quaternion.setFromUnitVectors(this._s.set(0, 0, 1), up);
      if (this.ground) { this.vh = 6 * Math.sqrt(scale); this.ground = false; } this.shakeT = .35; this._burst(24, scale * 1.6, col);
      for (const e of this.enemies.list) if (!e.dead && e.root.position.distanceTo(hero.root.position) < rad + e.T.size) this.enemies.hit(e, 2 + hero.stage * 2, hero.root.position);
      bus.emit('hero:ability', { id: hero.stage >= 5 ? 'supernova' : 'shockwave', pos: hero.root.position, radius: rad }); }
    if (this.wave.visible) { this.waveT += dt * 2.2; const k = 1 - Math.pow(1 - Math.min(1, this.waveT), 3); this.wave.scale.setScalar(Math.max(.01, k * this.waveR)); this.wave.material.opacity = (1 - k) * .9; if (this.waveT >= 1) this.wave.visible = false; }
  }
  _burst(n, scale, color) { const p = this._c.copy(this.hero.root.position), e = this._e;
    for (let i = 0; i < n; i++) { const a = i / n * 6.283; e.copy(this._a).multiplyScalar(Math.cos(a)).addScaledVector(this._b, Math.sin(a)).multiplyScalar(4 * scale).addScaledVector(this.dir, 1.5 * scale); this.particles.emit(p, e, color, .35 * scale, .5, 3, 3); } }
}
