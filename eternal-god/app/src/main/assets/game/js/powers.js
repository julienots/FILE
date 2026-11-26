// Pouvoirs divins : poing (et Coup absolu chargé), explosion céleste, rayon divin, météore,
// onde de choc, éclair. Chaque pouvoir déforme le terrain, détruit, projette, et marque
// la mémoire des villages (crainte ou vénération).
import * as THREE from 'three';
import { G, clamp, lerp, rand, emit, canvasTexture } from './core.js';
import { ULTIMATES } from './ultimates.js';

export const POWERS = [
  { id: 'explosion', name: 'Explosion céleste', icon: '✺', cd: 6, desc: 'Une sphère de lumière jaillit du dieu et souffle tout alentour.' },
  { id: 'laser', name: 'Rayon divin', icon: '⟋', cd: 3, desc: 'Maintenir : un rayon qui fend la terre, les maisons et les montagnes.' },
  { id: 'meteor', name: 'Météore', icon: '☄', cd: 7, desc: 'Fait tomber un astre de feu sur le point visé.' },
  { id: 'shockwave', name: 'Onde de choc', icon: '◎', cd: 5, desc: 'Frappe le sol : une vague de destruction roule dans toutes les directions.' },
  { id: 'lightning', name: 'Éclair divin', icon: 'ϟ', cd: 2.2, desc: 'La foudre s\'abat sur la cible.' },
  ...ULTIMATES,
];

const BEAM_V = 'varying float vF; void main(){ vec3 n = normalize(normalMatrix * normal); vec4 mv = modelViewMatrix * vec4(position, 1.0); vF = abs(dot(n, normalize(-mv.xyz))); gl_Position = projectionMatrix * mv; }';
const BEAM_F = 'uniform vec3 uC; uniform float uO; varying float vF; void main(){ gl_FragColor = vec4(uC * pow(vF, 2.5) * uO * 1.6, 1.0); }';

