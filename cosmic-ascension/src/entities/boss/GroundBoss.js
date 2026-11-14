import * as THREE from 'three';
import { toonMaterial } from '../../render/shaders/Toon.js';
import { Assets } from '../../assets/AssetLibrary.js';
import { bus } from '../../core/EventBus.js';
import { State } from '../../core/State.js';
import { BOSSES } from '../../data/bosses.js';
import { QUESTS } from '../../data/quests.js';

// Boss au sol générique (Empereur, Titan Kaal, Dieu Céleste, Gardien…): marche vers le héros, frappe le sol (onde de choc
// à esquiver en sautant), contact. PV côté moteur; à la mort: `boss:defeated {id}` → récompenses et quête gérées par systems/Bosses.
const LOOK = {   // modèle GLB, hauteur (m), couleur d'énergie
  lab_warden: ['enemy_drone', 6, '#ff3a4a'], emperor: ['enemy_brute', 8, '#ff2a4a'], ancient_titan: ['hero_titan', 17, '#b48aff'],
  celestial_god: ['hero_god', 24, '#ffe9a0'], void_whale: ['enemy_dragon', 14, '#6ff2ff'],
};
// Boss attendu par une quête active sur cette planète (arène du boss ou planète de la quête), pas encore vaincu.
export function pendingGroundBoss(planetId) {
  const q = State.data.quests?.active ?? {}, fl = State.data.world.flags;
  for (const id in q) { const Q = QUESTS[id], st = Q?.steps[q[id].step]; if (st?.type !== 'boss') continue; const B = BOSSES[st.target];
    if (B && B.arena !== 'space' && LOOK[st.target] && !fl['boss_' + st.target] && (B.arena === planetId || Q.planet === planetId)) return st.target; }
  return null;
}
const _a = new THREE.Vector3(), _b = new THREE.Vector3(), _c = new THREE.Vector3(), _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _z = new THREE.Vector3(0, 0, 1);
export class GroundBoss {
  constructor(id, scene, planet, particles, dir) {
    const B = this.B = BOSSES[id], [model, height, glow] = LOOK[id]; this.id = id; this.scene = scene; this.P = planet.userData; this.particles = particles; this.glow = glow;
    this.h = height; this.r = height * .32; this.maxHp = Math.round(B.hp / 40); this.hp = this.maxHp; this.dir = dir.clone().normalize(); this.face = new THREE.Vector3(1, 0, 0);
    this.root = new THREE.Group(); const m = Assets.get(model);
    if (m) { m.scale.multiplyScalar(height / (Assets.metaHeight?.(model) ?? 1.2)); this.root.add(m); this.animV = m.userData.anim; }
    else { const body = new THREE.Mesh(new THREE.CapsuleGeometry(height * .22, height * .5, 6, 14), toonMaterial({ color: '#16121e', rim: glow, rimStrength: 2 })); body.position.y = height * .5; this.root.add(body);
      for (const s of [-1, 1]) { const e = new THREE.Mesh(new THREE.SphereGeometry(height * .04, 8, 8), new THREE.MeshBasicMaterial({ color: glow })); e.position.set(s * height * .09, height * .78, height * .2); this.root.add(e); } }
    this.light = new THREE.PointLight(glow, 40, height * 4); this.light.position.y = height * .6; this.root.add(this.light); scene.add(this.root);
    const ringMat = () => new THREE.MeshBasicMaterial({ color: glow, transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide });
    this.ring = new THREE.Mesh(new THREE.RingGeometry(.9, 1, 64), ringMat()); this.warn = new THREE.Mesh(new THREE.RingGeometry(.2, 1, 48), ringMat()); this.ring.visible = this.warn.visible = false; scene.add(this.ring, this.warn);
    this.t = 0; this.atk = 3; this.state = 'walk'; this.ringR = 0; this.touchCd = 0; this.phase = 0; this.dead = 0; this.flash = 0;
    bus.emit('boss:spawn', { id, name: B.name, title: B.title, hp: this.hp, maxHp: this.maxHp });
  }
  damage(n, from) {
    if (this.dead) return; this.hp = Math.max(0, this.hp - n); this.flash = .12;
    for (let k = 0; k < 6; k++) this.particles.emit(from, _a.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(14), this.glow, .5, .4, 3, 0);
    bus.emit('boss:damaged', { id: this.id, dmg: n, hp: this.hp, maxHp: this.maxHp });
    if (!this.phase && this.hp < this.maxHp * .5) { this.phase = 1; bus.emit('boss:phase', { id: this.id, phase: 1 }); }
    if (this.hp <= 0) { this.dead = .001; this.ring.visible = this.warn.visible = false; bus.emit('cinematic:start', { id: 'boss-death', boss: this.id }); }
  }
  // Renvoie true si `pos` touche le boss (projectiles).
  hits(pos, r = .5) { return !this.dead && _a.copy(pos).sub(this.root.position).addScaledVector(this.dir, -this.h * .45).length() < this.h * .5 + r; }
  dispose(silent) { this.scene.remove(this.root, this.ring, this.warn); if (!silent && !this.done) bus.emit('boss:despawn', { id: this.id }); }
  // heroAlt: hauteur du héros au-dessus du sol. Renvoie false quand le boss a fini de mourir.
  update(dt, heroPos, heroAlt, heroScale) {
    const P = this.P, R = P.radius, up = this.dir; this.t += dt; this.touchCd -= dt; this.flash = Math.max(0, this.flash - dt);
    if (this.dead) { this.dead += dt; const k = Math.max(0, 1 - this.dead / 3.2); this.root.scale.setScalar(.2 + .8 * k); this.root.rotation.y += dt * 6; this.light.intensity = 200 * k;
      for (let i = 0; i < 5; i++) this.particles.emit(_a.copy(this.root.position).addScaledVector(up, Math.random() * this.h * k), _b.set(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(30), i % 2 ? this.glow : '#ffffff', 1.2, .9, 2, 0);
      if (this.dead > 3.2) { this.done = true; this.scene.remove(this.root); bus.emit('cinematic:end', { id: 'boss-death' }); bus.emit('boss:defeated', { id: this.id, name: this.B.name }); return false; }
      return true; }
    const dist = this.root.position.distanceTo(heroPos), speed = (2.6 + this.phase * 1.6) * (this.state === 'walk' ? 1 : 0);
    _a.copy(heroPos).normalize(); const ang = up.angleTo(_a);
    if (ang > .002) { _b.crossVectors(up, _a).normalize(); _c.copy(up); if (dist > this.r + heroScale && speed) up.applyAxisAngle(_b, Math.min(ang, speed * dt / R)); this.face.copy(_a).sub(_c).normalize(); }
    const gr = Math.max(P.heightAt(up), P.seaRadius), hop = this.state === 'windup' ? Math.sin(Math.min(1, this.st / .9) * Math.PI) * this.h * .35 : Math.abs(Math.sin(this.t * 2.2)) * this.h * .03;
    this.root.position.copy(up).multiplyScalar(gr + hop);
    const z = _a.copy(this.face).addScaledVector(up, -this.face.dot(up)); if (z.lengthSq() < 1e-5) z.set(1, 0, 0).addScaledVector(up, -up.x); z.normalize(); const x = _b.crossVectors(up, z).normalize();
    this.root.quaternion.slerp(_q.setFromRotationMatrix(_m.makeBasis(x, up, z)), Math.min(1, dt * 4)); this.light.intensity = this.flash ? 160 : 40 + Math.sin(this.t * 3) * 12;
    if (this.animV) this.animV.set(this.state === 'walk' ? .8 : 0, this.t * 4, this.state === 'windup' ? 1 : 0, this.state === 'windup' ? Math.min(1, this.st * 2) : 0);
    // frappe au sol: télégraphe → onde de choc qui s'étend; on l'esquive en sautant par-dessus
    this.atk -= dt;
    if (this.state === 'walk' && this.atk <= 0 && dist < 60) { this.state = 'windup'; this.st = 0; }
    if (this.state === 'windup') { this.st += dt; const w = this.warn; w.visible = true; w.position.copy(up).multiplyScalar(gr + .25); w.quaternion.setFromUnitVectors(_z, up); w.scale.setScalar(this.r * 2.2); w.material.opacity = .25 + .35 * Math.abs(Math.sin(this.st * 14));
      if (this.st >= .9) { this.state = 'wave'; this.ringR = this.r; w.visible = false; this.ring.visible = true; this.ringHit = false; bus.emit('boss:slam', { id: this.id });
        for (let k = 0; k < 40; k++) { const a = k / 40 * 6.283; this.particles.emit(this.root.position, _c.copy(x).multiplyScalar(Math.cos(a)).addScaledVector(z, Math.sin(a)).multiplyScalar(22).addScaledVector(up, 6), this.glow, 1, .7, 2, 3); } } }
    if (this.state === 'wave') { this.ringR += dt * (17 + this.phase * 6); const max = 30 + this.phase * 8, k = this.ringR / max; const rg = this.ring; rg.position.copy(up).multiplyScalar(gr + .3 - k * k * max * max / (2 * R)); rg.quaternion.setFromUnitVectors(_z, up); rg.scale.setScalar(this.ringR); rg.material.opacity = (1 - k) * .9;
      if (!this.ringHit && Math.abs(dist - this.ringR) < 1.6 + heroScale * .4 && heroAlt < 1.1 * heroScale) { this.ringHit = true; bus.emit('boss:hit-hero', { id: this.id, dmg: this.B.dmg }); }
      if (k >= 1) { this.state = 'walk'; rg.visible = false; this.atk = Math.max(1.6, 4 - this.phase * 1.4); } }
    if (dist < this.r + heroScale * .6 && this.touchCd <= 0) { this.touchCd = 1.2; bus.emit('boss:hit-hero', { id: this.id, dmg: Math.round(this.B.dmg * .6) }); bus.emit('enemy:touch', { uid: -1, type: '__boss', dmg: 0, pos: this.root.position }); }
    return true;
  }
}
