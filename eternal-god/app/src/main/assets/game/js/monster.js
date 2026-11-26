// Le Titan de magma : colosse de roche (≈ 26 m) parcouru de fissures de lave. Il émerge du sol,
// marche sur les villages, affronte le dieu (frappe au sol, jet de rocher, souffle de lave),
// chancelle sous les coups et s'effondre en éboulis. Il revient plus tard, d'ailleurs.
import * as THREE from 'three';
import { G, WORLD, clamp, lerp, damp, dampAngle, rand, emit, pick } from './core.js';

const HP = 5200;

export class Monster {
  constructor(scene) {
    this.scene = scene;
    const tl = new THREE.TextureLoader();
    const map = tl.load('img/tex/mossy_rock_diffuse.jpg'), nor = tl.load('img/tex/mossy_rock_nor_gl.jpg');
    map.colorSpace = THREE.SRGBColorSpace;
    for (const t of [map, nor]) { t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(2, 2); }
    this.rockMat = new THREE.MeshStandardMaterial({ map, normalMap: nor, color: 0x6a5e56, roughness: 0.95, flatShading: true });
    this.lavaMat = new THREE.MeshStandardMaterial({ color: 0x2a0a02, emissive: 0xff4a0a, emissiveIntensity: 3, roughness: 0.6 });
    this.root = new THREE.Group();
    this.B = {};
    this.build();
    this.root.visible = false;
    scene.add(this.root);
    this.alive = false;
    this.pos = new THREE.Vector3();
    this.heading = 0;
    this.radius = 9;
    this.respawnT = 50;      // première apparition
    this.state = 'dormant';
    this.t = 0;
    this.phase = 0;
    this.boulders = [];
    this.target = null;
    this.engaged = false;
    this.tag = 'monster'; this.name = 'Titan de magma';
    this.kv = new THREE.Vector3();
  }

  rock(parent, r, x, y, z, sx = 1, sy = 1, sz = 1, detail = 1) {
    const g = new THREE.DodecahedronGeometry(r, detail);
    const p = g.attributes.position;
    for (let i = 0; i < p.count; i++) { const k = 1 + (Math.sin(p.getX(i) * 3.1 + p.getY(i) * 2.3) * 0.12 + Math.sin(p.getZ(i) * 4.7) * 0.08); p.setXYZ(i, p.getX(i) * k, p.getY(i) * k, p.getZ(i) * k); }
    g.computeVertexNormals();
    const m = new THREE.Mesh(g, this.rockMat); m.position.set(x, y, z); m.scale.set(sx, sy, sz); m.castShadow = true;
    parent.add(m);
    // cœur de lave légèrement plus petit : visible entre les plaques
    const l = new THREE.Mesh(new THREE.IcosahedronGeometry(r * 0.86, 1), this.lavaMat); l.position.copy(m.position); l.scale.copy(m.scale); parent.add(l);
    return m;
  }
  joint(parent, name, x, y, z) { const g = new THREE.Group(); g.position.set(x, y, z); parent.add(g); this.B[name] = g; return g; }

  build() {
    const S = this;
    const hips = this.joint(this.root, 'hips', 0, 11, 0);
    this.rock(hips, 3.2, 0, 0, 0, 1.4, 0.8, 1);
    const spine = this.joint(hips, 'spine', 0, 1.5, 0);
    this.rock(spine, 4.2, 0, 4, 0, 1.5, 1.2, 1.1);
    this.rock(spine, 2.4, -3.8, 6.4, -0.6, 1.2, 1.1, 1.2); this.rock(spine, 2.4, 3.8, 6.4, -0.6, 1.2, 1.1, 1.2);
    for (let i = 0; i < 4; i++) this.rock(spine, 1.2, rand(-2, 2), 7 + rand(0, 1), -2.5 - rand(0, 1), 1, 1.6, 1, 0);
    const head = this.joint(spine, 'head', 0, 8, 1.2);
    this.rock(head, 1.9, 0, 0.8, 0.4, 1.1, 0.9, 1.1);
    // yeux de magma
    for (const s of [-1, 1]) { const e = new THREE.Mesh(new THREE.SphereGeometry(0.35, 8, 6), new THREE.MeshBasicMaterial({ color: 0xffd080 })); e.position.set(s * 0.7, 1.0, 2.2); head.add(e); }
    this.B.mouth = new THREE.Object3D(); this.B.mouth.position.set(0, 0.3, 2.4); head.add(this.B.mouth);
    for (const s of [-1, 1]) {
      const k = s < 0 ? 'L' : 'R';
      const sh = this.joint(spine, 'arm' + k, s * 5.5, 6.2, 0);
      this.rock(sh, 1.6, 0, -2.2, 0, 1, 1.6, 1);
      const fore = this.joint(sh, 'fore' + k, 0, -5, 0);
      this.rock(fore, 1.5, 0, -2.2, 0, 1, 1.5, 1);
      const fist = this.joint(fore, 'fist' + k, 0, -5, 0);
      this.rock(fist, 2.2, 0, -0.8, 0, 1.1, 1, 1.1);
      const leg = this.joint(hips, 'leg' + k, s * 2.6, -1, 0);
      this.rock(leg, 1.9, 0, -2.4, 0, 1, 1.5, 1);
      const shin = this.joint(leg, 'shin' + k, 0, -5, 0);
      this.rock(shin, 1.8, 0, -2.2, 0.3, 1, 1.4, 1.1);
      this.rock(shin, 1.6, 0, -4.4, 0.9, 1.1, 0.5, 1.5);
    }
    this.light = new THREE.PointLight(0xff5a1a, 0, 45, 1.8);
    this.light.position.set(0, 16, 2);
    this.root.add(this.light);
    this.root.scale.setScalar(1.0);
  }

