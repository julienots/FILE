import * as THREE from 'three';
import { toonMaterial, addOutline } from '../render/shaders/Toon.js';
import { Assets } from '../assets/AssetLibrary.js';
import { bus } from '../core/EventBus.js';

// Ennemis au sol (poolés) + projectiles du héros. Fournit le rendu, l'animation et une IA de secours (errance/poursuite/contact).
// B (systems/combat) peut reprendre la main: `enemy:order {uid, order:'die'|'stun'}` et écouter `enemy:hit`, `enemy:touch`, `enemy:killed`.
const TYPES = {
  slime: { color: '#3a1460', rim: '#d070ff', hp: 3, speed: 3.2, size: 1, sight: 20, model: 'enemy_slime' },
  scarab: { color: '#0e3a44', rim: '#3affe0', hp: 5, speed: 4.5, size: 1.3, sight: 26, model: 'enemy_scarab' },
  brute: { color: '#2a2228', rim: '#ff7a3a', hp: 14, speed: 2.2, size: 3, sight: 30, model: 'enemy_brute' },
};
const _a = new THREE.Vector3(), _b = new THREE.Vector3(), _c = new THREE.Vector3(), _m = new THREE.Matrix4(), _q = new THREE.Quaternion();
function makeBody(type) {
  const T = TYPES[type], g = new THREE.Group(), model = Assets.get(T.model);
  if (model) { model.scale.multiplyScalar(T.size); g.add(model); return g; }
  const mat = toonMaterial({ color: T.color, rim: T.rim, rimStrength: 1.3 });
  const b = new THREE.Mesh(new THREE.SphereGeometry(.6, 16, 12), mat); b.position.y = .5; b.scale.set(1.1, .85, 1.1); addOutline(b, '#1a0f33', .04); g.add(b);
  const white = new THREE.MeshBasicMaterial({ color: '#ff2a4a' }), red = new THREE.MeshBasicMaterial({ color: '#ff2a4a' });
  for (const s of [-1, 1]) { const e = new THREE.Mesh(new THREE.SphereGeometry(.15, 10, 8), white); e.position.set(.24 * s, .62, .46); const p = new THREE.Mesh(new THREE.SphereGeometry(.08, 8, 6), red); p.position.z = .1; e.add(p); g.add(e);
    const brow = new THREE.Mesh(new THREE.BoxGeometry(.3, .07, .08), new THREE.MeshBasicMaterial({ color: '#1a0f33' })); brow.position.set(.24 * s, .8, .52); brow.rotation.z = -.5 * s; g.add(brow);
    if (type !== 'slime') { const h = new THREE.Mesh(new THREE.ConeGeometry(.12, .5, 6), red); h.position.set(.35 * s, 1.1, 0); h.rotation.z = -.5 * s; g.add(h); } }
  g.scale.setScalar(T.size); g.userData.mat = mat; return g;
}
export class Enemies {
  constructor(scene, particles) {
    this.scene = scene; this.particles = particles; this.list = []; this.uid = 0;
    this.bolts = []; const bg = new THREE.IcosahedronGeometry(.35, 1);
    for (let i = 0; i < 14; i++) { const m = new THREE.Mesh(bg, new THREE.MeshBasicMaterial({ color: '#fff' })); m.visible = false; scene.add(m); this.bolts.push({ mesh: m, vel: new THREE.Vector3(), life: 0, dmg: 1, size: 1 }); }
    bus.on('enemy:order', ({ uid, order }) => { const e = this.list.find(x => x.uid === uid); if (e && order === 'die') this._kill(e); });
  }
  clear() { for (const e of this.list) this.scene.remove(e.root); this.list.length = 0; for (const b of this.bolts) { b.life = 0; b.mesh.visible = false; } }
  // Peuple la planète: def.enemies [{type,count,lat,lon,radius}] sinon quelques slimes répartis.
  populate(planet) {
    this.clear(); this.P = planet.userData; const def = this.P.def; let s = def.seed * 57 + 3; const rnd = () => (s = (s * 16807) % 2147483647) / 2147483647;
    const specs = def.enemies?.length ? def.enemies : [{ type: 'slime', count: 10 }, { type: 'scarab', count: 3 }, { type: 'brute', count: 1 }];
    for (const sp of specs) for (let i = 0, tries = 0; i < (sp.count ?? 1) && tries < 200; tries++) {
      const d = new THREE.Vector3();
      if (sp.lat != null) { const la = THREE.MathUtils.degToRad(sp.lat), lo = THREE.MathUtils.degToRad(sp.lon), r = (sp.radius ?? 10) / this.P.radius; d.set(Math.cos(la) * Math.cos(lo), Math.sin(la), Math.cos(la) * Math.sin(lo)).add(_a.set(rnd() - .5, rnd() - .5, rnd() - .5).multiplyScalar(r * 2)).normalize(); }
      else { const u = rnd() * 2 - 1, t = rnd() * 6.283, r = Math.sqrt(1 - u * u); d.set(r * Math.cos(t), u, r * Math.sin(t)); if (d.y > .9) continue; }   // pas au point d'apparition
      if (this.P.sample(d) - this.P.sea < .03) continue;
      this.spawn(sp.type ?? 'slime', d); i++;
    }
  }
  spawn(type, dir) {
    const T = TYPES[type] ?? TYPES.slime, root = makeBody(TYPES[type] ? type : 'slime'); this.scene.add(root);
    const e = { uid: ++this.uid, type, T, root, dir: dir.clone().normalize(), home: dir.clone().normalize(), face: new THREE.Vector3(1, 0, 0), hp: T.hp, t: Math.random() * 9, hurt: 0, touchCd: 0, dead: 0, wander: new THREE.Vector3(), kb: new THREE.Vector3() };
    this.list.push(e); return e;
  }
  nearest(pos, maxDist) { let best = null, bd = maxDist; for (const e of this.list) { if (e.dead) continue; const d = e.root.position.distanceTo(pos); if (d < bd) { bd = d; best = e; } } return best; }
  // Tire un projectile depuis `from` dans la direction tangentielle `dir`.
  fire(from, dir, heroScale, color) {
    const b = this.bolts.find(x => x.life <= 0); if (!b) return;
    b.life = 1.3; b.size = Math.sqrt(heroScale); b.dmg = Math.round(1 + heroScale); b.vel.copy(dir).multiplyScalar(30 * Math.sqrt(heroScale)); b.mesh.position.copy(from); b.mesh.scale.setScalar(b.size); b.mesh.material.color.set(color); b.mesh.visible = true; b.r = from.length();
  }
  hit(e, dmg, fromPos) {
    e.hp -= dmg; e.hurt = .25; e.kb.copy(e.root.position).sub(fromPos).normalize().multiplyScalar(9);
    for (let k = 0; k < 8; k++) this.particles.emit(e.root.position, _a.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(10), e.T.rim, .4 * e.T.size, .4, 3, 0);
    bus.emit('enemy:hit', { uid: e.uid, type: e.type, dmg, hp: e.hp, pos: e.root.position });
    if (e.hp <= 0) this._kill(e);
  }
  _kill(e) {
    if (e.dead) return; e.dead = .001;
    for (let k = 0; k < 26; k++) this.particles.emit(e.root.position, _a.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(16 * e.T.size), k % 2 ? e.T.color : '#ffffff', .5 * e.T.size, .7, 2.5, 2);
    bus.emit('enemy:killed', { uid: e.uid, type: e.type, pos: e.root.position.clone(), xp: e.T.hp * 4 });
  }
  update(dt, heroPos, heroScale, heroAlive = true) {
    const P = this.P, R = P.radius;
    for (const e of this.list) {
      if (e.dead) { e.dead += dt; const k = Math.max(0, 1 - e.dead * 4); e.root.scale.setScalar(e.T.size * k); e.root.visible = k > 0;
        if (e.dead > 25) { e.dead = 0; e.hp = e.T.hp; e.dir.copy(e.home); e.root.visible = true; e.root.scale.setScalar(e.T.size); } continue; }   // réapparition
      e.t += dt; e.touchCd -= dt; e.hurt = Math.max(0, e.hurt - dt);
      const dist = e.root.position.distanceTo(heroPos), chase = heroAlive && dist < e.T.sight * Math.max(1, heroScale * .5) && heroScale < 4;   // les formes géantes font fuir
      let speed = 0;
      if (chase) { _a.copy(heroPos).normalize(); speed = e.T.speed; }
      else { if (e.t % 4 < dt) e.wander.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(.3).add(e.home).normalize(); _a.copy(e.wander.lengthSq() ? e.wander : e.home); speed = e.T.speed * .4; }
      const ang = e.dir.angleTo(_a);
      if (ang > .004 && dist > (e.T.size * .6 + heroScale * .5)) { _b.crossVectors(e.dir, _a).normalize(); _c.copy(e.dir); e.dir.applyAxisAngle(_b, Math.min(ang, speed * dt / R)); e.face.copy(e.dir).sub(_c).normalize(); }
      if (e.kb.lengthSq() > .01) { _c.copy(e.dir).multiplyScalar(R).addScaledVector(e.kb, dt); e.dir.copy(_c).normalize(); e.kb.multiplyScalar(Math.max(0, 1 - dt * 6)); }
      if (P.sample(e.dir) - P.sea < .01) e.dir.lerp(e.home, .02).normalize();      // évite l'eau
      const hop = Math.abs(Math.sin(e.t * (chase ? 7 : 3.5))), gr = P.heightAt(e.dir);
      e.root.position.copy(e.dir).multiplyScalar(gr + hop * .5 * e.T.size);
      const z = _a.copy(e.face).addScaledVector(e.dir, -e.face.dot(e.dir)); if (z.lengthSq() < 1e-4) z.set(1, 0, 0).addScaledVector(e.dir, -e.dir.x); z.normalize(); const x = _b.crossVectors(e.dir, z).normalize();
      e.root.quaternion.slerp(_q.setFromRotationMatrix(_m.makeBasis(x, e.dir, z)), Math.min(1, dt * 8));
      const sq = 1 + (hop - .5) * .25, hs = e.hurt > 0 ? 1.25 : 1; e.root.scale.set(e.T.size * (2 - sq) * hs, e.T.size * sq * hs, e.T.size * (2 - sq) * hs);
      if (e.root.userData.mat) e.root.userData.mat.emissive.setScalar(e.hurt > 0 ? .8 : 0);
      if (heroAlive && dist < e.T.size * .7 + heroScale * .6 && e.touchCd <= 0) { e.touchCd = 1; bus.emit('enemy:touch', { uid: e.uid, type: e.type, dmg: Math.round(e.T.hp * 1.5), pos: e.root.position }); }
    }
    for (const b of this.bolts) { if (b.life <= 0) continue; b.life -= dt; const p = b.mesh.position;
      p.addScaledVector(b.vel, dt); _a.copy(p).normalize(); const r = Math.max(P.heightAt(_a) + .6 * b.size, b.r); p.copy(_a).multiplyScalar(r); b.vel.addScaledVector(_a, -b.vel.dot(_a));
      this.particles.emit(p, _b.set(0, 0, 0), b.mesh.material.color, .5 * b.size, .25, 0, 0);
      for (const e of this.list) if (!e.dead && e.root.position.distanceTo(p) < e.T.size * .8 + b.size * .6) { this.hit(e, b.dmg, p); b.life = 0; break; }
      if (b.life <= 0) { b.mesh.visible = false; for (let k = 0; k < 6; k++) this.particles.emit(p, _b.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(8), b.mesh.material.color, .35 * b.size, .3, 3, 0); } }
  }
}