export class Powers {
  constructor(scene) {
    this.scene = scene;
    this.selected = 0;
    this.cool = {};
    for (const p of POWERS) this.cool[p.id] = 0;
    this.cool.punch = 0;
    this.charge = 0;
    this.meteors = [];
    this.waves = [];
    // rayon divin
    this.beam = new THREE.Group();
    for (const [r0, r1, c, o] of [[0.45, 1.3, 0xfff4d0, 1], [1.0, 3.6, 0xffb040, 0.35], [0.8, 6.5, 0xff7a20, 0.12]]) {
      const m = new THREE.Mesh(new THREE.CylinderGeometry(r1, r0, 1, 16, 1, true).rotateX(Math.PI / 2).translate(0, 0, 0.5),
        new THREE.ShaderMaterial({ transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, uniforms: { uC: { value: new THREE.Color(c) }, uO: { value: o } }, vertexShader: BEAM_V, fragmentShader: BEAM_F }));
      m.frustumCulled = false; this.beam.add(m);
    }
    this.beam.visible = false; scene.add(this.beam);
    this.laserT = 0; this.laserOn = false; this.laserTick = 0;
    // astre de feu
    const glowTex = canvasTexture(128, 128, (ctx, w) => { const g = ctx.createRadialGradient(w / 2, w / 2, 0, w / 2, w / 2, w / 2); g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.25, 'rgba(255,210,140,0.7)'); g.addColorStop(1, 'rgba(255,120,30,0)'); ctx.fillStyle = g; ctx.fillRect(0, 0, w, w); });
    this.glowTex = glowTex;
    this.metRock = new THREE.Mesh(new THREE.DodecahedronGeometry(1, 1), new THREE.MeshStandardMaterial({ color: 0x241a12, emissive: 0xff5010, emissiveIntensity: 2.2, roughness: 1, flatShading: true }));
  }

  ready(id) { return this.cool[id] <= 0; }
  cdFrac(id) { const p = POWERS.find((x) => x.id === id); return p ? clamp(this.cool[id] / p.cd, 0, 1) : clamp(this.cool[id] / 0.45, 0, 1); }

  // ---------- dégâts de zone communs ----------
  areaDamage(p, r, power, type = 'blast', src = 'power') {
    const from = p.clone();
    let hits = 0;
    if (G.villages) hits += G.villages.damageArea(p.x, p.z, r, power, from);
    if (G.veg) G.veg.damageArea(p.x, p.z, r, power * 0.6, type === 'burn' ? 'burn' : (power > 300 ? 'blast' : 'push'), from);
    if (G.people) {
      G.people.notify(src === 'step' ? 'quake' : 'destroy', p.x, p.z, r, Math.min(80, power * 0.1));
      if (power > 60) G.people.hitArea(p.x, p.z, r * 0.7, Math.min(60, power * 0.06), from);
    }
    if (G.fauna && power > 60) G.fauna.hitArea(p.x, p.z, r * 0.7);
    if (G.physics) G.physics.blast(p.x, p.z, r * 1.2, Math.min(60, power * 0.05));
    if (G.monster && G.monster.alive) {
      const d = G.monster.center().distanceTo(p);
      if (d < r + G.monster.radius) G.monster.damage(power * (1 - clamp(d / (r + G.monster.radius), 0, 0.8)), from);
    }
    this.memory(p, r, power, hits);
    return hits;
  }

  // les villages se souviennent : destruction = crainte
  memory(p, r, power, hits) {
    if (!G.villages || power < 50) return;
    for (const v of G.villages.villages) {
      const d = Math.hypot(v.x - p.x, v.z - p.z);
      if (d > v.r + r + 60) continue;
      v.fear = clamp(v.fear + 0.004 * Math.min(power, 600) / 60 * (hits ? 2 : 1), 0, 1);
      if (hits && !v._warned) { v._warned = true; emit('journal', { icon: '⚠', text: `${v.name} tremble devant la colère divine.` }); setTimeout(() => { v._warned = false; }, 60000); }
    }
  }

  handPos() { const v = new THREE.Vector3(); G.god.B.handR.getWorldPosition(v); return v; }

  // ---------- poing divin ----------
  punch(charged = 0) {
    if (this.cool.punch > 0) return;
    const god = G.god;
    this.cool.punch = charged > 0.95 ? 1.5 : 0.45;
    god.playAction('punch', 0.5);
    const dir = new THREE.Vector3(Math.sin(god.heading), 0, Math.cos(god.heading));
    // petit élan vers l'avant
    if (god.state === 'ground') god.pos.addScaledVector(dir, 2.5);
    setTimeout(() => {
      const p = god.pos.clone().addScaledVector(dir, 5).setY(god.pos.y + 2.5);
      const ground = G.terrain.heightAt(p.x, p.z);
      if (charged > 0.95) {
        // Coup absolu : rien ne lui résiste
        if (G.monster && G.monster.alive && G.monster.center().distanceTo(p) < G.monster.radius + 25) G.monster.damage(1e6, p);
        G.terrain.crater(p.x, p.z, 16, 6, { rim: 0.4, burn: 0.3 });
        G.fx.sphere(p, 22, 0.6, 0xfff0c8); G.fx.ring(p.clone().setY(ground), 90, 1.2); G.fx.flash(p, 5000, 0.6, 0xfff0d0); G.fx.shake(0.9, p);
        G.fx.rocks(p.clone().setY(ground), 30, 22); G.fx.dust(p.clone().setY(ground), 16, 30);
        this.areaDamage(p, 40, 1400, 'blast', 'punch');
        if (G.audio) G.audio.explosion(p, 1);
        emit('journal', { icon: '✊', text: 'Le Coup absolu fend la terre.' });
      } else {
        this.areaDamage(p, 11, 380, 'push', 'punch');
        if (Math.abs(p.y - ground) < 5) { G.terrain.crater(p.x, p.z, 4, 1.2, { rim: 0.3 }); G.fx.decal(p.clone().setY(ground), 6); G.fx.rocks(p.clone().setY(ground), 6, 10); }
        G.fx.sphere(p, 5, 0.3, 0xffe8b0); G.fx.sparks(p, 24); G.fx.shake(0.22, p);
        if (G.audio) G.audio.punch(p);
      }
    }, 170);
  }

  // ---------- pouvoir sélectionné ----------
  cast(id, aim) {
    if (!this.ready(id)) return false;
    const P = POWERS.find((p) => p.id === id);
    const god = G.god;
    if (id === 'explosion') {
      god.playAction('raise', 1.0);
      setTimeout(() => {
        const c = god.pos.clone().add(new THREE.Vector3(0, 3, 0));
        const g = G.terrain.heightAt(c.x, c.z);
        G.fx.sphere(c, 48, 1.1, 0xfff0c0); G.fx.ring(c.clone().setY(g), 140, 1.6, 0xffd890); G.fx.flash(c, 9000, 1.0, 0xfff2d0); G.fx.shake(0.85, c);
        if (c.y - g < 30) { G.terrain.crater(c.x, c.z, 20, 5, { rim: 0.3, burn: 0.7, heat: 0.25 }); G.fx.rocks(c.clone().setY(g), 40, 25); G.fx.dust(c.clone().setY(g), 30, 40); }
        G.env && (G.env.flash = 0.6);
        this.areaDamage(c.clone().setY(Math.max(g, c.y - 10)), 52, 900, 'blast', 'explosion');
        if (G.audio) G.audio.explosion(c, 1);
      }, 550);
    } else if (id === 'laser') {
      this.laserOn = true; this.laserT = 0; god.playAction('cast', 3.6);
      if (G.audio) G.audio.laser(true);
      return true; // le temps de recharge part à la fin du rayon
    } else if (id === 'meteor') {
      god.playAction('raise', 0.9);
      this.spawnMeteor(aim.point.clone(), 1);
      emit('journal', { icon: '☄', text: 'Un astre de feu fend le ciel.' });
    } else if (id === 'shockwave') {
      god.playAction('slam', 0.9);
      setTimeout(() => {
        const p = god.pos.clone(); p.y = G.terrain.groundAt(p.x, p.z);
        G.terrain.crater(p.x, p.z, 7, 2.5, { rim: 0.5 });
        G.fx.ring(p, 190, 1.8, 0xe8d0a0); G.fx.ring(p, 120, 1.3, 0xffffff); G.fx.decal(p, 18); G.fx.shake(0.7, p); G.fx.dust(p, 10, 30);
        this.waves.push({ p, t: 0, r: 0, done: 0 });
        if (G.audio) G.audio.impact(p, 1);
      }, 480);
    } else if (id === 'lightning') {
      god.playAction('cast', 0.6);
      this.strike(aim.point, true);
    } else if (G.ultimates && G.ultimates.cast(id, aim)) {
      // pouvoir ultime (voir ultimates.js)
    }
    this.cool[id] = P.cd;
    return true;
  }

  spawnMeteor(target, s = 1) {
    const start = target.clone().add(new THREE.Vector3(rand(-160, 160) * s, 380 + rand(0, 120), rand(-160, 160) * s));
    const m = this.metRock.clone(); m.scale.setScalar(5 * s); m.position.copy(start); this.scene.add(m);
    const glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: this.glowTex, color: 0xffa040, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
    glow.scale.setScalar(9); m.add(glow);
    this.meteors.push({ m, start, target, t: 0, dur: 2.2 + rand(0, 0.6), s });
    if (G.audio && s > 0.7) G.audio.meteorWhistle();
  }

  strike(p, divine) {
    const top = p.clone().add(new THREE.Vector3(rand(-20, 20), 260, rand(-20, 20)));
    G.fx.bolt(top, p, divine ? 0.9 : 0.6); G.fx.bolt(top, p.clone().add(new THREE.Vector3(rand(-6, 6), 0, rand(-6, 6))), 0.35);
    G.fx.flash(p.clone().add(new THREE.Vector3(0, 6, 0)), divine ? 7000 : 4000, 0.4, 0xd8e4ff);
    if (G.env) G.env.flash = divine ? 0.8 : 0.5;
    G.terrain.crater(p.x, p.z, divine ? 6 : 3, divine ? 2 : 0.8, { rim: 0.2, burn: 0.9, heat: 0.4 });
    G.fx.sparks(p.clone().add(new THREE.Vector3(0, 1, 0)), 40, [0.8, 0.9, 1], 18); G.fx.decal(p, divine ? 10 : 5); G.fx.shake(divine ? 0.35 : 0.15, p);
    this.areaDamage(p, divine ? 13 : 6, divine ? 520 : 200, 'burn', 'lightning');
    if (G.veg) G.veg.damageArea(p.x, p.z, divine ? 14 : 8, 2, 'burn');
    if (G.audio) G.audio.thunder(p, divine ? 1 : 0.6);
  }

  update(dt, input) {
    for (const k in this.cool) this.cool[k] = Math.max(0, this.cool[k] - dt);
    const god = G.god;
    // rayon divin
    if (this.laserOn) {
      this.laserT += dt;
      const aim = G.cam.aim(900);
      const hand = this.handPos();
      const dir = aim.point.clone().sub(hand); const L = dir.length(); dir.normalize();
      // le rayon traverse : on prolonge au-delà du premier impact
      this.beam.visible = true; this.beam.position.copy(hand); this.beam.lookAt(aim.point); this.beam.scale.set(1, 1, L);
      const k = Math.min(1, this.laserT * 4) * (this.laserT > 3.2 ? Math.max(0, (3.6 - this.laserT) / 0.4) : 1);
      this.beam.children.forEach((c, i) => { c.material.uniforms.uO.value = [1, 0.35, 0.12][i] * k * (0.9 + Math.random() * 0.2); });
      god.heading = Math.atan2(dir.x, dir.z);
      this.laserTick -= dt;
      if (this.laserTick <= 0) {
        this.laserTick = 0.07;
        const p = aim.point;
        if (aim.ground !== false) { G.terrain.scorch(p.x, p.z, 4.5, 1.6, 0.8, 1.0); G.fx.decal(p, 5); }
        G.fx.sparks(p, 10, [1, 0.6, 0.2], 16); G.fx.fire(p, 3); G.fx.smoke(p, 1, 7, 0.35);
        G.fx.flash(p.clone().add(new THREE.Vector3(0, 3, 0)), 2500, 0.15, 0xff9040);
        this.areaDamage(p, 9, 260, 'burn', 'laser');
        // tout ce qui se trouve sur la trajectoire est tranché
        if (G.villages) for (let s = 10; s < L; s += 8) { const q = hand.clone().addScaledVector(dir, s); if (q.y - G.terrain.heightAt(q.x, q.z) < 25) G.villages.damageArea(q.x, q.z, 3, 120, hand); }
        if (G.monster && G.monster.alive && aim.monster) G.monster.damage(90, hand);
        G.fx.shake(0.06, p);
      }
      if (this.laserT > 3.6 || !input.powerHeld) { this.laserOn = false; this.beam.visible = false; this.cool.laser = POWERS[1].cd; if (G.audio) G.audio.laser(false); god.action = null; }
    }
    // météores
    for (let i = this.meteors.length - 1; i >= 0; i--) {
      const M = this.meteors[i]; M.t += dt;
      const k = Math.min(1, M.t / M.dur);
      M.m.position.lerpVectors(M.start, M.target, k * k);
      M.m.rotation.x += dt * 3; M.m.rotation.y += dt * 2;
      for (let n = 0; n < 4; n++) { G.fx.fire(M.m.position.clone().add(new THREE.Vector3(rand(-3, 3), rand(-3, 3), rand(-3, 3))), 9 * (M.s || 1)); }
      G.fx.smoke(M.m.position, 1, 14, 0.5);
      if (k >= 1) {
        const p = M.target;
        this.scene.remove(M.m);
        this.meteors.splice(i, 1);
        const s = M.s || 1;
        G.terrain.crater(p.x, p.z, 28 * s, 12 * s, { rim: 0.4, burn: 1, heat: 1 });
        G.fx.sphere(p.clone().add(new THREE.Vector3(0, 6, 0)), 55 * s, 1.3, 0xffb060); G.fx.ring(p, 220 * s, 2.0, 0xffb070); G.fx.flash(p.clone().add(new THREE.Vector3(0, 15, 0)), 12000 * s, 1.4, 0xff9a40);
        G.fx.rocks(p, Math.round(60 * s), 35 * s); G.fx.dust(p, 30 * s, Math.round(60 * s), [0.35, 0.3, 0.26]); G.fx.shake(s, p); G.fx.decal(p, 70 * s);
        for (let n = 0; n < 30 * s; n++) G.fx.fire(p.clone().add(new THREE.Vector3(rand(-20, 20) * s, rand(0, 6), rand(-20, 20) * s)), 8 * s);
        if (G.env) G.env.flash = 0.6 * s;
        this.areaDamage(p, 60 * s, 1600 * s, 'blast', 'meteor');
        if (G.veg) G.veg.damageArea(p.x, p.z, 70 * s, 2, 'burn');
        if (G.audio) G.audio.explosion(p, 1.3 * s);
      }
    }
    // ondes de choc qui roulent sur le sol
    for (let i = this.waves.length - 1; i >= 0; i--) {
      const W = this.waves[i]; W.t += dt;
      const r = Math.min(95, W.t * 75);
      while (W.done < r) {
        W.done += 8;
        const ring = W.done;
        for (let a = 0; a < 6.28; a += 12 / Math.max(ring, 12)) {
          const q = new THREE.Vector3(W.p.x + Math.cos(a) * ring, 0, W.p.z + Math.sin(a) * ring);
          q.y = G.terrain.heightAt(q.x, q.z);
          if (Math.random() < 0.25) G.fx.dust(q, 3, 2);
        }
        this.areaDamage(W.p, ring, Math.max(80, 700 * (1 - ring / 100)), 'push', 'wave');
      }
      if (W.t > 1.4) this.waves.splice(i, 1);
    }
  }
}