  center() { return this.pos.clone().add(new THREE.Vector3(0, 15, 0)); }

  spawn(near = false) {
    // émerge loin du dieu, du côté des montagnes, puis marche vers le village le plus proche
    const god = G.god.pos;
    let best = null;
    for (let i = 0; i < 30; i++) {
      const a = Math.random() * 6.28, d = near ? rand(110, 170) : rand(220, 360);
      const x = clamp(god.x + Math.cos(a) * d, -WORLD.half + 80, WORLD.half - 80), z = clamp(god.z + Math.sin(a) * d, -WORLD.half + 80, WORLD.half - 80);
      if (G.terrain.heightAt(x, z) > WORLD.water + 2) { best = [x, z]; break; }
    }
    if (!best) return;
    this.pos.set(best[0], G.terrain.heightAt(best[0], best[1]) - 26, best[1]);
    this.hp = HP; this.alive = true; this.state = 'emerge'; this.t = 0; this.engaged = false;
    this.root.visible = true;
    const vs = G.villages ? G.villages.villages.filter((v) => v.buildings.some((b) => b.state === 'intact')) : [];
    this.target = vs.length ? vs.reduce((a, b) => (Math.hypot(a.x - this.pos.x, a.z - this.pos.z) < Math.hypot(b.x - this.pos.x, b.z - this.pos.z) ? a : b)) : null;
    emit('journal', { icon: '🌋', text: this.target ? `Un Titan de magma surgit des profondeurs et marche sur ${this.target.name} !` : 'Un Titan de magma surgit des profondeurs !' });
    emit('monster', { state: 'spawn', name: this.name });
    if (G.audio) G.audio.roar(this.pos, 1);
    G.terrain.crater(this.pos.x, this.pos.z, 18, 4, { rim: 0.6, burn: 0.4, heat: 0.8 });
  }

  damage(amount, from) {
    if (!this.alive || this.state === 'emerge' && this.t < 1) return;
    this.hp -= amount;
    this.engaged = true;
    this.hitT = 0.25;
    if (G.fx) G.fx.sparks(this.center(), 20, [1, 0.5, 0.15], 16);
    if (amount > 600 && this.state !== 'dying' && this.state !== 'launched') { this.state = 'stagger'; this.t = 0; if (G.audio) G.audio.roar(this.pos, 0.6); }
    emit('monster', { state: 'hit', hp: this.hp / HP, name: this.name });
    if (this.hp <= 0) this.die(from);
  }

  // projeté par un coup divin : le colosse décolle, tournoie et s'écrase
  launch(dir, power) {
    if (!this.alive || this.state === 'emerge') return;
    this.state = 'launched'; this.t = 0;
    this.kv.copy(dir).setY(0).normalize().multiplyScalar(power).setY(power * 0.55 + 10);
    if (G.audio) G.audio.roar(this.pos, 0.5);
  }

  die(from) {
    this.state = 'dying'; this.t = 0; this.alive = false;
    emit('monster', { state: 'dead', name: this.name });
    emit('journal', { icon: '⚔', text: 'Le Titan de magma s\'effondre. Les mortels n\'en reviennent pas.' });
    // protéger un village = vénération
    if (G.villages) for (const v of G.villages.villages) if (Math.hypot(v.x - this.pos.x, v.z - this.pos.z) < v.r + 260) { v.worship = clamp(v.worship + 0.18, -1, 1); v.fear = Math.max(0, v.fear - 0.1); }
    if (G.people) G.people.notify('bless', this.pos.x, this.pos.z, 200, 0);
    this.respawnT = rand(240, 420);
  }

  update(dt) {
    const god = G.god, T = G.terrain, B = this.B;
    if (!this.alive && this.state !== 'dying') {
      this.respawnT -= dt;
      if (this.respawnT <= 0 && G.started) this.spawn();
      return;
    }
    this.t += dt;
    const ground = T.heightAt(this.pos.x, this.pos.z);
    const toGod = new THREE.Vector3(god.pos.x - this.pos.x, 0, god.pos.z - this.pos.z);
    const dGod = toGod.length();
    let walking = false, wantFace = null;
    this.light.intensity = 600 + Math.sin(G.time * 3) * 150;
    switch (this.state) {
      case 'emerge':
        this.pos.y = lerp(ground - 26, ground, Math.min(1, this.t / 4));
        if (G.fx && Math.random() < dt * 20) { G.fx.dust(this.pos.clone().setY(ground), 14, 3); G.fx.rocks(this.pos.clone().setY(ground), 1, 12); }
        if (G.fx) G.fx.shake(0.05, this.pos);
        if (this.t > 4) { this.state = 'walk'; this.t = 0; if (G.audio) G.audio.roar(this.pos, 1); }
        break;
      case 'walk': {
        if (dGod < 140 || this.engaged) { this.state = 'fight'; this.t = 0; break; }
        const tgt = this.target ? new THREE.Vector3(this.target.x, 0, this.target.z) : god.pos;
        const dir = tgt.clone().sub(this.pos).setY(0);
        if (dir.length() < 25) { this.attackNear(); break; }
        wantFace = dir; walking = true;
        break;
      }
      case 'fight': {
        wantFace = toGod;
        if (this.t > 2.6) {
          this.t = 0;
          if (dGod < 30) this.startSmash();
          else if (dGod < 170) this.state = Math.random() < 0.55 ? 'throw' : 'breath';
          else walking = true;
        } else if (dGod > 32) walking = true;
        break;
      }
      case 'smash': {
        const k = this.t / 1.6;
        if (k > 0.55 && !this.smashed) {
          this.smashed = true;
          const p = this.pos.clone().addScaledVector(new THREE.Vector3(Math.sin(this.heading), 0, Math.cos(this.heading)), 14); p.y = T.heightAt(p.x, p.z);
          T.crater(p.x, p.z, 11, 4, { rim: 0.5, burn: 0.3, heat: 0.4 });
          if (G.fx) { G.fx.ring(p, 90, 1.2, 0xff9a50); G.fx.dust(p, 14, 26); G.fx.rocks(p, 24, 20); G.fx.shake(0.8, p); G.fx.decal(p, 26); }
          if (G.audio) G.audio.impact(p, 1);
          if (G.powers) G.powers.areaDamage(p, 26, 450, 'blast', 'monster');
          if (god.pos.distanceTo(p) < 34 && god.state !== 'teleport') god.knock(god.pos.clone().sub(p), 38, 1.4);
        }
        if (this.t > 1.9) { this.state = 'fight'; this.t = 0; this.smashed = false; }
        break;
      }
      case 'throw': {
        wantFace = toGod;
        if (this.t > 0.9 && !this.thrown) {
          this.thrown = true;
          const hand = new THREE.Vector3(); B.fistR.getWorldPosition(hand);
          const m = new THREE.Mesh(new THREE.DodecahedronGeometry(2.6, 1), this.rockMat); m.castShadow = true; m.position.copy(hand); this.scene.add(m);
          const to = god.pos.clone().add(new THREE.Vector3(0, 3, 0)).addScaledVector(god.vel, 1.2);
          const T0 = Math.max(1, hand.distanceTo(to) / 60);
          const vel = to.clone().sub(hand).divideScalar(T0); vel.y += 0.5 * 30 * T0;
          this.boulders.push({ m, vel, t: 0 });
          if (G.audio) G.audio.whoosh(hand);
        }
        if (this.t > 1.6) { this.state = 'fight'; this.t = 0; this.thrown = false; }
        break;
      }
      case 'breath': {
        wantFace = toGod;
        if (this.t > 0.6 && this.t < 3) {
          const m = new THREE.Vector3(); B.mouth.getWorldPosition(m);
          const dir = god.pos.clone().add(new THREE.Vector3(0, 2, 0)).sub(m).normalize();
          for (let i = 0; i < 4 && G.fx; i++) { const sp = rand(25, 45); G.fx.add.emit(m.x, m.y, m.z, dir.x * sp + rand(-3, 3), dir.y * sp + rand(-3, 3), dir.z * sp + rand(-3, 3), rand(3, 6), rand(0.6, 1.1), 1, rand(0.3, 0.55), 0.08, 0.9, 1.8, 4, 0.4); }
          const hitP = m.clone().addScaledVector(dir, Math.min(dGod, 60));
          if (G.terrain && Math.random() < dt * 6) { const gp = hitP.clone(); gp.y = T.heightAt(gp.x, gp.z); T.scorch(gp.x, gp.z, 4, 0.6, 0.6, 0.6); if (G.veg) G.veg.damageArea(gp.x, gp.z, 8, 1, 'burn'); }
          if (god.pos.distanceTo(m) < 70 && Math.random() < dt * 1.5 && god.state !== 'knock') god.knock(dir, 18, 0.6);
        }
        if (this.t > 3.4) { this.state = 'fight'; this.t = 0; }
        break;
      }
      case 'launched': {
        this.kv.y -= 32 * dt;
        this.pos.addScaledVector(this.kv, dt);
        this.root.rotation.x = Math.max(-1.3, this.root.rotation.x - dt * 2.2);
        if (G.fx && Math.random() < dt * 20) G.fx.dust(this.center(), 4, 2);
        if (this.pos.y <= ground && this.t > 0.15) {
          this.pos.y = ground; this.root.rotation.x = 0;
          const k = clamp(Math.hypot(this.kv.x, this.kv.z) / 60, 0.4, 1.4);
          T.crater(this.pos.x, this.pos.z, 16 * k, 4 * k, { rim: 0.6, burn: 0.2 });
          if (G.fx) { G.fx.ring(this.pos, 110 * k, 1.2, 0xd8b890); G.fx.dust(this.pos, 22, 40); G.fx.rocks(this.pos, 30, 20); G.fx.shake(0.9, this.pos); G.fx.decal(this.pos, 30); }
          if (G.audio) G.audio.impact(this.pos, 1);
          if (G.powers) G.powers.areaDamage(this.pos, 30 * k, 500, 'blast', 'monster');
          this.state = 'stagger'; this.t = 0;
          this.damage(300 * k, this.pos);
        }
        break;
      }
      case 'stagger': if (this.t > 1.4) { this.state = 'fight'; this.t = 0; } break;
      case 'dying': {
        const k = Math.min(1, this.t / 3);
        this.root.rotation.x = -k * 1.4;
        this.pos.y = lerp(ground, ground - 6, k);
        this.lavaMat.emissiveIntensity = 3 * (1 - k);
        this.light.intensity = 600 * (1 - k);
        if (G.fx && Math.random() < dt * 12) G.fx.dust(this.pos.clone().setY(ground), 16, 4);
        if (this.t > 3 && !this.collapsed) {
          this.collapsed = true;
          if (G.fx) { G.fx.rocks(this.pos.clone().setY(ground + 4), 70, 18); G.fx.shake(0.9, this.pos); G.fx.dust(this.pos.clone().setY(ground), 30, 50); }
          T.crater(this.pos.x, this.pos.z, 22, -3, { rim: 0, burn: 0.3 }); // éboulis : un tertre de roche reste
          if (G.audio) G.audio.collapse(this.pos, 1);
        }
        if (this.t > 5) { this.root.visible = false; this.state = 'dormant'; this.collapsed = false; this.root.rotation.x = 0; this.lavaMat.emissiveIntensity = 3; }
        break;
      }
    }
    // déplacement
    if (wantFace) this.heading = dampAngle(this.heading, Math.atan2(wantFace.x, wantFace.z), 1.5, dt);
    if (walking) {
      const sp = 5.5;
      this.pos.x += Math.sin(this.heading) * sp * dt; this.pos.z += Math.cos(this.heading) * sp * dt;
      this.phase += dt * 1.4;
      const step = Math.floor(this.phase / Math.PI);
      if (step !== this.lastStep) {
        this.lastStep = step;
        const fp = this.pos.clone(); fp.y = T.heightAt(fp.x, fp.z);
        T.crater(fp.x, fp.z, 4, 0.8, { rim: 0.4 });
        if (G.fx) { G.fx.dust(fp, 6, 8); G.fx.shake(0.18, fp); G.fx.decal(fp, 8); }
        if (G.audio) G.audio.footstep(fp, true, 2);
        if (G.powers) G.powers.areaDamage(fp, 9, 160, 'push', 'monsterStep');
      }
    }
    if (this.state !== 'emerge' && this.state !== 'dying' && this.state !== 'launched') this.pos.y = damp(this.pos.y, T.heightAt(this.pos.x, this.pos.z), 6, dt);
    this.animate(dt, walking);
    // rochers lancés
    for (let i = this.boulders.length - 1; i >= 0; i--) {
      const b = this.boulders[i]; b.t += dt;
      b.vel.y -= 30 * dt; b.m.position.addScaledVector(b.vel, dt); b.m.rotation.x += dt * 4;
      const g = T.heightAt(b.m.position.x, b.m.position.z);
      const hitGod = b.m.position.distanceTo(god.pos.clone().add(new THREE.Vector3(0, 2.6, 0))) < 5;
      if (hitGod || b.m.position.y < g || b.t > 6) {
        const p = b.m.position.clone();
        if (hitGod) god.knock(b.vel.clone(), 34, 1.0);
        if (G.fx) { G.fx.rocks(p, 14, 14); G.fx.dust(p, 6, 12); G.fx.shake(0.4, p); }
        if (b.m.position.y < g + 1) T.crater(p.x, p.z, 6, 2, { rim: 0.4 });
        if (G.powers) G.powers.areaDamage(p, 10, 300, 'blast', 'monster');
        this.scene.remove(b.m); this.boulders.splice(i, 1);
      }
    }
  }

  attackNear() {
    // détruit le village visé tant que le dieu n'intervient pas
    const b = G.villages && G.villages.nearestBuilding(this.pos.x, this.pos.z, (x) => x.state === 'intact');
    if (b && Math.hypot(b.x - this.pos.x, b.z - this.pos.z) < 40) { this.heading = Math.atan2(b.x - this.pos.x, b.z - this.pos.z); this.startSmash(); }
    else if (b) this.target = { x: b.x, z: b.z, name: this.target ? this.target.name : '' };
  }
  startSmash() { this.state = 'smash'; this.t = 0; this.smashed = false; if (G.audio) G.audio.roar(this.pos, 0.5); }

  animate(dt, walking) {
    const B = this.B, t = this.t;
    this.root.position.copy(this.pos);
    this.root.rotation.y = this.heading;
    if (this.state !== 'launched' && this.state !== 'dying') this.root.rotation.x = damp(this.root.rotation.x, 0, 4, dt);
    const s = Math.sin(this.phase), w = walking ? 1 : 0;
    B.legL.rotation.x = s * 0.45 * w; B.legR.rotation.x = -s * 0.45 * w;
    B.shinL.rotation.x = Math.max(0, -s) * 0.6 * w; B.shinR.rotation.x = Math.max(0, s) * 0.6 * w;
    B.hips.position.y = 11 - Math.abs(Math.cos(this.phase)) * 0.6 * w;
    let aL = -s * 0.35 * w, aR = s * 0.35 * w, sp = 0.15 + Math.sin(G.time * 0.8) * 0.03;
    if (this.state === 'smash') { const k = t / 1.6; const up = k < 0.5 ? k / 0.5 : Math.max(-0.3, 1 - (k - 0.5) / 0.12); aL = aR = -2.8 * Math.max(0, up) + 0.4; sp = 0.15 + (k > 0.5 ? 0.5 : -0.2); }
    if (this.state === 'throw') { const k = Math.min(1, t / 0.9); aR = k < 1 ? -2.6 * k : -2.6 + (t - 0.9) * 6; aR = Math.min(aR, 0.6); }
    if (this.state === 'breath') sp = -0.25;
    if (this.state === 'stagger') { sp = -0.4 + Math.sin(t * 8) * 0.1; }
    B.armL.rotation.x = damp(B.armL.rotation.x, aL, 8, dt); B.armR.rotation.x = damp(B.armR.rotation.x, aR, 8, dt);
    B.armL.rotation.z = -0.25; B.armR.rotation.z = 0.25;
    B.foreL.rotation.x = -0.4; B.foreR.rotation.x = -0.4;
    B.spine.rotation.x = damp(B.spine.rotation.x, sp, 4, dt);
    B.head.rotation.x = this.state === 'breath' ? -0.3 : 0;
    if (this.hitT > 0) { this.hitT -= dt; this.lavaMat.emissiveIntensity = 6; } else if (this.state !== 'dying') this.lavaMat.emissiveIntensity = 3 + Math.sin(G.time * 2) * 0.6;
  }
}
